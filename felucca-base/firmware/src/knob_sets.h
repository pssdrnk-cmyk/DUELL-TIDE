/* SPDX-License-Identifier: GPL-3.0-only */
/* 1.0.0-b9 (owner 2026-11-11): HOME page 1's knobs 2..4 follow the genre. Knob 1 is always MORPH; each of the other three is a MACRO: one knob moves up to three
 * parameters together, each from the part's own value by its own step factor and sign (work/b9/QUICK_KNOBS_DESIGN.md v2). The sets are tables, included once
 * (morph.c: the sound's values are taken there; ui_draw.c: the row; ui_input.c: the knobs; tide_ui.c: the KNOBS row picks the set).
 *
 * A factor is in quarters of a knob step: 4 = one step for one step of the knob (the macro's main parameter), 2 = half the rate, 1 = a quarter ("a touch"); the sign
 * is the direction (-4: the parameter goes down while the knob goes up). The design gives the factors in words; the numbers are these: the main parameter 4, "resonance
 * at half the rate" 2, "a touch / a little" 1, every other secondary 2.
 * b9 KNOBSTR (owner: some knobs felt underwhelming): the genre sets measured on the host (work/b9/reports/B9-KNOBSTR.md) and their factors raised where the travel of +-32 detents did too little:
 * a send, a pump or a cutoff now crosses most of its range in 20 to 30 detents (factors 12 to 24 = three to six steps a detent), TIME a quarter step (17 note values in order of length),
 * and BITE / TENSE move ENV 2's cutoff depth (P_E2FLT): P_ED_FLT, the AMP ENV's, has had no effect since DUELL 1.1. STD is as it was. */
#ifndef KNOB_SETS_H
#define KNOB_SETS_H
enum { KS_STD, KS_DB, KS_HS, KS_TC, KS_AC, KS_EL, KS_AM, KS_N };   /* the sets: STD (no prefix) and the six categories, in the order of KN_TAG */
/* the parameters the macros can move (P_DKSRC is no entry: PUMP turns an OFF ducker to PUMP, ui_input.c morph_knob, and OCT- puts the sound's source back) (a macro entry names one by its place here); the first KN_NP are the part's own (track_t p[]), the rest globals (song.g[]) that move
 * once, not once per part. The sound's values of all of them are taken when a sound is loaded (morph.c h1_take) for OCT- + the knob */
static const uint8_t KN_ID[] = {P_FREEA, P_FREEB, P_DLY, P_REV, P_E2FLT, P_ATK, P_DEC, P_REL, P_LRATE, P_LD_FLT, P_DKDEP, P_AGATE, P_SGATE, P_APROB, P_DIST, P_CHOR, P_DKSRC,
                                G_DFDBK, G_DCOLOR, G_DTIME};
#define KN_NP 17u                                              /* (the part's own parameters; a place from here on is a global) */
#define KN_NID ((uint32_t)sizeof KN_ID)
enum { K_FLT, K_RES, K_DLY, K_REV, K_E2F, K_ATK, K_DEC, K_REL, K_LRT, K_LDF, K_DKD, K_AGT, K_SGT, K_APB, K_DST, K_CHO, K_DKS, K_FDB, K_COL, K_TIM };   /* (KN_ID's places) */
_Static_assert(K_TIM == sizeof KN_ID - 1u && K_DKS == KN_NP - 1u && P_E0 <= 128, "knob sets");
typedef struct { uint8_t ix; int8_t mul; } kn_e;                /* a macro entry: the parameter (a place in KN_ID), its factor in quarter steps (0: no entry; the sign is the direction) */
/* [set][knob 2..4][entry]; the first entry is the one the row shows */
static const kn_e KN_E[KS_N][3][3] = {
    {{{K_FLT, 4}}, {{K_RES, 4}}, {{K_DLY, 4}}},                                                      /* STD: FLT, RSN, ECH: the page as it was */
    {{{K_DLY, 14}, {K_REV, 3}}, {{K_FDB, 16}, {K_COL, -8}, {K_DLY, 8}}, {{K_TIM, 1}}},               /* DB: SND, FDBK (colour darker as it rises, the send follows), TIME (a quarter: 17 notes in order of length) */
    {{{K_FLT, 14}, {K_RES, 3}}, {{K_DKD, 24}, {K_REV, -6}, {K_DLY, -3}}, {{K_REV, 20}, {K_REL, 3}}},                           /* HS: FLT (the classic sweep), PMP (the sends go down as the pump deepens, so the dip is not filled), REV */
    {{{K_FLT, 9}, {K_RES, 3}, {K_E2F, 3}}, {{K_DEC, 6}}, {{K_REV, 12}, {K_DLY, 4}}},                 /* TC: TNS, DEC, WSH */
    {{{K_FLT, 6}, {K_RES, 3}}, {{K_E2F, 8}, {K_FLT, 4}, {K_DEC, -4}}, {{K_DST, 12}}},                /* AC: SQL, BTE (ENV 2's depth, the cutoff up a little, decay shorter), DRT */
    {{{K_FLT, 8}}, {{K_DST, 16}, {K_CHO, 4}}, {{K_AGT, -8}, {K_SGT, -8}, {K_APB, -12}}},             /* EL: FLT, GRT, GLT (gates shorter, steps dropped) */
    {{{K_ATK, 8}, {K_REL, 8}, {K_REV, 4}}, {{K_LDF, 14}, {K_LRT, -2}, {K_FLT, -6}}, {{K_DLY, 16}, {K_REV, 16}}}};   /* AM: BLM, DRF (the cutoff comes down so the wobble has room), SPC */
static const char *const KN_L[KS_N][3] = {{"FLT", "RSN", "ECH"}, {"SND", "FDBK", "TIME"}, {"FLT", "PMP", "REV"}, {"TNS", "DEC", "WSH"},
                                          {"SQL", "BTE", "DRT"}, {"FLT", "GRT", "GLT"}, {"BLM", "DRF", "SPC"}};   /* (vowels dropped, the owner's: SND for SEND, PMP for PUMP) */
/* the columns' left ends of each set (the name there, its value 5 px after it): laid out for the widest value each knob can show, 12 px or more between the columns, the
 * last ends by x 237 (tests/knobs_pics.c and tests/morph_ui_test.c check every value; tests/knobs_pics.c prints the rows) */
static const uint8_t KN_X[KS_N][3] = {{4, 79, 164}, {3, 88, 172}, {3, 85, 172}, {3, 82, 168}, {3, 83, 172}, {3, 86, 173}, {3, 87, 172}};   /* (STD: as the owner has had it since b7) */
/* the multi's genre: the first word of its name (up to the first space) is one of these tags: the sets 1..6 in order; anything else: STD */
static const char *const KN_TAG[KS_N - 1u] = {"DUB", "HSE", "TECH", "ACID", "ELEC", "AMB"};
#endif
