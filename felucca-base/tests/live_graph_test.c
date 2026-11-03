/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE LIVE GRAPHS (firmware/src/ui_live.c) against ui_test.c's stub display; the 1.1 port of JAM's live_graph_test.
 *   build/host/live_graph_test           (host_tests.sh "livegraph")
 *   ENV      AMP ENV: ONE playhead, the latest note's, on graph_adsr's curve (attack by level, decay / sustain / release on the drawn curve),
 *            frame by frame with the real audio; ENV 2 (FILTER ENV) only while the audio ticks it; a release before the sustain continues from the
 *            observed point; an engine with its own envelopes (FM6) has no AMP ENV playhead
 *   FM6      OP ENV and AMP DEST follow the selected operator's real envelope (graph_fm6env), frame by frame; finished notes remove the marker
 *   LFO      LFO 1 and LFO 2 (all three pages each): the playhead on graph_lfo's wave; an LFO 2 nothing reads moves on its own UI clock (PREVIEW)
 *            while the audio generator stands still; used (matrix, TWIN), it follows the audio phase
 *   CYCLE    the latest note's real rise / fall position and value; unrouted or no note: a silent animated PREVIEW; ONCE ends at zero
 *   FILTER   the latest note's effective cutoff (voice.c t->lvc): the dial with no route, an ENV sweep moves it; nothing without a voice
 *   TWIN     a tick at each filter's live cutoff (t->lvc2); COMB: none for filter 2
 *   INSERT   CHORUS: the playhead at the sweep's phase on the drawn sine
 *   MOD      under each acting row a bar for its source; a row that does nothing: none
 *   CHORD    a bar in a sounding key; none without a voice
 *   COST     a still picture is not drawn again; a moving one at most every 2nd frame (30 a second); MENU > ANIM OFF: static, no overlay, the
 *            screen exactly the picture; the picture with ANIM on and nothing sounding equals the one with ANIM off (the overlay only adds)
 *   HARD RULE viewing a graph never ticks the audio generator (LFO 2's phase, the CYCLE arrays, the FM6 envelope) or adds a route
 *   live_graph_test DIR: the screens too, DIR/live_<page>.ppm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static const char *shot_dir;
static void shot(const char *name)                   /* the screen as a PPM (the panel's byte order -> RGB) */
{
    char path[512];
    FILE *f;
    uint32_t i;
    if (!shot_dir)
        return;
    snprintf(path, sizeof path, "%s/live_%s.ppm", shot_dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    fprintf(f, "P6 240 240 255 ");
    for (i = 0; i < 240u * 240u; i++) {
        uint32_t v = host_screen[i], c = ((v >> 8) | (v << 8)) & 0xFFFFu;
        fputc((int)(((c >> 11) & 31u) * 255u / 31u), f);
        fputc((int)(((c >> 5) & 63u) * 255u / 63u), f);
        fputc((int)((c & 31u) * 255u / 31u), f);
    }
    fclose(f);
}
static void expect(const char *what, int ok)
{
    printf("livegraph: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void quiet(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        t->v[i].active = 0, t->v[i].stage = 0;
}
static void voice_at(track_t *t, uint32_t i, uint32_t stage, int32_t env)
{
    t->v[i].active = 1, t->v[i].stage = (uint8_t)stage, t->v[i].env = env;
}
static uint32_t redraws(uint32_t n, void (*move)(uint32_t))   /* frames whose graph was drawn again */
{
    uint32_t i, r = 0, s;
    for (i = 0; i < n; i++) {
        if (move)
            move(i);
        s = ui.graph_sig;
        frame();
        r += ui.graph_sig != s;
    }
    return r;
}
static void env_rise(uint32_t i) { TSEL->v[0].env = (int32_t)((i + 1u) << 20); }
static uint32_t ufilt_engine(void)
{
    uint32_t i;
    for (i = 0; i < NENGINES; i++)
        if (ENGINES[i]->ufilt && ENGINES[i]->ownenv == 0)
            return i;
    return 0;
}
static uint16_t px(int32_t x, int32_t y) { uint16_t v = host_screen[(uint32_t)y * 240u + (uint32_t)x]; return (uint16_t)((v >> 8) | (v << 8)); }
static uint32_t panel_hash(void)                      /* the picture area's pixels */
{
    uint32_t h = 2166136261u, y, x;
    for (y = Y_GRAPH; y < Y_GRAPH + H_GRAPH && y < 240u; y++)
        for (x = 0; x < 240u; x++)
            h = (h ^ host_screen[y * 240u + x]) * 16777619u;
    return h;
}
/* the marker on the picture's own line: the same page with ANIM OFF is the picture alone; its line colour (the part colour, or the accent where a stage is picked) must be within 2 rows of the dot at the dot's column */
static int line_hit(void)
{
    int32_t x = lv.x, y = lv.y, r, ok = 0;
    ui_prefs |= PREF_ANIM_OFF;
    frames(80);
    for (r = y - 2; r <= y + 2; r++)
        ok |= px(x, Y_GRAPH + GOY + r) == T_THEME || px(x, Y_GRAPH + GOY + r) == T_ACCENT;
    if (!ok)
        for (r = y - 8; r <= y + 8; r++)
            printf("   col %d row %d: %04x (theme %04x accent %04x)\n", (int)x, (int)r, px(x, Y_GRAPH + GOY + r), T_THEME, T_ACCENT);
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF;
    frames(80);
    return ok;
}
static void on_line(const char *what) { expect(what, line_hit()); }
static uint32_t screen_hash(void)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < 240u * 240u; i++)
        h = (h ^ host_screen[i]) * 16777619u;
    return h;
}
static void render(uint32_t n)
{
    int32_t o[2u * CTL];
    while (n--) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
    }
}
/* walk n steps of (render r blocks, frames f ms) and count how many different playhead positions (lx / ex) and different panel images were seen */
typedef struct { uint32_t pos, img, active; } mv_t;
static mv_t moves_env(uint32_t n, uint32_t r, uint32_t ms)
{
    mv_t m = {0, 0, 0};
    uint32_t i, hs = 0;
    int32_t ox = -1000, oy = -1000;
    for (i = 0; i < n; i++) {
        uint32_t h;
        render(r);
        frames(ms);
        h = panel_hash();
        m.active += lv.n;
        if (lv.n && (lv.x != ox || lv.y != oy))
            m.pos++;
        if (h != hs)
            m.img++;
        ox = lv.x, oy = lv.y, hs = h;
    }
    return m;
}
static mv_t moves_lx(uint32_t n, uint32_t r, uint32_t ms)
{
    mv_t m = {0, 0, 0};
    uint32_t i, hs = 0;
    int32_t ox = -1000, oy = -1000;
    for (i = 0; i < n; i++) {
        uint32_t h;
        render(r);
        frames(ms);
        h = panel_hash();
        m.active += lv.n;
        if (lv.n && (lv.x != ox || lv.y != oy))
            m.pos++;
        if (h != hs)
            m.img++;
        ox = lv.x, oy = lv.y, hs = h;
    }
    return m;
}

