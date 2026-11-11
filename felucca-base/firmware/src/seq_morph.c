/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): SEQ MORPH v2, the dial model. Each shown part's pattern has two ends, A and B, and a fader between them. An end is the
 * stored pattern (the Original) varied by four dials, -100..100 (each held within SEQ AMT), and a seed that ranks which steps
 * they touch:
 *   DENS   + hits added on free steps (a softer copy of the hit before; the offbeats first), from +50 a ratchet; - hits dropped
 *   SYNC   hits shifted a 16th later (+) or earlier (-)
 *   ACC    accents added (+) or taken off (-)
 *   VOIC   chords inverted up (+) or down (-), an octave at most; single notes: from 50, a few an octave up or down
 * No dial moves a note to another pitch class: a random note cannot know the harmony.
 * The anchors (the downbeats: steps 1 5 9 13 .., and the motif's own: seq_rhythm.c sqm_anchor) are never dropped or moved; a held hit (tie, slide, ratchet) keeps its
 * timing; a pattern keeps at least one hit, and at most two are added or dropped. An end is computed from the Original every time,
 * never from another end, so nothing accumulates (v1 kept each variation and varied it again: in 32 generations the stabs became
 * quiet ghosts spread over three octaves).
 * End A is the Original until a WANDER or JOURNEY glide makes it a generation: each arrival rolls the end left as the next one, its
 * dials a step on from the end reached (momentum, the step by the phrase tier), so every trip goes from one generation to the
 * next. The fader brings End B in a beat at a time (each beat its own threshold): monotone and reversible.
 * VARY BARS (TIDE LIST): EVERY 4TH (the default) plays the variation only on every 4th bar since PLAY (bars 4 and 8 of each 8-bar
 * phrase; the others play End A), as a dub techno loop holds and changes at the turnaround (after DUB-1, sbegueria/fm1-dub);
 * ALL plays it on every bar. A bar is 4 beats of the clock (seq.c sqm_bar), whatever the pattern's length or DIV.
 * Contexts: the ends are built in the main loop into sqm_pat (seqm_poll, and every control); seq_play_step() runs in the audio
 * interrupt and only reads them. Nothing is stored: a power cycle starts with the fader at 0 and End A the Original. */
/* MUSIC WNDR/JRNY also carries the cumulative phrase overlay (seq_rhythm.c,
 * seq_phrase.c): retained routing, repeats/rests and phrase-level pitch edits.
 * These are session state until KEEP commits the reached ordinary steps. */
#define SQM_HIST 4u                                      /* rolls kept per part (a ring of End B) */
enum { SD_DENS, SD_SYNC, SD_ACC, SD_VOIC, SD_N };
#define SQM_PITCH_BYTES ((NSTEP * 5u + 7u) / 8u)
typedef struct { uint16_t seed; int8_t d[SD_N]; uint8_t pitch[SQM_PITCH_BYTES]; } sqm_end_t;
typedef struct {
    sqm_end_t b[SQM_HIST];                               /* a ring: b[top] is End B */
    sqm_end_t a;                                         /* End A while aon (a WANDER / JOURNEY generation), else the Original */
    uint8_t aon, top, n;                                 /* n: rolls kept (0: none yet, the part's default comes first) */
    uint8_t fader;                                       /* 0 End A .. 127 End B */
    uint8_t amt_x;                                       /* SEQ AMT 0..100 as SQM_AMT_DEF ^ it (no knob; 0 = the default, so the table is .bss, not 504 B of flash; tests set it) */
    int8_t dir[SD_N];                                    /* WANDER: each dial's way */
    uint8_t ok;                                          /* sqm_pat holds the ends of sig */
    uint8_t hold;                                        /* WNDR+BOTH: minimum note span of the heard FM6 shape */
    uint32_t sig, tseed;                                 /* .. its key; the beats' thresholds */
} sqm_part_t;
#define SQM_AMT_DEF 50u                                  /* SEQ AMT (40 left most generations alike) */
#define SQM_AMT(q) ((uint32_t)((q)->amt_x ^ SQM_AMT_DEF))
static sqm_part_t sqm[NSHOWN];
static step_t sqm_pat[NSHOWN][2][NSTEP];               /* the two ends as played (.bss: the audio interrupt reads it) */
static step_t sqm_buf[NSTEP] __attribute__((section(".pool")));   /* an end being built, KEEP's copy (main loop; the pool: .bss is the tighter, plan §4.9.2) */
static uint32_t sqm_poll_ms;
static uint32_t sqm_len(const track_t *t);
static uint32_t fnv(uint32_t h, const void *p, uint32_t n);   /* (ui.c: FNV-1a over n bytes, the TIDE keys' hash too) */

static uint32_t sqm_key(const sqm_part_t *q) { return q->sig ^ q->tseed; }   /* (the UI's redraw key: changes with what the ends play) */
static uint32_t sqm_hash(uint32_t seed, uint32_t idx, uint32_t salt)
{
    uint32_t x = seed * 0x9E3779B1u ^ (idx + 1u) * 0x85EBCA77u ^ (salt + 1u) * 0xC2B2AE3Du;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ x >> 16;
}
static int sqm_hit(const step_t *s) { return s->time == ST_NOTE && s->n; }
/* the Original's lane (C1, TIDE_ROLES_AND_MULTIS_SPEC.md): from its lowest hit note (a lane that starts above C3 may still not reach under C3: 48) to its highest plus an octave, inside the outer bound 24..108; a note outside it is never generated.
 * Used by sqm_build's VOIC, voicelead_walk and the pitch walk (seq_voicelead.c, seq_evolve.c, included below). The Original's own notes are never moved by it: only new ones are tested */
static void sqm_lane(const step_t *p, uint32_t len, int32_t *lo, int32_t *hi)
{
    uint32_t i, k;
    int32_t l = 127, h = 0;
    for (i = 0; i < len; i++)
        if (sqm_hit(&p[i]))
            for (k = 0; k < p[i].n && k < 4u; k++) {
                l = p[i].note[k] < l ? p[i].note[k] : l;
                h = p[i].note[k] > h ? p[i].note[k] : h;
            }
    if (l > h) l = 24, h = 96;                           /* (no hit: the old bounds) */
    l = l > 48 ? 48 : l < 24 ? 24 : l;                   /* (a pad, a skank: never under C3) */
    h += 12;
    *lo = l, *hi = h > 108 ? 108 : h;
}
static int sqm_in_lane(int32_t n, int32_t lo, int32_t hi) { return n >= lo && n <= hi; }
static int sqm_free(const step_t *s) { return s->time != ST_TIE && (s->time == ST_REST || !s->n); }
/* a hit whose timing is fixed: it ties or slides into the next step, or is ratcheted already */
static int sqm_held(const step_t *p, uint32_t idx, uint32_t len)
{
    const step_t *s = &p[idx];
    return step_cont(s) || step_cont(&p[(idx + 1u) % len]) || (s->flags & SF_SLIDE) ||
           p[(idx + 1u) % len].time == ST_TIE || step_ratchet(s) != 1u;
}
/* a free step a hit may take (nothing ties into the step after it either) */
static int sqm_target(const step_t *p, uint32_t idx, uint32_t len) { return sqm_free(&p[idx]) && p[(idx + 1u) % len].time != ST_TIE; }

/* how many steps a dial touches: |d| % of max, rounded */
static uint32_t sqm_cnt(int32_t d, uint32_t max) { return ((uint32_t)(d < 0 ? -d : d) * max + 50u) / 100u; }
/* a candidate's rank: pref first (1..), then the seed's order */
static uint32_t sqm_score(uint32_t seed, uint32_t idx, uint32_t salt, uint32_t pref) { return pref << 24 | (sqm_hash(seed, idx, salt) & 0xFFFFFFu); }
/* the best-ranked candidate left (taken), len: none */
static uint32_t sqm_best(uint32_t *score, uint32_t len)
{
    uint32_t i, b = len;
    for (i = 0; i < len; i++)
        if (score[i] && (b == len || score[i] > score[b]))
            b = i;
    if (b < len)
        score[b] = 0;
    return b;
}

static volatile uint32_t sqm_thin[NSHOWN][2];            /* the steps a thinned part does not play, a bit each (seq_answer.c, main loop) */
static volatile int8_t sqm_dyn;                         /* ARC's dynamics, tarc_bright: +1 louder, -1 softer (seq_answer.c, main loop) */
static volatile uint8_t sqm_lead;                        /* the parts that listen in CALL AND ANSWER and whose chords thin to two voices, a bit each: neither a bass nor a pad (seq_answer.c, main loop) */
static volatile uint8_t sqm_arc;                         /* ARC: 0 OFF, 1 RISE, 2 DROP (tide_ui.c, the TIDE LIST; seq_arc.c) */
static volatile uint8_t tarc_dropphr = 0xFFu, tarc_lev, tarc_unit;   /* seq_arc.c's planner to the audio side: the drop's 8-bar unit; the parts thinned (bits 0-1) and no POLY (bit 2) now,
                                                          * and the same three bits for the next 8-bar unit (bits 3-5); the unit the planner last looked at. The audio side takes the next unit's from
                                                          * its first bar on (tarc_lvl), so a new phase's first downbeat plays with its levers, not the main loop's catching up (1.0.0-b8) */
#define TARC_ARMED 1
static __attribute__((noinline)) uint32_t tarc_lvl(uint32_t bar)   /* what bar plays with: the parts thinned (bits 0-1), no POLY (bit 2) */
{
    return tarc_lev >> (((bar >> 3) & 63u) != tarc_unit ? 3u : 0u) & 7u;   /* (a unit the main loop has not looked at yet: the levers armed for it) */
}
#define tarc_thin_at(bar) (tarc_lvl(bar) & 3u)
#define tarc_hold_at(bar) (tarc_lvl(bar) >> 2)
#define TARC_DROP_BAR(bar) (tarc_dropphr == (((bar) >> 3) & 63u) && ((bar) & 7u) < 2u)   /* (the drop's silent bars: its unit's first 2) */
static uint32_t tarc_sync(void);
static uint32_t glide_tide_on(void);
static int tide_bass_part(const track_t *t);
static uint32_t tarc_cap(uint32_t base);
static int32_t tarc_bright(void);
static volatile uint8_t sqm_bassm;                       /* B1: the parts whose sound is in the BASS family, a bit each (seqm_poll, main loop): no POLY, no VOIC, no phrase gaps; bits 4-5: a PAD sound's */
static volatile struct { uint8_t arc, hold, cp, cell, n[NSHOWN], src[NSHOWN][4], out[NSHOWN][4]; } sqm_v __attribute__((section(".pool")));   /* the chord bed's voicing with ARC (seq_voicelead.c):
    arc 0 as written, 1 open (PEAK), 2 closed (BREAK); hold: KEEP builds without it; an ARP SEQ part's chord as voiced (n notes, src: as played, out: voiced; the arp's list reads it) */
#define sqm_varc sqm_v.arc
#define sqm_vhold sqm_v.hold
static volatile uint8_t sqm_auto;                       /* (below: the parts in automatic WNDR / JRNY) */
static uint32_t tarc_need(void);
static uint32_t sl_cat(uint32_t k);
static uint8_t sqm_build_bass;                           /* B1: sqm_build is building a bass part's end (sqm_refresh sets it around the calls) */
static volatile uint8_t sqm_sync = 1;                   /* SYNC: 0 OFF, 1 SOFT, 2 PUSH (tide_ui.c, the TIDE LIST; seq_rhythm.c, seq_dialogue.c) */
static void sqm_refresh(uint32_t pi, int force);
#include "seq_rhythm.c"

#include "seq_voicelead.c"  /* upper voices connect while the bass stays */
#include "seq_evolve.c"      /* JAM: optional scale-degree changes carried with each end */
#include "seq_dialogue.c"    /* main-loop peer attack prediction */
#include "seq_phrase.c"      /* JAM: cumulative phrase edits, main loop only */

/* the pattern end x plays: the Original p varied by its dials (each within amt) */
static void sqm_build(const step_t *p, uint32_t len, const sqm_end_t *x, uint32_t amt, step_t *out)
{
    uint32_t i, j, k, n, hits = 0, seed = x->seed, score[NSTEP];
    int32_t d[SD_N], lane_lo, lane_hi, v;
    for (k = 0; k < SD_N; k++)
        d[k] = clamp(x->d[k], -(int32_t)amt, (int32_t)amt);
    memcpy(out, p, len * sizeof *out);
    sqm_lane(p, len, &lane_lo, &lane_hi);
    sqm_pitch_apply(p, len, x, out);
    for (i = 0; i < len; i++)
        hits += (uint32_t)sqm_hit(&p[i]);
    if (hits < 2u || len < 3u)                           /* (a drone or texture, one onset or none, is held still: R3, R7; tarc_cap does the same for the phrase edits) */
        return;
    n = sqm_cnt(d[SD_DENS], 2u);                         /* DENS */
    if (d[SD_DENS] < 0) {
        n = n < hits - 1u ? n : hits - 1u;
        for (i = 0, k = 0; i < len; i++)
            k += !!(score[i] = sqm_hit(&p[i]) && !sqm_held(p, i, len) && !rhythm_fixed(p, i) && !sqm_anchor(p, len, i) ? sqm_score(seed, i, 1u, i & 1u ? 2u : 1u) : 0u);
        n = k > 1u ? n : 0u;                             /* (MR6: a motif with one free hit at most plays whole) */
        for (k = 0; k < n && (i = sqm_best(score, len)) < len; k++)
            out[i].time = ST_REST, out[i].n = 0;
    } else if (n) {
        for (i = 0; i < len; i++)
            score[i] = sqm_target(p, i, len) && !rhythm_reserved(p, i) ? sqm_score(seed, i, 2u, i % 4u == 2u ? 3u : i & 1u ? 2u : 1u) : 0u;
        for (k = 0; k < n && (i = sqm_best(score, len)) < len; k++) {
            for (j = 1; j < len && !sqm_hit(&p[(i + len - j) % len]); j++)
                ;
            out[i] = out[(i + len - j) % len];             /* a softer copy of the hit before */
            out[i].flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE | SF_RATCH);
            out[i].vel = (uint8_t)(out[i].vel * 3u / 4u > 40u ? out[i].vel * 3u / 4u : 40u);
        }
        if (d[SD_DENS] >= 50) {                          /* .. and a ratchet */
            for (i = 0; i < len; i++)
                score[i] = i % 4u && sqm_hit(&out[i]) && out[i].n == 1u && !sqm_held(out, i, len) && !rhythm_fixed(p, i) ? sqm_score(seed, i, 3u, 1u) : 0u;
            if ((i = sqm_best(score, len)) < len)
                step_set_ratchet(&out[i], 2u);
        }
    }
    n = sqm_cnt(d[SD_SYNC], 3u);                         /* SYNC */
    if (n) {
        uint32_t dir = d[SD_SYNC] > 0 ? 1u : len - 1u;
        for (i = 0; i < len; i++)
            score[i] = sqm_hit(&out[i]) && !sqm_held(out, i, len) && !rhythm_fixed(p, i) && !sqm_anchor(p, len, i) ? sqm_score(seed, i, 4u, 1u) : 0u;
        for (k = 0; k < n && (i = sqm_best(score, len)) < len; k++) {
            uint32_t to = (i + dir) % len;
            if (!sqm_target(out, to, len) || rhythm_reserved(p, to))
                continue;
            out[to] = out[i];
            out[i].time = ST_REST, out[i].n = 0;
            score[to] = 0;
        }
    }
    n = sqm_cnt(d[SD_ACC], 3u);                          /* ACC */
    for (i = 0; i < len; i++)
        score[i] = i % 4u && sqm_hit(&out[i]) && (d[SD_ACC] > 0) == !(out[i].flags & SF_ACCENT) ? sqm_score(seed, i, 5u, i % 4u == 2u ? 2u : 1u) : 0u;
    for (k = 0; k < n && (i = sqm_best(score, len)) < len; k++)
        out[i].flags ^= SF_ACCENT;
    if (d[SD_VOIC] && !sqm_build_bass) {                 /* VOIC: every chord the same inversions; single notes an octave (B1: never on the bass) */
        if (MM_ON()) {                                  /* (the octaves of the chord's own tones: PITCH, the new notes, is not needed) */
            sqm_voicelead(p, len, out);
        } else for (i = 0; i < len; i++) {
            step_t *s = &out[i];
            uint32_t m = s->n < 4u ? s->n : 4u, lo, hi, q;
            if (!sqm_hit(s) || m < 2u)
                continue;
            for (k = sqm_cnt(d[SD_VOIC], m); k; k--) {
                for (q = 1, lo = hi = 0; q < m; q++) {
                    lo = s->note[q] < s->note[lo] ? q : lo;
                    hi = s->note[q] > s->note[hi] ? q : hi;
                }
                q = d[SD_VOIC] > 0 ? lo : hi;           /* (a move out of the lane, which lies inside 24..108, is not made) */
                v = (int32_t)s->note[q] + (d[SD_VOIC] > 0 ? 12 : -12);
                if (sqm_in_lane(v, lane_lo, lane_hi))
                    s->note[q] = (uint8_t)v;
            }
        }
        n = sqm_cnt(d[SD_VOIC], 4u);
        n = n > 1u ? n - 1u : 0u;
        for (i = 0; i < len; i++)
            score[i] = i % 4u && sqm_hit(&out[i]) && out[i].n == 1u && out[i].note[0] >= 48u && sqm_in_lane((int32_t)out[i].note[0] + (d[SD_VOIC] > 0 ? 12 : -12), lane_lo, lane_hi) ? sqm_score(seed, i, 6u, 1u) : 0u;
        for (k = 0; k < n && (i = sqm_best(score, len)) < len; k++)
            out[i].note[0] = (uint8_t)(out[i].note[0] + (d[SD_VOIC] > 0 ? 12 : -12));
    }
}

