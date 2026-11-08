/* SPDX-License-Identifier: GPL-3.0-only */
/* scope_test: the MORPH page's wave (fx.c's scope tap wv_tap, ui_scope.c's reader wl_step / wl_stack; docs/design/WAVE_DISPLAY_PLAN.md 3 and 4, reduced to 512 mono entries of 8 bits) on synthetic ring contents.
 * The ridge wl_step makes (wl[k].dy[0]: px from its middle, one per point) is the picture: its pixels are a function of it.
 *   trigger      the same signal at 10 starting offsets gives the same picture (within 1 px as drawn: the two sets of pixels, each to the nearest of the other; a saw, a resonant saw, a resonant saw at
 *                100.37 entries a cycle, a sine); the same signal as 10 pictures in a row: 0 changed points (all but the 100.37 one)
 *   beat         two sines 3 Hz apart: the height of the ridge swings at least 4 : 1 over 60 pictures
 *   soft         a sine 26 dB down is drawn small, not blown up to the full height
 *   silence      the part's silent flag, a ring of zeros, a wave under a pixel high: one flat ridge (the history restarts)
 *   stack        a held note builds up WL_HN ridges, no more
 *   wrap         the ring's free-running counter past 2^32 gives the picture it gives below it
 *   torn         the writer moves on past what the ring leaves between the read and the second look at the counter: once = one retry and a picture, twice = the old picture stays
 *   inside       every point of every picture inside the box (a full-scale square, noise, three box heights)
 *   tap          the entry the tap writes (the mean of four frames, half scale, 1/128 steps, clamped); a block the part did not tap (part 2 skipped, a shut gate, silence) moves the gap up, a late job
 *                (the block's own number, not wv_blk) does not; the reader draws only what lies after the gap
 *   gate         ANIM OFF leaves the gate shut and the picture as it was; otherwise the gate opens ahead; leaving the page starts the pictures over
 * Run: scope_test (exit 0 = all ok) */
#define UI_TEST_NO_MAIN 1
static void tear_hook(unsigned k);                       /* (ui_scope.c's WL_TEST_AFTER_COPY: the writer moves on after the reader's read) */
#define WL_TEST_AFTER_COPY(k) tear_hook(k)
#include "ui_test.c"
static int tear_left;                                    /* reads still to be torn */
static void ring(uint32_t k, uint32_t cnt);
static void tear_hook(unsigned k)
{
    if (tear_left > 0) {
        tear_left--;
        ring(k, wv_cnt[k] + 200u);                       /* the writer moves on by more than the 112 entries the ring leaves after the read (and writes the signal on) */
    }
}

static int bad, ncheck;
static void ok(const char *what, int c)
{
    ncheck++;
    printf("scope: %-100s %s\n", what, c ? "ok" : "FAIL");
    if (!c)
        bad++;
}

