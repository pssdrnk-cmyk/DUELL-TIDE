/* SPDX-License-Identifier: GPL-3.0-only */
/* Felucca #282: a transient uninitialized tempo must not divide by zero.
 * DUELL must retain AUTO's active-source selection and its extra divisions. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint32_t failures;
static void expect(const char *name, int ok)
{
    printf("clock safety: %-56s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}
int main(void)
{
    static const struct { int32_t bpm; uint32_t samples; } cases[] = {
        {40, 66150}, {97, 27278}, {120, 22050}, {240, 11025},
        {0, 22050}, {-1, 22050}, {39, 22050}
    };
    _Static_assert(FS == 44100, "reference periods are for the FM-1's 44.1 kHz clock");
    song.g[G_CLOCK] = 0;
    for (uint32_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        song.g[G_BPM] = cases[i].bpm;
        expect("valid tempo unchanged; invalid tempo uses 120 BPM", beat_samples() == cases[i].samples);
    }
    for (uint32_t port = 1; port <= 2; port++) {
        song.g[G_CLOCK] = (int16_t)port;
        song.g[G_BPM] = 0;
        midi_beat_samples = 30000;
        expect("USB/TRS measured tempo takes precedence", beat_samples() == 30000);
        midi_beat_samples = 0;
        expect("USB/TRS before first measured tempo is safe", beat_samples() == 22050);
    }
    song.g[G_CLOCK] = CLK_AUTO;
    midi_beat_samples = 30000;
    midi_clock.lock = 0;
    expect("AUTO unlocked ignores stale external tempo", beat_samples() == 22050);
    midi_clock.lock = 2;
    expect("AUTO locked uses external tempo", beat_samples() == 30000);
    song.g[G_CLOCK] = 0;
    expect("straight, slow and DUELL triplet divisions remain valid",
           div_samples(2) == 5512 && div_samples(9) == 352800 && div_samples(10) == 14700);
    return failures != 0;
}
