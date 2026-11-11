/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): GLIDE. A launched fader travels by itself from where it is to an end (A or B) over a number of bars, along an
 * S-curve. Generic lanes: a lane is a 0..127 value of a part set with a getter and a setter; the MORPH fader and the SEQ
 * MORPH fader are the two lanes today.
 *
 * Time: bars of the musical clock only (the internal tempo or an external MIDI clock: the beats the sequencer counts,
 * perf_beat, plus the position inside the beat): a glide moves while the transport plays, never on its own (owner, 2026-11-01).
 * Launched while playing it starts on the next downbeat; a panel launch while stopped starts PLAY and the trip with its first
 * bar (an external clock, a REC armed or a MIDI launch: it waits for PLAY). STOP pauses it where it is, to the sample of the
 * beat; PLAY goes on from there.
 * MODE, at each arrival (the end left is silent there: its weight is 0), for each lane TARGET moves:
 *   ONCE     stops.
 *   LOOP     turns back: the same two ends (the SEQ comes home each trip).
 *   DRIFT    the end left is rolled again from the Original: the sound and the pattern vary around themselves.
 *   WANDER   the end left walks on from the end reached (morph_music.c mm_walk; seq_morph.c seqm_wander_gen): always computed
 *            from the Original, within AMOUNT / SEQ AMT, nothing pulls it home.
 *   JOURNEY  the end left becomes the next target (tide_ui.c tide_journey): a sound of the same engine and kind, never the
 *            other part's; the pattern a WANDER generation, each part its own.
 * WAIT holds each arrival that many bars before the next trip. WANDER and JOURNEY follow the phrase grid: the bars run (gl.wbars)
 * give each generation the tier of the largest boundary crossed since the last (morph_music.c mm_tier: 16, 32, 64, 128 bars).
 * A hand on a fader (its knob, a MIDI CC) takes that part's lane over; 8 untouched bars later, on the next 4-bar phrase (sqm_bar), the
 * lane picks it up again from where the hand left it (MR7, owner 2026-11-09). Only the touched part waits; the lane, the pattern's own
 * evolution (sqm_auto) and ARC go on. A sound load (mm_loads) still stops the lane for good, as STOP, HOME and K1 do.
 * Context: main loop only (glide_poll from ui_input); it writes through the morphs' own actions, nothing in the
 * interrupt changes. */

enum { GT_SOUND, GT_SEQ, GT_BOTH };                      /* TARGET */
enum { GM_ONCE, GM_LOOP, GM_DRIFT, GM_WANDER, GM_JOURNEY };   /* MODE (JOURNEY: each arrival, the end left the next target, tide_ui.c tide_journey) */
#define GLIDE_WAITS 6u
static const uint8_t GLIDE_WAIT_BARS[GLIDE_WAITS] = {0, 1, 2, 4, 8, 16};
#define GLIDE_TIMES 7u
static const uint8_t GLIDE_BARS[GLIDE_TIMES] = {1, 2, 4, 8, 16, 32, 64};
#define GLIDE_LANES 2u
#define GLIDE_Q 16                                       /* progress in Q16 */

typedef struct {
    uint32_t (*get)(uint32_t pi);
    void (*set)(uint32_t pi, uint32_t mask, uint32_t v);
    void (*roll)(uint32_t pi, uint32_t mask, uint32_t end);   /* a new version of end (0 A, 1 B), heard nowhere at this fader */
} glide_lane_t;

static void gl_sound_set(uint32_t pi, uint32_t mask, uint32_t v) { (void)pi; morph_do(MQ_FADER, mask, v); }
static uint8_t gl_wander;                               /* the rolls of this arrival WANDER (glide_arrive) */
static void gl_sound_roll(uint32_t pi, uint32_t mask, uint32_t end)
{
    (void)pi;
    mm_walk = gl_wander;                                 /* (WANDER: each part's new end walks on from the end it is at) */
    morph_do(MQ_ROLL, mask, end);
    mm_walk = 0;
}
static void gl_seq_set(uint32_t pi, uint32_t mask, uint32_t v)
{
    uint32_t k;
    (void)pi;
    for (k = 0; k < NSHOWN; k++)
        if ((mask >> k) & 1u)
            seqm_set_fader(k, v);
}
static void gl_seq_roll(uint32_t pi, uint32_t mask, uint32_t end)
{
    uint32_t k;
    (void)pi;
    for (k = 0; k < NSHOWN; k++)
        if ((mask >> k) & 1u) {
            if (gl_wander)
                seqm_wander_gen(k, end, (uint32_t)mm_tier);
            else if (end == 1u)                          /* (DRIFT: End A is the Original pattern) */
                seqm_roll(k);
        }
}
static const glide_lane_t GLIDE_LANE[GLIDE_LANES] = {
    {morph_fader_of, gl_sound_set, gl_sound_roll},
    {seqm_fader_of, gl_seq_set, gl_seq_roll},
};

