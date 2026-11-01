/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's morph side (tide.h H5-H8), included by morph.c before morph_end_pure (after morph_music.c, morph_fm_shape.c and
 * morph_complement.c). From the JAM fork (branch jam, 71654ba: morph.c's MORPH TO and MUSICAL glue), re-fitted to 1.1's
 * chained ROLL (docs/design/MIGRATION_1_1_PLAN.md §4.1, §4.10):
 *   tide_roll        a MUSICAL end is made from the Original and the dials, never from the end as it is, with the FM6 end
 *                    by the SHAPE coordinate; its depth is 2 (values: no seed makes it again); SMART / WILD stay 1.1's
 *   morph_to_sound   MORPH TO: end x becomes a factory sound of the part's engine, exactly (JOURNEY's moves)
 *   tide_fm6_target  the FM6 consumer: the bytes a MORPH TO target does not blend (algorithm, ratios, detune, ...) sound
 *                    from the side the fader is on (from S2: the transition's side)
 * Contexts: main loop, except tide_fm6_target (the audio ISR's fm6_sync, through morph_fm6). */

static uint8_t morph_fmto[NSHOWN][2][FP_SIZE];          /* MORPH TO on FM6: the target's whole patch per end */
static uint8_t morph_fmto_k[NSHOWN][2];                  /* .. the tok it is of (0: none) */

/* SLOT value s's patch into v (FP_SIZE + 1 bytes, sanitized), as fm6_load_slot reads it, without loading it. 1: none there */
static int fm6_slot_patch(uint32_t s, uint8_t *v)
{
    uint8_t pk[FM6_PACKED];
    int32_t k = fm6_bank_slot((int32_t)s), x = fm6_xslot((int32_t)s);
    if (x >= 0)
        memcpy(pk, FM6_XFACTORY[x], FM6_PACKED);
    else if (k < 0)
        memcpy(pk, FM6_FACTORY[s % FM6_NFACTORY], FM6_PACKED);
    else if (!fm6_slot_ok((int32_t)s) || !fm6_bank_read || fm6_bank_read((uint32_t)k, pk))
        return 1;
    fm6_unpack(pk, v);
    fm6_sanitize(v);
    return 0;
}

/* H5: a MUSICAL roll of end x (cfg has MC_MUSIC): the end from the Original into end / fend, its depth into *ndep. 0: not
 * MUSICAL, 1.1's roll goes on */
static int tide_roll(morph_part_t *mp, uint32_t pi, uint32_t x, int8_t *end, uint8_t *fend, uint32_t seed, uint32_t amount,
                     uint32_t cfg, uint32_t *ndep)
{
    uint8_t sv = mrc;
    if (!(cfg & MC_MUSIC))
        return 0;
    mrc = (uint8_t)(cfg & (MC_SET | MC_STEREO | MC_MUSIC));
    mm_x = (int8_t)x;                                    /* (MUSICAL keeps this end's dials) */
    morph_roll_musical(mp->base, end, ENGINES[eng_idx(mp->eng)], amount, seed, cfg & MC_PART ? 1u : 0u, (cfg & MC_STEREO) != 0u);
    mm_x = -1;
    memcpy(fend, mp->fb, NFM);
    if (morph_isfm(mp))
        morph_fm_shape(mp->fb, fend, morph_fm_car(pi, mp->base), fm_shape_value(mp->base, end[P_DEC]));
    mrc = sv;
    *ndep = 2u;                                          /* (dial and walk dependent: saved as values, never re-made from the seed) */
    return 1;
}

/* MORPH TO (TIDE LIST's TO, morph_glide.c JOURNEY): end x of part pi becomes factory sound k of the part's engine, exactly: every
 * value but the mix, the arp, the pattern (MF_NEVER, MF_ARP) and, with PITCH OFF, the pitch items; what does not blend (a switch,
 * a route, TRIM) flips at the middle (morph_build). No SMART rules: both ends are designed sounds. FM6: the 65 bytes the morph
 * blends go to the target's, the rest flips to it at the middle (tide_fm6_target); SLOT itself stays. 0: none */
static int morph_to_sound(uint32_t pi, uint32_t x, uint32_t k)
{
    static int16_t v[EXT_P_LEN];
    static int8_t end[EXT_P_LEN], v8[EXT_P_LEN];
    static uint8_t fv[FP_SIZE + 1u], fend[NFM];
    int fm;
    morph_part_t *mp;
    const engine_t *e;
    const ds_sound_t *s;
    uint32_t id, j, en;
    if (pi >= NSHOWN)
        return 0;
    mp = morph_ready(pi);
    en = eng_idx(mp->eng);
    e = ENGINES[en];
    if (!(s = ds_get(en, k)))
        return 0;
    x &= 1u;
    for (id = 0; id < P_E0; id++)
        v[id] = TP[id].def;
    for (id = 0; id < NX; id++)
        v[XW_BASE + id] = XTP[id].def;
    for (id = 0; id < 8u; id++)
        v[P_E0 + id] = e->edit[id].def;
    ds_put(s, v, 0);
    fm = morph_isfm(mp);
    if (fm && fm6_slot_patch((uint32_t)clamp(v[P_E7], 0, 255), fv))
        return 0;
    for (id = 0; id < EXT_P_LEN; id++)
        v8[id] = (int8_t)clamp(v[id], -128, 127);
    MORPH_LOCK();
    morph_fold(pi);
    MORPH_UNLOCK();
    for (id = 0; id < EXT_P_LEN; id++) {
        mtag_t g, h;
        int8_t tv = v8[id];
        end[id] = mp->base[id];
        if (id >= P_COUNT && id < XW_BASE)
            continue;
        g = morph_tag(e, mp->base, id, duell_stereo);
        h = morph_tag(e, v8, id, duell_stereo);
        if (!((g.flags | h.flags) & (MF_NEVER | MF_ARP | MF_GONE)) && (!((g.flags | h.flags) & MF_PIT) || (morph.cfg & MC_PIT)))
            end[id] = tv;                                /* (a switch, a route, TRIM too: morph_build flips what does not blend at the middle) */
    }
    if (fm) {                                            /* FM6: the patch's blended bytes; SLOT stays (turning it loads a patch) */
        end[P_E7] = mp->base[P_E7];
        for (id = 0; id < NFM; id++) {
            uint32_t pitem = (id >= 54u && id < 61u) || id == 63u;
            fend[id] = pitem && !(morph.cfg & MC_PIT) ? mp->fb[id] : (uint8_t)clamp(fv[fm_byte(id)], 0, 99);
        }
    }
    MORPH_LOCK();
    morph_ramp(mp, x);
    j = (mp->top[x] + 1u) % MORPH_HIST;
    memcpy(mp->h[x][j], end, P_COUNT);
    memcpy(mp->hx[x][j % XH], end + XW_BASE, NX);
    mp->xv[x] = (uint8_t)(mp->xv[x] < XH ? mp->xv[x] + 1u : XH);
    memcpy(mp->fh[x][j], fm ? fend : mp->fb, NFM);
    if (fm) {
        memcpy(morph_fmto[pi][x], fv, FP_SIZE);
        if (!(morph.cfg & MC_PIT))
            morph_fmto[pi][x][FP_TRNSP] = fm6_patch[pi % NTRK][FP_TRNSP];
        morph_fmto_k[pi][x] = (uint8_t)(k + 1u);
    }
    mp->tok[x][j] = (uint8_t)(k + 1u);
    mp->seed[x][j] = 0;
    mp->rcfg[x][j] = mp->ramt[x][j] = 0;
    mp->rdep[x][j] = 2u;                                 /* (values: no seed makes it again) */
    mp->top[x] = (uint8_t)j;
    if (mp->n[x] < MORPH_HIST)
        mp->n[x]++;
    if (x) {
        memset(mp->pin[j % EDITB_PINH], 0, PIN_BYTES);
        mp->bstay = 0;
    }
    morph_build(mp);
    MORPH_UNLOCK();
    return 1;
}

/* the cached target patch of end x again when an UNDO came back to another MORPH TO entry (FM6) */
static void morph_fmto_sync(uint32_t pi, uint32_t x)
{
    static uint8_t fv[FP_SIZE + 1u];
    morph_part_t *mp = &mph[pi % NSHOWN];
    uint32_t tk = mp->tok[x & 1u][mp->top[x & 1u]];
    const ds_sound_t *s;
    if (!morph_isfm(mp) || !tk || tk == morph_fmto_k[pi % NSHOWN][x & 1u] || !(s = ds_get(eng_idx(mp->eng), tk - 1u)) ||
        fm6_slot_patch((uint32_t)clamp(ds_value(s, P_E7, 0), 0, 255), fv))
        return;
    if (!(morph.cfg & MC_PIT))
        fv[FP_TRNSP] = fm6_patch[pi % NTRK][FP_TRNSP];
    MORPH_LOCK();
    memcpy(morph_fmto[pi % NSHOWN][x & 1u], fv, FP_SIZE);
    morph_fmto_k[pi % NSHOWN][x & 1u] = (uint8_t)tk;
    mp->fser++;
    MORPH_UNLOCK();
}

/* the factory sound end x of part pi is (MORPH TO), -1 none */
static int32_t morph_to_of(uint32_t pi, uint32_t x)
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    return pi < NSHOWN && mp->n[0] ? (int32_t)mp->tok[x & 1u][mp->top[x & 1u]] - 1 : -1;
}

/* H8 (audio ISR, fm6_sync through morph_fm6): after the blend of part tr's patch copy p at B weight w, the bytes a MORPH TO
 * target does not blend, from the side the fader is on (S1: its middle; S2: the transition's side) */
static void tide_fm6_target(const morph_part_t *mp, uint32_t tr, uint8_t *p, int32_t w)
{
    static uint8_t fade[FP_SIZE], made;
    uint32_t sd = w >= 16384, i, tk = mp->tok[sd][mp->top[sd]];
    if (!tk || tk != morph_fmto_k[tr][sd])
        return;
    if (!made) {                                         /* (the bytes the blend owns: they keep the blend) */
        for (i = 0; i < NFM; i++)
            fade[fm_byte(i)] = 1;
        made = 1;
    }
    for (i = 0; i < FP_NAME; i++)
        if (!fade[i])
            p[i] = morph_fmto[tr][sd][i];
}

/* S2 (the smooth crossings, morph_xfer.c) is not ported yet: what GLIDE writes for them (glide_motion) is kept here and read by
 * nothing, and WASH LEVEL is a setting with no sound yet (TIDE LIST shows and saves it) */
typedef struct { uint16_t motion; uint8_t follow, side; } mxs_t;
static mxs_t mxs[NSHOWN];
static uint8_t morph_wlvl = 2;
static const uint16_t XWL[4] = {0u, 16384u, 32768u, 49152u};
static int32_t xrise(int32_t x) { return x; }
