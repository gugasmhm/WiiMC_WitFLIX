/*
 * WitFLIX: compensacao de movimento de croma H.264 (8 bits).
 *
 * Cada palavra de 32 bits com 4 pixels e dividida em duas com 2 faixas de 16
 * bits, e uma multiplicacao passa a calcular 2 pixels. O maior valor de uma
 * faixa e 255*64+32 = 16352, entao nao ha vai-um entre faixas. Cada byte volta
 * para a posicao de onde saiu, por isso o resultado independe da ordem dos
 * bytes (e o teste roda no computador).
 */
#include "h264chroma_swar.h"

#define LANES 0x00FF00FFu
#define ROUND 0x00200020u

static inline uint32_t load32(const uint8_t *p)
{
	uint32_t v;
	__builtin_memcpy(&v, p, 4);
	return v;
}

static inline void store32(uint8_t *p, uint32_t v)
{
	__builtin_memcpy(__builtin_assume_aligned(p, 4), &v, 4);
}

static inline void emit(uint8_t *dst, uint32_t even, uint32_t odd, const int avg)
{
	uint32_t v = (((even + ROUND) >> 6) & LANES) | ((((odd + ROUND) >> 6) & LANES) << 8);

	if (avg)
	{
		uint32_t d = load32(dst);
		v = (d | v) - (((d ^ v) & 0xFEFEFEFEu) >> 1);   /* (d+v+1)>>1 por byte */
	}
	store32(dst, v);
}

static inline __attribute__((always_inline))
void chroma_mc(uint8_t *dst, const uint8_t *src, int stride, int h, int x, int y,
		const int words, const int avg)
{
	const uint32_t A = (8 - x) * (8 - y), B = x * (8 - y), C = (8 - x) * y, D = x * y;
	int i, j;

	if (D)
	{
		for (j = 0; j < words; j++)
		{
			const uint8_t *s = src + 4 * j;
			uint8_t *d = dst + 4 * j;
			uint32_t a = load32(s), b = load32(s + 1);

			for (i = 0; i < h; i++, d += stride)
			{
				/* a linha de baixo vira a de cima na proxima volta */
				uint32_t c, e;
				s += stride;
				c = load32(s);
				e = load32(s + 1);
				emit(d,
						A * (a & LANES) + B * (b & LANES) + C * (c & LANES) + D * (e & LANES),
						A * ((a >> 8) & LANES) + B * ((b >> 8) & LANES)
						+ C * ((c >> 8) & LANES) + D * ((e >> 8) & LANES), avg);
				a = c;
				b = e;
			}
		}
	}
	else
	{
		const uint32_t E = B + C;
		const int step = C ? stride : 1;

		for (i = 0; i < h; i++, dst += stride, src += stride)
			for (j = 0; j < words; j++)
			{
				uint32_t a = load32(src + 4 * j), b = load32(src + 4 * j + step);
				emit(dst + 4 * j,
						A * (a & LANES) + E * (b & LANES),
						A * ((a >> 8) & LANES) + E * ((b >> 8) & LANES), avg);
			}
	}
}

void witflix_put_h264_chroma_mc8(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y)
{
	chroma_mc(dst, src, stride, h, x, y, 2, 0);
}

void witflix_put_h264_chroma_mc4(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y)
{
	chroma_mc(dst, src, stride, h, x, y, 1, 0);
}

void witflix_avg_h264_chroma_mc8(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y)
{
	chroma_mc(dst, src, stride, h, x, y, 2, 1);
}

void witflix_avg_h264_chroma_mc4(uint8_t *dst, uint8_t *src, int stride, int h, int x, int y)
{
	chroma_mc(dst, src, stride, h, x, y, 1, 1);
}
