/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 item 132 (HANDOFF_V2): the two-core runner's deadline and wait limit (twocore_run.c tcr_deadline) are the same, bit for bit, as before item 132, which raised the NOTES
 * budget's BUDGET_LIMIT 80 -> 90 and stopped charging the hand-over BUDGET_HO in the caps (budget.c budget_solve2). tcr_deadline reads only part_fixed, part_cost and global_cost
 * (BD_U, BUDGET_MARGIN 1.15 kept); this test proves the values did not move.
 *   GRID     LAYER / STEREO / DUEL x SENDS on / off x PART FX on / off x every engine on part 1 x every engine on part 2 (each its first factory sound, cleaned) x the multimode
 *            filter of each part OFF / 12 dB / 24 dB / 12 dB + TWIN LP; and every factory sound of every engine as stored, on both parts, LAYER and DUEL
 *   VALUES   for each setup, both NOTES rules (core 1 up / jobs paused), each block of the half 0..3 and the sounding voices of each part 0, 1, 2, 3, 4, 6, 8: the five cost items
 *            (c1f c1v c2f c2v g), the deadline and the wait limit (ticks after the half's start)
 *   CHECK    the FNV-1a hash of all of them equals DL_REF, the hash the same test printed on the sources before item 132 (scratch copy, 2026-10-26)
 * DUELL 1.1 item 137 (owner: BUDGET_MARGIN 1.15 -> 1.00 for the caps, BUDGET_LIMIT 88 -> 81): tcr_deadline reads part_fixed_dl / part_cost_dl / global_cost_dl (BUDGET_DL_MARGIN 1.15),
 * so DL_REF must still hold. The deadline a job is POSTED with (tcr_post_deadline) has one change: under the two-core rule, when the 1.15 deadline is already past at the post,
 * the true-cost deadline (tcr_deadline_true) if it is still ahead and before the wait limit (the old code had no take-over there). For every setup, rule, block, voice count
 * and 16 post moments (every quarter block of the half) the second part checks:
 *   SAME     wherever the 1.15 deadline is ahead of the post, or the one-core rule holds, the posted deadline IS it (bit for bit)
 *   LATER    the posted deadline is never earlier than the 1.15 deadline; the wait limit is the 1.15 one, never moved (it is in DL_REF)
 *   INSIDE   a changed deadline is after the post and before the wait limit
 *   TRUE     a changed deadline is not later than the deadline of the exact 1.00 items (part_fixed, part_cost, global_cost: BUDGET_MARGIN 100) and within 1 % of a block of it
 *   KEPT     wherever the exact true deadline is ahead of the post by more than that 1 % and before the wait limit, a take-over deadline exists (the item's purpose)
 *            and it prints where the posted deadline differs (counts by block and post moment) and the hash of the posted deadlines (DL_REF2, recorded by item 137)
 *   CORE 1   (the BUDGET_LIMIT 81 choice) with both parts at their two-core caps (budget_solve2, KEY SENSE +57 as on the unit), block 0's job, posted 0.27 block into the half
 *            (polymax median 196 us) and running the true cost of part 2, ends at least 6 points of a block (the measured peak over the average) before the 1.15 wait limit
 * DUELL 1.1 item 142 (HANDOFF_V2 item 139, owner-approved): the WAIT LIMIT is the later of the 1.15 one and the same rest at TRUE cost (the 1.15 items brought back, rounded up) less
 * TCR_SLACK (0.2 block), never after the half's end; BUDGET_LIMIT 81 -> 84. DL_REF now hashes the 1.15 wait limit as this test works it out (the items and the deadline are the firmware's):
 * it still holds, so items, deadline and the old limit are unchanged. The firmware's new limit:
 *   WL-TRUE  it is exactly max(1.15 limit, t0 + H - ceil-true rest - TCR_SLACK), never after t0 + H (this test's own formula, every setup, rule, block and voice count)
 *   WL-LATER never earlier than the 1.15 limit and never after the half's end
 *   WL-SAFE  where it moved, it + the EXACT true rest (part_fixed / part_cost / global_cost at margin 1.00; one-core rule with part 2) <= t0 + H - TCR_SLACK
 *   CORE 1   both parts at their two-core caps of BUDGET_LIMIT (KEY SENSE +57), every block k: core 1's job k, posted at 0.27 block + k x (globals + the dearer of part 1 and the job,
 *            true cost) and running part 2's true cost, + 6 points of a block (the peak allowance) ends before the wait limit (least slack printed, also with 13 points: the measured
 *            worst single job, item 139); CORE 0: BUDGET_LIMIT + 6 <= 90. The least slack by limit 80 .. 88 is printed (caps worked out here at each limit)
 *   and SAME / LATER / INSIDE / TRUE / KEPT above against the new limit, the new hashes DL_REF2 (posted deadlines) and DL_REF3 (the new limits), the limits changed by block
 * CPU fix (wave 14, 2026-11-05): BUDGET_LIMIT 84 -> 72 with BUDGET_PEAK 18 (budget.h: the unit's per-second peaks against the model), so CORE 0 is BUDGET_LIMIT + BUDGET_PEAK = 90 and CORE 1 keeps
 * the allowance in points of a block as BUDGET_PEAK too (58.9 points of slack after 6, 46.9 after 18 at limit 72; the table of limits now runs 64 .. 88). The deadline and the wait limit read neither:
 * DL_REF, DL_REF2 and DL_REF3 hold unchanged.
 * Build: cc -O1 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -fno-builtin -D_FORTIFY_SOURCE=0
 *        -o build/host/deadline_test tests/deadline_test.c -lm -lpthread   (host_tests.sh name: deadline) */
#define DUELL_TWOCORE 1
#define TC_AUTO_ARM 0
#define UI_TEST_NO_MAIN 1
#include <stdint.h>
#include <time.h>
#include "ui_test.c"
#include "scenes.h"

#define DL_REF 0x49eceb6648533d0dull   /* RE-BASED 2026-11-11 (b9 rework3: the records of M02 (EQ, phaser) and M06 HSE FRENCH (MELLOW EP + FRETLESS, was HSE PIANO) and M18 (PRIO LOW) load other sounds and values; no firmware source changed; was 0x215d66fdb3d7ece0). RE-BASED 2026-11-11 (b9 rework2: the records of M02, M07, M08, M12, M15, M16 and the drone M18 AMB GROWL load other sounds, notes and values; no firmware source changed; was 0x858490f0773a1cc7). RE-BASED 2026-11-11 (b9 rc2: the records of the b9 sound set, M01 to M16 re-voiced and renamed (b9-sound, lanes A and B, the names): the 16-multi loop loads them; with the b9-integ records (py and bins) put back the value below passes; was 0x6d79ef90178eb8c2). Earlier: RE-BASED 2026-11-11 (b9 drone sub: the records of M17 DUNE HORN and M18 VOID DEEP, slot 16 here, carry the sub (SUB LEVEL 72, OCTAVE 1) and -2 part levels; the records only: with fce3d8b's M17 / M18 bins the old value comes back; was 0x213b3e19ecfda305). RE-BASED 2026-11-11 (b8: factory multis M09 HYPNO 303, M10 TWO 303S, M11 ACID LINES are slots 9, 10, 11 of the loop below; the records only, no firmware cost model: with b7's duell_multis.h the old value comes back, with and without DRIFT; was 0xcbd7ab6687fa2552).                  /* RE-BASED 2026-11-10 (the reel re-voiced: beds, echoes and part swaps in M01, M03..M16; was 0x397c31ec82ee61c2). RE-BASED 2026-11-10 (reel, 2026-11-10: the fifteen new factory multis M01, M03..M16 load other sounds, patterns and values; no firmware source and no cost model changed, the records only; was 0xca501cf07736094f). RE-BASED 2026-11-10 (b7 integration: MULTIS' and TECHNO's records together, TECHNO's M03 / M04 / M07 / M13-M15 new; no cost model changed); was 0x9d8e57de94896d21. RE-BASED 2026-11-10 (b7 MULTIS: M05 swing, M08 / M09 pads an octave down, M17 / M18 beds as ARP SEQ + slower steps + levels, M10 / M11 / M13 / M16 levels and M16 low cut change the loaded multis); was 0x2e8bf35f0db5d399. RE-BASED 2026-11-09 (TIDE b6, fix/b6-droneseq: M17 DUNE HORN, this test's slot 16, has a sequence on part 1 now (ARP SEQ, tools/drone_pack.py); nothing else moved (verified: with the 44067bd M17 / M18 records all three hashes are their earlier values exactly); was 0x7354ac5679c0af7d). RE-BASED 2026-11-09 (TIDE b6, drones: M17/M18 added to the factory multis, so this test's slot 16 is now M17 DUNE HORN where it wrapped to M01 before MU_SHOWN 18; M01-M16 unchanged (verified: with slot 16 loading M01, all three hashes are b5's exactly); was 0x9b99961d4f7e9462). RE-BASED 2026-11-05 (D31, audit G14: the same: the parts of the 16 multis are other parts, no cost model changed; was 0x7c9f17ff34b5a9bc). RE-BASED 2026-11-05 (audit A01: TIDE's DSP items, budget.c BD_TIDE_*, are in the parts of the 16 factory multis, the only setups here with a MORPH; was 0x16fbf57e4a8345f2). RE-BASED 2026-11-05 on integ 202400b: 793f41f fix/fx-extend (audit R09: ROOM charged as HALL, budget.c BD_REV; moves M01, M02 and M15) and 9aaac54 fix/bank-content (audit G06, G10, G12, G15: the factory sounds re-voiced / re-ordered, three FM6 dub voices added; items of the same kind, no cost model change); was 0xe2d0569e4ca5f697; the pre-item-132 sources (BUDGET_LIMIT 80, BUDGET_HO charged), this test, 2026-10-26 (item 142: with the 1.15 limit as worked out here; re-based 2026-11-03: U2 B, ten factory multis hold sequences / other arps, was 0xb8db0653a8dfc654) */ /* (b9: b10 round 5: DUB MINIMAL's tape hiss a steady quiet floor; DUB CHORD's stab cleaner and steadier, proven by revert; was 0x622a4c62b306ee9e) */ /* (b9: b13 sound work combined (PR 110 + PR 111 + rc18c fixes), the cause for repin, proven by revert; was 0x30da22d099ce7e28) */ /* (b9: The 12 factory drum slots become tonal sounds: an imported FM piano, three Clipper translations, eight hand-voiced, proven by revert; was 0x11ebf3aa2efb7eac) */ /* (b9: Ex-drum slots: only human-made patches (owner) - eight slots take Felucca's own engine presets, proven by revert; was 0xe02f37d91f6e2ae4) */
#define DL_REF2 0xc5826ca8cddff4f4ull   /* RE-BASED 2026-11-11 (b9 rework3: the records of M02 (EQ, phaser) and M06 HSE FRENCH (MELLOW EP + FRETLESS, was HSE PIANO) and M18 (PRIO LOW) load other sounds and values; no firmware source changed; was 0xd73b1b3233981cf0). RE-BASED 2026-11-11 (b9 rework2: the records of M02, M07, M08, M12, M15, M16 and the drone M18 AMB GROWL load other sounds, notes and values; no firmware source changed; was 0xd5d9ee365f4bd39f). RE-BASED 2026-11-11 (b9 rc2: the records of the b9 sound set, M01 to M16 re-voiced and renamed (b9-sound, lanes A and B, the names): the 16-multi loop loads them; with the b9-integ records (py and bins) put back the value below passes; was 0xbd1ef98985b38e60). Earlier: RE-BASED 2026-11-11 (b9 drone sub: the records of M17 DUNE HORN and M18 VOID DEEP, slot 16 here, carry the sub (SUB LEVEL 72, OCTAVE 1) and -2 part levels; the records only: with fce3d8b's M17 / M18 bins the old value comes back; was 0x7cc92a93eb9d4edc). RE-BASED 2026-11-11 (b8: factory multis M09 HYPNO 303, M10 TWO 303S, M11 ACID LINES are slots 9, 10, 11 of the loop below; the records only, no firmware cost model: with b7's duell_multis.h the old value comes back, with and without DRIFT; was 0x494154b548342a05).                 /* RE-BASED 2026-11-10 (the reel re-voiced, as DL_REF; was 0xc407d74db6dda025). RE-BASED 2026-11-10 (reel, 2026-11-10, as DL_REF; was 0x9be0bfca379fcba4). RE-BASED 2026-11-10 (b7 integration, as DL_REF; was 0x4a4bc366ce67e484 after b7 MULTIS); before that 0x571e0398c0feeb09. RE-BASED 2026-11-09 (TIDE b6, fix/b6-droneseq: M17 DUNE HORN, this test's slot 16, has a sequence on part 1 now (ARP SEQ, tools/drone_pack.py); nothing else moved (verified: with the 44067bd M17 / M18 records all three hashes are their earlier values exactly); was 0xbc1d8cc3ae626df6). RE-BASED 2026-11-09 (TIDE b6, drones: M17/M18 added to the factory multis, so this test's slot 16 is now M17 DUNE HORN where it wrapped to M01 before MU_SHOWN 18; M01-M16 unchanged (verified: with slot 16 loading M01, all three hashes are b5's exactly); was 0xe409915f0a1142ca). RE-BASED 2026-11-05 (D31, audit G14: the same: the parts of the 16 multis are other parts, no cost model changed; was 0x4b7848a52aa8bddd). RE-BASED 2026-11-05 (audit A01: TIDE's DSP items in the multis' parts; was 0x1f20de1d06f657f3). RE-BASED 2026-11-05 on integ 202400b: 793f41f fix/fx-extend (audit R09: ROOM charged as HALL, budget.c BD_REV; moves M01, M02 and M15) and 9aaac54 fix/bank-content (audit G06, G10, G12, G15: the factory sounds re-voiced / re-ordered, three FM6 dub voices added; items of the same kind, no cost model change); was 0xc0af709a3883c486; item 142: the posted deadlines (tcr_post_deadline) at 16 post moments with the new wait limit (item 137's was 0xccfd48c6e1c8ecc7; re-based 2026-11-03: U2 B, ten factory multis hold sequences / other arps, was 0x40924773c6c4fbf0) */ /* (b9: b10 round 5: DUB MINIMAL's tape hiss a steady quiet floor; DUB CHORD's stab cleaner and steadier, proven by revert; was 0x9cb1e3bb8042179d) */ /* (b9: b13 sound work combined (PR 110 + PR 111 + rc18c fixes), the cause for repin, proven by revert; was 0x9105e9b4bf01c9ae) */ /* (b9: The 12 factory drum slots become tonal sounds: an imported FM piano, three Clipper translations, eight hand-voiced, proven by revert; was 0xde4d750390661e3e) */ /* (b9: Ex-drum slots: only human-made patches (owner) - eight slots take Felucca's own engine presets, proven by revert; was 0xce70ea9cd6b21b27) */
#define DL_REF3 0x5c77c0c0acab6676ull   /* RE-BASED 2026-11-11 (b9 rework3: the records of M02 (EQ, phaser) and M06 HSE FRENCH (MELLOW EP + FRETLESS, was HSE PIANO) and M18 (PRIO LOW) load other sounds and values; no firmware source changed; was 0x8eefdc5bbcb649d7). RE-BASED 2026-11-11 (b9 rework2: the records of M02, M07, M08, M12, M15, M16 and the drone M18 AMB GROWL load other sounds, notes and values; no firmware source changed; was 0x8633818f5c1ad186). RE-BASED 2026-11-11 (b9 rc2: the records of the b9 sound set, M01 to M16 re-voiced and renamed (b9-sound, lanes A and B, the names): the 16-multi loop loads them; with the b9-integ records (py and bins) put back the value below passes; was 0xc558c6bdf7a5fad0). Earlier: RE-BASED 2026-11-11 (b9 drone sub: the records of M17 DUNE HORN and M18 VOID DEEP, slot 16 here, carry the sub (SUB LEVEL 72, OCTAVE 1) and -2 part levels; the records only: with fce3d8b's M17 / M18 bins the old value comes back; was 0xb1c18d8cb6cce4e2). RE-BASED 2026-11-11 (b8: factory multis M09 HYPNO 303, M10 TWO 303S, M11 ACID LINES are slots 9, 10, 11 of the loop below; the records only, no firmware cost model: with b7's duell_multis.h the old value comes back, with and without DRIFT; was 0x67196918df2bde7d).                 /* RE-BASED 2026-11-10 (the reel re-voiced, as DL_REF; was 0x44dc5099aaa98111). RE-BASED 2026-11-10 (reel, 2026-11-10, as DL_REF; was 0xaf84b86293170187). RE-BASED 2026-11-10 (b7 integration, as DL_REF; was 0x6bfa7a8aa7e01964 after b7 MULTIS); before that 0xefce42cd64a43d3a. RE-BASED 2026-11-09 (TIDE b6, fix/b6-droneseq: M17 DUNE HORN, this test's slot 16, has a sequence on part 1 now (ARP SEQ, tools/drone_pack.py); nothing else moved (verified: with the 44067bd M17 / M18 records all three hashes are their earlier values exactly); was 0x503d075fcf1b8842). RE-BASED 2026-11-09 (TIDE b6, drones: M17/M18 added to the factory multis, so this test's slot 16 is now M17 DUNE HORN where it wrapped to M01 before MU_SHOWN 18; M01-M16 unchanged (verified: with slot 16 loading M01, all three hashes are b5's exactly); was 0xcf4b39279a8e8e89). RE-BASED 2026-11-05 (D31, audit G14: the same: the parts of the 16 multis are other parts, no cost model changed; was 0x400bc7ef6465b408). RE-BASED 2026-11-05 (audit A01: TIDE's DSP items in the multis' parts; was 0x5a319bd6ae893342). RE-BASED 2026-11-05 on integ 202400b: 793f41f fix/fx-extend (audit R09: ROOM charged as HALL, budget.c BD_REV; moves M01, M02 and M15) and 9aaac54 fix/bank-content (audit G06, G10, G12, G15: the factory sounds re-voiced / re-ordered, three FM6 dub voices added; items of the same kind, no cost model change); was 0x05a0318a41d71b48; item 142: the new wait limits (recorded on the item 142 sources, 2026-10-27; re-based 2026-11-03: U2 B, ten factory multis hold sequences / other arps, was 0x70d45c7084c0d110) */ /* (b9: b10 round 5: DUB MINIMAL's tape hiss a steady quiet floor; DUB CHORD's stab cleaner and steadier, proven by revert; was 0x35ae00abede37c1d) */ /* (b9: b13 sound work combined (PR 110 + PR 111 + rc18c fixes), the cause for repin, proven by revert; was 0x88d980090d06976b) */ /* (b9: The 12 factory drum slots become tonal sounds: an imported FM piano, three Clipper translations, eight hand-voiced, proven by revert; was 0xd9f3e4caa72dfb58) */ /* (b9: Ex-drum slots: only human-made patches (owner) - eight slots take Felucca's own engine presets, proven by revert; was 0x7e10e5601376ad9f) */
static uint64_t h3 = 1469598103934665603ull;
static uint64_t n_lim_chg[4], bad_wltrue, bad_wllater, bad_wlsafe, n_lim;
static uint64_t n_hand, bad_hand;                       /* b11 HANDOVER (from DUELL 1.2's deadline_test, OSCILL/R, GPL-3.0): the main loop's items (budget.c bd_dl_publish) = the runner's own */
static uint32_t n_lclear;                               /* b11 LOAD / AGE: the clears, publishes and the age limit seen (6 of 6) */
static int32_t lim_chg_max;
static void put3(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h3 ^= (v >> (8u * i)) & 0xFFu;
        h3 *= 1099511628211ull;
    }
}
#define T_TRUE(x) (((x) * 100u + BUDGET_DL_MARGIN - 1u) / BUDGET_DL_MARGIN)
/* the wait limit, ticks after the half's start (negative: before it), as the runner must have it: old = the 1.15 rest; new = the later of that and the ceil-true rest + TCR_SLACK */
static int32_t lim_of(uint32_t f1, uint32_t v1, uint32_t f2, uint32_t v2, uint32_t g, uint32_t k, uint32_t a1, uint32_t a2, uint32_t two, int32_t *old)
{
    uint32_t c1 = f1 + a1 * v1, c2 = f2 + a2 * v2, need = c2 + g + (TCR_BLOCKS - 1u - k) * (two ? c1 + g : c1 + c2 + g), rest, rt, c1t = T_TRUE(f1) + a1 * T_TRUE(v1);
    int32_t lo, lt;
    need = need > TCR_NEED_MAX ? TCR_NEED_MAX : need;
    rest = need > c2 ? need - c2 : 0u;
    lo = (int32_t)TCR_HALF_TICKS - (int32_t)(rest * TCR_BLOCK_TICKS / 10000u);
    rt = T_TRUE(g) + (TCR_BLOCKS - 1u - k) * (two ? c1t + T_TRUE(g) : c1t + T_TRUE(f2) + a2 * T_TRUE(v2) + T_TRUE(g));
    rt = rt > TCR_NEED_MAX ? TCR_NEED_MAX : rt;
    lt = (int32_t)TCR_HALF_TICKS - (int32_t)(rt * TCR_BLOCK_TICKS / 10000u) - (int32_t)TCR_SLACK_TICKS;
    *old = lo;
    lo = lt > lo ? lt : lo;
    return lo < (int32_t)TCR_HALF_TICKS ? lo : (int32_t)TCR_HALF_TICKS;
}
static uint64_t h2 = 1469598103934665603ull;
static uint64_t n_post, n_chg, n_chg_blk[4], n_chg_j[16], n_nott, n_nott_true, bad_same, bad_later, bad_inside, bad_true, bad_kept;
static int32_t chg_min = 0x7fffffff, chg_max;           /* how much later a changed deadline is (ticks) */
static void put2(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h2 ^= (v >> (8u * i)) & 0xFFu;
        h2 *= 1099511628211ull;
    }
}

