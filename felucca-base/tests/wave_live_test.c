/* SPDX-License-Identifier: GPL-3.0-only */
/* wave_live_test: the MORPH page's wave picture from REAL sound, end to end: the browser emulator's device (web/emu/felucca_web.c: the synth, fx.c's scope tap in the audio interrupt, the
 * main loop, ui_scope.c's reader, the screen) built natively, a fresh unit (M01, LAYER, the part selection on BOTH), the page reached with HOME, then keys played. The picture is judged on
 * the screen itself: in each part's half, column by column, the lowest lit row is the front ridge's foot; the front is ALIVE in a frame when those rows span 3 rows or more (a flat line
 * spans 0). scope_test feeds synthetic rings and passed while the page showed a flat stack in the emulator (b5 WIP, 2026-11-08: the 8-bit tap at 1/128 of the half scale turned a held chord
 * into flat ridges within half a second); this test takes the sound path the player hears.
 *   stab      M01 as shipped (FM6 BIG STAB, a short stab): a chord struck every 450 ms for 3 s (as the WAVE LAB harness): the two parts alive in 40 % of the frames together (b5 WIP: 28 %)
 *   held      a held 4-note chord sequence on both parts (sustain full, PLAY): each part alive in 80 % of the frames, and the picture moves (frames differ)
 *   quiet     the same with both parts' MIXER LEVEL at 60 (the tap is after it): each part alive in 80 % of the frames (b5 WIP, the 8-bit tap: 1 and 2 of 20, a flat stack)
 *   busy      #18 PEDAL BASS with the sequence and the chord: each half of the picture at most WL_HN pieces (one unbroken line a ridge; the b5 merge's occluded stack fell into dozens)
 *   select    the ALGORITHM encoder on the page: PART 1, BOTH, PART 2 in that order and stopping at the ends; the picture alive in all three
 *   off HOME  an edit page: the encoder picks PART 1 / PART 2 only, BOTH stays for HOME
 * Run: wave_live_test (exit 0 = all ok) */
#include "../web/emu/felucca_web.c"
#ifndef QUIET_LVL
#define QUIET_LVL 60u                                    /* (the quiet scene's MIXER LEVEL: unity is 112) */
#endif

static int bad, ncheck;
static void ok(const char *what, int c)
{
    ncheck++;
    printf("wavelive: %-110s %s\n", what, c ? "ok" : "FAIL");
    if (!c)
        bad++;
}
static void run(uint32_t ms)
{
    uint32_t t;
    for (t = 0; t < ms; t++)
        web_render(48);                                  /* (1 ms at 48 kHz) */
}
static void press(uint32_t b)
{
    web_buttons(1u << b);
    run(120);
    web_buttons(0);
    run(300);
}
static void turn(uint32_t role, int32_t n)
{
    web_enc(role, n);
    run(150);
}
/* the picture's halves, as ui_scope.c wl_area / wl_card lays them out */
static int32_t pic_y0, pic_ym, pic_y1;
static void pic_rows(void)
{
    int32_t ay, ah;
    area_rect(&ay, &ah);
    pic_y0 = ay;
    pic_y1 = ay + ah - 20;
    pic_ym = (pic_y0 + pic_y1) / 2;
}
/* a pixel's brightness (the frame buffer holds RGB565 high byte first, as the LCD takes it) */
static int32_t lum(uint32_t x, uint32_t y)
{
    const uint16_t q = web_fb[y * 240u + x], v = (uint16_t)(q >> 8 | q << 8);
    return 2 * ((v >> 11) & 31) + ((v >> 5) & 63) + 2 * (v & 31);
}
/* the front line's span in rows y0 .. y1 - 1, whatever the style draws under it: in each column 40 .. 228 (right of the part's tag) the lowest pixel at 70 % of the half's brightest or
 * brighter (the front is the brightest line), max - min of those rows; -1: nothing lit */
static int32_t foot_span(int32_t y0, int32_t y1)
{
    int32_t x, y, top = 0, lo = 1000, hi = -1;
    for (y = y0 + 1; y < y1; y++)
        for (x = 40; x <= 228; x++)
            top = lum((uint32_t)x, (uint32_t)y) > top ? lum((uint32_t)x, (uint32_t)y) : top;
    if (top < 20)
        return -1;
    for (x = 40; x <= 228; x++) {
        for (y = y1 - 1; y > y0; y--)
            if (lum((uint32_t)x, (uint32_t)y) * 10 >= top * 7)
                break;
        if (y == y0)
            continue;
        lo = y < lo ? y : lo;
        hi = y > hi ? y : hi;
    }
    return hi < 0 ? -1 : hi - lo;
}
/* the pieces the picture falls into in rows y0 .. y1 - 1, x 22 .. 236 (right of the tag and the bar): 8-connected groups of lit pixels. One unbroken line a ridge is at most
 * WL_HN pieces (fewer where lines touch); lines cut into fragments (the b5 merge's look, owner 2026-11-09: "noodles scattered") are many */
