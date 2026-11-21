/* SPDX-License-Identifier: GPL-3.0-only */
/* B16 lane A (owner notes 2026-11-19 / 20, items 1, 1b, 2): the SEQ tools in every TIDE state. Host group "seqtools".
 * Each SEQ tool - SWELL and RIPPLE on the take's part and on the answer's part, KEEP, CLEAR, and the STEP page's one-step clear (EDIT) - is pressed in every
 * combination of ANSWER (BASS LINE / OFF), ANSWER ON (the other part / OFF), REC (armed on the take's part / not) and the transport (playing / stopped),
 * after a take; with no answer made (ANSWER OFF or ANSWER ON OFF) both ways round: turned off after the answer was made, and off before the take.
 * What must hold:
 *   - never silent: every press says what it did or why it did nothing, in the header;
 *   - an answer made (ANSWER + ANSWER ON): SWELL / RIPPLE on the take's or the answer's part wait for the loop start while playing (NEXT LOOP), land at once
 *     when stopped, and with REC armed are remembered for the pass (NEXT PASS);
 *   - no answer made: SWELL / RIPPLE change the pressed part only, at once (no NEXT LOOP, no NEXT PASS), and the other part plays on as it is (the answer
 *     is never dropped, RIPPLE never does nothing);
 *   - what lands says how many steps of the pressed part changed ("RIPPLE  3 STEPS"), and that count is the steps that differ;
 *   - one undo: OCT- held + a PRESETS turn gives the part back (SWELL, RIPPLE, CLEAR, the step clear); with REC armed the pass's take holds the change;
 *   - KEEP's answer (NOTHING TO KEEP, SEQ KEPT) stays readable (3 s);
 *   - QUICK SEQ's header names the part the tools act on; a guessed key says GUESSED, never a bare "?".
 * Built on tests/take_test.c's harness (its helpers; its own main is not run here). */
#define TAKE_TEST_NO_MAIN 1
#include "take_test.c"

static const char *const TOOL[2] = {"SWELL", "RIPPLE"};
static char sc[96];                                     /* the scenario's name, in front of every check */
#define SCHECK(what, cond) do { char w_[200]; snprintf(w_, sizeof w_, "%s: %s", sc, what); CHECK(w_, cond); } while (0)

/* a take on part 1 (index 0) with ANSWER / ANSWER ON as given while it closes; part 2 holds the PAT pattern (16 notes) */
static void make_take(uint32_t ans, uint32_t anson)
{
    boot();
    empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY);
    tide_answer = (uint8_t)ans; tide_answer_on = (uint8_t)anson;
    start();
    song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); rec_note_at(0, 14, 74, 60);
    rec_note_at(0, 18, 72, 60); rec_note_at(0, 22, 69, 60);
    at_step(&trk[0], 26);
    rec_off();
}
/* the state: the take (its answer made when on before), then the settings, the transport, REC armed on the take's part */
static void state(uint32_t ans, uint32_t anson, uint32_t rec, uint32_t play, uint32_t before)
{
    if (before)
        make_take(ans, anson);
    else {
        make_take(TA_BASS, 0);
        tide_answer = (uint8_t)ans; tide_answer_on = (uint8_t)anson;
        run(48); land();
    }
    if (!play)
        stop();
    if (rec) {
        song.rec = 1;
        run(100);
    }
    ui.msg_t = 0;                                        /* (the take's own message is not what a press says) */
    snprintf(sc, sizeof sc, "ANSWER %s, ANSWER ON %s%s, %s, %s", ans ? "BASS LINE" : "OFF", anson ? "OFF" : "OTHER PART", before ? " (before the take)" : "",
             rec ? "REC armed" : "REC off", play ? "playing" : "stopped");
}
static uint32_t on_now(void) { return tide_answer && !tide_answer_on; }
/* OCT- held + a PRESETS turn left through the panel, for the plain undo (ui.c undo_swap: the take history's gesture() reports only that history's moves) */
static void undo_gesture(void) { (void)gesture(-1); }

