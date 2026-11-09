/* SPDX-License-Identifier: GPL-3.0-only */
/* ARC (TIDE LIST, M4/M9; docs/design/TIDE_NEXT_PROPOSALS.md step 3). Included by tide_seq.c after seq_answer.c.
 * One shape over 40-64 bars, counted in 8-bar units of sqm_bar (MR4: every phase starts on a bar = 0 mod 8), for the parts in automatic
 * WNDR / JRNY (sqm_auto): RISE 2-4 units (syncopation, repeats, brighter sounds), PEAK 1-2, DROP (both parts start no
 * notes for its first 2 bars; ties and the effects' tails ring; it is the breakdown's first unit), BREAK 1-2 with the drop (both parts down to
 * their anchors and beats, a chord part to two voices, softer sounds: seq_answer.c), RECOVER 1 (one part still thinned for its first 4 bars).
 * ARC RISE has no drop of its own; ARC DROP drops on the first arc after PLAY, then on about half; the hand
 * (OCT- + K2 on the TIDE page, tarc_drop_hand) drops at the next unit in either. Playback masks only: KEEP stores the whole pattern,
 * nothing here is saved, a power cycle starts at RISE. STOP and PLAY leave the arc where it was; a load or a K1 stop starts it again. ARC OFF: every lever neutral, as before. The planner is main loop only; the
 * audio interrupt reads tarc_dropphr, tarc_thin, tarc_fast (seq_morph.c seq_play_step) and tarc_hold (seq_poly.c). */
enum { AP_RISE, AP_PEAK, AP_DROP, AP_BREAK, AP_RECOVER };
static struct { uint8_t last, phase, left, len, next, t, hand, cnt, run, phr, hleft, au; } tarc;   /* hleft: the units left before a hand drop shortened the phase; au: the arc's units so far */

/* the dice: the roll's seed and the session's (morph.gen: S3, so the arcs differ from night to night the way the notes do) */
static uint32_t tarc_rnd(uint32_t salt) { return sqm_hash(((uint32_t)morph.seed << 8) ^ tarc.cnt ^ morph.gen, (uint32_t)tarc.phr * 16u + tarc.run, 0xA4C0u + salt); }
static uint32_t tarc_next_of(uint32_t ph)
{
    switch (ph) {
    case AP_RISE: return AP_PEAK;
    case AP_PEAK: return sqm_arc == 2u && (tarc.cnt <= 2u || (tarc_rnd(1u) & 1u)) ? AP_DROP : AP_BREAK;   /* (DROP: the first arc after PLAY, then about half) */
    case AP_DROP: return AP_BREAK;
    case AP_BREAK: return AP_RECOVER;
    default: return AP_RISE;
    }
}
/* MR4: counted in 8-bar units, so every phase starts on a bar = 0 (mod 8): RISE 2-4, PEAK 1-2, BREAK 1-2 (a DROP is its first unit; the
 * drop's silence its first 2 bars), RECOVER 1; one arc 40-64 bars (a long rise leaves the peak and the breakdown one unit each) */