typedef int32_t (*wsig_t)(uint32_t t);                   /* the ring's entry (8-bit steps) at the counter's value t */
static wsig_t sig_m;
static const double PI2 = 6.283185307179586;
/* the ring as the writer leaves it when its counter is cnt: the last WVR_N entries of sig at the entries' own counter values */
static void ring(uint32_t k, uint32_t cnt)
{
    uint32_t i;
    for (i = 0; i < WVR_N; i++) {
        uint32_t t = cnt - WVR_N + i;
        int32_t v = sig_m ? sig_m(t) : 0;
        wv_m[k][t & (WVR_N - 1u)] = (int8_t)(v > 127 ? 127 : v < -128 ? -128 : v);
    }
    wv_cnt[k] = cnt;
}
static void fresh(uint32_t k)                            /* a part with no history: the gate opened long ago, no gap, no flag */
{
    memset(&wl[k], 0, sizeof wl[k]);
    wl[k].base = wv_cnt[k] - 100000u;
    wv_gap[k] = wv_cnt[k] - 100000u;
    wv_sil[k] = 0;
}
#define HS 21                                            /* the half height of the old card's box (45 rows) */
static int32_t saw100(uint32_t t) { return (int32_t)(t % 100u) * 120 / 100 - 60; }
static int32_t saw128(uint32_t t) { return (int32_t)(t % 128u) * 120 / 128 - 60; }
/* a resonant saw: the saw and a 7.3 x ring that decays in each cycle (several zero crossings a cycle) */
static int32_t ressaw(uint32_t t) { uint32_t p = t % 100u; return (int32_t)(p * 90 / 100) - 45 + (int32_t)(26.0 * sin(PI2 * 7.3 * p / 100.0) * exp(-(double)p / 30.0)); }
/* the same at a period that is no whole number of entries (100.37: 219.7 Hz) */
static int32_t ressaw_f(uint32_t t) { double p = fmod((double)t, 100.37); return (int32_t)(p * 0.9) - 45 + (int32_t)(26.0 * sin(PI2 * 7.3 * p / 100.37) * exp(-p / 30.0)); }
static int32_t sine128(uint32_t t) { return (int32_t)(60.0 * sin(PI2 * (t % 128u) / 128.0)); }
static int32_t sine_q(uint32_t t) { return sine128(t) / 20; }
static int32_t beat(uint32_t t) { return (int32_t)(30.0 * sin(PI2 * 200.0 * t / 22050.0) + 30.0 * sin(PI2 * 203.0 * t / 22050.0)); }
static int32_t square_fs(uint32_t t) { return (t / 50u) & 1u ? 127 : -128; }
static uint32_t rnd = 12345;
static int32_t noise(uint32_t t) { (void)t; rnd = rnd * 1664525u + 1013904223u; return (int32_t)(rnd >> 24) - 128; }
static int32_t dither(uint32_t t) { return (int32_t)(t & 1u); }
static int32_t zero(uint32_t t) { (void)t; return 0; }

static int inside(const wl_t *s, int32_t hs)
{
    uint32_t i, r;
    for (r = 0; r < WL_HN; r++)
        for (i = 0; i < WL_NP; i++)
            if (s->dy[r][i] > hs || s->dy[r][i] < -hs)
                return 0;
    return 1;
}
static int32_t height(const wl_t *s)                     /* the newest ridge's peak to peak, px */
{
    int32_t mx = -127, mn = 127;
    uint32_t i;
    for (i = 0; i < WL_NP; i++) {
        mx = s->dy[0][i] > mx ? s->dy[0][i] : mx;
        mn = s->dy[0][i] < mn ? s->dy[0][i] : mn;
    }
    return mx - mn;
}
/* two ridges as drawn: each rasterised (the segments between the points, 3 px apart), and the distance of the two sets of pixels, the larger of the two directions (every lit pixel of one to the
 * nearest lit pixel of the other, the larger of the column and the row distance) */
#define PW (3 * WL_NP)
#define PH 128
static void raster(const int8_t *dy, uint8_t *bm)
{
    int32_t p;
    memset(bm, 0, PW * PH);
    for (p = 0; p + 1 < (int32_t)WL_NP; p++) {
        int32_t x0 = 3 * p, y0 = dy[p] + PH / 2, x1 = x0 + 3, y1 = dy[p + 1] + PH / 2, dx = x1 - x0, ay = y1 > y0 ? y1 - y0 : y0 - y1, n = ay > 3 ? ay : 3, k;
        for (k = 0; k <= n; k++) {
            int32_t x = x0 + (dx * k + n / 2) / n, y = y0 + ((y1 - y0) * k + (y1 > y0 ? n / 2 : -n / 2)) / n;
            if (y >= 0 && y < PH && x < PW)
                bm[y * PW + x] = 1;
        }
    }
}
static int32_t reach(const uint8_t *a, const uint8_t *b)
{
    int32_t worst = 0, x, y;
    for (y = 0; y < PH; y++)
        for (x = 0; x < PW; x++) {
            int32_t r;
            if (!a[y * PW + x])
                continue;
            for (r = 0; r < 40; r++) {
                int32_t u, v, hit = 0;
                for (v = y - r; v <= y + r && !hit; v++)
                    for (u = x - r; u <= x + r; u++)
                        if (u >= 0 && u < PW && v >= 0 && v < PH && b[v * PW + u]) {
                            hit = 1;
                            break;
                        }
                if (hit)
                    break;
            }
            worst = r > worst ? r : worst;
        }
    return worst;
}
static int32_t picdist(const int8_t *a, const int8_t *b)
{
    static uint8_t ba[PW * PH], bb[PW * PH];
    int32_t d1, d2;
    raster(a, ba);
    raster(b, bb);
    d1 = reach(ba, bb);
    d2 = reach(bb, ba);
    return d1 > d2 ? d1 : d2;
}

