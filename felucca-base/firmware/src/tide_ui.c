/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (called JAM before): AUTO TIDE and TIDE SETTINGS, two pages of the HOME round (MORPH, TIDE SETTINGS, AUTO TIDE; TIDE SETTINGS stands in 1.1's
 * MORPH SET's place: SMART is its ROLL TYPE row; 1.0.0-b7: ARP and PITCH have no row, their stored switches still act, OFF by default).
 *   AUTO TIDE      (GR_TIDE) the SOUND and SEQ faders with the glide's path, the 16 steps around the playhead, a status line. K1 GLIDE (twist
 *                  left: glide to A, right: to B, two detents to stop a running glide), K2 MODE (EVOLVE / TRAVEL), K3 TIME
 *                  (bars), K4 WAIT; OCT- + K1 back to the Original, + K2 an ARC drop, + K3 / K4 TIME and WAIT to the STYLE's   ... play
 *   TIDE SETTINGS  (GR_TIDELIST) the settings in a list, as the MENU's: K1 picks the row, OCT+ or K2..K4 the next value, OCT- the one before.
 *                  STYLE, MODE, TARGET, ARC, KNOBS, AMOUNT, REC, PLAY MODE, XFADE, SPLIT (JL_NAME;1.0.0-b7, MR8: VARY BARS, WASH, TURNS, POLY and OFFBEAT
 *                  are STYLE's alone, no rows)        ... set and forget
 * LINK: TARGET BOTH links the two faders (b10: it read SND+SEQ): GLIDE moves both, and the MORPH fader moved by hand (the MORPH page, the mod wheel, CC 70:
 * ui_input.c morph_act) moves the SEQ fader with it (tide_link). ALGORITHM is the morph's selector here too: PART 1, BOTH, PART 2
 * (morph_mask). The panel shows both faders with the glide on them, and the selected part's 16 steps as they play: varied ones
 * lit, dropped ones hollow. MODE, TIME, WAIT and the list are kept over a power cycle (tide_settings).
 * Context: main loop. Included at the end of ui_input.c (tide.h H13; every UI source it uses is defined above it), with the
 * main-loop poll (tide_poll) at its end. The settings live in TIDE's own storage object (tide_store.c). */
#include "morph_glide.c"
/* b11 MORPH polish: the end part k's morph is heading for, 0 (A) .. 100 (B); -1 while it is not gliding there (ui_graph.c mv_lean) */
static int32_t mv_lean_to(uint32_t k) { return gl.on && !gl.hold && ((gl.mask >> k) & 1u) ? (gl.to ? 100 : 0) : -1; }

#define TIDE_G_GLIDE 4u                                  /* ui.mg gestures: 0..3 are MORPH's and GLO's (ui.c: TIDE_MG_N of them) */

static const char *const GL_TARGET_NAME[3] = {"SOUND", "SEQ", "BOTH"};
static const char *const GL_MODE_NAME[5] = {"EVOLVE", "EVOLVE", "EVOLVE", "EVOLVE", "TRAVEL"}   /* (b9: the player's MODE is EVOLVE (was WANDER) or TRAVEL (was JOURNEY); ONCE / LOOP / DRIFT are bench and console only and show as EVOLVE) */;

static void tide_wait_text(char *val, uint32_t n, const char **unit)   /* unit may be NULL */
{
    uint32_t b = GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS];
    if (!b) {
        str_cpy(val, "OFF", n);
        if (unit)
            *unit = "";
    } else {
        fmt_int(val, (int32_t)b);
        if (unit)
            *unit = b == 1u ? "BAR" : "BARS";
    }
}

/* JOURNEY's memory (tide_journey): the last JH_N sounds each part was sent to, newest first, as sound + 1 of engine jh_eng (0:
 * none). RAM only, never saved; cleared when a WNDR/JRNY session starts (glide_start) and on HOME */
#define JH_N 4u
static uint8_t jh[2][JH_N], jh_eng[2];
static void jh_clear(uint32_t k) { jh[k & 1u][0] = jh[k & 1u][1] = jh[k & 1u][2] = jh[k & 1u][3] = 0; }

/* OCT- + K1 on AUTO TIDE: home. The glide stops, each part of mask back to its Original sound (its ends rebased, the fader at A: End B
 * a roll again) and pattern (End A the Original, End B a variation) */
static void tide_home(uint32_t mask)
{
    uint32_t k;
    glide_stop();
    for (k = 0; k < NSHOWN; k++) {
        if (!((mask >> k) & 1u))
            continue;
        jh_clear(k);
        {
            uint8_t bv[MBV_MAX];
            up_ldv_rec(&trk[k], bv);                     /* (b7, review C2: a USER record's own End B, not a fresh roll) */
            morph_rebase_part(k);
            morph_ldv = 0;
        }
        seqm_home(k);
    }
}
/* part pi sounds other than its Original: the fader off A, or End A itself moved (a roll or MORPH TO of A: End A is not the Original; 1.0.0-b5: no
 * history, so this compares the values instead of asking whether End A's ring moved) */
static int tide_sound_moved(uint32_t pi)
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    return pi < NSHOWN && morph_view_on(pi) &&
           (mp->fader || mp->tok[0][0] || memcmp(mp->h[0][0], mp->base, P_COUNT) || memcmp(mp->hx[0][0], mp->base + XW_BASE, NX) ||
            (morph_isfm(mp) && memcmp(mp->fh[0][0], mp->fb, NFM)));
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
/* KEEP (SEQ TOOLS, G4: ui_tools.c TL_KEEP; plan H14): what part pi plays now becomes its own, one undo for both halves (OCT- + a PRESETS turn:
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

/* the SAVE dialog (ui_draw.c draw_confirm; trk2 2 = both parts): SAVE stores the sound as loaded, not what JOURNEY moved the heard end to (KEEP freezes what is
 * heard); 1 when a part it is about to store plays a factory sound MORPH TO or JOURNEY put there (the footer's name for it, tide_part_name): the dialog says
 * so, as long as it is open (1.0.0-b8) */
