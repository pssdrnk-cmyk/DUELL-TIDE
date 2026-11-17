/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* B15 THE RING (audio.c AUDIO_RING, work/b15/cpu/cores/DESIGN.md): the real audio ISR with a simulated DMA and clock, rendering one half ahead.
 *   - the DMA's free half gets, bit for bit, the half the interrupt before rendered (one half of latency, nothing else changes);
 *   - the half is acknowledged at the entry (a half that comes due during a long render keeps its pending flag);
 *   - the debt: an on-time entry has none, a late one has exactly its lateness, an entry a whole half late is a miss (cpu_miss, felucca_dbg.late);
 *   - a render over 100 % of a half is no miss when the next entry is less than a half late; the ease ladder steps up only for a debt that stays;
 *   - a half of 95 % with no debt is not "over" for the ladder (the old ping-pong: two of them stepped it up);
 *   - the flash duck waits for three silent halves (rbuf and both DMA halves) and the DMA buffer is all 0 then.
 * The clock: fm1_ticks() steps host_us at each read; the ISR reads it at its entry and at its end only (the twocore and probe code is off here), so an entry at E with a
 * render of R is host_clock = E - R, host_us = R. FM1_TICKS_PER_US 1: a half is 2,901 ticks (RING_HALF_TICKS) */
