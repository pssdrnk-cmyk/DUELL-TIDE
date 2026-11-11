/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE roles (design spec work/design/TIDE_ROLES_AND_MULTIS_SPEC.md, task T2, metrics 4.1 - 4.5): the sixteen factory multis, each 64 bars under its own STYLE and under its
 * neighbour's (Appendix B), loaded with project_load, PLAY with no key held (a part with a classic arp gets a C minor chord held: the arp plays nothing else), TIDE as the unit
 * runs it: the style's nine rows, TARGET SND+SEQ, ROLL MUSIC, the glide launched with PLAY (glide_launch). What a part plays is read off its voices (a voice started: its note and
 * the step the part is on), so these are the notes as the unit sounds them (LAYER sends no MIDI out for part 2, so the ring cannot be used). Everything is run with PITCH OFF
 * and PITCH ON, each run from the boot state in a forked child (ui_power_on() leaves voices, arcs and held keys behind). Time: about 10 s.
 *   4.1 lane      every note started lies in the Original's lane: [lowest, highest + 12] of the notes the multi plays on PLAY alone with TIDE idle (8 bars, the same load),
 *                 where a lane that starts at C3 (48) or above may reach down to 48 (spec C1). (b9: no floor under C2: the low end is welcome, 4.1f is gone.)
 *                 'strict' (printed only): notes outside [Original's lowest, highest + 12] without C1's allowance.
 *   4.2 density   onsets a bar, averaged over every 4-bar window (the bars of a DROP's two left out), inside the role's range (Appendix B DENS); a sustained role (PAD, ATMOS, AIR,
 *                 DRONE, PROG) never shows a bar with more onsets than the steps of the Original that started in it.
 *   4.3 low band  (b9: no bound. The 30 - 120 Hz band was held 15 dB under the full band (12 dB, 30 - 60 Hz, for a bass multi); the owner ended that rule on 11 November: low end and
 *                 sub are welcome, the FM-1 only must not play the drums. The band is still measured and printed, on a 10 s render of PLAY and of PLAY with JRNY.)
 *   4.4 loudness  the 16 within 3 dB of their median, K-weighted as `gen_factory_multis loud` measures (same excitation: PLAY where a part has steps and no arp, else C4 E4 G4 held;
 *                 each multi from the boot state, so within about 1 dB of that tool, which runs them one after the other in one process), the peak under -3 dBFS
 *   4.5 motif     every loop of the Original (all its steps visited: a bar for a 16-step pattern, more for 32 steps or a slow one) but a DROP's keeps max(2, 3/5) of its hits in place;
 *                 the mean Jaccard of a loop's onset set against the Original's is 0.55 or more for SKANK, STAB, MOTIF, PULSE, ANSWER
 * The table TR[] is the E1 baseline's (the records of felucca-base/tools/factory_multis.py at integ 8308edb) with the spec's roles where a part fits a card; role 0 is measured and
 * printed, not checked, a LIVE part is not run, and a classic-arp part (ARP) gets its lane from 8 bars of itself. Tasks T8 and T9 replace the rows with the spec's Appendix B
 * as they replace the records (a name that does not match its slot fails). Not in the default list of host_tests.sh while the baseline fails: `sh host_tests.sh tideroles`.
 *   TIDEROLES_V=1        every violation: slot, style, pitch, part, kind, bar, step, note
 *   TIDEROLES_ONLY=n     one slot (1..16)      TIDEROLES_BARS=n   bars a run (64)      TIDEROLES_MUSIC=0 / TIDEROLES_RENDER=0   skip a half      TIDEROLES_DUMP=n   the first n bars of
 *                        every run: onsets, steps visited, steps played, the Original's hits (hex masks) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
#include <math.h>
#include "loudness.h"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tideroles: FAIL %s\n", s); bad++; } } while (0)

/* every run from the boot state, in a forked child (tests/loudness.h does the same): ui_power_on() does not clear all of what a run leaves behind (voices, arcs, held keys), and a
 * render that follows another must not hear it. fn(arg) fills *out; the child hands the n bytes of *out back through a pipe. A child that dies counts as a failed check. */
static void iso(void (*fn)(void *), void *arg, void *out, uint32_t n)
{
    int fd[2], st = 0;
    ssize_t got = 0;
    if (pipe(fd)) {
        fn(arg);                                       /* (no pipe: in this process) */
        return;
    }
    fflush(stdout);
    pid_t pid = fork();
    if (!pid) {
        close(fd[0]);
        fn(arg);
        if (write(fd[1], out, n) != (ssize_t)n)
            _exit(2);
        _exit(0);
    }
    close(fd[1]);
    if (pid > 0)
        got = read(fd[0], out, n);
    close(fd[0]);
    if (pid > 0)
        waitpid(pid, &st, 0);
    if (pid < 0 || got != (ssize_t)n || st) {
        checks++, bad++;
        printf("tideroles: FAIL a run did not finish (exit status %d)\n", st);
        memset(out, 0, n);
    }
}

/* ---------------------------------------------------------------- the table (Appendix B, the E1 baseline's rows) */
enum { R_SKANK = 1, R_PAD, R_ATMOS, R_STAB, R_MOTIF, R_AIR, R_DRONE, R_PULSE, R_BASS, R_ANSWER, R_PROG, R_ARP, R_LIVE, R_N };
static const char *const RN[R_N] = {"-", "SKANK", "PAD", "ATMOS", "STAB", "MOTIF", "AIR", "DRONE", "PULSE", "BASS", "ANSWER", "PROG", "ARP", "LIVE"};
static const char *const SN[5] = {"DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK"};
/* slot (1..16), the multi's style and its neighbour (STY_*), the role of each part, the bass flag (a part may go under C3), the name */
static const struct { uint8_t slot, style, near, role[2], bass; const char *name; } TR[16] = {
    { 1, STY_DUB,     STY_DEEP,    {R_SKANK, R_ATMOS}, 0, "DUB BASIC"},
    { 2, STY_DUB,     STY_DEEP,    {R_SKANK, R_PAD},   0, "DUB CHORD"},     /* (re-voiced: the stab over a held pad, no bass line) */
    { 3, STY_DEEP,    STY_DUB,     {R_PAD,   R_BASS},  1, "HSE JUNO"},        /* (reel, 2026-11-10: the fifteen new factory multis, tools/factory_multis.py; M02 the owner's DUB CHORD as before) */
    { 4, STY_DEEP,    STY_DUB,     {R_STAB,  R_PAD},   0, "HSE RHODES"},
    { 5, STY_DEEP,    STY_PEAK,    {R_MOTIF, R_SKANK}, 0, "HSE ORGAN"},
    { 6, STY_PEAK,    STY_DEEP,    {R_STAB,  R_PAD},   0, "HSE PIANO"},
    { 7, STY_PEAK,    STY_DEEP,    {R_STAB,  R_PAD},   0, "TECH DETROIT"},
    { 8, STY_DEEP,    STY_PEAK,    {R_PROG,  R_MOTIF}, 0, "TECH SOUL"},
    { 9, STY_MINIMAL, STY_PEAK,    {R_STAB,  R_MOTIF}, 0, "MINIMAL 12"},
    {10, STY_MINIMAL, STY_PEAK,    {R_MOTIF, R_DRONE}, 0, "ACID TWO 303"},
    {11, STY_MINIMAL, STY_PEAK,    {R_BASS,  R_MOTIF}, 1, "ACID LINES"},
    {12, STY_MINIMAL, STY_PEAK,    {R_BASS,  R_PROG},  1, "ELEC ROBOT"},
    {13, STY_PEAK,    STY_MINIMAL, {R_MOTIF, R_PROG},  0, "ELEC DEEPSEA"},
    {14, STY_AMBIENT, STY_DEEP,    {R_MOTIF, R_PROG},  0, "ELEC WARPED"},
    {15, STY_DEEP,    STY_MINIMAL, {R_PULSE, R_PROG},  0, "AMB BERLIN"},
    {16, STY_AMBIENT, STY_DEEP,    {R_AIR,   R_PAD},   0, "AMB LOOPS"},
};
/* density bounds per role: onsets a bar, averaged over a 4-bar window, as tenths (5 = 0.5) */
static const struct { uint8_t lo, hi; } DENS[14] = {{0, 0}, {20, 60}, {5, 20}, {0, 10}, {10, 40}, {30, 80}, {5, 20}, {2, 10}, {60, 80}, {20, 50}, {10, 30}, {5, 10}, {10, 80}, {0, 0}};
/* a role whose Original must never gain an onset in any bar */
static int role_still(uint32_t r) { return r == R_PAD || r == R_ATMOS || r == R_AIR || r == R_DRONE || r == R_PROG; }
/* the Jaccard floor of 4.5 applies to */
static int role_motif(uint32_t r) { return r == R_SKANK || r == R_STAB || r == R_MOTIF || r == R_PULSE || r == R_ANSWER; }
/* a part TIDE's SEQ lane edits: steps played by the sequencer (no classic arp, not played live) */
static int role_steps(uint32_t r) { return r && r != R_ARP && r != R_LIVE; }

static int verbose;
static uint32_t blk;                                   /* control blocks since the load */
static void block_step(void)                           /* one control block of the sequencer and its main loop (tide_motif_test.c run()) */
{
    fm1_ms = 10u + (uint32_t)((uint64_t)(++blk) * CTL * 1000u / FS);
    events_block(CTL);
    if ((blk % ((uint32_t)FS / CTL / 60u + 1u)) == 0) {
        frame();
        while (wave_service())
            ;
    }
}
static void load(uint32_t slot)                        /* a fresh power-on, the factory multi `slot` (0..15) as MODE RECALL ON loads it (tests/multi_loudness.c) */
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
static uint32_t chord_mask(const uint8_t *notes, uint32_t n)   /* the keys (fm1_in.notes) whose notes are `notes` on part 1 */
{
    uint32_t m = 0, i, k;
    for (i = 0; i < n; i++)
        for (k = 0; k < 27u; k++)
            if (kb_map(&trk[0], k) == notes[i])
                m |= 1u << k;
    return m;
}
static const uint8_t KEYS_MIN[3] = {60, 63, 67}, KEYS_MAJ[3] = {60, 64, 67};

/* ---------------------------------------------------------------- one run: what the two parts play */
#define NB_MAX 128
typedef struct { uint32_t ons, expct; uint64_t played, visited; } bar_t;   /* a bar: onsets, steps started that are hits of the Original, the steps that sounded / started */
typedef struct {
    int32_t seen_lo, seen_hi;                          /* the notes started, lowest and highest (-1: none) */
    uint32_t nons, nnotes;
    uint32_t lane_bad, strict_bad;                     /* notes outside the lane, under the Original's lowest / over its highest + 12 */
    uint32_t dwin, dbad;                               /* 4-bar windows measured / outside the role's range */
    double dmin, dmax;                                 /* the lowest and highest window average (onsets a bar) */
    uint32_t still_bad;                                /* bars with more onsets than the Original's steps have (sustained roles) */
    uint32_t anch_bad, anch_kept, anch_need;           /* bars under max(2, 3/5); the closest bar */
    double jsum;
    uint32_t jn;
    uint32_t loopn;                                    /* the Original's loop in bars (8 at most): all its steps visited */
    uint32_t bass, bpoly, bnote, pbreak;               /* B1: a bass part (sqm_bassm); its bars with a POLY stretch; its notes the Original's steps do not hold; BREAK bars with a stretch */
} res_t;
typedef struct { int32_t lo, hi; uint32_t loopn; } lane_t;   /* the Original's lane: [lo, hi] (lo > hi: none), and its loop in bars */

static void viol(uint32_t row, uint32_t style, uint32_t pitch, uint32_t p, const char *kind, uint32_t bar, int idx, int32_t note, const char *more)
{
    if (!verbose)
        return;
    printf("tideroles:   M%02u %s %s PITCH %s p%u %s bar %u step ", TR[row].slot, TR[row].name, SN[style], pitch ? "ON" : "OFF", p + 1u, kind, bar);
    if (idx >= 0 && idx < 64)
        printf("%d", idx);
    else
        printf("-");
    if (note >= 0)
        printf(" note %d", note);
    printf("%s\n", more);
}
static uint32_t cur_idx(uint32_t p, int *steps)        /* the step part p is on (99: none / not a step part) */
{
    const track_t *t = &trk[p];
    uint32_t full = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i;
    *steps = 0;
    if (arp_mode(t) == AM_SEQ_ID) {
        *steps = 1;
        if (t->arp_idx == 0xFFFFFFFFu)
            return 99;
        i = tide_arp_idx(t, t->arp_idx, full);
        return i < 64u ? i : 99u;
    }
    if (arp_mode(t))
        return 99;
    *steps = 1;
    return t->seq_idx < 64u ? (uint32_t)t->seq_idx : 99u;
}
static uint32_t popc64(uint64_t x) { uint32_t n = 0; for (; x; x >>= 1) n += (uint32_t)(x & 1u); return n; }

/* tide 0: PLAY alone, TIDE idle (the Original); 1: TIDE as style `style` runs it, PITCH `pitch`. keys: a chord held (a classic arp plays only on one). ref: the lane to hold the
 * notes to (tide 1), roles for the density rules */
static void play(uint32_t row, uint32_t style, uint32_t pitch, uint32_t tide, uint32_t nbars, int keys, const lane_t *ref, res_t *res)
{
    static bar_t hist[2][NB_MAX];
    static uint8_t drop[NB_MAX];
    bar_t cur[2];
    uint64_t orig[2], onote[2][2];
    uint32_t p, i, bars_done = 0, curbar = 0, started = 0, last, guard, lastblk[2] = {0, 0}, seen_on[2] = {0, 0}, lastidx[2] = {99, 99}, bpm;
    uint32_t perbar;
    memset(res, 0, 2u * sizeof *res);
    memset(hist, 0, sizeof hist);
    memset(drop, 0, sizeof drop);
    if (nbars > NB_MAX)
        nbars = NB_MAX;
    load(TR[row].slot - 1u);
    bpm = (uint32_t)song.g[G_BPM];
    for (p = 0; p < 2u; p++) {
        uint32_t len = (uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP);
        res[p].seen_lo = res[p].seen_hi = -1;
        res[p].dmin = 99.0, res[p].dmax = -1.0;
        res[p].anch_kept = res[p].anch_need = 0;
        onote[p][0] = onote[p][1] = 0;
        for (orig[p] = 0, i = 0; i < len; i++)
            if (sqm_hit(&trk[p].step[i])) {
                orig[p] |= 1ull << i;
                for (uint32_t k = 0; k < trk[p].step[i].n && k < 4u; k++)
                    onote[p][trk[p].step[i].note[k] >> 6 & 1u] |= 1ull << (trk[p].step[i].note[k] & 63u);
            }
    }
    if (tide) {
        tide_style_apply(style);
        gl.target = GT_BOTH;
        morph_musical = 1;
        morph.cfg = (uint8_t)(pitch ? morph.cfg | MC_PIT : morph.cfg & ~MC_PIT);
    }
    fm1_in.notes = keys ? chord_mask(KEYS_MIN, 3u) : 0u;
    for (i = 0; i < 24u; i++)                                      /* (the keys down, their direct notes and the arp's first ones, before PLAY) */
        block_step();
    last = vage;
    if (tide)
        glide_launch(1u, 3u, 0u);                                  /* (the TIDE page's K1: PLAY starts with it) */
    else
        transport_req = 1u;
    perbar = 4u * (uint32_t)FS * 60u / (bpm ? bpm : 120u) / CTL;
    guard = (nbars + 3u) * perbar + 4000u;
    memset(cur, 0, sizeof cur);
    while (bars_done < nbars && guard--) {
        block_step();
        if (!song.playing)
            continue;
        if (!started) {
            started = 1;
            curbar = sqm_bar;
            drop[0] = TARC_DROP_BAR((uint32_t)sqm_bar + sqm_bar_turn);
        } else if ((uint32_t)sqm_bar != curbar) {
            for (p = 0; p < 2u; p++) {
                hist[p][bars_done] = cur[p];
                if (getenv("TIDEROLES_DUMP") && bars_done < (uint32_t)atoi(getenv("TIDEROLES_DUMP")))
                    printf("  dump tide %u bar %u p%u: ons %u visited %016llx played %016llx orig %016llx slen %d sdiv %d arate %d\n", tide, bars_done, p + 1u, cur[p].ons, (unsigned long long)cur[p].visited, (unsigned long long)cur[p].played, (unsigned long long)orig[p], (int)trk[p].p[P_SLEN], (int)trk[p].p[P_SDIV], (int)trk[p].p[P_ARATE]);
            }
            if (tide) {
                for (p = 0; p < 2u; p++)
                    if ((sqm_bassm >> p) & 1u)
                        res[p].bass = 1, res[p].bpoly += poly_len[p] != 0u;
                res[0].pbreak += sqm_arc && sqm_auto && tarc.phase == AP_BREAK && (poly_len[0] | poly_len[1]);
            }
            bars_done++;
            if (bars_done >= nbars)
                break;
            curbar = sqm_bar;
            memset(cur, 0, sizeof cur);
            drop[bars_done] = TARC_DROP_BAR((uint32_t)sqm_bar + sqm_bar_turn);
        }
        for (p = 0; p < 2u; p++) {
            int steps;
            uint32_t idx = cur_idx(p, &steps), now = vage;
            if (idx < 64u && idx != lastidx[p]) {                        /* (a step that started in this bar: the one running across the bar line belongs to the bar before) */
                cur[p].visited |= 1ull << idx;
                cur[p].expct += (uint32_t)(orig[p] >> idx & 1u);
            }
            lastidx[p] = idx;
            for (i = 0; i < NVOICE; i++) {
                const voice_t *v = &trk[p].v[i];
                int32_t n;
                if (!v->active || v->age <= last || v->age > now)
                    continue;
                n = v->note;
                res[p].nnotes++;
                if (res[p].seen_lo < 0 || n < res[p].seen_lo)
                    res[p].seen_lo = n;
                if (n > res[p].seen_hi)
                    res[p].seen_hi = n;
                if (!seen_on[p] || blk - lastblk[p] > 4u) {         /* (a chord's notes start up to three blocks apart: one onset) */
                    cur[p].ons++;
                    res[p].nons++;
                    if (idx < 64u)
                        cur[p].played |= 1ull << idx;
                }
                seen_on[p] = 1;
                lastblk[p] = blk;
                if (tide) {
                    if (((sqm_bassm >> p) & 1u) && !arp_mode(&trk[p]) && !(onote[p][n >> 6 & 1] >> (n & 63) & 1u)) {
                        res[p].bnote++;
                        viol(row, style, pitch, p, "bass note not in the Original", bars_done, steps ? (int)idx : -1, n, "");
                    }
                    if (ref && ref[p].lo <= ref[p].hi) {
                        int32_t lo = ref[p].lo >= 48 ? 48 : ref[p].lo, hi = ref[p].hi + 12;
                        if (n < lo || n > hi) {
                            char more[48];
                            snprintf(more, sizeof more, " (lane %d..%d)", (int)lo, (int)hi);
                            res[p].lane_bad++;
                            viol(row, style, pitch, p, "lane", bars_done, steps ? (int)idx : -1, n, more);
                        }
                        if (n < ref[p].lo || n > ref[p].hi + 12)
                            res[p].strict_bad++;
                    }
                }
            }
        }
        last = vage;
    }
    fm1_in.notes = 0;
    transport_req = 2u;
    block_step();
    block_step();
    /* the bars, finished */
    for (p = 0; p < 2u; p++) {
        uint32_t r = TR[row].role[p], w, b, nw = bars_done / 4u;
        res_t *q = &res[p];
        if (!tide || !r)
            continue;
        if (DENS[r].hi)
            for (w = 0; w < nw; w++) {
                uint32_t s = 0, n = 0;
                double avg;
                for (b = 4u * w; b < 4u * w + 4u; b++)
                    if (!drop[b])
                        s += hist[p][b].ons, n++;
                if (!n)
                    continue;
                avg = (double)s / n;
                q->dwin++;
                q->dmin = avg < q->dmin ? avg : q->dmin;
                q->dmax = avg > q->dmax ? avg : q->dmax;
                if (avg * 10.0 + 1e-9 < DENS[r].lo || avg * 10.0 - 1e-9 > DENS[r].hi) {
                    char more[64];
                    q->dbad++;
                    snprintf(more, sizeof more, " bars %u..%u: %.2f onsets a bar (range %.1f..%.1f)", 4u * w, 4u * w + 3u, avg, DENS[r].lo / 10.0, DENS[r].hi / 10.0);
                    viol(row, style, pitch, p, "density", 4u * w, -1, -1, more);
                }
            }
        for (b = 0; b < bars_done; b++)
            if (!drop[b] && role_still(r) && hist[p][b].ons > hist[p][b].expct) {
                char more[64];
                q->still_bad++;
                snprintf(more, sizeof more, " %u onsets, the Original's steps give %u", hist[p][b].ons, hist[p][b].expct);
                viol(row, style, pitch, p, "added onset", b, -1, -1, more);
            }
        if (role_steps(r)) {                                          /* the anchors and the Jaccard: over the Original's loop (a bar for a loop of a bar or less) */
            uint32_t ln = ref && ref[p].loopn ? ref[p].loopn : 1u;
            for (b = 0; b + ln <= bars_done; b += ln) {
                uint64_t vis = 0, pl = 0, exp;
                uint32_t j, pe, kept, need, dr = 0;
                for (j = b; j < b + ln; j++)
                    vis |= hist[p][j].visited, pl |= hist[p][j].played, dr |= drop[j];
                exp = orig[p] & vis;
                pe = popc64(exp), kept = popc64(pl & orig[p]);
                if (dr || !pe)
                    continue;
                need = pe < 2u ? pe : (pe * 3u + 4u) / 5u < 2u ? 2u : (pe * 3u + 4u) / 5u;
                if (kept < need) {
                    char more[64];
                    q->anch_bad++;
                    snprintf(more, sizeof more, " keeps %u of %u hits, needs %u (loop of %u bars)", kept, pe, need, ln);
                    viol(row, style, pitch, p, "anchors", b, -1, -1, more);
                }
                if (!q->anch_need || kept * q->anch_need < q->anch_kept * need)
                    q->anch_kept = kept, q->anch_need = need;
                if (role_motif(r)) {
                    uint32_t un = popc64(pl | exp);
                    if (un) {
                        q->jsum += (double)popc64(pl & exp) / un;
                        q->jn++;
                    }
                }
            }
        }
    }
    if (!tide)                                                         /* the loop of the Original in bars: the first n bars that visit every step */
        for (p = 0; p < 2u; p++) {
            uint64_t vis = 0, full = clamp(trk[p].p[P_SLEN], 1, NSTEP) >= 64 ? ~0ull : (1ull << clamp(trk[p].p[P_SLEN], 1, NSTEP)) - 1u;
            uint32_t b;
            res[p].loopn = 1;
            for (b = 0; b < bars_done && b < 8u; b++) {
                vis |= hist[p][b].visited;
                if ((vis & full) == full)
                    break;
            }
            res[p].loopn = b + 1u > 8u ? 8u : b + 1u;
            if (!vis)
                res[p].loopn = 1;
        }
}

typedef struct { uint32_t row, style, pitch, tide, nbars; int keys, has_ref; lane_t ref[2]; res_t res[2]; } pjob_t;
static void play_job(void *a) { pjob_t *j = a; play(j->row, j->style, j->pitch, j->tide, j->nbars, j->keys, j->has_ref ? j->ref : 0, j->res); }
static void play_iso(uint32_t row, uint32_t style, uint32_t pitch, uint32_t tide, uint32_t nbars, int keys, const lane_t *ref, res_t *res)
{
    static pjob_t j;
    memset(&j, 0, sizeof j);
    j.row = row, j.style = style, j.pitch = pitch, j.tide = tide, j.nbars = nbars, j.keys = keys;
    if (ref)
        j.has_ref = 1, j.ref[0] = ref[0], j.ref[1] = ref[1];
    iso(play_job, &j, j.res, sizeof j.res);
    memcpy(res, j.res, sizeof j.res);
}

/* ---------------------------------------------------------------- the music half: 4.1, 4.2, 4.5 */
static void music(uint32_t row, uint32_t nbars)
{
    static const uint32_t PIT[2] = {0, 1};
    uint32_t si, p, pt;
    lane_t ref[2];
    res_t idle[2], r[2][2];                                         /* [pitch][part] */
    int keys = TR[row].role[0] == R_ARP || TR[row].role[1] == R_ARP;
    play_iso(row, TR[row].style, 0, 0, 8u, keys, 0, idle);              /* the Original on PLAY alone */
    for (p = 0; p < 2u; p++) {
        ref[p].lo = idle[p].seen_lo < 0 ? 1 : idle[p].seen_lo;
        ref[p].hi = idle[p].seen_lo < 0 ? 0 : idle[p].seen_hi;
        ref[p].loopn = idle[p].loopn;
    }
    printf("tideroles: M%02u %-12s Original on PLAY alone%s: p1 %s %d..%d, p2 %s %d..%d (%u and %u onsets in 8 bars; loops of %u and %u bars)\n", TR[row].slot, TR[row].name, keys ? " (chord held)" : "",
           RN[TR[row].role[0]], idle[0].seen_lo, idle[0].seen_hi, RN[TR[row].role[1]], idle[1].seen_lo, idle[1].seen_hi, idle[0].nons, idle[1].nons, idle[0].loopn, idle[1].loopn);
    for (si = 0; si < 2u; si++) {
        uint32_t style = si ? TR[row].near : TR[row].style;
        for (pt = 0; pt < 2u; pt++)
            play_iso(row, style, PIT[pt], 1, nbars, keys, ref, r[pt]);
        for (p = 0; p < 2u; p++) {
            uint32_t role = TR[row].role[p], bound = DENS[role].hi != 0;
            const res_t *a = &r[0][p], *b = &r[1][p];
            char head[96], msg[240];
            snprintf(head, sizeof head, "M%02u %s %s p%u %s", TR[row].slot, TR[row].name, SN[style], p + 1u, RN[role]);
            printf("tideroles: %s: notes %d..%d lane %d..%d | lane out %u/%u strict %u/%u | dens %.2f..%.2f of %u windows (range %.1f..%.1f) out %u/%u | still %u/%u | anchors worst %u of %u, out %u/%u | jacc %.2f/%.2f\n",
                   head, a->seen_lo < b->seen_lo ? a->seen_lo : b->seen_lo, a->seen_hi > b->seen_hi ? a->seen_hi : b->seen_hi,
                   ref[p].lo > ref[p].hi ? 0 : (int)(ref[p].lo >= 48 ? 48 : ref[p].lo), ref[p].lo > ref[p].hi ? 0 : (int)ref[p].hi + 12,
                   a->lane_bad, b->lane_bad, a->strict_bad, b->strict_bad, a->dmin > 90 ? 0.0 : a->dmin, a->dmax < 0 ? 0.0 : a->dmax, a->dwin,
                   DENS[role].lo / 10.0, DENS[role].hi / 10.0, a->dbad, b->dbad, a->still_bad, b->still_bad, a->anch_kept, a->anch_need, a->anch_bad, b->anch_bad,
                   a->jn ? a->jsum / a->jn : 0.0, b->jn ? b->jsum / b->jn : 0.0);
            if (!role || role == R_LIVE)
                continue;
            if (ref[p].lo <= ref[p].hi) {
                snprintf(msg, sizeof msg, "%s: 4.1 lane: %u (PITCH OFF) + %u (PITCH ON) of %u + %u notes outside the Original's lane %d..%d (first below with TIDEROLES_V=1)", head, a->lane_bad,
                         b->lane_bad, a->nnotes, b->nnotes, ref[p].lo >= 48 ? 48 : ref[p].lo, ref[p].hi + 12);
                CHECK(msg, !a->lane_bad && !b->lane_bad);
            }
            if (bound) {
                snprintf(msg, sizeof msg, "%s: 4.2 density: %.2f..%.2f onsets a bar over 4-bar windows (PITCH OFF; ON %.2f..%.2f), range %.1f..%.1f, %u + %u of %u windows out", head,
                         a->dmin > 90 ? 0.0 : a->dmin, a->dmax < 0 ? 0.0 : a->dmax, b->dmin > 90 ? 0.0 : b->dmin, b->dmax < 0 ? 0.0 : b->dmax, DENS[role].lo / 10.0, DENS[role].hi / 10.0, a->dbad,
                         b->dbad, a->dwin);
                CHECK(msg, !a->dbad && !b->dbad);
            }
            if (role_still(role)) {
                snprintf(msg, sizeof msg, "%s: 4.2 sustained role: %u + %u bars with more onsets than the Original's steps", head, a->still_bad, b->still_bad);
                CHECK(msg, !a->still_bad && !b->still_bad);
            }
            if (role_steps(role)) {
                snprintf(msg, sizeof msg, "%s: 4.5 anchors: %u + %u bars keep under max(2, 3/5) of the Original's hits in place (closest: %u of %u)", head, a->anch_bad, b->anch_bad, a->anch_kept, a->anch_need);
                CHECK(msg, !a->anch_bad && !b->anch_bad);
            }
            if (role_motif(role)) {
                double ja = a->jn ? a->jsum / a->jn : 1.0, jb = b->jn ? b->jsum / b->jn : 1.0;
                snprintf(msg, sizeof msg, "%s: 4.5 motif retention: mean Jaccard %.2f (PITCH ON %.2f), under 0.55", head, ja, jb);
                CHECK(msg, ja >= 0.55 && jb >= 0.55);
            }
        }
    }
}

/* B1 (tide-morph audit): under DUB and MINIMAL a bass part keeps its loop length (no POLY) and its notes (no VOIC octave), and no BREAK bar has a stretch */
static void bass_lock(uint32_t row, uint32_t nbars)
{
    static const uint32_t ST[2] = {STY_DUB, STY_MINIMAL};
    int keys = TR[row].role[0] == R_ARP || TR[row].role[1] == R_ARP;
    char msg[240];
    for (uint32_t si = 0; si < 2u; si++) {
        res_t r[2];
        play_iso(row, ST[si], 0, 1, nbars, keys, 0, r);
        printf("tideroles: M%02u %-12s %s B1: bass parts %u%u, POLY bars %u/%u, notes off the Original %u/%u, BREAK bars with POLY %u\n", TR[row].slot, TR[row].name, SN[ST[si]], r[0].bass, r[1].bass,
               r[0].bpoly, r[1].bpoly, r[0].bnote, r[1].bnote, r[0].pbreak);
        for (uint32_t p = 0; p < 2u; p++) {
            if (!r[p].bass)
                continue;
            snprintf(msg, sizeof msg, "M%02u %s %s p%u: B1 the bass part: %u bars with a POLY stretch, %u notes outside the Original's", TR[row].slot, TR[row].name, SN[ST[si]], p + 1u, r[p].bpoly, r[p].bnote);
            CHECK(msg, !r[p].bpoly && !r[p].bnote);
        }
        snprintf(msg, sizeof msg, "M%02u %s %s: B1 %u BREAK bars with a POLY stretch", TR[row].slot, TR[row].name, SN[ST[si]], r[0].pbreak);
        CHECK(msg, !r[0].pbreak);
    }
}

/* ---------------------------------------------------------------- the render half: 4.3 and 4.4 */
typedef struct { double k_db, peak_db, band, band1; uint32_t silent, name_ok; } rend_t;
/* the multi rendered 10 s, 2 - 10 s measured. mode 0: `gen_factory_multis loud`'s excitation (PLAY where a part has steps and no arp, else C4 E4 G4 held); 1: PLAY (the chord held for a
 * row with a classic arp); 2: as 1 with JRNY, TIME 1 bar, WAIT OFF (the multi's own style's other rows) */
static void render(uint32_t row, uint32_t mode, rend_t *r)
{
    static int32_t out[2 * CTL];
    uint32_t b, i, c, nb = (uint32_t)(10.0 * FS) / CTL, b0 = (uint32_t)(2.0 * FS) / CTL, macc = 0, pat = 0, keys = TR[row].role[0] == R_ARP || TR[row].role[1] == R_ARP, kmask = 0;
    lz_kw_t kw;
    lz_band_t bd;
    if (mode == 0) {                                                /* (gen_factory_multis loud() as it is: no flash, no UI frames, the keys down at once) */
        memset(nor, 0xFF, sizeof nor);
        ui_power_on();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        project_load(TR[row].slot - 1u);
    } else {
        load(TR[row].slot - 1u);
    }
    song.master_q12 = 2048;                                         /* (MASTER at its default position, main.c) */
    r->name_ok = !strcmp(mu_names[TR[row].slot - 1u], TR[row].name);   /* (the table out of step with the records: tasks T8 / T9 change them together) */
    for (c = 0; c < 2u; c++)
        for (i = 0; i < NSTEP; i++)
            pat |= trk[c].step[i].time == ST_NOTE && trk[c].step[i].n && trk[c].p[P_AMODE] == 0;
    lz_kw_init(&kw);
    lz_band_init(&bd, 30.0, TR[row].bass ? 60.0 : 120.0);
    for (i = 0; i < 8u; i++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
    }
    if (mode == 0)
        kmask = pat ? 0u : chord_mask(KEYS_MAJ, 3u);
    else if (keys)
        kmask = chord_mask(KEYS_MIN, 3u);
    fm1_in.notes = kmask;
    if (mode == 2) {
        tide_style_apply(TR[row].style);
        gl.mode = GM_JOURNEY, gl.time = 0, gl.wait = 0, gl.target = GT_BOTH;
        morph_musical = 1;
        glide_launch(1u, 3u, 0u);
    } else if (mode || pat) {
        transport_req = 1;
    }
    for (b = 0; b < nb; b++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
        macc += 1000u * CTL;
        if (mode && macc >= (uint32_t)FS * 16u) {
            macc -= (uint32_t)FS * 16u;
            frame();
        }
        if (b < b0)
            continue;
        for (i = 0; i < CTL; i++)
            for (c = 0; c < 2u; c++) {
                lz_kw_push(&kw, c, out[2u * i + c] / 32767.0);
                lz_band_push(&bd, c, (double)((c ? mix_r[i] : mix_l[i]) + wet[i]));
            }
    }
    fm1_in.notes = 0;
    r->k_db = lz_kw_db(&kw);
    r->peak_db = lz_kw_peak_db(&kw);
    r->silent = bd.full < 1.0;
    r->band = lz_band_db(&bd);
    r->band1 = lz_band_db1(&bd);
}
typedef struct { uint32_t row, mode; rend_t r; } rjob_t;
static void render_job(void *a) { rjob_t *j = a; render(j->row, j->mode, &j->r); }
static void render_iso(uint32_t row, uint32_t mode, rend_t *r)
{
    static rjob_t j;
    memset(&j, 0, sizeof j);
    j.row = row, j.mode = mode;
    iso(render_job, &j, &j.r, sizeof j.r);
    *r = j.r;
}
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

int main(void)
{
    uint32_t row, nbars = getenv("TIDEROLES_BARS") ? (uint32_t)atoi(getenv("TIDEROLES_BARS")) : 64u;
    uint32_t only = getenv("TIDEROLES_ONLY") ? (uint32_t)atoi(getenv("TIDEROLES_ONLY")) : 0u, n = 0;
    int do_music = !getenv("TIDEROLES_MUSIC") || getenv("TIDEROLES_MUSIC")[0] != '0', do_render = !getenv("TIDEROLES_RENDER") || getenv("TIDEROLES_RENDER")[0] != '0';
    double kl[16];
    char msg[240];
    verbose = getenv("TIDEROLES_V") != 0;
    if (do_music)
        for (row = 0; row < 16u; row++)
            if (!only || TR[row].slot == only)
                music(row, nbars), bass_lock(row, nbars);
    if (do_render) {
        for (row = 0; row < 16u; row++) {
            rend_t a, p, j;
            if (only && TR[row].slot != only)
                continue;
            render_iso(row, 0, &a);
            render_iso(row, 1, &p);
            render_iso(row, 2, &j);
            kl[n++] = a.k_db;
            snprintf(msg, sizeof msg, "M%02u: the table says %s, the record is another multi (update TR[] with the records)", TR[row].slot, TR[row].name);
            CHECK(msg, a.name_ok);
            printf("tideroles: M%02u %-12s loud %.1f dB K-weighted, peak %.1f dBFS | low band %s%u..%u Hz under the full band: PLAY %.1f dB (one-pole %.1f), after JRNY %.1f dB (one-pole %.1f), no bound\n", TR[row].slot,
                   TR[row].name, a.k_db, a.peak_db, p.silent ? "(silent) " : "", 30u, TR[row].bass ? 60u : 120u, p.band, p.band1, j.band, j.band1);
            snprintf(msg, sizeof msg, "M%02u %s: 4.4 peak %.1f dBFS, not under -3", TR[row].slot, TR[row].name, a.peak_db);
            CHECK(msg, a.peak_db <= -3.0);
        }
        if (n == 16u) {
            double med[16];
            memcpy(med, kl, sizeof med);
            qsort(med, 16, sizeof med[0], cmpd);
            double m = 0.5 * (med[7] + med[8]);
            printf("tideroles: loudness median %.1f dB K-weighted, range %.1f .. %.1f\n", m, med[0], med[15]);
            for (row = 0; row < 16u; row++) {
                snprintf(msg, sizeof msg, "M%02u %s: 4.4 loudness %.1f dB K-weighted, %+.1f dB from the median %.1f (window +-3)", TR[row].slot, TR[row].name, kl[row], kl[row] - m, m);
                CHECK(msg, fabs(kl[row] - m) <= 3.0);
            }
        }
    }
    printf("tideroles: %d of %d checks failed\n", bad, checks);
    return bad != 0;
}
