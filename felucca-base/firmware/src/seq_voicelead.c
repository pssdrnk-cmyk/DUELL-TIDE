/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop MUSIC voicing: keep the bass, chord tones and note count, and choose
 * nearby upper-note octaves against the preceding chord. No chord memory or new
 * endpoint data. Held/slide chains keep their existing note identities. */
static int32_t voicelead_abs(int32_t n) { return n < 0 ? -n : n; }
static void sqm_voicelead(const step_t *source, uint32_t len, step_t *out)
{
    int32_t previous[4] = {0};uint32_t pn = 0;
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
                    int32_t base = note_snap_to(trk, s->note[id], 1u);
                    int32_t note = base + ((int32_t)(bits % 3u) - 1) * 12;bits /= 3u;
                    candidate[k] = note;
                    if (note < 24 || note > 108 || voicelead_abs(note - (source[i].n == n ? source[i].note[id] : s->note[id])) > 12 ||
                        note <= candidate[k-1] || !((scale_mask(trk) >> (uint32_t)((note-root_g+240)%12)) & 1u)) {ok = 0;break;}
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
            for (uint32_t k = 1; k < n; k++) s->note[order[k]] = (uint8_t)best[k];
        }
        for (uint32_t k = 0; k < n; k++) previous[k] = s->note[order[k]];
        pn = n;
    }
}
