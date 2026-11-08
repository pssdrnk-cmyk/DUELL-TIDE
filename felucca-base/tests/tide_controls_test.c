/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise assignable CCs through the actual map and queued main-loop dispatch. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

static void screen(const char *path)
{
    FILE *f=fopen(path,"wb");
    if (!f) return;
    fprintf(f,"P6\n240 240\n255\n");
    for (uint32_t i=0;i<240u*240u;i++) {
        uint16_t v=swap16(host_screen[i]);
        uint8_t rgb[3]={(uint8_t)(((v>>11)&31u)*255u/31u),(uint8_t)(((v>>5)&63u)*255u/63u),(uint8_t)((v&31u)*255u/31u)};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}

/* the main loop and the audio together: 16 ms frames with the audio blocks that fall in them (the transport and the beats run in mix_block) */
static void run(uint32_t ms)
{
    static uint32_t acc;
    for (uint32_t i = 0; i < ms / 16u; i++) {
        int32_t out[2 * CTL];
        acc += (uint32_t)FS * 16u;
        while (acc >= 1000u * CTL) {
            acc -= 1000u * CTL;
            mix_block(out, CTL);
        }
        frame();
    }
}
/* owner 2026-11-01: GLIDE moves with PLAY, never on its own clock */
static void test_glide_transport(void)
{
    uint32_t f2, p2, p3;
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.sel = 0; morph.target = MT_SELECTED;
    gl.mode = GM_ONCE; gl.target = GT_SOUND; gl.time = 0; gl.wait = 0; gl.on = gl.armed = 0;
    song.g[G_BPM] = 120;                                 /* (1 bar: 2 s) */
    glide_launch(1u, morph_mask(0), 0);
    CHECK("glide/PLAY: a panel launch while stopped asks for PLAY, whatever it moves (SOUND)", transport_req == 1u && gl.armed == 2u);
    run(96);
    CHECK("glide/PLAY:   PLAY started, the trip with its first bar", song.playing && gl.on && !gl.armed);
    run(640);
    CHECK("glide/PLAY:   it moves while playing", morph_fader_of(0) > 0u && morph_fader_of(0) < 127u);
    transport_req = 2u; run(48);
    f2 = morph_fader_of(0); p2 = gl.prog;
    run(1200);
    CHECK("glide/PLAY: STOP pauses it where it is (no real-time clock)", !song.playing && gl.on && morph_fader_of(0) == f2 && gl.prog == p2);
    transport_req = 1u; run(32);
    p3 = gl.prog;
    CHECK("glide/PLAY: PLAY goes on from there, no jump", song.playing && p3 >= p2 && p3 - p2 < (1u << GLIDE_Q) / 20u);
    run(3000);
    CHECK("glide/PLAY:   and arrives at B with the music", !gl.on && morph_fader_of(0) == 127u);
    transport_req = 2u; run(48);
    glide_start(0u, morph_mask(0), 0);                   /* (a MIDI launch: the gear runs the transport) */
    run(800);
    CHECK("glide/PLAY: a MIDI launch while stopped waits for PLAY", !song.playing && gl.on && gl.armed && morph_fader_of(0) == 127u);
    transport_req = 1u; run(640);
    CHECK("glide/PLAY:   and moves with it", song.playing && gl.on && !gl.armed && morph_fader_of(0) < 127u);
    gl.on = gl.armed = 0;
    transport_req = 2u; run(48);
}

/* WAIT turned OFF while an arrival is held: the hold ends (it divided by a zero-bar hold) and the trip goes on */
static void test_wait_off_in_hold(void)
{
    uint32_t i;
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.sel = 0; morph.target = MT_SELECTED;
    gl.mode = GM_LOOP; gl.target = GT_SOUND; gl.time = 0; gl.wait = 1; gl.on = gl.armed = 0;
    song.g[G_BPM] = 120;
    glide_launch(1u, morph_mask(0), 0);
    for (i = 0; i < 400u && !gl.hold; i++) run(16);
    CHECK("wait off: the LOOP reaches a held arrival", gl.on && gl.hold);
    gl.wait = 0;
    run(160);
    CHECK("wait off:   the hold ends, the trip back runs", gl.on && !gl.hold && gl.to == 0u);
    gl.on = gl.armed = 0;
    transport_req = 2u; run(48);
}

/* audit C3: TIME changed mid-trip (the knob, a STYLE) lands at the next turn: the running trip keeps its length, the fader no jump,
 * the status' bar count goes on with the trip that runs while the page's TIME reads the new setting */
static void test_time_mid_trip(void)
{
    uint32_t i, p0, p1, p2, f0, f1;
    char st[40];
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.sel = 0; morph.target = MT_SELECTED;
    gl.mode = GM_LOOP; gl.target = GT_SOUND; gl.time = 2; gl.wait = 0; gl.on = gl.armed = 0;   /* (4 bars) */
    song.g[G_BPM] = 120;
    glide_launch(1u, morph_mask(0), 0);
    run(4000);                                           /* (about 2 bars of 4) */
    p0 = glide_progress(); f0 = morph_fader_of(0);
    CHECK("TIME mid-trip: the 4-bar trip is under way", gl.on && !gl.armed && p0 > (1u << GLIDE_Q) / 4u && p0 < (3u << GLIDE_Q) / 4u);
    tide_knob(2u, -1); tide_knob(2u, -1);                /* (TIME 1 bar) */
    p1 = glide_progress();
    gl.time = 6u;                                        /* (64 bars: MIDI, a STYLE) */
    p2 = glide_progress();
    gl.time = 0u;
    CHECK("TIME mid-trip: the progress stays where it was (no jump to the end, no slide back)", p0 == p1 && p1 == p2);
    tide_status(st);
    CHECK("TIME mid-trip:   the status counts the trip that runs (x/4), the page's TIME is the new 1 BAR", strlen(st) > 2u && !strcmp(st + strlen(st) - 2u, "/4") && glide_bars() == 1u);
    run(32);
    f1 = morph_fader_of(0);
    CHECK("TIME mid-trip:   the fader goes on smoothly, the trip not arrived", gl.on && gl.to == 1u && f1 + 3u >= f0 && f1 <= f0 + 6u && f1 < 127u);
    tide_style_apply(STY_PEAK); gl.mode = GM_LOOP; gl.time = 0u;   /* (a STYLE pick sets TIME directly) */
    run(32);
    CHECK("TIME mid-trip: a STYLE pick leaves the running trip's length too", gl.on && gl.to == 1u && morph_fader_of(0) < 127u);
    for (i = 0; i < 400u && gl.to == 1u; i++) run(16);
    CHECK("TIME mid-trip:   the trip arrives at B at its own 4 bars (about 2 more)", gl.to == 0u && i > 180u && i < 300u);
    tide_status(st);
    CHECK("TIME mid-trip:   the next trip runs the new TIME (x/1)", strlen(st) > 2u && !strcmp(st + strlen(st) - 2u, "/1"));
    for (i = 0; i < 400u && gl.to == 0u; i++) run(16);
    CHECK("TIME mid-trip:   and takes 1 bar", gl.to == 1u && i > 100u && i < 150u);
    sqm_turns = 0; sqm_poly = 0; sqm_sync = 0; sqm_arc = 0; sqm_vary_all = 0; morph_wlvl = 0;
    glide_stop(); transport_req = 2u; run(48);
}

/* Both panel routes must also release a paused hold without moving the endpoint. */
static void test_wait_off_paused(void)
{
    for (uint32_t reset = 0; reset < 2u; reset++) {
        uint32_t i;
        ui_power_on(); frame();
        transport_req = 2u; run(48);
        song.sel = 0; morph.target = MT_SELECTED;
        gl.mode = GM_LOOP; gl.target = GT_SOUND; gl.time = 0; gl.wait = 1; gl.on = gl.armed = 0;
        song.g[G_BPM] = 120;
        glide_launch(1u, 1u, 0);
        for (i = 0; i < 400u && !gl.hold; i++) run(16);
        CHECK("paused WAIT: reaches endpoint hold", gl.on && gl.hold && morph_fader_of(0) == 127u);
        transport_req = 2u; run(48);
        tide_style = STY_MINIMAL;                        /* (WAIT OFF: what OCT- + K4 goes back to) */
        if (reset) tide_reset(3u); else tide_knob(3u, -1);
        run(48);
        CHECK("paused WAIT: OFF removes hold at the endpoint", !gl.hold && gl.on && gl.to == 0u && !song.playing && morph_fader_of(0) == 127u);
        run(640);
        CHECK("paused WAIT: return remains paused", gl.prog == 0u && morph_fader_of(0) == 127u);
        transport_req = 1u; run(32);
        CHECK("paused WAIT: PLAY resumes from endpoint", gl.on && !gl.hold && gl.prog < (1u << GLIDE_Q) / 20u && morph_fader_of(0) >= 126u);
        run(640);
        CHECK("paused WAIT: return moves after PLAY", morph_fader_of(0) < 126u);
        glide_stop(); transport_req = 2u; run(48);
    }
}

/* a BOTH-parts glide launched from part 1: a hand on part 2's fader takes the lane over too */
static void test_both_takeover(void)
{
    uint32_t f0;
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.sel = 0; morph.target = MT_SELECTED;
    gl.mode = GM_ONCE; gl.target = GT_SOUND; gl.time = 2; gl.wait = 0; gl.on = gl.armed = 0;
    song.g[G_BPM] = 120;
    morph_do(MQ_FADER, 3u, 35u); frame();
    glide_launch(1u, 3u, 0);
    run(2000);
    CHECK("both: the glide moves both parts", gl.on && morph_fader_of(0) > 35u && morph_fader_of(1) == morph_fader_of(0));
    morph_do(MQ_FADER, 2u, 100u);                        /* (part 2 selected, its fader moved) */
    run(32);
    f0 = morph_fader_of(0);
    run(2000);
    CHECK("both:   part 2's hand stops the lane", !(gl.on & (1u << GT_SOUND)));
    CHECK("both:   part 2 stays where the hand left it", morph_fader_of(1) == 100u);
    CHECK("both:   part 1 stays where it stopped", morph_fader_of(0) == f0 && f0 < 127u);
    gl.on = gl.armed = 0;
    transport_req = 2u; run(48);
}

/* a glide of target / mode / WAIT on the parts of mask, read from part pi, 1-bar trips at 120 BPM, playing */
static void r0_launch(uint32_t pi, uint32_t mask, uint32_t target, uint32_t mode, uint32_t wait)
{
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.sel = (uint8_t)pi; morph.target = MT_SELECTED;
    gl.mode = (uint8_t)mode; gl.target = (uint8_t)target; gl.time = 0; gl.wait = (uint8_t)wait; gl.on = gl.armed = 0;
    song.g[G_BPM] = 120;
    glide_launch(1u, mask, pi);
    run(96);
}
static void r0_hold(void)
{
    for (uint32_t i = 0; i < 400u && !gl.hold; i++) run(16);
}

/* R0a: WAIT edited while playing during a hold: up keeps the hold (no new destination), OFF ends it once, the knob or OCT- reset */
static void test_wait_edit_running(void)
{
    for (uint32_t reset = 0; reset < 2u; reset++) {
        uint32_t n0, n1, s0, s1;
        r0_launch(0, 1u, GT_SOUND, GM_DRIFT, 1u);
        r0_hold();
        CHECK("running WAIT: a DRIFT reaches a held arrival at B", gl.on && gl.hold && gl.to == 0u && morph_fader_of(0) == 127u);
        n0 = mph[0].n[0]; s0 = mph[0].seed[0][mph[0].top[0]];
        tide_knob(3u, 1); run(48);
        CHECK("running WAIT: WAIT up keeps the hold", gl.on && gl.hold && gl.to == 0u && morph_fader_of(0) == 127u);
        tide_style = STY_MINIMAL;
        if (reset) tide_reset(3u); else { tide_knob(3u, -1); tide_knob(3u, -1); }
        run(32);
        n1 = mph[0].n[0]; s1 = mph[0].seed[0][mph[0].top[0]];
        CHECK("running WAIT: OFF ends the hold, the return starts at the endpoint", gl.on && !gl.hold && gl.to == 0u && gl.prog < (1u << GLIDE_Q) / 20u && morph_fader_of(0) >= 126u);
        CHECK("running WAIT:   no second destination (End A not rolled again)", n1 == n0 && s1 == s0);
        for (uint32_t i = 0; i < 400u && gl.to == 0u; i++) run(16);
        CHECK("running WAIT:   the return arrives at A, then turns", gl.on && gl.to == 1u && morph_fader_of(0) <= 1u);        glide_stop(); transport_req = 2u; run(48);
    }
}

/* R0b: a BOTH-parts glide taken over on either part, from either launching part, on each lane, moving or held */
static void test_takeover_symmetric(void)
{
    static const char *const NAME[2][2] = {{"takeover: part 1 launches, part 2 SOUND", "takeover: part 1 launches, part 2 SEQ"},
                                           {"takeover: part 2 launches, part 1 SOUND", "takeover: part 2 launches, part 1 SEQ"}};
    for (uint32_t pi = 0; pi < 2u; pi++)
        for (uint32_t lane = 0; lane < 2u; lane++)
            for (uint32_t held = 0; held < 2u; held++) {
                uint32_t other = pi ^ 1u, keep = lane ^ 1u, still;
                char label[96];
                r0_launch(pi, 3u, GT_BOTH, GM_LOOP, held ? 1u : 0u);
                if (held) r0_hold(); else run(800);
                CHECK(NAME[pi][lane], gl.on == 3u && (!held || gl.hold));
                if (lane == GT_SOUND) morph_do(MQ_FADER, 1u << other, 100u); else seqm_set_fader(other, 100u);
                run(32);
                still = GLIDE_LANE[lane].get(pi);
                snprintf(label, sizeof label, "%s%s: the touched lane stops", NAME[pi][lane], held ? " (held)" : "");
                CHECK(label, !((gl.on >> lane) & 1u));
                snprintf(label, sizeof label, "%s%s:   the other lane goes on", NAME[pi][lane], held ? " (held)" : "");
                CHECK(label, (gl.on >> keep) & 1u);
                run(2400);
                snprintf(label, sizeof label, "%s%s:   the hand's value stays, the launcher's too", NAME[pi][lane], held ? " (held)" : "");
                CHECK(label, GLIDE_LANE[lane].get(other) == 100u && GLIDE_LANE[lane].get(pi) == still);
                glide_stop(); transport_req = 2u; run(48);
            }
}

/* R0b: MIDI on the other part, selecting the other part, and a patch load on it (also one that leaves the fader where it was) */
static void test_takeover_routes(void)
{
    uint32_t f;
    r0_launch(0, 3u, GT_BOTH, GM_LOOP, 0u);
    run(800);
    song.sel = 1; run(320);
    CHECK("takeover: selecting part 2 alone cancels nothing", gl.on == 3u);
    song.sel = 0;
    morph_push(MQ_FADER, 1u, 90u); run(32);
    CHECK("takeover: a MIDI MORPH on part 2, TARGET BOTH: LINK moves its SEQ fader too, both lanes stop",
          !gl.on && morph_fader_of(1) == 90u && seqm_fader_of(1) == 90u);
    glide_stop(); transport_req = 2u; run(48);

    r0_launch(0, 3u, GT_SOUND, GM_LOOP, 0u);
    run(800);
    morph_push(MQ_FADER, 1u, 90u); run(32);
    f = morph_fader_of(0);
    run(800);
    CHECK("takeover: a MIDI MORPH on part 2 stops a SOUND glide", !gl.on && morph_fader_of(1) == 90u && morph_fader_of(0) == f);
    glide_stop(); transport_req = 2u; run(48);

    r0_launch(1, 3u, GT_BOTH, GM_LOOP, 0u);
    run(800);
    morph_push(MQ_SEQF, 0u, 90u); run(32);
    CHECK("takeover: a MIDI SEQ fader on part 1 stops the SEQ lane", (gl.on & 1u) && !(gl.on & 2u) && seqm_fader_of(0) == 90u);
    glide_stop(); transport_req = 2u; run(48);

    r0_launch(0, 3u, GT_SOUND, GM_LOOP, 0u);
    run(800);
    apply_preset_to(&trk[1], 3u); run(64);
    CHECK("takeover: a patch load on part 2 stops the SOUND lane", !gl.on && morph_fader_of(1) == 0u);
    f = morph_fader_of(0);
    run(800);
    CHECK("takeover:   part 1 stays where it stopped", morph_fader_of(0) == f);
    glide_stop(); transport_req = 2u; run(48);

    r0_launch(0, 3u, GT_BOTH, GM_JOURNEY, 0u);           /* (the glide's own arrivals and targets are no load) */
    run(12000);
    CHECK("takeover: a JOURNEY's own arrivals never take it over (6 trips)", gl.on == 3u);
    glide_stop(); transport_req = 2u; run(48);

    for (uint32_t p = 0; p < 2u; p++) {                  /* a held arrival at A: the load leaves the fader at 0 */
        char label[80];
        r0_launch(p, 3u, GT_SOUND, GM_LOOP, 1u);
        r0_hold();                                       /* (at B) */
        gl.wait = 0; run(96);
        gl.wait = 1;
        r0_hold();                                       /* (at A now) */
        CHECK("takeover: a LOOP holds at A", gl.on && gl.hold && gl.to == 1u && morph_fader_of(0) == 0u && morph_fader_of(1) == 0u);
        apply_preset_to(&trk[p ^ 1u], 3u); run(64);
        snprintf(label, sizeof label, "takeover: a same-value load on part %u during a hold at A stops the lane", (unsigned)(p ^ 1u) + 1u);
        CHECK(label, !gl.on);
        run(4000);
        CHECK("takeover:   neither fader leaves A", morph_fader_of(0) == 0u && morph_fader_of(1) == 0u);
        glide_stop(); transport_req = 2u; run(48);
    }
}

/* owner 2026-11-01: on the TIDE page a K1 twist while gliding stops the glide where it is; PLAY goes on, the next twist launches */
static void test_twist_stop(void)
{
    uint32_t pg = 0, pages = sizeof PAGES / sizeof PAGES[0], f;
    ui_power_on(); frame();
    while (pg < pages && PAGES[pg].graph != GR_TIDE) pg++;
    CHECK("twist: the TIDE page exists", pg < pages);
    ui.page = pg; ui.cursor = ui.bank = 0; page_entered();
    song.sel = 0; morph.target = MT_SELECTED;
    gl.mode = GM_ONCE; gl.target = GT_SOUND; gl.time = 0; gl.wait = 0; gl.on = gl.armed = 0;
    song.g[G_BPM] = 120;
    run(480);
    tide_knob(0, 1);
    CHECK("twist: a twist launches (and starts PLAY)", gl.on && gl.to == 1u);
    run(1200);
    f = morph_fader_of(0);
    CHECK("twist:   it is moving", song.playing && gl.on && f > 0u && f < 127u);
    run(480);
    tide_knob(0, 1);                                     /* (audit C4, D9: one detent is a nudge, two stop a running tide) */
    CHECK("twist: one detent while gliding does not stop it", glide_running() && song.playing);
    tide_knob(0, 1);
    f = morph_fader_of(0);
    CHECK("twist: two detents while gliding stop it, PLAY goes on", !glide_running() && song.playing);
    tide_knob(0, 1); tide_knob(0, 1);
    CHECK("twist:   the rest of the same twist does nothing", !glide_running());
    run(1200);
    CHECK("twist:   the sound stays where it stopped", song.playing && morph_fader_of(0) == f && f > 0u && f < 127u);
    tide_knob(0, -1);
    CHECK("twist: the next twist launches again", gl.on && gl.to == 0u);
    gl.on = gl.armed = 0;
    transport_req = 2u; run(48);
}

/* audit C17: MODE, TIME and WAIT move one step an event however fast the knob turns (a turn of 5 used to skip 16 bars -> 64) */
static void test_knob_steps(void)
{
    uint32_t pg = 0, pages = sizeof PAGES / sizeof PAGES[0];
    ui_power_on(); frame();
    while (pg < pages && PAGES[pg].graph != GR_TIDE) pg++;
    ui.page = pg; ui.cursor = ui.bank = 0; page_entered();
    gl.mode = GM_ONCE; gl.time = 1; gl.wait = 1;
    tide_knob(1u, 5); tide_knob(2u, 3); tide_knob(3u, 4);
    CHECK("knob steps: a fast turn moves MODE, TIME and WAIT by one", gl.mode == GM_LOOP && gl.time == 2u && gl.wait == 2u);
    tide_knob(1u, -4); tide_knob(2u, -6); tide_knob(3u, -9);
    CHECK("knob steps:   and back by one", gl.mode == GM_ONCE && gl.time == 1u && gl.wait == 1u);
}

/* audit C9 (D9): on AUTO TIDE OCT- + K3 / K4 go back to the STYLE's TIME and WAIT (the style a hand edit left), K1 says BACK TO ORIGINAL; the TIDE SETTINGS list has no reset */
static void test_oct_reset_style(void)
{
    uint32_t pg = 0, pages = sizeof PAGES / sizeof PAGES[0];
    ui_power_on(); frame();
    while (pg < pages && PAGES[pg].graph != GR_TIDE) pg++;
    ui.page = pg; ui.cursor = ui.bank = 0; page_entered();
    jl_put(JL_STYLE, STY_AMBIENT); tide_settings();
    gl.time = 0; gl.wait = 0;
    CHECK("OCT- reset: K3 goes to the style's TIME (AMBIENT: 64 bars)", tide_reset(2u) == 1 && gl.time == 6u);
    CHECK("OCT- reset: K4 goes to the style's WAIT (AMBIENT: 8 bars)", tide_reset(3u) == 1 && gl.wait == 4u);
    gl.time = 1; tide_settings();                        /* (a hand edit: CUSTOM, the style it came from is kept) */
    CHECK("OCT- reset: the edit made it CUSTOM", tide_style == STY_CUSTOM);
    tide_reset(2u);
    CHECK("OCT- reset:   from CUSTOM, K3 still goes back to AMBIENT's TIME", gl.time == 6u);
    jl_put(JL_STYLE, STY_PEAK); tide_settings();
    gl.time = 0; gl.wait = 5;
    tide_reset(2u); tide_reset(3u);
    CHECK("OCT- reset: PEAK's TIME and WAIT (8 bars, OFF)", gl.time == 3u && gl.wait == 0u);
    tide_reset(0u);
    CHECK("OCT- reset: K1 says BACK TO ORIGINAL", !strcmp(ui.msg, "BACK TO ORIGINAL"));
}

/* a WILD roll with ARP ON keeps a SEQ Original on SEQ and never draws SEQ for another (morph.c, TIDE_AMODE_KEEP / _END) */
static void test_seq_rule(void)
{
    int8_t o[P_COUNT], end[P_COUNT];
    uint32_t i, seed, kept = 1, drawn = 0, moved = 0;
    uint8_t sv = mrc;
    for (i = 0; i < P_COUNT; i++)
        o[i] = (int8_t)trk[0].p[i];
    mrc = MC_WILD | MC_ARP;
    o[P_AMODE] = AM_SEQ_ID;
    for (seed = 1; seed <= 300u; seed++) {
        morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
        kept &= end[P_AMODE] == AM_SEQ_ID;
    }
    o[P_AMODE] = 2;
    for (seed = 1; seed <= 300u; seed++) {
        morph_roll_pure(o, end, &ENG_ANALOG, 100, seed, 0, 1);
        drawn += end[P_AMODE] == AM_SEQ_ID;
        moved += end[P_AMODE] != 2;
    }
    mrc = sv;
    CHECK("SEQ rule: a SEQ Original stays SEQ (300 WILD rolls)", kept);
    CHECK("SEQ rule: another arp mode is never rolled into SEQ", !drawn);
    CHECK("SEQ rule:   while WILD still moves the arp mode", moved > 0u);
}

/* owner 2026-11-01: ARP SEQ on PLAY with no key held plays from the last chord held, else C4 (the steps as written); recording changes neither (audit liverec L1) */
static void test_seq_ghost(void)
{
    track_t *t = &trk[0];
    uint32_t i, heard = 0;
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    t->p[P_SLEN] = 4;
    memset(t->step, 0, sizeof t->step);
    for (i = 0; i < 4u; i++) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = (uint8_t)(60 + 2 * i); t->step[i].vel = 100; }
    t->p[P_AMODE] = AM_SEQ_ID; t->p[P_ARATE] = 5; t->p[P_APROB] = 127; t->p[P_AOCT] = 1;
    memset(tide_chord_n, 0, sizeof tide_chord_n);
    run(320);
    CHECK("seq ghost: stopped, no key: silent as before", !t->arp_note);
    transport_req = 1u;
    for (i = 0; i < 60u; i++) { run(16); heard |= t->arp_note >= 60u && t->arp_note <= 66u; }
    CHECK("seq ghost: PLAY, no key: the steps play from C4, as written", song.playing && heard);
    input_on(t, 48, 100); heard = 0;
    for (i = 0; i < 60u; i++) { run(16); heard |= t->arp_note >= 48u && t->arp_note <= 54u; }
    CHECK("seq ghost: a key held plays as ever (from it)", t->nheld == 1u && heard);
    input_off(t, 48); heard = 0;
    for (i = 0; i < 60u; i++) { run(16); heard |= t->arp_note >= 48u && t->arp_note <= 54u; }
    CHECK("seq ghost: let go, it goes on from that key (the last chord)", !t->nheld && heard);
    {   /* REC armed (audit liverec L1): the stand-in keeps playing, and an ARP SEQ part's arp records nothing, so the pattern is as it was */
        step_t was[NSTEP];
        uint32_t last = 0, changes = 0;
        memcpy(was, t->step, sizeof was);
        song.rec = 1;
        for (i = 0; i < 240u; i++) { run(16); if (t->arp_note && t->arp_note != last) changes++; last = t->arp_note; }
        CHECK("seq ghost: recording: keeps playing, the pattern unchanged", changes >= 8u && !memcmp(was, t->step, sizeof was));
    }
    song.rec = 0;
    transport_req = 2u; run(320);
    CHECK("seq ghost: STOP: silent", !song.playing && !t->arp_note);
    t->p[P_AMODE] = 0;
}

