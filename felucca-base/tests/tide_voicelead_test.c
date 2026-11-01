/* M2a: MUSIC voicing over the loop join. The second walk from the last chord is kept only when the
 * whole cycle moves less; bass, chord tones, note count, range and held/slid chords stay. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(msg, x) do { checks++; if (!(x)) { failures++; printf("FAIL: %s\n", msg); } } while (0)

static uint32_t vl_rng = 12345u;
static uint32_t rnd(uint32_t n) { vl_rng = vl_rng * 1103515245u + 12345u; return (vl_rng >> 16) % n; }

/* upper-voice movement between consecutive chords of equal size, and the last-to-first join */
static void cycle(const step_t *s, uint32_t len, int32_t *inner, int32_t *join)
{
    int32_t prev[4] = {0}, first[4] = {0};uint32_t pn = 0, fn = 0;
    *inner = *join = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (!sqm_hit(&s[i]) || s[i].n > 4u) continue;
        int32_t c[4];uint32_t n = s[i].n;
        for (uint32_t k = 0; k < n; k++) c[k] = s[i].note[k];
        for (uint32_t a = 1; a < n; a++) for (uint32_t b = a; b && c[b] < c[b-1]; b--) { int32_t t = c[b]; c[b] = c[b-1]; c[b-1] = t; }
        if (fn) { if (pn == n) for (uint32_t k = 1; k < n; k++) *inner += voicelead_abs(c[k] - prev[k]); }
        else { memcpy(first, c, sizeof c); fn = n; }
        memcpy(prev, c, sizeof c); pn = n;
    }
    if (fn && fn == pn) for (uint32_t k = 1; k < fn; k++) *join += voicelead_abs(first[k] - prev[k]);
}
static void plain(const step_t *src, uint32_t len, step_t *out)          /* the voicing before M2a */
{
    int32_t c[4] = {0};uint32_t n = 0;
    memcpy(out, src, len * sizeof *out);
    voicelead_walk(src, len, out, c, &n, 1);
}
static int lowest(const step_t *s) { int m = 999; for (uint32_t k = 0; k < s->n; k++) if (s->note[k] < m) m = s->note[k]; return m; }
static unsigned pcs(const step_t *s) { unsigned m = 0; for (uint32_t k = 0; k < s->n; k++) m |= 1u << (s->note[k] % 12u); return m; }

static void chord(step_t *s, int bass, int a, int b, int c)
{
    memset(s, 0, sizeof *s);
    s->time = ST_NOTE; s->vel = 100;
    s->note[0] = (uint8_t)bass; s->note[1] = (uint8_t)a; s->note[2] = (uint8_t)b; s->n = 3;
    if (c) s->note[3] = (uint8_t)c, s->n = 4;
}

/* invariants between the generated input and the voiced result */
static void invariants(const char *what, const step_t *in, const step_t *out, uint32_t len)
{
    int ok = 1;
    for (uint32_t i = 0; i < len; i++) {
        const step_t *a = &in[i], *b = &out[i];
        if (a->n != b->n || a->time != b->time || a->flags != b->flags) { ok = 0; continue; }
        if (!sqm_hit(a)) continue;
        if (lowest(a) != lowest(b) || pcs(a) != pcs(b)) ok = 0;
        if (a->n < 3u || sqm_held(in, i, len) || (a->flags & SF_SLIDE)) { if (memcmp(a->note, b->note, sizeof a->note)) ok = 0; continue; }
        for (uint32_t k = 0; k < b->n; k++) {
            if (b->note[k] < 24 || b->note[k] > 108 || voicelead_abs((int32_t)b->note[k] - a->note[k]) > 12) ok = 0;
            for (uint32_t q = 0; q < b->n; q++) if (q != k && b->note[q] == b->note[k]) ok = 0;
        }
    }
    char m[160]; snprintf(m, sizeof m, "%s: bass, chord tones, note count, range, held and slid chords kept", what);
    CHECK(m, ok);
}