static int32_t tide_heard_to(uint32_t pi);
static void tide_save_note(int32_t w)
{
    uint32_t t = ui.confirm_trk;
    if (tide_heard_to(t & 1u) >= 0 || (t >= 2u && tide_heard_to(1) >= 0)) {   /* (t 2: both parts; part 0 asked first, then part 1) */
        const char *s = "SAVES THE LOADED SOUND\0KEEP FIRST = AS HEARD";   /* (two lines in one string) */
        for (t = 0; t < 2u; t++, s += 23)
            cv_text_in(0, (int32_t)t * 15, w, &AF_S, s, T_MID, T_BG);
    }
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

/* ---------------------------------------------------------------- TIDE SETTINGS --- */
enum { JL_STYLE, JL_GMODE, JL_TARGET, JL_ARC, JL_KNOB, JL_AMOUNT, JL_REC, JL_ANS, JL_ANSON, JL_MODE, JL_XFADE, JL_SPLIT, JL_N };   /* (GMODE: the glide's MODE, AUTO TIDE's K2 too; REC: live recording's; MODE (PLAY MODE), XFADE, SPLIT, the last three: the output mode's own settings; REC and those three no part of STYLE; the three go through ui_input.c mode_knob, whose header describes the old MODE page: TIDE's rows are these three) */
static const char *const JL_NAME[JL_N] = {"STYLE", "MODE", "TARGET", "ARC", "KNOBS", "AMOUNT", "REC", "ANSWER", "ANSWER ON", "PLAY MODE", "XFADE", "SPLIT"};   /* (AMOUNT: how far a roll goes, the part's; on the MORPH page's K4 until 1.0.0-b7) */
/* what kind of row each is: the code branches on the kind, not on the row's place. The order matters: JK_OUT .. JK_SPLIT are the output mode's rows (mode_knob, the last three
 * rows), JK_XFADE and JK_SPLIT are LAYER's only. LIST: a value from JL_V (TARGET, REC), ARC: the same, and it acts only in automatic EVOLVE / TRAVEL on the SEQ */
enum { JK_LIST, JK_STYLE, JK_GMODE, JK_ARC, JK_KNOBS, JK_AMOUNT, JK_OUT, JK_XFADE, JK_SPLIT };
static const uint8_t JL_KIND[JL_N] = {JK_STYLE, JK_GMODE, JK_LIST, JK_ARC, JK_KNOBS, JK_AMOUNT, JK_LIST, JK_LIST, JK_LIST, JK_OUT, JK_XFADE, JK_SPLIT};
static uint8_t jl_sel;                                   /* the row picked (K1) */
/* 1.0.0-b9 KNOBS: HOME page 1's knobs 2..4 (knob_sets.h). Stored in the byte ROLL TYPE had (TS_KNOB): 0 or 1 AUTO (1 the default, an older record's 0 / 1 too), 2 STD, 3..8 DB HS TC AC EL AM.
 * AUTO takes the set from the first word of the music's name ("ACID HYPNO", knob_sets.h KN_TAG); any other first word, or no multi: STD */
static uint8_t tide_knobs = 1;
static uint32_t kn_cur(void)                             /* the set (KS_*) the knobs have now */
{
    char b[13];
    uint32_t k;
    if (tide_knobs > 1u)
        return tide_knobs - 2u;
    project_cur_name(b);
    for (k = 0; k < KS_N - 1u; k++) {
        uint32_t i;
        for (i = 0; KN_TAG[k][i] && b[i] == KN_TAG[k][i]; i++)
            ;
        if (!KN_TAG[k][i] && (b[i] == ' ' || !b[i]))
            return k + 1u;
    }
    return KS_STD;
}
static void morph_act(uint32_t kind, uint32_t mask, uint32_t v, uint32_t pi);   /* (ui_input.c) */
static void tide_pick_mode(void) { jl_sel = JL_MODE; }   /* HOME OPENS = MODE (ui.c go_main_to): TIDE SETTINGS with the PLAY MODE row picked */

/* STYLE: one move sets the eight values below together. MODE and ARC are rows, TIME and WAIT AUTO TIDE's knobs; VARY, POLY, SYNC (OFFBEAT) and WASH
 * are the style's alone (1.0.0-b7, MR8: no rows; the engine reads them as before; TURNS is gone since 1.0.0-b8: see TS_TURNS_OLD). TARGET chooses what TIDE may change, not the genre, and stays.
 * A hand edit of MODE, TIME, WAIT or ARC turns it into CUSTOM and says "STYLE CUSTOM" (tide_settings); CUSTOM itself changes nothing and keeps the style values it had.
 * mode, time (index), wait (index), VARY, POLY, SYNC, ARC, WASH */
enum { STY_DUB, STY_DEEP, STY_MINIMAL, STY_AMBIENT, STY_PEAK, STY_DRONE, STY_CUSTOM };   /* (CUSTOM was 5 until 1.0.0-b4: a stored 5 is read as DRONE only if its rows are DRONE's, else CUSTOM; tide_settings) */
static const uint8_t STY[6][8] = {
    {GM_WANDER, 4, 2, 0, 1, 1, 1, 0},                    /* DUB: the showcase (the no-record rows) with LEAD and a RISE; WANDER keeps the multi's own sounds (1.0.0-b7, MR1: JOURNEY before) */
    {GM_WANDER, 5, 3, 0, 0, 1, 1, 1},                    /* DEEP */
    {GM_WANDER, 3, 0, 1, 2, 2, 0, 0},                    /* MINIMAL */
    {GM_JOURNEY, 6, 4, 0, 0, 0, 0, 3},                   /* AMBIENT: no groove tricks, so POLY OFF (1.0.0-b7, MR1: GENTLE before) */
    {GM_JOURNEY, 3, 0, 1, 1, 2, 2, 0},                   /* PEAK: ARC DROP and LEAD (ECHO before: D7) */
    {GM_WANDER, 6, 0, 0, 0, 0, 0, 3}};                   /* DRONE: a held chord of a drone sound; WNDR keeps the sound, 64 bars, always moving, nothing rhythmic */
#define STY_HAND 0x47u                                   /* (the values a hand can change, STY's columns: MODE, TIME, WAIT, ARC; the others only STYLE sets) */
static uint8_t tide_style = STY_DUB, tide_style_last = STY_DUB;   /* (the style a hand edit left: OCT- + K3 / K4 go back to its TIME and WAIT) */
#include "tide_take.c"                              /* b10 round 8 (after STYLE, which the answer's genre reads): TAKE + ANSWER, and the two settings rows' variables */
static volatile uint8_t *const TIDE_ROW[8] = {&gl.mode, &gl.time, &gl.wait, &sqm_vary_all, &sqm_poly, &sqm_sync, &sqm_arc, &morph_wlvl};   /* (STY's order) */
static __attribute__((noinline)) void tide_rows_set(const uint8_t *t)
{
    for (uint32_t i = 0; i < 8u; i++)
        *TIDE_ROW[i] = t[i];
}
static void tide_style_apply(uint32_t s)
{
    tide_rows_set(STY[s]);
    tide_style = (uint8_t)s;
}
static __attribute__((noinline)) void tide_rows_get(uint8_t *t)   /* the eight values as they are now */
{
    for (uint32_t i = 0; i < 8u; i++)
        t[i] = *TIDE_ROW[i];
    t[7] &= 3u;                                          /* (WASH) */
}
static int tide_style_holds(void)                        /* the values a hand can change are the style's (the others: only STYLE sets them) */
{
    uint8_t t[8];
    tide_rows_get(t);
    for (uint32_t i = 0; i < 8u; i++)
        if (((STY_HAND >> i) & 1u) && t[i] != STY[tide_style % 6u][i])
            return 0;
    return 1;
}
/* CUSTOM's eight values, kept when a style replaces them: the STYLE row back onto CUSTOM brings them back (RAM only, not saved) */
static uint8_t tide_custom[8], tide_custom_ok;
static void tide_custom_take(void)
{
    tide_rows_get(tide_custom);
    tide_custom_ok = 1;
}
/* what row r lacks now (0: it acts). ARC acts only in automatic WNDR / JRNY with the SEQ gliding (glide_poll: sqm_auto), whatever ROLL TYPE is (1.0.0-b7, MR8);
 * XFADE and SPLIT are LAYER's only (1.0.0-b9: the KNOBS row acts always) */
static __attribute__((noinline)) const char *jl_needs(uint32_t r)
{
    uint32_t k = JL_KIND[r];
#if TIDE_PLAYSEL
    if (k == JK_SPLIT || (k == JK_XFADE && duell_mode_req == DM_LAYER))   /* 1.0.0-b7: XFADE and SPLIT act in ALGORITHM's middle only (PART 1 / PART 2 play one part); SPLIT there in STEREO and DUEL too */
        return song.both ? 0 : "2 PARTS ONLY";
#endif
    if (k >= JK_XFADE)
        return duell_mode_req == DM_LAYER ? 0 : "LAYER ONLY";
    if (k != JK_ARC)
        return 0;
    return gl.mode < GM_WANDER ? "NEEDS EVOLVE" : gl.target == GT_SOUND ? "NEEDS SEQ" : 0;   /* (short: knob 1's line has room for these after "SETTING ARC"; EVOLVE / TRAVEL in full do not fit) */
}
/* does row r's value draw dim (it does nothing now)? whenever it lacks something (tests/mode_test.c checks this) */
static int jl_dimmed(uint32_t r) { return jl_needs(r) != 0; }
/* the rows as tables: how many values each has, its byte (STYLE, MODE, TARGET, ARC, REC; ROLL TYPE is the morph's), the values' names, and the
 * header message a change shows: the prefix + the value's name, or a message of its own (TARGET) */
static const uint8_t JL_NV[JL_N] = {7, 2, 3, 3, KS_N + 1, 101, 2, TA_N, 2, 3, 101, 130};   /* (AMOUNT and XFADE 101: 0..100 %; SPLIT 130: 0 OFF, 1 MIDDLE (1.0.0-b7, mix_split SPLIT_MIDDLE), then the notes 0..127 (mix_split note + 1, the list 2..129): jl_split_ix) */
static volatile uint8_t *const JL_VAR[JL_N] = {[JL_STYLE] = &tide_style, [JL_GMODE] = &gl.mode, [JL_TARGET] = &gl.target, [JL_ARC] = &sqm_arc,
                                               [JL_REC] = &tide_rec_replace, [JL_ANS] = &tide_answer, [JL_ANSON] = &tide_answer_on};   /* (REC is no part of a STYLE: seq.c, live recording) */
static const char *const JL_V[] = {"DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK", "DRONE", "CUSTOM", "SOUND", "SEQ", "BOTH", "OFF", "RISE", "DROP",
                                   "AUTO", "STD", "DUB", "HSE", "TECH", "ACID", "ELEC", "AMB", "ADD", "REPLACE", "LAYER", "STEREO", "DUEL", "OFF", "BASS LINE", "DOUBLE", "REPLY", "ECHO", "OTHER", "OFF"};
static const uint8_t JL_V0[JL_N] = {0, 0, 7, 10, 13, 0, 21, 26, 31, 23, 0, 0};   /* (each row's first name in JL_V; MODE's are GL_MODE_NAME) */
static const char *const JL_SAY[JL_N] = {"STYLE ", "MODE ", 0, "ARC ", "", "AMOUNT ", "REC ", "ANSWER ", "ANSWER ON ", 0, 0, 0};
static const char *const JL_OWN[3] = {"TARGET SOUND", "TARGET SEQ", "TARGET BOTH"};   /* (TARGET's messages) */
static uint32_t jl_n(uint32_t r) { return JL_NV[r % JL_N]; }
/* SPLIT: the stored code (core.h mix_split) <-> its place in the list (OFF, MIDDLE, C-1 .. G9) */
static uint32_t jl_split_ix(uint32_t s) { return s == SPLIT_MIDDLE ? 1u : s && s <= 128u ? s + 1u : 0u; }
static uint32_t jl_split_code(uint32_t v) { return v == 1u ? SPLIT_MIDDLE : v ? v - 1u : 0u; }
static uint32_t jl_get(uint32_t r)
{
    uint32_t k;
    r %= JL_N;
    k = JL_KIND[r];
    if (k == JK_OUT)                                     /* (the list reads LAYER, STEREO, DUEL: mode_knob's order) */
        return DM_LAYER - (duell_mode_req < DM_N ? duell_mode_req : DM_SPLIT);
    if (k == JK_XFADE || k == JK_SPLIT)
        return k == JK_XFADE ? mix_xf : jl_split_ix(mix_split);
    if (k == JK_AMOUNT)
        return morph_amount_of(song.sel);
    if (k == JK_GMODE)                                   /* (b9: EVOLVE 0, TRAVEL 1; a stored ONCE / LOOP / DRIFT loads as EVOLVE, tide_settings) */
        return gl.mode >= GM_JOURNEY ? 1u : 0u;
    if (JL_VAR[r])
        return *JL_VAR[r];
    return tide_knobs > 1u ? tide_knobs - 1u : 0u;      /* (KNOBS: AUTO, STD, DB .. AM) */
}
static const char *jl_vname(uint32_t r, uint32_t v)
{
    static char b[8];                                    /* (one buffer for XFADE and SPLIT: the caller draws or copies the name before the next call; do not cache the pointer) */
    uint32_t k;
    r %= JL_N;
    k = JL_KIND[r];
    if (k == JK_XFADE || k == JK_AMOUNT) {
        fmt_int(b, (int32_t)(v % 101u));
        str_cpy(b + str_len(b), "%", 2);
        return b;
    }
    if (k == JK_SPLIT) {
        v %= 130u;
        if (v == 1u)
            return "MIDDLE";
        if (v)
            note_name(b, v - 2u);
        else
            return "OFF";
        return b;
    }
    if (k == JK_GMODE)
        return GL_MODE_NAME[GM_WANDER + v % 2u];
    return JL_V[JL_V0[r] + v % JL_NV[r]];
}
static void jl_put(uint32_t r, uint32_t v)
{
    uint32_t pi = song.sel, mask = morph_mask(pi);
    const char *say;
    uint32_t k;
    r %= JL_N, v %= JL_NV[r];
    k = JL_KIND[r];
    if (k == JK_SPLIT) {                              /* (a place in the list, set as it is: mode_knob would step out of MIDDLE to its note) */
        if (TIDE_PLAYSEL ? song.both : duell_mode_req == DM_LAYER)
            mix_split = (uint8_t)jl_split_code(v);
        return;
    }
    if (k >= JK_OUT) {                                /* the old MODE page's knobs, by steps from where the value is (mode_knob: its message, its limits, SPLIT's middle-key start) */
        mode_knob(r - JL_MODE, (int32_t)v - (int32_t)jl_get(r));
        return;
    }
    if (k == JK_STYLE) {
        if (v < STY_CUSTOM) {
            if (tide_style == STY_CUSTOM || !tide_style_holds())
                tide_custom_take();                      /* (the values being replaced are CUSTOM: kept for the way back) */
            tide_style_apply(v);
        } else {
            if (tide_custom_ok)
                tide_rows_set(tide_custom);
            if (tide_style < STY_CUSTOM)
                tide_style_last = tide_style;
            tide_style = STY_CUSTOM;
        }
    } else if (k == JK_AMOUNT) {                         /* (BOTH: each part from its own AMOUNT by the same steps) */
        int32_t d = (int32_t)v - (int32_t)jl_get(r);
        for (uint32_t k = 0; k < NSHOWN; k++)
            if ((mask >> k) & 1u)
                morph_act(MQ_AMOUNT, 1u << k, (uint32_t)clamp((int32_t)morph_amount_of(k) + d, 0, 100), k);
    } else if (k == JK_KNOBS) {                          /* (AUTO is stored 1, as the retired ROLL TYPE's MUSIC was: an older record's 0 or 1 reads AUTO) */
        tide_knobs = (uint8_t)(v ? v + 1u : 1u);
    } else if (k == JK_GMODE)                            /* (MODE: the glide's, as AUTO TIDE's K2: EVOLVE or TRAVEL) */
        gl.mode = (uint8_t)(GM_WANDER + v);
    else                                                 /* (ARC: seq_arc.c) */
        *JL_VAR[r] = (uint8_t)v;
    say = JL_SAY[r];
    ui_say(say ? say : JL_OWN[v], say ? jl_vname(r, v) : "");
}
/* what TIDE SETTINGS draws (ui_graph.c graph_signature): the rows' values, the row picked, the parts' colours */
static uint32_t tide_list_sig(void)
{
    uint32_t r, h = (uint32_t)jl_sel * 2654435761u ^ (uint32_t)T_PTHEME(0) * 0x85EBCA77u ^ (uint32_t)T_PTHEME(1) * 0xC2B2AE3Du;   /* (the rows' values; MODE, TARGET and PLAY MODE, which decide the dim rows, are among them) */
    h ^= (uint32_t)song.both * 0x27D4EB2Fu;                                 /* (1.0.0-b7: SPLIT dims by the selection too) */
    for (r = 0; r < JL_N; r++)
        h = (h ^ jl_get(r)) * 16777619u;
    return h;
}
/* the page's OCT- / OCT+ (ui_input.c): the row's value one back / on, stopping at the ends, as in the MENU */
static int tide_list_on(void) { return !ui.perf && !ui.layer && cur_page()->graph == GR_TIDELIST; }
static void tide_list_oct(uint32_t oct)
{
    uint32_t r = jl_sel % JL_N, v = jl_get(r), k = JL_KIND[r];
    if (k == JK_AMOUNT) {                                /* AMOUNT steps 5 % */
        jl_put(r, (uint32_t)clamp((int32_t)v + ((oct & 2u) ? 5 : (oct & 1u) ? -5 : 0), 0, 100));
        return;
    }
    if (k >= JK_XFADE) {                                 /* XFADE steps 10 %, SPLIT one octave (12) from where it is; out of OFF the first step is MIDDLE, out of MIDDLE the note it stands for (mode_knob). Both stop at their ends */
        mode_knob(r - JL_MODE, (oct & 2u) ? (k == JK_SPLIT && v < 2u ? 1 : k == JK_SPLIT ? 12 : 10) : (oct & 1u) ? (k == JK_SPLIT ? -12 : -10) : 0);
        return;
    }
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
    if (cur_page()->graph == GR_TIDELIST) {               /* K1 the row; K2..K4 the picked row's value, a step an event as OCT+ / OCT- (audit C12: every other page has a knob a value) */
        if (slot == 0u)
            jl_sel = (uint8_t)clamp((int32_t)jl_sel + s, 0, JL_N - 1);
        else if (JL_KIND[jl_sel % JL_N] >= JK_OUT)    /* MODE, XFADE, SPLIT: the detents turned, as the old knobs (a turn moved XFADE and SPLIT by many) */
            mode_knob(jl_sel % JL_N - JL_MODE, s);
        else if (JL_KIND[jl_sel % JL_N] == JK_AMOUNT)   /* AMOUNT: the detents turned, as its knob on the MORPH page did (b6) */
            jl_put(JL_AMOUNT, (uint32_t)clamp((int32_t)jl_get(JL_AMOUNT) + accel(EN_K1 + slot, s, 100), 0, 100));
        else
            tide_list_oct(s > 0 ? 2u : 1u);
        return;
    }
    switch (slot) {
    case 0:                                              /* (gliding, a twist of two detents stops it where it is: PLAY goes on; the next one launches, at one) */
        if ((d = gesture_det(TIDE_G_GLIDE, s, glide_running() ? 2 : 1)) != 0 && glide_running()) {
            glide_stop();
            ui_message("GLIDE STOPPED");
        } else if (d) {
            glide_launch(d > 0, mask, pi);
            ui_message(d > 0 ? "GLIDE TO B" : "GLIDE TO A");
        }
        break;
    case 1:
        gl.mode = s > 0 ? GM_JOURNEY : GM_WANDER;   /* (one step an event on all three cards: a fast turn skipped 16 -> 64 bars; audit C17) */
        break;
    case 2:
        gl.time = (uint8_t)clamp((int32_t)gl.time + (s > 0 ? 1 : -1), 0, (int32_t)GLIDE_TIMES - 1);
        break;
    default:
        gl.wait = (uint8_t)clamp((int32_t)gl.wait + (s > 0 ? 1 : -1), 0, (int32_t)GLIDE_WAITS - 1);
        break;
    }
}

/* MIDI (midi_control.c, through the morph queue): GLIDE to end v, the SEQ fader to v, on the parts of mask */
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

/* OCT- held + a knob on AUTO TIDE (audit C9, D9): K1 back to the Original sound and pattern, K2 drops the ARC, K3 TIME and K4 WAIT to the STYLE's values
 * (the style a hand edit left, DUB when none: 16 bars, 2 bars); MODE has none */
static int tide_reset(uint32_t k)
{
    const uint8_t *sty = STY[(tide_style < STY_CUSTOM ? tide_style : tide_style_last) % 6u];
    char v[12];
    if (cur_page()->graph == GR_TIDELIST)
        return 0;
    if (k == 0u) {
        tide_home(morph_mask(song.sel));
        ui_message("BACK TO ORIGINAL");
        return 1;
    }
    if (k == 1u) {                                       /* MODE has no reset: OCT- + K2 drops (ARC) at the next phrase */
        ui_message(tarc_drop_hand());
        return 1;
    }
    if (k == 2u) {
        gl.time = sty[1];
        fmt_int(v, (int32_t)glide_bars());
        ui_say("TIME ", v);                              /* (what the TIME row shows; the plain reset_say says nothing) */
        return 1;
    }
    if (k == 3u) {
        gl.wait = sty[2];
        tide_wait_text(v, sizeof v, 0);
        ui_say("WAIT ", v);
        return 1;
    }
    return 0;
}

/* JOURNEY (glide_arrive, glide_start): end x of the parts of mask the next target. The sound: a factory sound of the part's engine
 * and the Original's role family (a USER record's own category) and attack class (PAD, STAB or KEYS by ATK and SUS: a stab never
 * turns into a slow pad nor a pad into a stab; a family with none of the class still turns, any class, C2), not the one just reached nor (when there is another) the one left,
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
/* JOURNEY roams a role family, not one category: HARMONIC (pads, keys), BASS, LEAD / PLUCK, PERC (hits), TEXT (textures, drones;
 * OTHER, a user record's catch-all, too: a texture is the quietest place for a sound that says nothing of its role) and NOISE
 * (the one-shot FX: pings, sweeps, bursts). Every category is mapped here; a new one stops the build until it is placed (a category
 * past the table counts as HARMONIC). A factory FX sound is TEXT only with the DSF_TEXT flag (duell_sounds.py text=True; C3 of
 * TIDE_ROLES_AND_MULTIS_SPEC.md, D39: a hidden flag, no PATCHES category); a USER record has no flag, so its FX reads as NOISE */
enum { FAM_HARM, FAM_BASS, FAM_LEAD, FAM_PERC, FAM_TEXT, FAM_NOISE, FAM_N };
static const uint8_t JFAM[CAT_N] = {
    [CAT_NONE] = FAM_HARM, [CAT_BASS] = FAM_BASS, [CAT_LEAD] = FAM_LEAD, [CAT_PAD] = FAM_HARM, [CAT_PLUCK] = FAM_LEAD,
    [CAT_KEYS] = FAM_HARM, [CAT_PERC] = FAM_PERC, [CAT_FX] = FAM_NOISE, [CAT_OTHER] = FAM_TEXT};
typedef char jfam_covers_every_category[CAT_N == 9 ? 1 : -1];
static uint32_t fam_of(uint32_t cat) { return cat < CAT_N ? JFAM[cat] : FAM_HARM; }
/* the family of factory sound k of engine e: its category's, except an FX sound with the TEXT flag, which is a texture or drone */
static uint32_t fam_of_sound(uint32_t e, uint32_t k)
{
    const ds_sound_t *s = ds_get(e, k);
    uint32_t c = ds_cat_of(e, k);
    return c == CAT_FX && s && (s->flags & DSF_TEXT) ? (uint32_t)FAM_TEXT : fam_of(c);
}
/* the part's sound is in the BASS family (a factory sound's category, or the USER record's): the pitch walk leaves it alone (seq_evolve.c) */
static int tide_bass_part(const track_t *t)
{
    return fam_of(UP_REC_USER(t) ? sl_cat((uint32_t)t->user - 1u) : ds_cat_of(t->eng_req, t->preset)) == FAM_BASS;
}
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
/* a sound's attack class from its ATK and SUS: 2 PAD (ATK 60 or more), 1 STAB (SUS 20 or less), 0 KEYS (C2 of TIDE_ROLES_AND_MULTIS_SPEC.md) */
static uint32_t tide_cls(int32_t atk, int32_t sus) { return atk >= 60 ? 2u : sus <= 20 ? 1u : 0u; }
/* J1 (audit 2026-11-09, owner 9 November): JOURNEY travels to sounds near the part's Original in level and colour as they play in their
 * class's pattern (jrny_ctx.h, measured by tests/jrny_ctx.c: a byte a sound, its level in dB << 3 | its brightness in octaves). Of the
 * n candidates (family, class) the nearest to factory sound `at` stay, first: JCTX_MIN at least, JCTX_KEEP at most, past the minimum
 * only those within JCTX_NEAR; the memory and the random and contrast picks then choose among them. A dB a unit, an octave of
 * brightness 4. Measured from the Original, not from the sound left, so the steps cannot add up over a set. The sounds themselves are
 * not changed. Main loop. */
/* (jrny_ctx.h, JCTX_NONE and jctx_of: morph_tide.c, whose TRAVEL level match reads them first) */
#define JCTX_KEEP 4u                                     /* (at most these: the memory, 4 deep, then rotates through the Original's nearest) */
#define JCTX_MIN 2u                                      /* (at least these, so the sound keeps moving between the Original and two more) */
#define JCTX_NEAR 3u                                     /* (past the minimum, only those within 3 dB; an octave of brightness is out) */
static uint32_t jctx_dist(uint32_t a, uint32_t b)
{
    int32_t dl = (int32_t)(a >> 3) - (int32_t)(b >> 3), db = (int32_t)(a & 7u) - (int32_t)(b & 7u);
    return (uint32_t)(dl < 0 ? -dl : dl) + 4u * (uint32_t)(db < 0 ? -db : db);
}
static uint32_t jctx_near(uint32_t e, uint32_t at, uint32_t *pick, uint32_t n, uint32_t pk)
{
    uint32_t i, j, m, keep = n < JCTX_KEEP ? n : JCTX_KEEP, a = jctx_of(e, at), col = tide_mu[pk];
    if (a == JCTX_NONE)                                  /* (the Original has no row: nothing to stay near, as for a USER record) */
        return n;
    for (i = 0; i < keep; i++) {                         /* (a partial selection sort: the nearest first, the lower index on a tie. b10 Later 6b, MR5, col: a part
                                                          * as a factory multi loaded it keeps the multi's colour: a sound in another brightness octave than the
                                                          * Original, or with no row, is never kept (TRAVEL took M12's 1 kHz pad to 210 and 511 Hz, M17's held
                                                          * drone to a decaying BELL STAB), nor one whose level its TRIM cannot reach (M19 -25.5 LUFS). A sound loaded alone walks as J1) */
        uint32_t best = i, bd = 0xFFFFFFFFu, d, v;
        for (j = i; j < n; j++)
            if ((d = (v = jctx_of(e, pick[j])) == JCTX_NONE || (col && (((v ^ a) & 7u) ||   /* (.. nor one whose level its TRIM cannot reach: morph_to_sound) */
                 ds_value(ds_get(e, pick[j]), P_TRIM, TP[P_TRIM].def) + JRNY_MU[col - 1u][pk] - 2 * (int32_t)(v >> 3) > 127)) ? 0xFFFFFFFEu : jctx_dist(a, v)) < bd)
                bd = d, best = j;
        m = pick[i], pick[i] = pick[best], pick[best] = m;
        if ((col && bd >= 0xFFFFFFFEu) || (i >= JCTX_MIN && bd > JCTX_NEAR))
            return i;
    }
    return keep;
}
static void tide_journey(uint32_t mask, uint32_t x, uint32_t snd, uint32_t seq)
{
    uint32_t k, j, n, c, cls, pass, pick[64], r = morph_new_seed();
    for (k = 0; k < NSHOWN; k++) {
        const track_t *t = &trk[k];
        if (!((mask >> k) & 1u))
            continue;
        if ((snd >> k) & 1u) {
            const morph_part_t *mp = morph_ready(k);
            uint32_t e = t->eng_req, fam = UP_REC_USER(t) ? fam_of(sl_cat((uint32_t)t->user - 1u)) : fam_of_sound(e, t->preset);
            int32_t at = morph_to_of(k, x ^ 1u), was = morph_to_of(k, x);
            if (at < 0 && !UP_REC_USER(t))                      /* (the end reached is the Original or a roll: not the Original's own sound) */
                at = (int32_t)t->preset;
            cls = tide_cls(mp->base[P_ATK], mp->base[P_SUS]);
            for (pass = n = 0; pass < 3u && !n; pass++)   /* (0: the family and the class, neither end's; 1: the one left allowed; 2: any class) */
                for (j = 0; j < ds_count(e) && n < 64u; j++) {
                    const ds_sound_t *s = ds_get(e, j);
                    if (fam_of_sound(e, j) != fam || (int32_t)j == at || (!pass && (int32_t)j == was) || tide_taken(k, e, j))
                        continue;
                    if (pass < 2u && tide_cls(ds_value(s, P_ATK, TP[P_ATK].def), ds_value(s, P_SUS, TP[P_SUS].def)) != cls)
                        continue;
                    pick[n++] = j;
                }
            if (!UP_REC_USER(t) && t->preset < ds_count(e))   /* (J1: the nearest to the Original's factory sound, so a set cannot drift away step by step, as WANDER; a USER record has no row: as before) */
                n = jctx_near(e, t->preset, pick, n, k);
            n = jh_filter(k, e, pick, n);                /* (then the memory: it moves on through those, never out of them) */
            c = n ? pick[(r >> 8) % n] : 0u;
            if (n > 1u)
                c = sound_complement_pick(k, e, c, pick[(r >> 16) % n], r >> 4);
            if (n && morph_to_sound(k, x, c))
                jh_push(k, c);                           /* (only a target really adopted is remembered) */
            else if (n || !tide_mu[k])                   /* (b10: a factory multi's part with no sound of its colour keeps the end it left: it travels between its own two) */
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
        if ((seq >> k) & 1u)
            seqm_wander_gen(k, x, (uint32_t)mm_tier);
    }
}

/* ---------------------------------------------------------------- the footer --- */
/* the sound part pi plays now by name (b holds 20): the end its fader is nearer, the factory sound MORPH TO or JRNY put there
 * by its label ("#31 DEEP PAD", as the Original's "#24 REESE BASS"); else (the Original, or a roll of it) the part's own label */
static int32_t tide_heard_to(uint32_t pi)                /* the factory sound part pi's heard end was moved to (MORPH TO, JOURNEY), -1 none */
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    return mp->n[0] && !morph_stale(&trk[pi % NSHOWN], mp) ? morph_to_of(pi, mp->fader >= 64u) : -1;
}
static void tide_sound_name(uint32_t pi, int32_t k, char *b);
static void tide_part_name(uint32_t pi, char *b) { tide_sound_name(pi, tide_heard_to(pi), b); }
/* b10 TIDE identity (item 6): the sound at end `end` of part pi by name (b holds 20), as the footer names the heard one: TRAVEL names where it goes. JOURNEY
 * picks a target as the end is left (glide_arrive, glide_start), so the trip's end already holds it: this only reads it, the sound is not changed */
static void tide_end_name(uint32_t pi, uint32_t end, char *b)
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    tide_sound_name(pi, mp->n[0] && !morph_stale(&trk[pi % NSHOWN], mp) ? morph_to_of(pi, end & 1u) : -1, b);
}
static void tide_sound_name(uint32_t pi, int32_t k, char *b)
{
    const track_t *t = &trk[pi % NSHOWN];
    if (k >= 0)                                          /* (the destination keeps the number its slot has: audit C11; upreset.c ds_label) */
        ds_label(t->eng_req % NENGINES, (uint32_t)k, b);
    else
        sound_label(t, b);
}
/* HOME's three pages (AUTO TIDE, TIDE SETTINGS, MORPH) show both parts in the footer, part 1 in row 1, part 2 in row 2 (ui_draw.c draw_foot), each as the sound it
 * plays, so a JRNY's move to another sound shows. 0: not these pages */
