/* SPDX-License-Identifier: GPL-3.0-only */
/* ANSWER (TURNS = LEAD, ECHO; M8b/c, docs/design/TIDE_NEXT_PROPOSALS.md step 2). Included by tide_seq.c after seq_poly.c.
 * Per 4 shared bars, part 2 leads bars 1-2 and part 1 bars 3-4 (seq_play_step); the other part plays a thinned End A: the mask
 * sqm_thin (main loop, a bit per stored step) drops half of its optional hits, a playback mask only (KEEP stores sqm_step, never
 * the mask). A beat, an anchor (sqm_anchor), a held hit, a hit before a tie are never dropped, and a pattern of 2 hits or fewer is left whole.
 * Only a lead or pluck sound's single-note line answers (D2, sqm_lead): a pad's, a bass's or any chord part's pattern plays whole.
 * ECHO ranks the hits on the peer's thinned-away places first in the mask, and the phrase edits lean a hit into them. */
static int tide_echo(uint32_t pi, uint32_t idx)
{
    const track_t *a = &trk[pi], *b = &trk[pi ^ 1u];
    return sqm_turns == 3u && idx < NSTEP && a->p[P_SDIV] == b->p[P_SDIV] && sqm_len(a) == sqm_len(b) && ((sqm_thin[pi ^ 1u][idx >> 5] >> (idx & 31u)) & 1u);
}
static uint32_t sl_cat(uint32_t k);
static int sqm_thin_build(void)                          /* 1: a mask changed */
{
    uint32_t pi, i, k, ch = 0, lead = 0;
    for (pi = 0; pi < NSHOWN; pi++) {
        uint32_t m[2] = {0, 0};
        const track_t *t = &trk[pi];
        uint32_t len = sqm_len(t);
        if ((sqm_turns >= 2u || ((tarc_thin >> pi) & 1u)) && sqm[pi].ok && len > 4u) {
            const step_t *p = sqm_pat[pi][0], *pb = sqm_pat[pi][1];   /* (End A as played: it carries the phrase overlay; End B: VARY ALL plays it too) */
            uint32_t score[NSTEP], hits = 0, cand = 0, grid = tide_metric(t, 0) != SM_NONE;
            uint32_t c = UP_REC_USER(t) ? sl_cat((uint32_t)t->user - 1u) : ds_cat_of(t->eng_req, t->preset);   /* (the Original's category) */
            int32_t line = 0;                            /* (a single note +1, a chord -1) */
            for (i = 0; i < len; i++)
                hits += (uint32_t)sqm_hit(&p[i]), line += sqm_hit(&p[i]) ? p[i].n > 1u ? -1 : 1 : 0;
            if (line > 0 && (c == 2u || c == 4u))        /* D2: a LEAD or PLUCK sound (category.c's numbers: included later; stored, append-only) playing a line */
                lead |= 1u << pi;
            else if (!((tarc_thin >> pi) & 1u))
                hits = 0;                                /* (LEAD leaves a pad's, a bass's or a chord part's pattern whole) */
            for (i = 0; i < len; i++) {
                uint32_t mt = grid ? tide_metric(t, i) : SM_ODD, pref = mt == SM_AND ? 1u : mt == SM_A ? 2u : 3u;
                score[i] = 0;
                if (!sqm_hit(&p[i]) || mt == SM_BEAT || sqm_held(p, i, len) || sqm_anchor(t->step, len, i) ||
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
        ch |= (m[0] ^ sqm_thin[pi][0]) | (m[1] ^ sqm_thin[pi][1]);
        MORPH_LOCK();                                    /* (audit C12: the audio interrupt sees both words of one mask) */
        sqm_thin[pi][0] = m[0];
        sqm_thin[pi][1] = m[1];
        MORPH_UNLOCK();
    }
    sqm_lead = (uint8_t)lead;
    return ch != 0u;
}
/* main loop, every pass (tide_ui.c tide_poll): the masks again only when what they are built from changed (audit C9: not every frame):
 * the ends played (sqm sig / tseed / ok), the lengths, the grids and swings tide_metric reads, TURNS, the ARC's thinning and VARY BARS.
 * ECHO reads the peer's mask: built again until it settles (at most 3 passes) */
static uint32_t sqm_thin_k, sqm_thin_kok;
static void sqm_thin_poll(void)
{
    uint32_t pi, n, h = 2166136261u;
    for (pi = 0; pi < NSHOWN; pi++) {
        const track_t *t = &trk[pi];
        h = (h ^ sqm[pi].sig) * 16777619u;
        h = (h ^ sqm[pi].tseed) * 16777619u;
        h = (h ^ (sqm[pi].ok | sqm_len(t) << 1 | (uint32_t)arp_mode(t) << 8 | (uint32_t)(uint8_t)t->p[P_SDIV] << 12 | (uint32_t)(uint8_t)t->p[P_ARATE] << 20)) * 16777619u;
        h = (h ^ (uint32_t)(uint16_t)t->p[P_SSWING]) * 16777619u;
        h = (h ^ (t->preset | (uint32_t)t->user << 8 | (uint32_t)t->eng_req << 16)) * 16777619u;   /* (the sound: its category, D2) */
    }
    h = (h ^ ((uint32_t)(uint16_t)song.g[G_SWING] | (uint32_t)sqm_turns << 16 | (uint32_t)tarc_thin << 20 | (uint32_t)sqm_vary_all << 24)) * 16777619u;
    sqm_dyn = (int8_t)tarc_bright();                     /* (the dynamics' ARC side for the audio interrupt: seq_morph.c seq_play_step) */
    if (sqm_thin_kok && h == sqm_thin_k)
        return;
    sqm_thin_k = h, sqm_thin_kok = 1;
    for (n = 0; n < 3u && sqm_thin_build(); n++)
        ;
}
