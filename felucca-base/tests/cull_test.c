/* SPDX-License-Identifier: GPL-3.0-only */
/* THE CULL (CPU fix, wave 14): a released FM6 note whose loudest carrier fell below 2^-11 of a carrier at full level (-66 dB: fm6_core.c FM6_CULL) ends instead of running its six operators for
 * seconds more; the voice fades out over its last block (voice.c: the one-block fade of a voice_kill) and the next block does not render it. Before the cull the note ended at msfa's threshold
 * (2^-15.5, -89 dB). The other engines already end at an envelope threshold (voice.c adsr_step: the ADSR under 2^12 of 2^24, -72 dB); the cull does not touch them.
 * On hostsim.c as fm_bass_diff_test.c: every FM6 factory sound, notes C2 / C4 / C6 at velocity 30 and 127, held 0.4 s and released, DRY (sends, INSERT, SAT, EQ off: nothing rings after the voice), through
 * mix_block until its voice ends (at most 60 s). For each:
 *   - the voice ends (a sound whose last envelope level is above the cull level holds for ever: counted as a drone), and no voice sounds again after it;
 *   - SILENT: the output of the last 8 samples before the end (the level the cull saw) is under 64 LSB (-54 dBFS; what is left there is the DC blocker's residue of a fast release, DUB DRAWBAR's 45: the cull fades it, the end before cut it), and the second difference of the output (the click detector: a step or a kink shows as d2 far above
 *     the waveform's own curvature) in the 64 samples before and the 160 after the end is not above twice the largest d2 of the 3,000 samples before (+ 4 LSB): the cull is no click;
 *   - against the reference build (-DFM6_CULL=1120, the threshold of before the cull; argv[1] is its output): no note ends later, and the cull ends the notes of the whole list 5 s or more earlier
 *     in total (the reference build ends none earlier: its file is the evidence).
 * host_tests.sh "cull" builds both and runs the reference first (its own run checks the first points only).
 *   build/host/cull_test [REF.txt]   (host_tests.sh "cull") */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdio.h>

