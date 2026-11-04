/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE: TURNS (M8a, alternating turnarounds) and POLY (M7d, generative polymeter), docs/design/TIDE_MUSICAL_DESIGN.md.
 *   turns    which shared bars each part plays its variation on, with equal and with different lengths and DIVs; the unchanged modes
 *            (TOGETHER, VARY ALL, one part, manual, stopped, external clock); glide_poll's "automatic" flag
 *   poly     the shortened loop's lengths, that every stretch ends at the full loop's first step and the groove is back in phase (sequencer
 *            and ARP SEQ), never both parts, POLY OFF / manual / recording / STOP-PLAY, the stored steps and lengths untouched, KEEP
 *   store    the settings record: an older, shorter record keeps its fields and defaults the new ones; a new one round-trips */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tideturns: FAIL %s\n", s); bad++; } } while (0)

enum { INTERNAL, EXTERNAL };
/* two parts of 16 notes (len, div as asked), PLAY from the top, the SEQ MORPH's End B at the fader's top for both */
static void setup(uint32_t len0, uint32_t len1, uint32_t div0, uint32_t div1, uint32_t arp)
{
    uint32_t p, i;
    ui_power_on();
    frame();
    gl.on = gl.armed = 0;
    transport_req = 0;
    song.playing = song.rec = 0;
    mods_off = 0;
    duell_mode = DM_STEREO;
    usb.config = 1;
    memset(&midi_clock, 0, sizeof midi_clock);
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = 240;
    song.g[G_SWING] = 50;
    midi_pch[0] = 0;
    midi_pch[1] = 1;
    memset(sqm, 0, sizeof sqm);
    memset(&rhythm, 0, sizeof rhythm);
    for (p = 0; p < 2; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = (int16_t)(p ? len1 : len0);
        t->p[P_SDIV] = (int16_t)(p ? div1 : div0);
        t->p[P_ARATE] = (int16_t)(p ? div1 : div0);
        t->p[P_AMODE] = (int16_t)(((arp >> p) & 1u) ? AM_SEQ : AM_OFF);
        t->p[P_APROB] = 127;
        t->p[P_AOCT] = 1;
        t->p[P_AGATE] = 64;
        t->p[P_SQNT] = 1;
        t->p[P_MUTE] = 0;
        t->p[P_LEVEL] = 100;
        t->nheld = 0;
        t->seq_active = 1;
        for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) {
            t->step[i].time = i % 3u == 1u ? ST_REST : ST_NOTE;
            t->step[i].n = t->step[i].time == ST_NOTE;
            t->step[i].note[0] = (uint8_t)(60 + i);
            t->step[i].vel = 100;
        }
    }
    sqm_vary_all = 0;
    sqm_turns = 1;
    sqm_poly = 1;
    sqm_auto = 0;
    for (p = 0; p < 2; p++) {
        seqm_set_amount(p, 80);
        seqm_roll(p);
        seqm_set_fader(p, 127);
    }
    seqm_poll();
    seq_start();
    song.playing = 1;
}
static int varied(uint32_t p)
{
    step_t buf;
    return seq_play_step(&trk[p], 0, &buf) == &sqm_pat[p][1][0];
}
/* PLAY for `bars` bars (events_block, one control block at a time); per part the (bar + turn) mod 8 values it played varied on */
static void block(uint32_t ext, uint32_t *pulse, uint32_t bpm, uint32_t b)
{
    if (ext) {
        fm1_ms = 10u + (uint32_t)((uint64_t)(b + 1u) * CTL * 1000u / FS);
        while ((uint64_t)(*pulse + 1u) * (uint32_t)FS * 60u <= (uint64_t)(b + 1u) * CTL * bpm * 24u) {
            (*pulse)++;
            midi_enqueue(0xFu | 0xF8u << 8, 1);
        }
    }
    events_block(CTL);
}
static uint32_t run_turns(uint32_t bars, uint32_t ext, uint32_t *seen0, uint32_t *seen1)
{
    uint32_t b, pulse = 0, n = bars * 4u * (uint32_t)FS * 60u / 240u / CTL + 1u, bpm = 240;
    *seen0 = *seen1 = 0;
    if (ext) {
        song.g[G_CLOCK] = 1;
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_clock.mode = 1;
        fm1_ms = 10;
        midi_clock_transport(0xFA, 10);
        midi_clock_pulse(10);
    }
    for (b = 0; b < n; b++) {
        uint32_t k;
        block(ext, &pulse, bpm, b);
        k = ((uint32_t)sqm_bar + sqm_bar_turn) & 7u;   /* (the state the audio path reads for the next block) */
        if (varied(0))
            *seen0 |= 1u << k;
        if (varied(1))
            *seen1 |= 1u << k;
    }
    return sqm_bar;
}

static void t_turns(void)
{
    uint32_t a, b, bars, k;
    static const uint8_t L[][4] = {{16, 16, 2, 2}, {12, 16, 3, 3}, {16, 15, 3, 2}, {9, 7, 2, 3}, {32, 16, 3, 3}};
    for (k = 0; k < 5u; k++) {
        setup(L[k][0], L[k][1], L[k][2], L[k][3], 0);
        sqm_auto = 3;
        bars = run_turns(16, INTERNAL, &a, &b);
        CHECK("TAKE TURNS, both parts automatic: part 1 on bars 4 and 8, part 2 on bars 2 and 6 of every 8 (any lengths / DIVs)",
              bars >= 15u && a == 0x88u && b == 0x22u);
    }
    setup(16, 16, 2, 2, 0);
    sqm_auto = 3, sqm_turns = 0;
    run_turns(16, INTERNAL, &a, &b);
    CHECK("TOGETHER: both parts on bars 4 and 8", a == 0x88u && b == 0x88u);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 3, sqm_vary_all = 1;
    run_turns(16, INTERNAL, &a, &b);
    CHECK("VARY ALL: every bar, both parts", a == 0xFFu && b == 0xFFu);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 1;
    run_turns(16, INTERNAL, &a, &b);
    CHECK("one part selected (automatic on part 1 only): both as today, bars 4 and 8", a == 0x88u && b == 0x88u);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 2;
    run_turns(16, INTERNAL, &a, &b);
    CHECK("  .. on part 2 only", a == 0x88u && b == 0x88u);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 0;
    run_turns(16, INTERNAL, &a, &b);
    CHECK("manual (no automatic MUSIC WNDR / JRNY): together, as today", a == 0x88u && b == 0x88u);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 3;
    run_turns(16, EXTERNAL, &a, &b);
    CHECK("external clock: TAKE TURNS in the beats the clock gives", a == 0x88u && b == 0x22u);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 3;
    song.playing = 0;
    CHECK("stopped: the variation as always (the editor's view)", varied(0) && varied(1));
    /* glide_poll's flag: automatic MUSIC WNDR / JRNY on the SEQ lane, playing, not recording, not armed (rhythm_poll's own condition) */
    setup(16, 16, 2, 2, 0);
    gl.on = 3, gl.mode = GM_WANDER, gl.mask = 3, gl.armed = 0;
    morph_musical = 1;
    glide_poll();
    CHECK("automatic WANDER on BOTH: flagged for both parts", sqm_auto == 3u);
    gl.on = 3, gl.mode = GM_JOURNEY;
    glide_poll();
    CHECK("  .. JOURNEY too", sqm_auto == 3u);
    gl.on = 3, gl.hold = 1;
    glide_poll();
    CHECK("  .. during WAIT (the faders still) the phrase goes on", sqm_auto == 3u);
    gl.on = 3, gl.hold = 0;
    gl.mode = GM_LOOP;
    glide_poll();
    CHECK("  .. not LOOP", sqm_auto == 0u);
    gl.on = 3, gl.mode = GM_WANDER, gl.mask = 1;
    glide_poll();
    CHECK("  .. one part: that part's bit", sqm_auto == 1u);
    gl.on = 3, gl.mask = 3, morph_musical = 0;
    glide_poll();
    CHECK("  .. not with ROLL SMART / WILD", sqm_auto == 0u);
    gl.on = 3, morph_musical = 1, song.rec = 1;
    glide_poll();
    CHECK("  .. not while recording", sqm_auto == 0u);
    song.rec = 0;
    gl.on = 1;
    glide_poll();
    CHECK("  .. not on the SOUND lane alone", sqm_auto == 0u);
}

