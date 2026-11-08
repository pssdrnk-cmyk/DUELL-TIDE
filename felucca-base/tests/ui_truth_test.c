/* SPDX-License-Identifier: GPL-3.0-only */
/* The TIDE pages tell the truth (audit 2026-11-04, lane C: C5, C6, C10 and B03 / C1 / C2), on the real sources with the stubs of tests/ui_test.c:
 *   STEP      the roll's playhead is the step the sequencer plays: an ARP SEQ part at another rate than its steps (every note-on of the arp is a step
 *             with its own note; the page drawn right after it has the playhead on that step's column), and the steps past POLY's shortened loop
 *             are drawn dim (they do not play now)
 *   CUSTOM    TIDE SETTINGS > STYLE: one OCT- off CUSTOM and one OCT+ back leaves the nine values the player had
 *   INERT     TURNS / POLY / SYNC / ARC do nothing outside automatic MUSIC WNDR / JRNY with the SEQ gliding: their values are dim and knob 1's
 *             line names what is missing; with the setup right the values are in the theme colour and nothing is named
 *   ARC       AUTO TIDE's status keeps the trip ("B 5/16") while the ARC shape ("RISE 3/6") is shown
 * Built and run by host_tests.sh (name uitruth). */
#define UI_TEST_NO_MAIN 1
#define UI_TEXT_CAPTURE 1                                        /* (the texts drawn: rows_has reads knob 1's line) */
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("uitruth: FAIL %s\n", label); } } while (0)

static void run_blocks(uint32_t n)
{
    int32_t out[2 * CTL];
    while (n--)
        mix_block(out, CTL);
}
static int px_is(int32_t x, int32_t y, uint16_t c)
{
    uint16_t v = host_screen[y * 240 + x];
    return v == c || v == swap16(c);
}
static uint32_t col_sum(int32_t x0, int32_t x1, int32_t y0, int32_t y1)   /* the colours over a block of the screen, summed */
{
    uint32_t s = 0, x, y;
    for (y = (uint32_t)y0; y < (uint32_t)y1; y++)
        for (x = (uint32_t)x0; x < (uint32_t)x1; x++)
            s += host_screen[y * 240 + x] * (1u + (x & 3u));
    return s;
}
static int dots(int32_t x0, int32_t x1, int32_t y0, int32_t y1, uint16_t c)
{
    int32_t x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            n += px_is(x, y, c);
    return n;
}

/* ---- STEP: the playhead of an ARP SEQ part ---- */
static void test_roll_playhead(void)
{
    uint32_t i, blk, heard = 0, right = 0, ons = 0;
    ui_power_on(); frame();
    transport_req = 2u; frame(); frame();
    gl.on = gl.armed = 0;
    song.sel = 0; morph.target = MT_SELECTED;
    trk[0].p[P_SLEN] = 16;
    for (i = 0; i < NSTEP; i++)
        trk[0].step[i] = (step_t){{0}, 0, ST_REST, 0, 100, 0};
    for (i = 0; i < 16u; i++)
        trk[0].step[i] = (step_t){{(uint8_t)(40u + i)}, 1, ST_NOTE, 0, 100, 0};
    trk[0].p[P_AMODE] = AM_SEQ, trk[0].p[P_ARATE] = 3, trk[0].p[P_APROB] = 127, trk[0].p[P_SQNT] = 1, trk[0].p[P_MUTE] = 0;
    trk[0].nheld = 0;
    trk[0].seq_active = 1;
    trk[1].p[P_MUTE] = 1;
    song.g[G_BPM] = 240;
    song.g[G_CLOCK] = 0;
    midi_pch[0] = 0;
    midi_pch[1] = 1;
    usb.config = 1;
    go_page(GR_ROLL);
    ui.cursor = 0; ui.bank = 0;
    transport_req = 1u; frame(); frame();
    mo_w = mo_r = 0;
    for (blk = 0; blk < 4000u && heard < 40u; blk++) {
        uint32_t step = 0xFFFFu, c, found = 16u;
        int best;
        run_blocks(1);
        while (mo_r != mo_w) {
            uint32_t e = midi_out_q[mo_r % MQ];
            mo_r++;
            if (((e >> 8) & 0xF0u) == 0x90u && !((e >> 8) & 1u))
                step = (ons++) % 16u;                    /* (the arp starts at step 0 and plays every step: its k-th note-on is step k - 1 of the loop) */
        }
        if (step >= 16u)
            continue;
        ui.force = 1;
        ui_draw();
        for (c = 0, best = 0; c < 16u; c++) {                    /* (the column whose line at x + 6 is mostly the accent colour: notes draw over parts of it) */
            int n = dots(PR_X0 + (int32_t)c * PR_CW + 6, PR_X0 + (int32_t)c * PR_CW + 7, PR_WY0, PR_WY1, T_ACCENT);
            if (n > best)
                best = n, found = c;
        }
        c = best > (PR_WY1 - PR_WY0) / 3 ? found : 16u;
        heard++;
        right += c == step;
    }
    printf("uitruth: STEP playhead on the arp's step: %u of %u note-ons\n", right, heard);
    CHECK("STEP: an ARP SEQ part was heard", song.playing && heard >= 40u);
    CHECK("STEP: the playhead is the step the arp plays (every note-on)", heard >= 40u && right == heard);
    transport_req = 2u; frame(); frame();
}

