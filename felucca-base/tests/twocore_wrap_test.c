/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Regression guard for audit A1 (two-core job number wrap, audit F05, fix 9742bf2). One thread, core 1's step called by hand (as tcstage1).
 * The job number is 32 bits, one job per 32-frame block (0.73 ms): it wraps after 2^32 posts = about 36 days of play with core 1 up.
 * 0 means "no job", so tc_next() must skip it on both sides, and every answer test (tc_answer incl. the TCA_BAD "ahead" check, tc_closed,
 * take, yld, refusal) must keep working while the numbers cross 0xFFFFFFFF -> 1 with stale words from before the wrap in the mailbox.
 * Seeded runs: the counters start at 0xFFFFFFFF - k for k = 0..9 and 40 jobs of every kind (done, take before the claim, take after the
 * claim, refused) are walked across the wrap. Built and run by host_tests.sh (name: tcwrap) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define TC_JOB_RUN host_job
static void host_job(uint32_t arg);
#include "twocore_hal_host.h"
#include "../firmware/src/twocore.c"

static int bad, ncheck;
static void expect(const char *what, int ok, uint32_t j)
{
    ncheck++;
    if (!ok) {
        printf("tcwrap: FAIL %s (job %08x)\n", what, (unsigned)j);
        bad++;
    }
}
static uint32_t runs, last_arg;
static int take_in_job;
static void host_job(uint32_t arg)
{
    runs++;
    last_arg = arg;
    if (take_in_job) {
        take_in_job = 0;
        tc_take(tc.job);
    }
}
static tc_c1_t c1;
static const uint32_t *f_word;
static uint32_t f_xor, f_count;
static uint32_t hook(const uint32_t *p, uint32_t v)
{
    if (p == f_word && f_count) { f_count--; return v ^ f_xor; }
    return v;
}

int main(void)
{
    uint32_t k, n;
    for (k = 0; k < 10u; k++) {
        uint32_t start = 0xFFFFFFFFu - k, prev = start, j, r0;
        memset(&tc, 0, sizeof tc);
        memset(&tc_nv, 0, sizeof tc_nv);
        tc_host_ld_hook = 0;
        tc_host_start_ok = 1;
        tc_arm();
        tc_boot(1u, 0, 0);
        tc_c1_init(&c1);
        /* every word of the mailbox is "as after job `start`", the take word left by that job, the yield word one before */
        tc.job = tc_mb.job = tc_mb.done = tc_mb.claim = c1.last = start;
        tc_mb.yld = start - 1u; tc_mb.take = start;
        tc_mb.bad_job = start - 2u;
        expect("seed closed", tc_closed(), start);
        for (n = 0; n < 40u; n++) {
            r0 = runs;
            j = tc_post(0x100u + n);
            expect("never 0", j != 0u && tc_mb.job != 0u && tc.job != 0u, j);
            expect("sequential (skipping only 0)", j == (prev + 1u ? prev + 1u : 1u), j);
            expect("stale words are not answers (NONE, not BAD)", tc_answer(j) == TCA_NONE && !tc_closed(), j);
            switch (n % 4u) {
            case 0:                                     /* plain */
                tc_c1_step(&c1);
                expect("done", tc_answer(j) == TCA_DONE && tc_closed() && runs == r0 + 1u && last_arg == 0x100u + n, j);
                break;
            case 1:                                     /* take before core 1 looked */
                tc_take(j);
                tc_c1_step(&c1);
                expect("yld", tc_answer(j) == TCA_YIELD && tc_closed() && runs == r0, j);
                break;
            case 2:                                     /* take after the claim */
                take_in_job = 1;
                tc_c1_step(&c1);
                expect("done after a late take", tc_answer(j) == TCA_DONE && tc_closed() && runs == r0 + 1u, j);
                break;
            default:                                    /* check value stays wrong: refused */
                f_word = &tc_mb.chk; f_xor = 1u; f_count = 8u; tc_host_ld_hook = hook;
                tc_c1_step(&c1);
                tc_host_ld_hook = 0;
                expect("refused, not run", tc_answer(j) == TCA_REFUSED && runs == r0, j);
                tc_mb.done = j;                         /* (the runner would play it itself; mark the slot closed for the next post) */
                break;
            }
            expect("a step after the answer runs nothing", (tc_c1_step(&c1), runs) == runs, j);
            expect("core 1's last follows", c1.last == j, j);
            /* a done word one behind and one ahead of the head must classify across the wrap */
            expect("old done is not BAD", ((int32_t)(tc_mb.done - tc.job)) <= 0, j);
            prev = j;
        }
        expect("crossed the wrap", prev < 0x80u, prev);
        tc_c1_init(&c1);
    }
    printf("tcwrap: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
