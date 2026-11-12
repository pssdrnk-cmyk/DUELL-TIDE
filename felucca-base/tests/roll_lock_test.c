/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE 1.0.0-b9 (K2): ROLL TYPE retired. Built with -DTIDE_ROLL_LOCK=1 (the device's setting) and again with =0 (the old, free roll).
 *   new     with the lock, every new roll (morph_cfg_of, the factory B's cfg) is MUSIC whatever morph_musical and MC_WILD say; free: the old bits
 *   saved   a saved End B recalls from its own cfg byte: a SMART (0) and a WILD (MC_WILD) seed make the same End B in both builds. The test prints a hash of
 *           the values of both ends ("hash saved N") and the build compares the two runs (host_tests.sh rolllock) */
#define main hostsim_main
#include "hostsim.c"
#undef main
static int bad;
static void check(const char *what, int ok) { printf("rolllock: %-90s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void fresh(void)
{
    memset(&fx, 0, sizeof fx);
    memset(trk, 0, sizeof trk);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(mwork, 0, sizeof mwork);
    duell_mode_force(DM_SPLIT);
    host_tracks_init();
    host_preset(&trk[0], 0u, 0u);
    host_preset(&trk[1], 0u, 0u);
    song.sel = 0;
}
static uint64_t endhash(uint32_t pi, uint32_t x)
{
    const int8_t *w = mph[pi].h[x][mph[pi].top[x]];
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint32_t i = 0; i < P_COUNT; i++) h = (h ^ (uint8_t)w[i]) * 0x100000001B3ull;
    return h;
}
int main(void)
{
    static const uint32_t seeds[] = {0x1234u, 0xBEEF1u, 0x77777u, 0xC0FFEEu};
    uint64_t all = 0xCBF29CE484222325ull;
    uint32_t i;
    printf("rolllock: TIDE_ROLL_LOCK %d\n", TIDE_ROLL_LOCK);
    fresh();
    (void)morph_ready(0);
    morph_musical = 0; morph.cfg = 0;
    check("stored ROLL TYPE SMART: the next roll's cfg", (TIDE_ROLL_LOCK ? (morph_cfg_of(0) & MC_MUSIC) != 0 : (morph_cfg_of(0) & MC_MUSIC) == 0));
    morph_musical = 1; morph.cfg = MC_WILD;
    check("stored ROLL TYPE WILD: MUSIC, no WILD bit (locked) / WILD (free)", TIDE_ROLL_LOCK ? (morph_cfg_of(0) & MC_MUSIC) && !(morph_cfg_of(0) & MC_WILD) : (morph_cfg_of(0) & MC_WILD) && !(morph_cfg_of(0) & MC_MUSIC));
    morph_musical = 0; morph.cfg = 0;
    check("a factory sound's B cfg", (TIDE_FACTORY_CFG() != 0u) == (TIDE_ROLL_LOCK != 0));
    morph_musical = 1; morph.cfg = 0;
    check("MUSIC stored: MUSIC in both builds", (morph_cfg_of(0) & MC_MUSIC) && (TIDE_FACTORY_CFG() & MC_MUSIC));
    /* saved: End B from its own cfg byte (SMART 0, WILD), whatever the live settings are */
    for (uint32_t k = 0; k < 3; k++) {
        uint32_t cfg = k == 0 ? 0u : k == 1 ? MC_WILD : MC_MUSIC;
        for (i = 0; i < 4; i++) {
            uint64_t h;
            fresh();
            (void)morph_ready(0);
            morph_musical = 1; morph.cfg = 0;
            morph_roll_cfg(0, 1u, seeds[i], 40u, cfg);
            h = endhash(0, 1);
            all = (all ^ h) * 0x100000001B3ull;
            if (k == 0) check("saved SMART seed: not MUSIC, depth 1", !(mph[0].rcfg[1][mph[0].top[1]] & MC_MUSIC) && mph[0].rdep[1][mph[0].top[1]] <= 1u);
            if (k == 1) check("saved WILD seed keeps WILD", mph[0].rcfg[1][mph[0].top[1]] & MC_WILD);
        }
    }
    printf("rolllock: hash saved %016llx\n", (unsigned long long)all);
    printf("rolllock: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
