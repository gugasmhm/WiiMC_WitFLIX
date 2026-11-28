/* Compara a rotina de croma H.264 do WitFLIX com a referencia em C do FFmpeg.
 *   cc -O2 -Wall -o /tmp/t tools/tests/test_h264chroma.c \
 *      source/mplayer/ffmpeg/libavcodec/ppc/h264chroma_swar.c && /tmp/t
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../source/mplayer/ffmpeg/libavcodec/ppc/h264chroma_swar.h"

typedef void (*mc_func)(uint8_t *, uint8_t *, int, int, int, int);

/* mesma formula de dsputil_template.c (8 bits) */
static void ref_mc(uint8_t *dst, const uint8_t *src, int stride, int w, int h, int x, int y, int avg)
{
	int A = (8 - x) * (8 - y), B = x * (8 - y), C = (8 - x) * y, D = x * y;
	int i, j;
	for (i = 0; i < h; i++, dst += stride, src += stride)
		for (j = 0; j < w; j++)
		{
			int v;
			if (D)
				v = A * src[j] + B * src[j + 1] + C * src[stride + j] + D * src[stride + j + 1];
			else
				v = A * src[j] + (B + C) * src[(C ? stride : 1) + j];
			v = (v + 32) >> 6;
			dst[j] = avg ? (dst[j] + v + 1) >> 1 : v;
		}
}

#define STRIDE 40
#define SIZE (STRIDE * 24)

int main(void)
{
	static const struct { const char *name; mc_func f; int w, avg; } t[] = {
		{ "put8", witflix_put_h264_chroma_mc8, 8, 0 },
		{ "put4", witflix_put_h264_chroma_mc4, 4, 0 },
		{ "avg8", witflix_avg_h264_chroma_mc8, 8, 1 },
		{ "avg4", witflix_avg_h264_chroma_mc4, 4, 1 },
	};
	static uint8_t src[SIZE + 16], a[SIZE + 16], b[SIZE + 16], init[SIZE + 16];
	int k, x, y, h, off, round, i, cases = 0;

	srand(1);
	for (round = 0; round < 60; round++)
	{
		for (i = 0; i < SIZE; i++)
		{
			/* rodadas 0 e 1: extremos, onde um vai-um entre faixas apareceria */
			src[i] = round == 0 ? 255 : round == 1 ? (i & 1) * 255 : rand();
			init[i] = round == 0 ? 255 : rand();
		}
		for (k = 0; k < 4; k++)
			for (x = 0; x < 8; x++)
				for (y = 0; y < 8; y++)
					for (h = 2; h <= 16; h *= 2)
						for (off = 0; off < 4; off++)
						{
							memcpy(a, init, SIZE);
							memcpy(b, init, SIZE);
							ref_mc(a + 8, src + off, STRIDE, t[k].w, h, x, y, t[k].avg);
							t[k].f(b + 8, src + off, STRIDE, h, x, y);
							if (memcmp(a, b, SIZE))
							{
								printf("FALHOU %s x=%d y=%d h=%d off=%d rodada=%d\n",
										t[k].name, x, y, h, off, round);
								return 1;
							}
							cases++;
						}
	}
	printf("OK: %d casos identicos a referencia\n", cases);
	return 0;
}
