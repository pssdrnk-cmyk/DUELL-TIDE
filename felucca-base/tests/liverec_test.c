/* Live recording (audit liverec 2026-11-08, work/audit/liverec-audit-2026-11-08): REC armed while PLAY runs.
 *   H1   both ARP SEQ parts keep playing while REC is armed (L1)
 *   R2   keys record into an ARP SEQ pattern, heard as played (L2, owner 2026-11-08): tap, stand-in chord, next step, hold, ARATE, no drift, POLY
 *        (the earlier H2 / H2b / H3 / H4 asked for no change at all: they are answered by these)
 *   REC  ADD / REPLACE on an ordinary part, and the REC row of TIDE SETTINGS (default, STYLE, saved, an older record)
 *   H5   arp OFF (the plain path): a tap at step 5 adds the note to step 5 (control; L3: a long hold ties)
 *   R4   REC on HOME: armed, a tap disarms and leaves HOLD alone (L6); unarmed it stays HOLD and says how to record (L5)
 * Built from the probe work/audit/liverec-audit-2026-11-08/probe/liverec_probe.c (same harness as ui_test.c). */
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
#define LOOP_MS 2000u   /* 120 bpm, 16 x 1/16 = 1 bar = 2000 ms */

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
}
/* how many times the arp's note changes over ms */
static uint32_t changes(track_t *t, uint32_t ms)
{
    uint32_t i, last = 0, cnt = 0;
    for (i = 0; i < ms / 16u; i++) {
        run(16);
        if (t->arp_note && t->arp_note != last) cnt++;
        last = t->arp_note;
    }
    return cnt;
}
static void start(void) { transport_req = 1u; run(64); }
static void stop(void) { transport_req = 2u; run(100); }
static int same(const track_t *t, const step_t *was) { return !memcmp(t->step, was, sizeof t->step); }

static void h1(void)
{
    uint32_t a0, a1;
    boot(); setup(0, AM_SEQ_ID, V_POLY); setup(1, AM_SEQ_ID, V_POLY);
    start();
    CHECK("H1: before arming, both parts play", changes(&trk[0], 1000) >= 4u && changes(&trk[1], 1000) >= 4u);
    song.rec = 1; run(400);
    a0 = changes(&trk[0], 2000); a1 = changes(&trk[1], 2000);
    CHECK("H1: part 1 armed: its stand-in keeps playing", a0 >= 8u);
    CHECK("H1: part 1 armed: part 2's stand-in keeps playing", a1 >= 8u);
    song.rec = 3;
    CHECK("H1: both armed: both keep playing", changes(&trk[0], 1000) >= 4u && changes(&trk[1], 1000) >= 4u);
    song.rec = 0;
    stop();
}
/* ---- R2: keys record into an ARP SEQ pattern (owner 2026-11-08), REC = ADD / REPLACE ---- */
static void wait_arp(track_t *t, uint32_t k)             /* the arp starts step k (of 16), early in it */
{
    uint32_t n = 0;
    while (!(t->arp_idx != 0xFFFFFFFFu && t->arp_idx % 16u == k && t->arp_pos < arp_dur(t) / 4u) && n++ < 2000u) run(16);
}
static void wait_mid(track_t *t, uint32_t k)             /* ... past the middle of step k (60 .. 90 % of it) */
{
    uint32_t n = 0;
    while (!(t->arp_idx != 0xFFFFFFFFu && t->arp_idx % 16u == k && t->arp_pos > arp_dur(t) * 6u / 10u && t->arp_pos < arp_dur(t) * 9u / 10u) && n++ < 2000u) run(16);
}
static uint32_t plays(track_t *t, uint32_t k)            /* the note the arp plays at step k, as the next pass starts it */
{
    wait_arp(t, k); run(16);
    return t->arp_note;
}
static int only(const track_t *t, const step_t *was, uint32_t a, uint32_t b)   /* every step but a and b as it was */
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (i != a && i != b && memcmp(&t->step[i], &was[i], sizeof was[i])) return 0;
    return 1;
}
static void tap(track_t *t, uint32_t note, uint32_t ms) { input_on(t, note, 100); run(ms); input_off(t, note); }
static void arm_arp(uint32_t part, int voice)
{
    boot(); tide_rec_replace = 0; setup(part, AM_SEQ_ID, voice);
    start();
    song.rec = 1u << part; run(100);
}

