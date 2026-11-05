/* SPDX-License-Identifier: GPL-3.0-only */
/* Phrase edits reuse the timing map and the existing packed pitch offsets.
 * Main loop only. A source may move, repeat at its old place too, or rest;
 * the motif's anchors (sqm_anchor) and continuation/slide/ratchet chains stay intact. */
static uint32_t phrase_count(uint32_t pi, uint32_t len)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < len; i++) if (sqm_hit(&trk[pi].step[i])) {
        uint8_t m = rhythm_map[pi][i];
        if (m != RHYTHM_REST) n += 1u + !!(m & RHYTHM_REPEAT);
    }
    return n;
}
static int phrase_source(uint32_t pi, uint32_t i, uint32_t len)
{
    return sqm_hit(&trk[pi].step[i]) && !sqm_held(trk[pi].step, i, len) && !sqm_anchor(trk[pi].step, len, i);
}
static int phrase_gap(uint32_t pi, uint32_t seed)
{
    uint32_t len = sqm_len(&trk[pi]), base = 0;
    for (uint32_t i = 0; i < len; i++) base += sqm_hit(&trk[pi].step[i]);
    uint32_t count = phrase_count(pi, len);
    if (count <= (base + 1u) / 2u || count <= 1u) return 0;
    /* Prefer making room where the other part already attacks. A second pass
     * permits development when there are no overlaps to remove. */
    for (uint32_t pass = 0; pass < 2u; pass++)
    for (uint32_t j = 0; j < len; j++) {
        uint32_t i = (seed + j) % len;
        uint8_t m = rhythm_map[pi][i];
        if (!pass && !phrase_peer_attack(pi, (uint32_t)((int32_t)i + rhythm_offset(pi, i)))) continue;
        if (!phrase_source(pi, i, len) || m == RHYTHM_REST) continue;
        /* First remove an extra repeat; otherwise let this attack become a rest. */
        rhythm_map[pi][i] = m & RHYTHM_REPEAT ? (uint8_t)(m & ~RHYTHM_REPEAT) : RHYTHM_REST;
        rhythm_mark(pi);
        return 1;
    }
    return 0;
}
static int phrase_repeat(uint32_t pi, uint32_t seed)
{
    uint32_t len = sqm_len(&trk[pi]), base = 0;
    for (uint32_t i = 0; i < len; i++) base += sqm_hit(&trk[pi].step[i]);
    if (phrase_count(pi, len) >= tarc_cap(base)) return 0;
    /* Restore a dropped attack before adding another voice onset. */
    for (uint32_t j = 0; j < len; j++) {
        uint32_t i = (seed + j) % len;
        if (rhythm_map[pi][i] != RHYTHM_REST || phrase_peer_attack(pi, i) || rhythm_lands(pi, i, (int32_t)i, len, 0)) continue;
        rhythm_map[pi][i] = 0;
        rhythm_mark(pi);
        return 1;
    }
    for (uint32_t j = 0; j < len; j++) {
        uint32_t i = (seed + j) % len;
        if (phrase_peer_attack(pi, i) || !phrase_source(pi, i, len) || !rhythm_map[pi][i] ||
            (rhythm_map[pi][i] & RHYTHM_REPEAT)) continue;
        /* A moved attack can answer itself at its original empty place. Other
         * sources must not already occupy that place. */
        if (rhythm_lands(pi, i, (int32_t)i, len, 0)) continue;
        rhythm_map[pi][i] |= RHYTHM_REPEAT;
        rhythm_mark(pi);
        return 1;
    }
    return 0;
}
static int phrase_order(uint32_t pi, uint32_t seed)
{
    uint32_t len = sqm_len(&trk[pi]);
    for (uint32_t j = 0; j < len; j++) {
        uint32_t a = (seed + j) % len;
        if (!phrase_source(pi, a, len) || rhythm_map[pi][a] == RHYTHM_REST ||
            (rhythm_map[pi][a] & RHYTHM_REPEAT)) continue;
        int32_t da = (int32_t)a + rhythm_offset(pi, a);
        for (uint32_t d = 1; d < len; d++) {
            uint32_t b = (a + d) % len;
            if (!phrase_source(pi, b, len) || rhythm_map[pi][b] == RHYTHM_REST ||
                (rhythm_map[pi][b] & RHYTHM_REPEAT)) continue;
            int32_t db = (int32_t)b + rhythm_offset(pi, b);
            /* Local reordering changes a short motif; never exchange two
             * identical attacks, or take a hit two 16ths past its stored step. */
            if (rhythm_far(db - (int32_t)a) || rhythm_far(da - (int32_t)b) ||
                !memcmp(&trk[pi].step[a], &trk[pi].step[b], sizeof(step_t))) continue;
            rhythm_put(pi, a, db - (int32_t)a);
            rhythm_put(pi, b, da - (int32_t)b);
            return 1;
        }
    }
    return 0;
}
static int phrase_pitch(uint32_t pi, uint32_t seed)
{
    if (!(morph.cfg & MC_PIT)) return 0;
    sqm_part_t *q = &sqm[pi];
    sqm_end_t next = q->aon ? q->a : (sqm_end_t){0};
    if (q->fader >= 64u) next = q->b[q->top];
    uint32_t len = sqm_len(&trk[pi]);
    for (uint32_t attempt = 0; attempt < len; attempt++) {
        sqm_end_t trial = next;
        uint32_t at = sqm_pitch_walk(&trk[pi], &trial, seed + attempt * 7919u);
        if (at >= len || rhythm_map[pi][at] == RHYTHM_REST) continue;
        if (!q->aon) { memset(&q->a, 0, sizeof q->a); q->a.seed = q->b[q->top].seed; q->aon = 1; }
        int32_t delta = sqm_pitch_get(&trial, at);
        sqm_pitch_set(&q->a, at, delta);
        for (uint32_t k = 0; k < SQM_HIST; k++) sqm_pitch_set(&q->b[k], at, delta);
        return 1;
    }
    return 0;
}
static int phrase_mutate(uint32_t pi, uint32_t seed)
{
    uint32_t op = rhythm.phase[pi] % 6u;
    int changed = op == 1u || op == 5u ? phrase_pitch(pi, seed) :
                  op == 2u || op == 4u ? (sqm_hash(seed, pi, 71u) & 1u ?
                      phrase_repeat(pi, seed) : phrase_gap(pi, seed)) :
                  op == 3u ? phrase_order(pi, seed) : rhythm_mutate(pi, seed);
    /* A sparse phrase or PITCH OFF can make an operation unavailable. Advance
     * its turn anyway; otherwise one impossible edit stalls all development. */
    rhythm.phase[pi] = (uint8_t)((op + 1u) % 6u);
    return changed || (op && rhythm_mutate(pi, seed));
}
