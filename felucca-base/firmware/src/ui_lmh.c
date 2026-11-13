/* SPDX-License-Identifier: GPL-3.0-only */
/* b11 MIXER / QUICK GLO: LOW / MID / HIGH a part (owner: "display like the low mid high on the same design language we have"). One module, one call: ui_layer.c draw_layer
 * calls lmh_draw() each frame the MIXER or QUICK GLO is shown (template 7), in the level meter's place; drop this file and that call and the page has no meter.
 *
 * The levels: each part's LOW, MID and HIGH from the MORPH card's tap (fx.c wv_m: the part's finished block as one mono entry per two frames, 22.05 kHz, wvr_code; one writer a
 * part), read here in the main loop only while the page is shown (it opens the gate, wv_want, as the wave page does); nothing new in the audio interrupt. Two one-pole low-passes
 * split the new entries at ~250 Hz and ~2.5 kHz: LOW = the 250 Hz low-pass, MID = the 2.5 kHz low-pass less LOW, HIGH = the entry less the 2.5 kHz low-pass. Each band's mean
 * square over the new entries is its level (16 log2 of the RMS, gm_units' scale, x 16): up at once, down 36 dB/s as the meter; a peak hold a band (GM_HOLD_MS, then it falls).
 *
 * The look: in each MUTE card, under its row, three bands, HIGH on top, MID, LOW at the foot (pitch rises upward on every TIDE page), the label face's H M L left of them: each a
 * 4 px band with the note strip's rounded ends, the part's colour (DIM while the part is silent; ink on a muted card) on the LEVEL slider's dim track, filled on the LEVEL fader's
 * dB scale (lmh_px: a level of -6 dBFS ends where the LEVEL bar stands at -6.0 dB); the peak hold a 2 px tick as tall as the band (design review: the bands lead, not the ticks).
 * Each band its own small strip; after the first draw only the columns a band changed are sent */
