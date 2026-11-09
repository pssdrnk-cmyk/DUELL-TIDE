/* SPDX-License-Identifier: GPL-3.0-only */
/* THE THIN MASK (CALL AND ANSWER, ARC's breakdown; b7 replaces TURNS / LEAD). Included by tide_seq.c after seq_poly.c.
 * A thinned part (seq_morph.c sqm_thinned: a listening bar, sqm_listens, or ARC's tarc_thin) plays End A without the steps of its mask
 * sqm_thin (main loop, a bit per stored step), a playback mask only (KEEP stores sqm_step, never the mask): every optional hit goes, its
 * anchors (sqm_anchor) and beats stay (MR4), and a held hit, a hit before a tie are never dropped; a pattern of 2 hits or fewer is left whole.
 * A bass or a pad (the Original's category) never listens and keeps its own rule in the breakdown: half of its optional hits go. */
static uint32_t sl_cat(uint32_t k);
static int sqm_thin_build(void)                          /* 1: a mask changed */
{
    uint32_t pi, i, k, ch = 0, lead = 0;
    for (pi = 0; pi < NSHOWN; pi++) {
        uint32_t m[2] = {0, 0};
        const track_t *t = &trk[pi];
        uint32_t len = sqm_len(t);
        uint32_t c = UP_REC_USER(t) ? sl_cat((uint32_t)t->user - 1u) : ds_cat_of(t->eng_req, t->preset);   /* (the Original's category) */
        uint32_t keep = c == 1u || c == 3u;              /* (a BASS or a PAD sound, category.c's numbers: included later; stored, append-only) */
        if (!keep)
            lead |= 1u << pi;                            /* (it listens, and its chords thin to two voices) */
        if (((sqm_auto == 3u && !keep) || ((tarc_thin >> pi) & 1u)) && sqm[pi].ok && len > 4u) {
            const step_t *p = sqm_pat[pi][0], *pb = sqm_pat[pi][1];   /* (End A as played: it carries the phrase overlay; End B: VARY ALL plays it too) */
            uint32_t score[NSTEP], hits = 0, cand = 0, grid = tide_metric(t, 0) != SM_NONE;
            for (i = 0; i < len; i++)
                hits += (uint32_t)sqm_hit(&p[i]);
            for (i = 0; i < len; i++) {
                uint32_t mt = grid ? tide_metric(t, i) : SM_ODD, pref = mt == SM_AND ? 1u : mt == SM_A ? 2u : 3u;
                score[i] = 0;
                if (!sqm_hit(&p[i]) || mt == SM_BEAT || sqm_held(p, i, len) || sqm_anchor(t->step, len, i) ||
                    (sqm_vary_all && (!sqm_hit(&pb[i]) || sqm_held(pb, i, len))))   /* (audit C8: VARY ALL: a hit both ends may drop) */
                    continue;
                score[i] = sqm_score(sqm[pi].tseed ^ 0x71A5u, i, 0x73u, pref);
                cand++;
            }
            for (k = 0; hits > 2u && k < (keep ? (cand + 1u) / 2u : cand); k++) {   /* (MR4: anchors and beats only, but a bass's or a pad's half) */
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
 * the ends played (sqm sig / tseed / ok), the lengths, the grids and swings tide_metric reads, the parts gliding, the ARC's thinning and VARY BARS */
static uint32_t sqm_thin_k __attribute__((section(".pool"))), sqm_thin_kok __attribute__((section(".pool")));   /* (main loop only: the pool, zeroed at boot) */
static void sqm_thin_poll(void)
{
    uint32_t pi, h = 2166136261u;
    for (pi = 0; pi < NSHOWN; pi++) {
        const track_t *t = &trk[pi];
        h = (h ^ sqm[pi].sig) * 16777619u;
        h = (h ^ sqm[pi].tseed) * 16777619u;
        h = (h ^ (sqm[pi].ok | sqm_len(t) << 1 | (uint32_t)arp_mode(t) << 8 | (uint32_t)(uint8_t)t->p[P_SDIV] << 12 | (uint32_t)(uint8_t)t->p[P_ARATE] << 20)) * 16777619u;
        h = (h ^ (uint32_t)(uint16_t)t->p[P_SSWING]) * 16777619u;
        h = (h ^ (t->preset | (uint32_t)t->user << 8 | (uint32_t)t->eng_req << 16)) * 16777619u;   /* (the sound: its category, D2) */
    }
    h = (h ^ ((uint32_t)(uint16_t)song.g[G_SWING] | (uint32_t)sqm_auto << 16 | (uint32_t)tarc_thin << 20 | (uint32_t)sqm_vary_all << 24)) * 16777619u;
    sqm_dyn = (int8_t)(tarc_bright() < 0 ? -2 : tarc_bright());   /* (the dynamics' ARC side for the audio interrupt, seq_morph.c seq_play_step: MR4, the breakdown two steps softer) */
    if (sqm_thin_kok && h == sqm_thin_k)
        return;
    sqm_thin_k = h, sqm_thin_kok = 1;
    sqm_thin_build();
}
