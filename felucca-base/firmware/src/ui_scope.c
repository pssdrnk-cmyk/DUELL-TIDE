/* SPDX-License-Identifier: GPL-3.0-only */
/* The MORPH page's picture (DUELL_TIDE): the part's real sound, from fx.c's scope tap (wv_m, 512 mono entries a part at 11.025 kHz (46 ms), each a companded 8-bit code: wvr_code), drawn as a stack of ridges
 * (docs/design/WAVE_DISPLAY_PLAN.md 3.2, reduced to the RAM there is; the look is the owner's pick from the WAVE LAB preview on plan/wave-displays, WAVE STACK). Included by ui_graph.c before graph_morph.
 * Core 0, main loop, nothing in the audio path.
 * Per part and picture step (about 16 a second, the page's step: mv_tick):
 *   1 gate    wv_want = "wanted until block N", 256 blocks (190 ms) ahead; MENU > ANIM OFF shuts it: no tap, the last picture stays (a flat line before there was one)
 *   2 read    the newest 400 entries or fewer, only those written since the gate opened and since the part's last hole (wv_gap); read IN PLACE (wl_dec on each read), the counter looked at again
 *             afterwards: if the writer went on by more than the ring leaves (512 less 400, less a block) the picture is made again, once, else the last one stays
 *   3 trigger the ring smoothed (a 5-point kernel); crossings of its middle, rising (falling when the falling edges are clearly the steeper: a saw's drop), that come after a clear dip (the Pico scope's hysteresis), found to 1/16 of an entry; of those in the last 3.5
 *             periods the steepest (70 % of the steepest), and of those the one whose first 16 points match the last picture's best
 *   4 window  whole periods of the median crossing distance when it is steady: 2, or more for a high note (80 entries at least), fewer when they do not fit after a period of room
 *             for the crossing (1 at least); else the last one
 *   5 ridge   72 points, each the mean of the linear samples across its share of the window, or, where that share holds a jump of more than half the ridge's range, the sample at its
 *             centre (a saw's drop and a square's edges stay vertical); no smoothing over neighbours; the middle of the line's range taken out
 *   6 gain    the peak of the window: a rise of more than 6 % at once, a fall of more than 12 % by 1/4 a step (a decaying note keeps a readable shape while it fades, a held note does not breathe), never under WL_FLOOR
 *             (-36 dB of the half scale: a quiet held note still shows its shape; quieter is smaller), a ridge under 2 px high is flat (a tail, not a line of dither)
 *   7 stack   the last WL_HN ridges, the newest in front (lowest), each older one a few rows higher, a little lower and evenly fainter; every ridge is ONE unbroken line across the
 *             whole width (nothing is hidden or cut, owner 2026-11-09: "the lines are falling apart"). A new ridge is blended with the one before only when the shapes match closely
 *             (size within 2 : 1, mean difference at most 1/8 of the peak), else it is shown as it is; a new picture moves the stack up a step. Silence fades the front's shape (wl_fade), the stack runs out, and every lane stays drawn, flat: at rest the page shows the same set of lines as when it plays (owner 2026-11-09)
 * Samples inside are the tap's steps (1/4,096 of the half scale) times 16 (the interpolation's fraction). The ridges are kept in 1/4 px from their middle, a byte a point (WL_AMAX: the half height of
 * a ridge is at most 31 px; the front's is about 20 px in a 68-row lane). b6 layout (owner 2026-11-09, "the bottom half of the screen is badly used"): the picture takes the page
 * from the knob-1 line down to the compact ROLL / AMOUNT row (ui_draw.c morph_row) and the whole width, the part tags over the lanes' left edge; HOLD SAVE = SAVE PATCH is a hint in
 * that row while SAVE is held (ui_draw.c morph_row). */
