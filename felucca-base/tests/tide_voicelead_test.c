/* SPDX-License-Identifier: GPL-3.0-only */
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

/* ------------------------------------------------------------------ generation reset (audit M06, M07, B01, B02, B06, B17) --- */
/* two parts of 16 notes: part 1 a bass line (C2 .. G2), part 2 a lead line; PLAY from the top */
static void gen_setup(void)
{
    ui_power_on(); frame();
    gl.on = gl.armed = 0; transport_req = 0; song.playing = song.rec = 0;
    song.g[G_CLOCK] = 0; song.g[G_BPM] = 240;
    memset(sqm, 0, sizeof sqm);  memset(&rhythm, 0, sizeof rhythm);
    memset(&tarc, 0, sizeof tarc); tarc_clear();
    for (uint32_t p = 0; p < 2u; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_AMODE] = AM_OFF; t->p[P_MUTE] = 0; t->p[P_SQNT] = 1;
        t->seq_active = 1;
        for (uint32_t i = 0; i < 16u; i++) {
            t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].vel = 100;
            t->step[i].note[0] = (uint8_t)(p ? 60 + (i % 5u) * 2u : 36 + (i % 5u) * 2u);
        }
    }
    sqm_auto = 0;
    morph.cfg |= MC_PIT;
    scale_g = 1; root_g = 0;
    seq_start(); song.playing = 1;
}
static uint32_t gen_walks(uint32_t pi, uint32_t n)         /* how many of n seeds the pitch walk moves a note of part pi */
{
    uint32_t k, moved = 0;
    for (k = 0; k < n; k++) {
        sqm_end_t x;
        memset(&x, 0, sizeof x);
        moved += sqm_pitch_walk(&trk[pi], &x, 1000u + k * 77u) < sqm_len(&trk[pi]);
    }
    return moved;
}
static void gen_pitch_cases(void)
{
    uint32_t lead, bass;
    gen_setup();
    bass = gen_walks(0, 200u); lead = gen_walks(1, 200u);
    printf("   pitch walk on a major scale, 200 seeds: bass line %u, lead line %u\n", bass, lead);
    CHECK("M06: PITCH never walks a bass line (notes under C3)", bass == 0u);
    CHECK("M06: .. but it walks a lead line (the fixture is live)", lead > 100u);
    scale_g = 0;                                           /* CHR: any pitch class is allowed, a walk would be chromatic */
    lead = gen_walks(1, 200u);
    printf("   pitch walk on CHR (chromatic mask), 200 seeds: lead line %u\n", lead);
    CHECK("B17: PITCH does nothing while the scale is CHR", lead == 0u);
    scale_g = 1;
    for (uint32_t i = 0; i < 16u; i++) { trk[1].step[i].n = 3; trk[1].step[i].note[0] = 48; trk[1].step[i].note[1] = 60; trk[1].step[i].note[2] = 67; }
    {   /* a chord: only its top note walks, the bass never */
        uint32_t k, ok = 1, moved = 0;
        for (k = 0; k < 100u; k++) {
            sqm_end_t x;
            uint32_t at;
            memset(&x, 0, sizeof x);
            at = sqm_pitch_walk(&trk[1], &x, 5000u + k * 31u);
            if (at < 16u) { moved++; ok &= sqm_pitch_voice(&trk[1].step[at], at) == 2u; }
        }
        CHECK("M06: a chord's top note walks (the lowest, the bass, never)", ok && moved > 50u);
    }
}
static void gen_voicelead_cases(void)
{
    sqm_end_t x;
    step_t out[NSTEP];
    uint32_t i, kept = 0;
    gen_setup();
    morph.cfg &= ~MC_PIT;                                  /* PITCH OFF (the default) */
    morph_musical = 1;
    for (i = 0; i < 16u; i++) chord(&trk[0].step[i], 36, i % 2u ? 55 : 52, i % 3u == 1u ? 60 : i % 3u ? 62 : 59, 0);   /* (C major) */
    memset(&x, 0, sizeof x); x.seed = 7; x.d[SD_VOIC] = 60;
    sqm_build(trk[0].step, 16, &x, 100u, out);
    for (i = 0; i < 16u; i++) kept += lowest(&out[i]) == lowest(&trk[0].step[i]) && pcs(&out[i]) == pcs(&trk[0].step[i]);
    printf("   VOIC +60, PITCH OFF, MUSIC: chords whose bass and chord tones are kept: %u of 16\n", kept);
    CHECK("M07: voice leading runs with PITCH OFF (the bass of every chord stays)", kept == 16u);
    morph_musical = 0;
    sqm_build(trk[0].step, 16, &x, 100u, out);
    for (i = kept = 0; i < 16u; i++) kept += lowest(&out[i]) == lowest(&trk[0].step[i]);
    CHECK("M07: .. and ROLL SMART / WILD keeps the plain octave inversions", kept < 16u);
    morph_musical = 1;
    {   /* B3 (firmware review FR13): OFF-SCALE PLAY, C major, a step {C E G Bb}, VOIC +60: the Bb survives (SNAP: it is moved onto the scale, as before) */
        uint32_t keep = offscale_g, bb;
        for (i = 0; i < 16u; i++) chord(&trk[0].step[i], 48, 52, 55, i % 2u ? 58 : 70);
        offscale_g = OS_PLAY;
        sqm_build(trk[0].step, 16, &x, 100u, out);
        for (i = bb = 0; i < 16u; i++) bb += (pcs(&out[i]) >> 10 & 1u) && pcs(&out[i]) == pcs(&trk[0].step[i]);
        CHECK("B3: OFF-SCALE PLAY: VOIC keeps an out-of-scale chord tone (Bb in C major)", bb == 16u);
        offscale_g = OS_SNAP;
        sqm_build(trk[0].step, 16, &x, 100u, out);
        for (i = bb = 0; i < 16u; i++) bb += pcs(&out[i]) >> 10 & 1u;
        CHECK("B3: OFF-SCALE SNAP: the Bb is snapped onto the scale", bb == 0u);
        offscale_g = (uint8_t)keep;
    }
    {   /* b7 idea 2: the chord bed breathes with ARC: PEAK opens (drop-2, the top up while at G5 or under), BREAK closes and inverts down, never a new pitch class or under the lowest note */
        uint8_t a[4] = {60, 63, 67, 70}, b[4] = {48, 55, 63, 70}, c[4] = {60, 63, 67, 70};
        unsigned pa, pb;
        sqm_vchord(a, 4, 1u, 60, 82, 79);
        CHECK("ARC voicing: PEAK opens Cm7 60 63 67 70 to 60 67 70 75", a[0] == 60 && a[1] == 75 && a[2] == 67 && a[3] == 70);
        sqm_vchord(b, 4, 2u, 40, 82, 79);
        pa = 0, pb = 0;
        for (i = 0; i < 4u; i++) pa |= 1u << (b[i] % 12u), pb |= 1u << ((uint8_t[]){48, 55, 63, 70})[i] % 12u;
        CHECK("ARC voicing: BREAK closes 48 55 63 70 inside the octave and inverts it down, the same pitch classes", pa == pb && b[0] >= 40 && b[1] < 60 && b[2] < 60 && b[3] < 60);
        sqm_vchord(c, 4, 2u, 60, 82, 79);
        CHECK("ARC voicing: BREAK never goes under the part's lowest written note", c[0] == 60 && c[1] == 63 && c[2] == 67 && c[3] == 70);
        sqm_arc = 0; sqm_varc = 2; sqm_bass_scan();
        CHECK("ARC voicing: ARC OFF voices nothing", sqm_varc == 0u);
    }
}
static void gen_home_load_cases(void)
{
    uint32_t i, k, ok, nz;
    /* B01: HOME clears the phrase pitch edits of End B too */
    gen_setup();
    sqm_refresh(1, 1);
    for (k = ok = 0; k < 50u && !ok; k++) ok = (uint32_t)phrase_pitch(1, 1234u + k * 77u);
    for (i = nz = 0; i < SQM_PITCH_BYTES; i++) nz |= sqm[1].b[sqm[1].top].pitch[i];
    CHECK("(fixture: a phrase pitch edit is on End B)", ok && nz);
    seqm_home(1);
    for (i = nz = 0; i < SQM_PITCH_BYTES; i++) nz |= sqm[1].b[sqm[1].top].pitch[i] | sqm[1].a.pitch[i];
    CHECK("B01: HOME leaves no pitch edit on either end", !nz && !sqm[1].aon && !sqm[1].fader);
    /* B02: a multi load and a pattern load start as written */
    for (k = 0; k < 2u; k++) {
        uint32_t diff;
        gen_setup();
        project_save(1);
        for (i = 0; i < 12u; i++) seqm_wander_gen(1, 0u, 4u);
        for (i = ok = 0; i < 50u && !ok; i++) ok = (uint32_t)phrase_pitch(1, 999u + i * 31u);
        seqm_set_fader(1, 127);
        sqm_refresh(1, 1);
        CHECK("(fixture: a generation is on part 2)", sqm[1].aon && ok);
        if (k) pat_load(&trk[1], 0); else project_load(1);
        sqm_refresh(1, 0);
        for (i = nz = 0; i < SQM_PITCH_BYTES; i++) nz |= sqm[1].b[sqm[1].top].pitch[i] | sqm[1].a.pitch[i];
        for (i = diff = 0; i < 16u; i++) diff += memcmp(sqm_step(&trk[1], i, 0), &trk[1].step[i], sizeof(step_t)) != 0;
        printf("   %s load: aon %u fader %u pitch %u, steps played otherwise than written (End A) %u\n", k ? "pattern" : "multi", sqm[1].aon, sqm[1].fader, nz != 0u, diff);
        CHECK(k ? "B02: a pattern load starts as written (no generation, no pitch)" : "B02: a multi load starts as written (no generation, no pitch)",
              !sqm[1].aon && !sqm[1].fader && !nz && !diff);
    }
    /* B02: an edited pattern (a new source) drops the pitch offsets stored for the old step indexes */
    gen_setup();
    sqm_refresh(1, 1);
    for (k = ok = 0; k < 50u && !ok; k++) ok = (uint32_t)phrase_pitch(1, 4321u + k * 17u);
    trk[1].step[3].note[0] = (uint8_t)(trk[1].step[3].note[0] + 1u);
    sqm_refresh(1, 0);
    for (i = nz = 0; i < SQM_PITCH_BYTES; i++) nz |= sqm[1].b[sqm[1].top].pitch[i] | sqm[1].a.pitch[i];
    CHECK("B02: a pattern edit drops the pitch offsets of the old pattern", ok && !nz);
}
/* ARC: STOP / PLAY pauses it, a K1 stop or a load restarts it at RISE (D6, revised) */
static void gen_arc_blocks(uint32_t n)
{
    for (uint32_t b = 0; b < n; b++) {
        events_block(CTL);
        glide_poll();
        tarc_poll();
    }
}
static void gen_arc_case(void)
{
    uint32_t phr, phase, cnt;
    gen_setup();
    morph_musical = 1; sqm_arc = 2;
    gl.on = 3; gl.mode = GM_WANDER; gl.mask = 3; gl.armed = 0; gl.hold = 0;
    gen_arc_blocks(4u * 4u * 12000u / CTL * 3u);           /* about 3 phrases (4 bars each) */
    phr = tarc.phr; phase = tarc.phase; cnt = tarc.cnt;
    printf("   ARC after PLAY: run %u phrase %u phase %s\n", tarc.run, phr, tarc_name());
    CHECK("(fixture: ARC runs and counts phrases)", tarc.run && phr >= 2u);
    song.playing = 0; glide_poll(); tarc_poll(); gen_arc_blocks(20u);
    CHECK("ARC: STOP keeps the arc where it was", tarc.run && tarc.phr == phr && tarc.phase == phase && tarc.cnt == cnt);
    seq_start(); song.playing = 1;
    gen_arc_blocks(40u);
    CHECK("ARC: PLAY resumes it (no restart at RISE)", tarc.run && tarc.phr >= phr && tarc.cnt == cnt);
    glide_stop(); glide_poll(); tarc_poll();
    CHECK("ARC: a K1 stop ends it", !tarc.run);
    gl.on = 3; gl.mode = GM_WANDER; gl.mask = 3;
    gen_arc_blocks(4u * 4u * 12000u / CTL * 2u);
    CHECK("(fixture: running again)", tarc.run && tarc.phr >= 1u);
    pat_load(&trk[0], 0);
    CHECK("ARC: a pattern load restarts it", !tarc.run);
    gl.on = 3; gl.mode = GM_WANDER; gl.mask = 3;
    gen_arc_blocks(4u * 4u * 12000u / CTL * 2u);
    CHECK("(fixture: running once more)", tarc.run && tarc.phr >= 1u);
    project_save(2);
    project_load(2);
    CHECK("ARC: a multi load restarts it", !tarc.run);
    gl.on = 3; gl.mode = GM_WANDER; gl.mask = 3;           /* (the load ended the trip: the player starts it again) */
    gen_arc_blocks(40u);
    printf("   after the load: run %u phase %s phrase %u gl.on %u\n", tarc.run, tarc_name(), tarc.phr, gl.on);
    CHECK("  .. at RISE", tarc.run && tarc.phase == AP_RISE && tarc.phr < 2u);
}

