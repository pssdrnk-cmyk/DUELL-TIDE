/* SPDX-License-Identifier: GPL-3.0-only */
/* The MORPH page's picture: the part's real sound, from fx.c's scope tap (wv_m, 512 mono entries a part at 11.025 kHz (46 ms), each a companded 8-bit code: wvr_code), drawn as a stack of ridges
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
 *   5 ridge   56 points, each the mean of the linear samples across its share of the window, or, where that share holds a jump of more than half the ridge's range, the sample at its
 *             centre (a saw's drop and a square's edges stay vertical); no smoothing over neighbours; the middle of the line's range taken out
 *   6 gain    the peak of the window: a rise of more than 6 % at once, a fall of more than 12 % by 1/4 a step (a decaying note keeps a readable shape while it fades, a held note does not breathe), never under WL_FLOOR
 *             (-36 dB of the half scale: a quiet held note still shows its shape; quieter is smaller), a ridge under 2 px high is flat (a tail, not a line of dither)
 *   7 stack   every picture is a ridge (b6, owner 2026-11-09: "the initial version looked quite cool when we called it wave lab": WAVE LAB's WAVE STACK at this size): the last
 *             WL_HN, the newest in front (lowest), each older one about 2 rows higher, every one at the front's full height, coloured from the part's colour at the front toward the
 *             background at the back (mv_col's fade); a ribbon that scrolls back a step every picture. Each new picture is blended half and half into the front whenever the two
 *             are of a size (within 2 : 1, whatever the shape: the approved look's frame blend), so neighbouring lines stay alike and a busy sound flows instead of turning to noodles;
 *             a jump in size (an attack) is taken as it is. No line is laid flat or held still for its shape; an older line never crosses a newer one (wl_stack: it rides just above it).
 *             Silence fades the front's shape (wl_fade), the stack runs out, and every lane stays drawn, flat: at rest the page shows the same set of lines as when it plays (owner 2026-11-09)
 * Samples inside are the tap's steps (1/4,096 of the half scale) times 16 (the interpolation's fraction). The ridges are kept in 1/4 px from their middle, a byte a point (WL_AMAX: the half height of
 * a ridge is at most 31 px; the front's is about 20 px in a 68-row lane). b6 layout (owner 2026-11-09, "the bottom half of the screen is badly used"): the picture takes the page
 * from the knob-1 line down to the compact FILTER / RESONANCE / ECHO row (ui_draw.c morph_row; ROLL / AMOUNT until 1.0.0-b7) and the whole width, the part tags over the lanes' left edge; HOLD SAVE = SAVE PATCH is a hint in
 * that row while SAVE is held (ui_draw.c morph_row). */
#define WL_NP 56                                     /* points of a ridge (b6: 56, one every 3.9 px, so 13 ridges fit the RAM rule), spread over x WL_X0 .. WL_X1 (right of the part's tag column, x 0 .. 21: no line starts under or beside a tag) */
#define WL_X0 22
#define WL_X1 239                                     /* (b6: to the screen's last column, owner 2026-11-09) */
#define WL_HN 13                                     /* ridges kept: as many as fit 2 rows apart over the 68-row lanes (2 parts x 13 x 56 B; the build keeps 4 KB of RAM headroom) */
#define WL_AMAX 31                                   /* the largest half height of a ridge, px (31 x 4 quarters fit a byte) */
#define WL_STEP 2                                    /* the fewest rows between two ridges (WAVE LAB's) */
#define WL_R 400u                                    /* entries read (ring 512; 36 ms at the tap's 11.025 kHz) */
#define WL_MINW 80u
#define WL_MAXW 400u
#define WL_DEFW 200u
#define WL_FLOOR (64 * 16)                           /* the smallest peak the gain scales to: 64 of the tap's 4,096 steps (-36 dB of the half scale), times 16 */
#define WL_NC 24
/* b10 wave flow (owner 2026-11-12, of the rc6 alternatives: "I like [the settling one] but with the speed of the original ... you could add VERY subtle ripples";
 * then "a bit more calmer or slower, or we could link it to the bpm"): the stack moves back one ridge a 16th of the tempo (beat_samples: the BPM, or an external
 * clock's measured one; the same with the transport stopped), smoothly: each ridge drifts between its two places by the belt's phase, which runs at the tempo's
 * rate (an accumulator: a tempo change changes the speed, never jumps the place). The front takes every picture in place between the steps, so it follows the sound
 * as it dies; the last ridge fades out as it reaches the far end. An older ridge's shape settles flatter with its place (eased: the front whole, the back one at 30 %),
 * and the flat and settled lines carry very small ripples (WL_RIP_A, 150 px long, drifting a round a bar) that ride back with their ridge, the two parts out of phase,
 * so at rest the lines still flow back. WL_FLOW 0: the rc2 look (a ridge every picture, no settling, no ripples) */