#define WL_NP 72                                     /* points of a ridge, spread over x WL_X0 .. WL_X1 (right of the part's tag column, x 0 .. 21: no line starts under or beside a tag) */
#define WL_X0 22
#define WL_X1 236
#define WL_HN 6                                      /* ridges kept (6: each one a clean line apart at this size; 16 crowded into noodles, owner 2026-11-09) */
#define WL_AMAX 31                                   /* the largest half height of a ridge, px (31 x 4 quarters fit a byte) */
#define WL_STEP 3                                    /* the fewest rows between two ridges */
#define WL_R 400u                                    /* entries read (ring 512; 36 ms at the tap's 11.025 kHz) */
#define WL_MINW 80u
#define WL_MAXW 400u
#define WL_DEFW 200u
#define WL_FLOOR (64 * 16)                           /* the smallest peak the gain scales to: 64 of the tap's 4,096 steps (-36 dB of the half scale), times 16 */
#define WL_NC 24
typedef struct {
    uint32_t base;                                   /* wv_cnt where the gate last opened */
    uint32_t gpk;                                    /* the slow gain's peak */
    int16_t first[16];                               /* the last picture's first 16 points, in the tap's steps (the match) */
    int8_t rd[WL_HN][WL_NP];                         /* the ridges, the newest first: 1/16 px from the ridge's middle (sub-pixel: the lines are drawn smooth) */
    uint16_t flat;                                   /* bit r: ridge r is flat (silence, a note's tail) */
    uint16_t w;                                      /* the window */
    uint16_t per;                                    /* its period (1/16 of an entry; 0: none steady), so that a held note keeps its window and its count of cycles */
    uint8_t got, have, a, hn;                        /* a picture exists since the last reset; one with a trigger to match; the half height the ridges were made for; ridges kept */
} wl_t;
static wl_t wl[2];
static uint32_t wl_tk = 0xFFFFFFFFu;
_Static_assert(NLIVE == 2, "ui_scope.c keeps one reader state for each of the two parts");
_Static_assert(WL_HN <= 16, "wl_t.flat has a bit a ridge");
static uint32_t wl_k, wl_b, wl_per;                  /* (wl_per: the period the last wl_pic found, 1/16 entry, 0: none) */                          /* the ring being read, and the counter value of its entry 0 */
/* an entry back from wvr_code's sign and 7 bits: the middle of its step, in 1/4,096 of the half scale (0 stays 0) */
static int32_t wl_dec(int32_t c)
{
    uint32_t a = (uint32_t)(c < 0 ? -c : c), e = a >> 4, m = a & 15u;
    int32_t v = (int32_t)(e ? ((16u | m) << e) + (1u << (e - 1u)) : 2u * m);
    return c < 0 ? -v : v;
}
#define WLR(i) wl_dec(wv_m[wl_k][(wl_b + (uint32_t)(i)) & (WVR_N - 1u)])

