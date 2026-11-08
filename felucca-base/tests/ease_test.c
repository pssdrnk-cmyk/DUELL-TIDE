/* SPDX-License-Identifier: GPL-3.0-only */
/* K3 THE EASE LADDER (TIDE 1.0.0-b4; audio.c ease_step / ease_set, core.h ease; CPU audit docs/audits/2026-11-08/cpu/TASKS.md#k3). The real audio ISR with a
 * simulated clock (as tests/audio_test.c): each half reports the elapsed time the test sets (host_us; TIMER5 nested: 0, so render = elapsed).
 *   SEQUENCE  30 halves at 60 %, 2 at 90 %, 10 at 60 %, 1 at 110 %, then 50 %: the level after every half against the rule (up one after two halves in a row over
 *             85 %, at least 4 after a miss, down one after 1034 halves (~3 s) with the meter under 70 %, the count starting again), every change at its half
 *   COUNTERS  ease_n (entries into each level, a jump enters each level on the way), LIGHT FX (ovl_lite) exactly at levels 3 .. 5, ovl_count = entries into 3
 *   NOKILL    held and released voices through levels 1 .. 3 (four halves at 90 %, then 80 % for 2 s): no voice faded (shed_count, voice_kills), all still sounding
 *   ADMIT     level 2: a new POLY note takes over the oldest released voice instead of a free one when the part sounds np - 1 voices; with no released voice it
 *             takes a free one (no held note cut); level 0 and 1: a free one as ever
 *   LEVEL4/5  level 4 fades the two oldest released tails, never a held note, however long 90 % lasts (level 5 reached, no miss); level 5 and a half that misses:
 *             one held note fades (not the bass)
 *   LEVEL1    the valley and the wash with their coefficients every other block (level 1) against every block (level 0): the same input, the output within
 *             -30 dB of the level-0 render (a one-block lag of the coefficients) and no step over the level-0 render's own largest step (the coefficients still glide inside the block)
 * Built and run by DUELL/host_tests.sh (name: ease) */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us;
static volatile uint32_t t5_nested_ticks;
static uint32_t fm1_ticks(void) { t5_nested_ticks = 0; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return 0u; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p) { (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; (void)n; }
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? 1023 : 620; }
#include "../firmware/src/master.c"

#if !DUELL_TIDE
int main(void) { printf("SKIP the ease ladder is TIDE's (DUELL_TIDE=0: the two rules of before, tests/audio_test.c)\n"); return 0; }
#else
static int bad, ncheck;
static void expect(const char *what, int ok)
{
    ncheck++;
    printf("ease: %-128s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define HALF_US (HALF_FRAMES * 1000000u / FS)
#define US_OF(pct) (HALF_US * (pct) / 100u)

static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    for (p = 0; p < NPART; p++) host_preset(&trk[p], 0, 0);
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    audio_cpu_rem = audio_halves = audio_max_us = 0;
    shed_req = 0; shed_count = 0; shed_over = 0; voice_kills = 0;
    ease = 0; ease_calm = 0; memset(ease_n, 0, sizeof ease_n); ovl_lite = 0;
    host_clock = host_us = 0;
}
static void half(uint32_t pct) { host_us = US_OF(pct); fm1_alnk0_irq(); }
static uint32_t sounding(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].stage != 4u; return n; }
static uint32_t held_n(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u; return n; }

