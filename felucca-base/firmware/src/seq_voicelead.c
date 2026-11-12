/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop MUSIC voicing: keep the bass, chord tones and note count, and choose
 * nearby upper-note octaves against the preceding chord. No chord memory or new
 * endpoint data. Held/slide chains keep their existing note identities.
 * The loop join counts too: a second walk starts from the last chord, and is kept
 * only when the whole cycle (last back to first included) moves less. */
static int32_t voicelead_abs(int32_t n) { return n < 0 ? -n : n; }
/* one forward walk from previous[] of pn_io notes (0 = no chord before); leaves the last chord there.
 * Returns the upper voices' movement chord to chord within the walk and back to its first;
 * write 0 only measures */
static int32_t voicelead_walk(const step_t *source, uint32_t len, step_t *out, int32_t *previous, uint32_t *pn_io, int write)
{
    int32_t first[4] = {0}, cost = 0, lane_lo, lane_hi;uint32_t pn = *pn_io, fn = 0;
    sqm_lane(source, len, &lane_lo, &lane_hi);   /* (C1: a voice stays in the Original's lane) */
    for (uint32_t i = 0; i < len; i++) {
        step_t *s = &out[i];
        if (!sqm_hit(s) || s->n > 4u) continue;
        uint32_t n = s->n, order[4] = {0,1,2,3};
        for (uint32_t a = 1; a < n; a++) for (uint32_t b = a; b && s->note[order[b]] < s->note[order[b-1]]; b--) {
            uint32_t t = order[b];order[b] = order[b-1];order[b-1] = t;
        }
        uint32_t root = order[0];
        if (sqm_hit(&source[i]) && source[i].n == n) {
            for (uint32_t k = 0; k < n; k++) if (source[i].note[k] < source[i].note[root]) root = k;
            for (uint32_t k = 0; k < n; k++) if (order[k] == root) {
                for (uint32_t q = k; q; q--) order[q] = order[q-1];
                order[0] = root;break;
            }
        }
        int32_t best[4], reference[4];
        for (uint32_t k = 0; k < n; k++) {
            best[k] = s->note[order[k]];
            reference[k] = pn == n ? previous[k] : best[k];
        }
        if (n >= 3u && !sqm_held(source, i, len) && !(s->flags & SF_SLIDE)) {
            int32_t best_score = 0x7fffffff;
            for (uint32_t code = 0; code < (n == 4u ? 27u : 9u); code++) {
                uint32_t bits = code;int32_t candidate[4], score = 0;int ok = 1;
                candidate[0] = s->note[order[0]];
                for (uint32_t k = 1; k < n; k++) {
                    uint32_t id = order[k];
                    int32_t base = note_snap(trk, s->note[id]);   /* (B3: OFF-SCALE PLAY keeps an out-of-scale chord tone; SNAP snaps it, as the keys) */
                    int32_t note = base + ((int32_t)(bits % 3u) - 1) * 12;bits /= 3u;
                    candidate[k] = note;
                    if (!sqm_in_lane(note, lane_lo, lane_hi) || voicelead_abs(note - (source[i].n == n ? source[i].note[id] : s->note[id])) > 12 ||
                        note <= candidate[k-1] || ((note - (int32_t)s->note[id]) % 12 && !((scale_mask(trk) >> (uint32_t)((note-root_g+240)%12)) & 1u))) {ok = 0;break;}
                    score += 4 * voicelead_abs(note - reference[k]);
                    int32_t gap = note - candidate[k-1];
                    if (candidate[k-1] < 48 && gap < 5) score += 24 * (5-gap);
                    for (uint32_t q = 1; q < pn; q++) if (note == previous[q]) {score -= 4;break;}
                }
                if (ok && score < best_score) {
                    best_score = score;
                    memcpy(best,candidate,n*sizeof best[0]);
                }
            }
            if (write) for (uint32_t k = 1; k < n; k++) s->note[order[k]] = (uint8_t)best[k];
        }
        if (fn) {if (pn == n) for (uint32_t k = 1; k < n; k++) cost += voicelead_abs(best[k] - previous[k]);}
        else {memcpy(first,best,n*sizeof first[0]);fn = n;}
        memcpy(previous,best,n*sizeof best[0]);
        pn = n;
    }
    if (fn && fn == pn) for (uint32_t k = 1; k < fn; k++) cost += voicelead_abs(first[k] - previous[k]);
    *pn_io = pn;
    return cost;
}
static void sqm_voicelead(const step_t *source, uint32_t len, step_t *out)
{
    int32_t chord[4] = {0}, last[4];uint32_t n = 0;
    int32_t plain = voicelead_walk(source, len, out, chord, &n, 0);   /* from nothing, as before */
    uint32_t ln = n;memcpy(last,chord,sizeof last);
    int32_t joined = voicelead_walk(source, len, out, chord, &n, 0);  /* from the last chord */
    if (joined < plain) memcpy(chord,last,sizeof chord), n = ln;
    else memset(chord,0,sizeof chord), n = 0;
    voicelead_walk(source, len, out, chord, &n, 1);
}