static int32_t wl_abs(int32_t v) { return v < 0 ? -v : v; }
/* the ring smoothed for the trigger, entry i of n, times 32 (a 10-point kernel: 3 points, then 8: a ring on a resonant wave no longer moves the crossing) */
static int32_t wl_g(int32_t i, int32_t n)
{
    static const uint8_t K[5] = {1, 3, 4, 3, 1};
    int32_t j, s = 0;
    for (j = 0; j < 5; j++) {
        int32_t x = i + j - 2;
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
/* the same point keeping an edge: if its share of the window holds a jump of more than jump (times 16), the sample nearest the point's centre, not the mean (a saw's drop and a square's
 * edges stay near vertical instead of a ramp over two points) */
static int32_t wl_pte(uint32_t cq, uint32_t w, uint32_t p, int32_t jump)
{
    uint32_t span = w * 16u / WL_NP, k = 1u + span / 16u, m;
    int32_t sum = 0, lo = 0x7FFFFFFF, hi = -0x7FFFFFFF;
    for (m = 0; m < k; m++) {
        const int32_t v = wl_at(cq + (p * w * 16u) / WL_NP + (2u * m + 1u) * span / (2u * k));
        sum += v;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    if (hi - lo > jump)
        return wl_at(cq + (p * w * 16u) / WL_NP + span / 2u);
    return sum / (int32_t)k;
}
/* one picture of the part wl_k from the n newest entries: the ridge into nd (px, -a .. a), the next first[] into nf, the window into *nw, the gain into *ng. 1: made, 0: nothing there (flat) */
static int wl_pic(const wl_t *s, int32_t n, int32_t a, int8_t *nd, int16_t *nf, uint32_t *nw, uint32_t *ng)
{
    int32_t cand[WL_NC], slope[WL_NC], mx = -0x7FFFFFFF, mn = 0x7FFFFFFF, rmx = -32768, rmn = 32767, mid, thr, i, j, pk = 0, rm, last, maxs = 0, g;
    uint32_t nc = 0, armed = 0, per = 0, pq = 0, w, span, cq, np = 0, pool[WL_NC], best = 0, sel = 0, p;
    int32_t v[WL_NP], prev, cur, sg;
    for (i = 0; i < n; i++) {                        /* the range of the raw ring, and of the trigger's line without the ends of the copy (the smoothing there leans on one side) */
        int32_t r = WLR(i);
        rmx = r > rmx ? r : rmx;
        rmn = r < rmn ? r : rmn;
        if (i >= 4 && i + 4 < n) {
            int32_t q = wl_g(i, n);
            mx = q > mx ? q : mx;
            mn = q < mn ? q : mn;
        }
    }
    mid = (mx + mn) / 2;
    thr = (mx - mn) / 8;
    {   /* the trigger's direction: the rising crossings, unless the falling edges are clearly the steeper (a falling saw's drop: on its slow ramp the crossing would wander) */
        int32_t up = 0, dn = 0;
        for (i = 2; i + 2 < n; i++) {
            const int32_t d = wl_g(i + 2, n) - wl_g(i - 2, n);
            up = d > up ? d : up;
            dn = -d > dn ? -d : dn;
        }
        sg = dn > 2 * up ? -1 : 1;
    }
    mid *= sg;                                       /* (the line as sg * wl_g: the falling case runs as the rising one) */
    prev = sg * wl_g(0, n);
    for (i = 1; i < n; i++) {                        /* the crossings of the trigger's line in that direction, armed only after a clear dip (a full table drops its oldest) */
        cur = sg * wl_g(i, n);
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
            slope[nc] = sg * (wl_g(i + 2 < n ? i + 2 : n - 1, n) - wl_g(i >= 2 ? i - 2 : 0, n));
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
            pq = (uint32_t)m;                        /* (the period in 1/16 of an entry, as the crossings are found) */
    }
    if (pq && s->per && (pq > s->per ? pq - s->per : s->per - pq) * 6u <= s->per)
        pq = s->per;                                 /* (within 1/6 of the held period: the same note; the crossings' own jitter, up to 13 % on a driven saw's attack, does not move the window) */
    wl_per = pq;
    per = pq / 16u;
    if (per) {                                       /* whole cycles of the played pitch: 2, or as many as make WL_MINW for a high note, never more than fit with a period of room for the
                                                      * crossing before them (at least 1): a held note's window is a whole number of its periods (to 1/16 of an entry), the same every picture */
        uint32_t c = (uint32_t)n * 16u > pq ? ((uint32_t)n * 16u - pq) / pq : 0u, k = (WL_MINW * 16u + pq - 1u) / pq, kp = s->w && s->per == pq ? (s->w * 16u + pq / 2u) / pq : 0u;
        k = k < 2u ? 2u : k;
        if (kp >= 1u && kp * pq * 4u >= WL_MINW * 48u && kp >= k - 1u && kp <= k + 1u)   /* (the count the last picture had, while it still makes about the window: the period's own jitter */
            k = kp;                                                                         /* never flips a held note between 2 and 3 cycles) */
        while (k > 1u && (k > c || k * pq > WL_MAXW * 16u))
            k--;
        w = (k * pq + 8u) / 16u;
    } else {
        w = s->w ? s->w : WL_DEFW;
        w = w < WL_MINW ? WL_MINW : w > WL_MAXW ? WL_MAXW : w;
    }
    if (s->w && (w > s->w ? w - s->w : s->w - w) * 5u < s->w)   /* (within a fifth of the last: the same, so the picture does not breathe) */
        w = s->w;
    if (w + 20u > (uint32_t)n)
        w = (uint32_t)n - 20u;
    span = (per ? per * 7u / 2u : WL_DEFW) * 16u;
    last = (n - (int32_t)w - 3) * 16;                /* (Q4: the newest start that leaves the whole window and the interpolation's room) */
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
                    sad += wl_abs((wl_pt((uint32_t)cand[pool[j]], w, p) >> 4) - s->first[p]);
            } else {
                sad = 0x3FFFFFFF - slope[pool[j]];
            }
            if (sad < bs) {
                bs = sad;
                best = pool[j];
            }
        }
        sel = (uint32_t)cand[best] + 1u;
    }
    cq = sel ? sel - 1u : (uint32_t)(n - (int32_t)w - 3) * 16u;   /* no crossing at all (noise, a drone): the newest window, free */
    *nw = w;
#ifdef WL_TEST_PIC
    WL_TEST_PIC(cq, w, per);                         /* (host probe only: the window chosen) */
#endif
    for (p = 0; p < 16u; p++)
        nf[p] = (int16_t)(wl_pt(cq, w, p) >> 4);
    rm = (rmx + rmn) * 8;                            /* AC coupled: the middle of the line's range over everything read (times 16) */
    {   /* each point the mean of its share, except where the share holds a jump of more than half the ridge's range: there the sample at the point's centre (an edge stays sharp) */
        int32_t lo = 0x7FFFFFFF, hi = -0x7FFFFFFF;
        for (p = 0; p < WL_NP; p++) {
            v[p] = wl_pt(cq, w, p) - rm;
            lo = v[p] < lo ? v[p] : lo;
            hi = v[p] > hi ? v[p] : hi;
        }
        for (p = 0; p < WL_NP; p++)
            v[p] = wl_pte(cq, w, p, (hi - lo) / 2) - rm;
    }
    /* (b6, owner 2026-11-09 "do we over-smooth the curves": no smoothing over neighbours any more; each point is the mean of its share of the window (wl_pt) and the anti-aliased stroke
     * draws it smooth, so a saw keeps its edge and a square its corners) */
    {   /* the ridge centred on its own range (a low note shows less than a cycle: centred on the whole read it would sit off-centre and use half the height) */
        int32_t lo = v[0], hi = v[0], c;
        for (p = 1; p < WL_NP; p++) {
            lo = v[p] < lo ? v[p] : lo;
            hi = v[p] > hi ? v[p] : hi;
        }
        c = (lo + hi) / 2;
        for (p = 0; p < WL_NP; p++)
            v[p] -= c;
    }
    for (p = 0; p < WL_NP; p++)
        pk = wl_abs(v[p]) > pk ? wl_abs(v[p]) : pk;
    if (pk < 16)                                     /* under a step of the tap: nothing there */
        return 0;
    g = (int32_t)*ng;
    if ((uint32_t)pk > *ng + (*ng >> 4))             /* the gain: a rise of more than 6 % at once, */
        g = pk;
    else if ((uint32_t)pk + (*ng >> 3) < *ng)        /* a fall of more than 12 %: 1/4 a step (a decay shrinks, the shape stays readable), */
        g -= g >> 2;                                 /* otherwise it stays: a steady note does not breathe */
    *ng = (uint32_t)g;
    g = g > WL_FLOOR ? g : WL_FLOOR;
    if (pk * a < 2 * g)                              /* under 2 px high: a flat ridge (the stack shows sound, not the last pixel of a tail) */
        return 0;
    for (p = 0; p < WL_NP; p++) {
        int32_t q = -v[p] * a * 8;                   /* (1/4 px, rounded) */
        nd[p] = (int8_t)clamp((q + (q < 0 ? -g : g)) / (2 * g), -4 * a, 4 * a);
    }
    return 1;
}
/* point p of ridge r, 1/16 px from its middle; wl_rp: in whole px (rounded) */
static int32_t wl_rq(const wl_t *s, uint32_t r, uint32_t p) { return s->rd[r][p]; }
static int32_t wl_rp(const wl_t *s, uint32_t r, uint32_t p)
{
    const int32_t q = s->rd[r][p];
    return (q + (q < 0 ? -2 : 2)) / 4;
}
/* a picture becomes the newest ridge (nd 0: a flat one), the others move back. A picture of about the size and shape of the newest ridge (within 2 : 1, the mean difference at most 1/8 of the peak) is blended half and half with it,
 * so that a busy sound's ridges follow on from each other (one shape, not scattered lines); a jump in size (an attack, a much softer note) is taken as it is */
