/* Leitor HLS (m3u8) minimo: entrega os pedacos MPEG-TS como um fluxo continuo.
 * Nao depende do MPlayer: a rede vem por callbacks, para permitir testes. */
#ifndef WIIMC_HLS_CORE_H
#define WIIMC_HLS_CORE_H

#define HLS_URL_MAX 2048

typedef struct
{
	/* Abre a URL (ja seguindo redirecionamentos). final_url recebe o endereco
	 * efetivo, base dos caminhos relativos. NULL se falhar. */
	void *(*open)(const char *url, char *final_url);
	int (*read)(void *h, char *buf, int len); /* >0 bytes; <=0 fim ou erro */
	void (*close)(void *h);
	int (*aborted)(void);
	void (*sleep_ms)(int ms);
	void (*log)(const char *msg); /* sem URLs: podem conter senhas */
} hls_io;

typedef struct hls hls_t;

#define HLS_OK 0
#define HLS_NOT_HLS 1     /* nao e HLS (ou nao abriu): deixar para o http comum */
#define HLS_UNSUPPORTED 2 /* e HLS, mas nao da para tocar */

int hls_open(hls_t **out, const char *url, const hls_io *io);
int hls_read(hls_t *h, char *buf, int len); /* >0 bytes; -1 fim */
void hls_close(hls_t *h);
void hls_resolve(char *out, const char *base, const char *ref); /* out: HLS_URL_MAX */

#endif
