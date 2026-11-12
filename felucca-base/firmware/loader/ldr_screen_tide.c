/* SPDX-License-Identifier: GPL-3.0-only */
/* The update loader's screen in TIDE's look (TIDE 1.0.0-b5; included by ldr_screen.c, which keeps the guest rules, ls_check and the two entry points):
 * a TIDE page (firmware/src/ui_tokens.h): plain background, no boxes, no rule under the title.
 *     row 16   UPDATE                        the header's page title (face T, T_TEXT, centred)
 *     row 74   WRITING                       face S, T_MID, centred
 *     row 108  45 %                          the percent as a big value (face N, T_TEXT) and its unit (face M, T_MID) on one baseline, centred
 *     rows 120 .. 125                        the capsule bar, x 22 .. 217: the part 1 -> part 2 colour over the T_RAISE track, both ends round
 *     rows 156 .. 174  DO NOT SWITCH OFF     in a filled pill (T_THEME), the text in T_BG
*     row 231  TIDE                         the footer row (face S, T_MID)
 * Rows are the capitals' last rows. Only drawing lives here: buffers, colours, bitmaps, the band renderer. The data: ldr_screen_tide_data.h (tools/gen_loader_screen_tide.py). */
#include "ldr_screen_tide_data.h"

#define LS_BAND_H 44u                           /* the strip: 240 x 44 pixels, 21,120 B */
#define LS_UP_Y 84u                             /* the band that changes with the percent: the percent and the bar (rows 84 .. 127) */
#define LS_BAND_TAB 0, 44, 84, 128, 172, 216, 240   /* the whole screen in bands of LS_BAND_H rows at most, LS_UP_Y .. + LS_BAND_H one of them */
#define LS_NB 6u
#define LS_BUDGET_MS 400u                       /* a draw longer than this: the screen gives up */
#define LS_TITLE_ROW 16                         /* capitals' last rows */
#define LS_WRITING_ROW 74
#define LS_PCT_ROW 108
#define LS_PILL_Y 156                           /* the pill's top row; its text's capitals end on row LS_PILL_Y + 14 */
#define LS_NAME_ROW 231
#define LS_NAME_X 5                             /* the footer's first pen (UT_FOOT_X) */
#define LS_BAR_X0 22u                           /* the bar: x 22 .. 217 (both included), rows 120 .. 125 */
#define LS_BAR_X1 217u
#define LS_BAR_Y 120u

static uint16_t ls_buf[240u * LS_BAND_H];       /* byte-swapped RGB565, the order the panel takes */
static uint8_t ls_on;                           /* 1: drawing; 0: never started or given up */
static uint8_t ls_pct = 255u;                   /* the percent on the panel (255: none yet) */
static uint32_t ls_pct_hist[2];                 /* (the host test reads these) last percent drawn and the number of draws */
static uint32_t ls_t0;

