/* SPDX-License-Identifier: GPL-3.0-only */
/* ANSWER (TURNS = LEAD, ECHO; M8b/c, docs/design/TIDE_NEXT_PROPOSALS.md step 2). Included by tide_seq.c after seq_poly.c.
 * Per 4 shared bars, part 2 leads bars 1-2 and part 1 bars 3-4 (seq_play_step); the other part plays a thinned End A: the mask
 * sqm_thin (main loop, a bit per stored step) drops half of its optional hits, a playback mask only (KEEP stores sqm_step, never
 * the mask). A beat, an anchor, a held hit, a hit before a tie are never dropped, and a pattern of 2 hits or fewer is left whole.
 * ECHO ranks the hits on the peer's thinned-away places first in the mask, and the phrase edits lean a hit into them. */
static int tide_echo(uint32_t pi, uint32_t idx)
{
    const track_t *a = &trk[pi], *b = &trk[pi ^ 1u];
    return sqm_turns == 3u && idx < NSTEP && a->p[P_SDIV] == b->p[P_SDIV] && sqm_len(a) == sqm_len(b) && ((sqm_thin[pi ^ 1u][idx >> 5] >> (idx & 31u)) & 1u);
}
static void sqm_thin_build(void)
{
    uint32_t pi, i, k;
    for (pi = 0; pi < NSHOWN; pi++) {
        uint32_t m[2] = {0, 0};
        const track_t *t = &trk[pi];
        uint32_t len = sqm_len(t);
        if ((sqm_turns >= 2u || ((tarc_thin >> pi) & 1u)) && sqm[pi].ok && len > 4u) {
            const step_t *p = sqm_pat[pi][0], *pb = sqm_pat[pi][1];   /* (End A as played: it carries the phrase overlay; End B: VARY ALL plays it too) */
            uint32_t score[NSTEP], hits = 0, cand = 0, grid = tide_metric(t, 0) != SM_NONE;
            for (i = 0; i < len; i++)
                hits += (uint32_t)sqm_hit(&p[i]);
            for (i = 0; i < len; i++) {
                uint32_t mt = grid ? tide_metric(t, i) : SM_ODD, pref = mt == SM_AND ? 1u : mt == SM_A ? 2u : 3u;
                score[i] = 0;
                if (!sqm_hit(&p[i]) || !(i % 4u) || mt == SM_BEAT || sqm_held(p, i, len) ||
                    (sqm_vary_all && (!sqm_hit(&pb[i]) || sqm_held(pb, i, len))))   /* (audit C8: VARY ALL: a hit both ends may drop) */
                    continue;
                if (tide_echo(pi, i))
                    pref = 4u;                           /* (the answer sounds where the call left room) */
                score[i] = sqm_score(sqm[pi].tseed ^ 0x71A5u, i, 0x73u, pref);
                cand++;
            }
            for (k = 0; hits > 2u && k < (cand + 1u) / 2u; k++) {
                i = sqm_best(score, len);
                if (i >= len)
                    break;
                m[i >> 5] |= 1u << (i & 31u);
            }
        }
        sqm_thin[pi][0] = m[0];
        sqm_thin[pi][1] = m[1];
    }
}
