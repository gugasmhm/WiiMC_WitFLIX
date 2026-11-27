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
 * Ligado com -Wl,--wrap=mspace_malloc (ver Makefile).
 ***************************************************************************/
#include <stddef.h>

void *__real_mspace_malloc(void *msp, size_t bytes);

void *__wrap_mspace_malloc(void *msp, size_t bytes)
{
	if (bytes > (size_t) -1 - 32)
		return NULL;
	return __real_mspace_malloc(msp, bytes + 32);
}