static void r2(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    uint32_t i, l, ok, seen;

    /* ADD, MONO: a tap at step 5 */
    arm_arp(0, V_MONO);
    wait_arp(t, 5); memcpy(was, t->step, sizeof was);
    tap(t, 64, 40);
    CHECK("R2: MONO, E4 tapped at arp step 5: step 5 holds 64 alone", t->step[5].n == 1u && t->step[5].note[0] == 64u && t->step[5].time == ST_NOTE);
    CHECK("R2: ... and no other step changed", only(t, was, 5, 99));
    CHECK("R2: ... the stand-in plays 64 there afterwards (heard as played)", plays(t, 5) == 64u);
    CHECK("R2: ... and step 4 and 6 still play theirs", plays(t, 4) == PAT[4] && plays(t, 6) == PAT[6]);
    song.rec = 0; stop();

    /* the stand-in from a remembered chord (E4): the stored offset is relative to its root */
    arm_arp(0, V_MONO);
    tide_chord[0][0] = 64; tide_chord_n[0] = 1;
    wait_arp(t, 5);
    tap(t, 67, 40);
    CHECK("R2: stand-in chord E4 (root 64): G4 is stored as 63", t->step[5].note[0] == 63u);
    CHECK("R2: ... and the stand-in plays 67 there", plays(t, 5) == 67u);
    song.rec = 0; stop();

    /* past the middle of a step: the next step, played once */
    arm_arp(0, V_MONO);
    wait_mid(t, 5); memcpy(was, t->step, sizeof was);
    input_on(t, 64, 100);
    CHECK("R2: tapped past the middle of arp step 5: it goes into step 6", t->step[6].note[0] == 64u && t->step[5].note[0] == PAT[5]);
    seen = 0;
    for (i = 0; i < 40u; i++) { run(16); if (t->arp_idx % 16u == 6u && t->arp_note == 64u) seen = 1; }   /* (the key sounds on its own voice: the arp must not fire it too) */
    input_off(t, 64);
    CHECK("R2: ... and the arp does not fire it again in this pass", !seen);
    CHECK("R2: ... it plays there in the next pass", plays(t, 6) == 64u);
    song.rec = 0; stop();

    /* a hold: 3 steps */
    arm_arp(0, V_MONO);
    wait_arp(t, 5); memcpy(was, t->step, sizeof was);
    input_on(t, 64, 100);
    wait_arp(t, 8); run(16);
    CHECK("R2: E4 held from step 5 into 8: steps 6 and 7 are TIES", t->step[6].time == ST_TIE && t->step[7].time == ST_TIE && t->step[5].note[0] == 64u);
    input_off(t, 64); run(16);
    CHECK("R2: ... released early in step 8: that step is put back", !memcmp(&t->step[8], &was[8], sizeof was[8]));
    CHECK("R2: ... and nothing else changed", only(t, was, 5, 99) == 0 && !memcmp(&t->step[9], &was[9], sizeof was[9]) && !memcmp(&t->step[4], &was[4], sizeof was[4]));
    song.rec = 0; stop();

    /* ARATE 1/8 with SDIV 1/16: the arp's grid decides */
    arm_arp(0, V_MONO);
    t->p[P_ARATE] = 1;
    run(1000);
    wait_arp(t, 3); memcpy(was, t->step, sizeof was);
    ok = t->seq_idx % 16u != 3u;                         /* (the sequencer's cursor is elsewhere: it runs twice as fast) */
    tap(t, 64, 40);
    CHECK("R2: ARATE 1/8, SDIV 1/16: the key goes into the arp's step 3", t->step[3].note[0] == 64u && ok);
    CHECK("R2: ... and into no step of the sequencer's cursor", only(t, was, 3, 99));
    song.rec = 0; stop();

    /* MONO, a key held three loops: no drift */
    arm_arp(0, V_MONO);
    input_on(t, 64, 100);
    {
        step_t snap[3][NSTEP];
        run(LOOP_MS + 200); memcpy(snap[0], t->step, sizeof snap[0]);
        run(LOOP_MS); memcpy(snap[1], t->step, sizeof snap[1]);
        run(LOOP_MS); memcpy(snap[2], t->step, sizeof snap[2]);
        CHECK("R2: MONO, E4 held three loops: the steps are the same after loop 1, 2 and 3", !memcmp(snap[0], snap[1], sizeof snap[0]) && !memcmp(snap[1], snap[2], sizeof snap[0]));
    }
    input_off(t, 64); song.rec = 0; stop();

    /* POLY, ADD: an empty step takes it as note[0], a filled one appends (inert) */
    arm_arp(0, V_POLY);
    memset(&t->step[5], 0, sizeof t->step[5]);
    wait_arp(t, 5); memcpy(was, t->step, sizeof was);
    tap(t, 64, 40);
    CHECK("R2: POLY ADD: an empty step 5 gets 64 as note[0]", t->step[5].n == 1u && t->step[5].note[0] == 64u && t->step[5].time == ST_NOTE);
    wait_arp(t, 9);
    tap(t, 64, 40);
    CHECK("R3: POLY ADD on an ARP SEQ part: a filled step 9 takes 64 as its one note (the arp plays note[0] only: a key sets the step's note)", t->step[9].n == 1u && t->step[9].note[0] == 64u);
    song.rec = 0; stop();

    /* POLY, REPLACE on an ARP SEQ part: a key sets the step's note, as in ADD (R3): two keys together leave the last one, the next pass again */
    arm_arp(0, V_POLY); tide_rec_replace = 1;
    wait_arp(t, 5); memcpy(was, t->step, sizeof was);
    tap(t, 64, 40);
    CHECK("R2: POLY REPLACE: E4 at step 5 replaces its note", t->step[5].n == 1u && t->step[5].note[0] == 64u);
    CHECK("R2: ... no other step changed", only(t, was, 5, 99));
    wait_arp(t, 5);
    input_on(t, 64, 100); input_on(t, 67, 100); run(40); input_off(t, 64); input_off(t, 67);
    CHECK("R3: ... the next pass E4+G4 together: one note, the last key (the step holds one note on ARP SEQ)", t->step[5].n == 1u && t->step[5].note[0] == 67u);
    wait_arp(t, 5);
    tap(t, 72, 40);
    CHECK("R2: ... and the pass after, a key sets the note again", t->step[5].n == 1u && t->step[5].note[0] == 72u);
    song.rec = 0; stop();
    (void)l;
}