/* D3: a part whose sound is in the BASS family does not walk, whatever its notes; a lead sound still does */
static uint32_t gen_find_cat(uint32_t e, uint32_t cat)
{
    for (uint32_t k = 0; k < ds_count(e); k++)
        if (ds_cat_of(e, k) == cat) return k;
    return 0xFFFFu;
}
static void gen_bass_family_case(void)
{
    uint32_t e = trk[0].eng_req, kb, kl, bass, lead;
    gen_setup();
    e = trk[0].eng_req;
    kb = gen_find_cat(e, CAT_BASS); kl = gen_find_cat(e, CAT_LEAD);
    CHECK("(fixture: the engine has a bass and a lead sound)", kb != 0xFFFFu && kl != 0xFFFFu);
    trk[0].user = 0; trk[1].user = 0;
    trk[0].preset = (uint8_t)kb; trk[1].preset = (uint8_t)kl;
    trk[1].eng_req = (uint8_t)e;
    for (uint32_t i = 0; i < 16u; i++) trk[0].step[i].note[0] = (uint8_t)(60 + (i % 5u) * 2u);   /* the bass sound's line sits above C3 */
    bass = gen_walks(0, 200u); lead = gen_walks(1, 200u);
    printf("   pitch walk, a bass-family sound with its line above C3: %u of 200; a lead sound: %u\n", bass, lead);
    CHECK("D3: a BASS-family sound does not walk, its line above C3 or not", bass == 0u);
    CHECK("  .. a lead sound still walks", lead > 100u);
}
/* D6: a load in mid-trip puts the loaded parts back at A and the running trip starts again from there; the tide itself keeps running */
static uint32_t gen_varied(void)
{
    uint32_t n = 0;
    for (uint32_t p = 0; p < 2u; p++)
        for (uint32_t i = 0; i < 16u; i++)
            n += memcmp(sqm_step(&trk[p], i, 1), &trk[p].step[i], sizeof(step_t)) != 0;
    return n;
}
static void gen_trip_case(void)
{
    for (uint32_t k = 0; k < 2u; k++) {
        uint32_t b, worst = 0, mid;
        gen_setup();
        morph_musical = 1;
        project_save(3);
        gl.on = 3; gl.mode = GM_WANDER; gl.mask = 3; gl.armed = 0; gl.hold = 0; gl.time = 4; gl.to = 1; glide_begin_lanes();
        for (b = 0; b < 48000u / CTL * 8u; b++) { events_block(CTL); seqm_poll(); glide_poll(); }
        mid = sqm[0].fader | sqm[1].fader;
        CHECK("(fixture: mid-trip, the SEQ fader is off A and the variation plays)", mid && gen_varied());
        if (k) { pat_load(&trk[0], 0); pat_load(&trk[1], 0); } else project_load(3);
        for (b = 0; b < 48000u / CTL; b++) {                /* the next bar */
            uint32_t n;
            events_block(CTL); seqm_poll(); glide_poll();
            n = gen_varied();
            worst = n > worst ? n : worst;
        }
        printf("   %s load in mid-trip (fader %u): steps varied in the next bar: %u at most; tide lanes %u, SEQ fader now %u\n", k ? "pattern" : "multi", mid, worst, gl.on, sqm[0].fader);
        CHECK(k ? "D6: a pattern load in mid-trip: the next bar plays the pattern as written" : "D6: a multi load in mid-trip: the next bar plays the pattern as written", worst == 0u);
        CHECK("  .. and the tide is still running", (gl.on & 2u) && gl.mode == GM_WANDER);
    }
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
    gen_pitch_cases();
    gen_voicelead_cases();
    gen_home_load_cases();
    gen_arc_case();
    gen_bass_family_case();
    gen_trip_case();
    printf("   %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
