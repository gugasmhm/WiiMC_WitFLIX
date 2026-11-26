/* HTTPS para o WiiMC (mbedTLS). Ver tls.h. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_time.h"

#include "tls.h"
#include "tls_roots.h"

#define TLS_SLOTS 8
#define HANDSHAKE_TIMEOUT_MS 30000

#ifdef GEKKO
#include <gccore.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#define raw_send(fd, b, n) net_write(fd, b, n)
#define raw_recv(fd, b, n) net_read(fd, b, n)
#define raw_error(r) (-(r))
#define LOG_PATH "sd1:/wiimc_tls.log"
#define LOG_PATH_ALT "usb1:/wiimc_tls.log"
#define NOVERIFY_PATH "sd1:/apps/wiimc/tls_noverify.txt"
static mutex_t tls_mutex = LWP_MUTEX_NULL;
static void lock(void)
{
	if (tls_mutex == LWP_MUTEX_NULL)
		LWP_MutexInit(&tls_mutex, false);
	LWP_MutexLock(tls_mutex);
}
static void unlock(void) { LWP_MutexUnlock(tls_mutex); }
static uint64_t now_ms(void) { return ticks_to_millisecs(gettime()); }
#else /* testes no computador */
#include <sys/socket.h>
#include <time.h>
#define raw_send(fd, b, n) send(fd, b, n, 0)
#define raw_recv(fd, b, n) recv(fd, b, n, 0)
#define raw_error(r) (errno)
#define LOG_PATH "wiimc_tls.log"
#define LOG_PATH_ALT "wiimc_tls.log"
#define NOVERIFY_PATH "tls_noverify.txt"
static void lock(void) {}
static void unlock(void) {}
static uint64_t now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
#endif

typedef struct
{
	int fd;
	int used;
	mbedtls_ssl_context ssl;
} tls_slot;

static tls_slot slots[TLS_SLOTS];
static mbedtls_ssl_config conf;
static mbedtls_x509_crt ca;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context drbg;
static int ready = 0;

mbedtls_ms_time_t mbedtls_ms_time(void)
{
	return (mbedtls_ms_time_t) now_ms();
}

/* O Wii não oferece aqui um gerador aleatório de hardware: a semente vem do
 * relógio de alta resolução e da variação de tempo de execução. É mais fraca
 * que a de um computador. */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
	size_t i;
	(void) data;
#ifdef GEKKO
	static uint32_t state = 0x6a09e667;
	for (i = 0; i < len; i++)
	{
		volatile uint32_t spin = state & 63;
		while (spin) spin--;
		state = (state << 5) ^ (state >> 27) ^ (uint32_t) gettime();
		output[i] = (unsigned char) (state ^ (state >> 8) ^ (state >> 16));
	}
#else
	FILE *f = fopen("/dev/urandom", "rb");
	if (!f) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
	i = fread(output, 1, len, f);
	fclose(f);
	if (i != len) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
#endif
	*olen = len;
	return 0;
}

/* Nunca registrar caminhos de URL: podem conter senhas. */
static void tls_log(const char *fmt, ...)
{
	va_list ap;
	FILE *f = fopen(LOG_PATH, "a");
	if (!f) f = fopen(LOG_PATH_ALT, "a");
	if (!f) return;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

static void log_error(const char *what, const char *host, int ret)
{
	char msg[128];
	mbedtls_strerror(ret, msg, sizeof(msg));
	tls_log("%s %s: -0x%04x %s", what, host, (unsigned) -ret, msg);
}

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
	tls_slot *s = (tls_slot *) ctx;
	int r = raw_send(s->fd, buf, len);
	if (r >= 0) return r;
	return raw_error(r) == EAGAIN ? MBEDTLS_ERR_SSL_WANT_WRITE : -1;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len)
{
	tls_slot *s = (tls_slot *) ctx;
	int r = raw_recv(s->fd, buf, len);
	if (r >= 0) return r;
	return raw_error(r) == EAGAIN ? MBEDTLS_ERR_SSL_WANT_READ : -1;
}

