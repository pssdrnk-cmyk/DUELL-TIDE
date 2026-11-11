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
/* main-loop scratch shared by morph_to_sound, tide_mbv_load and tide_ui.c tide_keep_sound (RAM: .data + .bss is
 * tight). Lifetimes checked: none of them calls another while it holds these (tide_keep_sound is done with both before its
 * morph_rebase_part, which may reach tide_mbv_load); never from the audio ISR */
static int16_t tide_v[EXT_P_LEN];
static uint8_t tide_fv[FP_SIZE + 1u];

/* the FM6 patch bytes the morph blends (fm_byte): 1 there. The others are a MORPH TO target's structure (algorithm, ratios,
 * detune, ...). Made on first use, from either context (the same bytes each time) */
static uint8_t tide_fm_bl[FP_SIZE], tide_fm_bl_ok;
static const uint8_t *tide_fm_blended(void)
{
    uint32_t i;
    if (!tide_fm_bl_ok) {
        for (i = 0; i < NFM; i++)
            tide_fm_bl[fm_byte(i)] = 1;
        tide_fm_bl_ok = 1;
    }
    return tide_fm_bl;
}
/* the target patch fv's structure as a 16-bit check (Fletcher-16 over the bytes the morph does not blend, but TRANSPOSE: PITCH OFF
 * replaces it with the part's own): what MBV_FMT stores, so a load knows the target is still the one saved */
static uint32_t tide_fmto_sum(const uint8_t *fv)
{
    const uint8_t *bl = tide_fm_blended();
    uint32_t i, a = 1, b = 0;
    for (i = 0; i < FP_NAME; i++)
        if (!bl[i] && i != FP_TRNSP) {
            a = (a + fv[i]) % 255u;
            b = (b + a) % 255u;
        }
    return b << 8 | a;
}
/* end x of part pi's target cache := patch fv (FP_SIZE) of MORPH TO sound tk (index + 1). The caller holds the lock */
static void tide_fmto_put(uint32_t pi, uint32_t x, uint32_t tk, const uint8_t *fv)
{
    memcpy(morph_fmto[pi][x], fv, FP_SIZE);
    if (!(morph.cfg & MC_PIT))
        morph_fmto[pi][x][FP_TRNSP] = fm6_patch[pi % NTRK][FP_TRNSP];
    morph_fmto_k[pi][x] = (uint8_t)tk;
}

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
    static int8_t end[EXT_P_LEN], v8[EXT_P_LEN];
    static uint8_t fend[NFM];
    int16_t *v = tide_v;
    uint8_t *fv = tide_fv;
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
    {                                                    /* MR5 (b7): a part the multi turned down stays down: the destination's factory TRIM less what the part's TRIM
                                                          * takes off the Original's factory TRIM (M16's pads: JOURNEY played 9-12 dB over the multi). A TRIM raised to lift a
                                                          * quiet sound (M02's BIG STAB) is not carried: the destination has its own level */
        const track_t *t = &trk[pi];
        int32_t o;
        if (!UP_REC_USER(t) && t->preset < ds_count(en) && (o = mp->base[P_TRIM] - ds_value(ds_get(en, t->preset), P_TRIM, TP[P_TRIM].def)) < 0)
            end[P_TRIM] = (int8_t)clamp((int32_t)end[P_TRIM] + o, 0, 127);
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
    memcpy(mp->fh[x][j], fm ? fend : mp->fb, NFM);
    if (fm)
        tide_fmto_put(pi, x, k + 1u, fv);
    mp->tok[x][j] = (uint8_t)(k + 1u);
    mp->seed[x][j] = 0;
    mp->rcfg[x][j] = mp->ramt[x][j] = 0;
    mp->rdep[x][j] = 2u;                                 /* (values: no seed makes it again) */
    mp->top[x] = (uint8_t)j;
    if (mp->n[x] < MORPH_HIST)
        mp->n[x]++;
    MORPH_TEST_NEW(pi, x, 0);
    if (x)
        memset(mp->pin[j % EDITB_PINH], 0, PIN_BYTES);
    morph_build(mp);
    MORPH_UNLOCK();
    return 1;
}

/* the factory sound end x of part pi is (MORPH TO), -1 none */
static int32_t morph_to_of(uint32_t pi, uint32_t x)
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    return pi < NSHOWN && mp->n[0] ? (int32_t)mp->tok[x & 1u][mp->top[x & 1u]] - 1 : -1;
}

