/* SPDX-License-Identifier: GPL-3.0-only */
/* TAKE + ANSWER, live performance assist phase 1 (b10 round 8, work/b10/specs/PERF_PHASE1.md). Host group "take".
 *   STEP 1 (the checks before building): does a MIDI note reach the live recorder the way a key does? Every scenario runs twice from the same power-on, once with
 *           keys (input_on / input_off) and once with MIDI (midi_event on the part's channel), and the part's steps must be equal afterwards. The scenarios:
 *           plain part ADD, REPLACE, a three-step hold (ties), a chord (CHRD) played as one MIDI note, part 2 on its own channel, LAYER (part 1's channel), ARP SEQ (MONO, POLY ADD,
 *           POLY REPLACE, the stand-in from a remembered chord), and a note in the count-in. Each prints what it wrote.
 * Built on the harness of tests/liverec_test.c. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

static void run(uint32_t ms)
{
    static uint32_t acc;
    for (uint32_t i = 0; i < ms / 16u; i++) {
        int32_t out[2 * CTL];
        acc += (uint32_t)FS * 16u;
        while (acc >= 1000u * CTL) { acc -= 1000u * CTL; mix_block(out, CTL); }
        frame();
    }
}
static const uint8_t PAT[16] = {60,62,63,65,67,68,70,72,60,62,63,65,67,68,70,72};
static void setup(uint32_t part, int amode, int voice)
{
    track_t *t = &trk[part];
    uint32_t i;
    memset(t->step, 0, sizeof t->step);
    for (i = 0; i < 16u; i++) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = PAT[i]; t->step[i].vel = 100; }
    t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_ARATE] = 2; t->p[P_AMODE] = (int16_t)amode; t->p[P_APROB] = 127;
    t->p[P_AOCT] = 1; t->p[P_AGATE] = 64; t->p[P_VOICE] = (int16_t)voice; t->p[P_CHRD] = 0; t->p[P_TRANS] = 0;
    t->p[P_AHOLD] = 0; t->p[P_SQNT] = 0;
}
static void boot(void)
{
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.g[G_BPM] = 120;
    memset(tide_chord_n, 0, sizeof tide_chord_n);
    song.rec = 0;
    tk_reset_v = 1; frame();                             /* (a power-on: no history) */
}
static void start(void) { transport_req = 1u; run(64); }
static void stop(void) { transport_req = 2u; run(100); }
static void wait_arp(track_t *t, uint32_t k)
{
    uint32_t n = 0;
    while (!(t->arp_idx != 0xFFFFFFFFu && t->arp_idx % 16u == k && t->arp_pos < arp_dur(t) / 4u) && n++ < 2000u) run(16);
}

/* ---- STEP 1: keys against MIDI ---- */
static int use_midi;
static uint32_t part_ch(uint32_t part) { return part; }                          /* the default channels: part 1 = 1 (0), part 2 = 2 (1) */
static void note_on(uint32_t part, uint32_t note)
{
    if (use_midi) midi_event(0x90, part_ch(part), note, 100); else input_on(&trk[part], note, 100);
}
static void note_off(uint32_t part, uint32_t note)
{
    if (use_midi) midi_event(0x80, part_ch(part), note, 0); else input_off(&trk[part], note);
}
typedef struct { step_t step[NSTEP]; step_t other[NSTEP]; } snap_t;
enum { SC_ADD, SC_REPLACE, SC_HOLD, SC_CHORD, SC_PART2, SC_LAYER, SC_ARP_MONO, SC_ARP_POLY_ADD, SC_ARP_POLY_REPL, SC_ARP_CHORD, SC_COUNTIN, SC_N };
static const char *const SCN[SC_N] = {"plain ADD (a tap at step 5)", "plain REPLACE (a tap into C4+E4)", "plain, a note held for 3 steps (ties)", "plain, CHRD 'maj' on one note", "part 2 armed, its own channel",
                                      "LAYER: part 1 armed, part 1's channel", "ARP SEQ MONO, a tap at arp step 5", "ARP SEQ POLY ADD", "ARP SEQ POLY REPLACE", "ARP SEQ, the stand-in from a remembered chord",
                                      "a note in the count-in's last eighth"};