/* audit G08: the stand-in is the part's chord (CHRD MIN7: C Eb G Bb from C4), not a single C4; CHRD OFF stays the single note */
static void test_seq_ghost_chord(void)
{
    track_t *t = &trk[0];
    uint32_t i, root = 0, nch = 0, third = 0, fifth = 0, seventh = 0;
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    t->p[P_SLEN] = 4;
    memset(t->step, 0, sizeof t->step);
    for (i = 0; i < 4u; i++) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = 60; t->step[i].vel = 100; }
    t->p[P_AMODE] = AM_SEQ_ID; t->p[P_ARATE] = 5; t->p[P_APROB] = 127; t->p[P_AOCT] = 1;
    t->p[P_VOICE] = V_POLY; t->p[P_CHRD] = 7; t->p[P_VOIC] = 0;                    /* MIN7, CLOSE */
    memset(tide_chord_n, 0, sizeof tide_chord_n);
    run(320);
    transport_req = 1u;
    for (i = 0; i < 60u; i++) {
        run(16);
        root |= t->arp_note == 60u;
        nch += t->arp_ch[0] != 0u;
        third |= t->arp_ch[0] == 63u;
        fifth |= t->arp_ch[1] == 67u;
        seventh |= t->arp_ch[2] == 70u;
    }
    printf("seq ghost chord: CHRD MIN7, PLAY, no key: root %u, third %u, fifth %u, seventh %u (blocks with a chord note: %u)\n", root, third, fifth, seventh, nch);
    CHECK("seq ghost: CHRD MIN7, PLAY, no key: the stand-in is C Eb G Bb, not C4 alone", root && third && fifth && seventh);
    t->p[P_CHRD] = 0;
    transport_req = 2u; run(320);
    t->p[P_AMODE] = 0;
}

