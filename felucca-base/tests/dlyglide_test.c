/* SPDX-License-Identifier: GPL-3.0-only */
/* The plain delay's TIME glide (fx_bus.c fx_buses_m, fx.c dly_slide): a change of the delay time (the TIME knob, the TEMPO knob, a BPM step, an
 * external clock's re-measured tempo) bends the repeats like a tape instead of jumping the read head, which clicked (audit R01, R02).
 * On hostsim.c as plate_test.c / locut_test.c: a 330 Hz tone into the delay send, FDBK 60, COLR 127, MIX 127; the output is the shared bus's wet.
 *   build/host/dlyglide_test   (host_tests.sh "dlyglide")
 * 1. TIME 1/8 -> 1/4 note at 2 s (DIGITAL and PING-PONG): the burst above 6 kHz in the 150 ms after the change, against the tone's level, at most -28 dB
 *    (the read head's jump gave -7.3 dB, PING-PONG -2.6 dB)
 * 2. a BPM step 120 -> 123 (DIGITAL): at most -48 dB (the jump gave -27.8 dB)
 * 3. an external clock re-measured every 16th at +-1 ms (midi_beat_samples as midi_clock.c had it): no burst above -40 dB against the tone in 1.9 s (80 before)
 * 4. the glide ends on the time: an impulse's first repeat lands exactly at the delay time, in a steady delay and 6 s after a TIME change
 * 5. a time that does not fit the line halves until it does, in every mode (audit R03: it was clamped to 65535 samples, off the beat), and the
 *    DELAY card shows the division it plays (fx.c delay_samples, delay_div_shown) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdio.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("dlyglide: %-120s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define SECS 4u
static float outbuf[SECS * FS];
/* 4 s of the tone; what happens at 2 s: 1 TIME to 1/4, 2 BPM to 123; 3: an external clock's tempo jitters from the start */
static void run(uint32_t mode, uint32_t what)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t nb = SECS * FS / CTL, b, i, jr = 99u;
    double ph = 0;
    host_tracks_init();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    dlyev_clear();
    dly_plain_pp = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DFDBK] = 60;
    song.g[G_DCOLOR] = 127;
    song.g[G_DMIX] = 127;
    song.g[G_DMODE] = (int16_t)mode;
    song.g[G_DMDEP] = 0;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    midi_beat_samples = 0;
    if (what >= 4u) {
        song.g[G_BPM] = 40;
        song.g[G_DFDBK] = 0;
        song.g[G_DTIME] = what == 4u ? 5 : 0;
    }
    for (b = 0; b < nb; b++) {
        if (what == 3u) {
            song.g[G_CLOCK] = 1;
            if ((b * CTL) % 5512u < CTL) {
                jr = jr * 1103515245u + 12345u;
                midi_beat_samples = 22050u + ((jr >> 16) % 3u) * 176u - 176u;
            }
        }
        if (b == 2u * FS / CTL) {
            if (what == 1u) song.g[G_DTIME] = 0;
            if (what == 2u) song.g[G_BPM] = 123;
            if (what >= 4u) song.g[G_DTIME] = what == 4u ? 0 : 5;
        }
        for (i = 0; i < CTL; i++) {
            double x = b >= 64u ? sin(ph) * 20000. : 0.;
            ph += 2. * M_PI * 330. / FS;
            c[i] = r[i] = 0;
            d[i] = (int32_t)x;
            mix_l[i] = mix_r[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        if (dly_side) dly_side_mix(mix_l, mix_r, CTL);
        for (i = 0; i < CTL; i++)
            outbuf[b * CTL + i] = (float)(mix_l[i] + w[i] + mix_r[i] + w[i]) * .5f;
    }
}
/* a 4th-order Butterworth high-pass at 6 kHz (two biquads), over the whole run */
static void hp6k(const float *x, double *y, uint32_t n)
{
    static const double Q[2] = {0.5411961, 1.3065630};
    uint32_t k, i;
    double w0 = 2. * M_PI * 6000. / FS, cs = cos(w0), a = sin(w0);
    for (i = 0; i < n; i++) y[i] = x[i];
    for (k = 0; k < 2u; k++) {
        double al = a / (2. * Q[k]), b0 = (1. + cs) / 2., b1 = -(1. + cs), b2 = b0, a0 = 1. + al, a1 = -2. * cs, a2 = 1. - al;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (i = 0; i < n; i++) {
            double o = (b0 * y[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
            x2 = x1; x1 = y[i]; y2 = y1; y1 = o; y[i] = o;
        }
    }
}
static double tone_rms(void)
{
    double s = 0;
    uint32_t i, a = (uint32_t)(1.5 * FS), b = (uint32_t)(1.9 * FS);
    for (i = a; i < b; i++) s += (double)outbuf[i] * outbuf[i];
    return sqrt(s / (b - a));
}
static double peak_db(const double *h, double t0, double t1)
{
    double m = 1e-9;
    uint32_t i;
    for (i = (uint32_t)(t0 * FS); i < (uint32_t)(t1 * FS); i++) m = fabs(h[i]) > m ? fabs(h[i]) : m;
    return 20. * log10(m / tone_rms());
}
/* an impulse 6 s after the start (TIME 1/8 at 120 BPM; t: set to 1/4 at the start), the sample of the first repeat's peak, and where it is due */
static uint32_t first_repeat(uint32_t t, uint32_t *want)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, k, got = 0, at = 6u * FS;
    float pk = 0;
    host_tracks_init();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    dlyev_clear();
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DFDBK] = 60;
    song.g[G_DCOLOR] = 127;
    song.g[G_DMIX] = 127;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_DMDEP] = 0;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    if (t >= 2u) {
        song.g[G_BPM] = 40;
        song.g[G_DTIME] = t == 2u ? 5 : 0;
    }
    for (b = 0; b < 64u; b++) {                         /* the plain delay runs on its time */
        for (k = 0; k < CTL; k++) { c[k] = d[k] = r[k] = 0; mix_l[k] = mix_r[k] = 0; }
        fx_buses(c, d, r, w, CTL);
    }
    if (t)
        song.g[G_DTIME] = t == 3u ? 5 : 0;
    *want = at + delay_samples();
    for (b = 0; b * CTL < at + *want; b++) {
        for (k = 0; k < CTL; k++) {
            c[k] = r[k] = 0;
            d[k] = b * CTL + k == at ? 20000 : 0;
            mix_l[k] = mix_r[k] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (k = 0; k < CTL; k++) {
            float v = fabsf((float)(mix_l[k] + w[k]));
            if (b * CTL + k > at + 100u && v > pk) { pk = v; got = b * CTL + k; }
        }
    }
    return got;
}
static double h6[SECS * FS];
int main(void)
{
    char w[160];
    uint32_t mode, i, bursts;
    double db;
    for (mode = DMD_DIGITAL; mode <= DMD_PING; mode++) {
        run(mode, 1u);
        hp6k(outbuf, h6, SECS * FS);
        db = peak_db(h6, 1.99, 2.14);
        sprintf(w, "TIME 1/8 -> 1/4 at 2 s, %s: the burst above 6 kHz is %.1f dB against the tone (at most -28)", mode == DMD_PING ? "PING-PONG" : "DIGITAL", db);
        check(w, db <= -28.);
    }
    run(DMD_DIGITAL, 2u);
    hp6k(outbuf, h6, SECS * FS);
    db = peak_db(h6, 1.99, 2.14);
    sprintf(w, "a BPM step 120 -> 123 at 2 s, DIGITAL: the burst above 6 kHz is %.1f dB against the tone (at most -48)", db);
    check(w, db <= -48.);
    run(DMD_DIGITAL, 3u);
    hp6k(outbuf, h6, SECS * FS);
    {
        double tr = tone_rms() * 0.01;
        bursts = 0;
        for (i = 2u * FS + 1u; i < (uint32_t)(3.9 * FS); i++)
            bursts += fabs(h6[i]) > tr && fabs(h6[i - 1u]) <= tr;
    }
    sprintf(w, "an external clock re-measured at +-1 ms every 16th, DIGITAL: %u bursts above -40 dB in 1.9 s (none)", bursts);
    check(w, bursts == 0u);
    /* Crossing the signed Q16 midpoint must bend in the intended direction, rather than wrap the offset.
     * A half-sample/sample increase lowers the 330 Hz tone to 165 Hz; a decrease raises it to 495 Hz. */
    for (i = 4u; i <= 5u; i++) {
        uint32_t crossings = 0;
        double expected = i == 4u ? 165. : 495.;
        run(DMD_DIGITAL, i);
        for (uint32_t k = (uint32_t)(2.01 * FS); k < (uint32_t)(2.11 * FS); k++)
            crossings += outbuf[k] > 0 && outbuf[k - 1u] <= 0;
        sprintf(w, "large %s TIME change bends in the correct direction: about %u Hz (want %.0f)", i == 4u ? "upward" : "downward", crossings * 10u, expected);
        check(w, fabs(crossings * 10. - expected) <= 10.);
    }
    /* a steady delay, and a TIME changed 6 s before the impulse (the glide has ended): the impulse's first repeat lands exactly at the delay time */
    {
        uint32_t t, got, want;
        static const char *name[] = {"steady", "TIME changed 6 s before", "large upward TIME change (11025 -> 65535)", "large downward TIME change (65535 -> 11025)", "1/4 at 40 BPM (66150 samples) halved to fit"};
        for (t = 0; t < 5u; t++) {
            got = first_repeat(t, &want);
            sprintf(w, "%s: the impulse's first repeat lands at the delay time (%u, want %u)", name[t], got, want);
            check(w, got == want);
        }
    }
    /* R03: every division at every tempo plays as the named time halved until it fits (never clamped), and the card names it */
    {
        uint32_t bpm, dv, off = 0, tot = 0;
        for (bpm = 40; bpm <= 250u; bpm++)
            for (dv = 0; dv < 17u; dv++) {
                uint32_t want;
                song.g[G_BPM] = (int16_t)bpm;
                song.g[G_DTIME] = (int16_t)dv;
                want = div_samples(dv);
                while (want >= DLY_LEN)
                    want >>= 1;
                tot++;
                off += delay_samples() != (want < 16u ? 16u : want);
            }
        sprintf(w, "every division at 40 .. 250 BPM plays as the named time halved to fit the line: %u of %u off", off, tot);
        check(w, off == 0u);
    }
    {
        uint32_t k;
        static const struct { int bpm, tm, mode; uint32_t samples; int shown; } C[] = {
            {77, 6, DMD_RAND, 34363u, 0},       /* M05: 1/2 at 77 BPM (68726) halves to 1/4 (was 65535, 4.6 % off) */
            {71, 13, DMD_RAND, 55900u, 14},     /* M13: 2D at 71 BPM (111801) halves to 4D (was 65535, 41 % off) */
            {71, 14, DMD_PING, 27950u, 15},     /* M07: 4D at 71 BPM in PING-PONG plays 8D (the card said 4D) */
            {120, 14, DMD_DIGITAL, 33075u, 14}, /* fits: unchanged */
            {40, 0, DMD_OCTUP, 33075u, 1},      /* 1/4 at 40 BPM (66150) does not fit: halves to 1/8 */
        };
        for (k = 0; k < 5u; k++) {
            uint32_t got;
            song.g[G_BPM] = (int16_t)C[k].bpm;
            song.g[G_DTIME] = (int16_t)C[k].tm;
            song.g[G_DMODE] = (int16_t)C[k].mode;
            dm_split = 0;
            got = C[k].mode == DMD_PING ? delay_split() : delay_samples();
            sprintf(w, "BPM %d, TIME %d, mode %d: plays %u samples (want %u), the card shows division %u (want %d)", C[k].bpm, C[k].tm, C[k].mode, got, C[k].samples, delay_div_shown(), C[k].shown);
            check(w, got == C[k].samples && delay_div_shown() == (uint32_t)C[k].shown);
        }
    }
    printf("DELAY GLIDE: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
