/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 (ROLL CHAINED storage, PT_VER 4): a patch record carries End B as its values (morph.c MBV, flag PTF_MB) with the marks of the
 * values set by hand; the 1.0 extension block (PTF_XB, up_xb_in, its own edit list) is gone. For the tests that store patches with End B:
 * include after upreset.c and morph.c. */
#ifndef ENDB_STORE_H
#define ENDB_STORE_H

static uint8_t eb_blk[MBV_MAX];                 /* the End B block store_with_b put in the record */
static uint32_t eb_n;                           /* .. its length (0: none) */

/* part t (trk[0] or trk[1]) -> slot k, category cat (0: the sound's), with End B as the part's morph has it now, the block built as
 * up_patch_save_part builds it (none when the morph has none). up_store_trk's result */
static int store_endb_cat(track_t *t, uint32_t k, const char *nm, uint32_t cat)
{
    uint32_t pi = (uint32_t)(t - trk), i;
    int16_t ref[EXT_P_LEN];
    int rc;
    memset(ref, 0, sizeof ref);
    for (i = 0; i < P_COUNT; i++)
        ref[i] = motion_base_value(t, i);
    for (i = 0; i < NX; i++)
        ref[XW_BASE + i] = (int16_t)clamp(XV(t, i), -64, 127);
    eb_n = morph_bv_make(pi, ref, eb_blk);
    up_bv_n = eb_n;
    up_bv_in = eb_n ? eb_blk : 0;
    rc = up_store_trk(t, k, nm, cat);
    up_bv_in = 0;
    up_bv_n = 0;
    return rc;
}
/* .. with the values ed[0..ne) set by hand on its End B first (ids that part cannot move are left out); ne 0: no End B block */
static int store_with_b_cat(track_t *t, uint32_t k, const char *nm, uint32_t cat, const xb_edit_t *ed, uint32_t ne)
{
    uint32_t pi = (uint32_t)(t - trk);
    eb_n = 0;
    if (!ne)
        return up_store_trk(t, k, nm, cat);
    (void)morph_ready(pi);
    morph_pin_load(&mph[pi], ed, ne);
    return store_endb_cat(t, k, nm, cat);
}
static int store_with_b(track_t *t, uint32_t k, const char *nm, const xb_edit_t *ed, uint32_t ne) { return store_with_b_cat(t, k, nm, 0, ed, ne); }

/* part t (its worst-case values set before) into slot k with the biggest End B: every value its morph can move off the Original, the values
 * ed[0..ne) set by hand, FM6's bytes too: the record PT_WORST bounds */
static int store_worst_cat(track_t *t, uint32_t k, const char *nm, uint32_t cat, const xb_edit_t *ed, uint32_t ne)
{
    uint32_t pi = (uint32_t)(t - trk), w, i;
    morph_part_t *mp = morph_ready(pi);
    morph_pin_load(mp, ed, ne);
    for (w = 0; w < EXT_P_LEN; w++)
        if (mbv_wid(w) && !mbv_never(w) && morph_editable(mp, w, 0)) {
            const param_desc_t *d = morph_desc(ENGINES[eng_idx(mp->eng)], w);
            int32_t o = mp->base[w];
            *morph_bval(mp, w) = (int8_t)(o + 1 <= d->max ? o + 1 : o - 1);
        }
    if (morph_isfm(mp))
        for (i = 0; i < NFM; i++)
            mp->fh[1][mp->top[1]][i] = mp->fb[i] < 99u ? mp->fb[i] + 1u : mp->fb[i] - 1u;
    morph_build(mp);
    return store_endb_cat(t, k, nm, cat);
}

/* slot k's stored End B block (0: none) and its length */
static const uint8_t *rec_endb(uint32_t k, uint32_t *n)
{
    const uint8_t *p = pt_raw(k);
    uint32_t xbo, mb, pr, pat, fm;
    *n = 0;
    if (!p || !(p[4] & PTF_MB))
        return 0;
    pt_layout(p, &xbo, &mb, &pr, &pat, &fm);
    *n = pat - mb;
    return p + mb;
}

/* the extension slots that record p stores (a bit per slot: their wide ids' bits in the values' bitmap) */
static uint32_t rec_xbits(const uint8_t *p)
{
    uint32_t i, m = 0;
    for (i = 0; i < NX; i++)
        if (p[PT_FIX + (XW_BASE + i) / 8u] >> ((XW_BASE + i) % 8u) & 1u)
            m |= 1u << i;
    return m;
}
/* slot i's values a and b alike as a record keeps them (PT_VER 4: a value in its range's bits; a load fits both to the same value) */
static int xfit_eq(uint32_t eng, uint32_t i, int32_t a, int32_t b) { return wv_code(eng, XWID(i), a) == wv_code(eng, XWID(i), b); }
static int vfit_eq(uint32_t eng, uint32_t id, int32_t a, int32_t b) { return wv_code(eng, id, a) == wv_code(eng, id, b); }   /* (the same for value id) */
#endif