/* ---- REC = ADD / REPLACE on an ordinary part (owner 2026-11-08) ---- */
static void two_notes(track_t *t, uint32_t k) { t->step[k].n = 2; t->step[k].note[0] = 60; t->step[k].note[1] = 64; }
static void rec_modes(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    uint32_t has;

    boot(); tide_rec_replace = 0; setup(0, 0, V_POLY); two_notes(t, 5); two_notes(t, 9);
    start(); song.rec = 1;
    while (t->seq_idx != 5u) run(16);
    memcpy(was, t->step, sizeof was);
    tap(t, 67, 30);
    CHECK("REC ADD: G4 recorded into a step holding C4+E4: C4+E4+G4", t->step[5].n == 3u && t->step[5].note[0] == 60u && t->step[5].note[1] == 64u && t->step[5].note[2] == 67u);
    CHECK("REC ADD: ... an untouched step keeps its notes", t->step[9].n == 2u);
    song.rec = 0; stop();

    boot(); tide_rec_replace = 1; setup(0, 0, V_POLY); two_notes(t, 5); two_notes(t, 9);
    start(); song.rec = 1;
    while (t->seq_idx != 5u) run(16);
    tap(t, 67, 30);
    CHECK("REC REPLACE: G4 recorded into a step holding C4+E4: G4 alone", t->step[5].n == 1u && t->step[5].note[0] == 67u);
    CHECK("REC REPLACE: ... a step nobody plays into keeps its notes (the playhead erases nothing)", t->step[9].n == 2u && t->step[9].note[0] == 60u && t->step[9].note[1] == 64u);
    while (t->seq_idx == 5u) run(16);                    /* (the next pass) */
    while (t->seq_idx != 5u) run(16);
    input_on(t, 67, 100); input_on(t, 71, 100); run(30); input_off(t, 67); input_off(t, 71);
    has = t->step[5].n == 2u && t->step[5].note[0] == 67u && t->step[5].note[1] == 71u;
    CHECK("REC REPLACE: G4+B4 played together: both, in place of what was there", has);
    while (t->seq_idx == 5u) run(16);
    while (t->seq_idx != 5u) run(16);
    tap(t, 72, 30);
    CHECK("REC REPLACE: the next pass, one key replaces the chord again", t->step[5].n == 1u && t->step[5].note[0] == 72u);
    CHECK("REC REPLACE: ... a step still untouched keeps its notes", t->step[9].n == 2u);
    song.rec = 0; stop();

    /* MONO keeps one note per step in both modes */
    boot(); tide_rec_replace = 1; setup(0, 0, V_MONO); two_notes(t, 5);
    start(); song.rec = 1;
    while (t->seq_idx != 5u) run(16);
    tap(t, 67, 30);
    CHECK("REC REPLACE, MONO: one note", t->step[5].n == 1u && t->step[5].note[0] == 67u);
    song.rec = 0; stop();
    tide_rec_replace = 0;
}