static uint16_t ls_sw(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

static uint16_t ls_mix(uint16_t a, uint16_t b, int32_t pct)   /* ui_mix: a + (b - a) * pct / 100 per channel, native RGB565 */
{
    static const uint8_t SH[3] = {11, 5, 0}, MK[3] = {31, 63, 31};
    uint32_t out = 0, k;
    for (k = 0; k < 3u; k++) {
        int32_t x = (a >> SH[k]) & MK[k], y = (b >> SH[k]) & MK[k], d = (y - x) * pct;
        out |= (uint32_t)(x + (d + (d >= 0 ? 50 : -50)) / 100) << SH[k];
    }
    return (uint16_t)out;
}

/* a 4-bit alpha bitmap (w x h, two pixels a byte) in colour c, its top-left at screen (x, y), blended over what the strip (rows y0 ..) holds */
static void ls_alpha(const uint8_t *d, uint32_t w, uint32_t h, int32_t x, int32_t y, uint32_t y0, uint16_t c)
{
    uint32_t r, k = 0, col;
    for (r = 0; r < h; r++)
        for (col = 0; col < w; col++, k++) {
            uint32_t a = (k & 1u) ? d[k >> 1] & 15u : d[k >> 1] >> 4;
            int32_t py = y + (int32_t)r - (int32_t)y0, px = x + (int32_t)col;
            if (a && py >= 0 && py < (int32_t)LS_BAND_H && px >= 0 && px < 240)
                ls_buf[(uint32_t)py * 240u + (uint32_t)px] = ls_sw(ls_mix(ls_sw(ls_buf[(uint32_t)py * 240u + (uint32_t)px]), c, (int32_t)(a * 100u / 15u)));
        }
}

/* a text bitmap NAME (ldr_screen_tide_data.h) with its pen at x and its capitals' last row at row */
#define LS_TEXT_AT(NAME, x, row, y0, c) \
    ls_alpha(LS_##NAME, LS_##NAME##_W, LS_##NAME##_H, (int32_t)(x) + LS_##NAME##_BX, (int32_t)(row) + 1 + LS_##NAME##_BY, y0, c)

static void ls_percent(uint32_t pct, uint32_t y0)   /* "45" (face N) and "%" (face M), centred on x 120 */
{
    char s[3];
    uint32_t n = 0, i, adv = LS_PCT_ADV;
    int32_t x;
    if (pct >= 100u)
        s[n++] = 1;
    if (pct >= 10u)
        s[n++] = (char)(pct / 10u % 10u);
    s[n++] = (char)(pct % 10u);
    for (i = 0; i < n; i++)
        adv += LS_GLYPH[(uint32_t)(uint8_t)s[i] * 7u + 6u];
    x = 120 - (int32_t)(adv / 2u);
    for (i = 0; i < n; i++) {
        const uint8_t *g = LS_GLYPH + (uint32_t)(uint8_t)s[i] * 7u;
        ls_alpha(LS_GDATA + (g[0] | g[1] << 8), g[2], g[3], x + (int32_t)(int8_t)g[4], LS_PCT_ROW + 1 + (int32_t)(int8_t)g[5], y0, LS_TEXT);
        x += g[6];
    }
    LS_TEXT_AT(PCT, x, LS_PCT_ROW, y0, LS_MID);
}

static uint32_t ls_bar_end(uint32_t pct)        /* the fill's last column (pct > 0): in proportion, never shorter than its two round ends */
{
    uint32_t e = LS_BAR_X0 + (LS_BAR_X1 - LS_BAR_X0) * pct / 100u;
    return e < LS_BAR_X0 + LS_BAR_H - 1u ? LS_BAR_X0 + LS_BAR_H - 1u : e;
}

static uint32_t ls_cap(uint32_t a, uint32_t b, uint32_t x, uint32_t r)   /* the coverage (0 .. 15) of a capsule spanning columns a .. b at column x, row r */
{
    if (x - a < LS_BAR_H / 2u)
        return LS_CAPEND[r * (LS_BAR_H / 2u) + x - a];
    if (b - x < LS_BAR_H / 2u)
        return LS_CAPEND[r * (LS_BAR_H / 2u) + b - x];
    return 15u;
}

static void ls_bar(uint32_t pct, uint32_t y0)
{
    uint32_t r, x, xe = pct ? ls_bar_end(pct) : 0u;
    for (r = 0; r < LS_BAR_H; r++) {
        uint32_t y = LS_BAR_Y + r;
        if (y < y0 || y >= y0 + LS_BAND_H)
            continue;
        for (x = LS_BAR_X0; x <= LS_BAR_X1; x++) {
            uint16_t c = ls_mix(LS_BG, LS_RAISE_, (int32_t)(ls_cap(LS_BAR_X0, LS_BAR_X1, x, r) * 100u / 15u));
            if (pct && x <= xe)
                c = ls_mix(c, ls_mix(LS_C0, LS_C2, (int32_t)((x - LS_BAR_X0) * 100u / (LS_BAR_X1 - LS_BAR_X0))), (int32_t)(ls_cap(LS_BAR_X0, xe, x, r) * 100u / 15u));
            ls_buf[(y - y0) * 240u + x] = ls_sw(c);
        }
    }
}

static void ls_band(uint32_t y0, uint32_t h, uint32_t pct)   /* render rows y0 .. y0 + h of the screen into the strip and send them */
{
    uint32_t i, n = 240u * h;
    uint16_t bg = ls_sw(LS_BG);
    for (i = 0; i < n; i++)
        ls_buf[i] = bg;
    LS_TEXT_AT(TITLE, 120 - LS_TITLE_ADV / 2, LS_TITLE_ROW, y0, LS_TEXT);
    LS_TEXT_AT(WRITING, 120 - LS_WRITING_ADV / 2, LS_WRITING_ROW, y0, LS_MID);
    if (y0 <= (uint32_t)LS_PCT_ROW && y0 + h > (uint32_t)LS_PCT_ROW - 24u)
        ls_percent(pct, y0);
    ls_bar(pct, y0);
    ls_alpha(LS_PILL, LS_PILL_W, LS_PILL_H, 120 - LS_PILL_W / 2, LS_PILL_Y, y0, LS_C0);
    LS_TEXT_AT(DONOT, 120 - LS_DONOT_ADV / 2, LS_PILL_Y + 14, y0, LS_BG);
    LS_TEXT_AT(NAME, LS_NAME_X, LS_NAME_ROW, y0, LS_MID);
    lcd_blit(0, y0, 240, h, ls_buf);
    lcd_sync();                                   /* the strip is sent before anything else runs (and before the next one is drawn into it) */
}