static struct {
    uint8_t target, mode, time;                          /* the settings: GT_*, GM_*, an index of GLIDE_BARS */
    uint8_t on;                                          /* lanes running, a bit each */
    uint8_t armed;                                       /* waiting for the next downbeat */
    uint8_t to;                                          /* the end it goes to: 0 A, 1 B */
    uint8_t pi;                                          /* the part whose faders it reads; mask: the parts it moves */
    uint8_t mask;
    uint8_t playing;                                     /* the transport as the last pass saw it (a STOP pauses, a PLAY goes on) */
    uint8_t beat_seen;
    int16_t from[GLIDE_LANES][NSHOWN];                   /* each part's own fader at the trip's start (after a hand: the start that puts the path through it, MR7) */
    uint8_t last[GLIDE_LANES][NSHOWN];                   /* each moved part's fader as the last pass left it */
    uint8_t loads[NSHOWN];                               /* each moved part's sound loads then (mm_loads: a load at the fader's own value) */
    uint32_t beats;                                      /* whole beats since the start, or since PLAY went on after a STOP */
    uint32_t bq, run;                                    /* Q16 beats: run before the last STOP; run in this stretch (as last seen) */
    uint32_t prog;                                       /* progress 0..1 << GLIDE_Q, for the page */
    uint8_t wait;                                        /* WAIT: an index of GLIDE_WAIT_BARS */
    uint8_t hw[GLIDE_LANES], bar_seen;                   /* MR7: the parts a hand holds on each lane; sqm_bar as last counted (in the padding: RAM is tight) */
    uint16_t wbars, wlast;                               /* WANDER, JOURNEY: the bars run since its start, at the last arrival (16 bits: 36 hours at 120 BPM; MR7 needed the RAM) */
    uint8_t hold;                                        /* holding an arrival WAIT bars (the faders still) */
    uint8_t tt;                                          /* audit C3: the TIME the running trip took at its start (TIME lands at the next turn) */
    uint8_t hq[NSHOWN];                                  /* MR7: each held part's untouched bars (in the padding: RAM is tight) */
} gl = {.target = GT_BOTH, .mode = GM_WANDER, .time = 4u, .wait = 2u};   /* the defaults (no TIDE record yet): the showcase, BOTH, JRNY, 16 bars,
                                                                            * WAIT 2 bars (owner, 2026-11-01) */

static void glide_shape_follow(void)
{
    for (uint32_t k = 0; k < NSHOWN; k++) {
        uint32_t span = 0;
        if (gl.on == 3u && gl.mode == GM_WANDER && morph_musical && ((gl.mask >> k) & 1u)) {
            morph_part_t *mp = morph_ready(k);
            if (morph_isfm(mp)) {
                int32_t shape = fm_shape_value(mp->base, morph_view(k, P_DEC));
                if (shape > 0) span = (uint32_t)(1 + shape * 7 / 400);
            }
        }
        if (sqm[k].hold != span) {
            sqm[k].hold = (uint8_t)span;
            sqm_refresh(k, 1);
        }
    }
}
static uint32_t glide_bars(void) { return GLIDE_BARS[gl.time < GLIDE_TIMES ? gl.time : 3u]; }   /* the TIME setting (the page's TIME) */
static uint32_t glide_trip_bars(void) { return GLIDE_BARS[gl.tt < GLIDE_TIMES ? gl.tt : 3u]; }   /* the running trip's length */
static uint32_t glide_lanes_of(uint32_t target) { return target == GT_BOTH ? 3u : 1u << target; }

/* the S-curve 3p^2 - 2p^3, Q16 */
static uint32_t glide_ease(uint32_t p)
{
    uint64_t q = p;
    return (uint32_t)((q * q * (3u << GLIDE_Q) - 2u * q * q * q) >> (2 * GLIDE_Q));
}