/* owner 2026-11-01: TIDE SETTINGS flickered while playing: a glide moves the part colour, the page is drawn again, and the hidden
 * cards cleared the card row under the list each time */
static void test_settings_flicker(void)
{
    uint32_t pg = 0, tp = 0, pages = sizeof PAGES / sizeof PAGES[0], i, fl = 0;
    static uint16_t ref[240 * 240];
    ui_power_on(); frame();
    while (pg < pages && PAGES[pg].graph != GR_TIDELIST) pg++;
    while (tp < pages && PAGES[tp].graph != GR_TIDE) tp++;
    ui.page = pg; ui.cursor = ui.bank = 0; page_entered(); run(320);
    memcpy(ref, host_screen, sizeof ref);
    gl.mode = GM_WANDER; gl.target = GT_SOUND; gl.time = 0; gl.wait = 0;
    glide_launch(1u, morph_mask(0), 0); run(500);
    for (i = 0; i < 120u; i++) { host_fl_begin(); run(16); fl += host_fl_end(0, 240); }
    CHECK("settings flicker: gliding, the page never flashes", glide_running() && fl == 0u);
    glide_stop(); transport_req = 2u; run(320);
    ui.page = tp; page_entered(); run(320);
    ui.page = pg; page_entered(); run(320);
    CHECK("settings flicker: back from AUTO TIDE, no card left in the card row", !memcmp(host_screen + 142 * 240, ref + 142 * 240, 2u * 240u * 66u));
}

