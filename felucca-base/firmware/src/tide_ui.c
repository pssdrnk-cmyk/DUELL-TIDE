/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): the HOME round's TIDE and TIDE LIST pages (the round, as stock with TIDE added: MORPH, TIDE SET, MODE, TIDE;
 * TIDE SET is the TIDE LIST page under that title, in 1.1's MORPH SET's place: SMART / ARP / PITCH are its ROLL, ARP, PITCH).
 *   JAM       the SOUND and SEQ faders with the glide's path, the 16 steps around the playhead, a status line. K1 GLIDE (twist
 *             left: glide to A, right: to B), K2 MODE (ONCE / LOOP / DRIFT / WNDR / JOURNEY), K3 TIME (bars), K4 WAIT   ... play
 *   TIDE LIST  the settings in a list, as the MENU's: K1 picks the row, OCT+ the next value, OCT- the one before. TARGET (SOUND /
 *             SEQ / BOTH), VARY BARS (4 + 8 / ALL), ROLL (MUSIC / SMART / WILD), ARP, PITCH                    ... set and forget
 * LINK: TARGET BOTH links the two faders: GLIDE moves both, and the MORPH fader moved by hand (the MORPH page, the mod wheel, CC 70:
 * ui_input.c morph_act) moves the SEQ fader with it (tide_link). ALGORITHM is the morph's selector here too: PART 1, BOTH, PART 2
 * (morph_mask). The panel shows both faders with the glide on them, and the selected part's 16 steps as they play: varied ones
 * lit, dropped ones hollow. MODE, TIME, WAIT and the list are kept over a power cycle (tide_settings).
 * Context: main loop. Included at the end of ui_input.c (tide.h H13; every UI source it uses is defined above it), with the
 * main-loop poll (tide_poll) at its end. The settings live in TIDE's own storage object (tide_store.c). */
#include "morph_glide.c"

#define TIDE_G_GLIDE 4u                                  /* ui.mg gestures: 0..3 are MORPH's and GLO's (ui.c: TIDE_MG_N of them) */

static const char *const GL_TARGET_NAME[3] = {"SOUND", "SEQ", "BOTH"};
static const char *const GL_MODE_NAME[5] = {"ONCE", "LOOP", "DRIFT", "WNDR", "JRNY"};

static void tide_wait_text(char *val, uint32_t n, const char **unit)
{
    uint32_t b = GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS];
    if (!b) {
        str_cpy(val, "OFF", n);
        *unit = "";
    } else {
        fmt_int(val, (int32_t)b);
        *unit = b == 1u ? "BAR" : "BARS";
    }
}

/* OCT- + K1 on JAM: home. The glide stops, each part of mask back to its Original sound (its ends rebased, the fader at A: End B
 * a roll again) and pattern (End A the Original, End B a variation) */
static void tide_home(uint32_t mask)
{
    uint32_t k;
    glide_stop();
    for (k = 0; k < NSHOWN; k++) {
        if (!((mask >> k) & 1u))
            continue;
        morph_rebase_part(k);
        seqm_home(k);
    }
}
/* part pi sounds other than its Original: the fader off A, or End A itself moved (a roll or MORPH TO of A) */
static int tide_sound_moved(uint32_t pi)
{
    return pi < NSHOWN && morph_view_on(pi) && (mph[pi].fader || mph[pi].top[0]);
}
/* KEEP's sound half: what part pi sounds like now becomes its Original (then the ends rebased on it, End B a new roll). The values
 * as the fader blends them (morph_view_all); on FM6 the patch as it sounds (morph_tide.c tide_fm6_effective: the blend and a
 * MORPH TO target's structure) with the carriers' envelopes as the engine played them (the AMP ENV knobs' offsets baked in:
 * fm6_eff, fm6_env_apply), then the knobs on the kept patch's numbers (FM6_LOOK_NEW: offset 0), so the envelope heard stays even
 * where moving a knob would clamp at 0 / 127; its SLOT the factory patch it equals or OWN (as fm6_adopt). One locked swap: the
 * morph rests (no list) until the rebase, so no block hears the kept values blended again */
