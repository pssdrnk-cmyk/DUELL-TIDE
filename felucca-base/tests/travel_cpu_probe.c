/* SPDX-License-Identifier: GPL-3.0-only */
/* b14 (owner 2026-11-16: "the cpu is skipping hard on the house chord tide. m07 -> travel 32 bars"): AUTO TIDE TRAVEL (GM_JOURNEY) or EVOLVE (GM_WANDER) on a factory multi, as
 * the owner plays it: the multi loaded as MODE RECALL loads it, MODE and TIME set, K1 launched (glide_launch to B, both parts), the multi's own BPM, sequences and arps. Each audio
 * block is rendered by a copy of fx.c mix_block (the one-core path) with a timer around each part, so the two-core picture can be built: core 0 = the globals + part 1, core 1 =
 * the globals + part 2 (budget.c budget_solve2's two-core rule charges the globals on both). Per block: the model's charge per core for the voices sounding (global_cost +
 * part_fixed + voices x part_cost, as tests/glide_cpu_test.c model_now), the voices against the part's number (bnotes), the host cost (ticks x the SAT / EQ scale = host
 * instructions, % of a half: 3281.25 = 100). Per trip (arrival to arrival): the sounds each part travels to, and the worst of each.
 *   build/host/travel_cpu_probe [SLOT1 [MODE [TIME [TRIPS [BPM]]]]]   SLOT1 1..26 the list number (0: all), MODE 4 TRAVEL / 3 EVOLVE, TIME the GLIDE_BARS index (5 = 32 bars),
 *   TRIPS arrivals to run (6), BPM 0 = the multi's own.  TRAVELCPU_V=1 per-trip voice detail.
 * Report only (host_tests.sh travelcpu, not in the default list: it times code). -O1 -fno-builtin, like the load bench. */
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

#if defined(__x86_64__) || defined(__i386__)
static double wall_ns(void) { uint32_t lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (double)(((uint64_t)hi << 32) | lo); }
#else
static double wall_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec; }
#endif
static int32_t ob[2 * CTL];
#define FULL 3281.25

/* ---------------------------------------------------------------- the host scale (as tests/glide_cpu_test.c host_k) */
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
static double host_k(void)
{
    double n1, sat, eq;
    track_t *t;
    t = calscene(1); n1 = calns();
    t = calscene(1); t->p[P_DIST] = 64; sat = calns() - n1;
    t = calscene(1); t->p[P_EQLG] = 40; t->p[P_EQMG] = 40; t->p[P_EQHG] = 40; eq = calns() - n1;
    return ((double)BD_SAT / sat + (double)(BD_EQ_BASE + 3u * BD_EQ_BAND) / eq) / 2.0;
}

/* ---------------------------------------------------------------- fx.c mix_block, the one-core path, with a timer around each part: t[0] the globals, t[1] part 1, t[2] part 2 (ticks) */
static void probe_mix_block(int32_t *out, uint32_t n, double *t)
{
    int perf;
    blockstate_t bs;
    int32_t mg = MIX_MG();
    double a = wall_ns(), b, c, d;
    mix_events(n);
    sp_tap = snd_st != SND_OFF && (perf_space + mod_space_off() > 0 || sv.spc > 0);
    if (perf_idle()) { perf_tick(n); perf = 0; } else perf = perf_begin(n);
    blk_snap(&bs, sp_tap);
    wv_blk++;
    bs.wv_blk = wv_blk;
    bs.wv_on = (uint32_t)((int32_t)(wv_want - wv_blk) > 0);
    b = wall_ns();
    for (uint32_t i = 0; i < NLIVE; i++) {
        double m0 = wall_ns();
        int mo = morph_begin(&trk[i], 1);
        t[3 + i] = wall_ns() - m0;
        mix_part(&trk[i], n, &bs);
        if (mo) morph_end(&trk[i]);
        if (!i) c = wall_ns();
    }
    d = wall_ns();
    mix_gather(n, &bs);
    mix_sends(n);
    if (meter_on) { meter_on--; if (dm_split) meter_scan(n); }
    mix_master(out, n, perf, mg);
    t[0] = (b - a) + (wall_ns() - d);
    t[1] = c - b;
    t[2] = d - c;
}

