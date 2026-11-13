/* SPDX-License-Identifier: GPL-3.0-only */
/* TAKE + ANSWER (TIDE, b10 round 8: live performance assist, phase 1; work/b10/specs/PERF_PHASE1.md, work/b10/reports/PERF_ASSIST.md). Main loop only: nothing here runs in the audio interrupt
 * and the notes of the answer are written with the interrupt off, a whole part's steps at a time.
 *   A TAKE is one pass of the live recorder (seq.c rec_put): the part's REC bit goes up (the pass opens: both parts' steps are kept) and down (it closes). A pass that wrote something is a take:
 *   it goes on a history of the last 2 takes, and ANSWER (TIDE SETTINGS) writes a BASS LINE, a DOUBLE, a REPLY, an ECHO or the CHORDS of the take into the other part's pattern (ANSWER ON). b10 assist2 (work/b10/reports/PERF_ASSIST_REVIEW.md):
 *   the loop is the part's own LEN (REC arms and waits, the first key starts it: seq.c rec_wait), every answer fits inside that LEN (REPLY: the first half calls, the second answers; ECHO wraps round the loop), the answer is
 *   written in the other part on the next bar line (tk_pend, tk_land), an ARP SEQ part that holds a take or an answer plays single notes. OCT- held + a PRESETS turn goes back (left) and forward (right) along
 *   the history while the newest edit is a take (tk_fresh); a ROLL (morph_do MQ_ROLL, the gesture and word TIDE has) on the answering part writes the answer again with the roll's seed (no history step; also while REC is armed).
 *   The generators are the host prototype's (tools/perf/perf_probe.c): integer only, deterministic, the same seed the same answer.
 * Pool: 4 snapshots of both parts' steps (4.6 KB), the take's events and the answer's steps (1.1 KB). */
enum { TA_OFF, TA_BASS, TA_DOUBLE, TA_REPLY, TA_ECHO, TA_CHORDS, TA_N };
static volatile uint8_t tide_answer __attribute__((section(".pool")));           /* TIDE SETTINGS ANSWER: what the other part is given when a take closes (a fresh unit: BASS LINE) */
static volatile uint8_t tide_answer_on __attribute__((section(".pool")));                  /* TIDE SETTINGS ANSWER ON: 0 the other part, 1 OFF */

typedef struct {                                         /* the answer that is live now (it travels with a history state) */
    uint8_t ans;                                         /* the part that holds it + 1, 0 none */
    uint8_t src, mode, key, flags;                       /* the take's part; TA_*; the key's root (0..11); bit 0 minor, bit 1 guessed, bit 2 scale named, bits 3.. the SCL index */
    uint16_t seed, roll;                                 /* the seed it was written with; the rolls since the take (the header says ROLL n) */
    uint32_t sig;                                        /* steps_sig of the answer's part right after it was written: an edit since ends the roll */
    uint8_t who;                                         /* b11: the newest take's part + 1 (0: none, or both parts); travels with the history state, so an undo can never leave a stale identity */
    uint16_t take;                                       /* b11: that take's number (tk_total); tk_orig is only valid for it */
} tk_meta_t;
typedef struct {
    step_t s[2][NSTEP];
    uint8_t len[2];
    uint8_t gr[2][3];                                    /* each part's SDIV, ARATE and AMODE (an answer sets the grid of the part that holds it, and an ARP SEQ part plays single notes) */
    tk_meta_t m;
} tk_snap_t;
static tk_snap_t tk_h[3] __attribute__((section(".pool")));      /* history: tk_h[j] the state before step j + 1 (a take or a roll) */
static tk_snap_t tk_pre __attribute__((section(".pool")));       /* the state when the pass opened (or before the roll) */
static step_t tk_out[NSTEP] __attribute__((section(".pool")));   /* the answer being made */
typedef struct { uint8_t t, len, note, vel; } tk_ev_t;
#define TK_MAXE 96u
static tk_ev_t tk_ev[TK_MAXE] __attribute__((section(".pool")));
static uint32_t tk_nev __attribute__((section(".pool")));
static tk_meta_t tk_m __attribute__((section(".pool")));                                   /* the live answer's */
static uint16_t tk_hn[2] __attribute__((section(".pool")));                                /* history step j: its number */
static uint8_t tk_cnt __attribute__((section(".pool"))), tk_pos __attribute__((section(".pool"))), tk_open __attribute__((section(".pool")));   /* (tk_fresh_v ..: tide.h) */
static uint8_t tk_pend __attribute__((section(".pool"))), tk_pg __attribute__((section(".pool"))), tk_pplain __attribute__((section(".pool")));   /* the answer waits for the bar line: its part + 1 (0 none), the grid it sets (255 none), the part leaves ARP SEQ */
static uint16_t tk_total __attribute__((section(".pool")));
static uint32_t tk_mh __attribute__((section(".pool"))), tk_mu __attribute__((section(".pool"))), tk_lsig __attribute__((section(".pool")));   /* the header message held (its hash, until when: fm1_ms); the part's steps at the last live answer */
#define tk_who (tk_m.who)
static uint8_t tk_force __attribute__((section(".pool"))), tk_prest __attribute__((section(".pool")));   /* a SWELL (1) / RIPPLE (2) while REC is armed or waiting: made at the end of the armed pass; the pending "answer" is the part's old pattern coming back */
static uint16_t tk_orig_no __attribute__((section(".pool")));                           /* the take tk_orig belongs to */
static uint8_t tk_sw_k __attribute__((section(".pool"))), tk_sw_p __attribute__((section(".pool")));   /* b11 SWELL / RIPPLE waiting for the loop start: 1 SWELL, 2 RIPPLE (0 none); the part it was pressed on + 1 */
static uint16_t tk_sw_seed __attribute__((section(".pool")));                            /* its seed (never 0) */
static struct { step_t s[NSTEP]; uint8_t len, gr[3]; } tk_orig __attribute__((section(".pool")));   /* the answering part as it was before the newest take answered it (ANSWER OFF puts it back) */
static uint8_t tk_prg[3] __attribute__((section(".pool")));                                /* that old pattern's SDIV, ARATE, AMODE */
static uint8_t tk_wd __attribute__((section(".pool"))), tk_live __attribute__((section(".pool")));            /* this pass's answer is made; an answer was made while the pass was open */
#define tk_fresh tk_fresh_v

