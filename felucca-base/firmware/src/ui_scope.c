/* SPDX-License-Identifier: GPL-3.0-only */
/* The MORPH page's picture (DUELL_TIDE): the part's real sound, from fx.c's scope tap (wv_m, 512 mono entries of 8 bits a part at 22.05 kHz), drawn as a stack of ridges (docs/design/WAVE_DISPLAY_PLAN.md 3.2,
 * reduced to the RAM there is; the owner's pick from the WAVE LAB preview on plan/wave-displays). Included by ui_graph.c before graph_morph. Core 0, main loop, nothing in the audio path.
 * Per part and picture step (about 16 a second, the page's step: mv_tick):
 *   1 gate    wv_want = "wanted until block N", 256 blocks (190 ms) ahead; MENU > ANIM OFF shuts it: no tap, the last picture stays (a flat line before there was one)
 *   2 read    the newest 400 entries or fewer, only those written since the gate opened and since the part's last hole (wv_gap); read IN PLACE, the counter looked at again afterwards:
 *             if the writer went on by more than the ring leaves (512 less 400, less a block) the picture is made again, once, else the last one stays
 *   3 trigger the ring smoothed (a 10-point kernel); rising crossings of its middle that come after a clear dip (the Pico scope's hysteresis), found to 1/16 of an entry; of those in the last 3.5
 *             periods the steepest (70 % of the steepest), and of those the one whose first 16 points match the last picture's best
 *   4 window  2.5 periods of the median crossing distance when it is steady (160 .. 360 entries), else the last one
 *   5 ridge   72 points (1 to 5 linear samples across each point's share of the window), smoothed over neighbours twice, the middle of the line's range taken out
 *   6 gain    the peak of the window: a rise of more than 6 % at once, a fall of more than 12 % by 1/16 a step (about a second), never under WL_FLOOR: soft is small, loud fills, a beat stays a beat
 *   7 stack   the last 4 ridges, the newest in front, each older one a step higher and fainter, every line hiding what is under it; silence: one flat line in the middle
 * Samples inside are the ring's 8-bit steps times 16 (the interpolation's fraction), so a crossing keeps its 1/16 entry on a quiet note. */
#define WL_NP 72                                     /* points of a ridge: x = WL_X0 + 3 p (22 .. 235: the bar and the part's number stand to the left) */
#define WL_X0 22
#define WL_HN 4                                      /* ridges kept */
#define WL_R 400u                                    /* entries read (ring 512) */
#define WL_MINW 160u
#define WL_MAXW 360u
#define WL_DEFW 300u
#define WL_FLOOR 192                                 /* the smallest peak the gain scales to: 12 steps of the ring (1,500 of a half-scale mix) */
#define WL_NC 24
typedef struct {
    uint32_t base;                                   /* wv_cnt where the gate last opened */
    uint32_t gpk;                                    /* the slow gain's peak */
    int16_t first[16];                               /* the last picture's first 16 points (the match) */
    int8_t dy[WL_HN][WL_NP];                         /* the ridges as drawn, px from their middle: the newest first */
    uint16_t w;                                      /* the window */
    uint8_t got, have, h, hn;                        /* a picture exists since the last reset; one with a trigger to match; the half height it was made for; ridges kept */
} wl_t;
static wl_t wl[2];
static uint32_t wl_tk = 0xFFFFFFFFu;
_Static_assert(NLIVE == 2, "ui_scope.c keeps one reader state for each of the two parts");
static uint32_t wl_k, wl_b;                          /* the ring being read, and the counter value of its entry 0 */
#define WLR(i) ((int32_t)wv_m[wl_k][(wl_b + (uint32_t)(i)) & (WVR_N - 1u)])