/* ------------------------------------------------------------------ SEQUENCE: the rule, half by half --- */
static void sequence(void)
{
    static const struct { uint32_t n, pct; } SEQ[] = {{30, 60}, {2, 90}, {10, 60}, {1, 110}, {5000, 50}};
    uint32_t s, k, h = 0, model = 0, calm = 0, prev = 0, mism = 0, changes = 0, lite_ok = 1, at[8] = {0}, lv[8] = {0};
    fresh();
    for (s = 0; s < sizeof SEQ / sizeof SEQ[0]; s++)
        for (k = 0; k < SEQ[s].n; k++, h++) {
            uint32_t over = US_OF(SEQ[s].pct) > SHED_TICKS, miss = US_OF(SEQ[s].pct) > SHED_FULL_TICKS, two = over && prev;
            half(SEQ[s].pct);
            if (two || miss) {                          /* (the rule as the brief states it, written out again) */
                calm = 0;
                model += two && model < 5u;
                model = miss && model < 4u ? 4u : model;
            } else if (model) {
                calm = song.cpu_q8 < OVL_CALM_Q8 ? calm + 1u : 0u;
                if (calm >= OVL_CALM_HALVES)
                    model--, calm = 0;
            }
            prev = over;
            mism += ease != model;
            lite_ok &= ovl_lite == (ease >= 3u);
            if (ease != (changes ? lv[changes - 1u] : 0u) && changes < 8u)
                at[changes] = h, lv[changes++] = ease;
        }
    printf("ease: SEQUENCE the changes (half: level):");
    for (k = 0; k < changes; k++)
        printf(" %u:%u", at[k], lv[k]);
    printf("\n");
    expect("SEQUENCE: the level after every one of 5043 halves is the rule's (up after two halves over 85 %, 4 after a miss, down after ~3 s calm)", !mism);
    expect("  .. 0 for 30 halves at 60 %, 1 after the second half at 90 % (half 31), still 1 through 10 halves at 60 %", changes >= 1u && at[0] == 31u && lv[0] == 1u);
    expect("  .. the half at 110 % (a miss, half 42): straight to 4", changes >= 2u && at[1] == 42u && lv[1] == 4u);
    expect("  .. at 50 %: one level down every 1034 halves (3.0 s): 3 at half 1076, 2 at 2110, 1 at 3144, 0 at 4178, and nothing after",
           changes == 6u && at[2] == 42u + OVL_CALM_HALVES && lv[2] == 3u && at[3] == 42u + 2u * OVL_CALM_HALVES && lv[3] == 2u &&
           at[4] == 42u + 3u * OVL_CALM_HALVES && lv[4] == 1u && at[5] == 42u + 4u * OVL_CALM_HALVES && lv[5] == 0u);
    expect("COUNTERS: ease_n = 1, 1, 1, 1, 0 (the jump 1 -> 4 entered 2, 3 and 4), LIGHT FX exactly at levels 3 .. 5, ovl_count 1",
           ease_n[0] == 1u && ease_n[1] == 1u && ease_n[2] == 1u && ease_n[3] == 1u && ease_n[4] == 0u && lite_ok && ovl_count == 1u);
    expect("  .. no voice faded on the way (nothing was sounding: shed_count 0)", shed_count == 0u);
    fresh();                                            /* single slow halves never ease; a calm level 0 stays 0 */
    for (k = 0; k < 2000u; k++)
        half(k % 7u ? 70u : 97u);
    expect("  .. single halves at 97 % among 70 % ones (never two in a row) leave the level at 0", ease == 0u && !ease_n[0]);
    fresh();                                            /* 80 % (between 70 and 85) holds a level: no step up, no release */
    for (k = 0; k < 4u; k++)
        half(90u);
    for (k = 0; k < 4000u; k++)
        half(80u);
    expect("  .. four halves at 90 % reach 3; 80 % (the meter over 70 %) holds it for 11 s, no step up, no release", ease == 3u && ovl_lite && ease_n[3] == 0u);
    for (k = 0; k < 20u; k++)
        half(90u);
    expect("  .. 90 % on: 4, then 5 (one level a half at most), and there it stays", ease == 5u && ease_n[3] == 1u && ease_n[4] == 1u);
}

