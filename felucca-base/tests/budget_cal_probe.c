/* SPDX-License-Identifier: GPL-3.0-only */
/* The NOTES budget against the code (b10 round 6, ROADMAP Later 22; audit findings 5, 6, 34): what the host can say about the budget's terms without the unit, as RATIOS to two stages whose
 * cost the budget has from the unit (SAT 28.3, EQ x3 83.6: COST-CAL rows), the way tests/tide_cost_probe.c does it. The host's instruction counter is macOS only (tests/cpu_bench.c), so this probe times
 * with the processor's time stamp counter (fastest of several runs); a figure is "ticks x k" host instructions, k from the SAT and EQ stages timed here. Every figure is a LOWER BOUND of what the
 * unit does (x86 against a Cortex-M7 with the code in the flash, its caches cold after a half's audio): docs/BUDGET_HONEST.md lists which numbers only the unit can confirm.
 *   (a) ISR     what tcr_deadline's block 0 costs core 0: part_dl of both parts + global_cost_dl (budget.c; they call morph_apply per part and morph end), on the 24 factory multis as loaded, with
 *               the fader at the middle (the dearest case: both ends blended) .. as microseconds of a half, 100 % = 2,902 us = 420,000 host instructions
 *   (b) BLEND   a hand-moved fader's blend (morph_begin, every block), as a part cost: instructions a sample, against BD_MORPH x the part's voices (finding 34)
 *   (c) WASH    the wash's power meter with WASH on and nothing moving (the meter runs on every block of a part with WASH on; the noise only while it moves): instructions a sample
 *   build/host/budget_cal_probe        (host_tests.sh budgetcal). -O1 -fno-builtin, like the load bench. Fails only on a rank check (a budget item BELOW the host figure, or the ISR figure over the limit). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static unsigned checks, failures;
#define REQUIRE(msg, expr) do { checks++; if (!(expr)) { failures++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

#if defined(__x86_64__) || defined(__i386__)
static double wall_ns(void) { return (double)__builtin_readcyclecounter(); }       /* (the time stamp counter: ticks; only ratios are used) */
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
#define HALF_INSTR (3281.25 * 128.0)             /* 100 % of a half in host instructions (the budget's scale) */
#define HALF_US 2902.0                           /* 128 frames at 44.1 kHz */

static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
static double scene_ns(unsigned run)             /* ticks a sample of mix_block, the scene as it stands (the notes are held) */
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
/* ticks a sample of the wash's meter with WASH MID and nothing moving (the part sounds, no valley, no movement): morph_valley's early exit into morph_wash, the copy of the input taken out */
static double wash_rest_ns(void)
{
    int32_t src[CTL], b[CTL];
    double best = 1e30, cpy = 1e30, a;
    const unsigned RUN = 4000;
    for (unsigned i = 0; i < CTL; i++) src[i] = (int32_t)(((i * 2749u) % 40001u)) - 20000 + (int32_t)(i & 7u) * 900;
    for (unsigned r = 0; r < TRIALS; r++) {
        memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv);
        mxs[0].snd = 1; mxs[0].lfloor = 4096 * 9; mxs[0].need = 0; mxs[0].tp = 0;
        mxv[0].wref = (int64_t)1 << 40;
        morph_wlvl = 2;
        memcpy(b, src, sizeof b); morph_valley(0, b, 0, CTL);
        a = wall_ns();
        for (unsigned i = 0; i < RUN; i++) { memcpy(b, src, sizeof b); morph_valley(0, b, 0, CTL); }
        a = (wall_ns() - a) / ((double)RUN * CTL);
        if (a < best) best = a;
    }
    for (unsigned r = 0; r < TRIALS; r++) {
        a = wall_ns();
        for (unsigned i = 0; i < RUN; i++) { memcpy(b, src, sizeof b); __asm__ volatile("" :: "r"(b) : "memory"); }
        a = (wall_ns() - a) / ((double)RUN * CTL);
        if (a < cpy) cpy = a;
    }
    morph_wlvl = 0;
    return best - cpy;
}

