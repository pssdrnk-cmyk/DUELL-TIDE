/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL dub pack (helper, content only): store the pack's patches through the firmware's own store, read their records back, and render them.
 *   dub_render SPEC OUTDIR            SPEC: the file tools/dub_pack/pack.py writes (one S block per patch)
 * For every patch: part 1 is set up as a factory sound is loaded (engine defaults, then the sound's values), End B is the block the sound
 * data builds (tools/endb.py), the part is stored in a slot with up_store_trk (the record the editor exports), the slot is loaded back
 * (up_load, as PATCHES does) and rendered. Writes OUTDIR/records.txt ("NN hex-of-record") and the WAVs; prints levels.
 * Built on tests/patchraw_test.c's world (simulated NOR flash): see tools/dub_pack/build.sh */
/* (build.sh makes build/dub/patchraw_nomain.c: tests/patchraw_test.c with its main renamed; ui_test.c undefines a main macro) */
#include "patchraw_nomain.c"
#include <math.h>

static int32_t ob[2 * CTL];
static int16_t *pcm;
static uint32_t npcm, cappcm;
static double peak, sumsq;
static void dp_fresh(void)
{
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    lim_env = LIM_T;
    memset(trk, 0, sizeof trk);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(mwork, 0, sizeof mwork);
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    host_tracks_init();
#if DUELL_TIDE && !defined(NO_MUSICAL)
    morph_musical = 1;
#endif
    song.sel = 0;
    vcap[0] = vcap[1] = 0;
    song.playing = 0;
    transport_req = 0;
}
static void dp_blocks(double sec)
{
    uint32_t n = (uint32_t)(sec * FS / CTL + 0.5), b, i;
    for (b = 0; b < n; b++) {
        mix_block(ob, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t x = ob[i];
            double a;
            if (x > 32767) x = 32767;
            if (x < -32768) x = -32768;
            a = fabs((double)x);
            if (a > peak) peak = a;
            sumsq += (double)x * (double)x;
            if (npcm < cappcm) pcm[npcm++] = (int16_t)x;
        }
    }
}
static void dp_w32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24 & 255, f); }
static void dp_w16(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static void dp_wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(2); }
    fwrite("RIFF", 1, 4, f); dp_w32(f, 36 + npcm * 2); fwrite("WAVEfmt ", 1, 8, f); dp_w32(f, 16); dp_w16(f, 1); dp_w16(f, 2); dp_w32(f, FS); dp_w32(f, FS * 4); dp_w16(f, 4); dp_w16(f, 16);
    fwrite("data", 1, 4, f); dp_w32(f, npcm * 2); fwrite(pcm, 2, npcm, f); fclose(f);
}

typedef struct { uint8_t note[4], n, time, flags, vel, prob; } stp_t;
typedef struct {
    char name[16];
    int eng, cat, bpm, hold, tail, plain, base, qlen, qdiv, qswg, qgate, nb, npairs, key[4], nkey;
    int pid[160], pv[160];
    uint8_t blk[160];
    stp_t st[16];
} snd_t;

