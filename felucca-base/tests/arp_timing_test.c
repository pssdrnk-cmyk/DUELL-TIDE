/* SPDX-License-Identifier: GPL-3.0-only */
/* The internal-clock arp against the sequencer (lane S of the 2026-11-04 audit, S01 / S02 / S11).
 *   DRIFT  64 bars at 120 BPM, the arp (1/16, one key) next to the sequencer (16 steps of 1/16): every arp step starts at
 *          the sum of the step lengths, within 1 sample, and keeps a constant distance from the sequencer's grid (an arp
 *          step used to round up to a whole block and throw the overshoot away: 8 steps took 8.04 sixteenths)
 *   SWING  ARP SWG 75: the offbeat of a pair lands after the middle (it used to land at 30 %); SWG 50 stays straight
 *   PROB   ARP PROB 0 never plays a step (it played 1 in 128), PROB 127 plays every step
 * Built and run by host_tests.sh (name: arptiming): the firmware sources against ui_test.c's stubs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static int expect(const char *what, int ok)
{
    printf("arptiming: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

static uint32_t now_s;                              /* samples since the start */
static void blk(void) { events_block(CTL); now_s += CTL; }

static void fresh(void)
{
    uint32_t k, i;
    ui_power_on();
    memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um);
    mi_r = mi_w = 0;
    midi_beat_samples = 0;
    fm1_ms = 0;
    now_s = 0;
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = 120;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        set_engine_of(t, 0);
        apply_preset_to(t, 0);
        t->engine = t->eng_req;
        memset(t->step, 0, sizeof t->step);
        for (i = 0; i < NSTEP; i++)
            t->step[i].time = ST_REST;
        t->p[P_VOICE] = V_POLY;
        t->p[P_CHRD] = 0;
        t->p[P_AMODE] = 0;
    }
    blk();
}

#define BARS 64u
#define NSIX (BARS * 16u)                           /* 16ths in 64 bars */

/* one block at a time: the exact sample an arp step / a sequencer step started at (the block's end minus the overshoot
 * the step carries; an unfixed arp throws it away and reports the block's end) */
static uint32_t arp_at[NSIX + 8], seq_at[NSIX + 8];
static uint32_t n_arp, n_seq;

static void run_both(void)
{
    track_t *t = &trk[0];
    uint32_t la, ls, i;
    fresh();
    for (i = 0; i < 16; i++) {
        t->step[i].time = ST_NOTE;
        t->step[i].n = 1;
        t->step[i].note[0] = 60;
    }
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SSWING] = 50;
    t->p[P_AMODE] = AM_UP;
    t->p[P_ARATE] = 2;
    t->p[P_AOCT] = 1;
    t->p[P_ASWING] = 50;
    t->p[P_APROB] = 127;
    t->p[P_AHOLD] = 0;
    transport_req = 1;
    input_on(t, 60, 100);
    la = t->arp_idx;
    ls = 0xFFFFu;
    n_arp = n_seq = 0;
    while (n_seq < NSIX && n_arp < NSIX) {
        blk();
        if (t->arp_idx != la) {
            la = t->arp_idx;
            arp_at[n_arp++] = now_s - (t->arp_pos < CTL ? t->arp_pos : 0u);
        }
        if (t->seq_idx != ls) {
            ls = t->seq_idx;
            seq_at[n_seq++] = now_s - (t->seq_pos < CTL ? t->seq_pos : 0u);
        }
    }
}

static void test_drift(void)
{
    uint32_t P, k, n;
    int64_t maxd = 0, maxs = 0, dmin, dmax;
    run_both();
    n = n_arp < n_seq ? n_arp : n_seq;
    P = div_samples(2);
    dmin = dmax = (int64_t)arp_at[0] - (int64_t)seq_at[0];
    for (k = 1; k < n; k++) {
        int64_t ea = (int64_t)arp_at[k] - (int64_t)arp_at[0] - (int64_t)k * P;
        int64_t es = (int64_t)seq_at[k] - (int64_t)seq_at[0] - (int64_t)k * P;
        int64_t d = (int64_t)arp_at[k] - (int64_t)seq_at[k];
        if ((ea < 0 ? -ea : ea) > maxd) maxd = ea < 0 ? -ea : ea;
        if ((es < 0 ? -es : es) > maxs) maxs = es < 0 ? -es : es;
        if (d < dmin) dmin = d;
        if (d > dmax) dmax = d;
    }
    printf("arptiming: %u arp steps, %u sequencer steps, 1/16 = %u samples; arp off its own grid by at most %lld, "
           "sequencer by %lld, arp-to-sequencer distance %lld .. %lld samples\n",
           n_arp, n_seq, P, (long long)maxd, (long long)maxs, (long long)dmin, (long long)dmax);
    expect("64 bars at 120 BPM: the arp has as many steps as the sequencer (1024)", n >= NSIX - 1u);
    expect("every arp step is on its grid within 1 sample (no overshoot thrown away)", maxd <= 1);
    expect("the sequencer's own grid is exact within 1 sample (the reference)", maxs <= 1);
    expect("the arp keeps its distance from the sequencer's grid within 1 sample over 64 bars", dmax - dmin <= 1);
}