/* The chord bed breathes with ARC (b7, idea 2): a chord part's voicing follows the arc's phase, on the 8-bar grid. RISE and RECOVER as
 * written; PEAK open (the second-lowest voice an octave up: a drop-2 spread; then the top voice an octave up while it stays at G5 or
 * under); BREAK closed (a voice more than an octave over the lowest comes down inside it) and one inversion lower (the top voice under
 * the others) where that stays at or over the part's lowest written note. Octaves only: never a new pitch class; never over the
 * Original's highest + 12 (its lane), never under its lowest; never a bass or pad part, a single-note line, a held or sliding chord; only
 * for a part whose hits are mostly chords of 3 or 4 notes. Any SCALE (CHROMATIC too). Applied to the ends as they are built (sqm_refresh),
 * so the stored pattern and KEEP never see it; ARC OFF: nothing. Main loop only */
static uint32_t sqm_varc_now(void)                       /* the phase's voicing, from ARC's own levers: the breakdown's slow edits, the peak's brightness */
{
    uint32_t need = tarc_need();
    return need == 4u ? 2u : need == 2u && tarc_bright() > 0 ? 1u : 0u;
}
/* a chord part in automatic WNDR / JRNY: not a bass or pad sound; its hits mostly chords of 3 or 4 notes (ARP SEQ: the arp's chord, sqm_varp) */
static int sqm_chordp(uint32_t pi, const step_t *p, uint32_t len)
{
    uint32_t i, h = 0, c = 0;
    if (!((sqm_auto >> pi) & 1u) || ((sqm_bassm >> pi) & 0x11u))
        return 0;
    if (arp_mode(&trk[pi]) == AM_SEQ_ID)
        return (sqm_v.cp >> pi) & 1u;
    for (i = 0; i < len; i++)
        if (sqm_hit(&p[i]))
            h++, c += p[i].n >= 3u;
    return c && c * 2u >= h;
}
static uint32_t sqm_vmode(uint32_t pi, const step_t *p, uint32_t len)   /* 0 none, 1 open, 2 closed, 3 an ARP SEQ chord part's breakdown (its steps' octaves as written) */
{
    if (!sqm_varc || sqm_vhold || !sqm_chordp(pi, p, len))
        return 0;
    return arp_mode(&trk[pi]) != AM_SEQ_ID ? sqm_varc : sqm_varc == 2u ? 3u : 0u;
}
static uint32_t sqm_vtop(const uint8_t *nt, uint32_t n) { uint32_t t = 0; for (uint32_t k = 1; k < n; k++) t = nt[k] > nt[t] ? k : t; return t; }
static void sqm_vmove(uint8_t *nt, uint32_t n, uint32_t k, int32_t d, int32_t lo, int32_t hi)   /* note k by d if it stays in lo..hi on no other note */
{
    int32_t v = (int32_t)nt[k] + d;
    for (uint32_t j = 0; j < n; j++)
        if (nt[j] == v)
            return;
    if (v >= lo && v <= hi)
        nt[k] = (uint8_t)v;
}
static void sqm_vchord(uint8_t *nt, uint32_t n, uint32_t mode, int32_t lo, int32_t hi, int32_t top)
{
    uint32_t k, m = 0, s;
    if (!n)
        return;
    for (k = 1; k < n; k++)
        m = nt[k] < nt[m] ? k : m;
    lo = lo < 0 ? nt[m] : lo;
    if (mode == 1u) {
        for (s = !m, k = 0; k < n; k++)
            s = k != m && nt[k] < nt[s] ? k : s;     /* (the second-lowest voice) */
        sqm_vmove(nt, n, s, 12, lo, hi);
        sqm_vmove(nt, n, sqm_vtop(nt, n), 12, lo, hi < top ? hi : top);
    } else {
        for (k = 0; k < 3u && nt[s = sqm_vtop(nt, n)] > nt[m] + 12u; k++)
            sqm_vmove(nt, n, s, -12, nt[m] + 1, hi);
        sqm_vmove(nt, n, sqm_vtop(nt, n), -12, lo, hi);
    }
}
static void sqm_breathe(const step_t *p, uint32_t len, uint32_t mode, step_t *out)
{
    int32_t lo = 127, hi = 0;
    uint32_t i, k;
    if (!mode)
        return;
    if (mode == 3u) {                                    /* (the arp's chord stays in the register it was written in: a VOIC octave on the step is taken back) */
        for (i = 0; i < len; i++)
            if (sqm_hit(&out[i]) && sqm_hit(&p[i]) && !(((int32_t)out[i].note[0] - p[i].note[0]) % 12))
                out[i].note[0] = p[i].note[0];
        return;
    }
    for (i = 0; i < len; i++)
        for (k = 0; sqm_hit(&p[i]) && k < p[i].n && k < 4u; k++)
            lo = p[i].note[k] < lo ? p[i].note[k] : lo, hi = p[i].note[k] > hi ? p[i].note[k] : hi;
    for (i = 0; i < len; i++)
        if (sqm_hit(&out[i]) && out[i].n >= 3u && out[i].n <= 4u && !sqm_held(out, i, len))
            sqm_vchord(out[i].note, out[i].n, mode, lo, hi + 12 > 108 ? 108 : hi + 12, 79);
}
/* .. on an ARP SEQ part the chord is the one the arp fires: the keys held or the stand-in's (tide_seq.c, its CHRD on C4 when none was held).
 * The main loop voices that chord (sqm_varp) and publishes it beside its source; the arp's note list (seq.c arp_list, TIDE hook H2b) only
 * swaps a note of the source for its voiced octave when the list is that chord (a new chord held: as played, up to 20 ms) */
