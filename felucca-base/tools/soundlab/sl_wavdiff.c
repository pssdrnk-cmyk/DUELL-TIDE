/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab wavdiff A.wav B.wav: how much two renders differ (tools/soundlab/deadparams.py asks this of every min / max pair).
 * Prints one line:  same <rms dBFS> | diff <samples differing> <rms dB change> <centroid change in octaves> <max sample difference dBFS> <louder file's mean-square dBFS>
 * RMS over both channels; the centroid is the power-weighted mean frequency of the mono sum (first 65536 samples, Hann), 20 Hz and up. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sl_lab.h"

static double centroid(const double *x, size_t n, int sr)
{
    size_t N = 65536, i, k;
    double *w = (double *)calloc(N, sizeof(double)), *pw = (double *)malloc(sizeof(double) * (N / 2 + 1)), num = 0, den = 0;
    for (i = 0; i < N && i < n; i++)
        w[i] = x[i] * sl_hann(i, n < N ? n : N);
    sl_rfft_power(w, N, pw);
    for (k = 1; k <= N / 2; k++) {
        double f = (double)k * sr / (double)N;
        if (f < 20.0)
            continue;
        num += f * pw[k];
        den += pw[k];
    }
    free(w);
    free(pw);
    sl_fft_flush();
    return den > 1e-18 ? num / den : 0.0;
}

int sl_wavdiff_main(int argc, char **argv)
{
    sl_wav a, b;
    size_t i, n, nd = 0;
    double ea = 0, eb = 0, mx = 0, *ma, *mb, ca, cb;
    if (argc < 3) {
        fprintf(stderr, "usage: soundlab wavdiff A.wav B.wav\n");
        return 2;
    }
    if (sl_wav_read(argv[1], &a) || sl_wav_read(argv[2], &b)) {
        fprintf(stderr, "soundlab wavdiff: cannot read the files\n");
        return 1;
    }
    n = a.n < b.n ? a.n : b.n;
    ma = (double *)malloc(sizeof(double) * (n + 1));
    mb = (double *)malloc(sizeof(double) * (n + 1));
    for (i = 0; i < n; i++) {
        double dl = a.l[i] - b.l[i], dr = a.r[i] - b.r[i], d = fabs(dl) > fabs(dr) ? fabs(dl) : fabs(dr);
        if (d > 0)
            nd++;
        if (d > mx)
            mx = d;
        ea += a.l[i] * a.l[i] + a.r[i] * a.r[i];
        eb += b.l[i] * b.l[i] + b.r[i] * b.r[i];
        ma[i] = 0.5 * (a.l[i] + a.r[i]);
        mb[i] = 0.5 * (b.l[i] + b.r[i]);
    }
    if (a.n != b.n)
        nd += a.n > b.n ? a.n - b.n : b.n - a.n;
    if (!nd) {
        printf("same %.1f\n", 10.0 * log10(ea / (double)(n ? n : 1) + 1e-18));
        return 0;
    }
    ca = centroid(ma, n, a.sr);
    cb = centroid(mb, n, b.sr);
    printf("diff %zu %.3f %.3f %.1f %.1f\n", nd, 10.0 * log10((eb + 1e-18) / (ea + 1e-18)), (ca > 0 && cb > 0) ? log2(cb / ca) : 0.0, 20.0 * log10(mx + 1e-9), 10.0 * log10((ea > eb ? ea : eb) / (double)(n ? n : 1) + 1e-18));
    return 0;
}