/* ------------------------------------------------------------------ the voices --- */
static void scene(void)                                  /* part 1: six POLY notes with a long release, three of them released (50, 54, 58) */
{
    uint32_t i;
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_REL] = 120;
    for (i = 0; i < 6u; i++) trk_note_on(&trk[0], 48u + 2u * i, 100);
    for (i = 0; i < 8u; i++) half(30u);
    trk_note_off(&trk[0], 50); trk_note_off(&trk[0], 54); trk_note_off(&trk[0], 58);
    half(30u);
    shed_count = 0; voice_kills = 0;
}
static void voices(void)
{
    uint32_t k, s0, h0;
    scene();
    s0 = sounding(&trk[0]); h0 = held_n(&trk[0]);
    for (k = 0; k < 4u; k++)
        half(90u);
    for (k = 0; k < 700u; k++)
        half(80u);
    expect("NOKILL: through levels 1, 2, 3 (4 halves at 90 %, 2 s at 80 %): no voice faded (shed_count 0, voice_kills 0), the 6 voices still sound",
           ease == 3u && ease_n[0] == 1u && ease_n[1] == 1u && ease_n[2] == 1u && shed_count == 0u && voice_kills == 0u && s0 == 6u && sounding(&trk[0]) == 6u && h0 == 3u && held_n(&trk[0]) == 3u);

    {                                                   /* ADMIT */
        uint32_t np = trk_nvoice(&trk[0]), lv, ok[3] = {0, 0, 0}, few = 0;
        for (lv = 0; lv < 3u; lv++) {
            voice_t *v;
            scene();
            for (k = 0; sounding(&trk[0]) + 1u < np && k < NVOICE; k++)   /* (the part up to np - 1 sounding) */
                trk_note_on(&trk[0], 60u + 2u * k, 100);
            ease = (uint8_t)lv;
            v = voice_alloc(&trk[0], 80u);
            ok[lv] = lv < 2u ? !v->active : v->active && !v->gate && v->note == 50u;   /* (levels 0, 1: a free voice; 2: the oldest released, 50) */
        }
        scene();                                        /* 6 sounding: under np - 1 */
        ease = 2;
        few = np > 7u && !voice_alloc(&trk[0], 80u)->active;
        printf("ease: ADMIT the part's number %u\n", np);
        expect("ADMIT: level 2, a new note with np - 1 of the part's voices sounding takes over the oldest released voice (50), not a free one; levels 0 and 1 a free one",
               np > 2u && ok[0] && ok[1] && ok[2] && voice_kills == 0u);
        expect("  .. level 2 with fewer than np - 1 sounding: a free voice", few);
        scene();
        for (k = 0; k < NVOICE; k++)
            if (!trk[0].v[k].gate) trk[0].v[k].active = 0, trk[0].v[k].stage = 0;   /* (only the three held notes left) */
        for (k = 0; sounding(&trk[0]) + 1u < np && k < NVOICE; k++)
            trk_note_on(&trk[0], 60u + 2u * k, 100);    /* np - 1 held, none released */
        ease = 2;
        {
            uint32_t h1 = held_n(&trk[0]);
            voice_t *v = voice_alloc(&trk[0], 80u);
            expect("  .. level 2, no released voice: a free voice as ever (no held note is cut)", !v->active && held_n(&trk[0]) == h1 && voice_kills == 0u);
        }
    }

    scene();                                            /* LEVEL 4 / 5 */
    for (k = 0; k < 6u; k++)
        half(90u);
    expect("LEVEL4: six halves at 90 %: level 4 is entered and its half fades the two oldest released tails (50, 54); the held notes sound",
           ease >= 4u && shed_count == 2u && held_n(&trk[0]) == 3u);
    for (k = 0; k < 300u; k++)
        half(90u);
    expect("LEVEL5: 300 more halves at 90 % (level 5, no miss): the last tail faded, never a held note", ease == 5u && shed_count == 3u && held_n(&trk[0]) == 3u);
    half(110u);
    half(50u);
    {
        voice_t *b48 = 0;
        for (k = 0; k < NVOICE; k++)
            if (trk[0].v[k].active && trk[0].v[k].note == 48u) b48 = &trk[0].v[k];
        expect("  .. at level 5 a half that missed: one held note fades, not the bass (48)", shed_count == 4u && held_n(&trk[0]) == 2u &&
               b48 && b48->gate && b48->stage != 4u);
    }
    scene();
    half(110u);
    half(50u);
    expect("  .. a miss at level 0: to 4, the two oldest tails fade (the 0.2.11 fast shed), no held note", ease == 4u && shed_count == 2u && held_n(&trk[0]) == 3u);
}