static void wl_push(wl_t *s, int8_t *nd)
{
    uint32_t p;
    if (nd && s->hn && !(s->flat & 1u)) {
        int32_t pn = 0, po = 0, sn = 0, sd = 0;
        for (p = 0; p < WL_NP; p++) {
            int32_t a = nd[p] < 0 ? -nd[p] : nd[p], b = wl_rq(s, 0, p) < 0 ? -wl_rq(s, 0, p) : wl_rq(s, 0, p), d = nd[p] - wl_rq(s, 0, p);
            pn = a > pn ? a : pn;
            po = b > po ? b : po;
            sn += a;
            sd += d < 0 ? -d : d;
        }
        if (pn <= 2 * po && po <= 2 * pn && 8 * sd <= (int32_t)WL_NP * pn)   /* (only a close shape: size within 2 : 1 and the mean difference at most 1/8 of the peak) */
            for (p = 0; p < WL_NP; p++) {
                int32_t q = nd[p] + wl_rq(s, 0, p);
                nd[p] = (int8_t)((q + (q > 0)) / 2);
            }
    }
    for (p = WL_HN - 1u; p > 0; p--)                 /* (row by row from the oldest: the firmware has no memmove) */
        memcpy(s->rd[p], s->rd[p - 1u], sizeof s->rd[0]);
    for (p = 0; p < WL_NP; p++)
        s->rd[0][p] = nd ? nd[p] : 0;
    s->flat = (uint16_t)((s->flat << 1 | (nd ? 0u : 1u)) & ((1u << WL_HN) - 1u));
    s->hn = s->hn < WL_HN ? s->hn + 1u : WL_HN;
    s->got = 1;
}
/* the sound has gone (silence, a note's tail): the newest ridge again at half its height (a flat one once nothing is left), so a shape fades out over a few pictures instead of
 * dropping to a ruled line */
