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
static uint32_t boot_bpm = 120;
static void boot(void)
{
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.g[G_BPM] = (int16_t)boot_bpm;
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
static void at_step(track_t *t, uint32_t idx) { uint32_t n = 0; while (t->seq_idx != idx && n++ < 1000u) run(16); if (n > 1000u) printf("take: at_step %u never came%c", idx, 10); }
static void rec_note_at(uint32_t part, uint32_t idx, uint32_t note, uint32_t ms)
{
    at_step(&trk[part], idx);
    input_on(&trk[part], note, 100); run(ms); input_off(&trk[part], note);
}
static int said(const char *w) { return ui.msg_t && !strncmp(ui.msg, w, str_len(w)); }   /* (shown now: the text stays in the buffer after its frames are over) */
/* b10 assist2: an answer is written in the other part on the next bar line (tide_take.c tk_land) */
static void land(void) { uint32_t n = 0; while (tk_pend && n++ < 300u) run(16); run(32); }
static void rec_off(void) { song.rec = 0; run(48); land(); }
/* a take on an empty part 1 whose LEN is 2 bars (a melody over ~1.5 bars), closed by REC; part 2 holds the PAT pattern */
static void first_take(void)
{
    boot();
    empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY);
    tide_answer = TA_BASS; tide_answer_on = 0;
    start();
    song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); rec_note_at(0, 14, 74, 60);
    rec_note_at(0, 18, 72, 60); rec_note_at(0, 22, 69, 60);
    at_step(&trk[0], 26);
    rec_off();
}
static void take_lifecycle(void)
{
    uint32_t sig1_before, sig2_before, sig1_after, sig2_after, i, n2 = 0;
    boot();
    empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY);
    sig1_before = psig(0); sig2_before = psig(1);
    CHECK("a fresh unit: ANSWER is BASS LINE, ANSWER ON is the other part", tide_answer == TA_BASS && tide_answer_on == 0u && jl_get(JL_ANS) == TA_BASS && !strcmp(jl_vname(JL_ANS, 0), "OFF") &&
          !strcmp(jl_vname(JL_ANS, 1), "BASS LINE") && !strcmp(jl_vname(JL_ANS, 2), "DOUBLE") && !strcmp(JL_NAME[JL_ANSON], "ANSWER ON"));
    first_take();
    sig1_after = psig(0); sig2_after = psig(1);
    for (i = 0; i < NSTEP; i++) n2 += trk[1].step[i].n != 0;
    CHECK("the loop is the part's LEN (2 bars, 32 steps), whatever the take's length", trk[0].p[P_SLEN] == 32);
    CHECK("... the take is in the part (6 notes)", trk[0].step[2].note[0] == 69 && trk[0].step[22].note[0] == 69);
    CHECK("... the other part was given the answer on the bar line: its loop is the take's, its pattern is new", trk[1].p[P_SLEN] == 32 && sig2_after != sig2_before && n2 > 0);
    CHECK("... the header names it: BASS LINE and the key", said("BASS LINE"));
    CHECK("... one step of history", tk_cnt == 1 && tk_pos == 1 && tk_fresh);
    /* undo / redo: OCT- held + a PRESETS turn: left back, right forward */
    CHECK("undo: left goes back one take", take_undo(-1) == 1u && said("UNDO TAKE 1"));
    CHECK("... the take is gone and the answer with it", psig(0) == sig1_before && psig(1) == sig2_before && trk[0].p[P_SLEN] == 32 && trk[1].p[P_SLEN] == 16);
    CHECK("... nothing further back", take_undo(-1) == 0u);
    CHECK("redo: right brings the take and its answer back", take_undo(1) == 1u && said("REDO TAKE 1") && psig(0) == sig1_after && psig(1) == sig2_after && trk[1].p[P_SLEN] == 32);
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
        song.rec = 1; run(100);
        rec_note_at(0, 4 + k, 60 + 4 * k, 60);
        rec_off();
        s[k] = psig(1);
    }
    CHECK("4 takes: the history keeps the last 2", tk_cnt == 2 && tk_pos == 2);
    CHECK("... the 3rd take and its answer back", take_undo(-1) == 1u && psig(1) == s[2]);
    CHECK("... the 2nd back", take_undo(-1) == 1u && psig(1) == s[1]);
    CHECK("... the 1st is not kept", take_undo(-1) == 0u);
    CHECK("... redo twice goes to the last one", take_undo(1) == 1u && take_undo(1) == 1u && psig(1) == s[3]);
    take_undo(-1);
    song.rec = 1; run(100); rec_note_at(0, 9, 79, 60); rec_off();
    CHECK("a take after an undo drops what could be redone", tk_cnt == 2 && tk_pos == 2 && take_undo(1) == 0u);
}
static void take_settings(void)
{
    uint32_t before, n2 = 0, i;
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_OFF;
    before = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); at_step(&trk[0], 12); rec_off();
    CHECK("ANSWER OFF: the loop stays LEN (1 bar), the other part is not touched, the take is undoable", trk[0].p[P_SLEN] == 16 && psig(1) == before && tk_cnt == 1 && take_undo(-1) == 1u);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 1;
    before = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); at_step(&trk[0], 12); rec_off();
    CHECK("ANSWER ON OFF: the other part is not touched", psig(1) == before);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_DOUBLE; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); at_step(&trk[0], 12); rec_off();
    for (i = 0; i < NSTEP; i++) n2 += trk[1].step[i].n;
    CHECK("DOUBLE: the other part plays the take's notes a third above (2 notes at the same steps)", said("DOUBLE") && n2 == 2u && trk[1].step[2].n == 1u && trk[1].step[6].n == 1u);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0;
    before = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 30, 60); at_step(&trk[0], 12); rec_off();
    (void)before;
    CHECK("a take down in the bass register: no limit message", !said("NO ANSWER"));
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0; tide_rec_replace = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 5, 67, 60); rec_off();
    CHECK("a pass over a part that has a pattern (ADD): the loop stays 16, the answer is written, one take", trk[0].p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 16 && tk_cnt == 1);
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY);
    start(); song.rec = 1; run(100); rec_off();
    CHECK("a pass that writes nothing is no take", tk_cnt == 0);
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY);
    song.rec = 1;
    CHECK("an armed part plays its stored step, not an evolved one", seq_play_step_at(&trk[0], 3, 0) == &trk[0].step[3]);
    song.rec = 0;
}
/* the answer switches on the bar line, not when REC goes off */
/* b13 (hunt F2): a multi loaded while REC is armed: the pass starts again on the new music. REC off with nothing played on it is no take (the new multi's
 * parts stay as loaded: no answer nobody asked for), and after a note on it, the undo gives the new multi's parts back, not the old multi's */