/* ---- the key: SCL (scale_g, root_g), else the take's own (guessed) ---- */
static __attribute__((noinline)) int32_t tk_wrap(int32_t x) { return ((x % 12) + 12) % 12; }   /* a pitch class, whatever the sign (one copy of the division) */
static uint8_t tk_deg[8] __attribute__((section(".pool"))), tk_nd __attribute__((section(".pool"))), tk_kr __attribute__((section(".pool"))), tk_km __attribute__((section(".pool"))), tk_guess __attribute__((section(".pool")));
static const uint8_t TK_SMAJ[7] = {0, 2, 4, 5, 7, 9, 11}, TK_SMIN[7] = {0, 2, 3, 5, 7, 8, 10};
static __attribute__((noinline)) void tk_key(void)
{
    uint32_t mask = scale_mask(&trk[0]), i, nd = 0;
    tk_guess = 0;
    {   /* the multi's own key (duell_multis.h DF_KEY: tools/factory_multis.py KEYS), when SCL is chromatic */
        uint32_t k = project_key();
        if (mask == 0xFFFu && k != 0xFFu) {
            tk_kr = (uint8_t)(k & 15u);
            tk_km = (uint8_t)(k >> 4);
            for (i = 0; i < 7u; i++)
                tk_deg[i] = tk_km ? TK_SMIN[i] : TK_SMAJ[i];
            tk_nd = 7;
            return;
        }
    }
    if (mask != 0xFFFu) {
        tk_kr = (uint8_t)((uint32_t)root_g % 12u);
        for (i = 0; i < 12u && nd < 8u; i++)
            if ((mask >> i) & 1u)
                tk_deg[nd++] = (uint8_t)i;
        tk_nd = (uint8_t)nd;
        tk_km = (uint8_t)(((mask >> 3) & 1u) && !((mask >> 4) & 1u));
        return;
    }
    {   /* the take alone says: the pitch classes weighed by length and beat; a tonic, its fifth and its third (minor or major) count, the other third and the semitones above the tonic and the tritone count against */
        int32_t h[24] = {0}, best = -0x7FFFFFFF;
        uint32_t r, m;
        for (i = 0; i < tk_nev; i++)
            h[tk_ev[i].note % 12u] += (int32_t)tk_ev[i].len * 4 + (tk_ev[i].t % 4u == 0u ? 2 : 0);
        for (i = 0; i < 12u; i++)
            h[i + 12u] = h[i];
        for (m = 0; m < 2u; m++)
            for (r = 0; r < 12u; r++) {
                int32_t s = 3 * h[r] + 2 * h[r + 7] + 2 * h[r + 4 - m] - 3 * h[r + 3 + m] - 2 * h[r + 1] - 2 * h[r + 6];
                if (s > best) {
                    best = s;
                    tk_kr = (uint8_t)r;
                    tk_km = (uint8_t)m;
                }
            }
        for (i = 0; i < 7u; i++)
            tk_deg[i] = tk_km ? TK_SMIN[i] : TK_SMAJ[i];
        tk_nd = 7;
        tk_guess = 1;
    }
}
static int32_t tk_in_key(int32_t n)
{
    int32_t pc = tk_wrap(n - (int32_t)tk_kr);
    uint32_t i;
    for (i = 0; i < tk_nd; i++)
        if (tk_deg[i] == pc)
            return 1;
    return 0;
}
static int32_t tk_deg_of(int32_t n, int32_t *oct)        /* the scale degree at or below the note, and its octave */
{
    int32_t rel = n - (int32_t)tk_kr, o = rel >= 0 ? rel / 12 : -((11 - rel) / 12), pc = rel - o * 12, d = 0;
    uint32_t i;
    for (i = 0; i < tk_nd; i++)
        if (tk_deg[i] <= pc)
            d = (int32_t)i;
    *oct = o;
    return d;
}
static int32_t tk_note_of(int32_t d, int32_t o)
{
    int32_t nd = (int32_t)tk_nd;
    while (d < 0) { d += nd; o--; }
    while (d >= nd) { d -= nd; o++; }
    return (int32_t)tk_kr + o * 12 + tk_deg[d];
}
static int32_t tk_triad_pc(int32_t d, int32_t i) { return tk_wrap((int32_t)tk_kr + tk_deg[(d + 2 * i) % (int32_t)tk_nd]); }

/* ---- the take as events ---- */
/* b10 (rc6.4-test: an answer on an ARP SEQ part played at the wrong speed or not at all): a part's steps sit on its own grid - the sequencer's division, or on an ARP SEQ part the arp's rate (seq.c arp_dur) - so a take is read
 * in the grid of the part it was recorded on and the answer is written in the grid of the part that plays it (tk_put converts; equal grids: no change) */
