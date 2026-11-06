/* SPDX-License-Identifier: GPL-3.0-only */
/* GLIDE against the NOTES budget (wave 16, owner: AUTO TIDE, GLIDE in BACK AND FORTH, A AND B = MODE LOOP, "cpu skipping" on the unit): the cost of the audio path while a GLIDE moves the faders,
 * against what the budget charged. Each of the sixteen factory multis is loaded as MODE RECALL ON loads it, TIDE as a STYLE sets it (MODE LOOP, TARGET BOTH, ROLL MUSIC), PLAY with the multi's own
 * sequences and arps, the glide launched as the TIDE page's K1 does (glide_launch). Every audio block is timed (the time stamp counter; ticks a sample, turned into host instructions with the
 * budget's own SAT and EQ rows as tests/tide_cost_probe.c does) and set against the model's figure for the voices sounding then (budget.c: global + both parts' fixed + voices x their cost, as the
 * budget would charge it NOW, from the live values), by the part's fader in 16 bins.
 *   GLIDECPU_V=1      more lines       GLIDECPU_ONLY=n   one slot (1..16)    GLIDECPU_STYLE=n   the TIDE style (0 DUB .. 4 PEAK)   GLIDECPU_BLOCKS=n   blocks a run
 * -O1 -fno-builtin, like the load bench. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static unsigned checks, failures;
#define REQUIRE(msg,expr) do { checks++; if (!(expr)) { failures++; printf("FAIL: %s (%s:%d)\n",msg,__FILE__,__LINE__); } } while(0)

#if defined(__x86_64__) || defined(__i386__)

static double wall_ns(void) { uint32_t lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (double)(((uint64_t)hi << 32) | lo); }
#else
static double wall_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec; }
#endif
static int32_t ob[2 * CTL];
#define FULL 3281.25
#define NBIN 16u

/* ---------------------------------------------------------------- the host scale: ticks a sample -> host instructions a sample (SAT and a three-band EQ, the budget's measured rows) */
static track_t *calscene(unsigned notes)
{
    track_t *t;
    ui_power_on(); frame();
    song.playing = song.rec = 0; transport_req = 0; gl.on = gl.armed = 0; morph_musical = 0;
    host_preset(&trk[0], 0, 0);
    t = &trk[0];
    t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = AM_OFF; t->p[P_SUS] = 127; t->p[P_CHRD] = 0;
    t->p[P_DIST] = t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0; t->p[P_ITYPE] = 0;
    t->p[P_EQLG] = t->p[P_EQMG] = t->p[P_EQHG] = 0;
    trk[1].p[P_MUTE] = 1;
    memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv); memset(fm6_xt, 0, sizeof fm6_xt);
    for (unsigned i = 0; i < notes; i++) trk_note_on(t, 48 + 4 * i, 100);
    return t;
}
static double calns(void)
{
    double best = 1e30;
    for (unsigned k = 0; k < 20; k++) { mix_block(ob, CTL); while (wave_service()) ; }
    for (unsigned r = 0; r < 15; r++) {
        double a = wall_ns();
        for (unsigned i = 0; i < 200; i++) mix_block(ob, CTL);
        a = (wall_ns() - a) / (200.0 * CTL);
        if (a < best) best = a;
    }
    return best;
}
static double host_k(void)                              /* host instructions per tick of one sample */
{
    double n1, sat, eq;
    track_t *t;
    t = calscene(1); n1 = calns();
    t = calscene(1); t->p[P_DIST] = 64; sat = calns() - n1;
    t = calscene(1); t->p[P_EQLG] = 40; t->p[P_EQMG] = 40; t->p[P_EQHG] = 40; eq = calns() - n1;
    return ((double)BD_SAT / sat + (double)(BD_EQ_BASE + 3u * BD_EQ_BAND) / eq) / 2.0;
}