int main(void)
{
    uint32_t q[MQ_N], n;
    char name[40];
    map_info_t info;
    ui_power_on(); frame();
    song.playing = song.rec = 0;
    morph.target = MT_SELECTED;
    gl.mode = GM_ONCE; gl.target = GT_SOUND; gl.on = gl.armed = 0;
    mq_r = mq_w = 0;
    memset(midi_tide_fired, 0, sizeof midi_tide_fired);
    CHECK("new targets append to stock IDs", MAP_T_GLIDE == MAP_T_RES_SMART + 1u && MAP_T_SEQ == MAP_T_GLIDE + 1u);
    CHECK("GLIDE can be assigned to part 2", map_set(80, MAP_T_GLIDE, MAP_R_PART2, MAP_W_TYPED, 1, 0) == MAP_OK);
    CHECK("SEQ can be assigned to part 2", map_set(81, MAP_T_SEQ, MAP_R_PART2, MAP_W_TYPED, 1, 0) == MAP_OK);
    CHECK("GLIDE rejects a global rule", !map_rule_ok(MAP_T_GLIDE, MAP_R_GLOBAL));
    map_name(MAP_LINE(MAP_T_GLIDE, MAP_R_PART2, MAP_W_TYPED), name);
    CHECK("GLIDE has a map label", !strcmp(name, "GLIDE"));
    map_target_info(MAP_T_GLIDE, &trk[1], &info);
    CHECK("GLIDE is advertised as an action", info.kind == MAP_K_ACTION);
    map_target_info(MAP_T_SEQ, &trk[1], &info);
    CHECK("SEQ is advertised as continuous", info.kind == MAP_K_CONTINUOUS);

    midi_map_cc(3, &trk[0], 80, 0);
    midi_map_cc(3, &trk[0], 80, 20);
    n = morph_pop(q);
    CHECK("staying in A queues one launch", n == 1 && (q[0] & 255u) == MQ_GLIDE && (q[0] >> 16) == 0);
    CHECK("the queued part follows the map", n == 1 && ((q[0] >> 8) & 255u) == 1u);
    midi_map_cc(3, &trk[0], 80, 64);
    CHECK("neutral queues nothing", morph_pop(q) == 0);
    midi_map_cc(3, &trk[0], 80, 100);
    morph_poll();
    CHECK("B dispatch starts part 2's glide", gl.to == 1 && gl.mask == 2 && gl.pi == 1 && gl.on);
    midi_map_cc(3, &trk[0], 80, 127);
    CHECK("staying in B queues nothing", morph_pop(q) == 0);
    midi_map_cc(3, &trk[0], 80, 42);
    n = morph_pop(q);
    CHECK("crossing directly back to A retriggers", n == 1 && (q[0] >> 16) == 0);
    midi_map_cc(4, &trk[0], 80, 42);
    CHECK("zone latches are independent per channel", morph_pop(q) == 1);
    midi_map_cc(3, &trk[0], 81, 87);
    midi_map_cc(3, &trk[0], 81, 93);
    n = morph_pop(q);
    CHECK("a queued SEQ stream coalesces to its latest value", n == 1 && (q[0] & 255u) == MQ_SEQF && (q[0] >> 16) == 93);
    midi_map_cc(3, &trk[0], 81, 89);
    morph_poll();
    CHECK("SEQ dispatch moves part 2's sequence fader", seqm_fader_of(1) == 89);

    CHECK("GLIDE can also be assigned to part 1", map_set(82, MAP_T_GLIDE, MAP_R_PART1, MAP_W_TYPED, 1, 0) == MAP_OK);
    midi_map_cc(3, &trk[0], 82, 42);
    n = morph_pop(q);
    CHECK("same-channel part controllers have independent latches", n == 1 && ((q[0] >> 8) & 255u) == 0);
    midi_tide(3, NSHOWN, MAP_T_GLIDE, 0);
    CHECK("hidden tracks cannot launch a shown part", morph_pop(q) == 0);

    song.sel = 0; gl.on = gl.armed = 0;
    perf_enter();
    CHECK("PERFORM labels match its actions", !strcmp(PF_NAME[PF_ROLLA], "GLIDE A") && !strcmp(PF_NAME[PF_ROLLB], "GLIDE B"));
    perf_roll_n[1]++; perf_input(0, fm1_ms, 0);
    CHECK("PERFORM B launches a glide", gl.on && gl.to == 1 && gl.mask == 1);
    perf_roll_n[0]++; perf_input(0, fm1_ms, 0);
    CHECK("PERFORM A launches a glide", gl.on && gl.to == 0 && gl.mask == 1);
    frame();
    const char *shot = getenv("DUELL_TEST_SCREENSHOT");
    if (shot) screen(shot);
    gl.on = gl.armed = 0; perf_leave();
    btn_down(B_SEQ); frames(800);
    CHECK("SEQ tools opens with KEEP available", ui.layer == LAYER_SEQ && tl_cell(TL_KEEP));
    if (shot) screen("build/tide-seq-tools.ppm");
    btn_up(B_SEQ); frame();
    uint32_t pg=0, pages=sizeof PAGES/sizeof PAGES[0];
    while(pg<pages && PAGES[pg].graph!=GR_ROLL) pg++;
    CHECK("STEP page is reachable", pg<pages);
    if (pg<pages) {
        ui.page=pg; ui.cursor=ui.bank=0; page_entered();
        trk[0].p[P_SLEN]=16;
        memset(trk[0].step,0,sizeof trk[0].step);
        for(uint32_t k=0;k<16;k+=4) {
            trk[0].step[k].time=ST_NOTE; trk[0].step[k].n=1;
            trk[0].step[k].note[0]=(uint8_t)(60+k/2);
            trk[0].step[k].vel=100;
        }
        seqm_roll(0); seqm_set_fader(0,127);
        ui.force=1; frame();
        if (shot) screen("build/tide-step.ppm");
    }
    if (shot) {
        splash_frame(5,0); screen("build/tide-splash.ppm");
        ui.menu=2; ui.menu_scroll=1; ui.force=1; frame();
        screen("build/tide-about.ppm");
    }
    test_glide_transport();
    test_twist_stop();
    test_oct_reset_style();
    test_knob_steps();
    test_wait_off_in_hold();
    test_time_mid_trip();
    test_wait_off_paused();
    test_both_takeover();
    test_wait_edit_running();
    test_takeover_symmetric();
    test_takeover_routes();
    test_seq_rule();
    test_seq_ghost();
    test_seq_ghost_chord();
    test_settings_flicker();
    printf("tidecontrols: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