/* the same signal, ten different starting points: each time a fresh reader, the signal's counter values shifted */
static void trigger_offsets(const char *name, wsig_t f, int32_t tol)
{
    int8_t pic[10][WL_NP];
    uint32_t o;
    int32_t worst = 0;
    sig_m = f;
    for (o = 0; o < 10u; o++) {
        uint32_t cnt = 5000u + o * 37u;
        ring(0, cnt);
        fresh(0);
        wl_step(0, HS);
        memcpy(pic[o], wl[0].dy[0], WL_NP);
    }
    for (o = 1; o < 10u; o++) {
        int32_t d = picdist(pic[0], pic[o]);
        worst = d > worst ? d : worst;
    }
    {
        char w[160];
        snprintf(w, sizeof w, "trigger: %s at 10 starting offsets, the pictures are %d px apart (need %d)", name, worst, tol);
        ok(w, worst <= tol);
    }
}
/* ten pictures in a row (the writer 1,366 entries on each time, as 62 ms): the reader keeps its history */
static void trigger_frames(const char *name, wsig_t f)
{
    uint32_t j, changed = 0, cnt = 20000u, i;
    int8_t prev[WL_NP];
    sig_m = f;
    ring(0, cnt);
    fresh(0);
    wl_step(0, HS);
    cnt += 1366u;                                        /* (the first picture of a sound has no history to match against: it settles in the second) */
    ring(0, cnt);
    wl_step(0, HS);
    memcpy(prev, wl[0].dy[0], WL_NP);
    for (j = 0; j < 10u; j++) {
        cnt += 1366u;
        ring(0, cnt);
        wl_step(0, HS);
        for (i = 0; i < WL_NP; i++)
            changed += wl[0].dy[0][i] != prev[i];
        memcpy(prev, wl[0].dy[0], WL_NP);
    }
    {
        char w[160];
        snprintf(w, sizeof w, "trigger: %s as 10 pictures in a row, %u changed points (need 0)", name, changed);
        ok(w, changed == 0);
    }
}
/* one block of a part's mix as part_end taps it: a sine on the part's accumulators (the value in `amp`, Q15 of the mix), the block's number and gate in the state, the signal's own time running on
 * whether the block is tapped or not */
static void tap_block(mixpart_t *m, blockstate_t *b, uint32_t *blk, uint32_t *ts, int skip, int gate, int32_t amp)
{
    uint32_t i;
    (*blk)++;
    b->wv_blk = *blk;
    b->wv_on = (uint32_t)gate;
    if (!skip) {
        for (i = 0; i < CTL; i++) {
            int32_t v = (int32_t)((double)amp * sin(PI2 * 211.7 * (double)(*ts + i) / 22050.0 / 2.0));
            m->acc->l[i] = m->acc->r[i] = v;
        }
        wv_tap(m);
    }
    *ts += CTL;
}