static uint64_t h = 1469598103934665603ull;
static uint64_t n_val;
static void put(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h ^= (v >> (8u * i)) & 0xFFu;
        h *= 1099511628211ull;
    }
    n_val++;
}
static void boot(void) { ui_power_on(); multi_boot(); frame(); }
enum { V_OFF, V_M12, V_M24, V_TW, V_N };
static void clean(track_t *t, int va)
{
    uint32_t i;
    for (i = 0; i < NMSLOT; i++)
        t->p[MSLOT_ID(i)] = t->p[MSLOT_ID(i) + 1] = t->p[MSLOT_ID(i) + 2] = 0;
    t->p[P_TWTYPE] = TT_OFF, t->p[P_DRV2] = 0;
    for (i = 0; i < NX; i++)
        if (i != X_LINK && i != X_LFO1LVL && i != X_LFO2LVL)
            XV(t, i) = 0;
    t->p[P_KTRK] = 0, t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_ED_FLT] = t->p[P_L2FLT] = 0;
    t->p[P_VOICE] = V_POLY, t->p[P_AMODE] = 0;
    t->p[P_FREEA] = va == V_OFF ? UF_STEPS : 50, t->p[P_FREEC] = 0, t->p[P_FREED] = va == V_M24 ? 2 : 1;
    if (va == V_TW)
        t->p[P_TWTYPE] = TT_LP, t->p[P_CUT2] = 10;
}
/* CORE 1 (item 137, extended by item 142): both parts at their two-core caps at limit L (KEY SENSE +57 as on the unit): core 1's job k is posted at S + k P (S = 0.27 block, the polymax median
 * of item 139; P = the globals + the dearer of part 1 and the job, true cost) and runs part 2's true cost; its slack to the runner's wait limit of block k, points of a block */