static int bad;
static void ck(const char *what, int ok)
{
    printf("cull: %-150s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

#define MAXB (60u * FS / CTL)
static int32_t outl[MAXB * CTL];
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

typedef struct { int end; int32_t pk, tail, base, win; int stay; } res_t;
/* sound (e, k) at note / velocity, alone, dry; end = the block the voice ended in (-1: it never does) */
static void play(uint32_t e, uint32_t k, uint32_t note, uint32_t vel, res_t *r)
{
    uint32_t b, i, hold = (uint32_t)(0.4 * FS) / CTL, nb = 0;
    int32_t o[2 * CTL];
    memset(r, 0, sizeof *r);
    r->end = -1;
    r->stay = 1;
    host_tracks_init();
    song.g[G_RTYPE] = GP[G_RTYPE].def;
    duell_mode_force(DM_STEREO);
    host_ds_load(&trk[0], e, k);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
    trk[0].p[P_ITYPE] = 0;
    trk[0].p[P_AMODE] = 0;
    trk[0].p[P_CHRD] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_EQLG] = trk[0].p[P_EQMG] = trk[0].p[P_EQHG] = 0;
    trk[0].p[P_DKDEP] = 0;
    trk[1].p[P_MUTE] = 1;
    input_on(&trk[0], note, vel);
    for (b = 0; b < MAXB; b++) {
        uint32_t act = 0;
        if (b == hold)
            input_off(&trk[0], note);
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            outl[b * CTL + i] = o[2u * i];
        nb = b + 1u;
        for (i = 0; i < NVOICE; i++)
            act += trk[0].v[i].active != 0;
        if (r->end < 0 && b > hold && !act)
            r->end = (int)b;
        else if (r->end >= 0 && act)
            r->stay = 0;
        if (r->end >= 0 && b >= (uint32_t)r->end + 40u)     /* (40 more blocks: the window after the end) */
            break;
    }
    if (r->end >= 0) {
        uint32_t s = (uint32_t)r->end * CTL, j, a = s > 3200u ? s - 3200u : 2u;
        for (j = a; j + 2u < s - 64u; j++) {
            int32_t d2 = iabs(outl[j + 2u] - 2 * outl[j + 1u] + outl[j]);
            r->base = d2 > r->base ? d2 : r->base;
        }
        for (j = s - 64u; j + 2u < s + 160u && j + 2u < nb * CTL; j++) {
            int32_t d2 = iabs(outl[j + 2u] - 2 * outl[j + 1u] + outl[j]);
            r->win = d2 > r->win ? d2 : r->win;
        }
        for (j = 0; j < s; j++)
            r->pk = iabs(outl[j]) > r->pk ? iabs(outl[j]) : r->pk;
        for (j = s - 8u; j < s; j++)
            r->tail = iabs(outl[j]) > r->tail ? iabs(outl[j]) : r->tail;
    }
}

int main(int argc, char **argv)
{
    static const uint32_t NOTE[3] = {36, 60, 84}, VEL[2] = {30, 127};
    uint32_t n, N = ds_list_n(), e, k, a, v, nplay = 0, nend = 0, ndrone = 0, nearlier = 0;
    int ref_end[2048], nref = 0, later = 0, nclick = 0, nloud = 0, nstay = 0;
    long gain = 0;
    int32_t maxtail = 0, worst_win = 0, worst_base = 0;
    char line[200], w[300];
    FILE *rf = argc > 1 ? fopen(argv[1], "r") : 0;
    if (rf) {
        while (fgets(line, sizeof line, rf) && nref < 2048) {
            int x;
            char *q = strstr(line, "END ");
            if (q && sscanf(q + 4, "%d", &x) == 1)
                ref_end[nref++] = x;
        }
        fclose(rf);
    }
    for (n = 0; n < N; n++) {
        e = ds_list_at(n, &k);
        if (!ENGINES[e]->ownenv)
            continue;
        for (a = 0; a < 3u; a++)
            for (v = 0; v < 2u; v++) {
                res_t r;
                int re;
                play(e, k, NOTE[a], VEL[v], &r);
                printf("cull: END %d %-12s note %u vel %u tail %d of peak %d, d2 %d against %d\n", r.end, ds_get(e, k)->name, NOTE[a], VEL[v], r.tail, r.pk, r.win, r.base);
                re = rf && nplay < (uint32_t)nref ? ref_end[nplay] : -2;
                nplay++;
                if (r.end < 0) {
                    ndrone++;
                    continue;
                }
                nend++;
                nstay += !r.stay;
                nloud += r.tail > 64;
                nclick += r.win > 2 * r.base + 4;
                maxtail = r.tail > maxtail ? r.tail : maxtail;
                if (r.win - 2 * r.base > worst_win - 2 * worst_base) {
                    worst_win = r.win;
                    worst_base = r.base;
                }
                if (re >= 0) {
                    later += r.end > re;
                    nearlier += r.end < re;
                    gain += re - r.end;
                }
            }
    }
    sprintf(w, "%u FM6 notes played (every factory FM6 sound x C2 / C4 / C6 x velocity 30 / 127): %u end, %u hold for ever (a last envelope level above the cull level)", nplay, nend, ndrone);
    ck(w, nend >= nplay * 8u / 10u);
    sprintf(w, "no voice sounds again after its end (%d of %u do)", nstay, nend);
    ck(w, nstay == 0);
    sprintf(w, "the last 8 samples before the end are under 64 LSB, -54 dBFS (%d of %u are louder, the loudest %d)", nloud, nend, maxtail);
    ck(w, nloud == 0);
    sprintf(w, "the end is no click: second difference at the end within twice the waveform's own plus 4 (%d of %u are not; the worst %d against %d)", nclick, nend, worst_win, worst_base);
    ck(w, nclick == 0);
    if (rf) {
        sprintf(w, "no note ends later than in the build without the cull (%d do)", later);
        ck(w, later == 0);
        sprintf(w, "the cull ends %u of the %u notes earlier, %.1f s in all (needs 5 s)", nearlier, nend, gain * (double)CTL / FS);
        ck(w, gain * (long)CTL >= 5L * (long)FS);
    }
    printf("CULL: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
