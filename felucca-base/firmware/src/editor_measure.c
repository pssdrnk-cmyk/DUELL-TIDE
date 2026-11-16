/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DUELL contributors */
/* MEASURE for a unit CPU run over USB (b14: the owner's "cpu skipping" on M07 TRAVEL 32 measured from the computer, no hands on the unit). Measuring builds ONLY
 * (DUELL_MEASURE=1, tools/build.py: env DUELL_MEASURE=1); every other build has none of this, byte for byte. The normal firmware's own numbers (the CPU line of ABOUT, the
 * header of HOME's pages), read over the editor's SysEx: no new counter, nothing in the audio interrupt. jam/measure/travel_watch.py drives it.
 *   MEASURE 91  0              READ: replies 0, then four 32-bit values as 7-bit groups of five (low first): the longest elapsed half since the last READ (us; the budget
 *                              is CPU_BUDGET_US), the DMA periods missed since power-on, the voices shed (audio.c), and bits: 0 playing, 1 glide running, 2 the end it goes to, 3.. the ease level
 *              1, time, mode   GLIDE: AUTO TIDE's MODE (GM_*: 4 TRAVEL, 3 EVOLVE) and TIME (an index of GLIDE_BARS), then K1 turned right (glide_launch: PLAY starts with it); replies 1, glide running
 *              2               STOP the glide (K1's two detents): replies 2
 *              anything else   no reply */
enum { ED_MEASURE = 91 };
static int ed_measure_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t p;
    if (cmd != ED_MEASURE)
        return 0;
    if (n == 1u && a[0] == 0u) {
        const uint32_t w[4] = {cpu_win_take(CPU_W_CON), cpu_snap.miss, shed_count, (uint32_t)song.playing | (uint32_t)glide_running() << 1 | (gl.to & 1u) << 2 | (uint32_t)ease << 3};
        ed_b(0);
        for (p = 0; p < 20u; p++)
            ed_b(w[p / 5u] >> (7u * (p % 5u)) & 127u);
        return 1;
    }
    if (n == 3u && a[0] == 1u && a[1] < GLIDE_TIMES && a[2] <= GM_JOURNEY) {
        gl.time = a[1];
        gl.mode = a[2];
        if (!glide_running())
            glide_launch(1u, morph_mask(song.sel), song.sel);
        ed_b(1);
        ed_b(glide_running() != 0);
        return 1;
    }
    if (n == 1u && a[0] == 2u) {
        glide_stop();
        ed_b(2);
        return 1;
    }
    return 0;
}
