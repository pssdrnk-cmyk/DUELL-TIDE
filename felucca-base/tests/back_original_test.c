/* SPDX-License-Identifier: GPL-3.0-only */
/* B16 (owner note 5, 2026-11-19): the ways back to the original, each through the panel as the player does it, on a factory multi with the transport playing and a glide moving.
 *   K1 MORPH    OCT- held + K1 on HOME's MORPH page: the glide stops, the fader at A, End A the Original, End B the one the multi loaded, the pattern's variation gone, BACK TO ORIGINAL
 *   K1 TIDE     the same on AUTO TIDE
 *   K2..K4      OCT- held + K2 / K3 / K4 on MORPH: the knob's parameters back to the loaded sound's
 *   TAKES       OCT- held + a PRESETS turn on MORPH: back along the last 2 takes (left), forward (right)
 *   RELOAD      the multi loaded again: End A, End B as stored, the variation gone
 *   ROLL UNDO   OCT+ held + K1 rolls End B; OCT- held + a PRESETS turn left puts the End B before back, right the roll again; a lock edit or a take after the roll is the newest edit then
 * Host group "backorig". */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
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
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("backorig: FAIL: %s\n", label); } else printf("backorig: ok    %s\n", label); } while (0)

static void run(uint32_t ms)                             /* time passing with the audio running (the sequencer moves) */
{
    static uint32_t acc;
    for (uint32_t i = 0; i < ms / 16u; i++) {
        int32_t out[2 * CTL];
        acc += (uint32_t)FS * 16u;
        while (acc >= 1000u * CTL) { acc -= 1000u * CTL; mix_block(out, CTL); }
        frame();
    }
}
static int said(const char *w) { return ui.msg_t && !strncmp(ui.msg, w, str_len(w)); }
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
    transport_req = 2u; run(48);
    song.rec = 0;
    tk_reset_v = 1; frame();
}
static void load_multi(uint32_t slot) { project_load(MIDX(slot)); run(200); }

/* a part's two ends as they are */
typedef struct { int8_t h[2][P_COUNT], hx[2][NX]; uint8_t fh[2][NFM]; uint32_t fader; } ends_t;
static void ends_of(uint32_t pi, ends_t *e)
{
    const morph_part_t *mp = &mph[pi];
    uint32_t x;
    for (x = 0; x < 2u; x++) {
        memcpy(e->h[x], morph_cur(mp, x), P_COUNT);
        memcpy(e->hx[x], morph_curx(mp, x), NX);
        memcpy(e->fh[x], mp->fh[x][mp->top[x]], NFM);
    }
    e->fader = mp->fader;
}
static int end_same(const ends_t *a, const ends_t *b, uint32_t x)
{
    return !memcmp(a->h[x], b->h[x], P_COUNT) && !memcmp(a->hx[x], b->hx[x], NX) && !memcmp(a->fh[x], b->fh[x], NFM);
}
static int a_is_original(uint32_t pi)
{
    const morph_part_t *mp = &mph[pi];
    return !memcmp(morph_cur(mp, 0), mp->base, P_COUNT) && !memcmp(morph_curx(mp, 0), mp->base + XW_BASE, NX) && !mp->tok[0][mp->top[0]];
}

/* the gestures */
static void rest(void) { frames(480); }                  /* (the knobs and PRESETS still: the next gesture is a new one) */
static void oct_minus_knob(uint32_t en)
{
    btn_down(B_OCTDN); frame();
    turn(en, 1);
    btn_up(B_OCTDN); frames(64);
    rest();
}
static void oct_minus_presets(int32_t s)
{
    btn_down(B_OCTDN); frame();
    turn(EN_PRESET, s);
    btn_up(B_OCTDN); frames(64);
}
static void oct_plus_k1(void)
{
    btn_down(B_OCTUP); frame();
    turn(EN_K1, 1);
    btn_up(B_OCTUP); frames(64);
    rest();
}

/* the multi playing, and moved every way TIDE and the player move it: a glide on its way to B, End A rolled (MIDI ROLL A), the SEQ fader off A */
static void moved(void)
{
    go_title("MORPH"); frames(32);
    SEL_BOTH = 0; song.sel = 0;
    transport_req = 1u; run(64);
    glide_launch(1u, 1u, 0u);
    run(3000);
    morph_do(MQ_ROLL, 1u, 0u);
    seqm_set_fader(0, 90u);
    run(200);
}