/* the faders of lane l on every part it moves, as seen now; a hand on any of them (BOTH: the other part's too) takes the lane over,
 * and so does a sound load on the SOUND lane even where the fader was at A already */
static void glide_seen(uint32_t l)
{
    for (uint32_t k = 0; k < NSHOWN; k++)
        if ((gl.mask >> k) & 1u) {
            gl.last[l][k] = (uint8_t)GLIDE_LANE[l].get(k);
            if (l == GT_SOUND)
                gl.loads[k] = mm_loads[k];
        }
}
static uint32_t glide_touched(uint32_t l)               /* the parts a hand moved; 0x80: a sound load */
{
    uint32_t m = 0;
    for (uint32_t k = 0; k < NSHOWN; k++)
        if ((gl.mask >> k) & 1u) {
            if (GLIDE_LANE[l].get(k) != gl.last[l][k])
                m |= 1u << k;
            if (l == GT_SOUND && mm_loads[k] != gl.loads[k])
                m |= 0x80u;
        }
    return m;
}
/* a hand on lane l: the parts it moved wait (their bars counted from 0 again); a load stops the lane */
static __attribute__((noinline)) void glide_hand(uint32_t l)
{
    uint32_t t = glide_touched(l);
    if (t & 0x80u) {
        gl.on &= (uint8_t)~(1u << l);
        return;
    }
    if (!t)
        return;
    gl.hw[l] |= (uint8_t)t;
    for (uint32_t k = 0; k < NSHOWN; k++)
        if ((t >> k) & 1u)
            gl.hq[k] = 0;
    glide_seen(l);
}
/* MR7: each new bar a waiting part counts; after 8 untouched bars, on a phrase start, it is picked up where the hand left it: its path from a
 * start f that runs through the fader now, to the same end; when that path would be faster than a trip a quarter as long (far off, late in the
 * trip), it waits for the next trip */
static uint32_t glide_ease(uint32_t p);
static __attribute__((noinline)) void glide_back(void)
{
    uint32_t l, k, e, r;
    if (gl.bar_seen == sqm_bar)
        return;
    gl.bar_seen = sqm_bar;
    e = gl.hold ? 0u : glide_ease(gl.prog);
    r = (1u << GLIDE_Q) - e;
    for (k = 0; k < NSHOWN; k++) {
        if (!(((gl.hw[0] | gl.hw[1]) >> k) & 1u) || ++gl.hq[k] < 8u || (sqm_bar & 3u) || !r)
            continue;
        for (l = 0; l < GLIDE_LANES; l++)
            if ((gl.hw[l] >> k) & 1u) {
                int32_t cur = (int32_t)GLIDE_LANE[l].get(k), end = gl.to ? 127 : 0, f = cur + (cur - end) * (int32_t)e / (int32_t)r;
                if (f - end <= 508 && end - f <= 508)
                    gl.hw[l] &= (uint8_t)~(1u << k), gl.from[l][k] = (int16_t)f;
            }
    }
}

static void glide_begin_lanes(void)
{
    uint32_t l;
    for (l = 0; l < GLIDE_LANES; l++) {
        for (uint32_t k = 0; k < NSHOWN; k++)            /* (each part starts from its own fader: BOTH with the faders apart moves each from where it is) */
            gl.from[l][k] = (int16_t)GLIDE_LANE[l].get(k);
        glide_seen(l);
    }
    gl.beats = 0;
    gl.beat_seen = perf_beat;
    gl.bq = gl.run = 0;
    gl.playing = song.playing != 0;
    gl.prog = 0;
    gl.tt = gl.time;                                     /* (every trip start: launch, the downbeat, an arrival, a WAIT's end) */
}

