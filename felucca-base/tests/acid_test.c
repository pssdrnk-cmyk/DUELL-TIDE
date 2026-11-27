/* SPDX-License-Identifier: GPL-3.0-only */
/* ACID (b8, work/b8/acid/DESIGN.md): ANALOG WAVE 7 ACID / 8 ACSQ play their accents like a 303; same sources as the firmware (hostsim.c).
 * build/host/acid_test (host_tests.sh acid):
 *   LADDER  ACID's filter, the 303's diode ladder: RES 127 rings and stays bounded, a low note keeps its body at RES 127; ACSQ's square is high 51 .. 56 % of the cycle
 *   NONACID the same accent on WAVE SAW: ENV 2 keeps its Decay, the capacitor stays empty, the level step is the velocity's alone
 *   DECAY   an accented ACID note's ENV 2 falls to a half in about 140 ms (b13-acid: Open303's accent decay, a time constant of 200 ms, the 303's shortest) whatever a
 *           long Decay says (a plain note keeps the Decay's time); a Decay shorter than that stays (the accent never makes it longer)
 *   LEVEL   the accent's level burst: +6 .. 9 dB over a plain note at the start (Open303's VCA, ~+7 dB) (up to unity, no more: above it the amp ramp overflows), falling with the accent's ENV 2
 *   PUSH    the capacitor: one accent charges it, a plain note leaves it empty; accents in a row stack; it charges the same at any RES (RES mixes it with the
 *           accent's own sweep in the cutoff, voice.c)
 *   SLIDE   a slid step (LEGATO, slide_glide as the sequencer sets it) does not retrigger and is an RC lag: 90 % of a semitone and of an
 *           octave in 45 .. 60 ms, 99 % in 90 .. 120 ms
 *   PLAIN   a plain note slid into after an accent takes its own velocity: it is not accented
 *   WAVE    the WAVE list has ACID and ACSQ */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("acid: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int32_t o[2 * CTL];
static int32_t rec[400 * 2 * CTL];                    /* a render of up to 400 blocks */
static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
/* a clean ANALOG part: one oscillator, no effects, the filter open (bypassed), ENV 2 as a decay of E2DEC */
static track_t *fresh(int wave, int e2dec, int res)
{
    uint32_t k;
    track_t *t = &trk[0];
    memset(trk, 0, sizeof trk);                        /* every piece of audio state back to the start (as tests/velaccent_test.c dsp_clean) */
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(ins, 0, sizeof ins);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    mix_xf = mix_split = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    rng_state = 0x1234567u;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : eng_by_name("ANALOG"), 0);
    for (k = 1; k < NPART; k++)
        trk[k].p[P_MUTE] = 1;
    song.sel = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_TRIM] = 60;                                 /* well under the master limiter, whose gain would hide a level step */
    t->p[P_E0] = (int16_t)wave;
    t->p[P_E1] = t->p[P_E2] = t->p[P_E3] = t->p[P_E6] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 10;
    t->p[P_KTRK] = 0;
    t->p[P_VOICE] = V_LEGATO;
    t->p[P_GLMODE] = 1;                                /* TIME */
    t->p[P_GLIDE] = 55;                                /* 54 ms */
    t->p[P_FREEA] = UF_STEPS;                          /* the filter open: no filter in the way of the level */
    t->p[P_FREEB] = (int16_t)res;
    t->p[P_E2FLT] = 60;
    t->p[P_E2ATK] = 0;
    t->p[P_E2DEC] = (int16_t)e2dec;
    t->p[P_E2SUS] = 0;
    return t;
}
static void blocks(uint32_t n)
{
    while (n--)
        mix_block(o, CTL);
}
static double ms_of(uint32_t n) { return n * (double)CTL * 1000.0 / FS; }
static void render(uint32_t n, int32_t *dst)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        mix_block(o, CTL);
        memcpy(dst + i * 2 * CTL, o, sizeof o);
    }
}
static double energy(const int32_t *d, uint32_t b0, uint32_t b1)   /* mean |sample| of blocks b0 .. b1 - 1 */
{
    double s = 0;
    uint32_t i;
    for (i = b0 * 2u * CTL; i < b1 * 2u * CTL; i++)
        s += d[i] < 0 ? -d[i] : d[i];
    return s / ((double)(b1 - b0) * 2.0 * CTL);
}
/* blocks until ENV 2 of voice 0 falls under a half */
static uint32_t env2_half(track_t *t, uint32_t vel, uint32_t max)
{
    uint32_t n = 0;
    trk_note_on(t, 45, vel);
    blocks(1);
    while (t->v[0].env2 > (1 << 23) && n < max) {
        blocks(1);
        n++;
    }
    return n;
}

