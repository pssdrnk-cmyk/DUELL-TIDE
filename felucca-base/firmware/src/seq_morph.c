/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): SEQ MORPH v2, the dial model. Each shown part's pattern has two ends, A and B, and a fader between them. An end is the
 * stored pattern (the Original) varied by four dials, -100..100 (each held within SEQ AMT), and a seed that ranks which steps
 * they touch:
 *   DENS   + hits added on free steps (a softer copy of the hit before; the offbeats first), from +50 a ratchet; - hits dropped
 *   SYNC   hits shifted a 16th later (+) or earlier (-)
 *   ACC    accents added (+) or taken off (-)
 *   VOIC   chords inverted up (+) or down (-), an octave at most; single notes: from 50, a few an octave up or down
 * No dial moves a note to another pitch class: a random note cannot know the harmony.
 * The landmarks (the downbeats: steps 1 5 9 13 ..) are never dropped or moved; a held hit (tie, slide, ratchet) keeps its
 * timing; a pattern keeps at least one hit, and at most two are added or dropped. An end is computed from the Original every time,
 * never from another end, so nothing accumulates (v1 kept each variation and varied it again: in 32 generations the stabs became
 * quiet ghosts spread over three octaves).
 * End A is the Original until a WANDER or JOURNEY glide makes it a generation: each arrival rolls the end left as the next one, its
 * dials a step on from the end reached (momentum, the step by the phrase tier), so every trip goes from one generation to the
 * next. The fader brings End B in a beat at a time (each beat its own threshold): monotone and reversible.
 * VARY BARS (TIDE LIST): 4 + 8 (the default) plays the variation only on every 4th bar since PLAY (bars 4 and 8 of each 8-bar
 * phrase; the others play End A), as a dub techno loop holds and changes at the turnaround (after DUB-1, sbegueria/fm1-dub);
 * ALL plays it on every bar. A bar is 4 beats of the clock (seq.c sqm_bar), whatever the pattern's length or DIV.
 * Contexts: the ends are built in the main loop into sqm_pat (seqm_poll, and every control); seq_play_step() runs in the audio
 * interrupt and only reads them. Nothing is stored: a power cycle starts with the fader at 0 and End A the Original. */
/* MUSIC WNDR/JRNY also carries the cumulative phrase overlay (seq_rhythm.c,
 * seq_phrase.c): retained routing, repeats/rests and phrase-level pitch edits.
 * These are session state until KEEP commits the reached ordinary steps. */
#define SQM_HIST 4u                                      /* rolls UNDO can go back over */
enum { SD_DENS, SD_SYNC, SD_ACC, SD_VOIC, SD_N };
#define SQM_PITCH_BYTES ((NSTEP * 5u + 7u) / 8u)
typedef struct { uint16_t seed; int8_t d[SD_N]; uint8_t pitch[SQM_PITCH_BYTES]; } sqm_end_t;
typedef struct {
    sqm_end_t b[SQM_HIST];                               /* a ring: b[top] is End B */
    sqm_end_t a;                                         /* End A while aon (a WANDER / JOURNEY generation), else the Original */
    uint8_t aon, top, n;                                 /* n: rolls kept (0: none yet, the part's default comes first) */
    uint8_t fader;                                       /* 0 End A .. 127 End B */
    uint8_t notes;                                       /* SEQ AMT 0..100 (no knob: SQM_AMT_DEF; tests set it) */
    int8_t dir[SD_N];                                    /* WANDER: each dial's way */
    uint8_t ok;                                          /* sqm_pat holds the ends of sig */
    uint8_t hold;                                        /* WNDR+BOTH: minimum note span of the heard FM6 shape */
    uint32_t sig, tseed;                                 /* .. its key; the beats' thresholds */
} sqm_part_t;
#define SQM_AMT_DEF 50u                                  /* SEQ AMT (40 left most generations alike) */
static sqm_part_t sqm[NSHOWN] = { {.notes = SQM_AMT_DEF}, {.notes = SQM_AMT_DEF} };
_Static_assert(NSHOWN == 2, "seq_morph.c: one initialiser per shown part");
static step_t sqm_pat[NSHOWN][2][NSTEP];               /* the two ends as played (.bss: the audio interrupt reads it) */
static step_t sqm_buf[NSTEP] __attribute__((section(".pool")));   /* an end being built, KEEP's copy (main loop; the pool: .bss is the tighter, plan §4.9.2) */
static uint32_t sqm_poll_ms;
static uint32_t sqm_len(const track_t *t);

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

