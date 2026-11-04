/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE (JAM before): cumulative scale-note changes. Generation rolls remain rare; the phrase
 * lifecycle gives the same walk regular opportunities. One upper chord tone (or a melody note
 * away from the downbeat), at most an octave from its stored note. No pull home. Nothing under C3 walks (the bass), and on SCALE CHR
 * nothing does (every pitch class would be allowed).
 * Built only in the main loop; the audio ISR gets ordinary finished steps. */
/* Five bits retain the full +/-12 semitone range without 64-byte maps. */
static int32_t sqm_pitch_get(const sqm_end_t *x, uint32_t i)
{
    uint32_t bit = i * 5u, at = bit / 8u, v = x->pitch[at];
    if (at + 1u < SQM_PITCH_BYTES) v |= (uint32_t)x->pitch[at + 1u] << 8;
    return (int32_t)(((v >> (bit & 7u)) & 31u) ^ 16u) - 16;
}
static void sqm_pitch_set(sqm_end_t *x, uint32_t i, int32_t value)
{
    uint32_t bit = i * 5u, at = bit / 8u, shift = bit & 7u, v = x->pitch[at];
    if (at + 1u < SQM_PITCH_BYTES) v |= (uint32_t)x->pitch[at + 1u] << 8;
    v = (v & ~(31u << shift)) | (((uint32_t)value & 31u) << shift);
    x->pitch[at] = (uint8_t)v;
    if (at + 1u < SQM_PITCH_BYTES) x->pitch[at + 1u] = (uint8_t)(v >> 8);
}
static uint32_t sqm_pitch_voice(const step_t *s, uint32_t idx)
{
    uint32_t hi = 0, lo = 0, i;
    if (!sqm_hit(s) || s->n > 4u || (!idx && s->n == 1u)) return 4u;
    for (i = 1; i < s->n; i++) {
        if (s->note[i] > s->note[hi]) hi = i;
        if (s->note[i] < s->note[lo]) lo = i;
    }
    return (step_cont(s) & (1u << hi)) || (s->n > 1u && hi == lo) || s->note[hi] < 48u ? 4u : hi;   /* (nothing under C3: the bass never walks) */
}
static int sqm_pitch_ok(const track_t *t, const step_t *s, uint32_t k, int32_t n)
{
    uint32_t i;
    if (n < 24 || n > 108 || n < (int32_t)s->note[k] - 12 || n > (int32_t)s->note[k] + 12) return 0;
    if (!((scale_mask(t) >> (uint32_t)((n - root_g + 240) % 12)) & 1u)) return 0;
    for (i = 0; i < s->n; i++) if (i != k && note_snap_to(t, s->note[i], 1u) == n) return 0;
    return 1;
}
static int sqm_pitch_chain_ok(const track_t *t, const step_t *p, uint32_t len, uint32_t at, uint32_t k, int32_t n)
{
    uint32_t raw = p[at].note[k];
    if (!sqm_pitch_ok(t, &p[at], k, n)) return 0;
    for (uint32_t d = 1; d < len; d++) {
        const step_t *st = &p[(at + d) % len];
        if (st->time == ST_TIE) continue;
        uint32_t j = 0;
        while (j < st->n && st->note[j] != raw) j++;
        if (st->time != ST_NOTE || j >= st->n || !(step_cont(st) & (1u << j))) break;
        for (uint32_t q = 0; q < st->n; q++)
            if (q != j && note_snap_to(t, st->note[q], 1u) == n) return 0;
    }
    return 1;
}
static void sqm_pitch_apply(const step_t *p, uint32_t len, const sqm_end_t *x, step_t *out)
{
    if (!(morph.cfg & MC_PIT)) return;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t k = sqm_pitch_voice(&p[i], i);
        if (k == 4u || !sqm_pitch_get(x, i)) continue;
        int32_t n = note_snap_to(trk, (int32_t)p[i].note[k] + sqm_pitch_get(x, i), 1u);
        if (!sqm_pitch_chain_ok(trk, p, len, i, k, n)) continue;
        out[i].note[k] = (uint8_t)n;
        for (uint32_t d = 1; d < len; d++) {
            uint32_t at = (i + d) % len, j = 0;
            if (p[at].time == ST_TIE) continue;
            while (j < p[at].n && p[at].note[j] != p[i].note[k]) j++;
            if (p[at].time != ST_NOTE || j >= p[at].n || !(step_cont(&p[at]) & (1u << j))) break;
            out[at].note[j] = (uint8_t)n;
        }
    }
}
/* One nearest-scale-degree move; caller decides the musical opportunity. */
static uint32_t sqm_pitch_walk(track_t *t, sqm_end_t *x, uint32_t seed)
{
    uint32_t len = sqm_len(t), first, i, k, d;
    if (scale_mask(t) == 0xFFFu) return len;        /* (SCALE CHR: every pitch class would do, the walk would be chromatic) */
    first = sqm_hash(seed, 0, 42u) % len;
    for (uint32_t j = 0; j < len; j++) {
        i = (first + j) % len;
        k = sqm_pitch_voice(&t->step[i], i);
        if (k == 4u) continue;
        for (uint32_t turn = 0; turn < 2u; turn++) {
            int32_t sign = ((sqm_hash(seed, i, 43u) & 1u) ^ turn) ? 1 : -1;
            int32_t old = t->step[i].note[k], current = old + sqm_pitch_get(x, i);
            for (d = 1; d <= 12u; d++) {
                int32_t n = current + sign * (int32_t)d;
                if (!((scale_mask(t) >> (uint32_t)((n - root_g + 240) % 12)) & 1u)) continue;
                if (sqm_pitch_chain_ok(t, t->step, len, i, k, n)) {
                    sqm_pitch_set(x, i, n - old);
                    return i;
                }
                break;                              /* a boundary/collision turns the walk, never leaps over it */
            }
        }
    }
    return len;
}
static void sqm_pitch_mutate(track_t *t, sqm_end_t *x, uint32_t seed, uint32_t tier, uint32_t amount)
{
    if (!tier || !amount || !(morph.cfg & MC_PIT) || sqm_hash(seed, 0, 41u) % 8u) return;
    (void)sqm_pitch_walk(t, x, seed);
}
static void sqm_pitch_clear(sqm_part_t *q)
{
    memset(q->a.pitch, 0, sizeof q->a.pitch);
    for (uint32_t i = 0; i < SQM_HIST; i++) memset(q->b[i].pitch, 0, sizeof q->b[i].pitch);
}