static void k1_home(int on_tide)
{
    ends_t ld, now;
    uint32_t i, steps0, steps1;
    power();
    load_multi(0);
    ends_of(0, &ld);
    steps0 = steps_sig(&trk[0]);
    moved();
    ends_of(0, &now);
    CHECK(on_tide ? "AUTO TIDE: (set-up) the glide runs, the fader is off A, End A rolled, the SEQ fader off A" : "MORPH: (set-up) the glide runs, the fader is off A, End A rolled, the SEQ fader off A",
          gl.on && now.fader && !a_is_original(0) && sqm[0].fader);
    if (on_tide) {
        go_title("AUTO TIDE"); frames(32);
    }
    oct_minus_knob(EN_K1);
    for (i = 0; i < 4u && !said("BACK TO ORIGINAL"); i++)
        frame();
    ends_of(0, &now);
    steps1 = steps_sig(&trk[0]);
    printf("backorig: %s OCT- + K1: glide %s, fader %u, End A %s, End B %s, SEQ fader %u, steps %s\n", on_tide ? "AUTO TIDE" : "MORPH", gl.on ? "runs" : "stopped", now.fader,
           a_is_original(0) ? "the Original" : "moved", end_same(&now, &ld, 1) ? "as the multi loaded it" : "NOT the multi's", (unsigned)sqm[0].fader, steps1 == steps0 ? "as loaded" : "changed");
    CHECK(on_tide ? "AUTO TIDE OCT- + K1: the glide stops, the fader at A, End A the Original" : "MORPH OCT- + K1: the glide stops, the fader at A, End A the Original",
          !gl.on && now.fader == 0u && a_is_original(0));
    CHECK(on_tide ? "AUTO TIDE OCT- + K1: End B is the one the multi loaded" : "MORPH OCT- + K1: End B is the one the multi loaded", end_same(&now, &ld, 1));
    CHECK(on_tide ? "AUTO TIDE OCT- + K1: the pattern as loaded, no variation (SEQ fader at A)" : "MORPH OCT- + K1: the pattern as loaded, no variation (SEQ fader at A)",
          steps1 == steps0 && sqm[0].fader == 0u && !sqm[0].aon);
    CHECK(on_tide ? "AUTO TIDE OCT- + K1: the header says BACK TO ORIGINAL" : "MORPH OCT- + K1: the header says BACK TO ORIGINAL", said("BACK TO ORIGINAL"));
    transport_req = 2u; run(100);
}

static void k2_k4(void)
{
    const kn_e *en;
    uint32_t k, j, ok = 1;
    int16_t before[KN_NP];
    power();
    load_multi(0);
    go_title("MORPH"); frames(32);
    SEL_BOTH = 0; song.sel = 0;
    for (j = 0; j < KN_NP; j++)
        before[j] = trk[0].p[KN_ID[j]];
    for (k = 1; k < 4u; k++) {
        turn(EN_K1 + k, 6); turn(EN_K1 + k, 6);
        rest();
    }
    for (k = 1; k < 4u; k++)
        oct_minus_knob(EN_K1 + k);
    for (k = 1; k < 4u; k++) {                           /* (every parameter of each knob's macro, the set the multi's genre gives) */
        en = KN_E[kn_cur()][k - 1u];
        for (j = 0; j < 3u; j++)
            if (en[j].mul && en[j].ix < KN_NP && trk[0].p[KN_ID[en[j].ix]] != before[en[j].ix]) {
                printf("backorig: K%u's parameter %u is %d, loaded %d\n", k + 1u, (unsigned)KN_ID[en[j].ix], trk[0].p[KN_ID[en[j].ix]], before[en[j].ix]);
                ok = 0;
            }
    }
    CHECK("MORPH OCT- + K2 / K3 / K4: each knob's parameters back to the loaded sound's", ok);
}

