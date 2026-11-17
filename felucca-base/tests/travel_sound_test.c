/* SPDX-License-Identifier: GPL-3.0-only */
/* B15 (manager, sound first, 2026-11-17): what AUTO TIDE TRAVEL 32 plays on a factory multi, as the owner plays it (the multi loaded as MODE RECALL loads it, K1 launched to B,
 * both parts, the multi's BPM), rendered by the firmware's own mix_block (the one-core host path). At every arrival (gl.to changes) it hashes both parts' MORPH state (mph[]: the
 * ends' values, FM6's bytes: the destination's sound as TRAVEL made it), and over the run the output. Built the same way in another tree (1.0.0, jam11 before B15) the arrival
 * hashes show whether TRAVEL's destinations sound the same. Report only (not in the default list).
 *   build/host/travel_sound_test [SLOT1 [TRIPS]]   default M07, 4 arrivals */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int32_t ob[2 * CTL];
static uint32_t blk;
static void block_step(void)
{
    fm1_ms = 10u + (uint32_t)((uint64_t)(++blk) * CTL * 1000u / FS);
    events_block(CTL);
    if ((blk % ((uint32_t)FS / CTL / 60u + 1u)) == 0) {
        frame();
        while (wave_service())
            ;
        budget_poll();
    }
}
static void load(uint32_t slot)
{
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    blk = 0;
    gl.on = gl.armed = 0;
    transport_req = 0;
    song.playing = song.rec = 0;
    mods_off = 0;
    usb.config = 1;
}
static uint64_t ts_fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    size_t i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}
int main(int argc, char **argv)
{
    uint32_t slot = argc > 1 ? (uint32_t)atoi(argv[1]) - 1u : 6u, trips = argc > 2 ? (uint32_t)atoi(argv[2]) : 4u, i, k = 0, lastto, lim, p;
    uint64_t ho = 0xCBF29CE484222325ull;
    double e = 0;
    uint64_t nsamp = 0, vsum[2] = {0, 0};
    setvbuf(stdout, 0, _IONBF, 0);
    host_mix_follows_master = 0;                       /* (the device's own path: the mix at the fixed level, MASTER on the DAC feed) */
    load(slot);
    gl.mode = (uint8_t)GM_JOURNEY;
    gl.time = 5u;
    for (i = 0; i < 24u; i++) block_step();
    glide_launch(1u, 3u, 0u);
    lastto = gl.to;
    lim = (uint32_t)((uint64_t)GLIDE_BARS[5] * 4u * 60u * FS / CTL / (uint32_t)(song.g[G_BPM] ? song.g[G_BPM] : 120)) * (trips + 1u) + 20000u;
    printf("travelsound: M%02u TRAVEL 32, BPM %d, %u arrivals\n", slot + 1u, song.g[G_BPM], trips);
    for (i = 0; i < lim && k < trips; i++) {
        block_step();
        mix_block(ob, CTL);
        while (wave_service())
            ;
        ho = ts_fnv(ho, ob, sizeof ob);
        for (p = 0; p < 2u * CTL; p++, nsamp++)
            e += (double)ob[p] * ob[p];
        for (p = 0; p < 2u; p++) {
            uint32_t v, n = 0;
            for (v = 0; v < NVOICE; v++)
                n += trk[p].v[v].active != 0;
            vsum[p] += n;
        }
        if (gl.to != lastto) {                          /* an arrival: the parts' MORPH state as TRAVEL left it */
            uint64_t hm[2];
            char nm[2][24];
            lastto = gl.to;
            for (p = 0; p < 2u; p++) {
                hm[p] = ts_fnv(0xCBF29CE484222325ull, &mph[p], sizeof mph[p]);
                tide_end_name(p, gl.to & 1u, nm[p]);
            }
            printf("travelsound: arrival %u at block %u: part 1 -> %-18s morph %016llx  part 2 -> %-18s morph %016llx\n", k + 1u, i, nm[0], (unsigned long long)hm[0], nm[1],
                   (unsigned long long)hm[1]);
            k++;
        }
    }
    printf("travelsound: output %016llx  rms %.1f  voices a block %.3f / %.3f  (%u blocks)\n", (unsigned long long)ho, sqrt(e / (double)(nsamp ? nsamp : 1u)), (double)vsum[0] / i,
           (double)vsum[1] / i, i);
    return 0;
}