/* launch towards end `to` (0 A, 1 B) on the parts of mask, read from part pi */
static void tide_journey(uint32_t mask, uint32_t x, uint32_t snd, uint32_t seq);
static void jh_clear(uint32_t k);
static void glide_start(uint32_t to, uint32_t mask, uint32_t pi)
{
    if (gl.mode >= GM_WANDER && !gl.on) {
        gl.wbars = gl.wlast = 0;
        for (uint32_t k = 0; k < NSHOWN; k++)            /* (a new session: JOURNEY's memory of the targets starts empty) */
            if ((mask >> k) & 1u)
                jh_clear(k);
    }
    if (gl.mode == GM_JOURNEY && !gl.on && (to & 1u))   /* (JOURNEY starts as it goes on: End B a target too; each part by its own End B, B18; J2: only
                                                         * where the fader is at A: End B is heard elsewhere, the first trip goes to the B that is there) */
        for (uint32_t k = 0; k < NSHOWN; k++)
            if (((mask >> k) & 1u) && !morph_fader_of(k))
                tide_journey(1u << k, 1u, (uint32_t)(((glide_lanes_of(gl.target) >> GT_SOUND) & 1u) && morph_to_of(k, 1u) < 0) << k, 0u);
    if (gl.mode >= GM_WANDER && (glide_lanes_of(gl.target) & (1u << GT_SEQ)))
        rhythm_begin(mask);
    gl.to = (uint8_t)(to & 1u);
    gl.mask = (uint8_t)mask;
    gl.pi = (uint8_t)pi;
    gl.on = (uint8_t)glide_lanes_of(gl.target);
    gl.armed = 1;                                        /* (the next downbeat; stopped: the first one PLAY gives) */
    gl.hw[0] = gl.hw[1] = 0;
    glide_begin_lanes();
}
/* A panel launch (the TIDE page's K1, PERFORM's GLIDE TO) while stopped starts PLAY too, whatever the glide moves (owner,
 * 2026-11-01: the glide and PLAY go together). The request crosses the existing audio queue; armed=2 waits for that start
 * and the trip begins with the sequencer's first bar. Not on an external clock (its Start comes from the gear) nor with REC armed */
static void glide_launch(uint32_t to, uint32_t mask, uint32_t pi)
{
    glide_start(to, mask, pi);
    if (!song.playing && !song.rec && !clock_src()) {
        transport_req = 1u;
        gl.armed = 2;
    }
}
static void glide_stop(void) { gl.on = 0, gl.armed = 0, gl.hold = 0, gl.hw[0] = gl.hw[1] = 0; }
static int glide_running(void) { return gl.on != 0; }

/* progress of the glide now, Q16 (0 at the start .. 1 at the end) */
static uint32_t glide_progress(void)
{
    uint64_t total_beats = (uint64_t)(gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_trip_bars()) * 4u, p;
    if (song.playing) {                                  /* (this stretch: its whole beats and the position in the beat) */
        uint32_t bs = beat_samples(), pos = beat_pos;
        gl.run = (gl.beats << GLIDE_Q) + (uint32_t)(((uint64_t)(pos < bs ? pos : bs) << GLIDE_Q) / (bs ? bs : 1u));
    }
    if (!total_beats)                                    /* (WAIT turned OFF during a hold: the hold is over) */
        return 1u << GLIDE_Q;
    p = ((uint64_t)gl.bq + gl.run) / total_beats;
    return p > (1u << GLIDE_Q) ? 1u << GLIDE_Q : (uint32_t)p;
}

/* an arrival: ONCE stops; the others turn (DRIFT, WANDER, JOURNEY: a new version of the end left, now silent), after WAIT */
static void glide_arrive(void)
{
    uint32_t l, left = gl.to ^ 1u;
    if (gl.mode == GM_ONCE) {
        gl.on = 0;
        return;
    }
    gl_wander = gl.mode == GM_WANDER;
    if (gl.mode >= GM_WANDER) {                          /* the phrase tier of this generation */
        static const uint8_t B[4] = {16, 32, 64, 128};
        uint32_t k;
        gl.wbars = (uint16_t)(gl.wbars + glide_trip_bars());
        mm_tier = 0;
        for (k = 0; k < 4u; k++)
            if (gl.wbars / B[k] > gl.wlast / B[k])
                mm_tier = (int8_t)(k + 1u);
        gl.wlast = gl.wbars;
    }
    if (gl.mode == GM_JOURNEY)                           /* JOURNEY: the end left the next target (a sound of the same kind, the next slot); not on a part a hand holds (MR7: that end may be heard) */
        tide_journey(gl.mask, left, (gl.on >> GT_SOUND) & 1u ? gl.mask & ~gl.hw[GT_SOUND] : 0u, (gl.on >> GT_SEQ) & 1u ? gl.mask & ~gl.hw[GT_SEQ] : 0u);
    else if (gl.mode >= GM_DRIFT)                        /* DRIFT: rolled again from the Original; WANDER: walked on */
        for (l = 0; l < GLIDE_LANES; l++)
            if (((gl.on >> l) & 1u) && (gl.mask & ~gl.hw[l]))
                GLIDE_LANE[l].roll(gl.pi, gl.mask & ~gl.hw[l], left);
    gl_wander = 0;
    gl.to = (uint8_t)left;
    glide_begin_lanes();
    gl.hold = GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] != 0u;
}

