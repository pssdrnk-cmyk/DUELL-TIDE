/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop attack-space prediction. Read the peer's reached pattern on the
 * shared clock, respecting each part's length, division and swing. No audio
 * ducking, new voices or persistent state. Prediction uses the sequencer
 * sample clock. Unsupported extreme clock periods fall back to independent
 * evolution. Holds are not competing attacks. */
static uint32_t phrase_step_time(const track_t *t, uint32_t count, uint32_t period)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < count; i++) n += step_samples(t, period, i);
    return n;
}
static int phrase_peer_attack(uint32_t pi, uint32_t dest)
{
    const track_t *t = &trk[pi], *peer = &trk[pi ^ 1u];
    if (!song.playing || !peer->seq_active || peer->p[P_MUTE] || !peer->p[P_LEVEL] || peer->p[P_AMODE] == AM_SEQ)
        return 0; /* the SEQ arp's heard timing depends on held keys */
    uint32_t len = sqm_len(peer), period = div_samples((uint32_t)peer->p[P_SDIV]);
    uint32_t ownperiod = div_samples((uint32_t)t->p[P_SDIV]);
    /* Two swung 64-step cycles plus their within-step positions fit uint32_t.
     * Valid 40..300 BPM divisions are well below this clock-anomaly guard. */
    if (period > 0xFFFFFFFFu / (4u * NSTEP) || ownperiod > 0xFFFFFFFFu / (4u * NSTEP)) return 0;
    if ((t->seq_pos < 0x7FFFFFFFu && t->seq_pos > 2u * ownperiod) ||
        (peer->seq_pos < 0x7FFFFFFFu && peer->seq_pos > 2u * period)) return 0;
    uint32_t cycle = phrase_step_time(peer, len, period);
    if (!cycle) return 0;
    uint32_t ownlen = sqm_len(t), owncycle = phrase_step_time(t, ownlen, ownperiod);
    if (!owncycle) return 0;
    uint32_t ownpos = t->seq_pos < 0x7FFFFFFFu ? t->seq_pos : 0u;
    uint32_t now = (phrase_step_time(t, t->seq_idx % ownlen, ownperiod) + ownpos) % owncycle;
    uint32_t target = phrase_step_time(t, dest, ownperiod);
    /* A short division can advance again before the main loop sees the wrap.
     * Predict the next visit of a step already passed, rather than time zero. */
    target = target >= now ? target - now : owncycle - (now - target);
    uint32_t pos = phrase_step_time(peer, peer->seq_idx % len, period) + target;
    if (peer->seq_pos < 0x7FFFFFFFu) pos += peer->seq_pos;
    pos %= cycle;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t n = step_samples(peer, period, i);
        if (pos < n) {
            step_t buf;
            const step_t *s = seq_play_step(peer, i, &buf);
            return sqm_hit(s) && !step_cont(s) && step_chance(s) != 0u;
        }
        pos -= n;
    }
    return 0;
}
static int32_t phrase_direction(uint32_t pi, int32_t cur, uint32_t len, int32_t dir)
{
    int32_t a = cur + dir, b = cur - dir;
    if (a > 0 && b > 0 && a < (int32_t)len && b < (int32_t)len &&
        phrase_peer_attack(pi, (uint32_t)a) > phrase_peer_attack(pi, (uint32_t)b))
        return -dir;
    return dir;
}
