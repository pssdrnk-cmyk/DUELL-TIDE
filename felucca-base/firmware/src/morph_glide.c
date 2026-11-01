/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): GLIDE. A launched fader travels by itself from where it is to an end (A or B) over a number of bars, along an
 * S-curve. Generic lanes: a lane is a 0..127 value of a part set with a getter and a setter; the MORPH fader and the SEQ
 * MORPH fader are the two lanes today.
 *
 * Time: while the transport plays, bars of the musical clock (the internal tempo or an external MIDI clock: the beats the
 * sequencer counts, perf_beat, plus the position inside the beat), starting on the next downbeat; if the transport stops
 * the glide waits and goes on with PLAY. Launched while stopped, it runs in real time at the TEMPO setting.
 * MODE, at each arrival (the end left is silent there: its weight is 0), for each lane TARGET moves:
 *   ONCE     stops.
 *   LOOP     turns back: the same two ends (the SEQ comes home each trip).
 *   DRIFT    the end left is rolled again from the Original: the sound and the pattern vary around themselves.
 *   WANDER   the end left walks on from the end reached (morph_music.c mm_walk; seq_morph.c seqm_wander_gen): always computed
 *            from the Original, within AMOUNT / SEQ AMT, nothing pulls it home.
 *   JOURNEY  the end left becomes the next target (ui_jam.c tide_journey): a sound of the same engine and kind, never the
 *            other part's; the pattern a WANDER generation, each part its own.
 * WAIT holds each arrival that many bars before the next trip. WANDER and JOURNEY follow the phrase grid: the bars run (gl.wbars)
 * give each generation the tier of the largest boundary crossed since the last (morph_music.c mm_tier: 16, 32, 64, 128 bars).
 * Touching a fader (its knob, a MIDI CC, a patch load that resets it) stops that lane where it is.
 * Context: main loop only (glide_poll from ui_input); it writes through the morphs' own actions, nothing in the
 * interrupt changes. */

enum { GT_SOUND, GT_SEQ, GT_BOTH };                      /* TARGET */
enum { GM_ONCE, GM_LOOP, GM_DRIFT, GM_WANDER, GM_JOURNEY };   /* MODE (JOURNEY: each arrival, the end left the next target, ui_jam.c tide_journey) */
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
    uint8_t clocked;                                     /* 1: the musical clock, 0: real time */
    uint8_t beat_seen;
    uint8_t from[GLIDE_LANES], last[GLIDE_LANES];
    uint32_t beats;                                      /* whole beats since the start (clocked) */
    uint32_t t0;                                         /* fm1_ms of the start (real time) */
    uint32_t prog;                                       /* progress 0..1 << GLIDE_Q, for the page */
    uint8_t wait;                                        /* WAIT: an index of GLIDE_WAIT_BARS */
    uint32_t wbars, wlast;                               /* WANDER, JOURNEY: the bars run since its start, at the last arrival */
    uint8_t hold;                                        /* holding an arrival WAIT bars (the faders still) */
} gl = {GT_SOUND, GM_ONCE, 3u, 0, 0, 0, 0, 0, 0, 0, {0}, {0}, 0, 0, 0};

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
static uint32_t glide_bars(void) { return GLIDE_BARS[gl.time < GLIDE_TIMES ? gl.time : 3u]; }
static uint32_t glide_lanes_of(uint32_t target) { return target == GT_BOTH ? 3u : 1u << target; }

/* the S-curve 3p^2 - 2p^3, Q16 */
static uint32_t glide_ease(uint32_t p)
{
    uint64_t q = p;
    return (uint32_t)((q * q * (3u << GLIDE_Q) - 2u * q * q * q) >> (2 * GLIDE_Q));
}

static void glide_begin_lanes(void)
{
    uint32_t l;
    for (l = 0; l < GLIDE_LANES; l++)
        gl.from[l] = gl.last[l] = (uint8_t)GLIDE_LANE[l].get(gl.pi);
    gl.beats = 0;
    gl.beat_seen = perf_beat;
    gl.t0 = fm1_ms;
    gl.prog = 0;
}

