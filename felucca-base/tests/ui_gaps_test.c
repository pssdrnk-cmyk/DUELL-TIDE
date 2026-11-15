/* SPDX-License-Identifier: GPL-3.0-only */
/* uigaps (b10 ui-a, Phase A cost #2; b13): a full redraw fills nothing first (b13: gfx.c gut_mark, ui_draw.c gut_flush: the BG goes, at the end of the pass, only where nothing drew).
 * Every page of every engine (LAYER / STEREO / DUEL, both parts) and the QUICK layers is redrawn (ui.force) on a screen poisoned with one colour: no pixel of it may be left (nothing is
 * left uncovered), and the picture equals the one drawn over the old whole-body fill (live pictures, LFO 2 / CYCLE ENV, move between the two frames: counted, not failed). Then no press
 * flashes (b13, owner on 1.0.0-b13: "flickering when using the quick menus and pressing buttons"). Built and run by host_tests.sh (name uigaps). */
#define UI_TEST_NO_MAIN 1
static int prefill_on = 1;
#define GRID_PREFILL() do { if (prefill_on) lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG); else gut_arm(T_BG); } while (0)   /* (1: the old whole-body fill, the reference) */
#include <stdlib.h>
static unsigned long long fillpx;
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
    ntot++, nbadp += pz != 0u, nlive += bad;
    if (pz)
        printf("uigaps: FAIL %s|%s|grid %u: %u pixels left uncovered\n", eng, key, g, pz);
    memcpy(host_screen, ref, sizeof ref);
}
/* a flash (b11: the gutters' stripes; b13: anywhere): pixels that, in one frame, pass through a value that is neither the old picture's nor the new one's (ui_test.c host_fl) */
static uint32_t frame_stripes(void)
{
    host_fl_begin();
    frame();
    return host_fl_end(0, 240);
}
/* one action watched frame by frame for 12 frames: kind 0 a button (a), 1 an encoder step (role a), 2 a key (a): the pixels that flashed (b13: a change of layout too) */
static uint32_t act_stripes(uint32_t kind, uint32_t a)
{
    uint32_t f, n = 0;
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
    return n;
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
    {   /* b13 (was b10: another layout filled the whole body first): no page change fills the whole body, another layout or the same one */
        uint32_t fx = 12u, edit1 = 22u, voice = 33u, ok = 1;
        prefill_on = 0;
        ui_power_on(); go_main_to(HP_MORPH); frames(64);
        ui.page = (uint8_t)fx; page_entered(); frames(40);
        ok &= grid_on == 1u && page_visible(edit1) && page_visible(voice);
        maxfill = 0; ui.page = (uint8_t)edit1; page_entered(); frame();
        ok &= grid_on == 14u && maxfill < 240ull * 190ull;                  /* FX (grid 1) -> EDIT 1 (grid 14) */
        frames(20);
        maxfill = 0; ui.page = (uint8_t)voice; page_entered(); frame();
        ok &= grid_on == 14u && maxfill < 240ull * 190ull;                  /* EDIT 1 -> VOICE 1/2 (both 14) */
        printf("uigaps: %s: no page change fills the whole body (another layout or the same)" "%c", ok ? "ok" : "FAIL", 10);
        if (!ok) return 1;
    }
    {   /* b13 (owner on 1.0.0-b13: "I've still noticed flickering on the screen when using the quick menus and pressing buttons"): every button, tapped and held (its quick layer
         * opened, a knob turned in it, let go), on MORPH, AMP ENV and AUTO TIDE; a dialog (PATCHES K3: a sound's delete) and a count-in's beats: no pixel flashes. Before: 88 of
         * these 162 actions flashed, up to 9,676 px (the whole-body fill before every change of layout, the MIXER's bands sent over its cards, the step strip over QUICK SEQ's cards) */
        static const char *const BASE[3] = {"MORPH", "AMP ENV", "AUTO TIDE"};
        uint32_t v, b, n, tot = 0, nbad = 0, k;
        prefill_on = 0;
        for (v = 0; v < 3u; v++)
            for (b = 0; b < NB; b++) {
                if (b == B_PLAY || b == B_SAVE)
                    continue;
                ui_power_on(); if (v) go_title(BASE[v]); else go_main_to(HP_MORPH); frames(400);
                btn_down(b);
                for (k = n = 0; k < 52u; k++)
                    n += frame_stripes();
                n += act_stripes(1u, EN_K2);
                btn_up(b);
                for (k = 0; k < 20u; k++)
                    n += frame_stripes();
                n += act_stripes(0u, b);                /* (then a tap: a page or a page of the family) */
                n += act_stripes(0u, b);
                if (n)
                    printf("uigaps: %s, %s held, K2, let go, tapped twice: %u pixels flashed\n", BASE[v], B_NAME[b], n), nbad++;
                tot += n;
            }
        ui_power_on(); go_main_to(HP_MORPH); frames(200);
        for (k = 0; k < NPAGES && strcmp(PAGES[k].title, "PATCHES"); k++)
            ;
        ui.page = (uint8_t)k; page_entered(); frames(300);
        n = act_stripes(1u, EN_K3);                     /* (PATCHES K3: the delete dialog) */
        if (!ui.confirm) { printf("uigaps: PATCHES K3 opened no dialog\n"); nbad++; }
        n += act_stripes(1u, EN_K3);
        ui_power_on(); go_main_to(HP_MORPH); frames(200);
        song.playing = 0; song.rec = 1; cin_total = 4; cin_left = 4;
        for (k = 0; k < 6u; k++)
            n += frame_stripes();
        for (b = 3u; b > 0u; b--) {                     /* (a count-in's beats: the screen drawn again each beat) */
            cin_left = b;
            for (k = 0; k < 6u; k++)
                n += frame_stripes();
        }
        song.rec = 0; cin_left = cin_total = 0;
        for (k = 0; k < 10u; k++)
            n += frame_stripes();
        if (n)
            printf("uigaps: the delete dialog and a count-in: %u pixels flashed\n", n), nbad++;
        tot += n;
        printf("uigaps: %s: no press flashes (every button tapped and held on 3 pages, a dialog, a count-in: %u pixels flashed in %u cases)\n", nbad ? "FAIL" : "ok", tot, nbad);
        if (nbad) return 1;
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
        printf("uigaps: grid %2u: %4u screens\n", g2, ncase[g2]);
    }
    printf("uigaps: %u screens redrawn on a poisoned screen, %u with a pixel nothing drew, %u pixels in live pictures differ from the old fill\n", ntot, nbadp, nlive);
    if (nbadp) { printf("uigaps: FAIL\n"); return 1; }
    return 0;
}