/* ---- the REC row of TIDE SETTINGS ---- */
static void rec_row(void)
{
    uint32_t i;
    boot();
    tide_seen_ok = 0; tide_rec_replace = 1;
    memset(tide_tail, 0, TIDE_TAIL_N); tide_tail_new = 0;
    tide_settings();                                     /* no record: the defaults */
    CHECK("REC row: with no record it is ADD", tide_rec_replace == 0u && jl_get(JL_REC) == 0u && !strcmp(jl_vname(JL_REC, 0), "ADD") && !strcmp(jl_vname(JL_REC, 1), "REPLACE"));
    CHECK("REC row: after AMOUNT (1.0.0-b7; ROLL TYPE before it), before PLAY MODE, with two values; PLAY MODE, XFADE, SPLIT stay the last three", JL_REC == JL_AMOUNT + 1u && JL_AMOUNT == JL_ROLL + 1u && JL_MODE == JL_REC + 1u && JL_SPLIT == JL_N - 1u && jl_n(JL_REC) == 2u && !strcmp(JL_NAME[JL_REC], "REC"));
    jl_put(JL_STYLE, STY_PEAK); tide_settings();
    jl_put(JL_REC, 1); tide_settings();
    CHECK("REC row: set to REPLACE, the STYLE stays PEAK (not CUSTOM)", tide_rec_replace == 1u && tide_style == STY_PEAK);
    for (i = 0; i < 5u; i++) { jl_put(JL_STYLE, i); tide_settings(); }
    CHECK("REC row: picking a STYLE leaves REC as it is", tide_rec_replace == 1u);
    jl_put(JL_STYLE, STY_CUSTOM); tide_settings();
    CHECK("REC row: ... and CUSTOM too", tide_rec_replace == 1u);
    jl_put(JL_REC, 0); tide_settings();
    CHECK("REC row: back to ADD, the STYLE is not touched", tide_rec_replace == 0u && tide_style == STY_CUSTOM);
    jl_put(JL_REC, 1); tide_settings();
    CHECK("REC row: kept in TIDE's record (field count 13, the last byte 1)", tide_tail[4] == TS_N && tide_tail[5 + TS_REC] == 1u && TS_N == 13);
    tide_rec_replace = 0; tide_seen_ok = 0;
    tide_settings();
    CHECK("REC row: survives a save and a load (power-on)", tide_rec_replace == 1u);
    {   /* an older record, 12 fields (before REC) */
        uint8_t keep = tide_tail[5 + TS_REC];
        (void)keep;
        tide_tail[4] = 12; tide_tail[5 + TS_REC] = 1;    /* (the byte past the count is not read) */
        tide_rec_replace = 1; tide_seen_ok = 0;
        tide_settings();
        CHECK("REC row: an older record without the field loads as ADD", tide_rec_replace == 0u);
    }
    tide_rec_replace = 0;
}

static void h5(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    uint32_t i, other = 1, l, tail = 1;
    boot(); setup(0, 0, V_POLY);
    start();
    song.rec = 1;
    while (t->seq_idx != 5u) run(16);
    memcpy(was, t->step, sizeof was);
    input_on(t, 64, 100); run(300); input_off(t, 64); run(LOOP_MS); song.rec = 0;
    {
        const step_t *s = &t->step[5];
        uint32_t has = 0;
        for (i = 0; i < s->n && i < 4u; i++) has |= s->note[i] == 64u;
        CHECK("H5 (arp OFF): the tap at step 5 adds 64 to step 5", has);
    }
    for (i = 0; i < 16u; i++)
        if (i != 5u && i != 6u && memcmp(&t->step[i], &was[i], sizeof was[i])) other = 0;
    CHECK("H5 (arp OFF): no other step changes but the tie into 6", other);
    setup(0, 0, V_MONO); song.rec = 1;                   /* L3: a key held longer than the loop fills it with ties */
    input_on(t, 64, 100);
    for (l = 0; l < 3u; l++) run(LOOP_MS);
    input_off(t, 64); song.rec = 0;
    for (i = 0; i < 16u; i++) tail &= t->step[i].time == ST_TIE || t->step[i].note[0] == 64u || t->step[i].note[0] == 0u;
    CHECK("H5 (arp OFF, L3): a key held for whole loops leaves ties and that one note", tail);
    stop();
}

