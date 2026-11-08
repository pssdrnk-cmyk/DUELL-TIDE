/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE 1.0.0-b5 (CPU audit K5): the silent bus and master skip (fx.c sil_dly / sil_mst, fx_bus.c fx_buses_m, mix_master). The plain delay loop is left out of a
 * block whose delay input is 0 while the whole line reads back 0 and the loop's states are 0; the master's DC blocker, limiter and knee are left out of a block whose
 * mix is 0 while the DC blockers are at 0 and the limiters' envelopes back at LIM_T. It must be INVISIBLE: bit for bit what the loops give, whenever and wherever.
 * This file is built twice (host_tests.sh "silentskip"): as it is (the skip on) and with -DSIL_NOSKIP (the firmware without it). Both run the same scenario and
 *   write every output sample (int32, L R interleaved) to the file named by the first argument: the script compares the two files byte for byte;
 *   print HASH lines (the outputs, and every 16 blocks the delay lines and the delay / master states) and TAIL lines (per tail: the block it starts at, the
 *   sample index of the last output sample that is not 0, left and right): the script compares those lists too, so a tail cut early shows as a TAIL line that moved;
 *   count the blocks the skip took (SIL_SKIP_STATS): the skip build must have left the delay loop and the master out of many blocks (idle, tails ended), the
 *   reference of none. The skip build also prints, per tail, where its last sample not 0 is and where the master first rested (the master may rest inside a
 *   tail, between two delay repeats whose gap is exact silence: that is the same zeros; the files' equality is the proof).
 *   SCENES (STEREO, LAYER, DUEL, and STEREO with PING-PONG): silence from power-on (the master rests at once, the delay after its line's length of zeros), a chord
 *             with every send (chorus, delay with FDBK 90, HALL) and its tails into silence, a note the block after the skip took over, a loud chord that drives the
 *             limiter then silence (the envelope's release, then the rest), a low note (the DC blocker's tail), a TIME change during the rest (the slide), the
 *             delay's modulation and RANDOM (the other paths, then back), SENDS OFF and ON, a MODE change, short notes with gaps either side of the line's length
 *   FUZZ      8 s of random events per mode with gaps up to 3 s
 *   COST      the host's wall clock of 60 s of rest, printed (not compared: the unit's own meter is the measure) */
#define MODE_TEST_NO_MAIN 1
#define SIL_SKIP_STATS 1
#include "mode_test.c"
#include <time.h>

static uint64_t ss_fnv(const void *p, uint32_t bytes, uint64_t h)
{
    const uint8_t *b = p;
    for (uint32_t i = 0; i < bytes; i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}
static uint32_t ss_xs = 0x9E3779B9u;
static uint32_t ss_rnd(uint32_t n)
{
    ss_xs ^= ss_xs << 13;
    ss_xs ^= ss_xs >> 17;
    ss_xs ^= ss_xs << 5;
    return (ss_xs >> 8) % n;
}
static FILE *ss_raw;
static uint64_t ss_h, ss_hs;
static uint32_t ss_blocks;                               /* blocks rendered in the scene */
static uint64_t ss_samples;                              /* output samples (frames) rendered since the program started */
static int64_t ss_last_l, ss_last_r;                     /* the last frame (ss_samples index) whose left / right is not 0 */
static int64_t ss_mst_first;                             /* skip build: the first frame of the first master skip since the mark (-1: none) */
static uint32_t ss_mst_seen;
static void ss_state(void)
{
    ss_hs = ss_fnv(dly_buf, sizeof dly_buf, ss_hs);
    ss_hs = ss_fnv(&fx.dly_w, sizeof fx.dly_w, ss_hs);
    ss_hs = ss_fnv(&fx.dly_lp, sizeof fx.dly_lp, ss_hs);
    ss_hs = ss_fnv(fx.dly_h, sizeof fx.dly_h, ss_hs);
    ss_hs = ss_fnv(&fx.dly_g, sizeof fx.dly_g, ss_hs);
    ss_hs = ss_fnv(&fx.cho_w, sizeof fx.cho_w, ss_hs);
    ss_hs = ss_fnv(&fx.cho_ph, sizeof fx.cho_ph, ss_hs);
    ss_hs = ss_fnv(&fx_b.dly_w, sizeof fx_b.dly_w, ss_hs);
    ss_hs = ss_fnv(&fx_b.dly_lp, sizeof fx_b.dly_lp, ss_hs);
    ss_hs = ss_fnv(fx_b.dly_h, sizeof fx_b.dly_h, ss_hs);
    ss_hs = ss_fnv(&fx_b.dly_g, sizeof fx_b.dly_g, ss_hs);
    ss_hs = ss_fnv(&fx_b.cho_w, sizeof fx_b.cho_w, ss_hs);
    ss_hs = ss_fnv(&fx_b.cho_ph, sizeof fx_b.cho_ph, ss_hs);
    {
        int32_t m[6] = {dc_l, dc_r, dce_l, dce_r, lim_env, lim_env_r};
        ss_hs = ss_fnv(m, sizeof m, ss_hs);
    }
}
static void ss_blk(uint32_t n)                           /* n blocks: every output sample to the file and the hash, the tail marks */
{
    while (n--) {
        uint32_t ms = mst_skipped, i;
        mix_block(ob, CTL);
        if (ss_raw)
            fwrite(ob, sizeof ob, 1, ss_raw);
        ss_h = ss_fnv(ob, sizeof ob, ss_h);
        for (i = 0; i < CTL; i++) {
            if (ob[2u * i])
                ss_last_l = (int64_t)(ss_samples + i);
            if (ob[2u * i + 1u])
                ss_last_r = (int64_t)(ss_samples + i);
        }
        if (mst_skipped != ms) {
            ss_mst_seen++;
            if (ss_mst_first < 0)
                ss_mst_first = (int64_t)ss_samples;
        }
        ss_samples += CTL;
        if ((++ss_blocks & 15u) == 0)
            ss_state();
    }
}
/* a tail: from here, n blocks; prints where its last sample not 0 is (both builds: compared) and (skip build) where the master first rested */
static void ss_tail(const char *what, uint32_t n)
{
    int64_t start = (int64_t)ss_samples;
    ss_last_l = ss_last_r = -1;
    ss_mst_first = -1;
    ss_blk(n);
    printf("TAIL %-40s from %lld: last L %lld R %lld\n", what, (long long)start, (long long)ss_last_l, (long long)ss_last_r);
#ifndef SIL_NOSKIP
    {
        int64_t last = ss_last_l > ss_last_r ? ss_last_l : ss_last_r;
        printf("silentskip: tail %-40s ends %6.3f s in, the master's first rest %s%.3f s in\n", what, last < 0 ? 0.0 : (double)(last - start) / FS,
               ss_mst_first < 0 ? "(none) " : "", ss_mst_first < 0 ? 0.0 : (double)(ss_mst_first - start) / FS);
    }
#endif
}
static const uint32_t SS_1S = FS / CTL;
static void ss_setup(uint32_t mode)
{
    uint32_t k;
    dsp_clean(mode);
    mods_off = 0;
    ss_blocks = 0;
    ss_h = ss_hs = 1469598103934665603ull;
    dly_skipped = dly_skipped_b = mst_skipped = 0;
    song.g[G_CDEPTH] = 50;
    song.g[G_CRATE] = 40;
    song.g[G_RTYPE] = 2;                                 /* HALL */
    song.g[G_RLOCUT] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_DMDEP] = 0;
    song.g[G_DMIX] = 70;
    song.g[G_DFDBK] = 90;
    song.g[G_PINGSPREAD] = 100;
    song.g[G_DTIME] = 1;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    for (k = 0; k < 2u; k++) {
        host_preset(&trk[k], eng_of("ANALOG"), 0);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = k ? 30 : -30;
        trk[k].p[P_SUS] = 110;
        trk[k].p[P_REL] = 50;
        trk[k].p[P_AMODE] = 0;
        trk[k].p[P_CHOR] = 70;
        trk[k].p[P_DLY] = 90;
        trk[k].p[P_REV] = 90;
    }
}
static void ss_notes(uint32_t part, int on, uint32_t base, uint32_t vel)
{
    static const uint8_t CH[4] = {0, 4, 7, 12};
    for (uint32_t k = 0; k < 4u; k++) {
        if (on)
            trk_note_on(&trk[part], 48u + base + CH[k], (uint8_t)vel);
        else
            trk_note_off(&trk[part], 48u + base + CH[k]);
    }
}
static void ss_scenes(uint32_t mode, uint32_t ping)
{
    uint32_t g, k;
    ss_setup(mode);
    if (ping)
        song.g[G_DMODE] = DMD_PING;
    ss_tail("1 silence from power-on", SS_1S * 2u);
    ss_notes(0, 1, 0, 110);                                     /* 2 a chord with every send, its tails into silence */
    ss_notes(1, 1, 7, 100);
    ss_blk(SS_1S / 2u);
    ss_notes(0, 0, 0, 0);
    ss_notes(1, 0, 7, 0);
    ss_tail("2 chord, delay + HALL + chorus tails", SS_1S * 9u);
    ss_notes(0, 1, 5, 90);                                      /* 3 a note right after the rest took over (the block after the tail's window) */
    ss_blk(4);
    ss_notes(0, 0, 5, 0);
    ss_tail("3 note after the rest", SS_1S * 9u);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;   /* 4 a loud dry chord into the limiter, then silence: the envelope's release, then the rest */
    trk[1].p[P_CHOR] = trk[1].p[P_DLY] = trk[1].p[P_REV] = 0;
    trk[0].p[P_TRIM] = trk[1].p[P_TRIM] = 127;
    trk[0].p[P_LEVEL] = trk[1].p[P_LEVEL] = 127;
    ss_notes(0, 1, 0, 127);
    ss_notes(1, 1, 3, 127);
    ss_blk(SS_1S / 3u);
    ss_notes(0, 0, 0, 0);
    ss_notes(1, 0, 3, 0);
    ss_tail("4 loud dry chord, limiter release", SS_1S * 3u);
    ss_notes(0, 1, 0, 127);                                     /*  .. and a loud note the block the master rests again */
    ss_blk(2);
    ss_notes(0, 0, 0, 0);
    ss_tail("4b loud note after the limiter's rest", SS_1S * 3u);
    trk[0].p[P_TRIM] = trk[1].p[P_TRIM] = 100;
    trk_note_on(&trk[0], 24u, 127);                             /* 5 a low note: the DC blocker's tail */
    ss_blk(SS_1S);
    trk_note_off(&trk[0], 24u);
    ss_tail("5 low note, DC blocker tail", SS_1S * 4u);
    trk[0].p[P_DLY] = 110;                                      /* 6 the delay alone, its rest, then TIME changed during the rest (the slide) and a note */
    ss_notes(0, 1, 2, 100);
    ss_blk(SS_1S / 4u);
    ss_notes(0, 0, 2, 0);
    ss_tail("6 delay alone", SS_1S * 8u);
    song.g[G_DTIME] = 3;
    ss_blk(SS_1S / 10u);
    ss_notes(0, 1, 2, 100);
    ss_blk(3);
    ss_notes(0, 0, 2, 0);
    ss_tail("6b TIME changed in the rest, a note", SS_1S * 8u);
    song.g[G_DMDEP] = 40;                                       /* 7 the delay's modulation (dly_mod), then back to the plain loop */
    ss_notes(0, 1, 2, 100);
    ss_blk(SS_1S / 4u);
    ss_notes(0, 0, 2, 0);
    ss_blk(SS_1S);
    song.g[G_DMDEP] = 0;
    ss_tail("7 modulation, back to the plain loop", SS_1S * 8u);
    if (!ping && mode != DM_SPLIT) {
        song.g[G_DMODE] = DMD_RAND;                             /*  .. RANDOM (dly_ev), then back */
        song.g[G_DMDEP] = 60;
        ss_notes(0, 1, 2, 100);
        ss_blk(SS_1S / 4u);
        ss_notes(0, 0, 2, 0);
        ss_blk(SS_1S);
        song.g[G_DMODE] = DMD_DIGITAL;
        song.g[G_DMDEP] = 0;
        ss_tail("7b RANDOM, back to DIGITAL", SS_1S * 8u);
    }
    trk[0].p[P_REV] = trk[1].p[P_REV] = 90;                     /* 8 SENDS OFF during a tail and ON again */
    trk[0].p[P_CHOR] = 60;
    song.g[G_RLOCUT] = 40;
    ss_notes(0, 1, 0, 100);
    ss_blk(SS_1S / 4u);
    ss_notes(0, 0, 0, 0);
    ss_blk(SS_1S / 2u);
    mods_off |= MODS_SENDS;
    ss_blk(SS_1S / 2u);
    mods_off &= ~MODS_SENDS;
    ss_notes(1, 1, 4, 100);
    ss_blk(SS_1S / 4u);
    ss_notes(1, 0, 4, 0);
    ss_tail("8 SENDS OFF and ON, LOCUT", SS_1S * 8u);
    for (g = 1; g <= 6u; g++) {                                 /* 9 short notes, gaps either side of the line's length (1.49 s; DUEL 0.74 s) */
        ss_notes(g & 1u, 1, g * 2u, 100);
        ss_blk(6);
        ss_notes(g & 1u, 0, g * 2u, 0);
        ss_blk(SS_1S * g / 3u + (g & 1u ? 7u : 0u));
    }
    ss_tail("9 short notes and gaps", SS_1S * 8u);
    duell_mode = (uint8_t)(mode == DM_SPLIT ? DM_STEREO : DM_SPLIT);   /* 10 a MODE change (the dip, the lines cleared) and back, notes on either side */
    ss_notes(0, 1, 0, 100);
    ss_blk(SS_1S / 4u);
    ss_notes(0, 0, 0, 0);
    ss_blk(SS_1S * 2u);
    duell_mode = (uint8_t)mode;
    ss_notes(1, 1, 0, 100);
    ss_blk(SS_1S / 4u);
    ss_notes(1, 0, 0, 0);
    for (k = 0; k < 2u; k++)
        ss_blk(SS_1S);
    ss_tail("10 MODE change and back", SS_1S * 8u);
    ss_state();
}
static void ss_fuzz(uint32_t mode, uint32_t seconds)
{
    uint32_t t, e, part;
    ss_setup(mode);
    for (t = 0; t < seconds * SS_1S / 8u; t++) {
        part = ss_rnd(2);
        switch (ss_rnd(16)) {
        case 0: case 1: ss_notes(part, 1, ss_rnd(24), 40u + ss_rnd(88)); break;
        case 2: case 3: ss_notes(part, 0, ss_rnd(24), 0); break;
        case 4: trk[part].p[P_CHOR] = (int16_t)(ss_rnd(3) == 0 ? 0 : ss_rnd(128)); break;
        case 5: trk[part].p[P_DLY] = (int16_t)(ss_rnd(3) == 0 ? 0 : ss_rnd(128)); break;
        case 6: trk[part].p[P_REV] = (int16_t)(ss_rnd(3) == 0 ? 0 : ss_rnd(128)); break;
        case 7: song.g[G_DFDBK] = (int16_t)(ss_rnd(8) ? ss_rnd(101) : 100u + ss_rnd(21)); break;   /* (now and then past unity: a runaway that never rests) */
        case 8: song.g[G_DTIME] = (int16_t)ss_rnd(6); break;
        case 9: song.g[G_DMDEP] = (int16_t)(ss_rnd(3) ? 0 : ss_rnd(100)); break;
        case 10: song.g[G_DMODE] = (int16_t)(ss_rnd(3) == 0 ? DMD_PING : ss_rnd(5) == 0 ? DMD_RAND : DMD_DIGITAL); break;
        case 11: song.g[G_RTYPE] = (int16_t)ss_rnd(6); break;
        case 12: for (e = 0; e < 4u; e++) trk_note_off(&trk[part], 48u + ss_rnd(36)); break;
        case 13: trk[part].p[P_TRIM] = (int16_t)(60u + ss_rnd(68)); break;
        case 14: if (ss_rnd(2)) ss_blk(SS_1S + ss_rnd(2u * SS_1S)); break;   /* (a long gap: the rest engages) */
        default: break;
        }
        ss_blk(1u + ss_rnd(ss_rnd(5) == 0 ? 120u : 12u));
    }
    for (e = 0; e < 2u; e++)
        for (t = 0; t < 128u; t++)
            trk_note_off(&trk[e], t);
    ss_tail("fuzz end", SS_1S * 10u);
    ss_state();
}
int main(int argc, char **argv)
{
    static const char *const MN[4] = {"STEREO", "LAYER", "DUEL", "STEREO PING-PONG"};
    static const uint32_t MD[4] = {DM_STEREO, DM_LAYER, DM_SPLIT, DM_STEREO};
    uint32_t m, sd = 0, sm = 0, blocks = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    ss_raw = argc > 1 ? fopen(argv[1], "wb") : 0;
    if (argc > 1 && !ss_raw) {
        printf("silentskip: FAIL cannot write %s\n", argv[1]);
        return 1;
    }
    for (m = 0; m < 4u; m++) {
        ss_scenes(MD[m], m == 3u);
        printf("HASH %s scenes out %016llx lines+state %016llx\n", MN[m], (unsigned long long)ss_h, (unsigned long long)ss_hs);
        printf("silentskip: %s scenes: %u blocks; delay loop left out %u (A) + %u (B), master %u\n", MN[m], ss_blocks, dly_skipped, dly_skipped_b, mst_skipped);
#ifndef SIL_NOSKIP
        {
            char t[160];
            snprintf(t, sizeof t, "%s scenes: the delay loop rests (bus A%s) and the master rests", MN[m], MD[m] == DM_SPLIT ? " and B" : "");
            expect(t, dly_skipped > ss_blocks / 10u && (MD[m] != DM_SPLIT || dly_skipped_b > ss_blocks / 10u) && mst_skipped > ss_blocks / 10u);
        }
#endif
        sd += dly_skipped + dly_skipped_b;
        sm += mst_skipped;
        blocks += ss_blocks;
        ss_xs = 0x9E3779B9u + m;
        ss_fuzz(MD[m], 8);
        printf("HASH %s fuzz out %016llx lines+state %016llx\n", MN[m], (unsigned long long)ss_h, (unsigned long long)ss_hs);
        printf("silentskip: %s fuzz: %u blocks; delay loop left out %u (A) + %u (B), master %u\n", MN[m], ss_blocks, dly_skipped, dly_skipped_b, mst_skipped);
        sd += dly_skipped + dly_skipped_b;
        sm += mst_skipped;
        blocks += ss_blocks;
    }
    {                                                           /* COST: 60 s of rest (STEREO, power-on sends), the host's clock */
        clock_t c0;
        double us;
        uint32_t b, d0, m0;
        ss_setup(DM_STEREO);
        ss_blk(SS_1S * 3u);
        d0 = dly_skipped, m0 = mst_skipped;
        c0 = clock();
        for (b = 0; b < SS_1S * 60u; b++)
            mix_block(ob, CTL);                                 /* (the mix alone: no hash, no file) */
        us = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / (SS_1S * 60u);
        printf("COST idle STEREO (HALL, DIGITAL, every send up, nothing playing; delay rests %u, master %u of %u blocks): %.2f host us a block (%s)\n",
               dly_skipped - d0, mst_skipped - m0, SS_1S * 60u, us,
#ifdef SIL_NOSKIP
               "reference, no skip"
#else
               "skip"
#endif
        );
    }
    if (ss_raw)
        fclose(ss_raw);
#ifdef SIL_NOSKIP
    expect("reference build (-DSIL_NOSKIP): nothing is left out", sd == 0 && sm == 0);
#else
    expect("skip build: the delay loop and the master were left out of many blocks", sd > blocks / 5u && sm > blocks / 5u);
#endif
    printf("silentskip: %u blocks, delay loops left out %u, master %u; %d checks, %d failed\n", blocks, sd, sm, ncheck, bad);
    return bad ? 1 : 0;
}
