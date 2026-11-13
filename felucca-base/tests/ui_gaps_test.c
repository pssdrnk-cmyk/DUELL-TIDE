/* SPDX-License-Identifier: GPL-3.0-only */
/* uigaps (b10 ui-a, Phase A cost #2): a full redraw of a grid layout no longer fills the whole body first (ui_draw.c grid_fill: only the BG the strips leave). Every page of every engine
 * (LAYER / STEREO / DUEL, both parts) and the QUICK layers is redrawn (ui.force) on a screen poisoned with one colour: no pixel of it may be left (nothing is left uncovered), and the
 * picture equals the one drawn over the old whole-body fill (live pictures, LFO 2 / CYCLE ENV, move between the two frames: counted, not failed). Prints the pixels the pre-fill sends per
 * grid layout (the old fill: 46,320 px, 37 ms at 20 MHz). Built and run by host_tests.sh (name uigaps). */
#define UI_TEST_NO_MAIN 1
static int prefill_on = 1;
#define GRID_PREFILL() do { if (prefill_on) gap_rows(H_HEAD, Y_FOOT + 15); else grid_fill(); } while (0)
#include <stdlib.h>
static unsigned long long fillpx, gpx[64], gn[64];
static unsigned long long maxfill;
#define UI_FILL_HOOK(x_, y_, w_, h_) (fillpx += (unsigned long long)(w_) * (h_), maxfill = (unsigned long long)(w_) * (h_) > maxfill ? (unsigned long long)(w_) * (h_) : maxfill)
#include "ui_test.c"
static const char *const FAMN[] = {"HOME", "ENV", "LFO", "FX", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ", "TRK", "?"};
static uint16_t ref[240 * 240];
static uint32_t ncase[64], ntot, nbadp, nlive;
static void one(const char *eng, const char *key)
{
    uint32_t k, bad = 0, pz = 0, g = grid_on;
    prefill_on = 1;
    ui.force = 1; frame();
    memcpy(ref, host_screen, sizeof ref);
    for (k = 0; k < 240u * 240u; k++) host_screen[k] = 0x1357;
    prefill_on = 0;
    ui.force = 1; frame();
    prefill_on = 1;
    for (k = 0; k < 240u * 240u; k++) pz += host_screen[k] == 0x1357;
    for (k = 0; k < 240u * 240u; k++)
        bad += host_screen[k] != ref[k];
    ncase[g & 63]++;
    fillpx = 0; grid_fill(); gut_flush(); gpx[g & 63] += fillpx; gn[g & 63]++;
    ntot++, nbadp += pz != 0u, nlive += bad;
    if (pz)
        printf("uigaps: FAIL %s|%s|grid %u: %u pixels left uncovered\n", eng, key, g, pz);
    memcpy(host_screen, ref, sizeof ref);
}
/* b11 stripe check (below, main): a stripe = gutter pixels (gfx.c GUT) that, in one frame, pass through a value that is neither the old picture's nor the new one's
 * (ui_test.c host_fl), in a row where nothing else does */
static uint32_t frame_stripes(void)
{
    uint32_t y, x, i, n = 0;
    host_fl_begin();
    frame();
    for (y = GUT_Y0; y < GUT_Y0 + GUT_H; y++) {
        uint32_t ing = 0, outg = 0, gut;
        for (x = 0; x < 240u; x++) {
            const uint32_t k = y * 240u + x;
            if (host_fl_n[k] != 2u || host_fl_mid[k] == host_screen[k])
                continue;
            for (gut = 0, i = 0; i < 5u; i++)
                gut |= x >= GUT[i][0] && x < (uint32_t)GUT[i][0] + GUT[i][1];
            if (gut)
                ing++;
            else
                outg++;
        }
        if (!outg)
            n += ing;
    }
    host_fl_on = 0;
    return n;
}
/* one action watched frame by frame for 12 frames: kind 0 a button (a), 1 an encoder step (role a), 2 a key (a); 0 when the layout changed (another layout fills the whole body first) */
static uint32_t act_stripes(uint32_t kind, uint32_t a)
{
    uint32_t f, n = 0, g0 = grid_on;
    if (kind == 0u)
        btn_down(a);
    else if (kind == 1u)
        host_enc[panel.enc[a]] += panel.dir[a];
    else
        key_down(a);
    for (f = 0; f < 12u; f++) {
        if (f == 1u && kind == 0u)
            btn_up(a);
        if (f == 1u && kind == 2u)
            key_up(a);
        n += frame_stripes();
        if (f == 0u && kind == 1u)
            host_ticks += 200000u;
    }
    return g0 == grid_on ? n : 0u;
}
int main(void)
{
    uint32_t e, i, l, g, k, g2;
    static const struct { uint8_t b; const char *n; } LY[] = {{B_GLO, "QUICK GLO"}, {B_SCL, "QUICK SCL"}, {B_EDIT, "QUICK MULTI"}, {B_LFO, "QUICK LFO"}, {B_ENV, "QUICK ENV"}, {B_ARP, "QUICK ARP"}};
    for (g = 0; g < 6u; g++)
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        if (g && e > 2u) continue;
        for (i = 0; i < NPAGES; i++) {
            char key[64];
            ui_power_on(); song.sel = (uint8_t)(g & 1u); duell_mode_req = (uint8_t)(g < 2u ? DM_LAYER : g < 4u ? DM_STEREO : DM_SPLIT); set_engine_of(TSEL, e); go_main_to(HP_MORPH); frames(64);
            if (!page_visible(i)) continue;
            ui.page = (uint8_t)i; page_entered(); frames(160);
            snprintf(key, sizeof key, "%s:%s", FAMN[PAGES[i].fam % 12u], PAGES[i].title);
            one(ENGINES[e]->name, key);
        }
        for (l = 0; l < NELEM(LY); l++) {
            ui_power_on(); song.sel = 0; set_engine_of(TSEL, e); go_main_to(HP_MORPH); frames(64);
            btn_down(LY[l].b); frames(480);
            one(ENGINES[e]->name, LY[l].n);
            btn_up(LY[l].b); frames(64);
        }
    }
    {   /* b10 (rc6 "two vertical lines, a blink"): a page change to ANOTHER grid layout fills the whole body first (no thin BG stripes across the old page while the cards draw);
         * a page change within the same layout only the cheap gap prefill */
        uint32_t fx = 12u, edit1 = 22u, voice = 33u, ok = 1;
        prefill_on = 0;                                                     /* (the new gap prefill, not the old whole-body model) */
        ui_power_on(); go_main_to(HP_MORPH); frames(64);
        ui.page = (uint8_t)fx; page_entered(); frames(40);
        ok &= grid_on == 1u && page_visible(edit1) && page_visible(voice);
        maxfill = 0; ui.page = (uint8_t)edit1; page_entered(); frame();
        ok &= grid_on == 14u && maxfill >= 240ull * 190ull;                 /* FX (grid 1) -> EDIT 1 (grid 14): the whole body */
        frames(20);
        maxfill = 0; ui.page = (uint8_t)voice; page_entered(); frame();
        ok &= grid_on == 14u && maxfill < 240ull * 190ull;                  /* EDIT 1 -> VOICE 1/2 (both 14): the gap prefill only */
        printf("uigaps: %s: another layout fills the whole body first, the same layout only its gaps" "%c", ok ? "ok" : "FAIL", 10);
        if (!ok) return 1;
    }
    {   /* b11 (owner on rc8: "when a screen is redrawn there are still 3 vertical lines for a couple of milliseconds"; on SAVE > MULTIS turning ALGORITHM, and in QUICK SEQ
         * picking a tool): no vertical stripe through the old picture when a page is redrawn within one layout. The gutters' fill (gfx.c GUT: x 0..3, 79..81, 119..120,
         * 158..160, 236..239) went out before the strips and cut through whatever the old picture had there; it now goes out at the end of the pass, only where nothing drew */
        static const uint8_t BT[] = {B_HOME, B_HOME, B_HOME, B_FX, B_FX, B_ENV, B_LFO, B_EDIT, B_EDIT};
        uint32_t b, nb, n = 0, nm = 0, nq = 0, ok = 1;
        prefill_on = 0;
        ui_power_on(); go_main_to(HP_MORPH); frames(160);
        for (b = 0; b < NELEM(BT); b++) {
            nb = act_stripes(0u, BT[b]);
            if (nb)
                printf("uigaps: page change %u (grid %u): %u stripe pixels\n", b, grid_on, nb);
            n += nb;
            frames(160);
        }
        ui_power_on(); duell_mode_req = DM_LAYER; go_main_to(HP_MORPH); frames(160);   /* the owner's case 1: SAVE > MULTIS (LAYER mode's SAVE page), ALGORITHM turned (the part's row: the card row redrawn in place) */
        for (b = 0; b < NPAGES; b++)                    /* (straight to MULTIS: the SAVE family's slot page) */
            if (PAGES[b].graph == GR_SLOTS && page_visible(b)) {
                ui.page = (uint8_t)b;
                page_entered();
                break;
            }
        frames(300);
        if (cur_page()->graph != GR_SLOTS) { printf("uigaps: not on MULTIS: graph %u page %s\n", (unsigned)cur_page()->graph, cur_page()->title); ok = 0; }
        for (b = 0; b < 3u; b++) {
            nm += act_stripes(1u, EN_ALGO);
            frames(300);
        }
        ui_power_on(); go_main_to(HP_MORPH); frames(160);   /* the owner's case 2: QUICK SEQ (SEQ held), a key picks a tool (G3: REVERSE, A3: SHIFT <) */
        btn_down(B_SEQ); frames(700);
        if (ui.layer != LAYER_SEQ) { printf("uigaps: not in QUICK SEQ: layer %u\n", (unsigned)ui.layer); ok = 0; }
        nq += act_stripes(2u, 2u); frames(300);
        nq += act_stripes(2u, 4u); frames(300);
        btn_up(B_SEQ); frames(300);
        printf("uigaps: MULTIS + ALGORITHM: %u stripe pixels; QUICK SEQ + a key: %u stripe pixels%s\n", nm, nq, ok ? "" : " (FAIL: did not reach the page / the layer)");
        n += nm + nq;
        printf("uigaps: %s: a redraw within one layout sends no vertical stripe through the old picture (%u stripe pixels)\n", n || !ok ? "FAIL" : "ok", n);
        if (n || !ok) return 1;
    }
    for (g2 = 0; g2 < 64u; g2++) {
        if (!ncase[g2]) continue;
        printf("uigaps: grid %2u: %4u screens, the pre-fill sends %5llu px (was 46320: %.1f ms -> %.1f ms)\n", g2, ncase[g2], gpx[g2] / gn[g2], 46320 * 0.0008, gpx[g2] / gn[g2] * 0.0008);
    }
    printf("uigaps: %u screens redrawn on a poisoned screen, %u with a pixel nothing drew, %u pixels in live pictures differ from the old fill\n", ntot, nbadp, nlive);
    if (nbadp) { printf("uigaps: FAIL\n"); return 1; }
    return 0;
}
