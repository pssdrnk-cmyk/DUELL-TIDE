/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's audio DSP against the NOTES budget (audit A01, Q14): what the valley, the wash, the voices' dip and FM6's bridge cost on the host, as RATIOS to two stages whose
 * cost the budget has from the unit (SAT, a three-band EQ: COST-CAL rows 28.3 and 83.6), so budget.c's BD_TIDE_* items are an ESTIMATE with a stated basis until the unit
 * gives the number (tools/tide_measure.py via jam_dev.py measure, docs/TESTING.md). The host's instruction counter is macOS only (tests/cpu_bench.c), so this probe times
 * the stages with the processor's time stamp counter (fastest of several runs), as the PHYS per-model ratios of audit Q14 were taken (wall clock).
 *   build/host/tide_cost_probe        (host_tests.sh tidecost)
 * Prints the table. It fails only when a budget item is BELOW the host-derived figure (a rank check on a conservative estimate, not a timing test): the items carry a
 * safety factor of 1.5 over the figures here, so a noisy run does not fail it. -O1 -fno-builtin, like the load bench. */
#define UI_TEST_NO_MAIN 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <time.h>
#include "ui_test.c"

static unsigned checks, failures;
#define REQUIRE(msg,expr) do { checks++; if (!(expr)) { failures++; printf("FAIL: %s (%s:%d)\n",msg,__FILE__,__LINE__); } } while(0)

#if defined(__x86_64__) || defined(__i386__)
static double wall_ns(void) { return (double)__builtin_readcyclecounter(); }       /* (the time stamp counter: ticks, not ns; only ratios are used, and the Windows clock is 100 ns coarse) */
#else
static double wall_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
#endif
static int32_t ob[2 * CTL];
#define TRIALS 15
/* ticks a sample of mix_block as the scene stands: the fastest of TRIALS runs of `run` blocks (the scene's notes are held) */
static double scene_ns(unsigned run)
{
    double best = 1e30;
    for (unsigned k = 0; k < 20; k++) { mix_block(ob, CTL); while (wave_service()) ; }
    for (unsigned r = 0; r < TRIALS; r++) {
        double a = wall_ns();
        for (unsigned i = 0; i < run; i++) mix_block(ob, CTL);
        a = (wall_ns() - a) / ((double)run * CTL);
        if (a < best) best = a;
    }
    return best;
}
static track_t *scene(unsigned engine, unsigned notes)
{
    track_t *t;
    ui_power_on(); frame();
    song.playing = song.rec = 0; transport_req = 0; gl.on = gl.armed = 0; morph_musical = 0;
    host_preset(&trk[0], engine, 0);
    t = &trk[0];
    t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = AM_OFF; t->p[P_SUS] = 127; t->p[P_CHRD] = 0;
    t->p[P_DIST] = t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0; t->p[P_ITYPE] = 0;
    t->p[P_EQLG] = t->p[P_EQMG] = t->p[P_EQHG] = 0;
    trk[1].p[P_MUTE] = 1;
    memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv); memset(fm6_xt, 0, sizeof fm6_xt);
    for (unsigned i = 0; i < notes; i++) trk_note_on(t, 48 + 4 * i, 100);
    return t;
}
/* ticks a sample of the valley / wash for a state (mode 0 valley, 1 valley + wash MID, 2 wash alone: GLIDE's breath), the copy of the input taken out; side: SPREAD's second buffer */
static double stage_ns(unsigned mode, unsigned side)
{
    int32_t src[CTL], b[CTL], s[CTL];
    double best = 1e30, cpy = 1e30, a;
    const unsigned RUN = 4000;
    for (unsigned i = 0; i < CTL; i++) src[i] = (int32_t)(((i * 2749u) % 40001u)) - 20000 + (int32_t)(i & 7u) * 900;
    for (unsigned r = 0; r < TRIALS; r++) {
        memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv);
        mxs[0].snd = 1; mxs[0].lfloor = 4096 * 9; mxs[0].d = mode == 2 ? 0 : 16384; mxs[0].tp = 16384; mxs[0].need = XN_LP | XN_FILT;
        mxs[0].motion = mode == 2 ? 20000 : 0;
        mxv[0].wref = (int64_t)1 << 40;
        morph_wlvl = mode ? 2 : 0;
        memcpy(b, src, sizeof b); memcpy(s, src, sizeof s); morph_valley(0, b, side ? s : 0, CTL);   /* settle the states */
        a = wall_ns();
        for (unsigned i = 0; i < RUN; i++) { memcpy(b, src, sizeof b); memcpy(s, src, sizeof s); morph_valley(0, b, side ? s : 0, CTL); }
        a = (wall_ns() - a) / ((double)RUN * CTL);
        if (a < best) best = a;
    }
    for (unsigned r = 0; r < TRIALS; r++) {
        a = wall_ns();
        for (unsigned i = 0; i < RUN; i++) { memcpy(b, src, sizeof b); memcpy(s, src, sizeof s); __asm__ volatile("" :: "r"(b), "r"(s) : "memory"); }
        a = (wall_ns() - a) / ((double)RUN * CTL);
        if (a < cpy) cpy = a;
    }
    morph_wlvl = 0;
    return best - cpy;
}