/* ---- STEP: the steps past POLY's loop are dim ---- */
static void test_roll_poly_dim(void)
{
    uint32_t i, a, b, c, d;
    int32_t x13, x3;
    ui_power_on(); frame();
    transport_req = 2u; frame(); frame();
    song.sel = 0;
    trk[0].p[P_SLEN] = 16;
    trk[0].p[P_AMODE] = 0;
    for (i = 0; i < 16u; i++)
        trk[0].step[i] = (step_t){{60}, 1, ST_NOTE, 0, 100, 0};
    go_page(GR_ROLL);
    ui.cursor = 0; ui.bank = 0;
    x13 = PR_X0 + 13 * PR_CW;
    x3 = PR_X0 + 3 * PR_CW;
    poly_len[0] = 0;
    ui.force = 1; frame();
    a = col_sum(x13 + 1, x13 + PR_CW, PR_WY0 + 4, PR_WY1 - 4);
    c = col_sum(x3 + 1, x3 + PR_CW, PR_WY0 + 4, PR_WY1 - 4);
    poly_len[0] = 12;
    ui.force = 1; frame();
    b = col_sum(x13 + 1, x13 + PR_CW, PR_WY0 + 4, PR_WY1 - 4);
    d = col_sum(x3 + 1, x3 + PR_CW, PR_WY0 + 4, PR_WY1 - 4);
    poly_len[0] = 0;
    CHECK("STEP: step 14 past a 12-step POLY loop is dimmer than at full length", a != b);
    CHECK("STEP: step 4 inside the loop is unchanged", c == d);
}

/* ---- STYLE: CUSTOM comes back ---- */
static void test_style_custom(void)
{
    ui_power_on(); frame();
    transport_req = 2u; frame(); frame();
    gl.on = gl.armed = 0;
    tide_style_apply(STY_DUB);
    gl.mode = GM_WANDER; gl.time = 2; gl.wait = 1; sqm_vary_all = 1; sqm_turns = 2; sqm_poly = 2; sqm_sync = 2; sqm_arc = 2; morph_wlvl = 3;
    frame();
    CHECK("CUSTOM: a hand edit turns STYLE to CUSTOM", tide_style == STY_CUSTOM);
    jl_sel = JL_STYLE;
    tide_list_oct(1u);                                       /* OCT-: CUSTOM -> PEAK */
    CHECK("CUSTOM: OCT- picks PEAK and applies it", tide_style == STY_PEAK && gl.mode == GM_JOURNEY && gl.time == 3u && sqm_poly == 1u);
    tide_list_oct(2u);                                       /* OCT+: back onto CUSTOM */
    CHECK("CUSTOM: OCT+ is CUSTOM again", tide_style == STY_CUSTOM);
    CHECK("CUSTOM:   with the player's nine values back", gl.mode == GM_WANDER && gl.time == 2u && gl.wait == 1u && sqm_vary_all == 1u && sqm_turns == 2u &&
                                                        sqm_poly == 2u && sqm_sync == 2u && sqm_arc == 2u && (morph_wlvl & 3u) == 3u);
    tide_list_oct(1u); tide_list_oct(1u); tide_list_oct(2u); tide_list_oct(2u);   /* PEAK, AMBIENT, PEAK, CUSTOM */
    CHECK("CUSTOM: and again after two presets in between", tide_style == STY_CUSTOM && gl.mode == GM_WANDER && sqm_poly == 2u && sqm_arc == 2u && (morph_wlvl & 3u) == 3u);
    frame();
    CHECK("CUSTOM: it stays CUSTOM (the values are not a preset's)", tide_style == STY_CUSTOM);
}

