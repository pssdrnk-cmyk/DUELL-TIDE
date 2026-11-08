/* SPDX-License-Identifier: GPL-3.0-only */
/* Probe for audit finding 36 (2026-11-08): POLY + ARP SEQ, the step phase after the arp restarts (all keys released, then pressed) while a
 * shortened stretch is on. tide_arp_idx = (k - poly_base) % poly_len with unsigned k; arp_remove / arp_add set arp_idx back to 0 but
 * poly_base keeps the old arp_idx of the stretch's start, so the subtraction wraps and the phase is arbitrary.
 * Expected: the first step of the restarted arp (arp_idx 0) plays step 0, then 1, 2, ... (the short loop's own count).
 * Fixed in tide_arp_tick (poly_base reset at arp_idx 0). Also: a restart with no stretch changes nothing. Group f36. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("f36: FAIL %s\n", s); bad++; } } while (0)

/* all keys released, then one pressed: arp_remove, arp_add (nheld 1: arp_idx = 0xFFFFFFFF), the first arp step (arp_fire: arp_idx 0) */
static void key_restart(track_t *t)
{
    t->nheld = 0; t->arp_phys = 0;
    arp_add(t, 60);
    CHECK("press: arp_add left arp_idx at 0xFFFFFFFF", t->arp_idx == 0xFFFFFFFFu);
    arp_fire(t, 6000);
}

int main(void)
{
    track_t *t;
    uint32_t pi = 0, k, got[6], lc0, sl0;
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
    lc0 = poly_lc[pi]; sl0 = tide_seq_loops[pi];
    key_restart(t);                             /* the real path: release all, arp_add (arp_idx 0xFFFFFFFF), arp_fire (arp_idx 0) */
    CHECK("restart: arp_idx is 0", t->arp_idx == 0u);
    CHECK("restart: the stretch goes on", poly_len[pi] == 14u);
    CHECK("restart: poly_base is 0", poly_base[pi] == 0u);
    CHECK("restart: counts one full loop (poly_lc)", poly_lc[pi] == lc0 + 1u);
    CHECK("restart: counts one loop (tide_seq_loops)", (uint8_t)(tide_seq_loops[pi] - sl0) == 1u);
    printf("f36: after restart poly_len=%u poly_base=%u\n", (unsigned)poly_len[pi], (unsigned)poly_base[pi]);
    for (k = 0; k < 6; k++)
        got[k] = poly_len[pi] ? tide_arp_idx(t, k, 16) : k % 16u;
    printf("f36: steps for arp_idx 0..5 = %u %u %u %u %u %u (want 0 1 2 3 4 5)\n", got[0], got[1], got[2], got[3], got[4], got[5]);
    CHECK("arp_idx 0 after the restart plays step 0", got[0] == 0);
    CHECK("arp_idx 1..5 play steps 1..5", got[1] == 1 && got[2] == 2 && got[3] == 3 && got[4] == 4 && got[5] == 5);
    /* the other direction: no stretch, a restart changes nothing (steps 0..n) */
    poly_len[pi] = 0; poly_base[pi] = 0; poly_lc[pi] = 5; poly_end[pi] = 0; poly_free[pi] = 1000;   /* (poly_free far: no new stretch starts) */
    lc0 = poly_lc[pi]; sl0 = tide_seq_loops[pi];
    key_restart(t);
    CHECK("no stretch: arp_idx is 0", t->arp_idx == 0u);
    CHECK("no stretch: counts one full loop (poly_lc)", poly_lc[pi] == lc0 + 1u);
    CHECK("no stretch: counts one loop (tide_seq_loops)", (uint8_t)(tide_seq_loops[pi] - sl0) == 1u);
    CHECK("no stretch: poly_base is 0", poly_base[pi] == 0u);
    CHECK("no stretch: still none after a restart", poly_len[pi] == 0);
    for (k = 0; k < 20; k++)
        CHECK("no stretch: arp_idx k plays k % 16", tide_arp_idx(t, k, 16) == k % 16u);
    /* audit follow-up (b4-arpidx): the footer's playhead on the ARP pages reads the stretch too (foot_playhead), and tide_play_idx agrees with it */
    poly_len[pi] = 14; poly_base[pi] = 64; poly_lc[pi] = 5; poly_end[pi] = 13; poly_free[pi] = 0;
    t->nheld = 1; ui.layer = 0; ui.perf = 0; ui.bank = 0; page_over = 0; song.sel = (uint8_t)pi;
    for (k = 0; k < 64; k++) { ui.page = k; if (cur_page()->fam == FAM_ARP) break; }
    CHECK("an ARP page exists", foot_arp_page());
    for (k = 64; k < 80; k++) {
        t->arp_idx = k;
        CHECK("play_idx follows the stretch", tide_play_idx(t) == tide_arp_idx(t, k, 16));
        CHECK("footer playhead = the step that plays", foot_playhead(t) == tide_play_idx(t));
    }
    t->arp_idx = 0xFFFFFFFFu;
    CHECK("play_idx: none yet", tide_play_idx(t) == 0xFFFFFFFFu);
    CHECK("play_len is the stretch", tide_play_len(t) == 14u);
    /* peer prediction (seq_dialogue) is blind to a shortened part: it declines while a stretch is on */
    CHECK("dialogue declines in a stretch", tide_poly_on(t));
    printf("f36: %d checks, %d failed\n", checks, bad);
    return bad ? 1 : 0;
}
