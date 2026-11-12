/* SPDX-License-Identifier: GPL-3.0-only */
/* TAKE + ANSWER (TIDE, b10 round 8: live performance assist, phase 1; work/b10/specs/PERF_PHASE1.md, work/b10/reports/PERF_ASSIST.md). Main loop only: nothing here runs in the audio interrupt
 * but tide_take_step (a counter), and the notes of the answer are written with the interrupt off, a whole part's steps at a time.
 *   A TAKE is one pass of the live recorder (seq.c rec_put): the part's REC bit goes up (the pass opens: both parts' steps are kept) and down (it closes). A pass that wrote something is a take:
 *   it goes on a history of the last 2 steps (takes and rolls), the first take on an empty part sets the part's loop (1, 2 or 4 bars: the part's P_SLEN runs at 64 steps while it records, and the
 *   steps entered are counted), and ANSWER (TIDE SETTINGS) writes a BASS LINE or a DOUBLE of the take into the other part's pattern (ANSWER ON). OCT- held + a PRESETS turn goes back
 *   (left) and forward (right) along the history while the newest edit is a take or a roll (tk_fresh); a ROLL (morph_do MQ_ROLL, the gesture and word TIDE has) on the answering part writes
 *   the answer again with the roll's seed, one history step. The generators are the host prototype's (tools/perf/perf_probe.c): integer only, deterministic, the same seed the same answer.
 * Pool: 4 snapshots of both parts' steps (4.6 KB), the take's events and the answer's steps (1.1 KB). */
enum { TA_OFF, TA_BASS, TA_DOUBLE, TA_REPLY, TA_ECHO, TA_N };
static volatile uint8_t tide_answer __attribute__((section(".pool")));           /* TIDE SETTINGS ANSWER: what the other part is given when a take closes (a fresh unit: BASS LINE) */
static volatile uint8_t tide_answer_on __attribute__((section(".pool")));                  /* TIDE SETTINGS ANSWER ON: 0 the other part, 1 OFF */

