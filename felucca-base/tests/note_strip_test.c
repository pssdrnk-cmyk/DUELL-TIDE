/* SPDX-License-Identifier: GPL-3.0-only */
/* notestrip (b11, PR 79): the standard note strip's heights come from the pitch each step SOUNDS (ui.c pv_top / pv_rank, ui_graph.c ns_states /
 * ns_h, tide_ui.c tide_strip_states). A note is 6 .. 11 px (AUTO TIDE 6 .. 9) between the lowest and the highest note of the 16 shown, never a rest's 3;
 * one distinct note: the middle (8).
 *   1 all rests: nothing ranked, the classes untouched
 *   2 one distinct note: 8 px
 *   3 the range edges (a TRN or TRANS + / - that pushed the notes to 0 and 127): 6 and 11 px (a note 0 is a note, not "none"; the lowest is AUTO TIDE's 6, not a rest's 3)
 *   4 an ARP SEQ part sounds note[0] only (seq.c arp_seq_notes): its chord C3 + E5 is ranked by C3; a SEQ part's by E5 (its top)
 *   5 AUTO TIDE: an added note (a stored rest that plays a note now) is ranked by the note it plays, not drawn at the minimum
 * Built and run by host_tests.sh (name notestrip). */
#define UI_TEST_NO_MAIN 1
#include <stdlib.h>
#include "ui_test.c"
static int ns_fails;
static void ns_check(int ok, const char *what, int a, int b)
{
    if (!ok) {
        printf("notestrip: FAIL %s (%d, %d)\n", what, a, b);
        ns_fails++;
    }
}
static void ns_put(step_t *s, uint32_t n, uint8_t a, uint8_t b)
{
    memset(s, 0, sizeof *s);
    s->time = n ? ST_NOTE : ST_REST;
    s->n = (uint8_t)n;
    s->note[0] = a;
    s->note[1] = b;
}
static void ns_clear(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        ns_put(&t->step[i], 0, 0, 0);
    t->p[P_SLEN] = 16;
    t->p[P_AMODE] = 0;
}
int main(void)
{
    track_t *t;
    uint8_t st[16];
    uint32_t i;
    tv_t tv[16];
    ui_power_on();
    t = &trk[0];
    mods_off = 0;
    /* 1 all rests */
    ns_clear(t);
    ns_states(st, t, 0);
    for (i = 0; i < 16u; i++)
        ns_check(st[i] == 1u, "all rests: a rest's state is the rest class, nothing ranked", (int)i, st[i]);
    /* 2 one distinct note */
    ns_clear(t);
    ns_put(&t->step[2], 1, 60, 0);
    ns_put(&t->step[9], 1, 60, 0);
    ns_states(st, t, 0);
    ns_check(ns_h(st[2], NS_HMAX) == 8 && ns_h(st[9], NS_HMAX) == 8, "one distinct note: the middle, 8 px", ns_h(st[2], NS_HMAX), ns_h(st[9], NS_HMAX));
    /* 3 the range edges */
    ns_clear(t);
    ns_put(&t->step[2], 1, 0, 0);
    ns_put(&t->step[9], 1, 127, 0);
    ns_states(st, t, 0);
    ns_check(ns_h(st[2], NS_HMAX) == 6 && ns_h(st[9], NS_HMAX) == 11, "notes 0 and 127: 6 and 11 px (the lowest never a rest's 3)", ns_h(st[2], NS_HMAX), ns_h(st[9], NS_HMAX));
    ns_check(ns_h(st[2], 9) == 6 && ns_h(st[9], 9) == 9, "AUTO TIDE's rows (hmax 9): 6 and 9 px", ns_h(st[2], 9), ns_h(st[9], 9));
    /* 4 a chord on an ARP SEQ part sounds note[0]; on a SEQ part its top */
    ns_clear(t);
    ns_put(&t->step[2], 2, 48, 76);
    ns_put(&t->step[9], 1, 60, 0);
    t->p[P_AMODE] = AM_SEQ_ID;
    ns_states(st, t, 0);
    ns_check(ns_h(st[2], NS_HMAX) < ns_h(st[9], NS_HMAX), "ARP SEQ: the chord C3 + E5 sounds C3, under a lone C4", ns_h(st[2], NS_HMAX), ns_h(st[9], NS_HMAX));
    t->p[P_AMODE] = 0;
    ns_states(st, t, 0);
    ns_check(ns_h(st[2], NS_HMAX) > ns_h(st[9], NS_HMAX), "SEQ part: the chord C3 + E5 by its top, over a lone C4", ns_h(st[2], NS_HMAX), ns_h(st[9], NS_HMAX));
    /* 5 AUTO TIDE: an added note */
    ns_clear(t);
    ns_put(&t->step[2], 1, 48, 0);                      /* (step 5 is a stored rest) */
    memset(tv, 0, sizeof tv);
    for (i = 0; i < 16u; i++)
        tv[i].k = TV_REST;
    tv[2].k = TV_HIT, tv[2].n = 1, tv[2].top = 48 + 1;
    tv[5].k = TV_ADD, tv[5].n = 1, tv[5].top = 84 + 1;  /* (TIDE's variation plays C6 on the rest) */
    tide_strip_states(t, tv, 0, st);
    ns_check(ns_h(st[5], 9) == 9 && ns_h(st[2], 9) == 6, "AUTO TIDE: an added C6 over a C3 is the tallest (9), the C3 a whole note (6)", ns_h(st[5], 9), ns_h(st[2], 9));
    printf("notestrip: %s: the strip's heights follow the pitch each step sounds (%d failed)\n", ns_fails ? "FAIL" : "ok", ns_fails);
    return ns_fails != 0;
}
