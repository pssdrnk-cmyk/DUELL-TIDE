/* SPDX-License-Identifier: GPL-3.0-only */
/* The internal-clock arp against the sequencer (lane S of the 2026-11-04 audit, S01 / S02 / S11).
 *   DRIFT  64 bars at 120 BPM, the arp (1/16, one key) next to the sequencer (16 steps of 1/16): every arp step starts at
 *          the sum of the step lengths, within 1 sample, and keeps a constant distance from the sequencer's grid (an arp
 *          step used to round up to a whole block and throw the overshoot away: 8 steps took 8.04 sixteenths)
 *   SWING  ARP SWG 75: the offbeat of a pair lands after the middle (it used to land at 30 %); SWG 50 stays straight
 *   START  a key that lands while PLAY runs starts the arp on the next ARP RATE boundary of the transport (audit S07; it started in the block the key landed)
 *   PROB   ARP PROB 0 never plays a step (it played 1 in 128), PROB 127 plays every step
 *   TIE    ARP SEQ holds a note through the TIE steps after it (audit G07: the chord / pad patterns P01, P15, P17)
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
static int off_rate = 2;                                  /* (the ARP RATE it measures: N_DIV index) */
static double offbeat(int pct, double *whole)
{
    track_t *t = &trk[0];
    uint32_t at[12], n = 0, last;
    fresh();
    t->p[P_AMODE] = AM_UP;
    t->p[P_ARATE] = (int16_t)off_rate;
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
    *whole = (double)(at[8] - at[0]) / (8.0 * div_samples((uint32_t)off_rate));
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
    {   /* S14: a triplet RATE (8T, 16T, 4T) is not swung; the neighbouring straight ones (1/8, 1/16) still are */
        static const int R[5] = {4, 5, 10, 0, 2};
        uint32_t i;
        double q[5];
        for (i = 0; i < 5u; i++) {
            off_rate = R[i];
            q[i] = offbeat(75, &w);
        }
        off_rate = 2;
        printf("arptiming: ARP SWG 75 at 8T / 16T / 4T / 1/4 / 1/16: the offbeat lands at %.1f / %.1f / %.1f / %.1f / %.1f %%\n", q[0], q[1], q[2], q[3], q[4]);
        expect("ARP SWG 75 on a triplet RATE (8T, 16T, 4T): straight, the offbeat at 50 % (audit S14; it was 75)",
               q[0] > 49.0 && q[0] < 51.0 && q[1] > 49.0 && q[1] < 51.0 && q[2] > 49.0 && q[2] < 51.0);
        expect("ARP SWG 75 on 1/4 and 1/16: still swung to 75 %", q[3] > 74.0 && q[3] < 76.0 && q[4] > 74.0 && q[4] < 76.0);
    }
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

/* S07: a key that lands while PLAY runs starts the arp on the next boundary of the ARP RATE on the transport (1/8, 1/16, 1/32); stopped it starts at once */
static void test_start(void)
{
    static const uint32_t rate[3] = {1, 2, 3};
    track_t *t = &trk[0];
    uint32_t r, i, k, seq0 = 0, last, key_at, at = 0;   /* (i: the next note) */
    char what[120];
    for (r = 0; r < 3u; r++) {
        uint32_t P;
        fresh();
        for (i = 0; i < 16; i++) {
            t->step[i].time = ST_NOTE;
            t->step[i].n = 1;
            t->step[i].note[0] = 60;
        }
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_SSWING] = 50;
        t->p[P_AMODE] = AM_UP; t->p[P_ARATE] = (int16_t)rate[r]; t->p[P_AOCT] = 1; t->p[P_ASWING] = 50; t->p[P_APROB] = 127; t->p[P_AHOLD] = 0;
        P = div_samples(rate[r]);
        transport_req = 1;
        last = 0xFFFFu;
        for (k = 0; k < 1000u + 7u * r; k++) {          /* a key lands part way through a step, a different part for each rate */
            blk();
            if (t->seq_idx != last && last == 0xFFFFu)
                seq0 = now_s - (t->seq_pos < CTL ? t->seq_pos : 0u);
            last = t->seq_idx;
        }
        input_on(t, 60, 100);
        key_at = now_s;
        last = t->arp_idx;
        for (k = 0; k < 1500u && t->arp_idx == last; k++)
            blk();
        at = now_s - (t->arp_pos < CTL ? t->arp_pos : 0u);
        snprintf(what, sizeof what, "PLAY running, ARP RATE %u: the first arp note lands on the transport's grid (%u samples after its step, key %u in)",
                 rate[r], (at - seq0) % P, (key_at - seq0) % P);
        expect(what, t->arp_idx != last && ((at - seq0) % P <= 2u || (at - seq0) % P >= P - 2u) && at > key_at);
        last = t->arp_idx;
        for (k = 0; k < 1500u && t->arp_idx == last; k++)
            blk();
        i = now_s - (t->arp_pos < CTL ? t->arp_pos : 0u);
        expect("  .. and the next one a full RATE later", t->arp_idx != last && i - at + 2u >= P && i - at <= P + 2u);
    }
    fresh();
    t->p[P_AMODE] = AM_UP; t->p[P_ARATE] = 2; t->p[P_AOCT] = 1; t->p[P_AHOLD] = 0;
    input_on(t, 60, 100);
    last = t->arp_idx;
    blk();
    expect("PLAY stopped: the first arp note sounds in the block the key lands", t->arp_idx != last);
}

