/* SPDX-License-Identifier: GPL-3.0-only */
/* The update loader's screen in TIDE's look (TIDE 1.0.0-b5; b11, owner: "it's pink": the night sea; included by ldr_screen.c, which keeps the guest rules, ls_check and
 * the two entry points): a TIDE page on the ground colour, no boxes, no rule (firmware/src/ui_tokens.h, the TIDE palette).
 *     rows 40 .. 89    the splash's wordmark, where the splash has it (W1: the letters cut by the water line, foam above it, sea glass under it)
 *     row 116          WRITING                       face S, moonlight, centred
 *     row 150          45 %                          the percent as a big value (face N, foam) and its unit (face M, moonlight) on one baseline, centred
 *     rows 160 .. 166  the progress as the water line, x 22 .. 217: a 2 px wave, sea glass up to the percent, the line colour ahead, soft ends
 *     rows 186 .. 204  DO NOT SWITCH OFF             in a filled pill in amber (warn: "careful"), the text in the ground colour
 * Rows are the capitals' last rows. Only drawing lives here: buffers, colours, bitmaps, the band renderer. The data: ldr_screen_tide_data.h (tools/gen_loader_screen_tide.py). */
#include "ldr_screen_tide_data.h"

#define LS_BAND_H 44u                           /* the strip: 240 x 44 pixels, 21,120 B */
#define LS_UP_Y 124u                            /* the band that changes with the percent: the percent and the water line (rows 124 .. 167) */
#define LS_BAND_TAB 0, 44, 84, 124, 168, 212, 240   /* the whole screen in bands of LS_BAND_H rows at most, LS_UP_Y .. + LS_BAND_H one of them */
#define LS_NB 6u
#define LS_BUDGET_MS 400u                       /* a draw longer than this: the screen gives up */
#define LS_WM_X 24                              /* the wordmark's top-left: the splash's place */
#define LS_WM_Y 40
#define LS_WRITING_ROW 116                      /* capitals' last rows */
#define LS_PCT_ROW 150
#define LS_PILL_Y 186                           /* the pill's top row; its text's capitals end on row LS_PILL_Y + 14 */
#define LS_BAR_X0 22u                           /* the water line: x 22 .. 217 (both included), its middle on row LS_LINE_Y (1/16 px: + 8), 2 px thick, rows 160 .. 166 */
#define LS_BAR_X1 217u
#define LS_LINE_Y 163u
#define LS_SOFT 12u                             /* its soft ends, px */

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

/* the wordmark (16 colours, index 0 the ground: not drawn) at its place, the rows of it inside the strip */
static void ls_wordmark(uint32_t y0)
{
    uint32_t r, col, k;
    for (r = 0; r < LS_WM_H; r++) {
        const int32_t py = LS_WM_Y + (int32_t)r - (int32_t)y0;
        if (py < 0 || py >= (int32_t)LS_BAND_H)
            continue;
        for (col = 0, k = r * LS_WM_W; col < LS_WM_W; col++, k++) {
            const uint32_t i = (k & 1u) ? LS_WM[k >> 1] & 15u : LS_WM[k >> 1] >> 4;
            if (i)
                ls_buf[(uint32_t)py * 240u + LS_WM_X + col] = ls_sw(LS_WM_PAL[i]);
        }
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

static uint32_t ls_bar_end(uint32_t pct)        /* the sea glass's last column (pct > 0), in proportion */
{
    return LS_BAR_X0 + (LS_BAR_X1 - LS_BAR_X0) * pct / 100u;
}

/* the water line: per column its middle (1/16 px) on the swell, a 2 px stroke, each row's share of it its coverage; sea glass up to the percent, the line colour ahead;
 * the LS_SOFT columns at each end fade into the ground */
static void ls_water(uint32_t pct, uint32_t y0)
{
    uint32_t x, xe = pct ? ls_bar_end(pct) : 0u;
    for (x = LS_BAR_X0; x <= LS_BAR_X1; x++) {
        const int32_t m = (int32_t)LS_LINE_Y * 16 + 8 + LS_WAVE[(x - LS_BAR_X0) % LS_WAVE_N], top = m - 16, bot = m + 16;
        const uint32_t e = x - LS_BAR_X0 < LS_BAR_X1 - x ? x - LS_BAR_X0 : LS_BAR_X1 - x;
        const uint16_t c = pct && x <= xe ? LS_C0 : LS_LINE;
        int32_t row;
        for (row = top >> 4; row <= (bot - 1) >> 4; row++) {
            const int32_t a = (row * 16 + 16 < bot ? row * 16 + 16 : bot) - (row * 16 > top ? row * 16 : top), py = row - (int32_t)y0;
            if (a > 0 && py >= 0 && py < (int32_t)LS_BAND_H)
                ls_buf[(uint32_t)py * 240u + x] = ls_sw(ls_mix(LS_BG, c, (int32_t)((uint32_t)a * 100u / 16u * (e < LS_SOFT ? e + 1u : LS_SOFT) / LS_SOFT)));
        }
    }
}

static void ls_band(uint32_t y0, uint32_t h, uint32_t pct)   /* render rows y0 .. y0 + h of the screen into the strip and send them */
{
    uint32_t i, n = 240u * h;
    uint16_t bg = ls_sw(LS_BG);
    for (i = 0; i < n; i++)
        ls_buf[i] = bg;
    ls_wordmark(y0);
    LS_TEXT_AT(WRITING, 120 - LS_WRITING_ADV / 2, LS_WRITING_ROW, y0, LS_MID);
    if (y0 <= (uint32_t)LS_PCT_ROW && y0 + h > (uint32_t)LS_PCT_ROW - 24u)
        ls_percent(pct, y0);
    ls_water(pct, y0);
    ls_alpha(LS_PILL, LS_PILL_W, LS_PILL_H, 120 - LS_PILL_W / 2, LS_PILL_Y, y0, LS_WARN);
    LS_TEXT_AT(DONOT, 120 - LS_DONOT_ADV / 2, LS_PILL_Y + 14, y0, LS_BG);
    lcd_blit(0, y0, 240, h, ls_buf);
    lcd_sync();                                   /* the strip is sent before anything else runs (and before the next one is drawn into it) */
}
