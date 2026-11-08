# Felucca 1.7.0 check: #264, #258, #269, #282 (b3 item 6)

Compared 2026-11-08 against the Felucca history (not shallow; all four PRs found).

- **#264** (TRS UART pending events): `fm1_uart1_rx_take` takes a DMA batch only when RX pending (bit 14) or overtime pending (bit 11) is set. Before, it replayed a retained HRXCNT on every poll and advanced the reader past bytes never written. Also 16-bit CON0/CON1/BAUD/HRXCNT and RXCNT = 32 (not the full ring). **Ours has all of it**: `felucca-base/firmware/hal/fm1_uart.h` (gate `& 0x4800u` in `fm1_uart1_rx_take`, `FM1_UART1_RX_FRAME 32u`, `uint16_t` registers), tested by `tests/uart_dma_test.c` (group `uartdma`). No port needed; plain DUELL image unchanged.
- **#258** (TRS timestamped during audio renders): ours has `firmware/src/timer5.c` polling `uart_midi_poll` at 2 kHz nested (`sub % 5u == 2u`), tested by `tests/timer_clock_test.c` (`uarttimer`). What was missing was Felucca's heavier test, now ported as `tests/timer5_midi_test.c` (group `uartflood`, with a UART-off run): real TIMER5 handler, MIDI parser and queue (the UART DMA register layer is stubbed here; `uartdma` covers it), saturated CC stream plus a clock at 24 PPQN, 4.5 ms renders; passes (clock lag <= 1 ms, every CC kept, no drops). Its `TIMER5_SYNTH_TEST` variant needs upstream's MIDI Learn and the full UI test; not ported.
- **#269** (external clock drift): in our history as 820e533; groups `clockdrift` / `clockguard` pass.
- **#282** (`beat_samples` never divides by BPM 0): ours has it, `felucca-base/firmware/src/fx.c` `beat_samples` (BPM < 40 uses 120).