static int32_t wl_abs(int32_t v) { return v < 0 ? -v : v; }
/* the ring smoothed for the trigger, entry i of n, times 32 (a 10-point kernel: 3 points, then 8: a ring on a resonant wave no longer moves the crossing) */
static int32_t wl_g(int32_t i, int32_t n)
{
    static const uint8_t K[10] = {1, 3, 4, 4, 4, 4, 4, 4, 3, 1};
    int32_t j, s = 0;
    for (j = 0; j < 10; j++) {
        int32_t x = i + j - 5;
        s += K[j] * WLR(x < 0 ? 0 : x >= n ? n - 1 : x);
    }
    return s;
}
/* the ring at the position q (1/16 entry), linear between the two entries, times 16 */
static int32_t wl_at(uint32_t q)
{
    uint32_t i = q >> 4, f = q & 15u;
    return WLR(i) * (int32_t)(16u - f) + WLR(i + 1u) * (int32_t)f;
}
/* point p of w entries from the position cq (1/16 entry): the mean of 1 to 5 samples across the point's share of the window, times 16 */
static int32_t wl_pt(uint32_t cq, uint32_t w, uint32_t p)
{
    uint32_t span = w * 16u / WL_NP, k = 1u + span / 16u, m;
    int32_t sum = 0;
    for (m = 0; m < k; m++)
        sum += wl_at(cq + (p * w * 16u) / WL_NP + (2u * m + 1u) * span / (2u * k));
    return sum / (int32_t)k;
}
/* one picture of the part wl_k from the n newest entries: the ridge into nd, the next first[] into nf, the window into *nw, the gain into *ng. 1: made, 0: nothing there but dither (flat) */
static int wl_pic(const wl_t *s, int32_t n, int32_t hs, int8_t *nd, int16_t *nf, uint32_t *nw, uint32_t *ng)
{
    int32_t cand[WL_NC], slope[WL_NC], mx = -32768, mn = 32767, rmx = -128, rmn = 127, mid, thr, i, j, pk = 0, rm, last, maxs = 0, g, hh = hs * 15 / 16;
    uint32_t nc = 0, armed = 0, per = 0, w, span, cq, np = 0, pool[WL_NC], best = 0, sel = 0, p;
    int32_t v[WL_NP], prev, cur;
    for (i = 0; i < n; i++) {                        /* the range of the raw ring, and of the trigger's line without the ends of the copy (the smoothing there leans on one side) */
        int32_t r = WLR(i);
        rmx = r > rmx ? r : rmx;
        rmn = r < rmn ? r : rmn;
        if (i >= 8 && i + 8 < n) {
            int32_t q = wl_g(i, n);
            mx = q > mx ? q : mx;
            mn = q < mn ? q : mn;
        }
    }
    mid = (mx + mn) / 2;
    thr = (mx - mn) / 8;
    prev = wl_g(0, n);
    for (i = 1; i < n; i++) {                        /* the rising crossings of the trigger's line, armed only after a clear dip (a full table drops its oldest) */
        cur = wl_g(i, n);
        if (cur < mid - thr)
            armed = 1;
        if (armed && prev < mid && cur >= mid) {
            if (nc == WL_NC) {
                for (j = 1; j < WL_NC; j++) {
                    cand[j - 1] = cand[j];
                    slope[j - 1] = slope[j];
                }
                nc--;
            }
            cand[nc] = (i - 1) * 16 + ((mid - prev) * 16) / (cur - prev);   /* (Q4: the crossing between entries i - 1 and i) */
            slope[nc] = wl_g(i + 2 < n ? i + 2 : n - 1, n) - wl_g(i >= 2 ? i - 2 : 0, n);
            nc++;
            armed = 0;
        }
        prev = cur;
    }
    if (nc >= 2u) {                                  /* the period: the median distance, and is it steady (the distances within a fifth of it; two crossings are one distance: steady) */
        int32_t d[WL_NC], t, m;
        uint32_t q;
        for (j = 1; j < (int32_t)nc; j++)
            d[j - 1] = cand[j] - cand[j - 1];
        for (j = 1; j + 1 < (int32_t)nc; j++) {
            t = d[j];
            for (q = (uint32_t)j; q > 0 && d[q - 1u] > t; q--)
                d[q] = d[q - 1u];
            d[q] = t;
        }
        m = d[(nc - 2u) / 2u];       /* (for two crossings d[0]) */
        if (m > 0 && d[0] * 5 >= m * 4 && d[nc - 2u] * 4 <= m * 5)
            per = (uint32_t)m / 16u;
    }
    w = per ? per * 5u / 2u : s->w ? s->w : WL_DEFW;
    if (per && w + per + 46u > (uint32_t)n)          /* the trigger needs a period of room before the window (n is 400 at most: 1.8 cycles of a 110 Hz note, less for a lower one) */
        w = (uint32_t)n > per + 46u ? (uint32_t)n - per - 46u : 0u;
    w = w < WL_MINW ? WL_MINW : w > WL_MAXW ? WL_MAXW : w;
    if (s->w && (w > s->w ? w - s->w : s->w - w) * 5u < s->w)   /* (within a fifth of the last: the same, so the picture does not breathe) */
        w = s->w;
    if (w + 40u > (uint32_t)n)
        w = (uint32_t)n - 40u;
    span = (per ? per * 7u / 2u : WL_DEFW) * 16u;
    last = (n - (int32_t)w - 6) * 16;                /* (Q4: the newest start that leaves the whole window and the interpolation's room) */
    for (j = 0; j < (int32_t)nc; j++)
        if (cand[j] <= last && cand[j] + (int32_t)span >= last && slope[j] > maxs)
            maxs = slope[j];
    if (!maxs)                                       /* none in the span: the newest that fits */
        for (j = 0; j < (int32_t)nc; j++)
            if (cand[j] <= last)
                sel = (uint32_t)cand[j] + 1u;
    for (j = 0; maxs && j < (int32_t)nc; j++)
        if (cand[j] <= last && cand[j] + (int32_t)span >= last && slope[j] * 10 >= maxs * 7)
            pool[np++] = (uint32_t)j;
    if (np) {
        int32_t bs = 0x7FFFFFFF;
        for (j = 0; j < (int32_t)np; j++) {
            int32_t sad = 0;
            if (s->have) {
                for (p = 0; p < 16u; p++)
                    sad += wl_abs(wl_pt((uint32_t)cand[pool[j]], w, p) - s->first[p]);
            } else {
                sad = 1000000 - slope[pool[j]];
            }
            if (sad < bs) {
                bs = sad;
                best = pool[j];
            }
        }
        sel = (uint32_t)cand[best] + 1u;
    }
    cq = sel ? sel - 1u : (uint32_t)(n - (int32_t)w - 6) * 16u;   /* no crossing at all (noise, a drone): the newest window, free */
    *nw = w;
    for (p = 0; p < 16u; p++)
        nf[p] = (int16_t)wl_pt(cq, w, p);
    rm = (rmx + rmn) * 8;                            /* AC coupled: the middle of the line's range over everything read (times 16) */
    for (p = 0; p < WL_NP; p++)
        v[p] = wl_pt(cq, w, p) - rm;
    for (j = 0; j < 2; j++) {                        /* smoothed over the neighbours (3 points, twice: the Pico scope's finding) */
        int32_t pv = v[0], nv;
        for (p = 1; p + 1u < WL_NP; p++) {
            nv = (pv + 2 * v[p] + v[p + 1u]) / 4;
            pv = v[p];
            v[p] = nv;
        }
    }
    for (p = 0; p < WL_NP; p++)
        pk = wl_abs(v[p]) > pk ? wl_abs(v[p]) : pk;
    if (pk < 8)                                      /* under half a step: nothing there but dither */
        return 0;
    g = (int32_t)*ng;
    if ((uint32_t)pk > *ng + (*ng >> 4))             /* the gain: a rise of more than 6 % at once, */
        g = pk;
    else if ((uint32_t)pk + (*ng >> 3) < *ng)        /* a fall of more than 12 %: 1/16 a step, */
        g -= g >> 4;                                 /* otherwise it stays: a steady note does not breathe */
    *ng = (uint32_t)g;
    g = g > WL_FLOOR ? g : WL_FLOOR;
    if (pk * hh < g)                                 /* under a pixel high: a flat line, not a stack of them */
        return 0;
    for (p = 0; p < WL_NP; p++)
        nd[p] = (int8_t)clamp(-(v[p] * hh / g), -hs, hs);
    return 1;
}
/* a picture becomes the newest ridge, the others move back */
static void wl_push(wl_t *s, const int8_t *nd)
{
    uint32_t i;
    for (i = sizeof s->dy - 1u; i >= sizeof s->dy[0]; i--)   /* (backwards: the ridges overlap) */
        ((int8_t *)s->dy)[i] = ((int8_t *)s->dy)[i - sizeof s->dy[0]];
    memcpy(s->dy[0], nd, sizeof s->dy[0]);
    s->hn = s->hn < WL_HN ? s->hn + 1u : WL_HN;
    s->got = 1;
}
/* one part's picture step. hs: the half height in px that the lines may use */
static void wl_step(uint32_t k, int32_t hs)
{
    wl_t *s = &wl[k];
    uint32_t c0, c1, vs, avail, tries, nw = s->w, ng = s->gpk;
    int32_t n;
    int8_t nd[WL_NP];
    int16_t nf[16];
    int r = 0;
    memset(nd, 0, sizeof nd);
    if (s->h != (uint8_t)hs) {                       /* the box changed (BOTH, the layout): start over */
        memset(s->dy, 0, sizeof s->dy);
        s->h = (uint8_t)hs;
        s->have = s->hn = s->got = 0;
    }
    if (wv_sil[k]) {                                 /* the part is silent: one flat line */
        s->hn = 0;
        wl_push(s, nd);
        s->have = 0;
        s->gpk -= s->gpk >> 4;
        return;
    }
    for (tries = 0; tries < 2u; tries++) {           /* read in place, and look at the counter again */
        c0 = wv_cnt[k];
        vs = (int32_t)(s->base - wv_gap[k]) > 0 ? s->base : wv_gap[k];
        avail = (int32_t)(c0 - vs) > 0 ? c0 - vs : 0u;
        n = (int32_t)(avail < WL_R ? avail : WL_R);
        if (n < 200)
            return;                                  /* not enough since the gate opened or the hole: the last picture stays */
        wl_k = k;
        wl_b = c0 - (uint32_t)n;
        nw = s->w;
        ng = s->gpk;
        r = wl_pic(s, n, hs, nd, nf, &nw, &ng);
#ifdef WL_TEST_AFTER_COPY
        WL_TEST_AFTER_COPY(k);                       /* (host test: the writer moves on while the reader reads) */
#endif
        __asm__ volatile("" ::: "memory");
        c1 = wv_cnt[k];
        if (c1 - c0 + 16u <= WVR_N - (uint32_t)n)    /* (and the block the writer may be in the middle of: 16 entries) */
            break;
        if (tries)
            return;                                  /* torn twice: the last picture stays */
    }
    s->w = (uint16_t)nw;
    s->gpk = ng;
    if (!r) {                                        /* nothing there but dither: one flat line */
        s->hn = 0;
        s->gpk -= s->gpk >> 4;
        wl_push(s, nd);
        s->have = 0;
        return;
    }
    memcpy(s->first, nf, sizeof nf);
    s->have = 1;
    wl_push(s, nd);
}
/* a 1 px line from (x0, y0) to (x1, y1), clipped to the rows ymin .. ymax (mv_line's box is the old card's) */
static void wl_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t ymin, int32_t ymax)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, guard = 400;
    while (guard--) {
        int32_t e2 = 2 * err;
        if (y0 >= ymin && y0 <= ymax)
            cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
/* part k's ridges in rows y0 .. y1 - 1: the newest in front (lowest), each older one a step higher and fainter, every line hiding what is under it; the block of ridges is centred, one ridge (silence: a flat line)
 * is the middle. The selected part (song.sel) is in full brightness with a bar at the half's left edge (a second cue that needs no colour: POLICE has one colour a part, GREY little); the other is dimmed */
static void wl_stack(uint32_t k, int32_t y0, int32_t y1)
{
    const wl_t *s = &wl[k];
    const int32_t hot = k == song.sel, pct = (int32_t)morph_fader_of(k) * 100 / 127, hs = (y1 - y0) / 2 - 1;
    int32_t amp = hs * 2 / 5, nr, nf, step, i, p, nd;
    if (hs < 4)
        return;
    {   /* the selected part: a bar at the half's edge and its number in a filled pill (AUTO TIDE's cue: it works on MONO and GREY too); the other part's number plain, dimmed */
        const int32_t ty = (y0 + y1) / 2;
        const uint16_t kc = hot ? T_PTHEME(k) : ux.bw ? T_DIM : ux_mix(T_BG, T_PTHEME(k), 40);
        if (hot) {
            cv_rect(1, y0 + 1, 3, y1 - y0 - 2, T_PFILL(k));
            cv_rrect(UT_A_K1_DOT_X - 4, ty - 5, 14, 11, 2, kc, T_BG);
        }
        cv_text_on(UT_A_K1_DOT_X, UT_TY(S, ty + AF_S_CAP_H / 2), &AF_S, k ? "2" : "1", hot ? T_INK : kc, hot ? kc : T_BG);
    }
    if (hs > 30)                                     /* (the old non-grid box) */
        amp = hs * 3 / 5;
    step = (y1 - y0 - 2 * amp - 3) / ((int32_t)WL_HN - 1);
    step = step < 2 ? 2 : step;                      /* (a ridge every 2 rows at least: closer, they fill the box solid) */
    nr = (y1 - y0 - 2 * amp - 3) / step + 1;         /* the ridges that fit */
    nr = nr < 1 ? 1 : nr;
    nf = nr > 1 ? nr : 2;                            /* (the fade runs over all that fit, so a ridge keeps its colour while the history fills) */
    nd = (int32_t)s->hn;                             /* ridges to draw: none yet is a flat line */
    nr = nr > nd ? nd : nr;
    nr = nr < 1 ? 1 : nr;
    for (i = nr - 1; i >= 0; i--) {
        int32_t yb = (y0 + y1 - 1) / 2 + ((nr - 1) - 2 * i) * step / 2, py = 0;
        uint16_t col = mv_col(k, pct, i * 90 / nf + (hot ? 0 : 45));
        for (p = 0; p < WL_NP; p++) {
            int32_t x = WL_X0 + 3 * p, y = yb + (s->hn ? s->dy[i][p] * amp / hs : 0);
            y = clamp(y, y0, y1 - 1);
            if (y + 1 < y1)
                cv_rect(x, y + 1, 3, y1 - y - 1, T_BG);
            if (p)
                wl_line(x - 3, py, x, y, col, y0, y1 - 1);
            py = y;
        }
    }
}
/* the page's clock: arms the gate and says whether this draw makes a new picture (never with ANIM OFF: the tap stays shut) */
static int wl_tick(void)
{
    uint32_t tk = mv_tick();
    int stepit;
    if (ui_prefs & PREF_ANIM_OFF)
        return 0;
    stepit = tk != wl_tk || !(wl[0].got && wl[1].got);
    if (stepit) {
        if ((int32_t)(wv_want - wv_blk) <= 0) {      /* the gate was shut (the page was left): the pictures start over, what the rings hold is not fresh */
            memset(wl, 0, sizeof wl);
            wl[0].base = wv_cnt[0];
            wl[1].base = wv_cnt[1];
        }
        wv_want = wv_blk + 256u;                     /* the one word the UI writes */
        wl_tk = tk;
    }
    return stepit;
}
/* the picture in rows y0 .. y1 - 1: always both parts, part 1 in the upper half and part 2 in the lower, whatever ALGORITHM is set to (it decides what the fader moves, not the picture) */
static void wl_card(int32_t y0, int32_t y1)
{
    const int32_t ym = (y0 + y1) / 2;
    if (wl_tick()) {
        wl_step(0, (ym - y0) / 2 - 1);
        wl_step(1, (y1 - ym) / 2 - 1);
    }
    wl_stack(0, y0, ym);
    wl_stack(1, ym, y1);
}
/* TEMPLATE 9: the picture in the big area (screen rows ay .. ay + ah - 1, drawn on the area's canvas by ui_graph.c draw_graph); HOLD SAVE = SAVE PATCH keeps its place on the last line */
static void wl_area(int32_t ay, int32_t ah)
{
    wl_card(ay, ay + ah - 20);
    cv_text_on(UT_GR_NOTE_X, ay + ah - 19, &AF_S, "HOLD SAVE = SAVE PATCH", T_DIM, T_BG);
}
