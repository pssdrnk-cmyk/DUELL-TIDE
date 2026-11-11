/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab: FFT of any length (radix 2 + Bluestein), the BS.1770 loudness, true peak, wav reading, small statistics.
 * These are ports of the old numpy scripts (work/b9/dub, python files); definitions are noted at each function. */
#include "sl_lab.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------- FFT ---------------- */
typedef struct {
    size_t n, M; int pow2;
    double *wr, *wi;      /* chirp exp(-i pi k^2 / n), n entries (Bluestein only) */
    double *br, *bi;      /* FFT of the conjugate chirp, M entries */
    double *tr, *ti;      /* twiddles exp(-2 pi i k / M), M/2 entries */
    unsigned long stamp;
} plan;

#define NPLAN 6
static plan plans[NPLAN];
static unsigned long clock_ = 0;

static void fft2(double *re, double *im, size_t M, const plan *p) {
    size_t i, j, k, len;
    for (i = 1, j = 0; i < M; i++) {
        size_t bit = M >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (len = 2; len <= M; len <<= 1) {
        size_t half = len >> 1, step = M / len;
        for (i = 0; i < M; i += len) {
            for (k = 0; k < half; k++) {
                double wr = p->tr[k * step], wi = p->ti[k * step];
                double xr = re[i + k + half], xi = im[i + k + half];
                double tr = xr * wr - xi * wi, ti = xr * wi + xi * wr;
                re[i + k + half] = re[i + k] - tr; im[i + k + half] = im[i + k] - ti;
                re[i + k] += tr; im[i + k] += ti;
            }
        }
    }
}

static void plan_free(plan *p) {
    free(p->wr); free(p->wi); free(p->br); free(p->bi); free(p->tr); free(p->ti);
    memset(p, 0, sizeof *p);
}

static plan *get_plan(size_t n) {
    size_t i, k; int slot = -1; plan *p;
    for (i = 0; i < NPLAN; i++) if (plans[i].n == n) { plans[i].stamp = ++clock_; return &plans[i]; }
    for (i = 0; i < NPLAN; i++) if (!plans[i].n) { slot = (int)i; break; }
    if (slot < 0) { slot = 0; for (i = 1; i < NPLAN; i++) if (plans[i].stamp < plans[slot].stamp) slot = (int)i; plan_free(&plans[slot]); }
    p = &plans[slot];
    p->n = n; p->stamp = ++clock_;
    p->pow2 = (n & (n - 1)) == 0;
    p->M = n;
    if (!p->pow2) { p->M = 1; while (p->M < 2 * n - 1) p->M <<= 1; }
    p->tr = (double *)malloc(sizeof(double) * (p->M / 2 + 1)); p->ti = (double *)malloc(sizeof(double) * (p->M / 2 + 1));
    for (k = 0; k < p->M / 2; k++) { double a = -2.0 * M_PI * (double)k / (double)p->M; p->tr[k] = cos(a); p->ti[k] = sin(a); }
    if (!p->pow2) {
        p->wr = (double *)malloc(sizeof(double) * n); p->wi = (double *)malloc(sizeof(double) * n);
        p->br = (double *)calloc(p->M, sizeof(double)); p->bi = (double *)calloc(p->M, sizeof(double));
        for (k = 0; k < n; k++) {
            unsigned long long kk = ((unsigned long long)k * k) % (2ULL * n);
            double a = -M_PI * (double)kk / (double)n;
            p->wr[k] = cos(a); p->wi[k] = sin(a);
            p->br[k] = p->wr[k]; p->bi[k] = -p->wi[k];
            if (k) { p->br[p->M - k] = p->wr[k]; p->bi[p->M - k] = -p->wi[k]; }
        }
        fft2(p->br, p->bi, p->M, p);
    }
    return p;
}

void sl_fft(double *re, double *im, size_t n, int inverse) {
    plan *p; size_t k;
    if (n < 2) return;
    p = get_plan(n);
    if (inverse) for (k = 0; k < n; k++) im[k] = -im[k];
    if (p->pow2) fft2(re, im, n, p);
    else {
        size_t M = p->M;
        double *ar = (double *)calloc(M, sizeof(double)), *ai = (double *)calloc(M, sizeof(double));
        for (k = 0; k < n; k++) { ar[k] = re[k] * p->wr[k] - im[k] * p->wi[k]; ai[k] = re[k] * p->wi[k] + im[k] * p->wr[k]; }
        fft2(ar, ai, M, p);
        for (k = 0; k < M; k++) { double a = ar[k] * p->br[k] - ai[k] * p->bi[k], b = ar[k] * p->bi[k] + ai[k] * p->br[k]; ar[k] = a; ai[k] = -b; }  /* conj for the inverse */
        fft2(ar, ai, M, p);
        for (k = 0; k < n; k++) {
            double cr = ar[k] / (double)M, ci = -ai[k] / (double)M;
            re[k] = cr * p->wr[k] - ci * p->wi[k]; im[k] = cr * p->wi[k] + ci * p->wr[k];
        }
        free(ar); free(ai);
    }
    if (inverse) for (k = 0; k < n; k++) { re[k] /= (double)n; im[k] = -im[k] / (double)n; }
}

void sl_rfft(const double *x, size_t n, double *re, double *im) {
    double *a = (double *)malloc(sizeof(double) * n), *b = (double *)calloc(n, sizeof(double));
    size_t k;
    memcpy(a, x, sizeof(double) * n);
    sl_fft(a, b, n, 0);
    for (k = 0; k <= n / 2; k++) { re[k] = a[k]; im[k] = b[k]; }
    free(a); free(b);
}

void sl_rfft_power(const double *x, size_t n, double *pw) {
    size_t k; double *re = (double *)malloc(sizeof(double) * (n / 2 + 1)), *im = (double *)malloc(sizeof(double) * (n / 2 + 1));
    sl_rfft(x, n, re, im);
    for (k = 0; k <= n / 2; k++) pw[k] = re[k] * re[k] + im[k] * im[k];
    free(re); free(im);
}

void sl_lowpass_brick(const double *x, size_t n, double fc, int sr, double *out) {
    size_t k; double *a = (double *)malloc(sizeof(double) * n), *b = (double *)calloc(n, sizeof(double));
    memcpy(a, x, sizeof(double) * n);
    sl_fft(a, b, n, 0);
    for (k = 0; k <= n / 2; k++) {
        double f = (double)k / ((double)n * (1.0 / (double)sr));
        if (f > fc) {
            a[k] = 0; b[k] = 0;
            if (k && n - k != k) { a[n - k] = 0; b[n - k] = 0; }
        }
    }
    sl_fft(a, b, n, 1);
    memcpy(out, a, sizeof(double) * n);
    free(a); free(b);
}

/* ---------------- small statistics ---------------- */
double sl_hann(size_t i, size_t n) { return n < 2 ? 1.0 : 0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)(n - 1)); }

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