static void wl_fade(wl_t *s)
{
    int8_t nd[WL_NP];
    uint32_t p, any = 0;
    for (p = 0; p < WL_NP; p++) {
        int32_t v = s->hn && !(s->flat & 1u) ? wl_rq(s, 0, p) : 0;
        nd[p] = (int8_t)(v / 2);
        nd[p] = nd[p] > -2 && nd[p] < 2 ? 0 : nd[p];   /* (under half a px: gone) */
        any |= nd[p] != 0;
    }
    {
        const uint16_t was = s->flat;
        s->flat |= 1u;                               /* (no blend with it: the fade is the shape itself) */
        wl_push(s, any ? nd : 0);
        s->flat = (uint16_t)((was << 1 | (any ? 0u : 1u)) & ((1u << WL_HN) - 1u));
    }
}
/* one part's picture step. a: the ridges' half height in px */
static void wl_step(uint32_t k, int32_t a)
{
    wl_t *s = &wl[k];
    uint32_t c0, c1, vs, avail, tries, nw = s->w, ng = s->gpk;
    int32_t n;
    int8_t nd[WL_NP];
    int16_t nf[16];
    int r = 0;
    a = clamp(a, 1, WL_AMAX);
    if (s->a != (uint8_t)a) {                        /* the box changed (the layout): start over */
        memset(s->rd, 0, sizeof s->rd);
        s->a = (uint8_t)a;
        s->have = s->hn = s->got = 0;
        s->flat = 0;
    }
    if (wv_sil[k]) {                                 /* the part is silent: the front fades, the stack runs out */
        wl_fade(s);
        s->have = 0;
        s->gpk -= s->gpk >> 3;
        return;
    }
    for (tries = 0; tries < 2u; tries++) {           /* read in place, and look at the counter again */
        c0 = wv_cnt[k];
        vs = (int32_t)(s->base - wv_gap[k]) > 0 ? s->base : wv_gap[k];
        avail = (int32_t)(c0 - vs) > 0 ? c0 - vs : 0u;
        n = (int32_t)(avail < WL_R ? avail : WL_R);
        if (n < 100)
            return;                                  /* not enough since the gate opened or the hole: the last picture stays */
        wl_k = k;
        wl_b = c0 - (uint32_t)n;
        nw = s->w;
        ng = s->gpk;
        r = wl_pic(s, n, a, nd, nf, &nw, &ng);
#ifdef WL_TEST_AFTER_COPY
        WL_TEST_AFTER_COPY(k);                       /* (host test: the writer moves on while the reader reads) */
#endif
        __asm__ volatile("" ::: "memory");
        c1 = wv_cnt[k];
        if (c1 - c0 + 8u <= WVR_N - (uint32_t)n)     /* (and the block the writer may be in the middle of: 8 entries) */
            break;
        if (tries)
            return;                                  /* torn twice: the last picture stays */
    }
    s->w = (uint16_t)nw;
    if (wl_per)                                      /* (a picture with no steady period keeps the held one) */
        s->per = (uint16_t)wl_per;
    s->gpk = ng;
    if (!r) {                                        /* nothing there (a note's tail): the front fades, the stack runs out as the sound does */
        wl_fade(s);
        s->have = 0;
        return;
    }
    memcpy(s->first, nf, sizeof nf);
    s->have = 1;
    wl_push(s, nd);
}
/* A part's tag, shared by the MORPH page and AUTO TIDE: its number at the knob-1 dot's x, centred on row y. The selected part (hot) in full colour, its number in a filled pill
 * (the cue that works on MONO and GREY too); the other plain and dimmed (its colour mixed 40 % into the background; MONO: its grey). ui_part_col is that colour, ui_part_hot says which:
 * the selected part, or both with the part selection on BOTH (ui_input.c part_select) */
