/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tool (not in any test list; needs fork: Linux / WSL / macOS): the midpoints of JOURNEY's pairs. For every pair of factory
 * sounds of one engine in one role family (tide_ui.c JFAM) the part is rendered holding the chord C3 E3 G3 for 3 s at fader 0
 * (End A, the sound), 127 (End B = MORPH TO the other, morph_to_sound) and 32 / 64 / 96, measured as tests/loudness.h does (weighted
 * loudest window, raw peak, brightness = spectral centroid proxy). A pair FAILS where a glide point
 *   - dips more than DIP_DB under the quieter end              (level dip)
 *   - peaks PEAK_OVER dB over the louder end                    (peak)
 *   - is brighter than CENT_X times the brighter end           (harsh midpoint)
 * The renders run the main loop between blocks (pump: WAVE table builds, the morph plan): without it a WAVE glide never builds its new table and reads silent.
 *   build: cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -DFELUCCA_VERSION=\"x\" -o build/host/jrny_pair_audit tests/jrny_pair_audit.c -lm
 *   run:   build/host/jrny_pair_audit [family 0..3 (default 0 HARMONIC)]   prints every failing pair and the totals; JP_ALL=1 every pair
 *   list:  JP_EMIT=1 build/host/jrny_pair_audit 4 2>pairs.txt (family 4 = all): the failing pairs as PAIR lines on stderr */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/category.c"
#include "loudness.h"
static int sl_empty_user(uint32_t k) { (void)k; return 0; }   /* (the user slots are ui.c / upreset.c: not in this tool) */

#define DIP_DB 4.0
#define PEAK_OVER 2.0
#define CENT_X 1.8
static const char *const FAMN[4] = {"HARMONIC", "BASS", "LEAD/PLUCK", "PERC/FX"};
static uint32_t fam_of_cat(uint32_t c)
{
    return c == CAT_BASS ? 1u : c == CAT_LEAD || c == CAT_PLUCK ? 2u : c == CAT_PERC || c == CAT_FX ? 3u : 0u;
}
typedef struct { uint32_t e, a, b, f; } job_t;
static const uint8_t CH[3] = {48, 52, 55};
static void setup(void *v)
{
    const job_t *j = v;
    host_tracks_init();
    song.g[G_RTYPE] = GP[G_RTYPE].def;
    duell_mode_force(DM_STEREO);
    host_ds_load(&trk[0], j->e, j->a);
    trk[0].p[P_CHRD] = 0;
    morph_ready(0);
    if (j->b != 0xFFFFu)
        morph_to_sound(0, 1u, j->b);
    mph[0].fader = (uint8_t)j->f;
}
#define NJ 12u
/* the main loop between the audio blocks (main.c): a WAVE table build waits on it, and the morph plans its transition on it */
static void pump(void)
{
    static uint32_t n;
    fm1_ms += 3u;
    while (wave_service())
        ;
    if (!(++n & 7u))
        morph_xpoll();
}
static void run(job_t *jb, lz_t *L, uint32_t n)
{
    uint32_t i, j;
    lz_hold = 3.0;
    lz_pre_block = pump;
    for (i = 0; i < n; i += NJ) {
        lz_job_t t[NJ];
        for (j = i; j < n && j < i + NJ; j++)
            t[j - i] = lz_spawn(setup, &jb[j], CH, 3u);
        for (j = i; j < n && j < i + NJ; j++)
            L[j] = lz_collect(t[j - i]);
    }
}
int main(int argc, char **argv)
{
    uint32_t fam = argc > 1 ? (uint32_t)atoi(argv[1]) : 0u, e, a, b, f, np = 0, nfail = 0, nd = 0, nc = 0, nk = 0, all = getenv("JP_ALL") != 0;
    static job_t jb[8192];
    static lz_t L[8192];
    static uint32_t pe[2048], pa[2048], pb[2048], nj;
    uint32_t solo[16][64], np2 = 0;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;
        for (a = 0; a < ds_count(e) && a < 64u; a++)
            solo[e][a] = 0xFFFFFFFFu;
        for (a = 0; a < ds_count(e); a++)
            for (b = a + 1u; b < ds_count(e); b++)
                if (fam_of_cat(ds_get(e, a)->cat) == fam_of_cat(ds_get(e, b)->cat) && (fam == 4u || fam_of_cat(ds_get(e, a)->cat) == fam) && np < 2048u) {
                    pe[np] = e, pa[np] = a, pb[np] = b, np++;
                }
    }
    (void)solo; (void)np2;
    for (nj = 0; nj < np; nj++) {   /* per pair: A alone (fader 0), B alone (the sound b loaded plain), 32, 64, 96 */
        jb[5u * nj + 0u] = (job_t){pe[nj], pa[nj], 0xFFFFu, 0u};
        jb[5u * nj + 1u] = (job_t){pe[nj], pb[nj], 0xFFFFu, 0u};
        jb[5u * nj + 2u] = (job_t){pe[nj], pa[nj], pb[nj], 32u};
        jb[5u * nj + 3u] = (job_t){pe[nj], pa[nj], pb[nj], 64u};
        jb[5u * nj + 4u] = (job_t){pe[nj], pa[nj], pb[nj], 96u};
    }
    run(jb, L, 5u * np);
    printf("pairs of %s: %u; chord C3 E3 G3 held 3 s; fail: dip > %.0f dB under the quieter end, peak > %.0f dB over the louder end's, centroid > x%.1f of the brighter end\n",
           fam < 4u ? FAMN[fam] : "ALL FAMILIES", np, DIP_DB, PEAK_OVER, CENT_X);
    for (nj = 0; nj < np; nj++) {
        const lz_t *A = &L[5u * nj], *B = &L[5u * nj + 1u];
        double lo = A->loud < B->loud ? A->loud : B->loud, br = A->bright > B->bright ? A->bright : B->bright, dip = 0, cx = 0, pk = -200;
        int bad = 0, d = 0, c = 0, k = 0;
        for (f = 2; f < 5u; f++) {
            const lz_t *M = &L[5u * nj + f];
            if (lo - M->loud > dip) dip = lo - M->loud;
            if (br > 0 && M->bright / br > cx) cx = M->bright / br;
            if (M->peak > pk) pk = M->peak;
        }
        d = dip > DIP_DB, c = cx > CENT_X, k = pk > (A->peak > B->peak ? A->peak : B->peak) + PEAK_OVER || A->nan || B->nan;
        bad = d || c || k;
        np2++, nfail += bad, nd += d, nc += c, nk += k;
        if (bad && getenv("JP_EMIT")) fprintf(stderr, "PAIR %u %u %u\n", pe[nj], pa[nj], pb[nj]);
        if (getenv("JP_V") && dip > 0) printf("   levels A %.1f B %.1f  32 %.1f 64 %.1f 96 %.1f\n", A->loud, B->loud, L[5u*nj+2].loud, L[5u*nj+3].loud, L[5u*nj+4].loud);
        if (bad || all)
            printf("%s %-4s E%u %-12s -> %-12s dip %4.1f dB  centroid x%.2f  peak %5.1f  (ends %.1f/%.1f dB, %.0f/%.0f Hz)\n", bad ? "FAIL" : "ok  ", "", pe[nj],
                   ds_get(pe[nj], pa[nj])->name, ds_get(pe[nj], pb[nj])->name, dip, cx, pk, A->loud, B->loud, A->bright, B->bright);
    }
    printf("pairs %u: fail %u (dip %u, centroid %u, peak/NaN %u)\n", np, nfail, nd, nc, nk);
    return 0;
}
