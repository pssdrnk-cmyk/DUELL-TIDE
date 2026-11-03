/* SPDX-License-Identifier: GPL-3.0-only */
/* Cumulative phrase lifecycle. A byte per stored attack retains its reached
 * position (0: original; otherwise destination+1), an optional repeat, or a rest.
 * Positions live in the existing pool; no full extra step buffers or voices.
 * Main loop only; the ISR reads the ordinary published caches. */
#define RHYTHM_REPEAT 128u
#define RHYTHM_REST 127u
static int phrase_mutate(uint32_t pi, uint32_t seed);
static int32_t phrase_direction(uint32_t pi, int32_t cur, uint32_t len, int32_t dir, uint32_t seed);
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
/* Where stored step idx sits in the beat, against the FULL stored loop (never POLY's shortened one): SM_BEAT, E, AND, A (the push, a 16th before the
 * beat), SM_ODD (a 32nd), or SM_NONE for a grid not supported (triplets, slow rates, 1/48s, a loop that is not a whole number of beats), which then
 * behaves as before. Heavy swing (the sum, as step_samples) pulls the A towards the next beat: it counts as E. Main loop. */
enum { SM_NONE, SM_BEAT, SM_E, SM_AND, SM_A, SM_ODD };
static uint32_t tide_per(const track_t *t)      /* 32nds a step lasts (0: not a supported grid) */
{
    uint32_t d = (uint32_t)t->p[arp_mode(t) == AM_SEQ_ID ? P_ARATE : P_SDIV], per = d == 0u ? 8u : d == 1u ? 4u : d == 2u ? 2u : d == 3u ? 1u : 0u;
    return per && !((sqm_len(t) * per) % 8u) ? per : 0u;
}
static uint32_t tide_metric(const track_t *t, uint32_t idx)
{
    uint32_t per = tide_per(t), p32;
    if (!per)
        return SM_NONE;
    p32 = (idx * per) % 8u;
    if (p32 == 6u && sw_old(t->p[P_SSWING]) + sw_old(song.g[G_SWING]) >= 60)
        return SM_E;
    return p32 == 0u ? SM_BEAT : p32 == 4u ? SM_AND : p32 == 6u ? SM_A : p32 == 2u ? SM_E : SM_ODD;
}
/* the part's attacks on the push in the bar of step idx, as reached (stored hit + offset), the one at skip left out (M7b's cap) */
static uint32_t tide_pushes(uint32_t pi, uint32_t idx, uint32_t skip, uint32_t len)
{
    const track_t *t = &trk[pi];
    uint32_t per = tide_per(t), n = 0, k;
    for (k = 0; k < len && per; k++) {
        int32_t at = (int32_t)k + rhythm_offset(pi, k);
        if (k != skip && sqm_hit(&t->step[k]) && rhythm_map[pi][k] != RHYTHM_REST && at >= 0 && at < (int32_t)len &&
            tide_metric(t, (uint32_t)at) == SM_A && (uint32_t)at * per / 32u == idx * per / 32u)
            n++;
    }
    return n;
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
        int32_t preferred = phrase_direction(pi, cur, len, rhythm.dir[pi], seed);
        if (tarc_sync() && tide_metric(t, 0) != SM_NONE && tide_metric(t, (uint32_t)cur) == SM_AND && sqm_hash(seed, i, 0x5Du) % 4u < (tarc_sync() == 2u ? 3u : 2u))
            continue;                                /* SYNC: a hit that sits on the "and" tends to stay */
        for (uint32_t attempt = 0; attempt < 2u; attempt++) {
            int32_t dir = attempt ? -preferred : preferred;
            int32_t dest = cur + dir;
            if (dest <= 0 || dest >= (int32_t)len ||
                ((rhythm_map[pi][i] & RHYTHM_REPEAT) && (uint32_t)dest == i)) continue;
            if ((uint32_t)dest != i && !sqm_target(t->step, (uint32_t)dest, len)) continue;
            if (tarc_sync() && tide_metric(t, 0) != SM_NONE) {          /* SYNC: never onto a real beat; the push is an accent (M7b) */
                uint32_t m = tide_metric(t, (uint32_t)dest);
                if (m == SM_BEAT || (m == SM_A && (tide_pushes(pi, (uint32_t)dest, i, len) >= (tarc_sync() == 2u ? 2u : 1u) ||
                                                   sqm_hit(&t->step[((uint32_t)dest + 1u) % len])))) continue;
            }
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
