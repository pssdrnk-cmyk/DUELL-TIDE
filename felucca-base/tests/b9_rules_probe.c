/* SPDX-License-Identifier: GPL-3.0-only */
/* b9 musical rules probe (work/b9/MUSICAL_RULES_B9.md rules 6, 7, 8, 9) for one factory multi, in this process (no fork; run it once a case):
 *   b9_rules_probe SLOT CASE
 *   CASE a   End A  (both MORPH faders 0), PLAY / the arps' chord held, 10 s, 2..10 s measured
 *        m   the middle (faders 63)
 *        b   End B (faders 127)
 *        w   TIDE as the multi's STYLE runs it (WANDER where the style says so), glide launched, 16 bars, PITCH ON, the chord held
 * Prints: K-weighted level, sample peak and 4x true peak (dBFS), the under-150 Hz share, the low band's onsets per beat, and the whistle test: in 4096-point frames (hop 2048)
 * a spectral peak above 2 kHz more than 12 dB over the median of the bins 150 - 400 Hz either side of it, followed from frame to frame; the longest run in frames (1 = 43 ms)
 * and the largest excess. The master output (after MASTER and the limiter) is measured, as `gen_factory_multis loud` does. */

#include "trt_nomain.inc"   /* (build/host: tests/tide_roles_test.c with its main renamed: sed -e "s/^int main(void)/int tideroles_main(void)/") */

#define LO_HELPERS_ONLY 1
#include "lowend_probe.inc"