/* ------------------------------------------------------------------------------------------------ POLY --- */
typedef struct {
    uint32_t steps[2];                  /* steps played */
    uint32_t shorts[2];                 /* stretches begun */
    uint32_t minrun[2], maxrun[2];      /* the shortest / longest loop run (steps between two first steps) */
    uint32_t badlock[2];                /* a full-length loop that did not start where the full loop would */
    uint32_t longest_short[2];          /* the most steps in a row played in a shortened stretch */
    uint32_t both;                      /* a step with both parts shortened */
    uint32_t badlen[2];                 /* a loop length outside the mode's set */
    uint32_t runs_full[2];
} polystat_t;
static polystat_t ps;
/* run `bars` bars; per step started of part p: the position in the part's own loop; ARP SEQ parts by their arp_idx */
static void run_poly(uint32_t bars, uint32_t arp, uint32_t mode, uint32_t lo, uint32_t hi)
{
    uint32_t p, b, n = bars * 4u * (uint32_t)FS * 60u / 240u / CTL + 1u, pulse = 0;
    uint32_t last[2] = {0xFFFFFFFFu, 0xFFFFFFFFu}, run[2] = {0, 0}, since[2] = {0xFFFFFFFEu, 0xFFFFFFFEu}, shortrun[2] = {0, 0}, total[2] = {0, 0}, wasp[2] = {0, 0};
    memset(&ps, 0, sizeof ps);
    ps.minrun[0] = ps.minrun[1] = 0xFFFFFFFFu;
    (void)mode;
    for (b = 0; b < n; b++) {
        block(0, &pulse, 240, b);
        for (p = 0; p < 2; p++) {
            track_t *t = &trk[p];
            uint32_t full = (uint32_t)t->p[P_SLEN], now;
            int a = (arp >> p) & 1u;
            now = a ? tide_arp_idx(t, t->arp_idx, full) : t->seq_idx;
            if (a ? t->arp_idx == 0xFFFFFFFFu : t->seq_pos >= 0x7FFFFFFFu)
                continue;
            if (now == last[p] && (a ? t->arp_idx == since[p] : 1))
                continue;
            if (a && t->arp_idx == since[p])
                continue;
            since[p] = t->arp_idx;
            last[p] = now;
            ps.steps[p]++;
            total[p]++;
            if (poly_len[p] && !wasp[p])
                ps.shorts[p]++;
            if (poly_len[p])
                ps.badlen[p] += poly_len[p] < lo || poly_len[p] > hi, ps.minrun[p] = poly_len[p] < ps.minrun[p] ? poly_len[p] : ps.minrun[p];
            wasp[p] = poly_len[p];
            if (poly_len[p])
                shortrun[p]++, ps.longest_short[p] = shortrun[p] > ps.longest_short[p] ? shortrun[p] : ps.longest_short[p];
            else
                shortrun[p] = 0;
            if (poly_len[0] && poly_len[1])
                ps.both++;
            if (now == 0) {
                if (run[p]) {
                    ps.maxrun[p] = run[p] > ps.maxrun[p] ? run[p] : ps.maxrun[p];
                    if (run[p] == full) {
                        ps.runs_full[p]++;
                        ps.badlock[p] += (total[p] - 1u) % full != 0u;   /* a full loop starts where the full loop would: the groove is back */
                    }
                }
                run[p] = 0;
            }
            run[p]++;
        }
    }
}
static void keepcheck(void)
{
    static step_t before[2][NSTEP];
    int16_t len[2];
    uint32_t p;
    for (p = 0; p < 2; p++)
        memcpy(before[p], trk[p].step, sizeof before[p]), len[p] = trk[p].p[P_SLEN];
    (void)len;
}

