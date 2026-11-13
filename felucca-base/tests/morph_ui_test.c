/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL build step 4b: the MORPH's page, its MIDI CCs, the matrix source, its settings (SPEC 3.4, 3.4a, 3.5, 4.4).
 *   ROUND      MORPH, MORPH SET, MODE, MIXER (v2, SPEC 3.5; 0.2.14: no OVERVIEW): HOME's taps and SELECT; MORPH is the main page
 *   KNOBS      MORPH (v3): KNOB 1 the fader, 2 ROLL A (left) / B (right), 3 nothing (1.0.0-b5: no UNDO; no KEEP), 4 AMOUNT;
 *              MORPH SET: KNOB 1 SMART, 2 ARP, 3 PITCH, 4 empty (0.2.9: TARGET is ALGORITHM's BOTH on the MORPH page). Each drives
 *              its morph call on the parts TARGET says (SELECTED: the part ALGORITHM picked; BOTH: both)
 *   GESTURE    a nudge below the detent never acts (rested, reversed); one twist past it acts exactly once, however far
 *   KNOB 3     an empty card (1.0.0-b5: no UNDO, the morph keeps no history); a twist either way at any fader changes no end
 *   MIDI       CC 20-22, 25, 111 and 113-115 (CC 23, UNDO's until 1.0.0-b5, and 24 unassigned) through the real input path (USB packets and TRS bytes into the queue, events_block,
 *              then the main loop): each part's channel, SELECTED and BOTH, the >= 64 triggers and their re-arm, a hidden
 *              part's channel ignored, ROUT SEL, a learned CC the user's
 *   MATRIX     the MORPH source (fader / 127), no feedback (a roll never makes a route from MORPH nor changes one)
 *   LOAD       a sound load: the fader at A, End A the sound, End B a fresh roll
 *   SAVE HELD  SAVE held (any page): the SAVE OVER dialog, OCT+ saves the patch with End B (a load gives it back); a press\n *              opens the SAVE pages; a factory preset saved over: a copy shadows it in the browser, erased it comes back
 *   SETTINGS   AMOUNT, SMART, ARP, PITCH kept in the settings bytes (TARGET's bit reserved, not read or written: 0.2.9), back after a power cycle; the default writes nothing
 * Built and run by DUELL/host_tests.sh (name: morphui): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#define UI_TEXT_CAPTURE 1                                 /* (a card read from the texts drawn: ui_test.c card_match; ui.col is a hash since 1.1) */
static unsigned char mtn[4][2];                                   /* each end's new values since its sound's load, 1 = the Original only (morph.c MORPH_TEST_NEW: the count n[] gave up to 1.0.0-b4) */
#define MORPH_TEST_NEW(pi, e, reset) (mtn[(pi) & 3u][(e) & 1u] = (unsigned char)((reset) ? 1u : mtn[(pi) & 3u][(e) & 1u] + 1u))
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("morphui: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

static void settle(void) { frames(600); }               /* the knobs at rest (a gesture is over) */
static uint32_t where(void) { return ui.page; }
static uint32_t page_of(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    return i;
}
static void morph_page(void) { ui.menu = 0; go_title("MORPH"); frames(32); }
/* a factory sound's AMOUNT as a load sets it (TIDE: MUSIC's own while ROLL is MUSIC, tide.h TIDE_FACTORY_AMOUNT) */
static uint32_t snd_amount(uint32_t e, uint32_t k) { return (uint32_t)TIDE_FACTORY_AMOUNT(ds_amount(e, k)); }
static uint32_t rolls(uint32_t pi, uint32_t e) { return mtn[pi & 3u][e & 1u]; }   /* 1 + the end's new values since the load (1: the Original only) */
static uint32_t seed_now(uint32_t pi, uint32_t e) { return mph[pi].seed[e][mph[pi].top[e]]; }
static const int8_t *endp(uint32_t pi, uint32_t e) { return mph[pi].h[e][mph[pi].top[e]]; }
static int end_is_base(uint32_t pi, uint32_t e) { return !memcmp(mph[pi].h[e][mph[pi].top[e]], mph[pi].base, P_COUNT); }
static int card_is(uint32_t c, const char *label, const char *val)   /* the card shows the label and the value (""/"": nothing drawn in it) */
{
    char want[40];
    if (!label[0] && !val[0])
        return card_hidden(c);
    snprintf(want, sizeof want, "%s|%s", label, val);
    if (card_match(c, want))
        return 1;
    return 0;                                 /* (TIDE 1.0.0-b4: MORPH's K1 is a labelled card, "MORPH|A"; no big-value fallback) */
}
/* TIDE b6: the MORPH page has no card row; KNOB 2 and KNOB 4 are one slim row above the footer (ui_draw.c morph_row, rows MROW_Y .. + MROW_H - 1), the waves above it.
 * mrow_at: the text s drawn in that row (its box, after screen_read); mrow_only: every text drawn in the row is one of ok[] (the knob dots "2" and "4", ROLL and its way,
 * AMOUNT and its value: no KNOB 3 dot, no UNDO or any other control); mrow_bar: ink between AMOUNT and the value (b6: AMOUNT has no bar any more, owner 2026-11-09); no_k3: no "3" knob dot and no UNDO anywhere
 * from the knob-1 line down to the footer */
static int mrow_at(const char *s, int32_t *x0, int32_t *x1)
{
    uint32_t i;
    screen_read();
    for (i = 0; i < txt_n; i++)
        if (txt_log[i].y0 >= MROW_Y && txt_log[i].y1 <= MROW_Y + MROW_H && !strcmp(txt_log[i].s, s)) {
            *x0 = txt_log[i].x0;
            *x1 = txt_log[i].x1;
            return 1;
        }
    return 0;
}
static int mrow_has(const char *s) { int32_t a, b; return mrow_at(s, &a, &b); }
static int mrow_only(const char *way, const char *val)
{
    uint32_t i;
    screen_read();
    for (i = 0; i < txt_n; i++) {
        const char *s = txt_log[i].s;
        if (txt_log[i].y1 <= MROW_Y || txt_log[i].y0 >= MROW_Y + MROW_H)
            continue;
        if (strcmp(s, "2") && strcmp(s, "4") && strcmp(s, "ROLL") && strcmp(s, way) && strcmp(s, "AMOUNT") && strcmp(s, val))
            return 0;
    }
    return 1;
}
static int mrow_bar(const char *val)
{
    int32_t a0, a1, v0, v1, x, y, ink = 0;
    if (!mrow_at("AMOUNT", &a0, &a1) || !mrow_at(val, &v0, &v1))
        return 0;
    for (y = MROW_Y; y < MROW_Y + MROW_H; y++)
        for (x = a1 + 3; x < v0 - 2; x++)
            ink += host_screen[(uint32_t)y * 240u + (uint32_t)x] != host_screen[(uint32_t)MROW_Y * 240u + 239u];
    return ink >= 8;
}
static int no_k3(void)
{
    uint32_t i;
    screen_read();
    for (i = 0; i < txt_n; i++)
        if (txt_log[i].y0 >= UT_A_K1_Y && txt_log[i].y1 <= Y_FOOT && (!strcmp(txt_log[i].s, "3") || !strcmp(txt_log[i].s, "UNDO")))
            return 0;
    return 1;
}
/* TIDE: TIDE SETTINGS is in MORPH SET's place, a list (tide_ui.c): K1 the row, OCT+ the next value, OCT- the one before; its KNOBS row picks the HOME page 1 knob set (b9; ROLL TYPE is gone); ARP and PITCH have
 * no row since 1.0.0-b7 (MR8): their switches are set as a learned CC sets them (mset_cfg) */
#define MORPH_SET_TITLE "TIDE SETTINGS"
static void mset_row(uint32_t r) { turn(EN_K1, -8); if (r) turn(EN_K1, (int32_t)r); }
static void mset_put(uint32_t r, uint32_t v)            /* row r to its value v: OCT- to the first, then OCT+ v times */
{
    uint32_t k;
    mset_row(r);
    for (k = 0; k < 4u; k++)
        press(B_OCTDN);
    for (k = 0; k < v; k++)
        press(B_OCTUP);
    frame();
}
static void mset_cfg(uint32_t kind, uint32_t v) { morph_act(kind, morph_mask(song.sel), v, song.sel); frame(); }   /* (MQ_ARP / MQ_PITCH: the morph's own switch) */
/* the letter in part pi's header capsule (A or B; 0 none): the texts drawn in its cell (ui_draw.c head_capsule) */
static char cap_letter(uint32_t pi)
{
    uint32_t i;
    screen_read();
    for (i = 0; i < txt_n; i++)
        if (txt_log[i].y1 <= 22 && (pi ? txt_log[i].x0 >= 200 : txt_log[i].x1 <= 40) && (!strcmp(txt_log[i].s, "A") || !strcmp(txt_log[i].s, "B")))
            return txt_log[i].s[0];
    return 0;
}
/* End B as a patch record keeps it (PT_VER 4: the values, each lossless inside its range: morph.c wv_code), a against b on engine eng */
static int endb_same(uint32_t eng, const int8_t *a, const int8_t *b)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        if (i != P_LEVEL && wv_code(eng, i, a[i]) != wv_code(eng, i, b[i]))
            return 0;
    return 1;
}
static void fresh(void)
{
    ui_power_on();
    frame();                                            /* (morph_settings: the defaults read) */
}
/* 1.0.0-b7: the MORPH page's knobs 2..4 are FILTER, ECHO, LENGTH (b8: FILTER, RESONANCE, ECHO); ROLL is gone from it (a new End B is OCT+ held + K1, End A has no page gesture) and AMOUNT
 * is a TIDE SETTINGS row. The older checks below still turn K2 for a ROLL and K4 for AMOUNT on the MORPH page: on it they reach those through what the page
 * offers now (t_home1 checks the page's own knobs): K2 left a ROLL of A (MIDI's ROLL A, the same action), right OCT+ held + K1; K4 AMOUNT as the row does it,
 * from the selected part's value onto the selection; K3 nothing (as before) */
static void turn_ui(uint32_t role, int32_t s) { turn(role, s); }
static void turn_old(uint32_t role, int32_t s)
{
    if (!s || ui.layer || ui.perf || cur_page()->graph != GR_MORPH || (role != EN_K2 && role != EN_K3 && role != EN_K4)) {
        turn_ui(role, s);
        return;
    }
    if (role == EN_K2) {
        if (s > 0) {
            btn_down(B_OCTUP); frame();
            turn_ui(EN_K1, 1);
            btn_up(B_OCTUP); frame();
        } else {
            morph_act(MQ_ROLL, morph_mask(song.sel), 0u, song.sel);
            frame();
        }
    } else if (role == EN_K4) {
        morph_act(MQ_AMOUNT, morph_mask(song.sel), (uint32_t)clamp((int32_t)morph_amount_of(song.sel) + s, 0, 100), song.sel);
        frame();
    } else
        frame();
    host_ticks += 200000u;
}
#define turn turn_old
#define K1F(n) ((n) * 2)                                 /* (1.0.0-b7: two fader steps a detent) */

/* ------------------------------------------------------------------ ROUND --- */
static void t_round(void)
{
    static const char *const T[] = {"MORPH", "TIDE SETTINGS", "AUTO TIDE"};   /* (TIDE: TIDE SETTINGS in MORPH SET's place, then AUTO TIDE; MODE is rows of TIDE SETTINGS, owner 2026-11-08) */
    const uint32_t NT = sizeof T / sizeof T[0];
    uint32_t k, ok = 1;
    fresh();
    go_title("AMP ENV");                                 /* (from another page HOME opens MORPH) */
    for (k = 0; k < NT; k++) {
        press(B_HOME);
        frames(64);
        ok &= str_eq(PAGES[ui.page].title, T[k]);
    }
    press(B_HOME);
    frames(64);
    expect("HOME tapped: MORPH, MORPH SET (TIDE: TIDE SETTINGS, AUTO TIDE), MODE (plain DUELL), then MORPH again (0.2.14: no OVERVIEW; 0.2.25: no MIXER)", ok && str_eq(PAGES[ui.page].title, "MORPH"));
    press(B_HOME);
    frames(64);
    expect("  .. and MORPH SET; from another page HOME opens MORPH", str_eq(PAGES[ui.page].title, T[1]));
    go_title("AMP ENV");
    press(B_HOME);
    frames(64);
    ok = str_eq(PAGES[ui.page].title, "MORPH");
    for (k = 1; k <= NT; k++) {
        turn(EN_SELECT, 1);
        ok &= where() == page_of(T[k % NT]);
    }
    expect("SELECT right: the same round (then MORPH)", ok);
    turn(EN_SELECT, -1);
    expect("SELECT left from MORPH: the round's last page (the round backwards)", where() == page_of(T[NT - 1u]));
    morph_page();
    {
        char nm[20], ct[12];
        page_parts(nm, ct);
        expect("footer title: MORPH 1/3", str_eq(nm, "MORPH") && str_eq(ct, "1/3"));
    }
}

/* ---------------------------------------------------------------- EDITABLE B (cloud job G) --- */
static uint32_t ink_cols(int32_t x0, int32_t x1)       /* header columns x0..x1 with anything not BG */
{
    uint32_t n = 0;
    int32_t x, y;
    for (x = x0; x <= x1; x++)
        for (y = 2; y < 22; y++)
            if (swap16(host_screen[(uint32_t)y * 240u + (uint32_t)x]) != T_BG) { n++; break; }
    return n;
}
static uint32_t knob_of(uint32_t id)                    /* the page that has id on one of its four cards: the page is opened, the knob (0..3) returned; 9: none */
{
    uint32_t i, k;
    for (i = 0; i < NPAGES; i++)
        for (k = 0; k < 4u; k++)
            if (PAGES[i].id[k] == id && PAGES[i].scope == SC_TRACK && PAGES[i].graph != GR_BROWSE) {
                ui.menu = 0;
                go_title(PAGES[i].title);
                frames(32);
                return k;
            }
    return 9u;
}
static void live_b(void)                                /* a morph that moves things: B rolled at AMOUNT 40, the fader at the B end */
{
    morph_set_amount(40);
    morph_roll(1);
    morph_set_fader(127);
    frames(32);
}
static void t_editb(void)
{
    int16_t pa[EXT_P_LEN];
    xb_edit_t ed[XB_MAX];
    int8_t b0[EXT_P_LEN];
    uint32_t i, k, ok, n, sl, pos, total;
    char want[24], un[40];
    fresh();
    live_b();
    memcpy(pa, trk[0].p, sizeof pa);
    morph_set_fader(126);
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 3);
    expect("EDITABLE B: the fader at 126: a knob turn edits A (the stored ATK moves), nothing is marked", k < 4u && trk[0].p[P_ATK] != pa[P_ATK] && morph_edits(0, ed) == 0u);
    trk[0].p[P_ATK] = pa[P_ATK];
    morph_set_fader(127);
    frames(32);
    memcpy(pa, trk[0].p, sizeof pa);
    turn(EN_K1 + k, 3);
    frame();
    n = morph_edits(0, ed);
    expect("  at 127 (a live morph): the same turn sets End B's ATK, A is untouched (every stored value)", n == 1u && ed[0].wid == P_ATK && !memcmp(pa, trk[0].p, sizeof pa) && morph_view(0, P_ATK) == ed[0].val);
    {
        const char *unit;
        param_format(&TP[P_ATK], (int16_t)morph_view(0, P_ATK), want, &unit);
    }
    snprintf(un, sizeof un, "ATTACK|%s", want);
    expect("  the card shows B's value", card_match(k, un));
    expect("  the header letter is B in B's colour and the page is a sound page", head_ab_on() && head_ab_b() == 1u);
    {   /* DUELL 1.1 header (ui_draw.c head_capsule, item 82): each part's capsule [number | letter | voices] holds the end its knobs edit, A or B, on every part page */
        expect("  .. part 1's capsule reads B", cap_letter(0) == 'B');
        morph_set_fader(0);
        frames(32);
        expect("  .. A below the B end", cap_letter(0) == 'A' && head_ab_b() == 0u);
        morph_set_fader(127);
        frames(32);
        morph_page();
        expect("  .. the capsule's letter on MORPH too (1.1: no separate cushion)", cap_letter(0) == 'B');
        go_title(MORPH_SET_TITLE);
        expect("  .. and on MORPH SET", cap_letter(0) == 'B');
        morph_set_fader(0);
        frames(32);
        go_title("MIXER");
        ui.force = 1; draw_head(); ui.force = 0;
        expect("  .. nor on the global pages (GLO)", !head_ab_on());
        morph_set_fader(127);
    }
    k = knob_of(P_ATK);
    ed[0].val = 0;
    turn(EN_K1 + k, -2);
    n = morph_edits(0, ed);
    expect("  turning the same value again changes it, never counts again; A still untouched", n == 1u && !memcmp(pa, trk[0].p, sizeof pa));
    /* values both ends share */
    k = 0u; ui.menu = 0; go_title("VOICE 1/2"); frames(32);   /* (VOICE 1/2 K1: the CHORD page has the VOICE too, as its K4, shown only where chords need POLY) */
    ui.msg_t = 0;
    memcpy(pa, trk[0].p, sizeof pa);
    turn(EN_K1 + k, 1);
    expect("a value both ends share (VOICE MODE): the knob works, A changes, nothing is marked, the line says SAME ON A AND B", k < 4u && trk[0].p[P_VOICE] != pa[P_VOICE] && morph_edits(0, ed) == 1u && msg_is("SAME ON A AND B"));
    ui.msg_t = 0;
    turn(EN_K1 + k, 1);
    expect("  .. once: the next turn of it says nothing", !ui.msg_t);
    /* the 33rd value */
    {
        uint32_t w, nk = 0;
        morph_set_fader(0);
        morph_set_fader(127);
        for (w = 0; w < EXT_P_LEN && morph_edits(0, ed) < XB_MAX; w++)
            if (morph_b_mode(0, w) == 1u && w != P_SUS && w != P_ATK)
                morph_b_set(0, w, mph[0].base[w]);
        nk = knob_of(P_SUS);
        ui.msg_t = 0;
        memcpy(b0, endp(0, 1), P_COUNT);
        turn(EN_K1 + nk, 4);
        ok = morph_edits(0, ed) == XB_MAX && pin_get(morph_pins(&mph[0]), P_SUS) == 0u && !memcmp(b0, endp(0, 1), P_COUNT) && msg_is("B FULL: 32 VALUES");
        expect("the 33rd value: the knob does not move (End B and A as they were), the line says B FULL: 32 VALUES", ok);
        k = knob_of(P_ATK);
        turn(EN_K1 + k, 2);
        expect("  .. a value already set still turns", memcmp(b0, endp(0, 1), P_COUNT) != 0 && morph_edits(0, ed) == XB_MAX);
    }
    /* automation: not recorded at the B end */
    fresh();
    live_b();
    k = knob_of(P_DEC);
    song.rec = 1u;
    song.playing = 1;
    ui.msg_t = 0;
    n = motion.count;
    turn(EN_K1 + k, 3);
    ok = motion.count == n && msg_is("B: NOT RECORDED") && morph_edits(0, ed) == 1u;
    ui.msg_t = 0;
    turn(EN_K1 + k, 1);
    expect("recording armed: a knob turn at the B end is not recorded as automation, the line says so once", ok && motion.count == n && !ui.msg_t);
    song.rec = 0;
    song.playing = 0;
    /* OCT- reset at the B end */
    fresh();
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 6);
    memcpy(pa, trk[0].p, sizeof pa);
    ok = knob_reset(k) == 1 && morph_view(0, P_ATK) == TP[P_ATK].def && !memcmp(pa, trk[0].p, sizeof pa) && morph_edits(0, ed) == 1u;
    expect("OCT- reset at the B end: End B's value goes to the default, A is untouched", ok);
    /* a patch: edit A, edit B, save, load */
    fresh();
    pos = preset_pos(&total);
    preset_go(pos);
    frames(32);
    sl = slot_of(&trk[0]);
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 5);
    k = knob_of(P_SUS);
    turn(EN_K1 + k, -4);
    morph_set_fader(0);
    frames(32);
    trk[0].p[P_REL] = (int16_t)(trk[0].p[P_REL] > 60 ? 10 : 90);   /* edit A afterwards */
    hold(B_SAVE);
    press(B_OCTUP);
    n = morph_edits(0, ed);
    {   /* (1.1: the record keeps End B as the B end sounds it, morph_effective_x at 127: A edited after B moves what B sounds) */
        int16_t bx[EXT_P_LEN];
        uint32_t i;
        morph_effective_x(0, 1u, bx);
        for (i = 0; i < P_COUNT; i++)
            b0[i] = (int8_t)clamp(bx[i], -128, 127);
    }
    memcpy(pa, trk[0].p, sizeof pa);
    ok = up_used(sl) && !strncmp(ui.msg, "SAVED", 5) && n == 2u && (pt_raw(sl)[4] & PTF_MB);   /* (PT_VER 4: End B and its marks in the record's MBV block) */
    turn(EN_PRESET, -1);                                /* (back to the factory sound before it: the place after a saved sound is an empty USER slot, which loads nothing) */
    frames(32);
    expect("(control) another sound: no hand-set values", morph_edits(0, ed) == 0u);
    turn(EN_PRESET, 1);                                 /* (on again: the saved slot) */
    frames(32);
    expect("a patch saved with values set by hand: loading it gives A as saved, End B exactly as saved and the same two values, the fader at A",
           ok && trk[0].user == sl + 1u && mph[0].fader == 0u && !memcmp(pa, trk[0].p, sizeof pa) && endb_same(mph[0].eng, b0, endp(0, 1)) && morph_edits(0, ed) == 2u &&
           pin_get(morph_pins(&mph[0]), P_ATK) && pin_get(morph_pins(&mph[0]), P_SUS));
    /* b7 (review C2): another sound, then the load UNDO: the USER patch back with its own End B and the two values; AUTO TIDE's HOME the same */
    turn(EN_PRESET, -1);
    frames(32);
    undo_swap();
    frames(32);
    expect("the load UNDO back to the saved USER patch: End B exactly as saved and its two values (not a fresh roll)",
           trk[0].user == sl + 1u && endb_same(mph[0].eng, b0, endp(0, 1)) && morph_edits(0, ed) == 2u &&
           pin_get(morph_pins(&mph[0]), P_ATK) && pin_get(morph_pins(&mph[0]), P_SUS));
    tide_home(1u);
    frames(32);
    expect("AUTO TIDE's HOME on the saved USER patch: End B exactly as saved and its two values",
           trk[0].user == sl + 1u && endb_same(mph[0].eng, b0, endp(0, 1)) && morph_edits(0, ed) == 2u);
    /* undo of loads (SAVE held), both orders */
    fresh();
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 4);                                 /* a B edit on sound 1 */
    memcpy(pa, trk[0].p, sizeof pa);
    turn(EN_PRESET, 1);
    frames(32);                                         /* sound 2: the undo copy is sound 1 */
    morph_set_fader(127);
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 4);                                 /* a B edit on sound 2 */
    ok = morph_edits(0, ed) == 1u;
    undo_swap();
    frames(32);
    ok &= trk[0].p[P_ATK] == pa[P_ATK] && !memcmp(pa, trk[0].p, P_COUNT * sizeof pa[0]) && morph_edits(0, ed) == 0u;
    expect("undo of a load after a B edit on the sound loaded: sound 1 comes back exactly (A as it was, nothing of B in it), no stale mark", ok);
    undo_swap();
    frames(32);
    ok = morph_edits(0, ed) == 0u && trk[0].user == 0u;
    expect("  .. and redo: sound 2 (its A as loaded; the B edits belonged to the sound that was undone)", ok);
    fresh();
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 4);
    n = morph_view(0, P_ATK);
    memcpy(pa, trk[0].p, sizeof pa);
    morph_set_fader(0);
    turn(EN_PRESET, 1);
    frames(32);
    undo_swap();
    frames(32);
    expect("undo of a load that came after a B edit: the sound as it was, A untouched by the B edit, the morph re-based (no mark of the other sound)",
           !memcmp(pa, trk[0].p, P_COUNT * sizeof pa[0]) && morph_edits(0, ed) == 0u && trk[0].p[P_ATK] == pa[P_ATK] && (uint32_t)morph_view(0, P_ATK) != n + 1000u);
}