#define LMH_X 15                                         /* the bands: from x 15 of the card to the LEVEL bar's right end */
#define LMH_W (UT_T4_W - UT_T4_PAD - LMH_X)
#define LMH_Y0 25                                        /* the H band's middle row in the MUTE card, the rows LMH_DY apart */
#define LMH_DY 12
#define LMH_BH 4
#define LMH_A1 2253                                      /* Q15: 1 - exp(-2 pi 250 / 22050) */
#define LMH_A2 16705                                     /* .. 2.5 kHz */
static struct {
    uint32_t rd[NSHOWN], on, t;                          /* the next entry to read a part; the gate was open last frame; fm1_ms then */
    int32_t z1[NSHOWN], z2[NSHOWN];                      /* the two low-passes' states (x 16) */
    int32_t lvl[NSHOWN][3], hold[NSHOWN][3];             /* LOW, MID, HIGH: units x 16 */
    uint32_t ht[NSHOWN][3];
    int16_t lp[NSHOWN][3], lh[NSHOWN][3];                /* what each band's strip shows: its fill's end, its tick */
    uint8_t lc[NSHOWN];                                  /* .. and the part's state it was drawn in (muted, sounding, the theme) */
} lmh __attribute__((section(".pool")));                 /* (the pool: zero at power-on, outside .bss, which is tight) */
static void lmh_step(void)
{
    uint32_t k, b, now = fm1_ms, dt = now - lmh.t;
    lmh.t = now;
    if (ui_prefs & PREF_ANIM_OFF)
        return;
    if (!lmh.on || (int32_t)(wv_want - wv_blk) <= 0) {   /* (the gate was shut: start at what the rings hold now) */
        lmh.rd[0] = wv_cnt[0];
        lmh.rd[1] = wv_cnt[1];
        dt = 33u;
    }
    if (dt > 200u)
        dt = 33u;
    wv_want = wv_blk + 256u;                             /* the one word the UI writes (ui_scope.c wl_tick) */
    lmh.on = 1;
    for (k = 0; k < NSHOWN; k++) {
        const uint32_t cnt = wv_cnt[k], gap = wv_gap[k];
        uint32_t n = cnt - lmh.rd[k], i;
        uint32_t e[3] = {0, 0, 0};                       /* (each entry's band >> 4 squared: 256^2 x 512 entries fits 32 bits) */
        int32_t z1 = lmh.z1[k], z2 = lmh.z2[k];
        if (n > WVR_N)
            n = WVR_N;
        if (cnt - gap < n)
            n = cnt - gap;                               /* (only what belongs to this sound) */
        for (i = cnt - n; i != cnt; i++) {
            const int32_t x = wl_dec(wv_m[k][i & (WVR_N - 1u)]) * 16;   /* (x 16: 65,536 at most; times a Q15 coefficient stays in 32 bits) */
            int32_t lo, mi, hi;
            z1 += ((x - z1) * LMH_A1) >> 15;
            z2 += ((x - z2) * LMH_A2) >> 15;
            lo = z1 >> 8, mi = (z2 - z1) >> 8, hi = (x - z2) >> 8;   /* (1/16 of the entry's scale: 256 at most) */
            e[0] += (uint32_t)(lo * lo), e[1] += (uint32_t)(mi * mi), e[2] += (uint32_t)(hi * hi);
        }
        lmh.z1[k] = z1, lmh.z2[k] = z2;
        lmh.rd[k] = cnt;
        for (b = 0; b < 3u; b++) {                       /* (the meter's own rules: up at once, down 36 dB/s; the hold GM_HOLD_MS, then down to the level) */
            const int32_t u = n && glo_sounding(k) ? gm_units((int32_t)isqrt32(e[b] / n) * 128) * 16 : 0;   /* (the RMS, 1/256 of the half scale: x 128 to Q15 full scale) */
            int32_t *lv = &lmh.lvl[k][b], *hd = &lmh.hold[k][b];
            *lv = u > *lv ? u : *lv - (int32_t)(dt * 3u >> 1);
            if (*lv < 0)
                *lv = 0;
            if (u >= *hd) {
                *hd = u;
                lmh.ht[k][b] = now;
            } else if (now - lmh.ht[k][b] > GM_HOLD_MS) {
                *hd -= (int32_t)(dt * 3u >> 1);
                if (*hd < *lv)
                    *hd = *lv;
            }
            if (!glo_sounding(k))                        /* (a muted or stopped part: no tick left standing over its empty band) */
                *hd = *lv;
        }
    }
}
static int32_t lmh_px(int32_t u16)                       /* units x 16 (16 an octave, 0 dBFS at 240) -> the band's pixels, on the LEVEL fader's scale */
{
    const int32_t dbx10 = (u16 / 16 - 240) * 602 / 160;
    int32_t v = 0, lo = 1, hi = 127;
    if (u16 <= 0)
        return 0;
    while (lo <= hi) {                                   /* (the highest LEVEL step at or under the level: the table rises with the step) */
        const int32_t m = (lo + hi) / 2;
        if (LEVEL_DB_X10[m] <= dbx10)
            v = m, lo = m + 1;
        else
            hi = m - 1;
    }
    return v * LMH_W / 127;
}
/* one band's columns x0 .. x1 - 1 (of LMH_W), drawn whole into a canvas that wide and sent: the track, the fill to px, the tick at hp */
static void lmh_cols(uint32_t k, uint32_t r, int32_t x0, int32_t x1, int32_t px, int32_t hp, uint16_t under, uint16_t track, uint16_t col, uint16_t tick)
{
    x0 = x0 < 0 ? 0 : x0;
    x1 = x1 > LMH_W ? LMH_W : x1;
    if (x1 <= x0)
        return;
    cv_begin((uint32_t)(x1 - x0), LMH_BH, under);
    cv_rrect(-x0, 0, LMH_W, LMH_BH, 2, track, under);
    if (px > 0)
        cv_rrect(-x0, 0, px > 4 ? px : 4, LMH_BH, 2, col, track);
    if (hp > 0)
        cv_rect((hp < LMH_W - 2 ? hp : LMH_W - 2) - x0, 0, 2, LMH_BH, tick);
    cv_blit((uint32_t)(UT_T4_X0 + UT_T4_DX * (int32_t)k + LMH_X + x0), (uint32_t)(UT_T4_MUTE_Y + LMH_Y0 - LMH_BH / 2 + (int32_t)r * LMH_DY));
}
static void lmh_draw(void)
{
    static const char *const LB[3] = {"H", "M", "L"};
    uint32_t k, r;
    const int force = ui.force || gm.sig != 0x1u;        /* (the panel's redraw clears gm.sig: draw_layer) */
    lmh_step();
    for (k = 0; k < NSHOWN; k++) {
        const int mu = trk[k].p[P_MUTE] != 0;
        const uint16_t under = mu ? T_PTHEME(k) : T_BG, col = mu ? T_INK : glo_sounding(k) ? T_PTHEME(k) : T_DIM;
        const uint16_t track = mu ? ux_mix(under, T_INK, 30) : ux_mix(T_BASE_P1, T_TEXT, UT_T4_TRACK_PCT), tick = mu ? T_INK : T_TEXT;
        const uint8_t st = (uint8_t)(mu * 2 + glo_sounding(k) + ux.gen * 4u);
        const int whole = force || st != lmh.lc[k];
        lmh.lc[k] = st;
        if (whole) {                                     /* the H M L labels, in the label face (their own strip: the card under them is the panel's) */
            cv_begin(LMH_X - 2, 2 * LMH_DY + 12, under);
            for (r = 0; r < 3u; r++)
                cv_text_on(UT_T4_PAD - 1, UT_TY(S, 11 + (int32_t)r * LMH_DY), &AF_S, LB[r], mu ? T_INK : T_MID, under);
            cv_blit((uint32_t)(UT_T4_X0 + UT_T4_DX * (int32_t)k + 2), (uint32_t)(UT_T4_MUTE_Y + LMH_Y0 - 6));
        }
        for (r = 0; r < 3u; r++) {
            const uint32_t b = 2u - r;                   /* (row 0 HIGH, row 2 LOW) */
            const int32_t px = lmh_px(lmh.lvl[k][b]), hp = lmh_px(lmh.hold[k][b]), p0 = lmh.lp[k][r], h0 = lmh.lh[k][r];
            if (whole)
                lmh_cols(k, r, 0, LMH_W, px, hp, under, track, col, tick);
            else if (px != p0 || hp != h0) {            /* only the columns that changed: from the fill's old or new end to the tick's old or new place (the rounded end 3 px either side) */
                int32_t a0 = px < p0 ? px : p0, a1 = px > p0 ? px : p0;
                if (hp != h0) {
                    a0 = a0 < (hp < h0 ? hp : h0) ? a0 : (hp < h0 ? hp : h0);
                    a1 = a1 > (hp > h0 ? hp : h0) ? a1 : (hp > h0 ? hp : h0);
                }
                lmh_cols(k, r, a0 - 3, a1 + 3, px, hp, under, track, col, tick);
            }
            lmh.lp[k][r] = (int16_t)px;
            lmh.lh[k][r] = (int16_t)hp;
        }
    }
    gm.sig = 0x1u;                                       /* (the strips are drawn) */
}