/* main loop, every pass: count the beats, move the lanes, notice a hand on a fader */
/* GLIDE's movement, part of every trip (the FM-1 as a background that breathes by itself, docs/design/JAM_ROADMAP.md on branch jam): a
 * bump over the trip, sin^2 (pi p) (0 at both ends: each end is the sound itself), as deep as the trip is long (1 bar 0.3 ..
 * 16 bars and more 1.0). The audio reads it (morph_xfer.c): the filter breathes (morph_xcut, up to two octaves down, never under
 * the notes), the sends swell (half of it), the wash fills what that takes. Out of the patch's own values, back to them */
static void glide_motion(int on)
{
    static const uint16_t DEPTH[GLIDE_TIMES] = {9830, 14746, 21299, 27853, 32768, 32768, 32768};   /* 1 2 4 8 16 32 64 bars */
    uint32_t k, p = gl.prog >> (GLIDE_Q - 15), m = 0, xe;
    if (on) {
        p = p > 32768u ? 32768u : p;
        m = (uint32_t)xrise((int32_t)(p <= 16384u ? p * 2u : (32768u - p) * 2u));
        m = (m * DEPTH[gl.tt < GLIDE_TIMES ? gl.tt : 3u]) >> 15;
        m = (m * XWL[morph_wlvl & 3u]) >> 15;            /* (WASH: x 0, 0.5, 1, 1.5) */
    }
    /* X1: the transition's dip down over 2 bars, its floor 1 bar (never under 2 fader steps a side), up over 2; a 1-4 bar trip: XW0 .. XW1
     * as before. mxs_t xe by TIME (1 2 4 8 16 32 64 bars): the floor's half width and the ramp in 1/256 of the travel, a bar = 256 / TIME */
    static const uint16_t XE_OF[GLIDE_TIMES] = {0u, 0u, 0u, 16u | 64u << 8, 8u | 32u << 8, 4u | 16u << 8, 4u | 8u << 8};
    xe = XE_OF[gl.tt < GLIDE_TIMES ? gl.tt : 3u];
    for (k = 0; k < NSHOWN; k++) {
        if (mxs[k].follow || mxs[k].tp <= XW0 || mxs[k].tp >= XW1)   /* (a hand mid-dip keeps the edges it had: no jump in the dip's depth) */
            mxs[k].xe = (uint16_t)(on && ((gl.mask >> k) & 1u) ? xe : 0u);
        mxs[k].motion = (uint16_t)(on && ((gl.mask >> k) & 1u) ? m : 0u);   /* (MR7: under a hand too, so neither the touch nor the return jumps) */
        mxs[k].follow = (uint8_t)(gl.on && ((gl.on >> GT_SOUND) & 1u) && (((gl.mask & ~gl.hw[GT_SOUND]) >> k) & 1u));   /* (the transition rides the trip itself; a hand's fader is its own) */
    }
}
/* the trip is an automatic WNDR / JRNY on the SEQ lane, whatever ROLL TYPE is (1.0.0-b7, MR8), the transport aside: seq_arc.c keeps its arc through a STOP while this holds */
static uint32_t glide_tide_on(void) { return (gl.on & (1u << GT_SEQ)) && gl.mode >= GM_WANDER && (gl.mask & 3u); }
static void glide_poll(void)
{
    morph_gliding = gl.on != 0;                      /* (morph.c: the blend every other block, budget.c: the FM6 crossing's charge) */
    uint32_t l, k, e, end;
    uint8_t b = perf_beat;
    if (sqm_restart) {                                   /* a load (tide_loaded) put the SEQ fader at A: a running trip starts again from there, towards B */
        sqm_restart = 0;
        if (gl.on) {
            gl.to = 1;
            gl.hold = 0;
            gl.wbars = gl.wlast = 0;
            glide_begin_lanes();
            for (k = 0; k < NSHOWN; k++)
                gl.from[GT_SEQ][k] = 0;
        }
    }
    sqm_auto = (uint8_t)(glide_tide_on() && song.playing && !song.rec && !gl.armed ? gl.mask & 3u : 0u);   /* (automatic MUSIC WNDR / JRNY: TURNS, POLY) */
    rhythm_poll(sqm_auto);
    glide_shape_follow();
    if (!gl.on) {
        glide_motion(0);
        return;
    }
    if (gl.armed == 2u) {
        if (!song.playing) {
            if (transport_req != 1u) glide_stop();  /* STOP cancelled a queued launch */
            return;
        }
        gl.armed = 0;
        glide_begin_lanes();                      /* the sequencer's first bar, not a free-running trip */
    }
    {
        uint32_t nb = (uint8_t)(b - gl.beat_seen);
        gl.beat_seen = b;
        if (!song.playing) {                             /* STOP: it pauses where it is and waits for PLAY */
            if (gl.playing) {
                gl.bq += gl.run;
                gl.beats = gl.run = 0;
                gl.playing = 0;
            }
            if (gl.hold && !GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS]) {
                /* WAIT OFF releases the hold even while stopped; PLAY starts the planned return. */
                gl.hold = 0;
                glide_begin_lanes();
            }
            return;
        }
        if (gl.armed) {
            gl.playing = 1;
            if (!nb || beat_n != 0u)
                return;                                  /* (from the next downbeat; PLAY's own start is one) */
            gl.armed = 0;
            glide_begin_lanes();
            return;
        }
        if (!gl.playing) {                               /* PLAY again: the trip goes on from where STOP left it (PLAY's start beat is not one of it) */
            gl.playing = 1;
            nb = 0;
        }
        gl.beats += nb;
    }
    gl.prog = glide_progress();
    glide_motion(!gl.hold && ((gl.on >> GT_SOUND) & 1u));   /* (the SOUND lane only: on BOTH a load stops it and the SEQ lane goes on; the new sound does not move) */
    for (l = 0; l < GLIDE_LANES; l++)                    /* a hand (or a load) on any part it moves, moving or held */
        if ((gl.on >> l) & 1u)
            glide_hand(l);
    glide_back();
    if (gl.hold) {                                       /* WAIT: the faders still */
        if (gl.prog >= (1u << GLIDE_Q)) {
            gl.hold = 0;
            gl.wbars = (uint16_t)(gl.wbars + GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS]);   /* (WANDER's phrase count: the held bars too) */
            glide_begin_lanes();
        }
        return;
    }
    e = glide_ease(gl.prog);
    end = gl.to ? 127u : 0u;
    for (l = 0; l < GLIDE_LANES; l++) {
        uint32_t v[NSHOWN], any = 0;
        if (!((gl.on >> l) & 1u))
            continue;
        {                                                /* (rounded to the nearer step either way: C's division truncates toward 0, so a
                                                         * +half rounded a trip down only from above and 127 -> 0 ended at 1; the end itself, exactly) */
            int32_t h = 1 << (GLIDE_Q - 2);
            for (k = 0; k < NSHOWN; k++)                 /* each part on its own path, from its own start (a part a hand holds: not moved) */
                if (((gl.mask & ~gl.hw[l]) >> k) & 1u) {
                    int32_t num = ((int32_t)end - (int32_t)gl.from[l][k]) * (int32_t)(e >> 1);
                    int32_t w = gl.from[l][k] + (num >= 0 ? (num + h) / (2 * h) : -((-num + h) / (2 * h)));
                    v[k] = gl.prog >= (1u << GLIDE_Q) ? end : (uint32_t)(w < 0 ? 0 : w > 127 ? 127 : w);
                    any |= gl.last[l][k] != v[k];
                }
        }
        if (any) {
            for (k = 0; k < NSHOWN; k++)
                if (((gl.mask & ~gl.hw[l]) >> k) & 1u)
                    GLIDE_LANE[l].set(gl.pi, 1u << k, v[k]);
            glide_seen(l);
        }
    }
    if (gl.on && gl.prog >= (1u << GLIDE_Q))
        glide_arrive();
}