static int32_t pieces(int32_t y0, int32_t y1)
{
    static uint8_t seen[240 * 240];
    static uint16_t stk[240 * 240];
    const int32_t bg = lum(238u, (uint32_t)(y0 + 2));   /* (lit: brighter than the background; the faint anti-aliased back lines count) */
    int32_t x, y, n = 0;
    memset(seen, 0, sizeof seen);
    for (y = y0 + 1; y < y1; y++)
        for (x = 22; x <= 236; x++) {
            uint32_t sp = 0;
            if (seen[y * 240 + x] || lum((uint32_t)x, (uint32_t)y) <= bg)
                continue;
            n++;
            seen[y * 240 + x] = 1;
            stk[sp++] = (uint16_t)(y * 240 + x);
            while (sp) {
                const int32_t q = stk[--sp], qx = q % 240, qy = q / 240;
                int32_t dx, dy;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        const int32_t nx = qx + dx, ny = qy + dy;
                        if (nx < 22 || nx > 236 || ny <= y0 || ny >= y1 || seen[ny * 240 + nx] || lum((uint32_t)nx, (uint32_t)ny) <= bg)
                            continue;
                        seen[ny * 240 + nx] = 1;
                        stk[sp++] = (uint16_t)(ny * 240 + nx);
                    }
            }
        }
    return n;
}
static uint32_t pic_hash(void)
{
    uint32_t h = 2166136261u, i;
    for (i = (uint32_t)pic_y0 * 240u; i < (uint32_t)pic_y1 * 240u; i++)
        h = (h ^ web_fb[i]) * 16777619u;
    return h;
}
/* frames every 100 ms for ms: how many each half was alive in, and how many distinct pictures; strike: the chord again every 450 ms */
static void watch(uint32_t ms, uint32_t mask, int strike, uint32_t *alive0, uint32_t *alive1, uint32_t *frames, uint32_t *moved)
{
    uint32_t t, last = 0;
    *alive0 = *alive1 = *frames = *moved = 0;
    for (t = 0; t < ms; t += 50u) {
        if (strike && t % 450u < 50u) {
            web_keys(0);
            run(20);
            web_keys(mask);
            run(30);
        } else
            run(50);
        if (t % 100u == 0u) {
            uint32_t h = pic_hash();
            *alive0 += foot_span(pic_y0, pic_ym) >= 3;
            *alive1 += foot_span(pic_ym, pic_y1) >= 3;
            *moved += h != last;
            last = h;
            (*frames)++;
        }
    }
}

