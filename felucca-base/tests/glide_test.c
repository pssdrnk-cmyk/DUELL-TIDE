/* SPDX-License-Identifier: GPL-3.0-only */
/* GLIDE RATE and TIME against the GLD display (lane P of the 2026-11-04 audit, P01; voice.c glide_set, the glide step in
 * track_render, atg_voice), same sources as the firmware (hostsim.c). build/host/glide_test (host_tests.sh glide):
 *   TIME    MONO, 1 / 12 / 24 semitones up and down at GLD 40 / 64 / 127: the pitch reaches the new note within 5 % of the
 *           time GLD shows (TIME_MS_X10; it used to take at most ~16 ms a semitone, and GLD 40..127 all sounded alike)
 *   RATE    64 semitones in the time GLD shows: 1 / 12 / 24 semitones at GLD 100 / 110 / 127 take that share (5 %, or one
 *           block), and a higher GLD is slower (no plateau at the top of the knob)
 *   KEEP    a fresh voice and GLD 0 jump; LEGATO glides between held keys only; a POLY voice glides from its last pitch;
 *           a step SLIDE at GLD 0 keeps its RATE-40 speed
 *   FINE    a slow glide's pitch between the 1/16 semitones reaches the oscillator (m.fine): ANALOG's sine, TIME GLD 127, an
 *           octave, the frequency at 1 s / 2.5 s / 5 s within 1 cent of the straight line in semitones (whole 1/16 steps: ~3 c flat)
 *   TARGET  ARP TARGET moves at GLD's RATE (it used to move at least 1/16 semitone a block: GLD 85..127 alike) */
#define main hostsim_main
#include "hostsim.c"
#undef main

#ifdef GLIDE_OLD                                      /* (the firmware before P01: no fraction, apitch in 1/16 semitones) */
#define GFRAC(v) 0
#define APITCH(v) ((v)->apitch)
#else
#define GFRAC(v) ((v)->gfrac)
#define APITCH(v) ((v)->apitch >> 16)
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("glide: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static int32_t o[2 * CTL];
static track_t *fresh(uint32_t mode, int g, int glmode)
{
    uint32_t k;
    track_t *t = &trk[0];
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    vage = 0;
    rng_state = 0x1234567u;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : eng_by_name("ANALOG"), 0);
    for (k = 1; k < NPART; k++)
        trk[k].p[P_MUTE] = 1;
    song.sel = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 10;
    t->p[P_KTRK] = 0;
    t->p[P_VOICE] = (int16_t)mode;
    t->p[P_GLIDE] = (int16_t)g;
    t->p[P_GLMODE] = (int16_t)glmode;
    return t;
}
static void blocks(uint32_t n)
{
    while (n--)
        mix_block(o, CTL);
}
static int there(const voice_t *v) { return v->pitch_cur == v->pitch16 && !GFRAC(v); }
/* blocks until v's pitch is on its note (0: there before the first; max + 1: not there in max) */
static uint32_t arrive(const voice_t *v, uint32_t max)
{
    uint32_t n = 0;
    while (!there(v) && n <= max) {
        mix_block(o, CTL);
        n++;
    }
    return n;
}
static double ms_of(uint32_t n) { return n * (double)CTL * 1000.0 / FS; }
static double shown_ms(int g) { return TIME_MS_X10[g & 127] / 10.0; }
/* MONO: from a sounding note to another one; the blocks the glide takes */
static uint32_t mono_glide(int g, int glmode, uint32_t from, uint32_t to, uint32_t max)
{
    track_t *t = fresh(V_MONO, g, glmode);
    trk_note_on(t, from, 100);
    blocks(4);
    trk_note_off(t, from);
    trk_note_on(t, to, 100);
    return arrive(&t->v[0], max);
}

static void time_mode(void)
{
    static const int G[3] = {40, 64, 127}, IV[3] = {1, 12, 24};
    uint32_t gi, ii, dir;
    for (gi = 0; gi < 3u; gi++)
        for (ii = 0; ii < 3u; ii++)
            for (dir = 0; dir < 2u; dir++) {
                char s[128];
                double want = shown_ms(G[gi]), got;
                uint32_t a = dir ? 48u + (uint32_t)IV[ii] : 48u, b = dir ? 48u : 48u + (uint32_t)IV[ii];
                got = ms_of(mono_glide(G[gi], 1, a, b, (uint32_t)(want * 2.0 * FS / 1000.0 / CTL) + 1000u));
                snprintf(s, sizeof s, "TIME GLD %3d, %2d semitones %-4s: %8.1f ms (GLD shows %.1f ms)", G[gi], IV[ii], dir ? "down" : "up", got, want);
                check(s, got > want * 0.95 && got < want * 1.05);
            }
}