double sl_percentile(const double *x, size_t n, double q) {
    double *c, pos, r; size_t lo;
    if (!n) return NAN;
    c = (double *)malloc(sizeof(double) * n); memcpy(c, x, sizeof(double) * n);
    qsort(c, n, sizeof(double), cmpd);
    pos = q / 100.0 * (double)(n - 1); lo = (size_t)floor(pos);
    r = (lo + 1 < n) ? c[lo] + (c[lo + 1] - c[lo]) * (pos - (double)lo) : c[n - 1];
    free(c); return r;
}
double sl_median(const double *x, size_t n) { return sl_percentile(x, n, 50.0); }
double sl_db20(double v) { return 20.0 * log10(v > 1e-9 ? v : 1e-9); }
double sl_rms(const double *x, size_t n) { size_t i; double s = 0; if (!n) return 0; for (i = 0; i < n; i++) s += x[i] * x[i]; return sqrt(s / (double)n); }

double sl_band_pct(const double *x, size_t n, int sr, double lo, double hi) {
    size_t k, m = n / 2 + 1; double *pw, tot = 0, b = 0;
    if (n < 2) return 0;
    pw = (double *)malloc(sizeof(double) * m);
    sl_rfft_power(x, n, pw);
    for (k = 0; k < m; k++) {
        double f = (double)k / ((double)n * (1.0 / (double)sr));
        if (f >= 20) tot += pw[k];
        if (f >= lo && f < hi) b += pw[k];
    }
    free(pw);
    return 100.0 * b / (tot + 1e-30);
}

