/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* The update loader's screen (DUELL 1.0; TIDE builds draw it in TIDE's look: ldr_screen_tide.c, the same rules below): while the firmware is written the panel shows the DUELL word over the splash's wave lines, WRITING and the percent, a segmented
 * bar (20 segments of 5 percent) and DO NOT SWITCH OFF, as in release_drafts/LOADER_SCREEN_DESIGN.png (tools/loader_screen_design.py is the same picture).
 *
 * The write comes first, always. The screen is a guest of the loader:
 *   - it is drawn from two places only: ldr_screen_init() (before USB starts: the panel's init takes ~0.5 s, the host's first enumeration waits for it) and
 *     ldr_screen_progress() (after a sector is written and verified: the flash is idle then, nothing of the write runs at the same time);
 *   - every panel call is bounded (src/lcd.c: spins and fm1_lcd_wait have a timeout counter), the watchdog is fed between bands, and ONE failure of any kind
 *     (a timeout of the panel's SPI, a draw that took more than LS_BUDGET_MS) switches the screen off for good: from then on both functions return at once
 *     and the loader runs as if it had no screen;
 *   - it never returns a value, never touches the flash, the package, the USB state or the update record; its RAM is its own (a 240 x 40 strip, the wave points).
 * LDR_SCREEN 0 (tools/build.py, DUELL_LOADER_SCREEN=0) compiles both functions to nothing: the loader-free fallback package.
 * Needs (loader.c, tests/ldr_test.c): lcd.c and its hal functions, fm1_ticks / FM1_TICKS_PER_US, fm1_wdt_feed. Integer only, no 64-bit arithmetic. */
#ifndef LDR_SCREEN
#define LDR_SCREEN 1
#endif

#if LDR_SCREEN
#include "ldr_screen_tide.c"                    /* TIDE 1.0.0-b5: the same screen in TIDE's look (drawing only: the guest rules below are the same) */

static void ls_give_up(void) { ls_on = 0; }

static void ls_check(uint32_t timeouts0)         /* one failure ends the screen */
{
    uint32_t took = (fm1_ticks() - ls_t0) / (1000u * FM1_TICKS_PER_US);
    if (fm1_lcd_timeouts != timeouts0 || took > LS_BUDGET_MS)
        ls_give_up();
}

/* before usb_start(): the panel's init and the whole screen at 0 percent */
static void ldr_screen_init(void)
{
    static const uint8_t BAND[LS_NB + 1u] = {LS_BAND_TAB};
    uint32_t t0 = fm1_lcd_timeouts, b;
    ls_t0 = fm1_ticks();
    lcd_init();
    fm1_lcd_backlight(1);
    fm1_wdt_feed();
    ls_on = 1;
    for (b = 0; b < LS_NB && ls_on; b++) {
        fm1_wdt_feed();
        ls_band(BAND[b], (uint32_t)(BAND[b + 1u] - BAND[b]), 0u);
        if (fm1_lcd_timeouts != t0)
            ls_give_up();
    }
    ls_pct = 0u;
    ls_pct_hist[0] = 0u;
}

/* after a sector is written and verified: the percent of the app area, the strip with WRITING, the percent and the bar redrawn when the percent changed */
static void ldr_screen_progress(uint32_t done, uint32_t total)
{
    uint32_t pct, t0;
    if (!ls_on || !total)
        return;
    pct = done >= total ? 100u : done * 100u / total;
    if (pct == ls_pct)
        return;
    t0 = fm1_lcd_timeouts;
    ls_t0 = fm1_ticks();
    fm1_wdt_feed();
    ls_band(LS_UP_Y, LS_BAND_H, pct);
    ls_pct = (uint8_t)pct;
    ls_pct_hist[0] = pct;
    ls_pct_hist[1]++;
    ls_check(t0);
}
#else
static void ldr_screen_init(void) {}
static void ldr_screen_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#endif