/* a take on part 1 (REC armed, a few notes, REC off): the answer lands at the next bar line */
static void land(void) { uint32_t n = 0; while (tk_pend && n++ < 300u) run(16); run(32); }
static void at_step(track_t *t, uint32_t idx) { uint32_t n = 0; while (t->seq_idx != idx && n++ < 1000u) run(16); }
static void take(uint32_t base)
{
    static const uint8_t ST[4] = {2, 6, 10, 14};
    uint32_t i;
    song.rec = 1; run(100);
    for (i = 0; i < 4u; i++) {
        at_step(&trk[0], ST[i]);
        input_on(&trk[0], base + 2u * i, 100); run(60); input_off(&trk[0], base + 2u * i);
    }
    at_step(&trk[0], 15);
    song.rec = 0; run(48); land();
}
static void empty_part0(void)
{
    memset(trk[0].step, 0, sizeof trk[0].step);
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2; trk[0].p[P_AMODE] = 0; trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_CHRD] = 0;
}
static void takes(void)
{
    uint32_t s0, s1, s2;
    power();
    load_multi(0);
    go_title("MORPH"); frames(32);
    SEL_BOTH = 0; song.sel = 0;
    tide_answer = TA_BASS; tide_answer_on = 1;
    empty_part0();
    s0 = steps_sig(&trk[0]);
    transport_req = 1u; run(64);
    take(60); s1 = steps_sig(&trk[0]);
    take(67); s2 = steps_sig(&trk[0]);
    transport_req = 2u; run(100);
    CHECK("TAKES: (set-up) two takes, two history steps", tk_cnt == 2u && s1 != s0 && s2 != s1);
    oct_minus_presets(-1);
    CHECK("TAKES OCT- + PRESETS left: the take before (UNDO TAKE 2)", steps_sig(&trk[0]) == s1 && said("UNDO TAKE"));
    rest();
    oct_minus_presets(-1);
    CHECK("TAKES .. left again: before the first take", steps_sig(&trk[0]) == s0 && said("UNDO TAKE"));
    rest();
    oct_minus_presets(1);
    rest();
    oct_minus_presets(1);
    CHECK("TAKES .. right twice: the last take back (REDO TAKE)", steps_sig(&trk[0]) == s2 && said("REDO TAKE"));
    rest();
}

static void reload(void)
{
    ends_t ld, now;
    uint32_t steps0;
    power();
    load_multi(0);
    ends_of(0, &ld);
    steps0 = steps_sig(&trk[0]);
    moved();
    load_multi(0);
    ends_of(0, &now);
    CHECK("RELOAD: the fader at A, End A the Original, End B as the multi stores it", now.fader == 0u && a_is_original(0) && end_same(&now, &ld, 1));
    CHECK("RELOAD: the pattern as stored, the variation gone (SEQ fader at A)", steps_sig(&trk[0]) == steps0 && sqm[0].fader == 0u);
    transport_req = 2u; run(100);
}