static void tide_keep_sound(uint32_t pi)
{
    int16_t *v = tide_v;                                 /* (morph_tide.c's scratch: free here) */
    uint8_t *fp = tide_fv;
    morph_part_t *mp = &mph[pi];
    uint32_t fm = trk[pi].eng_req == ENGI_FM6 && morph_isfm(mp);
    morph_view_at(pi, v, 1);
    if (fm)
        tide_fm6_effective(pi, fp);
    MORPH_LOCK();
    if (fm) {
        int16_t env[4];
        int32_t eoff[4];
        uint32_t e0 = (uint32_t)v[P_E0], k;
        uint32_t alg = e0 >= 1u && e0 <= 32u ? e0 - 1u : fp[FP_ALG] & 31u;
        uint32_t base_alg = e0 >= 1u && e0 <= 32u ? e0 - 1u : fm6_patch[pi][FP_ALG] & 31u;
        /* Bake AMP ENV before any temporary bridge clones an operator. Other
         * macros remain knobs and must not be applied to the stored patch twice. */
        if (fm6_env_offsets(env, fm6_patch[pi], base_alg, &v[P_ATK], eoff))
            for (k = 0; k < 6u; k++)
                if ((fm6_carriers(alg) >> k) & 1u)
                    fm6_env_apply(fp + k * FP_OP, eoff);
    }
    memcpy(trk[pi].p, v, sizeof trk[pi].p);
    mp->nid = mp->fnid = 0;
    mp->cvalid = 0;
    if (fm) {
        fm6_set_patch(pi, fp);
        fm6_env_look(pi, FM6_LOOK_NEW);
        fm6_own_ok &= (uint8_t)~(1u << pi);
        trk[pi].p[P_E7] = (int16_t)fm6_adopt_slot(fm6_patch[pi], trk[pi].p[P_E7]);
        fm6_slot[pi] = (uint8_t)trk[pi].p[P_E7];
    }
    MORPH_UNLOCK();
    morph_rebase_part(pi);
}
/* KEEP (SEQ TOOLS, G4: ui_tools.c TL_KEEP; plan H14): what part pi plays now becomes its own, one undo for both halves (SAVE held:
 * the pattern and, when the sound was kept, the sound). The pattern half: the SEQ MORPH's variation as it plays (seqm_keep, 1.1's
 * ties); steps it empties lose their locks (tl_unlock). The message; 0 = nothing to keep (said) */
static const char *tide_tl_keep(track_t *t, uint64_t was, uint32_t len)
{
    uint32_t pi = trk_index(t);
    int sq = pi < NSHOWN && seqm_active(pi), snd = tide_sound_moved(pi);
    if (!sq && !snd) {
        ui_message("NOTHING TO KEEP");
        return 0;
    }
    tl_undo_take(t);
    if (snd)
        undo.what |= UNDO_SOUND;
    if (sq) {
        seqm_keep(t);
        tl_unlock(t, was, len);
    }
    if (snd)
        tide_keep_sound(pi);
    return sq && snd ? "KEPT" : sq ? "SEQ KEPT" : "SOUND KEPT";
}

/* LINK (TARGET BOTH): the MORPH fader moved by hand to v moves the SEQ faders of mask to v too (ui_input.c morph_act) */
static void tide_link(uint32_t mask, uint32_t v)
{
    uint32_t k;
    if (gl.target != GT_BOTH)
        return;
    for (k = 0; k < NSHOWN; k++)
        if ((mask >> k) & 1u)
            seqm_set_fader(k, v);
}

/* ---------------------------------------------------------------- TIDE LIST --- */
enum { JL_TARGET, JL_VARY, JL_ROLL, JL_ARP, JL_PITCH, JL_WASH, JL_TURNS, JL_N };
static const char *const JL_NAME[JL_N] = {"TARGET", "VARY BARS", "ROLL", "ARP", "PITCH", "WASH LEVEL", "TURNS"};
static uint8_t jl_sel;                                   /* the row picked (K1) */
static void morph_act(uint32_t kind, uint32_t mask, uint32_t v, uint32_t pi);   /* (ui_input.c) */

static uint32_t jl_n(uint32_t r) { return r == JL_WASH ? 4u : r == JL_TARGET || r == JL_ROLL ? 3u : 2u; }
static uint32_t jl_get(uint32_t r)
{
    switch (r) {
    case JL_TARGET: return gl.target;
    case JL_VARY: return sqm_vary_all;
    case JL_ROLL: return morph.cfg & MC_WILD ? 2u : morph_musical ? 0u : 1u;
    case JL_ARP: return (morph.cfg & MC_ARP) != 0u;
    case JL_WASH: return morph_wlvl & 3u;
    case JL_TURNS: return sqm_turns;
    default: return (morph.cfg & MC_PIT) != 0u;
    }
}
static const char *jl_vname(uint32_t r, uint32_t v)
{
    static const char *const VARY[2] = {"4 + 8", "ALL"}, *const ROLL[3] = {"MUSIC", "SMART", "WILD"}, *const SW[2] = {"OFF", "ON"},
                             *const WL[4] = {"OFF", "LOW", "MID", "HIGH"};
    return r == JL_TARGET ? GL_TARGET_NAME[v % 3u] : r == JL_VARY ? VARY[v & 1u] : r == JL_ROLL ? ROLL[v % 3u] : r == JL_WASH ? WL[v & 3u] : SW[v & 1u];
}
static void jl_put(uint32_t r, uint32_t v)
{
    uint32_t pi = song.sel, mask = morph_mask(pi);
    switch (r) {
    case JL_TARGET:
        gl.target = (uint8_t)v;
        ui_message(v == GT_BOTH ? "GLIDE SOUND + SEQ" : v == GT_SEQ ? "GLIDE SEQ" : "GLIDE SOUND");
        break;
    case JL_VARY:
        sqm_vary_all = (uint8_t)v;
        ui_message(v ? "VARY EVERY BAR" : "VARY BARS 4 + 8");
        break;
    case JL_ROLL:                                        /* (MUSIC and SMART are SMART's tags with or without the dials; WILD drops them) */
        morph_musical = (uint8_t)(v == 0u);
        if ((v == 2u) != ((morph.cfg & MC_WILD) != 0u))
            morph_act(MQ_SMART, mask, v != 2u, pi);
        ui_message(v == 0u ? "MUSIC" : v == 1u ? "SMART" : "WILD");
        break;
    case JL_ARP:
        morph_act(MQ_ARP, mask, v, pi);
        break;
    case JL_WASH:                                        /* (morph_xfer.c: GLIDE's breath, the swell and the wash, together) */
        morph_wlvl = (uint8_t)(v & 3u);
        ui_message(v == 0u ? "WASH OFF" : v == 1u ? "WASH LOW" : v == 2u ? "WASH MID" : "WASH HIGH");
        break;
    case JL_TURNS:
        sqm_turns = (uint8_t)(v & 1u);
        ui_message(v ? "PARTS TAKE TURNS" : "PARTS VARY TOGETHER");
        break;
    default:
        morph_act(MQ_PITCH, mask, v, pi);
        break;
    }
}
/* what the TIDE LIST draws (ui_graph.c graph_signature): the rows' values, the row picked, the parts' colours */
static uint32_t tide_list_sig(void)
{
    uint32_t r, h = (uint32_t)jl_sel * 2654435761u ^ (uint32_t)T_PTHEME(0) * 0x85EBCA77u ^ (uint32_t)T_PTHEME(1) * 0xC2B2AE3Du;
    for (r = 0; r < JL_N; r++)
        h = (h ^ jl_get(r)) * 16777619u;
    return h;
}
/* the page's OCT- / OCT+ (ui_input.c): the row's value one back / on, stopping at the ends, as in the MENU */
static int tide_list_on(void) { return !ui.perf && !ui.layer && cur_page()->graph == GR_TIDELIST; }
static void tide_list_oct(uint32_t oct)
{
    uint32_t r = jl_sel % JL_N, v = jl_get(r);
    if ((oct & 2u) && v + 1u < jl_n(r))
        jl_put(r, v + 1u);
    else if ((oct & 1u) && v)
        jl_put(r, v - 1u);
}