static int ui_part_hot(uint32_t k) { return song.both || k == song.sel; }
static uint16_t ui_part_col(uint32_t k, int hot)
{
    return hot ? T_PTHEME(k) : ux.bw ? T_DIM : ux_mix(T_BG, T_PTHEME(k), 40);
}
static void ui_part_tag(uint32_t k, int32_t y, int hot)
{
    const uint16_t kc = ui_part_col(k, hot);
    cv_rect(UT_A_K1_DOT_X - 5, y - 7, 17, 15, T_BG);    /* (a knockout, the tag's box and a 1-px margin, square: no line pixel inside it; the hot bar x 1 .. 3 stays) */
    if (hot)
        cv_rrect(UT_A_K1_DOT_X - 4, y - 5, 14, 11, 2, kc, T_BG);
    cv_text_on(UT_A_K1_DOT_X, UT_TY(S, y + AF_S_CAP_H / 2), &AF_S, k ? "2" : "1", hot ? T_INK : kc, hot ? kc : T_BG);
}
/* a box h rows high: the ridges' half height (*a), the rows from the front's middle to the oldest one's (*st: the stack spans the lane, the front's swing room at the foot, the oldest
 * ridge's (a / N) at the top) and how many fit (the return) */
static int32_t wl_fit(int32_t h, int32_t *a, int32_t *st)
{
    const int32_t aa = clamp(h * 3 / 10, 2, WL_AMAX), room = h - 3 - aa - (aa + WL_HN - 1) / WL_HN;   /* (the front fills most of the lane: 20 px of a 68-row half) */
    int32_t n = clamp(room / WL_STEP + 1, 1, WL_HN);
    *a = aa;
    *st = room;
    return n;
}
/* pixel (x, y) blended toward c by pct % (the canvas is 16-bit, byte-swapped: gfx.c swap16) */
static void wl_blend(int32_t x, int32_t y, uint16_t c, int32_t pct)
{
    uint16_t *q;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h || pct <= 0)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    *q = swap16(ux_mix(swap16(*q), c, pct > 100 ? 100 : pct));
}
/* one column x of an anti-aliased stroke: the line passes from ya to yb (1/16 px) across the column, w16 thick (1/16 px), at strength pct %; every column gets its ink, so the stroke
 * is one connected line at any slope (a steep part fills the rows between), the edges shaded by coverage */