/* H8 (audio ISR, fm6_sync through morph_fm6): after the blend of part tr's patch copy p at B weight w, the bytes a MORPH TO
 * target does not blend, from the side the fader is on (S1: its middle; S2: the transition's side) */
static void tide_fm6_side(const morph_part_t *mp, uint32_t tr, uint8_t *p, uint32_t sd)
{
    const uint8_t *fade;                                 /* (the bytes the blend owns: they keep the blend) */
    uint32_t i, tk = mp->tok[sd][mp->top[sd]];
    if (!tk || tk != morph_fmto_k[tr][sd])
        return;
    fade = tide_fm_blended();
    for (i = 0; i < FP_NAME; i++)
        if (!fade[i])
            p[i] = morph_fmto[tr][sd][i];
}

/* H9 (morph.c's MBV block, plan §4.10): End B's MORPH TO target on FM6 saved with it. MBV_FMT: 3 bytes at the block's end, the
 * target's factory index + 1 and tide_fmto_sum of its patch (LSB first). Written only when End B is a MORPH TO sound whose
 * patch is cached; every other block stays 1.1's (a plain 1.1 build refuses a block with the flag: integration note) */
#define MBV_FMT 4u
static uint32_t tide_mbv_make(uint32_t pi, uint8_t *out, uint32_t n)
{
    const morph_part_t *mp = &mph[pi];
    uint32_t tk = mp->tok[1][mp->top[1]], c;
    if (!morph_isfm(mp) || !tk || tk != morph_fmto_k[pi][1])
        return n;
    c = tide_fmto_sum(morph_fmto[pi][1]);
    out[0] |= MBV_FMT;
    out[n] = (uint8_t)tk;
    out[n + 1u] = (uint8_t)c;
    out[n + 2u] = (uint8_t)(c >> 8);
    return n + 3u;
}
/* the length of a block with MBV_FMT (len: without it; avail bytes there), 0 = no block */
static uint32_t tide_mbv_size(const uint8_t *b, uint32_t avail, uint32_t len)
{
    if (!(b[0] & MBV_FMT))
        return len;
    return len && len + 3u <= avail && b[len] ? len + 3u : 0u;
}
/* after morph_bv_load's End B (block b, len bytes): the target back when its sound's patch is still the one saved (its SLOT may be
 * a user voice bank slot changed since); else End B keeps only the blended bytes, as 1.1 */
static void tide_mbv_load(uint32_t pi, const uint8_t *b, uint32_t len)
{
    uint8_t *fv = tide_fv;
    morph_part_t *mp = &mph[pi];
    const ds_sound_t *s;
    uint32_t tk;
    if (!len || !(b[0] & MBV_FMT) || !morph_isfm(mp))
        return;
    tk = b[len - 3u];
    if (!(s = ds_get(eng_idx(mp->eng), tk - 1u)) || fm6_slot_patch((uint32_t)clamp(ds_value(s, P_E7, 0), 0, 255), fv) ||
        tide_fmto_sum(fv) != ((uint32_t)b[len - 2u] | (uint32_t)b[len - 1u] << 8))
        return;
    MORPH_LOCK();
    mp->tok[1][mp->top[1]] = (uint8_t)tk;
    tide_fmto_put(pi, 1u, tk, fv);
    morph_build(mp);
    MORPH_UNLOCK();
}

/* KEEP (tide_ui.c tide_keep_sound): part pi's FM6 patch as it sounds at the fader into p (FP_SIZE + 1): the stored patch, the
 * blend at the fader and a MORPH TO target's structure from the side that sounds (morph_fm6 with tide_fm6_target, as the audio
 * path makes it; S2's crossing decides the side there). The AMP ENV / E-knob offsets stay out (they are the Original's knobs) */
static void tide_fm6_effective(uint32_t pi, uint8_t *p)
{
    memcpy(p, fm6_patch[pi % NTRK], FP_SIZE + 1u);
    MORPH_LOCK();
    (void)morph_fm6(pi, p);
    MORPH_UNLOCK();
}


static void tide_fm6_target(const morph_part_t *mp, uint32_t tr, uint8_t *p, int32_t w)
{ tide_fm6_side(mp,tr,p,morph_xside(mp,w,1)); }
