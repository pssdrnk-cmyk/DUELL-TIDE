/* SPDX-License-Identifier: GPL-3.0-only */
/* JAM: Felucca #269's fixed musical clock, with DUELL's extra divisions.
 * Only external-clock progress uses these units; gates and engines keep samples.
 * At most half a quarter arrives per block, keeping the numerator below 2^32. */
#define MIDI_CLOCK_GRID 24000u
typedef struct { uint32_t remainder; uint8_t div; } midi_grid_t;
static midi_grid_t midi_seq_grid[NTRK], midi_arp_grid[NPART], midi_click_grid, midi_beat_grid;
static const uint8_t MIDI_EXTRA_NUM[7] = {2, 4, 1, 3, 3, 3, 3};
static const uint8_t MIDI_EXTRA_DEN[7] = {3, 3, 12, 1, 2, 4, 8};

static uint32_t midi_clock_period(uint32_t quarter, uint32_t div)
{
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) :
           quarter * DIV48[(div - 10u) % 7u] / 48u;
}
static uint32_t midi_grid_den(uint32_t div)
{
    return MIDI_CLOCK_GRID * (div < 10u ? 1u : MIDI_EXTRA_NUM[(div - 10u) % 7u]);
}
static __attribute__((noinline)) void midi_grid_reset(void)
{
    memset(midi_seq_grid, 0, sizeof midi_seq_grid);
    memset(midi_arp_grid, 0, sizeof midi_arp_grid);
    memset(&midi_click_grid, 0, sizeof midi_click_grid);
    memset(&midi_beat_grid, 0, sizeof midi_beat_grid);
}
static __attribute__((noinline)) uint32_t midi_grid_advance(midi_grid_t *g, uint32_t n, uint32_t div)
{
    uint32_t quarter = beat_samples(), den = midi_grid_den(div), numerator;
    if (g->div != div) {
        g->remainder = 0;                            /* a rate edit starts a fresh sub-sample carry */
        g->div = (uint8_t)div;
    }
    if (div < 6u)
        quarter = (quarter / DIV_DEN[div]) * DIV_DEN[div];
    else if (div >= 10u)
        quarter = midi_clock_period(quarter, div) * MIDI_EXTRA_DEN[(div - 10u) % 7u];
    numerator = n * quarter + g->remainder;
    g->remainder = numerator % den;
    return numerator / den;
}
static uint32_t midi_clock_scale(uint32_t value, uint32_t new_period, uint32_t old_period)
{
    return (uint32_t)(((uint64_t)value * new_period + old_period / 2u) / old_period);
}
static uint32_t midi_clock_scale_phase(uint32_t value, uint32_t new_period, uint32_t old_period,
                                      midi_grid_t *grid, uint32_t div)
{
    uint32_t den = midi_grid_den(div);
    uint64_t phase = (uint64_t)value * den + (grid->div == div ? grid->remainder : 0u);
    phase = (phase * new_period + old_period / 2u) / old_period;
    grid->remainder = (uint32_t)(phase % den);
    grid->div = (uint8_t)div;
    return (uint32_t)(phase / den);
}