int main(void)
{
    const uint32_t chord = (1u << 0) | (1u << 4) | (1u << 7);
    uint32_t a0, a1, n, mv, i;
    char w[200];
    web_nor_erase();
    web_boot();
    run(2500);
    press(B_HOME);                                       /* (the first start's splash: HOME skips the GUIDE) */
    for (i = 0; i < 6u && cur_page()->graph != GR_MORPH; i++)
        press(B_HOME);
    pic_rows();
    ok("the MORPH page is up, the part selection on BOTH (a new unit in LAYER)", cur_page()->graph == GR_MORPH && SEL_BOTH == 1u);

    /* stab: M01 as shipped, a chord struck every 450 ms */
    run(300);
    watch(3000, chord, 1, &a0, &a1, &n, &mv);
    snprintf(w, sizeof w, "stab: M01, a chord every 450 ms for 3 s: part 1 alive in %u, part 2 in %u of %u frames (need 40 %% together)", a0, a1, n);
    ok(w, (a0 + a1) * 5u >= 2u * n * 2u);
    web_keys(0);
    run(1500);

    /* held: a chord sequence on both parts, sustained, PLAY; a chord held on the keys too */
    web_test_heavy();
    press(B_PLAY);
    web_keys(chord);
    run(1000);
    watch(3000, chord, 0, &a0, &a1, &n, &mv);
    snprintf(w, sizeof w, "held: a held chord sequence on both parts: part 1 alive in %u, part 2 in %u of %u frames (need 80 %%), %u pictures differ", a0, a1, n, mv);
    ok(w, a0 * 5u >= n * 4u && a1 * 5u >= n * 4u && mv * 2u >= n);

    /* quiet: the same, both parts' MIXER LEVEL turned down to QUIET_LVL (the tap is after it): the shape must still show */
    {
        uint32_t lv0 = trk[0].p[P_LEVEL], lv1 = trk[1].p[P_LEVEL], k, pk[2] = {0, 0};
        trk[0].p[P_LEVEL] = trk[1].p[P_LEVEL] = QUIET_LVL;
        run(800);
        for (k = 0; k < 2u; k++)
            for (i = 0; i < WVR_N; i++)
                pk[k] = (uint32_t)(wv_m[k][i] < 0 ? -wv_m[k][i] : wv_m[k][i]) > pk[k] ? (uint32_t)(wv_m[k][i] < 0 ? -wv_m[k][i] : wv_m[k][i]) : pk[k];
        watch(2000, chord, 0, &a0, &a1, &n, &mv);
        snprintf(w, sizeof w, "quiet: the parts' LEVEL at %u (the ring's largest code %u, %u): part 1 alive in %u, part 2 in %u of %u frames (need 80 %%)", (unsigned)QUIET_LVL, pk[0], pk[1], a0, a1, n);
        ok(w, a0 * 5u >= n * 4u && a1 * 5u >= n * 4u);
        trk[0].p[P_LEVEL] = (uint8_t)lv0;
        trk[1].p[P_LEVEL] = (uint8_t)lv1;
        run(500);
    }

    /* busy: #18 PEDAL BASS on part 1 with the sequence and the chord (the owner's scene, 2026-11-09): every ridge one unbroken line, so each half is at most WL_HN pieces */
    {
        uint32_t f, worst0 = 0, worst1 = 0, sel = song.sel, both = SEL_BOTH;
        song.sel = 0;
        preset_go(17);                                   /* (#18 PEDAL BASS) */
        song.sel = (uint8_t)sel;
        SEL_BOTH = (uint8_t)both;
        run(1500);
        for (f = 0; f < 20u; f++) {
            uint32_t p0, p1;
            run(100);
            p0 = (uint32_t)pieces(pic_y0, pic_ym);
            p1 = (uint32_t)pieces(pic_ym, pic_y1);
            worst0 = p0 > worst0 ? p0 : worst0;
            worst1 = p1 > worst1 ? p1 : worst1;
        }
        snprintf(w, sizeof w, "busy: PEDAL BASS, sequence and chord: the picture is at most %u pieces a half (part 1 %u, part 2 %u: no fragments)", (unsigned)WL_HN, worst0, worst1);
        ok(w, worst0 <= WL_HN && worst1 <= WL_HN);
    }

    /* select: PART 1, BOTH, PART 2 on the page */
    turn(EN_ALGO, -1);
    ok("select: from BOTH one detent left is PART 1", !SEL_BOTH && song.sel == 0u);
    turn(EN_ALGO, -1);
    ok("select: and it stops there", !SEL_BOTH && song.sel == 0u);
    watch(1000, chord, 0, &a0, &a1, &n, &mv);
    snprintf(w, sizeof w, "select: PART 1, the picture alive (%u, %u of %u)", a0, a1, n);
    ok(w, a0 * 5u >= n * 4u && a1 * 5u >= n * 4u);
    turn(EN_ALGO, 1);
    ok("select: one right is BOTH (the part kept for the edit pages: PART 1)", SEL_BOTH && song.sel == 0u);
    watch(1000, chord, 0, &a0, &a1, &n, &mv);
    snprintf(w, sizeof w, "select: BOTH, the picture alive (%u, %u of %u)", a0, a1, n);
    ok(w, a0 * 5u >= n * 4u && a1 * 5u >= n * 4u);
    turn(EN_ALGO, 1);
    ok("select: one more right is PART 2", !SEL_BOTH && song.sel == 1u);
    turn(EN_ALGO, 1);
    ok("select: and it stops there", !SEL_BOTH && song.sel == 1u);
    watch(1000, chord, 0, &a0, &a1, &n, &mv);
    snprintf(w, sizeof w, "select: PART 2, the picture alive (%u, %u of %u)", a0, a1, n);
    ok(w, a0 * 5u >= n * 4u && a1 * 5u >= n * 4u);

    /* off HOME: an edit page picks PART 1 / PART 2 only; BOTH stays for the HOME pages */
    turn(EN_ALGO, -1);
    ok("select: back to BOTH", SEL_BOTH == 1u);
    press(B_ENV);
    ok("off HOME: the ENV page is not a BOTH page", !HOME_BOTH_PAGE(cur_page()) && !ui.perf);
    {
        uint32_t lit = badge_lit();
        ok("off HOME: one capsule lit (the part it edits)", lit == 1u << song.sel);
    }
    turn(EN_ALGO, 1);
    ok("off HOME: a turn right picks PART 2, BOTH stays set for HOME", song.sel == 1u && SEL_BOTH == 1u);
    turn(EN_ALGO, 1);
    ok("off HOME: no third position (still PART 2)", song.sel == 1u && SEL_BOTH == 1u);
    turn(EN_ALGO, -1);
    ok("off HOME: a turn left picks PART 1", song.sel == 0u && SEL_BOTH == 1u);
    press(B_HOME);
    ok("back on HOME's MORPH page: BOTH, both capsules lit", cur_page()->graph == GR_MORPH && SEL_BOTH == 1u && badge_lit() == 3u);

    printf("wavelive: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
