/* SPDX-License-Identifier: GPL-3.0-only */
/* The delay loop's feedback colour and its dub runaway (fx.c dly_wr, DLY_FBQ; audit E2, owner D21). On hostsim.c as dlyglide_test.c (the shared bus alone,
 * DIGITAL, TIME 1/8 at 120 BPM = 0.25 s, COLR 127, MIX 127):
 * 1. LOW CUT   a 0.1 s burst at FDBK 100 (0.70): the third repeat against the first, a 1 kHz tone loses 2 passes of 0.70 (-6.2 dB); a 60 Hz tone must lose at
 *              least 12 dB more (the loop's low cut of about 165 Hz takes 9 dB a pass; before it, the two were alike)
 * 2. RUNAWAY   FDBK 120 (1.05): a 1 kHz burst and then nothing for 12 s: the loop is still sounding in the last second (rms above 3000), held under the line's full
 *              scale by the knee in the loop (peak under 65536 in the output); FDBK 100 dies out (the last second is 0)
 *   build/host/dlyfb_test   (host_tests.sh "dlyfb") */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdio.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("dlyfb: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define SECS 13u
static float outbuf[SECS * FS];
/* a burst of freq Hz (amplitude amp, 0.1 s) at 0.5 s, then silence; the delay's wet in outbuf */
static void run(int fdbk, double freq, double amp)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t nb = SECS * FS / CTL, b, i, t = 0;
    host_tracks_init();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    dlyev_clear();
    dly_plain_pp = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DFDBK] = (int16_t)fdbk;
    song.g[G_DCOLOR] = 127;
    song.g[G_DMIX] = 127;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_DMDEP] = 0;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    midi_beat_samples = 0;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++, t++) {
            double x = t >= FS / 2u && t < FS / 2u + FS / 10u ? sin(2. * M_PI * freq * (t - FS / 2u) / FS) * amp : 0.;
            c[i] = r[i] = 0;
            d[i] = (int32_t)x;
            mix_l[i] = mix_r[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        if (dly_side) dly_side_mix(mix_l, mix_r, CTL);
        for (i = 0; i < CTL; i++)
            outbuf[b * CTL + i] = (float)(mix_l[i] + w[i] + mix_r[i] + w[i]) * .5f;
    }
}
/* the rms of the repeat k (k = 1: the burst's first echo, 0.25 s after it started) */
static double rep_rms(uint32_t k)
{
    double s = 0;
    uint32_t i, a = FS / 2u + k * (FS / 4u), n = FS / 10u;
    for (i = a; i < a + n; i++) s += (double)outbuf[i] * outbuf[i];
    return sqrt(s / n);
}
static double last_rms(double *pk)
{
    double s = 0, m = 0;
    uint32_t i, a = (SECS - 1u) * FS;
    for (i = a; i < SECS * FS; i++) {
        s += (double)outbuf[i] * outbuf[i];
        m = fabs(outbuf[i]) > m ? fabs(outbuf[i]) : m;
    }
    *pk = m;
    return sqrt(s / FS);
}
int main(void)
{
    double hi, lo, rms, pk;
    char w[160];
    run(100, 1000., 8000.);
    hi = 20. * log10(rep_rms(3) / rep_rms(1));
    run(100, 60., 8000.);
    lo = 20. * log10(rep_rms(3) / rep_rms(1));
    snprintf(w, sizeof w, "LOW CUT: repeat 3 against repeat 1 at FDBK 100: 1 kHz %.1f dB, 60 Hz %.1f dB (60 Hz at least 12 dB lower)", hi, lo);
    check(w, lo < hi - 12.);
    run(120, 1000., 8000.);
    rms = last_rms(&pk);
    snprintf(w, sizeof w, "RUNAWAY: FDBK 120, 12 s after the burst: rms %.0f, peak %.0f (rms above 3000, peak under 65536)", rms, pk);
    check(w, rms > 3000. && pk < 65536.);
    run(100, 1000., 8000.);
    rms = last_rms(&pk);
    snprintf(w, sizeof w, "FDBK 100 dies out: the last second's peak %.0f (0)", pk);
    check(w, pk == 0.);
    printf(bad ? "dlyfb: FAIL\n" : "dlyfb: ok\n");
    return bad != 0;
}