static void take_load_armed(void)
{
    uint32_t m0, m1;
    boot(); empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60);
    project_load(2); run(200);
    m0 = psig(0); m1 = psig(1);
    song.rec = 0; run(48); land(); run(400);
    CHECK("a load while REC is armed, REC off with nothing played on the new multi: no take, its parts as loaded", tk_cnt == 0 && psig(0) == m0 && psig(1) == m1);
    boot(); empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60);
    project_load(2); run(200);
    m0 = psig(0); m1 = psig(1);
    rec_note_at(0, 5, 72, 60);
    song.rec = 0; run(48); land(); run(100);
    CHECK("... a note on the new multi is a take; its undo gives the new multi's parts back", tk_cnt == 1 && psig(0) != m0 && take_undo(-1) == 1u && psig(0) == m0 && psig(1) == m1);
}
static void take_barline(void)
{
    uint32_t s2;
    boot(); empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0;
    s2 = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60);
    at_step(&trk[0], 9);
    song.rec = 0; run(48);
    CHECK("REC off mid-bar: the answer is made (the header says so) but waits for the bar line", said("BASS LINE") && tk_pend && psig(1) == s2);
    while (tk_pend && trk[0].seq_idx != 15u) run(16);
    CHECK("... it lands in the last 16th of the bar, before the downbeat", !tk_pend && psig(1) != s2 && (clk_step & 15u) == 15u);
}
/* b13 (hunt F1): a 2- or 4-bar answer to a part on a shorter loop plays in step with the take: bar n under bar n (it went on from the old 1-bar loop's step:
 * a bar out of step for good), whichever bar REC goes off in, the other part plain or ARP SEQ */
