/* Ver hls_core.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hls_core.h"

#define PLAYLIST_MAX (256 * 1024)
#define SEGMENTS_MAX 4000
#define LIVE_EDGE 3         /* comeca a tres pedacos do fim */
#define MAX_HEIGHT 480
#define STALL_LIMIT_MS 60000

struct hls
{
	hls_io io;
	char master_url[HLS_URL_MAX]; /* o endereco pedido */
	char media_url[HLS_URL_MAX];  /* playlist de pedacos */
	char media_base[HLS_URL_MAX]; /* media_url depois dos redirecionamentos */
	char **seg;
	int nseg;
	long long first_seq;
	long long next_seq;
	int target_ms;
	int endlist;
	void *conn;
	int errors;
	int stalled_ms;
};

void hls_resolve(char *out, const char *base, const char *ref)
{
	const char *scheme_end = strstr(base, "://");
	const char *host_end, *query, *slash, *p;
	const char *ref_scheme = strstr(ref, "://");
	const char *ref_query = strpbrk(ref, "/?#");

	if ((ref_scheme && (!ref_query || ref_scheme < ref_query)) || !scheme_end)
	{
		snprintf(out, HLS_URL_MAX, "%s", ref);
		return;
	}
	if (ref[0] == '/' && ref[1] == '/')
	{
		snprintf(out, HLS_URL_MAX, "%.*s:%s", (int) (scheme_end - base), base, ref);
		return;
	}
	host_end = scheme_end + 3 + strcspn(scheme_end + 3, "/?#");
	if (ref[0] == '/')
	{
		snprintf(out, HLS_URL_MAX, "%.*s%s", (int) (host_end - base), base, ref);
		return;
	}
	query = host_end + strcspn(host_end, "?#");
	slash = NULL;
	for (p = host_end; p < query; p++)
		if (*p == '/')
			slash = p;
	if (slash)
		snprintf(out, HLS_URL_MAX, "%.*s%s", (int) (slash + 1 - base), base, ref);
	else
		snprintf(out, HLS_URL_MAX, "%.*s/%s", (int) (host_end - base), base, ref);
}

/* Baixa uma playlist inteira. Retorna texto alocado ou NULL. */
static char *fetch(hls_t *h, const char *url, char *final_url)
{
	void *c = h->io.open(url, final_url);
	char *text, *bigger;
	int size = 0, cap = 8192, n;

	if (!c) return NULL;
	text = malloc(cap + 1);
	while (text && !h->io.aborted())
	{
		if (size == cap)
		{
			if (cap >= PLAYLIST_MAX) break;
			cap *= 2;
			bigger = realloc(text, cap + 1);
			if (!bigger) { free(text); text = NULL; break; }
			text = bigger;
		}
		n = h->io.read(c, text + size, cap - size);
		if (n <= 0) break;
		size += n;
		/* o que nao comeca como playlist (um video, por exemplo) para cedo */
		if (size >= 16 && !strstr((text[size] = 0, text), "#EXTM3U"))
			break;
	}
	h->io.close(c);
	if (text) text[size] = 0;
	return text;
}