#ifndef WL_FLOW
#define WL_FLOW 1
#endif
#define WL_RIP_A 12                                  /* the ripples' height, 1/16 px (0.75 px each way: the owner tried 2.25, "the initial was better") */
/* (b6: calm, 7032411's own drawing for sounds with no steady period, is gone: with every picture blended 1 : 1 into the front, a held pad chord measures the same without it,
 * neighbouring lines 1.66 px apart with it and 1.65 without; the blend does what it did) */
typedef struct {
    uint32_t base;                                   /* wv_cnt where the gate last opened */
    uint32_t gpk;                                    /* the slow gain's peak */
    int16_t first[16];                               /* the last picture's first 16 points, in the tap's steps (the match) */
    int8_t rd[WL_HN][WL_NP];                         /* the ridges, the newest first: 1/16 px from the ridge's middle (sub-pixel: the lines are drawn smooth) */
    uint16_t flat;                                   /* bit r: ridge r is flat (silence, a note's tail) */
    uint16_t w;                                      /* the window */
    uint16_t per;                                    /* its period (1/16 of an entry; 0: none steady), so that a held note keeps its window and its count of cycles */
    uint8_t fall;                                    /* the note's trigger takes falling crossings (held while its falling edges are the steeper; silence clears it) */
    uint8_t got, have, a, hn;                        /* a picture exists since the last reset; one with a trigger to match; the half height the ridges were made for; ridges kept */
} wl_t;
static wl_t wl[2] __attribute__((section(".pool")));   /* (b10 ui-c: the pool, not .bss; zeroed by wl_tick the first time) */
static uint8_t wl_zeroed;
static uint32_t wl_tk = 0xFFFFFFFFu;
#if WL_FLOW
static uint32_t wl_pn[2];                            /* each part's pushes so far: a ridge's ripple rides back with it */
static uint32_t wl_bpos, wl_brem, wl_bms, wl_bpn[2]; /* the belt's place (1/256 of a ridge) and the remainder of its last step, fm1_ms when it was last moved, the belt step at each part's last push */
static void wl_belt(void)                            /* once a picture: the belt on by the time gone, at a 16th of the tempo (4 FS / beat_samples 16ths a second) */
{
    const uint32_t dt = fm1_ms - wl_bms, d = dt < 250u ? dt : 250u, bs = beat_samples() ? beat_samples() : 1u;   /* (a long gap, the page away: no catching up) */
    const uint32_t num = d * (1024u * FS / 1000u) + wl_brem;   /* (256ths of a ridge x beat_samples: 32-bit, 250 ms x 45,158 at most) */
    wl_bms = fm1_ms;
    wl_bpos += num / bs;
    wl_brem = num % bs;
}
static const int8_t wl_sin[65] = {0, 6, 12, 19, 24, 30, 36, 41, 45, 49, 53, 56, 59, 61, 63, 64, 64, 64, 63, 61, 59, 56, 53, 49, 45, 41, 36, 30, 24, 19, 12, 6,
                                  0, -6, -12, -19, -24, -30, -36, -41, -45, -49, -53, -56, -59, -61, -63, -64, -64, -64, -63, -61, -59, -56, -53, -49, -45, -41, -36, -30, -24, -19, -12, -6, 0};
static uint32_t wl_ripc(uint32_t k, int32_t c)       /* the ripples' phase at column c, 1/16384 of a round: 150 px long, drifting a round a bar (16 ridges), the parts half a round apart */
{
    return (uint32_t)c * 16384u / 150u + wl_bpos * 4u + k * 8192u;
}
static int32_t wl_rip(uint32_t ph)                   /* the ripple at phase ph, 1/16 px */
{
    const uint32_t i = (ph >> 8) & 63u, f = ph & 255u;
    return (wl_sin[i] * (int32_t)(256u - f) + wl_sin[i + 1u] * (int32_t)f) * WL_RIP_A / (64 * 256);
}
#endif
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
static int wl_pic(const wl_t *s, int32_t n, int32_t a, int8_t *nd, int16_t *nf, uint32_t *nw, uint32_t *ng, uint32_t *nfall)
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
        sg = (s->fall ? dn > up : dn > 2 * up) ? -1 : 1;   /* (held for the note: a saw whose drop is about twice its rise, the shape moving under a ROLL, would flip
                                                                       * between the two every few pictures and each flip starts a ridge a third of a period off) */
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
    *nfall = sg < 0;
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
/* a picture becomes the newest ridge (nd 0: a flat one), the others move back. A picture of about the size of the newest ridge (within 2 : 1, whatever its shape) is blended half and half
 * with it, so that a busy sound's ridges follow on from each other (one shape flowing back, not scattered lines); a jump in size (an attack, a much softer note) is taken as it is */