/* ACID's filter is the 303's diode ladder (voice.c acid_lad), not filter 1's SVF: a plain note at RES 127 rings without running away; the 303's bass stays
 * (the loop's 150 Hz high-pass: RES 127 takes little of a low note's body); the 303 square (eng_analog.c osc_acsq) is high a little over half the cycle */
static double band_db(const int32_t *d, uint32_t b0, uint32_t b1, double f)   /* the level of one frequency (dB), left channel, blocks b0 .. b1 - 1 */
{
    double re = 0, im = 0, w = 2.0 * M_PI * f / FS;
    uint32_t i, n = (b1 - b0) * CTL;
    for (i = 0; i < n; i++) {
        double x = d[(b0 * CTL + i) * 2];
        re += x * cos(w * i);
        im += x * sin(w * i);
    }
    return 20.0 * log10(2.0 * sqrt(re * re + im * im) / n + 1e-9);
}
static void ladder(void)
{
    int32_t pk = 0;
    uint32_t i, hi = 0, n = 0, ph;
    double b0, b127;
    char s[200];
    track_t *t = fresh(7, 100, 127);
    t->p[P_FREEA] = 40;
    trk_note_on(t, 45, 96);
    render(300, rec);
    for (i = 0; i < 300u * 2u * CTL; i++)
        pk = rec[i] > pk ? rec[i] : -rec[i] > pk ? -rec[i] : pk;
    snprintf(s, sizeof s, "ACID: RES 127 at CUT 40 rings and stays bounded (peak %d, under full scale)", pk);
    check(s, pk > 1000 && pk < 32767);
    t = fresh(7, 100, 0);
    t->p[P_FREEA] = 40;
    trk_note_on(t, 33, 96);                             /* A1, 55 Hz */
    render(200, rec);
    b0 = band_db(rec, 100, 200, 55.0);
    t = fresh(7, 100, 127);
    t->p[P_FREEA] = 40;
    trk_note_on(t, 33, 96);
    render(200, rec);
    b127 = band_db(rec, 100, 200, 55.0);
    snprintf(s, sizeof s, "ACID: a low note keeps its body at RES 127: the fundamental %+.1f dB against RES 0 (no more than 6 dB lost)", b127 - b0);
    check(s, b127 > b0 - 6.0);
    for (ph = 0; ph < 0xFFFF0000u; ph += 0x10000u, n++)
        hi += osc_acsq(ph, 0x10000u) > 0;
    snprintf(s, sizeof s, "ACSQ: the 303 square is high for %.1f %% of the cycle (51 .. 56 %%)", 100.0 * hi / n);
    check(s, hi > n * 51u / 100u && hi < n * 56u / 100u);
}


static void nonacid(void)
{
    track_t *t = fresh(0, 85, 100);
    uint32_t plain, acc;
    plain = env2_half(t, 96, 2000);
    blocks(200);
    trk_note_off(t, 45);
    blocks(50);
    t = fresh(0, 85, 100);
    acc = env2_half(t, 127, 2000);
    check("WAVE SAW: an accent keeps ENV 2's decay (half time the same, +-1 block)", acc + 1u >= plain && acc <= plain + 1u);
    blocks(200);
    check("WAVE SAW: the ACID capacitor stays empty", t->acid_y == 0);
}