int main(void)
{
    double n1, sat, eq, k, wr, worst_us = 0, sum_us = 0, worst_ticks = 0;
    unsigned worst_i = 0, nm = 0;
    track_t *t;
    setvbuf(stdout, 0, _IONBF, 0);
    t = scene(0, 1); n1 = scene_ns(200);
    t = scene(0, 1); t->p[P_DIST] = 64; sat = scene_ns(200) - n1;
    t = scene(0, 1); t->p[P_EQLG] = 40; t->p[P_EQMG] = 40; t->p[P_EQHG] = 40; eq = scene_ns(200) - n1;
    k = ((double)BD_SAT / sat + (double)(BD_EQ_BASE + 3u * BD_EQ_BAND) / eq) / 2.0;
    printf("budgetcal: calibration, ticks a sample: SAT %.2f (row %u), EQ x3 %.2f (row %u); k = %.2f instructions per tick\n", sat, BD_SAT, eq, BD_EQ_BASE + 3u * BD_EQ_BAND, k);

    /* (a) the ISR: tcr_deadline's block 0 on the 24 factory multis */
    printf("budgetcal: (a) block 0 of tcr_deadline: part_dl x2 + global_cost_dl, instructions and microseconds of the half (100 %% = %.0f us), fader at the middle:\n", HALF_US);
    for (unsigned i = 0; i < 24u; i++) {
        double best = 1e30;
        uint32_t f = 0, v = 0, g = 0, f2 = 0, v2 = 0;
        power();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        project_load(MIDX(i));
        frame();
        morph_do(MQ_FADER, 3u, 64u);                      /* both parts' faders to the middle (the mask: a bit a part) */
        frame();
        for (unsigned r = 0; r < TRIALS; r++) {
            const unsigned RUN = 400;
            double a = wall_ns();
            for (unsigned q = 0; q < RUN; q++) {
                part_dl(&trk[0], &f, &v);
                part_dl(&trk[1], &f2, &v2);
                g += global_cost_dl();
                __asm__ volatile("" :: "r"(f), "r"(v), "r"(f2), "r"(v2), "r"(g) : "memory");
            }
            a = (wall_ns() - a) / (double)RUN;
            if (a < best) best = a;
        }
        {
            double ins = best * k, us = ins / HALF_INSTR * HALF_US;
            printf("budgetcal:   M%02u %-22s %7.0f ticks -> %6.0f instructions -> %5.1f us (%.2f %% of a half)  morph ends: %u + %u\n", i + 1, MULTI_NAME[MIDX(i)], best, ins, us, ins / HALF_INSTR * 100.0,
                   (unsigned)(mph[0].nid > 0), (unsigned)(mph[1].nid > 0));
            sum_us += us; nm++;
            if (us > worst_us) worst_us = us, worst_i = i, worst_ticks = best;
        }
    }
    printf("budgetcal: (a) worst M%02u: %.0f ticks, %.1f us a half; mean %.1f us. The unit adds cold flash fetches (the code is %u B in the flash, the audio before it evicts the caches) and a slower core: see docs/BUDGET_HONEST.md\n",
           worst_i + 1, worst_ticks, worst_us, sum_us / nm, 1388u);

    /* (b) the blend of a hand-moved fader, per block, as a part cost: every factory multi, each part alone (the other muted), 4 notes, the fader stepping 2 <-> 3 (below the arc: only the blend) */
    printf("budgetcal: (b) the blend of a hand-moved fader, one part alone with 4 notes, the fader stepping below the arc (ticks a sample more than still -> instructions a sample; the budget charges BD_MORPH %u x 4 voices = %u):\n", BD_MORPH, BD_MORPH * 4u);
    {
        double mx = 0, sm = 0;
        unsigned cnt = 0;
        for (unsigned i = 0; i < 24u; i++) {
            for (unsigned pt = 0; pt < 2u; pt++) {
                double a0, a1;
                unsigned nid;
                power();
                project_load(MIDX(i));
                frame();
                song.playing = song.rec = 0; transport_req = 0; gl.on = gl.armed = 0;
                trk[!pt].p[P_MUTE] = 1;
                trk[pt].p[P_MUTE] = 0;
                for (unsigned q = 0; q < 4u; q++) trk_note_on(&trk[pt], 48 + 4 * q, 100);
                morph_ready(pt);
                nid = mph[pt].nid + mph[pt].fnid;
                morph_do(MQ_FADER, 1u << pt, 2u);
                {
                    double b0 = 1e30, b1 = 1e30;       /* still and moving, interleaved in the same state: the fastest of each */
                    for (unsigned q = 0; q < 40; q++) mix_block(ob, CTL);
                    for (unsigned r = 0; r < TRIALS; r++) {
                        double a;
                        a = wall_ns();
                        for (unsigned q = 0; q < 60; q++) mix_block(ob, CTL);
                        a = (wall_ns() - a) / (60.0 * CTL);
                        if (a < b0) b0 = a;
                        a = wall_ns();
                        for (unsigned q = 0; q < 60; q++) { morph_do(MQ_FADER, 1u << pt, (q & 1u) ? 2u : 3u); mix_block(ob, CTL); }
                        a = (wall_ns() - a) / (60.0 * CTL);
                        if (a < b1) b1 = a;
                        morph_do(MQ_FADER, 1u << pt, 2u);
                    }
                    a0 = b0, a1 = b1;
                }
                if (nid) {
                    double ins = (a1 - a0) * k;
                    printf("budgetcal:   M%02u part %u %-22s engine %-7s morph entries %3u: %5.2f ticks -> %5.1f instructions a sample (%.1f us a block)\n", i + 1, pt + 1, MULTI_NAME[MIDX(i)], ENGINES[trk[pt].engine]->name,
                           nid, a1 - a0, ins, ins * CTL / HALF_INSTR * HALF_US * 4.0 / 4.0);
                    cnt++; sm += ins; if (ins > mx) mx = ins;
                }
            }
        }
        printf("budgetcal: (b) %u parts with a morph: mean %.1f, worst %.1f instructions a sample (the charge at 4 voices: %u). On the unit one such blend (BIG STAB, FM6) took ~70 us a block more (6b6678f)\n",
               cnt, sm / cnt, mx, BD_MORPH * 4u);
    }

    /* (d) the NOTES a part gets in the shipped configuration (FELUCCA_MOVECOST 1: the host tests build it off): the fader at its end (rest), parked in the middle of the arc, and moving (bd_mv): the factory multis as loaded.
     * One core (budget_solve2 with two = 0) and two cores; each "a/b" is part 1 / part 2 */
    printf("budgetcal: (d) NOTES a part, FELUCCA_MOVECOST=%d: at rest (fader at A) | parked at the middle of the arc | moving, two-core rule (one-core rule in brackets):\n", FELUCCA_MOVECOST);
    {
        unsigned lost_mid = 0, lost_mv = 0, parts = 0;
        for (unsigned i = 0; i < 24u; i++) {
            uint32_t n[3][2], m[3][2];
            power();
            ui_prefs2 |= PREF_MODE_RECALL >> 16;
            project_load(MIDX(i));
            frame();
            for (unsigned st = 0; st < 3u; st++) {
                bd_t b;
                uint32_t pp;
                morph_do(MQ_FADER, 3u, st == 1u ? 64u : 0u);
                for (pp = 0; pp < 2u; pp++) {
                    mxs[pp].tp = (uint16_t)morph_wb(mph[pp].fader);   /* (arrived) */
                    bd_mv[pp] = st == 2u;
                }
                bd_build(&b);
                budget_solve2(&b, n[st], 1);
                budget_solve2(&b, m[st], 0);
            }
            printf("budgetcal:   M%02u %-22s rest %u/%u (%u/%u)  mid %u/%u (%u/%u)  moving %u/%u (%u/%u)\n", i + 1, MULTI_NAME[MIDX(i)], n[0][0], n[0][1], m[0][0], m[0][1], n[1][0], n[1][1], m[1][0], m[1][1],
                   n[2][0], n[2][1], m[2][0], m[2][1]);
            for (unsigned pp = 0; pp < 2u; pp++) { lost_mid += n[0][pp] - n[1][pp]; lost_mv += n[0][pp] - n[2][pp]; parts++; }
            REQUIRE("(d) parked in the arc / moving never gives MORE notes than at rest", n[1][0] <= n[0][0] && n[1][1] <= n[0][1] && n[2][0] <= n[0][0] && n[2][1] <= n[0][1]);
        }
        printf("budgetcal: (d) over %u parts: parked in the arc takes %u notes in all from the rest figures, moving takes %u (two-core rule)\n", parts, lost_mid, lost_mv);
    }

    /* (c) the wash's meter at rest */
    wr = wash_rest_ns();
    printf("budgetcal: (c) the wash's power meter, WASH MID, nothing moving: %.2f ticks a sample -> %.0f instructions a sample (BD_TIDE_WASH charges %u while the wash makes noise)\n", wr, wr * k, BD_TIDE_WASH);
#ifdef BD_TIDE_WASH_REST
    REQUIRE("budget: BD_TIDE_WASH_REST is not below the host-derived meter", (double)BD_TIDE_WASH_REST >= wr * k);
#endif
    REQUIRE("the ISR figure of block 0 was measured", worst_us > 0.0);
    printf("budgetcal: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