/* ---------------------------------------------------------------- the knobs --- */
static void tide_knob(uint32_t slot, int32_t s)
{
    uint32_t pi = song.sel, mask = morph_mask(pi);
    int32_t d;
    if (cur_page()->graph == GR_TIDELIST) {               /* K1 the row; K2..K4 nothing (no card) */
        if (slot == 0u)
            jl_sel = (uint8_t)clamp((int32_t)jl_sel + s, 0, JL_N - 1);
        return;
    }
    switch (slot) {
    case 0:                                              /* (gliding, a twist stops it where it is: PLAY goes on; the next one launches) */
        if ((d = gesture_det(TIDE_G_GLIDE, s, 1)) != 0 && glide_running()) {
            glide_stop();
            ui_message("GLIDE STOPPED");
        } else if (d) {
            glide_launch(d > 0, mask, pi);
            ui_message(d > 0 ? "GLIDE TO B" : "GLIDE TO A");
        }
        break;
    case 1:
        gl.mode = (uint8_t)clamp((int32_t)gl.mode + s, GM_ONCE, GM_JOURNEY);
        break;
    case 2:
        gl.time = (uint8_t)clamp((int32_t)gl.time + s, 0, (int32_t)GLIDE_TIMES - 1);
        break;
    default:
        gl.wait = (uint8_t)clamp((int32_t)gl.wait + (s > 0 ? 1 : -1), 0, (int32_t)GLIDE_WAITS - 1);
        break;
    }
}

/* MIDI (midi_control.c midi_jam, through the morph queue): GLIDE to end v, the SEQ fader to v, on the parts of mask */
static void tide_midi(uint32_t kind, uint32_t mask, uint32_t v, uint32_t pi)
{
    uint32_t k;
    if (kind == MQ_GLIDE) {
        glide_start(v, mask, pi < NSHOWN ? pi : song.sel);
        ui_message(v ? "GLIDE TO B" : "GLIDE TO A");
    } else if (kind == MQ_SEQF) {
        for (k = 0; k < NSHOWN; k++)
            if ((mask >> k) & 1u)
                seqm_set_fader(k, v);
    }
}

/* OCT- held + a knob on JAM: K1 home (the Original sound and pattern), K3 TIME 8 bars, K4 WAIT off; MODE and the list have none */
static int tide_reset(uint32_t k)
{
    if (cur_page()->graph == GR_TIDELIST)
        return 0;
    if (k == 0u) {
        tide_home(morph_mask(song.sel));
        ui_message("HOME");
        return 1;
    }
    if (k == 2u) {
        gl.time = 3u;
        return reset_say("TIME");
    }
    if (k == 3u) {
        gl.wait = 0;
        return reset_say("WAIT");
    }
    return 0;
}

/* JOURNEY (glide_arrive, glide_start): end x of the parts of mask the next target. The sound: a factory sound of the part's engine
 * and the Original's category, not the one just reached nor (when there is another) the one left, and never one the other part is
 * on or heading to (its Original and both its ends, when it plays the same engine); a category of one: any of the engine's;
 * nothing left, or an engine without a list: a roll. The pattern: a WNDR generation, each part from its own seed, so the two
 * parts never play the same (seq_morph.c) */