/* ---- R4: REC on HOME ---- */
static void to_home(void) { go_main_to(HP_MORPH); frames(600); }
static void r4(void)
{
    track_t *t = &trk[0];
    boot(); setup(0, AM_SEQ_ID, V_POLY);
    song.sel = 0;
    go_page(GR_ROLL); frame();                           /* STEP */
    start();                                             /* (stopped, STEP arms step recording and says KEYS REPLACE STEPS) */
    press(B_REC); frames(600);
    CHECK("R4: REC on STEP arms the selected part", song.rec == 1u);
    CHECK("R4: ... on an ARP SEQ part the toast says the keys record", msg_is("ARP SEQ: KEYS RECORD"));
    to_home();
    CHECK("R4: HOME is where the test is", at_home());
    t->p[P_AHOLD] = 0;
    press(B_REC); frames(600);
    CHECK("R4 (L6): armed, REC on HOME disarms", song.rec == 0u);
    CHECK("R4 (L6): ... and leaves HOLD alone", t->p[P_AHOLD] == 0);
    CHECK("R4 (L6): ... and says REC OFF", msg_is("REC OFF"));
    stop();
    song.rec = 0;
    press(B_REC); frames(600);

    CHECK("R4: not armed, REC on HOME is the HOLD switch", song.rec == 0u && t->p[P_AHOLD] != 0);
    CHECK("R4 (L5): ... and the toast says how to record", msg_is("HOLD ON: SEQ+REC=REC"));
    press(B_REC); frames(600);
    CHECK("R4: the next tap: HOLD OFF", t->p[P_AHOLD] == 0 && msg_is("HOLD OFF: SEQ+REC=REC"));
    CHECK("R4 (L5): the toasts fit the header (message font, or the smaller one)", text_w(&AF_M, "HOLD OFF: SEQ+REC=REC") <= 150 && text_w(&AF_M, "ARP SEQ: KEYS RECORD") <= 150);
    printf("liverec: toast widths  M %d / S %d px (HOLD OFF: SEQ+REC=REC), M %d / S %d px (ARP SEQ: KEYS RECORD), header 150 px\n",
           text_w(&AF_M, "HOLD OFF: SEQ+REC=REC"), text_w(&AF_S, "HOLD OFF: SEQ+REC=REC"),
           text_w(&AF_M, "ARP SEQ: KEYS RECORD"), text_w(&AF_S, "ARP SEQ: KEYS RECORD"));
    /* another part armed, not the selected one: the tap behaves as unarmed (the header ring shows the other part) */
    song.rec = 2u; song.sel = 0; t->p[P_AHOLD] = 0;
    press(B_REC); frames(600);
    CHECK("R4: part 2 armed, part 1 selected: REC on HOME is HOLD, the arming stays", song.rec == 2u && t->p[P_AHOLD] != 0);
    song.rec = 0; t->p[P_AHOLD] = 0;
    /* SEQ held + REC arms from HOME and starts a stopped transport */
    to_home(); song.sel = 0;
    btn_down(B_SEQ); frames(600);
    press(B_REC); frames(600);
    btn_up(B_SEQ); frames(600);
    CHECK("R4 (L5): SEQ held + REC on HOME arms the selected part", (song.rec & 1u) && t->p[P_AHOLD] == 0);
    CHECK("R4 (L5): ... and starts the stopped transport", song.playing || transport_req == 1u);
    song.rec = 0;
    stop();
    /* b7 (FR7 D6): SEQ + REC as a chord, both let go in one pass, and SEQ let go first: arms, HOLD alone */
    for (uint32_t w = 0; w < 2u; w++) {
        to_home(); song.sel = 0; song.rec = 0; t->p[P_AHOLD] = 0;
        btn_down(B_SEQ); frames(600);
        btn_down(B_REC); frames(100);
        btn_up(B_SEQ);
        if (w)
            frame();
        btn_up(B_REC); frames(600);
        CHECK(w ? "R4 (D6): SEQ let go before REC: arms, HOLD alone" : "R4 (D6): SEQ and REC let go together: arms, HOLD alone",
              (song.rec & 1u) && t->p[P_AHOLD] == 0);
        song.rec = 0;
        stop();
    }
}