/* ------------------------------------------------------------------ KNOBS --- */
static void t_knobs(void)
{
    uint32_t a0, a1, s;
    char amt_txt[8];
    fresh();
    morph_page();
    turn(EN_K1, 10);
    expect("KNOB 1: the selected part's fader (10; TIDE 20: two steps a detent), the other part's stays (SELECTED)", mph[0].fader == K1F(10u) && mph[1].fader == 0u);
    turn(EN_K1, -3);
    expect("KNOB 1 left: the fader back (7; TIDE 14)", mph[0].fader == K1F(7u));
    turn(EN_K1, -100);
    expect("KNOB 1 clamps at 0 (A)", mph[0].fader == 0u);
    frame();
    expect("the MORPH card at 0 shows A", card_is(0, "MORPH", "A"));
    turn(EN_K1, 200);
    frame();
    expect("KNOB 1 clamps at 127 (B), the card shows B", mph[0].fader == 127u && card_is(0, "MORPH", "B"));
    turn(EN_K1, -127);
    a0 = mph[0].amount;
    a1 = mph[1].amount;
    turn(EN_K4, 5);
    snprintf(amt_txt, sizeof amt_txt, "%u", (unsigned)(a0 + 5u));
    expect("KNOB 4: AMOUNT of the selected part (+5), the other part's stays", mph[0].amount == a0 + 5u && mph[1].amount == a1
           );
    frame();
    {   /* b6: MORPH on the knob-1 line; KNOB 2 and 4 on the slim row: ROLL A<>B, AMOUNT with its value and no bar; nothing for KNOB 3 */
        char pc[12];
        snprintf(pc, sizeof pc, "%s%%", amt_txt);
        (void)pc;
        expect("  .. 1.0.0-b7: MORPH A on the knob-1 line, then the row: FLT, RSN, ECH; no ROLL, no AMOUNT (a TIDE SETTINGS row now)",
               card_is(0, "MORPH", "A") && mrow_has("FLT") && mrow_has("RSN") && mrow_has("ECH") && !mrow_has("ROLL") && !mrow_has("AMOUNT"));
    }
    {   /* the picture (TIDE: the part's real sound, ui_scope.c): a new one every MV_MS by fm1_ms (b6: mv_clock), not before; none with MENU > ANIM OFF */
        uint32_t s0, s1, s2;
        fm1_ms += MV_MS; mv_clock();
        s0 = graph_signature();
        fm1_ms += MV_MS - 1u; mv_clock();
        s1 = graph_signature();
        fm1_ms += 1u; mv_clock();
        s2 = graph_signature();
        ui_prefs |= PREF_ANIM_OFF;
        expect("the MORPH panel animates (a new signature MV_MS after the last, none sooner); ANIM OFF: still",
               s0 == s1 && s1 != s2 && (fm1_ms += MV_MS, mv_clock(), s1 = graph_signature(), fm1_ms += MV_MS, mv_clock(), s1 == graph_signature()));
        ui_prefs &= ~PREF_ANIM_OFF;
    }
    turn(EN_K4, 200);
    expect("KNOB 4 clamps at 100 %", mph[0].amount == 100u);
    turn(EN_K4, -60);
    settle();
    turn(EN_K2, -MORPH_DET);
    s = seed_now(0, 0);
    expect("KNOB 2 left past the detent: ROLL A (end A rolled, a seed shown, B untouched: it is the load's fresh roll)",
           rolls(0, 0) == 2u && s && s == morph.seed && rolls(0, 1) == 2u && !end_is_base(0, 0) && msg_is("ROLLED A"));
    expect("  .. the other part untouched (SELECTED)", rolls(1, 0) == 1u && rolls(1, 1) == 2u);
    settle();
    turn(EN_K2, MORPH_DET);
    expect("KNOB 2 right past the detent: ROLL B", rolls(0, 1) == 3u && rolls(0, 0) == 2u && msg_is("ROLLED B"));
    settle();
    turn(EN_K2, MORPH_DET);
    expect("ROLL B again: End B replaced in place (no history: one entry, MORPH_HIST 1)", rolls(0, 1) == 4u && mph[0].n[1] == 1u && MORPH_HIST == 1u);
    settle();
    {
        uint32_t sa = seed_now(0, 0);
        s = seed_now(0, 1);
        ui.msg[0] = 0;
        turn(EN_K3, MORPH_DET); settle();
        turn(EN_K3, -MORPH_DET);
        expect("KNOB 3 either way: nothing (1.0.0-b5: no UNDO), both ends as they were, no message", rolls(0, 1) == 4u && rolls(0, 0) == 2u &&
               seed_now(0, 1) == s && seed_now(0, 0) == sa && !msg_is("ROLLED A") && !msg_is("ROLLED B"));
    }
    /* TIDE: TIDE SETTINGS, the list in MORPH SET's place: ROLL SMART -> WILD, ARP ON, PITCH ON (the same settings bits); K2..K4 turn the picked row (C12); ALGORITHM is the
     * morph's selector there (PART 1, BOTH, PART 2: tide_ui.c) */
    go_title(MORPH_SET_TITLE);
    frames(32);
    roll_type_put(2u); mset_cfg(MQ_ARP, 1u); mset_cfg(MQ_PITCH, 1u);
    expect("ROLL WILD on TIDE SETTINGS (TIDE's list: K1 the row, OCT+ the value), ARP ON, PITCH ON (no rows: a learned CC); the settings bits",
           morph.cfg == (MC_WILD | MC_ARP | MC_PIT) && favorites.factory[15][8] == 7u);
    {
        uint32_t c = morph.cfg, t = SEL_BOTH;
        jl_sel = JL_REC;                                 /* (audit C12: K2..K4 turn the picked row's value, here one that is not the morph's) */
        turn(EN_K2, 1); turn(EN_K3, -1); turn(EN_K4, 1); turn(EN_K4, -1);
        frame();
        expect("  .. KNOB 2..4: the picked row's value, not the morph's (no card on the list)", morph.cfg == c && SEL_BOTH == t);
    }
    turn(EN_ALGO, 1);
    expect("  .. ALGORITHM on TIDE SETTINGS: BOTH next (TIDE: the morph's selector, as on MORPH)", song.sel == 0u && SEL_BOTH);
    turn(EN_ALGO, -1);
    roll_type_put(0u); mset_cfg(MQ_ARP, 0u); mset_cfg(MQ_PITCH, 0u);
    expect("  .. and back: MUSIC (TIDE's default: SMART with the musical dials), ARP OFF, PITCH OFF (the defaults)", !morph.cfg && !favorites.factory[15][8]);
}