/* G07: ARP SEQ honours TIE. The tied steps of a pattern hold the note before them (the chord / pad patterns P01 DUB CHORD, P15 PAD BREATH,
 * P17 HALF PAD used to play a short stab on each note step and a rest on each tie). Returns the longest sounding run in 1/16ths over two cycles
 * of the pattern (k: its place in engines.c PATTERNS[]) with ARP GATE 64 (half a step); *runs: how many separate notes sounded */
static double tie_longest(uint32_t k, uint32_t *runs)
{
    track_t *t = &trk[0];
    uint32_t i, up, run = 0, best = 0, P = div_samples(PATTERNS[k].div), blocks;
    fresh();
    for (i = 0; i < NSTEP; i++) {
        if (i < PATTERNS[k].len)
            pat_make_step(&t->step[i], PATTERNS[k].note[i], pat_fl(k, i));
    }
    t->p[P_SLEN] = PATTERNS[k].len;
    t->p[P_SDIV] = PATTERNS[k].div;
    t->p[P_AMODE] = AM_SEQ; t->p[P_ARATE] = PATTERNS[k].div; t->p[P_AOCT] = 1; t->p[P_AHOLD] = 0; t->p[P_AGATE] = 64; t->p[P_APROB] = 127;
    input_on(t, 60, 100);
    *runs = 0;
    blocks = 2u * PATTERNS[k].len * P / CTL;
    for (i = 0, up = 0; i < blocks; i++) {
        blk();
        if (ARP_UP(t)) {
            if (!up)
                (*runs)++;
            run += CTL;
        } else if (up)
            run = 0;
        up = ARP_UP(t) != 0;
        if (i > 4u * P / CTL && run > best)         /* (the first cycle's start-up skipped: it may start mid-pattern) */
            best = run;
    }
    return (double)best / P;
}

static void test_ties(void)
{
    static const uint32_t pat[3] = {0, 14, 16};     /* P01 DUB CHORD, P15 PAD BREATH, P17 HALF PAD */
    uint32_t j, runs[3];
    double len[3];
    for (j = 0; j < 3u; j++) {
        len[j] = tie_longest(pat[j], &runs[j]);
        printf("arptiming: %-10s ARP SEQ: longest note %.2f sixteenths, %u notes in two cycles\n", PATTERNS[pat[j]].name, len[j], runs[j]);
    }
    expect("P01 DUB CHORD: a note with two tied steps sounds about 2.5 sixteenths (not a half-step stab)", len[0] > 2.4 && len[0] < 2.6);
    expect("P15 PAD BREATH: the chord sounds through its 6 ties, about 6.5 sixteenths", len[1] > 6.4 && len[1] < 6.6);
    expect("P17 HALF PAD: the chord sounds through its 14 ties, about 14.5 sixteenths", len[2] > 14.4 && len[2] < 14.6);
    expect("a tied note is one note: PAD BREATH has 4 notes in two cycles (2 a cycle), not one per step", runs[1] == 4u);
}

int main(void)
{
    test_drift();
    test_swing();
    test_prob();
    test_velocity();
    test_start();
    test_ties();
    printf("arptiming: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
