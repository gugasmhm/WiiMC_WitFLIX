/* HTTPS para o WiiMC: TLS sobre um socket já conectado, identificado pelo fd. */
#ifndef WIIMC_TLS_H
#define WIIMC_TLS_H

#include <strings.h>

/* endereco de internet, com ou sem TLS */
static inline int is_http_url(const char *u)
{
	return !strncasecmp(u, "http:", 5) || !strncasecmp(u, "https:", 6);
}

#ifdef __cplusplus
extern "C" {
#endif

/* Faz o handshake. Retorna 0 se deu certo, -1 se falhou. */
int tls_connect(int fd, const char *host);
/* Para um fd sem TLS, as duas chamam o socket direto. */
int tls_send(int fd, const void *buf, int len);
int tls_recv(int fd, void *buf, int len);
/* Libera o estado TLS do fd (não fecha o socket). Seguro para qualquer fd. */
void tls_close(int fd);

#ifdef __cplusplus
}
#endif
#endif