static void decay(void)
{
    uint32_t plain, acc, plain2;
    double ap, aa;
    track_t *t = fresh(7, 110, 100);
    plain = env2_half(t, 96, 6000);
    t = fresh(7, 110, 100);
    acc = env2_half(t, 127, 6000);
    t = fresh(7, 120, 100);
    plain2 = env2_half(t, 96, 9000);
    ap = ms_of(plain);
    aa = ms_of(acc);
    {
        char s[160];
        snprintf(s, sizeof s, "ACID: ENV 2 half time, Decay 110: plain %.0f ms, accent %.0f ms (the accent: ~140 ms = tau 200 ms)", ap, aa);
        check(s, aa > 125.0 && aa < 155.0 && ap > 2.0 * aa);
    }
    t = fresh(7, 120, 100);
    acc = env2_half(t, 127, 9000);
    {
        char s[160];
        snprintf(s, sizeof s, "ACID: the accent's time does not follow the Decay knob (Decay 110 / 120: %.0f / %.0f ms), a plain note does (%.0f ms)", aa, ms_of(acc), ms_of(plain2));
        check(s, ms_of(acc) > aa * 0.9 && ms_of(acc) < aa * 1.1 && ms_of(plain2) > 1.5 * ap);
    }
    t = fresh(7, 85, 100);
    plain = env2_half(t, 96, 3000);
    t = fresh(7, 85, 100);
    acc = env2_half(t, 127, 3000);
    {
        char s[160];
        snprintf(s, sizeof s, "ACID: a short Decay (85) is never made longer by an accent: half time plain %.1f ms, accent %.1f ms", ms_of(plain), ms_of(acc));
        check(s, acc <= plain + 1u);
    }
}

static void level(void)
{
    double e0, l0, e1, l1;
    track_t *t = fresh(7, 85, 100);
    trk_note_on(t, 45, 96);
    render(300, rec);
    l0 = energy(rec, 1, 30);
    l1 = energy(rec, 250, 290);
    t = fresh(7, 85, 100);
    trk_note_on(t, 45, 127);
    render(300, rec);
    e0 = energy(rec, 1, 30);
    e1 = energy(rec, 250, 290);
    {
        char s[160];
        snprintf(s, sizeof s, "ACID: the accent's level over a plain note's: start +%.1f dB, 250 ms later %+.1f dB (+6 .. 9 dB, then falling with the accent's ENV 2: +1 .. 4 dB)", 20.0 * log10(e0 / l0), 20.0 * log10(e1 / l1));
        check(s, 20.0 * log10(e0 / l0) > 6.0 && 20.0 * log10(e0 / l0) < 9.0 && 20.0 * log10(e1 / l1) > 1.0 && 20.0 * log10(e1 / l1) < 4.0);
    }
}

/* four notes of 168 blocks (122 ms, a 16th at ~123 BPM), the gate 58 %, accents on the notes of mask; the capacitor's peak per note */
static void run4(int res, uint32_t mask, int *pk)
{
    uint32_t n, b, m;
    track_t *t = fresh(7, 85, res);
    for (n = 0; n < 4u; n++) {
        int mx = 0;
        trk_note_on(t, 45, mask >> n & 1u ? 127u : 96u);
        for (b = 0; b < 168u; b++) {
            if (b == 97u)
                trk_note_off(t, 45);
            blocks(1);
            m = (uint32_t)t->acid_y;
            if ((int)m > mx)
                mx = (int)m;
        }
        pk[n] = mx;
    }
}
static void push(void)
{
    int a[4], p[4], lo[4], hi[4];
    char s[200];
    run4(100, 1, a);
    run4(100, 0, p);
    run4(100, 15, hi);
    run4(0, 15, lo);
    snprintf(s, sizeof s, "ACID: one accent at RES 100 charges the capacitor to %.2f of full (0.25 .. 0.40); a plain note, %d", a[0] / 32768.0, p[0]);
    check(s, a[0] > 0.25 * 32768 && a[0] < 0.40 * 32768 && p[0] == 0 && p[3] == 0);
    snprintf(s, sizeof s, "ACID: four accents in a row at RES 100 stack: %.2f %.2f %.2f %.2f (each higher)", hi[0] / 32768.0, hi[1] / 32768.0, hi[2] / 32768.0, hi[3] / 32768.0);
    check(s, hi[1] > hi[0] * 1.2 && hi[2] > hi[1] && hi[3] > hi[2]);
    snprintf(s, sizeof s, "ACID: the capacitor charges the same at RES 0 and RES 100 (%.2f %.2f / %.2f %.2f): RES mixes it into the cutoff, it does not change it",
             lo[0] / 32768.0, lo[1] / 32768.0, hi[0] / 32768.0, hi[1] / 32768.0);
    check(s, lo[0] == hi[0] && lo[1] == hi[1] && lo[3] == hi[3]);
    run4(100, 1, a);
    snprintf(s, sizeof s, "ACID: the capacitor runs down through the plain notes after the accent (%.2f -> %.2f -> %.2f -> %.2f)", a[0] / 32768.0, a[1] / 32768.0, a[2] / 32768.0, a[3] / 32768.0);
    check(s, a[1] < a[0] && a[2] < a[1] && a[3] < a[2]);
}