static int tide_taken(uint32_t k, uint32_t e, uint32_t j)   /* 1: the other part than k is on or heading to sound j of engine e */
{
    uint32_t o = k ? 0u : 1u;
    const track_t *t = &trk[o];
    if (NSHOWN < 2u || t->eng_req != e)
        return 0;
    return (!t->user && t->preset == j) || morph_to_of(o, 0u) == (int32_t)j || morph_to_of(o, 1u) == (int32_t)j;
}
static void tide_journey(uint32_t mask, uint32_t x, uint32_t snd, uint32_t seq)
{
    uint32_t k, j, n, c, pass, pick[64], r = morph_new_seed();
    for (k = 0; k < NSHOWN; k++) {
        const track_t *t = &trk[k];
        if (!((mask >> k) & 1u))
            continue;
        if (snd) {
            uint32_t e = t->eng_req, cat = t->user ? 0u : ds_cat_of(e, t->preset);
            int32_t at = morph_to_of(k, x ^ 1u), was = morph_to_of(k, x);
            if (at < 0 && !t->user)                      /* (the end reached is the Original or a roll: not the Original's own sound) */
                at = (int32_t)t->preset;
            for (pass = n = 0; pass < 3u && !n; pass++)   /* (0: the kind, neither end's; 1: the one left allowed; 2: any kind) */
                for (j = 0; j < ds_count(e) && n < 64u; j++)
                    if ((t->user || pass == 2u || ds_cat_of(e, j) == cat) && (int32_t)j != at && (pass || (int32_t)j != was) &&
                        !tide_taken(k, e, j))
                        pick[n++] = j;
            c = n ? pick[(r >> 8) % n] : 0u;
            if (n > 1u)
                c = sound_complement_pick(k, e, c, pick[(r >> 16) % n], r >> 4);
            if (!n || !morph_to_sound(k, x, c))
                gl_sound_roll(k, 1u << k, x);
            if (morph_isfm(morph_ready(k))) {
                /* The scale checked the notes before the FM6 patch transpose.
                 * JRNY changes colour; keep the part's reference key. */
                MORPH_LOCK();
                morph_fmto[k][x & 1u][FP_TRNSP] = fm6_patch[k][FP_TRNSP];
                MORPH_UNLOCK();
            }
            r = r * 1103515245u + 12345u;
        }
        if (seq)
            seqm_wander_gen(k, x, (uint32_t)mm_tier);
    }
}

/* ---------------------------------------------------------------- the footer --- */
/* the sound part pi plays now by name (b holds 20): the end its fader is nearer, the factory sound MORPH TO or JRNY put there
 * by its name; else (the Original, or a roll of it) the part's own label, "#24 REESE BASS" */
static void tide_part_name(uint32_t pi, char *b)
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    const track_t *t = &trk[pi % NSHOWN];
    int32_t k = mp->n[0] && !morph_stale(t, mp) ? morph_to_of(pi, mp->fader >= 64u) : -1;
    if (k >= 0)
        str_cpy(b, ds_name(t->eng_req % NENGINES, (uint32_t)k), 16);
    else
        sound_label(t, b);
}
/* HOME's four pages (JAM, TIDE LIST, MODE, MORPH) show both parts in the footer, part 1 in row 1, part 2 in row 2 (ui_draw.c draw_foot), each as the sound it
 * plays, so a JRNY's move to another sound shows. 0: not these pages */
static int tide_foot_parts(void)
{
    uint32_t g = cur_page()->graph;
    return NSHOWN >= 2u && !ui.perf && !ui.layer && !ui.ml.on && (g == GR_TIDE || g == GR_TIDELIST || g == GR_MODE || g == GR_MORPH);
}
/* .. TIDE and TIDE SET, whose footer has its two rows (MORPH's and MODE's cards take row 1: there row 2 names the selected part):
 * part 1 on row 1, part 2 on row 2, each its engine in its colour and the sound it plays (owner, 2026-11-01: as JAM did) */
static int tide_foot_two(void)
{
    uint32_t g = cur_page()->graph;
    return tide_foot_parts() && !ui.menu && !ui.guide && !ui.confirm && (g == GR_TIDE || g == GR_TIDELIST);
}

/* ---------------------------------------------------------------- the cards --- */
/* template 9 (owner, 2026-11-01): knob 1 is the page's LINE, knobs 2 .. 4 the card row. TIDE: GLIDE's line says what the glide does
 * (the status line's words: "WNDR TO B  3/8", "GLIDE ON THE NEXT BAR"; at rest the gesture, A<>B, and at the right the part(s) it
 * moves); MODE, TIME, WAIT the cards. TIDE SET: knob 1 the row picked (SETTING), no cards */