static int dp_spec(const char *path, snd_t *S, int max)
{
    FILE *f = fopen(path, "r");
    char tag[8];
    int n = -1;
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    while (fscanf(f, "%7s", tag) == 1) {
        snd_t *s = n >= 0 ? &S[n] : 0;
        if (!strcmp(tag, "S")) {                       /* S eng cat bpm hold tail name */
            if (++n >= max) exit(3);
            s = &S[n];
            memset(s, 0, sizeof *s);
            if (fscanf(f, "%d %d %d %d %d %15[^\n]", &s->eng, &s->cat, &s->bpm, &s->hold, &s->tail, s->name) != 6) exit(3);
        } else if (!strcmp(tag, "P")) {
            if (fscanf(f, "%d %d", &s->pid[s->npairs], &s->pv[s->npairs]) != 2) exit(3);
            s->npairs++;
        } else if (!strcmp(tag, "E")) {                /* E nbytes hex.. */
            int i, v;
            if (fscanf(f, "%d", &s->nb) != 1) exit(3);
            for (i = 0; i < s->nb; i++) { if (fscanf(f, "%2x", &v) != 1) exit(3); s->blk[i] = (uint8_t)v; }
        } else if (!strcmp(tag, "R")) {                /* R base bpm hold tail name: a pattern slot played on the stored voice `base` (not stored) */
            if (++n >= max) exit(3);
            s = &S[n];
            memset(s, 0, sizeof *s);
            s->plain = 1;
            if (fscanf(f, "%d %d %d %d %15[^\n]", &s->base, &s->bpm, &s->hold, &s->tail, s->name) != 5) exit(3);
        } else if (!strcmp(tag, "Y")) {                /* Y len div swing gate */
            if (fscanf(f, "%d %d %d %d", &s->qlen, &s->qdiv, &s->qswg, &s->qgate) != 4) exit(3);
        } else if (!strcmp(tag, "K")) {                /* K n k0 k1 ..: keys held while rendering */
            int i;
            if (fscanf(f, "%d", &s->nkey) != 1) exit(3);
            for (i = 0; i < s->nkey; i++) if (fscanf(f, "%d", &s->key[i]) != 1) exit(3);
        } else if (!strcmp(tag, "T")) {                /* T idx time flags vel prob n n0..n3 */
            int i, t, fl, v, pr, nn, a[4];
            if (fscanf(f, "%d %d %d %d %d %d %d %d %d %d", &i, &t, &fl, &v, &pr, &nn, &a[0], &a[1], &a[2], &a[3]) != 10) exit(3);
            s->st[i] = (stp_t){{(uint8_t)a[0], (uint8_t)a[1], (uint8_t)a[2], (uint8_t)a[3]}, (uint8_t)nn, (uint8_t)t, (uint8_t)fl, (uint8_t)v, (uint8_t)pr};
        } else exit(3);
    }
    fclose(f);
    return n + 1;
}

static void dp_setup(const snd_t *s)
{
    track_t *t = &trk[0];
    uint32_t i, e = (uint32_t)s->eng;
    t->eng_req = (uint8_t)e;
    for (i = 0; i < P_E0; i++) t->p[i] = TP[i].def;
    ext_defaults(t);
    for (i = 0; i < 8u; i++) t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
    for (i = 0; i < (uint32_t)s->npairs; i++) t->p[s->pid[i]] = (int16_t)s->pv[i];
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
}
/* a pattern slot played by the plain sequencer on part 1 (what loading the slot into the part does): LEN DIV SWING GATE and the steps */
static void dp_pattern(const snd_t *s)
{
    track_t *t = &trk[0];
    uint32_t i, k;
    t->p[P_SLEN] = (int16_t)s->qlen;
    t->p[P_SDIV] = (int16_t)s->qdiv;
    t->p[P_SSWING] = (int16_t)s->qswg;
    t->p[P_SGATE] = (int16_t)s->qgate;
    t->p[P_SQNT] = 1;
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    for (i = 0; i < 16u; i++) {
        step_t *d = &t->step[i];
        const stp_t *q = &s->st[i];
        d->time = q->time == 0u && !q->n ? ST_REST : q->time;
        d->n = q->n; d->flags = q->flags; d->vel = q->vel; d->probability = q->prob;
        for (k = 0; k < 4u; k++) d->note[k] = q->note[k];
    }
}
static void hold_keys(const snd_t *s, int on)
{
    int i;
    for (i = 0; i < s->nkey; i++) {
        if (on) input_on(&trk[0], (uint32_t)s->key[i], 100);
        else input_off(&trk[0], (uint32_t)s->key[i]);
    }
}
/* what = 0 End A, 1 End B, 2 the morph A -> B over the held part */
static void dp_render(const snd_t *s, uint32_t slot, int what, const char *dir, int nn)
{
    static const char *W[] = {"A", "B", "morph"};
    track_t *t;
    char path[600], nm[40];
    uint32_t i, k = 0;
    dp_fresh();
    t = &trk[0];
    up_load(slot);
    t->engine = t->eng_req;
    eng_mem_claim(t);
    song.g[G_BPM] = (int16_t)s->bpm;
    cappcm = (uint32_t)((s->hold + s->tail) / 1000.0 * FS + 2.0 * FS) * 2u + 4096;
    pcm = realloc(pcm, cappcm * 2u);
    npcm = 0; peak = sumsq = 0;
    dp_blocks(0.2);
    npcm = 0; peak = sumsq = 0;
    mph[0].fader = what == 1 ? 127 : 0;
    if (s->plain) { dp_pattern(s); transport_req = 1; }
    hold_keys(s, 1);
    if (what == 2) {
        uint32_t st;
        dp_blocks(s->hold / 4000.0);
        for (st = 0; st <= 127u; st++) { mph[0].fader = (uint8_t)st; dp_blocks(s->hold * 0.5 / 1000.0 / 128.0); }
        dp_blocks(s->hold / 4000.0);
    } else dp_blocks(s->hold / 1000.0);
    hold_keys(s, 0);
    if (s->plain) transport_req = 0;
    dp_blocks(s->tail / 1000.0);
    for (i = 0; s->name[i] && k < 30u; i++) { char ch = s->name[i]; nm[k++] = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ? ch : '-'; }
    nm[k] = 0;
    snprintf(path, sizeof path, "%s/%02d-%s-%s.wav", dir, nn, nm, W[what]);
    dp_wav(path);
    printf("  %-5s peak %6.1f dB  rms %6.1f dB  %5.1f s  -> %s\n", W[what], 20 * log10(peak / 32768.0 + 1e-9),
           10 * log10(sumsq / (npcm ? npcm : 1) / 32768.0 / 32768.0 + 1e-12), npcm / 2.0 / FS, path);
}