static int tide_foot_parts(void)
{
    uint32_t g = cur_page()->graph;
    return NSHOWN >= 2u && !ui.perf && !ui.layer && !ui.ml.on && (g == GR_TIDE || g == GR_TIDELIST || g == GR_MORPH);
}
/* .. AUTO TIDE, TIDE SETTINGS and (b6, owner 2026-11-09) the MORPH page, whose footer has its two rows, the same on the three pages:
 * part 1 on row 1, part 2 on row 2, each its engine in its colour and the sound it plays (owner, 2026-11-01: as JAM, now AUTO TIDE, did) */
static int tide_foot_two(void)
{
    uint32_t g = cur_page()->graph;
    return tide_foot_parts() && !ui.menu && !ui.guide && !ui.confirm && (g == GR_TIDE || g == GR_TIDELIST || g == GR_MORPH);
}

/* ---------------------------------------------------------------- the cards --- */
/* template 9 (owner, 2026-11-01): knob 1 is the page's LINE, knobs 2 .. 4 the card row. TIDE: GLIDE's line says what the glide does
 * (the status line's words: "EVOLVING TO B  3/8", "GLIDE ON THE NEXT BAR"; at rest the gesture, A<>B, and at the right the part(s) it
 * moves); MODE, TIME, WAIT the cards. TIDE SETTINGS: knob 1 the row picked (SETTING), no cards */