/* ---- R5: PRESETS loads on STEP and PARAMETER LOCKS (owner 2026-11-08) ---- */
static void r5(void)
{
    track_t *t = &trk[0];
    static const uint32_t G[2] = {GR_ROLL, GR_EVENTS};
    static const char *const N[2] = {"STEP", "PARAMETER LOCKS"};
    uint32_t k, pre, locks, armed_locks;
    step_t was[NSTEP];
    for (k = 0; k < 2u; k++) {
        char b[96];
        boot(); setup(0, 0, V_POLY); song.sel = 0;
        go_page(G[k]); frames(100);
        memcpy(was, t->step, sizeof was);
        pre = t->preset; locks = motion.count;
        turn(EN_PRESET, 1); frames(200);
        snprintf(b, sizeof b, "R5: PRESETS on %s loads the next sound", N[k]);
        CHECK(b, t->preset != pre);
        snprintf(b, sizeof b, "R5: ... on %s the steps and the parameter locks are unchanged", N[k]);
        CHECK(b, same(t, was) && motion.count == locks);
    }
    boot(); setup(0, 0, V_POLY); song.sel = 0;
    go_page(GR_PATS); frames(100);
    pre = t->preset;
    turn(EN_PRESET, 1); frames(200);
    CHECK("R5: PRESETS on PATTERNS still does nothing", t->preset == pre);
    /* armed and playing, from STEP: the load records no lock and leaves the recording on */
    boot(); setup(0, 0, V_POLY); song.sel = 0;
    go_page(GR_ROLL); frames(100);
    start(); song.rec = 1; run(200);
    memcpy(was, t->step, sizeof was);
    pre = t->preset; armed_locks = motion.count;
    turn(EN_PRESET, 1); run(400);
    CHECK("R5: armed and playing, PRESETS on STEP loads the sound", t->preset != pre);
    CHECK("R5: ... records no parameter lock, keeps REC armed and the transport running", motion.count == armed_locks && song.rec == 1u && song.playing);
    song.rec = 0;
    stop();
}

/* ---- R6: the limits and the corners the review found ---- */
static uint32_t voices_on(track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
static void wait_seq(track_t *t, uint32_t k) { uint32_t n = 0; while (!(t->seq_idx == k && t->seq_pos < 1200u) && n++ < 2000u) run(16); }   /* (early in step k: the key lands in it) */
static uint32_t held_on(track_t *t, uint32_t note)   /* voices of this note still held by a key */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && t->v[i].note == note;
    return n;
}
static void leave_seq(track_t *t, uint32_t k) { uint32_t n = 0; while (t->seq_idx == k && n++ < 2000u) run(16); }
/* FR9 (b7, review B2): a lock on an ARP SEQ part sounds on the arp's step, not the sequencer's: a key pressed at the sequencer's step 9 restarts
 * the arp's pattern; the lock on step 5 comes with the arp's step 5. Also with ARP RATE 1/8 against DIV 1/16 */