/* SWELL / RIPPLE pressed on part p */
static void sw_case(uint32_t ans, uint32_t anson, uint32_t rec, uint32_t play, uint32_t before, uint32_t p, uint32_t ripple)
{
    static step_t pre[64], other[64];
    uint32_t cnt, n, o = 1u - p, on, w0;
    char w[96];
    state(ans, anson, rec, play, before);
    on = on_now();
    snprintf(sc + str_len(sc), sizeof sc - str_len(sc), ", %s on part %u (%s)", TOOL[ripple], (unsigned)p + 1u, p ? "the answer's" : "the take's");
    if (play)
        at_step(&trk[p], 4);                             /* (mid-loop: a press on the loop's last step lands at once) */
    memcpy(pre, trk[p].step, sizeof pre);
    memcpy(other, trk[o].step, sizeof other);
    w0 = osig(pre);
    cnt = tk_cnt;
    sw_press(p, ripple);
    SCHECK("the press says something", ui.msg_t && ui.msg[0]);
    if (on && rec) {                                     /* remembered for the armed pass */
        snprintf(w, sizeof w, "%s  NEXT PASS", TOOL[ripple]);
        SCHECK("an answer is made and REC is armed: NEXT PASS, nothing changed yet", said(w) && tk_force == 1u + ripple && !memcmp(pre, trk[p].step, sizeof pre));
        if (play) {
            static step_t ans0[64];
            memcpy(ans0, trk[1].step, sizeof ans0);
            to_loop_end(0);
            land();
            SCHECK("... the end of the pass makes the answer again (SWELL) or moves it (RIPPLE): the answer's part changed, the press is used up", steps_diff(ans0, 1) >= 1u && !tk_force);
            if (ripple)
                SCHECK("... RIPPLE keeps the answer's rhythm and says how many steps moved", osig(trk[1].step) == osig(ans0) && said("RIPPLE  ") && then_says("BASS LINE"));
        }
        if (play)
            at_step(&trk[0], 3);
        rec_off();
        return;
    }
    if (on && play) {                                    /* the take's part or the answer as the take made it: the loop start */
        snprintf(w, sizeof w, "%s  NEXT LOOP", TOOL[ripple]);
        SCHECK("an answer is made, playing: it waits for the loop start (NEXT LOOP), nothing changed yet", said(w) && tk_sw_k == 1u + ripple && !memcmp(pre, trk[p].step, sizeof pre));
        to_loop_end(p);
        land();
    }
    n = steps_diff(pre, p);
    SCHECK("it landed: the pressed part changed, nothing waits", n >= 1u && !tk_sw_k && !tk_force);
    SCHECK("... the header says how many steps of the pressed part changed, exactly", said_steps(TOOL[ripple], n));
    if (ripple)
        SCHECK("... RIPPLE keeps the rhythm (the onsets and chord sizes)", osig(trk[p].step) == w0);
    if (on) {
        SCHECK("... with the answer's words next", then_says("BASS LINE"));
    } else {
        SCHECK("no answer made: at once (no NEXT LOOP / NEXT PASS) and the other part plays on as it is (an answer is never dropped)", !memcmp(other, trk[o].step, sizeof other));
        SCHECK("... no answer's words follow", !ui.msg2[0]);
    }
    if (rec) {
        SCHECK("REC armed: no history step of its own (the pass's take holds it)", tk_cnt == cnt);
        if (play)
            at_step(&trk[0], 3);
        rec_off();
        return;
    }
    SCHECK("one history step", tk_cnt == (cnt < 2u ? cnt + 1u : 2u));
    snprintf(w, sizeof w, "UNDO %s", TOOL[ripple]);
    SCHECK("OCT- + PRESETS gives the part back, the header says UNDO and the tool", gesture(-1) == 1u && said(w) && !memcmp(pre, trk[p].step, sizeof pre));
}

/* CLEAR (SEQ held + F3) on the answer's part, and its undo */
static void clear_case(uint32_t ans, uint32_t anson, uint32_t rec, uint32_t play)
{
    static step_t pre[64];
    uint32_t i, empty = 1, len;
    state(ans, anson, rec, play, 0);
    snprintf(sc + str_len(sc), sizeof sc - str_len(sc), ", CLEAR on part 2");
    song.sel = 1;                                        /* (the tools act on the selected part) */
    memcpy(pre, trk[1].step, sizeof pre);
    len = (uint32_t)trk[1].p[P_SLEN];
    tools_key(TL_CLEAR);
    for (i = 0; i < len; i++)
        empty &= trk[1].step[i].time == ST_REST && !trk[1].step[i].n;
    SCHECK("CLEAR empties the part's steps 1..LEN and says so", empty && msg_is("SEQUENCE CLEARED"));
    undo_gesture();
    SCHECK("... OCT- + PRESETS brings them back", !memcmp(pre, trk[1].step, sizeof pre));
    if (rec) {
        if (play)
            at_step(&trk[0], 3);
        rec_off();
    }
}

