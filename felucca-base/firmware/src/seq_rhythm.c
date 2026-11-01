/* SPDX-License-Identifier: GPL-3.0-only */
/* Cumulative phrase lifecycle. A byte per stored attack retains its reached
 * position (0: original; otherwise destination+1), an optional repeat, or a rest.
 * Positions live in the existing pool; no full extra step buffers or voices.
 * Main loop only; the ISR reads the ordinary published caches. */
#define RHYTHM_REPEAT 128u
#define RHYTHM_REST 127u
static int phrase_mutate(uint32_t pi, uint32_t seed);
static int32_t phrase_direction(uint32_t pi, int32_t cur, uint32_t len, int32_t dir);
static uint8_t rhythm_map[NSHOWN][NSTEP] __attribute__((section(".pool")));
static struct {
    uint32_t source[NSHOWN];
    uint8_t seen[NSHOWN], repeats[NSHOWN], turn, active;
    int8_t dir[NSHOWN];
    uint8_t phase[NSHOWN];
} rhythm __attribute__((section(".pool")));
_Static_assert(sizeof rhythm == 20u, "rhythm control state budget");
static int32_t rhythm_offset(uint32_t pi, uint32_t i)
{
    uint32_t m = rhythm_map[pi][i] & ~RHYTHM_REPEAT;
    return m && m != RHYTHM_REST ? (int32_t)m - 1 - (int32_t)i : 0;
}
static void rhythm_mark(uint32_t pi)
{
    uint32_t any = 0;
    for (uint32_t j = 0; j < NSTEP; j++) any |= rhythm_map[pi][j];
    rhythm.active = (uint8_t)((rhythm.active & ~(1u << pi)) | (any ? 1u << pi : 0u));
}
static void rhythm_put(uint32_t pi, uint32_t i, int32_t d)
{
    rhythm_map[pi][i] = d ? (uint8_t)((int32_t)i + d + 1) : 0;
    rhythm_mark(pi);
}
static void rhythm_reset(uint32_t pi)
{
    memset(rhythm_map[pi], 0, sizeof rhythm_map[pi]);
    rhythm.active &= (uint8_t)~(1u << pi);
    rhythm.seen[pi] = tide_seq_loops[pi];
    rhythm.repeats[pi] = 0;
    rhythm.dir[pi] = 0;
    rhythm.phase[pi] = 0;
}
static uint32_t rhythm_hash(uint32_t pi)
{
    uint32_t h = 0;
    for (uint32_t j = 0; j < sizeof rhythm_map[pi]; j++)
        h = (h ^ rhythm_map[pi][j]) * 16777619u;
    return h;
}
static void rhythm_validate(uint32_t pi, uint32_t len)
{
    const track_t *t = &trk[pi];
    uint32_t h = (len ^ ((uint32_t)t->p[P_SDIV] << 8)) * 16777619u;
    const uint8_t *p = (const uint8_t *)t->step;
    for (uint32_t j = 0; j < len * sizeof(step_t); j++) h = (h ^ p[j]) * 16777619u;
    if (rhythm.source[pi] != h || !sqm[pi].notes) {
        rhythm_reset(pi);
        rhythm.source[pi] = h;
    }
}
static int rhythm_fixed(const step_t *p, uint32_t idx)
{
    for (uint32_t pi = 0; pi < NSHOWN; pi++)
        if (p == trk[pi].step) return rhythm_map[pi][idx] != 0;
    return 0;
}
static int rhythm_reserved(const step_t *p, uint32_t idx)
{
    for (uint32_t pi = 0; pi < NSHOWN; pi++)
        if (p == trk[pi].step && (rhythm.active & (1u << pi)))
            for (uint32_t j = 0; j < NSTEP; j++)
                if (rhythm_map[pi][j] != RHYTHM_REST && rhythm_map[pi][j] &&
                    ((int32_t)j + rhythm_offset(pi, j) == (int32_t)idx ||
                     ((rhythm_map[pi][j] & RHYTHM_REPEAT) && j == idx))) return 1;
    return 0;
}
static void rhythm_apply(uint32_t pi, uint32_t len, step_t *out)
{
    if (!(rhythm.active & (1u << pi))) return;
    /* Snapshot before routing: exchanges and repeated sources must not read a
     * cell already overwritten by an earlier source. Uses main-loop stack only. */
    step_t source[NSTEP];
    memcpy(source, out, len * sizeof *out);
    for (uint32_t i = 0; i < len; i++)
        if (rhythm_map[pi][i] && !(rhythm_map[pi][i] & RHYTHM_REPEAT)) {
            out[i].time = ST_REST; out[i].n = 0;
        }
    for (uint32_t i = 0; i < len; i++) {
        uint8_t m = rhythm_map[pi][i];
        int32_t dest = (int32_t)i + rhythm_offset(pi, i);
        if (!m || m == RHYTHM_REST || dest < 0 || dest >= (int32_t)len) continue;
        out[dest] = source[i];
    }
}
/* Move one onset from its reached position, retaining direction; never move an original anchor,
 * overwrite another attack or disturb held/ratcheted notes on either end. */