static uint8_t sqm_note(int32_t n) { return (uint8_t)(n < 24 ? n + 12 * ((24 - n + 11) / 12) : n > 108 ? n - 12 * ((n - 108 + 11) / 12) : n); }

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
    int32_t d[SD_N];
    for (k = 0; k < SD_N; k++)
        d[k] = clamp(x->d[k], -(int32_t)amt, (int32_t)amt);
    memcpy(out, p, len * sizeof *out);
    sqm_pitch_apply(p, len, x, out);
    for (i = 0; i < len; i++)
        hits += (uint32_t)sqm_hit(&p[i]);
    if (!hits || len < 3u)
        return;
    n = sqm_cnt(d[SD_DENS], 2u);                         /* DENS */
    if (d[SD_DENS] < 0) {
        n = n < hits - 1u ? n : hits - 1u;
        for (i = 0; i < len; i++)
            score[i] = i % 4u && sqm_hit(&p[i]) && !sqm_held(p, i, len) && !rhythm_fixed(p, i) ? sqm_score(seed, i, 1u, i & 1u ? 2u : 1u) : 0u;
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
            score[i] = i % 4u && sqm_hit(&out[i]) && !sqm_held(out, i, len) && !rhythm_fixed(p, i) ? sqm_score(seed, i, 4u, 1u) : 0u;
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
    if (d[SD_VOIC]) {                                    /* VOIC: every chord the same inversions; single notes an octave */
        if (morph_musical && (morph.cfg & MC_PIT)) {
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
                if (d[SD_VOIC] > 0)
                    s->note[lo] = sqm_note(s->note[lo] + 12);
                else
                    s->note[hi] = sqm_note(s->note[hi] - 12);
            }
        }
        n = sqm_cnt(d[SD_VOIC], 4u);
        n = n > 1u ? n - 1u : 0u;
        for (i = 0; i < len; i++)
            score[i] = i % 4u && sqm_hit(&out[i]) && out[i].n == 1u ? sqm_score(seed, i, 6u, 1u) : 0u;
        for (k = 0; k < n && (i = sqm_best(score, len)) < len; k++)
            out[i].note[0] = sqm_note(out[i].note[0] + (d[SD_VOIC] > 0 ? 12 : -12));
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
    const uint8_t *b = (const uint8_t *)t->step;
    const sqm_end_t *e[2] = {&q->a, &q->b[q->top]};
    uint32_t h = 2166136261u, i, k;
    for (i = 0; i < len * (uint32_t)sizeof(step_t); i++)
        h = (h ^ b[i]) * 16777619u;
    for (k = 0; k < 2u; k++) {
        h = (h ^ e[k]->seed) * 16777619u;
        for (i = 0; i < SD_N; i++)
            h = (h ^ (uint8_t)e[k]->d[i]) * 16777619u;
        for (i = 0; i < SQM_PITCH_BYTES; i++) h = (h ^ e[k]->pitch[i]) * 16777619u;
    }
    h = (h ^ (uint32_t)scale_g ^ ((uint32_t)root_g << 8) ^ ((morph.cfg & MC_PIT) << 16) ^ ((uint32_t)morph_musical << 24)) * 16777619u;
    return (h ^ (len | (uint32_t)q->aon << 7 | (uint32_t)q->notes << 8)) * 16777619u;
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
    uint32_t len = sqm_len(t), sig;
    if (!q->n) {                                         /* the part's default End B, the first entry UNDO can go back to */
        sqm_rand(&q->b[0], 0x5EEDu + 0x2F1u * pi, q->notes);
        q->n = 1, q->top = 0;
    }
    rhythm_validate(pi, len);
    sig = sqm_sig(t, q, len) ^ (uint32_t)q->hold * 0x85EBCA77u ^ rhythm_hash(pi);
    if (!force && q->ok && sig == q->sig)
        return;
    if (q->aon)
        sqm_build(t->step, len, &q->a, q->notes, sqm_buf);
    else
        memcpy(sqm_buf, t->step, len * sizeof sqm_buf[0]);
    rhythm_apply(pi, len, sqm_buf);
    sqm_shape_hold(sqm_buf, len, q->hold);
    MORPH_LOCK();
    memcpy(sqm_pat[pi][0], sqm_buf, len * sizeof sqm_buf[0]);
    MORPH_UNLOCK();
    sqm_build(t->step, len, &q->b[q->top], q->notes, sqm_buf);
    rhythm_apply(pi, len, sqm_buf);
    sqm_shape_hold(sqm_buf, len, q->hold);
    MORPH_LOCK();
    memcpy(sqm_pat[pi][1], sqm_buf, len * sizeof sqm_buf[0]);
    q->sig = sig;
    q->tseed = q->b[q->top].seed * 0x9E3779B1u ^ q->a.seed;
    q->ok = 1;
    MORPH_UNLOCK();
}

