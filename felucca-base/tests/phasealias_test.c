/* SPDX-License-Identifier: GPL-3.0-only */
/* PHASE's RESONANCE waves against aliasing (lane P of the 2026-11-04 audit, P06; eng_phase.c pd_edge). The RESONANT SAW restarts its cycle at full height, and a bent
 * wave next to a RESONANCE one in the WAVE / WAVE2 toggle steps at each wrap too: unlimited, that put -32 / -29 / -26 dB of inharmonic power (C3 / C4 / C5) under the
 * tone. build/host/phasealias_test (host_tests.sh phasealias):
 *   ALIAS   one held note, dry: the energy away from the harmonics (+-6 bins of k f0, Blackman-Harris, below 16 kHz) under the energy of the harmonics, RESONANT SAW
 *           at C3 / C4 / C5 and DCW 40 / 100, and RESONANT SAW with a bent SAW as WAVE2: at most -44 dB (they were -23 .. -31)
 *   KEEP    RESONANT TRIANGLE and TRAPEZOID (continuous at the wrap) stay at -60 dB or better, and a RESONANT SAW from C0 to C8 stays inside the 16 bit range */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FFT_N 16384u
static int bad;
static void check(const char *what, int ok)
{
    printf("phasealias: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2 * M_PI / m;
        for (k = 0; k < n; k += m)
            for (j = 0; j < m / 2u; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[k + j + m / 2u] * wr - im[k + j + m / 2u] * wi, xi = re[k + j + m / 2u] * wi + im[k + j + m / 2u] * wr;
                re[k + j + m / 2u] = re[k + j] - xr;
                im[k + j + m / 2u] = im[k + j] - xi;
                re[k + j] += xr;
                im[k + j] += xi;
            }
    }
}
static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}

/* one held note on part 0 (dry): the samples after 0.5 s into x[FFT_N] (left channel) */
static double x[FFT_N];
static int32_t peak;
static void render(int w1, int w2, int dcw, uint32_t note)
{
    int32_t o[2 * CTL];
    uint32_t b, i, k, nb = (FFT_N + 22050u) / CTL;
    track_t *t = &trk[0];
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    vage = 0;
    rng_state = 0x1234567u;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : eng_by_name("PHASE"), 0);
    for (k = 1; k < NPART; k++)
        trk[k].p[P_MUTE] = 1;
    song.sel = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_KTRK] = 0;
    t->p[P_E0] = (int16_t)w1;
    t->p[P_E1] = (int16_t)w2;
    t->p[P_E2] = (int16_t)dcw;
    t->p[P_E3] = t->p[P_E4] = t->p[P_E5] = 0;
    trk_note_on(t, note, 100);
    peak = 0;
    for (b = 0, i = 0; b < nb; b++) {
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++)
            if (o[2u * k] > peak || -o[2u * k] > peak)
                peak = o[2u * k] > 0 ? o[2u * k] : -o[2u * k];
        if (b * CTL >= 22050u)
            for (k = 0; k < CTL && i < FFT_N; k++, i++)
                x[i] = o[2u * k];
    }
}
/* dB of the energy away from the harmonics under the harmonics' (and the RMS of the block in *rms) */
static double alias_db(double *rms, uint32_t note, double div)
{
    static double re[FFT_N], im[FFT_N];
    uint32_t i;
    double f0 = (double)pitch_inc(note * 16u) * FS / 4294967296.0 / div, harm = 0, tot = 0, sq = 0;
    for (i = 0; i < FFT_N; i++) {
        double a = 2 * M_PI * i / FFT_N;
        sq += x[i] * x[i];
        re[i] = x[i] * (0.35875 - 0.48829 * cos(a) + 0.14128 * cos(2 * a) - 0.01168 * cos(3 * a));
        im[i] = 0;
    }
    *rms = sqrt(sq / FFT_N);
    fft(re, im, FFT_N);
    for (i = 8; i < FFT_N / 2u; i++) {
        double e = re[i] * re[i] + im[i] * im[i], fq = (double)i * FS / FFT_N, kk = floor(fq / f0 + 0.5);
        if (kk >= 1 && fabs(fq - kk * f0) <= 6.0 * FS / FFT_N)
            harm += e;
        else if (fq < 16000.0)
            tot += e;
    }
    return harm > 0 ? 10 * log10((tot > 1e-9 ? tot : 1e-9) / harm) : 0;
}

int main(void)
{
    static const uint8_t NOTES[3] = {48, 60, 72};
    static const int DCW[2] = {40, 100};
    uint32_t ni, di;
    double worst = -999, r, a;
    char m[200];
    for (di = 0; di < 2u; di++)
        for (ni = 0; ni < 3u; ni++) {
            render(5, 0, DCW[di], NOTES[ni]);
            a = alias_db(&r, NOTES[ni], 1.0);
            snprintf(m, sizeof m, "RESONANT SAW, DCW %3d, note %u: %6.1f dB off the harmonics (rms %.0f)", DCW[di], (unsigned)NOTES[ni], a, r);
            check(m, a < -44.0);
            if (a > worst)
                worst = a;
        }
    for (ni = 0; ni < 3u; ni++) {
        render(0, 6, 60, NOTES[ni]);                         /* a bent SAW with RESONANT SAW as WAVE2: every other cycle steps */
        a = alias_db(&r, NOTES[ni], 2.0);                    /* (the pattern repeats every two cycles) */
        snprintf(m, sizeof m, "SAW + RESONANT SAW (WAVE2), DCW 60, note %u: %6.1f dB off the harmonics", (unsigned)NOTES[ni], a);
        check(m, a < -44.0);
    }
    for (ni = 0; ni < 3u; ni++) {
        double r6;
        render(6, 0, 60, NOTES[ni]);
        a = alias_db(&r6, NOTES[ni], 1.0);
        snprintf(m, sizeof m, "RESONANT TRIANGLE, note %u: %6.1f dB off the harmonics (continuous at the wrap: it was fine)", (unsigned)NOTES[ni], a);
        check(m, a < -60.0);
        render(7, 0, 60, NOTES[ni]);
        a = alias_db(&r6, NOTES[ni], 1.0);
        snprintf(m, sizeof m, "RESONANT TRAPEZOID, note %u: %6.1f dB off the harmonics", (unsigned)NOTES[ni], a);
        check(m, a < -60.0);
    }
    for (ni = 0; ni <= 108u; ni += 12u) {
        render(5, 0, 100, ni);
        snprintf(m, sizeof m, "RESONANT SAW note %3u: no overflow (peak %d)", (unsigned)ni, (int)peak);
        check(m, peak > 0 && peak < 32767);
    }
    printf("phasealias: worst RESONANT SAW %.1f dB; %s\n", worst, bad ? "FAILED" : "all ok");
    return bad != 0;
}