#define LMIN 64u                                        /* the limits worked out: LMIN .. LMIN + NL - 1 (wave 14: BUDGET_LIMIT 72, was 80 .. 88 for 81 / 84) */
#define NL 25u
#define PKA ((double)BUDGET_PEAK)                      /* the peak allowance of the CORE 1 check, points of a block (wave 14: 18, was the 6 of items 137 / 142) */
static double c1_min[NL], c1_min13 = 1e9, c1_old_min = 1e9;   /* (c1_old: the same check against the 1.15 limit: what the change is for) */
static uint32_t c1_old_n6;
static uint32_t c1_n6[NL], c1_bad, c1_blk_min;
static double c1_blk[4] = {1e9, 1e9, 1e9, 1e9};
static char c1_where[160];
static uint32_t cap_at(const bd_part_t *r, uint32_t g, uint32_t L)   /* budget_solve2's two-core rule at limit L */
{
    uint32_t f = BD_U(r->ins + r->sat + r->eq + r->misc + r->base), c = BD_U(r->eng + r->filt + r->ext), k;
    for (k = r->cap; k > 1u; k--)
        if (g + f + k * c <= L * 100u)
            break;
    return k;
}
static void core1_slack(void)
{
    bd_t b;
    uint32_t n[2], L, k;
    bd_build(&b);
    b.fx += 57u;                                        /* (KEY SENSE: device builds) */
    if (!b.p[1].on)
        return;
    for (L = LMIN; L < LMIN + NL; L++) {
        uint32_t gu = BD_U(b.idle + b.fx);
        uint32_t f1 = BD_UDL(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), v1 = BD_UDL(b.p[0].eng + b.p[0].filt + b.p[0].ext);
        uint32_t f2 = BD_UDL(b.p[1].ins + b.p[1].sat + b.p[1].eq + b.p[1].misc + b.p[1].base), v2 = BD_UDL(b.p[1].eng + b.p[1].filt + b.p[1].ext), g = BD_UDL(b.idle + b.fx);
        uint32_t f1t = BD_U(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), v1t = BD_U(b.p[0].eng + b.p[0].filt + b.p[0].ext);
        uint32_t f2t = BD_U(b.p[1].ins + b.p[1].sat + b.p[1].eq + b.p[1].misc + b.p[1].base), v2t = BD_U(b.p[1].eng + b.p[1].filt + b.p[1].ext);
        uint32_t job, c1p, per;
        if (L == BUDGET_LIMIT)
            budget_solve2(&b, n, 1);                    /* (the firmware's own caps at its limit) */
        else
            n[0] = cap_at(&b.p[0], gu, L), n[1] = cap_at(&b.p[1], gu, L);
        if (!b.p[0].on)
            n[0] = 0, f1 = v1 = f1t = v1t = 0;
        job = f2t + n[1] * v2t;
        c1p = f1t + n[0] * v1t;
        per = gu + (c1p > job ? c1p : job);
        for (k = 0; k < TCR_BLOCKS; k++) {
            int32_t old, lim = lim_of(f1, v1, f2, v2, g, k, n[0], n[1], 1u, &old);
            double sl = (lim * 10000.0 / TCR_BLOCK_TICKS - (2700.0 + k * per + job)) / 100.0;   /* points of a block before the allowance */
            if (sl - PKA < c1_min[L - LMIN])
                c1_min[L - LMIN] = sl - PKA;
            c1_n6[L - LMIN] += sl < PKA;
            if (L != BUDGET_LIMIT)
                continue;
            {
                double so = (old * 10000.0 / TCR_BLOCK_TICKS - (2700.0 + k * per + job)) / 100.0;
                if (so - 6.0 < c1_old_min)
                    c1_old_min = so - 6.0;
                c1_old_n6 += so < 6.0;
            }
            if (sl - PKA < c1_blk[k])
                c1_blk[k] = sl - PKA;
            if (sl - 13.0 < c1_min13)
                c1_min13 = sl - 13.0;
            c1_bad += sl < PKA;
            if (sl - PKA <= c1_min[L - LMIN])
                snprintf(c1_where, sizeof c1_where, "mode %d, %s %u + %s %u notes, block %u", duell_mode_req, ENGINES[eng_idx(trk[0].eng_req)]->name, n[0],
                         ENGINES[eng_idx(trk[1].eng_req)]->name, n[1], k);
        }
    }
}
static void set_active(track_t *t, uint32_t n) { uint32_t i; for (i = 0; i < NVOICE; i++) t->v[i].active = i < n; }
static uint32_t setups;
static void sweep(void)                                 /* every rule, block and voice count of the present settings */
{
    static const uint8_t NA[] = {0, 1, 2, 3, 4, 6, 8};
    uint32_t two, k, a, b;
    int nofit;
    uint8_t keep[2][NVOICE];
    for (k = 0; k < NVOICE; k++)
        keep[0][k] = trk[0].v[k].active, keep[1][k] = trk[1].v[k].active;
    core1_slack();
    {   /* HANDOVER (b11, from DUELL 1.2): the items the main loop hands over (budget.c bd_dl_publish) are the ones the runner works out itself, and the runner takes them */
        uint32_t d1f, d1v, d2f, d2v, dg;
        int nf;
        part_dl(&trk[0], &d1f, &d1v), part_dl(&trk[1], &d2f, &d2v), dg = global_cost_dl();
        bd_dl_publish();
        tcr.blk = 0, tcr.cost_ok = 0, tcr.t0 = 1000000u;
        tcr.c1f = tcr.c1v = tcr.c2f = tcr.c2v = tcr.g = 0xFFFFFFFFu;
        (void)tcr_deadline(tcr.t0, &nf);
        bad_hand += !bd_dl_ok || tcr.c1f != d1f || tcr.c1v != d1v || tcr.c2f != d2f || tcr.c2v != d2v || tcr.g != dg;
        n_hand++;
        bd_dl_ok = 0;                                   /* (the rest of the sweep: the runner's own working out, as before; the hashes below are of that) */
    }
    for (two = 0; two < 2u; two++) {
        uint32_t e1f, e1v, e2f, e2v, eg;                /* item 137: the exact true items (BUDGET_MARGIN 100) */
        tc.up = 1, tc.failed = 0, tcr.paused = (uint8_t)!two;
        tcr.cost_ok = 0;
        tcr.t0 = 1000000u;
        e1f = part_fixed(&trk[0]), e1v = part_cost(&trk[0]), e2f = part_fixed(&trk[1]), e2v = part_cost(&trk[1]), eg = global_cost();
        for (k = 0; k < TCR_BLOCKS; k++)
            for (a = 0; a < NELEM(NA); a++)
                for (b = 0; b < NELEM(NA); b++) {
                    uint32_t dl, lim, j;
                    tcr.blk = (uint8_t)k;
                    set_active(&trk[0], NA[a]);
                    set_active(&trk[1], NA[b]);
                    dl = tcr_deadline(tcr.t0, &nofit);
                    {   /* item 142: the wait limit. DL_REF takes the 1.15 limit as worked out here from the firmware's items (unchanged = the items and the old formula unchanged) */
                        int32_t old, want = lim_of(tcr.c1f, tcr.c1v, tcr.c2f, tcr.c2v, tcr.g, k, NA[a], NA[b], tcr.two, &old), got = (int32_t)(tcr.lim - tcr.t0);
                        uint32_t xr;
                        put(tcr.c1f), put(tcr.c1v), put(tcr.c2f), put(tcr.c2v), put(tcr.g), put(tcr.two);
                        put(dl - tcr.t0), put((uint32_t)old), put((uint32_t)nofit);
                        put3((uint32_t)got);
                        n_lim++;
                        bad_wltrue += got != want;
                        bad_wllater += got < old || got > (int32_t)TCR_HALF_TICKS;
                        xr = eg + (TCR_BLOCKS - 1u - k) * (tcr.two ? e1f + NA[a] * e1v + eg : e1f + NA[a] * e1v + e2f + NA[b] * e2v + eg);   /* the EXACT true rest */
                        xr = xr > TCR_NEED_MAX ? TCR_NEED_MAX : xr;
                        if (got != old) {
                            n_lim_chg[k]++;
                            if (got - old > lim_chg_max)
                                lim_chg_max = got - old;
                            bad_wlsafe += got + (int32_t)(xr * TCR_BLOCK_TICKS / 10000u) > (int32_t)(TCR_HALF_TICKS - TCR_SLACK_TICKS);
                        }
                    }
                    lim = tcr.lim;
                    for (j = 0; j < 16u; j++) {         /* item 137: the deadline a job is posted with, at every quarter block of the half */
                        uint32_t now = tcr.t0 + j * (TCR_BLOCK_TICKS / 4u), pd, need, dt;
                        int nf2, past = (int32_t)(dl - now) <= 0;
                        pd = tcr_post_deadline(now, &nf2);
                        put2(pd - tcr.t0);
                        n_post++;
                        need = e2f + NA[b] * e2v + eg + (TCR_BLOCKS - 1u - k) * (e1f + NA[a] * e1v + eg);   /* (two-core rule) */
                        need = need > TCR_NEED_MAX ? TCR_NEED_MAX : need;
                        dt = tcr.t0 + TCR_HALF_TICKS - need * TCR_BLOCK_TICKS / 10000u - TCR_SLACK_TICKS;
                        bad_later += (int32_t)(pd - dl) < 0 || tcr.lim != lim;
                        if (!past || !tcr.two) {
                            bad_same += pd != dl;
                            continue;
                        }
                        n_nott++;
                        if (pd == dl) {
                            bad_kept += (int32_t)(dt - now) > (int32_t)(TCR_BLOCK_TICKS / 100u) && (int32_t)(dt - lim) < 0;
                            continue;
                        }
                        n_nott_true++, n_chg++, n_chg_blk[k]++, n_chg_j[j]++;
                        bad_inside += !((int32_t)(pd - now) > 0 && (int32_t)(pd - lim) < 0);
                        bad_true += (int32_t)(pd - dt) > 0 || (int32_t)(dt - pd) > (int32_t)(TCR_BLOCK_TICKS / 100u);
                        if ((int32_t)(pd - dl) < chg_min) chg_min = (int32_t)(pd - dl);
                        if ((int32_t)(pd - dl) > chg_max) chg_max = (int32_t)(pd - dl);
                    }
                }
    }
    for (k = 0; k < NVOICE; k++)
        trk[0].v[k].active = keep[0][k], trk[1].v[k].active = keep[1][k];
    tcr.paused = 0, tc.up = 0, tcr.cost_ok = 0;
    setups++;
}
/* A01 (audit 2026-11-04): TIDE's audio DSP is in the model (budget.c BD_TIDE_*: an estimate from the host's time ratios, until the unit measures it). A scene with all four stages: a factory
 * multi with FM6 on a part (the bridge), a live MORPH on both parts (the valley and the voices' dip), WASH MID (the wash), both parts at their two-core caps with the TIDE items in. Checks, per
 * mode (LAYER, DUEL): the items are in the deadline's figures (part_dl: WASH adds exactly BD_UDL(BD_TIDE_WASH) to a part's fixed cost, FM6 pays BD_TIDE_FM6 a voice more than the dip alone);
 * the wait limit with WASH MID is not later than with WASH OFF, and the CORE 1 check of the caps (core 1's job posted 0.27 block + k periods, +6 points) still ends before the limit. The hashes
 * above are not touched by this scene (they are saved and put back); a failure here adds to c1_bad. */
