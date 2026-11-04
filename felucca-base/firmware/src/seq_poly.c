/* SPDX-License-Identifier: GPL-3.0-only */
/* POLY (TIDE LIST, M7d): in automatic MUSIC WNDR / JRNY one part's loop is now and then a few steps shorter (GENTLE 15 or 14 of 16,
 * WIDE 12 to 15), so its motif walks against the beat, then plays the full length again. Included by tide_seq.c; the ISR side
 * (tide_slen, tide_seq_next, tide_arp_idx, tide_arp_tick) runs in the audio path, tide_poly_start at PLAY.
 *
 * The full loop (P_SLEN steps) is the reference: `ref` counts it whether or not the loop is shortened (ARP SEQ: arp_idx % P_SLEN), and
 * every change happens where it wraps (the loop's first step, the bar line for 16 steps), so a shortened stretch starts and ends on the
 * downbeat of the full loop and the groove is locked again afterwards: no drift, no phase to catch up. Within a stretch the short loop
 * restarts by itself, never at the shared bars.
 * Rules: the stored steps and P_SLEN are never touched (a performance, not an edit: KEEP stores the steps at the stored length, UNDO
 * has nothing to undo); only parts in automatic WNDR / JRNY; not before the groove has been heard (2 phrases); one part at a time; a stretch is one
 * or two phrases (WIDE 4 loops, GENTLE 8) followed by 8 or 16 full loops. STOP / PLAY starts every part full. POLY OFF, a recording, a
 * hand taking over (the glide stops) or a loop under 8 steps bring the part back at the next full-loop start. ARP SEQ is covered
 * (arp_idx), the peer prediction (M1) is blind to a shortened part. */
static uint8_t poly_len[NSHOWN];                         /* the loop's length now, 0: the stored one */
static uint8_t poly_ref[NSHOWN];                         /* sequencer: the full loop's step */
static uint16_t poly_lc[NSHOWN], poly_end[NSHOWN], poly_free[NSHOWN];   /* full loops since PLAY; the stretch's end; the earliest next start */
static uint32_t poly_base[NSHOWN];                       /* ARP SEQ: arp_idx at the stretch's start */

static uint32_t tide_poly_full(const track_t *t) { return (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }
static void tide_poly_start(void)
{
    uint32_t i;
    memset((void *)tide_miss, 0, sizeof tide_miss);
    for (i = 0; i < NSHOWN; i++) {
        poly_len[i] = 0;
        poly_ref[i] = (uint8_t)(tide_poly_full(&trk[i]) - 1u);
        poly_lc[i] = poly_end[i] = 0;
        poly_free[i] = 8;
        poly_base[i] = 0;
    }
}
/* the loop's length the sequencer plays (audio path) */
static uint32_t tide_slen(const track_t *t)
{
    uint32_t pi = trk_index(t), len = tide_poly_full(t);
    return pi < NSHOWN && poly_len[pi] && poly_len[pi] < len && arp_mode(t) != AM_SEQ_ID ? poly_len[pi] : len;
}
static int tide_poly_on(const track_t *t) { uint32_t pi = trk_index(t); return pi < NSHOWN && poly_len[pi] != 0u; }
/* the full loop has wrapped: stay, shorten or lengthen (part pi, stored length len) */
static void tide_poly_wrap(uint32_t pi, uint32_t len)
{
    static const uint8_t PCT[3] = {0, 15, 40}, SPAN[3] = {0, 8, 4}, FREE[3] = {0, 16, 8};
    uint32_t lc = ++poly_lc[pi], mode = sqm_poly % 3u, h;
    int ok = mode && len >= 8u && ((sqm_auto >> pi) & 1u) && song.playing && !song.rec;
    int calm = !tarc_hold;                               /* (ARC: no new stretch at the PEAK or the DROP) */
    if (poly_len[pi]) {
        if (!ok || lc >= poly_end[pi] || poly_len[pi] >= len) {
            poly_len[pi] = 0;
            poly_free[pi] = (uint16_t)(lc + FREE[mode ? mode : 1u]);
        }
        return;
    }
    if (!ok || !calm || lc < poly_free[pi] || (lc & 3u) || ((lc >> 2) & 1u) != pi || poly_len[(pi + 1u) % NSHOWN])
        return;                                          /* (a phrase's first loop; the parts take the phrases in turn; never both) */
    h = sqm_hash(sqm[pi].tseed ^ (uint32_t)morph.seed * 0x9E3779B1u, lc, 0x70u);
    if (h % 100u >= PCT[mode])
        return;
    poly_len[pi] = (uint8_t)(len - 1u - (h >> 8) % (mode == 2u ? 4u : 2u));
    poly_end[pi] = (uint16_t)(lc + SPAN[mode]);
}
/* the sequencer steps from seq_idx: the next one (audio path, seq_tick) */
static uint32_t tide_seq_next(track_t *t)
{
    uint32_t pi = trk_index(t), len = tide_poly_full(t), cur, ref;
    if (pi < NSHOWN && arp_mode(t) != AM_SEQ_ID) {
        ref = poly_ref[pi] + 1u;
        ref = ref >= len ? 0u : ref;
        poly_ref[pi] = (uint8_t)ref;
        if (!ref) {
            uint32_t was = poly_len[pi];
            tide_poly_wrap(pi, len);
            if (poly_len[pi] != was)
                return 0;                                /* (the full loop's first step: where a stretch begins or ends; inside one the short loop goes on) */
        } else if (poly_len[pi] && song.rec) {           /* audit C10: REC ends a stretch at once, on the full loop's own step (recorded notes land where they belong) */
            poly_len[pi] = 0;
            poly_free[pi] = (uint16_t)(poly_lc[pi] + 8u);
            return ref;
        }
    }
    cur = tide_slen(t);
    return ((uint32_t)t->seq_idx + 1u) % cur;
}
/* a pass of the sequencer's loop ends (the phrase lifecycle counts the full loops) */
static int tide_seq_wrapping(const track_t *t, uint32_t len)
{
    uint32_t pi = trk_index(t);
    if (pi < NSHOWN && poly_len[pi] && arp_mode(t) != AM_SEQ_ID)
        return poly_ref[pi] + 1u == tide_poly_full(t);
    return (uint32_t)t->seq_idx + 1u == len;
}
/* ARP SEQ: the step arp_idx k plays, and the arp step having started (arp_fire) */
static uint32_t tide_arp_idx(const track_t *t, uint32_t k, uint32_t len)
{
    uint32_t pi = trk_index(t);
    return pi < NSHOWN && poly_len[pi] && poly_len[pi] < len ? (k - poly_base[pi]) % poly_len[pi] : k % len;
}
static void tide_arp_tick(track_t *t)
{
    uint32_t pi = trk_index(t), len = tide_poly_full(t), was;
    if (pi >= NSHOWN || arp_mode(t) != AM_SEQ_ID || t->arp_idx % len)
        return;
    was = poly_len[pi];
    tide_poly_wrap(pi, len);
    if (poly_len[pi] && !was)
        poly_base[pi] = t->arp_idx;
}
