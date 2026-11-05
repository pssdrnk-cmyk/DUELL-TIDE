/* SPDX-License-Identifier: GPL-3.0-only */
/* PHYS per model on the host (audit Q14): the NOTES budget charges every PHYS note as the dearest model (budget.c BD_ENG, 406, the unit's COST-CAL row for MODAL), but SYMP costs about 0.65 x MODAL and a
 * plain STRING 0.35 .. 0.5 x on the host. This prints the host ratios the unit's per-model rows (tools/bench_suites/PHYS-MODELS.txt, jam_dev.py measure) are expected to show, so the table of the unit's
 * numbers has its expected values next to it. The charges are NOT changed by this: the budget keeps 406 until the unit has measured the models.
 *   build/host/phys_cost_probe        (host_tests.sh physcost; report only: it fails only when the ratios are not in order MEMB/MODAL >= SYMP >= STRING)
 * The cost of ONE MORE NOTE (3 notes less 1 note, over 2: PHYS plays 3 at most), in time stamp ticks a sample, fastest of several runs, the way tests/tide_cost_probe.c times (the host's instruction
 * counter is macOS only). Table 1 (information): the dearest MODAL factory sound with its MODEL set to each model. Table 2 (the expected values of the unit's rows, which play every factory PHYS sound as it is): the dearest and the mean of each model. -O1 -fno-builtin. */
#define UI_TEST_NO_MAIN 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <time.h>
#include "ui_test.c"

static unsigned checks, failures;
#define REQUIRE(msg,expr) do { checks++; if (!(expr)) { failures++; printf("FAIL: %s (%s:%d)\n",msg,__FILE__,__LINE__); } } while(0)

#if defined(__x86_64__) || defined(__i386__)
static double wall_ticks(void) { return (double)__builtin_readcyclecounter(); }
#else
static double wall_ticks(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
#endif
static int32_t ob[2 * CTL];
#define TRIALS 9
static double scene_ticks(unsigned run)
{
    double best = 1e30;
    for (unsigned k = 0; k < 30; k++) { mix_block(ob, CTL); while (wave_service()) ; }
    for (unsigned r = 0; r < TRIALS; r++) {
        double a = wall_ticks();
        for (unsigned i = 0; i < run; i++) mix_block(ob, CTL);
        a = (wall_ticks() - a) / ((double)run * CTL);
        if (a < best) best = a;
    }
    return best;
}
static const uint8_t N3[3] = {48, 52, 55};
/* one more note of PHYS sound k, with the model forced (-1: as the sound has it): ticks a sample, cleaned as tests/poly_calib.c clean() (no matrix, TWIN, SUB, MULTI bypassed, sends off, POLY) */
static double note_cost(unsigned k, int model, unsigned *own_model)
{
    double c1, c3;
    for (unsigned notes = 1; notes <= 3; notes += 2) {
        track_t *t;
        ui_power_on(); frame();
        song.playing = song.rec = 0; transport_req = 0; gl.on = gl.armed = 0; morph_musical = 0;
        trk[0].eng_req = ENGI_PHYS;
        for (unsigned i = 0; i < 8u; i++) trk[0].p[P_E0 + i] = ENGINES[ENGI_PHYS]->edit[i].def;
        apply_preset_to(&trk[0], k);
        t = &trk[0];
        if (own_model) *own_model = (unsigned)t->p[P_E0];
        if (model >= 0) t->p[P_E0] = (int16_t)model;
        for (unsigned i = 0; i < NMSLOT; i++) t->p[MSLOT_ID(i)] = t->p[MSLOT_ID(i) + 1] = t->p[MSLOT_ID(i) + 2] = 0;
        t->p[P_TWTYPE] = TT_OFF, t->p[P_DRV2] = 0, t->p[P_RES2] = 0;
        for (unsigned i = 0; i < NX; i++) if (i != X_LFO1LVL && i != X_LFO2LVL && i != X_LINK) XV(t, i) = 0;
        t->p[P_KTRK] = 0; t->p[P_FREEA] = UF_STEPS; t->p[P_FREEB] = 40; t->p[P_FREEC] = 0; t->p[P_FREED] = 1;
        t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_ED_FLT] = t->p[P_L2FLT] = 0;
        t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = 0; t->p[P_SUS] = 127; t->p[P_CHRD] = 0;
        t->p[P_DIST] = t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0; t->p[P_ITYPE] = 0;
        t->p[P_EQLG] = t->p[P_EQMG] = t->p[P_EQHG] = 0; t->p[P_DKDEP] = 0;
        trk[1].p[P_MUTE] = 1;
        memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv);
        for (unsigned i = 0; i < 40u; i++) mix_block(ob, CTL);
        for (unsigned i = 0; i < notes; i++) trk_note_on(t, N3[i], 110);
        if (notes == 1) c1 = scene_ticks(120); else c3 = scene_ticks(120);
    }
    return (c3 - c1) / 2.0;
}