static int rhythm_mutate(uint32_t pi, uint32_t seed)
{
    track_t *t = &trk[pi];
    uint32_t len = sqm_len(t), first = seed % len;
    if (!rhythm.dir[pi] || (seed >> 16) % 8u == 0u)
        rhythm.dir[pi] = (int8_t)(seed & 1u ? 1 : -1);
    for (uint32_t j = 0; j < len; j++) {
        uint32_t i = (first + j) % len;
        int32_t cur = (int32_t)i + rhythm_offset(pi, i);
        if (!(i % 4u) || !sqm_hit(&t->step[i]) || sqm_held(t->step, i, len) ||
            rhythm_map[pi][i] == RHYTHM_REST) continue;
        int32_t preferred = phrase_direction(pi, cur, len, rhythm.dir[pi]);
        for (uint32_t attempt = 0; attempt < 2u; attempt++) {
            int32_t dir = attempt ? -preferred : preferred;
            int32_t dest = cur + dir;
            if (dest <= 0 || dest >= (int32_t)len ||
                ((rhythm_map[pi][i] & RHYTHM_REPEAT) && (uint32_t)dest == i)) continue;
            if ((uint32_t)dest != i && !sqm_target(t->step, (uint32_t)dest, len)) continue;
            int ok = 1;
            for (uint32_t k = 0; k < len; k++)
                if (k != i && sqm_hit(&t->step[k]) && rhythm_map[pi][k] != RHYTHM_REST &&
                    ((int32_t)k + rhythm_offset(pi, k) == dest ||
                     ((rhythm_map[pi][k] & RHYTHM_REPEAT) && (int32_t)k == dest))) ok = 0;
            for (uint32_t e = 0; e < 2u; e++) {
                const step_t *p = sqm_pat[pi][e];
                if (!sqm_hit(&p[cur]) || sqm_held(p, (uint32_t)cur, len) ||
                    !sqm_target(p, (uint32_t)dest, len)) ok = 0;
            }
            if (!ok) continue;
            uint8_t repeat = rhythm_map[pi][i] & RHYTHM_REPEAT;
            rhythm_put(pi, i, dest - (int32_t)i);
            rhythm_map[pi][i] |= repeat;
            rhythm.dir[pi] = (int8_t)dir;
            return 1;
        }
    }
    return 0;
}
static void rhythm_poll(uint32_t mask)
{
    uint32_t boundary = 0;
    for (uint32_t pi = 0; pi < NSHOWN; pi++) {
        uint8_t now = tide_seq_loops[pi], d = (uint8_t)(now - rhythm.seen[pi]);
        rhythm.seen[pi] = now;
        if (!((mask >> pi) & 1u) || !sqm[pi].notes) { rhythm.repeats[pi] = 0; continue; }
        if (d) boundary |= 1u << pi;
        uint32_t n = rhythm.repeats[pi] + d;
        rhythm.repeats[pi] = (uint8_t)(n > 2u ? 2u : n);
    }
    for (uint32_t n = 0; n < NSHOWN; n++) {
        uint32_t pi = (rhythm.turn + n) % NSHOWN;
        if (!((boundary >> pi) & 1u) || rhythm.repeats[pi] < 2u) continue;
        if (phrase_mutate(pi, morph_new_seed())) {
            rhythm.turn = (uint8_t)(pi ^ 1u);
            for (uint32_t k = 0; k < NSHOWN; k++) rhythm.repeats[k] = 0;
            sqm_refresh(pi, 1);
            return;
        }
        rhythm.repeats[pi] = 0; /* no eligible attack: wait another two plays */
    }
}


/* WNDR/JRNY use inherited positions, rather than also moving the same attack
 * through the older SYNC dial. Other variation dials continue evolving. */
static void rhythm_begin(uint32_t mask)
{
    for (uint32_t pi = 0; pi < NSHOWN; pi++) if ((mask >> pi) & 1u) {
        sqm[pi].a.d[SD_SYNC] = 0;
        for (uint32_t j = 0; j < SQM_HIST; j++) sqm[pi].b[j].d[SD_SYNC] = 0;
        rhythm.repeats[pi] = 0;
        rhythm.seen[pi] = tide_seq_loops[pi];
        sqm_refresh(pi, 1);
    }
}