/* ---------------------------------------------------------------- GESTURE --- */
static void t_gesture(void)
{
    uint32_t i;
    fresh();
    morph_page();
    turn(EN_K3, -2);
    turn(EN_K3, 2);
    expect("KNOB 3 (no job since 1.0.0-b5): no roll of either end", rolls(0, 1) == 2u && rolls(0, 0) == 1u);
    settle();
    turn_ui(EN_K2, -1);
    turn_ui(EN_K2, 1);
    expect("1.0.0-b7: KNOB 2 is FILTER: no roll of either end", rolls(0, 1) == 2u && rolls(0, 0) == 1u);
    (void)i;
}

/* ---------------------------------------------------------------- KNOB 3 --- */
static void t_far(void)                                  /* (1.0.0-b5: UNDO's checks, the card and the far end, are gone with it: KNOB 3 is an empty card at any fader) */
{
    uint32_t sa, sb;
    fresh();
    morph_page();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    turn(EN_K1, 63);
    frame();
    frame();
    expect("1.0.0-b7: the slim row shows RSN for KNOB 3 and ECH for KNOB 4, no ROLL", mrow_has("RSN") && mrow_has("ECH") && !mrow_has("ROLL"));
    settle();
    sa = seed_now(0, 0);
    sb = seed_now(0, 1);
    turn(EN_K1, 100);
    turn(EN_K3, -MORPH_DET); settle();
    turn(EN_K1, -100);
    turn(EN_K3, MORPH_DET); settle();
    expect("fader 100 and fader 0: a twist of KNOB 3 either way changes neither end", seed_now(0, 0) == sa && seed_now(0, 1) == sb && rolls(0, 0) == 2u && rolls(0, 1) == 3u);
}