/* the STEP page: EDIT clears the step at the cursor (its locks with it), one undo; an empty step is left alone and keeps the last edit's undo */
static void step_case(uint32_t ans, uint32_t anson, uint32_t rec, uint32_t play)
{
    static step_t pre[64], ustep[64];
    uint32_t c = 3, before_trk;
    state(ans, anson, rec, play, 0);
    snprintf(sc + str_len(sc), sizeof sc - str_len(sc), ", STEP page EDIT clear on part 2");
    song.sel = 1;
    go_page(GR_ROLL); frame();
    SCHECK("the STEP page is shown", cur_page()->graph == GR_ROLL && song.seq_mode);
    trk[1].step[c].flags |= SF_ACCENT;
    memcpy(pre, trk[1].step, sizeof pre);
    ui.cursor = (uint8_t)c;
    SCHECK("EDIT on the STEP page acts there", page_tap(B_EDIT) == 1);
    SCHECK("... the step is cleared, the others stay", trk[1].step[c].time == ST_REST && !trk[1].step[c].n && !trk[1].step[c].flags &&
                                                         !memcmp(pre, trk[1].step, c * sizeof pre[0]) && !memcmp(pre + c + 1, trk[1].step + c + 1, (63u - c) * sizeof pre[0]));
    SCHECK("... the header says which step, then how to undo", msg_is("STEP 4 CLEARED") && str_eq(ui.msg2, "[OCT-] + PRESETS: UNDO") && ui.cursor == c + 1u);
    undo_gesture();
    SCHECK("... OCT- + PRESETS brings the step back", !memcmp(pre, trk[1].step, sizeof pre) && said("UNDO/REDO"));
    undo_gesture();
    SCHECK("... again: cleared again (the undo swaps)", trk[1].step[c].time == ST_REST && !trk[1].step[c].n);
    undo_gesture();
    SCHECK("... and back", !memcmp(pre, trk[1].step, sizeof pre));
    /* an empty step: nothing to clear, the undo copy stays the last edit's */
    memset(&trk[1].step[12], 0, sizeof trk[1].step[12]);
    memcpy(ustep, undo.step, sizeof ustep);
    before_trk = undo.trk;
    ui.cursor = 12;
    SCHECK("an empty step: EDIT says STEP EMPTY, the undo copy is not touched", page_tap(B_EDIT) == 1 && msg_is("STEP EMPTY") && undo.trk == before_trk && !memcmp(ustep, undo.step, sizeof ustep) && ui.cursor == 13u);
    go_title("MORPH"); frame();
    if (rec) {
        if (play)
            at_step(&trk[0], 3);
        rec_off();
    }
}

/* KEEP with nothing moved: it says so and the words stay readable */
static void keep_case(uint32_t ans, uint32_t anson, uint32_t rec, uint32_t play)
{
    uint32_t t0;
    state(ans, anson, rec, play, 0);
    snprintf(sc + str_len(sc), sizeof sc - str_len(sc), ", KEEP on part 1");
    song.sel = 0;
    seqm_set_fader(0, 0);
    tools_key(TL_KEEP);
    t0 = fm1_ms;
    SCHECK("KEEP with nothing moved says NOTHING TO KEEP", msg_is("NOTHING TO KEEP"));
    run(2500);
    SCHECK("... still there 2.5 s later (it was gone in a second)", msg_is("NOTHING TO KEEP") && fm1_ms - t0 >= 2400u);
    if (rec) {
        if (play)
            at_step(&trk[0], 3);
        rec_off();
    }
}
/* KEEP of the SEQ MORPH's variation: SEQ KEPT stays readable, then goes */
static void keep_kept(void)
{
    uint32_t i, t0;
    track_t *t;
    boot();
    song.sel = 0;
    t = TSEL;
    for (i = 0; i < NSTEP; i++)
        t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
    for (i = 0; i < 16u; i += 2u)
        t->step[i] = (step_t){{(uint8_t)(60 + i % 7u)}, 1, ST_NOTE, 0, 100, 0};
    t->p[P_SLEN] = 16;
    seqm_set_amount(0, 80);
    seqm_roll(0);
    seqm_set_fader(0, 127);
    tools_key(TL_KEEP);
    t0 = fm1_ms;
    snprintf(sc, sizeof sc, "KEEP of a moved pattern");
    SCHECK("SEQ KEPT", msg_is("SEQ KEPT"));
    run(2500);
    SCHECK("... still shown 2.5 s later", msg_is("SEQ KEPT") && fm1_ms - t0 >= 2400u);
    run(3000);
    SCHECK("... gone after 5.5 s", !msg_is("SEQ KEPT"));
}