static void wl_push(wl_t *s, int8_t *nd)
{
    uint32_t p;
    if (nd && s->hn && !(s->flat & 1u)) {
        int32_t pn = 0, po = 0;
        for (p = 0; p < WL_NP; p++) {
            const int32_t a = nd[p] < 0 ? -nd[p] : nd[p], b = wl_rq(s, 0, p) < 0 ? -wl_rq(s, 0, p) : wl_rq(s, 0, p);
            pn = a > pn ? a : pn;
            po = b > po ? b : po;
        }
        if (pn <= 2 * po && po <= 2 * pn)
            for (p = 0; p < WL_NP; p++) {
                const int32_t q = nd[p] + wl_rq(s, 0, p);
                nd[p] = (int8_t)((q + (q > 0)) / 2);
            }
    }
#if WL_FLOW
    if (s->hn && wl_bpn[s - wl] == wl_bpos >> 8) {    /* the belt has not moved a ridge since this part's last push: the front takes the picture in place */
        for (p = 0; p < WL_NP; p++)
            s->rd[0][p] = nd ? nd[p] : 0;
        s->flat = (uint16_t)((s->flat & ~1u) | (nd ? 0u : 1u));
        s->got = 1;
        return;
    }
    wl_bpn[s - wl] = wl_bpos >> 8;
#endif
    for (p = WL_HN - 1u; p > 0; p--)                 /* (row by row from the oldest: the firmware has no memmove) */
        memcpy(s->rd[p], s->rd[p - 1u], sizeof s->rd[0]);
    for (p = 0; p < WL_NP; p++)
        s->rd[0][p] = nd ? nd[p] : 0;
    s->flat = (uint16_t)((s->flat << 1 | (nd ? 0u : 1u)) & ((1u << WL_HN) - 1u));
    s->hn = s->hn < WL_HN ? s->hn + 1u : WL_HN;
    s->got = 1;
#if WL_FLOW
    wl_pn[s - wl]++;
#endif
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
#if WL_FLOW
        const int moved = !s->hn || wl_bpn[s - wl] != wl_bpos >> 8;   /* (the belt moves the stack back with this picture, or the front takes it in place) */
#else
        const int moved = 1;
#endif
        s->flat |= 1u;                               /* (no blend with it: the fade is the shape itself) */
        wl_push(s, any ? nd : 0);
        s->flat = moved ? (uint16_t)((was << 1 | (any ? 0u : 1u)) & ((1u << WL_HN) - 1u)) : (uint16_t)((was & ~1u) | (any ? 0u : 1u));
    }
}
/* a picture that could not be read (too little since a hole in the tap, or torn twice): the front again (flat if it is), so the stack still moves back a step while the part
 * sounds (b6, owner 2026-11-09: "check why the current display doesn't move when piece is playing") */
