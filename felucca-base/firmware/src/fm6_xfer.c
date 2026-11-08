/* SPDX-License-Identifier: GPL-3.0-only */
/* The TIDE bridge of the FM6 engine (DUELL_TIDE only): a crossing between two unlike FM6 sounds (morph_xfer.c) retunes operators, switches the
 * algorithm and stands in free operators for the carriers that change frequency. Included once by eng_fm6.c (one translation unit) at the place
 * these functions had in it, so the generated code is the same; eng_fm6.c keeps the types it needs earlier (fm6_xt_t, fm6_xsave_t) and the
 * few hooks (fm6_sync, fm6_note_on, fm6_render, fm6_block). fm6_macros is the macro body of fm6_sync, which the OFF build keeps inline. */
#define FM6_XTOL (1 << 24) / 48                         /* a quarter of a semitone (Q24 log2) */
#if DUELL_BENCH
/* Bench builds only (bench.c `tide` words, tools/tide_measure.py; a normal build has none of this and its functions keep their names and code): the time of TIDE's audio
 * stages as the part's own core runs them, in TC_TICKS (24 a us, the free-running clock either core reads). A stage is timed by a wrapper of the same name around the body
 * (MXT_BODY renames the body); `act` counts the calls that really did the stage's work, `sum` and `max` are of those calls. One writer a part (the core that renders it). */
typedef struct { uint32_t n, act, sum, max; } mxt_stage_t;
static struct { mxt_stage_t valley, wash, fm6; } mxt[NLIVE];
static struct { uint8_t on; uint16_t d, motion; } mxf[NLIVE];   /* `tide force`: the valley's depth and the movement held at these values (Q15), whatever the plan says */
#ifdef TC_TICKS
#define MXT_TICKS() TC_TICKS()
#else
static uint32_t fm1_ticks(void);                         /* (the host rigs: tests/ui_test.c's counter, defined after this file) */
#define MXT_TICKS() fm1_ticks()
#endif
static inline void mxt_add(mxt_stage_t *s, uint32_t dt, uint32_t act)
{
    s->n++;
    if (act) {
        s->act++;
        s->sum += dt;
        if (dt > s->max)
            s->max = dt;
    }
}
#define MXT_BODY(f) f##_body
#else
#define MXT_BODY(f) f
#endif
static void fm6_xphase(fm6_note_t *n, uint32_t k, uint32_t skip, uint32_t mute)
{
    uint32_t j, m, pass, best = 6u, bm = 0;
    for (pass = 0; pass < 2u && best >= 6u; pass++)
        for (j = 0; j < 6u; j++) {
            int32_t d = n->base[k] - n->base[j];
            if (j == k || ((skip >> j) & 1u) || (!pass && ((mute >> j) & 1u)) || d < -FM6_XTOL ||
                ((n->fixed >> j) & 1u) != ((n->fixed >> k) & 1u) || (best < 6u && n->base[j] >= n->base[best]))
                continue;
            for (m = 1; m < 32u && (d < FM6_COARSE[m] - FM6_XTOL || d > FM6_COARSE[m] + FM6_XTOL); m++)
                ;
            if (m < 32u && (m == 1u || !((n->fixed >> k) & 1u)))
                best = j, bm = m;
        }
    if (best < 6u)
        n->op[k].phase = (int32_t)((uint32_t)n->op[best].phase * bm);
}

/* Main-loop snapshot, published under MORPH_LOCK by morph_xplan. */
static void fm6_xpitch_pack(fm6_xpitch_t *d, const uint8_t *p)
{
    uint32_t k;
    for (k = 0; k < 6u; k++)
        memcpy(d->op[k], p + k * FP_OP + FP_MODE, 4u);
    d->transpose = p[FP_TRNSP];
}

