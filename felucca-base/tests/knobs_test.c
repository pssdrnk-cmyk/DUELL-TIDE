/* SPDX-License-Identifier: GPL-3.0-only */
/* 1.0.0-b9: HOME page 1's genre knob sets (knob_sets.h; ui_input.c morph_knob, reset_morph; tide_ui.c kn_cur and the KNOBS row).
 *   MACROS    for every set and every knob K2..K4: eight detents move exactly the macro's parameters, each by its own factor and sign (the quarter steps: 4 a step a detent, 2 every second
 *             detent, 1 every fourth), every other macro parameter of both parts and the other part's stay as they were (a card the selection hides: not moved)
 *   OCT-      OCT- + the knob puts all of the macro's parameters back to the loaded sound's values
 *   BOTH      each part moves from its own value by the same steps; a global (the delay's TIME, FDBK, COLR) moves once, not once per part
 *   PUMP      a ducker that is OFF starts as PUMP when the knob goes up, and OCT- puts the OFF back
 *   AUTO      the set comes from the first word of the music's name (DUB HSE TECH ACID ELEC AMB); any other first word: STD; a set picked on the KNOBS row wins
 *   RECORD    an older settings record (the byte ROLL TYPE had, 0 or 1) reads as AUTO; a stored set is kept; the KNOBS row's values and names
 * Built and run by DUELL/host_tests.sh (name: knobs): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#define UI_TEXT_CAPTURE 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        bad++;
        printf("knobs: %-100s FAIL\n", what);
    }
    return ok;
}
static void settle(void) { frames(600); }
/* a fresh unit with set `set` (tide_knobs as the KNOBS row stores it) and every macro parameter at a middle value, different on the two parts, taken as the loaded sound's */
static void setup(uint32_t set, uint32_t both)
{
    uint32_t q, k;
    ui_power_on();
    frame();
    song.playing = 0;
    project_load(0);
    frames(32);
    tide_knobs = (uint8_t)(set + 2u);
    ui.menu = 0;
    go_title("MORPH");
    frames(32);
    mods_off = 0;
    for (q = 0; q < 2u; q++)
        for (k = 0; k < KN_NP; k++) {
            const param_desc_t *d = track_desc(&trk[q], KN_ID[k]);
            trk[q].p[KN_ID[k]] = (int16_t)((d->min + d->max) / 2 + (int32_t)q * 3);
        }
    for (k = KN_NP; k < KN_NID; k++) {
        const param_desc_t *d = glob_desc(KN_ID[k]);
        song.g[KN_ID[k]] = (int16_t)((d->min + d->max) / 2);
    }
    trk[0].p[P_DKSRC] = trk[1].p[P_DKSRC] = 0;           /* (a PUMP ducker: its depth card shows) */
    trk[0].p[P_LSYNC] = trk[1].p[P_LSYNC] = 0;           /* (a free-running LFO: its RATE card shows) */
    song.g[G_DMIX] = 64;
    SEL_BOTH = (uint8_t)both;
    song.sel = 0;
    h1_take(0);
    h1_take(1);
    frames(32);
}
/* the value a parameter reaches from v by `steps` single steps (the sign is the direction) */
static int32_t walk(const param_desc_t *d, int32_t v, int32_t steps)
{
    for (; steps > 0; steps--)
        v = param_turn(d, v, 1);
    for (; steps < 0; steps++)
        v = param_turn(d, v, -1);
    return v;
}
static int16_t *cell(uint32_t q, uint32_t k) { return k < KN_NP ? &trk[q].p[KN_ID[k]] : &song.g[KN_ID[k]]; }
static const param_desc_t *desc(uint32_t k) { return k < KN_NP ? track_desc(&trk[0], KN_ID[k]) : glob_desc(KN_ID[k]); }
static void snap(int16_t s[2][KN_NID])
{
    uint32_t q, k;
    for (q = 0; q < 2u; q++)
        for (k = 0; k < KN_NID; k++)
            s[q][k] = *cell(q, k);
}
/* the macro (set, knob 0..2) turned by `det` detents on the selected part (part 0): its entries moved by factor * det / 4 steps, nothing else */
static int macro_exact(uint32_t set, uint32_t knob, int32_t det, const char *tag)
{
    int16_t b[2][KN_NID], a[2][KN_NID];
    uint32_t k, j, ok = 1, moved = 0;
    char w[96];
    setup(set, 0);
    snap(b);
    turn(EN_K2 + knob, det);
    snap(a);
    for (k = 0; k < KN_NID; k++) {
        int32_t want = b[0][k];
        for (j = 0; j < 3u; j++)
            if (KN_E[set][knob][j].mul && KN_E[set][knob][j].ix == k)
                want = walk(desc(k), b[0][k], det * KN_E[set][knob][j].mul / 4);
        ok &= a[0][k] == want;
        if (k < KN_NP)
            ok &= a[1][k] == b[1][k];                    /* (the other part: unchanged; a global is one cell) */
        moved += a[0][k] != b[0][k];
    }
    snprintf(w, sizeof w, "%s: set %s knob %u, %d detents: exactly the macro's parameters move, by their factors", tag, KN_L[set][knob], knob + 2u, det);
    return expect(w, ok && moved >= 1u);
}
/* the design's table (work/b9/QUICK_KNOBS_DESIGN.md v2) as parameters, factors in quarter steps and signs, written out here on its own so the macro table cannot drift from it unseen */
static const struct { uint8_t id; int8_t mul; } SPEC[KS_N][3][3] = {
    {{{P_FREEA, 4}}, {{P_FREEB, 4}}, {{P_DLY, 4}}},
    {{{P_DLY, 14}, {P_REV, 3}}, {{G_DFDBK, 16}, {G_DCOLOR, -8}, {P_DLY, 8}}, {{G_DTIME, 1}}},
    {{{P_FREEA, 14}, {P_FREEB, 3}}, {{P_DKDEP, 24}, {P_REV, -6}, {P_DLY, -3}}, {{P_REV, 20}, {P_REL, 3}}},
    {{{P_FREEA, 9}, {P_FREEB, 3}, {P_E2FLT, 3}}, {{P_DEC, 6}}, {{P_REV, 12}, {P_DLY, 4}}},
    {{{P_FREEA, 6}, {P_FREEB, 3}}, {{P_E2FLT, 8}, {P_FREEA, 4}, {P_DEC, -4}}, {{P_DIST, 12}}},
    {{{P_FREEA, 8}}, {{P_DIST, 16}, {P_CHOR, 4}}, {{P_AGATE, -8}, {P_SGATE, -8}, {P_APROB, -12}}},
    {{{P_ATK, 8}, {P_REL, 8}, {P_REV, 4}}, {{P_LD_FLT, 14}, {P_LRATE, -2}, {P_FREEA, -6}}, {{P_DLY, 16}, {P_REV, 16}}}};