static void wl_again(wl_t *s)
{
    int8_t nd[WL_NP];
    if (!s->hn)
        return;
    memcpy(nd, s->rd[0], sizeof nd);
    wl_push(s, (s->flat & 1u) ? 0 : nd);
}
/* one part's picture step. a: the ridges' half height in px */
static void wl_step(uint32_t k, int32_t a)
{
    wl_t *s = &wl[k];
    uint32_t c0, c1, vs, avail, tries, nw = s->w, ng = s->gpk, nfall = s->fall;
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
        s->fall = 0;
    }
    if (wv_sil[k]) {                                 /* the part is silent: the front fades, the stack runs out */
        wl_fade(s);
        s->have = 0;
        s->fall = 0;                                 /* (a new note finds its trigger's direction afresh) */
        s->gpk -= s->gpk >> 3;
        return;
    }
    for (tries = 0; tries < 2u; tries++) {           /* read in place, and look at the counter again */
        c0 = wv_cnt[k];
        vs = (int32_t)(s->base - wv_gap[k]) > 0 ? s->base : wv_gap[k];
        avail = (int32_t)(c0 - vs) > 0 ? c0 - vs : 0u;
        n = (int32_t)(avail < WL_R ? avail : WL_R);
        if (n < 100) {                               /* not enough since the gate opened or the hole: the last picture again */
            wl_again(s);
            return;
        }
        wl_k = k;
        wl_b = c0 - (uint32_t)n;
        nw = s->w;
        ng = s->gpk;
        r = wl_pic(s, n, a, nd, nf, &nw, &ng, &nfall);
#ifdef WL_TEST_AFTER_COPY
        WL_TEST_AFTER_COPY(k);                       /* (host test: the writer moves on while the reader reads) */
#endif
        __asm__ volatile("" ::: "memory");
        c1 = wv_cnt[k];
        if (c1 - c0 + 8u <= WVR_N - (uint32_t)n)     /* (and the block the writer may be in the middle of: 8 entries) */
            break;
        if (tries) {                                 /* torn twice: the last picture again */
            wl_again(s);
            return;
        }
    }
    s->w = (uint16_t)nw;
    if (wl_per)                                      /* (a picture with no steady period keeps the held one) */
        s->per = (uint16_t)wl_per;
    s->gpk = ng;
    s->fall = (uint8_t)nfall;
    if (!r) {                                        /* nothing there (a note's tail): the front fades, the stack runs out as the sound does */
        wl_fade(s);
        s->have = 0;
        return;
    }
    memcpy(s->first, nf, sizeof nf);
    s->have = 1;
    wl_push(s, nd);                                  /* (every picture, a steady period or not: the stack never stands still while the part sounds) */
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
    /* (b6, owner: "numbers are also thrash quality, they get out of the background color and not centered": the digit centred by its own ink in a pill of the wider digit's ink and 1 px each
     * side (the same for both parts), 1 row above and below the capitals (13 rows), the pill 2 px clear of the hot bar (x 1 .. 3); the knockout the pill and 1 px round it; a dimmed tag on the same grid) */
    const uint16_t kc = ui_part_col(k, hot);
    const char *d = k ? "2" : "1";
    const aag_t *g = &AF_S.g[glyph(&AF_S, (uint8_t)d[0])], *g1 = &AF_S.g[glyph(&AF_S, '1')], *g2 = &AF_S.g[glyph(&AF_S, '2')];
    const int32_t wm = (int32_t)(glyph_bw(g1) > glyph_bw(g2) ? glyph_bw(g1) : glyph_bw(g2));   /* (both pills as wide: the wider digit's ink; owner: a "1" in a pill of its own 4 px
                                                                                             * of ink read as an outlined box with a purple digit, its stem splitting the fill) */
    const int32_t px0 = 6, pw = wm + 2, ph = AF_S_CAP_H + 2;   /* (1 px round the ink: AUTO TIDE's two rows are 14 apart, a taller pill lost its foot to the next tag's knockout) */
    cv_rect(px0 - 1, y - ph / 2 - 1, pw + 2, ph + 2, T_BG);
    if (hot)
        cv_rrect(px0, y - ph / 2, pw, ph, 2, kc, T_BG);
    cv_text_on(px0 + HALF_UP(pw - (int32_t)glyph_bw(g)) - glyph_bx(g), UT_TY(S, y + AF_S_CAP_H / 2), &AF_S, d, hot ? T_INK : kc, hot ? kc : T_BG);
}
/* a box h rows high: the ridges' half height (*a), the rows from the front's middle to the oldest one's (*st: the stack spans the lane, the front's swing room at the foot, the oldest
 * ridge's, as tall, at the top) and how many fit WL_STEP apart (the return) */
static int32_t wl_fit(int32_t h, int32_t *a, int32_t *st)
{
    const int32_t aa = clamp(h * 3 / 10, 2, WL_AMAX), room = h - 3 - 2 * aa;   /* (the front fills most of the lane: 20 px of a 68-row half) */
    int32_t n = clamp(room / WL_STEP + 1, 1, WL_HN);
    *a = aa;
    *st = room;
    return n;
}
/* one column x of an anti-aliased stroke: the line passes from ya to yb (1/16 px) across the column, w16 thick (1/16 px); every column gets its ink, so the stroke is one connected line
 * at any slope (a steep part fills the rows between), the edges shaded by coverage. A pixel is blended toward the stroke's colour c by pct * its coverage (1/16 .. 16/16) %: from tab
 * (tab[n]: the canvas background bg blended by n/16, made once a stroke) where it is still the background, else mixed (ux_mix: the same colour either way, three divisions less) */
/* t[n], n 1 .. 16: colour c at str % covering n/16 of a pixel on the background, for the canvas (byte-swapped); one mix and shifts (b6 wavepace: 13 ridges' tables a lane) */
static uint16_t wl_bwn(uint16_t c) { return ux.bw ? ux_gray((uint32_t)c >> 11) : c; }   /* (MONO: a blend snapped to exact gray: the channels rounded apart drift
                                                                                        * off neutral where the flow's settling, ripples and fading line blend over each other) */