static void fixed_cases(void)
{
    step_t in[16], out[16], ref[16];int32_t i0, j0, i1, j1;
    /* an awkward join: the first chord is written an octave below the rest; walking forward from it lifts
     * the others down to meet it, but the loop then leaps from the last chord back to the first */
    memset(in, 0, sizeof in);
    chord(&in[0], 36, 52, 55, 0);  chord(&in[4], 41, 76, 79, 0);
    chord(&in[8], 43, 74, 79, 0);  chord(&in[12], 36, 76, 79, 0);
    memcpy(out, in, sizeof in); sqm_voicelead(in, 16, out); plain(in, 16, ref);
    cycle(ref, 16, &i0, &j0); cycle(out, 16, &i1, &j1);
    printf("   low first chord: inner %d -> %d, join %d -> %d\n", i0, i1, j0, j1);
    CHECK("low first chord: the whole cycle moves less", i1 + j1 < i0 + j0);
    CHECK("low first chord: the join is smoother", j1 < j0);
    invariants("low first chord", in, out, 16);

    /* an already smooth loop stays as the walk had it */
    memset(in, 0, sizeof in);
    chord(&in[0], 48, 60, 64, 67); chord(&in[4], 45, 60, 64, 69);
    chord(&in[8], 41, 60, 65, 69); chord(&in[12], 43, 59, 62, 67);
    memcpy(out, in, sizeof in); sqm_voicelead(in, 16, out); plain(in, 16, ref);
    CHECK("smooth loop: unchanged from the plain walk", !memcmp(out, ref, sizeof out));

    /* one chord: nothing to join */
    memset(in, 0, sizeof in); chord(&in[5], 48, 64, 67, 0);
    memcpy(out, in, sizeof in); sqm_voicelead(in, 16, out); plain(in, 16, ref);
    CHECK("one chord: as the plain walk", !memcmp(out, ref, sizeof out));

    /* a chord tied across the join is never moved */
    memset(in, 0, sizeof in);
    chord(&in[0], 48, 64, 67, 0); chord(&in[4], 41, 69, 72, 0); chord(&in[15], 43, 71, 74, 0);
    in[0].time = ST_TIE;           /* step 15 ties into step 0 */
    memcpy(out, in, sizeof in); sqm_voicelead(in, 16, out);
    CHECK("tie across the join: the held chord keeps its notes", !memcmp(out[15].note, in[15].note, sizeof in[15].note));
    invariants("tie across the join", in, out, 16);

    /* regenerating the same end gives the same voicing */
    memset(in, 0, sizeof in);
    chord(&in[0], 48, 64, 67, 0); chord(&in[4], 41, 69, 72, 0); chord(&in[8], 43, 71, 74, 0);
    memcpy(out, in, sizeof in); sqm_voicelead(in, 16, out);
    memcpy(ref, in, sizeof in); sqm_voicelead(in, 16, ref);
    CHECK("repeated generation: identical", !memcmp(out, ref, sizeof out));
}

static void random_cases(unsigned scale)
{
    unsigned better = 0, same = 0, worse = 0, invbad = 0;
    long gain = 0;
    scale_g = (int16_t)scale; root_g = (int16_t)rnd(12);
    for (unsigned run = 0; run < 3000; run++) {
        uint32_t len = 4u + rnd(13);step_t in[NSTEP], out[NSTEP], ref[NSTEP];
        memset(in, 0, sizeof in);
        for (uint32_t i = 0; i < len; i++) {
            step_t *s = &in[i];
            unsigned r = rnd(10);
            if (r < 3) continue;                                   /* rest */
            if (r == 3 && i) { s->time = ST_TIE; continue; }
            s->time = ST_NOTE; s->vel = 100;
            s->n = (uint8_t)(r == 4 ? 1 : 3 + rnd(2));
            if (rnd(12) == 0) s->flags = SF_SLIDE;
            int bass = (int)note_snap_to(trk, 36 + (int)rnd(14), 1u);
            s->note[0] = (uint8_t)bass;
            for (uint32_t k = 1; k < s->n; k++) {
                int nt;
                do nt = (int)note_snap_to(trk, bass + 3 + (int)rnd(30), 1u); while (nt <= bass);
                s->note[k] = (uint8_t)nt;
            }
            for (uint32_t k = 1; k < s->n; k++) for (uint32_t q = 0; q < k; q++) if (s->note[q] == s->note[k]) s->n = (uint8_t)k;
            if (s->n == 2) s->n = 1;
        }
        memcpy(out, in, len * sizeof *in); sqm_voicelead(in, len, out); plain(in, len, ref);
        int32_t i0, j0, i1, j1;
        cycle(ref, len, &i0, &j0); cycle(out, len, &i1, &j1);
        if (i1 + j1 < i0 + j0) better++, gain += (i0 + j0) - (i1 + j1);
        else if (memcmp(out, ref, len * sizeof *out)) worse++;   /* changed without a gain */
        else same++;
        unsigned f = failures; invariants("random", in, out, len); if (failures != f) { invbad++; failures = f + 1; }
    }
    printf("   scale %u: %u of 3000 loops improved (total movement -%ld semitones), %u kept, %u changed without a gain\n",
           scale, better, gain, same, worse);
    CHECK("random loops: some joins improve", better > 0);
    CHECK("random loops: never a change without a whole-cycle gain", worse == 0);
    CHECK("random loops: invariants on every loop", invbad == 0);
}

int main(void)
{
    ui_power_on(); frame();
    scale_g = 0; root_g = 0;
    printf("== tidevoicelead\n");
    fixed_cases();
    random_cases(0);
    random_cases(1);
    random_cases(3);
    printf("   %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