/* ------------------------------------------------------- SELECTED / BOTH --- */
static void t_target(void)
{
    fresh();
    morph_page();
    turn(EN_ALGO, 1);
    expect("ALGORITHM on MORPH (0.2.9: PART 1, BOTH, PART 2): right is BOTH, both badges, part 1 still selected",
           SEL_BOTH && song.sel == 0u && badge_lit() == 3u && msg_is(ALGO_BOTH_SAY));
    algo_turns(ALGO_TO_P2);                             /* */
    expect("ALGORITHM: part 2 selected", song.sel == 1u && !SEL_BOTH && badge_lit() == 2u);
    turn(EN_K1, 20);
    expect("SELECTED: KNOB 1 now moves part 2's fader only", mph[1].fader == K1F(20u) && mph[0].fader == 0u);
    turn(EN_K2, -MORPH_DET);
    expect("SELECTED: ROLL A rolls part 2 only", rolls(1, 0) == 2u && rolls(0, 0) == 1u);
    settle();
    go_title("AMP ENV"); frames(32);
    turn(EN_ALGO, -1);                                  /* (another page: PART 1 / PART 2 only) */
    morph_page();
    expect("ALGORITHM on AMP ENV: part 1, no BOTH", song.sel == 0u && !SEL_BOTH);
    turn(EN_K2, MORPH_DET);
    expect("ALGORITHM back to part 1: ROLL B rolls part 1 only", rolls(0, 1) == 3u && rolls(1, 1) == 2u);
    settle();
    turn(EN_ALGO, 1);                                   /* BOTH */
    turn(EN_K2, -MORPH_DET);
    expect("BOTH: ROLL A rolls both parts, the same seed shown", rolls(0, 0) == 2u && rolls(1, 0) == 3u &&
           seed_now(0, 0) == seed_now(1, 0) && seed_now(0, 0) == morph.seed);
    expect("  .. each part from its own Original (the ends differ)", memcmp(mph[0].h[0][mph[0].top[0]], mph[1].h[0][mph[1].top[0]], P_COUNT) != 0);
    settle();
    {   /* 1.0.0-b7: each part from its own fader by the same steps */
        uint32_t g0 = mph[0].fader, g1 = mph[1].fader;
        turn(EN_K1, 10);
        expect("BOTH: KNOB 1 moves each fader from where it is by the same steps (1.0.0-b7)", g0 != g1 && mph[0].fader == g0 + 20u && mph[1].fader == g1 + 20u);
    }
    {
        uint32_t b0 = mph[0].amount;
        turn(EN_K4, -5);
        expect("BOTH: KNOB 4 sets both parts' AMOUNT", mph[0].amount == b0 - 5u && mph[1].amount == b0 - 5u);
    }
    go_title(MORPH_SET_TITLE); frames(32);
    expect("BOTH, then MORPH SET: one badge (the selected part; TIDE's TIDE SETTINGS: both, as MORPH), BOTH kept", SEL_BOTH && badge_lit() == 3u);
    morph_page();
    expect("  .. back on MORPH: both badges again", SEL_BOTH && badge_lit() == 3u);
}