/* ---------------------------------------------------------------- one run */
static uint32_t blk, abl, dump;
static void block_step(void)
{
    fm1_ms = 10u + (uint32_t)((uint64_t)(++blk) * CTL * 1000u / FS);
    events_block(CTL);
    if ((blk % ((uint32_t)FS / CTL / 60u + 1u)) == 0) {
        frame();
        while (wave_service())
            ;
    }
}
static void load(uint32_t slot)
{
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    blk = 0;
    gl.on = gl.armed = 0;
    transport_req = 0;
    song.playing = song.rec = 0;
    mods_off = 0;
    usb.config = 1;
}
typedef struct { float ticks; float model; uint8_t f0, f1, nv0, nv1, to, rb; } rec_t;   /* ticks a sample (this block), the model's charge in % now, the faders, voices sounding, direction */
static uint32_t voices_on(uint32_t p) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += trk[p].v[i].active != 0; return n; }
static uint32_t model_now(void)                         /* 1/100 %: what budget.c charges for the voices sounding now, from the live values */
{
    uint32_t m = global_cost(), p;
    for (p = 0; p < 2u; p++)
        if (!trk[p].p[P_MUTE])
            m += part_fixed(&trk[p]) + voices_on(p) * part_cost(&trk[p]);
    return m;
}
/* ---------------------------------------------------------------- the ends of a morph, one at a time: the voice and the fixed cost of the plain sound against the budget's figures */
/* the plain sound `q` (and FM6's patch fp) on part pi, four notes more than one: host instructions a voice (the cal's way); also the model's figures. In a child process. */
static double vcost_child(uint32_t pi, const int16_t *q, const uint8_t *fp, double k, double *mv)
{
    track_t *t = &trk[pi];
    double c1, c5;
    uint32_t n;
    static const uint8_t NOTES[5] = {48, 52, 55, 60, 64};
    memcpy(t->p, q, EXT_P_LEN * sizeof q[0]);
    if (fp) memcpy(fm6_patch[pi], fp, FP_SIZE + 1u);
    mph[pi].nid = mph[pi].fnid = 0;
    memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv); memset(fm6_xt, 0, sizeof fm6_xt);
    trk[!pi].p[P_MUTE] = 1;
    t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = AM_OFF; t->p[P_SUS] = 127; t->p[P_CHRD] = 0; t->p[P_MUTE] = 0;
    t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
    song.playing = song.rec = 0; gl.on = 0; morph_musical = 0;
    trk_note_on(t, NOTES[0], 100);
    c1 = calns();
    for (n = 1; n < 5u; n++) trk_note_on(t, NOTES[n], 100);
    c5 = calns();
    *mv = (double)part_cost(t) / 3.0476;
    return (c5 - c1) / 4.0 * k;
}
static void ends_cost(uint32_t slot, uint32_t pi, double k, uint32_t ablate)
{
    int16_t q[2][EXT_P_LEN];
    uint8_t fp[2][FP_SIZE + 1u];
    uint32_t e, isfm, id;
    morph_part_t *mp;
    load(slot);
    tide_style_apply(3); gl.mode = GM_LOOP; gl.target = GT_BOTH; gl.time = 4; morph_musical = 1; song.g[G_BPM] = 240;
    for (e = 0; e < 24u; e++) block_step();
    glide_launch(1u, 3u, 0u);
    for (e = 0; e < 400u; e++) { block_step(); mix_block(ob, CTL); while (wave_service()) ; }
    glide_stop();
    mp = &mph[pi];
    if (!(mp->nid || mp->fnid) || morph_stale(&trk[pi], mp)) { printf("glidecpu: M%02u part %u: no morph\n", slot + 1u, pi + 1u); return; }
    isfm = morph_isfm(mp);
    for (e = 0; e < 2u; e++) {
        bd_end(&trk[pi], mp, e, q[e]);
        memcpy(fp[e], fm6_patch[pi], sizeof fp[e]);
        if (isfm) morph_fm6_at(mp, pi, fp[e], e ? 32768 : 0, e);
    }
    for (e = 0; e < 2u + (ablate ? EXT_P_LEN : 0u); e++) {
        pid_t pid;
        int st = 0;
        int16_t w[EXT_P_LEN];
        if (e >= 2u) {
            id = e - 2u;
            if (q[0][id] == q[1][id]) continue;
        }
        fflush(stdout);
        pid = fork();
        if (!pid) {
            double mv, hv;
            memcpy(w, q[e < 2u ? e : 1], sizeof w);
            if (e >= 2u) w[id] = q[0][id];               /* End B with this one value of End A's */
            hv = vcost_child(pi, w, isfm ? fp[e < 2u ? e : 1] : 0, k, &mv);
            if (e < 2u)
                printf("glidecpu: M%02u part %u end %c %-8s  voice: host %5.1f model %5.1f (x%.2f)\n", slot + 1u, pi + 1u, e ? 'B' : 'A', ENGINES[eng_idx(trk[pi].eng_req)]->name, hv, mv, hv / (mv ? mv : 1));
            else
                printf("glidecpu:     B with %s(%u)=%d (A's) -> %5.1f   [B %d]\n", TP[id].label, id, q[0][id], hv, q[1][id]);
            _exit(0);
        }
        if (pid > 0) waitpid(pid, &st, 0);
    }
}
static void ends_diff(void)                              /* (report) the values that differ between the two ends of each part's morph, as they sound */
{
    for (uint32_t pi = 0; pi < 2u; pi++) {
        const morph_part_t *mp = &mph[pi];
        int16_t a[EXT_P_LEN], b[EXT_P_LEN];
        printf("  part %u: engine %s -> ", pi, ENGINES[eng_idx(trk[pi].eng_req)]->name);
        if (!(mp->nid || mp->fnid)) { printf("no morph\n"); continue; }
        bd_end(&trk[pi], mp, 0, a);
        bd_end(&trk[pi], mp, 1, b);
        printf("ends differ in:");
        for (uint32_t id = 0; id < P_COUNT; id++)
            if (a[id] != b[id])
                printf(" %s(%d>%d)", TP[id].label, a[id], b[id]);
        printf("\n");
    }
}
/* mode 0: PLAY (the sequences / arps as they are), no glide; 1: PLAY with the glide; 2: PLAY with no glide, the faders set to the values the glide's run (traj) had at each block (the same
 * blend of the sounds without the glide's own movement: breath, swell, wash, the transition following the fader) */