/* launch towards end `to` (0 A, 1 B) on the parts of mask, read from part pi */
static void tide_journey(uint32_t mask, uint32_t x, uint32_t snd, uint32_t seq);
static void glide_start(uint32_t to, uint32_t mask, uint32_t pi)
{
    if (gl.mode >= GM_WANDER && !gl.on)
        gl.wbars = gl.wlast = 0;
    if (gl.mode == GM_JOURNEY && !gl.on && (to & 1u))   /* (JOURNEY starts as it goes on: End B a target too) */
        tide_journey(mask, 1u, ((glide_lanes_of(gl.target) >> GT_SOUND) & 1u) && morph_to_of(pi, 1u) < 0, 0u);
    if (gl.mode >= GM_WANDER && morph_musical && (glide_lanes_of(gl.target) & (1u << GT_SEQ)))
        rhythm_begin(mask);
    gl.to = (uint8_t)(to & 1u);
    gl.mask = (uint8_t)mask;
    gl.pi = (uint8_t)pi;
    gl.on = (uint8_t)glide_lanes_of(gl.target);
    gl.clocked = song.playing != 0;
    gl.armed = gl.clocked;
    glide_begin_lanes();
}
/* A panel launch of a prepared pattern starts PLAY too. The request crosses the
 * existing audio queue; armed=2 waits for that start before following its clock. */
static void glide_launch(uint32_t to, uint32_t mask, uint32_t pi)
{
    uint32_t has_notes = 0;
    if (!song.playing && !song.rec && !clock_src() && gl.target != GT_SOUND)
        for (uint32_t k = 0; k < NSHOWN; k++)
            if ((mask >> k) & 1u)
                for (uint32_t j = 0; j < (uint32_t)clamp(trk[k].p[P_SLEN], 1, NSTEP); j++)
                    has_notes |= trk[k].step[j].time == ST_NOTE && trk[k].step[j].n;
    glide_start(to, mask, pi);
    if (has_notes) {
        transport_req = 1u;
        gl.clocked = 1;
        gl.armed = 2;
    }
}
static void glide_stop(void) { gl.on = 0, gl.armed = 0, gl.hold = 0; }
static int glide_running(void) { return gl.on != 0; }