/* ---- ENV */
static void t_env(void)
{
    track_t *t;
    int32_t a, d, r, sus, x1, x3, bot = 88 * graph_ht / 100, top = 6 * graph_ht / 100;
    int16_t was;
    ui_power_on();
    frame();
    go_title("AMP ENV"); frame();
    t = TSEL;
    quiet(t);
    t->p[P_ATK] = 64, t->p[P_DEC] = 64, t->p[P_SUS] = 80, t->p[P_REL] = 64;
    a = 4 + 64 * 50 / 127, d = 6 + 64 * 50 / 127, r = 6 + 64 * 60 / 127, sus = 80 * 1000 / 127, x1 = 12 + a, x3 = 226 - r;
    t->m_vi = 0;
    voice_at(t, 0, 1, 1 << 23);
    frames(400);
    expect("ENV the playhead: the latest note, half way up the attack", lv.kind == LV_ENV && lv.n == 1u &&
           lv.x == 12 + a / 2 && abs(lv.y - (bot - (lv.x - 12) * (bot - top) / a)) <= 1);
    on_line("ENV the attack's dot is on the drawn line");
    voice_at(t, 1, 2, 80 << 17);                     /* (another voice: not followed) */
    frames(100);
    expect("ENV one playhead only: the latest note's, not another voice's", lv.x == 12 + a / 2);
    t->v[1].active = 0;
    voice_at(t, 0, 2, (80 << 17) + (1 << 22));
    frames(400);
    expect("ENV decay above SUS: on the decay, before its end", lv.x > x1 && lv.x < x1 + d);
    on_line("ENV the decay's dot is on the drawn line");
    voice_at(t, 0, 2, 80 << 17);
    frames(200);
    was = lv.x;
    frames(400);
    expect("ENV held at SUS: on the sustain line, creeping along it", lv.x >= x1 + d && lv.x > was && lv.x < x3 &&
           abs(lv.y - (bot - sus * (bot - top) / 1000)) <= 1);
    on_line("ENV the sustain's dot is on the drawn line");
    was = lv.x;
    frames(1008);
    expect("ENV the creep on a held sustain is 8 px a second, whatever the look rate", lv.x - was >= 7 && lv.x - was <= 9);
    shot("env");
    voice_at(t, 0, 3, (80 << 17) / 4);
    frames(400);
    expect("ENV released, at a quarter of SUS: on the release, falling", lv.x > x3 && lv.x <= x3 + r && lv.y > bot - sus * (bot - top) / 1000);
    on_line("ENV the release's dot is on the drawn line");
    t->p[P_REL] = 127;
    voice_at(t, 0, 3, 1 << 14);
    frames(80);
    expect("a morph lengthening RELEASE mid-note keeps the playhead inside the panel", lv.x <= 227);
    quiet(t);
    frames(400);
    expect("ENV no note: no playhead", lv.n == 0u);
    go_title("ENV 2"); frame();
    t->mdw.e2 = 0, t->p[P_E2FLT] = 0;
    t->v[0].active = 1, t->v[0].stage2 = 2, t->v[0].env2 = 1 << 23;
    frames(400);
    expect("ENV 2 not running (nothing reads it): no playhead", lv.kind == LV_ENV && lv.n == 0u);
    t->mdw.e2 = 1;
    frames(400);
    expect("ENV 2 running (a matrix slot reads it): its own stage and level", lv.n == 1u);
    t->mdw.e2 = 0, t->p[P_E2FLT] = 20;
    frames(400);
    expect("ENV 2 running (MULTI CUTOFF depth): its own stage and level", lv.n == 1u);
    t->p[P_E2FLT] = 0;
    quiet(t);
}

/* a release before the sustain continues from where the envelope was, not from a jump to the right */
static void t_env_early_release(void)
{
    track_t *t;
    lv_eg_t g;
    int16_t ay;
    ui_power_on(); frame();
    go_title("AMP ENV"); frame();
    t = TSEL;
    quiet(t);
    t->p[P_ATK] = 90, t->p[P_DEC] = 60, t->p[P_SUS] = 40, t->p[P_REL] = 70;
    t->m_vi = 0;
    voice_at(t, 0, 2, 600 << 14);                    /* (decaying, well above SUS) */
    frames(80);
    lv_eg(t, P_ATK, &g);
    voice_at(t, 0, 3, 590 << 14);
    frames(80);
    expect("ENV released during the decay: the dot goes to the start of the drawn release (it stays on the line)", lv.n && lv.estg == 3u && lv.x >= g.x3 && lv.x <= g.x3 + 3);
    on_line("ENV the early release's first dot is on the drawn line");
    ay = lv.y;
    voice_at(t, 0, 3, 100 << 14);
    frames(80);
    expect("ENV the early release falls on along the drawn release", lv.x > g.x3 + 3 && lv.y > ay);
    on_line("ENV the early release's later dot is on the drawn line");
    shot("env_early_release");
    quiet(t);
    frames(80);
}