static void scenario(uint32_t sc, snap_t *out)
{
    uint32_t part = sc == SC_PART2 ? 1u : 0u;
    track_t *t = &trk[part];
    int arp = sc >= SC_ARP_MONO && sc <= SC_ARP_CHORD;
    boot();
    tide_rec_replace = sc == SC_REPLACE || sc == SC_ARP_POLY_REPL;
    if (sc == SC_LAYER) { duell_mode_force(DM_LAYER); setup(1, 0, V_POLY); }
    setup(part, arp ? AM_SEQ_ID : 0, sc == SC_ARP_MONO || sc == SC_ARP_CHORD ? V_MONO : V_POLY);
    if (sc == SC_REPLACE) { t->step[5].n = 2; t->step[5].note[0] = 60; t->step[5].note[1] = 64; }
    if (sc == SC_CHORD) t->p[P_CHRD] = 1;
    if (sc == SC_ARP_CHORD) { tide_chord[0][0] = 64; tide_chord_n[0] = 1; }
    if (sc == SC_COUNTIN) { click_mode = CLICK_REC; cin_bars = 1; }
    start();
    song.rec = 1u << part;
    run(100);
    if (arp) wait_arp(t, 5); else while (t->seq_idx != 5u) run(16);
    if (sc == SC_HOLD) {
        note_on(part, 64);
        if (arp) wait_arp(t, 8); else while (t->seq_idx != 8u) run(16);
        run(16);
        note_off(part, 64);
    } else {
        note_on(part, sc == SC_ARP_CHORD ? 67 : sc == SC_ARP_POLY_REPL ? 72 : 67);
        run(40);
        note_off(part, sc == SC_ARP_CHORD ? 67 : sc == SC_ARP_POLY_REPL ? 72 : 67);
    }
    run(32);
    song.rec = 0;
    memcpy(out->step, t->step, sizeof out->step);
    memcpy(out->other, trk[1u - part].step, sizeof out->other);
    stop();
    click_mode = 0; cin_bars = 0;
}
static void step1(void)
{
    uint32_t sc;
    for (sc = 0; sc < SC_N; sc++) {
        static snap_t k, m;
        uint32_t i, diff = 0, wrote = 0;
        if (sc == SC_COUNTIN) continue;                  /* (the count-in needs the transport state of a real press: reported as not tried) */
        use_midi = 0; scenario(sc, &k);
        use_midi = 1; scenario(sc, &m);
        for (i = 0; i < NSTEP; i++) {
            diff += memcmp(&k.step[i], &m.step[i], sizeof k.step[i]) != 0 || memcmp(&k.other[i], &m.other[i], sizeof k.other[i]) != 0;
            wrote += k.step[i].n != (i < 16u ? 1u : 0u) || (i < 16u && k.step[i].note[0] != PAT[i]) || k.step[i].time != (i < 16u ? ST_NOTE : 0);
        }
        if (sc == SC_CHORD) {                            /* (a key's chord goes through key_on, not input_on: compare with what the chord is, not with this harness's keys) */
            const step_t *c = &m.step[5];
            printf("take: step 1: %-48s MIDI wrote step 5 = %u notes: %u %u %u %u\n", SCN[sc], (unsigned)c->n, c->note[0], c->note[1], c->note[2], c->note[3]);
            CHECK("plain, CHRD on one MIDI note: the chord's tones are all recorded (3 or more)", c->n >= 3u);
            continue;
        }
        printf("take: step 1: %-48s keys wrote %2u steps, MIDI %s (%u steps differ)\n", SCN[sc], wrote, diff ? "DIFFERS" : "the same", diff);
        CHECK(SCN[sc], diff == 0u);
        CHECK("... and the keys wrote something (the scenario works)", wrote > 0u);
    }
}