int main(void)
{
    uint32_t j, cnt;
    int32_t hmax = 0, hmin = 1000;
    wsig_t sigs[3] = {saw100, ressaw, sine128};
    const char *names[3] = {"a saw (100 entries a cycle)", "a resonant saw", "a sine (128 entries a cycle)"};
    for (j = 0; j < 3u; j++) {
        trigger_offsets(names[j], sigs[j], 1);
        trigger_frames(names[j], sigs[j]);
    }
    trigger_offsets("a resonant saw at 100.37 entries a cycle", ressaw_f, 1);

    /* the beat: two sines 3 Hz apart */
    sig_m = beat;
    cnt = 30000u;
    ring(0, cnt);
    fresh(0);
    for (j = 0; j < 60u; j++) {
        cnt += 1366u;
        ring(0, cnt);
        wl_step(0, HS);
        if (j >= 16u) {                                  /* (the gain has met a peak by then) */
            int32_t h = height(&wl[0]);
            hmax = h > hmax ? h : hmax;
            hmin = h < hmin ? h : hmin;
        }
    }
    {
        char w[160];
        snprintf(w, sizeof w, "beat: two sines 3 Hz apart over 60 pictures: the height swings %d : %d px (need 4 : 1)", hmax, hmin);
        ok(w, hmax >= 4 * (hmin ? hmin : 1));
    }

    /* soft against loud */
    sig_m = sine128;
    ring(0, 8000u);
    fresh(0);
    wl_step(0, HS);
    {
        int32_t h1 = height(&wl[0]);
        sig_m = sine_q;                                  /* the same shape, 26 dB down: smaller, not blown up to full height */
        ring(0, 8000u);
        fresh(0);
        wl_step(0, HS);
        {
            char w[160];
            snprintf(w, sizeof w, "soft: a sine 26 dB down is drawn %d px, the loud one %d px (not blown up)", height(&wl[0]), h1);
            ok(w, height(&wl[0]) * 3 < h1);
        }
    }

    /* silence */
    sig_m = sine128;
    ring(0, 8000u);
    fresh(0);
    for (j = 0; j < 6u; j++) {
        ring(0, 8000u + (j + 1u) * 1366u);
        wl_step(0, HS);
    }
    ok("stack: a held note builds up WL_HN ridges, no more", wl[0].hn == WL_HN);
    wv_sil[0] = 1;
    wl_step(0, HS);
    {
        uint32_t i, flat = 1;
        for (i = 0; i < WL_NP; i++)
            flat &= wl[0].dy[0][i] == 0;
        ok("silence (the part's flag): one flat ridge, the history restarts", flat && wl[0].hn == 1);
    }
    sig_m = zero;
    ring(0, 8000u);
    fresh(0);
    wl_step(0, HS);
    ok("silence (a ring of zeros): one flat ridge", wl[0].hn == 1 && height(&wl[0]) == 0);
    sig_m = dither;                                      /* a wave under a pixel high (one step of dither: the tail of a decay) */
    ring(0, 8000u);
    fresh(0);
    wl_step(0, HS);
    ok("silence (a step of dither, under a pixel at the gain's floor): one flat ridge, not a stack of them", wl[0].hn == 1 && height(&wl[0]) == 0);
    sig_m = sine128;                                     /* waking: the hole's gap says where; entries from before it are not used */
    ring(0, 9000u);
    fresh(0);
    wv_gap[0] = 9000u - 100u;                            /* only 100 entries since it woke: not enough, the picture stays as it was */
    wl[0].h = HS;
    wl[0].got = 1;
    wl[0].dy[0][7] = 5;
    wl_step(0, HS);
    ok("waking from silence: under 200 entries since it woke, nothing is made from older ones", wl[0].dy[0][7] == 5);

    /* the counter past 2^32 (a signal with 128 entries a cycle, so that its phase is the same at counters 128 apart, and 2^32 is a whole number of them) */
    {
        int8_t lo[WL_NP];
        sig_m = saw128;
        ring(0, 128u * 20u + 17u);
        fresh(0);
        wl_step(0, HS);
        memcpy(lo, wl[0].dy[0], WL_NP);
        ring(0, 128u * 5u + 17u);                         /* the newest entry 657 past 2^32: the entries before it straddle the wrap */
        fresh(0);
        wl_step(0, HS);
        {
            char w[160];
            int32_t d = picdist(lo, wl[0].dy[0]);
            snprintf(w, sizeof w, "wrap: the counter just past 2^32 draws the picture it draws below it (%d px apart, need 1)", d);
            ok(w, d <= 1);
        }
        ring(0, 0xFFFFFFF0u);
        fresh(0);
        wl_step(0, HS);
        ok("wrap: the counter just under 2^32 reads on (base and gap compare by signed difference)", wl[0].hn == 1 && height(&wl[0]) > 5);
    }

    /* a torn read: the writer's counter moves on by more than the ring leaves between the read and its second look at the counter (WL_TEST_AFTER_COPY) */
    {
        wl_t before;
        int8_t keep[WL_NP];
        sig_m = saw100;
        ring(0, 9000u);
        fresh(0);
        wl_step(0, HS);
        ring(0, 9000u + 1366u);                          /* (the first picture of a sound has no history to match against: it settles in the second) */
        wl_step(0, HS);
        before = wl[0];
        memcpy(keep, wl[0].dy[0], WL_NP);
        tear_left = 1;                                   /* torn once: the retry reads clean and makes the picture */
        ring(0, 9000u + 2u * 1366u);
        wl_step(0, HS);
        {
            char w[200];
            snprintf(w, sizeof w, "torn once: the reader retries and makes the picture (tear_left %d, ridges %u after %u, %d px from the one before)", tear_left, wl[0].hn, before.hn, picdist(keep, wl[0].dy[0]));
            ok(w, tear_left == 0 && wl[0].hn == before.hn + 1u && picdist(keep, wl[0].dy[0]) <= 1);
        }
        before = wl[0];
        tear_left = 2;                                   /* torn on the retry too: nothing is drawn, the last picture stays */
        ring(0, 9000u + 3u * 1366u);
        wl_step(0, HS);
        ok("torn twice: no new picture, the last one stays", tear_left == 0 && memcmp(&before, &wl[0], sizeof before) == 0);
        tear_left = 0;
    }

    /* inside the box */
    {
        wsig_t all[4] = {square_fs, noise, ressaw, beat};
        const char *nm[4] = {"a full-scale square", "noise", "a resonant saw", "a beat"};
        int32_t hss[4] = {HS, 10, 36, 29};               /* the card's old box, a BOTH half of it, the old non-grid box, the page's area */
        uint32_t i, q, f;
        for (i = 0; i < 4u; i++) {
            int allin = 1;
            sig_m = all[i];
            for (q = 0; q < 4u; q++) {
                ring(0, 7000u + i);
                fresh(0);
                for (f = 0; f < 6u; f++) {
                    ring(0, 7000u + i + f * 1366u);
                    wl_step(0, hss[q]);
                    allin &= inside(&wl[0], hss[q]);
                }
            }
            {
                char w[160];
                snprintf(w, sizeof w, "inside: %s, four box heights: every point in the box", nm[i]);
                ok(w, allin);
            }
        }
    }

    /* the tap: what it writes, and the blocks it did not take */
    {
        mixpart_t m;
        blockstate_t bsx;
        uint32_t blk = 0, ts = 0, gap_before, real_gap;
        int8_t clean[WL_NP], skipped[WL_NP];
        memset(&m, 0, sizeof m);
        memset(&bsx, 0, sizeof bsx);
        m.pp = 1;
        m.acc = &pacc[1];
        m.bs = &bsx;
        m.n = CTL;
        pacc_on[1] = 1;
        wv_cnt[1] = wv_gap[1] = wv_last[1] = 0;
        wv_sil[1] = 0;
        {   /* the entry: the mean of four frames, half scale, 1/128 steps: 16384 on both channels (half of a full-scale mix) is 127 steps, a full-scale mix 128 (clamped), -32768 is -128 */
            uint32_t i;
            for (i = 0; i < CTL; i++)
                pacc[1].l[i] = pacc[1].r[i] = 16384;
            bsx.wv_blk = ++blk;
            bsx.wv_on = 1;
            wv_tap(&m);
            ok("tap: 16384 on both channels (a quarter of a full-scale mix) is entry 64", wv_m[1][0] == 64);
            for (i = 0; i < CTL; i++)
                pacc[1].l[i] = pacc[1].r[i] = 100000;
            bsx.wv_blk = ++blk;
            wv_tap(&m);
            ok("tap: a mix past full scale clamps at 127", wv_m[1][16] == 127);
            for (i = 0; i < CTL; i++)
                pacc[1].l[i] = pacc[1].r[i] = -100000;
            bsx.wv_blk = ++blk;
            wv_tap(&m);
            ok("tap: and at -128", wv_m[1][32] == -128);
        }
        wv_cnt[1] = wv_gap[1] = wv_last[1] = 0;
        blk = 0;
        for (j = 0; j < 40u; j++)
            tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
        ok("tap: forty blocks in a row leave no gap after the first", wv_gap[1] == 0u && wv_cnt[1] == 40u * 16u);
        tap_block(&m, &bsx, &blk, &ts, 1, 1, 16384);     /* part 2 does not run this block */
        gap_before = wv_cnt[1];
        tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
        ok("tap: a skipped block moves the gap up to the next block's first entry", wv_gap[1] == gap_before && wv_cnt[1] == gap_before + 16u);
        real_gap = wv_gap[1];                            /* (what the tap wrote: the reader below uses that, not the value it should have) */
        sig_m = zero;
        fresh(1);
        wl[1].base = 0;
        wv_gap[1] = real_gap;
        wl[1].h = HS;
        wl[1].got = 1;
        wl[1].dy[0][7] = 5;
        wl_step(1, HS);
        ok("tap: right after the hole (16 entries since it) the reader makes no picture and draws no seam", wl[1].dy[0][7] == 5);
        for (j = 0; j < 40u; j++)
            tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
        ok("tap: forty more blocks leave the gap where it was", wv_gap[1] == real_gap);
        wl_step(1, HS);
        memcpy(skipped, wl[1].dy[0], WL_NP);
        /* the same sound without the hole (every block there, the same time): the picture must be the same within 1 px */
        wv_cnt[1] = wv_gap[1] = wv_last[1] = 0;
        blk = 0;
        ts = 0;
        for (j = 0; j < 82u; j++)
            tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
        fresh(1);
        wl_step(1, HS);
        memcpy(clean, wl[1].dy[0], WL_NP);
        {
            char w[160];
            int32_t d = picdist(clean, skipped);
            snprintf(w, sizeof w, "tap: a picture after the hole equals the picture of the same sound without one (%d px apart, need 1)", d);
            ok(w, d <= 1);
        }
        /* a shut gate: blocks the tap did not take leave the same mark */
        wv_cnt[1] = wv_gap[1] = wv_last[1] = 0;
        blk = 0;
        for (j = 0; j < 20u; j++)
            tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
        tap_block(&m, &bsx, &blk, &ts, 0, 0, 16384);
        tap_block(&m, &bsx, &blk, &ts, 0, 0, 16384);
        gap_before = wv_cnt[1];
        tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
        ok("tap: blocks with the gate shut leave the same mark", wv_gap[1] == gap_before && wv_cnt[1] == gap_before + 16u);
        /* a late job: its block number is the one made for it, whatever wv_blk says by the time it runs */
        {
            uint32_t g0 = wv_gap[1];
            wv_blk = blk + 5u;
            tap_block(&m, &bsx, &blk, &ts, 0, 1, 16384);
            ok("tap: a late job (the global block count already past it) is still the next block: no gap", wv_gap[1] == g0);
        }
    }

    /* the gate and the page's clock */
    {
        int st;
        sig_m = sine128;
        ring(0, 9000u);
        ring(1, 9000u);
        wl_tk = 0xFFFFFFFFu;
        wl[0].got = wl[1].got = 0;
        wv_want = wv_blk;                                /* shut */
        ui_prefs &= ~PREF_ANIM_OFF;
        st = wl_tick();
        ok("gate: on the page the gate opens 256 blocks ahead and a picture is due", st == 1 && (int32_t)(wv_want - wv_blk) == 256);
        wv_want = wv_blk;
        ui_prefs |= PREF_ANIM_OFF;
        st = wl_tick();
        ok("gate: with ANIM OFF the gate stays shut and no picture is made (the last one stays)", st == 0 && (int32_t)(wv_want - wv_blk) <= 0);
        ui_prefs &= ~PREF_ANIM_OFF;
        wl[0].got = wl[1].got = 1;
        wl[0].hn = 3;
        wv_want = wv_blk;                                /* the page was left: the gate lapsed */
        wl_tk = 0xFFFFFFFFu;
        wl_tick();
        ok("gate: back on the page after the gate lapsed, the pictures start over", wl[0].hn == 0);
        wl[0].hn = 3;
        wl[0].got = wl[1].got = 1;
        wl_tk = mv_tick();
        st = wl_tick();
        ok("gate: a part change does not start the pictures over (both parts are always drawn)", st == 0 && wl[0].hn == 3);
    }

    printf("scope: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