static void rate_mode(void)
{
    static const int G[3] = {100, 110, 127}, IV[3] = {1, 12, 24};
    double last[3] = {0, 0, 0};
    uint32_t gi, ii;
    for (gi = 0; gi < 3u; gi++)
        for (ii = 0; ii < 3u; ii++) {
            char s[128];
            double want = shown_ms(G[gi]) * IV[ii] / 64.0, got, tol;
            got = ms_of(mono_glide(G[gi], 0, 48u, 48u + (uint32_t)IV[ii], (uint32_t)(want * 2.0 * FS / 1000.0 / CTL) + 1000u));
            tol = want * 0.05 > ms_of(1) ? want * 0.05 : ms_of(1);
            snprintf(s, sizeof s, "RATE GLD %3d, %2d semitones up: %8.1f ms (64 semitones in %.1f ms: %.1f ms)", G[gi], IV[ii], got, shown_ms(G[gi]), want);
            check(s, got > want - tol && got < want + tol);
            snprintf(s, sizeof s, "RATE GLD %3d, %2d semitones: slower than the GLD below (no plateau)", G[gi], IV[ii]);
            check(s, got > last[ii] * 1.2);
            last[ii] = got;
        }
}

static void keep(void)
{
    track_t *t;
    voice_t *v;
    uint32_t i, n;
    t = fresh(V_MONO, 64, 1);                         /* a fresh voice: no glide from nowhere */
    trk_note_on(t, 60, 100);
    check("a fresh voice starts on its note (GLD 64 TIME)", there(&t->v[0]) && t->v[0].pitch_cur == 960);
    t = fresh(V_MONO, 0, 1);                          /* GLD 0: a jump */
    trk_note_on(t, 48, 100);
    blocks(4);
    trk_note_off(t, 48);
    trk_note_on(t, 72, 100);
    check("GLD 0: MONO jumps to the next note", there(&t->v[0]) && t->v[0].pitch_cur == 72 * 16);
    t = fresh(V_LEGATO, 64, 1);                       /* LEGATO: held keys glide, a fresh phrase jumps */
    trk_note_on(t, 48, 100);
    blocks(4);
    trk_note_on(t, 60, 100);
    n = arrive(&t->v[0], 400);
    check("LEGATO: a key over a held one glides in GLD's TIME (5 %)", ms_of(n) > shown_ms(64) * 0.95 && ms_of(n) < shown_ms(64) * 1.05);
    trk_note_off(t, 60);
    trk_note_off(t, 48);
    blocks(4);
    trk_note_on(t, 72, 100);
    check("LEGATO: a key after the release jumps", there(&t->v[0]) && t->v[0].pitch_cur == 72 * 16);
    t = fresh(V_POLY, 64, 1);                         /* POLY: the nearest free voice glides from its last pitch */
    trk_note_on(t, 48, 100);
    blocks(4);
    trk_note_off(t, 48);
    for (n = 0; n < 4000u; n++) {
        for (i = 0; i < NVOICE && !t->v[i].active; i++)
            ;
        if (i == NVOICE)
            break;
        mix_block(o, CTL);
    }
    trk_note_on(t, 60, 100);
    for (v = 0, i = 0; i < NVOICE; i++)
        if (t->v[i].gate && t->v[i].note == 60)
            v = &t->v[i];
    n = v ? arrive(v, 400) : 0;
    check("POLY: a voice glides from the pitch it played last in GLD's TIME (5 %)", v && ms_of(n) > shown_ms(64) * 0.95 && ms_of(n) < shown_ms(64) * 1.05);
    t = fresh(V_MONO, 0, 0);                          /* a step SLIDE at GLD 0: RATE at GLD 40's speed */
    trk_note_on(t, 48, 100);
    blocks(4);
    t->slide_glide = 1;
    trk_note_off(t, 48);
    trk_note_on(t, 60, 100);
    n = arrive(&t->v[0], 400);
    {
        double want = shown_ms(40) * 12.0 / 64.0;
        char s[128];
        snprintf(s, sizeof s, "step SLIDE at GLD 0 (RATE): 12 semitones in %.1f ms (GLD 40's rate: %.1f ms, one block)", ms_of(n), want);
        check(s, ms_of(n) > want - ms_of(1) && ms_of(n) < want + ms_of(1));
    }
}

