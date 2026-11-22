/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DUELL contributors */
/* MEASURE for a unit CPU run over USB (a CPU skip on M07 TRAVEL 32 measured from the computer, no hands on the unit). Measuring builds ONLY
 * (DUELL_MEASURE=1, tools/build.py: env DUELL_MEASURE=1); every other build has none of this, byte for byte. The normal firmware's own numbers (the CPU line of ABOUT, the
 * header of HOME's pages), read over the editor's SysEx: no new counter, nothing in the audio interrupt. jam/measure/travel_watch.py drives it.
 *   MEASURE 91  0              READ: replies 0, then four 32-bit values as 7-bit groups of five (low first): the longest elapsed half since the last READ (us; the budget
 *                              is CPU_BUDGET_US), the DMA periods missed since power-on, the voices shed (audio.c), and bits: 0 playing, 1 glide running, 2 the end it goes to, 3.. the ease level
 *              1, time, mode[, seed]  GLIDE (the seed, 0..127, default 0, restarts the draw stream): AUTO TIDE's MODE (GM_*: 4 TRAVEL, 3 EVOLVE) and TIME (an index of GLIDE_BARS), then K1 turned right (glide_launch: PLAY starts with it); replies 1, glide running
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
#if DUELL_PROBE_STAGES
        {                                               /* then the stage probe's longest blocks since the last READ (fx.c prb_stage_pk: morph_begin, part 1, part 2 wait, fm6_xblock, part 1 voices,
                                                         * the deadline's budget figures, the gather + send buses, the master, part 1's fm6_sync and its whole fm6_block), us, 14 bit each, low 7 first; cleared */
            extern volatile uint32_t prb_stage_pk[10];
            for (p = 0; p < 10u; p++) {
                uint32_t v = prb_stage_pk[p] / 24u;
                prb_stage_pk[p] = 0;
                ed_b(v & 127u);
                ed_b(v >> 7 & 127u);
            }
        }
#endif
        for (p = 0; p < 2u; p++) {                      /* then a byte each: the voices sounding now and the NOTES (bnotes) of part 1, then of part 2 */
            uint32_t i, nv = 0;
            for (i = 0; i < NVOICE; i++)
                nv += trk[p].v[i].active != 0;
            ed_b(nv);
            ed_b(bnotes[p]);
        }
#if DUELL_PROBE_STAGES
        {                                               /* then the stages' MEANS a block since the last READ (fx.c prb_sum), 0.1 us, 14 bit each, low 7 first; cleared */
            extern volatile uint32_t prb_sum[14], prb_nblk;
            uint32_t nb = prb_nblk ? prb_nblk : 1u;
            for (p = 0; p < 14u; p++) {
                uint32_t v = (uint32_t)((uint64_t)prb_sum[p] * 10u / 24u / nb);
                prb_sum[p] = 0;
                v = v > 16383u ? 16383u : v;
                ed_b(v & 127u);
                ed_b(v >> 7 & 127u);
            }
            prb_nblk = 0;
        }
#endif
        return 1;
    }
    if ((n == 3u || n == 4u) && a[0] == 1u && a[1] < GLIDE_TIMES && a[2] <= GM_JOURNEY) {
        morph.gen = n == 4u ? a[3] : 0u;                /* (the seed: TRAVEL's and EVOLVE's draws repeat run to run, morph.c morph_new_seed) */
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