static void wl_col(int32_t x, int32_t ya, int32_t yb, int32_t w16, uint16_t c, int32_t pct, int32_t ymin, int32_t ymax)
{
    const int32_t top = (ya < yb ? ya : yb) - w16 / 2, bot = (ya < yb ? yb : ya) + w16 / 2;   /* the span the stroke covers, 1/16 px */
    int32_t row;
    for (row = top >> 4; row <= bot >> 4; row++) {
        const int32_t r0 = row * 16 > top ? row * 16 : top, r1 = row * 16 + 16 < bot ? row * 16 + 16 : bot;
        if (row < ymin || row > ymax || r1 <= r0)
            continue;
        wl_blend(x, row, c, pct * (r1 - r0) / 16);
    }
}
/* part k's picture in rows y0 .. y1 - 1: the ridges, the oldest first and the newest (the front, at the foot) last, each *st rows higher than the one after it and lower
 * ((N - r) / N of the front's height, N the ridges that fit: the back ones are low ripples that rarely cross the front ones, the stack recedes); every ridge is ONE smooth anti-aliased stroke across the full width, nothing hidden (owner
 * 2026-11-09: "the lines are falling apart"). Colour: the part's, along its A..B gradient at the fader (mv_col); the front in full and about 2 px thick, each older one 1 px and evenly
 * fainter. The part the controls act on (ui_part_hot) in full colour with a bar at the half's edge, the other dimmed. A tail fades its shape (wl_fade); a ridge that has faded flat, a lane not
 * filled yet and silence are drawn as straight lines in their place with the same fades, so every lane is always there (resting flat, or moving) */
