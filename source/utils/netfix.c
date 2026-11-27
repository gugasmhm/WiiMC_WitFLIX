/****************************************************************************
 * Correção para o heap de rede da libogc2 (travava o Wii real ao abrir listas).
 *
 * Desde a libogc2 f0a7627 (out/2026) o heap de rede é um mspace do dlmalloc:
 * o cabeçalho do bloco seguinte pode ficar na mesma linha de cache (32 bytes)
 * do fim do buffer entregue ao IOS. Enquanto uma thread espera o IOS, outra
 * pode atualizar esse cabeçalho; na volta, o DCInvalidateRange descarta a
 * atualização e o heap se corrompe. O Dolphin não emula o cache, então lá o
 * defeito não aparece.
 *
 * Com 32 bytes de folga, nenhuma linha do buffer alcança o cabeçalho vizinho.
 * Vale só para o heap de rede: o malloc geral também passa por mspace_malloc.
 * Ligado com -Wl,--wrap (ver Makefile).
 ***************************************************************************/
#include <stddef.h>

#define NET_HEAP_SIZE (64 * 1024) /* como em libogc2/libogc/network_wii.c */

void *__real_create_mspace_with_base(void *base, size_t capacity, int locked);
void *__real_mspace_malloc(void *msp, size_t bytes);

static void *net_heap = NULL;

void *__wrap_create_mspace_with_base(void *base, size_t capacity, int locked)
{
	void *msp = __real_create_mspace_with_base(base, capacity, locked);
	if (capacity == NET_HEAP_SIZE)
		net_heap = msp;
	return msp;
}

void *__wrap_mspace_malloc(void *msp, size_t bytes)
{
	if (msp == net_heap && bytes <= NET_HEAP_SIZE)
		bytes += 32;
	return __real_mspace_malloc(msp, bytes);
}