static void tide_status(char *l);
static int tide_arc_text(char *l);
/* b12 (design overhaul part 5: AUTO TIDE is TIDE's home): the answer on knob 1's line, unless a take records (its TAKE  BAR n/m stays). Waiting for the bar line:
 * ANSWER NEXT BAR in sand (armed); live, while no glide runs (glide: 1, its trip keeps the line): the answer's words in the answering part's colour, until a load or ANSWER OFF */
static void tide_answer_line(char *st, int glide)
{
#if TIDE_HOME_ANSWER
    char b[48];
    uint32_t pr;
    if (!tk_m.ans || take_line(&pr) == 2u || (glide && !tk_pend))
        return;
    b[0] = 0;
    if (tk_pend)
        str_cpy(b, "ANSWER NEXT BAR", sizeof b);
    else
        tk_words(b);
    str_cpy(st, b, 40);
    k1_after_col = tk_pend ? ux.gth : T_PTHEME(tk_m.ans - 1u);
#else
    (void)st, (void)glide;
#endif
}
static void tide_columns(void)
{
    static char st[40];
    char val[12];
    const char *unit;
    int32_t g;
    if (cur_page()->graph == GR_TIDELIST) {
        const char *nd = jl_needs(jl_sel % JL_N);
        k1_after_next = nd ? nd : PLS_NAME[play_pos()];  /* a row that does nothing in this setup says what it needs (at the line's right end, as the status); else the play position (b10: the title names the page) */
        k1_after_col = T_MID;
        k1_after_r = 1;
        draw_column(0, "SETTING", JL_NAME[jl_sel % JL_N], "", VAL(0u), -1, ICON_NONE);
        return;                                          /* (no cards: the list and its hint own the card row; hiding them cleared it under the list on every redraw, a flicker while the colour follows a glide) */
    }
    g = morph_pending(TIDE_G_GLIDE);
    if (glide_running()) {                               /* (I5: running, knob 1's line is the gesture as at rest and the ARC's words and the trip at its right; the water shows how far, graph_tide) */
        char dn[16];
        str_cpy(val, g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", sizeof val);
        st[0] = 0;                                       /* b11 (design review 2.2): the status on knob 1's line, as MORPH's: the ARC's phase (b12: the page draws no ARC), then the trip */
        if (!gl.armed && song.playing && sqm_arc && sqm_auto && tarc.run) {
            str_cpy(st, tarc_name(), sizeof st);
            str_cpy(st + str_len(st), "  ", 4);
        }
        tide_trip_short(st + str_len(st));
        if (tide_arc_text(dn) && str_len(st) + str_len(dn) + 2u < sizeof st) {   /* (the hand's DROP NEXT) */
            str_cpy(st + str_len(st), "  ", 4);
            str_cpy(st + str_len(st), dn, 16);
        }
        k1_after_next = st;
        k1_after_col = T_ACCENT;
        k1_after_r = 1;
        tide_answer_line(st, 1);
        draw_column(0, "GLIDE", val, "", g ? T_ACCENT : VAL(0u), -1, ICON_NONE);
        goto cards;
    }
    tide_status(st);
    if (glide_running()) {
        draw_column(0, "GLIDE", st, "", T_ACCENT, -1, ICON_NONE);
    } else {
        str_cpy(val, g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", sizeof val);
        k1_after_next = st;                              /* (PART 1, PART 2, BOTH PARTS at the line's right end) */
        k1_after_col = T_MID;
        k1_after_r = 1;
        tide_answer_line(st, 0);
        draw_column(0, "GLIDE", val, "", g ? T_ACCENT : VAL(0u), -1, ICON_NONE);
    }
cards:
    draw_column(1, "MODE", GL_MODE_NAME[gl.mode % 5u], "", VAL(1u), -1, ICON_NONE);
    fmt_int(val, (int32_t)glide_bars());
    draw_column(2, "TIME", val, glide_bars() == 1u ? "BAR" : "BARS", VAL(2u), -1, ICON_NONE);
    tide_wait_text(val, sizeof val, &unit);
    draw_column(3, "WAIT", val, unit, VAL(3u), -1, ICON_NONE);
}

/* b10 TIDE identity: the tide line's state (ui_draw.c tide_line_draw): 0 off, 1 armed, 2 running, 3 waiting (slack water), 4 paused; the trip's progress (Q16),
 * its beat, the parts it moves */
static int tide_line_state(uint32_t *prog, uint32_t *beat, uint32_t *mask)
{
    uint32_t tk = take_line(prog);                       /* b10 round 9: a take records: the line is REC red (5 the count-in, 6 recording) */
    if (tk)
        return (int)tk + 4;
    if (!glide_running())
        return 0;
    *mask = gl.mask & 3u ? gl.mask & 3u : 3u;
    if (gl.armed)
        return 1;
    *prog = gl.prog;
    *beat = gl.beats;
    return !song.playing ? 4 : gl.hold ? 3 : 2;
}

/* ---------------------------------------------------------------- the panels --- */

/* "a/b" appended to l */
static __attribute__((noinline)) void tide_frac(char *l, uint32_t a, uint32_t b)
{
    l += str_len(l);
    fmt_int(l, (int32_t)a);
    l += str_len(l);
    *l++ = '/';
    fmt_int(l, (int32_t)b);
}
/* AUTO TIDE's status line: what the glide does now, with its bar ("EVOLVING TO B  3/8", "WAITING  1/4"); stopped, the part(s) */
static __attribute__((noinline)) void tide_status_arc(char *l, int arc);
static void tide_status(char *l) { tide_status_arc(l, 1); }
/* b10 round 9: knob 1's line while a take records: TAKE  BAR 3/4 (0: no take is recording) */
static uint32_t take_text(char *l)
{
    uint32_t pr = 0, p = song.rec & 1u ? 0u : 1u, spb = tk_spb(&trk[p]), len = TIDE_PLAY_LEN(&trk[p]);
    if (take_line(&pr) != 2u)
        return 0;
    str_cpy(l, "TAKE  BAR ", 40);
    tide_frac(l, TIDE_PLAY_IDX(&trk[p]) / spb + 1u, len / spb ? len / spb : 1u);
    return 1;
}
/* b10 Phase B (I4, I5): the trip without the ARC's words ("EVOLVING TO B  3/16", "WAITING  1/2"; armed, paused, a hand as tide_status) */
static __attribute__((unused)) void tide_trip(char *l) { tide_status_arc(l, 0); }
static __attribute__((unused)) void tide_trip_short(char *l) { tide_status_arc(l, 2); }   /* (the wave page's knob-1 line: no "BAR", it has less room) */
/* .. and whether the glide moves part pi's sound now (the wave page says so on knob 1's line) */
static __attribute__((unused)) int tide_moves(uint32_t pi) { return glide_running() && ((gl.on >> GT_SOUND) & 1u) && ((gl.mask >> pi) & 1u); }
/* .. the ARC's words alone ("ARC RISE 1/3", "ARC PEAK  DROP NEXT"), 0: no ARC running */
static __attribute__((unused)) int tide_arc_text(char *l)
{
    if (!(glide_running() && !gl.armed && song.playing && sqm_arc && sqm_auto && tarc.run))
        return 0;
    if (cur_page()->graph == GR_TIDE) {                  /* (on AUTO TIDE knob 1's line names the phase already, tide_columns: only the hand's DROP NEXT here) */
        if (!(tarc.hand && tarc.phase != AP_DROP))
            return 0;
        str_cpy(l, "DROP NEXT", 40);
        return 1;
    }
    str_cpy(l, "ARC ", 40);
    str_cpy(l + 4, tarc_name(), 36);
    if (tarc.phase != AP_PEAK && tarc.phase != AP_DROP) {
        str_cpy(l + str_len(l), " ", 4);
        tide_frac(l, tarc.len - tarc.left + 1u, tarc.len);
    }
    if (tarc.hand && tarc.phase != AP_DROP)
        str_cpy(l + str_len(l), "  DROP NEXT", 16);
    return 1;
}
static __attribute__((noinline)) void tide_status_arc(char *l, int arc)
{
    uint32_t bars = gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_trip_bars(), at = ((gl.prog * bars) >> GLIDE_Q) + 1u;
    if (take_text(l))
        return;                                          /* b10 round 9: a take records: TAKE  BAR 3/4 */
    if (!glide_running())
#if TIDE_PLAYSEL
        str_cpy(l, PLS_NAME[play_pos()], 40);              /* (1.0.0-b7: the selection is the play mode: PART 1, LAYER, STEREO, DUEL, PART 2) */
#else
        str_cpy(l, song.both ? "BOTH PARTS" : song.sel ? "PART 2" : "PART 1", 40);
#endif
    else if (gl.armed)
        str_cpy(l, song.playing || transport_req == 1u ? "GLIDE ON THE NEXT BAR" : "GLIDE STARTS WITH PLAY", 40);
    else if (!song.playing)
        str_cpy(l, "PAUSED: PLAY GOES ON", 40);
    else if (((gl.hw[0] | gl.hw[1]) >> song.sel) & 1u) {   /* MR7: a hand holds the selected part: the bars before TIDE takes it back */
        uint32_t q = gl.hq[song.sel], need = q < 8u ? 8u - q : 0u;
        need += (4u - ((sqm_bar + need) & 3u)) & 3u;
        str_cpy(l, "HAND  BACK IN ", 40);
        fmt_int(l + str_len(l), (int32_t)(need ? need : 4u));
        str_cpy(l + str_len(l), need == 1u ? " BAR" : " BARS", 8);
    } else if (arc == 1 && sqm_arc && sqm_auto && tarc.run) {   /* ARC: where the shape is ("RISE 3/6", "PEAK", "DROP", "BREAK 2/3"); not in the trip (I4, I5) */
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
        str_cpy(l, gl.hold ? "WAITING" : gl.mode == GM_JOURNEY ? "TRAVEL" : "EVOLVING", 40);   /* (b10: ONCE / LOOP / DRIFT show as EVOLVE everywhere else) */
        if (arc == 2 && !gl.hold)                        /* (the wave page's short line: "TO B  3/16", the fader's number before it leaves no room for the mode word) */
            l[0] = 0;
        if (!gl.hold && gl.mode == GM_JOURNEY && ((gl.on >> GT_SOUND) & 1u)) {   /* (b10 item 6: TRAVEL names the sound it goes to: "TRAVEL TO #168 VOID SHIMMER  2/16";
                                                                                   * the selected part's, else the first it moves; the wave page's short line: the name alone) */
            char nm[24];
            uint32_t pi = ((gl.mask >> song.sel) & 1u) ? song.sel : (gl.mask & 1u) ? 0u : 1u;
            tide_end_name(pi, gl.to, nm);
            str_cpy(l + str_len(l), arc == 2 ? "TO " : " TO ", 8);
            str_cpy(l + str_len(l), nm, 24);           /* (b10: with its number on the wave page too; ui_draw.c k1_line shortens the line to fit) */
            str_cpy(l + str_len(l), "  ", 4);
        } else {
            if (!gl.hold)
                str_cpy(l + str_len(l), (gl.to ? " TO B" : " TO A") + (arc == 2), 8);
            str_cpy(l + str_len(l), arc ? "  " : "   BAR ", 8);
        }
        tide_frac(l, at > bars ? bars : at, bars);
    }
}

/* the body, screen rows TB_Y0 .. TB_Y1 - 1 between the knob-1 line and the card row (lifted TIDE_LIFT), one canvas */
#define TB_Y0 UT_A_GR_Y
#define TB_Y1 (UT_A_CARD_Y - TIDE_LIFT)

/* The selected part (song.sel; with BOTH both parts: ui_part_hot) is shown in full, the other dimmed (the colour mixed 60 % into the background; MONO: its grey): its steps and its water */
static uint16_t tide_pc(uint32_t k)
{
    return ui_part_col(k, ui_part_hot(k));             /* (ui_scope.c: the MORPH page's and this page's one tag; BOTH: both in full) */
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
typedef struct { uint8_t k, n, maybe, top; } tv_t;   /* (b11 note strip: top, the pitch it plays + 1, ui.c pv_top) */
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
    uint32_t len = sqm_len(t), L = len, ph = TV_NOPH, pos = 0, per = 0, i, j, m;
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
        e->k = TV_END, e->n = e->maybe = e->top = 0;
        if (idx >= L)
            continue;
        if (ph != TV_NOPH) {
            for (j = idx < ph ? idx : ph; j < (idx > ph ? idx : ph); j++)
                at += (idx > ph ? 1 : -1) * (int32_t)phrase_duration(t, j, per, (int)arp, 0);
            bar = tv_bar(at + (arp ? bl / 32 : 0), ob, bl);   /* (an ARP SEQ step from the bar's last 32nd on plays in the next: seq_morph.c sqm_bar_of; lane ARC) */
        }
        v = seq_play_step_at(t, idx, bar);
        e->n = v->n;
        e->top = pv_top(v, arp);
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

/* a part's 16 steps around its playhead, standing on row y (bars up to hmax tall: 9), in the footer's
 * columns (bar_x) and its rounded bars: a hit a bar (a chord taller) in the part's colour, the accent where it varies, a lit outline
 * where it is added, a dim outline where the variation, the thinning or a chance roll drops it; a tie a short bar, an empty step a
 * stub, the steps past the pattern's end a line; the playing step underlined. A chance step not rolled yet is striped. As they play
 * now (tide_views) */
/* b11 note strip: the states of AUTO TIDE's 16 steps (page) as they play: the class from the views (a hit: a note, a chord with more), the pitch rank
 * from the notes they play (an added or moved note: its own, not the stored rest's), the accent from the stored step */
static void tide_strip_states(const track_t *t, const tv_t *tv, uint32_t page, uint8_t *st)
{
    uint8_t acc[16], top[16];
    uint32_t i;
    ns_states(acc, t, page * 16u);
    for (i = 0; i < 16u; i++) {
        const tv_t *e = &tv[i];
        st[i] = e->k >= TV_HIT ? (uint8_t)((e->n > 1u ? NS_CHORD : NS_NOTE) | (acc[i] & 8u)) : 0u;
        top[i] = e->k >= TV_HIT ? e->top : 0u;
    }
    pv_rank(st, top, 16u);
}
static void tide_steps(const track_t *t, int32_t y, int32_t hmax, uint16_t c)
{
    tv_t tv[16];
    uint8_t st[16];
    uint32_t ph = tide_views(t, 0xFFu, tv), page = ph == TV_NOPH ? 0u : ph / 16u, i;
    int32_t r;
    tide_strip_states(t, tv, page, st);
    for (i = 0; i < 16u; i++) {
        int32_t x = bar_x(i), h = 3;
        const tv_t *e = &tv[i];
        if (e->k >= TV_HIT)
            h = ns_h(st[i], hmax);
        if (e->k == TV_END)
            ns_step(x, y, hmax, NS_END, c);
        else if (e->k == TV_ADD)
            cv_frame(x, y - h, 9, h, ux_mix(c, T_TEXT, 45));   /* (AUTO TIDE's own states over the standard: added outlined, moved filled, both in the lane's colour lightened
                                                              * (b12: not the accent: it is part 1's sea glass, so a moved step in part 2's lane read as part 1's); dropped dim) */
        else if (e->k >= TV_HIT)
            ns_step(x, y, hmax, st[i], e->k == TV_MOVE ? ux_mix(c, T_TEXT, 45) : c);
        else if (e->k == TV_TIE)
            ns_step(x, y, hmax, NS_TIE, c);
        else if (e->k == TV_DROP)
            cv_frame(x, y - 6, 9, 6, T_DIM);
        else
            ns_step(x, y, hmax, NS_REST, c);
        if (e->maybe && e->k != TV_ADD && e->k != TV_END)
            for (r = 1; r < h; r += 2)
                cv_rect(x, y - h + r, 9, 1, aw_wet ? aw_under(y - h + r) : T_BG);   /* (the gaps show the water where the step stands in it) */
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

/* TIDE's body (b12, owner 2026-11-13: "the button to trigger the glide, mode, time and wait are fine, then the sequence; the others are clutter"; he picked RISING WATER,
 * "it's like a tide", then "make it more water like"): each part's steps, and behind them the sea standing as high as that part's tide: low water (the lane's foot) at A,
 * high water (near its top) at B. Part 1's lane above, part 2's below; the selected part in full, the other dimmed (tide_pc). LANGUAGE.md 5, the water line:
 *  - the surface: a swell of two sines (the splash sea's way, sine_i: 96 px and a 41 px ripple), 1.25 px anti-aliased, the crests catching the light (toward white).
 *    It always has its shape: rolling at the splash sea's far-line pace while the tide moves (2^32 a turn in 11.5 s), at half that in slack water (WAIT, in moonlight),
 *    held still (and dashed) while armed or in a take's count-in, held still and dim while stopped or paused; a take records in red;
 *  - the body: three flat tones of the part's colour, darker lower down (no gradient: owner, B11 review);
 *  - the steps stand in it, 6 .. 32 px by pitch, their corners and gaps blending with the water's tone (ui_graph.c ns_rr, aw_under).
 * Rolling, the main loop draws each lane again with the page's picture step (mv_tick, ~16 a second) and sends only the rows that changed (tide_water); nothing in the
 * audio interrupt. The page's own redraw draws the same lanes from the same phase */
#define AW_Y0(k) (TB_Y0 + 47 * (int32_t)(k))            /* part k's lane: rows AW_Y0 .. + 46 (the body is rows 48 .. 141) */
#define AW_ROWS 47
#define AW_TOP 5                                         /* high water: the surface's row in the lane (the crests 1 1/2 px above it, never at the lane's top) */
#define AW_BOT 45                                        /* low water: the surface on the lane's last row but one (the swell clear of the playhead marks) */
#define AW_RATE 372827u                                  /* the splash sea's far lines: 2^32 a turn in 11.5 s (ms) */
#define AW_A1 16                                         /* the swell, 1/16 px each way: 1 px, and a ripple of 0.4 px */
#define AW_A2 6
static uint32_t aw_t, aw_t2, aw_ms, aw_tk;               /* the swell's phase and the ripple's (2^32 a turn); fm1_ms and the picture they were moved at */
static int32_t aw_sy[NSHOWN];                            /* each lane's surface row (its mean) at the last draw */
static uint16_t aw_tone[NSHOWN][3];                      /* .. its three tones, top to deep */
static uint8_t aw_k;                                     /* the lane being drawn (aw_under) */
static int aw_rolls(int st) { return st == 2 || st == 3 || st == 6; }   /* (moving, slack water, a take recording) */
static uint16_t aw_under(int32_t y)
{
    const int32_t d = y - aw_sy[aw_k];
    return d <= 0 ? T_BG : aw_tone[aw_k][d <= 5 ? 0 : d <= 13 ? 1 : 2];
}
static int32_t aw_cov(int32_t y, int32_t lo, int32_t hi)  /* coverage (0 .. 16) of the 1/16 px span lo .. hi over pixel row y */
{
    const int32_t a0 = y * 16, a1 = a0 + 16, c = (a1 < hi ? a1 : hi) - (a0 > lo ? a0 : lo);
    return c < 0 ? 0 : c > 16 ? 16 : c;
}
static uint32_t aw_fader(uint32_t k) { return gl.target == GT_SEQ ? seqm_fader_of(k) : morph_fader_of(k); }   /* (the fader the glide moves: SOUND, SEQ with TARGET SEQ) */
static void aw_lane(uint32_t k, int32_t y0)
{
    uint32_t prog = 0, beat = 0, mask = 3u;
    const int st = tide_line_state(&prog, &beat, &mask);
    const uint16_t pc = tide_pc(k), sc = st >= 5 ? T_REC : st == 3 ? T_MID : st == 2 ? pc : ux_mix(T_BG, pc, 45);
    const int32_t sy = y0 + AW_BOT - (int32_t)aw_fader(k) * (AW_BOT - AW_TOP) / 127, ybot = y0 + AW_BOT + 1;
    int32_t x, y;
    aw_k = (uint8_t)k;
    aw_sy[k] = sy;
    aw_tone[k][0] = ux_mix(T_BG, pc, 20);
    aw_tone[k][1] = ux_mix(T_BG, pc, 13);
    aw_tone[k][2] = ux_mix(T_BG, pc, 8);
    for (x = 4; x < 236; x++) {
        const int32_t w = (sine_i((uint32_t)x * 44739243u - aw_t) * AW_A1 >> 15) + (sine_i((uint32_t)x * 104755300u - aw_t2 + 0x5A000000u) * AW_A2 >> 15), yc = sy * 16 + 8 + w;
        const int on = !((st == 1 || st == 5) && (x & 7) > 3);   /* (armed, a take's count-in: dashed) */
        const uint16_t lc = w < -(AW_A1 + AW_A2) * 11 / 20 ? ux_mix(sc, T_TEXT, (-w - (AW_A1 + AW_A2) * 11 / 20) * 120 / (AW_A1 + AW_A2) + 30) : sc;   /* (a crest catches the light) */
        for (y = (yc - 12) >> 4; y < ybot; y++) {
            const int32_t d = y - sy, cw = aw_cov(y, yc, ybot * 16), cl = on ? aw_cov(y, yc - 10, yc + 10) : 0;
            uint16_t px = cw ? ux_mix(T_BG, aw_tone[k][d <= 5 ? 0 : d <= 13 ? 1 : 2], cw * 100 / 16) : T_BG;
            if (cl)
                px = ux_mix(px, lc, cl * 100 / 16);
            if (px != T_BG)
                cv_pset(x, y, px);
        }
    }
    aw_wet = 1;
    tide_steps(&trk[k], y0 + 38, 32, pc);
    aw_wet = 0;
}
static void graph_tide(void)
{
    uint32_t k;
    CV_BANDS(TB_Y1 - TB_Y0) {
        cv_begin(240, TB_Y1 - TB_Y0, T_BG);
        cv_bg = T_BG;
        cv_oy = -TB_Y0;
        for (k = 0; k < NSHOWN; k++)
            aw_lane(k, AW_Y0(k));
        cv_oy = 0;
        cv_blit(0, TB_Y0);
    }
    tide_water(1);
}
/* the water rolling: each lane drawn again a picture (the main loop, TIDE_LIVE_STRIP) and only its changed rows sent; whole (the page's redraw): the rows' hashes taken, nothing sent */
static uint32_t aw_rh[NSHOWN][AW_ROWS] __attribute__((section(".pool")));
static void tide_water(int whole)
{
    uint32_t k, r, s, h, i, prog = 0, beat = 0, mask = 3u;
    if (!whole) {
        const uint32_t tk = mv_tick(), dt = fm1_ms - aw_ms;
        const int st = tide_line_state(&prog, &beat, &mask);
        if (!tk || tk == aw_tk)
            return;
        aw_tk = tk;
        aw_ms = fm1_ms;
        if (!aw_rolls(st))
            return;
        aw_t += dt * (st == 3 ? AW_RATE / 2u : AW_RATE);   /* (slack water at half the pace; the phase carries on where it was, the swell never jumps) */
        aw_t2 += dt * (st == 3 ? AW_RATE * 3u / 4u : AW_RATE * 3u / 2u);
    }
    for (k = 0; k < NSHOWN; k++) {
        const int32_t y0 = AW_Y0(k);
        cv_begin(240, AW_ROWS, T_BG);
        cv_bg = T_BG;
        cv_oy = -y0;
        aw_lane(k, y0);
        cv_oy = 0;
        GFX_HOOK_BLIT(0, (uint32_t)y0, 0u);
        for (s = r = 0; r <= AW_ROWS; r++) {
            uint32_t ch = 0;
            if (r < AW_ROWS) {
                const uint16_t *q = cv_px + r * 240u;
                for (h = 2166136261u, i = 0; i < 240u; i++)
                    h = (h ^ q[i]) * 16777619u;
                ch = (aw_rh[k][r] ^ h) && !whole;
                aw_rh[k][r] = h;
            }
            if (!ch) {
                if (s < r)
                    lcd_blit(0, (uint32_t)y0 + s, 240u, r - s, cv_px + s * 240u);
                s = r + 1u;
            }
        }
    }
}
/* TIDE SETTINGS: the settings as the list pages' rows (PATCHES, template 9: UT_L5_*), from row UT_L5_Y0, 20 apart: the name (M) at the left,
 * the value (M, the part's colour; a switch for ON / OFF) at the right, the picked row filled in the part's colour with dark text.
 * Knob 1 picks the row (knob 1's line names it), OCT+ the next value, OCT- the one before */
#define JL_VIS 6u                                        /* rows shown: the list scrolls to keep the picked one in view */
static void graph_tidelist(void)
{
    uint32_t r, top = jl_sel % JL_N >= JL_VIS ? jl_sel % JL_N - (JL_VIS - 1u) : 0u;
    int32_t y0 = UT_L5_Y0 - 2;
    CV_BANDS(UT_L5_PITCH * (int32_t)JL_VIS + 2) {
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
            cv_text_r(xr, ty, &AF_M, v, sel ? T_INK : jl_dimmed(r) ? T_DIM : T_THEME, bg);   /* (a row that does nothing now: dim) */
        }
        if (JL_N > JL_VIS) {                                 /* where the window is in the list: a 2 px mark at the right (audit C12) */
            int32_t hh = UT_L5_PITCH * (int32_t)JL_VIS, th = hh * (int32_t)JL_VIS / (int32_t)JL_N;
            cv_rect(236, UT_L5_Y0, 2, hh, T_RAISE);
            cv_rect(236, UT_L5_Y0 + (hh - th) * (int32_t)top / (int32_t)(JL_N - JL_VIS), 2, th, T_MID);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
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

/* what makes the AUTO TIDE and TIDE SETTINGS pages redraw: the faders, the glide, the steps playing and their variation, the settings, the list */
static uint32_t tide_sig(void)
{
    uint32_t pi = song.sel, bars = gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_trip_bars();
    const track_t *t = TSEL;
    return (morph_fader_of(pi) | seqm_fader_of(pi) << 7 | (uint32_t)gl.on << 14 | (uint32_t)gl.armed << 16 | (uint32_t)gl.target << 17 |
            (uint32_t)gl.mode << 19 | (uint32_t)gl.to << 22 | (uint32_t)gl.hold << 23 | (song.playing ? t->seq_idx : 0xFFu) << 24) ^
           (gl.prog >> 10) * 2654435761u ^ ((gl.prog * bars) >> GLIDE_Q) * 0x27D4EB2Fu ^ sqm_key(&sqm[pi < NSHOWN ? pi : 0u]) * 40503u ^
           ((uint32_t)gl.time | (uint32_t)gl.wait << 4 | (uint32_t)jl_sel << 8 | (uint32_t)sqm_vary_all << 12 | (uint32_t)sqm_poly << 28 | (uint32_t)sqm_sync << 30 |
            (uint32_t)morph.cfg << 16 | (uint32_t)((sqm_bar + sqm_bar_turn) & 3u) << 24 | (uint32_t)song.playing << 26) * 0x9E3779B1u ^
           ((uint32_t)sqm_arc | (uint32_t)tarc.phase << 2 | (uint32_t)tarc.left << 5 | (uint32_t)tarc.hand << 9 | (uint32_t)tarc.len << 10) * 0x2545F491u ^   /* (audit C4: no field ORed over another) */
           ((morph_fader_of(1u - pi) | seqm_fader_of(1u - pi) << 7 | (song.playing ? trk[1u - pi].seq_idx : 0xFFu) << 14) * 0x85EBCA77u ^
            sqm_key(&sqm[(1u - pi) % NSHOWN]) * 0xC2B2AE3Du ^ tide_steps_sig(&trk[1u - pi]) * 0x165667B1u ^ 0x27D4EB2Fu) ^
           tide_steps_sig(t) * 0xD3A2646Cu ^ (uint32_t)morph.seed * 0x1B873593u ^   /* (the seed: a ROLL sets it at once and CALL AND ANSWER's listening bars follow; the ends' key comes later) */
           ((uint32_t)(sqm_bar & 7u) | (uint32_t)tarc.next << 3 | (uint32_t)tarc.run << 6 | (uint32_t)sqm_auto << 7 | (uint32_t)tarc.cnt << 9) * 0x61C88647u ^   /* (b10 item 4: the ARC's phase and bar on knob 1's line, and its fall) */
           ((uint32_t)tk_m.ans | (uint32_t)tk_pend << 2 | (uint32_t)tk_m.mode << 4 | (uint32_t)tk_m.key << 8 | (uint32_t)tk_m.flags << 12) * 0x7FEB352Du;   /* (b12: the answer on knob 1's line) */
}

/* ---------------------------------------------------------------- kept over a power cycle --- */
/* MODE, TIME, WAIT, TARGET, VARY BARS, the ROLL's MUSIC and WASH LEVEL and the rest (TS_*) in TIDE's own record (tide_store.c: the settings record's reserved tail, so
 * every backup holds it; an earlier build's object 48 is read once; plan §4.5), not in the bytes the first build used (1.1 uses them). ARP, PITCH and WILD are the morph's own byte
 * (ui.c ui_morph_cfg). The record is read once at power-on and after a settings restore (none or damaged: the defaults; neither is written back); a change is saved ~2 s after the last one,
 * while stopped and not during ERASE DATA (tide_store_poll) */
enum { TS_MODE, TS_TIME, TS_TARGET, TS_WAIT, TS_VARY, TS_KNOB, TS_WASH, TS_OLD, TS_TURNS = TS_OLD, TS_POLY, TS_SYNC, TS_ARC, TS_STYLE, TS_REC, TS_ANS, TS_ANSON, TS_N };   /* (TS_OLD: the fields of the first record; a record of that length loads, the later fields at their defaults) */
static uint8_t tide_seen[TS_N], tide_seen_ok;
#define TS_TURNS_OLD 1u                                  /* TURNS played nothing since 1.0.0-b7 and has no variable since b8: the record keeps its byte (the same size, so a
                                                          * build that reads it still can), always written as 1 (TAKE TURNS, the old default) and never read (TS_VAR 0) */
/* each field's byte; read back: a value under TS_LIM kept, else TS_DEF (TS_LIM 0: masked with TS_DEF) */
static volatile uint8_t *const TS_VAR[TS_N] = {&gl.mode, &gl.time, &gl.target, &gl.wait, &sqm_vary_all, &tide_knobs, &morph_wlvl, 0, &sqm_poly,
                                               &sqm_sync, &sqm_arc, &tide_style, &tide_rec_replace, &tide_answer, &tide_answer_on};
static const uint8_t TS_LIM[TS_N] = {GM_JOURNEY + 1u, GLIDE_TIMES, GT_BOTH + 1u, GLIDE_WAITS, 0, KS_N + 2u, 0, 3, 3, 3, 3, STY_CUSTOM, 2, TA_N, 2},
                     TS_DEF[TS_N] = {GM_WANDER, 3, GT_SOUND, 0, 1, 1, 3, 1, 1, 1, 0, STY_CUSTOM, 0, TA_BASS, 0};
/* TIDE 1.0.0-b6: while a built-in drone multi (M25, M26: MU_DRONE) is the multi loaded, TIDE plays STYLE DRONE: the eight rows are DRONE's and TIDE SETTINGS
 * shows DRONE. What was set (the eight rows and STYLE) is kept here and comes back when any other multi (or none) is loaded; the store always gets the
 * kept values, so the saved STYLE never changes. A hand edit of a row while the drone multi is loaded lasts until then. Main loop only (no audio cost) */
static uint8_t tide_drone_on, tide_drone_keep[9];       /* (STY's eight rows, then STYLE) */
static void tide_drone(int reread)
{
    uint32_t cs = project_cur_slot(), on = cs < MU_SHOWN && MU_DRONE(cs) && project_factory(cs);   /* (b10 U4: only while the slot shows the factory drone; a multi saved over it is an ordinary one) */
    if (reread)                                          /* (the rows were just read from the store: they are what is kept now) */
        tide_drone_on = 0;
    if (on == tide_drone_on)
        return;
    if (on) {
        tide_rows_get(tide_drone_keep);
        tide_drone_keep[8] = tide_style;
        tide_style_apply(STY_DRONE);
    } else {
        tide_rows_set(tide_drone_keep);
        tide_style = tide_drone_keep[8];
    }
    tide_drone_on = (uint8_t)on;
}
static void tide_settings(void)
{
    uint8_t w[TS_N];
    uint32_t i;
    int reread = !tide_seen_ok || tide_tail_new;
    if (tide_tail_new) {                                 /* a settings restore (editor_backup.c) brought other fields: read them again */
        tide_tail_new = 0;
        tide_seen_ok = 0;
    }
    if (!tide_seen_ok) {
        uint8_t b[TS_N] = {[TS_TURNS] = 1u, [TS_POLY] = 1u, [TS_SYNC] = 1u, [TS_ARC] = 0u, [TS_STYLE] = STY_CUSTOM, [TS_REC] = 0u, [TS_ANS] = TA_BASS, [TS_ANSON] = 0u};   /* (the defaults of the fields an older record lacks) */
        uint32_t have = tide_store_load_min(b, TS_N, TS_OLD);
        if (have) {                                      /* (an older record: STYLE CUSTOM, its values kept; a stored ECHO reads as LEAD: D7) */
            for (i = 0; i < TS_N; i++)
                if (i != TS_TURNS)
                    *TS_VAR[i] = (uint8_t)(!TS_LIM[i] ? b[i] & TS_DEF[i] : b[i] < TS_LIM[i] ? b[i] : TS_DEF[i]);
            if (gl.mode < GM_WANDER)                     /* (b9: a stored ONCE, LOOP or DRIFT is no choice any more: EVOLVE; the record's byte is rewritten only when a row changes) */
                gl.mode = GM_WANDER;
            if (tide_style == STY_DRONE && !tide_style_holds())   /* (b4 and later: DRONE is 5, CUSTOM 6; an earlier build stored CUSTOM as 5: not DRONE's rows, so CUSTOM, not a hand edit of DRONE) */
                tide_style = STY_CUSTOM;
            else if (tide_style < STY_CUSTOM) {          /* (1.0.0-b7: a stored STYLE whose hand values are its own takes the style's values as this build has them, the ones
                                                          * with no row too: b6's AMBIENT (POLY GENTLE) stays AMBIENT, b6's DUB (JOURNEY) DUB; other values: CUSTOM, below) */
                uint8_t m = gl.mode;
                if (tide_style == STY_DUB && m == GM_JOURNEY)
                    gl.mode = GM_WANDER;
                if (tide_style_holds())
                    tide_style_apply(tide_style);
                else
                    gl.mode = m;
            }
        } else {
            tide_style_apply(STY_DUB);                   /* (no record: the DUB showcase; REC ADD) */
            tide_rec_replace = 0;
            tide_answer = TA_BASS, tide_answer_on = 0;
        }
    }
    tide_drone(reread);
    if (tide_style < STY_CUSTOM && !tide_style_holds()) {
        tide_style_last = tide_style;
        tide_style = STY_CUSTOM;                         /* a hand edit of any of the eight rows (knobs, rows, resets, MIDI) */
        if (!reread)
            ui_say("STYLE ", "CUSTOM");                  /* (b10: the player is told; not when a record is read) */
    }
    for (i = 0; i < TS_N; i++) {
        w[i] = i == TS_TURNS ? TS_TURNS_OLD : *TS_VAR[i];
        if (tide_drone_on)                               /* (a drone multi loaded: the kept rows and STYLE are what is saved) */
            for (uint32_t j = 0; j < 9u; j++)
                if (j < 8u ? TS_VAR[i] == TIDE_ROW[j] : i == TS_STYLE)
                    w[i] = tide_drone_keep[j];
    }
    w[TS_WASH] &= 3u;
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
    sqm_thin_poll();                                     /* LEAD and ARC: the follower's mask (when what it reads changed) */
    glide_poll();                                        /* a launched GLIDE moves its faders */
    seqm_poll();                                         /* the SEQ MORPH's ends follow pattern edits (TRANS, SHIFT, an edit) */
    tide_ghost_poll();                                   /* MULTIS lane (b7): the ghost chord of a HOLD ALL part under AUTO TIDE (tide_seq.c) */
    morph_xpoll();                                       /* plan any destination GLIDE generated in this same pass */
    tide_store_poll();
    take_poll();                                         /* b10 round 8: a pass of the live recorder opens or closes (tide_take.c) */
}