/* -------------------------------------------------------------------- MIDI --- */
static void cc(uint32_t ch, uint32_t n, uint32_t v)   /* a USB-MIDI CC packet, as the USB IN endpoint queues it */
{
    midi_in_event(0x0Bu | (0xB0u | ch) << 8 | n << 16 | v << 24);
}
static void run(void) { events_block(CTL); frame(); }  /* the audio ISR's events, then the main loop */
static void t_midi(void)
{
    uint32_t s, ok;
    fresh();
    song.g[G_ROUTE] = 0;                                 /* (ROUT CH1-4) */
    cc(0, 20, 100); cc(1, 20, 30); run();
    expect("CC 20 ch 1 -> part 1's fader, ch 2 -> part 2's (SELECTED)", mph[0].fader == 100u && mph[1].fader == 30u);
    song.sel = 1;
    cc(0, 20, 5); run();
    expect("CC 20 ch 1 with part 2 selected: still part 1 (the channel's part, not the selection)", mph[0].fader == 5u &&
           mph[1].fader == 30u);
    song.sel = 0;
    cc(2, 20, 77); cc(3, 20, 77); run();
    expect("CC 20 on ch 3 / ch 4 (hidden parts): ignored", mph[0].fader == 5u && mph[1].fader == 30u);
    cc(0, 25, 127); cc(1, 25, 0); run();
    expect("CC 25 AMOUNT: ch 1 127 -> 100 %, ch 2 0 -> 0 %", mph[0].amount == 100u && mph[1].amount == 0u);
    cc(0, 25, 64); run();
    expect("CC 25 64 -> 50 %", mph[0].amount == 50u);
    cc(0, 21, 127); run();
    s = seed_now(0, 0);
    expect("CC 21 ch 1 >= 64: ROLL A on part 1 (ROLLED A)", rolls(0, 0) == 2u && s && rolls(1, 0) == 1u && msg_is("ROLLED A"));
    cc(0, 21, 100); cc(0, 21, 127); run();
    expect("CC 21 held up (100, 127 without a release): no second roll", rolls(0, 0) == 2u);
    cc(0, 21, 0); cc(0, 21, 64); run();
    expect("CC 21 released below 64, then 64: rolls again", rolls(0, 0) == 3u);
    cc(1, 22, 127); cc(1, 22, 0); run();
    expect("CC 22 ch 2: ROLL B on part 2 only", rolls(1, 1) == 3u && rolls(0, 1) == 2u);
    s = seed_now(1, 1);
    cc(1, 23, 127); cc(1, 23, 0); cc(1, 23, 30); run();
    expect("CC 23 ch 2: unassigned since 1.0.0-b5 (UNDO's), ignored", rolls(1, 1) == 3u && seed_now(1, 1) == s && rolls(1, 0) == 1u);
    cc(1, 20, 0); run();
    cc(1, 24, 127); run();
    expect("CC 24 ch 2: unassigned, ignored (no KEEP any more)", rolls(1, 1) == 3u && !msg_is("KEPT >B"));
    cc(1, 24, 10); run();
    cc(1, 111, 127); run();
#if TIDE_PLAYSEL
    expect("CC 111 86..127: TARGET BOTH (1.0.0-b7: named as the play mode it lands on)", SEL_BOTH && msg_is(PLS_NAME[play_pos()]) && play_pos() != PLS_P1 && play_pos() != PLS_P2);
#else
    expect("CC 111 86..127: TARGET BOTH", SEL_BOTH && msg_is("BOTH"));
#endif
    cc(0, 20, 90); run();
    expect("BOTH: CC 20 on ch 1 moves both faders", mph[0].fader == 90u && mph[1].fader == 90u);
    cc(1, 20, 12); run();
    expect("BOTH: CC 20 on ch 2 moves both faders", mph[0].fader == 12u && mph[1].fader == 12u);
    cc(1, 21, 0); cc(1, 21, 127); run();
    expect("BOTH: CC 21 on ch 2 rolls A on both parts", rolls(0, 0) == 4u && rolls(1, 0) == 2u && seed_now(0, 0) == seed_now(1, 0));
    cc(0, 25, 127); run();
    expect("BOTH: CC 25 sets both parts' AMOUNT", mph[0].amount == 100u && mph[1].amount == 100u);
    cc(0, 111, 63); run();
    ok = !SEL_BOTH && song.sel == 1u && msg_is("PART 2");
    cc(0, 111, 42); run();
    expect("CC 111 43..85: PART 2 (selected); 0..42: PART 1", ok && !SEL_BOTH && song.sel == 0u &&
           msg_is("PART 1"));
    cc(0, 113, 0); cc(0, 114, 100); cc(0, 115, 64); run();
    expect("CC 113 SMART (0 = WILD), CC 114 ARP, CC 115 PITCH (>= 64 on)", morph.cfg == (MC_WILD | MC_ARP | MC_PIT));
    cc(0, 113, 127); cc(0, 114, 0); cc(0, 115, 63); run();
    expect("  .. and back", !morph.cfg);
    {   /* TRS MIDI IN: the same path (midi_uart.c um_byte), running status */
        static const uint8_t B[] = {0xB1, 20, 44, 25, 127};
        uint32_t i;
        for (i = 0; i < sizeof B; i++)
            um_byte(B[i]);
        run();
        expect("TRS: CC 20 ch 2 (44) and, running status, CC 25 (127) -> part 2", mph[1].fader == 44u && mph[1].amount == 100u &&
               mph[0].fader == 12u);
    }
    song.g[G_ROUTE] = 1;                                 /* ROUT SEL: every channel the selected part */
    song.sel = 1;
    cc(6, 20, 101); run();
    expect("ROUT SEL: CC 20 on ch 7 -> the selected part (2)", mph[1].fader == 101u && mph[0].fader == 12u);
    song.g[G_ROUTE] = 0;
    song.sel = 0;
    cc(0, 121, 0); cc(0, 21, 127); run();
    expect("CC 121 re-arms the triggers (CC 21 held at 127 before it: a roll now)", rolls(0, 0) == 5u);
    cc(0, 21, 0); run();
    {   /* MIDI LEARN: a CC learned is the user's (as for the standard map): CC 20 learned to part 1's LEVEL */
        ml_learn_f(20, 0, P_LEVEL, 1, 0);               /* (CC 20 is the fader's: the learn takes it only forced now) */
        cc(0, 20, 0); run();
        expect("a learned CC 20 sets what it was learned to, not the fader", trk[0].p[P_LEVEL] == 0 && mph[0].fader == 12u);
        ml_clear_all();                                   /* (the number is the fader's again) */
    }
    {   /* the queue: 16 fader values of one part in a block (0.2.11: the MIDI drain's limit a block, seq.c MIDI_DRAIN) take one slot (the last value wins) */
        uint32_t i;
        for (i = 0; i < 16u; i++)
            cc(0, 20, 24u + i);
        run();
        expect("a CC 20 stream in one block: one queue slot, the last value", mph[0].fader == 39u && !mq_lost);
    }
}

/* ------------------------------------------------------------------ MATRIX --- */
static int32_t blk[2u * CTL];
static void t_matrix(void)
{
    track_t *t = &trk[0];
    uint32_t k, ok = 1, from = 1;
    fresh();
    expect("the matrix's sources: MORPH right after LFO2 (append-only), then DUCK (the sidechain), N_MSRC 'MORPH'",
           MS_MORPH == MS_LFO2 + 1 && MS_N >= MS_MORPH + 1 && str_eq(N_MSRC[MS_MORPH], "MORPH"));
    morph_do(MQ_FADER, 1u, 127u);
    morph_do(MQ_FADER, 2u, 64u);
    mix_block(blk, CTL);                            /* (a block: morph_begin puts each part's fader for the matrix) */
    expect("MORPH source: fader / 127 (127 -> 32766, 64 -> 16512), per part, as the block has it (t->mf)",
           mod_tsrc(&trk[0], MS_MORPH, 0) == 32766 && mod_tsrc(&trk[1], MS_MORPH, 0) == 64 * 258);
    t->p[P_PAN] = 0;
    t->p[P_M1SRC] = MS_MORPH; t->p[P_M1DST] = MD_PAN; t->p[P_M1AMT] = 63;
    mod_begin(t);
    k = (uint32_t)t->p[P_PAN];
    mod_end(t);
    morph_do(MQ_FADER, 1u, 0u);
    mix_block(blk, CTL);
    mx_sl[t - trk].ok = 0;                          /* (0.2.25: a mapping probe: the fader's jump at once, not through the matrix's source slew) */
    mod_begin(t);
    ok = t->p[P_PAN] == 0;
    mod_end(t);
    expect("MORPH -> PAN +63: fader 127 pans fully right (+63), fader 0 not at all", k == 63u && ok && t->p[P_PAN] == 0);
    /* no feedback: rolls never make a route from MORPH, SMART rolls only a MORPH route's AMT (SRC DST kept) */
    t->p[P_M2SRC] = MS_LFO; t->p[P_M2DST] = MD_CUT; t->p[P_M2AMT] = 30;
    t->p[P_M3SRC] = MS_ENV; t->p[P_M3DST] = MD_SHP; t->p[P_M3AMT] = -20;
    for (k = 1, ok = 1; k <= 400u; k++) {
        int8_t o[P_COUNT], end[P_COUNT];
        uint32_t id, j;
        for (id = 0; id < P_COUNT; id++)
            o[id] = (int8_t)clamp(t->p[id], -128, 127);
        morph_roll_pure(o, end, ENGINES[eng_idx(t->eng_req)], 100, k, 0, 0);
        for (j = 0; j < NMSLOT; j++)
            ok &= end[MSLOT_ID(j)] != MS_MORPH || o[MSLOT_ID(j)] == MS_MORPH;
        ok &= end[P_M1SRC] == MS_MORPH && end[P_M1DST] == MD_PAN;   /* (SMART 2026-10-12: its AMT rolls too, SRC DST kept) */
        from &= end[P_M2SRC] == MS_LFO && end[P_M2DST] == MD_CUT && end[P_M3SRC] == MS_ENV && end[P_M3DST] == MD_SHP;   /* SMART (2026-10-12): SRC / DST kept */
        for (j = 3; j < NMSLOT; j++)
            from &= !end[MSLOT_ID(j)] == !o[MSLOT_ID(j)];   /* no route added */
    }
    expect("400 SMART rolls at 100 %: every assigned route keeps SRC / DST, none added; the MORPH route keeps SRC / DST (its AMT rolls)", ok && from);
}

/* ------------------------------------------------------------------- VIEW --- */
/* the parameter pages show the blended value (morph_view) and a mark on the moved cards */
static void t_view(void)
{
    uint32_t j, k, pg = 0, slot = 0, id = 0, ex = 0xFFFFu, i;
    int found = 0, ok;
    int32_t before, vb, pb;
    fresh();
    morph_page();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    for (j = 0; j < mph[0].nid && !found; j++) {         /* a continuous value on a plain page, away from its range's edge */
        const param_desc_t *d;
        id = mph[0].ent[j].id;
        if ((mph[0].ent[j].kind & 0x7Fu) != MK_CONT || id >= P_COUNT)
            continue;
        d = track_desc(&trk[0], id);
        if (trk[0].p[id] <= d->min + 2 || trk[0].p[id] >= d->max - 2)
            continue;
        for (i = 0; i < NPAGES && !found; i++)
            for (k = 0; k < 4u; k++)
                if (PAGES[i].scope == SC_TRACK && PAGES[i].graph != GR_MOD && PAGES[i].id[k] == id && !found) {
                    pg = i; slot = k; found = 1;
                }
    }
    for (i = 2; i < P_COUNT && ex == 0xFFFFu; i++) {      /* an untouched one: no entry at i, i - 1 or i - 2 */
        for (j = 0, k = 0; j < mph[0].nid; j++)
            k |= mph[0].ent[j].id >= i - 2u && mph[0].ent[j].id <= i;
        if (!k)
            ex = i;
    }
    expect("a rolled continuous value on a plain page found", found);
    if (!found)
        return;
    mph[0].fader = 127;
    ok = morph_view(0, id) == endp(0, 1)[id] && (morph_view_moved(0, id) == (endp(0, 1)[id] != trk[0].p[id]));
    mph[0].fader = 0;
    ok &= morph_view(0, id) == endp(0, 0)[id] && (morph_view_moved(0, id) == (endp(0, 0)[id] != trk[0].p[id]));
    mph[0].fader = 127;
    expect("(a) fader 127: the card value is End B's (marked if it differs from the Original); fader 0: End A's", ok);
    ui.menu = 0; go_title(PAGES[pg].title); frames(32);
    mph[0].fader = 126; frames(8);                       /* (cloud job G: at 127 the knob is End B's, below it the Original's) */
    vb = morph_view(0, id); pb = trk[0].p[id];
    turn(EN_K1 + slot, 1); frames(8);
    before = trk[0].p[id] - pb;
    ok = before != 0 && (morph_view(0, id) - vb) - before >= -1 && (morph_view(0, id) - vb) - before <= 1 && trk[0].p[id] == pb + before && !morph_edits(0, (xb_edit_t[XB_MAX]){0});
    expect("(b) below the B end a knob turn edits the Original by one step; the shown value follows it", ok);
    mph[0].fader = 127; frames(8);
    vb = morph_view(0, id); pb = trk[0].p[id];
    turn(EN_K1 + slot, 1); frames(8);
    ok = trk[0].p[id] == pb && morph_view(0, id) != vb && morph_view(0, id) == endp(0, 1)[id] && pin_get(morph_pins(&mph[0]), id);
    expect("(b2) at the B end (127) the same turn sets End B's value (the Original is as it was), the shown value is B's", ok);
    if (ex != 0xFFFFu)
        expect("(c) a value the morph never moves: shown as stored, no mark", morph_view(0, ex) == trk[0].p[ex] && !morph_view_moved(0, ex));
}

