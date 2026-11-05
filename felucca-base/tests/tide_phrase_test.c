/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's phrase lifecycle and pitch walk (audit E12): firmware/src/seq_phrase.c (phrase_gap / repeat / order / pitch / mutate) and seq_evolve.c
 * (the 5-bit pitch offsets, sqm_pitch_walk, sqm_pitch_apply), which no other test names. Statistical bounds over many seeds, not one trace:
 *   pitch codec   a signed offset of -12..+12 round-trips at every step and leaves its neighbours alone
 *   pitch walk    one move is one scale step on a hit, never a bass note (under C3), never beyond an octave from the stored note, never chromatic
 *                 (SCALE CHR: no move); hundreds of moves in a row stay in the scale and inside the octave; the moves reach several steps
 *   phrase ops    repeats never pass the density cap (tarc_cap), a gap never leaves under half the attacks, an order swap keeps the number of
 *                 attacks and every hit within two 16ths of its stored step; the anchors (the motif's identity floor) are never moved or dropped;
 *                 the mutate cycle changes the phrase in most turns and edits more than three different steps */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tidephrase: FAIL %s\n", s); bad++; } } while (0)

enum { LEN = 16, LOW_STEP = 6 };
static const uint8_t MAJ_NOTES[8] = { 72, 74, 76, 77, 79, 81, 83, 84 };
static int in_scale(int32_t n) { return (SCALE_MASK[1] >> (uint32_t)((n - root_g + 240) % 12)) & 1u; }

/* part 0: 16 steps, a hit on every step but 1, 5, 9 and 13 (12 attacks; C major notes from C5 up), step 6 a bass note (C2); part 1 empty; SCALE MAJ, ROOT C, pitch moves on */
static void setup(void)
{
    uint32_t i;
    ui_power_on();
    frame();
    memset(sqm, 0, sizeof sqm);
    memset(&rhythm, 0, sizeof rhythm);
    memset(rhythm_map, 0, sizeof rhythm_map);
    {   /* a LEAD sound (the default sound is in the bass family, whose notes never walk) */
        uint32_t e = trk[0].eng_req, j;
        for (j = 0; j < ds_count(e) && ds_cat_of(e, j) != CAT_LEAD; j++)
            ;
        trk[0].preset = (uint8_t)j, trk[0].user = 0;
    }
    scale_g = 1;
    root_g = 0;
    morph.cfg |= MC_PIT;
    memset(trk[0].step, 0, sizeof trk[0].step);
    memset(trk[1].step, 0, sizeof trk[1].step);
    trk[0].p[P_SLEN] = LEN;
    trk[1].p[P_SLEN] = LEN;
    for (i = 0; i < LEN; i++) {
        trk[0].step[i].time = i % 4u == 1u ? ST_REST : ST_NOTE;
        trk[0].step[i].n = i % 4u == 1u ? 0 : 1;
        trk[0].step[i].note[0] = i == LOW_STEP ? 36 : MAJ_NOTES[(i * 3u) % 8u];
        trk[0].step[i].vel = 100;
    }
}
static uint32_t attacks(void) { return phrase_count(0, LEN); }
static uint32_t base_hits(void) { uint32_t i, n = 0; for (i = 0; i < LEN; i++) n += sqm_hit(&trk[0].step[i]); return n; }
static int anchors_intact(void)
{
    uint32_t i;
    for (i = 0; i < LEN; i++)
        if (sqm_anchor(trk[0].step, LEN, i) && sqm_hit(&trk[0].step[i]) && rhythm_map[0][i] != 0)
            return 0;
    return 1;
}
static int offsets_near(void)
{
    uint32_t i;
    for (i = 0; i < LEN; i++)
        if (sqm_hit(&trk[0].step[i]) && rhythm_map[0][i] != RHYTHM_REST && rhythm_far(rhythm_offset(0, i)))
            return 0;
    return 1;
}

static void t_codec(void)
{
    sqm_end_t x;
    int ok = 1;
    uint32_t i, j;
    memset(&x, 0, sizeof x);
    for (i = 0; i < NSTEP; i++)
        sqm_pitch_set(&x, i, (int32_t)((i * 7u) % 25u) - 12);
    for (i = 0; i < NSTEP; i++)
        if (sqm_pitch_get(&x, i) != (int32_t)((i * 7u) % 25u) - 12)
            ok = 0;
    CHECK("pitch codec: -12..+12 round-trips at every step", ok);
    for (j = 0; j < 25; j++) {
        sqm_pitch_set(&x, 9, (int32_t)j - 12);
        ok &= sqm_pitch_get(&x, 9) == (int32_t)j - 12 && sqm_pitch_get(&x, 8) == (int32_t)((8u * 7u) % 25u) - 12 &&
              sqm_pitch_get(&x, 10) == (int32_t)((10u * 7u) % 25u) - 12;
    }
    CHECK("pitch codec: all 25 values at step 9, its neighbours unchanged", ok);
}

static void t_walk(void)
{
    sqm_end_t x, one;
    uint32_t seed, at, i, seen = 0, ok_one = 1, ok_scale = 1, ok_len = 1, any_none = 0;
    int32_t d, n;
    setup();
    for (seed = 0; seed < 300; seed++) {
        memset(&one, 0, sizeof one);
        at = sqm_pitch_walk(&trk[0], &one, seed * 2654435761u + 1u);
        if (at >= LEN) { any_none++; continue; }
        d = sqm_pitch_get(&one, at);
        n = trk[0].step[at].note[0] + d;
        seen |= 1u << at;
        if (!sqm_hit(&trk[0].step[at]) || at == LOW_STEP || trk[0].step[at].note[0] < 48u) ok_one = 0;
        if (d == 0 || d < -12 || d > 12 || n < 24 || n > 108) ok_len = 0;
        if (!in_scale(n)) ok_scale = 0;
        for (i = 0; i < LEN; i++)
            if (i != at && sqm_pitch_get(&one, i)) ok_one = 0;
    }
    CHECK("pitch walk: every move lands on a hit that is not the bass note and changes no other step", ok_one);
    CHECK("pitch walk: every move is 1..12 semitones, inside the playable range", ok_len);
    CHECK("pitch walk: every move lands in the scale", ok_scale);
    CHECK("pitch walk: a track with hits always finds a move", any_none == 0);
    CHECK("pitch walk: 300 seeds reach at least 4 different steps", __builtin_popcount(seen) >= 4);

    /* hundreds of moves in a row */
    memset(&x, 0, sizeof x);
    for (seed = 0; seed < 400; seed++)
        (void)sqm_pitch_walk(&trk[0], &x, seed * 40503u + 7u);
    {
        step_t out[NSTEP];
        int ok_far = 1, ok_apply = 1, moved = 0;
        memcpy(out, trk[0].step, sizeof out);
        sqm_pitch_apply(trk[0].step, LEN, &x, out);
        for (i = 0; i < LEN; i++) {
            d = sqm_pitch_get(&x, i);
            if (d < -12 || d > 12) ok_far = 0;
            if (out[i].note[0] != trk[0].step[i].note[0]) {
                moved++;
                if (!in_scale(out[i].note[0]) || out[i].note[0] > trk[0].step[i].note[0] + 12 || out[i].note[0] + 12 < trk[0].step[i].note[0]) ok_apply = 0;
            }
        }
        CHECK("pitch walk: 400 moves keep every offset within an octave", ok_far);
        CHECK("pitch walk: the applied notes are in the scale and within an octave of the stored ones", ok_apply);
        CHECK("pitch walk: the bass note never walks", out[LOW_STEP].note[0] == 36);
        CHECK("pitch walk: 400 moves changed at least 3 steps", moved >= 3);
    }

    /* SCALE CHR: nothing walks */
    scale_g = 0;
    memset(&one, 0, sizeof one);
    at = sqm_pitch_walk(&trk[0], &one, 12345u);
    CHECK("pitch walk: on SCALE CHR there is no move", at >= LEN && !sqm_pitch_get(&one, 0));
    scale_g = 1;
}

static void t_phrase(void)
{
    uint32_t seed, base, ok_cap = 1, ok_half = 1, ok_ord = 1, n0, i, moved = 0, changed = 0, calls = 0, ok_anchor = 1, ok_near = 1, ok_floor = 1;
    setup();
    base = base_hits();
    CHECK("phrase: the fixture has 12 attacks", base == 12u && attacks() == 12u);
    /* gaps: down to half the attacks, no further; the anchors stay */
    for (seed = 0; seed < 200; seed++) {
        (void)phrase_gap(0, seed * 17u + 5u);
        if (attacks() < (base + 1u) / 2u) ok_half = 0;
        if (!anchors_intact()) ok_anchor = 0;
    }
    n0 = attacks();
    CHECK("phrase: gaps never leave fewer than half the attacks, and do take some away", ok_half && n0 < base);
    /* repeats (a dropped attack comes back first, then a moved one answers at its old place): up to the cap, never past it */
    for (seed = 0; seed < 200; seed++) {
        (void)phrase_repeat(0, seed * 31u + 3u);
        if (attacks() > tarc_cap(base)) ok_cap = 0;
        if (!anchors_intact()) ok_anchor = 0;
    }
    CHECK("phrase: repeats restore what a gap dropped and stop at the density cap (tarc_cap)", ok_cap && attacks() > n0 && attacks() <= tarc_cap(base));
    /* order: swaps keep the number of attacks and every hit within two 16ths of its stored step */
    setup();
    for (seed = 0; seed < 200; seed++) {
        n0 = attacks();
        if (phrase_order(0, seed * 13u + 1u) && attacks() != n0) ok_ord = 0;
        if (!offsets_near()) ok_near = 0;
        if (!anchors_intact()) ok_anchor = 0;
    }
    CHECK("phrase: an order swap keeps the number of attacks", ok_ord);
    CHECK("phrase: every hit stays within two 16ths of its stored step", ok_near);
    /* the whole mutate cycle (6 phases) over many seeds */
    setup();
    for (seed = 0; seed < 600; seed++) {
        uint8_t before[NSTEP];
        memcpy(before, rhythm_map[0], sizeof before);
        calls++;
        changed += phrase_mutate(0, seed * 2654435761u + 11u) != 0;
        for (i = 0; i < LEN; i++)
            if (before[i] != rhythm_map[0][i])
                moved |= 1u << i;
        if (attacks() < 1u || attacks() > tarc_cap(base) + 1u) ok_floor = 0;
        if (!offsets_near()) ok_near = 0;
        if (!anchors_intact()) ok_anchor = 0;
    }
    CHECK("phrase: the anchors are never moved, repeated or dropped (a gap, a repeat, an order swap, a mutate cycle)", ok_anchor);
    CHECK("phrase: the mutate cycle keeps every hit within two 16ths", ok_near);
    CHECK("phrase: the mutate cycle keeps between 1 attack and the cap", ok_floor);
    CHECK("phrase: the mutate cycle reports a change in at least 40 % of its turns", changed * 10u >= calls * 4u);
    CHECK("phrase: the mutate cycle edits more than three different steps", __builtin_popcount(moved) > 3);
}

int main(void)
{
    t_codec();
    t_walk();
    t_phrase();
    printf("tidephrase: %d of %d checks failed\n", bad, checks);
    return bad != 0;
}
