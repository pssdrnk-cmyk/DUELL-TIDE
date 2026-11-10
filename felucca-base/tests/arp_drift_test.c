/* SPDX-License-Identifier: GPL-3.0-only */
/* ARP SEQ parts against the beat grid over a long set (b8 lane DRIFT; finding work/b7-night/reports/ARC.md:101).
 * Four factory ARP SEQ multis (M02 DUB CHORD, M08, M14, M12: ARP RATE 1/16, 4BAR, 1/2, 1/8, 1/1, 1/4) play 400 bars at 117, 120, 128 and 93 BPM,
 * on the internal clock and on a 24 ppq external clock (USB, pulses at their exact microsecond, fm1_ms as the unit's millisecond tick).
 * Every arp step's onset is placed on the beat grid that TIDE's bars run on (beat_n / beat_pos, beat_samples(): the place in samples
 * = beats counted * beat + beat_pos - the samples the new step has already run at the block's end) and compared with its exact place
 * (step k at k * beat * RATE, RATE the division as a fraction of a quarter, counted from the first step's place); the difference is the drift.
 * Also: the tempo changed twice as the TAP does (G_BPM, then clock_rescale: +3 BPM at bar 100, 93 at bar 200); the drift counts from the second on-beat step after each change.
 * Steps on a beat and even (swing moves only the odd ones) are compared. A row every 50 bars; PASS: |drift| <= 1 sample at bar 400.
 * (Before b8 the arp's step was the beat's division rounded down: 117 BPM, 1/16 = 5653 samples of 5653.75, 12 samples a bar.)
 * Built and run by host_tests.sh (name: arpdrift): the firmware sources against ui_test.c's stubs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static const uint32_t MULTI[4] = {1u, 7u, 13u, 11u};     /* project_load slots: M02, M08, M14, M12 */
static const uint32_t BPM[4] = {117u, 120u, 128u, 93u};
#define BARS 400u