static __attribute__((noinline)) void fm6_xtune(const track_t *t, uint32_t tr, fm6_note_t *n, const voice_t *v, uint32_t mask)
{
    const fm6_xt_t *x = &fm6_xt[tr];
    uint32_t k;
    (void)t;
    for (k = 0; k < 6u; k++) {
        uint32_t b = (x->tune >> k) & 1u, g = (x->glide >> k) & 1u;
        const fm6_xpitch_t *p = b ? &x->pb : &x->pa;
        const uint8_t *op = p->op[k];
        int32_t note = clamp((int32_t)v->note + p->transpose - 24, 0, 127);
        if (!((mask >> k) & 1u) || ((n->xs | n->xr) >> k) & 1u || (!g && ((n->xb >> k) & 1u) == b))
            continue;
        if (g) {                                       /* (same mode, coarse and fine: only DET differs, a few cents) */
            int32_t a0 = fm6_osc_base4(x->pa.op[k], note, 0), b0 = fm6_osc_base4(x->pb.op[k], note, 0);
            n->base[k] = a0 + (int32_t)(((int64_t)(b0 - a0) * x->gv) >> 15);
            continue;
        }
        n->base[k] = fm6_osc_base4(op, note, 0);
        n->fixed = (uint8_t)((n->fixed & ~(1u << k)) | ((op[0] & 1u) << k));
        n->xb = (uint8_t)((n->xb & ~(1u << k)) | (b << k));
        fm6_xphase(n, k, 0u, x->mute);
    }
}

static void fm6_xcopy(fm6_note_t *n, uint32_t d, uint32_t s)
{
    n->env[d] = n->env[s];
    n->ol0[d] = n->ol0[s];
    n->base[d] = n->base[s];
    n->fixed = (uint8_t)((n->fixed & ~(1u << d)) | (((n->fixed >> s) & 1u) << d));
    n->op[d].phase = n->op[s].phase;
}

static __attribute__((noinline)) void MXT_BODY(fm6_xblock)(track_t *t, uint32_t tr)
{
    fm6_xt_t *x = &fm6_xt[tr];
    fm6_xsave_t *sv0 = (fm6_xsave_t *)((fm6_note_t *)ENG_MEM(tr) + FM6_POLY);
    /* K3 ease level 1: the retune every other block; between, the notes keep the last one. ZERO MARGIN: an operator is retuned click-free only on a block it
     * was silent in, and the block before too (fm6_xtune's mask: mute = now & before); morph_xfer.c XTP_MAX = XZ / 3 makes a silent operator's plateau three
     * blocks long at least, so it has two such blocks in a row, and one of any two blocks in a row is a retune block here. A plateau shorter than three blocks
     * (a larger XTP_MAX, a coarser fader step) could miss its retune at level 1: change both together */
    uint32_t i, s, dirty = 0, tune = x->on && !(EASE >= EASE_RATE && (x->ez ^= 1u));
    if (tune)
        EASE_PROBE(1);
    for (i = 0; i < FM6_POLY; i++) {
        fm6_note_t *n = (fm6_note_t *)ENG_MEM(tr) + i;
        uint32_t want = x->on ? x->xs : 0u;
        if (!n->live) {
            n->xs = n->xr = 0;
            continue;
        }
        for (s = 0; s < 6u; s++) {
            fm6_xsave_t *sv = sv0 + i * 6u + s;
            uint32_t r = x->sfor[s] - 1u, bit = 1u << s;
            if (n->xr & bit) {                          /* the stand-in's own state back (silent since the last block) */
                n->env[s] = sv->env;
                n->ol0[s] = sv->ol0;
                n->base[s] = sv->base;
                n->op[s].phase = sv->phase;
                n->fixed = (uint8_t)((n->fixed & ~bit) | ((uint32_t)sv->fixed << s));
                n->xr = (uint8_t)(n->xr & ~bit);
                fm6_xphase(n, s, n->xs, x->mute);
            }
            if (r >= 6u)
                continue;
            if ((want & bit) && !(n->xs & bit)) {       /* take: the copy of the carrier, at its B frequency */
                const uint8_t *op = x->pb.op[r];
                sv->env = n->env[s];
                sv->ol0 = n->ol0[s];
                sv->base = n->base[s];
                sv->phase = n->op[s].phase;
                sv->fixed = (uint8_t)((n->fixed >> s) & 1u);
                fm6_xcopy(n, s, r);
                n->base[s] = fm6_osc_base4(op, clamp((int32_t)t->v[i].note + x->pb.transpose - 24, 0, 127), 0);
                n->fixed = (uint8_t)((n->fixed & ~bit) | ((op[0] & 1u) << s));
                if (!((n->xb >> r) & 1u))               /* (going back the carrier plays this frequency: the copy keeps its phase) */
                    fm6_xphase(n, s, 1u << r, x->mute);
                n->xs = (uint8_t)(n->xs | bit);
            } else if (!(want & bit) && (n->xs & bit)) {   /* give: on the bridge's B side the carrier takes the copy */
                if (((x->tune >> r) & 1u) && !((n->xb >> r) & 1u)) {
                    fm6_xcopy(n, r, s);
                    n->xb = (uint8_t)(n->xb | (1u << r));
                }
                n->xs = (uint8_t)(n->xs & ~bit);
                n->xr = (uint8_t)(n->xr | bit);
            }
        }
        dirty |= n->xs | n->xr;
        if (tune)
            fm6_xtune(t, tr, n, &t->v[i], x->mute | x->glide);
    }
    x->dirty = (uint8_t)(dirty != 0u);
}
#if DUELL_BENCH
static __attribute__((noinline)) void fm6_xblock(track_t *t, uint32_t tr)
{
    uint32_t t0 = MXT_TICKS(), act = tr < NLIVE && (fm6_xt[tr].on || fm6_xt[tr].dirty);
    fm6_xblock_body(t, tr);
    if (tr < NLIVE)
        mxt_add(&mxt[tr].fm6, MXT_TICKS() - t0, act);
}
#endif