/* ---- TIDE SETTINGS: rows that do nothing ---- */
static void test_inert_rows(void)
{
    int32_t y = UT_L5_Y0 + 3 * UT_L5_PITCH, x1 = UT_L5_X + UT_L5_W - 4, x0 = x1 - 90;   /* (ARC picked: the list shows PITCH .. ARC, POLY is its fourth row) */
    ui_power_on(); frame();
    transport_req = 2u; frame(); frame();
    gl.on = gl.armed = 0;
    go_title("TIDE SETTINGS");
    jl_sel = JL_ARC;
    gl.mode = GM_JOURNEY; morph_musical = 1; gl.target = GT_BOTH;
    ui.force = 1; ui_draw();
    CHECK("INERT: JRNY, MUSIC, SEQ: POLY's value in the theme colour", dots(x0, x1, y, y + UT_L5_H, T_THEME) > 0);
    CHECK("INERT:   and knob 1's line names nothing", !rows_has(0, 80, "WNDR OR JRNY") && !rows_has(0, 80, "ROLL MUSIC") && !rows_has(0, 80, "TARGET SEQ"));
    gl.mode = GM_ONCE;
    ui.force = 1; ui_draw();
    CHECK("INERT: MODE ONCE: POLY's value is dim, not the theme colour", dots(x0, x1, y, y + UT_L5_H, T_DIM) > 0 && dots(x0, x1, y, y + UT_L5_H, T_THEME) == 0);
    CHECK("INERT:   knob 1's line says WNDR OR JRNY", rows_has(0, 80, "WNDR OR JRNY"));
    gl.mode = GM_WANDER; morph_musical = 0;
    ui.force = 1; ui_draw();
    CHECK("INERT: ROLL SMART: ROLL MUSIC is named", rows_has(0, 80, "ROLL MUSIC") && dots(x0, x1, y, y + UT_L5_H, T_DIM) > 0);
    morph_musical = 1; gl.target = GT_SOUND;
    ui.force = 1; ui_draw();
    CHECK("INERT: TARGET SOUND: TARGET SEQ is named", rows_has(0, 80, "TARGET SEQ") && dots(x0, x1, y, y + UT_L5_H, T_DIM) > 0);
    jl_sel = JL_PITCH;
    ui.force = 1; ui_draw();
    CHECK("INERT: a row that always acts (PITCH) names nothing", !rows_has(0, 80, "TARGET SEQ"));
    gl.target = GT_BOTH;
}

/* ---- TIDE SETTINGS: knobs 2 .. 4 turn the picked row's value, a mark shows where the window is in the list (audit C12) ---- */
static void test_list_knobs(void)
{
    ui_power_on(); frame();
    go_title("TIDE SETTINGS");
    gl.target = GT_SOUND;
    jl_sel = JL_TARGET;
    tide_knob(1u, 1);
    CHECK("LIST: knob 2 right is the next value", gl.target == GT_SEQ);
    tide_knob(3u, 7);
    CHECK("LIST: knob 4 right, a step an event", gl.target == GT_BOTH);
    tide_knob(2u, 1);
    CHECK("LIST: it stops at the last value", gl.target == GT_BOTH);
    tide_knob(2u, -3);
    CHECK("LIST: knob 3 left is the one before", gl.target == GT_SEQ && jl_sel == JL_TARGET);
    CHECK("LIST: the row names say what they do (D9)", !strcmp(JL_NAME[JL_ARP], "ROLL ARP") && !strcmp(JL_NAME[JL_PITCH], "ROLL PITCH") && !strcmp(JL_NAME[JL_WASH], "WASH") &&
          !strcmp(jl_vname(JL_TARGET, 2), "SND+SEQ") && !strcmp(jl_vname(JL_VARY, 0), "EVERY 4TH") && !strcmp(jl_vname(JL_VARY, 1), "EVERY BAR"));
    sqm_arc = 1; sqm_auto = 0;
    CHECK("LIST: a hand drop with no automatic tide says what it needs", !strcmp(tarc_drop_hand(), "DROP NEEDS WNDR/JRNY"));
    sqm_arc = 0;
    jl_sel = JL_STYLE;
    ui.force = 1; ui_draw();
    CHECK("LIST: the scroll mark is at the top with the first row picked", px_is(236, UT_L5_Y0 + 2, T_MID) && px_is(236, UT_L5_Y0 + 6 * UT_L5_PITCH - 2, T_RAISE));
    jl_sel = JL_N - 1;
    ui.force = 1; ui_draw();
    CHECK("LIST:   and at the bottom with the last", px_is(236, UT_L5_Y0 + 6 * UT_L5_PITCH - 2, T_MID) && px_is(236, UT_L5_Y0 + 2, T_RAISE));
}