static void wl_tab(uint16_t *t, uint16_t c, int32_t str)
{
    const uint32_t f = ux_mix(T_BG, c, str), b = T_BG;
    const int32_t r0 = (int32_t)(b >> 11), g0 = (int32_t)(b >> 5 & 63u), b0 = (int32_t)(b & 31u);
    const int32_t dr = (int32_t)(f >> 11) - r0, dg = (int32_t)(f >> 5 & 63u) - g0, db = (int32_t)(f & 31u) - b0;
    int32_t n;
    for (n = 1; n <= 16; n++)
        t[n] = ux.mono ? swap16(ux_mix(T_BG, c, str * n / 16)) : swap16(wl_bwn((uint16_t)((r0 + ((dr * n + 8) >> 4)) << 11 | (g0 + ((dg * n + 8) >> 4)) << 5 | (b0 + ((db * n + 8) >> 4)))));
}
static void wl_col(int32_t x, int32_t ya, int32_t yb, int32_t w16, uint16_t c, int32_t pct, int32_t ymin, int32_t ymax, const uint16_t *tab, uint16_t bg)
{
    const int32_t top = (ya < yb ? ya : yb) - w16 / 2, bot = (ya < yb ? yb : ya) + w16 / 2;   /* the span the stroke covers, 1/16 px */
    int32_t row;
    if ((uint32_t)x >= cv_w)
        return;
    for (row = top >> 4; row <= bot >> 4; row++) {
        const int32_t r0 = row * 16 > top ? row * 16 : top, r1 = row * 16 + 16 < bot ? row * 16 + 16 : bot, y = row + CV_Y, m = pct * (r1 - r0) / 16;
        uint16_t *q;
        if (row < ymin || row > ymax || r1 <= r0 || (uint32_t)y >= cv_h || m <= 0)
            continue;
        q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
        *q = *q == bg ? tab[r1 - r0] : swap16(wl_bwn(ux_mix(swap16(*q), c, m > 100 ? 100 : m)));
    }
}
/* ridge r's height at x16 (1/16 px) of its span from xl16, mul the points a 1/16 px (Q16): the two nearest points, linear; 1/16 px from its middle */
static int32_t wl_atx(const wl_t *s, uint32_t r, int32_t x16, int32_t xl16, int32_t mul)
{
    const int32_t u = (x16 - xl16) * mul >> 8, i = u >> 8, f = u & 255;
    const int32_t a = wl_rq(s, r, (uint32_t)i), b = i + 1 < (int32_t)WL_NP ? wl_rq(s, r, (uint32_t)(i + 1)) : a;
    return (a * (256 - f) + b * f) >> 6;
}
/* part k's picture in rows y0 .. y1 - 1 (WAVE LAB's WAVE STACK): the ridges, the oldest first and the newest (the front, at the foot) last, each higher than the one after it and as
 * tall as the front; every ridge is ONE stroke across its width, nothing cut (owner 2026-11-09: "the lines are falling apart"). Colour: the part's, along its A..B gradient at the fader
 * (mv_col), toward the background from the front to the back (WAVE LAB's fade); the front an anti-aliased stroke about 2 px thick, the others anti-aliased 1-px strokes (b6: plain
 * 1-px lines for the cost, "the new UI is definitely laggy"; then sub-pixel again, cheaply, owner: "somehow the smoothness is off": a plain line moves in 1-px jumps). The part the controls act on (ui_part_hot) in full colour with a bar at the half's edge, the other dimmed. A tail fades its shape
 * (wl_fade); a ridge that has faded flat and a lane not filled yet are drawn as straight lines in their place with the same colours, so every lane is always there.
 * Depth (b6, owner 2026-11-09: "something is missing"): the stack recedes like a landscape. Each older ridge is narrower, centred (the oldest 4/5 of the front's width), and the gaps
 * between ridges close toward the back (gap i from the front: 2 (N - 1) - i, the back one about half the front one); each ridge hides what lies behind it (WAVE LAB's): a column's
 * ridges are drawn from the front back and an older one only above the top of the newer ones there, so nothing behind a ridge shows under it. At rest (nothing yet, or silence) the
 * stack is the playing one with every line flat, the same middles and widths (b6, owner: "there is a fuckup with the default screen": the rest screen had its own even spread, the
 * front apart). The gaps never widen further back and are at least WL_STEP (owner: "a bigger gap after the 3rd line": each line's own rounding to whole rows made 3 2 3 2 ..);
 * in 1/16 px, so 13 lines in the 25 rows of spread a 68-row lane leaves are about 2 1/16 px apart each.
 * No older ridge is laid flat for being unlike the front or crossing it; a shaped ridge is never drawn under a newer one: where it would cross, the part behind is not drawn (b10, hidden lines: it used to ride on the newer one,
 * and a pile of 1-px lines fused into solid bars on steep slopes); elsewhere its shape is its own. A flat one is drawn flat.
 * Column by column: every ridge's height there in one pass from the front back (each one's bound is the newer ones'), and drawn in that order under the horizon they leave */