#define AUDIO_RING 1
#define main hostsim_main
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_acks, host_nest, host_ack_in_render, host_in_render;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
#define AUDIO_BLK_TICKS() (host_clock)
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t host_half;
static uint32_t fm1_audio_free_half(void) { return host_half; }
static void host_isr(void);
#define AUDIO_DUCK_IDLE() host_isr()
static void fm1_audio_ack_half(void) { host_acks++; host_ack_in_render += host_in_render; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1
static uint32_t host_taps;
static void uac_tap(const int32_t *out, uint32_t n) { (void)out; (void)n; host_taps++; host_in_render = 1; }   /* (called inside the render: an ack after this is an ack at the end) */
static void uac_render_start(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_pot = 1023;
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? host_pot : 620; }
#include "../firmware/src/master.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("audioring: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t E;                                      /* the next entry's time */
static void isr_at(uint32_t entry, uint32_t render)
{
    host_us = render ? render : 1u;
    host_clock = entry - host_us;
    host_pending = FM1_AUDIO_HALF;
    host_in_render = 0;
    fm1_alnk0_irq();
    host_half ^= 1u;                                    /* (the DMA alternates the halves) */
}
static void fresh(void)
{
    uint32_t p;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    for (p = 0; p < NPART; p++) host_preset(&trk[p], 0, 0);
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    memset(abuf, 0, sizeof abuf);
    memset(rbuf, 0, sizeof rbuf);
    audio_cpu_rem = audio_halves = audio_max_us = 0;
    shed_req = 0; shed_count = 0; shed_over = 0;
    ease = 0; ease_calm = 0; ease_avg = 0; ovl_lite = 0;
    cpu_miss = 0;
    host_acks = host_ack_in_render = host_taps = 0; host_nest = 0; host_half = 0;
    ring_run = 0; ring_due = 0; ring_debt = ring_debt_pk = 0;
    transport_req = panic_req = 0;
    duck_on = duck_zero = 0; duck_g = 32768u;
    E = 1000000u;
}
static int32_t peak(const int32_t *o, uint32_t words)
{
    int32_t m = 0;
    uint32_t i;
    for (i = 0; i < words; i++) m = abs(o[i]) > m ? abs(o[i]) : m;
    return m;
}

int main(void)
{
    static int32_t prev[HALF_WORDS];
    uint32_t i, same = 1, n = 0, acks0, miss0, late0;
    setvbuf(stdout, NULL, _IONBF, 0);
    host_mix_follows_master = 0;
    printf("audioring: a half is %u ticks (RING_HALF_TICKS), the old CPU_BUDGET_US %u\n", (unsigned)RING_HALF_TICKS, (unsigned)CPU_BUDGET_US);

    /* 1. one half of latency, bit for bit; the ack at the entry */
    fresh();
    trk[0].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[0], 48, 110); trk_note_on(&trk[0], 60, 110); trk_note_on(&trk[1], 55, 100);
    isr_at(E, 1500u); E += RING_HALF_TICKS;
    check("the first entry copies silence (nothing rendered before it)", peak(&abuf[0], HALF_WORDS) == 0);
    for (i = 0; i < 40u; i++) {
        uint32_t h = host_half;
        memcpy(prev, rbuf, sizeof prev);
        isr_at(E, 1500u); E += RING_HALF_TICKS;
        same &= memcmp(&abuf[h * HALF_WORDS], prev, sizeof prev) == 0;
        n += peak(prev, HALF_WORDS) > 1000;
    }
    check("each entry gives the DMA's free half exactly the half rendered at the entry before (one half later, bit for bit)", same && n > 30u);
    check("the half is acknowledged at the entry, before the render (one ack a half, none from inside it)", host_acks == 41u && host_ack_in_render == 0 && host_taps > 0);
    check("on-time entries: no debt, no miss", ring_debt == 0 && ring_debt_pk == 0 && cpu_miss == 0 && felucca_dbg.late == 0);

    /* 2. a render over 100 %: no miss, the next entry late by what it ran over, paid back by a lighter half */
    isr_at(E, RING_HALF_TICKS * 13u / 10u);             /* 130 % */
    E += RING_HALF_TICKS;
    isr_at(E + RING_HALF_TICKS * 3u / 10u, 1200u);      /* the next entry follows the long render: 30 % of a half late */
    check("a 130 % half: the next entry is 30 % late (its debt), no miss", ring_debt == RING_HALF_TICKS * 13u / 10u - RING_HALF_TICKS && cpu_miss == 0 && felucca_dbg.late == 0);
    E += RING_HALF_TICKS;
    isr_at(E, 1200u); E += RING_HALF_TICKS;             /* the lighter half paid it back: on time again */
    check("the half after a lighter one is on time again (the debt paid back)", ring_debt == 0 && cpu_miss == 0);
    check("one 130 % half does not step the ease ladder up (it needs two halves over in a row)", ease == 0);

    /* 3. 95 % halves with no debt: never "over" for the ladder (the old ping-pong stepped up after two) */
    for (i = 0; i < 20u; i++) { isr_at(E, RING_HALF_TICKS * 95u / 100u); E += RING_HALF_TICKS; }
    check("twenty 95 % halves with no debt: the ease ladder stays at 0, no miss, no tail faded", ease == 0 && cpu_miss == 0 && shed_count == 0);

    /* 4. a debt that stays (two halves over 100 % in a row): the ladder steps up */
    isr_at(E, RING_HALF_TICKS * 11u / 10u); E += RING_HALF_TICKS;
    isr_at(E + RING_HALF_TICKS / 10u, RING_HALF_TICKS * 11u / 10u); E += RING_HALF_TICKS;
    check("two halves at 110 % in a row (a debt that grows): the ladder steps up one level, no miss", ease == 1u && cpu_miss == 0);
    isr_at(E + RING_HALF_TICKS / 5u, 800u); E += RING_HALF_TICKS;
    isr_at(E, 800u); E += RING_HALF_TICKS;

    /* 5. an entry a whole half late: a miss */
    miss0 = cpu_miss; late0 = felucca_dbg.late;
    isr_at(E + RING_HALF_TICKS + 50u, 800u); E += 2u * RING_HALF_TICKS + 50u;
    check("an entry a whole half late is one miss (the DMA replayed a half); the due time starts again from it", cpu_miss == miss0 + 1u && felucca_dbg.late == late0 + 1u && ring_debt == 0);
    isr_at(E, 800u); E += RING_HALF_TICKS;
    check("the entry after it is on time (no debt carried over from the miss)", ring_debt == 0 && cpu_miss == miss0 + 1u);

    /* 6. drift and jitter: entries a few ticks early or late never add up to a debt */
    ring_debt_pk = 0;
    for (i = 0; i < 2000u; i++) { isr_at(E + (i & 1u ? 2u : 0u), 800u); E += RING_HALF_TICKS + 1u; }
    check("2,000 entries drifting 1 tick a half late, with 2 ticks of jitter: no debt builds up, no miss", ring_debt == 0 && ring_debt_pk == 0 && cpu_miss == miss0 + 1u);
    for (i = 0; i < 2000u; i++) { isr_at(E, 800u); E += RING_HALF_TICKS - 1u; }
    check("2,000 entries drifting 1 tick a half early: no debt, no miss", ring_debt == 0 && cpu_miss == miss0 + 1u);

    /* 7. the flash duck: three silent halves, then the DMA buffer is all 0 */
    acks0 = host_acks;
    duck_on = 1;
    for (i = 0; i < 8u && duck_zero < DUCK_SILENT_HALVES; i++) { isr_at(E, 800u); E += RING_HALF_TICKS; }
    check("flash duck: the duck counts three silent halves (rbuf and both DMA halves) before an erase may stop the interrupts", duck_zero == DUCK_SILENT_HALVES && host_acks - acks0 <= 5u);
    check("flash duck: then both DMA halves and the half ahead are 0 (the DMA loops silence)", peak(abuf, 2u * HALF_WORDS) == 0 && peak(rbuf, HALF_WORDS) == 0);
    miss0 = cpu_miss; late0 = felucca_dbg.late;
    isr_at(E + 14u * RING_HALF_TICKS, 800u); E += 15u * RING_HALF_TICKS;   /* the erase: the interrupts off for ~40 ms, the first entry 14 halves late */
    check("flash duck: the entry after the erase (14 halves late, the DMA looped silence) counts no miss, and the due time starts again", cpu_miss == miss0 && felucca_dbg.late == late0 && ring_debt == 0);
    audio_unduck();
    for (i = 0; i < 3u; i++) { isr_at(E, 800u); E += RING_HALF_TICKS; }
    check("flash duck: after it the music comes back (the half ahead first, then the DMA)", peak(rbuf, HALF_WORDS) > 1000 && peak(abuf, 2u * HALF_WORDS) > 1000);

    printf(bad ? "audioring: %d FAILED\n" : "audioring: all passed\n", bad);
    return bad != 0;
}