static float pct(float *v, uint32_t n, float q);
static void run_multi(uint32_t slot, uint32_t style, uint32_t mode, uint32_t nblk, rec_t *out, const rec_t *traj)
{
    uint32_t i;
    load(slot);
    tide_style_apply(style);
    gl.mode = GM_LOOP;
    gl.target = GT_BOTH;
    gl.time = getenv("GLIDECPU_TIME") ? (uint8_t)atoi(getenv("GLIDECPU_TIME")) : 4;   /* 16 bars: the motion at its full depth */
    morph_musical = 1;
    song.g[G_BPM] = 240;
    if (dump && mode == 1u)
        ends_diff();
    for (i = 0; i < 24u; i++)
        block_step();
    if (mode == 1u)
        glide_launch(1u, 3u, 0u);
    else
        transport_req = 1u;
    for (i = 0; i < 300u; i++) {                         /* (PLAY starts; the first notes settle) */
        block_step();
        mix_block(ob, CTL);
        while (wave_service())
            ;
    }
    for (i = 0; i < nblk; i++) {
        double a;
        block_step();
        if (mode == 2u && traj) {
            morph_do(MQ_FADER, 1u, traj[i].f0 + (getenv("GLIDECPU_ALT") ? (i & 1u) * 3u : 0u));
            morph_do(MQ_FADER, 2u, traj[i].f1 + (getenv("GLIDECPU_ALT") ? (i & 1u) * 3u : 0u));
        }
        if (mode == 1u && abl) {                         /* ablations (report only): which stage the extra cost is */
            for (uint32_t q = 0; q < 2u; q++) {
                if (abl & 2u) mxs[q].motion = 0;
                if (abl & 4u) mxs[q].need = 0;
                if (abl & 8u) mxs[q].d = 0, mxs[q].df = 0, mxs[q].dins = 0, mxs[q].bv = 0;
                if (abl & 32u) mxs[q].dub = 0;
            }
            if (abl & 1u) morph_wlvl = 0;
            if (abl & 64u) mph[0].cvalid = mph[1].cvalid = 0;
        }
        if (dump && mode == 1u && i % 900u == 0) {
            printf("  blk %5u f %3u %3u to %u voices %u %u |", i, morph_fader_of(0), morph_fader_of(1), gl.to, voices_on(0), voices_on(1));
            for (uint32_t q = 0; q < 2u; q++)
                printf(" p%u eng %u need %u d %5u df %5u dins %5u mot %5u snd %u fm6 %u |", q, eng_idx(trk[q].eng_req), mxs[q].need, mxs[q].d, mxs[q].df, mxs[q].dins, mxs[q].motion, mxs[q].snd, fm6_xt[q].on);
            printf(" wl %u\n", morph_wlvl);
        }
        { uint32_t rbf = 0; for (uint32_t q = 0; q < 2u; q++) rbf |= (!mph[q].cvalid || mph[q].cfader != mph[q].fader) ? 1u << q : 0u; out[i].rb = (uint8_t)rbf; }
        { uint8_t wl = morph_wlvl; if (mode == 1u && (abl & 16u)) morph_wlvl = 0;   /* (the wash alone: the movement itself stays) */
        a = wall_ns();
        mix_block(ob, CTL);
        morph_wlvl = wl; }
        a = (wall_ns() - a) / CTL;
        while (wave_service())
            ;
        out[i].ticks = (float)a;
        out[i].model = (float)model_now() / 100.0f;
        out[i].f0 = (uint8_t)morph_fader_of(0);
        out[i].f1 = (uint8_t)morph_fader_of(1);
        out[i].nv0 = (uint8_t)voices_on(0);
        out[i].nv1 = (uint8_t)voices_on(1);
        out[i].to = gl.to;
        if (dump && mode == 1u && i > 40u) {
            static float ma = 0;
            if (!ma) ma = (float)a;
            if (a > 1.5f * ma)
                printf("  SPIKE blk %u ticks %.1f (avg %.1f) f %u nv %u %u fm6on %u alg %u need %u d %u tp %u\n", i, a, ma, morph_fader_of(0), voices_on(0), voices_on(1), fm6_xt[0].on, fm6_xt[0].alg, mxs[0].need, mxs[0].d, mxs[0].tp);
            ma += ((float)a - ma) * 0.02f;
        }
    }
}
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
static float pct(float *v, uint32_t n, float q) { if (!n) return 0; qsort(v, n, sizeof *v, cmpf); return v[(uint32_t)((n - 1u) * q)]; }
#define NWIN 21u
static void window(const rec_t *r, uint32_t nblk, uint32_t w, double k, float *med, float *p95, float *nv, float *fd, float *mod)
{
    static float buf[20000];
    uint32_t lo = w * (nblk / NWIN), hi = lo + nblk / NWIN, i, n = 0;
    float vs = 0, fs = 0, ms = 0;
    for (i = lo; i < hi && n < 20000u; i++, n++) {
        buf[n] = r[i].ticks * (float)k / (float)FULL * 100.0f;
        vs += r[i].nv0 + r[i].nv1;
        fs += r[i].f0 > r[i].f1 ? r[i].f0 : r[i].f1;
        ms += r[i].model;
    }
    *nv = n ? vs / (float)n : 0; *fd = n ? fs / (float)n : 0; *mod = n ? ms / (float)n : 0;
    *med = pct(buf, n, 0.5f);
    *p95 = pct(buf, n, 0.95f);
}