static uint32_t tk_gi(const track_t *t) { return (uint32_t)(arp_mode(t) == AM_SEQ_ID ? P_ARATE : P_SDIV); }   /* the parameter that sets the part's grid */
static uint32_t tk_gp(const track_t *t) { return div_samples((uint32_t)t->p[tk_gi(t)]); }
static uint32_t tk_ps __attribute__((section(".pool"))), tk_pd __attribute__((section(".pool"))), tk_slen __attribute__((section(".pool")));   /* the take's grid, the answer's grid (samples a step), the take's length in steps */
static uint32_t tk_spb(const track_t *t)                 /* steps in a bar on the part's grid (16 at 1/16) */
{
    uint32_t period = tk_gp(t), n = period ? (4u * click_beat_len() + period / 2u) / period : 16u;
    return n < 4u ? 4u : n > 64u ? 64u : n;
}
/* what a step note of the part sounds at: its TRN, and on an ARP SEQ part the steps are offsets from the stand-in's lowest note (seq.c rec_arp_note): + (that note - 60) */
static int32_t tk_poff(const track_t *t)
{
    uint32_t i, list[16], cnt = 0, root = 127;
    int32_t off = t->p[P_TRANS];
    if (arp_mode(t) == AM_SEQ_ID) {
        if (arp_list(t, list, &cnt) && cnt)
            for (i = 0; i < cnt && i < 16u; i++)
                root = list[i] < root ? list[i] : root;
        else
            root = 60;
        off += (int32_t)root - 60;
    }
    return off;
}
static __attribute__((noinline)) void tk_events(const track_t *t, uint32_t len)
{
    uint32_t i, k, l;
    int32_t off = tk_poff(t);
    tk_nev = 0;
    for (i = 0; i < len && i < NSTEP; i++) {
        const step_t *s = &t->step[i];
        if (s->time != ST_NOTE || !s->n)
            continue;
        for (l = 1; i + l < len && t->step[i + l].time == ST_TIE; l++)
            ;
        for (k = 0; k < s->n && k < 4u && tk_nev < TK_MAXE; k++) {
            int32_t n = (int32_t)s->note[k] + off;
            tk_ev[tk_nev].t = (uint8_t)i;
            tk_ev[tk_nev].len = (uint8_t)l;
            tk_ev[tk_nev].note = (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
            tk_ev[tk_nev++].vel = s->vel ? s->vel : 100u;
        }
    }
}
static uint32_t tk_hs(uint32_t seed, uint32_t a)         /* a hash of the seed and a place: the variations, the same for the same seed */
{
    uint32_t h = (seed + 1u) * 2654435761u ^ (a + 1u) * 40503u;
    h ^= h >> 15;
    h *= 2246822519u;
    return h ^ (h >> 13);
}
/* the take's notes sounding at step t: how many, their lowest */
static uint32_t tk_sounding(int32_t t, int32_t *lo)
{
    uint32_t i, c = 0;
    *lo = 999;
    for (i = 0; i < tk_nev; i++)
        if (tk_ev[i].t <= t && t < tk_ev[i].t + tk_ev[i].len) {
            c++;
            if (tk_ev[i].note < *lo)
                *lo = tk_ev[i].note;
        }
    return c;
}
/* a semitone, major seventh or minor ninth against a take note the new one overlaps */
static uint32_t tk_rubs(int32_t t, int32_t len, int32_t note)
{
    uint32_t i, r = 0;
    for (i = 0; i < tk_nev; i++) {
        const tk_ev_t *e = &tk_ev[i];
        int32_t iv = (int32_t)e->note - note;
        iv = iv < 0 ? -iv : iv;
        if (e->t < t + len && t < e->t + e->len && iv < 24 && (iv % 12 == 1 || iv % 12 == 11))
            r++;
    }
    return r;
}

/* ---- the answer's steps ---- */
static uint32_t tk_olen __attribute__((section(".pool")));                                 /* the answer's length in steps */
static void tk_put(int32_t t, int32_t len, int32_t note, uint32_t vel, uint32_t mono)
{
    step_t *s;
    int32_t i;
    if (t < 0 || t >= (int32_t)tk_slen || note < 0 || note > 127)
        return;
    if (tk_ps != tk_pd) {                                /* the answer's part runs on another grid: the same moments, its own steps */
        len = (int32_t)(((uint32_t)len * tk_ps + tk_pd / 2u) / tk_pd);
        t = (int32_t)((uint32_t)t * tk_ps / tk_pd);
        if (len < 1)
            len = 1;
        if (t >= (int32_t)tk_olen)
            return;
    }
    s = &tk_out[t];
    if (mono || s->time != ST_NOTE)
        memset(s, 0, sizeof *s);                         /* (a bass note is alone in its step; a tie gives way to a note) */
    if (s->n < 4u) {
        uint32_t k;
        for (k = 0; k < s->n && s->note[k] != note; k++)
            ;
        if (k == s->n)
            s->note[s->n++] = (uint8_t)note;
    }
    s->time = ST_NOTE;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    if (vel > 110u)
        s->flags |= SF_ACCENT;
    for (i = 1; i < len && t + i < (int32_t)tk_olen; i++) {
        step_t *q = &tk_out[t + i];
        if (!q->n && q->time == ST_NOTE)
            q->time = ST_TIE;
    }
}

/* ---- chords, a bar each: the diatonic triad whose notes the bar weighs most ---- */
static uint8_t tk_chord[4] __attribute__((section(".pool")));
static __attribute__((noinline)) void tk_chords(uint32_t bars, uint32_t spb)
{
    static const int8_t PMIN[7] = {3, 0, 1, 2, 1, 2, 2}, PMAJ[7] = {3, 1, 0, 2, 2, 2, 0};
    uint32_t b, d, i;
    int32_t tp[8][4];                                    /* (the pitch classes of the triad on each degree, and the one a semitone under its root) */
    for (d = 0; d < tk_nd; d++) {
        for (i = 0; i < 3u; i++)
            tp[d][i] = tk_triad_pc((int32_t)d, (int32_t)i);
        tp[d][3] = tk_wrap(tp[d][0] - 1);
    }
    for (b = 0; b < bars && b < 4u; b++) {
        int32_t best = -9999, bd = 0;
        for (d = 0; d < tk_nd; d++) {
            int32_t s = (d < 7u ? (tk_km ? PMIN : PMAJ)[d] : 0) * 4 + (b && tk_chord[b - 1] == d ? 3 : 0) + (b == 0 && d == 0 ? 4 : 0);
            if (tk_wrap(tp[d][2] - tp[d][0]) == 6)
                continue;                                /* (the diminished triad: no bass root) */
            for (i = 0; i < tk_nev; i++) {
                const tk_ev_t *e = &tk_ev[i];
                int32_t a = e->t > b * spb ? e->t : (int32_t)(b * spb), z = e->t + e->len < (int32_t)((b + 1u) * spb) ? e->t + e->len : (int32_t)((b + 1u) * spb), w, pc = e->note % 12;
                if (z <= a)
                    continue;
                w = (z - a) + (e->t % 4 == 0 ? 2 : 0) + (e->t % spb == 0u ? 2 : 0);
                s += pc == tp[d][0] ? 3 * w : pc == tp[d][1] || pc == tp[d][2] ? 2 * w : pc == tp[d][3] ? w / 2 : -w;
            }
            if (s > best) {
                best = s;
                bd = (int32_t)d;
            }
        }
        tk_chord[b] = (uint8_t)bd;
    }
}

/* cand as the bass plays it: in key, at least an octave under the take's lowest note sounding at t, and (or an octave lower) free of rubs; -1: it cannot */
static int32_t tk_fit(int32_t t, int32_t len, int32_t cand)
{
    int32_t plo, k;
    if (!tk_in_key(cand))
        return -1;
    if (tk_sounding(t, &plo))
        while (cand > plo - 12 && cand > 24)           /* (b10: never lower than C1: a take down in the bass register leaves no room under it, the bass stays audible or is not written) */
            cand -= 12;
    for (k = 0; k < 2; k++, cand -= 12)
        if (cand >= 24 && !tk_rubs(t, len, cand))
            return cand;
    return -1;
}
/* ---- BASS LINE: a template a genre (16 steps a bar: R root, O octave, F fifth, A approach into the next bar's root; the lengths in steps, g a whole bar) ---- */
static const char TK_BT[6][17] = {"R.......F.R.....", "..R...R...O...RA", "..RR..RR..RR..RA", "R.RO.RFR.R.OR.A.", "R.....O...R..F..", "R..............."};
static const char TK_BL[6][17] = {"6.......2.4.....", "..2...2...2...11", "..11..11..11..11", "1.11.111.1.11.1.", "3.....2...2..2..", "g..............."};
static const uint8_t TK_LO[6] = {28, 33, 33, 33, 31, 29};      /* the root's lowest note (E1 .. A1): it sits in lo .. lo + 11 */
static __attribute__((noinline)) void tk_bass(uint32_t bars, uint32_t spb, uint32_t g, uint32_t seed)
{
    uint32_t b, s, i;
    for (b = 0; b < bars && b < 4u; b++) {
        int32_t rpc = tk_triad_pc(tk_chord[b], 0), lo = TK_LO[g], root = lo + tk_wrap(rpc - lo), nroot_pc = tk_triad_pc(tk_chord[b + 1u < bars ? b + 1u : 0u], 0);
        uint32_t busy = 0, h = seed ? tk_hs(seed, b) : 0u, drop = (h & 7u) == 0u ? 1u + (h >> 3) % 15u : 99u, pop = ((h >> 6) & 7u) == 0u, pass = ((h >> 12) & 3u) == 0u, first = 1;   /* (first: nothing placed in this bar yet) */
        if (seed && ((h >> 9) & 3u) == 0u && root + 12 <= lo + 23)
            root += 12;                                  /* (a roll: this bar's root an octave up) */
        for (i = 0; i < tk_nev; i++)
            busy += tk_ev[i].t >= b * spb && tk_ev[i].t < b * spb + spb;
        for (s = 0; s < 16u; s++) {
            char c = TK_BT[g][s];
            int32_t t = (int32_t)(b * spb + (s * spb >> 4)), note, plo, o, d, j, len = TK_BL[g][s] == 'g' ? (int32_t)spb : TK_BL[g][s] >= '0' ? (int32_t)((uint32_t)(TK_BL[g][s] - '0') * spb >> 4) : 0;
            if (c == '.' && seed) {                      /* a roll adds a note to an empty step now and then: a passing note, a fifth, an approach */
                if (pass && s == 14u && TK_BT[g][15] == 'A')
                    c = 'P';
                else if (s == 8u && ((h >> 14) & 3u) == 0u)
                    c = 'F', len = (int32_t)(spb >> 2);
                else if (s == 15u && ((h >> 16) & 3u) == 0u)
                    c = 'A';
            }
            if (c == '.')
                continue;
            if (s == drop && c != 'A' && !first)
                continue;                                /* (a roll: one onset left out, never the bar's first) */
            if (c == 'R' && s && pop && !first) {
                c = 'O';                                 /* (a roll: the first root after the downbeat an octave up) */
                pop = 0;
            }
            if (len < 1)
                len = 1;
            if (busy >= 7u && s % 4u && c != 'A' && c != 'P') {   /* the melody is busy: the bass leaves its off-steps that land on a melody note */
                for (j = 0; j < (int32_t)tk_nev && tk_ev[j].t != t; j++)
                    ;
                if (j < (int32_t)tk_nev)
                    continue;
            }
            note = c == 'O' ? root + 12 : c == 'F' ? root + 7 : root;
            if (c == 'A' || c == 'P') {                  /* approach: the scale step next to the next root, from below if the root rises; passing: the step above the root */
                int32_t nr = lo + tk_wrap(nroot_pc - lo);
                if (c == 'P') {
                    d = tk_deg_of(root, &o);
                    note = tk_note_of(d + 1, o);
                } else if (nr == root)
                    note = root + 7;
                else {
                    d = tk_deg_of(nr, &o);
                    note = tk_note_of(nr > root ? d - 1 : d + 1, o);
                }
            }
            {   /* the note the bar's chord asks for, else the other chord tones: in key, an octave under the melody, never a semitone / major 7th / minor 9th against it; nothing fits: a rest */
                int32_t alt[4], a;
                alt[0] = note;
                alt[1] = root;
                alt[2] = root + 7;
                alt[3] = root + tk_wrap(tk_triad_pc(tk_chord[b], 1) - root);
                for (a = 0; a < 4 && (note = tk_fit(t, len, alt[a])) < 0; a++)
                    ;
                if (note < 0)
                    continue;
            }
            tk_put(t, len, note, c == 'A' || c == 'P' ? 80u : first ? 110u : 96u, 1u);
            first = 0;
        }
    }
}
/* ---- CHORDS (b13): the bar's triad (tk_chords) as a pad in the middle, C3..C5: 3 notes in the first step of the bar tied for the whole bar, dropped an octave under the take's lowest note when that sits in the range (never under G2),
 * the inversion that moves least from the bar before (bar 1: root position nearest C4), a note that rubs left out. A roll with bit 1 of the seed set plays the chords as short stabs in the genre's rhythm (digits: the length in
 * 16ths, as TK_BL; a genre with none, ACID and AMB, keeps the pad) ---- */
static const char TK_PT[6][17] = {"......2.........", "..2...2...2...2.", "...2......2.....", "", "2......2........", ""};
static __attribute__((noinline)) void tk_pad(uint32_t bars, uint32_t spb, uint32_t g, uint32_t seed)
{
    int32_t pv[3] = {0, 0, 0};
    uint32_t b, s, i, r, o, held = !(seed & 2u) || !TK_PT[g][0];
    for (b = 0; b < bars && b < 4u; b++) {
        int32_t pc[3], v[3], bv[3], plo = 999, best = 99999, st = (int32_t)(b * spb), en = st + (int32_t)spb;
        for (i = 0; i < 3u; i++)
            pc[i] = tk_triad_pc(tk_chord[b], (int32_t)i);
        for (i = 0; i < tk_nev; i++)                     /* (the take's lowest note sounding in the bar) */
            if (tk_ev[i].t < en && tk_ev[i].t + tk_ev[i].len > st && tk_ev[i].note < plo)
                plo = tk_ev[i].note;
        for (r = 0; r < 3u; r++)
            for (o = 0; o < 3u; o++) {
                int32_t c = 0, sh = 0;
                v[0] = 48 + pc[r] + 12 * (int32_t)o;     /* (the inversion r: its bottom note, then the next chord tones up) */
                v[1] = v[0] + tk_wrap(pc[(r + 1u) % 3u] - pc[r]);
                v[2] = v[1] + tk_wrap(pc[(r + 2u) % 3u] - pc[(r + 1u) % 3u]);
                if (v[2] > 72)
                    continue;
                while (plo >= 48 && v[2] + sh >= plo && v[0] + sh - 12 >= 43)
                    sh -= 12;
                for (i = 0; i < 3u; i++) {
                    v[i] += sh;
                    c += b ? (v[i] > pv[i] ? v[i] - pv[i] : pv[i] - v[i]) : 0;
                }
                if (!b)
                    c = r ? 999 : (v[1] > 60 ? v[1] - 60 : 60 - v[1]);
                if (c < best) {
                    best = c;
                    memcpy(bv, v, sizeof bv);
                }
            }
        memcpy(pv, bv, sizeof pv);
        for (s = 0; s < 16u; s++) {
            char ch = held ? (s ? '.' : 'g') : TK_PT[g][s];
            int32_t t = st + (int32_t)(s * spb >> 4), len = ch == 'g' ? (int32_t)spb : (int32_t)((uint32_t)(ch - '0') * spb >> 4);
            if (ch == '.')
                continue;
            for (i = 0; i < 3u; i++)
                if (!tk_rubs(t, len < 1 ? 1 : len, bv[i]))
                    tk_put(t, len < 1 ? 1 : len, bv[i], 80u, 0u);
        }
    }
}
/* ---- DOUBLE: the take's notes a diatonic third above, in time (a roll: an octave down on odd seeds) ---- */
static __attribute__((noinline)) void tk_double(uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < tk_nev; i++) {
        const tk_ev_t *e = &tk_ev[i];
        int32_t o, d = tk_deg_of(e->note, &o), n = tk_note_of(d + 2, o);
        if (tk_rubs(e->t, e->len, n))
            n = tk_note_of(d + 4, o);                    /* (a rub with another note of the take: the fifth degree above) */
        if (tk_rubs(e->t, e->len, n) || !tk_in_key(n))
            continue;
        if (seed & 1u)
            n -= 12;
        tk_put(e->t, e->len, n, e->vel, 0u);
    }
}
/* ---- REPLY: the first half of the loop is the call (the second half when he played only there), the other half the answer: the call's rhythm and notes moved in scale steps so the last note is the tonic, giving way where it
 * would rub with what he plays there; ECHO: the call's last note thrown 4 times (100, 80, 60, 40), wrapping round the loop ---- */
