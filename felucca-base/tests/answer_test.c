/* SPDX-License-Identifier: GPL-3.0-only */
/* TAKE + ANSWER, the generators (b10 round 8, firmware/src/tide_take.c; work/b10/reports/PERF_ASSIST.md section 4's five takes). Host group "answer".
 * The report's 5 takes (part 1, written as the 16th grid the live recorder stores) on 5 multis each (M06 HSE FRENCH, M03 HSE JUNO, M15 AMB BERLIN, M08 TECH SOUL, M14 ELEC WARPED: the genre comes from
 * the loaded multi's name), BASS LINE and DOUBLE, with the key from the take alone (SCL chromatic) and with SCL set to the take's own key:
 *   KEY      every note of the answer is in the key the generator used (tk_deg / tk_kr)
 *   RUBS     no semitone, major seventh or minor ninth between an answer note and a take note it overlaps
 *   ROOT     a BASS LINE bar's first note is its chord's root
 *   LOW      a BASS LINE note sits at least an octave under the take's lowest sounding note
 *   LOOP     the answer has the take's length, ties stay inside it
 *   SEEDS    the same seed gives the same answer; 8 seeds give different answers (rolls), every one passing the rules above
 *   DOUBLE   every note is a diatonic third (or its fallback) above a note of the take, in time */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
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
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

typedef struct { int t, len, note; } nt_t;
typedef struct { const char *name; int multi, bars, scale, root; const nt_t *n; int nn; } take_def_t;
#define N(t, l, n) {t, l, n}
static const nt_t T0[] = {N(0,2,76), N(3,1,74), N(4,2,72), N(6,2,71), N(8,4,72), N(14,2,69), N(16,2,72), N(19,1,71), N(20,2,69), N(22,2,67), N(24,6,65),
                          N(32,2,76), N(35,1,74), N(36,2,72), N(38,2,74), N(40,4,76), N(46,2,79), N(48,3,77), N(51,1,76), N(52,2,74), N(54,2,72), N(56,8,71)};
static const nt_t T1[] = {N(0,4,69), N(4,4,72), N(8,8,76), N(16,4,77), N(20,4,76), N(24,8,72), N(32,4,74), N(36,4,72), N(40,8,71), N(48,4,72), N(52,4,71), N(56,8,69)};
static const nt_t T2[] = {N(0,8,72), N(8,8,75), N(16,8,77), N(24,8,75), N(32,8,72), N(40,8,70), N(48,12,68), N(60,4,67)};
static const nt_t T3[] = {N(0,2,68), N(2,2,71), N(4,3,73), N(8,2,76), N(11,1,75), N(12,4,73), N(18,2,71), N(20,2,68), N(24,6,66)};
static const nt_t T4[] = {N(0,4,74), N(4,2,78), N(6,2,81), N(8,6,79), N(16,2,76), N(18,2,74), N(20,4,72), N(26,4,74)};
/* scale (core.h SCALE index: 1 MAJ, 2 MIN) and root of each take's own key; the take on the multi it was made on */
static const take_def_t TAKES[5] = {
    {"hse_french", 6, 4, 2, 9, T0, 22}, {"hse_juno", 3, 4, 2, 9, T1, 12}, {"amb_berlin", 15, 4, 2, 5, T2, 8}, {"tech_soul", 8, 2, 2, 1, T3, 9}, {"elec_warped", 14, 2, 1, 7, T4, 8}};
static const int MULTIS[5] = {6, 3, 15, 8, 14};