double sl_hf_db(const double *x, size_t n, int sr) {
    size_t i, k, m = n / 2 + 1; double *w, *pw, hf = 0, all = 0;
    if (n < 64) return NAN;
    w = (double *)malloc(sizeof(double) * n); pw = (double *)malloc(sizeof(double) * m);
    for (i = 0; i < n; i++) w[i] = x[i] * sl_hann(i, n);
    sl_rfft_power(w, n, pw);
    for (k = 0; k < m; k++) {
        double f = (double)k / ((double)n * (1.0 / (double)sr));
        all += pw[k]; if (f >= 2000) hf += pw[k];
    }
    free(w); free(pw);
    return 10.0 * log10((hf + 1e-30) / (all + 1e-30));
}

/* ---------------- loudness ---------------- */
static void biquad(const double *x, double *y, size_t n, const double *b, const double *a) {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0; size_t i;
    for (i = 0; i < n; i++) {
        double v = x[i], o = b[0] * v + b[1] * x1 + b[2] * x2 - a[0] * y1 - a[1] * y2;
        x2 = x1; x1 = v; y2 = y1; y1 = o; y[i] = o;
    }
}
static void kweight(const double *x, double *y, size_t n, int sr) {
    double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    double K = tan(M_PI * f0 / sr), Vh = pow(10.0, G / 20.0), Vb = pow(Vh, 0.4996667741545416);
    double a0 = 1 + K / Q + K * K, b[3], a[2], b2[3] = { 1.0, -2.0, 1.0 };
    double *t = (double *)malloc(sizeof(double) * n);
    b[0] = (Vh + Vb * K / Q + K * K) / a0; b[1] = 2 * (K * K - Vh) / a0; b[2] = (Vh - Vb * K / Q + K * K) / a0;
    a[0] = 2 * (K * K - 1) / a0; a[1] = (1 - K / Q + K * K) / a0;
    biquad(x, t, n, b, a);
    f0 = 38.13547087613982; Q = 0.5003270373253953;
    K = tan(M_PI * f0 / sr); a0 = 1 + K / Q + K * K;
    a[0] = 2 * (K * K - 1) / a0; a[1] = (1 - K / Q + K * K) / a0;
    biquad(t, y, n, b2, a);
    free(t);
}

double sl_lufs(const double *l, const double *r, size_t n, int sr) {
    size_t blk = (size_t)(0.4 * sr), hop = blk / 4, s, nb = 0, i, cap;
    double *zl, *zr, *ms, sum = 0, rel, res; size_t cnt = 0;
    if (n <= blk) return -99.0;
    zl = (double *)malloc(sizeof(double) * n); zr = (double *)malloc(sizeof(double) * n);
    kweight(l, zl, n, sr); kweight(r, zr, n, sr);
    cap = (n - blk) / hop + 2; ms = (double *)malloc(sizeof(double) * cap);
    for (s = 0; s + blk < n; s += hop) {
        double a = 0, b = 0;
        for (i = s; i < s + blk; i++) { a += zl[i] * zl[i]; b += zr[i] * zr[i]; }
        ms[nb++] = a / (double)blk + b / (double)blk;
    }
    free(zl); free(zr);
    for (i = 0; i < nb; i++) if (-0.691 + 10 * log10(ms[i] + 1e-30) > -70) { sum += ms[i]; cnt++; }
    if (!cnt) { free(ms); return -99.0; }
    rel = -0.691 + 10 * log10(sum / (double)cnt) - 10;
    sum = 0; cnt = 0;
    for (i = 0; i < nb; i++) if (-0.691 + 10 * log10(ms[i] + 1e-30) > rel) { sum += ms[i]; cnt++; }
    res = cnt ? -0.691 + 10 * log10(sum / (double)cnt) : -99.0;
    free(ms); return res;
}