static void take_in_step(void)
{
    static const uint8_t CASE[5][3] = {{2, 0, 0}, {2, 1, 0}, {4, 3, 0}, {2, 0, 1}, {2, 1, 1}};   /* bars, the bar REC goes off in, the other part ARP SEQ */
    uint32_t c, i, b, out = 0, n;
    for (c = 0; c < 5u; c++) {
        uint32_t bars = CASE[c][0], len = 16u * bars, mis = 0;
        boot(); empty_part(0); trk[0].p[P_SLEN] = (int16_t)len; setup(1, CASE[c][2] ? AM_SEQ_ID : 0, V_POLY);
        if (CASE[c][2]) { tide_chord[1][0] = 48; tide_chord_n[1] = 1; }
        tide_answer = TA_BASS; tide_answer_on = 0;
        start(); song.rec = 1; run(100);
        for (b = 0; b < bars; b++) { rec_note_at(0, 16u * b + 2u, 69u - 3u * b, 60); rec_note_at(0, 16u * b + 8u, 72, 60); }
        at_step(&trk[0], 16u * CASE[c][1] + 9u);
        song.rec = 0; run(48);
        for (n = 0; tk_pend && n < 400u; n++) run(16);
        run(48);
        for (i = 0; i < 200u; i++) { run(16); mis += TIDE_PLAY_IDX(&trk[0]) != TIDE_PLAY_IDX(&trk[1]); }
        out += mis != 0u;
        if (mis)
            printf("take: in step: %u-bar take, REC off in bar %u%s: %u of 200 frames out of step\n", bars, CASE[c][1] + 1u, CASE[c][2] ? ", ARP SEQ answer part" : "", mis);
    }
    CHECK("a 2- or 4-bar answer to a 1-bar part plays in step with the take (5 cases)", !out);
}
static void take_roll_test(void)
{
    uint32_t a0, a1, a2, h0, s2;
    boot(); setup(1, 0, V_POLY);
    s2 = psig(1);
    first_take();
    a0 = psig(1);
    morph_do(MQ_ROLL, 1u, 0u); run(32); land();
    CHECK("a ROLL of the take's own part: the answer is left alone", psig(1) == a0 && tk_cnt == 1);
    morph_do(MQ_ROLL, 2u, 0u); run(32); land();
    a1 = psig(1);
    CHECK("a ROLL of the answering part writes the answer again with the roll's seed, and takes no history step", a1 != a0 && tk_cnt == 1 && tk_m.roll == 1u);
    CHECK("... the header says ROLL 1  BASS LINE", said("ROLL 1  BASS LINE"));
    CHECK("... the take stays", trk[0].step[2].note[0] == 69 && trk[0].p[P_SLEN] == 32);
    h0 = psig(0);
    morph_do(MQ_ROLL, 3u, 1u); run(32); land();
    a2 = psig(1);
    CHECK("a second roll (BOTH parts): another answer, still one history step", a2 != a1 && tk_cnt == 1 && tk_m.roll == 2u && psig(0) == h0);
    CHECK("undo after rolls: back to before the take (the rolls are not steps)", take_undo(-1) == 1u && said("UNDO TAKE 1") && psig(1) == s2);
    CHECK("... redo: the take with the last rolled answer", take_undo(1) == 1u && said("REDO TAKE 1") && psig(1) == a2);
    trk[1].step[3].n = 1; trk[1].step[3].note[0] = 50; trk[1].step[3].time = ST_NOTE;   /* the answer edited by hand is the player's now */
    a0 = psig(1);
    morph_do(MQ_ROLL, 2u, 0u); run(32); land();
    CHECK("a ROLL after the answer was edited leaves it alone", psig(1) == a0);
    /* ROLL while REC is armed: the answer is rolled, and the pass keeps the rolled seed */
    first_take();
    song.rec = 1; run(100);
    a0 = psig(1);
    morph_do(MQ_ROLL, 2u, 0u); run(32); land();
    a1 = psig(1);
    CHECK("ROLL while REC is armed: the answer is written again", a1 != a0 && tk_m.roll == 1u && tk_live);
    at_step(&trk[0], 3);
    rec_off();
    CHECK("... REC off with no change: the rolled answer stays, no new take", psig(1) == a1 && tk_cnt == 1);
}
/* b11: SWELL (C4) and RIPPLE (D4) of SEQ TOOLS (tide_take.c tide_tl_swell): the buttons make the answer again, the menu changes nothing by itself; they land at the start of the next loop, one history step each */
static uint32_t osig(const step_t *st) { uint32_t i, m = 17; for (i = 0; i < 64u; i++) m = m * 31u + ((uint32_t)st[i].time << 3) + st[i].n; return m; }   /* the onsets: what is a note, a tie or a rest, and how big a chord */
static uint32_t changed_steps(const step_t *a, uint32_t p, uint32_t *nnotes)
{
    uint32_t i, k, c = 0, n = 0;
    for (i = 0; i < 64u; i++) {
        uint32_t d = 0;
        n += trk[p].step[i].time == ST_NOTE && trk[p].step[i].n;
        for (k = 0; k < 4u; k++)
            d |= a[i].note[k] != trk[p].step[i].note[k];
        c += d;
    }
    *nnotes = n;
    return c;
}
static void sw_press(uint32_t part, uint32_t ripple) { (void)tl_do(&trk[part], ripple ? TL_COOK : TL_RANDOM); }
static void to_loop_end(uint32_t part) { at_step(&trk[part], (uint32_t)TIDE_PLAY_LEN(&trk[part]) - 1u); run(48); }
/* OCT- held + a PRESETS turn through the panel (ui_input.c: the take history), not take_undo: 1 when the history moved */
static uint32_t gesture(int32_t dir)
{
    uint32_t pos = tk_pos;
    btn_down(B_OCTDN); frame();
    turn(EN_PRESET, dir); frame();
    btn_up(B_OCTDN); frame();
    run(400);
    return tk_pos != pos;
}
static uint32_t h_off1, h_on1, h_off_n, h_on_n, h_trk;
static void swell_hook(int on)                           /* the interrupt-off window of tl_sw_apply's write: the part's steps as they are when it opens and when it closes */
{
    if (on) { if (!h_on_n) h_on1 = steps_sig(&trk[h_trk]); h_on_n++; } else { if (!h_off_n) h_off1 = steps_sig(&trk[h_trk]); h_off_n++; }   /* (the first window: the take's write comes before the answer's) */
}
static int key_ok(const step_t *st, uint32_t part)
{
    uint32_t i, k;
    int32_t off = tk_poff(&trk[part]);
    for (i = 0; i < 64u; i++)
        for (k = 0; k < st[i].n && k < 4u; k++)
            if (st[i].note[k] && !tk_in_key((int32_t)st[i].note[k] + off))
                return 0;
    return 1;
}
static void take_swell(void)
{
    uint32_t a0, a1, s2, h0, on0, n, c, i, ok, pr = 0, bt = 0, mk = 0, w0, w1;
    step_t keep[64];
    static morph_part_t sv;
    boot(); setup(1, 0, V_POLY);
    s2 = psig(1);
    first_take();
    a0 = psig(1); h0 = psig(0);
    /* the menu alone changes nothing */
    tide_answer = TA_DOUBLE; run(48); land();
    tide_answer_on = 1; run(48); land(); tide_answer_on = 0; run(48); land();
    CHECK("ANSWER / ANSWER ON turned: a standing answer is left as it is (no re-make, no history step)", psig(1) == a0 && tk_m.mode == TA_BASS && !tk_pend && !tk_sw_k && tk_cnt == 1);
    /* SWELL on the answer's part: the new mode, at the start of the next loop */
    at_step(&trk[1], 6);
    memcpy(&sv, &mph[1], sizeof sv);
    sw_press(1, 0);
    CHECK("SWELL on the answer: it waits for the loop start, the header says so", tk_sw_k == 1u && said("SWELL  NEXT LOOP") && psig(1) == a0);
    CHECK("... and the tide line shows it (state 7, accent dots) until it lands", take_line(&pr) == 3u && tide_line_state(&pr, &bt, &mk) == 7);
    at_step(&trk[1], 20); run(16);
    CHECK("... not at once, not at the bar line: the answer is still the old one halfway through the loop", psig(1) == a0 && tk_sw_k == 1u);
    to_loop_end(1);
    a1 = psig(1);
    CHECK("... at the loop start it lands: a new answer in the mode ANSWER shows now (DOUBLE), a history step", a1 != a0 && tk_m.mode == TA_DOUBLE && !tk_sw_k && tk_cnt == 2 && said("SWELL  DOUBLE"));
    CHECK("... the tide line is back to nothing", take_line(&pr) == 0u && tide_line_state(&pr, &bt, &mk) == 0);
    CHECK("... the take is untouched, the sound's End B too", psig(0) == h0 && !memcmp(&mph[1], &sv, sizeof sv));
    CHECK("OCT- + PRESETS (the panel gesture): SWELL goes back one step (the first answer), then the take", gesture(-1) == 1u && said("UNDO SWELL") && psig(1) == a0 && gesture(-1) == 1u && said("UNDO TAKE 1") && psig(1) == s2);
    CHECK("... redo twice brings them back", gesture(1) == 1u && gesture(1) == 1u && said("REDO SWELL") && psig(1) == a1);
    /* a second press before the loop turns replaces the first (no queue) */
    at_step(&trk[1], 4);
    sw_press(1, 0); sw_press(1, 1);
    CHECK("pressing again before the loop turns replaces the waiting press (RIPPLE now)", tk_sw_k == 2u);
    memcpy(keep, trk[1].step, sizeof keep);
    w0 = osig(keep);
    to_loop_end(1);
    CHECK("... one step lands, the header says RIPPLE", !tk_sw_k && said("RIPPLE  DOUBLE") && tk_cnt == 2 && (tk_hn[1] & 0x4000u));
    /* RIPPLE: a quarter of the notes at most move, the rhythm stays, in the key, no rubs against the take */
    c = changed_steps(keep, 1, &n);
    CHECK("RIPPLE moves at least one note and at most a quarter of them (or one)", c >= 1u && c <= (n * 25u / 100u > 1u ? n * 25u / 100u : 1u));
    CHECK("... the rhythm is kept: every onset, tie and chord size as before (compared)", osig(trk[1].step) == w0);
    tk_events(&trk[0], seq_len(&trk[0])); tk_key();
    for (i = 0, ok = 1; i < 64u; i++) {
        uint32_t k;
        for (k = 0; k < 4u; k++)
            if (keep[i].note[k] != trk[1].step[i].note[k] && trk[1].step[i].note[k] && (!tk_in_key(trk[1].step[i].note[k]) || tk_rubs((int32_t)i, 1, trk[1].step[i].note[k])))
                ok = 0;
    }
    CHECK("... the moved notes are in the key and free of rubs against the take", ok);
    /* the same seed gives the same notes */
    memcpy(keep, trk[1].step, sizeof keep);
    on0 = tl_sw_apply(&trk[1], 0, 1234, 0); a0 = psig(1);
    memcpy(trk[1].step, keep, sizeof keep);
    (void)tl_sw_apply(&trk[1], 0, 1234, 0);
    CHECK("the same seed gives the same notes (SWELL of a part); another seed another wave", on0 && psig(1) == a0 && (memcpy(trk[1].step, keep, sizeof keep), tl_sw_apply(&trk[1], 0, 4321, 0), psig(1) != a0));
    /* SWELL on the take's part: the take changes (a scratch copy, written with the interrupt off), the answer follows, one step for both */
    first_take();
    a0 = psig(1); h0 = psig(0);
    at_step(&trk[0], 5);
    sw_press(0, 0);
    CHECK("SWELL on the take's part waits for the loop start too", tk_sw_k == 1u && psig(0) == h0 && psig(1) == a0);
    h_trk = 0; h_off_n = h_on_n = 0; host_irq_hook = swell_hook;
    to_loop_end(0);
    host_irq_hook = 0;
    CHECK("... the take is a new one and the answer is made from it, one history step for the pair", psig(0) != h0 && psig(1) != a0 && tk_cnt == 2 && !tk_sw_k);
    CHECK("... the playing part's steps are written whole inside the interrupt-off window (unchanged when it opens, new when it closes)", h_off_n >= 1u && h_on_n >= 1u && h_off1 == h0 && h_on1 != h0);
    CHECK("... OCT- + PRESETS brings back both", gesture(-1) == 1u && psig(0) == h0 && psig(1) == a0);
    /* UNDO TAKE, then a press on the part that held it: a plain tool at once (the identity went back with the history) */
    first_take();
    a0 = psig(1); h0 = psig(0);
    CHECK("undo the take (the gesture)", gesture(-1) == 1u && psig(1) != a0);
    a1 = psig(1);
    sw_press(0, 0);
    CHECK("... a press on the part that held the undone take is a plain tool: at once, the other part is not touched", !tk_sw_k && psig(0) != h0 && psig(1) == a1);
    h0 = psig(0);
    sw_press(0, 0);
    CHECK("... and the next press too (the history step of the first one must not bring the undone take's identity back)", !tk_sw_k && psig(0) != h0 && psig(1) == a1);
    /* stopped: at once */
    first_take();
    a0 = psig(1);
    stop();
    sw_press(1, 0);
    CHECK("stopped: SWELL lands at once", !tk_sw_k && psig(1) != a0 && said("SWELL"));
    /* STOP while pending: lands at once, once */
    first_take();
    a0 = psig(1);
    at_step(&trk[1], 4);
    sw_press(1, 0);
    stop();
    CHECK("STOP while a press waits: it lands at once, once (one history step)", !tk_sw_k && psig(1) != a0 && tk_cnt == 2);
    /* a load while pending: nothing lands */
    boot(); setup(1, 0, V_POLY); first_take();
    a0 = psig(1);
    at_step(&trk[1], 4);
    sw_press(1, 0);
    tk_reset_v = 1; run(48);
    to_loop_end(1);
    CHECK("a load while a press waits: it is dropped, nothing lands on the new music", !tk_sw_k && psig(1) == a0 && tk_cnt == 0);
    /* undo while pending: dropped */
    first_take();
    s2 = psig(1);
    at_step(&trk[1], 4);
    sw_press(1, 0);
    h0 = psig(1);
    CHECK("the gesture while a press waits goes back to before the take", gesture(-1) == 1u && !tk_sw_k && psig(1) != h0);
    a0 = psig(1);
    to_loop_end(1);
    CHECK("... and the dropped press does not land afterwards", psig(1) == a0 && tk_cnt == 1);
    /* REC arms while pending: kept, lands at the end of the armed pass */
    first_take();
    a0 = psig(1);
    at_step(&trk[1], 4);
    sw_press(1, 0);
    song.rec = 1; run(100);
    to_loop_end(1);
    /* (b13, hunt F1: the answer now plays in step with the take, so the answer part's loop end IS the armed pass's end: the press is made there, in one frame) */
    land();
    CHECK("REC arms while a press waits: it is not dropped, it lands at the end of the armed pass: the answer is made again", !tk_sw_k && psig(1) != a0 && !tk_force);
    at_step(&trk[0], 3);
    rec_off();
    /* ANSWER OFF: SWELL gives the other part its old pattern back */
    boot(); setup(1, 0, V_POLY); s2 = psig(1);
    first_take();
    tide_answer = TA_OFF; run(48);
    CHECK("ANSWER OFF in the menu: the answer stays", psig(1) != s2);
    at_step(&trk[1], 4);
    sw_press(1, 0);
    to_loop_end(1);
    CHECK("... SWELL then puts the other part's old pattern back", psig(1) == s2 && !tk_m.ans);
    tide_answer = TA_BASS;
    /* no take: the stock tool with TIDE's rules, at once, one history step */
    boot(); setup(1, 0, V_POLY); empty_part(0);
    h0 = psig(0);
    sw_press(0, 0);
    CHECK("SWELL on a part with no take: a pattern in the genre's rhythm, at once, one history step", psig(0) != h0 && tk_cnt == 1 && said("SWELL"));
    CHECK("... the gesture gives the empty part back", gesture(-1) == 1u && psig(0) == h0);
    /* no key (SCL off, no key byte): chromatic notes are allowed; with SCL set they are in it */
    boot(); setup(1, 0, V_POLY); empty_part(0);
    root_g = 2; scale_g = 2;
    sw_press(0, 0);
    for (i = 0, ok = 1; i < 64u; i++)
        if (trk[0].step[i].time == ST_NOTE && trk[0].step[i].n)
            ok &= tl_in_scale(&trk[0], trk[0].step[i].note[0]);
    CHECK("SWELL with SCL set: every note is in the scale", ok);
    scale_g = 0;
    /* MONO answer part */
    boot(); setup(1, 0, V_MONO);
    first_take();
    memcpy(keep, trk[1].step, sizeof keep);
    w0 = osig(keep);
    at_step(&trk[1], 4);
    sw_press(1, 1);
    to_loop_end(1);
    c = changed_steps(keep, 1, &n);
    CHECK("MONO answer part: RIPPLE moves a note, in the key, the rhythm kept", c >= 1u && osig(trk[1].step) == w0 && key_ok(trk[1].step, 1));
    /* ARP SEQ on the answer part: the steps are offsets, the rubs and the key are about what sounds */
    boot(); setup(1, 0, V_POLY);
    first_take();
    trk[1].p[P_AMODE] = AM_SEQ_ID;
    memcpy(keep, trk[1].step, sizeof keep);
    w0 = osig(keep);
    at_step(&trk[1], 4);
    sw_press(1, 1);
    to_loop_end(1);
    c = changed_steps(keep, 1, &n);
    CHECK("ARP SEQ answer part: RIPPLE moves a note, the rhythm kept, the notes in the key as they sound", c >= 1u && osig(trk[1].step) == w0 && key_ok(trk[1].step, 1));
    /* TRN on the answer part: the notes are stored lower than they sound; RIPPLE keeps what SOUNDS in the key */
    boot(); setup(1, 0, V_POLY);
    first_take();
    for (i = 0; i < 64u; i++)                            /* (the answer as it would be written for a part with TRN +3: stored 3 lower, sounding the same) */
        for (ok = 0; ok < 4u; ok++)
            if (trk[1].step[i].note[ok] > 3u)
                trk[1].step[i].note[ok] = (uint8_t)(trk[1].step[i].note[ok] - 3u);
    trk[1].p[P_TRANS] = 3;
    tk_m.sig = steps_sig(&trk[1]);
    memcpy(keep, trk[1].step, sizeof keep);
    w0 = osig(keep);
    at_step(&trk[1], 4);
    sw_press(1, 1);
    to_loop_end(1);
    c = changed_steps(keep, 1, &n);
    CHECK("answer part with TRN +3: RIPPLE moves a note and what sounds (stored + TRN) stays in the key, the rhythm kept", c >= 1u && osig(trk[1].step) == w0 && key_ok(trk[1].step, 1));
    trk[1].p[P_TRANS] = 0;
    /* what sounds, not what is stored: a part's TRN counts in the rub check */
    boot(); setup(1, 0, V_POLY); empty_part(0);
    trk[0].step[0].time = ST_NOTE; trk[0].step[0].n = 1; trk[0].step[0].note[0] = 60;
    trk[1].p[P_SLEN] = 16; trk[1].step[0].time = ST_NOTE; trk[1].step[0].n = 1; trk[1].step[0].note[0] = 60;
    trk[1].p[P_TRANS] = 0;
    w0 = (uint32_t)tl_rub(&trk[0], 0, 60);
    trk[1].p[P_TRANS] = 1;
    w1 = (uint32_t)tl_rub(&trk[0], 0, 60);
    trk[1].p[P_TRANS] = 0;
    CHECK("rubs are about what sounds: the same stored notes rub when the other part's TRN makes them a semitone apart", w0 == 0u && w1 == 1u);
    /* REC armed: SWELL and RIPPLE are remembered for the next pass, each with its own header */
    first_take();
    song.rec = 1; run(100);
    a1 = psig(1);
    sw_press(1, 0);
    CHECK("REC armed: SWELL makes nothing at once, it forces the next pass's re-make", psig(1) == a1 && tk_force == 1u && said("SWELL  NEXT PASS"));
    to_loop_end(0);
    land();
    CHECK("... the last step of the pass: the answer is made again", psig(1) != a1 && !tk_force);
    at_step(&trk[0], 3);
    rec_off();
    first_take();
    song.rec = 1; run(100);
    memcpy(keep, trk[1].step, sizeof keep);
    w0 = osig(keep);
    sw_press(1, 1);
    CHECK("REC armed: RIPPLE says RIPPLE (not SWELL) and is remembered as one", tk_force == 2u && said("RIPPLE  NEXT PASS"));
    to_loop_end(0);
    land();
    c = changed_steps(keep, 1, &n);
    CHECK("... at the end of the pass the answer is rippled, not made new: a few notes moved, the rhythm kept, the header says RIPPLE", c >= 1u && c <= (n * 25u / 100u > 1u ? n * 25u / 100u : 1u) && osig(trk[1].step) == w0 && said("RIPPLE") && !tk_force);
    at_step(&trk[0], 3);
    rec_off();
}
/* b11: QUICK SEQ (SEQ held) shows what a SWELL / RIPPLE does: the layer's redraw signature (ui.layer_sig) moves when a press is pending and again when it lands, also on a stopped transport */
static void take_swell_layer(void)
{
    uint32_t s0, s1, s2, s3, a0;
    boot(); setup(1, 0, V_POLY);
    first_take();
    song.sel = 1; frame();
    btn_down(B_SEQ); run(700);
    CHECK("the layer is open (SEQ held)", ui.layer == LAYER_SEQ);
    s0 = ui.layer_sig;
    at_step(&trk[1], 4);
    sw_press(1, 0);
    frame(); frame();
    s1 = ui.layer_sig;
    CHECK("a SWELL that waits for the loop start changes the layer's picture (the lit box)", tk_sw_k == 1u && s1 != s0);
    a0 = psig(1);
    to_loop_end(1);
    frame(); frame();
    s2 = ui.layer_sig;
    CHECK("... and landing changes it again (the new steps are in the signature), the pending mark is gone", psig(1) != a0 && !tk_sw_k && s2 != s1 && s2 != s0);
    stop();
    sw_press(1, 0);
    frame(); frame();
    s3 = ui.layer_sig;
    CHECK("stopped: SWELL lands at once and the layer's signature moves", !tk_sw_k && s3 != s2);
    btn_up(B_SEQ); frame();
}
static uint32_t notes_in(uint32_t p, uint32_t a, uint32_t b) { uint32_t i, n = 0; for (i = a; i < b; i++) n += trk[p].step[i].n != 0; return n; }
static void take_reply(void)
{
    static const uint8_t LENS[3] = {16, 32, 64};
    uint32_t a0, li;
    for (li = 0; li < 3u; li++) {
        uint32_t L = LENS[li], h = L / 2u;
        boot(); empty_part(0); trk[0].p[P_SLEN] = (int16_t)L; setup(1, 0, V_POLY); tide_answer = TA_REPLY; tide_answer_on = 0;
        start(); song.rec = 1; run(100);
        rec_note_at(0, 1, 69, 60); rec_note_at(0, 3, 72, 60); rec_note_at(0, 5, 76, 60); at_step(&trk[0], 7); rec_off();
        if (li == 0u)
            printf("take: REPLY on LEN 16: part 2 steps 9 11 13: %u %u %u, first half %u notes\n", trk[1].step[9].n, trk[1].step[11].n, trk[1].step[13].n, notes_in(1, 0, 8));
        CHECK("REPLY on LEN 16 / 32 / 64: the loops stay LEN, the header says REPLY", trk[0].p[P_SLEN] == (int16_t)L && trk[1].p[P_SLEN] == (int16_t)L && said("REPLY"));
        CHECK("... the first half calls, the second answers: 3 notes at the call's steps + LEN / 2, nothing in the first half", trk[1].step[h + 1u].n == 1 && trk[1].step[h + 3u].n == 1 && trk[1].step[h + 5u].n == 1 && notes_in(1, 0, h) == 0u);
    }
    a0 = psig(1);
    for (uint32_t r = 0; r < 4u && psig(1) == a0; r++) {          /* (a REPLY rolls among 3 octaves: a roll may land on the same one) */
        morph_do(MQ_ROLL, 2u, r); run(32); land();
    }
    CHECK("a ROLL of the reply writes it again inside LEN, no history step", trk[0].p[P_SLEN] == 64 && trk[1].p[P_SLEN] == 64 && tk_cnt == 1 && psig(1) != a0);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_REPLY; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 10, 69, 60); rec_note_at(0, 13, 72, 60); at_step(&trk[0], 15); rec_off();
    CHECK("he played only in the second half: the first half answers it", trk[1].step[2].n == 1 && trk[1].step[5].n == 1 && notes_in(1, 8, 16) == 0u);
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_ECHO; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 8, 72, 60); at_step(&trk[0], 13); rec_off();
    CHECK("ECHO: 4 throws of the last note, wrapping round LEN 16 (steps 9 12 15 and 2)", said("ECHO") && notes_in(1, 0, 16) == 4u && trk[1].p[P_SLEN] == 16 && trk[1].step[9].n == 1u && trk[1].step[2].n == 1u);
    boot(); empty_part(0); trk[0].p[P_SLEN] = 64; setup(1, 0, V_POLY); tide_answer = TA_REPLY; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); at_step(&trk[0], 40); rec_off();
    CHECK("a call in a 4 bar loop: no limit, the reply is written in bars 3-4", trk[0].p[P_SLEN] == 64 && trk[1].step[34].n == 1 && !said("CALL"));
}
static void take_line_test(void)
{
    uint32_t pr = 0, beat = 0, mask = 3u;
    char l[48];
    boot(); empty_part(0); trk[0].p[P_SLEN] = 64; setup(1, 0, V_POLY);
    CHECK("no take: the tide line and knob 1 are as they were", take_line(&pr) == 0u && !take_text(l) && tide_line_state(&pr, &beat, &mask) == 0);
    start(); song.rec = 1; run(100);
    at_step(&trk[0], 20);
    CHECK("a take records: take_line says recording, the tide line is REC red (state 6), the fill is 20 of LEN 64", take_line(&pr) == 2u && tide_line_state(&pr, &beat, &mask) == 6 && pr / 1024u == 20u);
    CHECK("... knob 1's line says TAKE  BAR 2/4", take_text(l) == 1u && !strcmp(l, "TAKE  BAR 2/4"));
    cin_left = 2;
    CHECK("the count-in of a take: dashes (state 5)", tide_line_state(&pr, &beat, &mask) == 5);
    cin_left = 0;
    rec_off();
    CHECK("closed: the line is as before", take_line(&pr) == 0u && tide_line_state(&pr, &beat, &mask) == 0);
}
/* REC from stop arms and waits; the first key starts PLAY at step 1 with its note on step 1 */
static void take_first_key(void)
{
    uint32_t pr = 0, beat = 0, mask = 3u;
    boot(); empty_part(0); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 0; cin_bars = 0;
    song.sel = 0;
    rec_tap(1u); run(100);
    CHECK("REC from stop arms and waits: no PLAY", (song.rec & 1u) && !song.playing);
    CHECK("... the tide line shows REC armed (dashes, state 5)", tide_line_state(&pr, &beat, &mask) == 5);
    run(500);
    CHECK("... still waiting half a second later", !song.playing);
    input_on(&trk[0], 64, 100); run(40); input_off(&trk[0], 64); run(40);
    CHECK("the first key starts PLAY", song.playing);
    CHECK("... its note is on step 1", trk[0].step[0].n == 1 && trk[0].step[0].note[0] == 64);
    CHECK("... the tide line turns REC red (state 6)", tide_line_state(&pr, &beat, &mask) == 6);
    rec_tap(1u); run(48); land();
    CHECK("REC off: the take and its answer", tk_cnt == 1 && tk_m.ans == 2u);
    stop();
    /* MIDI: the same */
    boot(); empty_part(0); setup(1, 0, V_POLY); cin_bars = 0; song.sel = 0;
    rec_tap(1u); run(100);
    midi_event(0x90, 0, 67, 100); run(40); midi_event(0x80, 0, 67, 0); run(40);
    CHECK("a MIDI note starts it the same way, on step 1", song.playing && trk[0].step[0].n == 1 && trk[0].step[0].note[0] == 67);
    song.rec = 0; run(48); stop();
    /* the count-in on: as before (REC starts it) */
    boot(); empty_part(0); setup(1, 0, V_POLY); cin_bars = 1; click_mode = CLICK_REC; song.sel = 0;
    rec_tap(1u); run(40);
    CHECK("with COUNT-IN on, REC from stop counts in as before", seq_counting() || song.playing);
    song.rec = 0; stop(); cin_bars = 0; click_mode = 0;
}
/* LAYER: while a part records, only it hears the keys */
static void take_layer(void)
{
    uint32_t i, on1 = 0, on2 = 0;
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY); duell_mode_force(DM_LAYER);
    start(); song.rec = 2u; run(100);
    midi_event(0x90, 0, 71, 100); run(40);
    for (i = 0; i < NVOICE; i++) {
        on1 += trk[0].v[i].active && trk[0].v[i].note == 71u;
        on2 += trk[1].v[i].active && trk[1].v[i].note == 71u;
    }
    midi_event(0x80, 0, 71, 0); run(40);
    CHECK("LAYER, part 2 recording: the note plays part 2 only", on2 > 0u && on1 == 0u);
    song.rec = 0; run(48); stop();
    duell_mode_force(DM_STEREO);
}
/* an ARP SEQ part: the held chord's notes (CHRD) on a key, and the take played back as single notes */
static void take_arpseq(void)
{
    uint32_t i, n;
    boot(); setup(0, AM_SEQ_ID, V_POLY); setup(1, 0, V_POLY); tide_answer = TA_OFF; tide_rec_replace = 0;
    trk[0].p[P_CHRD] = 1;                                /* (a major chord on every key) */
    start(); song.rec = 1; run(100);
    wait_arp(&trk[0], 5);
    midi_event(0x90, 0, 67, 100); run(40); midi_event(0x80, 0, 67, 0); run(40);
    printf("take: ARP SEQ, CHRD maj on one MIDI note: step 5 = %u notes: %u (the pattern's %u)\n", trk[0].step[5].n, trk[0].step[5].note[0], PAT[5]);
    CHECK("ARP SEQ with CHRD: a key's chord sets the step once, to the key's own note (not its top tone)", trk[0].step[5].n == 1u && trk[0].step[5].note[0] == 67u);
    rec_off();
    CHECK("the take on an ARP SEQ part plays single notes: it leaves ARP SEQ, on the arp's rate", trk[0].p[P_AMODE] == 0 && trk[0].p[P_SDIV] == trk[0].p[P_ARATE]);
    for (i = 0, n = 0; i < 16u; i++) n += trk[0].step[i].n == 1u;
    CHECK("... one note a step", n == 16u);
    CHECK("undo brings ARP SEQ back", take_undo(-1) == 1u && trk[0].p[P_AMODE] == AM_SEQ_ID);
    stop();
    /* the answer on an ARP SEQ part */
    boot(); empty_part(0); setup(1, AM_SEQ_ID, V_POLY); trk[1].p[P_ARATE] = 9; tide_answer = TA_BASS; tide_answer_on = 0;
    start(); song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); at_step(&trk[0], 12); rec_off();
    for (i = 0, n = 0; i < 16u; i++) n += trk[1].step[i].n > 1u;
    CHECK("a BASS LINE on an ARP SEQ pad (slow rate): single notes on the take's grid", trk[1].p[P_AMODE] == 0 && trk[1].p[P_SDIV] == 2 && notes_in(1, 0, 16) >= 1u && n == 0u);
    stop();
}
/* ---- the answer follows the live pass (owner: "when I overwrite an existing sequence via live play the response should be generated instantly") ---- */
static void take_live(void)
{
    uint32_t s1, s2, a1 = 0, a2 = 0, a3 = 0, pass, m;
    for (m = TA_BASS; m <= TA_CHORDS; m++) {
        boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY); tide_answer = (uint8_t)m; tide_answer_on = 0; tide_rec_replace = 0;
        s1 = psig(0); s2 = psig(1);
        start(); song.rec = 1; run(100);
        CHECK("REC armed over an existing part: nothing is written yet", psig(1) == s2 && tk_cnt == 0);
        for (pass = 1; pass <= 3u; pass++) {
            rec_note_at(0, 2 + 3 * pass, 60 + 4 * pass, 60);
            at_step(&trk[0], 15); run(60);                    /* the last step of the pass: the answer is made from the part's steps as they are now, and lands on the bar line */
            if (pass == 1) a1 = psig(1);
            if (pass == 2) a2 = psig(1);
            if (pass == 3) a3 = psig(1);
            CHECK("every mode: an answer at the end of the loop pass, before REC is off, no history step yet", psig(1) != s2 && tk_cnt == 0 && tk_live);
        }
        CHECK("the answer follows the take (it changed between passes)", a1 != a2 || a2 != a3 || a1 != s2);
        at_step(&trk[0], 3); rec_off();
        CHECK("REC off: one history step for the whole armed session, the answer unchanged (same seed)", tk_cnt == 1 && tk_pos == 1 && psig(1) == a3);
        CHECK("undo goes back to before the armed session", take_undo(-1) == 1u && psig(0) == s1 && psig(1) == s2);
        stop();
    }
    /* ANSWER ON OFF and ANSWER OFF: nothing changes */
    boot(); setup(0, 0, V_POLY); setup(1, 0, V_POLY); tide_answer = TA_BASS; tide_answer_on = 1; tide_rec_replace = 0;
    s2 = psig(1);
    start(); song.rec = 1; run(100);
    rec_note_at(0, 5, 67, 60); at_step(&trk[0], 15); run(60);
    CHECK("ANSWER ON OFF: no answer while the pass is open", psig(1) == s2 && !tk_live);
    tide_answer_on = 0; tide_answer = TA_OFF;
    rec_note_at(0, 9, 70, 60); at_step(&trk[0], 15); run(60);
    CHECK("ANSWER OFF: no answer while the pass is open", psig(1) == s2 && !tk_live);
    rec_off();
}
/* b13: ANSWER CHORDS: a take is answered with the pad (3 notes at each bar start, tied for the bar), the header says CHORDS, a ROLL (seed bit 1 set or not) writes it again without a history step, undo brings the old part back, and a
 * stored ANSWER of an older build (0..4) still loads */