static __attribute__((noinline)) void tk_reply(uint32_t len, uint32_t seed)
{
    uint32_t i, half = len / 2u, lo, hi, last = 0, any = 0;
    int32_t o, nd = (int32_t)tk_nd, dl, shift, oc = seed % 3u == 1u ? -12 : seed % 3u == 2u ? 12 : 0, dt;
    for (i = 0; i < tk_nev; i++)
        any += tk_ev[i].t < half;
    lo = any ? 0u : half;
    hi = any ? half : len;
    dt = any ? (int32_t)half : -(int32_t)half;
    for (i = 0; i < tk_nev; i++)
        if (tk_ev[i].t >= lo && tk_ev[i].t < hi)
            last = i + 1u;
    dl = tk_deg_of(tk_ev[last - 1u].note, &o);
    shift = dl == 0 ? -2 : dl <= nd / 2 ? -dl : nd - dl;
    for (i = 0; i < tk_nev; i++) {
        int32_t d, n;
        if (tk_ev[i].t < lo || tk_ev[i].t >= hi)
            continue;
        d = tk_deg_of(tk_ev[i].note, &o);
        n = tk_note_of(d + shift, o);
        if (i + 1u == last) {                            /* ends home: the tonic nearest the note */
            d = tk_deg_of(n, &o);
            n = tk_note_of(0, d >= nd / 2 ? o + 1 : o);
        }
        n += oc;
        if (tk_rubs(tk_ev[i].t + dt, tk_ev[i].len, n))
            continue;                                    /* (gives way) */
        tk_put(tk_ev[i].t + dt, tk_ev[i].len, n, 96u, 0u);
    }
}
static __attribute__((noinline)) void tk_echo(uint32_t seed)
{
    const tk_ev_t *e = &tk_ev[tk_nev - 1u];
    uint32_t r, h = seed ? tk_hs(seed, 99u) % 3u : 0u, gap = h == 1u ? 4u : h == 2u ? 6u : 3u;   /* (a dotted eighth; a roll: a quarter or a dotted quarter) */
    for (r = 0; r < 4u; r++)
        tk_put((int32_t)((e->t + e->len + gap * r) % tk_slen), 2, e->note, 100u - 20u * r, 1u);
}
static uint32_t tk_genre(void)                           /* DUB HSE TECH ACID ELEC AMB: the multi's name, else STYLE */
{
    static const char *const G[6] = {"DUB", "HSE", "TECH", "ACID", "ELEC", "AMB"};
    char b[16];
    uint32_t g, k;
    project_cur_name(b);
    for (g = 0; g < 6u; g++) {
        for (k = 0; G[g][k] && b[k] == G[g][k]; k++)
            ;
        if (!G[g][k] && b[k] == ' ')
            return g;
    }
    return tide_style == STY_DUB ? 0u : tide_style == STY_DEEP ? 1u : tide_style == STY_PEAK ? 4u : tide_style == STY_AMBIENT || tide_style == STY_DRONE ? 5u : 2u;
}