/* a fresh end: each dial at half to all of amt, either way */
static void sqm_rand(sqm_end_t *x, uint32_t seed, uint32_t amt)
{
    uint32_t r = seed * 2654435761u + 1u, k;
    memset(x->pitch, 0, sizeof x->pitch);
    x->seed = (uint16_t)(seed ? seed : 1u);
    for (k = 0; k < SD_N; k++) {
        int32_t m;
        r = r * 1103515245u + 12345u;
        m = (int32_t)amt * (50 + (int32_t)((r >> 16) % 51u)) / 100;
        x->d[k] = (int8_t)((r >> 30) & 1u ? m : -m);
    }
}

static uint32_t sqm_len(const track_t *t) { return (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }
static uint32_t sqm_sig(const track_t *t, const sqm_part_t *q, uint32_t len)
{
    const sqm_end_t *e[2] = {&q->a, &q->b[q->top]};
    uint32_t h = fnv(2166136261u, t->step, len * (uint32_t)sizeof(step_t)), k;
    _Static_assert(__builtin_offsetof(sqm_end_t, pitch) == __builtin_offsetof(sqm_end_t, d) + SD_N, "sqm_sig: the dials and the pitch bytes in one run");
    for (k = 0; k < 2u; k++)
        h = fnv((h ^ e[k]->seed) * 16777619u, e[k]->d, SD_N + SQM_PITCH_BYTES);
    h = (h ^ (uint32_t)scale_g ^ ((uint32_t)root_g << 8) ^ ((morph.cfg & MC_PIT) << 16) ^ ((uint32_t)MM_ON() << 24)) * 16777619u;
    return (h ^ (len | (uint32_t)q->aon << 7 | (uint32_t)SQM_AMT(q) << 8)) * 16777619u;
}
/* the ends of part pi as they play now (main loop): built again when anything they depend on changed (force: always) */
/* WNDR+BOTH on FM6 (glide_shape_follow): a hit holds into the free cells after it, span - 1 of them, as 1.1's ties (a whole-chord
 * hold: ST_TIE keeps every voice the sequence owns). A tie is a plain cell at normal chance: a chance marker on it would release the
 * chord (seq_step checks the chance first), and a tie after a missed onset sounds nothing (no voice to keep). Free cells only */
static void sqm_shape_hold(step_t *p, uint32_t len, uint32_t span)
{
    if (span < 2u) return;
    for (uint32_t i = 0; i < len; i++) {
        if (!sqm_hit(&p[i])) continue;
        for (uint32_t n = 1; n < span && n < len; n++) {
            step_t *s = &p[(i + n) % len];
            if (!sqm_free(s)) break;
            s->time = ST_TIE;
            s->n = 0;
            s->flags = 0;
            s->vel = 0;
            s->probability = 0;                          /* (0: the normal 100 %) */
        }
    }
}
static void sqm_refresh(uint32_t pi, int force)
{
    sqm_part_t *q = &sqm[pi];
    const track_t *t = &trk[pi];
    uint32_t len = sqm_len(t), sig, src = rhythm.source[pi];
    if (!q->n) {                                         /* the part's default End B, the first entry of the ring */
        sqm_rand(&q->b[0], 0x5EEDu + 0x2F1u * pi, SQM_AMT(q));
        q->n = 1, q->top = 0;
    }
    rhythm_validate(pi, len);
    if (rhythm.source[pi] != src)                        /* another pattern: the pitch offsets belong to the steps before */
        sqm_pitch_clear(q);
    uint32_t vm = sqm_vmode(pi, t->step, len);
    if (vm && !q->aon) {                                 /* (ARC's voicing plays on End A too: a generation of the Original's own dials, as a phrase pitch edit starts one) */
        memset(&q->a, 0, sizeof q->a);
        q->a.seed = q->b[q->top].seed;
        q->aon = 1;
    }
    sig = sqm_sig(t, q, len) ^ (uint32_t)(q->hold | ((sqm_bassm >> pi) & 1u) << 7 | vm << 8 | (uint32_t)sqm_sync << 10 | ((sqm_v.cp >> pi) & 1u) << 12 | ((sqm_auto >> pi) & 1u) << 13) * 0x85EBCA77u ^ rhythm_hash(pi);
    if (!force && q->ok && sig == q->sig)
        return;
    sqm_build_bass = (uint8_t)((sqm_bassm >> pi) & 1u);
    if (q->aon)
        sqm_build(t->step, len, &q->a, SQM_AMT(q), sqm_buf);
    else
        memcpy(sqm_buf, t->step, len * sizeof sqm_buf[0]);
    rhythm_apply(pi, len, sqm_buf);
    sqm_breathe(t->step, len, vm, sqm_buf);
    sqm_shape_hold(sqm_buf, len, q->hold);
    MORPH_LOCK();
    memcpy(sqm_pat[pi][0], sqm_buf, len * sizeof sqm_buf[0]);
    MORPH_UNLOCK();
    sqm_build(t->step, len, &q->b[q->top], SQM_AMT(q), sqm_buf);
    sqm_build_bass = 0;
    rhythm_apply(pi, len, sqm_buf);
    vm = (uint32_t)sqm_cell(pi, t->step, len, q->b[q->top].seed, sqm_buf) << 8 | vm;   /* (the variation bar's rhythm: a cell of the chord vocabulary, seq_phrase.c) */
    sqm_breathe(t->step, len, vm & 3u, sqm_buf);
    sqm_shape_hold(sqm_buf, len, q->hold);
    MORPH_LOCK();
    memcpy(sqm_pat[pi][1], sqm_buf, len * sizeof sqm_buf[0]);
    sqm_v.cell = (uint8_t)((sqm_v.cell & ~(1u << pi)) | (vm >> 8) << pi);
    q->sig = sig;
    q->tseed = q->b[q->top].seed * 0x9E3779B1u ^ q->a.seed;
    q->ok = 1;
    MORPH_UNLOCK();
}

static volatile uint8_t sqm_vary_all;                   /* VARY BARS: 0 EVERY 4TH, 1 EVERY BAR (tide_ui.c, the TIDE LIST) */
static volatile uint8_t sqm_poly = 1;                   /* POLY: 0 OFF, 1 GENTLE, 2 WIDE (tide_ui.c, the TIDE LIST; seq_poly.c) */
static volatile uint8_t poly_len[NSHOWN];                /* POLY (seq_poly.c): the loop's length now, 0: the stored one (audio path writes; a part in a stretch is never thinned) */
static const step_t SQM_REST = {{0}, 0, ST_REST, 0, 0, 0};
static volatile uint8_t sqm_restart;                     /* tide_loaded put the loaded parts at A: glide_poll starts the running trip again from there */
static volatile uint8_t sqm_auto;                       /* the parts in automatic MUSIC WNDR/JRNY, a bit each (glide_poll, main loop) */
/* the step part t plays at idx with the variation heard (vary 1; 0: End A): the stored one while End A is the Original and the
 * fader at 0, else the end its beat is on. The UI's view and KEEP (vary 1 always: the variation as it plays on a varied bar) */
static const step_t *sqm_step(const track_t *t, uint32_t idx, uint32_t vary)
{
    uint32_t pi = (uint32_t)(t - trk), len = sqm_len(t);
    const sqm_part_t *q;
    if (pi >= NSHOWN || idx >= len || !(q = &sqm[pi])->ok || (!q->fader && !q->aon && !q->hold && !(rhythm.active & (1u << pi))))
        return &t->step[idx];
    if (!vary)
        return q->aon || q->hold || (rhythm.active & (1u << pi)) ? &sqm_pat[pi][0][idx] : &t->step[idx];
    return &sqm_pat[pi][((sqm_v.cell >> pi) & 1u) || q->fader >= 1u + sqm_hash(q->tseed, idx / 4u, 9u) % 127u][idx];   /* (a chord part's rhythm cell: the bar whole) */
}
/* CALL AND ANSWER (no setting; replaces TURNS, whose stored value is ignored): with both parts in automatic WNDR / JRNY, part pi listens
 * in this bar: it drops to its anchors and beats, a chord to two voices, a step softer, while the other speaks. Per 4 bars: bars 1-3 one
 * part then the other (who opens: by the bar and the seed, so it swaps every 4 bars, now and then not), bar 4 both play (their variation
 * bar). A bass or a pad never listens (sqm_lead); against one, the other listens on bar 3 of 4 alone. Never in ARC's breakdown or
 * RECOVER's thinned half (tarc_lvl: the arc thins), so the two parts are never both thinned outside it */
static int sqm_listens(uint32_t pi, uint32_t bar)
{
    uint32_t q = bar & 3u;
    if (sqm_auto != 3u || tarc_thin_at(bar) || !((sqm_lead >> pi) & 1u) || q == 3u)
        return 0;
    if (sqm_lead != 3u)
        return q == 2u;
    return pi == ((q ^ (bar >> 2) ^ sqm_hash((uint32_t)morph.seed, bar >> 3, 0x4Cu)) & 1u);
}
/* part pi plays its thinned End A in this bar: ARC's breakdown (tarc_lvl), a listening bar */
static int sqm_thinned(uint32_t pi, uint32_t bar) { return ((tarc_thin_at(bar) >> pi) & 1u) || sqm_listens(pi, bar); }
/* the step the sequencer plays (audio interrupt): with VARY BARS EVERY 4TH, while playing, End A but on every 4th bar */
static const step_t *seq_play_step_at(const track_t *t, uint32_t idx, uint32_t bar)
{
    uint32_t pi = (uint32_t)(t - trk);
    const step_t *s = sqm_step(t, idx, sqm_vary_all || !song.playing || (bar & 3u) == 3u);
    /* a thinned bar (sqm_thinned) plays End A's mask: onsets only, a tie after one stays silent */
    if (pi < NSHOWN && song.playing && s->time == ST_NOTE) {
        /* ARC: the drop's first 2 bars, both parts in automatic WNDR / JRNY start nothing (ties and tails ring) */
        if (TARC_DROP_BAR(bar) && ((sqm_auto >> pi) & 1u))
            return &SQM_REST;
        if (idx < NSTEP && !poly_len[pi] && ((sqm_thin[pi][idx >> 5] >> (idx & 31u)) & 1u) && sqm_thinned(pi, bar))
            return &SQM_REST;
    }
    return s;
}
/* the bar part t's step plays in now (audio path): an ARP SEQ step's from the bar's last 32nd on is the next bar's (its clock runs a little
 * ahead of the beat's: a downbeat would get the bar before's thinning and dynamics) */
static uint32_t sqm_bar_of(const track_t *t) { return (uint32_t)sqm_bar + (arp_mode(t) == AM_SEQ_ID ? sqm_bar_soon : sqm_bar_turn); }
/* MR4: a thinned chord part (sqm_lead) plays two voices of its chord, the lowest and the top: the count .. */
static uint32_t sqm_voices(uint32_t pi, uint32_t n, uint32_t bar)
{
    return n > 2u && pi < NSHOWN && ((sqm_lead >> pi) & 1u) && song.playing && sqm_thinned(pi, bar) ? 2u : n;
}
/* .. and the notes (n in out, sorted or not; the count back) */
static uint32_t sqm_two(uint32_t pi, uint8_t *out, uint32_t n, uint32_t bar)
{
    uint32_t i, lo = 0, hi = 0;
    if (sqm_voices(pi, n, bar) == n)
        return n;
    for (i = 1; i < n; i++)
        lo = out[i] < out[lo] ? i : lo, hi = out[i] > out[hi] ? i : hi;
    i = out[hi], out[0] = out[lo], out[1] = (uint8_t)i;
    return 2u;
}
/* .. in the bar now (the audio interrupt), with the dynamics (D4, M08; no setting): a part in automatic WNDR / JRNY plays its hits an
 * 8th louder in ARC's late rise and peak, two steps softer in the breakdown (MR4), a step softer on its listening bars (an accent counts as 127). buf
 * holds the shaped copy; KEEP and the TIDE page read the steps themselves (the page asks for the bar each step plays in: tide_ui.c) */
static const uint8_t SQM_VS[4] = {72, 96, 0, 144};      /* (x/128: softer twice, softer, -, louder) */
static int32_t sqm_dyn_of(uint32_t pi, uint32_t bar)     /* the dynamics now: -2 .. +1, 0 none (ARC OFF, not automatic, stopped) */
{
    int32_t b;
    if (pi >= NSHOWN || !song.playing || !((sqm_auto >> pi) & 1u))
        return 0;
    b = sqm_dyn - sqm_listens(pi, bar);
    return b < -2 ? -2 : b;
}
static uint32_t sqm_dyn_scale(uint32_t v, int32_t b) { v = v * SQM_VS[b + 2] >> 7; return v > 127u ? 127u : v; }
static const step_t *seq_play_step(const track_t *t, uint32_t idx, step_t *buf)
{
    uint32_t pi = (uint32_t)(t - trk), bar = sqm_bar_of(t);
    const step_t *s = seq_play_step_at(t, idx, bar);
    int32_t b;
    uint8_t n;
    if (!sqm_hit(s) || arp_mode(t) == AM_SEQ_ID)
        return s;                                        /* (D1: an ARP SEQ part keeps its accent; the arp's velocity is scaled in tide_arp_out, its chord in tide_arp_out) */
    b = sqm_dyn_of(pi, bar);
    *buf = *s;
    n = (uint8_t)sqm_two(pi, buf->note, s->n, bar);
    if (!b && n == s->n)
        return s;
    buf->n = n;
    if (b) {
        buf->vel = (uint8_t)sqm_dyn_scale(s->flags & SF_ACCENT ? 127u : s->vel ? s->vel : 96u, b);
        buf->flags &= (uint8_t)~SF_ACCENT;
    }
    return buf;
}
/* TIDE hook (seq.c arp_seq_notes, audio path, a call per arp step): an ARP SEQ step's velocity (the arp's, an accent's 127) with the same
 * dynamics (D1), and its chord's voices (MR4: two while thinned) */
static __attribute__((noinline)) uint32_t tide_arp_out(const track_t *t, uint8_t *out, uint32_t m, uint32_t *vel)
{
    uint32_t pi = trk_index(t), bar = sqm_bar_of(t);
    int32_t b = sqm_dyn_of(pi, bar);
    if (b)
        *vel = sqm_dyn_scale(*vel, b);
    return sqm_two(pi, out, m, bar);
}

/* ------------------------------------------------------------------ the controls (main loop) --- */
static uint32_t seqm_fader_of(uint32_t pi) { return pi < NSHOWN ? sqm[pi].fader : 0u; }
static uint32_t seqm_notes_of(uint32_t pi) { return pi < NSHOWN ? SQM_AMT(&sqm[pi]) : 0u; }
static int seqm_active(uint32_t pi) { return pi < NSHOWN && (sqm[pi].fader || sqm[pi].aon || (rhythm.active & (1u << pi))); }

static void seqm_set_fader(uint32_t pi, uint32_t v)
{
    if (pi >= NSHOWN)
        return;
    sqm_refresh(pi, 0);                                  /* (the signature check: ends built before an edit are rebuilt now, not up to 20 ms on; B04) */
    sqm[pi].fader = (uint8_t)(v > 127u ? 127u : v);
}
static void seqm_set_amount(uint32_t pi, uint32_t amt)   /* SEQ AMT */
{
    if (pi >= NSHOWN)
        return;
    sqm[pi].amt_x = (uint8_t)((amt > 100u ? 100u : amt) ^ SQM_AMT_DEF);
    sqm_refresh(pi, 1);
}
/* main loop, every pass: the ends built again when the pattern was edited (every 20 ms at most) */
static void sqm_bass_scan(void)                         /* B1: sqm_bassm from the parts' sounds (main loop); the PAD sounds too (CAT_PAD, category.c: included later) */
{
    uint32_t k, b = 0;
    for (k = 0; k < NSHOWN; k++) {
        const track_t *t = &trk[k];
        b |= (uint32_t)tide_bass_part(t) << k | (uint32_t)((UP_REC_USER(t) ? sl_cat((uint32_t)t->user - 1u) : ds_cat_of(t->eng_req, t->preset)) == 3u) << (k + 4u);
    }
    sqm_bassm = (uint8_t)b;
    sqm_varc = (uint8_t)(!sqm_arc || !sqm_auto || song.rec ? 0u : sqm_varc_now());   /* (it follows ARC's phase, which changes on ARC's grid: 8 bars, MR4) */
}
static void seqm_poll(void)
{
    uint32_t k;
    if ((uint32_t)(fm1_ms - sqm_poll_ms) < 20u)
        return;
    sqm_poll_ms = fm1_ms;
    sqm_bass_scan();
    for (k = 0; k < NSHOWN; k++) {
        sqm_varp(k);
        if (seqm_active(k) || sqm_varc)                  /* (ARC's voicing: the ends of a part still on the Original are built too) */
            sqm_refresh(k, 0);
    }
}

/* a new End B; returns its seed */
static uint32_t seqm_roll(uint32_t pi)
{
    sqm_part_t *q;
    uint32_t seed = morph_new_seed();
    if (pi >= NSHOWN)
        return 0;
    q = &sqm[pi];
    sqm_refresh(pi, 0);                                  /* (the default entry first) */
    q->top = (uint8_t)((q->top + 1u) % SQM_HIST);
    sqm_rand(&q->b[q->top], seed, SQM_AMT(q));
    q->n = (uint8_t)(q->n < SQM_HIST ? q->n + 1u : SQM_HIST);
    sqm_refresh(pi, 1);
    return seed;
}
/* WANDER (glide_arrive): end x becomes the next generation, its dials a step on from the other end's (the end reached; the
 * Original's are 0), each its own way, turning now and then and at SEQ AMT's edges; the step grows with the phrase tier (0 ..
 * 4: 16 .. 128 bars), and from 64 bars (sometimes at 32) other steps are taken (a new seed). JOURNEY's pattern side too: each part
 * draws its own seed, so two parts never play the same generation */
static void seqm_wander_gen(uint32_t pi, uint32_t x, uint32_t tier)
{
    static const uint8_t TF[5] = {3, 4, 6, 8, 12};       /* the step, 16ths of SEQ AMT */
    sqm_part_t *q;
    sqm_end_t nx;
    uint32_t k, r = morph_new_seed() * 2654435761u + 1u;
    int32_t amt, step;
    if (pi >= NSHOWN)
        return;
    q = &sqm[pi];
    sqm_refresh(pi, 0);
    tier = tier > 4u ? 4u : tier;
    amt = SQM_AMT(q);
    step = 1 + amt * TF[tier] / 16;
    if (!x)
        nx = q->b[q->top];
    else if (q->aon)
        nx = q->a;
    else {
        memset(&nx, 0, sizeof nx);
        nx.seed = q->b[q->top].seed;
    }
    for (k = 0; k < SD_N; k++) {
        if (k == SD_SYNC && MM_ON()) { nx.d[k] = 0; continue; }
        r = r * 1103515245u + 12345u;
        if (!q->dir[k] || (r >> 16) % 6u == 0u)
            q->dir[k] = (int8_t)((r >> 30) & 1u ? 1 : -1);
        nx.d[k] = (int8_t)tide_walk(nx.d[k], &q->dir[k], q->dir[k], step, amt);   /* (morph_music.c: the sound's dials walk the same way) */
    }
    sqm_pitch_mutate(&trk[pi], &nx, r, tier, SQM_AMT(q));
    if (tier >= 3u || (tier == 2u && (r >> 8) & 1u))
        nx.seed = (uint16_t)(morph_new_seed() | 1u);
    if (x) {
        q->top = (uint8_t)((q->top + 1u) % SQM_HIST);
        q->b[q->top] = nx;
        q->n = (uint8_t)(q->n < SQM_HIST ? q->n + 1u : SQM_HIST);
    } else {
        q->a = nx;
        q->aon = 1;
    }
    sqm_refresh(pi, 1);
}

/* HOME: End A the Original again, the fader at 0 */
static void seqm_home(uint32_t pi)
{
    if (pi >= NSHOWN)
        return;
    rhythm_reset(pi);
    sqm_pitch_clear(&sqm[pi]);
    sqm[pi].aon = 0;
    sqm[pi].fader = 0;
    sqm_refresh(pi, 1);
}

/* KEEP: the variation as it plays (on a varied bar, VARY BARS EVERY 4TH) becomes the stored pattern (the caller takes the undo copy
 * first); End A the Original (the new one), the fader at 0, and End B a variation of the new Original. 0 = nothing to keep */
static int seqm_keep(track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk), len = sqm_len(t), i;
    if (!seqm_active(pi))
        return 0;
    sqm_vhold = 1;                                       /* (KEEP stores the variation, not ARC's voicing of the moment) */
    sqm_refresh(pi, 1);
    for (i = 0; i < len; i++)
        sqm_buf[i] = *sqm_step(t, i, 1u);
    sqm_vhold = 0;
    MORPH_LOCK();
    memcpy(t->step, sqm_buf, len * sizeof t->step[0]);
    sqm[pi].fader = 0;
    sqm[pi].aon = 0;
    sqm_pitch_clear(&sqm[pi]);
    rhythm_reset(pi);
    MORPH_UNLOCK();
    sqm_refresh(pi, 1);
    return 1;
}