/* progress of the glide now, Q16 (0 at the start .. 1 at the end) */
static uint32_t glide_progress(void)
{
    uint64_t total_beats = (uint64_t)(gl.hold ? GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS] : glide_bars()) * 4u, p;
    if (gl.clocked) {
        uint32_t bs = beat_samples(), pos = beat_pos;
        p = (((uint64_t)gl.beats << GLIDE_Q) + ((uint64_t)(pos < bs ? pos : bs) << GLIDE_Q) / (bs ? bs : 1u)) / total_beats;
    } else {
        uint64_t ms = (uint64_t)total_beats * 60000u / (uint32_t)clamp(song.g[G_BPM], 40, 240);
        p = ((uint64_t)(fm1_ms - gl.t0) << GLIDE_Q) / (ms ? ms : 1u);
    }
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
        gl.wbars += glide_bars();
        mm_tier = 0;
        for (k = 0; k < 4u; k++)
            if (gl.wbars / B[k] > gl.wlast / B[k])
                mm_tier = (int8_t)(k + 1u);
        gl.wlast = gl.wbars;
    }
    if (gl.mode == GM_JOURNEY)                           /* JOURNEY: the end left the next target (a sound of the same kind, the next slot) */
        tide_journey(gl.mask, left, (gl.on >> GT_SOUND) & 1u, (gl.on >> GT_SEQ) & 1u);
    else if (gl.mode >= GM_DRIFT)                        /* DRIFT: rolled again from the Original; WANDER: walked on */
        for (l = 0; l < GLIDE_LANES; l++)
            if ((gl.on >> l) & 1u)
                GLIDE_LANE[l].roll(gl.pi, gl.mask, left);
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
    uint32_t k, p = gl.prog >> (GLIDE_Q - 15), m = 0;
    if (on) {
        p = p > 32768u ? 32768u : p;
        m = (uint32_t)xrise((int32_t)(p <= 16384u ? p * 2u : (32768u - p) * 2u));
        m = (m * DEPTH[gl.time < GLIDE_TIMES ? gl.time : 3u]) >> 15;
        m = (m * XWL[morph_wlvl & 3u]) >> 15;            /* (WASH LEVEL: x 0, 0.5, 1, 1.5) */
    }
    for (k = 0; k < NSHOWN; k++) {
        mxs[k].motion = (uint16_t)(on && ((gl.mask >> k) & 1u) ? m : 0u);
        mxs[k].follow = (uint8_t)(gl.on && ((gl.on >> GT_SOUND) & 1u) && ((gl.mask >> k) & 1u));   /* (the transition rides the trip itself) */
    }
}
static void glide_poll(void)
{
    uint32_t l, e, end;
    uint8_t b = perf_beat;
    rhythm_poll(gl.on && (gl.on & (1u << GT_SEQ)) && gl.mode >= GM_WANDER &&
                morph_musical && song.playing && !song.rec && gl.clocked && !gl.armed ? gl.mask : 0u);
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
    if (gl.clocked) {
        uint32_t nb = (uint8_t)(b - gl.beat_seen);
        gl.beat_seen = b;
        if (!song.playing)
            return;                                      /* (stopped: it waits for PLAY) */
        if (gl.armed) {
            if (!nb || beat_n != 0u)
                return;                                  /* (from the next downbeat) */
            gl.armed = 0;
            glide_begin_lanes();
            return;
        }
        gl.beats += nb;
    }
    gl.prog = glide_progress();
    glide_motion(!gl.hold && ((gl.on >> GT_SOUND) & 1u));   /* (the SOUND lane only: on BOTH a load stops it and the SEQ lane goes on; the new sound does not move) */
    if (gl.hold) {                                       /* WAIT: the faders still; a hand still takes a lane over */
        for (l = 0; l < GLIDE_LANES; l++)
            if (((gl.on >> l) & 1u) && GLIDE_LANE[l].get(gl.pi) != gl.last[l])
                gl.on &= (uint8_t)~(1u << l);
        if (gl.prog >= (1u << GLIDE_Q)) {
            gl.hold = 0;
            gl.wbars += GLIDE_WAIT_BARS[gl.wait % GLIDE_WAITS];   /* (WANDER's phrase count: the held bars too) */
            glide_begin_lanes();
        }
        return;
    }
    e = glide_ease(gl.prog);
    end = gl.to ? 127u : 0u;
    for (l = 0; l < GLIDE_LANES; l++) {
        uint32_t v, now;
        if (!((gl.on >> l) & 1u))
            continue;
        now = GLIDE_LANE[l].get(gl.pi);
        if (now != gl.last[l]) {                         /* a hand (or a load) moved it: this lane stops here */
            gl.on &= (uint8_t)~(1u << l);
            continue;
        }
        {                                                /* (rounded to the nearer step either way: C's division truncates toward 0, so a
                                                         * +half rounded a trip down only from above and 127 -> 0 ended at 1; the end itself, exactly) */
            int32_t num = ((int32_t)end - (int32_t)gl.from[l]) * (int32_t)(e >> 1), h = 1 << (GLIDE_Q - 2);
            v = gl.prog >= (1u << GLIDE_Q) ? end
                : (uint32_t)((int32_t)gl.from[l] + (num >= 0 ? (num + h) / (2 * h) : -((-num + h) / (2 * h))));
        }
        if (v != now)
            GLIDE_LANE[l].set(gl.pi, gl.mask, v);
        gl.last[l] = (uint8_t)GLIDE_LANE[l].get(gl.pi);
    }
    if (gl.on && gl.prog >= (1u << GLIDE_Q))
        glide_arrive();
}