/* the answer waits in tk_out for the next bar line (the last 1/16 of the bar on the transport's grid: seq.c clk_step); stopped, it lands at once */
static int tk_due(void) { return !(song.playing && clk_pos != CLK_START) || (clk_step & 15u) == 15u; }
static void tk_land(void)
{
    uint32_t dp = tk_pend - 1u, was = (uint32_t)trk[dp].p[P_SLEN], si;
    track_t *td = &trk[dp], *ts = &trk[1u - dp];        /* (the answer is always the other part's: ts holds the take) */
    fm1_irq_off();
    memcpy(td->step, tk_out, sizeof td->step);
    td->p[P_SLEN] = (int16_t)tk_olen;
    if (tk_pg != 255u) {
        if (tk_pplain)
            td->p[P_AMODE] = 0;
        td->p[P_SDIV] = tk_pg;
    }
    if (tk_prest) {
        td->p[P_SDIV] = tk_prg[0];
        td->p[P_ARATE] = tk_prg[1];
        td->p[P_AMODE] = tk_prg[2];
        tk_prest = 0;
    }
    /* b13 (hunt F1): a new loop length starts the answer where the take part is in its loop, so bar n of the answer plays under bar n of the take (it went on
     * from its old, shorter loop's step before: a 2-bar answer a bar out of step for good). Same length: the parts keep the phase they had */
    si = tide_play_idx(ts);
    if (tk_olen != was && si != 0xFFFFFFFFu && arp_mode(td) != AM_SEQ_ID && tk_gp(td)) {
        si = si * tk_gp(ts) / tk_gp(td) % tk_olen;          /* (on the answer's grid; 64 steps x a whole note's samples fit 32 bits) */
        td->seq_idx = (uint16_t)si;
        poly_ref[dp] = (uint8_t)si;
    }
    fm1_irq_on();
    tk_m.sig = steps_sig(td);
    tk_pend = 0;
}
/* make the answer to part src's pattern for part dst's, with the seed; 1 when something was made (it lands on the next bar line) */
static uint32_t tk_gen(uint32_t src, uint32_t dst, uint32_t seed, uint32_t mode)
{
    track_t *ts = &trk[src], *td = &trk[dst];
    uint32_t len = seq_len(ts), spb = tk_spb(ts), bars, i, n = 0, call = mode - TA_REPLY <= TA_ECHO - TA_REPLY, dlen, gset = 0, plain = arp_mode(td) == AM_SEQ_ID;
    int32_t shift = td->p[P_TRANS] - ts->p[P_TRANS];
    bars = (len + spb - 1u) / spb;
    if (bars > 4u)
        bars = 4u;
    if (call && len < 2u)
        return 0;
    tk_events(ts, len);
    if (!tk_nev)
        return 0;
    tk_key();
    tk_pend = 0;
    tk_prest = 0;
    memset(tk_out, 0, sizeof tk_out);
    tk_slen = len;
    tk_ps = tk_gp(ts);
    tk_pd = tk_gp(td);
    dlen = tk_ps == tk_pd || !tk_pd ? tk_slen : (tk_slen * tk_ps + tk_pd / 2u) / tk_pd;
    if (plain || (tk_pd && tk_pd != tk_ps && (tk_pd > tk_ps || dlen > NSTEP))) {   /* the answer wins: an ARP SEQ part plays single notes (it leaves ARP SEQ), and when the part's own grid is too coarse to hold the answer (or too fine to fit 64 steps) it is written on the take's: the part's DIV is set to match while the answer stands (the history step holds the old one) */
        gset = 1;
        tk_pd = tk_ps;
        dlen = tk_slen;
    }
    if (!dlen || dlen > NSTEP)
        return 0;
    if (!tk_pd)
        tk_pd = tk_ps;
    tk_olen = dlen;
    if (mode == TA_REPLY)
        tk_reply(len, seed);
    else if (mode == TA_ECHO)
        tk_echo(seed);
    else {
        tk_chords(bars, spb);
        if (mode == TA_BASS)
            tk_bass(bars, spb, tk_genre(), seed);
        else if (mode == TA_CHORDS)
            tk_pad(bars, spb, tk_genre(), seed);
        else
            tk_double(seed);
    }
    for (i = 0; i < tk_olen; i++)
        n += tk_out[i].n != 0;
    if (!n || (mode == TA_BASS && n < bars))
        return 0;                                        /* (nothing, or less than a note a bar: the other part keeps its pattern) */
    if (shift)                                           /* (the answer sounds as written: the other part's TRN is taken off) */
        for (i = 0; i < tk_olen; i++)
            for (n = 0; n < tk_out[i].n; n++) {
                int32_t v = (int32_t)tk_out[i].note[n] - shift;
                tk_out[i].note[n] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
            }
    tk_pend = (uint8_t)(dst + 1u);
    tk_pg = gset ? (uint8_t)ts->p[tk_gi(ts)] : 255u;
    tk_pplain = (uint8_t)plain;
    tk_m.ans = (uint8_t)(dst + 1u);
    tk_m.src = (uint8_t)src;
    tk_m.mode = (uint8_t)mode;
    tk_m.key = tk_kr;
    tk_m.flags = (uint8_t)(tk_km | (tk_guess << 1) | (tk_nd != 7u ? 4u | (uint32_t)scale_g << 3 : 0u));
    tk_m.seed = (uint16_t)seed;
    if (tk_due())
        tk_land();
    return 1;
}