static void tide_status(char *l);
static void tide_columns(void)
{
    static char st[40];
    char val[12];
    const char *unit;
    int32_t g;
    if (cur_page()->graph == GR_TIDELIST) {
        draw_column(0, "SETTING", JL_NAME[jl_sel % JL_N], "", VAL(0u), -1, ICON_NONE);
        return;                                          /* (no cards: the list and its hint own the card row; hiding them cleared it under the list on every redraw, a flicker while the colour follows a glide) */
    }
    g = morph_pending(TIDE_G_GLIDE);
    tide_status(st);
    if (glide_running()) {
        draw_column(0, "GLIDE", st, "", T_ACCENT, -1, ICON_NONE);
    } else {
        str_cpy(val, g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", sizeof val);
        k1_after_next = st;                              /* (PART 1, PART 2, BOTH PARTS at the line's right end) */
        k1_after_col = T_MID;
        k1_after_r = 1;
        draw_column(0, "GLIDE", val, "", g ? T_ACCENT : VAL(0u), -1, ICON_NONE);
    }
    draw_column(1, "MODE", GL_MODE_NAME[gl.mode % 5u], "", VAL(1u), -1, ICON_NONE);
    fmt_int(val, (int32_t)glide_bars());
    draw_column(2, "TIME", val, glide_bars() == 1u ? "BAR" : "BARS", VAL(2u), -1, ICON_NONE);
    tide_wait_text(val, sizeof val, &unit);
    draw_column(3, "WAIT", val, unit, VAL(3u), -1, ICON_NONE);
}

/* ---------------------------------------------------------------- the panels --- */
static int tide_both(void) { return morph.target == MT_BOTH; }   /* ALGORITHM on BOTH: the JAM page shows both parts */

/* JAM's status line: what the glide does now, with its bar ("WNDR TO B  3/8", "WAITING  1/4"); stopped, the part(s) */
static void tide_status(char *l)
{
    static const char *const ST[5] = {"GLIDING", "LOOPING", "DRIFTING", "WNDR", "JRNY"};
    uint32_t bars = gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_bars(), at = ((gl.prog * bars) >> GLIDE_Q) + 1u;
    char n[8];
    if (!glide_running())
        str_cpy(l, tide_both() ? "BOTH PARTS" : song.sel ? "PART 2" : "PART 1", 40);
    else if (gl.armed)
        str_cpy(l, song.playing || transport_req == 1u ? "GLIDE ON THE NEXT BAR" : "GLIDE STARTS WITH PLAY", 40);
    else if (!song.playing)
        str_cpy(l, "PAUSED: PLAY GOES ON", 40);
    else {
        str_cpy(l, gl.hold ? "WAITING" : ST[gl.mode % 5u], 40);
        if (!gl.hold)
            str_cpy(l + str_len(l), gl.to ? " TO B" : " TO A", 8);
        str_cpy(l + str_len(l), "  ", 4);
        fmt_int(n, (int32_t)(at > bars ? bars : at));
        str_cpy(l + str_len(l), n, 8);
        str_cpy(l + str_len(l), "/", 4);
        fmt_int(n, (int32_t)bars);
        str_cpy(l + str_len(l), n, 8);
    }
}

/* the body, screen rows TB_Y0 .. TB_Y1 - 1 between the knob-1 line and the card row (lifted TIDE_LIFT), one canvas */
#define TB_Y0 UT_A_GR_Y
#define TB_Y1 (UT_A_CARD_Y - TIDE_LIFT)
#define JP_X0 82                                         /* the faders' tracks: x JP_X0 .. JP_X1 */
#define JP_X1 222
static int32_t tide_x(uint32_t f) { return JP_X0 + (int32_t)f * (JP_X1 - JP_X0) / 127; }

/* a fader's row on screen row y (its track's middle): its name (S, MID; BOTH: the part's number in its colour before it, as its knob and steps, the name on the
 * first row of a pair only), the track (a 3 px rounded bar, as the cards' bars), the glide's way on it (from where it started to the end
 * it goes to, the tint), A and B at its ends, the knob (the part's colour). part: -1 the selected part's row; 0 / 1 that part's row in BOTH */
static void tide_track(int32_t y, const char *name, uint32_t f, uint32_t lane, int32_t part)
{
    uint16_t kc = part < 0 ? T_THEME : T_PTHEME((uint32_t)part);
    int32_t ty = UT_TY(S, y + AF_S_CAP_H / 2), x = tide_x(f);
    if (part >= 0)
        cv_text_on(UT_A_K1_DOT_X, ty, &AF_S, part ? "2" : "1", kc, T_BG);
    if (name[0])
        cv_text_on(part < 0 ? UT_A_K1_DOT_X : 20, ty, &AF_S, name, T_MID, T_BG);
    cv_text_on(JP_X0 - 12, ty, &AF_S, "A", T_DIM, T_BG);   /* (clear of the knob at A: its 7 px start at JP_X0 - 3) */
    cv_text_on(JP_X1 + 5, ty, &AF_S, "B", T_DIM, T_BG);
    cv_rrect(JP_X0, y - 1, JP_X1 - JP_X0 + 1, 3, 1, T_RAISE, T_BG);
    if (glide_running() && ((gl.on >> lane) & 1u) && (part < 0 || ((gl.mask >> part) & 1u))) {
        int32_t a = tide_x(gl.from[lane]), b = tide_x(gl.to ? 127u : 0u);
        cv_rrect(a < b ? a : b, y - 1, (a < b ? b - a : a - b) + 1, 3, 1, T_TINT, T_RAISE);
    }
    if (part < 0)
        cv_rrect(x - 3, y - 6, 7, 13, 2, kc, T_BG);
    else
        cv_rrect(x - 2, y - 4, 5, 9, 2, kc, T_BG);
}

/* a part's 16 steps around its playhead, standing on row y (bars up to hmax tall: 22 alone, 9 in BOTH's two rows), in the footer's
 * columns (bar_x) and its rounded bars: a hit a bar (a chord taller) in the part's colour, the accent where it varies, a lit outline
 * where it is moved or added, a dim outline where the variation drops it; a tie a short bar, an empty step a stub, the steps past
 * the pattern's end a line; the playing step underlined. As they play now (seq_play_step: with VARY BARS 4 + 8 the variation shows
 * on every 4th bar) */
static void tide_steps(const track_t *t, int32_t y, int32_t hmax, uint16_t c)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), page = song.playing ? t->seq_idx / 16u : 0u, i;
    int32_t h1 = hmax >= 20 ? 10 : 6, hs = hmax >= 20 ? 5 : 3;   /* (a single note, each further note of a chord; one part tall, both parts short) */
    for (i = 0; i < 16u; i++) {
        uint32_t idx = page * 16u + i;
        int32_t x = bar_x(i), h;
        const step_t *o, *v;
        step_t b;
        if (idx >= len) {
            cv_rect(x, y - 1, 9, 1, T_LINE);
            continue;
        }
        o = &t->step[idx];
        v = seq_play_step(t, idx, &b);
        if (v->time == ST_NOTE && v->n) {
            h = h1 + hs * (int32_t)(v->n - 1u);
            h = h > hmax ? hmax : h;
            if (v == o || !memcmp(v, o, sizeof *v))
                cv_rrect(x, y - h, 9, h, 2, c, T_BG);
            else if (o->time == ST_NOTE && o->n)
                cv_rrect(x, y - h, 9, h, 2, T_ACCENT, T_BG);
            else
                cv_frame(x, y - h, 9, h, T_ACCENT);
        } else if (v->time == ST_TIE) {
            cv_rrect(x, y - h1 / 2, 9, h1 / 2, 1, T_MID, T_BG);
        } else if (o->time == ST_NOTE && o->n) {
            cv_frame(x, y - h1, 9, h1, T_DIM);
        } else {
            cv_rrect(x, y - 3, 9, 3, 1, T_DIM, T_BG);
        }
        if (song.playing && idx == t->seq_idx)
            cv_rect(x, y + 2, 9, 2, T_TEXT);
    }
}

