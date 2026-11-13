/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hügelton Instruments contributors */
/* Ported from Felucca tests/timer5_midi_test.c (#258), without its TIMER5_SYNTH_TEST variant (it needs upstream's MIDI Learn).
 * Real TIMER5 scheduling and TRS parser against a simulated RX DMA. A busy
 * audio ISR must not postpone input timestamping until its render completes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#ifndef FELUCCA_UART
#define FELUCCA_UART 1
#endif
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_uart.h"
static uint32_t rx_pending, rx_write, rx_polls;
static uint32_t host_rx_take(void) { uint32_t n = rx_pending; rx_pending = 0; rx_polls++; return n; }
#define fm1_uart1_rx_take() host_rx_take()
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"
static uint32_t host_us, scans, acks, usb_polls, uac_polls;
static uint32_t fm1_ticks(void) { return host_us; }
static void fm1_timer5_ack(void) { acks++; }
static void fm1_input_tick(void) { scans++; }
static void host_usb_poll(void) { usb_polls++; }
#define usb_poll() host_usb_poll()
#undef FELUCCA_UAC
#define FELUCCA_UAC 1
static void uac_service(void) { uac_polls++; }
#define FM1_TICKS_PER_US 1u
static struct { uint32_t timer_irqs, nested, in_audio; } felucca_dbg;
static uint32_t t5_nested_ticks;
#include "../firmware/src/timer5.c"
static int check(const char *name, int ok) { printf("%s: %s\n", name, ok ? "ok" : "FAIL"); return !ok; }
static void wire_byte(uint32_t b) {
    um_ring[rx_write++ % UM_RING] = (uint8_t)b;
    rx_pending++;
}
int main(void) {
    int bad = 0, clock_timing_ok = 1, cc_ok = 1, usb_deferred_ok = 1;
    uint32_t next_byte = 320u, next_clock = 20833u, byte_phase = 0, value = 0;
    uint32_t clocks_sent = 0, clocks_seen = 0, cc_seen = 0, cc_sent = 0, bytes_sent = 0, clock_lag_max = 0;
    uint32_t clock_sent_ms[512];
    for (host_us = 100; host_us <= 10000000u; host_us += 100) {
        while (next_byte <= host_us) {
            if (next_byte >= next_clock) {
                wire_byte(0xF8u);
                clock_sent_ms[clocks_sent++] = next_byte / 1000u;
                next_clock = (clocks_sent + 1u) * 125000u / 6u;
            } else if (byte_phase == 0) { wire_byte(0xB0u); byte_phase = 1; }
            else if (byte_phase == 1) {
                wire_byte(12u);
                byte_phase = 2;
            }
            else { wire_byte(value++ & 127u); byte_phase = 0; cc_sent++; }
            next_byte += 320u;
            bytes_sent++;
        }
        /* 4.5 ms renders alternate with 1.5 ms outside the render. */
        felucca_dbg.in_audio = host_us % 6000u < 4500u;
        uint32_t usb_before = usb_polls;
        fm1_timer5_irq();
        if (felucca_dbg.in_audio && usb_polls != usb_before) usb_deferred_ok = 0;
        /* Audio consumes only after a render, as on hardware. */
        if (!felucca_dbg.in_audio) {
            uint32_t read = mi_r;
            while (read != mi_w) {
                uint32_t at = read++ % MQ, pkt = midi_in_q[at];
                if (((pkt >> 8) & 255u) == 0xF8u) {
                    uint32_t lag = midi_in_ms[at] - clock_sent_ms[clocks_seen++];
                    if (lag > clock_lag_max) clock_lag_max = lag;
                    if (lag > 1u || midi_in_source[at] != 2u) clock_timing_ok = 0;
                } else {
                    uint32_t cc = 12u;
                    if (((pkt >> 8) & 255u) != 0xB0u || ((pkt >> 16) & 255u) != cc ||
                        (pkt >> 24) != (cc_seen & 127u)) cc_ok = 0;
                    cc_seen++;
                }
            }
            mi_r = read;
        }
    }
    for (uint32_t i = 0; i < 60; i++) {
        host_us += 100; felucca_dbg.in_audio = 0;
        fm1_timer5_irq();
    }
    uint32_t read = mi_r;
    while (read != mi_w) {
        uint32_t at = read++ % MQ, pkt = midi_in_q[at];
        if (((pkt >> 8) & 255u) == 0xF8u) {
            uint32_t lag = midi_in_ms[at] - clock_sent_ms[clocks_seen++];
            if (lag > clock_lag_max) clock_lag_max = lag;
            if (lag > 1u) clock_timing_ok = 0;
        } else {
            if ((pkt >> 24) != (cc_seen & 127u)) cc_ok = 0;
            cc_seen++;
        }
    }
    mi_r = read;
#if FELUCCA_UART
    bad += check("TRS clock timestamps stay within 1 ms under saturated CC and 4.5 ms audio renders", clock_timing_ok && clocks_seen == clocks_sent);
    bad += check("interleaved realtime preserves every CC, with no DMA/queue drops", cc_ok && cc_seen == cc_sent && !um.drops && !midi_in_overflow && um.bytes == bytes_sent);
    bad += check("TRS is polled at 2 kHz throughout nested audio", rx_polls >= 20000u && rx_polls <= 20020u);
#else
    bad += check("UART-disabled build never polls or queues TRS input", !rx_polls && !mi_w);
#endif
    bad += check("USB MIDI remains deferred during audio while USB audio is serviced", usb_deferred_ok && usb_polls && uac_polls);
    bad += check("panel scan and nested accounting remain active", scans == acks && scans == felucca_dbg.timer_irqs && felucca_dbg.nested > 70000u);
    printf("wire: %u CC messages, %u clock pulses, %u bytes; drops: %u; max clock timestamp lag: %u ms\n", cc_seen, clocks_seen, bytes_sent, um.drops, clock_lag_max);
    return bad != 0;
}