int main(void)
{
    static const char *const MN[4] = {"MODAL", "STRING", "MEMB", "SYMP"};
    unsigned n = ds_count(ENGI_PHYS), dear = 0, own = 0;
    double all[64], best = 0, mc[4] = {0}, msum[4] = {0}, mmax[4] = {0};
    unsigned mn[4] = {0};
    setvbuf(stdout, 0, _IONBF, 0);
    if (n > 64u) n = 64u;
    /* table 2 first: every factory PHYS sound as it is */
    for (unsigned k = 0; k < n; k++) {
        all[k] = note_cost(k, -1, &own);
        if (own < 4u) { mn[own]++; msum[own] += all[k]; if (all[k] > mmax[own]) mmax[own] = all[k]; }
        if (own == 0u && all[k] > best) { best = all[k]; dear = k; }
    }
    printf("physcost: %u factory PHYS sounds; the dearest MODAL sound is #%u %s (%.2f ticks a sample a note)\n", n, dear, ds_name(ENGI_PHYS, dear), best);
    printf("physcost: table 2, factory sounds as they are, ratio to the dearest MODAL sound (the unit's model: budget.c BD_ENG PHYS %u):\n", 406u);
    for (unsigned m = 0; m < 4u; m++)
        if (mn[m])
            printf("physcost:   %-7s %2u sounds  dearest %.2f x  mean %.2f x  -> expected unit charge %.0f (dearest) / %.0f (mean)\n", MN[m], mn[m], mmax[m] / best, msum[m] / mn[m] / best,
                   mmax[m] / best * 406.0, msum[m] / mn[m] / best * 406.0);
    /* table 1: the dearest MODAL sound with its MODEL forced: what `sound 1 PHYS DEAR` + `set 1 MODEL ..` measures */
    printf("physcost: table 1 (information), the dearest MODAL sound with its MODEL set, ratio to MODAL:\n");
    for (unsigned m = 0; m < 4u; m++)
        mc[m] = note_cost(dear, (int)m, 0);
    for (unsigned m = 0; m < 4u; m++)
        printf("physcost:   %-7s %.2f ticks a note-sample  %.2f x MODAL  -> expected unit charge %.0f (the budget charges 406 for all)\n", MN[m], mc[m], mc[m] / mc[0], mc[m] / mc[0] * 406.0);
    /* table 1 is information: the other sounds' parameters (BOW, EXC) apply to a forced model and blur the ratios; the rank is read from table 2, the factory sounds as they are */
    REQUIRE("every model costs less than MODAL's dearest on the host (factory sounds as they are)", mmax[1] < best && mmax[2] < best && mmax[3] < best);
    REQUIRE("a plain STRING costs less than SYMP on the host (factory sounds)", mmax[1] < mmax[3]);
    REQUIRE("a plain STRING costs at most 0.6 x MODAL (the audit read 0.35 .. 0.5)", mmax[1] < 0.6 * best);
    printf("physcost: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