/* ---------------------------------------------------------------- a run */
static uint32_t blk;
static void block_step(void)
{
    fm1_ms = 10u + (uint32_t)((uint64_t)(++blk) * CTL * 1000u / FS);
    events_block(CTL);
    if ((blk % ((uint32_t)FS / CTL / 60u + 1u)) == 0) {
        frame();
        while (wave_service())
            ;
        budget_poll();                                  /* (main.c's loop, every frame: the NOTES) */
        {                                               /* .. the unit runs the two-core rule (budget_poll: bd_two_now), the host has no core 1 */
            bd_t b;
            uint32_t n2[2];
            bd_build(&b);
            budget_solve2(&b, n2, 1);
            bnotes[0] = (uint8_t)n2[0], bnotes[1] = (uint8_t)n2[1];
        }
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
static uint32_t voices_on(uint32_t p) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += trk[p].v[i].active != 0; return n; }
static uint32_t voices_rel(uint32_t p) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += trk[p].v[i].active && trk[p].v[i].stage == 4u; return n; }
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }

typedef struct {                                        /* a trip (arrival to arrival) */
    int32_t to[2];                                      /* the sound each part travels to (morph_to_of at the end gone to; -1 the Original / a roll) */
    char nm[2][24];
    uint32_t nblk, over72[2], over90[2], vmax[2], capmin[2], capmax[2], above[2], fmx[2];
    float mmax[2], hp99[2], hmax[2], hmed[2], xmed[2], xp99[2], rmed[2], mx[2];
    uint32_t nx[2], rxt[2], nr[2];
    double mbr[2], mbx[2];
} trip_t;
#define MAXTRIP 16u
static float hb[2][200000], hx[2][200000], hr[2][200000];
static uint32_t nxr[2][2];
static double kk;
static uint32_t verbose;