int main(void)
{
    uint32_t set, knob, k, j;
    int16_t b[2][KN_NID], a[2][KN_NID];
    setvbuf(stdout, NULL, _IONBF, 0);
    {
        uint32_t ok = 1;
        for (set = 0; set < KS_N; set++)
            for (knob = 0; knob < 3u; knob++)
                for (j = 0; j < 3u; j++) {
                    ok &= KN_E[set][knob][j].mul == SPEC[set][knob][j].mul;
                    ok &= !SPEC[set][knob][j].mul || KN_ID[KN_E[set][knob][j].ix] == SPEC[set][knob][j].id;
                }
        expect("the macro table is the design's: each knob's parameters, factors and signs", ok);
    }
    /* every set, every knob: forward and back */
    for (set = 0; set < KS_N; set++)
        for (knob = 0; knob < 3u; knob++) {
            macro_exact(set, knob, 8, "forward");
            macro_exact(set, knob, -8, "back");
        }
    /* OCT- + the knob: the macro's parameters back to the sound's values (the others stay where the hand put them) */
    for (set = 0; set < KS_N; set++)
        for (knob = 0; knob < 3u; knob++) {
            char w[96];
            uint32_t ok = 1, u;
            setup(set, 0);
            snap(b);
            turn(EN_K2 + knob, 8);
            settle();
            for (u = K_CHO; u > 0u; u--) {               /* (a parameter in no macro of this knob, moved by hand: it stays) */
                uint32_t in = 0;
                for (j = 0; j < 3u; j++)
                    in |= KN_E[set][knob][j].mul && KN_E[set][knob][j].ix == u;
                if (!in)
                    break;
            }
            *cell(0, u) = (int16_t)(b[0][u] + 5);
            snap(a);
            btn_down(B_OCTDN);
            frame();
            turn(EN_K2 + knob, 1);
            btn_up(B_OCTDN);
            frames(64);
            for (j = 0; j < 3u; j++)
                if (KN_E[set][knob][j].mul)
                    ok &= *cell(0, KN_E[set][knob][j].ix) == b[0][KN_E[set][knob][j].ix];
            for (k = 0; k < KN_NID; k++) {
                uint32_t in_macro = 0;
                for (j = 0; j < 3u; j++)
                    in_macro |= KN_E[set][knob][j].mul && KN_E[set][knob][j].ix == k;
                if (!in_macro && k != K_DKS)
                    ok &= *cell(0, k) == a[0][k];
                if (k == u)
                    ok &= a[0][k] == b[0][k] + 5;
            }
            snprintf(w, sizeof w, "OCT- + K%u in set %s (%s): all of the macro's parameters back to the sound's, the others kept", knob + 2u, KN_L[set][0], KN_L[set][knob]);
            expect(w, ok);
        }
    /* BOTH: each part from its own value by the same steps (AC: cutoff 6 and resonance 3 quarter steps a detent) */
    setup(KS_AC, 1);
    snap(b);
    turn(EN_K2, 8);
    snap(a);
    expect("BOTH, ACID K2: cutoff +12 and resonance +6 on each part from its own value",
           a[0][K_FLT] == walk(desc(K_FLT), b[0][K_FLT], 12) && a[1][K_FLT] == walk(desc(K_FLT), b[1][K_FLT], 12) &&
           a[0][K_RES] == walk(desc(K_RES), b[0][K_RES], 6) && a[1][K_RES] == walk(desc(K_RES), b[1][K_RES], 6) && b[0][K_FLT] != b[1][K_FLT]);
    /* globals once: DB's FDBK + COLR, and TIME, with BOTH */
    setup(KS_DB, 1);
    snap(b);
    turn(EN_K3, 8);
    snap(a);
    expect("BOTH, DUB K3: the delay's FDBK moves 32 once (not 64), COLR 16 down once, the echo send 16 on each part", a[0][K_FDB] == walk(desc(K_FDB), b[0][K_FDB], 32) && a[0][K_COL] == walk(desc(K_COL), b[0][K_COL], -16) &&
           a[0][K_FDB] != walk(desc(K_FDB), b[0][K_FDB], 64) && a[0][K_DLY] == walk(desc(K_DLY), b[0][K_DLY], 16) && a[1][K_DLY] == walk(desc(K_DLY), b[1][K_DLY], 16));
    settle();
    snap(b);
    turn(EN_K4, 8);
    snap(a);
    expect("BOTH, DUB K4: TIME moves once (eight detents, two steps: a quarter step a detent)", a[0][K_TIM] == walk(desc(K_TIM), b[0][K_TIM], 2) && a[0][K_TIM] != walk(desc(K_TIM), b[0][K_TIM], 4));
    /* the macro's first parameter is the one the row shows: every macro has one, and STD (the page as it was) keeps a step a detent */
    for (set = 0; set < KS_N; set++)
        for (knob = 0; knob < 3u; knob++)
            expect("every macro has a first entry (the row's parameter); STD's is a step a detent", KN_E[set][knob][0].mul != 0 && (set != KS_STD || KN_E[set][knob][0].mul == 4));
    /* KNOBSTR (b9): no macro entry is on a parameter that no engine reads: P_ED_FLT (AMP ENV's cutoff depth) has been T_GONE since DUELL 1.1; the filter envelope's depth is ENV 2's (P_E2FLT) */
    {
        uint32_t ok = 1;
        for (k = 0; k < KN_NP; k++)                      /* (the part's own places; a global's id is another count) */
            ok &= KN_ID[k] != P_ED_FLT;
        expect("no knob macro moves P_ED_FLT (it has no effect since DUELL 1.1)", ok);
    }
    /* PUMP: an OFF ducker starts as PUMP, OCT- puts it back */
    setup(KS_HS, 0);
    trk[0].p[P_DKSRC] = 3;
    trk[0].p[P_DKDEP] = 0;
    h1_take(0);
    frames(32);
    turn(EN_K3, 6);
    expect("HSE K3 PMP: the OFF ducker is PUMP now and its depth went up 36 (six a detent)", trk[0].p[P_DKSRC] == 0 && trk[0].p[P_DKDEP] == 36);
    settle();
    btn_down(B_OCTDN);
    frame();
    turn(EN_K3, 1);
    btn_up(B_OCTDN);
    frames(64);
    expect("  .. OCT- + K3: OFF and depth 0 again", trk[0].p[P_DKSRC] == 3 && trk[0].p[P_DKDEP] == 0);
    /* a card the selection hides does nothing: DB's SEND with SENDS OFF moves neither send */
    setup(KS_DB, 0);
    mods_put(MODS_SENDS, 1u);
    frame();
    snap(b);
    turn(EN_K2, 8);
    snap(a);
    expect("SENDS OFF: DUB K2 (SND): the echo send does not move (its card is hidden)", a[0][K_DLY] == b[0][K_DLY]);
    mods_put(MODS_SENDS, 0u);
    /* AUTO and the KNOBS row */
    tide_knobs = 1;
    {
        static const char *const N[] = {"DUB CHORD", "HSE JUNO", "TECH DETROIT", "ACID HYPNO", "ELEC DEEPSEA", "AMB BERLIN"};
        static const char *const NO[] = {"HYPNO 303", "DB BASIC", "DUBX FOO", "AC", "", "dub CHORD", "TEC HNO", " DUB X", "AMBIENT", "TECHNO"};
        for (k = 0; k < 6u; k++) {
            str_cpy(proj_name, N[k], sizeof proj_name);
            expect("AUTO: a name that starts with a genre tag picks its set", kn_cur() == k + 1u);
        }
        for (k = 0; k < 10u; k++) {
            str_cpy(proj_name, NO[k], sizeof proj_name);
            expect("AUTO: any other first word is STD", kn_cur() == KS_STD);
        }
        str_cpy(proj_name, "DUB CHORD", sizeof proj_name);
        tide_knobs = 2;
        expect("KNOBS STD wins over a prefixed name", kn_cur() == KS_STD);
        tide_knobs = 8;
        str_cpy(proj_name, "", sizeof proj_name);
        expect("KNOBS AM wins over no name", kn_cur() == KS_AM);
        tide_knobs = 0;
        str_cpy(proj_name, "TECH SOUL", sizeof proj_name);
        expect("a stored 0 (an older record's ROLL TYPE MUSIC) is AUTO", kn_cur() == KS_TC && jl_get(JL_KNOB) == 0u);
    }
    /* the KNOBS row: AUTO, STD, DB .. AM, where ROLL TYPE was; it never dims */
    expect("the row: KNOBS, 8 values, after ARC and before AMOUNT, never dim", JL_KNOB == 4u && str_eq(JL_NAME[JL_KNOB], "KNOBS") && jl_n(JL_KNOB) == 8u && !jl_dimmed(JL_KNOB) && JL_AMOUNT == JL_KNOB + 1u);
    {
        static const char *const V[8] = {"AUTO", "STD", "DUB", "HSE", "TECH", "ACID", "ELEC", "AMB"};
        uint32_t ok = 1;
        for (k = 0; k < 8u; k++) {
            jl_put(JL_KNOB, k);
            ok &= jl_get(JL_KNOB) == k && str_eq(jl_vname(JL_KNOB, k), V[k]) && (k ? kn_cur() == k - 1u : 1);
        }
        expect("the row's values AUTO STD DUB HSE TECH ACID ELEC AMB: put, read and named", ok);
        jl_put(JL_KNOB, 0);
        expect("  .. AUTO stores 1 (the default of the byte)", tide_knobs == 1u);
    }
    /* the settings record: an older one (the byte was ROLL TYPE's MUSIC / SMART, 1 / 0) reads as AUTO; a stored set comes back; a byte out of range reads the default */
    {
        static const uint8_t R[4][TS_N] = {{GM_WANDER, 4, GT_BOTH, 2, 1, 1, 3, 1, 1, 1, 0, STY_CUSTOM, 0},   /* (old: ROLL TYPE MUSIC) */
                                           {GM_WANDER, 4, GT_BOTH, 2, 1, 0, 3, 1, 1, 1, 0, STY_CUSTOM, 0},   /* (old: SMART) */
                                           {GM_WANDER, 4, GT_BOTH, 2, 1, 5, 3, 1, 1, 1, 0, STY_CUSTOM, 0},   /* (KNOBS AC) */
                                           {GM_WANDER, 4, GT_BOTH, 2, 1, 200, 3, 1, 1, 1, 0, STY_CUSTOM, 0}};   /* (damaged) */
        static const int WANT[4] = {1, 0, 5, 1};
        uint8_t keep[TIDE_TAIL_N];
        memcpy(keep, tide_tail, sizeof keep);
        for (k = 0; k < 4u; k++) {
            tide_knobs = 7;
            tide_store_mark(R[k], TS_N);
            tide_seen_ok = 0;
            tide_settings();
            expect("an older record reads AUTO (0 or 1), a stored KNOBS set is kept, a damaged byte reads the default", tide_knobs == WANT[k] && (k == 2u ? jl_get(JL_KNOB) == 4u : jl_get(JL_KNOB) == 0u));
        }
        memcpy(tide_tail, keep, sizeof keep);
        tide_store_dirty = 0;
        tide_seen_ok = 0;
        tide_settings();
    }
    printf("knobs: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