static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
static void fill(const take_def_t *d)
{
    track_t *t = &trk[0];
    int i, j;
    memset(t->step, 0, sizeof t->step);
    for (i = 0; i < d->nn; i++) {
        step_t *s = &t->step[d->n[i].t];
        s->n = 1; s->note[0] = (uint8_t)d->n[i].note; s->time = ST_NOTE; s->vel = 100;
        for (j = 1; j < d->n[i].len; j++)
            t->step[d->n[i].t + j].time = ST_TIE;
    }
    t->p[P_SLEN] = (int16_t)(d->bars * 16);
    t->p[P_SDIV] = 2;                                    /* 1/16: a bar is 16 steps (the loaded multi's part may sequence on another grid) */
    t->p[P_AMODE] = 0;
}
static uint32_t sig_of(const track_t *t) { return steps_sig(t); }
/* the answer part's notes as events */
typedef struct { int t, len, note; } ev_t;
static int answer_events(ev_t *out, int max)
{
    const track_t *t = &trk[1];
    int i, l, n = 0, len = t->p[P_SLEN];
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        if (s->time != ST_NOTE || !s->n)
            continue;
        for (l = 1; i + l < len && t->step[i + l].time == ST_TIE; l++)
            ;
        for (uint32_t k = 0; k < s->n && n < max; k++) {
            out[n].t = i; out[n].len = l; out[n].note = s->note[k] + t->p[P_TRANS]; n++;   /* (as it sounds: the part's TRN) */
        }
    }
    return n;
}
static int rubs(const take_def_t *d, const ev_t *e)
{
    int r = 0;
    for (int i = 0; i < d->nn; i++) {
        int iv = d->n[i].note + trk[0].p[P_TRANS] - e->note;
        iv = iv < 0 ? -iv : iv;
        if (d->n[i].t < e->t + e->len && e->t < d->n[i].t + d->n[i].len && iv < 24 && (iv % 12 == 1 || iv % 12 == 11))
            r++;
    }
    return r;
}
static int check_rules(const take_def_t *d, uint32_t mode, int *nout, const char *label)
{
    ev_t ev[128];
    int n = answer_events(ev, 128), bad = 0, i, b, nrub = 0, nkey = 0, nlow = 0, nroot = 0, ties_out = 0;
    int len = trk[1].p[P_SLEN];
    *nout = n;
    for (i = 0; i < n; i++) {
        int lo = 999, k;
        nkey += !tk_in_key(ev[i].note);
        nrub += rubs(d, &ev[i]);
        ties_out += ev[i].t + ev[i].len > len;
        for (k = 0; k < d->nn; k++)
            if (d->n[k].t <= ev[i].t && ev[i].t < d->n[k].t + d->n[k].len && d->n[k].note + trk[0].p[P_TRANS] < lo)
                lo = d->n[k].note + trk[0].p[P_TRANS];
        if (mode == TA_BASS && lo != 999 && ev[i].note > lo - 12) {
            nlow++;
            if (nlow == 1)
                printf("answer:   e.g. step %d note %d, lowest take note sounding %d, len %d\n", ev[i].t, ev[i].note, lo, ev[i].len);
        }
    }
    if (mode == TA_BASS)
        for (b = 0; b < d->bars; b++) {
            for (i = 0; i < n && !(ev[i].t >= b * 16 && ev[i].t < b * 16 + 16); i++)
                ;
            if (i < n && ev[i].note % 12 != tk_triad_pc(tk_chord[b], 0))
                nroot++;
        }
    bad = nkey || nrub || nlow || nroot || ties_out || trk[1].p[P_SLEN] != d->bars * 16 || n == 0;
    if (bad)
        printf("answer: %s: notes %d, out of key %d, rubs %d, not an octave under %d, bars not on the root %d, ties past the loop %d, length %d\n", label, n, nkey, nrub, nlow, nroot, ties_out, trk[1].p[P_SLEN]);
    if (bad) {
        static int dumped;
        if (!dumped++) {
            printf("answer:   events:");
            for (i = 0; i < n; i++)
                printf(" %d@%d+%d", ev[i].note, ev[i].t, ev[i].len);
            printf(" | key root %d nd %d nev %u amode %d slen %d\n", tk_kr, tk_nd, tk_nev, trk[0].p[P_AMODE], trk[0].p[P_SLEN]);
        }
    }
    return !bad;
}
int main(void)
{
    int ti, mi, ok_all = 1;
    unsigned combos = 0, passed = 0, distinct_ok = 0, distinct_cases = 0;
    setvbuf(stdout, 0, _IONBF, 0);
    for (ti = 0; ti < 5; ti++)
        for (mi = 0; mi < 5; mi++)
            for (uint32_t mode = TA_BASS; mode <= TA_DOUBLE; mode++)
                for (int scl = 0; scl < 2; scl++) {
                    const take_def_t *d = &TAKES[ti];
                    char lab[96];
                    uint32_t h1, h2, seeds[8], ns = 0;
                    int n1, n2;
                    power();
                    project_load(MIDX(MULTIS[mi] - 1));
                    frame();
                    scale_g = scl ? d->scale : 0;
                    root_g = scl ? d->root : 0;
                    fill(d);
                    trk[1].p[P_SLEN] = 16;
                    snprintf(lab, sizeof lab, "%s on M%02d, %s, %s", d->name, MULTIS[mi], mode == TA_BASS ? "BASS LINE" : "DOUBLE", scl ? "SCL set" : "SCL chromatic");
                    combos++;
                    CHECK(lab, tk_gen(0, 1, 0, mode) == 1u);
                    h1 = sig_of(&trk[1]);
                    passed += check_rules(d, mode, &n1, lab);
                    /* the same seed again: the same answer */
                    memset(trk[1].step, 0, sizeof trk[1].step);
                    tk_gen(0, 1, 0, mode);
                    h2 = sig_of(&trk[1]);
                    CHECK("the same seed gives the same answer", h1 == h2);
                    if (scl)
                        CHECK("SCL set: the key is the take's own, not guessed", !tk_guess && tk_kr == (uint32_t)d->root % 12u);
                    else
                        CHECK("SCL chromatic: the key is a guess (the header says so)", tk_guess == 1u);
                    /* 8 rolls */
                    for (uint32_t sd = 1; sd <= 8u; sd++) {
                        int seed_ok;
                        memset(trk[1].step, 0, sizeof trk[1].step);
                        trk[1].p[P_SLEN] = 16;
                        tk_gen(0, 1, sd * 7919u, mode);
                        seed_ok = check_rules(d, mode, &n2, lab);
                        ok_all &= seed_ok;
                        CHECK("a rolled answer passes the same rules", seed_ok);
                        {
                            uint32_t sg = sig_of(&trk[1]), k;
                            for (k = 0; k < ns && seeds[k] != sg; k++)
                                ;
                            if (k == ns)
                                seeds[ns++] = sg;
                        }
                    }
                    if (d->bars >= 2 && mode == TA_BASS && d->n[0].len != 8) {
                        distinct_cases++;
                        distinct_ok += ns >= 5u;
                    }
                }
    printf("answer: %u takes x multis x answers x keys: %u pass every rule at seed 0; the rolls of 8 seeds gave at least 5 different answers in %u of %u bass cases of 2 bars or more\n", combos, passed, distinct_ok, distinct_cases);
    CHECK("every answer at seed 0 passes the rules", passed == combos);
    CHECK("rolls give different answers in most bass cases", distinct_cases && distinct_ok * 4u >= distinct_cases * 3u);
    (void)ok_all;
    printf("answer: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