/* 4x oversampling with a Kaiser-windowed sinc (32 taps a phase); the old script zero-padded the whole spectrum (ideal band-limited
 * interpolation), the difference is a few hundredths of a dB on music. */
static double bessel_i0(double x) { double s = 1, t = 1; int k; for (k = 1; k < 40; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
double sl_true_peak(const double *l, const double *r, size_t n) {
    enum { T = 16 };
    double h[3][2 * T], pk = 0; int ph, k; const double *ch[2]; int c; size_t i;
    double beta = 9.0, i0b = bessel_i0(beta);
    for (ph = 1; ph <= 3; ph++) {
        for (k = -T + 1; k <= T; k++) {
            double t = (double)k - ph / 4.0, w, s;
            double u = t / (double)T;
            w = fabs(u) < 1 ? bessel_i0(beta * sqrt(1 - u * u)) / i0b : 0;
            s = fabs(t) < 1e-12 ? 1.0 : sin(M_PI * t) / (M_PI * t);
            h[ph - 1][k + T - 1] = s * w;
        }
    }
    ch[0] = l; ch[1] = r;
    for (c = 0; c < 2; c++) {
        const double *x = ch[c];
        for (i = 0; i < n; i++) {
            double a = fabs(x[i]);
            if (a > pk) pk = a;
            for (ph = 0; ph < 3; ph++) {
                double y = 0;
                for (k = -T + 1; k <= T; k++) {
                    long j = (long)i + k;
                    if (j >= 0 && (size_t)j < n) y += x[j] * h[ph][k + T - 1];
                }
                if (fabs(y) > pk) pk = fabs(y);
            }
        }
    }
    return pk;
}

/* ---------------- wav ---------------- */
static unsigned rd16(const unsigned char *p) { return p[0] | (unsigned)p[1] << 8; }
static unsigned long rd32(const unsigned char *p) { return p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24; }

int sl_wav_read(const char *fn, sl_wav *w) {
    FILE *f = fopen(fn, "rb"); unsigned char *b; long sz; size_t pos = 12, i;
    int fmt = 0, ch = 0, bits = 0, sr = 0; const unsigned char *data = NULL; size_t dlen = 0;
    memset(w, 0, sizeof *w);
    if (!f) return -1;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    b = (unsigned char *)malloc((size_t)sz + 8);
    if (fread(b, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(b); return -1; }
    fclose(f);
    if (sz < 12 || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) { free(b); return -2; }
    while (pos + 8 <= (size_t)sz) {
        size_t cl = rd32(b + pos + 4);
        if (!memcmp(b + pos, "fmt ", 4)) { fmt = (int)rd16(b + pos + 8); ch = (int)rd16(b + pos + 10); sr = (int)rd32(b + pos + 12); bits = (int)rd16(b + pos + 22); }
        else if (!memcmp(b + pos, "data", 4)) {
            data = b + pos + 8; dlen = cl;
            if (pos + 8 + dlen > (size_t)sz || dlen == 0 || dlen == 0xFFFFFFFFu) dlen = (size_t)sz - pos - 8;
            break;
        }
        pos += 8 + cl + (cl & 1);
    }
    if (!data || (fmt != 1 && fmt != 0xFFFE) || bits != 16 || (ch != 1 && ch != 2)) { free(b); return -3; }
    w->sr = sr; w->n = dlen / (size_t)(2 * ch);
    w->l = (double *)malloc(sizeof(double) * (w->n + 1)); w->r = (double *)malloc(sizeof(double) * (w->n + 1));
    for (i = 0; i < w->n; i++) {
        short a = (short)rd16(data + i * 2 * (size_t)ch), c2 = ch == 2 ? (short)rd16(data + i * 4 + 2) : a;
        w->l[i] = a / 32768.0; w->r[i] = c2 / 32768.0;
    }
    free(b); return 0;
}
void sl_wav_free(sl_wav *w) { free(w->l); free(w->r); memset(w, 0, sizeof *w); }