/* real audio, frame by frame: the playhead and the picture move */
static void t_env_audio(void)
{
    track_t *t;
    mv_t m;
    ui_power_on(); frame();
    t = TSEL;
    t->eng_req = 0; t->engine = 0;
    t->p[P_ATK] = 100, t->p[P_DEC] = 70, t->p[P_SUS] = 60, t->p[P_REL] = 90;
    morph_rebase_part(song.sel);
    go_title("AMP ENV"); frames(80);
    trk_note_on(t, 60, 100);
    m = moves_env(40, 8, 48);
    printf("livegraph: AMP ENV audio: %u positions, %u images in 40 steps (%u active)\n", m.pos, m.img, m.active);
    expect("ENV AMP ENV with real audio: the playhead moves and the picture follows, frame by frame", m.pos >= 6 && m.img >= 6 && m.active >= 30);
    shot("env_audio");
    trk_note_off(t, 60);
    m = moves_env(30, 12, 48);
    expect("ENV AMP ENV release with real audio moves", m.pos >= 5);
    render(6000); frames(80);
    expect("ENV the finished note: no playhead", lv.n == 0u);
    /* FILTER ENV (ENV 2), read by MULTI CUTOFF */
    t->p[P_E2ATK] = 100, t->p[P_E2DEC] = 70, t->p[P_E2SUS] = 40, t->p[P_E2REL] = 90, t->p[P_E2FLT] = 40;
    morph_rebase_part(song.sel);
    go_title("ENV 2"); frames(80);
    trk_note_on(t, 60, 100);
    m = moves_env(40, 8, 48);
    printf("livegraph: ENV 2 audio: %u positions, %u images in 40 steps\n", m.pos, m.img);
    expect("ENV FILTER ENV (ENV 2) with real audio: the playhead moves and the picture follows", lv.kind == LV_ENV && m.pos >= 5 && m.img >= 5);
    shot("env2_audio");
    trk_note_off(t, 60);
    render(6000);
    t->p[P_E2FLT] = 0; morph_rebase_part(song.sel);
}

/* FM6: OP ENV and AMP DEST follow the operator envelope */
static void fm_bass2(track_t *t)
{
    uint32_t k;
    set_engine_of(t, ENGI_FM6); t->engine = ENGI_FM6;
    for (k = 0; k < ds_count(ENGI_FM6) && !str_eq(ds_name(ENGI_FM6, k), "FM BASS 2"); k++) {}
    apply_preset_to(t, k);
}
static void t_fm6(void)
{
    track_t *t;
    uint32_t i, saved_state;
    mv_t m;
    int16_t x;
    ui_power_on(); frame();
    t = TSEL; fm_bass2(t); render(4);
    go_title("OP ENV"); frames(80);
    expect("FM6 OP ENV without a note has no false live marker", lv.kind == LV_FMENV && !lv.n);
    trk_note_on(t, 60, 100);
    m = moves_env(32, 88, 64);
    printf("livegraph: FM6 OP ENV: %u visible positions, %u images in 32 steps (%u active)\n", m.pos, m.img, m.active);
    expect("FM BASS 2 OP ENV visibly moves across real audio-driven frames (the picture too)", m.pos >= 8 && m.active >= 16 && m.img >= 8);
    shot("fm6_op_env");
    saved_state = (uint32_t)fm6_note_of(t, &t->v[t->m_vi % NVOICE])->env[5].level;
    frames(64);
    expect("FM6 live drawing never advances the audio envelope", saved_state == (uint32_t)fm6_note_of(t, &t->v[t->m_vi % NVOICE])->env[5].level);
    go_title("AMP DEST"); frames(80); x = lv.x;
    expect("FM6 AMP DEST draws the operator envelope (graph_fm6env), live", lv.kind == LV_FMENV && TIDE_AMPDEST_FM6(cur_page(), t));
    render(88); frames(80);
    expect("FM6 AMP DEST also follows its real operator envelope", lv.kind == LV_FMENV && lv.n && lv.x != x);
    on_line("FM6 AMP DEST: the dot is on the operator envelope's line");
    shot("fm6_amp_dest_live");
    go_title("AMP ENV"); frames(80);
    expect("FM6 AMP ENV (the unused generic ADSR): no playhead", lv.kind == LV_NONE && !TIDE_AMPDEST_FM6(cur_page(), t));
    go_title("AMP DEST"); frames(80);
    trk_note_off(t, 60);
    m = moves_env(64, 22, 16);
    expect("FM6 release moves the live marker too", m.pos > 0);
    render(4000); frames(80);
    expect("FM6 finished notes remove the marker", !lv.n);
    ui_prefs |= PREF_ANIM_OFF; frames(80);
    expect("FM6 ANIM OFF: AMP DEST keeps the operator envelope picture, static, no marker", lv.kind == LV_NONE && TIDE_AMPDEST_FM6(cur_page(), t));
    expect("FM6 ANIM OFF: AMP DEST is not drawn again while nothing changes", redraws(20, 0) == 0);
    turn(EN_K1, 1); frames(80);
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF;
    (void)i;
}

/* ---- cost and ANIM OFF */
static void t_cost(void)
{
    track_t *t;
    uint32_t n, h_off, h_on;
    ui_power_on();
    frame();
    go_title("AMP ENV"); frame();
    t = TSEL;
    quiet(t);
    t->m_vi = 0;
    voice_at(t, 0, 1, 1 << 20);
    frames(80);
    n = redraws(20, 0);
    expect("COST a still envelope: the graph is not drawn again", n == 0u);
    n = redraws(20, env_rise);
    printf("livegraph: a rising envelope: %u redraws in 20 frames\n", (unsigned)n);
    expect("COST a moving envelope: drawn every 4th frame (15 a second), and it is drawn", n >= 4u && n <= 6u);
    ui_prefs |= PREF_ANIM_OFF;
    n = redraws(2, env_rise);                        /* (one redraw takes the overlay away) */
    n = redraws(20, env_rise) + (n > 1u) * 100u;
    expect("COST MENU > ANIM OFF: one redraw clears the overlay, then none while it moves", n == 0u && lv.kind == LV_NONE);
    quiet(t);
    frames(80);
    h_off = panel_hash();
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF;
    frames(80);
    h_on = panel_hash();
    expect("the picture with ANIM on and nothing sounding is the ANIM OFF picture (the overlay only adds)", h_on == h_off);
}