/* chamada com o mutex travado */
static int tls_setup(void)
{
	int ret, verify = MBEDTLS_SSL_VERIFY_REQUIRED;
	FILE *f;

	if (ready) return 0;

	mbedtls_ssl_config_init(&conf);
	mbedtls_x509_crt_init(&ca);
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&drbg);

	ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
			(const unsigned char *) "wiimc", 5);
	if (ret != 0) goto fail;

	/* retorno > 0 significa que alguns certificados não foram lidos */
	ret = mbedtls_x509_crt_parse(&ca, (const unsigned char *) ca_bundle, sizeof(ca_bundle));
	if (ret < 0) goto fail;
	if (ret > 0) tls_log("%d certificados raiz ignorados", ret);

	ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret != 0) goto fail;

	f = fopen(NOVERIFY_PATH, "r");
	if (f)
	{
		fclose(f);
		verify = MBEDTLS_SSL_VERIFY_NONE;
		tls_log("AVISO: verificacao de certificados desligada");
	}
	mbedtls_ssl_conf_authmode(&conf, verify);
	mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
	ready = 1;
	return 0;

fail:
	log_error("inicializacao", "", ret);
	mbedtls_ssl_config_free(&conf);
	mbedtls_x509_crt_free(&ca);
	mbedtls_ctr_drbg_free(&drbg);
	mbedtls_entropy_free(&entropy);
	return -1;
}

static tls_slot *find(int fd)
{
	int i;
	for (i = 0; i < TLS_SLOTS; i++)
		if (slots[i].used && slots[i].fd == fd)
			return &slots[i];
	return NULL;
}

int tls_connect(int fd, const char *host)
{
	tls_slot *s = NULL;
	uint64_t start;
	int i, ret;

	tls_close(fd);

	/* o handshake usa o gerador aleatório compartilhado: um de cada vez */
	lock();
	if (tls_setup() != 0) goto out_fail;

	for (i = 0; i < TLS_SLOTS && !s; i++)
		if (!slots[i].used)
			s = &slots[i];
	if (!s)
	{
		tls_log("sem conexoes TLS livres (%s)", host);
		goto out_fail;
	}

	s->fd = fd;
	mbedtls_ssl_init(&s->ssl);
	ret = mbedtls_ssl_setup(&s->ssl, &conf);
	if (ret == 0) ret = mbedtls_ssl_set_hostname(&s->ssl, host);
	if (ret != 0)
	{
		log_error("preparo", host, ret);
		goto out_free;
	}
	mbedtls_ssl_set_bio(&s->ssl, s, bio_send, bio_recv, NULL);

	start = now_ms();
	while ((ret = mbedtls_ssl_handshake(&s->ssl)) != 0)
	{
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
			break;
		if (now_ms() - start > HANDSHAKE_TIMEOUT_MS)
			break;
		usleep(2000);
	}
	if (ret != 0)
	{
		log_error("handshake", host, ret);
		if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
			tls_log("verificacao=0x%x (confira a data e a hora do Wii)",
					(unsigned) mbedtls_ssl_get_verify_result(&s->ssl));
		goto out_free;
	}

	s->used = 1;
	tls_log("conectado %s %s %s", host, mbedtls_ssl_get_version(&s->ssl),
			mbedtls_ssl_get_ciphersuite(&s->ssl));
	unlock();
	return 0;

out_free:
	mbedtls_ssl_free(&s->ssl);
out_fail:
	unlock();
	return -1;
}

int tls_send(int fd, const void *buf, int len)
{
	tls_slot *s = find(fd);
	int done = 0, ret;
	uint64_t start;

	if (!s) return raw_send(fd, buf, len);

	start = now_ms();
	while (done < len)
	{
		ret = mbedtls_ssl_write(&s->ssl, (const unsigned char *) buf + done, len - done);
		if (ret > 0)
			done += ret;
		else if ((ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
				&& now_ms() - start < HANDSHAKE_TIMEOUT_MS)
			usleep(1000);
		else
			return -1;
	}
	return done;
}

/* Como net_read: >0 bytes, 0 fim, <0 erro (-EAGAIN se não houver dados). */
int tls_recv(int fd, void *buf, int len)
{
	tls_slot *s = find(fd);
	int ret;

	if (!s) return raw_recv(fd, buf, len);

	ret = mbedtls_ssl_read(&s->ssl, (unsigned char *) buf, len);
	if (ret >= 0) return ret;
	if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
	{
		errno = EAGAIN;
		return -EAGAIN;
	}
	if (ret == MBEDTLS_ERR_SSL_CONN_EOF) return 0;
	log_error("leitura", "", ret);
	return -1;
}

void tls_close(int fd)
{
	tls_slot *s;
	lock();
	s = find(fd);
	if (s)
	{
		s->used = 0;
		mbedtls_ssl_free(&s->ssl);
	}
	unlock();
}