static int bad, verbose = 1;
static int expect(const char *what, int ok)
{
    printf("arpdrift: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

static void div_frac(uint32_t div, uint32_t *num, uint32_t *den)   /* ARP RATE as a fraction of a quarter (fx.c div_samples, exact) */
{
    static const uint8_t D[6] = {1, 2, 4, 8, 3, 6}, D48[7] = {32, 64, 4, 144, 72, 36, 18};
    if (div < 6u) *num = 1u, *den = D[div];
    else if (div < 10u) *num = 1u << (div - 5u), *den = 1u;
    else *num = D48[(div - 10u) % 7u], *den = 48u;
}

static uint64_t now_us, next_us;
static uint32_t pulse;
static void blk(uint32_t ext, uint32_t bpm)
{
    now_us += (uint64_t)CTL * 1000000u / FS;               /* (CTL samples a block; the rounding of 1/44100 s is the clock's, not the arp's) */
    fm1_ms = (uint32_t)(now_us / 1000u) + 10u;
    while (ext && next_us <= now_us) {
        midi_enqueue(0xFu | 0xF8u << 8, 1u);
        pulse++;
        next_us = (uint64_t)pulse * 60000000u / ((uint64_t)bpm * 24u);
    }
    events_block(CTL);
}

typedef struct { uint32_t idx, k, kb, num, den, live, pend; int64_t gb, drift; } part_t;   /* (kb, gb: the step and grid place the drift counts from: the first, or after a tempo change the second on-beat one) */

static void run(uint32_t slot, uint32_t bpm, uint32_t ext, uint32_t tap)   /* tap: the tempo changes at bar 100 (to +3 BPM) and 200 (to 93) as the TAP does: G_BPM, then clock_rescale */
{
    part_t ps[NSHOWN];
    uint32_t p, beats = 0, printed = 0, first = 0, tapped = 0;
    char what[160];
    ui_power_on();
    memset(&midi_clock, 0, sizeof midi_clock);
    memset(&um, 0, sizeof um);
    mi_r = mi_w = 0;
    midi_beat_samples = 0;
    fm1_ms = 0;
    now_us = next_us = 0;
    pulse = 0;
    events_block(CTL);
    project_load(slot);
    song.g[G_BPM] = (int16_t)bpm;
    song.g[G_CLOCK] = ext ? 1 : 0;
    memset(ps, 0, sizeof ps);
    for (p = 0; p < NSHOWN; p++) {
        trk[p].p[P_MUTE] = 0;
        ps[p].idx = 0xFFFFFFFFu;
        ps[p].live = arp_mode(&trk[p]) == AM_SEQ_ID;
        div_frac((uint32_t)trk[p].p[P_ARATE], &ps[p].num, &ps[p].den);
    }
    if (ext)
        midi_enqueue(0xFu | 0xFAu << 8, 1u);               /* Start: PLAY goes with the first pulse */
    else
        transport_req = 1;
    while (beats < BARS * 4u + 4u) {
        uint32_t ob = beat_n, bs;
        if (tap && tapped < 2u && beats == 4u * 100u * (tapped + 1u)) {
            uint32_t was = beat_samples();
            song.g[G_BPM] = (int16_t)(tapped ? 93 : bpm + 3u);
            clock_rescale(was, beat_samples());
            tapped++;
            for (p = 0; p < NSHOWN; p++)
                ps[p].pend = 2u;
        }
        blk(ext, bpm);
        if (!song.playing)
            continue;
        beats += (beat_n - ob) & 3u;
        bs = beat_samples();
        for (p = 0; p < NSHOWN; p++) {
            part_t *s = &ps[p];
            const track_t *t = &trk[p];
            int64_t grid, off;
            if (!s->live || t->arp_idx == s->idx || t->arp_pos >= 0x8000000u)
                continue;
            s->k = s->idx == 0xFFFFFFFFu ? 0u : s->k + (t->arp_idx - s->idx);
            s->idx = t->arp_idx;
            if ((s->k * s->num) % s->den || (s->k & 1u))
                continue;                                  /* (on a beat, an even step: no swing) */
            grid = (int64_t)beats * bs + beat_pos - t->arp_pos;
            if (s->pend) {                                 /* after a tempo change: the second on-beat step is the new baseline (the first spans the change) */
                if (!--s->pend)
                    s->kb = s->k, s->gb = grid;
                continue;
            }
            if (!s->k)
                s->gb = grid, s->kb = 0u;
            off = grid - s->gb - (int64_t)((uint64_t)(s->k - s->kb) * s->num * bs / s->den);
            s->drift = off;
        }
        if (beats >= 4u * (printed + 50u) || (!first && beats >= 4u)) {
            printed = first ? beats / 4u - (beats / 4u) % 50u : 0u;
            first = 1u;
            for (p = 0; p < NSHOWN && verbose; p++)
                if (ps[p].live)
                    printf("arpdrift: %s M%02u %3u BPM part %u rate %2d  bar %3u  drift %6lld\n", tap ? "TAP" : ext ? "EXT" : "INT", slot + 1u, bpm, p + 1u,
                           trk[p].p[P_ARATE], printed, (long long)ps[p].drift);
        }
    }
    for (p = 0; p < NSHOWN; p++)
        if (ps[p].live) {
            snprintf(what, sizeof what, "%s%s M%02u %3u BPM part %u (rate %d, %u steps): |drift| <= 1 sample after %u bars (%lld)", ext ? "EXT" : "INT", tap ? " TAP" : "", slot + 1u, bpm,
                     p + 1u, trk[p].p[P_ARATE], ps[p].k, BARS, (long long)ps[p].drift);
            expect(what, ps[p].k > 0u && llabs(ps[p].drift) <= 1);
        }
}

int main(void)
{
    uint32_t ext, i, j;
    for (ext = 0; ext < 2u; ext++)
        for (i = 0; i < 4u; i++)
            for (j = 0; j < 4u; j++)
                run(MULTI[i], BPM[j], ext, 0u);
    for (i = 0; i < 4u; i += 3u)                           /* (the internal clock's tempo changes: M02 at 117 BPM, M12 at 120) */
        run(MULTI[i], i ? 120u : 117u, 0u, 1u);
    printf("arpdrift: %d FAIL\n", bad);
    return bad ? 1 : 0;
}