/* QUICK SEQ's header names the part its tools act on (the selected part; ALGORITHM's middle keeps the last single part) */
static void head_case(void)
{
    uint8_t l = ui.layer, b = song.both, s = song.sel;
    snprintf(sc, sizeof sc, "QUICK SEQ header");
    ui.layer = LAYER_SEQ;
    song.both = 0; song.sel = 0;
    SCHECK("part 1 selected: QUICK SEQ  PART 1", str_eq(layer_head(), "QUICK SEQ  PART 1"));
    song.sel = 1;
    SCHECK("part 2 selected: QUICK SEQ  PART 2", str_eq(layer_head(), "QUICK SEQ  PART 2"));
    song.both = 1;
    SCHECK("ALGORITHM in the middle after part 2: the tools act on part 2 and the header says so", str_eq(layer_head(), "QUICK SEQ  PART 2") && TSEL == &trk[1]);
    song.sel = 0;
    SCHECK("ALGORITHM in the middle after part 1: PART 1", str_eq(layer_head(), "QUICK SEQ  PART 1") && TSEL == &trk[0]);
    ui.layer = LAYER_GLO;
    SCHECK("another layer keeps its own header", str_eq(layer_head(), "QUICK GLO"));
    ui.layer = l; song.both = b; song.sel = s;
}

/* a key guessed from the take (no SCL, no key in the music): the words say GUESSED, no "?" */
static void guess_case(void)
{
    char b[48];
    boot();
    proj_cur = PROJ_NO_SLOT;                             /* (music that came from no factory multi: it has no key) */
    empty_part(0); trk[0].p[P_SLEN] = 32; setup(1, 0, V_POLY);
    tide_answer = TA_BASS; tide_answer_on = 0;
    start();
    song.rec = 1; run(100);
    rec_note_at(0, 2, 69, 60); rec_note_at(0, 6, 72, 60); rec_note_at(0, 10, 76, 60); rec_note_at(0, 14, 74, 60);
    at_step(&trk[0], 26);
    rec_off();
    snprintf(sc, sizeof sc, "a guessed key");
    b[0] = 0;
    tk_words(b);
    SCHECK("the answer's key was guessed from the take", (tk_m.flags & 2u) != 0u);
    SCHECK("... its words end in GUESSED, no question mark", strstr(b, " GUESSED") && !strchr(b, '?'));
    printf("seqtools: the guessed key's words: \"%s\"\n", b);
    stop();
}

int main(void)
{
    uint32_t ans, anson, rec, play, before, p, r, n = 0;
    head_case();
    guess_case();
    keep_kept();
    for (ans = 0; ans < 2u; ans++)
        for (anson = 0; anson < 2u; anson++)
            for (rec = 0; rec < 2u; rec++)
                for (play = 0; play < 2u; play++) {
                    uint32_t a = ans ? TA_BASS : TA_OFF;
                    for (before = 0; before < (a && !anson ? 1u : 2u); before++)   /* (no answer made: off after the answer was made, and off before the take) */
                        for (p = 0; p < 2u; p++)
                            for (r = 0; r < 2u; r++, n++)
                                sw_case(a, anson, rec, play, before, p, r);
                    clear_case(a, anson, rec, play);
                    step_case(a, anson, rec, play);
                    keep_case(a, anson, rec, play);
                    n += 3u;
                }
    printf("seqtools: %u presses in 16 states\n", (unsigned)n);
    printf("seqtools: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