/* -------------------------------------------------------------------- LOAD --- */
static void t_load(void)
{
    uint32_t am0;
    fresh();
    morph_page();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    turn(EN_K1, 25);                  /* (TIDE: 25 detents, two steps each) */
    expect("rolled: both ends differ from the Original, the fader at 50", !end_is_base(0, 0) && !end_is_base(0, 1) && mph[0].nid &&
           mph[0].fader == 50u);
    am0 = mph[0].amount;
    turn(EN_PRESET, 1);
    frames(32);
    expect("a sound load (PRESETS): the fader back at A, End A the sound, End B a fresh roll (seeded, the AMOUNT), no history",
           mph[0].fader == 0u && end_is_base(0, 0) && !end_is_base(0, 1) && rolls(0, 0) == 1u && rolls(0, 1) == 2u &&
           !seed_now(0, 0) && seed_now(0, 1) && mph[0].nid && mph[0].preset == trk[0].preset);
    expect("TIDE b6: a factory sound load keeps the player's AMOUNT (KNOB 4 shows the AMOUNT it had)", !trk[0].user && mph[0].amount == am0);
    turn(EN_K4, 7);
    expect("  .. turning KNOB 4 changes the AMOUNT", mph[0].amount == am0 + 7u);
    am0 = mph[0].amount;
    turn(EN_PRESET, 1);
    frames(32);
    expect("  .. and the next sound load keeps that one", mph[0].amount == am0);
}

/* TIDE b6 (MORPH c3): a factory multi keeps the player's AMOUNT; the AMOUNT card dims while JOURNEY runs */
static void t_amount_keep(void)
{
    fresh();
    morph_page();
    SEL_BOTH = 1;                                      /* (b5: the one selection, song.both; c3 was written before it, on morph.target) */
    turn(EN_K4, 31);
    settle();
    {
        uint32_t a0 = mph[0].amount, a1 = mph[1].amount;
        project_load(MIDX(0));                               /* (a factory multi: nothing stored in the test's flash) */
        frames(32);
        expect("a factory multi load keeps both parts' AMOUNT", mph[0].amount == a0 && mph[1].amount == a1 && a0 != 15u);
    }
    gl.on = 3u;
    gl.mode = GM_JOURNEY;
    expect("1.0.0-b7: the AMOUNT row is not dim while JOURNEY runs (it still shapes a hand's roll)", !jl_dimmed(JL_AMOUNT));
    gl.on = 0;
}
/* the slim row (owner on the unit, 2026-11-10; b9: every genre set, knob_sets.h): every value each column can show, in every form (the engine and ENV SYNC loops are left from when K4 was LEN),
 * fits its column: the number dot (KN_DW), the name, 5 px, the widest value, then KN_GAP px or more before the next column; the last ends by x 237 */
static int h1_fits(void)
{
    uint32_t set, i, f, v, ok = 1;
    uint8_t keep = tide_knobs;
    for (set = 0; set < KS_N; set++) {
        int32_t w[3] = {0, 0, 0}, x;
        tide_knobs = (uint8_t)(set + 2u);
        for (i = 0; i < 3u; i++) {
            uint32_t ix = KN_E[set][i][0].ix;
            for (f = 0; f < (ix < KN_NP ? 3u : 1u); f++) {
                track_t t = trk[0];
                const param_desc_t *d;
                char b[PV_LEN + 4];
                t.eng_req = f == 1u ? ENGI_FM6 : 0u;
                t.p[P_ESYNC] = (int16_t)(f == 2u);
                d = ix < KN_NP ? track_desc(&t, KN_ID[ix]) : glob_desc(KN_ID[ix]);
                for (v = (uint32_t)(int32_t)d->min; (int32_t)v <= d->max; v++) {
                    h1_val(ix, &t, (int32_t)(int16_t)v, b);
                    w[i] = text_w(&AF_S, b) > w[i] ? text_w(&AF_S, b) : w[i];
                }
            }
        }
        for (i = 0; i < 3u; i++) {
            x = KN_X[set][i] + KN_DW + text_w(&AF_S, KN_L[set][i]) + 5 + w[i];   /* (b11: the knob's number dot first) */
            ok &= x + (i < 2u ? KN_GAP : 0) <= (i < 2u ? KN_X[set][i + 1u] : 237);
            if (set == 0u)
                printf("morphui: row column %s: widest value %d px, ends at x %d\n", KN_L[set][i], w[i], x);
        }
    }
    tide_knobs = keep;
    return (int)ok;
}
/* 1.0.0-b7 (owner 2026-11-10): the MORPH page is the hands-on page */
static void t_home1(void)
{
    expect("the slim row: the widest value of every knob of every set (STD and the six genres) fits its column, 12 px or more apart", h1_fits());
    int32_t o;
    uint32_t f0, s1;
    int16_t c0, c1, c2;
    fresh();
    morph_page();
    SEL_BOTH = 0;
    song.sel = 0;
    morph_do(MQ_FADER, 3u, 0u);
    turn_ui(EN_K1, 10);
    expect("K1: two fader steps a detent (half the twisting)", mph[0].fader == 20u);
    turn_ui(EN_K1, 60);
    expect("  .. B reached exactly", mph[0].fader == 127u);
    turn_ui(EN_K1, -64);
    expect("  .. and A", mph[0].fader == 0u);
    morph_do(MQ_FADER, 1u, 10u);
    morph_do(MQ_FADER, 2u, 50u);
    SEL_BOTH = 1;
    turn_ui(EN_K1, 5);
    expect("K1 with BOTH: each part from its own fader by the same steps", mph[0].fader == 20u && mph[1].fader == 60u);
    morph_do(MQ_FADER, 3u, 0u);
    trk[0].p[P_FREEA] = 40; trk[1].p[P_FREEA] = 60;
    trk[0].p[P_FREEB] = 10; trk[1].p[P_FREEB] = 20;
    trk[0].p[P_DLY] = 30; trk[1].p[P_DLY] = 70;
    trk[0].p[P_REL] = 55; trk[1].p[P_REL] = 66;
    turn_ui(EN_K2, 3);
    turn_ui(EN_K3, 2);
    turn_ui(EN_K4, -4);
    expect("K2 FLT, K3 RSN, K4 ECH with BOTH: each part by the same steps", trk[0].p[P_FREEA] == 43 && trk[1].p[P_FREEA] == 63 &&
           trk[0].p[P_FREEB] == 12 && trk[1].p[P_FREEB] == 22 &&
           trk[0].p[P_DLY] == 26 && trk[1].p[P_DLY] == 66 &&
           trk[0].p[P_REL] == 55 && trk[1].p[P_REL] == 66);
    SEL_BOTH = 0;
    turn_ui(EN_K2, -3);
    expect("  .. PART 1 alone moves part 1 only", trk[0].p[P_FREEA] == 40 && trk[1].p[P_FREEA] == 63);
    mods_put(MODS_SENDS, 1u);
    frame();
    c0 = trk[0].p[P_DLY];
    turn_ui(EN_K4, 5);
    expect("ECHO with SENDS OFF: the knob does nothing, the row dims and says why", trk[0].p[P_DLY] == c0 && h1_echo_why() && str_eq(h1_echo_why(), "ECHO: SENDS OFF"));
    c0 = trk[0].p[P_FREEB];
    turn_ui(EN_K3, 5);
    expect("RSN with SENDS OFF: the knob still works", trk[0].p[P_FREEB] != c0);
    mods_put(MODS_SENDS, 0u);
    frame();
    o = song.octave;
    f0 = mph[0].fader;
    s1 = seed_now(0, 1);
    btn_down(B_OCTUP); frame();
    turn_ui(EN_K1, -1);
    turn_ui(EN_K1, -1);
    btn_up(B_OCTUP); frames(64);
    expect("OCT+ held + K1: End B a new roll, the fader stays, no octave on the release", seed_now(0, 1) != s1 && mph[0].fader == f0 && song.octave == o);
    c0 = h1_orig[0][0];
    c1 = h1_orig[0][1];
    c2 = h1_orig[0][2];
    trk[0].p[P_FREEA] = (int16_t)(c0 > 10 ? c0 - 10 : c0 + 10);
    trk[0].p[P_FREEB] = (int16_t)(c1 > 10 ? c1 - 10 : c1 + 10);
    trk[0].p[P_DLY] = (int16_t)(c2 > 10 ? c2 - 10 : c2 + 10);
    btn_down(B_OCTDN); frame();
    turn_ui(EN_K2, 1);
    turn_ui(EN_K3, 1);
    turn_ui(EN_K4, 1);
    btn_up(B_OCTDN); frames(64);
    expect("OCT- + K2 / K3 / K4: FLT, RSN and ECH back to the loaded sound's, no octave", trk[0].p[P_FREEA] == c0 && trk[0].p[P_FREEB] == c1 && trk[0].p[P_DLY] == c2 && song.octave == o);
    SEL_BOTH = 1;
    frames(400);                                        /* (the knobs at rest after the first OCT- gesture) */
    trk[1].p[P_FREEB] = (int16_t)(h1_orig[1][1] > 10 ? h1_orig[1][1] - 10 : h1_orig[1][1] + 10);
    btn_down(B_OCTDN); frame();
    turn_ui(EN_K3, 1);
    btn_up(B_OCTDN); frames(64);
    expect("OCT- + K3 with BOTH: RSN back on both parts", trk[0].p[P_FREEB] == h1_orig[0][1] && trk[1].p[P_FREEB] == h1_orig[1][1]);
    SEL_BOTH = 0;
    morph_do(MQ_FADER, 1u, 90u);
    frames(400);                                        /* (K1 at rest after the OCT+ roll: its further detents were swallowed for 300 ms) */
    btn_down(B_OCTDN); frame();
    turn_ui(EN_K1, 1);
    btn_up(B_OCTDN); frames(64);
    expect("OCT- + K1: back to the Original (the fader at A)", mph[0].fader == 0u);
    /* (the glide is not held by K2..K4: tide_controls_test.c, where the transport plays) */
    morph_do(MQ_AMOUNT, 1u, 20u);
    morph_do(MQ_AMOUNT, 2u, 50u);
    SEL_BOTH = 1;
    jl_put(JL_AMOUNT, 30u);
    expect("TIDE SETTINGS' AMOUNT row with BOTH: each part by the same steps", mph[0].amount == 30u && mph[1].amount == 60u && str_eq(JL_NAME[JL_AMOUNT], "AMOUNT") && JL_AMOUNT == JL_KNOB + 1u);
    SEL_BOTH = 0;
}