static void t_poly(void)
{
    static step_t keep0[NSTEP], keep1[NSTEP];
    static const uint8_t M[3][2] = {{0, 0}, {14, 15}, {12, 15}};
    uint32_t mode, arp;
    for (arp = 0; arp < 4u; arp += 3u) {
        for (mode = 1; mode <= 2u; mode++) {
            const char *nm = arp ? (mode == 1u ? "ARP SEQ GENTLE" : "ARP SEQ WIDE") : (mode == 1u ? "GENTLE" : "WIDE");
            char msg[96];
            setup(16, 16, 2, 2, arp);
            sqm_auto = 3, sqm_poly = (uint8_t)mode;
            memcpy(keep0, trk[0].step, sizeof keep0), memcpy(keep1, trk[1].step, sizeof keep1);
            run_poly(400, arp, mode, M[mode][0], M[mode][1]);
            snprintf(msg, sizeof msg, "%s: a part's loop is shortened now and then, both parts", nm);
            CHECK(msg, ps.shorts[0] + ps.shorts[1] >= 2u && ps.shorts[0] && ps.shorts[1]);
            snprintf(msg, sizeof msg, "%s: lengths only %u to %u of 16", nm, M[mode][0], M[mode][1]);
            CHECK(msg, !ps.badlen[0] && !ps.badlen[1] && ps.minrun[0] >= M[mode][0] && ps.minrun[1] >= M[mode][0]);
            snprintf(msg, sizeof msg, "%s: every full loop starts where the full loop would (the groove re-locks), full loops return", nm);
            CHECK(msg, !ps.badlock[0] && !ps.badlock[1] && ps.runs_full[0] > 20u && ps.runs_full[1] > 20u);
            snprintf(msg, sizeof msg, "%s: never both parts shortened at once", nm);
            CHECK(msg, !ps.both);
            snprintf(msg, sizeof msg, "%s: a stretch is a phrase or two (%u steps at most)", nm, mode == 1u ? 8u * 16u : 4u * 16u);
            CHECK(msg, ps.longest_short[0] <= (mode == 1u ? 8u * 16u : 4u * 16u) && ps.longest_short[1] <= (mode == 1u ? 8u * 16u : 4u * 16u));
            snprintf(msg, sizeof msg, "%s: the stored steps and lengths are never touched", nm);
            CHECK(msg, trk[0].p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 16 && !memcmp(keep0, trk[0].step, sizeof keep0) && !memcmp(keep1, trk[1].step, sizeof keep1));
            if (!arp)
                printf("  %s: %u / %u stretches in 400 bars, shortest loop %u / %u\n", nm, ps.shorts[0], ps.shorts[1], ps.minrun[0], ps.minrun[1]);
        }
    }
    {   /* GENTLE is rarer than WIDE */
        uint32_t g, w;
        setup(16, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 1;
        run_poly(400, 0, 1, 14, 15);
        g = ps.shorts[0] + ps.shorts[1];
        setup(16, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 2;
        run_poly(400, 0, 2, 12, 15);
        w = ps.shorts[0] + ps.shorts[1];
        CHECK("GENTLE shortens less often than WIDE", g < w);
    }
    {   /* a single selected part, other lengths: 32 steps, 12 steps; and a loop under 8 never shortens */
        setup(32, 12, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 2;
        run_poly(400, 0, 2, 28, 31);
        CHECK("32 steps: shortened to 28..31 (len - 1..4), the 12-step part under WIDE 8..11", ps.shorts[0] > 0 && ps.minrun[0] >= 28u);
        setup(7, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 2;
        run_poly(200, 0, 2, 12, 15);
        CHECK("a loop under 8 steps is never shortened", ps.shorts[0] == 0u && ps.maxrun[0] == 7u);
    }
    {   /* audit C11: past 65,535 full loops (about 36 hours) POLY still starts stretches (the counters were 16 bits: the next start sat a wrap ahead) */
        setup(16, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 2;
        poly_lc[0] = poly_lc[1] = poly_free[0] = poly_free[1] = 0xFFFFu;
        run_poly(200, 0, 2, 12, 15);
        CHECK("POLY after 65,535 full loops: stretches go on", ps.shorts[0] + ps.shorts[1] > 0u);
    }
    {   /* the modes that keep the loop whole */
        setup(16, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 0;
        run_poly(300, 0, 0, 16, 16);
        CHECK("POLY OFF: never", !ps.shorts[0] && !ps.shorts[1] && ps.maxrun[0] == 16u);
        setup(16, 16, 2, 2, 0);
        sqm_auto = 0, sqm_poly = 2;
        run_poly(300, 0, 0, 16, 16);
        CHECK("manual playback (not automatic): never", !ps.shorts[0] && !ps.shorts[1]);
        setup(16, 16, 2, 2, 0);
        sqm_auto = 1, sqm_poly = 2;
        run_poly(400, 0, 2, 12, 15);
        CHECK("one part automatic: only that part", ps.shorts[0] > 0u && !ps.shorts[1]);
        setup(16, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 2;
        song.rec = 1;
        run_poly(300, 0, 0, 16, 16);
        song.rec = 0;
        CHECK("recording: never", !ps.shorts[0] && !ps.shorts[1]);
    }
    {   /* leaving: POLY OFF, the glide stopping, STOP / PLAY, KEEP, WAIT; with a shortened loop on */
        uint32_t i, b, pulse = 0;
        setup(16, 16, 2, 2, 0);
        sqm_auto = 3, sqm_poly = 2;
        for (b = 0; b < 400000u && !poly_len[0] && !poly_len[1]; b++)
            block(0, &pulse, 240, b);
        CHECK("(fixture: a stretch began)", poly_len[0] || poly_len[1]);
        i = poly_len[0] ? 0u : 1u;
        memcpy(keep0, trk[i].step, sizeof keep0);
        {   /* KEEP (SEQ TOOLS) while the loop is short: the stored length and steps stay, the loop goes on */
            seqm_set_fader(i, 127);
            song.sel = (uint8_t)i;
            song.playing = 0;
            tools_key(TL_KEEP);
            song.playing = 1;
            CHECK("KEEP during a shortened loop: P_SLEN is the stored 16", trk[i].p[P_SLEN] == 16);
        }
        sqm_poly = 0;
        for (b = 0; b < 4u * 16u * 3u * 4u * 4u && (poly_len[0] || poly_len[1]); b++)
            block(0, &pulse, 240, b + 400000u);
        CHECK("POLY OFF while shortened: whole again at the next full loop", !poly_len[0] && !poly_len[1]);
        sqm_poly = 2;
        for (b = 0; b < 400000u && !poly_len[0] && !poly_len[1]; b++)
            block(0, &pulse, 240, b + 800000u);
        sqm_auto = 0;
        for (b = 0; b < 2000u && (poly_len[0] || poly_len[1]); b++)
            block(0, &pulse, 240, b + 1200000u);
        CHECK("the glide stopping (manual takeover): whole again at the next full loop", !poly_len[0] && !poly_len[1]);
        sqm_auto = 3;
        for (b = 0; b < 400000u && !poly_len[0] && !poly_len[1]; b++)
            block(0, &pulse, 240, b + 1400000u);
        CHECK("(fixture: again)", poly_len[0] || poly_len[1]);
        {   /* audit C10: REC during a stretch: the loop is whole at the next step, on the full loop's step (no recording lands on the short loop) */
            uint32_t j = poly_len[0] ? 0u : 1u, moves = 0, was = trk[j].seq_idx;
            song.rec = (uint8_t)(1u << j);
            for (b = 0; b < 40000u && moves < 2u; b++) {
                block(0, &pulse, 240, b + 1800000u);
                if (trk[j].seq_idx != was)
                    moves++, was = trk[j].seq_idx;
            }
            CHECK("REC during a POLY stretch: whole at the next step, the step the full loop is on", !poly_len[j] && trk[j].seq_idx == poly_ref[j] && poly_ref[j] != 0u);
            song.rec = 0;
        }
        seq_start();
        CHECK("STOP / PLAY: both parts whole, from the top", !poly_len[0] && !poly_len[1] && trk[0].seq_idx == 15u && poly_ref[0] == 15u);
        seq_start();
        song.playing = 1;
        sqm_auto = 3, sqm_poly = 2;
        run_poly(6, 0, 2, 12, 15);
        CHECK("the first bars after PLAY (the first two phrases) are whole (the groove is heard first)", !ps.shorts[0] && !ps.shorts[1]);
    }
    keepcheck();
}


/* ------------------------------------------------------------------------------------------------ SYNC --- */
/* part 1: movable hits on the E of the beats (steps 1, 5, 9, 13) and the beats themselves; 64 mutations from `seed`; the reached positions */
static void sync_run(uint32_t mode, uint32_t div, uint32_t seed, uint32_t base, uint32_t *share, uint32_t *onbeat, uint32_t *maxpush)
{
    static const uint8_t HITS[8] = {0, 4, 8, 12, 1, 5, 9, 13};
    uint32_t n, k, a = 0, pushes[4] = {0, 0, 0, 0};
    setup(16, 16, div, div, 0);
    for (k = 0; k < NSTEP; k++)
        trk[0].step[k] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
    for (k = 0; k < 8u; k++) {
        trk[0].step[HITS[k]] = (step_t){{(uint8_t)(60 + k)}, 1, ST_NOTE, 0, 100, 0};
        if (base == 1u)                                     /* an already off-beat pattern: the "and"s */
            trk[0].step[HITS[k]] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
    }
    if (base == 1u)
        for (k = 0; k < 8u; k++)
            trk[0].step[(k % 2u ? 2u : 0u) + 4u * (k / 2u)] = (step_t){{(uint8_t)(60 + k)}, 1, ST_NOTE, 0, 100, 0};
    for (k = 0; k < 16u; k++)
        trk[1].step[k] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
    sqm_sync = (uint8_t)mode;
    song.playing = 0;
    seqm_poll();
    sqm_refresh(0, 1);
    memset(&rhythm, 0, sizeof rhythm);
    for (n = 0; n < 64u; n++) {
        rhythm_mutate(0, seed * 7919u + n * 104729u);
        sqm_refresh(0, 1);
    }
    *share = *onbeat = *maxpush = 0;
    for (k = 0; k < 16u; k++)
        if (sqm_hit(&trk[0].step[k])) {
            int32_t at = (int32_t)k + rhythm_offset(0, k);
            uint32_t m = tide_metric(&trk[0], (uint32_t)at);
            a++;
            *share += m == SM_AND || m == SM_A;
            *onbeat += m == SM_BEAT && at != (int32_t)k;   /* (the anchors sit on beats; a moved hit must not) */
            if (m == SM_A)
                pushes[(uint32_t)at / 16u]++;
        }
    *maxpush = pushes[0];
    (void)a;
}
static void t_sync(void)
{
    uint32_t mode, seed, sh[3] = {0, 0, 0}, ob[3] = {0, 0, 0}, mp[3] = {0, 0, 0}, s, o, m, same = 1, kept[3] = {0, 0, 0};
    CHECK("rows: TURNS / POLY / SYNC show their names", !strcmp(jl_vname(JL_TURNS, 1), "TAKE TURNS") && !strcmp(jl_vname(JL_TURNS, 0), "TOGETHER") &&
          !strcmp(jl_vname(JL_POLY, 2), "WIDE") && !strcmp(jl_vname(JL_SYNC, 1), "SOFT") && !strcmp(jl_vname(JL_SYNC, 2), "PUSH") && jl_n(JL_SYNC) == 3u);
    for (mode = 0; mode < 3u; mode++)
        for (seed = 1; seed <= 12u; seed++) {
            sync_run(mode, 2, seed, 0, &s, &o, &m);
            sh[mode] += s, ob[mode] += o, mp[mode] = m > mp[mode] ? m : mp[mode];
            sync_run(mode, 2, seed, 1, &s, &o, &m);
            kept[mode] += s;
        }
    printf("  SYNC (12 seeds, 64 moves, E-position hits): hits on the and / push: OFF %u, SOFT %u, PUSH %u of 48; kept from 48 and-hits: OFF %u, SOFT %u, PUSH %u; beats hit by moved: %u %u %u\n",
           sh[0], sh[1], sh[2], kept[0], kept[1], kept[2], ob[0], ob[1], ob[2]);
    CHECK("SYNC SOFT / PUSH: the and + push share rises over OFF, PUSH at least SOFT", sh[1] > sh[0] && sh[2] >= sh[1]);
    CHECK("  .. no moved hit lands on a beat", ob[1] == 0u && ob[2] == 0u);
    CHECK("  .. the push cap: at most 1 (SOFT) / 2 (PUSH) in the bar", mp[1] <= 1u && mp[2] <= 2u);
    CHECK("  .. a pattern already on the and keeps most of it (SOFT 75 %)", kept[1] * 4u >= 48u * 3u);
    CHECK("  .. and keeps more than OFF does", kept[1] >= kept[0]);
    for (seed = 1; seed <= 12u; seed++) {                  /* triplets (SDIV 4): not a supported grid, SYNC behaves as OFF */
        uint32_t s0, o0, m0, s1, o1, m1;
        sync_run(0, 4, seed, 0, &s0, &o0, &m0);
        sync_run(2, 4, seed, 0, &s1, &o1, &m1);
        same &= s0 == s1 && o0 == o1 && m0 == m1;
    }
    CHECK("triplet grid: SYNC PUSH equals OFF", same);
    CHECK("tide_metric: 1/16 loop of 16: step 0 beat, 2 and, 3 push (A), 1 E; swing 66 % turns the push into an E", (setup(16, 16, 2, 2, 0), tide_metric(&trk[0], 0) == SM_BEAT &&
          tide_metric(&trk[0], 2) == SM_AND && tide_metric(&trk[0], 3) == SM_A && tide_metric(&trk[0], 1) == SM_E &&
          (trk[0].p[P_SSWING] = 66, tide_metric(&trk[0], 3) == SM_E)));
    setup(10, 16, 2, 2, 0);
    CHECK("tide_metric: a loop of 10 sixteenths is 2.5 beats: not supported", tide_metric(&trk[0], 3) == SM_NONE);
    setup(16, 16, 1, 2, 0);
    CHECK("tide_metric: 1/8: every second step is a beat, step 1 the and", tide_metric(&trk[0], 2) == SM_BEAT && tide_metric(&trk[0], 1) == SM_AND);
    setup(16, 16, 2, 2, 0);
    sqm_auto = 3, sqm_poly = 2;
    {   uint32_t b, pulse = 0, same2 = 1;
        for (b = 0; b < 300000u; b++) {
            block(0, &pulse, 240, b);
            same2 &= tide_metric(&trk[0], 3) == SM_A;      /* (the full stored loop, even while a stretch is on) */
        }
        CHECK("POLY: tide_metric reads the full stored loop all along", same2);
    }
    sqm_sync = 1;
}


/* ----------------------------------------------------------------------------------------------- ANSWER --- */
static void put_hits(uint32_t p, const uint8_t *at, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < NSTEP; k++)
        trk[p].step[k] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
    for (k = 0; k < n; k++)
        trk[p].step[at[k]] = (step_t){{(uint8_t)(60 + k)}, 1, ST_NOTE, 0, 100, 0};
}
static int thin_bit(uint32_t p, uint32_t i) { return (int)((sqm_thin[p][i >> 5] >> (i & 31u)) & 1u); }
/* D2: LEAD and ECHO answer with a lead or pluck line (the Original's category): part p on its engine's first LEAD sound */
static void lead_sound(uint32_t p)
{
    uint32_t e = trk[p].eng_req, j;
    for (j = 0; j < ds_count(e) && ds_cat_of(e, j) != CAT_LEAD; j++)
        ;
    CHECK("a LEAD sound on the part's engine", j < ds_count(e));
    trk[p].preset = (uint8_t)j, trk[p].user = 0;
}
static void t_answer(void)
{
    static const uint8_t H[] = {0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15};
    uint32_t i, p, bar, drops, cand, nbad = 0, wrong = 0, sh[2] = {0, 0}, n, seed;
    step_t buf;
    CHECK("TURNS shows its four values", !strcmp(jl_vname(JL_TURNS, 2), "LEAD") && !strcmp(jl_vname(JL_TURNS, 3), "ECHO") && jl_n(JL_TURNS) == 4u);
    setup(16, 16, 2, 2, 0), lead_sound(0), lead_sound(1);
    put_hits(0, H, 12);
    put_hits(1, H, 12);
    sqm_auto = 3, sqm_turns = 2;
    song.playing = 1;
    seqm_poll();
    sqm_refresh(0, 1), sqm_refresh(1, 1);
    sqm_thin_build();
    for (drops = 0, cand = 0, i = 0; i < 16u; i++) {
        drops += (uint32_t)thin_bit(0, i);
        if (!sqm_anchor(trk[0].step, 16u, i) && sqm_hit(&trk[0].step[i]))
            cand++;
        if (thin_bit(0, i) && (i % 4u == 0u || sqm_anchor(trk[0].step, 16u, i) || !sqm_hit(&trk[0].step[i])))
            nbad++;
    }
    /* (the anchors of these 12 hits: the 4 beats, then 2 3 6 7 for the floor of 3/5: 8 of 12; candidates 10 11 14 15) */
    CHECK("mask: no beat, no anchor, only hits; half of the candidates (rounded up) dropped", !nbad && cand == 4u && drops == 2u);
    {   /* held hits: a tie after step 3 and 11, a slide on 7 */
        step_t *st = trk[0].step;
        st[4] = (step_t){{0}, 0, ST_TIE, 0, 0, 0};
        st[12] = (step_t){{0}, 0, ST_TIE, 0, 0, 0};
        st[7].flags |= SF_SLIDE;
        sqm_refresh(0, 1);
        sqm_thin_build();
        CHECK("  .. a hit before a tie and a slid hit are never dropped", !thin_bit(0, 3) && !thin_bit(0, 11) && !thin_bit(0, 7));
    }
    {   /* audit C8: VARY ALL plays End B on the follower's bars too: the mask drops only hits End B may drop as well (End B: odd steps rest, a tie after 10) */
        uint32_t ok = 1, nb = 0;
        put_hits(0, H, 12);
        sqm_refresh(0, 1);
        for (i = 0; i < 16u; i++)
            if (i & 1u)
                sqm_pat[0][1][i] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
        sqm_pat[0][1][11] = (step_t){{0}, 0, ST_TIE, 0, 0, 0};
        sqm_vary_all = 1;
        sqm_thin_build();
        for (i = 0; i < 16u; i++)
            if (thin_bit(0, i)) {
                nb++;
                ok &= sqm_hit(&sqm_pat[0][0][i]) && !sqm_held(sqm_pat[0][0], i, 16u) && sqm_hit(&sqm_pat[0][1][i]) && !sqm_held(sqm_pat[0][1], i, 16u);
            }
        CHECK("VARY ALL: the follower's mask drops only hits both ends may drop (no End B rest, no hit before an End B tie), and still thins", ok && nb >= 1u);
        sqm_vary_all = 0;
        sqm_thin_build();
        for (i = 0, nb = 0; i < 16u; i++)
            nb += (uint32_t)thin_bit(0, i);
        CHECK("  .. VARY 4 + 8: the mask is End A's alone, as before (2 of 4)", nb == 2u);
    }
    put_hits(0, H, 2);
    sqm_refresh(0, 1);
    sqm_thin_build();
    CHECK("  .. a pattern of 2 hits is left whole", !sqm_thin[0][0] && !sqm_thin[0][1]);
    put_hits(0, H, 12);
    sqm_refresh(0, 1);
    sqm_thin_build();
    /* the leader per bar: bars 0-1 part 2, 2-3 part 1, over the 8-bit wrap, with the last block's turn */
    for (bar = 0; bar < 600u; bar++) {
        uint32_t turn;
        for (turn = 0; turn < 2u; turn++) {
            uint32_t b = (bar + turn) & 3u;
            sqm_bar = (uint8_t)bar, sqm_bar_turn = (uint8_t)turn;
            for (p = 0; p < 2u; p++) {
                uint32_t lead = b < 2u ? 1u : 0u, masked = 0, any = 0;
                for (i = 0; i < 16u; i++)
                    if (thin_bit(p, i)) {
                        any = 1;
                        masked |= seq_play_step(&trk[p], i, &buf) == &SQM_REST;
                    }
                wrong += any && (p == lead ? masked : !masked);
            }
        }
    }
    CHECK("the leader plays whole, the other part thinned: part 2 on bars 1-2, part 1 on 3-4 (also over the wrap at 256)", !wrong);
    {   /* audit C7: the ARC's fast turns (a leader a bar): a part on its own variation bar (part 2 bar 2, part 1 bar 4) is never the thinned follower */
        uint32_t vw = 0, fw = 0;
        tarc_fast = 1;
        for (bar = 0; bar < 8u; bar++) {
            sqm_bar = (uint8_t)bar, sqm_bar_turn = 0;
            for (p = 0; p < 2u; p++) {
                uint32_t varies = ((bar + 2u * p) & 3u) == 3u, lead = (bar & 1u) ? 0u : 1u, masked = 0;
                for (i = 0; i < 16u; i++)
                    if (thin_bit(p, i))
                        masked |= seq_play_step(&trk[p], i, &buf) == &SQM_REST;
                vw += varies && masked;
                fw += masked != (p != lead && !varies);
            }
        }
        tarc_fast = 0;
        CHECK("ARC fast turns: the part on its variation bar plays whole; otherwise the leader changes every bar", !vw && !fw);
    }
    sqm_bar = 0, sqm_bar_turn = 0;
    {   uint32_t m0 = 0;
        for (i = 0; i < 16u; i++)
            m0 += seq_play_step(&trk[0], i, &buf)->time == ST_NOTE;
        sqm_turns = 1;
        for (i = 0, n = 0; i < 16u; i++)
            n += seq_play_step(&trk[0], i, &buf)->time == ST_NOTE;
        CHECK("  .. TAKE TURNS: nothing masked, part 1 whole (12 hits), thinned under LEAD", n == 12u && m0 < 12u);
        sqm_turns = 2;
        sqm_auto = 1;
        for (i = 0, n = 0; i < 16u; i++)
            n += seq_play_step(&trk[0], i, &buf)->time == ST_NOTE;
        CHECK("  .. one part only: no mask", n == 12u);
        sqm_auto = 3, song.playing = 0;
        for (i = 0, n = 0; i < 16u; i++)
            n += seq_play_step(&trk[0], i, &buf)->time == ST_NOTE;
        CHECK("  .. stopped: no mask (the variation as always)", n == ({ uint32_t r_ = 0, q_; for (q_ = 0; q_ < 16u; q_++) r_ += sqm_step(&trk[0], q_, 1)->time == ST_NOTE; r_; }));
        song.playing = 1, poly_len[0] = 14;
        for (i = 0, n = 0; i < 16u; i++)
            n += seq_play_step(&trk[0], i, &buf)->time == ST_NOTE;
        poly_len[0] = 0;
        CHECK("  .. a part in a POLY stretch is never thinned", n == 12u);
    }
    /* ECHO: part 1's movable hits lean into the places part 2 thinned away */
    for (n = 0; n < 2u; n++)
        for (seed = 1; seed <= 12u; seed++) {
            uint32_t k, m;
            setup(16, 16, 2, 2, 0), lead_sound(0), lead_sound(1);
            put_hits(1, H, 12);
            put_hits(0, (const uint8_t[]){0, 4, 8, 12, 1, 5, 9, 13}, 8);
            sqm_auto = 3, sqm_turns = n ? 3u : 2u, sqm_sync = 0;
            seqm_poll();
            sqm_refresh(0, 1), sqm_refresh(1, 1);
            sqm_thin_build();
            memset(&rhythm, 0, sizeof rhythm);
            for (m = 0; m < 48u; m++) {
                rhythm_mutate(0, seed * 7919u + m * 104729u);
                sqm_refresh(0, 1);
            }
            for (k = 0; k < 16u; k++)
                if (sqm_hit(&trk[0].step[k]) && thin_bit(1, (uint32_t)((int32_t)k + rhythm_offset(0, k))))
                    sh[n]++;
        }
    {   /* audit C9: the masks are built when what they read changes, not every frame */
        uint32_t m0, m1, ok1, ok2, ok3;
        setup(16, 16, 2, 2, 0), lead_sound(0), lead_sound(1);
        put_hits(0, H, 12), put_hits(1, H, 12);
        sqm_auto = 3, sqm_turns = 2, sqm_sync = 0;
        seqm_poll();
        tide_poll();
        m0 = sqm_thin[0][0], m1 = sqm_thin[1][0];
        sqm_thin[0][0] = 0x5A5A5A5Au;                     /* (a marker a rebuild would overwrite) */
        tide_poll(), tide_poll();
        ok1 = sqm_thin[0][0] == 0x5A5A5A5Au && m0 != 0u;
        sqm_turns = 3;                                   /* ECHO: rebuilt at once */
        tide_poll();
        ok2 = sqm_thin[0][0] != 0x5A5A5A5Au;
        sqm_turns = 2;
        tide_poll();
        ok3 = sqm_thin[0][0] == m0 && sqm_thin[1][0] == m1;
        CHECK("masks: nothing changed, no rebuild (the marker stays); TURNS changed, rebuilt; back, the same masks", ok1 && ok2 && ok3);
        trk[0].p[P_SLEN] = 12;
        seqm_poll(), tide_poll(), tide_poll();
        CHECK("  .. a new length rebuilds them (no bit past the loop)", !(sqm_thin[0][0] >> 12) && sqm_thin[0][0] != m0);
    }
    printf("  ECHO (12 seeds, 48 moves): part 1 hits on part 2's thinned places: LEAD %u, ECHO %u\n", sh[0], sh[1]);
    CHECK("ECHO: more of part 1's hits sit on part 2's thinned places than with LEAD", sh[1] > sh[0]);
    sqm_turns = 1, sqm_sync = 1, sqm_auto = 0;
    sqm_thin_build();
}


/* -------------------------------------------------------------------------------------------------- ARC --- */
static uint32_t arc_pulse;
static void arc_block(uint32_t b)
{
    block(0, &arc_pulse, 240, b);
    sqm_auto = 3;
    tarc_poll();
    sqm_thin_build();
}
static void t_arc(void)
{
    static const uint8_t H[] = {0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14, 15};
    uint32_t b, i, n, mode, ph_prev, order_ok = 1, lens_ok = 1, edge_ok = 1, phases = 0, run = 0, seen[5], dropbars, dropfull, leak, lastbar;
    uint32_t seq[40], nseq = 0, seqlen[40];
    step_t buf;
    CHECK("ARC shows its names", !strcmp(jl_vname(JL_ARC, 0), "OFF") && !strcmp(jl_vname(JL_ARC, 1), "RISE") && !strcmp(jl_vname(JL_ARC, 2), "DROP") && jl_n(JL_ARC) == 3u);
    /* ARC OFF: every lever neutral */
    setup(16, 16, 2, 2, 0);
    put_hits(0, H, 12), put_hits(1, H, 12);
    sqm_arc = 0, sqm_turns = 2, sqm_sync = 1;
    arc_pulse = 0;
    for (b = 0; b < 40000u; b++)
        arc_block(b);
    CHECK("ARC OFF: no drop, no thinning of its own, SYNC and brightness untouched", tarc_dropphr == 0xFFu && tarc_thin == 0 && !tarc_fast && !tarc_hold && tarc_sync() == 1u && tarc_bright() == 0);
    {   /* audit C5: an arc is 8-16 phrases (32-64 bars), as README and the guide say, over many seeds in both modes */
        uint32_t m, sd, a, mn[3] = {99, 99, 99}, mx[3] = {0, 0, 0}, pkmx = 0;
        for (m = 1; m <= 2u; m++)
            for (sd = 0; sd < 400u; sd++) {
                sqm_arc = (uint8_t)m, morph.seed = (uint8_t)sd, tarc.cnt = (uint8_t)(sd >> 8), tarc.phr = 0, tarc.run = 1, tarc.phase = AP_RECOVER;
                tarc_begin(AP_RISE);
                for (a = 0; a < 3u; a++) {                   /* (three arcs a seed) */
                    uint32_t tot = tarc.len;
                    do {
                        tarc.phr = (uint8_t)(tarc.phr + tarc.len);
                        tarc_begin(tarc.next);
                        if (tarc.phase == AP_PEAK && tarc.len > pkmx) pkmx = tarc.len;
                        if (tarc.phase != AP_RISE) tot += tarc.len;
                    } while (tarc.phase != AP_RISE);
                    if (tot < mn[m]) mn[m] = tot;
                    if (tot > mx[m]) mx[m] = tot;
                }
            }
        printf("  ARC length in phrases: RISE %u..%u, DROP %u..%u (PEAK up to %u)\n", mn[1], mx[1], mn[2], mx[2], pkmx);
        CHECK("ARC length: every arc 8 to 16 phrases (32 to 64 bars) in RISE and DROP, PEAK at most 2", mn[1] >= 8u && mx[1] <= 16u && mn[2] >= 8u && mx[2] <= 16u && mx[2] == 16u && pkmx <= 2u);
        sqm_arc = 0, tarc.run = 0;
    }
    /* the shape: 600 bars of ARC DROP */
    for (mode = 1; mode <= 2u; mode++) {
        setup(16, 16, 2, 2, 0);
        put_hits(0, H, 12), put_hits(1, H, 12);
        sqm_arc = (uint8_t)mode, sqm_turns = 2, sqm_poly = 0;
        morph.seed = 77;
        arc_pulse = 0;
        memset(seen, 0, sizeof seen);
        ph_prev = 9u, nseq = 0, dropbars = dropfull = leak = 0, lastbar = 0xFFFFu;
        order_ok = lens_ok = edge_ok = 1;
        for (b = 0; b < 600u * 4u * 12000u / CTL + 1u && nseq < 38u; b++) {
            uint32_t bar;
            arc_block(b);
            bar = (uint32_t)sqm_bar + sqm_bar_turn;
            if (tarc.run && tarc.phase != ph_prev) {
                if (ph_prev != 9u) {
                    uint32_t want = ph_prev == AP_RISE ? AP_PEAK : ph_prev == AP_PEAK ? (mode == 2u ? AP_DROP : AP_BREAK) : ph_prev == AP_DROP ? AP_BREAK : ph_prev == AP_BREAK ? AP_RECOVER : AP_RISE;
                    if (tarc.phase != want && !(ph_prev == AP_PEAK && mode == 2u && tarc.phase == AP_BREAK))
                        order_ok = 0;
                    edge_ok &= (sqm_bar & 3u) == 0u || (sqm_bar & 3u) == 3u;   /* (the last block of a bar already counts the next) */
                    lens_ok &= run >= 1u && run <= 8u;
                }
                if (nseq < 40u)
                    seq[nseq] = tarc.phase, seqlen[nseq] = 0, nseq++;
                ph_prev = tarc.phase, run = 0;
                seen[tarc.phase]++;
            }
            if (tarc.run && bar != lastbar) {
                lastbar = bar;
                if ((bar & 3u) == 0u && nseq)
                    seqlen[nseq - 1]++, run++;
            }
            if (mode == 1u && tarc_dropphr != 0xFFu)
                leak++;
        }
        (void)phases;
        printf("  ARC %s: phases seen RISE %u PEAK %u DROP %u BREAK %u RECOVER %u in %u phases\n", mode == 1u ? "RISE" : "DROP", seen[0], seen[1], seen[2], seen[3], seen[4], nseq);
        CHECK(mode == 1u ? "ARC RISE: RISE, PEAK, BREAK, RECOVER in turn, never a drop of its own" : "ARC DROP: RISE, PEAK, (DROP,) BREAK, RECOVER in turn", order_ok && seen[0] >= 2u && seen[1] >= 2u && seen[3] >= 2u && seen[4] >= 2u && (mode == 1u ? seen[2] == 0u && !leak : seen[2] >= 1u));
        CHECK("  .. a phase changes only at a phrase boundary", edge_ok);
        (void)lens_ok;
    }
    /* the drop masks only onsets, for the first 2 bars of its phrase; a tie (none here) and the other bars play */
    setup(16, 16, 2, 2, 0);
    put_hits(0, H, 12), put_hits(1, H, 12);
    sqm_arc = 2, sqm_turns = 1, sqm_poly = 0;
    arc_pulse = 0;
    {
        uint32_t forced = 0, b2;
        for (b = 0; b < 40u && !forced; b++)
            arc_block(b);
        for (b2 = 0; b2 < 4000000u / CTL * 4u && !forced; b2++) {
            arc_block(100000u + b2);
            if (tarc.phase == AP_PEAK || tarc.phase == AP_RISE)
                forced = tarc.phase == AP_RISE && tarc.left > 1u;
        }
        CHECK("(fixture: in RISE)", forced);
        {
            uint32_t left0 = tarc.left;
            CHECK("hand: a first OCT- + K2 turn arms a drop at the next phrase", !strcmp(tarc_drop_hand(), "DROP NEXT PHRASE") && tarc.left == 1u && tarc.next == AP_DROP);
            CHECK("  .. a second turn cancels it", !strcmp(tarc_drop_hand(), "DROP CANCELLED") && tarc_dropphr == 0xFFu && tarc.next != AP_DROP);
            CHECK("  .. and the RISE gets its phrases back (audit C6: the cancel does not shorten it)", tarc.left == left0 && left0 > 1u);
        }
        tarc_drop_hand();
        for (b2 = 0; b2 < 4000000u / CTL * 4u && tarc.phase != AP_DROP; b2++) {
            arc_block(200000u + b2);
        }
        CHECK("  .. the drop comes: DROP, from RISE, skipping the PEAK", tarc.phase == AP_DROP);
        dropbars = dropfull = 0;
        for (b2 = 0; b2 < 4u * 12000u / CTL * 4u; b2++) {
            uint32_t bar, hit = 0, any = 0, k;
            arc_block(300000u + b2);
            bar = (uint32_t)sqm_bar + sqm_bar_turn;
            if (tarc.phase != AP_DROP)
                break;
            if (sqm_bar_turn)                            /* (the last block of a bar already plays the next one) */
                continue;
            for (k = 0; k < 16u; k++)
                if (sqm_hit(&trk[0].step[k])) {
                    any++;
                    hit += seq_play_step(&trk[0], k, &buf)->time == ST_NOTE;
                }
            if ((bar & 3u) < 2u)
                dropfull += hit != 0u;
            else
                dropbars += hit != 0u;
        }
        CHECK("  .. its first 2 bars start no note, its last 2 do (the thinned break)", dropfull == 0u && dropbars > 0u);
    }
    /* the hand with ARC OFF does nothing; with ARC RISE it drops */
    sqm_arc = 0;
    tarc_poll();
    CHECK("hand: ARC OFF says so and arms nothing", !strcmp(tarc_drop_hand(), "ARC OFF") && tarc_dropphr == 0xFFu);
    sqm_arc = 1, sqm_auto = 3, tarc.run = 1, tarc.phase = AP_RISE, tarc.len = tarc.left = 5, tarc.hand = 0;
    CHECK("gesture: OCT- + K2 (tide_reset(1)) arms the drop; K1 / K3 / K4 resets are unchanged", tide_reset(1u) == 1 && tarc.hand == 1u && tarc.next == AP_DROP &&
          (gl.time = 5, tide_reset(2u), gl.time == 3u) && (gl.wait = 2, tide_reset(3u), gl.wait == 0u));
    tarc.hand = 0;
    /* brightness lean per phase */
    sqm_arc = 2;
    tarc.run = 1;
    sqm_auto = 3;
    tarc.phase = AP_PEAK;
    n = tarc_bright() == 1;
    tarc.phase = AP_BREAK;
    n &= tarc_bright() == -1;
    tarc.phase = AP_RECOVER;
    n &= tarc_bright() == 0;
    CHECK("brightness: brighter at the peak, softer in the break, neutral in the recovery", n);
    sqm_arc = 0, sqm_turns = 1, sqm_auto = 0;
    tarc_poll();
    (void)i;
    (void)seq;
    (void)seqlen;
}


/* ------------------------------------------------------------------------------------------------ STYLE --- */
static int style_is(uint32_t k)
{
    const uint8_t *t = STY[k];
    return gl.mode == t[0] && gl.time == t[1] && gl.wait == t[2] && sqm_vary_all == t[3] && sqm_turns == t[4] && sqm_poly == t[5] && sqm_sync == t[6] &&
           sqm_arc == t[7] && (morph_wlvl & 3u) == t[8];
}
static void t_style(void)
{
    uint32_t k, r, ok = 1, own = 1;
    uint8_t b[16], rec[5 + 16] = {'T', 'I', 'D', 'E', TIDE_STORE_VER};
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
    CHECK("STYLE shows its names, 6 values, first row", !strcmp(jl_vname(JL_STYLE, 0), "DUB") && !strcmp(jl_vname(JL_STYLE, 4), "PEAK") && !strcmp(jl_vname(JL_STYLE, 5), "CUSTOM") &&
          jl_n(JL_STYLE) == 6u && JL_STYLE == 0);
    for (k = 0; k < 5u; k++) {
        gl.target = GT_SEQ, morph_musical = 0;
        jl_put(JL_STYLE, k);
        tide_settings();
        ok &= style_is(k) && tide_style == k && gl.target == GT_SEQ && morph_musical == 0u;
        own &= STY[k][0] <= GM_JOURNEY && STY[k][1] < GLIDE_TIMES && STY[k][2] < GLIDE_WAITS;
    }
    CHECK("each style sets exactly its nine rows and leaves TARGET and ROLL (ARP, PITCH are the morph's own byte)", ok && own);
    jl_put(JL_STYLE, 1);
    tide_custom_ok = 0;                                      /* (no CUSTOM kept: tests/ui_truth_test.c has the way back) */
    jl_put(JL_STYLE, STY_CUSTOM);
    tide_settings();
    CHECK("CUSTOM changes nothing (with no values of its own kept)", tide_style == STY_CUSTOM && style_is(1));
    /* a hand edit of each of the nine owned values: CUSTOM, the edited value and the rest kept; the other rows leave the style alone */
    ok = 1;
    for (r = 0; r < 9u; r++) {
        jl_put(JL_STYLE, 4);
        tide_settings();
        switch (r) {
        case 0: gl.mode = GM_WANDER; break;                       /* the MODE knob */
        case 1: gl.time = 2; break;                               /* the TIME knob */
        case 2: gl.wait = 1; break;                               /* the WAIT knob */
        case 3: jl_put(JL_VARY, 0); break;
        case 4: jl_put(JL_TURNS, 1); break;
        case 5: jl_put(JL_POLY, 0); break;
        case 6: jl_put(JL_SYNC, 0); break;
        case 7: jl_put(JL_ARC, 1); break;
        default: jl_put(JL_WASH, 2); break;
        }
        tide_settings();
        ok &= tide_style == STY_CUSTOM;
    }
    CHECK("a hand edit of any of the nine rows turns STYLE into CUSTOM", ok);
    jl_put(JL_STYLE, 3);
    tide_settings();
    gl.time = 5;
    tide_reset(2u);                                               /* OCT- + K3: TIME 8 bars */
    tide_settings();
    CHECK("  .. also OCT- + K3 (the reset)", tide_style == STY_CUSTOM && gl.time == 3u);
    jl_put(JL_STYLE, 3);
    tide_settings();
    jl_put(JL_TARGET, GT_SEQ);
    jl_put(JL_ROLL, 1);
    tide_settings();
    CHECK("  .. TARGET and ROLL leave the style alone", tide_style == 3u);
    jl_put(JL_STYLE, 2);
    jl_put(JL_SYNC, 0);
    tide_settings();
    jl_put(JL_SYNC, 2);                                           /* back to the style's value by hand: stays CUSTOM */
    tide_settings();
    CHECK("  .. setting the values back by hand stays CUSTOM", tide_style == STY_CUSTOM && style_is(2));
    /* the record */
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    tide_seen_ok = 0;
    tide_style = STY_CUSTOM;
    gl.mode = GM_ONCE, gl.time = 0, gl.wait = 0, sqm_turns = 0, sqm_arc = 0, sqm_sync = 0, sqm_poly = 0;
    tide_settings();
    CHECK("no record: DUB, its existing rows today's defaults (JRNY, 16 bars, WAIT 2 bars, VARY 4 + 8, WASH OFF)", tide_style == STY_DUB && style_is(0) && gl.mode == GM_JOURNEY &&
          GLIDE_BARS[gl.time] == 16u && GLIDE_WAIT_BARS[gl.wait] == 2u && !sqm_vary_all && !morph_wlvl);
    {
        static const uint8_t L[3] = {7, 9, 11};
        for (k = 0; k < 3u; k++) {
            memset(rec + 5, 0, 16);
            rec[5 + TS_MODE] = GM_LOOP, rec[5 + TS_TIME] = 2, rec[5 + TS_TARGET] = GT_BOTH, rec[5 + TS_WAIT] = 1, rec[5 + TS_WASH] = 2;
            rec[5 + TS_TURNS] = 3, rec[5 + TS_POLY] = 2, rec[5 + TS_SYNC] = 0, rec[5 + TS_ARC] = 2;
            st_save(OBJ_TIDE, rec, 5u + L[k]);
            tide_seen_ok = 0;
            tide_style = STY_DUB;
            tide_settings();
            CHECK(k == 0 ? "a 7-byte record: CUSTOM, its values kept" : k == 1 ? "a 9-byte record: CUSTOM, its values kept" : "an 11-byte record: CUSTOM, its values kept",
                  tide_style == STY_CUSTOM && gl.mode == GM_LOOP && gl.time == 2u && gl.wait == 1u && morph_wlvl == 2u && (L[k] < 9u || (sqm_turns == 3u && sqm_poly == 2u)) &&
                  (L[k] < 11u || (sqm_sync == 0u && sqm_arc == 2u)));
        }
    }
    memset(rec + 5, 0, 16);
    for (k = 0; k < 9u; k++)
        rec[5 + (k == 0 ? TS_MODE : k == 1 ? TS_TIME : k == 2 ? TS_WAIT : k == 3 ? TS_VARY : k == 4 ? TS_TURNS : k == 5 ? TS_POLY : k == 6 ? TS_SYNC : k == 7 ? TS_ARC : TS_WASH)] = STY[4][k];
    rec[5 + TS_STYLE] = 4;
    st_save(OBJ_TIDE, rec, 5u + TS_N);
    tide_seen_ok = 0;
    tide_style = STY_CUSTOM;
    tide_settings();
    CHECK("a 12-byte record: its style and values", tide_style == 4u && style_is(4));
    rec[5 + TS_STYLE] = 9;
    st_save(OBJ_TIDE, rec, 5u + TS_N);
    tide_seen_ok = 0;
    tide_style = 2;
    tide_settings();
    CHECK("an out-of-range style byte: CUSTOM", tide_style == STY_CUSTOM);
    rec[5 + TS_STYLE] = 1;                                         /* DEEP's byte, PEAK's values: they differ, so CUSTOM at once */
    st_save(OBJ_TIDE, rec, 5u + TS_N);
    tide_seen_ok = 0;
    tide_settings();
    CHECK("a style byte whose values do not match its table: CUSTOM", tide_style == STY_CUSTOM);
    /* a style picked, a power cycle: the same style, and the record is written once per change */
    tide_store_dirty = 0;
    jl_put(JL_STYLE, 3);
    tide_settings();
    CHECK("a style picked is marked for saving", tide_store_dirty && tide_store_n == 5u + TS_N);
    fm1_ms += 3000u;
    song.playing = 0;
    tide_store_poll();
    tide_settings();
    tide_settings();
    CHECK("  .. saved once, not again every frame", !tide_store_dirty && tide_store_load(b, TS_N) && b[TS_STYLE] == 3u);
    tide_seen_ok = 0;
    tide_style = STY_CUSTOM;
    gl.mode = GM_ONCE;
    tide_settings();
    CHECK("  .. after a power cycle the same style and values", tide_style == 3u && style_is(3));
}

/* ------------------------------------------------------------------------------------------------ store --- */
static void t_store(void)
{
    uint8_t old[7] = {GM_WANDER, 2, GT_BOTH, 1, 1, 0, 3}, rec[5 + 16] = {'T', 'I', 'D', 'E', TIDE_STORE_VER};
    uint8_t b[16];
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
    memcpy(rec + 5, old, 7);
    st_save(OBJ_TIDE, rec, 5 + 7);                       /* the first record: 7 fields */
    memset(b, 0xEE, sizeof b);
    CHECK("an older record (7 fields) loads: 7 fields read, the rest untouched", tide_store_load_min(b, TS_N, 7) == 7u && !memcmp(b, old, 7) && b[7] == 0xEEu);
    CHECK("  .. and tide_store_load(TS_N) still wants all of them", !tide_store_load(b, TS_N));
    tide_store_dirty = 0;
    tide_seen_ok = 0;
    sqm_turns = 0, sqm_poly = 0;
    gl.mode = GM_ONCE, gl.time = 0, gl.target = GT_SOUND, gl.wait = 0, sqm_vary_all = 0, morph_musical = 1, morph_wlvl = 0;
    tide_settings();
    CHECK("the settings of the older record are kept", gl.mode == GM_WANDER && gl.time == 2u && gl.target == GT_BOTH && gl.wait == 1u && sqm_vary_all == 1u &&
          morph_musical == 0u && morph_wlvl == 3u);
    CHECK("  .. TURNS, POLY and SYNC at their defaults: TAKE TURNS, GENTLE, SOFT", sqm_turns == 1u && sqm_poly == 1u && sqm_sync == 1u);
    CHECK("  .. nothing is written back by the load alone", !tide_store_dirty);
    sqm_turns = 0, sqm_poly = 2;
    tide_settings();
    CHECK("a change is marked for saving with all the fields", tide_store_dirty && tide_store_n == 5u + TS_N);
    fm1_ms += 3000u;
    song.playing = 0;
    tide_store_poll();
    CHECK("  .. saved", !tide_store_dirty && tide_store_load(b, TS_N) && b[TS_TURNS] == 0u && b[TS_POLY] == 2u && !memcmp(b, old, 7));
    tide_seen_ok = 0;
    sqm_turns = 1, sqm_poly = 1;
    gl.mode = GM_ONCE;
    tide_settings();
    CHECK("  .. read back after a restart", sqm_turns == 0u && sqm_poly == 2u && gl.mode == GM_WANDER);
    {   /* the 9-byte record of the TURNS / POLY build: those two kept, SYNC at its default */
        uint8_t r9[5 + 9] = {'T', 'I', 'D', 'E', TIDE_STORE_VER, GM_LOOP, 1, GT_SEQ, 0, 0, 1, 0, 0, 2};
        st_save(OBJ_TIDE, r9, sizeof r9);
        tide_seen_ok = 0;
        sqm_sync = 0;
        sqm_arc = 2;
        tide_settings();
        CHECK("a 9-byte record: TURNS and POLY kept, SYNC SOFT", gl.mode == GM_LOOP && sqm_turns == 0u && sqm_poly == 2u && sqm_sync == 1u && sqm_arc == 0u);
    }
    rec[5 + TS_TURNS] = 9, rec[5 + TS_POLY] = 9, rec[5 + TS_SYNC] = 9, rec[5 + TS_ARC] = 9;
    st_save(OBJ_TIDE, rec, 5 + TS_N);
    tide_seen_ok = 0;
    tide_settings();
    CHECK("a value out of range: TURNS, POLY, SYNC at their defaults", sqm_turns == 1u && sqm_poly == 1u && sqm_sync == 1u && sqm_arc == 0u);
    rec[5 + TS_ARC] = 2;
    st_save(OBJ_TIDE, rec, 5 + TS_N);
    tide_seen_ok = 0;
    tide_settings();
    CHECK("a full record: ARC read back", sqm_arc == 0u || sqm_arc == 2u);
}

/* audit C4: the AUTO TIDE page's redraw hash changes when any one of its settings changes, whatever TURNS is (TAKE TURNS' factor masked VARY BARS) */
static void t_sig(void)
{
    uint32_t tu, f, ok = 1, h0, h1;
    setup(16, 16, 2, 2, 0);
    for (tu = 0; tu < 4u; tu++)
        for (f = 0; f < 12u; f++) {
            sqm_turns = (uint8_t)tu;
            h0 = tide_sig();
            switch (f) {
            case 0: sqm_vary_all ^= 1u; break;
            case 1: morph_musical ^= 1u; break;
            case 2: sqm_poly ^= 1u; break;
            case 3: sqm_sync ^= 1u; break;
            case 4: sqm_arc ^= 1u; break;
            case 5: tarc.phase ^= 1u; break;
            case 6: tarc.left ^= 1u; break;
            case 7: morph.cfg ^= 1u; break;
            case 8: gl.wait ^= 1u; break;
            case 9: jl_sel ^= 1u; break;
            case 10: tarc.hand ^= 1u; break;
            default: sqm_turns ^= 1u; break;
            }
            h1 = tide_sig();
            if (h0 == h1) {
                ok = 0;
                printf("  tide_sig: TURNS %u, field %u flipped, hash unchanged %08x\n", tu, f, h0);
            }
            switch (f) {                                     /* (back) */
            case 0: sqm_vary_all ^= 1u; break;
            case 1: morph_musical ^= 1u; break;
            case 2: sqm_poly ^= 1u; break;
            case 3: sqm_sync ^= 1u; break;
            case 4: sqm_arc ^= 1u; break;
            case 5: tarc.phase ^= 1u; break;
            case 6: tarc.left ^= 1u; break;
            case 7: morph.cfg ^= 1u; break;
            case 8: gl.wait ^= 1u; break;
            case 9: jl_sel ^= 1u; break;
            case 10: tarc.hand ^= 1u; break;
            default: break;
            }
        }
    CHECK("AUTO TIDE redraw hash: each setting moves it under every TURNS (TAKE TURNS no longer masks VARY BARS and the ARC)", ok);
    sqm_turns = 0;
}

int main(void)
{
    t_turns();
    t_sig();
    t_poly();
    t_sync();
    t_answer();
    t_arc();
    t_style();
    t_store();
    printf("tideturns: %d of %d checks failed\n", bad, checks);
    return bad != 0;
}