/* ---- LFO 1 and 2 */
static void t_lfo(void)
{
    track_t *t;
    uint32_t ph0;
    int32_t x;
    ui_power_on();
    frame();
    go_title("LFO 1"); frame();
    t = TSEL;
    ph0 = (uint32_t)t->p[P_LRATE + 2u] << 25;
    t->lfo_ph = ph0 + 100u * LV_LFO_STEP + 7u;
    frames(80);
    shot("lfo");
    expect("LFO 1 the playhead at the phase (100 px into the first cycle), the dot on the wave", lv.kind == LV_LFO && lv.x == PANEL_X0 + 100 && lv.y == lv_lfo_y(t, 0, 100));
    x = lv.x;
    expect("LFO 1 the dot is on the screen (TEXT)", px(x, Y_GRAPH + GOY + lv.y) == T_TEXT);
    on_line("LFO 1 the dot is on the drawn wave");
    t->lfo_ph = ph0 + 10u * LV_LFO_STEP + 7u;
    frames(80);
    expect("LFO 1 after a wrap: the second cycle", lv.x == PANEL_X0 + 120);
    t->lfo_ph = ph0 + 5u * LV_LFO_STEP + 7u;
    frames(80);
    expect("LFO 1 after the next wrap: the first again", lv.x == PANEL_X0 + 5);
}
/* the dot on the line all along: the LFO's five waves at eleven phases, the envelope's attack, decay and release at ten levels each */
static void t_on_line_sweeps(void)
{
    track_t *t;
    uint32_t w, k, ph0, bad;
    int32_t a;
    ui_power_on(); frame();
    go_title("LFO 1"); frame();
    t = TSEL;
    ph0 = (uint32_t)t->p[P_LRATE + 2u] << 25;
    for (w = 0; w < 5u; w++) {
        t->p[P_LWAVE] = (int16_t)w;
        for (bad = 0, k = 0; k < 11u; k++) {
            t->lfo_ph = ph0 + (k * 19u + 3u) * LV_LFO_STEP + 5u;
            frames(80);
            bad += !line_hit();
        }
        printf("livegraph: LFO wave %u: %u of 11 dots off the line\n", (unsigned)w, (unsigned)bad);
        expect("LFO the dot follows the drawn wave", bad == 0u);
    }
    go_title("AMP ENV"); frame();
    quiet(t);
    t->p[P_ATK] = 64, t->p[P_DEC] = 64, t->p[P_SUS] = 50, t->p[P_REL] = 64;
    t->m_vi = 0;
    a = 0;
    for (w = 1; w <= 3u; w++) {                       /* stage 1 attack, 2 decay, 3 release */
        for (bad = 0, k = 0; k < 10u; k++) {
            int32_t lvl = w == 1u ? (int32_t)(k * 100u + 50u) : (int32_t)(1000u - k * 70u);
            if (w == 3u) lvl = (int32_t)(500u - k * 45u);
            if (w == 2u) { voice_at(t, 0, 1, 1 << 23); frames(160); }   /* (a new note for each decay) */
            voice_at(t, 0, w, (int32_t)(((int64_t)lvl << 24) / 1000));
            frames(160);
            bad += lv.n && !line_hit();
            a += lv.n;
        }
        printf("livegraph: ENV stage %u: %u of 10 dots off the line\n", (unsigned)w, (unsigned)bad);
        expect("ENV the dot follows the drawn curve", bad == 0u);
    }
    expect("ENV the sweep had dots to check", a >= 25);
    quiet(t);
}
static void t_on_line_more(void)
{
    track_t *t;
    uint32_t k, ty, bad, n;
    ui_power_on(); frame();
    t = TSEL;
    XV(t, X_CYMODE) = 0, XV(t, X_CYRISE) = 70, XV(t, X_CYFALL) = 70, XV(t, X_CYSYNC) = 0;
    for (n = 0; n < 2u; n++) {                       /* CYCLE ENV 2/2 (one big shape, bent) and 1/2 (three small), a preview all along */
        XV(t, X_CYCURVE) = n ? -30 : 40;
        go_title(n ? "CYCLE ENV 1/2" : "CYCLE ENV 2/2"); frames(80);
        for (bad = 0, k = 0; k < 14u; k++) {
            frames(240);
            bad += lv.n && !line_hit();
        }
        printf("livegraph: CYCLE page %u: %u of 14 dots off the line\n", (unsigned)n, (unsigned)bad);
        expect("CYCLE the dot follows the drawn shape", bad == 0u);
    }
    t->p[P_IB] = 100;
    go_title("INSERT 1/2"); frame();
    for (ty = IT_PHASER; ty <= IT_CHORUS; ty++) {
        t->p[P_ITYPE] = (int16_t)ty;
        for (bad = 0, k = 0; k < 10u; k++) {
            ins[song.sel % NLIVE].ph = (k * 23u + 3u) * LV_LFO_STEP + 7u;
            frames(80);
            bad += !line_hit();
        }
        printf("livegraph: INSERT type %u: %u of 10 dots off the line\n", (unsigned)ty, (unsigned)bad);
        expect("INSERT the dot follows the drawn sweep", bad == 0u);
    }
    t->p[P_ITYPE] = IT_OFF;
    ui_power_on(); frame();
    t = TSEL; fm_bass2(t); render(4);
    go_title("OP ENV"); frames(80);
    trk_note_on(t, 60, 100);
    for (bad = 0, n = 0, k = 0; k < 30u; k++) {
        render(60); frames(80);
        if (lv.n) { n++; bad += !line_hit(); }
    }
    printf("livegraph: OP ENV: %u of %u dots off the line\n", (unsigned)bad, (unsigned)n);
    expect("FM6 OP ENV the dot follows the operator envelope's line", n >= 10u && bad == 0u);
    trk_note_off(t, 60);
}
static void t_lfo_audio(void)
{
    static const char *const pages[] = {"LFO 1", "LFO 1 TRIG", "LFO 1 DEST"};
    track_t *t;
    mv_t m;
    uint32_t i;
    ui_power_on(); frame();
    t = TSEL;
    t->eng_req = 0; t->engine = 0;
    t->p[P_LWAVE] = 0, t->p[P_LRATE] = 96, t->p[P_LSYNC] = 0;
    for (i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = 0;
    t->p[P_M1SRC] = MS_LFO, t->p[P_M1DST] = MD_CUT, t->p[P_M1AMT] = 16;
    morph_rebase_part(song.sel); render(8);
    trk_note_on(t, 60, 100);
    for (i = 0; i < NELEM(pages); i++) {
        go_title(pages[i]); frames(80);
        m = moves_lx(24, 6, 80);
        printf("livegraph: %s audio: %u positions, %u images in 24 steps\n", pages[i], m.pos, m.img);
        expect(pages[i], lv.kind == LV_LFO && m.pos >= 12 && m.img >= 12);
        if (!i) shot("lfo1_audio");
    }
    trk_note_off(t, 60);
    for (i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = 0;
    morph_rebase_part(song.sel);
}
static void t_lfo2(void)
{
    static const char *const pages[] = {"LFO 2", "LFO 2 TRIG", "LFO 2 DEST"};
    track_t *t;
    uint32_t i, ph;
    int16_t x;
    mv_t m;
    ui_power_on(); frame();
    t = TSEL;
    t->eng_req = (uint8_t)ufilt_engine(); t->engine = t->eng_req;   /* (an engine with the multimode filter: the matrix slot to CUTOFF acts) */
    for (i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = 0;
    t->p[P_TWTYPE] = TT_OFF, XV(t, X_LFO2) = 0;
    t->p[P_L2WAVE] = 0, t->p[P_L2RATE] = 96, t->p[P_L2SYNC] = 0;
    morph_rebase_part(song.sel); render(4);
    go_title("LFO 2 DEST"); frames(80); ph = t->lfo2.ph;
    render(32); frames(80);
    m = moves_lx(24, 0, 80);
    expect("LFO 2 unused: its silent preview moves (the picture too) while the real generator remains idle", lv.kind == LV_LFO &&
           lv.n && lv.pv && m.pos >= 10 && m.img >= 10 && !lv_l2_on(t) && t->lfo2.ph == ph);
    shot("lfo2_unrouted");
    t->p[P_M1SRC] = MS_LFO2, t->p[P_M1DST] = MD_CUT, t->p[P_M1AMT] = 16;
    morph_rebase_part(song.sel); render(8); frames(80);
    expect("LFO 2 routed in the matrix: the real phase", lv_l2_on(t) && lv.n && !lv.pv && t->lfo2.ph != ph);
    x = lv.x; render(32); frames(80);
    expect("LFO 2 playhead advances with the audio and stays on its own wave", lv.x != x && lv.y == lv_lfo_y(t, 1, lv.x - PANEL_X0));
    ph = t->lfo2.ph; frames(64);
    expect("LFO 2 display never ticks the generator", t->lfo2.ph == ph);
    for (i = 0; i < NELEM(pages); i++) {
        go_title(pages[i]); frames(80);
        expect(pages[i], lv_lfo2_page(cur_page()) && lv.kind == LV_LFO && lv.n && lv.y == lv_lfo_y(t, 1, lv.x - PANEL_X0));
    }
    shot("lfo2_running");
    t->p[P_M1AMT] = 0;
    morph_rebase_part(song.sel); render(8); frames(80);
    expect("LFO 2 matrix amount zero: back to the silent preview", !lv_l2_on(t) && lv.n && lv.pv);
    t->p[P_TWTYPE] = TT_LP, XV(t, X_LFO2) = 16;
    morph_rebase_part(song.sel); render(8); frames(80);
    expect("LFO 2 TWIN-only depth keeps its live display active", lv_l2_on(t) && lv.n && !lv.pv);
    t->p[P_TWTYPE] = TT_OFF;
    morph_rebase_part(song.sel); render(8); frames(80);
    expect("LFO 2 a disabled TWIN filter uses the silent preview", !lv_l2_on(t) && lv.n && lv.pv);
    go_title("LFO 1"); frames(80);
    expect("LFO 1 retains its always-running playhead", lv.kind == LV_LFO && lv.n && !lv.pv);
    ui_prefs |= PREF_ANIM_OFF; go_title("LFO 2"); frames(80);
    ph = panel_hash(); frames(200);
    expect("LFO 2 with ANIM OFF: no preview, the picture static", lv.kind == LV_NONE && ph == panel_hash());
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF;
}

/* ---- CYCLE */
static void t_cycle(void)
{
    track_t *t;
    voice_t *v;
    lv_cy_t g;
    uint32_t i, vi, st;
    int32_t lin;
    uint16_t y;
    int16_t x;
    mv_t m;
    ui_power_on(); frame();
    t = TSEL;
    t->p[P_ATK] = 0, t->p[P_SUS] = 127, t->p[P_CHRD] = 0, t->p[P_AMODE] = 0;
    for (i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = 0;
    XV(t, X_CYMODE) = 0, XV(t, X_CYRISE) = 70, XV(t, X_CYFALL) = 70;
    XV(t, X_CYCURVE) = 0, XV(t, X_CYSYNC) = 0;
    morph_rebase_part(song.sel);
    render(2); go_title("CYCLE ENV 2/2"); frames(80);
    m = moves_lx(24, 0, 80);
    expect("CYCLE unused: its preview moves (the picture too) without enabling the audio generator", lv.kind == LV_CYCLE &&
           lv.n && lv.pv && m.pos >= 10 && m.img >= 10 && !t->cy);
    shot("cycle_unrouted");
    t->p[P_M1SRC] = MS_CYCLE, t->p[P_M1DST] = MD_CUT, t->p[P_M1AMT] = 16;
    morph_rebase_part(song.sel);
    render(4); frames(80); x = lv.x; frames(160);
    expect("CYCLE routed but with no sounding note still has an animated silent preview", t->cy &&
           lv.n && lv.pv && lv.x != x);
    trk_note_on(t, 60, 100); render(4); frames(80);
    vi = t->m_vi % NVOICE; v = &t->v[vi];
    lv_cy_geom(t, &g);
    expect("CYCLE routed: the latest sounding note has a playhead on its rise", t->cy && lv.n && !lv.pv &&
           cy_st[song.sel][vi] == 1 && lv.x >= g.x0 && lv.x < g.x0 + g.rw);
    x = lv.x; render(20); frames(80);
    expect("CYCLE real audio ticks advance the playhead", lv.x > x);
    lin = cy_lin[song.sel][vi]; y = cy_y[song.sel][vi]; st = cy_st[song.sel][vi];
    frames(64);
    expect("CYCLE display reads the state without advancing or changing it", cy_lin[song.sel][vi] == lin &&
           cy_y[song.sel][vi] == y && cy_st[song.sel][vi] == st);
    XV(t, X_CYCURVE) = 40; morph_rebase_part(song.sel);
    render(12); frames(80);
    expect("CYCLE curved rise: the dot uses the actual generated value", abs(lv.y - (g.base - (int32_t)cycle_value(t, v) * (g.base - g.top) / 32768)) <= 2);
    on_line("CYCLE the dot is on the drawn curve (its CURVE bent)");
    shot("cycle_rise");
    m = moves_lx(40, 14, 80);
    printf("livegraph: CYCLE audio: %u positions, %u images in 40 steps\n", m.pos, m.img);
    expect("CYCLE with real audio: the playhead and the picture move frame by frame", m.pos >= 20 && m.img >= 20);
    for (i = 0; i < 500 && cy_st[song.sel][vi] != 2; i++) render(1);
    render(10); frames(80);
    expect("CYCLE fall travels to the right on the falling half", cy_st[song.sel][vi] == 2 &&
           lv.x >= g.x0 + g.rw && abs(lv.y - (g.base - (int32_t)cycle_value(t, v) * (g.base - g.top) / 32768)) <= 2);
    shot("cycle_fall");
    go_title("CYCLE ENV 1/2"); frames(80); lv_cy_geom(t, &g);
    expect("CYCLE first page uses its time-proportioned repeated shape", lv.n && g.n == 3 &&
           lv.x >= g.x0 && lv.x <= g.x0 + g.n * g.cw);
    shot("cycle_overview");
    ui_prefs |= PREF_ANIM_OFF; frames(80);
    expect("CYCLE respects ANIM OFF", lv.kind == LV_NONE);
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF; frames(80);
    expect("CYCLE a stationary audio state does not redraw repeatedly", redraws(20, 0) == 0);
    XV(t, X_CYSYNC) = 1, XV(t, X_CYRISE) = 42, XV(t, X_CYFALL) = 42;
    morph_rebase_part(song.sel); render(4); frames(80); lv_cy_geom(t, &g);
    expect("CYCLE SYNC uses the same live state and note-value geometry", lv.n && lv.x >= g.x0 && lv.x <= g.x0 + g.n * g.cw);
    XV(t, X_CYMODE) = 1; morph_rebase_part(song.sel);
    cycle_start(t, v, 0); render(800); frames(80); lv_cy_geom(t, &g);
    expect("CYCLE ONCE ends at zero and the end of its single shape", cy_st[song.sel][vi] == 0 && lv.n &&
           lv.x == g.x0 + g.cw && lv.y == g.base);
    t->p[P_M1AMT] = 0; morph_rebase_part(song.sel); render(2); frames(80);
    expect("CYCLE amount zero switches to a silent preview", !t->cy && lv.n && lv.pv);
    quiet(t); frames(80);
    song.sel = 1; t = TSEL;
    fm_bass2(t);
    for (i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = 0;
    t->p[P_AMODE] = 0, t->p[P_M1SRC] = MS_CYCLE, t->p[P_M1DST] = MD_CUT, t->p[P_M1AMT] = 16;
    XV(t, X_CYMODE) = 0, XV(t, X_CYRISE) = 70, XV(t, X_CYFALL) = 70;
    XV(t, X_CYCURVE) = 0, XV(t, X_CYSYNC) = 0;
    morph_rebase_part(song.sel); render(4);
    go_title("CYCLE ENV 2/2"); trk_note_on(t, 60, 100); render(8); frames(80);
    lv_cy_geom(t, &g); vi = t->m_vi % NVOICE;
    expect("CYCLE follows FM BASS 2 on part 2 using that part's actual generator", t->cy && lv.n &&
           t->v[vi].active && cy_st[1][vi] == 1 && abs(lv.y - (g.base - (int32_t)cycle_value(t, &t->v[vi]) * (g.base - g.top) / 32768)) <= 2);
    shot("cycle_fm6"); quiet(t); frames(80);
    song.sel = 0;
}

/* ---- FILTER and TWIN */
static void t_filt(void)
{
    track_t *t;
    uint32_t i, lo = 255, hi = 0;
    ui_power_on();
    frame();
    t = TSEL;
    t->eng_req = (uint8_t)ufilt_engine();
    render(256);
    go_title("MULTI FILTER 1/2"); frame();
    t->p[P_FREEA] = 40, t->p[P_FREEC] = 0, t->p[P_KTRK] = 0, t->p[P_ED_FLT] = 0, t->p[P_E2FLT] = 0, t->p[P_LD_FLT] = 0, t->p[P_L2FLT] = 0;
    morph_rebase_part((uint32_t)(t - trk));             /* (the values set here are the Original: the MORPH's ends no longer move them) */
    for (i = 0; i < NMSLOT; i++)
        t->p[P_M1SRC + 3u * i] = 0;
    frames(80);
    expect("FILTER no voice: no live curve", lv.kind == LV_FILT && lv.fc == 0u);
    trk_note_on(t, 60, 100);
    render(8);
    frames(80);
    printf("livegraph: the dial 40, no route: lvc %u\n", (unsigned)t->lvc);
    expect("FILTER a note, no route: the live cutoff is the dial's (40 steps)", t->lvc == 80u && lv.fc == gf_cut_hz(40 + 198 * 80));
    expect("FILTER the live cutoff's tick is on the screen (ACCENT)", px(PANEL_X0 + gf_x(lv.fc << 8), Y_GRAPH + GOY + 10) == T_ACCENT);
    trk_note_off(t, 60);
    render(400);
    t->p[P_E2FLT] = 100, t->p[P_E2ATK] = 40, t->p[P_E2DEC] = 60, t->p[P_E2SUS] = 0;
    trk_note_on(t, 60, 100);
    for (i = 0; i < 120u; i++) {
        render(4);
        if (i == 6u) {
            frames(80);
            shot("filter");
        }
        lo = t->lvc < lo ? t->lvc : lo;
        hi = t->lvc > hi ? t->lvc : hi;
    }
    printf("livegraph: an ENV 2 sweep: lvc %u .. %u\n", (unsigned)lo, (unsigned)hi);
    expect("FILTER ENV 2 -> CUTOFF: the live cutoff sweeps above the dial and back", lo <= 82u && hi > 100u);
    t->p[P_E2FLT] = 0;
    trk_note_off(t, 60);
    render(400);
}
static void t_twin(void)
{
    track_t *t;
    int32_t x1, x2;
    ui_power_on();
    frame();
    t = TSEL;
    t->eng_req = (uint8_t)ufilt_engine();
    render(256);
    t->p[P_TWTYPE] = TT_LP, XV(t, X_LINK) = 0, t->p[P_CUT2] = 30, XV(t, X_KTRK2) = 0, XV(t, X_ENV2) = 0, XV(t, X_LFO1) = 0, XV(t, X_LFO2) = 0;
    t->p[P_FREEA] = 60, t->p[P_KTRK] = 0, t->p[P_ED_FLT] = 0, t->p[P_E2FLT] = 0, t->p[P_LD_FLT] = 0, t->p[P_L2FLT] = 0;
    morph_rebase_part((uint32_t)(t - trk));
    go_title("TWIN FILTER"); frame();
    trk_note_on(t, 60, 100);
    render(8);
    frames(80);
    printf("livegraph: TWIN lvc %u lvc2 %u\n", (unsigned)t->lvc, (unsigned)t->lvc2);
    x1 = PANEL_X0 + gf_x(gf_cut_hz(40 + 198 * 120) << 8), x2 = PANEL_X0 + gf_x(gf_cut_hz(40 + 198 * 60) << 8);
    expect("TWIN filter 2 LINK off, no route: its live cutoff is CUTOFF 2's dial (30 steps); filter 1's its own (60)", lv.kind == LV_TWIN && t->lvc2 == 60u && t->lvc == 120u);
    expect("TWIN the two ticks: filter 1 capped at the top, filter 2 at the bottom", px(x1, Y_GRAPH + GOY + 5) == T_ACCENT && px(x2, Y_GRAPH + GOY + 60) == T_ACCENT);
    shot("twin");
    t->p[P_TWTYPE] = TT_COMB;
    frames(80);
    expect("TWIN COMB: no tick for filter 2 (no cutoff)", lv.fc2 == 0u && lv.fc != 0u);
    trk_note_off(t, 60);
    render(400);
    frames(80);
    expect("TWIN no voice: no tick", lv.fc == 0u && lv.fc2 == 0u);
    go_title("TWIN CHAIN"); frames(80);
    t->p[P_TWTYPE] = TT_LP;
    expect("TWIN CHAIN (the flow strip): no ticks", lv.kind == LV_NONE);
    t->p[P_TWTYPE] = TT_OFF;
}

/* ---- INSERT, MOD, CHORD */
static void t_ins(void)
{
    track_t *t;
    ui_power_on();
    frame();
    t = TSEL;
    t->p[P_ITYPE] = IT_CHORUS, t->p[P_IB] = 100;
    ins[(uint32_t)(t - trk) % NLIVE].ph = 0u;                   /* (the sweep the other tests left: from its start) */
    go_title("INSERT 1/2"); frame();
    ins[(uint32_t)(t - trk) % NLIVE].ph = 40u * LV_LFO_STEP + 9u;
    frames(80);
    shot("insert");
    expect("INSERT CHORUS: the playhead at the sweep's phase (40 px), the dot on the sine", lv.kind == LV_INS && lv.x == PANEL_X0 + 40 &&
           px(lv.x, Y_GRAPH + GOY + lv.y) == T_TEXT);
    ins[(uint32_t)(t - trk) % NLIVE].ph = 90u * LV_LFO_STEP + 9u;
    frames(80);
    expect("INSERT the playhead moves with the phase", lv.x == PANEL_X0 + 90);
    on_line("INSERT the dot is on the drawn sweep");
    t->p[P_ITYPE] = IT_SOFT;
    frames(80);
    expect("INSERT a drive (no sweep): no playhead", lv.kind == LV_NONE);
    t->p[P_ITYPE] = IT_OFF;
}
static void mw_sweep(uint32_t i) { TSEL->mw = (uint8_t)(i * 6u & 127u); }
static void t_mod(void)
{
    track_t *t;
    uint32_t i;
    int32_t y1, y2, y3;
    ui_power_on();
    frame();
    t = TSEL;
    for (i = 0; i < NMSLOT; i++)
        t->p[MSLOT_ID(i)] = 0, t->p[MSLOT_ID(i) + 1u] = 0, t->p[MSLOT_ID(i) + 2u] = 0;
    t->p[MSLOT_ID(0)] = MS_MODW, t->p[MSLOT_ID(0) + 1u] = MD_CUT, t->p[MSLOT_ID(0) + 2u] = 50;
    t->p[MSLOT_ID(1)] = MS_LFO, t->p[MSLOT_ID(1) + 1u] = MD_CUT, t->p[MSLOT_ID(1) + 2u] = 50, t->p[P_LPOL] = 0;
    t->p[MSLOT_ID(2)] = MS_MODW, t->p[MSLOT_ID(2) + 1u] = MD_CUT, t->p[MSLOT_ID(2) + 2u] = 0;
    go_title("MATRIX 1-4"); frame();
    y1 = MX_Y0 + MX_H, y2 = MX_Y0 + MX_PITCH + MX_H, y3 = MX_Y0 + 2 * MX_PITCH + MX_H;
    t->mw = 127, t->lfo_val = -32767, t->lfo_fade = 32767;
    frames(80);
    shot("mod");
    expect("MOD MOD WHEEL at 127: a full bar from the left under row 1", lv.kind == LV_MOD && px(35, y1) == T_ACCENT && px(92, y1) == T_ACCENT);
    expect("MOD LFO 1 at its bottom: a bar from the middle to the left under row 2", px(40, y2) == T_ACCENT && px(80, y2) != T_ACCENT);
    expect("MOD a slot that does nothing (AMT 0): no bar", lv.mv[2] == INT32_MIN && px(35, y3) != T_ACCENT && px(35, y3) != T_RAISE);
    t->mw = 64;
    frames(80);
    expect("MOD MOD WHEEL at half: half a bar", px(35, y1) == T_ACCENT && px(80, y1) == T_RAISE);
    {
        uint32_t h0, h1, n = redraws(20, mw_sweep);
        frames(80);
        h0 = screen_hash();
        t->mw = 20; frames(80);
        h1 = screen_hash();
        expect("MOD a moving source: the bar moves and the page is not drawn again for it (strips only)", n == 0u && h0 != h1);
    }
    t->mw = 0;
    frames(80);
    expect("MOD MOD WHEEL at 0: the bar gone", px(35, y1) != T_ACCENT);
    for (i = 0; i < 3u; i++)
        t->p[MSLOT_ID(i)] = 0;
}
static void t_chord(void)
{
    track_t *t;
    uint32_t note, h_off, h_on;
    ui_power_on();
    frame();
    t = TSEL;
    t->p[P_CHRD] = CH_MAJ;
    go_title("CHORD"); frame();
    quiet(t);
    frames(80);
    expect("CHORD the page is live with a chord type set", lv.kind == LV_CHORD);
    h_on = screen_hash();
    ui_prefs |= PREF_ANIM_OFF; frames(80);
    h_off = screen_hash();
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF; frames(80);
    expect("CHORD silent: the picture is the ANIM OFF picture", h_on == h_off);
    t->m_vi = 0;
    voice_at(t, 0, 2, 1 << 24);
    t->v[0].note = 64;                               /* E4: the chord is C major from C4, so the keyboard runs C4 .. B5 */
    frames(80);
    shot("chord");
    h_on = screen_hash();
    ui_prefs |= PREF_ANIM_OFF; frames(80);
    h_off = screen_hash();
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF; frames(80);
    expect("CHORD a sounding key: a bar on the keyboard (the screen differs from the ANIM OFF picture)", h_on != h_off);
    voice_at(t, 0, 3, 1 << 22);
    frames(80);
    expect("CHORD releasing: the bar is shorter and in MID (another image)", screen_hash() != h_on);
    quiet(t);
    frames(80);
    expect("CHORD silent again: no bar", screen_hash() == h_off || lv.n == 0u);
}

/* the main-loop cost of the overlay in basic blocks a frame (cpu-budget's unit: gcc -fsanitize-coverage=trace-pc, tools/cpu_budget.sh), built with -DLG_BB and a
 * counter bbc_ext: the same moving page frame by frame with ANIM on and ANIM OFF; the difference is the overlay (a still page: 0 once drawn) */
#ifdef LG_BB
extern unsigned long long bbc_ext;
static uint32_t bb_redraws;
static unsigned long long bb_frames(uint32_t n, void (*move)(uint32_t))
{
    unsigned long long a = bbc_ext, b = 0;
    uint32_t i, s;
    bb_redraws = 0;
    for (i = 0; i < n; i++) {
        if (move)
            move(i);
        s = ui.graph_sig;
        a = bbc_ext;
        frame();
        b += bbc_ext - a;
        bb_redraws += ui.graph_sig != s;
    }
    return b;
}
static void env_bb(uint32_t i) { TSEL->v[0].env = (int32_t)((i % 120u) * ((1u << 24) / 121u)); }
static void mw_move(uint32_t i) { TSEL->mw = (uint8_t)(i * 3u & 127u); }
static void ins_move(uint32_t i) { ins[song.sel % NLIVE].ph += 0x2400000u; (void)i; }
static void lvl_move(uint32_t i) { TSEL->v[0].env = (int32_t)(((i * 37u) & 1023u) << 14); }
static void cut_move(uint32_t i) { TSEL->lvc = (uint8_t)(40 + (i * 3u & 63u)); }
static void lfo_move(uint32_t i) { TSEL->lfo_ph += 0x1800000u; (void)i; }
static void bb_row(const char *what, void (*move)(uint32_t))
{
    unsigned long long on, off;
    uint32_t r_on;
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF;
    frames(160);
    on = bb_frames(240, move);
    r_on = bb_redraws;
    ui_prefs |= PREF_ANIM_OFF;
    frames(160);
    off = bb_frames(240, move);
    ui_prefs &= (uint8_t)~PREF_ANIM_OFF;
    printf("livegraph: BB %-32s ANIM on %6.0f / frame, ANIM OFF %6.0f / frame: overlay +%.0f / frame, %u redraws in 240 frames = %.0f a redraw, %.0f BB/s at 60 frames/s\n", what, on / 240.0, off / 240.0,
           (on - off) / 240.0, (unsigned)r_on, r_on ? (double)(on - off) / r_on : 0.0, (on - off) / 240.0 * 60.0);
}
static void t_bb(void)
{
    track_t *t;
    uint32_t i;
    ui_power_on(); frame();
    t = TSEL; t->eng_req = (uint8_t)ufilt_engine(); t->engine = t->eng_req;
    t->p[P_ATK] = 100, t->p[P_DEC] = 70, t->p[P_SUS] = 60, t->p[P_REL] = 90;
    go_title("AMP ENV"); frame(); t->m_vi = 0;
    voice_at(t, 0, 1, 1 << 20);
    bb_row("AMP ENV (playhead, lit curve)", env_bb);
    go_title("LFO 1"); frame();
    bb_row("LFO 1 (playhead)", lfo_move);
    go_title("MULTI FILTER 1/2"); frame();
    t->p[P_FREEA] = 40;
    voice_at(t, 0, 2, 1 << 23); t->lvc = 60;
    bb_row("MULTI FILTER (still: one cutoff)", 0);
    for (i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = 0, t->p[MSLOT_ID(i) + 1u] = 0, t->p[MSLOT_ID(i) + 2u] = 0;
    t->p[MSLOT_ID(0)] = MS_MODW, t->p[MSLOT_ID(0) + 1u] = MD_CUT, t->p[MSLOT_ID(0) + 2u] = 50;
    go_title("MATRIX 1-4"); frame();
    bb_row("MATRIX 1-4 (a row's bar)", mw_move);
    t->p[P_ITYPE] = IT_CHORUS, t->p[P_IB] = 100;
    go_title("INSERT 1/2"); frame();
    bb_row("INSERT CHORUS (playhead)", ins_move);
    t->p[P_TWTYPE] = TT_LP;
    go_title("TWIN FILTER"); frame();
    voice_at(t, 0, 2, 1 << 23);
    bb_row("TWIN FILTER (two ticks)", cut_move);
    t->p[P_TWTYPE] = TT_OFF;
    go_title("CYCLE ENV 2/2"); frame();
    quiet(t);
    bb_row("CYCLE ENV 2/2 (silent preview)", 0);
    t->p[P_CHRD] = CH_MAJ;
    go_title("CHORD"); frame();
    voice_at(t, 0, 2, 1 << 23); t->v[0].note = 64;
    bb_row("CHORD (a key's bar)", lvl_move);
    quiet(t);
}
#endif

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    shot_dir = argc > 1 ? argv[1] : 0;
    t_env();
    t_env_early_release();
    t_env_audio();
    t_fm6();
    t_cost();
    t_lfo();
    t_lfo_audio();
    t_on_line_sweeps();
    t_on_line_more();
    t_lfo2();
    t_cycle();
    t_filt();
    t_twin();
    t_ins();
    t_mod();
    t_chord();
#ifdef LG_BB
    t_bb();
#endif
    printf("%s\n", bad ? "LIVE GRAPH TEST FAILED" : "live graph test passed");
    return bad != 0;
}
