/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop attack prediction. Snapshot the two playback clocks, then search
 * bounded pattern cycles after releasing the audio publication guard. */
typedef struct {
    uint32_t idx, pos, rem;
    uint8_t nheld, key[16];
} phrase_clock_t;

/* External clocks advance each division through its own fractional grid.
 * Convert to common Q8 musical units; internal clocks use Q8 samples. */
static uint64_t phrase_time(const track_t *t, uint32_t value, uint32_t rem, int arp, int external)
{
    if (!external) return (uint64_t)value * 256u;
    uint32_t div = (uint32_t)t->p[arp ? P_ARATE : P_SDIV], quarter = beat_samples();
    if (div < 6u) quarter = quarter / DIV_DEN[div] * DIV_DEN[div];
    else if (div >= 10u) quarter = midi_clock_period(quarter, div) * MIDI_EXTRA_DEN[(div - 10u) % 7u];
    return (((uint64_t)value * midi_grid_den(div) + rem) * 256u) / (quarter ? quarter : 1u);
}
static uint32_t phrase_duration(const track_t *t, uint32_t idx, uint32_t period, int arp, int external)
{
    if (!arp) return step_samples(t, period, idx);
    int32_t sw = sw_old(t->p[P_ASWING]) * (int32_t)period / 250;
    uint32_t n = period + (uint32_t)((idx & 1u) ? sw : -sw);
    /* Only the internal arp discards overshoot at every step. */
    return external ? n : ((n + CTL - 1u) / CTL) * CTL;
}
static int phrase_available(const track_t *t, const phrase_clock_t *c, int arp)
{
    if (!arp) return !arp_mode(t) && t->p[P_AMODE] != AM_SEQ;
    if (atgt_bits(t)) return 0; /* modulation-target arp has no note onset */
    if (!song.rec) return 1; /* TIDE's remembered chord / C4 stand-in */
    for (uint32_t i = 0; i < c->nheld && i < 16u; i++) {
        uint32_t k = c->key[i];
        if (!k || kb_pitch(t, (k & 31u) - 1u, (int32_t)(k >> 5) - 3) != KB_SILENT) return 1;
    }
    return 0;
}
static int phrase_peer_attack(uint32_t pi, uint32_t dest)
{
    const track_t *t = &trk[pi], *peer = &trk[pi ^ 1u];
    phrase_clock_t c[2];
    int external = clock_src() != 0, ownarp = arp_mode(t) == AM_SEQ_ID, arp = arp_mode(peer) == AM_SEQ_ID;
    if (!song.playing || (mods_off & MODS_SEQ) || !peer->seq_active ||
        peer->p[P_MUTE] || !peer->p[P_LEVEL]) return 0;
    MORPH_LOCK();
    for (uint32_t k = 0; k < 2u; k++) {
        const track_t *v = &trk[pi ^ k];
        int a = k ? arp : ownarp;
        c[k].idx = a ? v->arp_idx : v->seq_idx;
        c[k].pos = a ? v->arp_pos : v->seq_pos;
        c[k].rem = a ? midi_arp_grid[pi ^ k].remainder : midi_seq_grid[pi ^ k].remainder;
        c[k].nheld = v->nheld;
        memcpy(c[k].key, held_key[pi ^ k], sizeof c[k].key);
    }
    MORPH_UNLOCK();
    if (!phrase_available(t, &c[0], ownarp) || !phrase_available(peer, &c[1], arp)) return 0;
    if (tide_poly_on(t) || tide_poly_on(peer)) return 0;   /* (a shortened loop's cycle is not the pattern's: no prediction) */
    uint32_t len = sqm_len(peer), ownlen = sqm_len(t);
    uint32_t period = div_samples((uint32_t)peer->p[arp ? P_ARATE : P_SDIV]);
    uint32_t ownperiod = div_samples((uint32_t)t->p[ownarp ? P_ARATE : P_SDIV]);
    if (dest >= ownlen || period < 2u * CTL || ownperiod < 2u * CTL ||
        period > 0xFFFFFFFFu / (8u * NSTEP) || ownperiod > 0xFFFFFFFFu / (8u * NSTEP) ||
        c[0].pos >= 2u * ownperiod || c[1].pos >= 2u * period) return 0;
    uint64_t window = external ? ((uint64_t)MIDI_CLOCK_GRID * 256u * CTL + beat_samples() - 1u) / beat_samples() : CTL * 256u;
    uint64_t ownpos = phrase_time(t, c[0].pos, c[0].rem, ownarp, external), at = 0;
    uint32_t idx = c[0].idx;
    step_t buf;
    const step_t *s = seq_play_step(t, dest, &buf);
    int32_t nudge = ownarp ? 0 : seq_nudge(t, s);
    int64_t offset = (int64_t)phrase_time(t, (uint32_t)(nudge < 0 ? -nudge : nudge) * ownperiod / NUDGE_DIV, 0, ownarp, external);
    if (nudge < 0) offset = -offset;
    /* External interpolation is timestamped in whole milliseconds; an onset
     * dispatched this block can overshoot by one timestamp quantum. */
    uint64_t dispatch = external && midi_clock.interp_q8 > window ? midi_clock.interp_q8 : window;
    if (idx % ownlen != dest || (int64_t)ownpos > offset + (int64_t)dispatch) {
        at = phrase_time(t, phrase_duration(t, idx, ownperiod, ownarp, external), 0, ownarp, external);
        if (ownpos > at) return 0;
        at -= ownpos;
        for (uint32_t n = 0; n < ownlen; n++) {
            idx = ownarp ? idx + 1u : (idx + 1u) % ownlen;
            if (idx % ownlen == dest) break;
            at += phrase_time(t, phrase_duration(t, idx, ownperiod, ownarp, external), 0, ownarp, external);
        }
        if ((int64_t)at + offset < 0) return 0;
        at = (uint64_t)((int64_t)at + offset);
    } else if (offset > (int64_t)ownpos) at = (uint64_t)offset - ownpos;
    /* The internal dispatch fires at the first block at or after its onset. */
    if (external) {
        volatile uint64_t blocks = __udivdi3(at + window - 1u, window);
        at = blocks * window;
    } else at = ((at + CTL * 256u - 1u) / (CTL * 256u)) * (CTL * 256u);
    uint32_t steps = arp && (len & 1u) ? 2u * len : len;
    uint64_t cycle = 0;
    idx = c[1].idx;
    for (uint32_t n = 0, j = idx; n < steps; n++) {
        cycle += phrase_time(peer, phrase_duration(peer,j,period,arp,external),0,arp,external);
        j = arp ? j + 1u : (j + 1u) % len;
    }
    uint64_t pos = at + phrase_time(peer,c[1].pos,c[1].rem,arp,external), start = 0;
    /* The freestanding runtime supplies division, but no 64-bit remainder. */
    volatile uint64_t turns = __udivdi3(pos, cycle); /* keep the compiler from forming __umoddi3 */
    pos -= turns * cycle;
    for (uint32_t n = 0; n < steps; n++) {
        uint32_t duration = phrase_duration(peer,idx,period,arp,external);
        uint64_t span = phrase_time(peer,duration,0,arp,external);
        s = seq_play_step(peer,idx % len,&buf);
        /* Arp SEQ ignores step CHANCE and APROB is inclusive (0 still allows
         * 1/128). Mark possible attacks without consuming the live RNG. */
        if (sqm_hit(s) && !step_cont(s) && (arp || step_chance(s))) {
            uint32_t hits = arp ? 1u : step_ratchet(s);
            int32_t nd = arp ? 0 : seq_nudge(peer,s);
            int64_t shift = (int64_t)phrase_time(peer,(uint32_t)(nd < 0 ? -nd : nd)*period/NUDGE_DIV,0,arp,external);
            if (nd < 0) shift = -shift;
            for (uint32_t h = 0; h < hits; h++) {
                uint64_t rat = phrase_time(peer,h*(duration/hits),0,arp,external);
                int64_t event = (int64_t)(start + rat) + shift;
                if (event < 0) event += (int64_t)cycle;
                uint64_t when = (uint64_t)event;
                if (when >= cycle) when -= cycle;
                uint64_t past = pos >= when ? pos - when : cycle - (when - pos);
                uint64_t ahead = when >= pos ? when - pos : cycle - (pos - when);
                /* A sequencer's sub-block overshoot distinguishes an onset
                 * fired this block/last block from a sustained gate. */
                if (past < 2u * window || ahead <= window) return 1;
            }
        }
        start += span;
        idx = arp ? idx + 1u : (idx + 1u) % len;
    }
    return 0;
}
static int32_t phrase_direction(uint32_t pi, int32_t cur, uint32_t len, int32_t dir, uint32_t seed)
{
    int32_t a = cur + dir, b = cur - dir;
    if (a > 0 && b > 0 && a < (int32_t)len && b < (int32_t)len) {
        if (sqm_sync && tide_metric(&trk[pi], 0) != SM_NONE) {   /* SYNC: the "and" and the push over the other 16ths; the peer's attack stays the stronger term */
            static const uint8_t W[6] = {0, 0, 1, 3, 2, 1};
            uint32_t sa = (phrase_peer_attack(pi, (uint32_t)a) ? 0u : 4u) + W[tide_metric(&trk[pi], (uint32_t)a)];
            uint32_t sb = (phrase_peer_attack(pi, (uint32_t)b) ? 0u : 4u) + W[tide_metric(&trk[pi], (uint32_t)b)];
            if (sa != sb && sqm_hash(seed, (uint32_t)cur, 0x5Cu) % 24u < (sqm_sync == 2u ? 21u : 16u))
                return sa > sb ? dir : -dir;
        }
        if (phrase_peer_attack(pi, (uint32_t)a) > phrase_peer_attack(pi, (uint32_t)b))
            return -dir;
    }
    return dir;
}