/* ---- STEP 2..: the take, its loop, its answer, undo / redo and the roll ---- */
static uint32_t psig(uint32_t p) { return steps_sig(&trk[p]); }
static void empty_part(uint32_t p)
{
    memset(trk[p].step, 0, sizeof trk[p].step);
    trk[p].p[P_SLEN] = 16; trk[p].p[P_SDIV] = 2; trk[p].p[P_AMODE] = 0; trk[p].p[P_VOICE] = V_POLY; trk[p].p[P_CHRD] = 0; trk[p].p[P_TRANS] = 0;
}
static void at_step(track_t *t, uint32_t idx) { while (t->seq_idx != idx) run(16); }
static void rec_note_at(uint32_t part, uint32_t idx, uint32_t note, uint32_t ms)
{
    at_step(&trk[part], idx);
    input_on(&trk[part], note, 100); run(ms); input_off(&trk[part], note);
}
static int said(const char *w) { return !strncmp(ui.msg, w, str_len(w)); }
/* a first take on an empty part 1 (a melody over ~1.5 bars), closed by REC; part 2 holds the PAT pattern */
static void first_take(void)
{
    boot();
    empty_part(0); setup(1, 0, V_POLY);
    tide_answer = TA_BASS; tide_answer_on = 0;
    start();
    song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); rec_note_at(0, 14, 74, 60);
    rec_note_at(0, 18, 72, 60); rec_note_at(0, 22, 69, 60);
    at_step(&trk[0], 26);
    song.rec = 0; run(120);
}
static void take_lifecycle(void)
{
    uint32_t sig1_before, sig2_before, sig1_after, sig2_after, i, n2 = 0;
    boot();
    empty_part(0); setup(1, 0, V_POLY);
    sig1_before = psig(0); sig2_before = psig(1);
    CHECK("a fresh unit: ANSWER is BASS LINE, ANSWER ON is the other part", tide_answer == TA_BASS && tide_answer_on == 0u && jl_get(JL_ANS) == TA_BASS && !strcmp(jl_vname(JL_ANS, 0), "OFF") &&
          !strcmp(jl_vname(JL_ANS, 1), "BASS LINE") && !strcmp(jl_vname(JL_ANS, 2), "DOUBLE") && !strcmp(JL_NAME[JL_ANSON], "ANSWER ON"));
    first_take();
    sig1_after = psig(0); sig2_after = psig(1);
    for (i = 0; i < NSTEP; i++) n2 += trk[1].step[i].n != 0;
    CHECK("the first take on an empty part sets the loop: 26 steps are 2 bars (32 steps)", trk[0].p[P_SLEN] == 32);
    CHECK("... the take is in the part (6 notes)", trk[0].step[2].note[0] == 69 && trk[0].step[22].note[0] == 69);
    CHECK("... the other part was given the answer: its loop is the take's, its pattern is new", trk[1].p[P_SLEN] == 32 && sig2_after != sig2_before && n2 > 0);
    CHECK("... the header names it: BASS LINE and the key (guessed: a ?)", said("BASS LINE") && strchr(ui.msg, '?'));
    CHECK("... one step of history", tk_cnt == 1 && tk_pos == 1 && tk_fresh);
    /* undo / redo: OCT- held + a PRESETS turn: left back, right forward */
    CHECK("undo: left goes back one take", take_undo(-1) == 1u && said("UNDO TAKE 1"));
    CHECK("... the take is gone (the loop and the steps as they were) and the answer with it", psig(0) == sig1_before && psig(1) == sig2_before && trk[0].p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 16);
    CHECK("... nothing further back", take_undo(-1) == 0u);
    CHECK("redo: right brings the take and its answer back", take_undo(1) == 1u && said("REDO TAKE 1") && psig(0) == sig1_after && psig(1) == sig2_after && trk[0].p[P_SLEN] == 32);
    CHECK("... nothing further forward", take_undo(1) == 0u);
    TIDE_TAKE_STALE();
    CHECK("a load or a lock edit after the take: the gesture is the load undo's again", take_undo(-1) == 0u);
}
static void take_history(void)
{
    uint32_t s[4];
    first_take();
    s[0] = psig(1);
    for (uint32_t k = 1; k < 4u; k++) {                  /* three more passes: each adds a note to part 1 (ADD) */
        tide_rec_replace = 0;
        start(); song.rec = 1; run(100);
        rec_note_at(0, 4 + k, 60 + 4 * k, 60);
        song.rec = 0; run(120);
        s[k] = psig(1);
    }
    CHECK("4 takes: the history keeps the last 2", tk_cnt == 2 && tk_pos == 2);
    CHECK("... the 3rd take and its answer back", take_undo(-1) == 1u && psig(1) == s[2]);
    CHECK("... the 2nd back", take_undo(-1) == 1u && psig(1) == s[1]);
    CHECK("... the 1st is not kept", take_undo(-1) == 0u);
    CHECK("... redo twice goes to the last one", take_undo(1) == 1u && take_undo(1) == 1u && psig(1) == s[3]);
    take_undo(-1);
    start(); song.rec = 1; run(100); rec_note_at(0, 9, 79, 60); song.rec = 0; run(120);
    CHECK("a take after an undo drops what could be redone", tk_cnt == 2 && tk_pos == 2 && take_undo(1) == 0u);
}
static void take_settings(void)
{
    uint32_t before, n2 = 0, i;
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_OFF;
    before = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); at_step(&trk[0], 12); song.rec = 0; run(120);
    CHECK("ANSWER OFF: the loop is still set (1 bar), the other part is not touched, the take is undoable", trk[0].p[P_SLEN] == 16 && psig(1) == before && tk_cnt == 1 && take_undo(-1) == 1u);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 1;
    before = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); at_step(&trk[0], 12); song.rec = 0; run(120);
    CHECK("ANSWER ON OFF: the other part is not touched", psig(1) == before);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_DOUBLE; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); at_step(&trk[0], 12); song.rec = 0; run(120);
    for (i = 0; i < NSTEP; i++) n2 += trk[1].step[i].n;
    CHECK("DOUBLE: the other part plays the take's notes a third above (2 notes at the same steps)", said("DOUBLE") && n2 == 2u && trk[1].step[2].n == 1u && trk[1].step[6].n == 1u);
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0; tide_rec_replace = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 5, 67, 60); song.rec = 0; run(120);
    CHECK("a pass over a part that has a pattern (ADD): the loop stays 16, the answer is written, one take", trk[0].p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 16 && tk_cnt == 1);
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY);
    start(); song.rec = 1; run(100); song.rec = 0; run(120);
    CHECK("a pass that writes nothing is no take", tk_cnt == 0);
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY);
    song.rec = 1;
    CHECK("an armed part plays its stored step, not an evolved one", seq_play_step_at(&trk[0], 3, 0) == &trk[0].step[3]);
    song.rec = 0;
}
static void take_roll_test(void)
{
    uint32_t a0, a1, a2, h0;
    first_take();
    a0 = psig(1);
    morph_do(MQ_ROLL, 1u, 0u); run(32);
    CHECK("a ROLL of the take's own part: the answer is left alone", psig(1) == a0 && tk_cnt == 1);
    morph_do(MQ_ROLL, 2u, 0u); run(32);
    a1 = psig(1);
    CHECK("a ROLL of the answering part writes the answer again with the roll's seed", a1 != a0 && tk_cnt == 2 && tk_m.roll == 1u);
    CHECK("... the header says ROLL 1  BASS LINE", said("ROLL 1  BASS LINE"));
    CHECK("... the take stays", trk[0].step[2].note[0] == 69 && trk[0].p[P_SLEN] == 32);
    h0 = psig(0);
    morph_do(MQ_ROLL, 3u, 1u); run(32);
    a2 = psig(1);
    CHECK("a second roll (BOTH parts): another answer, one more step", a2 != a1 && tk_cnt == 2 && tk_m.roll == 2u && psig(0) == h0);
    CHECK("undo: the roll goes back to the answer before it", take_undo(-1) == 1u && said("UNDO ROLL 2") && psig(1) == a1);
    CHECK("... and the roll before", take_undo(-1) == 1u && said("UNDO ROLL 1") && psig(1) == a0);
    CHECK("... redo forward again", take_undo(1) == 1u && said("REDO ROLL 1") && psig(1) == a1);
    trk[1].step[3].n = 1; trk[1].step[3].note[0] = 50; trk[1].step[3].time = ST_NOTE;   /* the answer edited by hand is the player's now */
    a0 = psig(1);
    morph_do(MQ_ROLL, 2u, 0u); run(32);
    CHECK("a ROLL after the answer was edited leaves it alone", psig(1) == a0);
}