/* ------------------------------------------------------------ item 154: BREADBIN --- */
/* The owner's steps: #15 BREADBIN on part 1, the LFO 1 DEST page, ROLL (AMOUNT 8), back to the page; again at AMOUNT 50 / 60. What the cards show (ui_draw.c card_vv = morph_view) is what the FADER sounds:
 * at A that is End A = the Original, so a roll of B changes nothing on a card until the fader moves; ROLL A (the end the fader is at) shows at once. The extension slots (TWIN ENV 2 / LFO 1 / LFO 2, SUB,
 * CYCLE: the LFO 2 DEST page's TWIN CUTOFF, the TWIN MOD page) were not taken by card_vv (ids >= 128): at B they showed the Original although the sound was End B's. */
static void pump_audio(void)
{
    int32_t o[2u * CTL];
    uint32_t b;
    for (b = 0; b < 300u; b++) {                        /* (the engine switch and the ~50 ms ramp of a roll run in the audio blocks) */
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
    }
}
static void t_item154(void)
{
    static const uint32_t AM[3] = {8, 50, 60};
    uint32_t n, tot, a, k, ok_a = 1, ok_b = 1, ok_x = 1, ok_msg = 1, ok_mark = 1, moved_b = 0;
    int ok_load = 0;
    static const uint32_t XI[3] = {X_ENV2, X_LFO1, X_LFO2};    /* TWIN ENV 2 / LFO 1 / LFO 2 (BREADBIN has TWIN on) */
    static const uint32_t PI[3] = {P_LD_FLT, P_LD_SHP, P_LD_AMP};
    fresh();
    go_title(MORPH_SET_TITLE); frames(32);
    roll_type_put(1u);                                   /* (TIDE: ROLL SMART, 1.1's roll the owner's steps used; MUSIC, TIDE's default, keeps the LFO depths) */
    preset_pos(&tot);
    for (n = 0; n < tot; n++) {
        preset_go(n);
        frames(16);
        pump_audio();
        if (slot_of(&trk[0]) == 14u)
            break;
    }
    ok_load = n < tot && trk[0].eng_req == 6u && trk[0].p[P_TWTYPE] != 0;
    expect("item 154: #15 BREADBIN (TRIO, TWIN on) is on part 1", ok_load);
    go_title("LFO 1 DEST");
    frames(64);
    turn(EN_K2, 17); frames(64); turn(EN_K3, 11); frames(64); turn(EN_K4, 11); frames(64);       /* the owner's page: MULTI CUTOFF 17 %, AMP 11 % */
    expect("  .. the LFO 1 DEST page: MULTI CUTOFF 17 %, AMP 11 % (his numbers)", trk[0].p[P_LD_FLT] == 17 && trk[0].p[P_LD_AMP] == 11);
    for (a = 0; a < 3u; a++) {
        int8_t o[P_COUNT];
        morph_page();
        turn(EN_K1, -127); settle(); pump_audio();      /* the fader at A */
        turn(EN_K4, (int32_t)AM[a] - (int32_t)mph[0].amount); settle(); pump_audio();
        memcpy(o, mph[0].base, P_COUNT);
        turn(EN_K2, MORPH_DET); settle(); pump_audio();    /* ROLL B */
        ok_msg &= !strncmp(ui.msg, "ROLLED B", 8);
        go_title("LFO 1 DEST"); frames(64);
        for (k = 0; k < 3u; k++)                        /* the page after the roll, the fader at A: End A = the Original */
            ok_a &= card_vv(&trk[0].p[PI[k]]) == trk[0].p[PI[k]] && endp(0, 0)[PI[k]] == trk[0].p[PI[k]];
        morph_page();
        turn(EN_K1, 127); settle(); pump_audio();       /* the fader to B */
        go_title("LFO 1 DEST"); frames(64);
        for (k = 0; k < 3u; k++) {
            ok_b &= card_vv(&trk[0].p[PI[k]]) == endp(0, 1)[PI[k]];
            moved_b += card_vv(&trk[0].p[PI[k]]) != trk[0].p[PI[k]];
        }
        for (k = 0; k < 3u; k++)                        /* the extension slots (LFO 2 DEST's TWIN CUTOFF, TWIN MOD's depths): End B's value at B */
            ok_x &= card_vv(&XV(&trk[0], XI[k])) == morph_curx(&mph[0], 1)[XI[k]] && morph_view(0, XW_BASE + XI[k]) == morph_curx(&mph[0], 1)[XI[k]];
        ok_mark &= card_vv(&XV(&trk[0], X_ENV2)) == XV(&trk[0], X_ENV2) || card_mk_next;   /* a moved card carries the morph mark */
    }
    expect("item 154: ROLL B (AMOUNT 8, 50, 60) with the fader at A: the LFO 1 DEST cards still show the Original (End A): nothing to see until the fader moves", ok_a);
    expect("item 154: the fader at B: the cards show the rolled End B, and a rolled value differs from the Original (cards moved: at least 6 of 9)", ok_b && moved_b >= 6u);
    expect("item 154: .. the extension cards too (TWIN ENV 2 / LFO 1 / LFO 2) show End B at B (card_vv takes the wide ids)", ok_x);
    expect("item 154: .. and a card the morph moves carries the morph mark", ok_mark);
    expect("item 154: the roll says ROLLED B on the header (the only sign of it with the fader at A)", ok_msg);
    morph_page();
    turn(EN_K1, -127); settle(); pump_audio();
    turn(EN_K2, -MORPH_DET); settle(); pump_audio();    /* ROLL A with the fader at A */
    go_title("LFO 1 DEST"); frames(64);
    {
        uint32_t seen = 0;
        for (k = 0; k < 3u; k++)
            seen += card_vv(&trk[0].p[PI[k]]) != trk[0].p[PI[k]];
        expect("item 154: ROLL A with the fader at A: the cards change at once (the end the fader is at)", seen >= 2u);
    }
    go_title(MORPH_SET_TITLE); frames(32);
    roll_type_put(0u);                                   /* (back to MUSIC, TIDE's default: the setting outlives a power cycle) */
}

/* ---------------------------------------------------------------- SAVE HELD --- */
/* 0.2.17: ONE list of 128 slots (a slot is its saved record, else its factory sound). Hold SAVE: OVERWRITE #NN, the slot the part's sound came from,
 * K1 picks another; OCT+ writes it, End B and AMOUNT with it. There is no 'new' sound, no shadow copy and no NAME screen. */
