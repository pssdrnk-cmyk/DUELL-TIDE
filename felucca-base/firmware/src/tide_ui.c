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

/* JOURNEY's memory (tide_journey): the last JH_N sounds each part was sent to, newest first, as sound + 1 of engine jh_eng (0:
 * none). RAM only, never saved; cleared when a WNDR/JRNY session starts (glide_start) and on HOME */
#define JH_N 4u
static uint8_t jh[2][JH_N], jh_eng[2];
static void jh_clear(uint32_t k) { jh[k & 1u][0] = jh[k & 1u][1] = jh[k & 1u][2] = jh[k & 1u][3] = 0; }

/* OCT- + K1 on JAM: home. The glide stops, each part of mask back to its Original sound (its ends rebased, the fader at A: End B
 * a roll again) and pattern (End A the Original, End B a variation) */
static void tide_home(uint32_t mask)
{
    uint32_t k;
    glide_stop();
    for (k = 0; k < NSHOWN; k++) {
        if (!((mask >> k) & 1u))
            continue;
        jh_clear(k);
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
enum { JL_STYLE, JL_TARGET, JL_VARY, JL_ROLL, JL_ARP, JL_PITCH, JL_WASH, JL_TURNS, JL_POLY, JL_SYNC, JL_ARC, JL_N };
static const char *const JL_NAME[JL_N] = {"STYLE", "TARGET", "VARY BARS", "ROLL", "ARP", "PITCH", "WASH LEVEL", "TURNS", "POLY", "SYNC", "ARC"};
static uint8_t jl_sel;                                   /* the row picked (K1) */
static void morph_act(uint32_t kind, uint32_t mask, uint32_t v, uint32_t pi);   /* (ui_input.c) */

/* STYLE: one move sets the nine rows below it together (TARGET, ROLL, ARP and PITCH choose what TIDE may change, not the genre, and stay). A hand edit
 * of any of the nine turns it into CUSTOM (tide_settings); CUSTOM itself changes nothing. mode, time (index), wait (index), VARY, TURNS, POLY, SYNC, ARC, WASH */
enum { STY_DUB, STY_DEEP, STY_MINIMAL, STY_AMBIENT, STY_PEAK, STY_CUSTOM };
static const uint8_t STY[5][9] = {
    {GM_JOURNEY, 4, 2, 0, 2, 1, 1, 1, 0},                /* DUB: the showcase (today's no-record rows) with LEAD and a RISE */
    {GM_WANDER, 5, 3, 0, 1, 0, 1, 1, 1},                 /* DEEP */
    {GM_WANDER, 3, 0, 1, 2, 2, 2, 0, 0},                 /* MINIMAL */
    {GM_JOURNEY, 6, 4, 0, 0, 1, 0, 0, 3},                /* AMBIENT */
    {GM_JOURNEY, 3, 0, 1, 3, 1, 2, 2, 0}};               /* PEAK: ARC DROP and ECHO */
static uint8_t tide_style = STY_DUB;
static __attribute__((noinline)) void tide_rows_set(const uint8_t *t)
{
    gl.mode = t[0], gl.time = t[1], gl.wait = t[2];
    sqm_vary_all = t[3], sqm_turns = t[4], sqm_poly = t[5], sqm_sync = t[6], sqm_arc = t[7], morph_wlvl = t[8];
}
static void tide_style_apply(uint32_t s)
{
    tide_rows_set(STY[s]);
    tide_style = (uint8_t)s;
}
static __attribute__((noinline)) void tide_rows_get(uint8_t *t)   /* the nine rows as they are now */
{
    t[0] = gl.mode, t[1] = gl.time, t[2] = gl.wait, t[3] = sqm_vary_all, t[4] = sqm_turns, t[5] = sqm_poly, t[6] = sqm_sync, t[7] = sqm_arc, t[8] = (uint8_t)(morph_wlvl & 3u);
}
static int tide_style_holds(void)                        /* the nine rows are the style's */
{
    uint8_t t[9];
    tide_rows_get(t);
    return !memcmp(t, STY[tide_style % 5u], sizeof t);
}
/* CUSTOM's nine values, kept when a style replaces them: the STYLE row back onto CUSTOM brings them back (RAM only, not saved) */
static uint8_t tide_custom[9], tide_custom_ok;
static void tide_custom_take(void)
{
    tide_rows_get(tide_custom);
    tide_custom_ok = 1;
}
/* TURNS, POLY, SYNC and ARC act only in automatic MUSIC WNDR / JRNY with the SEQ gliding (glide_poll: sqm_auto): what row r lacks now (0: it acts) */
static __attribute__((noinline)) const char *jl_needs(uint32_t r)
{
    if (r < JL_TURNS)
        return 0;
    return gl.mode < GM_WANDER ? "WNDR OR JRNY" : !morph_musical ? "ROLL MUSIC" : gl.target == GT_SOUND ? "TARGET SEQ" : 0;   /* (what the row needs, short: knob 1's line has 91 px after "SETTING TURNS") */
}
static uint32_t jl_n(uint32_t r) { return r == JL_WASH ? 4u : r == JL_STYLE ? 6u : r == JL_TURNS ? 4u : r == JL_TARGET || r == JL_ROLL || r == JL_POLY || r == JL_SYNC || r == JL_ARC ? 3u : 2u; }
static uint32_t jl_get(uint32_t r)
{
    switch (r) {
    case JL_TARGET: return gl.target;
    case JL_VARY: return sqm_vary_all;
    case JL_ROLL: return morph.cfg & MC_WILD ? 2u : morph_musical ? 0u : 1u;
    case JL_ARP: return (morph.cfg & MC_ARP) != 0u;
    case JL_WASH: return morph_wlvl & 3u;
    case JL_TURNS: return sqm_turns;
    case JL_POLY: return sqm_poly;
    case JL_SYNC: return sqm_sync;
    case JL_ARC: return sqm_arc;
    case JL_STYLE: return tide_style;
    default: return (morph.cfg & MC_PIT) != 0u;
    }
}
static const char *jl_vname(uint32_t r, uint32_t v)
{
    static const char *const VARY[2] = {"4 + 8", "ALL"}, *const ROLL[3] = {"MUSIC", "SMART", "WILD"}, *const SW[2] = {"OFF", "ON"},
                             *const WL[4] = {"OFF", "LOW", "MID", "HIGH"},
                             *const TURNS[4] = {"TOGETHER", "TAKE TURNS", "LEAD", "ECHO"}, *const POLY[3] = {"OFF", "GENTLE", "WIDE"}, *const SYNC[3] = {"OFF", "SOFT", "PUSH"}, *const ARC[3] = {"OFF", "RISE", "DROP"},
                             *const STYLE[6] = {"DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK", "CUSTOM"};
    return r == JL_TURNS ? TURNS[v & 3u] : r == JL_POLY ? POLY[v % 3u] : r == JL_SYNC ? SYNC[v % 3u] : r == JL_ARC ? ARC[v % 3u] : r == JL_STYLE ? STYLE[v % 6u] : r == JL_TARGET ? GL_TARGET_NAME[v % 3u] : r == JL_VARY ? VARY[v & 1u] : r == JL_ROLL ? ROLL[v % 3u] : r == JL_WASH ? WL[v & 3u] : SW[v & 1u];
}
static void jl_put(uint32_t r, uint32_t v)
{
    uint32_t pi = song.sel, mask = morph_mask(pi);
    switch (r) {
    case JL_STYLE:
        if (v < STY_CUSTOM) {
            if (tide_style == STY_CUSTOM || !tide_style_holds())
                tide_custom_take();                      /* (the values being replaced are CUSTOM: kept for the way back) */
            tide_style_apply(v);
        } else {
            if (tide_custom_ok)
                tide_rows_set(tide_custom);
            tide_style = STY_CUSTOM;
        }
        ui_message(v == 0u ? "STYLE DUB" : v == 1u ? "STYLE DEEP" : v == 2u ? "STYLE MINIMAL" : v == 3u ? "STYLE AMBIENT" : v == 4u ? "STYLE PEAK" : "STYLE CUSTOM");
        break;
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
        sqm_turns = (uint8_t)(v & 3u);
        ui_message(v == 0u ? "PARTS VARY TOGETHER" : v == 1u ? "PARTS TAKE TURNS" : v == 2u ? "PARTS LEAD IN TURNS" : "THE ANSWER ECHOES");
        break;
    case JL_POLY:                                        /* (seq_poly.c: one part's loop shorter now and then, back at a phrase) */
        sqm_poly = (uint8_t)(v % 3u);
        ui_message(v == 0u ? "POLY OFF" : v == 1u ? "POLY GENTLE" : "POLY WIDE");
        break;
    case JL_SYNC:                                        /* (seq_rhythm.c: the phrase edits lean to the "and" and the push) */
        sqm_sync = (uint8_t)(v % 3u);
        ui_message(v == 0u ? "SYNC OFF" : v == 1u ? "SYNC SOFT" : "SYNC PUSH");
        break;
    case JL_ARC:                                         /* (seq_arc.c: a build, a peak, an optional drop and a breakdown over 32-64 bars) */
        sqm_arc = (uint8_t)(v % 3u);
        ui_message(v == 0u ? "ARC OFF" : v == 1u ? "ARC RISE" : "ARC DROP");
        break;
    default:
        morph_act(MQ_PITCH, mask, v, pi);
        break;
    }
}
/* what the TIDE LIST draws (ui_graph.c graph_signature): the rows' values, the row picked, the parts' colours */
static uint32_t tide_list_sig(void)
{
    uint32_t r, h = (uint32_t)jl_sel * 2654435761u ^ (uint32_t)T_PTHEME(0) * 0x85EBCA77u ^ (uint32_t)T_PTHEME(1) * 0xC2B2AE3Du ^ (uint32_t)(gl.mode < GM_WANDER) * 40503u;   /* (the rows' values; ROLL and TARGET are among them, MODE decides the inert rows with them) */
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
    if (k == 1u) {                                       /* MODE has no reset: OCT- + K2 drops (ARC) at the next phrase */
        ui_message(tarc_drop_hand());
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
 * and the Original's role family (a USER record's own category), not the one just reached nor (when there is another) the one left,
 * and never one the other part is on or heading to (its Original and both its ends, when it plays the same engine); nothing left
 * (a sound alone in its family too), or an engine without a list: a roll of the sound, which keeps its role. The pattern: a WNDR generation, each part from its own seed, so the two
 * parts never play the same (seq_morph.c) */
static int tide_taken(uint32_t k, uint32_t e, uint32_t j)   /* 1: the other part than k is on or heading to sound j of engine e */
{
    uint32_t o = k ? 0u : 1u;
    const track_t *t = &trk[o];
    if (NSHOWN < 2u || t->eng_req != e)
        return 0;
    return (!UP_REC_USER(t) && t->preset == j) || morph_to_of(o, 0u) == (int32_t)j || morph_to_of(o, 1u) == (int32_t)j;
}
/* JOURNEY roams a role family, not one category: HARMONIC (pads, keys), BASS, LEAD / PLUCK, PERC (hits), FX (textures, drones;
 * OTHER, a user record's catch-all, too: a texture is the quietest place for a sound that says nothing of its role). Every
 * category is mapped here; a new one stops the build until it is placed (a category past the table counts as HARMONIC) */
enum { FAM_HARM, FAM_BASS, FAM_LEAD, FAM_PERC, FAM_TEXT };
static const uint8_t JFAM[CAT_N] = {
    [CAT_NONE] = FAM_HARM, [CAT_BASS] = FAM_BASS, [CAT_LEAD] = FAM_LEAD, [CAT_PAD] = FAM_HARM, [CAT_PLUCK] = FAM_LEAD,
    [CAT_KEYS] = FAM_HARM, [CAT_PERC] = FAM_PERC, [CAT_FX] = FAM_TEXT, [CAT_OTHER] = FAM_TEXT};
typedef char jfam_covers_every_category[CAT_N == 9 ? 1 : -1];
static uint32_t fam_of(uint32_t cat) { return cat < CAT_N ? JFAM[cat] : FAM_HARM; }
/* the candidates pick[0..n) of part k without its last h targets, for the most h that leaves any (the oldest forgiven first: a
 * group of four or fewer sounds still turns); n unchanged where none is in the history or the memory is empty */
static uint32_t jh_filter(uint32_t k, uint32_t e, uint32_t *pick, uint32_t n)
{
    uint32_t h, i, m, j;
    if (jh_eng[k] != e) {                                /* (another engine: the sounds it holds are not these) */
        jh_clear(k);
        jh_eng[k] = (uint8_t)e;
    }
    for (h = JH_N; h; h--) {
        for (i = m = 0; i < n; i++) {
            for (j = 0; j < h && jh[k][j] != pick[i] + 1u; j++)
                ;
            m += j == h;
        }
        if (m)
            break;
    }
    if (!h || m == n)
        return n;
    for (i = m = 0; i < n; i++) {
        for (j = 0; j < h && jh[k][j] != pick[i] + 1u; j++)
            ;
        if (j == h)
            pick[m++] = pick[i];
    }
    return m;
}
static void jh_push(uint32_t k, uint32_t c)
{
    jh[k][3] = jh[k][2], jh[k][2] = jh[k][1], jh[k][1] = jh[k][0], jh[k][0] = (uint8_t)(c + 1u);
}
static void tide_journey(uint32_t mask, uint32_t x, uint32_t snd, uint32_t seq)
{
    uint32_t k, j, n, c, pass, pick[64], r = morph_new_seed();
    for (k = 0; k < NSHOWN; k++) {
        const track_t *t = &trk[k];
        if (!((mask >> k) & 1u))
            continue;
        if (snd) {
            uint32_t e = t->eng_req, fam = fam_of(UP_REC_USER(t) ? sl_cat((uint32_t)t->user - 1u) : ds_cat_of(e, t->preset));
            int32_t at = morph_to_of(k, x ^ 1u), was = morph_to_of(k, x);
            if (at < 0 && !UP_REC_USER(t))                      /* (the end reached is the Original or a roll: not the Original's own sound) */
                at = (int32_t)t->preset;
            for (pass = n = 0; pass < 2u && !n; pass++)   /* (0: the family, neither end's; 1: the one left allowed) */
                for (j = 0; j < ds_count(e) && n < 64u; j++)
                    if (fam_of(ds_cat_of(e, j)) == fam && (int32_t)j != at && (pass || (int32_t)j != was) &&
                        !tide_taken(k, e, j))
                        pick[n++] = j;
            n = jh_filter(k, e, pick, n);
            c = n ? pick[(r >> 8) % n] : 0u;
            if (n > 1u)
                c = sound_complement_pick(k, e, c, pick[(r >> 16) % n], r >> 4);
            if (!n || !morph_to_sound(k, x, c))
                gl_sound_roll(k, 1u << k, x);
            else
                jh_push(k, c);                           /* (only a target really adopted is remembered) */
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
        const char *nd = jl_needs(jl_sel % JL_N);
        if (nd) {                                        /* a row that does nothing in this setup says what it needs (at the line's right end, as the status) */
            k1_after_next = nd;
            k1_after_col = T_MID;
            k1_after_r = 1;
        }
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

/* "a/b" appended to l */
static __attribute__((noinline)) void tide_frac(char *l, uint32_t a, uint32_t b)
{
    l += str_len(l);
    fmt_int(l, (int32_t)a);
    l += str_len(l);
    *l++ = '/';
    fmt_int(l, (int32_t)b);
}
/* JAM's status line: what the glide does now, with its bar ("WNDR TO B  3/8", "WAITING  1/4"); stopped, the part(s) */
static void tide_status(char *l)
{
    static const char *const ST[5] = {"GLIDING", "LOOPING", "DRIFTING", "WNDR", "JRNY"};
    uint32_t bars = gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_trip_bars(), at = ((gl.prog * bars) >> GLIDE_Q) + 1u;
    if (!glide_running())
        str_cpy(l, tide_both() ? "BOTH PARTS" : song.sel ? "PART 2" : "PART 1", 40);
    else if (gl.armed)
        str_cpy(l, song.playing || transport_req == 1u ? "GLIDE ON THE NEXT BAR" : "GLIDE STARTS WITH PLAY", 40);
    else if (!song.playing)
        str_cpy(l, "PAUSED: PLAY GOES ON", 40);
    else if (sqm_arc && sqm_auto && tarc.run) {         /* ARC: where the shape is ("RISE 3/6", "PEAK", "DROP", "BREAK 2/3") */
        str_cpy(l, tarc_name(), 40);
        if (tarc.phase != AP_PEAK && tarc.phase != AP_DROP) {
            str_cpy(l + str_len(l), " ", 4);
            tide_frac(l, tarc.len - tarc.left + 1u, tarc.len);
        }
        if (tarc.hand && tarc.phase != AP_DROP)
            str_cpy(l + str_len(l), "  DROP NEXT", 16);
        else {                                           /* (the trip stays shown, its direction and bar; DROP NEXT takes its room: the line is full with both) */
            str_cpy(l + str_len(l), gl.hold ? "  WAIT " : gl.to ? "  B " : "  A ", 8);
            tide_frac(l, at > bars ? bars : at, bars);
        }
    } else {
        str_cpy(l, gl.hold ? "WAITING" : ST[gl.mode % 5u], 40);
        if (!gl.hold)
            str_cpy(l + str_len(l), gl.to ? " TO B" : " TO A", 8);
        str_cpy(l + str_len(l), "  ", 4);
        tide_frac(l, at > bars ? bars : at, bars);
    }
}

/* the body, screen rows TB_Y0 .. TB_Y1 - 1 between the knob-1 line and the card row (lifted TIDE_LIFT), one canvas */
#define TB_Y0 UT_A_GR_Y
#define TB_Y1 (UT_A_CARD_Y - TIDE_LIFT)
#define JP_X0 82                                         /* one part: the faders' tracks, x JP_X0 .. JP_X1 */
#define JP_X1 218
#define JB_X0 24                                         /* BOTH: the SOUND column's tracks x JB_X0 .. + JB_W, the SEQ column's JB_DX further */
#define JB_W 88
#define JB_DX 116
static int32_t tide_x(int32_t x0, int32_t x1, uint32_t f) { return x0 + (int32_t)f * (x1 - x0) / 127; }

/* fader c (0 SOUND, 1 SEQ) on screen row y (its track's middle), x x0 .. x1: the track (a 3 px rounded bar, as the cards' bars), the glide's way on it (from where it
 * started to the end it goes to, the tint; MONO's white), the knob (the part's colour: 7 x 13 alone, 5 x 9 in BOTH's rows) and its labels. part -1: the selected part's row, its name,
 * A and B on it. part 0 / 1: that part's row in BOTH's column c (part 1's draws the column's head 10 rows up, A, the name centred, B; SOUND's the part's number) */
static void tide_track(int32_t y, int32_t x0, int32_t x1, uint32_t c, int32_t part)
{
    static const char *const LN[2] = {"SOUND", "SEQ"};
    uint32_t pk = part < 0 ? song.sel : (uint32_t)part, f = c ? seqm_fader_of(pk) : morph_fader_of(pk);
    uint16_t kc = part < 0 ? T_THEME : T_PTHEME(pk);
    int32_t x = tide_x(x0, x1, f), ty = UT_TY(S, y + AF_S_CAP_H / 2);
    if (part < 0) {
        cv_text_on(UT_A_K1_DOT_X, ty, &AF_S, LN[c], T_MID, T_BG);
        cv_text_on(x0 - 15, ty, &AF_S, "A", T_DIM, T_BG);   /* (4 px clear of the knob at A: its 7 px start at x0 - 3; B as far from the knob at B) */
        cv_text_on(x1 + 7, ty, &AF_S, "B", T_DIM, T_BG);
    } else {
        if (!part) {
            int32_t hy = UT_TY(S, y - 10);
            cv_text_on(x0, hy, &AF_S, "A", T_DIM, T_BG);
            cv_text_in(x0, hy, x1 - x0, &AF_S, LN[c], T_MID, T_BG);
            cv_text_r(x1 + 1, hy, &AF_S, "B", T_DIM, T_BG);
        }
        if (!c)
            cv_text_on(UT_A_K1_DOT_X, ty, &AF_S, part ? "2" : "1", kc, T_BG);
    }
    cv_rrect(x0, y - 1, x1 - x0 + 1, 3, 1, T_RAISE, T_BG);
    if (glide_running() && ((gl.on >> c) & 1u) && (part < 0 || ((gl.mask >> part) & 1u))) {
        int32_t a = tide_x(x0, x1, gl.from[c]), b = gl.to ? x1 : x0;
        cv_rrect(a < b ? a : b, y - 1, (a < b ? b - a : a - b) + 1, 3, 1, ux.bw ? T_MID : T_TINT, T_RAISE);   /* (MONO has no tint: its black erased the track) */
    }
    if (part < 0)
        cv_rrect(x - 3, y - 6, 7, 13, 2, kc, T_BG);
    else
        cv_rrect(x - 2, y - 4, 5, 9, 2, kc, T_BG);
}

/* The steps as the sequencer plays them: ONE model for the page (tide_steps, tide_sig) and its test (tests/tide_steps_test.c). A page of
 * 16 steps of part t, each as it sounds (seq_play_step_at, the very function the audio interrupt calls) in the bar it plays in: the
 * bar now for the step playing, the next ones for the steps ahead (their time from the playhead's position in the step, each step's
 * swung length, the beat in the bar), the bars behind for the steps gone; not the one bar now for all 16. Also:
 *  - the loop's length now (POLY shortens it: the steps past it do not play) and, on an ARP SEQ part, the arp's step as the playhead
 *    (the arp steps alone, the SEQ-mode steps do not play, a tie is no note, the chance is the arp's PROB);
 *  - a chance step: undecided (maybe) until the sequencer rolled it, then its roll (tide_miss: sounding or dropped); a tie sounds
 *    only if the hit it holds does. The page draws an undecided step striped.
 * Returns the playhead's step (TV_NOPH: stopped). page 0xFF: the playhead's page */
enum { TV_END, TV_REST, TV_DROP, TV_TIE, TV_HIT, TV_MOVE, TV_ADD };
typedef struct { uint8_t k, n, maybe; } tv_t;
#define TV_NOPH 0xFFu
static int tv_hit(const step_t *s) { return s->time == ST_NOTE && s->n; }
/* step s (index j) of part t: 0 silent, 1 sounds, 2 undecided (its roll not made yet) */
static uint32_t tv_state(const track_t *t, const step_t *s, uint32_t j, uint32_t rolled, uint32_t arp)
{
    uint32_t c = arp ? (t->p[P_APROB] >= 127 ? 100u : 50u) : step_chance(s);
    if (c >= 100u)
        return 1;
    if (!c)
        return 0;
    return rolled ? !((tide_miss[trk_index(t)][(j >> 5) & 1u] >> (j & 31u)) & 1u) : 2u;
}
/* the bar a step starts in (tide_views): `at` its start from now in samples (the playhead's position in its step, the steps' lengths); the
 * audio side starts a step in the block it falls in, with that block's bar turn: the bar's line at or before the block's end. ob: into the bar now, bl: its length */
static uint32_t tv_bar(int32_t at, int32_t ob, int32_t bl)
{
    int32_t d = ob + (at >= 0 ? (at + (int32_t)CTL - 1) / (int32_t)CTL : -(-at / (int32_t)CTL)) * (int32_t)CTL;
    return (uint32_t)sqm_bar + (uint32_t)(d >= 0 ? d / bl : -((-d + bl - 1) / bl));
}
static uint32_t tide_views(const track_t *t, uint32_t page, tv_t *out)
{
    uint32_t pi = trk_index(t), len = sqm_len(t), L = len, ph = TV_NOPH, pos = 0, per = 0, i, j, m;
    uint32_t arp = arp_mode(t) == AM_SEQ_ID;
    int32_t ob = 0, bl = 1;
    if (song.playing) {
        L = tide_play_len(t);
        ph = tide_play_idx(t);
        pos = arp ? t->arp_pos : t->seq_pos;
        ph = ph < L ? ph : TV_NOPH;
        pos = pos < 0x7FFFFFFu ? pos : 0u;                   /* (before the first block of a PLAY) */
        per = div_samples((uint32_t)t->p[arp ? P_ARATE : P_SDIV]);
        bl = (int32_t)(4u * beat_samples());
        ob = (int32_t)(beat_n * beat_samples() + beat_pos);  /* (into the bar now) */
    }
    if (page == 0xFFu)
        page = ph == TV_NOPH ? 0u : ph / 16u;
    for (i = 0; i < 16u; i++) {
        uint32_t idx = page * 16u + i, bar = sqm_bar, st = 1;
        int32_t at = -(int32_t)pos;
        const step_t *o = &t->step[idx < NSTEP ? idx : 0u], *v, *w;
        tv_t *e = &out[i];
        e->k = TV_END, e->n = e->maybe = 0;
        if (idx >= L)
            continue;
        if (ph != TV_NOPH) {
            for (j = idx < ph ? idx : ph; j < (idx > ph ? idx : ph); j++)
                at += (idx > ph ? 1 : -1) * (int32_t)phrase_duration(t, j, per, (int)arp, 0);
            bar = tv_bar(at, ob, bl);
        }
        v = seq_play_step_at(t, idx, bar);
        e->n = v->n;
        e->k = tv_hit(v) ? (v == o || !memcmp(v, o, sizeof *v) ? TV_HIT : tv_hit(o) ? TV_MOVE : TV_ADD) : v->time == ST_TIE ? TV_TIE : TV_REST;
        if (e->k >= TV_HIT || e->k == TV_TIE) {
            st = arp && e->k == TV_TIE ? 0u : tv_state(t, v, idx, ph != TV_NOPH && idx <= ph, arp);
            if (e->k == TV_TIE && !arp) {                    /* a tie sounds while the hit before it is held: each step held it through to the next (a tie after it in the bar it started in, or a slide) */
                uint32_t cur = idx, b2 = bar;
                int32_t a2 = at;
                for (m = 0; m < L; m++) {
                    j = cur ? cur - 1u : L - 1u;
                    if (ph != TV_NOPH)
                        a2 -= (int32_t)phrase_duration(t, j, per, 0, 0), b2 = tv_bar(a2, ob, bl);
                    w = seq_play_step_at(t, j, b2);
                    if (seq_play_step_at(t, cur, b2)->time != ST_TIE && !(w->flags & SF_SLIDE)) {
                        m = L;
                        break;
                    }
                    if (w->time != ST_TIE)
                        break;
                    cur = j;
                }
                m = m < L && tv_hit(w) && step_ratchet(w) == 1u ? tv_state(t, w, j, ph != TV_NOPH && (j <= ph || j > idx), 0u) : 0u;
                st = !st || !m ? 0u : (st | m) & 2u ? 2u : 1u;
            }
            e->maybe = st == 2u;
            if (!st)
                e->k = TV_REST;
        }
        if (e->k == TV_REST && tv_hit(o))
            e->k = TV_DROP;
    }
    return ph;
}

/* a part's 16 steps around its playhead, standing on row y (bars up to hmax tall: 22 alone, 9 in BOTH's two rows), in the footer's
 * columns (bar_x) and its rounded bars: a hit a bar (a chord taller) in the part's colour, the accent where it varies, a lit outline
 * where it is added, a dim outline where the variation, the thinning or a chance roll drops it; a tie a short bar, an empty step a
 * stub, the steps past the pattern's end a line; the playing step underlined. A chance step not rolled yet is striped. As they play
 * now (tide_views) */
static void tide_steps(const track_t *t, int32_t y, int32_t hmax, uint16_t c)
{
    tv_t tv[16];
    uint32_t ph = tide_views(t, 0xFFu, tv), page = ph == TV_NOPH ? 0u : ph / 16u, i;
    int32_t h1 = hmax >= 20 ? 10 : 6, hs = hmax >= 20 ? 5 : 3, r;   /* (a single note, each further note of a chord; one part tall, both parts short) */
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i), h = h1 / 2;
        const tv_t *e = &tv[i];
        if (e->k == TV_END) {
            cv_rect(x, y - 1, 9, 1, T_LINE);
            continue;
        }
        if (e->k >= TV_HIT) {
            h = h1 + hs * (int32_t)(e->n - 1u);
            h = h > hmax ? hmax : h;
            if (e->k == TV_ADD)
                cv_frame(x, y - h, 9, h, T_ACCENT);
            else
                cv_rrect(x, y - h, 9, h, 2, e->k == TV_MOVE ? T_ACCENT : c, T_BG);
        } else if (e->k == TV_TIE)
            cv_rrect(x, y - h, 9, h, 1, T_MID, T_BG);
        else if (e->k == TV_DROP)
            cv_frame(x, y - h1, 9, h1, T_DIM);
        else
            cv_rrect(x, y - 3, 9, 3, 1, T_DIM, T_BG);
        if (e->maybe && e->k != TV_ADD)
            for (r = 1; r < h; r += 2)
                cv_rect(x, y - h + r, 9, 1, T_BG);
        if (ph == page * 16u + i)
            cv_rect(x, y + 2, 9, 2, T_TEXT);
    }
}
/* what the steps look like now (the page's redraw: nothing the model reads may change without it) */
static uint32_t tide_steps_sig(const track_t *t)
{
    tv_t tv[16];
    uint32_t h = tide_views(t, 0xFFu, tv), i;
    for (i = 0; i < 16u; i++)
        h = (h ^ (uint32_t)tv[i].k ^ (uint32_t)tv[i].n << 4 ^ (uint32_t)tv[i].maybe << 8) * 16777619u;
    return h;
}

/* TIDE's body. One part (ALGORITHM on PART 1 / PART 2): its SOUND and SEQ faders, its steps. BOTH: the SOUND and SEQ faders in two
 * columns (their names between A and B over them), a row per part (its number and colour), the steps in two rows, part 1 above. The
 * glide's status is knob 1's line (tide_columns). Rows (capitals, knobs, bars, playhead marks), one part: faders 54 .. 66 and 76 .. 88,
 * BOTH: the column heads 50 .. 60, faders 65 .. 75 and 79 .. 89; then the MODE line 94 .. 104, the steps 111 .. 136 (BOTH 110 .. 122 and
 * 124 .. 136), the glide's bar 139 .. 141: 4 px or more between them. The steps and the part numbers take the part's own colour (T_PTHEME, as the knobs and the MORPH
 * page's curves), not vt's text fallback, which turned one part black in the light palettes */
/* what each MODE does, one short line under the faders (owner, 2026-11-01) */
static const char *const GL_MODE_WHAT[5] = {"ONE TRIP, THEN IT STOPS", "BACK AND FORTH, A AND B", "NEW TWISTS ON YOUR SOUND",
                                            "WALKS AWAY FROM YOUR SOUND", "TRAVELS THROUGH YOUR SOUNDS"};
static void graph_tide(void)
{
    uint32_t k;
    cv_begin(240, TB_Y1 - TB_Y0, T_BG);
    cv_bg = T_BG;
    cv_oy = -TB_Y0;
    if (tide_both()) {                                   /* two columns, SOUND and SEQ, their name between A and B over them; a row per part, its number in its colour */
        for (k = 0; k < NSHOWN; k++) {
            tide_track(70 + 14 * (int32_t)k, JB_X0, JB_X0 + JB_W, GT_SOUND, (int32_t)k);
            tide_track(70 + 14 * (int32_t)k, JB_X0 + JB_DX, JB_X0 + JB_DX + JB_W, GT_SEQ, (int32_t)k);
            tide_steps(&trk[k], 119 + 14 * (int32_t)k, 9, T_PTHEME(k));
        }
    } else {                                             /* one part: a row per fader, its name, A, the track, B */
        tide_track(60, JP_X0, JP_X1, GT_SOUND, -1);
        tide_track(82, JP_X0, JP_X1, GT_SEQ, -1);
        tide_steps(TSEL, 133, 22, T_THEME);
    }
    cv_text_in(0, UT_TY(S, 104), 240, &AF_S, GL_MODE_WHAT[gl.mode % 5u], T_MID, T_BG);   /* (between the faders and the steps' tallest bars) */
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
    const char *nd = jl_needs(JL_TURNS);                 /* (what TURNS .. ARC lack now: those rows' values dim) */
    cv_begin(240, (uint32_t)(UT_L5_PITCH * (int32_t)JL_VIS + 2), T_BG);
    cv_bg = T_BG;
    cv_oy = -y0;
    for (r = top; r < JL_N && r < top + JL_VIS; r++) {
        int32_t y = UT_L5_Y0 + (int32_t)(r - top) * UT_L5_PITCH, ty = UT_TY(M, y + UT_L5_TEXT_BASE), xr = UT_L5_X + UT_L5_W - 5;   /* (the value 5 px in from the row's right end, as the name from its left: the SAVE lists' LR_R) */
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
            cv_text_r(xr, ty, &AF_M, v, sel ? T_INK : nd && r >= JL_TURNS ? T_DIM : T_THEME, bg);   /* (a row that does nothing now: dim) */
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
    return (seqm_fader_of(pi) | seqm_notes_of(pi) << 8) ^ sqm_key(&sqm[pi]) * 2654435761u ^ tide_play_len(t) * 40503u;   /* (the loop POLY plays: the dimmed steps) */
}

/* what makes the JAM pages redraw: the faders, the glide, the steps playing and their variation, the settings, the list */
static uint32_t tide_sig(void)
{
    uint32_t pi = song.sel, bars = gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_trip_bars();
    const track_t *t = TSEL;
    return (morph_fader_of(pi) | seqm_fader_of(pi) << 7 | (uint32_t)gl.on << 14 | (uint32_t)gl.armed << 16 | (uint32_t)gl.target << 17 |
            (uint32_t)gl.mode << 19 | (uint32_t)gl.to << 22 | (uint32_t)gl.hold << 23 | (song.playing ? t->seq_idx : 0xFFu) << 24) ^
           (gl.prog >> 10) * 2654435761u ^ ((gl.prog * bars) >> GLIDE_Q) * 0x27D4EB2Fu ^ sqm_key(&sqm[pi < NSHOWN ? pi : 0u]) * 40503u ^
           ((uint32_t)gl.time | (uint32_t)gl.wait << 4 | (uint32_t)jl_sel << 8 | (uint32_t)sqm_vary_all << 12 | (uint32_t)morph_musical << 13 | (uint32_t)(sqm_turns & 3u) << 14 | (uint32_t)sqm_poly << 28 | (uint32_t)sqm_sync << 30 |
            (uint32_t)morph.cfg << 16 | (uint32_t)((sqm_bar + sqm_bar_turn) & 3u) << 24 | (uint32_t)song.playing << 26) * 0x9E3779B1u ^
           ((uint32_t)sqm_arc | (uint32_t)tarc.phase << 2 | (uint32_t)tarc.left << 5 | (uint32_t)tarc.hand << 9 | (uint32_t)tarc.len << 10) * 0x2545F491u ^   /* (audit C4: no field ORed over another) */
           (tide_both() ? (morph_fader_of(1u - pi) | seqm_fader_of(1u - pi) << 7 | (song.playing ? trk[1u - pi].seq_idx : 0xFFu) << 14) * 0x85EBCA77u ^
                             sqm_key(&sqm[(1u - pi) % NSHOWN]) * 0xC2B2AE3Du ^ tide_steps_sig(&trk[1u - pi]) * 0x165667B1u ^ 0x27D4EB2Fu : 0u) ^
           tide_steps_sig(t) * 0xD3A2646Cu;
}

/* ---------------------------------------------------------------- kept over a power cycle --- */
/* MODE, TIME, WAIT, TARGET, VARY BARS, the ROLL's MUSIC and WASH LEVEL in TIDE's own record (tide_store.c: OBJ_TIDE, patch chunk 21's
 * sector pair; plan §4.5), not in the settings record (1.1 uses the bytes JAM had there). ARP, PITCH and WILD are the morph's own byte
 * (ui.c ui_morph_cfg). The record is read once at power-on (none or damaged: the defaults; neither is written back); a change is saved ~2 s after the last one,
 * while stopped and not during ERASE DATA (tide_store_poll) */
enum { TS_MODE, TS_TIME, TS_TARGET, TS_WAIT, TS_VARY, TS_MUSIC, TS_WASH, TS_OLD, TS_TURNS = TS_OLD, TS_POLY, TS_SYNC, TS_ARC, TS_STYLE, TS_N };   /* (TS_OLD: the fields of the first record; a record of that length loads, the later fields at their defaults) */
static uint8_t tide_seen[TS_N], tide_seen_ok;
static void tide_settings(void)
{
    uint8_t w[TS_N];
    if (!tide_seen_ok) {
        uint8_t b[TS_N] = {[TS_TURNS] = 1u, [TS_POLY] = 1u, [TS_SYNC] = 1u, [TS_ARC] = 0u, [TS_STYLE] = STY_CUSTOM};   /* (the defaults of the fields an older record lacks) */
        uint32_t have = tide_store_load_min(b, TS_N, TS_OLD);
        if (have) {
            gl.mode = (uint8_t)(b[TS_MODE] <= GM_JOURNEY ? b[TS_MODE] : GM_ONCE);
            gl.time = (uint8_t)(b[TS_TIME] < GLIDE_TIMES ? b[TS_TIME] : 3u);
            gl.target = (uint8_t)(b[TS_TARGET] <= GT_BOTH ? b[TS_TARGET] : GT_SOUND);
            gl.wait = (uint8_t)(b[TS_WAIT] < GLIDE_WAITS ? b[TS_WAIT] : 0u);
            sqm_vary_all = (uint8_t)(b[TS_VARY] & 1u);
            morph_musical = (uint8_t)(b[TS_MUSIC] & 1u);
            morph_wlvl = (uint8_t)(b[TS_WASH] & 3u);
            sqm_turns = (uint8_t)(b[TS_TURNS] < 4u ? b[TS_TURNS] : 1u);
            sqm_poly = (uint8_t)(b[TS_POLY] < 3u ? b[TS_POLY] : 1u);
            sqm_sync = (uint8_t)(b[TS_SYNC] < 3u ? b[TS_SYNC] : 1u);
            sqm_arc = (uint8_t)(b[TS_ARC] < 3u ? b[TS_ARC] : 0u);
            tide_style = (uint8_t)(have > TS_STYLE && b[TS_STYLE] < STY_CUSTOM ? b[TS_STYLE] : STY_CUSTOM);   /* (an older record: CUSTOM, its values kept) */
        } else
            tide_style_apply(STY_DUB);                   /* (no record: the DUB showcase) */
    }
    if (tide_style < STY_CUSTOM && !tide_style_holds())
        tide_style = STY_CUSTOM;                         /* a hand edit of any of the nine rows (knobs, rows, resets, MIDI) */
    w[TS_MODE] = gl.mode;
    w[TS_TIME] = gl.time;
    w[TS_TARGET] = gl.target;
    w[TS_WAIT] = gl.wait;
    w[TS_VARY] = sqm_vary_all;
    w[TS_MUSIC] = morph_musical;
    w[TS_WASH] = (uint8_t)(morph_wlvl & 3u);
    w[TS_TURNS] = sqm_turns;
    w[TS_POLY] = sqm_poly;
    w[TS_SYNC] = sqm_sync;
    w[TS_ARC] = sqm_arc;
    w[TS_STYLE] = tide_style;
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
    tarc_poll();                                         /* ARC: the phrase counted, the levers set */
    sqm_thin_poll();                                     /* LEAD / ECHO and ARC: the follower's mask (when what it reads changed) */
    glide_poll();                                        /* a launched GLIDE moves its faders */
    seqm_poll();                                         /* the SEQ MORPH's ends follow pattern edits (TRANS, SHIFT, an edit) */
    morph_xpoll();                                       /* plan any destination GLIDE generated in this same pass */
    tide_store_poll();
}