/* the worst model charge per core over the whole run, for the summary */
static float run_trips(uint32_t slot, uint32_t mode, uint32_t tidx, uint32_t ntrip, uint32_t bpm, trip_t *tr, uint32_t *nt)
{
    uint32_t i, k = 0, lastto, n = 0, lim;
    float worst = 0;
    load(slot);
    gl.mode = (uint8_t)mode;
    gl.time = (uint8_t)tidx;
    if (bpm) song.g[G_BPM] = (int16_t)bpm;
    for (i = 0; i < 24u; i++) block_step();
    glide_launch(1u, 3u, 0u);
    for (i = 0; i < 300u; i++) { block_step(); mix_block(ob, CTL); while (wave_service()) ; }
    lastto = gl.to;
    memset(tr, 0, sizeof *tr * MAXTRIP);
    lim = (uint32_t)((uint64_t)GLIDE_BARS[tidx % GLIDE_TIMES] * 4u * 60u * FS / CTL / (uint32_t)(song.g[G_BPM] ? song.g[G_BPM] : 120)) * (ntrip + 1u) + 20000u;
    for (i = 0; i < lim && k < ntrip; i++) {
        double t[5];
        float h[2], m[2];
        trip_t *q = &tr[k];
        block_step();
        probe_mix_block(ob, CTL, t);
        while (wave_service())
            ;
        if (!q->nblk) {
            for (uint32_t p = 0; p < 2u; p++) {
                q->to[p] = mph[p].n[0] && !morph_stale(&trk[p], &mph[p]) ? morph_to_of(p, gl.to & 1u) : -1;
                tide_end_name(p, gl.to & 1u, q->nm[p]);
                q->capmin[p] = 99;
            }
            n = 0;
            nxr[0][0] = nxr[0][1] = nxr[1][0] = nxr[1][1] = 0;
        }
        for (uint32_t p = 0; p < 2u; p++) {
            uint32_t g = global_cost(), nv = voices_on(p), cap = bnotes[p];
            m[p] = trk[p].p[P_MUTE] ? 0 : (float)(g + part_fixed(&trk[p]) + nv * part_cost(&trk[p])) / 100.0f;
            h[p] = (float)((t[0] + t[1 + p]) / CTL * kk / FULL * 100.0);
            if (m[p] > q->mmax[p]) q->mmax[p] = m[p];
            if (m[p] > worst) worst = m[p];
            q->over72[p] += m[p] > (float)BUDGET_LIMIT;
            q->over90[p] += m[p] > 90.0f;
            if (nv > q->vmax[p]) q->vmax[p] = nv;
            if (cap < q->capmin[p]) q->capmin[p] = cap;
            if (cap > q->capmax[p]) q->capmax[p] = cap;
            q->above[p] += nv > cap;
            q->fmx[p] += fm6_xt[p].on != 0;
            if (n < 200000u) hb[p][n] = h[p];
            if (h[p] > q->hmax[p]) q->hmax[p] = h[p];
            {
                uint32_t f = morph_fader_of(p), x = f > 4u && f < 123u;   /* (the crossing: the fader inside its ends) */
                if (x && m[p] > q->mx[p]) q->mx[p] = m[p];
                if (nxr[p][x] < 200000u) (x ? hx : hr)[p][nxr[p][x]++] = h[p];
                if (!x) { q->nr[p]++; q->rxt[p] += fm6_xt[p].on || fm6_xt[p].dirty || fm6_xt[p].drain; q->mbr[p] += t[3 + p] / CTL * kk / FULL * 100.0; }
                else q->mbx[p] += t[3 + p] / CTL * kk / FULL * 100.0;
            }
        }
        n++;
        q->nblk++;
        if (gl.to != lastto || !gl.on) {                /* an arrival: the trip is over */
            for (uint32_t p = 0; p < 2u; p++) {
                uint32_t c = n < 200000u ? n : 200000u;
                qsort(hb[p], c, sizeof(float), cmpf);
                q->hp99[p] = hb[p][(c - 1u) * 99u / 100u];
                q->hmed[p] = hb[p][(c - 1u) / 2u];
                q->nx[p] = nxr[p][1];
                if (nxr[p][1]) { qsort(hx[p], nxr[p][1], sizeof(float), cmpf); q->xmed[p] = hx[p][(nxr[p][1] - 1u) / 2u]; q->xp99[p] = hx[p][(nxr[p][1] - 1u) * 99u / 100u]; }
                if (nxr[p][0]) { qsort(hr[p], nxr[p][0], sizeof(float), cmpf); q->rmed[p] = hr[p][(nxr[p][0] - 1u) / 2u]; }
            }
            lastto = gl.to;
            k++;
            if (!gl.on) break;
        }
    }
    *nt = k;
    return worst;
}


/* TRAVELCPU_ENG=name: every factory sound of that engine on part 1 alone (the selected part's PATCHES load: select_engine, apply_preset), its effects off, 1 then 5 held notes
 * at velocity 100, the cost from 0.1 s to 1.1 s after the note-on as the sound plays it (its own envelopes: FM6 skips an operator that has gone quiet): host instructions a voice
 * a sample, the model's figure beside it (part_cost) */
