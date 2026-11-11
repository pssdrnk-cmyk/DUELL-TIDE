/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): MUSICAL MORPH v4, the dial model. An end is the Original turned on five dials, -1000..1000 each; a dial moves a set of
 * values together, each toward the edge of its role's band (where the factory sounds of that role play it: SOUND_STUDY.md, the
 * 10..90 % ranges, always held around the Original and at least MM_MIN wide either way), with the answers built in:
 *   BRIGHT  the cutoff, the filter envelope's depth (half), the engine's bright values (half, up only a third of their reach: they
 *           turn metallic); resonance the other way (a third), so a brighter end is not a louder one
 *   MOVE    the LFO on the cutoff and its rate (half)
 *   SHAPE   the decay (the filter envelope's along), the attack (a third), the release (half): the stab's length, the pad's swell
 *   SPACE   reverb, delay (two thirds), chorus (half); the release a little the other way
 *   CHAR    the engine's own values, each its own way (a fixed mix per value), in a third of their band around the Original
 *   PITCH   (PITCH ON only) the engine's pitch values: TRIO's intervals an octave past 600, else detune in a small band
 * AMOUNT is how far the dials may turn (100: to the band edges); a roll turns each dial half to all of that, either way, so even
 * AMOUNT 20 is audible. Never moved: drive, level, the played note, voicing, arp, routes, choices.
 * WANDER (morph_glide.c, mm_walk set) walks the dials of the end reached (mm_dv) instead of rolling them: each keeps its way,
 * turns now and then (1 in 6) and bounces at AMOUNT's edges; the step grows with the phrase tier (mm_tier: 16 .. 128 bars).
 * BRIGHT, MOVE and CHAR step every generation, SHAPE and SPACE from 16 bars, PITCH from 64. The end is always computed from the
 * Original and the dials, never from the end before, so nothing accumulates and it cannot run away (v3 walked the values one by
 * one with a compass, a budget, a neighbourhood and pairs on top: a roll at a factory AMOUNT of 7 moved one value one step).
 * Then the SMART safety rules. Included by morph.c (before morph_roll_full); main loop only. */

#define MC_MUSIC 64u                                     /* the end's roll: MUSICAL (cfg bits, saved with the end; mrc while it rolls) */
#define MM_AMOUNT 40u                                    /* AMOUNT for a factory sound's B and the part's knob (MUSICAL's own scale) */
#define MM_MIN 20                                        /* a value's reach either way at least this (out of 127) */
/* 1.0.0-b9 (K2): ROLL TYPE is retired. With MM_LOCK every new roll is MUSIC whatever the stored ROLL TYPE byte (morph_musical, morph.cfg MC_WILD) says; the
 * bytes are still read and kept, and a saved End B recalls from its own cfg bits (SMART / WILD seeds unchanged: morph_roll_cfg takes the saved cfg, not these).
 * The device build locks; host_tests.sh builds with -DTIDE_ROLL_LOCK=0 (the old, free roll: the tests set morph_musical to check the plain roll), tests/roll_lock_test.c with 1 */
#ifndef TIDE_ROLL_LOCK
#define TIDE_ROLL_LOCK 1
#endif
#define MM_LOCK (TIDE_ROLL_LOCK)
#define MM_ON() (MM_LOCK || morph_musical)
#define MM_NOWILD (MM_LOCK ? MC_WILD : 0u)
static uint8_t morph_musical = 1;                        /* the player's choice (TIDE LIST's ROLL): the next rolls MUSICAL (the fork's default; the DSP
                                                          * harness, tests/hostsim.c, clears it: upstream's tests check the classic roll) */
static uint8_t mm_walk;                                  /* WANDER: set while the next roll walks the dials of the end reached */
static int8_t mm_tier;                                   /* WANDER: the phrase tier of this generation (0 .. 4: 16 .. 128 bars; morph_glide.c) */
static int8_t mm_x = -1;                                 /* the end being rolled (morph_roll_cfg), -1: none (its dials not kept) */

enum { MMR_BASS, MMR_LEAD, MMR_PAD, MMR_PLUCK, MMR_N };      /* the roles (KEYS, FX, OTHER play as LEAD: its bands are the widest) */
enum { MB_CUT, MB_RES, MB_E2F, MB_ATK, MB_DEC, MB_REL, MB_LFO, MB_CHO, MB_DLY, MB_REV, MB_N };
/* the bands, lo .. hi per role (SOUND_STUDY.md 2: the factory sounds' 10 .. 90 %; LEAD widened by KEYS'; audit P07: RES in the exponential law's steps of the same damping) */
static const int8_t MM_BAND[MMR_N][MB_N][2] = {
    /*            CUT       RES       E2>F     ATK      DEC        REL        LFO>F    CHO       DLY      REV */
    /* BASS  */ {{27, 52}, {4, 24},  {0, 31}, {0, 10}, {70, 82},  {44, 60},  {0, 8}, {0, 0},   {0, 0},  {0, 6}},
    /* LEAD  */ {{41, 82}, {0, 29},  {0, 37}, {0, 59}, {70, 109}, {44, 106}, {0, 7}, {0, 64},  {0, 32}, {15, 64}},
    /* PAD   */ {{47, 61}, {3, 6},   {7, 17}, {69, 94}, {70, 105}, {92, 105}, {0, 5}, {32, 90}, {0, 19}, {42, 89}},
    /* PLUCK */ {{43, 67}, {4, 8},   {7, 31}, {0, 10}, {70, 95},  {60, 76},  {0, 5}, {0, 28},  {0, 38}, {23, 51}},
};

/* the engines' own values a SOURCE move may take, with their bands (the factory sounds' 10 .. 90 %); eng: the engine's index (engines.c ENGINES[]) */
typedef struct { uint8_t eng, e; int8_t lo, hi, bright, pit; } mm_src_t;   /* bright: up brightens (or turns metallic); pit: a pitch value */
static const mm_src_t MM_ENG[] = {                     /* 0 ANALOG, 2 PHASE, 3 LOFI, 5 VOICE, 6 TRIO, 7 WHEEL, 9 PHYS, 11 NOISE, 12 FM6, 14 WAVE */
    {0, 1, 0, 37, 0, 1}, {0, 2, 0, 102, 0, 0},
    {2, 2, 0, 50, 1, 0}, {2, 3, 14, 71, 1, 0}, {2, 4, 0, 13, 0, 1},
    {3, 2, 40, 92, 0, 0}, {3, 7, 52, 110, 1, 0},
    {5, 0, 0, 95, 0, 0}, {5, 1, 0, 127, 0, 0}, {5, 2, 20, 100, 0, 0}, {5, 4, 30, 96, 1, 0}, {5, 6, 50, 110, 1, 0},
    {6, 3, 0, 9, 0, 1}, {6, 7, 17, 64, 0, 0},
    {7, 2, -4, 0, 0, 0}, {7, 3, -8, 2, 1, 0}, {7, 5, 0, 50, 1, 0},
    {9, 1, 8, 116, 1, 0}, {9, 2, 47, 100, 1, 0}, {9, 3, 56, 120, 0, 0}, {9, 4, 10, 44, 0, 0},
    {11, 1, 0, 64, 1, 0}, {11, 6, 0, 47, 0, 0},
    {12, 2, -8, 14, 1, 0}, {12, 4, 0, 25, 1, 0}, {12, 6, 0, 70, 1, 1},
    {14, 1, 10, 50, 0, 0}, {14, 5, 0, 10, 0, 1},
};

static int mm_role(const int8_t *o)
{
    if (o[P_ATK] >= 60)
        return MMR_PAD;
    if (o[P_SUS] <= 20)
        return MMR_PLUCK;
    if (o[P_REV] <= 8 && !o[P_CHOR] && !o[P_DLY])
        return MMR_BASS;
    return MMR_LEAD;
}

static void mm_put(int8_t *end, const engine_t *e, uint32_t id, int32_t v)
{
    const param_desc_t *pd = morph_desc(e, id);
    end[id] = (int8_t)clamp(v, pd->min, pd->max);
}

static int mm_filter_on(const engine_t *e, const int8_t *o) { return e->ufilt && (o[P_FREEA] < UF_STEPS || o[P_FREEC] > 0); }

enum { MDL_BRIGHT, MDL_MOVE, MDL_SHAPE, MDL_SPACE, MDL_CHAR, MDL_PITCH, MDL_N };
static int32_t sound_complement_dir(uint32_t pi, uint32_t axis, int32_t direction);
static int16_t mm_dv[NSHOWN][2][MDL_N];                   /* each end's dials (WANDER walks on from them) */
static int8_t mm_dir[NSHOWN][MDL_N];                      /* WANDER: each dial's way */
static uint8_t mm_loads[NSHOWN];                          /* each part's sound loads (morph_rebase_part): GLIDE sees a load that left the fader as it was */

/* WANDER's dial walk, one for the sound's dials (morph_roll_musical) and the pattern's (seq_morph.c seqm_wander_gen): dial v a step of
 * move (+1 / -1) on, bouncing at lim (its way, *dir, turns there) */
static int32_t tide_walk(int32_t v, int8_t *dir, int32_t move, int32_t step, int32_t lim)
{
    v += move * step;
    if (v > lim)
        v = lim, *dir = -1;
    else if (v < -lim)
        v = -lim, *dir = 1;
    return v;
}

/* value id at dial d (-1000..1000, times w/6): toward hi at +1000, lo at -1000; the reach either way at least min */
static void mm_dial(int8_t *end, const engine_t *e, const int8_t *o, uint32_t id, int32_t d, int32_t w, int32_t lo, int32_t hi, int32_t min)
{
    int32_t c = o[id], up = hi - c, dn = c - lo;
    up = up < min ? min : up, dn = dn < min ? min : dn;
    d = d * w / 6;
    mm_put(end, e, id, c + (d > 0 ? up * d : dn * d) / 1000);
}

/* the end: the Original o turned on the dials d */
static void mm_apply(const int8_t *o, int8_t *end, const engine_t *e, const int32_t *d, uint32_t part)
{
    const int8_t(*b)[2] = MM_BAND[mm_role(o)];
    uint32_t i;
    if (e->ufilt) {                                      /* BRIGHT */
        mm_dial(end, e, o, P_FREEA, d[MDL_BRIGHT], 6, b[MB_CUT][0], b[MB_CUT][1], MM_MIN);
        mm_dial(end, e, o, P_FREEB, -d[MDL_BRIGHT], 2, b[MB_RES][0], b[MB_RES][1], 4);   /* (audit P07: the least span 4, the damping change 12 steps made at the bands' low ends, RES 0 .. 11, under the linear law) */
        mm_dial(end, e, o, P_E2FLT, d[MDL_BRIGHT], 3, b[MB_E2F][0], b[MB_E2F][1], 12);
    }
    if (mm_filter_on(e, o)) {                            /* MOVE */
        mm_dial(end, e, o, P_LD_FLT, d[MDL_MOVE], 6, b[MB_LFO][0], b[MB_LFO][1], 8);
        mm_dial(end, e, o, P_LRATE, d[MDL_MOVE], 3, 17, 40, 10);
    }
    mm_dial(end, e, o, P_DEC, d[MDL_SHAPE], 6, b[MB_DEC][0], b[MB_DEC][1], MM_MIN);   /* SHAPE */
    mm_put(end, e, P_E2DEC, o[P_E2DEC] + end[P_DEC] - o[P_DEC]);
    mm_dial(end, e, o, P_ATK, d[MDL_SHAPE], 2, b[MB_ATK][0], b[MB_ATK][1], 10);
    mm_dial(end, e, o, P_REL, d[MDL_SHAPE] - d[MDL_SPACE] / 3, 3, b[MB_REL][0], b[MB_REL][1], MM_MIN);
    mm_dial(end, e, o, P_REV, d[MDL_SPACE], 6, b[MB_REV][0], b[MB_REV][1], 14);       /* SPACE */
    mm_dial(end, e, o, P_DLY, d[MDL_SPACE], 4, b[MB_DLY][0], b[MB_DLY][1], 10);
    mm_dial(end, e, o, P_CHOR, d[MDL_SPACE], 3, b[MB_CHO][0], b[MB_CHO][1], 10);
    for (i = 0; i < NELEM(MM_ENG); i++) {                /* the engine's own: BRIGHT, CHAR, PITCH */
        const mm_src_t *q = &MM_ENG[i];
        uint32_t id = P_E0 + q->e;
        int32_t w = (q->hi - q->lo) / 3 + 1, lo, hi;
        if (ENGINES[q->eng] != e)
            continue;
        lo = o[id] - w > q->lo ? o[id] - w : q->lo;
        hi = o[id] + w < q->hi ? o[id] + w : q->hi;
        lo = lo < o[id] ? lo : o[id], hi = hi > o[id] ? hi : o[id];
        if (q->pit) {
            if (!(mrc & MC_PIT) || !d[MDL_PITCH])
                continue;
            if (e == &ENG_TRIO) {               /* (an interval an octave, past 600) */
                if (d[MDL_PITCH] > 600 || d[MDL_PITCH] < -600)
                    mm_put(end, e, id, o[id] + (d[MDL_PITCH] > 0 ? 12 : -12));
            } else {
                mm_dial(end, e, o, id, d[MDL_PITCH], 6, lo, hi, 4);
            }
        } else if (q->bright) {
            int32_t h = o[id] + w / 3 + 1;
            mm_dial(end, e, o, id, d[MDL_BRIGHT], e->ufilt ? 3 : 6, lo, h < hi ? h : hi, 4);
        } else {
            mm_dial(end, e, o, id, (i * 7u + part) % 3u == 1u ? -d[MDL_CHAR] : d[MDL_CHAR], 6, lo, hi, 6);
        }
    }
}

/* the MUSICAL end of the Original o: the dials rolled (or, WANDER, walked on from the end reached), then the SMART safety rules */
static void morph_roll_musical(const int8_t *o, int8_t *end, const engine_t *e, uint32_t amount, uint32_t seed, uint32_t part, uint32_t stereo)
{
    static const uint8_t TF[5] = {2, 3, 4, 6, 8};        /* WANDER's step per tier, 16ths of AMOUNT */
    uint32_t r = morph_state(seed, part + 48u), i, pi = part % NSHOWN;
    int32_t d[MDL_N], lim;
    (void)stereo;
    for (i = 0; i < EXT_P_LEN; i++)
        end[i] = o[i];
    amount = amount > 100u ? 100u : amount;
    lim = (int32_t)amount * 10;
    if (mm_walk && mm_x >= 0) {                          /* WANDER: a step on from the end reached */
        int32_t step = 1 + lim * TF[(uint32_t)mm_tier % 5u] / 16;
        for (i = 0; i < MDL_N; i++) {
            int8_t *dr = &mm_dir[pi][i];
            d[i] = clamp(mm_dv[pi][mm_x ^ 1][i], -lim, lim);
            if ((i == MDL_SHAPE || i == MDL_SPACE) && mm_tier < 1)
                continue;
            if (i == MDL_PITCH && (mm_tier < 3 || !(mrc & MC_PIT)))
                continue;
            if (!*dr || !mr_below(&r, 6u))
                *dr = (int8_t)(mr_below(&r, 2u) ? 1 : -1);
            /* Only automatic WNDR: retain the independent walk half the time.
             * A separate seed hash leaves the other dials' random stream intact. */
            int32_t move = *dr;
            if ((i == MDL_BRIGHT || i == MDL_SHAPE || i == MDL_MOVE || i == MDL_SPACE) && (morph_state(seed, part + 80u + i) & 1u)) {
                int32_t contrast = sound_complement_dir(pi, i, move);
                int32_t next = d[i] + contrast * step;
                /* Contrast must not pin an end against AMOUNT instead of letting its walk turn inward. */
                if (next >= -lim && next <= lim)
                    move = contrast;
            }
            if (i == MDL_BRIGHT && tarc_bright())            /* ARC: brighter towards the peak, softer in the breakdown */
                move = tarc_bright();
            d[i] = tide_walk(d[i], dr, move, step, lim);
        }
    } else {                                             /* a roll: each dial half to all of AMOUNT, either way */
        for (i = 0; i < MDL_N; i++) {
            int32_t m = lim * (500 + (int32_t)mr_below(&r, 501u)) / 1000;
            d[i] = mr_below(&r, 2u) ? m : -m;
        }
        if (!(mrc & MC_PIT) || mr_below(&r, 3u))         /* (PITCH: rarely, and only with PITCH ON) */
            d[MDL_PITCH] = 0;
    }
    if (mm_x >= 0)
        for (i = 0; i < MDL_N; i++)
            mm_dv[pi][mm_x][i] = (int16_t)d[i];
    if (!amount)
        return;
    mm_apply(o, end, e, d, part);
    morph_rules(e, o, o, end, amount);                  /* (1.1's rules read the start too: here the Original) */
    if (e->ufilt) {
        int32_t c = end[P_FREEA];
        morph_uf_safe(o, o, end, amount);
        if (end[P_FREEC] == o[P_FREEC] && c < end[P_FREEA])   /* (its window holds a dark Original's CUT from going lower; the MODE
                                                         * stays, so darker by up to 12 steps, ~1.5 octaves, is safe: BRIGHT works both ways) */
            end[P_FREEA] = (int8_t)(c > o[P_FREEA] - 12 ? c : o[P_FREEA] - 12);
    }
    if (e == &ENG_ANALOG || e == &ENG_TRIO)
        morph_uf_open(o, end, e);
}