/* TIDE's body. One part (ALGORITHM on PART 1 / PART 2): its SOUND and SEQ faders, its steps. BOTH: each fader row a pair (part 1 over
 * part 2, their numbers and colours) and the steps in two rows, part 1 above. The glide's status is knob 1's line (tide_columns).
 * BOTH's rows (capitals, bars, playhead marks): faders 49 .. 96, the MODE line 99 .. 109, steps 111 .. 123 and 125 .. 137, the glide's
 * bar 139 .. 141: nothing touches. The steps and the part numbers take the part's own colour (T_PTHEME, as the knobs and the MORPH
 * page's curves), not vt's text fallback, which turned one part black in the light palettes */
/* what each MODE does, one short line under the faders (owner, 2026-11-01) */
static const char *const GL_MODE_WHAT[5] = {"ONE TRIP, THEN IT STOPS", "BACK AND FORTH, A AND B", "NEW TWISTS ON YOUR SOUND",
                                            "WALKS AWAY FROM YOUR SOUND", "TRAVELS THROUGH YOUR SOUNDS"};
static void graph_tide(void)
{
    uint32_t pi = song.sel, k;
    cv_begin(240, TB_Y1 - TB_Y0, T_BG);
    cv_bg = T_BG;
    cv_oy = -TB_Y0;
    if (tide_both()) {
        for (k = 0; k < NSHOWN; k++) {
            tide_track(54 + 12 * (int32_t)k, k ? "" : "SOUND", morph_fader_of(k), GT_SOUND, (int32_t)k);
            tide_track(79 + 12 * (int32_t)k, k ? "" : "SEQ", seqm_fader_of(k), GT_SEQ, (int32_t)k);
            tide_steps(&trk[k], 120 + 14 * (int32_t)k, 9, T_PTHEME(k));
        }
    } else {
        tide_track(62, "SOUND", morph_fader_of(pi), GT_SOUND, -1);
        tide_track(84, "SEQ", seqm_fader_of(pi), GT_SEQ, -1);
        tide_steps(TSEL, 135, 22, T_THEME);
    }
    cv_text_in(0, UT_TY(S, (tide_both() ? 104 : 102) + AF_S_CAP_H / 2), 240, &AF_S, GL_MODE_WHAT[gl.mode % 5u], T_MID, T_BG);   /* (between the faders and the steps' tallest bars) */
    /* Keep GLIDE's progress bar as well as its status and moving fader knobs.
     * This strip sits below both parts' playheads and above the card row. */
    if (glide_running() && !gl.armed) {
        int32_t w = (int32_t)((222u * gl.prog) >> GLIDE_Q);
        cv_rrect(9, TB_Y1 - 3, 222, 3, 1, T_RAISE, T_BG);
        if (w > 0)
            cv_rrect(9, TB_Y1 - 3, w, 3, 1, T_ACCENT, T_RAISE);
    }
    cv_oy = 0;
    cv_blit(0, TB_Y0);
}
/* TIDE SET: the settings as the list pages' rows (PATCHES, template 9: UT_L5_*), from row UT_L5_Y0, 20 apart: the name (M) at the left,
 * the value (M, the part's colour; a switch for ON / OFF) at the right, the picked row filled in the part's colour with dark text.
 * Knob 1 picks the row (knob 1's line names it), OCT+ the next value, OCT- the one before */