static void t_save(void)
{
    int ok_both;
    int8_t b0[P_COUNT];
    int16_t fac[P_COUNT];
    uint32_t sd, am, pos, total, t2, sl, e;
    fresh();
    morph_page();
    press(B_SAVE);
    frames(32);
    expect("a press of SAVE on the MORPH page opens the SAVE pages (PATCHES) as everywhere; nothing saved, no dialog",
           !at_home() && str_eq(cur_page()->title, "PATCHES") && !up_count() && !ui.confirm);
    morph_page();
    pos = preset_pos(&total);
    preset_go(pos);                                     /* (a factory sound, as the browser loads it) */
    frames(32);
    sl = slot_of(&trk[0]);
    e = trk[0].eng_req;
    memcpy(fac, trk[0].p, sizeof fac);
    trk[0].p[P_REL] = (int16_t)(trk[0].p[P_REL] > 60 ? 10 : 90);   /* (the user's edit) */
    turn(EN_K4, 20);
    turn(EN_K2, MORPH_DET); settle();                   /* ROLL B at AMOUNT 45 */
    turn(EN_K1, 70);
    expect("a factory sound is in its slot (the browse position), the list is the 128 slots and the 4 displaced places", sl == pos && total == UP_VSLOTS && sl < UP_SLOTS);
    hold(B_SAVE);
    expect("SAVE held on MORPH: the OVERWRITE dialog for part 1, offering the slot the sound came from (nothing saved yet); OCT- cancels",
           ui.confirm == CF_SAVE_OVER && ui.confirm_trk == 0u && ui.svs == sl && !up_count() && (press(B_OCTDN), !ui.confirm && !up_count()));
    hold(B_SAVE);
    press(B_OCTUP);
    memcpy(b0, endp(0, 1), P_COUNT);
    sd = seed_now(0, 1);
    am = mph[0].amount;
    expect("  OCT+: SAVED #NN, still on MORPH; one record, in that slot; the part plays that slot now",
           up_count() == 1u && up_used(sl) && !strncmp(ui.msg, "SAVED", 5) && str_eq(cur_page()->title, "MORPH") && trk[0].user == sl + 1u &&
           up_rec(sl)->engine == e && up_value(up_rec(sl), P_REL) == trk[0].p[P_REL]);
    expect("the browser: the saved slot follows the factory sounds now, the empty USER slots after it (same length, the slot's number and the part's place unchanged)", preset_pos(&t2) == t2 - 1u - UP_NUSER && t2 == UP_VSLOTS && preset_at(t2 - 1u - UP_NUSER) == sl);
    turn(EN_K4, -30);                                   /* AMOUNT 15, another B, another sound */
    turn(EN_PRESET, -1);                                /* (the factory sound before it: the place after a saved sound is an empty USER slot) */
    frames(32);
    expect("(control) another sound: End B is another roll", memcmp(endp(0, 1), b0, P_COUNT) != 0);
    turn(EN_PRESET, 1);                                 /* (on again: the saved slot) */
    frames(32);
    expect("PRESETS on one: the saved slot loads: fader at A, the saved sound, End B exactly as saved (PT_VER 4: its values, no seed; AMOUNT)",
           trk[0].user == sl + 1u && mph[0].fader == 0u && end_is_base(0, 0) && trk[0].p[P_REL] != fac[P_REL] &&
           endb_same(mph[0].eng, endp(0, 1), b0) && seed_now(0, 1) == 0u && sd != 0u && mph[0].amount == am);
    hold(B_SAVE);
    press(B_OCTUP);
    expect("SAVE held over it again: the same slot (no second one)", up_count() == 1u && up_used(sl) && !strncmp(ui.msg, "SAVED", 5) && preset_pos(&t2) == t2 - 1u - UP_NUSER);
    up_ui(1u, sl);                                      /* RESTORE */
    preset_go(pos);
    frames(32);
    expect("RESTORE: the factory sound back in the slot; loading it gives the factory sound",
           !up_count() && preset_at(pos) == sl && trk[0].user == 0u && slot_of(&trk[0]) == sl && !memcmp(trk[0].p, fac, sizeof fac));
    confirm_open(CF_INIT_SOUND, 0u);
    press(B_OCTUP);
    hold(B_SAVE);
    press(B_OCTUP);                                     /* (INIT SOUND is the engine's first sound: SAVE offers the slot it belongs to; OCT+ writes) */
    expect("after INIT SOUND: SAVE held offers the slot of the engine's first sound; OCT+ saves it there (no NAME screen)",
           up_count() == 1u && !name_on() && !strncmp(ui.msg, "SAVED", 5) && trk[0].user != 0u && up_used(trk[0].user - 1u));
    SEL_BOTH = 1;
    hold(B_SAVE);
    ok_both = ui.confirm == CF_SAVE_OVER && ui.confirm_trk == 2u;
    press(B_OCTUP);
    expect("TARGET BOTH on MORPH: OVERWRITE BOTH PATCHES, OCT+: SAVED 1+2 (each part over its own slot)",
           ok_both && str_eq(ui.msg, "SAVED 1+2") && up_count() == 2u && trk[0].user != 0u && trk[1].user != 0u && trk[0].user != trk[1].user);
}

/* FR12 (b7, review D3): SAVE held with BOTH, both parts on sounds of the read-only places (no slot of their own): two USER slots, both saved */
static void t_both_ro(void)
{
#if UP_NUSER
    uint32_t e, p, n = 0, r;
    int ok;
    fresh();
    for (e = 0; e < NENGINES && n < 2u; e++)
        for (p = 0; p < ds_count(e) && n < 2u; p++)
            if ((r = ds_list_rank(e, p)) >= UP_USER0 && r < UP_SLOTS + UP_NXTRA) {
                trk[n].eng_req = (uint8_t)e; trk[n].preset = (uint8_t)p; trk[n].user = 0; n++;
            }
    morph_page();
    SEL_BOTH = 1;
    hold(B_SAVE);
    ok = n == 2u && ui.confirm == CF_SAVE_OVER && ui.confirm_trk == 2u;
    press(B_OCTUP);
    expect("BOTH, two sounds of the read-only places: SAVE held asks for two USER slots; OCT+ saves both (SAVED 1+2)",
           ok && str_eq(ui.msg, "SAVED 1+2") && up_count() == 2u && trk[0].user != trk[1].user);
    SEL_BOTH = 0;
#endif
}

/* ---------------------------------------------------------------- SETTINGS --- */
static void power_cycle(void)                           /* the morph's RAM gone, the settings bytes kept (flash) */
{
    uint8_t keep[3] = {favorites.factory[15][12], favorites.factory[15][13], favorites.factory[15][8]};
    ui_power_on();
    favorites.factory[15][12] = keep[0];
    favorites.factory[15][13] = keep[1];
    favorites.factory[15][8] = keep[2];
    frame();
}
static void t_settings(void)
{
    fresh();
    frames(2500);
    expect("the default AMOUNT and SELECTED write nothing (the bytes stay 0, no flash write at power-on)",
           !favorites.factory[15][12] && !favorites.factory[15][13] && !ui.mset_dirty);
    morph_page();
    turn(EN_K4, 30 - (int)mph[0].amount);   /* (30: TIDE's MUSIC roll loads at 40 already, a turn to 40 would turn nothing) */
    turn(EN_ALGO, 1);
    algo_turns(ALGO_TO_P2);                             /* (0.2.9: BOTH, then PART 2) */
    turn(EN_K4, 5 - (int)mph[1].amount);
    turn(EN_ALGO, -1);                                  /* BOTH: not saved */
    go_title(MORPH_SET_TITLE); frames(32);
    mset_cfg(MQ_ARP, 1u); mset_cfg(MQ_PITCH, 1u);                 /* (ARP ON, PITCH ON) */
    expect("AMOUNT 30 / 5 into the settings bytes (1 + %), the selection BOTH in bit 7 (1.0.0-b5), a save queued",
           favorites.factory[15][12] == (0x80u | 31u) && favorites.factory[15][13] == 6u && ui.mset_dirty && SEL_BOTH);
    frames(2200);
    expect("  .. saved once the knob rested ~2 s (settings_save)", !ui.mset_dirty);
    power_cycle();
    expect("after a power cycle: each part's AMOUNT = its sound's (the settings keep 30 / 5 as the player's own), the selection BOTH kept (bit 7), ARP and PITCH ON; the ends' rolls and faders not kept; "
           "part 1's factory sound: its own End B (its seed and AMOUNT, SMART, no ARP / PITCH: sounds.c)",
           mph[0].amount == 30u && mph[1].amount == 5u &&      /* (TIDE b6: a load keeps AMOUNT, so the settings' 30 / 5 stand) */
           favorites.factory[15][12] == (0x80u | 31u) && favorites.factory[15][13] == 6u && SEL_BOTH && rolls(0, 0) == 1u && !mph[0].fader &&
           morph.cfg == (MC_ARP | MC_PIT) && !trk[0].user && trk[0].preset < ds_count(trk[0].eng_req) &&
           mph[0].seed[1][mph[0].top[1]] == ds_seed(trk[0].eng_req, trk[0].preset) &&
           mph[0].ramt[1][mph[0].top[1]] == snd_amount(trk[0].eng_req, trk[0].preset) && !(mph[0].rcfg[1][mph[0].top[1]] & MC_SET));
    favorites.factory[15][12] = 0x80u | 31u;            /* an older setting with TARGET BOTH */
    power_cycle();
    frames(2500);
    expect("an older setting's TARGET BOTH bit: power-on has the selection BOTH (1.0.0-b5: the bit is read), no rewrite, no flash write",
           SEL_BOTH && favorites.factory[15][12] == (0x80u | 31u) && !ui.mset_dirty);
    morph_page();
    turn(EN_ALGO, 1);                                   /* BOTH -> PART 2 */
    frames(2500);
    expect("  .. leaving BOTH writes the bit clear (saved once)", !SEL_BOTH && song.sel == 1u && favorites.factory[15][12] == 31u && !ui.mset_dirty);
    favorites.factory[15][12] = 0xFF; favorites.factory[15][13] = 102;
    frame();
    expect("unknown codes read as the default (15 %); bit 7 read as BOTH", mph[0].amount == MORPH_AMOUNT_DEF &&
           mph[1].amount == MORPH_AMOUNT_DEF && SEL_BOTH);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    t_round();
    t_knobs();
    t_gesture();
    t_far();
    t_target();
    t_midi();
    t_matrix();
    t_view();
    t_load();
    t_amount_keep();
    t_home1();
    t_item154();
    t_save();
    t_both_ro();
    t_settings();
    t_editb();
    printf("morphui: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
