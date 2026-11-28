/* Compara cada rotina paired singles de audio com a referencia em C, no emulador. */
#include <gccore.h>
#include <math.h>
#include <string.h>
#include "libavcodec/dsputil.h"
#include "libavcodec/fft.h"
void ff_float_init_paired(DSPContext *c, AVCodecContext *avctx);
void ff_dsputil_init_paired(DSPContext *c, AVCodecContext *avctx);
void ff_imdct_half_c(FFTContext *s, FFTSample *output, const FFTSample *input);
void ff_imdct_calc_c(FFTContext *s, FFTSample *output, const FFTSample *input);
void ff_ac3_downmix_c(float (*samples)[256], float (*matrix)[2], int out_ch, int in_ch, int len);

#define N 8192
#define A32 __attribute__((aligned(32)))
static float A32 s0[N], s1[N], s2[N], r[N], t[N], r2[N], t2[N];
static float A32 sm[6][256], sm2[6][256], mx[6][2];
static unsigned seed = 1;
static float rnd(void) { seed = seed * 1103515245 + 12345; return ((int)(seed >> 8) % 20001 - 10000) / 10000.0f; }
static void fill(float *p, int n) { while (n--) *p++ = rnd(); }

static void check(const char *name, int len, const float *a, const float *b, int n)
{
	float max = 0, err = 0; int i, bad = -1, nbad = 0;
	for (i = 0; i < n; i++) { float m = fabsf(a[i]); if (m > max) max = m; }
	for (i = 0; i < n; i++) {
		float d = fabsf(a[i] - b[i]);
		if (!(d <= max * 1e-3f + 1e-6f)) { if (bad < 0) bad = i; nbad++; }
		if (d > err) err = d;
	}
	SYS_Report("EMUTEST %s %s len=%d erradas=%d/%d primeira=%d erro=%d/1000000\n",
		nbad ? "FALHOU" : "ok", name, len, nbad, n, bad, (int)(err / (max + 1e-9f) * 1e6f));
}
#define ON(f) if (c.f)
#define RESET() do { fill(r, N); memcpy(t, r, sizeof(r)); } while (0)

int main(void)
{
	static DSPContext c;
	int len, i, j, k;

	ff_dsputil_init_paired(&c, NULL);   /* tambem configura os GQR */
	ff_float_init_paired(&c, NULL);
	fill(s0, N); fill(s1, N); fill(s2, N);

	for (len = 64; len <= 1024; len *= 4) {
		RESET(); for (i = 0; i < len; i++) r[i] = s0[i] * s1[i];
		ON(vector_fmul) { c.vector_fmul(t, s0, s1, len); check("vector_fmul", len, r, t, len + 8); }
		RESET(); for (i = 0; i < len; i++) r[i] = s0[i] * s1[len - 1 - i];
		ON(vector_fmul_reverse) { c.vector_fmul_reverse(t, s0, s1, len); check("vector_fmul_reverse", len, r, t, len + 8); }
		RESET(); for (i = 0; i < len; i++) r[i] = s0[i] * s1[i] + s2[i];
		ON(vector_fmul_add) { c.vector_fmul_add(t, s0, s1, s2, len); check("vector_fmul_add", len, r, t, len + 8); }
		RESET();
		for (i = 0, j = 2 * len - 1; i < len; i++, j--) {
			float a = s0[i], b = s1[j - len], wi = s2[i], wj = s2[j];
			r[i] = a * wj - b * wi; r[j] = a * wi + b * wj;
		}
		ON(vector_fmul_window) { c.vector_fmul_window(t, s0, s1, s2, len); check("vector_fmul_window", len, r, t, 2 * len + 8); }
		fill(r, N); fill(r2, N); memcpy(t, r, sizeof(r)); memcpy(t2, r2, sizeof(r));
		for (i = 0; i < len; i++) { float d = r[i] - r2[i]; r[i] += r2[i]; r2[i] = d; }
		ON(butterflies_float) {
			c.butterflies_float(t, t2, len);
			check("butterflies_v1", len, r, t, len + 8); check("butterflies_v2", len, r2, t2, len + 8);
		}
		ON(scalarproduct_float) { float a = 0, b; for (i = 0; i < len; i++) a += s0[i] * s1[i];
		  b = c.scalarproduct_float(s0, s1, len); check("scalarproduct", len, &a, &b, 1); }
		RESET(); for (i = 0; i < len; i++) r[i] = s0[i] * 0.37f;
		ON(vector_fmul_scalar) { c.vector_fmul_scalar(t, s0, 0.37f, len); check("vector_fmul_scalar", len, r, t, len + 8); }
	}

	/* vorbis */
	fill(r, 512); fill(r2, 512); memcpy(t, r, 512 * 4); memcpy(t2, r2, 512 * 4);
	for (i = 0; i < 256; i++) {
		float m = r[i], a = r2[i];
		if (m > 0) { if (a > 0) r2[i] = m - a; else { r2[i] = m; r[i] = m + a; } }
		else       { if (a > 0) r2[i] = m + a; else { r2[i] = m; r[i] = m - a; } }
	}
	if (c.vorbis_inverse_coupling) {
		c.vorbis_inverse_coupling(t, t2, 256);
		check("vorbis_mag", 256, r, t, 256); check("vorbis_ang", 256, r2, t2, 256);
	}

	/* downmix AC3 */
	for (j = 1; j <= 2; j++) for (k = 2; k <= 5; k++) {
		if (k <= j) continue;
		for (i = 0; i < 6; i++) { fill(sm[i], 256); mx[i][0] = rnd(); mx[i][1] = rnd(); }
		memcpy(sm2, sm, sizeof(sm));
		ff_ac3_downmix_c(sm, mx, j, k, 256);
		if (!c.ac3_downmix) continue;
		c.ac3_downmix(sm2, mx, j, k, 256);
		check(j == 2 ? "ac3_downmix_para2" : "ac3_downmix_para1", k, sm[0], sm2[0], 256 * 6);
	}

	/* imdct */
	for (k = 7; k <= 12; k++) {
		static FFTContext s;
		int n = 1 << k;
		memset(&s, 0, sizeof(s));
		ff_mdct_init(&s, k, 1, 1.0);
		fill(s0, n / 2);
		RESET(); ff_imdct_half_c(&s, r, s0); s.imdct_half(&s, t, s0);
		check("imdct_half", n, r, t, n / 2 + 8);
		RESET(); ff_imdct_calc_c(&s, r, s0); s.imdct_calc(&s, t, s0);
		check("imdct_calc", n, r, t, n + 8);
	}
	SYS_Report("EMUTEST FIM\n");
	for (;;) VIDEO_WaitVSync();
}