/* ---- the header ---- */
/* b10 (owner: "the text on the top that adding bass only briefly visible"): a take / answer message stays in the header for 4 bars (4 s at least, 8 s at most) unless another message replaces it: take_poll keeps its frames up */
static void tk_hold(void)
{
    uint32_t bpm = (uint32_t)song.g[G_BPM];
    tk_mh = str_hash(7u, ui.msg);
    tk_mu = fm1_ms + (uint32_t)clamp((int32_t)(bpm ? 960000u / bpm : 8000u), 4000, 8000);
}
static void tk_cat(char *b, const char *s) { str_cpy(b + str_len(b), s, 48u - str_len(b)); }
static void tk_words(char *b);
static void tk_say(const char *pre, uint32_t no)
{
    char b[48], n[8];
    if (!pre && TIDE_HOME_ANSWER && PAGES[ui.page % NPAGES].graph == GR_TIDE)   /* (the page under a quick layer too: REC is pressed with SEQ held) */
        return;                                          /* (b12: a take's answer on AUTO TIDE: the title stays, knob 1's line says it, tide_ui.c tide_answer_line; a ROLL, SWELL or RIPPLE still says its own) */
    b[0] = 0;
    if (pre) {
        tk_cat(b, pre);
        if (no) {
            fmt_int(n, (int32_t)no);
            tk_cat(b, n);
        }
        tk_cat(b, "  ");
    }
    tk_words(b);
    ui_say(b, "");
    tk_hold();
}
/* the live answer in words, appended to b (48): "BASS LINE  A MINOR" (b12: AUTO TIDE's knob-1 line says it too, tide_ui.c tide_columns) */
static __attribute__((noinline)) void tk_words(char *b)
{
    tk_cat(b, tk_m.mode == TA_DOUBLE ? "DOUBLE  " : tk_m.mode == TA_REPLY ? "REPLY  " : tk_m.mode == TA_ECHO ? "ECHO  " : tk_m.mode == TA_CHORDS ? "CHORDS  " : "BASS LINE  ");
    tk_cat(b, N_NOTE[tk_m.key % 12u]);
    tk_cat(b, (tk_m.flags & 4u) ? " " : tk_m.flags & 1u ? " MINOR" : " MAJOR");
    if (tk_m.flags & 4u)
        tk_cat(b, N_SCALE[(tk_m.flags >> 3) & 15u]);
    if (tk_m.flags & 2u)
        tk_cat(b, "?");
}

/* ---- the history ---- */
static void tk_save(tk_snap_t *d)
{
    uint32_t p;
    for (p = 0; p < 2u; p++) {
        memcpy(d->s[p], trk[p].step, sizeof d->s[p]);
        d->len[p] = (uint8_t)trk[p].p[P_SLEN];
        d->gr[p][0] = (uint8_t)trk[p].p[P_SDIV];
        d->gr[p][1] = (uint8_t)trk[p].p[P_ARATE];
        d->gr[p][2] = (uint8_t)trk[p].p[P_AMODE];
    }
    d->m = tk_m;
}
static void tk_load(const tk_snap_t *d)
{
    uint32_t p;
    fm1_irq_off();
    for (p = 0; p < 2u; p++) {
        memcpy(trk[p].step, d->s[p], sizeof trk[p].step);
        trk[p].p[P_SLEN] = d->len[p];
        trk[p].p[P_SDIV] = d->gr[p][0];
        trk[p].p[P_ARATE] = d->gr[p][1];
        trk[p].p[P_AMODE] = d->gr[p][2];
    }
    fm1_irq_on();
    tk_m = d->m;
    tk_pend = 0;
    tk_sw_k = 0;
    tk_prest = 0;                                        /* (an answer that waited for its bar line is gone with the state it was made for) */
}
static void tk_push(uint32_t no)                         /* tk_pre is the state before this step; the tracks hold the state after it */
{
    if (tk_pos < tk_cnt)
        tk_cnt = tk_pos;                                 /* (a new step drops what could be redone) */
    if (tk_cnt == 2u) {
        memcpy(&tk_h[0], &tk_h[1], sizeof tk_h[0]);
        tk_hn[0] = tk_hn[1];
        tk_cnt = 1;
    }
    memcpy(&tk_h[tk_cnt], &tk_pre, sizeof tk_h[0]);
    tk_hn[tk_cnt++] = (uint16_t)no;
    tk_pos = tk_cnt;
    tk_fresh = 1;
}
/* OCT- held + a PRESETS turn while the newest edit is a take or a roll: left goes back one step, right forward (ui_input.c); 1 when it did */
static __attribute__((noinline)) uint32_t take_undo(int32_t dir)
{
    uint32_t j, hn;
    char b[48];
    if (!tk_fresh || (dir < 0 ? tk_pos == 0u : tk_pos >= tk_cnt))
        return 0;
    tk_sw_k = 0;                                         /* (a SWELL waiting for the loop start is dropped: the player went back) */
    if (tk_pend)
        tk_land();                                       /* (the state saved for a redo has the answer in it) */
    j = dir < 0 ? tk_pos - 1u : tk_pos;
    hn = tk_hn[j];
    tk_save(&tk_h[tk_pos]);
    tk_load(&tk_h[dir < 0 ? tk_pos - 1u : tk_pos + 1u]);
    tk_pos = (uint8_t)(dir < 0 ? tk_pos - 1u : tk_pos + 1u);
    b[0] = 0;
    tk_cat(b, dir < 0 ? "UNDO " : "REDO ");
    if (hn & 0xC000u)
        tk_cat(b, hn & 0x8000u ? "SWELL" : "RIPPLE");
    else {
        tk_cat(b, "TAKE ");
        fmt_int(b + str_len(b), (int32_t)(hn & 0x3FFFu));
    }
    ui_say(b, "");
    tk_hold();
    return 1;
}
#define TAKE_STALE() (tk_fresh = 0)                       /* a load or a lock edit took the undo copy: the newest edit is not a take any more (ui.c) */

/* ---- a ROLL of the part that holds the answer (morph.c morph_do MQ_ROLL: the knob, MIDI, the same word): no history step; while REC is armed too (the later remakes of the pass keep its seed) ---- */
static __attribute__((noinline)) void take_roll(uint32_t mask, uint32_t seed)
{
    uint32_t ap;
    tk_meta_t keep;
    if (!tk_m.ans || !((mask >> (tk_m.ans - 1u)) & 1u))
        return;
    ap = (uint32_t)tk_m.ans - 1u;
    if (!tk_pend && steps_sig(&trk[ap]) != tk_m.sig)
        return;                                          /* the answer was edited since: it is the player's now */
    keep = tk_m;
    tk_m.roll++;
    if (!tk_gen(tk_m.src, ap, (seed & 0x7FFFu) | 1u, tk_m.mode)) {   /* (a seed is never 0: 0 is the take's first answer) */
        tk_m = keep;
        return;
    }
    if (tk_open) {
        tk_live = 1;                                     /* (a pass is open: the answer made at its close, and its remakes, keep the rolled seed) */
        tk_lsig = steps_sig(&trk[tk_m.src]);
    }
    tk_say("ROLL ", tk_m.roll);
}

/* ---- SWELL and RIPPLE (b11; SEQ held + C4 / D4, ui_tools.c; were RANDOM and COOK): the player's buttons to make the answer again, never the menu. On the part that holds the answer: SWELL makes it again with a fresh seed in the mode ANSWER
 * shows now (OFF: the other part gets its old pattern back), RIPPLE moves 15..25 % of its notes inside the key and the rules, its rhythm kept. On the part that holds the take: the take changes (tl_sw_apply) and the answer is made again
 * from it. Both wait for the start of the next loop (the last step of the part's LEN pass; stopped: at once), a second press replaces the waiting one, and they are one history step like a take (OCT- + PRESETS: SWELL / RIPPLE).
 * Any other part: the stock tool with TIDE's rules, at once, also one history step. While REC is armed a SWELL forces the next pass's re-make with a fresh seed. ---- */