#define PR_N 4096u
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j = 0, k, len;
    for (i = 1; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (k = 0; k < len / 2; k++) {
                double wr = cos(ang * k), wi = sin(ang * k), ur = re[i + k], ui = im[i + k];
                double vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi, vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
                re[i + k] = ur + vr, im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr, im[i + k + len / 2] = ui - vi;
            }
    }
}
static int cmpd2(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static float *mono;
static uint32_t mono_n;
static void whistle(double *worst_db, uint32_t *longest, double *at_hz)
{
    static double re[PR_N], im[PR_N], db[PR_N / 2];
    static uint32_t run[PR_N / 2];
    uint32_t f, k, i;
    double bin_hz = (double)FS / PR_N;
    *worst_db = 0, *longest = 0, *at_hz = 0;
    memset(run, 0, sizeof run);
    for (f = 0; f + PR_N <= mono_n; f += PR_N / 2u) {
        uint32_t nrun[PR_N / 2];
        memset(nrun, 0, sizeof nrun);
        for (i = 0; i < PR_N; i++) {
            double w = 0.5 - 0.5 * cos(2.0 * M_PI * i / PR_N);
            re[i] = mono[f + i] * w, im[i] = 0;
        }
        fft(re, im, PR_N);
        for (k = 0; k < PR_N / 2u; k++)
            db[k] = 10.0 * log10(re[k] * re[k] + im[k] * im[k] + 1e-20) - 20.0 * log10(PR_N / 4.0);   /* (dBFS of a full-scale sine = 0) */
        for (k = (uint32_t)(2000.0 / bin_hz); k < (uint32_t)(16000.0 / bin_hz); k++) {
            double nb[256];
            uint32_t n = 0, lo = (uint32_t)(150.0 / bin_hz), hi = (uint32_t)(400.0 / bin_hz), d;
            if (db[k] < db[k - 1] || db[k] < db[k + 1] || db[k] < -80.0)
                continue;
            for (d = lo; d <= hi && n < 250u; d++) {
                nb[n++] = db[k - d];
                nb[n++] = db[k + d];
            }
            qsort(nb, n, sizeof *nb, cmpd2);
            double ex = db[k] - nb[n / 2];
            if (ex > 12.0) {
                uint32_t r = 1;
                for (d = k > 2 ? k - 2 : 0; d <= k + 2; d++)
                    if (run[d] + 1 > r)
                        r = run[d] + 1;
                for (d = k > 2 ? k - 2 : 0; d <= k + 2; d++)
                    nrun[d] = nrun[d] > r ? nrun[d] : r;
                if (r > *longest || (r == *longest && ex > *worst_db))
                    *longest = r, *worst_db = ex, *at_hz = k * bin_hz;
            }
        }
        memcpy(run, nrun, sizeof run);
    }
}
int main(int argc, char **argv)
{
    uint32_t slot = argc > 2 ? (uint32_t)atoi(argv[1]) - 1u : 0u, row, b, i, c, nb, b0 = (uint32_t)(2.0 * FS) / CTL, n = 0, w = 0, wn = 0, nw, nbars, bar, s, macc = 0;
    char cs = argc > 2 ? argv[2][0] : 'a';
    double bpm, wacc = 0, lmax = 1e-30, elow = 0, efull = 0, spk = 0, tpk = 0, slotv[16], other = 0, prev[4] = {0, 0, 0, 0};
    static int32_t out[2 * CTL];
    lz_kw_t kw;
    lo_bq_t l1, l2;
    for (row = 0; row < 16u && TR[row].slot != slot + 1u; row++)
        ;
    if (row == 16u)
        return 2;
    load(slot);
    song.master_q12 = 2048;
    bpm = song.g[G_BPM] ? (double)song.g[G_BPM] : 120.0;
    nw = (uint32_t)(FS * 60.0 / bpm / 4.0);
    nb = cs == 'w' ? (uint32_t)((16.0 * 4.0 * 60.0 / bpm + 2.0) * FS) / CTL : (uint32_t)(10.0 * FS) / CTL;
    if (cs == 'w')
        b0 = (uint32_t)(2.0 * FS) / CTL;
    mono = (float *)calloc((size_t)nb * CTL, sizeof *mono);
    lz_kw_init(&kw);
    lo_lp(&l1, 150.0, 0.5412, FS);
    lo_lp(&l2, 150.0, 1.3066, FS);
    for (i = 0; i < 8u; i++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
    }
    if (cs == 'm' || cs == 'b' || cs == 'a')
        morph_ready(0)->fader = morph_ready(1)->fader = cs == 'a' ? 0 : cs == 'm' ? 63 : 127;
    fm1_in.notes = chord_mask(KEYS_MIN, 3u);
    if (cs == 'w') {
        tide_style_apply(TR[row].style);
        gl.target = GT_BOTH;
        morph_musical = 1;
        morph.cfg |= MC_PIT;
        glide_launch(1u, 3u, 0u);
        printf("style %u mode %u (3 = WANDER)\n", (unsigned)TR[row].style, (unsigned)gl.mode);
    } else {
        transport_req = 1;
    }
    for (b = 0; b < nb; b++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
        macc += 1000u * CTL;
        if (macc >= (uint32_t)FS * 16u) {
            macc -= (uint32_t)FS * 16u;
            frame();
        }
        for (i = 0; i < CTL; i++) {
            double m = (out[2u * i] + out[2u * i + 1u]) / 65534.0, lo = lo_run(&l2, lo_run(&l1, m));
            wacc += lo * lo;
            if (++wn >= nw) {
                if (w < LO_WIN)
                    lo_env[w++] = wacc / nw;
                wacc = 0, wn = 0;
            }
            if (b < b0)
                continue;
            for (c = 0; c < 2u; c++) {
                double x = out[2u * i + c] / 32767.0;
                lz_kw_push(&kw, c, x);
                if (fabs(x) > spk)
                    spk = fabs(x);
            }
            elow += lo * lo, efull += m * m;
            mono[mono_n++] = (float)m;
        }
    }
    fm1_in.notes = 0;
    /* true peak: 4x by Catmull-Rom on each channel is not kept (only the mono sum is); use the mono sum's 4x as an estimate and the channel samples' maximum above: the limiter
     * keeps both channels under the same ceiling, so the mono estimate is a lower bound; report both */
    for (i = 3; i + 1 < mono_n; i++)
        for (c = 1; c < 4u; c++) {
            double t = c / 4.0, p0 = mono[i - 2], p1 = mono[i - 1], p2 = mono[i], p3 = mono[i + 1];
            double v = 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t);
            if (fabs(v) > tpk)
                tpk = fabs(v);
        }
    for (i = 0; i < w; i++)
        lmax = lo_env[i] > lmax ? lo_env[i] : lmax;
    nbars = w / 16u;
    for (s = 0; s < 16u; s++)
        slotv[s] = 0;
    for (bar = 0; bar < nbars; bar++)
        for (s = 0; s < 16u; s++) {
            uint32_t at = bar * 16u + s;
            double cur = lo_env[at], pv = at ? lo_env[at - 1u] : 0.0;
            if (cur > 4.0 * pv && cur > lmax * 1e-3 && cur > 1e-12)
                slotv[s] += 1.0;
        }
    for (s = 0; s < 16u; s++) {
        slotv[s] = nbars ? slotv[s] / nbars : 0.0;
        if (s % 4u)
            other += slotv[s];
    }
    (void)prev; (void)n;
    {
        double wdb; uint32_t wl; double whz;
        whistle(&wdb, &wl, &whz);
        printf("M%02u %-12s case %c: K-weighted %6.1f dB | peak %6.2f dBFS, 4x estimate %6.2f dBFS (mono sum) | under150 %5.1f %% | low onsets/beat b1 %.2f b2 %.2f b3 %.2f b4 %.2f other %.2f | whistle: longest %u frames, %.1f dB over its neighbours at %.0f Hz\n",
               slot + 1u, TR[row].name, cs, lz_kw_db(&kw), 20 * log10(spk + 1e-12), 20 * log10(tpk + 1e-12), efull > 0 ? 100.0 * elow / efull : 0.0, slotv[0], slotv[4], slotv[8], slotv[12], other / 12.0, wl, wdb, whz);
    }
    return 0;
}