static volatile uint8_t sqm_vary_all;                   /* VARY BARS: 0 4 + 8, 1 ALL (tide_ui.c, the TIDE LIST) */
static volatile uint8_t sqm_turns = 1;                  /* TURNS: 0 TOGETHER, 1 TAKE TURNS (tide_ui.c, the TIDE LIST) */
static volatile uint8_t sqm_poly = 1;                   /* POLY: 0 OFF, 1 GENTLE, 2 WIDE (tide_ui.c, the TIDE LIST; seq_poly.c) */
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
    return &sqm_pat[pi][q->fader >= 1u + sqm_hash(q->tseed, idx / 4u, 9u) % 127u][idx];
}
/* the step the sequencer plays (audio interrupt): with VARY BARS 4 + 8, while playing, End A but on every 4th bar (TAKE TURNS, both parts in automatic WNDR / JRNY: part 2 on bars 2 and 6) */
static const step_t *seq_play_step(const track_t *t, uint32_t idx, step_t *buf)
{
    uint32_t off = sqm_turns && sqm_auto == 3u && t == &trk[1];   /* TAKE TURNS: part 2 two bars on, in the shared bars (bars 2 and 6) */
    (void)buf;
    return sqm_step(t, idx, sqm_vary_all || !song.playing || ((sqm_bar + sqm_bar_turn + 2u * off) & 3u) == 3u);
}

/* ------------------------------------------------------------------ the controls (main loop) --- */
static uint32_t seqm_fader_of(uint32_t pi) { return pi < NSHOWN ? sqm[pi].fader : 0u; }
static uint32_t seqm_notes_of(uint32_t pi) { return pi < NSHOWN ? sqm[pi].notes : 0u; }
static uint32_t seqm_hist_of(uint32_t pi) { return pi < NSHOWN && sqm[pi].n > 1u ? sqm[pi].n - 1u : 0u; }
static int seqm_active(uint32_t pi) { return pi < NSHOWN && (sqm[pi].fader || sqm[pi].aon || (rhythm.active & (1u << pi))); }

static void seqm_set_fader(uint32_t pi, uint32_t v)
{
    if (pi >= NSHOWN)
        return;
    if (!sqm[pi].ok)
        sqm_refresh(pi, 1);
    sqm[pi].fader = (uint8_t)(v > 127u ? 127u : v);
}
static void seqm_set_amount(uint32_t pi, uint32_t amt)   /* SEQ AMT */
{
    if (pi >= NSHOWN)
        return;
    sqm[pi].notes = (uint8_t)(amt > 100u ? 100u : amt);
    sqm_refresh(pi, 1);
}
/* main loop, every pass: the ends built again when the pattern was edited (every 20 ms at most) */
static void seqm_poll(void)
{
    uint32_t k;
    if ((uint32_t)(fm1_ms - sqm_poll_ms) < 20u)
        return;
    sqm_poll_ms = fm1_ms;
    for (k = 0; k < NSHOWN; k++)
        if (seqm_active(k))
            sqm_refresh(k, 0);
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
    sqm_rand(&q->b[q->top], seed, q->notes);
    q->n = (uint8_t)(q->n < SQM_HIST ? q->n + 1u : SQM_HIST);
    sqm_refresh(pi, 1);
    return seed;
}
/* back to the roll before; 0 = none left */
static int seqm_undo(uint32_t pi)
{
    sqm_part_t *q;
    if (pi >= NSHOWN || sqm[pi].n < 2u)
        return 0;
    q = &sqm[pi];
    q->top = (uint8_t)((q->top + SQM_HIST - 1u) % SQM_HIST);
    q->n--;
    sqm_refresh(pi, 1);
    return 1;
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
    int32_t amt, step, v;
    if (pi >= NSHOWN)
        return;
    q = &sqm[pi];
    sqm_refresh(pi, 0);
    tier = tier > 4u ? 4u : tier;
    amt = q->notes;
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
        if (k == SD_SYNC && morph_musical) { nx.d[k] = 0; continue; }
        r = r * 1103515245u + 12345u;
        if (!q->dir[k] || (r >> 16) % 6u == 0u)
            q->dir[k] = (int8_t)((r >> 30) & 1u ? 1 : -1);
        v = nx.d[k] + q->dir[k] * step;
        if (v > amt)
            v = amt, q->dir[k] = -1;
        else if (v < -amt)
            v = -amt, q->dir[k] = 1;
        nx.d[k] = (int8_t)v;
    }
    sqm_pitch_mutate(&trk[pi], &nx, r, tier, q->notes);
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
    sqm[pi].aon = 0;
    sqm[pi].fader = 0;
    sqm_refresh(pi, 1);
}

/* KEEP: the variation as it plays (on a varied bar, VARY BARS 4 + 8) becomes the stored pattern (the caller takes the undo copy
 * first); End A the Original (the new one), the fader at 0, and End B a variation of the new Original. 0 = nothing to keep */
static int seqm_keep(track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk), len = sqm_len(t), i;
    if (!seqm_active(pi))
        return 0;
    sqm_refresh(pi, 0);
    for (i = 0; i < len; i++)
        sqm_buf[i] = *sqm_step(t, i, 1u);
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
