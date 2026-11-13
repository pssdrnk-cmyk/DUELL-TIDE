/* SPDX-License-Identifier: GPL-3.0-only */
/* RESO's combs ring at the notes of the scale whatever DAMP is (fx_bus.c rev_reso; audit R04). Each comb's loop holds a one-pole low-pass whose group delay,
 * a / (1 - a) samples at its DC, adds to the period: without taking it off, the combs went flat as DAMP rose (-26 cents at DAMP 120).
 * On hostsim.c as dlyglide_test.c: an impulse into the reverb send, RESO at SIZE 127 (long rings, narrow lines), MAJ with the root C: the combs C3 E3 G3 B3 (130.81 164.81 196.00 246.94 Hz), the
 * peak of a windowed spectrum of the ringing tail, at DAMP 0, 60, 90 and 120: every one within 2 cents.
 *   build/host/resotune_test   (host_tests.sh "resotune") */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdio.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("resotune: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define LEN 12288u   /* (0.28 s: audit R08 closed the loops' rounding dead bands, so the integer noise that follows the ring comes earlier at DAMP 120) */
static float tail[LEN];
/* an impulse through RESO; the next 0.28 s of the ring (later it falls to the integer noise of the loop) are kept */
static void run(int damp)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t nb = 64u + LEN / CTL + 2u, b, i, n = 0;
    host_tracks_init();
    memset(&fx, 0, sizeof fx);
    memset(rev_comb, 0, sizeof rev_comb);
    song.g[G_RTYPE] = 4;
    song.g[G_RSIZE] = 127;
    song.g[G_RDAMP] = (int16_t)damp;
    song.g[G_RLOCUT] = 0;
    scale_g = 1;
    root_g = 0;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = b == 64u && i == 0 ? 150000 : 0;
            mix_l[i] = mix_r[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            uint32_t t = b * CTL + i;
            if (t >= 64u * CTL && n < LEN)
                tail[n++] = (float)(mix_l[i] + w[i]);
        }
    }
}
/* |DFT|^2 of the Hann-windowed ring at f */
static double mag(double f)
{
    double re = 0, im = 0, ph = 0, dp = 2. * M_PI * f / FS;
    uint32_t i;
    for (i = 0; i < LEN; i++) {
        double x = tail[i] * (0.5 - 0.5 * cos(2. * M_PI * i / LEN));
        re += x * cos(ph);
        im -= x * sin(ph);
        ph += dp;
    }
    return re * re + im * im;
}
/* the peak within +-3 % of f0, found coarse (0.4 Hz) then fine (0.01 Hz) */
static double peak(double f0)
{
    double f, best = f0, m, bm = -1;
    for (f = f0 * .97; f < f0 * 1.03; f += .4)
        if ((m = mag(f)) > bm) { bm = m; best = f; }
    for (f = best - .4, bm = -1; f < best + .4; f += .01)
        if ((m = mag(f)) > bm) { bm = m; best = f; }
    return best;
}
int main(void)
{
    static const double want[4] = {130.81, 164.81, 196.00, 246.94};
    static const int damp[4] = {0, 60, 90, 120};
    uint32_t a, k;
    char w[160];
    for (a = 0; a < 4u; a++) {
        double worst = 0;
        char s[64];
        s[0] = 0;
        run(damp[a]);
        for (k = 0; k < 4u; k++) {
            double ct = 1200. * log2(peak(want[k]) / want[k]);
            sprintf(s + strlen(s), " %+.1f", ct);
            worst = fabs(ct) > worst ? fabs(ct) : worst;
        }
        sprintf(w, "RESO MAJ / C, DAMP %3d: the combs C3 E3 G3 B3 are off by%s cents (at most 2)", damp[a], s);
        check(w, worst <= 2.);
    }
    printf("RESO TUNE: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