static __attribute__((noinline)) void fm6_xapply(uint32_t tr)
{
    const fm6_xt_t *x = &fm6_xt[tr];
    uint8_t *p = fm6_eff[tr].p;
    uint32_t k;
    fm6_eff[tr].alg = x->alg;
    fm6_eff[tr].fb = x->fb;
    for (k = 0; k < 6u; k++)                            /* (a stand-in plays with its carrier's levels and EG) */
        if ((x->xs >> k) & 1u && x->sfor[k])
            memcpy(p + k * FP_OP, p + (x->sfor[k] - 1u) * FP_OP, FP_OP);
}

static __attribute__((noinline)) void fm6_xlv(const int32_t *dt, int32_t *ul)
{
    uint32_t k, tr = (uint32_t)(((const uint8_t *)dt - (const uint8_t *)fm6_eff[0].dt) / sizeof fm6_eff[0]);
    if (tr < NLIVE && fm6_xt[tr].on)
        for (k = 0; k < 6u; k++)
            ul[k] += fm6_xt[tr].lv[k];
}

static __attribute__((noinline)) void fm6_macros(uint8_t *p, const int16_t *e, uint8_t *alg_out, uint8_t *fb_out, int32_t *dt, const int32_t *eoff)
{
    uint32_t alg = e[0] >= 1 && e[0] <= 32 ? (uint32_t)e[0] - 1u : p[FP_ALG] & 31u, car = fm6_carriers(alg), k, j, n = 0;
    int32_t meg = clamp(e[4], -64, 63) * 40 / 64;              /* MEG: rate steps (+ slower) */
    int32_t step = clamp(e[6], 0, 127) * (12 * 13981) / 127;   /* DTUN: up to 12 cents a step (1 cent = 13981) */
    *alg_out = (uint8_t)alg;
    *fb_out = (uint8_t)clamp(p[FP_FB] + e[1], 0, 7);
    for (k = 0; k < 6u; k++) {
        uint8_t *op = p + (5u - k) * FP_OP;                     /* OP1 first: carriers in their order */
        uint32_t ki = 5u - k;
        dt[ki] = 0;
        if ((car >> ki) & 1u) {                                 /* the carriers a step apart, centred on the note */
            dt[ki] = (2 * (int32_t)n++ - (int32_t)__builtin_popcount(car) + 1) * step / 2;
            if (eoff) fm6_env_apply(op, eoff);
            continue;
        }
        if (!op[FP_MODE])
            op[FP_FC] = (uint8_t)clamp(op[FP_FC] + e[3], 0, 31);
        for (j = 0; j < 4u; j++)
            op[FP_R1 + j] = (uint8_t)clamp(op[FP_R1 + j] - meg, 0, 99);
        op[FP_KVS] = (uint8_t)clamp(op[FP_KVS] + e[5], 0, 7);
    }
}

static void fm6_cross_macros(uint32_t tr, uint8_t *p, const int16_t *a, uint8_t *alg, uint8_t *fb, int32_t *dt)
{
    int16_t env[4]; int32_t eoff[4];
    uint32_t on = fm6_env_offsets(env, fm6_patch[tr], a[P_E0] >= 1 && a[P_E0] <= 32 ? (uint32_t)a[P_E0] - 1u : fm6_patch[tr][FP_ALG] & 31u, &a[P_ATK], eoff);
    fm6_macros(p, &a[P_E0], alg, fb, dt, on ? eoff : 0);
}