static int tk_stands(void) { return tk_fresh && tk_cnt && tk_pos == tk_cnt && tk_who; }
static uint32_t tk_fresh_seed(void) { return (tk_hs(tk_total + 1u + (uint32_t)tk_m.roll, fm1_ms) & 0x7FFFu) | 1u; }
/* RIPPLE of the answer: 15..25 % of the NOTE steps (at least one) move to a neighbouring degree of the take's key, free of rubs against the take; the rhythm stays. 1 when a note moved (waiting in tk_out for tk_land) */
static __attribute__((noinline)) uint32_t tk_ripple(uint32_t src, uint32_t dst, uint32_t seed)
{
    track_t *ts = &trk[src], *td = &trk[dst];
    uint32_t len = seq_len(td), n = 0, m, i, j, k, moved = 0, tries, same = tk_gp(ts) == tk_gp(td);
    int32_t poff = tk_poff(td);                          /* (the answer part's steps sound poff higher: key and rubs are about what sounds) */
    static const int8_t DD[4] = {1, -1, 2, -2};
    tk_events(ts, seq_len(ts));
    tk_key();
    memcpy(tk_out, td->step, sizeof tk_out);
    for (i = 0; i < len && i < NSTEP; i++)
        n += tk_out[i].time == ST_NOTE && tk_out[i].n;
    if (!n)
        return 0;
    m = n * (15u + tk_hs(seed, 99u) % 11u) / 100u;
    m = m ? m : 1u;
    for (j = 0, tries = 0; j < m && tries < 4u * m + 8u; tries++) {
        uint32_t h = tk_hs(seed, 100u + tries), pick = h % n, st = 0, ok = 0, dsel = (h >> 8) & 3u;
        step_t *sp;
        for (i = 0; i < len && i < NSTEP; i++)
            if (tk_out[i].time == ST_NOTE && tk_out[i].n && pick-- == 0u) {
                st = i;
                break;
            }
        sp = &tk_out[st];
        for (k = 0; k < 4u && !ok; k++) {
            int32_t d = DD[(dsel + k) & 3u], oc, dg, cand[4], x;
            uint32_t q, good = 1;
            for (q = 0; q < sp->n && q < 4u; q++) {
                int32_t sn = (int32_t)sp->note[q] + poff;
                dg = tk_deg_of(sn, &oc);
                x = tk_in_key(sn) ? tk_note_of(dg + d, oc) : tk_note_of(dg, oc);
                cand[q] = x - poff;
                if (x < 12 || x > 108 || cand[q] < 0 || cand[q] > 127 || (same && tk_rubs((int32_t)st, 1, x)))
                    good = 0;
            }
            if (good && sp->n) {
                for (q = 0; q < sp->n && q < 4u; q++)
                    sp->note[q] = (uint8_t)cand[q];
                ok = 1;
            }
        }
        if (ok) {
            moved++;
            j++;
        }
    }
    if (!moved)
        return 0;
    tk_olen = len;
    tk_pend = (uint8_t)(dst + 1u);
    tk_pg = 255u;
    tk_pplain = 0;
    tk_prest = 0;
    return 1;
}
/* the other part gets what it had before the newest take (ANSWER OFF): waiting in tk_out for tk_land */
static uint32_t tk_restore(uint32_t dst)
{
    if (tk_orig_no != tk_m.take)
        return 0;                                        /* (the pattern kept is another take's) */
    memcpy(tk_out, tk_orig.s, sizeof tk_out);
    tk_olen = tk_orig.len;
    tk_pg = 255u;
    tk_pplain = 0;
    tk_prg[0] = tk_orig.gr[0], tk_prg[1] = tk_orig.gr[1], tk_prg[2] = tk_orig.gr[2];
    tk_prest = 1;
    tk_pend = (uint8_t)(dst + 1u);
    tk_m.ans = 0;
    return 1;
}
static uint32_t tide_sw_pending(void) { return tk_sw_k; }
static int tk_sw_due(void)
{
    track_t *t = &trk[tk_sw_p - 1u];
    uint32_t len = TIDE_PLAY_LEN(t);
    return !song.playing || len < 2u || TIDE_PLAY_IDX(t) + 1u >= len;
}
/* the waiting SWELL / RIPPLE lands: one history step; the header says what was made */
static __attribute__((noinline)) void tk_sw_run(void)
{
    uint32_t ripple = tk_sw_k == 2u, pi = (uint32_t)tk_sw_p - 1u, seed = tk_sw_seed, src, dst, made = 0, on;
    tk_sw_k = 0;
    tk_mu = 0;                                           /* (the NEXT LOOP line is over: what lands says its own)*/
    if (!tk_stands())
        return;
    src = (uint32_t)tk_who - 1u;
    dst = 1u - src;
    if (tk_open || (song.rec & 3u)) {                    /* REC armed while it waited: not dropped, it lands at the end of the armed pass (the armed path makes the answer again) */
        tk_force = (uint8_t)(1u + ripple);
        tk_live = 1;
        tk_m.seed = (uint16_t)seed;
        ui_say(ripple ? "RIPPLE  NEXT PASS" : "SWELL  NEXT PASS", "");
        tk_hold();
        return;
    }
    on = tide_answer && !tide_answer_on;
    tk_save(&tk_pre);                                    /* (the state before this step) */
    if (pi == src) {                                     /* the take changes (a scratch copy, written with the interrupt off); the answer follows */
        if (!tl_sw_apply(&trk[src], ripple, seed, 1u))
            return;
        if (on)
            made = tk_gen(src, dst, seed, tide_answer);
        else if (tk_m.ans)
            (void)tk_restore(dst);
    } else if (on) {
        if (ripple) {
            made = tk_ripple(src, dst, seed);
            if (!made) {
                ui_message("NO NOTE TO MOVE");
                return;
            }
            tk_m.seed = (uint16_t)seed;
        } else {
            made = tk_gen(src, dst, seed, tide_answer);
            if (!made)
                return;
            tk_m.roll = 0;
        }
    } else if (!ripple && tk_m.ans) {
        if (!tk_restore(dst))
            return;
    } else {
        return;
    }
    if (tk_pend)
        tk_land();
    tk_push(ripple ? 0x4000u : 0x8000u);
    if (made)
        tk_say(ripple ? "RIPPLE" : "SWELL", 0);
    else {
        ui_say(ripple ? "RIPPLE" : "SWELL", on ? "" : "  ANSWER OFF");
        tk_hold();
    }
}
/* SEQ held + C4 / D4 on part t (the TIDE hook of ui_tools.c tl_do): the message to show, "" when this said it itself, 0 nothing happened */
static const char *tide_tl_swell(track_t *t, uint32_t ripple)
{
    uint32_t pi = trk_index(t), seed = tk_fresh_seed(), role = 0;
    if (pi > 1u)
        return "";
    if (tk_stands() && !((song.rec >> (1u - (tk_who - 1u))) & 1u)) {
        uint32_t src = (uint32_t)tk_who - 1u, dst = 1u - src;
        if (pi == src)
            role = 1;                                    /* the take's part */
        else if (pi == dst && tk_m.ans == dst + 1u && (tk_pend || steps_sig(t) == tk_m.sig))
            role = 2;                                    /* the answer's part, as the take made it */
    }
    if (tk_open) {                                       /* REC armed: the next pass's answer is made again (SWELL) or moved (RIPPLE) with a fresh seed */
        tk_force = (uint8_t)(1u + ripple);
        tk_live = 1;
        tk_m.seed = (uint16_t)seed;
        ui_say(ripple ? "RIPPLE  NEXT PASS" : "SWELL  NEXT PASS", "");
        tk_hold();
        return "";
    }
    if (role) {
        tk_sw_k = (uint8_t)(1u + ripple);
        tk_sw_p = (uint8_t)(pi + 1u);
        tk_sw_seed = (uint16_t)seed;
        if (tk_sw_due()) {                               /* stopped (or the last step now): at once */
            tk_sw_run();
        } else {
            ui_say(ripple ? "RIPPLE  NEXT LOOP" : "SWELL  NEXT LOOP", "");
            tk_hold();
            tk_mu = fm1_ms + 120000u;                    /* (b11: the line stays until it lands, however long the loop) */
        }
        return "";
    }
    /* any other part: the stock tool with TIDE's rules, at once, one history step */
    tk_save(&tk_pre);
    if (!tl_sw_apply(t, ripple, seed, 0u))
        return 0;
    tk_push(ripple ? 0x4000u : 0x8000u);
    ui_say(ripple ? "RIPPLE" : "SWELL", "");
    tk_hold();
    return "";
}