#define JL_VIS 6u                                        /* rows shown: the list scrolls to keep the picked one in view */
static void graph_tidelist(void)
{
    uint32_t r, top = jl_sel % JL_N >= JL_VIS ? jl_sel % JL_N - (JL_VIS - 1u) : 0u;
    int32_t y0 = UT_L5_Y0 - 2;
    cv_begin(240, (uint32_t)(UT_L5_PITCH * (int32_t)JL_VIS + 2), T_BG);
    cv_bg = T_BG;
    cv_oy = -y0;
    for (r = top; r < JL_N && r < top + JL_VIS; r++) {
        int32_t y = UT_L5_Y0 + (int32_t)(r - top) * UT_L5_PITCH, ty = UT_TY(M, y + UT_L5_TEXT_BASE), xr = UT_L5_X + UT_L5_W - 8;
        int sel = r == jl_sel % JL_N;
        uint16_t bg = sel ? T_THEME : T_BG;
        const char *v = jl_vname(r, jl_get(r));
        if (sel) {
            CV_HL(UT_L5_X, y, UT_L5_W, UT_L5_H);          /* (the picked row's pill: an allowed highlight, as the other lists') */
            cv_rrect(UT_L5_X, y, UT_L5_W, UT_L5_H, UT_T3_ROW_R, T_THEME, T_BG);
        }
        cv_text_on(UT_L5_TAG_X, ty, &AF_M, JL_NAME[r], sel ? T_INK : T_TEXT, bg);
        if (jl_n(r) == 2u && (str_eq(v, "ON") || str_eq(v, "OFF"))) {   /* a switch (the MENU's, in the row's colours) */
            int on = str_eq(v, "ON");
            int32_t x = xr - 26, sy = y + HALF_UP(UT_L5_H - 14);
            uint16_t track = on ? (sel ? T_INK : T_THEME) : T_DIM, knob = on ? (sel ? T_THEME : T_INK) : T_TEXT;
            cv_rrect(x, sy, 26, 14, -7, track, bg);
            cv_rrect(on ? x + 14 : x + 2, sy + 2, 10, 10, -5, knob, track);
        } else
            cv_text_r(xr, ty, &AF_M, v, sel ? T_INK : T_THEME, bg);
    }
    cv_oy = 0;
    cv_blit(0, (uint32_t)y0);
    {   /* how a value changes (owner, 2026-11-01): the keycaps under the list, S, the hints' colour (the list's own strip: the canvas holds 124 rows) */
        int32_t hy = UT_L5_Y0 + UT_L5_PITCH * (int32_t)JL_VIS + 4, x;
        cv_begin(240, 20, T_BG);
        cv_bg = T_BG;
        cv_oy = -hy;
        x = cv_free_hint(UT_L5_TAG_X, UT_TY(S, hy + 14), "[OCT-]", T_MID, T_BG, 60);
        cv_free_hint(x + 4, UT_TY(S, hy + 14), "[OCT+] CHANGE THE VALUE", T_MID, T_BG, 222 - x);
        cv_oy = 0;
        cv_blit(0, (uint32_t)hy);
    }
}

/* STEP page (ui_graph.c graph_roll): while the part's SEQ fader is above A, each step SEQ MORPH plays otherwise than it is stored shows
 * the notes it plays as outlines in its column, and a stored hit it drops is hollowed. The variation as it plays on a varied bar
 * (seq_morph.c sqm_step) */
static void tide_roll_ghosts(const track_t *t, uint32_t base, uint32_t ncol)
{
    uint32_t i, j, pi = (uint32_t)(t - trk);
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    if (pi >= NSHOWN || !seqm_active(pi))
        return;
    for (i = 0; i < ncol; i++) {
        const step_t *st = &t->step[base + i], *v = sqm_step(t, base + i, 1u);
        int32_t x = PR_X0 + (int32_t)i * PR_CW, y;
        if (v == st || !memcmp(v, st, sizeof *st))
            continue;
        if (sqm_hit(st) && !sqm_hit(v))
            for (j = 0; j < st->n && j < 4u; j++)
                if ((y = pr_row_y(st->note[j])) >= PR_Y0 && y < ybot)
                    cv_rect(x + 4, y + 2, 5, 1, T_BG);
        if (sqm_hit(v))
            for (j = 0; j < v->n && j < 4u; j++)
                if ((y = pr_row_y(v->note[j])) >= PR_Y0 && y < ybot)
                    cv_frame(x + 1, y, PR_CW - 2, PR_RH, T_ACCENT);
    }
}
/* .. what changes them: the fader, End B (its seed) and the amounts */
static uint32_t tide_roll_sig(const track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk);
    if (pi >= NSHOWN)
        return 0;
    return (seqm_fader_of(pi) | seqm_notes_of(pi) << 8) ^ sqm_key(&sqm[pi]) * 2654435761u;
}

