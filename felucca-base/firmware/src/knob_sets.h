/* SPDX-License-Identifier: GPL-3.0-only */
/* 1.0.0-b9 (owner 2026-11-11): HOME page 1's knobs 2..4 follow the genre. Knob 1 is always MORPH; each of the other three is a MACRO: one knob moves up to three
 * parameters together, each from the part's own value by its own step factor and sign (work/b9/QUICK_KNOBS_DESIGN.md v2). The sets are tables, included once
 * (morph.c: the sound's values are taken there; ui_draw.c: the row; ui_input.c: the knobs; tide_ui.c: the KNOBS row picks the set).
 *
 * A factor is in quarters of a knob step: 4 = one step for one step of the knob (the macro's main parameter), 2 = half the rate, 1 = a quarter ("a touch"); the sign
 * is the direction (-4: the parameter goes down while the knob goes up). The design gives the factors in words; the numbers are these: the main parameter 4, "resonance
 * at half the rate" 2, "a touch / a little" 1, every other secondary 2. */
#ifndef KNOB_SETS_H
#define KNOB_SETS_H
enum { KS_STD, KS_DB, KS_HS, KS_TC, KS_AC, KS_EL, KS_AM, KS_N };   /* the sets: STD (no prefix) and the six categories, in the order of KN_PFX */
/* the parameters the macros can move (a macro entry names one by its place here); the first KN_NP are the part's own (track_t p[]), the rest globals (song.g[]) that move
 * once, not once per part. The sound's values of all of them are taken when a sound is loaded (morph.c h1_take) for OCT- + the knob */
static const uint8_t KN_ID[] = {P_FREEA, P_FREEB, P_DLY, P_REV, P_ED_FLT, P_ATK, P_DEC, P_REL, P_LRATE, P_LD_FLT, P_DKDEP, P_AGATE, P_SGATE, P_APROB, P_DIST, P_CHOR,
                                G_DFDBK, G_DCOLOR, G_DTIME};
#define KN_NP 16u                                              /* (the part's own parameters; a place from here on is a global) */
#define KN_NID ((uint32_t)sizeof KN_ID)
enum { K_FLT, K_RES, K_DLY, K_REV, K_EDF, K_ATK, K_DEC, K_REL, K_LRT, K_LDF, K_DKD, K_AGT, K_SGT, K_APB, K_DST, K_CHO, K_FDB, K_COL, K_TIM };   /* (KN_ID's places) */
_Static_assert(K_TIM == sizeof KN_ID - 1u && K_CHO == KN_NP - 1u && P_E0 <= 128, "knob sets");
typedef struct { uint8_t ix; int8_t mul; } kn_e;                /* a macro entry: the parameter (a place in KN_ID), its factor in quarter steps (0: no entry; the sign is the direction) */
/* [set][knob 2..4][entry]; the first entry is the one the row shows */
static const kn_e KN_E[KS_N][3][3] = {
    {{{K_FLT, 4}}, {{K_RES, 4}}, {{K_DLY, 4}}},                                                      /* STD: FLT, RSN, ECH: the page as it was */
    {{{K_DLY, 4}, {K_REV, 1}}, {{K_FDB, 4}, {K_COL, -2}}, {{K_TIM, 4}}},                             /* DB: THROW, SPACE (colour darker as the feedback rises), TIME */
    {{{K_FLT, 4}, {K_RES, 1}}, {{K_DKD, 4}}, {{K_REV, 4}, {K_REL, 2}}},                              /* HS: FLT (the classic sweep), PUMP, ROOM */
    {{{K_FLT, 4}, {K_RES, 2}, {K_EDF, 2}}, {{K_DEC, 4}}, {{K_REV, 4}, {K_DLY, 2}}},                  /* TC: TENSE, STAB, WASH */
    {{{K_FLT, 4}, {K_RES, 2}}, {{K_EDF, 4}, {K_DEC, -2}}, {{K_DST, 4}}},                             /* AC: SQUELCH (resonance at half the rate), BITE, DIRT */
    {{{K_FLT, 4}}, {{K_DST, 4}, {K_CHO, 2}}, {{K_AGT, -4}, {K_SGT, -4}, {K_APB, -2}}},               /* EL: FLT, GRIT, GLITCH (gates shorter, steps dropped) */
    {{{K_ATK, 4}, {K_REL, 4}, {K_REV, 2}}, {{K_LDF, 4}, {K_LRT, -2}}, {{K_DLY, 4}, {K_REV, 4}}}};    /* AM: BLOOM, DRIFT, SPACE */
static const char *const KN_L[KS_N][3] = {{"FLT", "RSN", "ECH"}, {"SEND", "FDBK", "TIME"}, {"FLT", "PUMP", "REV"}, {"RISE", "DEC", "WSH"},
                                          {"SQL", "BITE", "DIRT"}, {"FLT", "GRIT", "GLCH"}, {"BLM", "DRF", "SPC"}};
/* the columns' left ends of each set (the name there, its value 5 px after it): laid out for the widest value each knob can show, 12 px or more between the columns, the
 * last ends by x 237 (tests/knobs_pics.c and tests/morph_ui_test.c check every value; tests/knobs_pics.c prints the rows) */
static const uint8_t KN_X[KS_N][3] = {{4, 79, 164}, {3, 91, 172}, {3, 81, 172}, {3, 83, 168}, {3, 79, 168}, {3, 80, 164}, {3, 87, 172}};   /* (STD: as the owner has had it since b7) */
/* the multi's category: the first two letters of its name and a space (KN_PFX: DB HS TC AC EL AM, two letters each, the sets 1..6 in order); no prefix: STD */
static const char KN_PFX[] = "DBHSTCACELAM";
#endif