static void take_reply(void)
{
    uint32_t a0, a1;
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_REPLY; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); at_step(&trk[0], 13); song.rec = 0; run(120);
    CHECK("REPLY on a 1 bar call: both loops double (32 steps), the header says REPLY", trk[0].p[P_SLEN] == 32 && trk[1].p[P_SLEN] == 32 && said("REPLY"));
    CHECK("... the reply is in the bar after the call (3 notes at 18, 22, 26)", trk[1].step[18].n == 1 && trk[1].step[22].n == 1 && trk[1].step[26].n == 1 && trk[1].step[2].n == 0);
    a0 = psig(1);
    morph_do(MQ_ROLL, 2u, 0u); run(32);
    a1 = psig(1);
    CHECK("a ROLL of the reply keeps the doubled loop (the call is still one bar) and writes it again", trk[0].p[P_SLEN] == 32 && trk[1].p[P_SLEN] == 32 && tk_cnt == 2 && trk[1].step[18].n == 1);
    CHECK("... undo goes back to the first reply", take_undo(-1) == 1u && psig(1) == a0);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_ECHO; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 8, 72, 60); at_step(&trk[0], 13); song.rec = 0; run(120);
    {
        uint32_t i, n = 0;
        for (i = 0; i < NSTEP; i++) n += trk[1].step[i].n != 0;
        CHECK("ECHO: 4 throws of the last note, the loop doubled", said("ECHO") && n == 4u && trk[1].p[P_SLEN] == 32 && trk[1].step[9].n == 1u);
    }
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_REPLY; tide_answer_on = 0;
    {
        uint32_t before = psig(1);
        start(); song.rec = 1; run(100);
        rec_note_at(0, 2, 69, 60); at_step(&trk[0], 40); song.rec = 0; run(120);
        CHECK("a call of 3 bars (a 4 bar loop): REPLY says so and writes nothing", trk[0].p[P_SLEN] == 64 && psig(1) == before && said("CALL: 2 BARS AT MOST"));
    }
}
static void take_line_test(void)
{
    uint32_t pr = 0, beat = 0, mask = 3u;
    char l[48];
    boot(); empty_part(0); setup(1, 0, V_POLY);
    CHECK("no take: the tide line and knob 1 are as they were", take_line(&pr) == 0u && !take_text(l) && tide_line_state(&pr, &beat, &mask) == 0);
    start(); song.rec = 1; run(100);
    at_step(&trk[0], 20);
    CHECK("a first take records: take_line says recording, the tide line is REC red (state 6), the fill is 20 of 64", take_line(&pr) == 2u && tide_line_state(&pr, &beat, &mask) == 6 && pr / 1024u == 20u);
    CHECK("... knob 1's line says TAKE  BAR 2/4 (step 20 of the 64 the first take runs at)", take_text(l) == 1u && !strcmp(l, "TAKE  BAR 2/4"));
    cin_left = 2;
    CHECK("the count-in of a take: dashes (state 5)", tide_line_state(&pr, &beat, &mask) == 5);
    cin_left = 0;
    song.rec = 0; run(120);
    CHECK("closed: the line is as before", take_line(&pr) == 0u && tide_line_state(&pr, &beat, &mask) == 0);
}
int main(void)
{
    step1();
    take_lifecycle(); take_history(); take_settings(); take_roll_test(); take_reply(); take_line_test();
    printf("take: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