static uint8_t tide_chord[NSHOWN][4], tide_chord_n[NSHOWN];   /* (tide_seq.c: the chord the stand-in remembers) */
static void sqm_varp(uint32_t pi)
{
    const track_t *t = &trk[pi];
    uint8_t c[CHORD_MAX];
    uint32_t n = 0, i;
    int32_t r;
    uint16_t mk;
    if (((sqm_auto >> pi) & 1u) && !((sqm_bassm >> pi) & 0x11u) && arp_mode(t) == AM_SEQ_ID)
        n = tide_chord_n[pi] ? (memcpy(c, tide_chord[pi], 4), tide_chord_n[pi]) : chord_make(t, 60, c, &r, &mk);
    n = n - 3u < 2u ? n : 0u;                            /* (a chord of 3 or 4 notes: a chord part) */
    sqm_v.cp = (uint8_t)((sqm_v.cp & ~(1u << pi)) | (n ? 1u << pi : 0u));
    n = sqm_varc ? n : 0u;
    if (n) {                                             /* b10 Later 6: the steps play the chord moved by their offset from C4, so the chord may rise only by what the steps
                                                          * leave of the Original's lane (its highest step + 12, sqm_lane): a VOIC octave on a step and PEAK's open voicing
                                                          * added up past it (M01's stab 70 + 12 + 4 = 86, M07 +5, M08 +3, M04 +2) */
        uint32_t len = sqm_len(t), e, k;
        int32_t lo, hi, mx = 0;
        sqm_lane(t->step, len, &lo, &hi);
        for (e = 0; e < 2u; e++)
            for (i = 0; i < len; i++)
                for (k = 0; sqm_hit(&sqm_pat[pi][e][i]) && k < sqm_pat[pi][e][i].n && k < 4u; k++)
                    mx = sqm_pat[pi][e][i].note[k] > mx ? sqm_pat[pi][e][i].note[k] : mx;
        r = c[sqm_vtop(c, n)] + hi - mx;
    } else
        r = 108;
    MORPH_LOCK();
    for (i = 0; i < n; i++)
        sqm_v.src[pi][i] = c[i];
    sqm_vchord(c, n, sqm_varc, -1, r, 0);                /* (the chord's own lowest note the floor; its top no higher than the steps leave room for) */
    for (i = 0; i < n; i++)
        sqm_v.out[pi][i] = c[i];
    sqm_v.n[pi] = (uint8_t)n;
    MORPH_UNLOCK();
}
/* the audio path (arp_list): the list voiced when it is the published chord; it reads the tables only */
static uint32_t tide_seq_voice(const track_t *t, uint32_t *list, uint32_t n)
{
    uint32_t pi = trk_index(t), i, j, v[4];
    if (pi >= NSHOWN || !n || n != sqm_v.n[pi])
        return n;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n && list[i] != sqm_v.src[pi][j]; j++)
            ;
        if (j == n)
            return n;
        v[i] = sqm_v.out[pi][j];
    }
    for (i = 0; i < n; i++)
        list[i] = v[i];
    return n;
}