static void fr9(void)
{
    track_t *t = &trk[0];
    for (uint32_t r = 0; r < 2u; r++) {
        int16_t base, lv;
        boot(); setup(0, AM_SEQ_ID, V_POLY);
        if (r)
            t->p[P_ARATE] = 1;                          /* (1/8: the arp at half the sequencer's rate) */
        base = t->p[P_REV]; lv = (int16_t)(base == 100 ? 40 : 100);
        motion_set_lock(t, 5, P_REV, lv);
        start();
        wait_seq(t, 9);
        input_on(t, 60, 100);
        wait_arp(t, 4); run(16);
        CHECK(r ? "FR9: ARP RATE 1/8: the arp's step 4 plays without the lock" : "FR9: ARP SEQ, a key at the sequencer's step 9: the arp's step 4 plays without the lock", t->p[P_REV] == base);
        wait_arp(t, 5); run(16);
        CHECK(r ? "FR9: ARP RATE 1/8: the lock on step 5 sounds on the arp's step 5" : "FR9: ARP SEQ: the lock on step 5 sounds on the arp's step 5", t->p[P_REV] == lv);
        input_off(t, 60);
        stop();
        motion_clear(t);
    }
}
static void r6(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    uint32_t len, i;

    /* REPLACE on a pattern of 1 and of 2 steps: each pass replaces, the last key wins */
    for (len = 1; len <= 2; len++) {
        char b[96];
        boot(); tide_rec_replace = 1; setup(0, 0, V_POLY); t->p[P_SLEN] = (int16_t)len; two_notes(t, 0);
        start(); song.rec = 1;
        wait_seq(t, 0);
        tap(t, 64, 30);
        snprintf(b, sizeof b, "R6: REPLACE, SLEN %u: the first key replaces the two notes", len);
        CHECK(b, t->step[0].n == 1u && t->step[0].note[0] == 64u);
        leave_seq(t, 0); wait_seq(t, 0);
        tap(t, 67, 30);
        snprintf(b, sizeof b, "R6: REPLACE, SLEN %u: a pass later the next key replaces it again (the mark is cleared)", len);
        CHECK(b, t->step[0].n == 1u && t->step[0].note[0] == 67u);
        song.rec = 0; stop();
    }
    /* the pattern length changes mid-pass: the marks of the steps inside the new length still clear */
    boot(); tide_rec_replace = 1; setup(0, 0, V_POLY); two_notes(t, 3);
    start(); song.rec = 1;
    wait_seq(t, 3);
    tap(t, 67, 30);
    CHECK("R6: REPLACE, SLEN 16: step 3 takes G4 alone", t->step[3].n == 1u && t->step[3].note[0] == 67u);
    t->p[P_SLEN] = 5; two_notes(t, 3);
    leave_seq(t, 3); wait_seq(t, 3);
    tap(t, 71, 30);
    CHECK("R6: ... SLEN 5 set mid-pass: a pass later the key still replaces (one note, B4)", t->step[3].n == 1u && t->step[3].note[0] == 71u);
    song.rec = 0; stop();

    /* ARP SEQ in a POLY stretch (loop 4 of 16): the REPLACE marks of the steps the arp walks clear 2 behind IT, not 2 behind SLEN (review R2) */
    boot(); tide_rec_replace = 1; setup(0, AM_SEQ_ID, V_POLY);
    poly_len[0] = 4; poly_base[0] = 0;
    t->arp_idx = 4u; t->arp_skip = 0;                    /* (idx 0 of the stretch) */
    t->rec_wr[0] = (1u << 2) | (1u << 3) | (1u << 14) | (1u << 15);
    arp_rec_fire(t);
    CHECK("R6 (R2): POLY stretch of 4 in 16, the arp enters its step 0: the mark of step 2 (two behind, in the stretch) clears and step 3 stays", !(t->rec_wr[0] & (1u << 2)) && (t->rec_wr[0] & (1u << 3)));
    CHECK("R6 (R2): ... steps 14 and 15 (outside the stretch) are not touched", (t->rec_wr[0] & ((1u << 14) | (1u << 15))) == ((1u << 14) | (1u << 15)));
    poly_len[0] = 0; t->rec_wr[0] = 0; tide_rec_replace = 0;

    /* ARP SEQ part, a key held (the keyboard's way: key_on, key_off through kb_trk): transport stop / disarm / another part selected / STOP ALL, then the key up:
     * a voice is held just before it (so the check cannot pass with nothing sounding), none after, the mark cleared */
    for (i = 0; i < 4u; i++) {
        char b[110];
        uint32_t before;
        arm_arp(0, V_POLY);
        wait_arp(t, 5);
        kb_note[3] = 64; kb_trk[3] = 0;
        key_on(3, t); run(100);
        CHECK("R6: a key held on an armed ARP SEQ part sounds on its own voice", held_on(t, 64) >= 1u && rec_key_set(t, 64, 1) == 1u);
        if (i == 0u) { transport_req = 2u; run(100); }
        else if (i == 1u) { song.rec = 0; run(100); }
        else if (i == 2u) { song.sel = 1; run(100); }
        else { stop_all(); run(100); }
        before = held_on(t, 64);
        if (i == 3u)
            CHECK("R6 (R5): STOP ALL clears the key's mark (kb_chn is zeroed, the key up never reaches input_off) and ends its voice", rec_key_set(t, 64, 0) == 0u && before == 0u);
        else
            CHECK("R6: ... the voice is still held just before the key up", before >= 1u);
        key_off(3, &trk[kb_trk[3] % NTRK]); run(300);
        snprintf(b, sizeof b, "R6: ... after %s, the key up lets its voice go (none held)", i == 0u ? "a stop" : i == 1u ? "a disarm" : i == 2u ? "selecting the other part" : "STOP ALL");
        CHECK(b, held_on(t, 64) == 0u);
        CHECK("R6: ... and its mark is clear", rec_key_set(t, 64, 0) == 0u);
        song.rec = 0; song.sel = 0; stop();
    }
    /* a restart with a key still held: the marks go (R5) and the key up still ends the voice */
    arm_arp(0, V_POLY);
    wait_arp(t, 5);
    input_on(t, 64, 100); run(100);
    transport_req = 2u; run(100); start(); run(200);
    CHECK("R6 (R5): a start clears the marks of the keys held", rec_key_set(t, 64, 0) == 0u);
    input_off(t, 64); run(300);
    CHECK("R6 (R5): ... and the key up still ends the voice", held_on(t, 64) == 0u);
    song.rec = 0; stop();

    /* disarm in the middle of a hold: the hold and the skipped step go */
    arm_arp(0, V_MONO);
    wait_mid(t, 5);
    input_on(t, 64, 100);
    CHECK("R6: a key past the middle of arp step 5 is put into step 6 and skipped there", t->arp_skip != 0u && t->rh_n == 1u);
    song.rec = 0; run(300);
    CHECK("R6: disarmed mid-hold: rh_n and arp_skip are clear", t->rh_n == 0u && t->arp_skip == 0u);
    input_off(t, 64); run(300);
    CHECK("R6: ... and the key up ends the voice", held_on(t, 64) == 0u && rec_key_set(t, 64, 0) == 0u);
    stop();

    /* MIDI OUT (R4): an armed ARP SEQ part's key goes out as exactly one note-on (its note, its velocity) and, at its key up, exactly one note-off */
    {
        uint32_t m0, m, on = 0, off = 0, onv = 0, mc;
        arm_arp(0, V_POLY);
        wait_arp(t, 5);
        usb.config = 1;
        mc = trk_midi_ch(0);
        kb_note[3] = 64; kb_trk[3] = 0;
        m0 = mo_w;
        key_on(3, t);
        run(100);
        for (m = m0; m != mo_w; m++) {
            uint32_t pk = midi_out_q[m % MQ];
            if (((pk >> 16) & 0xFFu) != 64u) continue;
            if (((pk >> 8) & 0xFFu) == (0x90u | mc)) { on++; onv = pk >> 24; }
            if (((pk >> 8) & 0xFFu) == (0x80u | mc)) off++;
        }
        CHECK("R6 (R4): the key is a bypassed key (mark set), sent to MIDI OUT", rec_key_set(t, 64, 1) == 1u && ((kb_mo >> 3) & 1u));
        CHECK("R6 (R4): ... exactly one note-on of note 64 on the part's channel, at the key's velocity, and no note-off yet", on == 1u && off == 0u && onv == kb_svel[3] && onv != 0u);
        m0 = mo_w;
        key_off(3, t);
        run(300);
        on = off = 0;
        for (m = m0; m != mo_w; m++) {
            uint32_t pk = midi_out_q[m % MQ];
            if (((pk >> 16) & 0xFFu) != 64u) continue;
            if (((pk >> 8) & 0xFFu) == (0x90u | mc)) on++;
            if (((pk >> 8) & 0xFFu) == (0x80u | mc)) off++;
        }
        CHECK("R6 (R4): ... its key up sends exactly one note-off of note 64 (and no second note-on)", off == 1u && on == 0u && !((kb_mo >> 3) & 1u));
        CHECK("R6 (R4): ... no voice held", held_on(t, 64) == 0u);
        usb.config = 0;
        song.rec = 0; stop();
    }

    /* accent and velocity */
    arm_arp(0, V_MONO);
    wait_arp(t, 5);
    input_on(t, 64, 120); run(40); input_off(t, 64);
    CHECK("R6: ARP SEQ, a key at velocity 120: the step is accented and holds 120", (t->step[5].flags & SF_ACCENT) && t->step[5].vel == 120u);
    song.rec = 0; stop();
    boot(); tide_rec_replace = 1; setup(0, 0, V_POLY); two_notes(t, 5);
    start(); song.rec = 1;
    wait_seq(t, 5);
    memcpy(was, t->step, sizeof was);
    input_on(t, 64, 120); input_on(t, 67, 80); run(30); input_off(t, 64); input_off(t, 67);
    CHECK("R6: POLY part, REPLACE, a chord at velocity 120 then 80: both notes, accent, velocity 120 stays", t->step[5].n == 2u && (t->step[5].flags & SF_ACCENT) && t->step[5].vel == 120u);
    song.rec = 0; stop();
    tide_rec_replace = 0;
}

/* ---- REC survives a settings backup and restore ---- */
static void rec_backup(void)
{
    persist_t p;
    boot();
    tide_seen_ok = 0; tide_rec_replace = 1;
    memset(tide_tail, 0, TIDE_TAIL_N); tide_tail_new = 0;
    tide_settings();
    tide_rec_replace = 1;
    jl_put(JL_REC, 1); tide_settings();
    settings_export(&p);                                 /* the backup's settings object */
    memset(tide_tail, 0, TIDE_TAIL_N);                   /* a unit with nothing */
    tide_rec_replace = 0; tide_seen_ok = 0;
    CHECK("REC backup: the settings object restores", settings_import(&p, sizeof p) == 1);
    tide_settings();
    CHECK("REC backup: REC = REPLACE is back after the restore", tide_rec_replace == 1u);
    tide_rec_replace = 0;
}

int main(void)
{
    h1(); r2(); h5(); rec_modes(); rec_row(); r6(); rec_backup(); r4(); r5(); fr9();
    printf("liverec: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