typedef struct {                                         /* the answer that is live now (it travels with a history state) */
    uint8_t ans;                                         /* the part that holds it + 1, 0 none */
    uint8_t src, mode, key, flags, clen;                       /* the take's part; TA_*; the key's root (0..11); bit 0 minor, bit 1 guessed, bit 2 scale named, bits 3.. the SCL index */
    uint16_t seed, roll;                                 /* the seed it was written with; the rolls since the take (the header says ROLL n) */
    uint32_t sig;                                        /* steps_sig of the answer's part right after it was written: an edit since ends the roll */
} tk_meta_t;
typedef struct {
    step_t s[2][NSTEP];
    uint8_t len[2];
    uint8_t gr[2][2];                                    /* each part's SDIV and ARATE (an answer may set the grid of the part that holds it) */
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
static uint16_t tk_hn[2] __attribute__((section(".pool")));                                /* history step j: its number, bit 15 a roll */
static uint8_t tk_cnt __attribute__((section(".pool"))), tk_pos __attribute__((section(".pool"))), tk_open __attribute__((section(".pool"))), tk_oldlen[2] __attribute__((section(".pool")));   /* (tk_fresh_v, tk_first, tk_adv ..: tide.h) */
static uint16_t tk_total __attribute__((section(".pool")));
static uint32_t tk_mh __attribute__((section(".pool"))), tk_mu __attribute__((section(".pool"))), tk_lsig __attribute__((section(".pool")));   /* the header message held (its hash, until when: fm1_ms); the part's steps at the last live answer */
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
static __attribute__((noinline)) void tk_events(const track_t *t, uint32_t len)
{
    uint32_t i, k, l, list[16], cnt = 0, root = 127;
    int32_t off = t->p[P_TRANS];
    tk_nev = 0;
    if (arp_mode(t) == AM_SEQ_ID) {                      /* the steps are offsets from the stand-in's lowest note (seq.c rec_arp_note) */
        if (arp_list(t, list, &cnt) && cnt)
            for (i = 0; i < cnt && i < 16u; i++)
                root = list[i] < root ? list[i] : root;
        else
            root = 60;
        off += (int32_t)root - 60;
    }
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
/* ---- REPLY: the call's rhythm in the bars after it, moved in scale steps so the last note is the tonic; ECHO: the call's last note thrown 4 times (100, 80, 60, 40) ---- */
static __attribute__((noinline)) void tk_reply(uint32_t len, uint32_t seed)
{
    uint32_t i, last = tk_nev - 1u;
    int32_t o, nd = (int32_t)tk_nd, dl = tk_deg_of(tk_ev[last].note, &o), shift = dl == 0 ? -2 : dl <= nd / 2 ? -dl : nd - dl, oc = seed % 3u == 1u ? -12 : seed % 3u == 2u ? 12 : 0;
    for (i = 0; i < tk_nev; i++) {
        int32_t d = tk_deg_of(tk_ev[i].note, &o), n = tk_note_of(d + shift, o);
        if (i == last) {                                 /* ends home: the tonic nearest the note */
            d = tk_deg_of(n, &o);
            n = tk_note_of(0, d >= nd / 2 ? o + 1 : o);
        }
        tk_put(tk_ev[i].t + (int32_t)len, tk_ev[i].len, n + oc, 96u, 0u);
    }
}
static __attribute__((noinline)) void tk_echo(uint32_t seed)
{
    const tk_ev_t *e = &tk_ev[tk_nev - 1u];
    uint32_t r, h = seed ? tk_hs(seed, 99u) % 3u : 0u, gap = h == 1u ? 4u : h == 2u ? 6u : 3u;   /* (a dotted eighth; a roll: a quarter or a dotted quarter) */
    for (r = 0; r < 4u; r++)
        tk_put((int32_t)(e->t + e->len + gap * r), 2, e->note, 100u - 20u * r, 1u);
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

/* make the answer to part src's pattern in part dst's, with the seed; 1 when something was written */
static uint32_t tk_gen(uint32_t src, uint32_t dst, uint32_t seed, uint32_t mode)
{
    track_t *ts = &trk[src], *td = &trk[dst];
    uint32_t len = seq_len(ts), spb = tk_spb(ts), bars, i, n = 0, call = mode >= TA_REPLY, dlen, gset = 0;
    int32_t shift = td->p[P_TRANS] - ts->p[P_TRANS];
    if (call && seed && tk_m.ans && tk_m.mode == mode && tk_m.clen)
        len = tk_m.clen;                                 /* (a roll: the part's loop is the doubled one: the call is the first half) */
    bars = (len + spb - 1u) / spb;
    if (bars > 4u)
        bars = 4u;
    if (call && (bars > 2u || 2u * len > NSTEP))
        return 0;                                        /* (a call is 2 bars at most: it and its answer share 64 steps) */
    tk_events(ts, len);
    if (!tk_nev)
        return 0;
    tk_key();
    memset(tk_out, 0, sizeof tk_out);
    tk_slen = call ? 2u * len : len;
    tk_ps = tk_gp(ts);
    tk_pd = tk_gp(td);
    dlen = tk_ps == tk_pd || !tk_pd ? tk_slen : (tk_slen * tk_ps + tk_pd / 2u) / tk_pd;
    if (tk_pd && tk_pd != tk_ps && (tk_pd > tk_ps || dlen > NSTEP)) {   /* b10 (M02's pad, an ARP SEQ part at a slow rate, gave NO ANSWER): the answer wins - when the part's own grid is too coarse to hold it (or too fine to fit 64 steps) it is written on the take's grid and the part's RATE / DIV is set to match while the answer stands (the history step holds the old one) */
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
        else
            tk_double(seed);
    }
    for (i = 0; i < tk_olen; i++)
        n += tk_out[i].n != 0;
    if (!n || (mode == TA_BASS && n < bars))
        return 0;                                        /* (nothing, or less than a note a bar: the other part keeps its pattern; a take down in the bass register leaves no room under it) */
    if (shift)                                           /* (the answer sounds as written: the other part's TRN is taken off) */
        for (i = 0; i < tk_olen; i++)
            for (n = 0; n < tk_out[i].n; n++) {
                int32_t v = (int32_t)tk_out[i].note[n] - shift;
                tk_out[i].note[n] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
            }
    fm1_irq_off();
    memcpy(td->step, tk_out, sizeof td->step);
    td->p[P_SLEN] = (int16_t)tk_olen;
    if (gset)
        td->p[tk_gi(td)] = ts->p[tk_gi(ts)];
    if (call)
        ts->p[P_SLEN] = (int16_t)tk_slen;               /* the call's loop doubles: the call, then the answer */
    fm1_irq_on();
    tk_m.clen = (uint8_t)len;
    tk_m.ans = (uint8_t)(dst + 1u);
    tk_m.src = (uint8_t)src;
    tk_m.mode = (uint8_t)mode;
    tk_m.key = tk_kr;
    tk_m.flags = (uint8_t)(tk_km | (tk_guess << 1) | (tk_nd != 7u ? 4u | (uint32_t)scale_g << 3 : 0u));
    tk_m.seed = (uint16_t)seed;
    tk_m.sig = steps_sig(td);
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
static void tk_say(const char *pre, uint32_t no)
{
    char b[48], n[8];
    b[0] = 0;
    if (pre) {
        tk_cat(b, pre);
        fmt_int(n, (int32_t)no);
        tk_cat(b, n);
        tk_cat(b, "  ");
    }
    tk_cat(b, tk_m.mode == TA_DOUBLE ? "DOUBLE  " : tk_m.mode == TA_REPLY ? "REPLY  " : tk_m.mode == TA_ECHO ? "ECHO  " : "BASS LINE  ");
    tk_cat(b, N_NOTE[tk_m.key % 12u]);
    tk_cat(b, (tk_m.flags & 4u) ? " " : tk_m.flags & 1u ? " MINOR" : " MAJOR");
    if (tk_m.flags & 4u)
        tk_cat(b, N_SCALE[(tk_m.flags >> 3) & 15u]);
    if (tk_m.flags & 2u)
        tk_cat(b, "?");
    ui_say(b, "");
    tk_hold();
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
    }
    fm1_irq_on();
    tk_m = d->m;
}
static void tk_push(uint32_t no, uint32_t roll)           /* tk_pre is the state before this step; the tracks hold the state after it */
{
    if (tk_pos < tk_cnt)
        tk_cnt = tk_pos;                                 /* (a new step drops what could be redone) */
    if (tk_cnt == 2u) {
        memcpy(&tk_h[0], &tk_h[1], sizeof tk_h[0]);
        tk_hn[0] = tk_hn[1];
        tk_cnt = 1;
    }
    memcpy(&tk_h[tk_cnt], &tk_pre, sizeof tk_h[0]);
    tk_hn[tk_cnt++] = (uint16_t)(no | (roll ? 0x8000u : 0u));
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
    j = dir < 0 ? tk_pos - 1u : tk_pos;
    hn = tk_hn[j];
    tk_save(&tk_h[tk_pos]);
    tk_load(&tk_h[dir < 0 ? tk_pos - 1u : tk_pos + 1u]);
    tk_pos = (uint8_t)(dir < 0 ? tk_pos - 1u : tk_pos + 1u);
    b[0] = 0;
    tk_cat(b, dir < 0 ? "UNDO " : "REDO ");
    tk_cat(b, hn & 0x8000u ? "ROLL " : "TAKE ");
    fmt_int(b + str_len(b), (int32_t)(hn & 0x7FFFu));
    ui_say(b, "");
    tk_hold();
    return 1;
}
#define TAKE_STALE() (tk_fresh = 0)                       /* a load or a lock edit took the undo copy: the newest edit is not a take any more (ui.c) */

/* ---- a ROLL of the part that holds the answer (morph.c morph_do MQ_ROLL: the knob, MIDI, the same word) ---- */
static __attribute__((noinline)) void take_roll(uint32_t mask, uint32_t seed)
{
    uint32_t ap;
    if (tk_open || !tk_m.ans || !((mask >> (tk_m.ans - 1u)) & 1u))
        return;                                          /* (not while a pass is open: its history step is the pass's) */
    ap = (uint32_t)tk_m.ans - 1u;
    if (steps_sig(&trk[ap]) != tk_m.sig)
        return;                                          /* the answer was edited since: it is the player's now */
    tk_save(&tk_pre);
    tk_m.roll++;
    if (!tk_gen(tk_m.src, ap, (seed & 0x7FFFu) | 1u, tk_m.mode)) {   /* (a seed is never 0: 0 is the take's first answer) */
        tk_m = tk_pre.m;
        return;
    }
    tk_push(tk_m.roll, 1u);
    tk_say("ROLL ", tk_m.roll);
}

/* ---- the tide line and knob 1's line while a take records (b10 round 9, on the identity's tide line) ---- */
static uint32_t take_line(uint32_t *prog)                  /* 0 no take, 1 the count-in, 2 recording (prog: how far through the loop, Q16) */
{
    uint32_t p, len, idx;
    if (!(song.rec & 3u))
        return 0;
    if (cin_left)
        return 1;
    if (!song.playing)
        return 0;
    p = song.rec & 1u ? 0u : 1u;
    len = TIDE_PLAY_LEN(&trk[p]);
    idx = TIDE_PLAY_IDX(&trk[p]);
    *prog = len ? (idx % len) * 65536u / len : 0u;
    return 2;
}
/* ---- passes: a take opens when a part's REC bit goes up, closes when it goes down ---- */
static __attribute__((noinline)) void tk_close(uint32_t mask)                       /* the parts of mask stopped recording */
{
    uint32_t p, changed = 0, nch = 0, who = 0;
    for (p = 0; p < 2u; p++) {
        track_t *t = &trk[p];
        uint32_t ch;
        if (!((mask >> p) & 1u))
            continue;
        ch = memcmp(t->step, tk_pre.s[p], sizeof t->step) != 0;
        if ((tk_first >> p) & 1u) {
            if (ch) {                                    /* the first take: its length is the loop, whole bars */
                uint32_t spb = tk_spb(t), lw = 0, i, n, bars;
                for (i = 0; i < NSTEP; i++)
                    if (t->step[i].n || t->step[i].time == ST_TIE)
                        lw = i + 1u;
                n = (uint32_t)tk_adv[p] + 1u > lw ? (uint32_t)tk_adv[p] + 1u : lw;
                bars = (n + spb - 1u) / spb;
                bars = bars <= 1u ? 1u : bars == 2u ? 2u : 4u;
                while (bars > 1u && bars * spb > NSTEP)
                    bars >>= 1;
                n = bars * spb > NSTEP ? NSTEP : bars * spb;
                fm1_irq_off();
                t->p[P_SLEN] = (int16_t)n;
                for (i = n; i < NSTEP; i++)
                    memset(&t->step[i], 0, sizeof t->step[i]);
                fm1_irq_on();
            } else
                t->p[P_SLEN] = tk_oldlen[p];
            tk_first &= (uint8_t)~(1u << p);
        }
        if (ch) {
            changed |= 1u << p;
            nch++;
            who = p;
        }
    }
    if (!changed)
        return;
    tk_total++;
    {
        uint32_t sd = tk_live ? tk_m.seed : 0u;          /* (b10: the answer made while the pass was open keeps its seed: it changes only where the take did) */
        tk_live = 0;
        tk_m.ans = 0;
    if (nch == 1u && tide_answer && !tide_answer_on) {
        uint32_t o = 1u - who;
        if (!((song.rec >> o) & 1u) && tk_gen(who, o, sd, tide_answer)) {
            tk_m.roll = 0;
            tk_push(tk_total, 0u);
            tk_say(0, 0);
            return;
        }
        if (!((song.rec >> o) & 1u))
        {
            ui_message(tide_answer >= TA_REPLY ? "CALL: 2 BARS AT MOST" : "NO ANSWER: NO ROOM UNDER IT");
            tk_hold();
        }   /* (REPLY and ECHO need room: the call and its answer share 64 steps; a take already in the bass register leaves none for a bass line) */
    }
    }
    tk_push(tk_total, 0u);
}
static __attribute__((noinline)) void take_poll(void)                                /* main loop, every frame */
{
    uint32_t now = song.rec & 3u, was = tk_open, p;
    if (tk_roll_mask) {                                  /* a ROLL (morph_do) since the last frame */
        uint32_t m = tk_roll_mask;
        tk_roll_mask = 0;
        take_roll(m, tk_roll_seed);
    }
    if (tk_reset_v) {                                    /* a load: no history, no live answer, no pass that was open */
        tk_reset_v = 0;
        tk_cnt = tk_pos = 0, tk_total = 0;
        tk_m.ans = 0;
        tk_open = (uint8_t)now;
        tk_first = 0;
        return;
    }
    if (tk_mu) {                                         /* a take / answer message is held: its frames are kept up while it is still the one shown */
        if ((int32_t)(tk_mu - fm1_ms) > 0 && ui.msg_t && str_hash(7u, ui.msg) == tk_mh)
            ui.msg_t = 40;
        else
            tk_mu = 0;
    }
    if (was && tide_answer && !tide_answer_on && tide_answer <= TA_DOUBLE && song.playing && !cin_left && (was == 1u || was == 2u)) {   /* b10 (owner: "the response should be generated instantly"): while REC stays armed on one part, the answer is made again in the last step of each loop pass, from the part's steps as they are then, so it lands on the next downbeat; BASS LINE and DOUBLE (REPLY and ECHO double the loop: at the close) */
        uint32_t q = was & 1u ? 0u : 1u, len = TIDE_PLAY_LEN(&trk[q]), idx = TIDE_PLAY_IDX(&trk[q]);
        if (((tk_first >> q) & 1u) || len < 2u)
            tk_wd = 1;
        else if (idx + 1u < len)
            tk_wd = 0;
        else if (!tk_wd) {
            uint32_t sg = steps_sig(&trk[q]);
            tk_wd = 1;
            if (sg != tk_lsig && tk_gen(q, 1u - q, tk_live ? tk_m.seed : 0u, tide_answer)) {
                tk_live = 1;
                tk_lsig = sg;
                tk_say(0, 0);
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
        for (p = 0; p < 2u; p++)
            if (((now & ~was) >> p) & 1u) {
                uint32_t i, empty = 1;
                for (i = 0; i < NSTEP; i++)
                    empty &= !trk[p].step[i].n;
                if (empty && !((tk_first >> p) & 1u)) {    /* the first take on an empty part: it runs at 64 steps until REC closes it */
                    tk_first |= (uint8_t)(1u << p);
                    tk_adv[p] = 0;
                    tk_oldlen[p] = (uint8_t)trk[p].p[P_SLEN];
                    fm1_irq_off();
                    trk[p].p[P_SLEN] = NSTEP;
                    fm1_irq_on();
                }
            }
    }
    tk_open = (uint8_t)now;
    if (was & ~now)
        tk_close(was & ~now);
}
