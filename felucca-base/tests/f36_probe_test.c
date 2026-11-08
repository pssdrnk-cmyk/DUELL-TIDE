/* SPDX-License-Identifier: GPL-3.0-only */
/* Probe for audit finding 36 (2026-11-08): POLY + ARP SEQ, the step phase after the arp restarts (all keys released, then pressed) while a
 * shortened stretch is on. tide_arp_idx = (k - poly_base) % poly_len with unsigned k; arp_remove / arp_add set arp_idx back to 0 but
 * poly_base keeps the old arp_idx of the stretch's start, so the subtraction wraps and the phase is arbitrary.
 * Expected: the first step of the restarted arp (arp_idx 0) plays step 0, then 1, 2, ... (the short loop's own count).
 * KNOWN FAILING while finding 36 is open; not in host_tests.sh's default list (run: sh host_tests.sh f36). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("f36: FAIL %s\n", s); bad++; } } while (0)

int main(void)
{
    track_t *t;
    uint32_t pi = 0, k, got[6];
    ui_power_on();
    t = &trk[pi];
    t->p[P_SLEN] = 16;
    t->p[P_AMODE] = AM_SEQ_ID;
    sqm_poly = 1;                         /* GENTLE */
    sqm_auto |= 1u << pi;
    song.playing = 1; song.rec = 0;
    /* a stretch began at arp_idx 64 (the 5th full loop), 14 of 16 steps, long enough to be still on */
    poly_len[pi] = 14; poly_base[pi] = 64; poly_lc[pi] = 5; poly_end[pi] = 13; poly_free[pi] = 0;
    CHECK("setup: arp_mode is ARP SEQ", arp_mode(t) == AM_SEQ_ID);
    /* normal running, inside the stretch: arp_idx 64 + j plays j */
    CHECK("control: arp_idx 64 is the short loop's step 0", tide_arp_idx(t, 64, 16) == 0);
    CHECK("control: arp_idx 70 is step 6", tide_arp_idx(t, 70, 16) == 6);
    /* keys released, pressed again: arp_idx restarts at 0, the arp tick for step 0 runs */
    t->arp_idx = 0;
    tide_arp_tick(t);
    printf("f36: after restart poly_len=%u poly_base=%u\n", (unsigned)poly_len[pi], (unsigned)poly_base[pi]);
    for (k = 0; k < 6; k++)
        got[k] = poly_len[pi] ? tide_arp_idx(t, k, 16) : k % 16u;
    printf("f36: steps for arp_idx 0..5 = %u %u %u %u %u %u (want 0 1 2 3 4 5)\n", got[0], got[1], got[2], got[3], got[4], got[5]);
    CHECK("arp_idx 0 after the restart plays step 0", got[0] == 0);
    CHECK("arp_idx 1..5 play steps 1..5", got[1] == 1 && got[2] == 2 && got[3] == 3 && got[4] == 4 && got[5] == 5);
    printf("f36: %d checks, %d failed\n", checks, bad);
    return bad ? 1 : 0;
}