static void take_chords(void)
{
    uint32_t a0, a1, s2, n, i, held = 0, stab = 0, r;
    boot(); setup(1, 0, V_POLY);
    s2 = psig(1);
    tide_answer = TA_CHORDS;
    CHECK("ANSWER has a CHORDS row value after ECHO", jl_n(JL_ANS) == TA_N && TA_N == 6 && !strcmp(jl_vname(JL_ANS, 5), "CHORDS") && !strcmp(jl_vname(JL_ANS, 4), "ECHO") && !strcmp(jl_vname(JL_ANSON, 0), "OTHER") && !strcmp(jl_vname(JL_ANSON, 1), "OFF"));
    empty_part(0); trk[0].p[P_SLEN] = 32;
    start();
    song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); rec_note_at(0, 14, 74, 60);
    rec_note_at(0, 18, 72, 60); rec_note_at(0, 22, 69, 60);
    at_step(&trk[0], 26);
    rec_off();
    a0 = psig(1);
    for (i = 0, n = 0; i < 32u; i++)
        n += trk[1].step[i].n != 0;
    CHECK("CHORDS: the other part holds the chords of the take: one step a bar, 3 notes, tied for the bar, the loop the take's", a0 != s2 && trk[1].p[P_SLEN] == 32 && n == 2u && trk[1].step[0].n == 3u && trk[1].step[16].n == 3u && trk[1].step[1].time == ST_TIE && trk[1].step[15].time == ST_TIE && trk[1].step[17].time == ST_TIE);
    CHECK("... the header says CHORDS and the key, one step of history", said("CHORDS") && tk_m.mode == TA_CHORDS && tk_cnt == 1);
    for (r = 1; r <= 8u; r++) {                          /* (rolls: the seed's bit 1 picks the stabs, the pad otherwise; none takes a history step) */
        morph_do(MQ_ROLL, 2u, 0u); run(32); land();
        for (i = 0, n = 0; i < 32u; i++)
            n += trk[1].step[i].n != 0;
        if (n == 2u && trk[1].step[0].n == 3u && trk[1].step[16].n == 3u) held++; else stab += n > 0u;
    }
    CHECK("CHORDS: the rolls give both the pad and stabs, no history step, the header says ROLL n  CHORDS", held > 0u && stab > 0u && tk_cnt == 1 && said("ROLL 8  CHORDS"));
    a1 = psig(1);
    CHECK("... undo goes back to before the take", take_undo(-1) == 1u && psig(1) == s2);
    CHECK("... redo brings the last rolled answer back", take_undo(1) == 1u && psig(1) == a1);
    stop();
}
/* the header message of a take / answer stays 4 bars, 4 s at least, 8 s at most (120 bpm: 8 s) */
static void take_hold(void)
{
    uint32_t t0;
    first_take();
    CHECK("the answer's message is shown", said("BASS LINE"));
    t0 = fm1_ms;
    run(3000);
    CHECK("... still there after 3 s", said("BASS LINE") && fm1_ms - t0 >= 2900u);
    run(3000);
    CHECK("... and after 6 s (4 bars at 120 bpm = 8 s from the take)", said("BASS LINE") && fm1_ms - t0 >= 5900u);
    run(4000);
    CHECK("... and gone 10 s later", !said("BASS LINE"));
    boot_bpm = 240;
    first_take();
    t0 = fm1_ms;
    run(2500);
    CHECK("at 240 bpm (4 bars = 4 s) the minimum applies: still there 2.5 s later", said("BASS LINE"));
    run(3500);
    CHECK("... gone after 6 s", !said("BASS LINE"));
    boot_bpm = 120;
}
int main(void)
{
    step1();
    take_lifecycle(); take_history(); take_settings(); take_barline(); take_in_step(); take_load_armed(); take_roll_test(); take_swell(); take_swell_layer(); take_reply(); take_line_test(); take_first_key(); take_layer(); take_arpseq(); take_live(); take_chords(); take_hold();
    printf("take: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