/* ------------------------------------------------------------------ LEVEL 1: the valley and the wash --- */
static void level1(void)
{
    static int32_t ya[2][64], yb[2][64];
    uint32_t blk, i, ph = 0, run;
    double e2 = 0, s2 = 0, er[2] = {0, 0}, sr[2] = {0, 0};
    int32_t st0 = 0, st1 = 0, la = 0, lb = 0;
    for (run = 0; run < 2u; run++) {                    /* run 0: the valley alone (WASH OFF); 1: with the wash */
        memset(mxv, 0, sizeof mxv); memset(mxs, 0, sizeof mxs);
        mxv[0].wr = mxv[1].wr = 0x2468ACEu;
        morph_wlvl = (uint8_t)(run ? 2u : 0u);
        for (blk = 0; blk < 4000u; blk++) {
            uint32_t p;
            int32_t amp = (blk / 300u) % 2u ? 2000000 : 400000;
            for (p = 0; p < 2u; p++) {
                mxs[p].d = (uint16_t)((blk * 41u) % 65536u > 32767u ? 65535u - (blk * 41u) % 65536u : (blk * 41u) % 65536u);   /* (a slow triangle of depth) */
                mxs[p].lfloor = 4096u * 9u;
                mxs[p].snd = 1;
                mxs[p].motion = (uint16_t)((blk / 200u) % 2u ? 20000u : 0u);
            }
            for (i = 0; i < 32u; i++, ph++) {
                int32_t x = (int32_t)(amp * sin(ph * 0.031) + amp / 3 * sin(ph * 0.47));
                ya[run][i] = yb[run][i] = x;
            }
            ease = 0;
            morph_valley(0, ya[run], 0, 32u);
            ease = 1;
            morph_valley(1, yb[run], 0, 32u);
            for (i = 0; i < 32u; i++) {
                double d = (double)ya[run][i] - yb[run][i];
                int32_t s0 = ya[run][i] - la, s1 = yb[run][i] - lb;
                s0 = s0 < 0 ? -s0 : s0, s1 = s1 < 0 ? -s1 : s1;
                if (blk > 20u) {
                    st0 = s0 > st0 ? s0 : st0, st1 = s1 > st1 ? s1 : st1;
                    er[run] += d * d, sr[run] += (double)ya[run][i] * ya[run][i];
                }
                la = ya[run][i], lb = yb[run][i];
            }
        }
    }
    ease = 0;
    morph_wlvl = 0;
    e2 = er[0] + er[1], s2 = sr[0] + sr[1];
    printf("ease: LEVEL1 the valley alone %.1f dB under the signal, with the wash %.1f dB\n", 10.0 * log10(sr[0] / (er[0] + 1e-9)), 10.0 * log10(sr[1] / (er[1] + 1e-9)));
    printf("ease: LEVEL1 the valley and the wash, coefficients every other block against every block: difference %.1f dB under the signal, largest step %d (every block: %d)\n",
           10.0 * log10(s2 / (e2 + 1e-9)), st1, st0);
    expect("LEVEL1: the valley and the wash at level 1 within -30 dB of level 0's render (coefficients a block late), no step over level 0's largest + 1 %", s2 > 0 && e2 < s2 * 1e-3 && st1 <= st0 + st0 / 100);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    sequence();
    voices();
    level1();
    printf("ease: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
#endif