static void tarc_begin(uint32_t ph)
{
    uint32_t h = tarc_rnd(2u), len = ph == AP_RISE ? 2u + h % 3u : ph == AP_PEAK || ph == AP_BREAK ? 1u + h % 2u : 1u;
    if (ph == AP_RISE)
        tarc.au = 0;
    if (ph == AP_BREAK && tarc.phase == AP_DROP)
        len--;                                           /* (the drop's unit was the breakdown's first) */
    if (len && tarc.au + len + (ph == AP_PEAK ? 2u : 1u) > 8u)
        len--;                                           /* (room for what follows: the breakdown and RECOVER after a peak, RECOVER after a breakdown) */
    if (!len)
        ph = AP_RECOVER, len = 1u;
    tarc.au = (uint8_t)(tarc.au + len);
    tarc.phase = (uint8_t)ph;
    tarc.left = tarc.len = (uint8_t)len;
    tarc.cnt++;
    tarc.next = (uint8_t)tarc_next_of(ph);
    tarc.hand = 0;
}
static void tarc_clear(void)
{
    tarc_dropphr = 0xFFu, tarc_thin = 0, tarc_fast = 0, tarc_hold = 0;
    tarc.t = 0, tarc.hand = 0;
}
/* the levers of the phase and the phrase now */
static void tarc_levers(uint32_t auto_mask)
{
    uint32_t el = (uint32_t)tarc.len - tarc.left, ph = tarc.phase;   /* phrases of the phase done */
    uint8_t th;
    tarc.t = (uint8_t)(ph == AP_RISE ? (el + 1u) * 255u / tarc.len : ph == AP_PEAK ? 255u : ph == AP_RECOVER ? (el + 1u) * 100u / tarc.len : 0u);
    th = (uint8_t)(auto_mask & (ph == AP_BREAK || ph == AP_DROP ? 3u : ph == AP_RECOVER && !el && !(sqm_bar & 4u) ? 1u << (tarc.cnt & 1u) : 0u));   /* (RECOVER: one part still thinned for its first 4 bars) */
    MORPH_LOCK();                                        /* (audit C12: the three levers change together for the audio interrupt) */
    tarc_fast = tarc.t >= 200u;
    tarc_thin = th;
    tarc_hold = ph == AP_PEAK || ph == AP_DROP || ph == AP_BREAK || tarc_dropphr != 0xFFu;   /* (B1: no POLY stretch in the breakdown either) */
    MORPH_UNLOCK();
}
static void tarc_poll(void)
{
    uint32_t cur = (uint32_t)(sqm_bar >> 3) & 63u, am = sqm_auto;   /* (the 8-bar unit now: MR4) */
    if (!sqm_arc || (!am && !glide_tide_on()))
        tarc.run = 0;                                    /* (no tide to follow: a K1 stop, a lane taken by hand; STOP / PLAY, REC and the launch wait only pause the arc) */
    if (!am || !sqm_arc) {                                /* (ARC OFF: every lever neutral at once, a drop under way too) */
        tarc_clear();
        return;
    }
    if (!tarc.run) {                                     /* a session starts at RISE */
        tarc.run = 1, tarc.last = (uint8_t)cur, tarc.phr = 0, tarc.cnt = 0;   /* (cnt 0: under ARC DROP the first arc drops) */
        tarc_begin(AP_RISE);
    } else if (cur != tarc.last) {
        uint32_t adv = ((cur - tarc.last) & 63u) == 1u;  /* (PLAY took the bar count back to 0: resynchronise, no phrase) */
        tarc.last = (uint8_t)cur;
        if (adv) {
            tarc.phr++;
            if (tarc.left > 1u)
                tarc.left--;
            else {
                tarc_begin(tarc.next);
                if (tarc.phase == AP_DROP && tarc_dropphr == 0xFFu)
                    tarc_dropphr = (uint8_t)cur;         /* (late: the hand came in the last moment) */
            }
        }
    }
    if (tarc.phase != AP_DROP && tarc.left == 1u && tarc.next == AP_DROP) {
        tarc_dropphr = (uint8_t)((cur + 1u) & 63u);      /* the audio side knows the phrase ahead of the boundary (a hand that comes in the last moment: set late, below) */
    } else if (cur != tarc_dropphr)
        tarc_dropphr = 0xFFu;
    tarc_levers(am);
}
/* OCT- + K2 on the TIDE page: a drop at the next phrase; a second turn before it cancels (main loop) */
static const char *tarc_drop_hand(void)
{
    if (!sqm_arc || !sqm_auto)
        return sqm_arc ? "DROP NEEDS WANDER/JOURNEY" : "ARC OFF";
    if (tarc.hand && tarc.phase != AP_DROP) {
        tarc.hand = 0;
        tarc_dropphr = 0xFFu;
        tarc.next = (uint8_t)tarc_next_of(tarc.phase);
        tarc.left = tarc.hleft;                          /* (audit C6: the phase gets its phrases back) */
        return "DROP CANCELLED";
    }
    if (tarc.phase == AP_DROP)
        return "DROPPING";
    tarc.hand = 1;
    tarc.next = AP_DROP;
    tarc.hleft = tarc.left;
    if (tarc.left > 1u)
        tarc.left = 1;                                   /* (the phase ends with this phrase) */
    return "DROP NEXT PHRASE";
}
/* the levers the phrase edits and the sounds read (main loop) */
static uint32_t tarc_sync(void) { return sqm_sync == 1u && sqm_arc && sqm_auto && tarc.t >= 160u && tarc.phase <= AP_PEAK ? 2u : sqm_sync; }
static uint32_t tarc_cap(uint32_t base)                  /* the density bound: base/4 more, base/2 at the peak */
{
    uint32_t q;
    if (base <= 2u)
        return base;                                     /* (a sustained part, two chords a bar at most, gains no onset: the anchors already hold them) */
    q = sqm_arc && sqm_auto && tarc.t >= 200u && tarc.phase <= AP_PEAK ? base / 2u : base / 4u;
    return base + (q ? q : 1u);
}
static uint32_t tarc_need(void)                          /* the phrase edits' cadence: every loop in the rise, every 4th in the breakdown, else every 2nd (M09) */
{
    if (!sqm_arc || !sqm_auto || !tarc.run)
        return 2u;
    return tarc.phase == AP_RISE ? 1u : tarc.phase == AP_BREAK || tarc.phase == AP_DROP ? 4u : 2u;
}
static int32_t tarc_bright(void)                         /* +1 brighter, -1 softer, 0 neither */
{
    if (!sqm_arc || !sqm_auto || !tarc.run)
        return 0;
    return tarc.phase == AP_PEAK || (tarc.phase == AP_RISE && tarc.t >= 128u) ? 1 : tarc.phase == AP_BREAK || tarc.phase == AP_DROP ? -1 : 0;
}
static const char *tarc_name(void) { static const char *const N[5] = {"RISE", "PEAK", "DROP", "BREAK", "RECOVER"}; return N[tarc.phase % 5u]; }
/* TIDE hook H18: a multi, project or pattern load into the parts of mask. The tide starts as written (no generation, no pitch edits, D6) and the
 * arc at RISE; a trip that runs starts again from A (sqm_restart: glide_poll) */
static void tide_loaded(uint32_t mask)
{
    for (uint32_t k = 0; k < NSHOWN; k++)
        if ((mask >> k) & 1u)
            seqm_home(k);
    tarc.run = 0;
    sqm_restart = 1;
}
