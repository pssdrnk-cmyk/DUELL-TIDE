/* SPDX-License-Identifier: GPL-3.0-only */
/* The reverb and delay loops end in exact silence (audit R08): a one-pole or a loop gain that rounds to the nearest (or floors) has a dead band: a few LSB where "o + round(d k)" gives d back,
 * and the loop holds that DC for ever (ROOM at DAMP 124 up held 38 .. 40 LSB, RESO at DAMP 127 64, the delay loop 2 .. 90 at COLR 0 .. 127). fx.c mulq15d / mulq15t / mulq15u / sh_tzl take the
 * last LSB off, so every loop falls to 0. On hostsim.c as resotune_test.c: a burst into the reverb send (ROOM at DAMP 60 / 124 / 127, RESO at DAMP 60 / 127) or the delay send (DIGITAL, PING-PONG,
 * RANDOM, OCT UP; FDBK 60 / 80 / 100; COLOR 0 / 40 / 127; a sine and noise), 30 s of silence after it, and the last second of the return must be exact zeros.
 *   build/host/dctail_test   (host_tests.sh "dctail") */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdio.h>

static int bad, ncase, nbad_case;
static void check(const char *what, int ok)
{
    printf("dctail: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
/* one case: the burst for `dur` s from block 64, `secs` in all; returns the number of non-zero samples in the last second and the largest |sample| there */
static uint32_t run(uint32_t bus, const char *sig, double amp, double dur, double secs, int32_t *mx)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t nb = (uint32_t)(secs * FS / CTL), b, i, nz = 0, rng = 12345;
    double ph = 0;
    *mx = 0;
    dm_lines_clear();
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++) {
            double x = 0;
            rng = rng * 1664525u + 1013904223u;
            if (b >= 64u && (double)(b - 64u) * CTL / FS < dur)
                x = sig[0] == 's' ? sin(ph) * amp : ((double)(int32_t)rng / 2147483648.0) * amp * 1.732;
            ph += 2. * M_PI * 330. / FS;
            c[i] = 0;
            d[i] = bus == 1u ? (int32_t)x : 0;
            r[i] = bus == 2u ? (int32_t)x : 0;
            mix_l[i] = mix_r[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        if (hl.side)
            rev_side_mix(mix_l, mix_r, CTL);
        if (dly_side)
            dly_side_mix(mix_l, mix_r, CTL);
        if (b >= nb - FS / CTL)
            for (i = 0; i < CTL; i++) {
                int32_t l = mix_l[i] + w[i], rr = mix_r[i] + w[i];
                nz += (l != 0) + (rr != 0);
                *mx = abs(l) > *mx ? abs(l) : *mx;
                *mx = abs(rr) > *mx ? abs(rr) : *mx;
            }
    }
    return nz;
}
static void setup(int rtype, int damp)
{
    host_tracks_init();
    song.g[G_RTYPE] = (int16_t)rtype;
    song.g[G_RSIZE] = 100;
    song.g[G_RDAMP] = (int16_t)damp;
    song.g[G_RLOCUT] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DMIX] = 127;
    scale_g = 1;
    root_g = 0;
}
int main(void)
{
    static const int RD[3] = {60, 124, 127};
    uint32_t k, nz;
    int32_t mx;
    int fb, col, mode, sg;
    char w[160];
    for (k = 0; k < 3u; k++) {
        setup(0, RD[k]);
        nz = run(2, "noise", 8000, 0.05, 30, &mx);
        sprintf(w, "ROOM, DAMP %3d, a burst into the reverb send: the last second is %u non-zero samples (largest %d)", RD[k], nz, (int)mx);
        check(w, nz == 0);
    }
    for (k = 0; k < 3u; k += 2u) {
        setup(4, RD[k]);
        nz = run(2, "noise", 8000, 0.05, 30, &mx);
        sprintf(w, "RESO, DAMP %3d, a burst into the reverb send: the last second is %u non-zero samples (largest %d)", RD[k], nz, (int)mx);
        check(w, nz == 0);
    }
    for (mode = 0; mode < 4; mode++)
        for (fb = 60; fb <= 100; fb += 20)
            for (col = 0; col <= 127; col += (col < 40 ? 40 : 87))
                for (sg = 0; sg < 2; sg++) {
                    setup(0, 60);
                    song.g[G_DFDBK] = (int16_t)fb;
                    song.g[G_DCOLOR] = (int16_t)col;
                    song.g[G_DMODE] = (int16_t)mode;
                    nz = run(1, sg ? "noise" : "sine", 8000, sg ? 0.5 : 1.0, 30, &mx);
                    ncase++;
                    nbad_case += nz != 0;
                    if (nz)
                        printf("dctail:   delay mode %d FDBK %d COLOR %d %s: %u non-zero samples in the last second (largest %d)\n", mode, fb, col, sg ? "noise" : "sine", nz, (int)mx);
                }
    sprintf(w, "the delay loop (DIGITAL, PING-PONG, RANDOM, OCT UP x FDBK 60 / 80 / 100 x COLOR 0 / 40 / 127 x sine / noise): %d of %d cases fall to exact silence", ncase - nbad_case, ncase);
    check(w, nbad_case == 0);
    printf("DC TAIL: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
