/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of SWING (issue #31), same sources as the firmware (through hostsim.c):
 *   build/host/swing_test
 * SWG (the track's, the global one, the arp's) is stored and shown as 50 .. 75 %, 50 = straight.
 * 1. the sequencer: the second step of a pair starts the percentage SWG shows of the way through the pair, measured from the
 *    transport; the pair always adds up to two straight steps; the track's and the global SWG add (their parts over 50), at most 75 %.
 * 2. the display: SWG shows its percentage, 50 .. 75; 0..127 percents unchanged. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int check(const char *what, int ok)
{
    printf("swing: %-75s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void set_swing(track_t *t, int own, int global)
{
    t->p[P_SSWING] = (int16_t)own;
    song.g[G_SWING] = (int16_t)global;
}

#define SWING_SCALE 1.0          /* percent of the pair the offbeat moves per percent of SWG over 50 */

/* where the offbeat of a pair lands, in percent of the pair, for a track SWG and a global SWG (50 = straight) */
static double play_pct(int own, int global)
{
    int ex = (own > 50 ? own - 50 : 0) + (global > 50 ? global - 50 : 0);
    return 50.0 + (ex > 25 ? 25 : ex) * SWING_SCALE;
}

/* ----------------------------------------------------- 1. sequencer --- */
/* the sample where each step of track 1 starts (block exact: CTL), n steps from the transport start */
static uint32_t seq_starts(uint32_t *at, uint32_t n)
{
    uint32_t f, got = 0, last = 0xFFFFFFFFu;
    transport_req = 1;
    for (f = 0; got < n && f < 20u * FS; f += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        if (trk[0].seq_idx != last) {
            last = trk[0].seq_idx;
            at[got++] = f;
        }
    }
    transport_req = 2;
    {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
    return got;
}

static int test_seq(void)
{
    static const int C[][2] = {{50, 50}, {62, 50}, {66, 50}, {75, 50}, {50, 75}, {60, 60}, {75, 75}, {70, 70}};
    uint32_t c, at[17];
    int bad = 0;
    char what[160];
    for (c = 0; c < sizeof C / sizeof C[0]; c++) {
        uint32_t base, k, got, err = 0;
        double mid = 0.0;
        host_tracks_init();
        song.g[G_BPM] = 120;
        trk[0].p[P_SLEN] = 16;
        set_swing(&trk[0], C[c][0], C[c][1]);
        base = div_samples((uint32_t)trk[0].p[P_SDIV]);
        got = seq_starts(at, 17);
        for (k = 1; k + 1 < got; k += 2) {                        /* step k is the offbeat of the pair k-1, k */
            double pos = 100.0 * (double)(at[k] - at[k - 1]) / (double)(at[k + 1] - at[k - 1]);
            err += fabs(pos - play_pct(C[c][0], C[c][1])) > 0.7;
            mid += pos;
        }
        err += abs((int32_t)(at[16] - at[0]) - 16 * (int32_t)base) > (int32_t)CTL;
        snprintf(what, sizeof what, "sequencer: SWG %d + global %d: the offbeat lands at %.1f %% of the pair (%.1f %%)", C[c][0], C[c][1],
                 mid / 8.0, play_pct(C[c][0], C[c][1]));
        bad += check(what, got == 17 && !err);
    }
    /* the extreme: 75 % (the offbeat at 75 %: 1.5 x and 0.5 x), never past, even with both at 75 */
    host_tracks_init();
    set_swing(&trk[0], 75, 75);
    bad += check("step_samples at 75 + 75: 1.5 x and 0.5 x of the straight step",
                 step_samples(&trk[0], 6000, 0) == (uint32_t)(2.0 * play_pct(75, 75) * 60.0) &&
                 step_samples(&trk[0], 6000, 1) == 12000u - (uint32_t)(2.0 * play_pct(75, 75) * 60.0));
    return bad;
}

/* -------------------------------------------------------- 2. display --- */
static const char *shown(const param_desc_t *d, int32_t v)
{
    static char val[PV_LEN];
    const char *unit;
    param_format(d, v, val, &unit);
    return val;
}

static int test_display(void)
{
    int ok = 1;
    const param_desc_t *sw[3] = {&TP[P_SSWING], &GP[G_SWING], &TP[P_ASWING]};
    uint32_t k;
    for (k = 0; k < 3u; k++)
        ok &= sw[k]->fmt == F_PCT && sw[k]->min == 50 && sw[k]->max == 75 && !strcmp(shown(sw[k], 75), "75") &&
              !strcmp(shown(sw[k], 62), "62") && !strcmp(shown(sw[k], 50), "50");
    ok &= !strcmp(shown(&TP[P_SUS], 127), "100") && !strcmp(shown(&TP[P_SUS], 64), "50") &&
          !strcmp(shown(&TP[P_DLY], 0), "0");
    return check("display: SWG / global SWG / ARP SWG 75 -> 75 %, 50 -> 50 %; 0..127 percents unchanged", ok);
}

int main(void)
{
    int bad = test_seq() + test_display();
    printf("swing: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