static void wl_stack(uint32_t k, int32_t y0, int32_t y1)
{
    const wl_t *s = &wl[k];
    const int hot = ui_part_hot(k);
    const int32_t pct = (int32_t)ux.fader[k & 1u] * 100 / 127, str = hot ? 100 : 55;   /* (the fader as the theme took it: in steps, on a picture's frame (ui_draw.c), so
                                                                                      * a turn of knob 1 redraws the waves with a picture: ui_graph.c graph_signature) */
    const int32_t fstr = ux.mono || ux.bw ? 100 : str;   /* I12: GREY / MONO: the other part's front line at full strength, so it does not read as switched off */
    const uint16_t bg = swap16(T_BG);
    const int32_t cx16 = (WL_X0 + WL_X1) * 8, hw16 = (WL_X1 - WL_X0) * 8;   /* (the front's middle and half width, 1/16 px) */
    int32_t a, st, nf = wl_fit(y1 - y0, &a, &st), nr = s->hn < (uint32_t)nf ? (int32_t)s->hn : nf, r, c, gs = 0, left, m0, sp, mid = (y1 - 2 - a) * 16 + 8;
    int32_t yb[WL_HN], xl[WL_HN], xr[WL_HN], mul[WL_HN], py[WL_HN];
    uint16_t col[WL_HN], ft[WL_HN][17];              /* (each ridge's colours by the share of a pixel its stroke covers, 0 .. 16 sixteenths: on the background) */
    uint32_t shp = 0, seen = 0;                      /* (bit r: ridge r drawn with its shape; ridge r has had a column) */
#if WL_FLOW
    int32_t se[WL_HN];                               /* (ridge r's shape, 1/256: flatter further back; the rest of it the ripple's share) */
    uint32_t rp[WL_HN];                              /* (ridge r's ripple phase: it rides back with the ridge) */
#endif
    if (y1 - y0 < 8)
        return;
    if (hot)                                         /* the selected part (both with BOTH): a bar at the half's edge (the tag goes over the lines, last) */
        cv_rect(1, y0 + 1, 3, y1 - y0 - 2, T_PFILL(k));
    if (nr < 1 || s->a != (uint8_t)a || (s->flat & ((1u << nr) - 1u)) == (1u << nr) - 1u)
        nr = 0;                                      /* nothing yet, made for another box, or silence: every lane at rest, flat */
    for (r = 0; r + 1 < nf; r++)
        gs += 2 * (nf - 1) - r;                      /* (the gaps' weights: 2 (N - 1) - i, closing toward the back) */
    m0 = st >= WL_STEP * (nf - 1) ? WL_STEP * 16 : 0;   /* (every gap at least WL_STEP rows, so two 1-px lines never touch; the rest by the weights; 1/16 px) */
    sp = st * 16 - m0 * (nf - 1);
    left = sp;
    for (r = 0; r + 1 < nf; r++)
        left -= sp * (2 * (nf - 1) - r) / gs;        /* (the sixteenths the gaps leave over: one more each to the front ones) */
    for (r = 0; r < nf; r++) {
        const int32_t w = nf > 1 ? hw16 * (5 * (nf - 1) - r) / (5 * (nf - 1)) : hw16;   /* (the half width: the front's, to 4/5 at the back) */
        yb[r] = mid;                                 /* (ridge r's middle, 1/16 px: the front's swing room at the foot, the oldest's at the top; the gaps never wider further back. b6
                                                      * wavepace, owner: "somehow the smoothness is off": in 1/16 px again, as the lines are drawn sub-pixel) */
        if (r + 1 < nf)
            mid -= m0 + sp * (2 * (nf - 1) - r) / gs + (r < left);
        xl[r] = cx16 - w;
        xr[r] = cx16 + w;
        mul[r] = (int32_t)(((WL_NP - 1u) << 16) / (uint32_t)(2 * w > 0 ? 2 * w : 1));
        shp |= (uint32_t)(r < nr && !((s->flat >> r) & 1u)) << r;
        col[r] = mv_col(k, pct, r * 90 / nf);        /* (WAVE LAB's fade toward the back) */
        wl_tab(ft[r], col[r], r ? str : fstr);
#if WL_FLOW
        rp[r] = (wl_pn[k] - (uint32_t)r) * 256u;      /* (1/64 of a round from one ridge to the next) */
#endif
    }
#if WL_FLOW
    {   /* the belt: ridge r between places r - 1 and r by its phase (ridge 1 peels off the front), the last one fading out as it reaches the far end; the shape settles
         * by the place */
        const int32_t ph = (int32_t)(wl_bpos & 255u);
        for (r = nf - 1; r >= 0; r--) {
            const int32_t pz = r ? (r - 1) * 256 + ph : 0, x = nf > 1 ? pz / (nf - 1) : 0;   /* (its place, 1/256 of a ridge; 0 .. 256: the front .. the back) */
            se[r] = 256 - 179 * x * x / 65536;
            if (r) {
                const int32_t w0 = nf > 1 ? hw16 * (5 * (nf - 1) - (r - 1)) / (5 * (nf - 1)) : hw16, w1 = nf > 1 ? hw16 * (5 * (nf - 1) - r) / (5 * (nf - 1)) : hw16;
                const int32_t w = w0 + (((w1 - w0) * ph) >> 8);
                yb[r] = yb[r - 1] + (((yb[r] - yb[r - 1]) * ph) >> 8);
                xl[r] = cx16 - w;
                xr[r] = cx16 + w;
                mul[r] = (int32_t)(((WL_NP - 1u) << 16) / (uint32_t)(2 * w > 0 ? 2 * w : 1));
                col[r] = mv_col(k, pct, pz * 90 / (nf * 256));
                wl_tab(ft[r], col[r], r == nf - 1 ? str * (256 - ph) / 256 : str);
            }
        }
    }
#endif
    {
#if WL_FLOW
#define WL_RP(r, cph) wl_rip((cph) + rp[r])
#else
#define WL_RP(r, cph) 0
#endif
        int32_t y0f = shp & 1u ? yb[0] + wl_atx(s, 0u, WL_X0 * 16, xl[0], mul[0]) : yb[0] + WL_RP(0, wl_ripc(k, WL_X0));   /* (the front's height in this column; the next one's is found in the loop) */
        for (c = WL_X0; c <= WL_X1; c++) {
#if WL_FLOW
            const uint32_t cph = wl_ripc(k, c), nph = cph + 16384u / 150u;
#endif
            const int32_t x16 = c * 16, yn = (shp & 1u) && c < WL_X1 ? yb[0] + wl_atx(s, 0u, x16 + 16, xl[0], mul[0]) : !(shp & 1u) && c < WL_X1 ? yb[0] + WL_RP(0, nph) : y0f;
            int32_t y[WL_HN], m, hz = y1 - 1;   /* (m: the ridges drawn in this column, the front and the older ones as wide as it reaches: each is narrower than the one before;
                                                      * hz: the horizon, the top row drawn in this column so far: an older ridge shows only above it) */
            y[0] = y0f;
            for (m = 1; m < nf && x16 >= xl[m] && x16 <= xr[m]; m++) {   /* (the heights, the front back: each shaped one at most at the bound the newer shaped ones leave) */
                if (!((shp >> m) & 1u)) {
                    y[m] = yb[m] + WL_RP(m, cph);
                    continue;
                }
#if WL_FLOW
                y[m] = yb[m] + (wl_atx(s, (uint32_t)m, x16, xl[m], mul[m]) * se[m] + WL_RP(m, cph) * (256 - se[m])) / 256;
#else
                y[m] = yb[m] + wl_atx(s, (uint32_t)m, x16, xl[m], mul[m]);
#endif
            }
            for (r = 0; r < m; r++) {
                const int32_t ya = (seen >> r) & 1u ? py[r] : y[r];
#ifdef WL_TEST_Y
                WL_TEST_Y(k, r, c, y[r]);            /* (host probe only: the height as drawn) */
#endif
                py[r] = y[r];
                seen |= 1u << r;
                if (!r) {                            /* (the front: from halfway to the last column's height to halfway to the next one's, anti-aliased) */
                    const int32_t a0 = (ya + y[0]) / 2, a1 = (y[0] + yn) / 2;
                    wl_col(c, a0, a1, 28, col[0], fstr, y0 + 1, y1 - 1, ft[0], bg);
                    hz = ((a0 < a1 ? a0 : a1) - 14) >> 4;
                } else {                             /* (an older ridge: a 1-px stroke from its height in the last column, anti-aliased as the front (b6 wavepace: the plain whole-row line
                                                      * moved in 1-px jumps), the rows above the horizon only) */
                    const int32_t t16 = (ya < y[r] ? ya : y[r]) - 8, b16 = (ya < y[r] ? y[r] : ya) + 8;   /* (the stroke's top and bottom, 1/16 px) */
                    int32_t lo = t16 >> 4, hi = (b16 - 1) >> 4;
                    hi = hi < hz ? hi : hz - 1;      /* (above the horizon only: those pixels are still the background, so a coverage's colour is written as it is) */
                    lo = lo > y0 + 1 ? lo : y0 + 1;
                    lo = lo > -CV_Y ? lo : -CV_Y;  /* (and inside the canvas) */
                    hi = hi < (int32_t)cv_h - 1 - CV_Y ? hi : (int32_t)cv_h - 1 - CV_Y;
                    if (lo <= hi && (uint32_t)c < cv_w) {
                        uint16_t *q = &cv_px[(uint32_t)(lo + CV_Y) * cv_w + (uint32_t)c];
                        int32_t n2;
                        for (n2 = lo; n2 <= hi; n2++, q += cv_w)
                            *q = ft[r][(b16 < n2 * 16 + 16 ? b16 : n2 * 16 + 16) - (t16 > n2 * 16 ? t16 : n2 * 16)];
                        hz = lo;
                    }
                }
            }
            y0f = yn;
        }
    }
    ui_part_tag(k, (y0 + y1) / 2, hot);              /* (over the lines' left edge) */
}
/* the page's clock: arms the gate and says whether this draw makes a new picture (never with ANIM OFF: the tap stays shut) */
static int wl_tick(void)
{
    uint32_t tk = mv_tick();
    int stepit;
    if (!wl_zeroed) {
        memset(wl, 0, sizeof wl);
        wl_zeroed = 1;
    }
    if (ui_prefs & PREF_ANIM_OFF)
        return 0;
    stepit = tk != wl_tk || !(wl[0].got && wl[1].got);
    if (stepit) {
#if WL_FLOW
        wl_belt();
#endif
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
#if WL_ROWS_DIFF   /* (OFF by default: build.py DUELL_WAVE_ROWS=1; not proven on the unit, +164 B of flash) */
/* Only the rows that changed go to the panel (ROADMAP Later 21: the SPI link costs ~44 ms for a whole picture, 32,640 px): a 32-bit hash of every screen row of what the panel
 * holds, kept in the canvas's unused tail (a half is 68 rows, the canvas holds 124: from row 72 on, a word a screen row; anything else drawn there only costs a row sent, never a
 * wrong picture), the changed rows blitted in runs. Every row after a page change or any redraw of the screen (ui.force: the area may hold anything).
 * WL_TEST_HALF: host check (before / after a half) that the panel then equals the canvas, and the pixels sent */
#ifndef WL_TEST_HALF
#define WL_TEST_HALF(y, n, end) ((void)0)
#endif
typedef uint32_t wl_u32;
static wl_u32 wl_rh[240] __attribute__((section(".pool")));   /* (the hash of every screen row of what the panel holds) */
static void wl_rows(uint32_t y)
{
    wl_u32 *const rh = wl_rh + y;
    const uint32_t n = cv_h;
    uint32_t r, s = 0, i, h, ch;
    GFX_HOOK_BLIT(0, y, 0u);
    WL_TEST_HALF(y, n, 0);
    for (r = 0; r <= n; r++) {
        ch = 0;
        if (r < n) {
            const uint16_t *q = cv_px + r * 240u;
            for (h = i = 0; i < 240u; i++)
                h = (h ^ q[i]) * 16777619u;
            ch = (rh[r] ^ h) | ui.force;
            rh[r] = h;
        }
        if (!ch) {                                   /* (an unchanged row, or the end: the run before it goes out) */
            if (s < r)
                lcd_blit(0, y + s, 240u, r - s, cv_px + s * 240u);
            s = r + 1u;
        }
    }
    WL_TEST_HALF(y, n, 1);
}
#endif
/* TEMPLATE 9: the picture in the big area, screen rows ay .. ay + ah - 1, all of it (b6: the hint went to the slim row under it). Taller than one canvas (gfx.c CV_MAX, 124 rows):
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
#if WL_ROWS_DIFF
    wl_rows((uint32_t)ay);
#else
    cv_blit(0, (uint32_t)ay);
#endif
    cv_begin(240, (uint32_t)(y1 - ym), T_BG);
    cv_bg = T_BG;
    cv_oy = -ym;
    wl_stack(1, ym, y1);
    cv_oy = 0;
#if WL_ROWS_DIFF
    wl_rows((uint32_t)ym);
#else
    cv_blit(0, (uint32_t)ym);
#endif
}