/* Proxima linha, sem espacos nas pontas; altera o texto. */
static char *next_line(char **cursor)
{
	char *line = *cursor, *end;

	if (!line || !*line) return NULL;
	end = strchr(line, '\n');
	if (end) { *end = 0; *cursor = end + 1; }
	else *cursor = line + strlen(line);
	while (*line == ' ' || *line == '\t') line++;
	end = line + strlen(line);
	while (end > line && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
		*--end = 0;
	return line;
}

/* Valor de um atributo NOME=valor, respeitando virgulas entre aspas. */
static int attribute(const char *list, const char *name, char *out, int size)
{
	size_t len = strlen(name);
	const char *p = list;

	while (*p)
	{
		int match = !strncmp(p, name, len) && p[len] == '=';
		const char *v = strchr(p, '=');
		int quoted, n = 0;

		if (!v) break;
		v++;
		quoted = *v == '"';
		if (quoted) v++;
		while (*v && (quoted ? *v != '"' : *v != ','))
		{
			if (match && n < size - 1) out[n++] = *v;
			v++;
		}
		if (match) { out[n] = 0; return 1; }
		if (quoted && *v) v++;
		if (*v == ',') v++;
		p = v;
	}
	return 0;
}

/* Escolhe a variante: a maior ate 480 linhas; sem resolucao, a de menor taxa;
 * se so houver HD, a menor. Retorna 0 se a playlist nao tiver variantes. */
static int pick_variant(char *text, char *out_ref, int *out_height, int *separate_audio)
{
	char *cursor = text, *line, value[64], media_type[16], uri[8];
	long best_rank = -1, bandwidth = 0;
	int height = 0, pending = 0, found = 0;

	while ((line = next_line(&cursor)))
	{
		if (!strncmp(line, "#EXT-X-STREAM-INF:", 18))
		{
			bandwidth = attribute(line + 18, "BANDWIDTH", value, sizeof(value)) ? atol(value) : 0;
			height = 0;
			if (attribute(line + 18, "RESOLUTION", value, sizeof(value)) && strchr(value, 'x'))
				height = atoi(strchr(value, 'x') + 1);
			pending = 1;
		}
		else if (!strncmp(line, "#EXT-X-MEDIA:", 13))
		{
			if (attribute(line + 13, "TYPE", media_type, sizeof(media_type))
					&& !strcmp(media_type, "AUDIO") && attribute(line + 13, "URI", uri, sizeof(uri)))
				*separate_audio = 1;
		}
		else if (pending && *line && *line != '#')
		{
			long rank, kbps = bandwidth / 1000;
			if (kbps > 999999) kbps = 999999;
			if (height > 0 && height <= MAX_HEIGHT)
				rank = 3000000000L - 1000000000L + (long) height * 1000000 + kbps;
			else if (height == 0)
				rank = 1000000000L - kbps;
			else
				rank = 100000 - (height > 99999 ? 99999 : height);
			if (rank > best_rank)
			{
				best_rank = rank;
				snprintf(out_ref, HLS_URL_MAX, "%s", line);
				*out_height = height;
				found = 1;
			}
			pending = 0;
		}
	}
	return found;
}

static void free_segments(char **seg, int n)
{
	int i;
	for (i = 0; i < n; i++) free(seg[i]);
	free(seg);
}

/* Interpreta a playlist de pedacos. Retorna HLS_OK ou HLS_UNSUPPORTED. */
static int parse_media(hls_t *h, char *text)
{
	char *cursor = text, *line, method[32];
	char **seg = NULL, **bigger;
	int n = 0, cap = 0, expect = 0, endlist = 0, target = 6000;
	long long first = 0;

	while ((line = next_line(&cursor)))
	{
		if (!strncmp(line, "#EXT-X-TARGETDURATION:", 22))
			target = atoi(line + 22) * 1000;
		else if (!strncmp(line, "#EXT-X-MEDIA-SEQUENCE:", 22))
			first = strtoll(line + 22, NULL, 10);
		else if (!strncmp(line, "#EXT-X-ENDLIST", 14))
			endlist = 1;
		else if (!strncmp(line, "#EXT-X-KEY:", 11)
				&& attribute(line + 11, "METHOD", method, sizeof(method)) && strcmp(method, "NONE"))
		{
			h->io.log("canal criptografado: nao suportado nesta versao");
			goto unsupported;
		}
		else if (!strncmp(line, "#EXT-X-MAP:", 11) || !strncmp(line, "#EXT-X-BYTERANGE:", 17))
		{
			h->io.log("formato de pedacos (fMP4/byterange) nao suportado");
			goto unsupported;
		}
		else if (!strncmp(line, "#EXTINF:", 8))
			expect = 1;
		else if (expect && *line && *line != '#')
		{
			expect = 0;
			if (n == SEGMENTS_MAX) break;
			if (n == cap)
			{
				cap = cap ? cap * 2 : 32;
				bigger = realloc(seg, cap * sizeof(char *));
				if (!bigger) goto unsupported;
				seg = bigger;
			}
			seg[n] = strdup(line);
			if (!seg[n]) goto unsupported;
			n++;
		}
	}

	free_segments(h->seg, h->nseg);
	h->seg = seg;
	h->nseg = n;
	h->first_seq = first;
	h->endlist = endlist;
	h->target_ms = target < 1000 ? 1000 : target;
	return HLS_OK;

unsupported:
	free_segments(seg, n);
	return HLS_UNSUPPORTED;
}

/* Carrega a playlist a partir de url, passando pela lista de variantes. */
static int load(hls_t *h, const char *url, int verbose)
{
	char final_url[HLS_URL_MAX], ref[HLS_URL_MAX], msg[64];
	char *text, *copy;
	int height = 0, audio = 0, result;

	snprintf(h->media_url, HLS_URL_MAX, "%s", url);
	text = fetch(h, url, final_url);
	if (!text) return HLS_NOT_HLS;
	if (!strstr(text, "#EXT-X-")) { free(text); return HLS_NOT_HLS; }

	copy = strdup(text);
	if (copy && pick_variant(copy, ref, &height, &audio))
	{
		if (verbose)
		{
			snprintf(msg, sizeof(msg), "variante escolhida: %d linhas", height);
			h->io.log(msg);
			if (audio) h->io.log("aviso: audio em faixa separada nao e suportado");
		}
		free(text);
		hls_resolve(h->media_url, final_url, ref);
		text = fetch(h, h->media_url, final_url);
		if (!text || !strstr(text, "#EXT"))
		{
			free(text);
			free(copy);
			return HLS_UNSUPPORTED;
		}
	}
	free(copy);

	snprintf(h->media_base, HLS_URL_MAX, "%s", final_url);
	result = parse_media(h, text);
	free(text);
	return result;
}

static long long live_start(hls_t *h)
{
	return h->first_seq + (h->nseg > LIVE_EDGE ? h->nseg - LIVE_EDGE : 0);
}

int hls_open(hls_t **out, const char *url, const hls_io *io)
{
	hls_t *h = calloc(1, sizeof(*h));
	int result;

	*out = NULL;
	if (!h) return HLS_UNSUPPORTED;
	h->io = *io;
	snprintf(h->master_url, HLS_URL_MAX, "%s", url);
	result = load(h, url, 1);
	if (result == HLS_OK && h->nseg == 0)
		result = HLS_UNSUPPORTED;
	if (result != HLS_OK)
	{
		hls_close(h);
		return result;
	}
	h->next_seq = h->endlist ? h->first_seq : live_start(h);
	h->io.log(h->endlist ? "HLS: video gravado" : "HLS: ao vivo");
	*out = h;
	return HLS_OK;
}

static int wait_ms(hls_t *h, int ms)
{
	while (ms > 0 && !h->io.aborted())
	{
		h->io.sleep_ms(100);
		ms -= 100;
	}
	return !h->io.aborted();
}

/* Recarrega a playlist ao vivo. Retorna 0 se deve desistir. */
static int reload(hls_t *h)
{
	char url[HLS_URL_MAX], final_url[HLS_URL_MAX];
	char *text = NULL;
	int result = HLS_NOT_HLS;

	/* depois de falhas, volta ao endereco original (sessoes expiram) */
	if (h->errors >= 3)
	{
		snprintf(url, HLS_URL_MAX, "%s", h->master_url);
		result = load(h, url, 0);
	}
	else if ((text = fetch(h, h->media_url, final_url)) && strstr(text, "#EXTINF"))
	{
		snprintf(h->media_base, HLS_URL_MAX, "%s", final_url);
		result = parse_media(h, text);
	}
	free(text);

	if (result == HLS_UNSUPPORTED) return 0;
	if (result != HLS_OK) return ++h->errors <= 10;

	/* o servidor reiniciou a numeracao ou ficamos para tras */
	if (h->next_seq > h->first_seq + h->nseg + LIVE_EDGE)
		h->next_seq = live_start(h);
	if (h->next_seq < h->first_seq)
		h->next_seq = h->first_seq;
	return 1;
}

int hls_read(hls_t *h, char *buf, int len)
{
	char url[HLS_URL_MAX], final_url[HLS_URL_MAX];
	int n, pause;

	while (!h->io.aborted())
	{
		if (h->conn)
		{
			n = h->io.read(h->conn, buf, len);
			if (n > 0)
			{
				h->errors = 0;
				return n;
			}
			h->io.close(h->conn);
			h->conn = NULL;
			h->next_seq++;
			continue;
		}

		if (h->next_seq < h->first_seq + h->nseg)
		{
			h->stalled_ms = 0;
			hls_resolve(url, h->media_base, h->seg[h->next_seq - h->first_seq]);
			h->conn = h->io.open(url, final_url);
			if (!h->conn)
			{
				h->io.log("falha ao abrir um pedaco");
				if (++h->errors > 5 || !wait_ms(h, 500)) break;
				h->next_seq++;
			}
			continue;
		}

		if (h->endlist) break;

		pause = h->target_ms / 2;
		if (!wait_ms(h, pause)) break;
		h->stalled_ms += pause;
		if (h->stalled_ms > STALL_LIMIT_MS)
		{
			h->io.log("canal parado: sem pedacos novos");
			break;
		}
		if (!reload(h)) break;
	}
	return -1;
}

void hls_close(hls_t *h)
{
	if (!h) return;
	if (h->conn) h->io.close(h->conn);
	free_segments(h->seg, h->nseg);
	free(h);
}