/* the first 9 onsets of an UP arp over three keys with ARP SWG pct: where the offbeat lands in the pair, in % */
static double offbeat(int pct, double *whole)
{
    track_t *t = &trk[0];
    uint32_t at[12], n = 0, last;
    fresh();
    t->p[P_AMODE] = AM_UP;
    t->p[P_ARATE] = 2;
    t->p[P_AOCT] = 1;
    t->p[P_ASWING] = (int16_t)pct;
    t->p[P_APROB] = 127;
    t->p[P_AHOLD] = 0;
    input_on(t, 60, 100);
    input_on(t, 64, 100);
    input_on(t, 67, 100);
    last = t->arp_idx;
    while (n < 9 && now_s < FS * 8u) {
        blk();
        if (t->arp_idx != last) {
            last = t->arp_idx;
            at[n++] = now_s - (t->arp_pos < CTL ? t->arp_pos : 0u);
        }
    }
    *whole = (double)(at[8] - at[0]) / (8.0 * div_samples(2));
    return 100.0 * (double)(at[1] - at[0]) / (double)(at[2] - at[0]);
}

static void test_swing(void)
{
    double w, p50 = offbeat(50, &w), p62, p75;
    expect("ARP SWG 50: the offbeat is in the middle of the pair (50 %)", p50 > 49.0 && p50 < 51.0);
    p62 = offbeat(62, &w);
    p75 = offbeat(75, &w);
    printf("arptiming: ARP SWG 50 / 62 / 75: the offbeat lands at %.1f %% / %.1f %% / %.1f %% of the pair\n", p50, p62, p75);
    expect("ARP SWG 62: the offbeat lands at 62 % of the pair (audit S03)", p62 > 61.0 && p62 < 63.0);
    expect("ARP SWG 75: the offbeat lands at 75 % of the pair (audit S03; it was 70)", p75 > 74.0 && p75 < 76.0);
    expect("ARP SWG 75: the steps still add up (8 steps = 4 pairs of 2 sixteenths)", w > 0.998 && w < 1.002);
}

/* steps played by an UP arp in `n` steps at ARP PROB p: an arp note sounding right after its step */
static uint32_t played(int p, uint32_t n)
{
    track_t *t = &trk[0];
    uint32_t last, steps = 0, hit = 0;
    fresh();
    song.g[G_BPM] = 240;
    t->p[P_AMODE] = AM_UP;
    t->p[P_ARATE] = 3;
    t->p[P_AOCT] = 1;
    t->p[P_ASWING] = 50;
    t->p[P_APROB] = (int16_t)p;
    t->p[P_AGATE] = 64;
    t->p[P_AHOLD] = 0;
    input_on(t, 60, 100);
    last = t->arp_idx;
    while (steps < n) {
        blk();
        if (t->arp_idx != last) {
            last = t->arp_idx;
            steps++;
            hit += ARP_UP(t) ? 1u : 0u;
        }
    }
    return hit;
}

static void test_prob(void)
{
    uint32_t p0 = played(0, 3000), p127 = played(127, 3000), p64 = played(64, 3000);
    printf("arptiming: of 3000 steps ARP PROB 0 plays %u, 64 plays %u, 127 plays %u\n", p0, p64, p127);
    expect("ARP PROB 0 never plays a step", p0 == 0);
    expect("ARP PROB 127 plays every step", p127 == 3000u);
    expect("ARP PROB 64 plays about half of the steps", p64 > 1300u && p64 < 1700u);
}

/* S06: the arp plays the velocity of the key (the newest key for a chord), an ACCENT step 127; it used to be 100 */
static void test_velocity(void)
{
    track_t *t = &trk[0];
    uint32_t up, i, v_up = 0, v_ch = 0, v_sq = 0, v_acc = 0;
    fresh();
    t->p[P_AMODE] = AM_UP; t->p[P_ARATE] = 2; t->p[P_AOCT] = 1; t->p[P_AHOLD] = 0;
    input_on(t, 60, 20);
    for (i = 0; i < 400; i++) {
        blk();
        v_up = t->m_vel;
    }
    expect("ARP UP on a key at velocity 20 plays velocity 20", v_up == 20u);
    input_on(t, 64, 90);
    for (i = 0; i < 400; i++) {
        blk();
        v_up = t->m_vel;
    }
    expect("with a second key (velocity 90) the arp plays the newest key's velocity", v_up == 90u);
    input_off(t, 60); input_off(t, 64);
    fresh();
    t->p[P_AMODE] = AM_CHORD; t->p[P_ARATE] = 2; t->p[P_AOCT] = 2; t->p[P_AHOLD] = 0;
    input_on(t, 60, 40); input_on(t, 67, 70);
    for (i = 0; i < 400; i++) {
        blk();
        v_ch = t->m_vel;
    }
    expect("ARP CHORD plays the newest key's velocity (70)", v_ch == 70u);
    fresh();
    t->p[P_AMODE] = AM_SEQ; t->p[P_ARATE] = 2; t->p[P_AOCT] = 1; t->p[P_AHOLD] = 0; t->p[P_SLEN] = 2;
    t->step[0].time = ST_NOTE; t->step[0].n = 1; t->step[0].note[0] = 60;
    t->step[1].time = ST_NOTE; t->step[1].n = 1; t->step[1].note[0] = 60; t->step[1].flags = SF_ACCENT;
    input_on(t, 60, 55);
    up = t->arp_idx;
    for (i = 0; i < 200; i++) {
        blk();
        if (t->arp_idx != up && ARP_UP(t)) {
            up = t->arp_idx;
            if (t->step[up % 2u].flags & SF_ACCENT) v_acc = t->m_vel; else v_sq = t->m_vel;
        }
    }
    printf("arptiming: ARP SEQ: plain step velocity %u, accent step %u\n", v_sq, v_acc);
    expect("ARP SEQ plays the key's velocity (55), an ACCENT step 127", v_sq == 55u && v_acc == 127u);
}

int main(void)
{
    test_drift();
    test_swing();
    test_prob();
    test_velocity();
    printf("arptiming: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