/* ---- AUTO TIDE: the ARC line keeps the trip ---- */
static void test_arc_trip(void)
{
    char l[40];
    ui_power_on(); frame();
    transport_req = 2u; frame(); frame();
    song.playing = 1;
    gl.on = 3; gl.mask = 3; gl.armed = 0; gl.hold = 0; gl.mode = GM_JOURNEY; gl.to = 1; gl.prog = 0; gl.time = 2;
    sqm_arc = 1; sqm_auto = 3; tarc.run = 1; tarc.phase = AP_RISE; tarc.len = 6; tarc.left = 4; tarc.hand = 0;
    tide_status(l);
    CHECK("ARC: the shape stays (RISE 3/6)", !strncmp(l, "RISE 3/6", 8));
    CHECK("ARC: and the trip's direction and bar follow (B 1/..)", strstr(l, "  B 1/") != 0);
    gl.to = 0;
    tide_status(l);
    CHECK("ARC: a trip to A says A", strstr(l, "  A 1/") != 0);
    tarc.hand = 1;
    tide_status(l);
    CHECK("ARC: DROP NEXT still shows (its room: the line is full with both)", strstr(l, "DROP NEXT") != 0);
    sqm_arc = 0; tarc.run = 0; tarc.hand = 0;
    tide_status(l);
    CHECK("ARC: no ARC: the trip line as before", !strncmp(l, "JRNY TO A", 9));
    song.playing = 0; gl.on = 0;
}

/* ---- the footer: a JRNY / MORPH TO destination keeps the "#NN" of the Original's label (audit C11) ---- */
static void test_footer_number(void)
{
    char a[20], b[20], want[20];
    uint32_t e, j, r;
    ui_power_on(); frame();
    e = trk[0].eng_req % NENGINES;
    j = (trk[0].preset + 1u) % ds_count(e);
    morph_ready(0);
    CHECK("footer: the destination can be set", morph_to_sound(0, 1u, j));
    tide_part_name(0, a);
    CHECK("footer: at A the part's own label has its number", a[0] == '#');
    morph_do(MQ_FADER, 1u, 127u);
    frame();
    tide_part_name(0, b);
    r = ds_list_rank(e, j);
    want[0] = 0;
    if (r < UP_USER0 && !up_used(r)) {
        up_slot_label(want, r);
        str_cpy(want + str_len(want), " ", 2);
    }
    str_cpy(want + str_len(want), ds_name(e, j), 16);
    CHECK("footer: at B the destination shows its number and name", !strcmp(b, want) && strcmp(a, b));
}

/* the footer numbers a destination past the 124 slots too (#125 .. the last read-only place, as PATCHES numbers it): audit PR 1 T1. The expected text is
 * built from the numbers PATCHES shows (rank + 1: #125 for rank 124), not from tide_part_name's own branch */
static void test_footer_number_past_slots(void)
{
    char b[20], want[20], num[8];
    uint32_t n, e = 0, j = 0, ranks = 0, last = UP_SLOTS + UP_NXTRA - 1u;
    ui_power_on(); frame();
    for (n = UP_USER0; n <= last; n++) {                 /* ranks 124 .. : the first and the last are tried */
        uint32_t ee, jj = 0;
        ee = ds_list_at(n, &jj);
        if (ds_list_rank(ee, jj) != n)
            continue;
        e = ee; j = jj; ranks++;
        trk[0].engine = trk[0].eng_req = (uint8_t)e; trk[0].user = 0;
        trk[0].preset = (uint8_t)((j + 1u) % ds_count(e));   /* (the part plays another sound: the label must come from the destination) */
        morph_ready(0);
        CHECK("footer past the slots: the destination can be set", morph_to_sound(0, 1u, j));
        morph_do(MQ_FADER, 1u, 127u);
        frame();
        tide_part_name(0, b);
        fmt_int(num, (int32_t)(n + 1u));
        want[0] = '#'; want[1] = 0;
        str_cpy(want + 1, num, 8);
        str_cpy(want + str_len(want), " ", 2);
        str_cpy(want + str_len(want), ds_name(e, j), 16);
        CHECK("footer past the slots: the destination shows #NNN and its name", !strcmp(b, want));
        if (n > UP_USER0 && n < last)
            n = last - 1u;                               /* (the first and the last place) */
    }
    CHECK("footer past the slots: the list has places there to try", ranks >= 2u);
}

int main(void)
{
    test_roll_playhead();
    test_roll_poly_dim();
    test_style_custom();
    test_inert_rows();
    test_list_knobs();
    test_arc_trip();
    test_footer_number();
    test_footer_number_past_slots();
    printf("uitruth: %u checks, %u failed\n", checks, failures);
    return failures ? 1 : 0;
}