static int report_main(void)
{
    uint32_t nblk = 12600, only = 0, style = 3, slot, verbose = 0;
    double k;
    const char *e;
    static const char *const SN[5] = {"DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK"};
    rec_t *rec[3];
    setvbuf(stdout, 0, _IONBF, 0);
    if ((e = getenv("GLIDECPU_ONLY"))) only = (uint32_t)atoi(e);
    if ((e = getenv("GLIDECPU_STYLE"))) style = (uint32_t)atoi(e) % 5u;
    if ((e = getenv("GLIDECPU_BLOCKS"))) nblk = (uint32_t)atoi(e);
    if ((e = getenv("GLIDECPU_V"))) verbose = (uint32_t)atoi(e);
    if ((e = getenv("GLIDECPU_ABL"))) abl = (uint32_t)atoi(e);
    dump = verbose >= 2u;
    memset(nor, 0xFF, sizeof nor);
    k = host_k();
    printf("glidecpu: host scale %.2f instructions per tick of a sample; STYLE %s (MODE LOOP, TARGET BOTH), %u blocks a run, BPM 240, ABL %u\n", k, SN[style], nblk, abl);
    if (getenv("GLIDECPU_ENDS")) { for (slot = 0; slot < 16u; slot++) if (!only || only == slot + 1u) for (uint32_t pi = 0; pi < 2u; pi++) ends_cost(slot, pi, k, getenv("GLIDECPU_ABLATE") != 0); return 0; }
    for (uint32_t m = 0; m < 3u; m++)
        rec[m] = mmap(0, sizeof(rec_t) * nblk, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    for (slot = 0; slot < 16u; slot++) {
        uint32_t pass, w;
        if ((e = getenv("GLIDECPU_SINGLE"))) { uint32_t md = (uint32_t)atoi(e); if ((e = getenv("GLIDECPU_PARK"))) for (uint32_t q = 0; q < nblk; q++) rec[1][q].f0 = rec[1][q].f1 = (uint8_t)atoi(e); run_multi(only - 1u, style, md, nblk, rec[md], rec[1]); return 0; }
        if (only && only != slot + 1u)
            continue;
        for (pass = 1; pass < 4u; pass++) {              /* glide (1), no glide (0), the glide's faders replayed (2): each from the boot state in a child */
            pid_t pid;
            int st = 0;
            uint32_t mode = pass == 1u ? 1u : pass == 2u ? 0u : 2u;
            fflush(stdout);
            pid = fork();
            if (!pid) { run_multi(slot, style, mode, nblk, rec[mode], rec[1]); _exit(0); }
            if (pid > 0) waitpid(pid, &st, 0);
            REQUIRE("a run finished", pid > 0 && !st);
        }
        printf("glidecpu: M%02u windows of %u blocks: fader, voices, then host %% of a half: glide | fader replayed | no glide | glide - replay | replay - no glide  [model %%]\n", slot + 1u, nblk / NWIN);
        for (w = 0; w < NWIN; w++) {
            float m[3], p[3], nv[3], fd[3], md[3];
            for (uint32_t q = 0; q < 3u; q++)
                window(rec[q], nblk, w, k, &m[q], &p[q], &nv[q], &fd[q], &md[q]);
            printf("glidecpu:   f %3.0f nv %3.1f | %5.1f %5.1f %5.1f | %+5.1f %+5.1f | p95 %5.1f %5.1f %5.1f | model %5.1f\n", fd[1], nv[1], m[1], m[2], m[0], m[1] - m[2], m[2] - m[0], p[1], p[2], p[0], md[1]);
        }
        {
            static float tb[20000];
            for (uint32_t q = 0; q < 3u; q++) {
                uint32_t n = 0, c16 = 0, c25 = 0;
                float med;
                for (uint32_t i = 300; i < nblk; i++) tb[n++] = rec[q][i].ticks * (float)k / (float)FULL * 100.0f;
                med = pct(tb, n, 0.5f);
                for (uint32_t i = 0; i < n; i++) { c16 += tb[i] > 1.6f * med; c25 += tb[i] > 2.5f * med; }
                printf("glidecpu:   %s: median %.1f p99 %.1f p99.9 %.1f, blocks over 1.6 x median %.2f %%, over 2.5 x %.2f %%\n", q == 1u ? "glide" : q == 2u ? "replay" : "none", med, pct(tb, n, 0.99f), pct(tb, n, 0.999f), 100.0 * c16 / n, 100.0 * c25 / n);
            }
        }
        {
            double s0 = 0, s1 = 0; uint32_t n0 = 0, n1 = 0;
            for (uint32_t i = 300; i < nblk; i++) { if (rec[1][i].rb) { s1 += rec[1][i].ticks; n1++; } else { s0 += rec[1][i].ticks; n0++; } }
            printf("glidecpu:   glide run: %u blocks make the morph blend again (%.1f %% of the blocks), they cost %.1f %% of a half against %.1f %% for the others\n", n1, 100.0 * n1 / (n0 + n1 + 1), n1 ? s1 / n1 * k / FULL * 100.0 : 0.0, n0 ? s0 / n0 * k / FULL * 100.0 : 0.0);
        }
    }
    (void)verbose;
    printf("glidecpu: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}

/* ---------------------------------------------------------------- the checks (host_tests.sh glidecpu) */
/* the wash as it was before wave 16 (64 bit multiplies), the reference of the 32 bit one */
static void ref_wash(uint32_t pp, int32_t *b, uint32_t n)
{
    mxv_t *v = &mxv[pp];
    const mxs_t *x = &mxs[pp];
    int64_t p = 0, want = 0, pn = 0;
    int32_t g1 = 0, dg, wl, wh, al, ah, i;
    int move = x->snd && (x->motion || (x->need && x->tp > XW0 && x->tp < XW1));
    int32_t nz[64];
    if (n > 64u || (!morph_wlvl && !v->wg))
        return;
    for (i = 0; i < (int32_t)n; i++)
        p += (int64_t)b[i] * b[i] >> 8;
    v->wnow += (p - v->wnow) >> 4;
    if (x->snd && !move)
        v->wref += (p - v->wref) >> 7;
    if (!move && !v->wg)
        return;
    if (!v->wr)
        v->wr = 0x9E3779B9u ^ pp;
    wl = xexp2(x->lfloor + XV_LW), wh = xexp2(x->lfloor + 2 * 4096 + XV_LW);
    al = (int32_t)(((int64_t)wl << 15) / (65536 + wl)), ah = (int32_t)(((int64_t)wh << 15) / (65536 + wh));
    for (i = 0; i < (int32_t)n; i++) {
        int32_t w;
        xs32(&v->wr);
        w = (int32_t)(v->wr >> 16) - 32768;
        v->wy[0] += (int32_t)(((int64_t)ah * (w - v->wy[0])) >> 15);
        v->wy[2] += (int32_t)(((int64_t)ah * (v->wy[0] - v->wy[2])) >> 15);
        v->wy[1] += (int32_t)(((int64_t)al * (v->wy[2] - v->wy[1])) >> 15);
        nz[i] = v->wy[2] - v->wy[1];
        pn += (int64_t)nz[i] * nz[i] >> 8;
    }
    if (move && morph_wlvl) {
        int64_t fill = (v->wref >> XWS[morph_wlvl & 3u]) - v->wnow;
        want = fill > 0 ? fill : 0;
        if (x->need)
            want += ((v->wref >> (XWS[morph_wlvl & 3u] + 3u)) * morph_xdepth(x->tp)) >> 15;
        want = want > (v->wref >> XWS[morph_wlvl & 3u]) ? v->wref >> XWS[morph_wlvl & 3u] : want;
    }
    if (want > 0 && pn > 0) {
        int64_t r = (want << 10) / pn;
        r = r > ((int64_t)256 << 10) - 1 ? ((int64_t)256 << 10) - 1 : r;
        g1 = (int32_t)isqrt32((uint32_t)(r << 14));
    }
    if (g1 != v->wg)
        g1 = v->wg + (g1 - v->wg) / 64 + (g1 > v->wg ? 1 : -1);
    dg = (g1 - v->wg) / (int32_t)n;
    for (i = 0; i < (int32_t)n; i++) {
        v->wg += dg;
        b[i] += (int32_t)(((int64_t)nz[i] * v->wg) >> 12);
    }
    v->wg = g1;
}
static uint32_t rs = 12345;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static void check_wash(void)
{
    static const uint16_t floors[] = {0, 28290, 35968, 44916, 40000};
    unsigned bad = 0, blocks = 0, active = 0, floor_changes = 0, fading = 0, restarts = 0, lvl, run;
    for (lvl = 1; lvl <= 3u; lvl++)
        for (run = 0; run < 6u; run++) {
            unsigned i;
            int32_t a[32], b[32];
            memset(mxv, 0, sizeof mxv); memset(mxs, 0, sizeof mxs);
            mxv[0].wr = mxv[1].wr = 0x1234567u + run;
            morph_wlvl = (uint8_t)lvl;
            for (i = 0; i < 6000u; i++) {
                unsigned j, amp = (run & 1u) ? 40000u : 3000000u;
                uint16_t floor = floors[(i / 83u + run) % (sizeof floors / sizeof floors[0])];
                floor_changes += floor != mxs[0].lfloor && mxv[0].wg != 0;
                mxs[0].lfloor = mxs[1].lfloor = floor;
                morph_wlvl = (uint8_t)((i / 233u) % 4u == 3u ? 0u : lvl);
                fading += !morph_wlvl && mxv[0].wg != 0;
                if (i && i % 997u == 0u) {
                    morph_vrest(0);
                    morph_vrest(1);
                    restarts++;
                }
                if ((i / 400u) % 3u) amp /= 12u;                  /* (the sound falls while the part moves: the wash has a gap to fill) */
                mxs[0].snd = mxs[1].snd = (uint8_t)((i / 700u) & 1u ? 0 : 1);
                mxs[0].motion = mxs[1].motion = (uint16_t)((i / 400u) % 3u ? (i * 37u) & 32767u : 0u);
                mxs[0].need = mxs[1].need = (uint8_t)(run >= 4u ? 1 : 0);
                mxs[0].tp = mxs[1].tp = (uint16_t)(1700u + (i * 5u) % 28000u);
                for (j = 0; j < 32u; j++) a[j] = b[j] = (int32_t)(rnd() % (2u * amp)) - (int32_t)amp;
                morph_wash(0, a, 32u);
                ref_wash(1, b, 32u);
                blocks++;
                if (memcmp(a, b, sizeof a) || mxv[0].wg != mxv[1].wg || mxv[0].wnow != mxv[1].wnow || mxv[0].wref != mxv[1].wref || mxv[0].wr != mxv[1].wr || memcmp(mxv[0].wy, mxv[1].wy, sizeof mxv[0].wy))
                    bad++;
                active += mxv[0].wg != 0;
            }
        }
    morph_wlvl = 0;
    printf("glidecpu: optimized wash against the legacy reference: %u blocks (%u active, %u sounding floor changes, %u fading, %u restarts), %u differ\n",
           blocks, active, floor_changes, fading, restarts, bad);
    REQUIRE("optimized wash preserves legacy state and samples across floor changes, fading and restarts",
            bad == 0 && active > 1000u && floor_changes > 100u && fading > 100u && restarts > 0u);
}
int main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    memset(nor, 0xFF, sizeof nor);
    if (getenv("GLIDECPU_REPORT") || getenv("GLIDECPU_ENDS") || getenv("GLIDECPU_ONLY"))
        return report_main();
    check_wash();
    printf("glidecpu: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