static void wl_stack(uint32_t k, int32_t y0, int32_t y1)
{
    const wl_t *s = &wl[k];
    const int hot = ui_part_hot(k);
    const int32_t pct = (int32_t)morph_fader_of(k) * 100 / 127;
    const uint16_t col = mv_col(k, pct, 0);
    int32_t a, st, nf = wl_fit(y1 - y0, &a, &st), nr = s->hn < (uint32_t)nf ? (int32_t)s->hn : nf, yf = y1 - 2 - a, r, p;
    if (y1 - y0 < 8)
        return;
    if (hot)                                         /* the selected part (both with BOTH): a bar at the half's edge (the tag goes over the lines, last) */
        cv_rect(1, y0 + 1, 3, y1 - y0 - 2, T_PFILL(k));
    if (nr < 1 || s->a != (uint8_t)a || (s->flat & ((1u << nr) - 1u)) == (1u << nr) - 1u)
        nr = 0;                                      /* nothing yet, made for another box, or silence: every lane at rest, flat (the same lines as when it plays) */
    for (r = nf - 1; r >= 0; r--) {
        const int flat = r >= nr || ((s->flat >> r) & 1u);
        const int32_t yb16 = yf * 16 + 8 - (nf > 1 ? st * 16 * r / (nf - 1) : 0), num = nf - r;   /* (the ridge's middle, 1/16 px, spread over the lane; its height (N - r) / N) */
        const int32_t str = (r ? 15 + (nf > 2 ? 60 * (nf - 1 - r) / (nf - 2) : 60) : 100) * (hot ? 100 : 55) / 100;   /* (strength %: front 100, then 75, 60 .. 15 evenly; the same at rest) */
        const int32_t w16 = !r ? 28 : 16;
        int32_t py = yb16;
        int32_t px = WL_X0;
        for (p = 0; p < WL_NP; p++) {
            const int32_t x = WL_X0 + (int32_t)p * (WL_X1 - WL_X0) / (int32_t)(WL_NP - 1u), y = flat ? yb16 : yb16 + wl_rq(s, (uint32_t)r, (uint32_t)p) * 4 * num / nf;
            if (p && x > px) {                       /* (the columns of the segment from the last point, each from its left edge to its right) */
                const int32_t d2 = 2 * (x - px);
                int32_t c;
                for (c = px + 1; c <= x; c++) {
                    const int32_t e = 2 * (c - px) + 1;
                    wl_col(c, py + (y - py) * (2 * (c - px) - 1) / d2, py + (y - py) * (e < d2 ? e : d2) / d2, w16, col, str, y0 + 1, y1 - 1);
                }
            } else if (!p)
                wl_col(x, y, y, w16, col, str, y0 + 1, y1 - 1);
            px = x;
            py = y;
        }
    }
    ui_part_tag(k, (y0 + y1) / 2, hot);              /* (over the lines' left edge) */
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
/* the picture in rows y0 .. y1 - 1: always both parts, part 1 in the upper half and part 2 in the lower (the part selection decides which of them the controls act on, and the highlight shows it) */
static void wl_card(int32_t y0, int32_t y1)
{
    const int32_t ym = (y0 + y1) / 2;
    int32_t a0, a1, st;
    wl_fit(ym - y0, &a0, &st);
    wl_fit(y1 - ym, &a1, &st);
    if (wl_tick()) {
        wl_step(0, a0);
        wl_step(1, a1);
    }
    wl_stack(0, y0, ym);
    wl_stack(1, ym, y1);
}
/* TEMPLATE 9: the picture in the big area, screen rows ay .. ay + ah - 1, all of it (b6: the hint went to the ROLL / AMOUNT row). Taller than one canvas (gfx.c CV_MAX, 124 rows):
 * each part's half on a canvas of its own, blitted at its first row */
static void wl_area(int32_t ay, int32_t ah)
{
    const int32_t y1 = ay + ah, ym = (ay + y1) / 2;
    int32_t a0, a1, st;
    wl_fit(ym - ay, &a0, &st);
    wl_fit(y1 - ym, &a1, &st);
    if (wl_tick()) {
        wl_step(0, a0);
        wl_step(1, a1);
    }
    cv_begin(240, (uint32_t)(ym - ay), T_BG);
    cv_bg = T_BG;
    cv_oy = -ay;
    wl_stack(0, ay, ym);
    cv_oy = 0;
    cv_blit(0, (uint32_t)ay);
    cv_begin(240, (uint32_t)(y1 - ym), T_BG);
    cv_bg = T_BG;
    cv_oy = -ym;
    wl_stack(1, ym, y1);
    cv_oy = 0;
    cv_blit(0, (uint32_t)ym);
}