static void roll_undo(void)
{
    ends_t ld, r1, now;
    uint32_t sa, i;
    power();
    load_multi(0);
    go_title("MORPH"); frames(32);
    SEL_BOTH = 0; song.sel = 0;
    ends_of(0, &ld);
    oct_plus_k1();
    ends_of(0, &r1);
    CHECK("ROLL: (set-up) OCT+ + K1 rolled End B, End A and the fader stay", !end_same(&r1, &ld, 1) && end_same(&r1, &ld, 0) && r1.fader == ld.fader);
    oct_minus_presets(-1);
    ends_of(0, &now);
    CHECK("ROLL UNDO OCT- + PRESETS left: End B before the roll back (UNDO ROLL B)", end_same(&now, &ld, 1) && end_same(&now, &ld, 0) && said("UNDO ROLL B"));
    rest();
    oct_minus_presets(1);
    ends_of(0, &now);
    CHECK("ROLL UNDO .. right: the rolled End B again (REDO ROLL B)", end_same(&now, &r1, 1) && said("REDO ROLL B"));
    rest();
    oct_minus_presets(1);
    CHECK("ROLL UNDO .. right again: NOTHING TO REDO, the load undo not touched", end_same(&now, &r1, 1) && said("NOTHING TO REDO"));
    rest();
    /* rolls in a row: one step back, the End B of the roll before */
    oct_plus_k1();
    ends_of(0, &now);
    oct_plus_k1();
    oct_minus_presets(-1);
    ends_of(0, &r1);
    CHECK("ROLL UNDO after two rolls: the End B of the first of them (one step)", end_same(&now, &r1, 1));
    rest();
    oct_minus_presets(-1);
    CHECK("ROLL UNDO .. left again: no further roll back (the gesture is the older edits')", !said("UNDO ROLL B"));
    rest();
    /* BOTH: both parts' End B back */
    SEL_BOTH = 1; frames(32);
    {
        ends_t b0, b1, c0, c1;
        ends_of(0, &b0); ends_of(1, &b1);
        oct_plus_k1();
        oct_minus_presets(-1);
        ends_of(0, &c0); ends_of(1, &c1);
        CHECK("ROLL UNDO with BOTH: each part's End B back", end_same(&b0, &c0, 1) && end_same(&b1, &c1, 1) && said("UNDO ROLL B"));
        rest();
    }
    SEL_BOTH = 0; frames(32);
    /* a later edit is the newest: a sound load after the roll, then the gesture is the load undo's */
    oct_plus_k1();
    sa = trk[0].preset;
    turn(EN_PRESET, 1); frames(64); rest();
    oct_minus_presets(-1);
    CHECK("a sound load after the roll: OCT- + PRESETS is the load undo (the sound before back), not the roll's", trk[0].preset == sa && !said("UNDO ROLL"));
    rest();
    /* a take after the roll is the newest: the take first, then the roll */
    load_multi(0);
    go_title("MORPH"); frames(32);
    tide_answer = TA_BASS; tide_answer_on = 1;
    empty_part0();
    ends_of(0, &ld);
    oct_plus_k1();
    ends_of(0, &r1);
    transport_req = 1u; run(64);
    take(60);
    transport_req = 2u; run(100);
    oct_minus_presets(-1);
    ends_of(0, &now);
    CHECK("a take after the roll: OCT- + PRESETS undoes the take first, End B stays the rolled one", said("UNDO TAKE") && end_same(&now, &r1, 1));
    rest();
    oct_minus_presets(-1);
    ends_of(0, &now);
    CHECK("  .. the roll is not undone under the take any more (the take was newer)", !said("UNDO ROLL") && end_same(&now, &r1, 1));
    rest();
    /* a roll after a take: the roll first, then the take */
    load_multi(0);
    go_title("MORPH"); frames(32);
    empty_part0();
    transport_req = 1u; run(64);
    take(60);
    transport_req = 2u; run(100);
    ends_of(0, &ld);
    oct_plus_k1();
    oct_minus_presets(-1);
    ends_of(0, &now);
    CHECK("a roll after a take: the roll is undone first", said("UNDO ROLL B") && end_same(&now, &ld, 1));
    rest();
    oct_minus_presets(-1);
    CHECK("  .. then the take (UNDO TAKE)", said("UNDO TAKE"));
    rest();
    oct_minus_presets(1);
    rest();
    oct_minus_presets(1);
    ends_of(0, &now);
    CHECK("  .. right twice: the take, then the roll again", said("REDO ROLL B") && !end_same(&now, &ld, 1));
    rest();
    /* an edit of End B by hand after the roll: the B is the player's, no roll undo */
    load_multi(0);
    go_title("MORPH"); frames(32);
    oct_plus_k1();
    morph_do(MQ_FADER, 1u, 127u); frames(32);
    for (i = 0; i < 3u; i++)
        turn(EN_K2, 4);
    rest();
    ends_of(0, &r1);
    oct_minus_presets(-1);
    ends_of(0, &now);
    CHECK("End B edited by hand after the roll: no roll undo (the edit stays)", end_same(&now, &r1, 1) && !said("UNDO ROLL"));
    rest();
    /* HOME after the roll: back to the original, no roll undo */
    morph_do(MQ_FADER, 1u, 0u);
    oct_plus_k1();
    oct_minus_knob(EN_K1);
    oct_minus_presets(-1);
    CHECK("HOME after the roll: the roll's undo is gone with the ends", !said("UNDO ROLL"));
    rest();
}

/* a sound loaded on the part after the multi: OCT- + K1 gives that sound's own End B, not the multi's */
static void k1_after_sound_load(void)
{
    ends_t ld, now;
    power();
    load_multi(0);
    go_title("MORPH"); frames(32);
    SEL_BOTH = 0; song.sel = 0;
    turn(EN_PRESET, 1); frames(64); rest();
    ends_of(0, &ld);
    oct_plus_k1();
    morph_do(MQ_FADER, 1u, 80u); frames(32);
    oct_minus_knob(EN_K1);
    ends_of(0, &now);
    CHECK("a sound loaded after the multi: OCT- + K1 gives the sound's own End B, the fader at A", now.fader == 0u && end_same(&now, &ld, 1) && a_is_original(0));
}

int main(void)
{
    k1_after_sound_load();
    k1_home(0);
    k1_home(1);
    k2_k4();
    takes();
    reload();
    roll_undo();
    printf("backorig: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