/* ---- the tide line and knob 1's line while a take records (b10 round 9, on the identity's tide line) ---- */
static uint32_t take_line(uint32_t *prog)                  /* 0 no take, 1 the count-in, 2 recording (prog: how far through the loop, Q16) */
{
    uint32_t p, len, idx;
    if (!(song.rec & 3u))
        return tk_sw_k ? 3u : 0u;                        /* (b11: 3, a SWELL / RIPPLE waits for the loop start: the line's accent dashes) */
    if (cin_left)
        return 1;
    if (!song.playing)
        return (rec_wait & song.rec & 3u) ? 1u : 0u;      /* (armed and waiting for the first key: the same dashes as a count-in) */
    p = song.rec & 1u ? 0u : 1u;
    len = TIDE_PLAY_LEN(&trk[p]);
    idx = TIDE_PLAY_IDX(&trk[p]);
    *prog = len ? (idx % len) * 65536u / len : 0u;
    return 2;
}
/* ---- passes: a take opens when a part's REC bit goes up, closes when it goes down ---- */
/* an ARP SEQ part that holds a take plays it as single notes: it leaves ARP SEQ and sequences on the arp's rate (its steps are offsets from 60 + the stand-in's lowest note: they become the notes they sounded) */
static void tk_plain(uint32_t p)
{
    track_t *t = &trk[p];
    uint32_t i, k, list[16], cnt = 0, root = 127;
    int32_t off;
    if (arp_mode(t) != AM_SEQ_ID)
        return;
    if (arp_list(t, list, &cnt) && cnt)
        for (i = 0; i < cnt && i < 16u; i++)
            root = list[i] < root ? list[i] : root;
    else
        root = 60;
    off = (int32_t)root - 60;
    fm1_irq_off();
    for (i = 0; i < NSTEP; i++)
        for (k = 0; k < t->step[i].n && k < 4u; k++)
            t->step[i].note[k] = (uint8_t)clamp((int32_t)t->step[i].note[k] + off, 0, 127);
    t->p[P_SDIV] = t->p[P_ARATE];
    t->p[P_AMODE] = 0;
    t->arp_skip = 0;                                     /* (the arp's hold and its skipped step go with ARP SEQ) */
    t->rh_n = 0;
    fm1_irq_on();
}
static __attribute__((noinline)) void tk_close(uint32_t mask)                       /* the parts of mask stopped recording */
{
    uint32_t p, changed = 0, nch = 0, who = 0, made = 0;
    for (p = 0; p < 2u; p++)
        if (((mask >> p) & 1u) && memcmp(trk[p].step, tk_pre.s[p], sizeof trk[p].step)) {
            changed |= 1u << p;
            nch++;
            who = p;
        }
    if (!changed)
        return;
    tk_total++;
    tk_who = nch == 1u ? (uint8_t)(who + 1u) : 0u;
    tk_m.take = tk_total;
    tk_sw_k = 0;
    if (nch == 1u) {                                     /* (the other part as it was before this take: ANSWER OFF puts it back) */
        tk_orig_no = tk_total;
        memcpy(tk_orig.s, tk_pre.s[1u - who], sizeof tk_orig.s);
        tk_orig.len = tk_pre.len[1u - who];
        memcpy(tk_orig.gr, tk_pre.gr[1u - who], sizeof tk_orig.gr);
    }
    {
        uint32_t sd = tk_live ? tk_m.seed : 0u;          /* (the answer made while the pass was open keeps its seed: it changes only where the take did) */
        tk_live = 0;
        tk_m.ans = 0;
        if (nch == 1u && tide_answer && !tide_answer_on && !((song.rec >> (1u - who)) & 1u) && tk_gen(who, 1u - who, sd, tide_answer)) {
            tk_m.roll = 0;
            made = 1;
        }
    }
    for (p = 0; p < 2u; p++)
        if ((changed >> p) & 1u)
            tk_plain(p);
    tk_push(tk_total);
    if (made)
        tk_say(0, 0);
}
static __attribute__((noinline)) void take_poll(void)                                /* main loop, every frame */
{
    uint32_t now = song.rec & 3u, was = tk_open;
    if (tk_roll_mask) {                                  /* a ROLL (morph_do) since the last frame */
        uint32_t m = tk_roll_mask;
        tk_roll_mask = 0;
        take_roll(m, tk_roll_seed);
    }
    if (tk_reset_v) {                                    /* a load: no history, no live answer, no pass that was open */
        tk_reset_v = 0;
        tk_cnt = tk_pos = 0, tk_total = 0;
        tk_m.ans = 0;
        tk_pend = 0;
        tk_prest = 0;
        tk_who = 0;
        tk_force = 0;
        tk_sw_k = 0;
        tk_mu = 0;
        tk_open = (uint8_t)now;
        return;
    }
    if (tk_sw_k && tk_sw_due())
        tk_sw_run();
    if (tk_pend && tk_due())
        tk_land();
    if (tk_mu) {                                         /* a take / answer message is held: its frames are kept up while it is still the one shown */
        if ((int32_t)(tk_mu - fm1_ms) > 0 && ui.msg_t && str_hash(7u, ui.msg) == tk_mh)
            ui.msg_t = 40;
        else
            tk_mu = 0;
    }
    if (was && tide_answer && !tide_answer_on && song.playing && !cin_left && (was == 1u || was == 2u)) {   /* (owner: "the response should be generated instantly"): while REC stays armed on one part, the answer is made again in the last step of each loop pass, from the part's steps as they are then, so it lands on the next bar line */
        uint32_t q = was & 1u ? 0u : 1u, len = TIDE_PLAY_LEN(&trk[q]), idx = TIDE_PLAY_IDX(&trk[q]);
        if (len < 2u)
            tk_wd = 1;
        else if (idx + 1u < len)
            tk_wd = 0;
        else if (!tk_wd) {
            uint32_t sg = steps_sig(&trk[q]), ok = 0, rp = tk_force == 2u && tk_m.ans == 2u - q;   /* (rp: a RIPPLE pressed while armed, the answer stands in the other part) */
            tk_wd = 1;
            if (rp)
                ok = tk_ripple(q, 1u - q, tk_m.seed);
            else if (sg != tk_lsig || tk_force)
                ok = tk_gen(q, 1u - q, tk_live ? tk_m.seed : 0u, tide_answer);
            if (ok) {
                if (rp && tk_due())
                    tk_land();
                tk_force = 0;
                tk_live = 1;
                tk_lsig = sg;
                tk_say(rp ? "RIPPLE" : 0, 0);
            }
        }
    }
    if (now == was)
        return;
    if (now & ~was) {                                      /* a part armed */
        tk_live = tk_wd = 0;
        tk_lsig = steps_sig(&trk[now & 1u ? 0u : 1u]);   /* (the armed part's steps now: an answer is made when a pass has changed them) */
        if (!was)
            tk_save(&tk_pre);                              /* (the state of both parts when the pass opens) */
    }
    tk_open = (uint8_t)now;
    if (was & ~now) {
        tk_force = 0;
        tk_close(was & ~now);
    }
}
