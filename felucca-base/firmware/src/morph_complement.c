/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop palette contrast, not audio analysis. LP cutoff is a colour proxy;
 * envelope time coordinates (FM6 carrier rates for own-envelope patches) are
 * a length proxy. Bias only nearby colours/lengths, leaving wider differences
 * and regular independent choices free. No levels, pitches, effects or extra state. */
static int sound_peer_available(uint32_t pi)
{
    const track_t *t = &trk[pi ^ 1u];
    return !t->p[P_MUTE] && t->p[P_LEVEL] > 0;
}
static int32_t sound_length(const int16_t *p, const uint8_t *fm)
{
    if (!fm) return (p[P_ATK] + p[P_DEC] + p[P_REL]) / 3;
    uint32_t alg = p[P_E0] >= 1 && p[P_E0] <= 32 ? (uint32_t)p[P_E0] - 1u : fm[FP_ALG];
    uint32_t carriers = fm6_carriers(alg & 31u);
    int32_t longest = 0;
    for (uint32_t k = 0; k < 6u; k++) if (((carriers >> k) & 1u) && fm[k * FP_OP + FP_OL]) {
        uint32_t o = k * FP_OP;
        int32_t length = (297 - fm[o + FP_R1] - fm[o + FP_R1 + 1u] - fm[o + FP_R1 + 3u]) * 127 / 297;
        if (length > longest) longest = length;
    }
    return longest;
}
static int32_t sound_axis(uint32_t pi, uint32_t axis)
{
    if (axis == MDL_BRIGHT) return morph_view(pi, P_FREEA);
    if (axis == MDL_MOVE) {
        if (morph_view(pi, P_FREEA) >= UF_STEPS && !morph_view(pi, P_FREEC)) return 0;
        int32_t d = morph_view(pi, P_LD_FLT);
        return d < 0 ? -d : d;
    }
    if (axis == MDL_SPACE)
        return (morph_view(pi, P_REV) + morph_view(pi, P_DLY) + morph_view(pi, P_CHOR)) / 3;

    int16_t p[EXT_P_LEN] = {0};
    p[P_ATK] = (int16_t)morph_view(pi, P_ATK);p[P_DEC] = (int16_t)morph_view(pi, P_DEC);
    p[P_REL] = (int16_t)morph_view(pi, P_REL);
    if (trk[pi].eng_req != ENGI_FM6) return sound_length(p, 0);
    uint8_t fm[FP_SIZE] = {0};
    p[P_E0] = (int16_t)morph_view(pi, P_E0);
    fm[FP_ALG] = fm6_patch[pi][FP_ALG];
    for (uint32_t k = 0; k < 6u; k++) {
        uint32_t o = k * FP_OP;
        fm[o + FP_OL] = (uint8_t)morph_view_fm(pi, o + FP_OL);
        fm[o + FP_R1] = (uint8_t)morph_view_fm(pi, o + FP_R1);
        fm[o + FP_R1 + 1u] = (uint8_t)morph_view_fm(pi, o + FP_R1 + 1u);
        fm[o + FP_R1 + 3u] = (uint8_t)morph_view_fm(pi, o + FP_R1 + 3u);
    }
    return sound_length(p, fm);
}
static int32_t sound_complement_dir(uint32_t pi, uint32_t axis, int32_t direction)
{
    if (!sound_peer_available(pi)) return direction;
    if (axis == MDL_BRIGHT && (morph_view(pi, P_FREEC) || morph_view(pi ^ 1u, P_FREEC))) return direction;
    int32_t a = sound_axis(pi, axis), b = sound_axis(pi ^ 1u, axis);
    int32_t gap = axis == MDL_BRIGHT ? 8 : axis == MDL_MOVE ? 6 : axis == MDL_SPACE ? 10 : 12;
    if (a - b >= gap || b - a >= gap) return direction;
    int32_t contrast = a == b ? (pi ? 1 : -1) : a > b ? 1 : -1;
    /* Negative LFO depth grows stronger by becoming more negative. */
    return axis == MDL_MOVE && morph_view(pi, P_LD_FLT) < 0 ? -contrast : contrast;
}
static int32_t sound_candidate_score(uint32_t pi, uint32_t engine, uint32_t preset)
{
    const engine_t *e = ENGINES[engine];
    const ds_sound_t *s = ds_get(engine, preset);
    if (!s) return -1;
    int16_t p[EXT_P_LEN] = {0};
    for (uint32_t id = 0; id < P_E0; id++) p[id] = TP[id].def;
    for (uint32_t id = 0; id < NX; id++) p[XW_BASE + id] = XTP[id].def;
    for (uint32_t id = 0; id < 8u; id++) p[P_E0 + id] = e->edit[id].def;
    ds_put(s, p, 0);
    int32_t score = 0;
    if (!p[P_FREEC] && !morph_view(pi ^ 1u, P_FREEC)) {
        int32_t d = p[P_FREEA] - sound_axis(pi ^ 1u, MDL_BRIGHT);
        if (d < 0) d = -d;
        score += d > 8 ? 8 : d;
    }
    int32_t strength = p[P_LD_FLT] < 0 ? -p[P_LD_FLT] : p[P_LD_FLT];
    if (p[P_FREEA] >= UF_STEPS && !p[P_FREEC]) strength = 0;
    int32_t delta = strength - sound_axis(pi ^ 1u, MDL_MOVE);
    if (delta < 0) delta = -delta;
    score += delta > 6 ? 6 : delta;
    delta = (p[P_REV] + p[P_DLY] + p[P_CHOR]) / 3 - sound_axis(pi ^ 1u, MDL_SPACE);
    if (delta < 0) delta = -delta;
    score += delta > 10 ? 10 : delta;
    uint8_t fm[FP_SIZE + 1u];
    if (engine == ENGI_FM6 && fm6_slot_patch((uint32_t)clamp(p[P_E7], 0, 255), fm)) return -1;
    int32_t d = sound_length(p, engine == ENGI_FM6 ? fm : 0) - sound_axis(pi ^ 1u, MDL_SHAPE);
    if (d < 0) d = -d;
    return score + (d > 12 ? 12 : d);
}
static uint32_t sound_complement_pick(uint32_t pi, uint32_t engine, uint32_t a, uint32_t b, uint32_t seed)
{
    if (!morph_musical || (morph.cfg & MC_WILD) || !(seed & 3u) || !sound_peer_available(pi)) return a;
    return sound_candidate_score(pi, engine, b) > sound_candidate_score(pi, engine, a) ? b : a;
}
