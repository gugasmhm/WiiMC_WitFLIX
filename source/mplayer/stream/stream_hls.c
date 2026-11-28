/* Liga o leitor HLS (hls_core.c) ao MPlayer do WiiMC. */
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "mp_msg.h"
#include "stream.h"
#include "network.h"
#include "http.h"
#include "url.h"
#include "libmpdemux/demuxer.h"
#include "hls_core.h"

extern int controlledbygui;

typedef struct
{
	int fd;
	HTTP_header_t *hdr;
	size_t body_pos;
} conn_t;

static void io_close(void *h)
{
	conn_t *c = h;
	if (c->fd >= 0) closesocket(c->fd);
	http_free(c->hdr);
	free(c);
}

static void *io_open(const char *url, char *final_url)
{
	char current[HLS_URL_MAX], next[HLS_URL_MAX];
	int redirects;

	snprintf(current, sizeof(current), "%s", url);
	for (redirects = 0; redirects <= 10; redirects++)
	{
		URL_t *u = url_new(current);
		conn_t *c = calloc(1, sizeof(*c));
		const char *location = NULL;

		if (!u || !c || !u->protocol
				|| (strcasecmp(u->protocol, "http") && strcasecmp(u->protocol, "https")))
		{
			if (u) url_free(u);
			free(c);
			return NULL;
		}
		c->fd = http_send_request(u, 0);
		url_free(u);
		if (c->fd >= 0) c->hdr = http_read_response(c->fd);
		if (!c->hdr)
		{
			io_log(c->fd < 0 ? "diag: sem conexao" : "diag: servidor nao respondeu");
			io_close(c);
			return NULL;
		}
		if (http_get_field(c->hdr, "Transfer-Encoding")) io_log("diag: resposta em Transfer-Encoding");
		if (http_get_field(c->hdr, "Content-Encoding")) io_log("diag: resposta em Content-Encoding");

		if (c->hdr->status_code == 200)
		{
			snprintf(final_url, HLS_URL_MAX, "%s", current);
			return c;
		}
		if (c->hdr->status_code >= 301 && c->hdr->status_code <= 308)
			location = http_get_field(c->hdr, "Location");
		if (!location)
		{
			mp_msg(MSGT_NETWORK, MSGL_ERR, "HLS: server returned %d\n", c->hdr->status_code);
			{
				char msg[48];
				snprintf(msg, sizeof(msg), "diag: servidor respondeu %d", c->hdr->status_code);
				io_log(msg);
			}
			io_close(c);
			return NULL;
		}
		hls_resolve(next, current, location);
		strcpy(current, next);
		io_close(c);
	}
	return NULL;
}

static int io_read(void *h, char *buf, int len)
{
	conn_t *c = h;

	if (c->body_pos < c->hdr->body_size) /* veio junto com o cabecalho */
	{
		size_t n = c->hdr->body_size - c->body_pos;
		if (n > (size_t) len) n = len;
		memcpy(buf, c->hdr->body + c->body_pos, n);
		c->body_pos += n;
		return n;
	}
	return recv(c->fd, buf, len, 0);
}

static int io_aborted(void) { return controlledbygui == 2; }
static void io_sleep(int ms) { usleep(ms * 1000); }

void wiiHlsDiag(const char *msg);

static void io_log(const char *msg)
{
	FILE *f = fopen("sd1:/wiimc_tls.log", "a");
	if (!f) f = fopen("usb1:/wiimc_tls.log", "a");
	if (!f) return;
	fprintf(f, "%s\n", msg);
	fclose(f);
}

void wiiHlsDiag(const char *msg) { io_log(msg); }

static const hls_io io = { io_open, io_read, io_close, io_aborted, io_sleep, io_log };

static int has_m3u8(const char *url)
{
	for (; *url; url++)
		if (!strncasecmp(url, ".m3u8", 5))
			return 1;
	return 0;
}

/* Usado pelo menu: este endereco e um fluxo HLS (e nao uma lista de canais)? */
int wiiIsHlsUrl(const char *url)
{
	char final_url[HLS_URL_MAX], head[4097];
	int size = 0, n;
	void *c;

	if (!has_m3u8(url))
		return 0;
	if (!(c = io_open(url, final_url)))
	{
		io_log("diag menu: nao abriu o m3u8");
		return 0;
	}
	while (size < 4096 && (n = io_read(c, head + size, 4096 - size)) > 0)
	{
		size += n;
		head[size] = 0;
		if (strstr(head, "#EXT-X-")) break;
	}
	head[size] = 0;
	io_close(c);
	io_log(strstr(head, "#EXT-X-") ? "diag menu: e HLS" : "diag menu: nao parece HLS (tratado como lista)");
	return strstr(head, "#EXT-X-") != NULL;
}

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
	int n = hls_read(s->priv, buffer, max_len);
	if (n < 0) errno = 0;
	return n;
}

static void close_s(stream_t *s)
{
	hls_close(s->priv);
	s->priv = NULL;
}

static int open_s(stream_t *stream, int mode, void *opts, int *file_format)
{
	hls_t *h;
	int result;

	if (mode != STREAM_READ || *file_format == DEMUXER_TYPE_PLAYLIST
			|| !has_m3u8(stream->url))
		return STREAM_UNSUPPORTED;

	io_log("diag player: abrindo HLS");
	result = hls_open(&h, stream->url, &io);
	if (result != HLS_OK)
		io_log(result == HLS_NOT_HLS ? "diag player: nao e HLS" : "diag player: HLS nao suportado");
	if (result == HLS_NOT_HLS) return STREAM_UNSUPPORTED;
	if (result != HLS_OK) return STREAM_ERROR;

	stream->priv = h;
	stream->fd = -1;
	stream->type = STREAMTYPE_STREAM;
	stream->fill_buffer = fill_buffer;
	stream->close = close_s;
	*file_format = DEMUXER_TYPE_MPEG_TS;
	return STREAM_OK;
}

const stream_info_t stream_info_hls = {
	"HLS (m3u8)",
	"hls",
	"WitFLIX",
	"http e https",
	open_s,
	{"http", "https", NULL},
	NULL,
	0
};