int main(void)
{
    double n1, sat, eq, ksat, keq, k, valley, valley2, vwash, wash, xcut, bridge_on = 0, bridge_off = 0, xb = 0;
    track_t *t;
    setvbuf(stdout, 0, _IONBF, 0);
    /* the calibration: the budget's measured SAT and EQ rows against the same stages timed here */
    t = scene(0, 1); n1 = scene_ns(200);
    t = scene(0, 1); t->p[P_DIST] = 64; sat = scene_ns(200) - n1;
    t = scene(0, 1); t->p[P_EQLG] = 40; t->p[P_EQMG] = 40; t->p[P_EQHG] = 40; eq = scene_ns(200) - n1;
    ksat = (double)BD_SAT / sat;
    keq = (double)(BD_EQ_BASE + 3u * BD_EQ_BAND) / eq;
    k = (ksat + keq) / 2.0;                                 /* host instructions per tick of one sample */
    printf("tidecost: calibration, ticks a sample: SAT %.2f (row %u instr -> %.2f instr/tick), EQ x3 %.2f (row %u -> %.2f); used %.2f, the two differ by %.0f %%\n",
           sat, BD_SAT, ksat, eq, BD_EQ_BASE + 3u * BD_EQ_BAND, keq, k, (ksat > keq ? ksat / keq : keq / ksat) * 100.0 - 100.0);
    valley = stage_ns(0, 0);
    valley2 = stage_ns(0, 1);
    vwash = stage_ns(1, 0);
    wash = stage_ns(2, 0);
    printf("tidecost: valley, SPREAD off                %.2f ticks a sample -> %.0f instructions a sample\n", valley, valley * k);
    printf("tidecost: valley, SPREAD on (both buffers)  %.2f ticks a sample -> %.0f instructions\n", valley2, valley2 * k);
    printf("tidecost: valley + wash MID                 %.2f ticks a sample -> %.0f instructions (the wash on top: %.0f)\n", vwash, vwash * k, (vwash - valley) * k);
    printf("tidecost: wash alone (GLIDE's breath)       %.2f ticks a sample -> %.0f instructions\n", wash, wash * k);
    /* morph_xcut: per voice and block */
    {
        double best = 1e30, a;
        const unsigned RUN = 200000;
        volatile int32_t sink = 0;
        t = scene(0, 1);
        t->mwon = 1; t->mwpi = 0; morph_cdd[0] = 16384; mxs[0].lfloor = 4096 * 9; mxs[0].lp = mxs[0].vf = 1; mxs[0].motion = 9000;
        for (unsigned r = 0; r < TRIALS; r++) {
            a = wall_ns();
            for (unsigned i = 0; i < RUN; i++) sink += morph_xcut(t, (int32_t)(UF_TOP / 2 + (i & 255u)));
            a = (wall_ns() - a) / ((double)RUN * CTL);
            if (a < best) best = a;
        }
        xcut = best; morph_cdd[0] = 0; t->mwon = 0;
        printf("tidecost: voice dip (morph_xcut)            %.3f ticks a sample and voice -> %.2f instructions\n", xcut, xcut * k);
    }
    /* FM6's bridge: a held chord through the retuning crossing of tests/tide_crossings_test.c, blocks with the bridge on against the same blocks at rest */
    {
        static double on[2200], off[2200];
        double xb_per_sample = 0;
        double med_on = 0, med_off = 0;
        unsigned non = 0, noff = 0;
        t = scene(ENGI_FM6, 0);
        {   morph_part_t *mp = morph_ready(0);
            unsigned alg = 0, car;
            t->p[P_E0] = 0; t->p[P_E1] = t->p[P_E3] = t->p[P_E4] = t->p[P_E5] = t->p[P_E6] = 0;
            for (; alg < 32; alg++) { unsigned m = fm6_carriers(alg); if (m && !(m & (m - 1))) break; }
            car = 0; while (!((fm6_carriers(alg) >> car) & 1u)) car++;
            for (unsigned side = 0; side < 2; side++) {
                uint8_t *p = morph_fmto[0][side];
                memcpy(p, fm6_patch[0], FP_SIZE);
                p[FP_ALG] = (uint8_t)alg; p[FP_FB] = 0;
                for (unsigned q = 0; q < 6; q++) { uint8_t *op = p + q * FP_OP; op[FP_MODE] = 0; op[FP_FC] = (uint8_t)(q == car ? 1 + side : 1); op[FP_FF] = 0; }
                mp->tok[side][mp->top[side]] = (uint8_t)(side + 1);
                morph_fmto_k[0][side] = (uint8_t)(side + 1);
            }
            morph_build(mp); mp->fser++; mp->cvalid = 0;
            morph_xplan(0);
        }
        morph_do(MQ_FADER, 1, 0); trk_note_on(t, 60, 110); trk_note_on(t, 64, 110); trk_note_on(t, 67, 110);
        for (unsigned i = 0; i < 128; i++) mix_block(ob, CTL);
        morph_do(MQ_FADER, 1, 127);
        for (unsigned i = 0; i < 2200; i++) {
            double a = wall_ns();
            if (!xb_per_sample && fm6_xt[0].on && fm6_xt[0].alg == 31) {   /* on the bridge: fm6_xblock alone (the hook fm6_block runs once a block and part), state held */
                const unsigned RUN = 20000;
                double best = 1e30;
                for (unsigned r = 0; r < TRIALS; r++) {
                    double b = wall_ns();
                    for (unsigned q = 0; q < RUN; q++) fm6_xblock(t, 0);
                    b = (wall_ns() - b) / ((double)RUN * CTL);
                    if (b < best) best = b;
                }
                xb_per_sample = best;
                a = wall_ns();
            }
            mix_block(ob, CTL);
            a = (wall_ns() - a) / CTL;
            if (fm6_xt[0].on && fm6_xt[0].alg == 31) on[non++] = a; else if (!fm6_xt[0].on && i > 1000) off[noff++] = a;
        }
        for (unsigned pass = 0; pass < 2; pass++) {       /* medians (a sort of the few hundred values) */
            double *v = pass ? off : on; unsigned n = pass ? noff : non;
            for (unsigned i = 1; i < n; i++) { double x = v[i]; unsigned j = i; while (j && v[j - 1] > x) { v[j] = v[j - 1]; j--; } v[j] = x; }
            if (pass) med_off = n ? v[n / 2] : 0; else med_on = n ? v[n / 2] : 0;
        }
        bridge_on = med_on; bridge_off = med_off;
        xb = xb_per_sample;
        printf("tidecost: FM6 bridge, 3 notes: %u blocks on the bridge (median %.2f ticks a sample), %u at rest (median %.2f): +%.2f -> %.0f instructions (%.0f a note)\n",
               non, bridge_on, noff, bridge_off, bridge_on - bridge_off, (bridge_on - bridge_off) * k, (bridge_on - bridge_off) * k / 3.0);
        printf("tidecost: fm6_xblock alone on the bridge (3 held notes) %.3f ticks a sample -> %.1f instructions a sample for the part, %.1f a note (the whole-block timing above cannot see it: noise)\n", xb, xb * k, xb * k / 3.0);
        REQUIRE("the FM6 bridge ran (blocks on the bridge were timed)", non > 20u && noff > 20u);
    }
#ifdef BD_TIDE_VALLEY
    REQUIRE("budget: BD_TIDE_VALLEY is not below the host-derived valley (SPREAD on)", (double)BD_TIDE_VALLEY >= valley2 * k);
    REQUIRE("budget: BD_TIDE_WASH is not below the host-derived wash on top of the valley", (double)BD_TIDE_WASH >= (vwash - valley) * k);
    REQUIRE("budget: BD_TIDE_WASH is not below the host-derived GLIDE-breath wash alone", (double)BD_TIDE_WASH >= wash * k);
    REQUIRE("budget: BD_TIDE_FM6 (a note) is not below the host-derived bridge cost", (double)BD_TIDE_FM6 * 8.0 >= xb * k / 3.0 * 8.0);
    printf("tidecost: budget items: valley %u, wash %u, FM6 bridge a note %u, voice dip a voice %u (instructions a sample)\n", BD_TIDE_VALLEY, BD_TIDE_WASH, BD_TIDE_FM6, BD_TIDE_DIP);
    REQUIRE("budget: BD_TIDE_DIP is not below the host-derived dip", (double)BD_TIDE_DIP >= xcut * k);
#endif
    printf("tidecost: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