int main(int argc, char **argv)
{
    static snd_t S[32];
    int n, i;
    char path[600];
    FILE *rf;
    if (argc < 3) { fprintf(stderr, "usage: dub_render SPEC OUTDIR\n"); return 2; }
    n = dp_spec(argv[1], S, 32);
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    snprintf(path, sizeof path, "%s/records.txt", argv[2]);
    rf = fopen(path, "w");
    for (i = 0; i < n; i++) {
        snd_t *s = &S[i];
        uint8_t rec[PT_WORST + 8];
        uint32_t len, k;
        int rc;
        dp_fresh();
        song.playing = 0;                                /* (a pattern render left the transport on: a store is refused while it plays) */
        transport_req = 0;
        if (s->plain) {                                  /* a pattern slot: played on the voice it is made for, nothing stored */
            printf("%02d %s (pattern, on voice %d)\n", i + 1, s->name, s->base + 1);
            dp_render(s, (uint32_t)s->base, 0, argv[2], i + 1);
            dp_render(s, (uint32_t)s->base, 1, argv[2], i + 1);
            dp_render(s, (uint32_t)s->base, 2, argv[2], i + 1);
            continue;
        }
        dp_setup(s);
        if (s->nb) { up_bv_in = s->blk; up_bv_n = (uint32_t)s->nb; }
        rc = up_store_trk(&trk[0], (uint32_t)i, s->name, (uint32_t)s->cat);
        up_bv_in = 0; up_bv_n = 0;
        if (rc) { fprintf(stderr, "store %s: rc %d\n", s->name, rc); return 1; }
        len = pt_rd16(pt_raw((uint32_t)i));
        memcpy(rec, pt_raw((uint32_t)i), len);
        rec[2] = rec[3] = 0;
        fprintf(rf, "%d ", i);
        for (k = 0; k < len; k++) fprintf(rf, "%02x", rec[k]);
        fprintf(rf, "\n");
        printf("%02d %s (%u bytes)\n", i + 1, s->name, len);
        dp_render(s, (uint32_t)i, 0, argv[2], i + 1);
        dp_render(s, (uint32_t)i, 1, argv[2], i + 1);
        dp_render(s, (uint32_t)i, 2, argv[2], i + 1);
    }
    fclose(rf);
    return 0;
}