static void fine(void)
{
    static const double AT[3] = {1.0, 2.5, 5.0};
    track_t *t = fresh(V_MONO, 127, 1);
    uint32_t b = 0, k, i, cross = 0;
    double first = 0, last = 0, x0 = 0, n = 0;
    t->p[P_E0] = 3;                                   /* SIN */
    t->p[P_E1] = t->p[P_E2] = t->p[P_E3] = t->p[P_E5] = t->p[P_E6] = t->p[P_E7] = 0;   /* OSC A alone */
    t->p[P_E4] = 127;
    trk_note_on(t, 48, 100);
    blocks(4);
    trk_note_off(t, 48);
    trk_note_on(t, 60, 100);
    for (k = 0; k < 3u; k++) {
        uint32_t b0 = (uint32_t)((AT[k] - 0.05) * FS / CTL), b1 = (uint32_t)((AT[k] + 0.05) * FS / CTL);
        double want, got, c;
        char s[128];
        for (; b < b0; b++)
            mix_block(o, CTL);
        for (cross = 0; b < b1; b++) {
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++, n++) {
                double x = o[2u * i];
                if (x0 < 0 && x >= 0) {               /* a rising zero crossing, between the samples */
                    double at = n - 1.0 + x0 / (x0 - x);
                    if (!cross)
                        first = at;
                    last = at;
                    cross++;
                }
                x0 = x;
            }
        }
        got = (cross - 1u) * (double)FS / (last - first);
        want = 440.0 * pow(2.0, (48.0 + 12.0 * ((b0 + b1) * 0.5 * CTL / FS) / 10.0 - 69.0) / 12.0);
        c = 1200.0 * log2(got / want);
        snprintf(s, sizeof s, "FINE TIME GLD 127, an octave, at %.1f s: %+.2f cents from the straight line", AT[k], c);
        check(s, c > -1.0 && c < 1.0);
    }
}

/* ARP TARGET (ANALOG mask B, one key: root, root + 12, ...): how far the arp pitch moves in its first 60 blocks at 72 (a step is ~86) */
static void target(void)
{
    static const int G[2] = {100, 127};
    uint32_t gi;
    for (gi = 0; gi < 2u; gi++) {
        track_t *t = fresh(V_POLY, G[gi], 0);
        voice_t *v = 0;
        uint32_t b, i, on72 = 1;
        int32_t a0;
        double want = 60.0 * 1024.0 * CTL / (shown_ms(G[gi]) * FS / 1000.0), got;
        char s[128];
        t->p[P_AMODE] = AM_UP;
        t->p[P_AOCT] = 2;
        t->p[P_AGATE] = 127;
        t->p[P_APROB] = 127;
        t->p[P_ARATE] = 3;
        t->p[P_ATGT] = 2;                             /* B */
        input_on(t, 60, 100);
        for (b = 0; b < 2000u && t->atg_note != 72u; b++)
            mix_block(o, CTL);
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].gate)
                v = &t->v[i];
        if (!v) {
            check("ARP TARGET: a voice for the key", 0);
            continue;
        }
        a0 = APITCH(v);
        for (b = 0; b < 60u; b++) {
            mix_block(o, CTL);
            on72 &= t->atg_note == 72u;
        }
        got = APITCH(v) - a0;
        snprintf(s, sizeof s, "ARP TARGET GLD %3d (RATE): %5.1f / 16 semitone in 60 blocks (GLD's rate: %.1f)", G[gi], got, want);
        check(s, on72 && got > want * 0.95 - 1.0 && got < want * 1.05 + 1.0);
        input_off(t, 60);
    }
}

int main(void)
{
    time_mode();
    rate_mode();
    keep();
    fine();
    target();
    printf("glide: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad != 0;
}
