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
/* MR6: a motif with one free hit at most, and the bass (B1), keeps its hits outside ARC's breakdown: no gap, and no repeat added either
 * (the density would only climb); a dropped one still comes back */
static int phrase_whole(uint32_t pi, uint32_t len)
{
    uint32_t free = 0;
    for (uint32_t i = 0; i < len; i++) free += phrase_source(pi, i, len);
    return (free <= 1u || ((sqm_bassm >> pi) & 1u)) && !((tarc_thin >> pi) & 1u);
}
static int phrase_gap(uint32_t pi, uint32_t seed)
{
    uint32_t len = sqm_len(&trk[pi]), base = 0;
    for (uint32_t i = 0; i < len; i++) base += sqm_hit(&trk[pi].step[i]);
    uint32_t count = phrase_count(pi, len);
    if (count <= (base + 1u) / 2u || count <= 1u || phrase_whole(pi, len)) return 0;
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
    for (uint32_t j = 0; j < len && !phrase_whole(pi, len); j++) {
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

/* Chord-rhythm vocabulary (b7, idea 1). A chord part's variation bar (End B: VARY BARS' every 4th bar, or every bar with ALL) plays a one-bar
 * rhythm cell a house / dub-techno keyboard player knows, the one nearest the part's own pattern (fewest steps different), now and then
 * the second nearest: never the pattern itself. Its hits take notes, velocity and accent from the nearest hit of the pattern (no new
 * pitch). A cell is offered for a pattern only when every hit of it lies within two 16ths of one of the pattern's (the guide's "a hit
 * moves two 16ths at most"), it keeps the pattern's beat hits, and it has one hit more or fewer at most. OFFBEAT (STYLE's SYNC) leans to
 * the cells with more off-beats. A 16-step loop on the 1/16 grid only; End A, the other bars, plays the part's own pattern. A part with a
 * cell plays its variation bar from End B whole, from a trip's first beat (sqm_step: no beat-by-beat blend there). Main loop */
static const uint16_t SQM_CELL[12] = {
    0x4444,                                              /* the skank: the four "and"s */
    0xC444,                                              /* .. with a throw: the last "and" doubled into the next bar */
    0x8444,                                              /* .. the last one pushed (the 16th before the downbeat) */
    0x8404,                                              /* the off-beat half-time with a push */
    0x1041,                                              /* 3-3-2 in 8ths (0 6 12) */
    0x4949,                                              /* 3-3-2 twice in 16ths (0 3 6, 8 11 14) */
    0x1249,                                              /* 3-3-3-3-4 (0 3 6 9 12) */
    0x0101,                                              /* half-time: 1 and 3 */
    0x0404,                                              /* half-time on the off-beats */
    0x0408,                                              /* the Detroit two-hit: the "a" of 1, the "and" of 3 */
    0x4040,                                              /* dub: the "and"s of 2 and 4 */
    0x0041,                                              /* the Charleston: 1 and the "and" of 2 */
};
static uint32_t sqm_pop(uint32_t m) { uint32_t n = 0; for (; m; m &= m - 1u) n++; return n; }
static uint32_t sqm_rot(uint32_t m, uint32_t r) { return (m << r | m >> (16u - r)) & 0xFFFFu; }
static int sqm_cell(uint32_t pi, const step_t *p, uint32_t len, uint32_t seed, step_t *out)   /* 1: End B plays a cell */
{
    uint32_t i, k, j, pm = 0, near, c, sc, best = 12, b2 = 12, sb = ~0u, s2 = ~0u;
    step_t src[16];
    if (len != 16u || tide_per(&trk[pi]) != 2u || !sqm_chordp(pi, p, len))
        return 0;
    for (i = 0; i < 16u; i++)
        pm |= (uint32_t)sqm_hit(&p[i]) << i;
    near = pm | sqm_rot(pm, 1) | sqm_rot(pm, 2) | sqm_rot(pm, 14) | sqm_rot(pm, 15);
    for (k = 0; k < 12u; k++) {
        c = SQM_CELL[k];
        if (c == pm || sqm_pop(c) + 1u - sqm_pop(pm) > 2u || (c & ~near) || (pm & 0x1111u & ~c))
            continue;
        sc = 4u * sqm_pop(c ^ pm) + 8u - (sqm_sync ? sqm_pop(c & 0xEEEEu) : 0u);
        if (sc < sb)
            b2 = best, s2 = sb, best = k, sb = sc;
        else if (sc < s2)
            b2 = k, s2 = sc;
    }
    if (best >= 12u)
        return 0;
    if (b2 < 12u && !(sqm_hash(seed, pi, 0xCE11u) & 3u))
        best = b2;                                       /* (a quarter of the generations: the second nearest) */
    c = SQM_CELL[best];
    memcpy(src, out, sizeof src);
    for (i = 0; i < 16u; i++) {
        if (!((c >> i) & 1u)) {
            if (sqm_hit(&out[i]))
                out[i].time = ST_REST, out[i].n = 0;
            continue;
        }
        if (((pm >> i) & 1u) && sqm_hit(&src[i]))
            continue;
        for (k = 0; k < 3u; k++)                         /* (the nearest hit of the pattern, the earlier one first) */
            if (((pm >> (j = (i + 16u - k) & 15u)) & 1u) || ((pm >> (j = (i + k) & 15u)) & 1u))
                break;
        out[i] = sqm_hit(&src[j]) ? src[j] : p[j];
        out[i].flags &= (uint8_t)~(SF_SLIDE | SF_RATCH);
    }
    return 1;
}