/* a slid step: the second note while the first is held, slide_glide set as the sequencer sets it; blocks to 90 % of the way */
static void slide_one(uint32_t from, uint32_t to, double *ms90, double *ms99, int *retrig)
{
    track_t *t = fresh(7, 85, 100);
    voice_t *v = &t->v[0];
    int32_t q0, q1, q;
    uint32_t n = 0, age;
    trk_note_on(t, from, 100);
    blocks(30);
    age = v->age;
    t->slide_glide = 1;
    trk_note_on(t, to, 100);
    t->slide_glide = 0;
    *retrig = v->age != age || v->stage == 1u;
    q0 = from * 16 * 65536;
    q1 = to * 16 * 65536;
    *ms90 = *ms99 = -1.0;
    do {
        q = v->pitch_cur * 65536 + (int32_t)v->gfrac;
        if (*ms90 < 0 && (double)(q - q0) >= 0.9 * (double)(q1 - q0))
            *ms90 = ms_of(n);
        if ((double)(q - q0) >= 0.99 * (double)(q1 - q0)) {
            *ms99 = ms_of(n);
            break;
        }
        mix_block(o, CTL);
        n++;
    } while (n < 800u);
}
static void slide(void)
{
    double a, b, a9, b9;
    int r1, r2;
    char s[200];
    slide_one(45, 46, &a, &a9, &r1);
    slide_one(45, 57, &b, &b9, &r2);
    snprintf(s, sizeof s, "ACID: a slid step does not retrigger (age and stage kept)");
    check(s, !r1 && !r2);
    snprintf(s, sizeof s, "ACID: a slid step is at 90 %% in %.1f ms for a semitone and %.1f ms for an octave (45 .. 60 ms)", a, b);
    check(s, a >= 45.0 && a <= 60.0 && b >= 45.0 && b <= 60.0);
    snprintf(s, sizeof s, "ACID: the RC tail: 99 %% in %.1f ms for a semitone and %.1f ms for an octave (90 .. 120 ms; 99 %% / 90 %% about 2)", a9, b9);
    check(s, a9 >= 90.0 && a9 <= 120.0 && b9 >= 90.0 && b9 <= 120.0 && a9 > 1.7 * a && b9 > 1.7 * b);
}

/* an accent slid into a plain note: the plain note takes its own velocity (not accented), so ENV 2 / the capacitor / the level run down as for a plain note */
static void plain_after_accent(void)
{
    track_t *t = fresh(7, 85, 100);
    voice_t *v = &t->v[0];
    int y0;
    trk_note_on(t, 45, 127);
    blocks(30);
    y0 = t->acid_y;
    t->slide_glide = 1;
    trk_note_on(t, 47, 96);
    t->slide_glide = 0;
    check("ACID: a plain note slid into after an accent is plain (its own velocity, no retrigger)", v->vel == 96u && v->stage != 1u);
    blocks(60);
    check("ACID: and the capacitor runs down under it (an accent's would keep charging)", t->acid_y < y0);
}

static void wave(void)
{
    const param_desc_t *d = &ENGINES[0]->edit[0];
    check("the ANALOG WAVE list ends with ACID and ACSQ", d->max == 8 && str_eq(d->names[7], "ACID") && str_eq(d->names[8], "ACSQ"));
}

int main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    ladder();
    nonacid();
    decay();
    level();
    push();
    slide();
    plain_after_accent();
    wave();
    printf("acid: %s\n", bad ? "FAIL" : "ok");
    return bad ? 1 : 0;
}