/* what makes the JAM pages redraw: the faders, the glide, the steps playing and their variation, the settings, the list */
static uint32_t tide_sig(void)
{
    uint32_t pi = song.sel, bars = gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_bars();
    const track_t *t = TSEL;
    return (morph_fader_of(pi) | seqm_fader_of(pi) << 7 | (uint32_t)gl.on << 14 | (uint32_t)gl.armed << 16 | (uint32_t)gl.target << 17 |
            (uint32_t)gl.mode << 19 | (uint32_t)gl.to << 22 | (uint32_t)gl.hold << 23 | (song.playing ? t->seq_idx : 0xFFu) << 24) ^
           (gl.prog >> 10) * 2654435761u ^ ((gl.prog * bars) >> GLIDE_Q) * 0x27D4EB2Fu ^ sqm_key(&sqm[pi < NSHOWN ? pi : 0u]) * 40503u ^
           ((uint32_t)gl.time | (uint32_t)gl.wait << 4 | (uint32_t)jl_sel << 8 | (uint32_t)sqm_vary_all << 12 | (uint32_t)morph_musical << 13 | (uint32_t)sqm_turns << 27 |
            (uint32_t)morph.cfg << 16 | (uint32_t)((sqm_bar + sqm_bar_turn) & 3u) << 24 | (uint32_t)song.playing << 26) * 0x9E3779B1u ^
           (tide_both() ? (morph_fader_of(1u - pi) | seqm_fader_of(1u - pi) << 7 | (song.playing ? trk[1u - pi].seq_idx : 0xFFu) << 14) * 0x85EBCA77u ^
                             sqm_key(&sqm[(1u - pi) % NSHOWN]) * 0xC2B2AE3Du ^ 0x27D4EB2Fu : 0u);
}

/* ---------------------------------------------------------------- kept over a power cycle --- */
/* MODE, TIME, WAIT, TARGET, VARY BARS, the ROLL's MUSIC and WASH LEVEL in TIDE's own record (tide_store.c: OBJ_TIDE, patch chunk 21's
 * sector pair; plan §4.5), not in the settings record (1.1 uses the bytes JAM had there). ARP, PITCH and WILD are the morph's own byte
 * (ui.c ui_morph_cfg). The record is read once at power-on (none or damaged: the defaults; neither is written back); a change is saved ~2 s after the last one,
 * while stopped and not during ERASE DATA (tide_store_poll) */
enum { TS_MODE, TS_TIME, TS_TARGET, TS_WAIT, TS_VARY, TS_MUSIC, TS_WASH, TS_OLD, TS_TURNS = TS_OLD, TS_N };   /* (TS_OLD: the fields of the first record; a record of that length loads, the later fields at their defaults) */
static uint8_t tide_seen[TS_N], tide_seen_ok;
static void tide_settings(void)
{
    uint8_t w[TS_N];
    if (!tide_seen_ok) {
        uint8_t b[TS_N] = {[TS_TURNS] = 1u};   /* (the defaults of the fields an older record lacks) */
        uint32_t have = tide_store_load_min(b, TS_N, TS_OLD);
        if (have) {
            gl.mode = (uint8_t)(b[TS_MODE] <= GM_JOURNEY ? b[TS_MODE] : GM_ONCE);
            gl.time = (uint8_t)(b[TS_TIME] < GLIDE_TIMES ? b[TS_TIME] : 3u);
            gl.target = (uint8_t)(b[TS_TARGET] <= GT_BOTH ? b[TS_TARGET] : GT_SOUND);
            gl.wait = (uint8_t)(b[TS_WAIT] < GLIDE_WAITS ? b[TS_WAIT] : 0u);
            sqm_vary_all = (uint8_t)(b[TS_VARY] & 1u);
            morph_musical = (uint8_t)(b[TS_MUSIC] & 1u);
            morph_wlvl = (uint8_t)(b[TS_WASH] & 3u);
            sqm_turns = (uint8_t)(b[TS_TURNS] & 1u);
        }
    }
    w[TS_MODE] = gl.mode;
    w[TS_TIME] = gl.time;
    w[TS_TARGET] = gl.target;
    w[TS_WAIT] = gl.wait;
    w[TS_VARY] = sqm_vary_all;
    w[TS_MUSIC] = morph_musical;
    w[TS_WASH] = (uint8_t)(morph_wlvl & 3u);
    w[TS_TURNS] = sqm_turns;
    if (!tide_seen_ok) {                                 /* (what was read, or the defaults: not a change, nothing to write at every power-on) */
        memcpy(tide_seen, w, TS_N);
        tide_seen_ok = 1;
    } else if (memcmp(w, tide_seen, TS_N)) {
        memcpy(tide_seen, w, TS_N);
        tide_store_mark(w, TS_N);
    }
}

/* ---------------------------------------------------------------- the main loop (tide.h H13) --- */
static void tide_poll(void)
{
    tide_settings();
    glide_poll();                                        /* a launched GLIDE moves its faders */
    seqm_poll();                                         /* the SEQ MORPH's ends follow pattern edits (TRANS, SHIFT, an edit) */
    morph_xpoll();                                       /* plan any destination GLIDE generated in this same pass */
    tide_store_poll();
}