static double held_ns(uint32_t nb)
{
    double s = 0;
    for (uint32_t i = 0; i < nb; i++) {
        double a = wall_ns();
        mix_block(ob, CTL);
        s += wall_ns() - a;
        while (wave_service()) ;
    }
    return s / ((double)nb * CTL);
}
static void eng_cost(const char *nm)
{
    uint32_t e, k, b0 = (uint32_t)(0.1 * FS / CTL), nb = (uint32_t)(1.0 * FS / CTL);
    static const uint8_t NOTES[5] = {48, 52, 55, 59, 62};
    for (e = 0; e < NENGINES && strcmp(ENGINES[e]->name, nm); e++) ;
    if (e >= NENGINES) { printf("travelcpu: no engine %s\n", nm); return; }
    for (k = 0; k < ds_count(e); k++) {
        double c[2], best[2] = {1e30, 1e30};
        char lb[24];
        for (uint32_t r = 0; r < 3u; r++)
            for (uint32_t w = 0; w < 2u; w++) {
                track_t *t = &trk[0];
                ui_power_on(); frame();
                song.playing = song.rec = 0; transport_req = 0; gl.on = gl.armed = 0;
                song.sel = 0;
                select_engine(e);
                apply_preset(k);
                frame();
                t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = AM_OFF; t->p[P_CHRD] = 0;
                t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
                trk[1].p[P_MUTE] = 1;
                for (uint32_t i = 0; i < 20u; i++) { mix_block(ob, CTL); while (wave_service()) ; }
                for (uint32_t i = 0; i < (w ? 5u : 1u); i++) trk_note_on(t, NOTES[i], 100);
                for (uint32_t i = 0; i < b0; i++) { mix_block(ob, CTL); while (wave_service()) ; }
                c[w] = held_ns(nb);
                if (c[w] < best[w]) best[w] = c[w];
            }
        ds_label(e, k, lb);
        printf("travelcpu: %s %-22s voice: host %6.1f  model %5u  (x%.2f)\n", nm, lb, (best[1] - best[0]) / 4.0 * kk, part_cost(&trk[0]), (best[1] - best[0]) / 4.0 * kk / (double)(part_cost(&trk[0]) ? part_cost(&trk[0]) : 1u));
    }
}
int main(int argc, char **argv)
{
    uint32_t only = argc > 1 ? (uint32_t)atoi(argv[1]) : 7u, mode = argc > 2 ? (uint32_t)atoi(argv[2]) : GM_JOURNEY, tidx = argc > 3 ? (uint32_t)atoi(argv[3]) : 5u;
    uint32_t ntrip = argc > 4 ? (uint32_t)atoi(argv[4]) : 6u, bpm = argc > 5 ? (uint32_t)atoi(argv[5]) : 0u, slot;
    static trip_t tr[MAXTRIP];
    setvbuf(stdout, 0, _IONBF, 0);
    verbose = getenv("TRAVELCPU_V") ? (uint32_t)atoi(getenv("TRAVELCPU_V")) : 0u;
    if (ntrip > MAXTRIP) ntrip = MAXTRIP;
    memset(nor, 0xFF, sizeof nor);
    kk = host_k();
    if (getenv("TRAVELCPU_ENG")) { printf("travelcpu: host scale %.2f\n", kk); eng_cost(getenv("TRAVELCPU_ENG")); return 0; }
    printf("travelcpu: host scale %.2f; MODE %s, TIME %u bars, %u trips, BPM %s; limit %u %%\n", kk, mode == GM_JOURNEY ? "TRAVEL" : "EVOLVE", GLIDE_BARS[tidx % GLIDE_TIMES], ntrip, bpm ? "set" : "the multi's", BUDGET_LIMIT);
    for (slot = 0; slot < MU_SLOTS && slot < 26u; slot++) {
        uint32_t nt = 0;
        float w;
        if (only && only != slot + 1u) continue;
        w = run_trips(slot, mode, tidx, ntrip, bpm, tr, &nt);
        printf("travelcpu: M%02u %-16s BPM %3d  %s %s / %s %s  worst model %5.1f %%\n", slot + 1u, mu_names[slot], song.g[G_BPM], ENGINES[eng_idx(trk[0].eng_req)]->name, "", ENGINES[eng_idx(trk[1].eng_req)]->name, "", w);
        for (uint32_t j = 0; j < nt; j++) {
            const trip_t *q = &tr[j];
            for (uint32_t p = 0; p < 2u; p++)
                printf("travelcpu:   trip %u p%u -> %-20s model max %5.1f over72 %5u over90 %5u | voices max %u cap %u..%u above %5u | fm6x %5u | host rest med %5.1f | cross %5u blk med %5.1f p99 %5.1f model %5.1f | rest: xt %5u of %5u, morph_begin %4.1f, cross %4.1f\n",
                       j + 1u, p + 1u, q->nm[p], q->mmax[p], q->over72[p], q->over90[p], q->vmax[p], q->capmin[p], q->capmax[p], q->above[p], q->fmx[p], q->rmed[p], q->nx[p], q->xmed[p], q->xp99[p], q->mx[p], q->rxt[p], q->nr[p], q->nr[p] ? q->mbr[p] / q->nr[p] : 0.0, q->nx[p] ? q->mbx[p] / q->nx[p] : 0.0);
        }
    }
    return 0;
}
