/* SPDX-License-Identifier: GPL-3.0-only */
/* ARC (TIDE LIST, M4/M9; docs/design/TIDE_NEXT_PROPOSALS.md step 3). Included by tide_seq.c after seq_answer.c.
 * One shape over 36-64 bars, counted in the shared 4-bar phrases of sqm_bar (the clock TURNS and LEAD read), for the parts in automatic
 * WNDR / JRNY (sqm_auto): RISE 4-8 phrases (syncopation, repeats, faster turns, brighter sounds), PEAK 1, DROP (both parts start no
 * notes for its first 2 bars; ties and the effects' tails ring), BREAK 2-4 (both parts thinned, softer sounds), RECOVER 1-2 (the
 * thinning lifts one part at a time). ARC RISE has no drop of its own; ARC DROP drops by itself on about half of the arcs; the hand
 * (OCT- + K2 on the TIDE page, tarc_drop_hand) drops at the next phrase in either. Playback masks only: KEEP stores the whole pattern,
 * nothing here is saved, a power cycle starts at RISE. ARC OFF: every lever neutral, as before. The planner is main loop only; the
 * audio interrupt reads tarc_dropphr, tarc_thin, tarc_fast (seq_morph.c seq_play_step) and tarc_hold (seq_poly.c). */
enum { AP_RISE, AP_PEAK, AP_DROP, AP_BREAK, AP_RECOVER };
static struct { uint8_t last, phase, left, len, next, t, hand, cnt, run, phr; } tarc;

static uint32_t tarc_rnd(uint32_t salt) { return sqm_hash(((uint32_t)morph.seed << 8) ^ tarc.cnt, (uint32_t)tarc.phr * 16u + tarc.run, 0xA4C0u + salt); }
static uint32_t tarc_next_of(uint32_t ph)
{
    switch (ph) {
    case AP_RISE: return AP_PEAK;
    case AP_PEAK: return sqm_arc == 2u && (tarc_rnd(1u) & 1u) ? AP_DROP : AP_BREAK;   /* (DROP: about half of the arcs) */
    case AP_DROP: return AP_BREAK;
    case AP_BREAK: return AP_RECOVER;
    default: return AP_RISE;
    }
}
static void tarc_begin(uint32_t ph)
{
    uint32_t h = tarc_rnd(2u), len = ph == AP_RISE ? 4u + h % 5u : ph == AP_PEAK ? (sqm_arc == 2u ? 1u + h % 3u : 1u) : ph == AP_DROP ? 1u :
                                     ph == AP_BREAK ? 2u + h % 3u : 1u + h % 2u;
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
    tarc.t = (uint8_t)(ph == AP_RISE ? (el + 1u) * 255u / tarc.len : ph == AP_PEAK ? 255u : ph == AP_RECOVER ? (el + 1u) * 100u / tarc.len : 0u);
    tarc_fast = tarc.t >= 200u;
    tarc_thin = (uint8_t)(auto_mask & (ph == AP_BREAK || ph == AP_DROP ? 3u : ph == AP_RECOVER && !el ? 1u << (tarc.cnt & 1u) : 0u));
    tarc_hold = ph == AP_PEAK || ph == AP_DROP || tarc_dropphr != 0xFFu;
}
static void tarc_poll(void)
{
    uint32_t cur = (uint32_t)(sqm_bar >> 2) & 63u, am = sqm_auto;
    if (!sqm_arc || !am) {
        tarc.run = 0;
        tarc_clear();
        return;
    }
    if (!song.playing)
        return;
    if (!tarc.run) {                                     /* a session starts at RISE */
        tarc.run = 1, tarc.last = (uint8_t)cur, tarc.phr = 0;
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
        return sqm_arc ? "NO AUTO TIDE YET" : "ARC OFF";
    if (tarc.hand && tarc.phase != AP_DROP) {
        tarc.hand = 0;
        tarc_dropphr = 0xFFu;
        tarc.next = (uint8_t)tarc_next_of(tarc.phase);
        return "DROP CANCELLED";
    }
    if (tarc.phase == AP_DROP)
        return "DROPPING";
    tarc.hand = 1;
    tarc.next = AP_DROP;
    if (tarc.left > 1u)
        tarc.left = 1;                                   /* (the phase ends with this phrase) */
    return "DROP NEXT PHRASE";
}
/* the levers the phrase edits and the sounds read (main loop) */
static uint32_t tarc_sync(void) { return sqm_sync == 1u && sqm_arc && sqm_auto && tarc.t >= 160u && tarc.phase <= AP_PEAK ? 2u : sqm_sync; }
static uint32_t tarc_cap(uint32_t base)                  /* the density bound: base/4 more, base/2 at the peak */
{
    uint32_t q = sqm_arc && sqm_auto && tarc.t >= 200u && tarc.phase <= AP_PEAK ? base / 2u : base / 4u;
    return base + (q ? q : 1u);
}
static int32_t tarc_bright(void)                         /* +1 brighter, -1 softer, 0 neither */
{
    if (!sqm_arc || !sqm_auto || !tarc.run)
        return 0;
    return tarc.phase == AP_PEAK || (tarc.phase == AP_RISE && tarc.t >= 128u) ? 1 : tarc.phase == AP_BREAK || tarc.phase == AP_DROP ? -1 : 0;
}
static const char *tarc_name(void) { static const char *const N[5] = {"RISE", "PEAK", "DROP", "BREAK", "RECOVER"}; return N[tarc.phase % 5u]; }