static double tide_slack = 1e9;
static uint32_t tide_scenes, tide_bad;
static void tide_scene(void)
{
    uint64_t sh = h, sh2 = h2, sh3 = h3, snv = n_val;
    uint32_t i, m, k, found = 0;
    for (i = 0; i < UP_VSLOTS && tide_scenes < 8u; i++)
        for (m = 0; m < 2u; m++) {
            uint32_t p, w0f[2], w0v[2], w2f[2], w2v[2], wl0, wl2, d0, d2, spin;
            int nofit;
            if (i >= UP_USER0 && i < UP_BANK)
                continue;
            boot();
            duell_mode_force(m ? DM_SPLIT : DM_LAYER);
            pc_recall(0, i, 0);                       /* part 1: factory sound i (its MORPH End B rolled by the load) */
            if (ENGINES[eng_idx(trk[0].eng_req)] != &ENG_FM6)
                continue;
            pc_recall(1, 0, 0);                       /* part 2: #01 BEHIVE, ANALOG */
            for (spin = 0; spin < 40u; spin++) {
                int32_t o[2 * CTL];
                mix_block(o, CTL);
                frame();
            }
            if (!(mph[0].nid || mph[0].fnid) || !(mph[1].nid || mph[1].fnid) || morph_stale(&trk[0], &mph[0]) || morph_stale(&trk[1], &mph[1]) || trk[0].p[P_MUTE] || trk[1].p[P_MUTE])
                continue;
            bd_mv[0] = bd_mv[1] = 1;                  /* both faders are moving: with FELUCCA_MOVECOST (the shipped build; b10 round 6) the valley and the wash are charged only while a transition is under way
                                                       * (budget.c bd_where; at rest only the wash's meter is), and this scene is about their full figures. Without the switch they are always charged: no change there. */
            trk[1].p[P_FREEC] = 40;                   /* part 2's filter MODE is no low-pass (a band-pass patch): its voices' filter cannot carry the dip, the valley after them does */
            {   /* all four stages: the valley is possible on a part too */
                bd_part_t r0, r1;
                morph_wlvl = 2;
                bd_part(&trk[0], &r0);
                bd_part(&trk[1], &r1);
                morph_wlvl = 0;
                if (r0.tide < BD_TIDE_VALLEY + BD_TIDE_WASH && r1.tide < BD_TIDE_VALLEY + BD_TIDE_WASH)
                    continue;
            }
            found++;
            tide_scenes++;
            tc.up = 1, tc.failed = 0, tcr.paused = 0, tcr.cost_ok = 0, tcr.t0 = 1000000u, tcr.blk = 0;
            set_active(&trk[0], 4u);
            set_active(&trk[1], 4u);
            morph_wlvl = 0;
            for (p = 0; p < 2u; p++)
                part_dl(&trk[p], &w0f[p], &w0v[p]);
            d0 = tcr_deadline(tcr.t0, &nofit);
            wl0 = tcr.lim;
            morph_wlvl = 2;
            for (p = 0; p < 2u; p++)
                part_dl(&trk[p], &w2f[p], &w2v[p]);
            d2 = tcr_deadline(tcr.t0, &nofit);
            wl2 = tcr.lim;
            for (p = 0; p < 2u; p++) {
                bd_part_t r;
                bd_part(&trk[p], &r);
                tide_bad += w2f[p] + 1u < w0f[p] + BD_UDL(BD_TIDE_WASH) || w2f[p] > w0f[p] + BD_UDL(BD_TIDE_WASH) + 1u || w2v[p] != w0v[p];   /* (WASH MID adds the wash to the fixed cost, rounding aside) */
                tide_bad += r.misc < r.tide || r.tide < BD_TIDE_WASH || r.ext < BD_MORPH + BD_TIDE_DIP + (ENGINES[eng_idx(trk[p].eng_req)] == &ENG_FM6 ? BD_TIDE_FM6_IDLE : 0u);   /* (the bridge's full charge only while a glide / the fader moves: budget.c bd_mv) */
            }
            tide_bad += (int32_t)(wl2 - wl0) > 0 || (int32_t)(d2 - d0) > 0;     /* the dearer scene never gets a later deadline or wait limit */
            {   /* the CORE 1 check of these caps (core1_slack at BUDGET_LIMIT, WASH MID in the items) */
                double keep = c1_min[BUDGET_LIMIT - LMIN];
                uint32_t bad0 = c1_bad;
                c1_min[BUDGET_LIMIT - LMIN] = 1e9;
                core1_slack();
                if (c1_min[BUDGET_LIMIT - LMIN] < tide_slack)
                    tide_slack = c1_min[BUDGET_LIMIT - LMIN];
                tide_bad += c1_bad != bad0;
                if (keep < c1_min[BUDGET_LIMIT - LMIN])
                    c1_min[BUDGET_LIMIT - LMIN] = keep;
            }
            morph_wlvl = 0;
            tc.up = 0, tcr.cost_ok = 0;
            bd_mv[0] = bd_mv[1] = 0;
        }
    h = sh, h2 = sh2, h3 = sh3, n_val = snv;
    tide_bad += !found;
}
int main(void)
{
    static const int MODES[3] = {DM_LAYER, DM_STEREO, DM_SPLIT};
    uint32_t m, s, f, e1, e2, v1, v2, k, i;
    for (i = 0; i < NL; i++)
        c1_min[i] = 1e9;
    for (m = 0; m < 3u; m++)
        for (s = 0; s < 4u; s++)                        /* SENDS on/off x PART FX on/off */
            for (e1 = 0; e1 < NENG_SHOWN; e1++) {
                boot();
                duell_mode_force(MODES[m]);
                for (e2 = 0; e2 < NENG_SHOWN; e2++)
                    for (v1 = 0; v1 < V_N; v1++)
                        for (v2 = 0; v2 < V_N; v2++) {
                            uint32_t ve[2] = {v1, v2}, en[2] = {ENGINE_ORDER[e1], ENGINE_ORDER[e2]};
                            for (k = 0; k < 2u; k++) {
                                trk[k].eng_req = (uint8_t)en[k];
                                apply_preset_to(&trk[k], 0);
                                clean(&trk[k], (int)ve[k]);
                                mph[k].nid = mph[k].fnid = 0;
                            }
                            mods_off = (s & 1u ? MODS_SENDS : 0u) | (s & 2u ? MODS_PFX : 0u);
                            sweep();
                        }
                mods_off = 0;
            }
    for (m = 0; m < 2u; m++)                            /* every factory sound as stored (inserts, SAT, EQ, matrix, SUB, TWIN, morph-free), both parts: the bank's 128 (the references are theirs;
                                                         * tools/duell_sounds.py N_BASE: the sounds added after them, indices from DL_BANK_N up, are not part of "the same as before") */
        for (e1 = 0; e1 < NENG_SHOWN; e1++) {
            static const uint8_t DL_BANK_N[16] = {20, 0, 11, 9, 0, 8, 13, 7, 0, 12, 0, 11, 14, 0, 23, 0};
            uint32_t en = ENGINE_ORDER[e1], n = ds_count(en) < DL_BANK_N[en] ? ds_count(en) : DL_BANK_N[en];
            boot();
            duell_mode_force(m ? DM_SPLIT : DM_LAYER);
            for (f = 0; f < n; f++) {
                for (k = 0; k < 2u; k++) {
                    trk[k].eng_req = (uint8_t)en;
                    apply_preset_to(&trk[k], f);
                    mph[k].nid = mph[k].fnid = 0;
                }
                sweep();
            }
        }
    for (i = 1; i <= 16u; i++) {                        /* the 16 factory multis as loaded (MODE RECALL), morphs included (slots 1..16: M02..M15, the STRESS TEST scene, and since TIDE b6 slot 16 = M17 AMB DUNEHORN; until b6 slot 16 wrapped to M01) */
        boot();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        if (i == 15u)
            scene_load(15, "M16_STRESS_TEST");          /* (U2 B: the STRESS TEST, now a test scene, as these hashes had it; CELLAR TAPE is the factory M16) */
        else
            project_load(MIDX((int)i));
        frame();
        sweep();
    }
    tide_scene();
    {   /* b11: a load clears the hand-over (ui.c load_begin, project.c project_restore_runtime): the runner works the items out again until budget_poll's next publish.
         * And the host's time for the block-0 items, worked out (the old way, in the audio interrupt) against read from the copy (an equivalent of the unit's st_budget_us) */
        struct timespec t0, t1;
        double us_calc, us_copy;
        uint32_t r, c1f, c1v, c2f, c2v, g, sum = 0;
        boot();
        project_load(MIDX(6));                          /* (slot 6, HSE RHODES: the sweep's worst multi) */
        budget_poll();                                  /* (the main loop's pass: main.c; the tests' frame() does not run it, so every other host test keeps the runner's own working out) */
        n_lclear = bd_dl_ok;
        load_begin(&trk[0], UNDO_SOUND), load_end(&trk[0]);
        n_lclear += !bd_dl_ok;
        bd_dl_publish();
        project_load(MIDX(2));
        n_lclear += !bd_dl_ok;
        budget_poll();
        n_lclear += bd_dl_ok;
        {   /* AGE: a copy BD_DL_AGE_MS old is not used (the main loop stalled): the copy's items spoiled, the runner reads them 1 ms before the limit and works its own at the limit */
            uint32_t w1f, w1v, w2f, w2v, wg, ms0 = fm1_ms;
            int nf;
            bd_dl_t *d = &bd_dl_buf[bd_dl_i];
            part_dl(&trk[0], &w1f, &w1v), part_dl(&trk[1], &w2f, &w2v), wg = global_cost_dl();
            d->c1f = d->c1v = d->c2f = d->c2v = d->g = 0x7777u;
            fm1_ms = ms0 + BD_DL_AGE_MS - 1u;
            tcr.blk = 0, tcr.cost_ok = 0, tcr.t0 = 1000000u;
            (void)tcr_deadline(tcr.t0, &nf);
            n_lclear += tcr.c1f == 0x7777u && tcr.g == 0x7777u;
            fm1_ms = ms0 + BD_DL_AGE_MS;
            tcr.blk = 0, tcr.cost_ok = 0;
            (void)tcr_deadline(tcr.t0, &nf);
            n_lclear += tcr.c1f == w1f && tcr.c1v == w1v && tcr.c2f == w2f && tcr.c2v == w2v && tcr.g == wg;
            fm1_ms = ms0;
            bd_dl_publish();
        }
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (r = 0; r < 20000u; r++) {
            part_dl(&trk[0], &c1f, &c1v), part_dl(&trk[1], &c2f, &c2v), g = global_cost_dl();
            sum += c1f + c1v + c2f + c2v + g;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        us_calc = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 1e3 / 20000.0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (r = 0; r < 20000u; r++) {
            const bd_dl_t *d = &bd_dl_buf[bd_dl_i];
            c1f = d->c1f, c1v = d->c1v, c2f = d->c2f, c2v = d->c2v, g = d->g;
            sum += c1f + c1v + c2f + c2v + g;
            __asm__ volatile("" ::: "memory");
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        us_copy = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 1e3 / 20000.0;
        printf("deadline:   block-0 items on the host (multi 2 loaded): worked out %.3f us, read from the copy %.4f us (%u)\n", us_calc, us_copy, sum & 1u);
    }
    printf("deadline: %s HANDOVER: the main loop's items (budget.c bd_dl_publish) are the runner's own, and the runner reads them (%llu setups, %llu wrong)\n", bad_hand || !n_hand ? "FAIL" : "ok  ",
           (unsigned long long)n_hand, (unsigned long long)bad_hand);
    printf("deadline: %s LOAD: budget_poll publishes, a sound load and a multi load clear it, the next budget_poll publishes again; a copy BD_DL_AGE_MS old is not used (%u of 6)\n", n_lclear == 6u ? "ok  " : "FAIL", n_lclear);
    printf("deadline: %u setups, %llu values, hash 0x%016llx (reference 0x%016llx)\n", setups, (unsigned long long)n_val, (unsigned long long)h, (unsigned long long)DL_REF);
    printf("deadline: %s the runner's deadline, wait limit and cost items are the same as before item 132 for every setup\n", h == DL_REF ? "ok  " : "FAIL");
    printf("deadline: item 137, posted deadlines: %llu (setup x rule x block x voices x 16 post moments); 1.15 deadline past under the two-core rule: %llu, of them given a take-over deadline: %llu\n",
           (unsigned long long)n_post, (unsigned long long)n_nott, (unsigned long long)n_nott_true);
    printf("deadline:   changed by block 0..3: %llu %llu %llu %llu; by post moment (quarter blocks 0..15):", (unsigned long long)n_chg_blk[0], (unsigned long long)n_chg_blk[1],
           (unsigned long long)n_chg_blk[2], (unsigned long long)n_chg_blk[3]);
    for (i = 0; i < 16u; i++)
        printf(" %llu", (unsigned long long)n_chg_j[i]);
    printf("\ndeadline:   a changed deadline is %d .. %d ticks (%.3f .. %.3f blocks) later than the 1.15 one\n", n_chg ? chg_min : 0, chg_max, n_chg ? chg_min / (double)TCR_BLOCK_TICKS : 0.0,
           chg_max / (double)TCR_BLOCK_TICKS);
    printf("deadline:   posted-deadline hash 0x%016llx (DL_REF2 0x%016llx)\n", (unsigned long long)h2, (unsigned long long)DL_REF2);
    printf("deadline: %s SAME: the posted deadline is the 1.15 one wherever that is ahead of the post or the one-core rule holds (%llu wrong)\n", bad_same ? "FAIL" : "ok  ", (unsigned long long)bad_same);
    printf("deadline: %s LATER: never earlier than the 1.15 deadline, the wait limit never moved (%llu wrong)\n", bad_later ? "FAIL" : "ok  ", (unsigned long long)bad_later);
    printf("deadline: %s INSIDE: a changed deadline is after the post and before the wait limit (%llu wrong)\n", bad_inside ? "FAIL" : "ok  ", (unsigned long long)bad_inside);
    printf("deadline: %s TRUE: a changed deadline is not later than the exact 1.00 deadline and within 1 %% of a block of it (%llu wrong)\n", bad_true ? "FAIL" : "ok  ", (unsigned long long)bad_true);
    printf("deadline: %s KEPT: where the exact true deadline is ahead of the post (by > 1 %% of a block) and before the wait limit, a take-over deadline exists (%llu missing)\n",
           bad_kept ? "FAIL" : "ok  ", (unsigned long long)bad_kept);
    printf("deadline: %s the posted-deadline hash equals DL_REF2\n", h2 == DL_REF2 ? "ok  " : "FAIL");
    printf("deadline: item 142, the wait limit: %llu limits; moved later (by block 0..3): %llu %llu %llu %llu, at most %d ticks (%.3f block); new-limit hash 0x%016llx (DL_REF3 0x%016llx)\n",
           (unsigned long long)n_lim, (unsigned long long)n_lim_chg[0], (unsigned long long)n_lim_chg[1], (unsigned long long)n_lim_chg[2], (unsigned long long)n_lim_chg[3], lim_chg_max,
           lim_chg_max / (double)TCR_BLOCK_TICKS, (unsigned long long)h3, (unsigned long long)DL_REF3);
    printf("deadline: %s WL-TRUE: the wait limit is max(the 1.15 limit, t0 + H - ceil-true rest - TCR_SLACK), never after t0 + H (%llu wrong)\n", bad_wltrue ? "FAIL" : "ok  ", (unsigned long long)bad_wltrue);
    printf("deadline: %s WL-LATER: never earlier than the 1.15 limit, never after the half's end (%llu wrong)\n", bad_wllater ? "FAIL" : "ok  ", (unsigned long long)bad_wllater);
    printf("deadline: %s WL-SAFE: a moved limit + the exact true rest (1.00 items) <= t0 + H - TCR_SLACK (%llu wrong)\n", bad_wlsafe ? "FAIL" : "ok  ", (unsigned long long)bad_wlsafe);
    printf("deadline: %s the new-limit hash equals DL_REF3\n", h3 == DL_REF3 ? "ok  " : "FAIL");
    printf("deadline:   CORE 1 by BUDGET_LIMIT (caps worked out at each limit, least slack after BUDGET_PEAK points / setup x blocks under BUDGET_PEAK):");
    for (i = 0; i < NL; i++)
        printf(" %u: %.1f/%u", LMIN + i, c1_min[i], c1_n6[i]);
    printf("\n");
    printf("deadline: %s CORE 1 at the caps of BUDGET_LIMIT %u / margin %u %%: every block's job (posted 0.27 block + k periods) + BUDGET_PEAK points ends before the wait limit (least slack after BUDGET_PEAK points %.1f: %s; "
           "by block 0..3: %.1f %.1f %.1f %.1f; after 13 points %.1f; %u under BUDGET_PEAK)\n",
           c1_bad ? "FAIL" : "ok  ", BUDGET_LIMIT, BUDGET_MARGIN, c1_min[BUDGET_LIMIT - LMIN], c1_where, c1_blk[0], c1_blk[1], c1_blk[2], c1_blk[3], c1_min13, c1_bad);
    printf("deadline:   (the same caps against the 1.15 wait limit of before item 142: least slack after 6 points %.1f, %u setup x blocks under 6)\n", c1_old_min, c1_old_n6);
    printf("deadline: %s TIDE (A01, an estimate until the unit measures it): %u scenes (an FM6 factory sound on part 1, BEHIVE with a band-pass filter MODE on part 2, LAYER and DUEL) with FM6 (bridge), a live MORPH on both parts (valley, dip) and WASH MID: the TIDE items are in the figures, the dearer scene never gets a later deadline, and core 1's job at the two-core caps ends before the wait limit (least slack after 6 points %.1f)\n",
           tide_bad ? "FAIL" : "ok  ", tide_scenes, tide_slack);
    printf("deadline: %s CORE 0: BUDGET_LIMIT %u + BUDGET_PEAK %u points of peak = 90 %%\n", BUDGET_LIMIT + BUDGET_PEAK == 90u ? "ok  " : "FAIL", BUDGET_LIMIT, BUDGET_PEAK);
    return tide_bad || bad_hand || !n_hand || n_lclear != 6u || h != DL_REF || bad_same || bad_later || bad_inside || bad_true || bad_kept || h2 != DL_REF2 || c1_bad || bad_wltrue || bad_wllater || bad_wlsafe || h3 != DL_REF3 ||
           BUDGET_LIMIT + BUDGET_PEAK != 90u || BUDGET_LIMIT < LMIN || BUDGET_LIMIT >= LMIN + NL;
}
