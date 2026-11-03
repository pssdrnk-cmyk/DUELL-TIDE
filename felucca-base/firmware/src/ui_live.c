/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE LIVE GRAPHS (design: docs/design/LIVE_GRAPHS.md): over the pictures of ui_graph.c, what the selected part is doing now: a playhead (a line and a dot) on the envelopes,
 * LFOs, CYCLE and the INSERT sweep, the live cutoff on MULTI FILTER and TWIN, a bar under each acting MATRIX row, a bar in each sounding CHORD key. An overlay: the pictures are
 * drawn as before. The taps below only READ the audio's state (a torn read draws one frame wrong); the one audio-side code is the pair of bytes track_t.lvc / lvc2 it writes.
 * Every kind ends in one marker (lv.x, lv.y: canvas px of the picture; lv.pv: it comes from a UI clock, not the audio) drawn by lv_mark; one UI clock (lv_uiph) serves every
 * PREVIEW; one sweep (lv_cyc) the LFO, INSERT and CYCLE cycles; one curve walk (lv_eg_lvl) the envelopes. MENU > ANIMATION OFF (PREF_ANIM_OFF): none of it.
 * Included by ui_graph.c after the pictures, whose geometry it repeats (graph_adsr, graph_fm6env, graph_lfo, graph_cycle, graph_filt, graph_insert, graph_mod_new). */

enum { LV_NONE, LV_ENV, LV_LFO, LV_FILT, LV_CHORD, LV_TWIN, LV_INS, LV_MOD, LV_CYCLE, LV_FMENV };
#define LV_LFO_STEP (0xFFFFFFFFu / (PANEL_W / 2u))   /* graph_lfo, graph_insert: two cycles over the panel */
#define LV_H0 2166136261u
/* one look every LV_EVERY frames (15 a second: a picture redraw is 87 .. 150 k basic blocks, the animation costs by the redraw rate); the marker's line: first canvas row, rows */
#define LV_EVERY 4u
#define LV_BUSY 205u                                 /* the audio's load (song.cpu_q8, the figure ABOUT shows and the overload guard reads; 256 = 100 %): above 80 % no look is taken, the marker stands where it was */
static const uint8_t LV_LINE[10][2] = {{0, 0}, {2, 66}, {6, 64}, {0, 0}, {0, 0}, {0, 0}, {6, 64}, {0, 0}, {4, 60}, {20, 46}};

static struct {
    uint32_t sig, kind, n, pv;                       /* the look's signature, the page's kind, a marker is there, it is a PREVIEW */
    int16_t x, y;                                    /* the marker */
    int16_t elvl, erel;                              /* ENV: the last level seen, the level the release began at */
    uint8_t evi, estg, epid;                         /* .. voice (+1), stage, which part / envelope */
    uint32_t t0;                                     /* fm1_ms when the marker settled on a held level (the creep), 0: not yet */
    uint32_t ph, odd, who;                           /* the sweep: the phase last seen, the cycle, whose */
    uint32_t uph, ums, uid;                          /* the UI clock: its phase, its last ms, whose */
    uint32_t fc, fc2, ch;                            /* FILTER, TWIN: the live cutoffs (Hz of the table), 0 none; CHORD: the voices' levels */
    int32_t mv[4], mq[4];                            /* MATRIX: the four sources (Q15, INT32_MIN: the row does nothing) and what the bars show */
} lv;

/* mod.c mod_tsrc and voice.c lfo_sync_inc run in RAM (range X / Z, tools/ram_fns_*.py): flash code enters range X only through a pointer */
static int32_t (*volatile const lv_tsrc)(const track_t *, uint32_t, int32_t) = mod_tsrc;
static uint32_t (*volatile const lv_sync_inc)(uint32_t) = lfo_sync_inc;

/* what the audio reads (voice.c track_render: l2on, e2on); mod.c mod_begin leaves its flags for the next look (its own on closes after the block) */
static int lv_l2_on(const track_t *t) { return t->mdw.l2 || (t->p[P_TWTYPE] && XV(t, X_LFO2)); }
static int lv_e2_on(const track_t *t) { return t->mdw.e2 || t->p[P_E2FLT] || (t->p[P_TWTYPE] && XV(t, X_ENV2)); }
static int lv_lfo2_page(const page_t *pg)            /* graph_lfo's test */
{
    return (pg->id[0] >= P_L2RATE && pg->id[0] <= P_L2FADE) || (pg->id[0] >= P_L2SYNC && pg->id[0] <= P_L2POL) || pg->id[0] == XW_BASE + X_LFO2;
}

static uint32_t live_kind(const page_t *pg, const track_t *t)
{
    uint32_t id0 = pg->id[0], k;
    if (ui_prefs & PREF_ANIM_OFF)
        return LV_NONE;
    switch (pg->graph) {
    case GR_FMENV:
        return fop_ok() ? LV_FMENV : LV_NONE;
    case GR_ADSR:
        if ((id0 == P_ATK || id0 == P_ED_PIT) && ENGINES[t->eng_req % NENGINES]->ownenv)   /* (FM6: AMP DEST draws the operator envelope, AMP ENV has none of its own) */
            return id0 == P_ED_PIT && t->eng_req % NENGINES == ENGI_FM6 && fop_ok() ? LV_FMENV : LV_NONE;
        return LV_ENV;
    case GR_LFO:
        return LV_LFO;
    case GR_CYCLE:
        return LV_CYCLE;
    case GR_FILT:                                    /* graph_filt's kinds: 1 TWIN FILTER, 2 TWIN MOD, 3 TWIN CHAIN (no ticks), 4 MULTI FILTER with filter 2 running */
        k = id0 == P_TWTYPE ? 1u : id0 == XW_BASE + X_KTRK2 ? 2u : id0 == XW_BASE + X_ROUTE ? 3u : (id0 == P_FREEA || id0 == P_KTRK) && twin_on(t) ? 4u : 0u;
        return k == 3u ? LV_NONE : k ? LV_TWIN : ENGINES[t->engine % NENGINES]->ufilt ? LV_FILT : LV_NONE;
    case GR_CHORD:
        return t->p[P_CHRD] ? LV_CHORD : LV_NONE;
    case GR_INS:
        return t->p[P_ITYPE] >= IT_PHASER && t->p[P_ITYPE] <= IT_CHORUS ? LV_INS : LV_NONE;
    case GR_MOD:
        return LV_MOD;
    default:
        return LV_NONE;
    }
}

/* ---- the shared parts */
/* the UI clock of a PREVIEW (id: whose): a Q32 phase advanced by inc (Q32 a ms) over the real time since the last look; start where it begins. Writes no audio state */
static uint32_t lv_uiph(uint32_t id, uint64_t inc, uint32_t start)
{
    uint32_t now = fm1_ms, dt;
    if (lv.uid != id)
        lv.uid = id, lv.uph = start, lv.ums = now;
    dt = now - lv.ums;
    lv.uph += (uint32_t)(inc * (dt > 1000u ? 1000u : dt));
    lv.ums = now;
    return lv.uph;
}
/* the cycle (of n drawn) a sweep is on: a phase that fell back since the last look is the next one; another sweep starts on the first */
static uint32_t lv_cyc(uint32_t ph, uint32_t who, uint32_t n)
{
    if (lv.who != who)
        lv.who = who, lv.odd = 0;
    else if (ph < lv.ph)
        lv.odd = (lv.odd + 1u) % n;
    lv.ph = ph;
    return lv.odd;
}
static int32_t lv_creep(int32_t room)                /* a held marker moves on 8 px a second, whatever the look rate */
{
    int32_t c;
    if (!lv.t0)
        lv.t0 = fm1_ms | 1u;
    c = (int32_t)((fm1_ms - lv.t0) >> 7);
    return c < room ? c : room;
}
static void lv_mark(void)                            /* the playhead: a line over the picture's height and a dot */
{
    cv_rect(lv.x, LV_LINE[lv.kind][0], 1, LV_LINE[lv.kind][1], T_ACCENT);
    cv_rrect(lv.x - 3, lv.y - 3, 7, 7, 3, T_TEXT, T_BG);
}

/* ---- ENV: graph_adsr's curve, as one pixel walk (x 12 .. 226, levels 0 .. 1000) */
typedef struct { int32_t a, d, r, top, bot, sus, x0, x1, x3, rel; } lv_eg_t;
static void lv_eg(const track_t *t, uint32_t e0, lv_eg_t *g)
{
    g->a = 4 + t->p[e0] * 50 / 127;
    g->d = 6 + t->p[e0 + 1u] * 50 / 127;
    g->r = 6 + t->p[e0 + 3u] * 60 / 127;
    g->top = 6 * graph_ht / 100, g->bot = 88 * graph_ht / 100;
    g->sus = g->rel = t->p[e0 + 2u] * 1000 / 127;
    g->x0 = 12, g->x1 = g->x0 + g->a, g->x3 = 226 - g->r;
}
static int32_t lv_eg_lvl(const lv_eg_t *g, int32_t x, int32_t *e)   /* the level at x: x goes up by 1, e (Q15, 1 at a stage's start) is carried */
{
    if (x > g->x3) {                                 /* release: exponential from rel */
        if (x == g->x3 + 1)
            *e = 32768;
        *e = (*e * (g->r > 4 ? 32768 - 150733 / g->r : 0)) >> 15;
        return g->rel * *e >> 15;
    }
    if (x > g->x1 + g->d)
        return g->sus;
    if (x > g->x1) {                                 /* decay: exponential to SUS */
        if (x == g->x1 + 1)
            *e = 32768;
        *e = (*e * (32768 - 150733 / g->d)) >> 15;
        return g->sus + ((1000 - g->sus) * *e >> 15);
    }
    return (x - g->x0) * 1000 / g->a;                /* attack: linear */
}
static int32_t lv_eg_y(const lv_eg_t *g, int32_t x, int32_t *e)   /* the picture's row at x: the attack is one straight line, the rest one vertex a pixel */
{
    int32_t l = lv_eg_lvl(g, x, e);
    return x <= g->x1 && x <= g->x3 ? g->bot - (x - g->x0) * (g->bot - g->top) / g->a : g->bot - l * (g->bot - g->top) / 1000;
}
static int32_t lv_eg_at(const lv_eg_t *g, int32_t x)
{
    int32_t i, e = 32768, y = g->bot;
    for (i = g->x0; i <= x; i++)
        y = lv_eg_y(g, i, &e);
    return y;
}
/* the first x after from at which the curve has fallen to lvl (to: none before it) */
static int32_t lv_eg_find(const lv_eg_t *g, int32_t from, int32_t to, int32_t lvl)
{
    int32_t x, e = 32768;
    for (x = from + 1; x < to; x++)
        if (lv_eg_lvl(g, x, &e) <= lvl)
            break;
    return x;
}
/* the place of level lvl in stage 1 (attack) or 2 (decay) */
static int32_t lv_eg_x(const lv_eg_t *g, uint32_t stg, int32_t lvl)
{
    return stg == 1u ? g->x0 + g->a * lvl / 1000 : lv_eg_find(g, g->x1, g->x1 + g->d, lvl);
}
static void lv_eg_lit(const lv_eg_t *g, int32_t xmax, uint16_t c)   /* the part already played, lit over the picture's curve */
{
    int32_t x, e = 32768, y, py = 0;
    for (x = g->x0; x <= xmax; x++, py = y) {
        y = lv_eg_y(g, x, &e);
        if (x > g->x0 && (y - py > 1 || py - y > 1))
            cv_line_t(x - 1, py, x, y, c, 2);
        else
            cv_rect(x, y, 1, 2, c);
    }
}
static void lv_env_take(const track_t *t, const page_t *pg)
{
    uint32_t e0 = env_of(pg), e2 = e0 == P_E2ATK, vi = t->m_vi % NVOICE, stg, id = 1u + song.sel * 2u + e2;
    const voice_t *v = &t->v[vi];
    lv_eg_t g;
    int32_t lvl, x;
    lv.n = 0;
    if ((e2 && !lv_e2_on(t)) || !v->active) {        /* (ENV 2 is ticked only while something reads it) */
        lv.evi = 0;
        return;
    }
    stg = e2 ? v->stage2 : v->stage;
    lvl = (int32_t)(((int64_t)clamp(e2 ? v->env2 : v->env, 0, 1 << 24) * 1000) >> 24);
    lv_eg(t, e0, &g);
    if (lv.epid != id || lv.evi != vi + 1u || (stg == 1u && lv.estg != 1u))   /* a new note or view */
        lv.t0 = 0, lv.evi = (uint8_t)(vi + 1u), lv.estg = 0, lv.elvl = (int16_t)lvl;
    lv.epid = (uint8_t)id;
    if (stg == 3u && lv.estg != 3u)                  /* the release begins: from the higher of the last level seen and this one */
        lv.erel = (int16_t)(lv.elvl > lvl ? lv.elvl : lvl);
    lv.estg = (uint8_t)stg;
    if (stg == 3u) {                                 /* the drawn release starts at SUS: the marker is as far down it as the real one is down from where it began (an early release: from the line's start) */
        int32_t share = lv.erel ? lvl * g.sus / lv.erel : 0;
        x = share < g.sus ? lv_eg_find(&g, g.x3, g.x3 + g.r, share) : g.x3;
    } else if (stg == 1u || stg == 2u) {
        x = lv_eg_x(&g, stg, lvl);
        if (stg == 2u && g.sus && g.x3 > g.x1 + g.d && x >= g.x1 + g.d - 1)   /* held on the sustain: creeping along it */
            x = g.x1 + g.d + lv_creep(g.x3 - g.x1 - g.d - 1);
        else
            lv.t0 = 0;
    } else {
        return;
    }
    lv.x = (int16_t)x, lv.y = (int16_t)lv_eg_at(&g, x), lv.elvl = (int16_t)lvl;   /* (on the drawn line: the row of the picture at x, not of the level) */
    lv.n = 1;
}

/* ---- FM6: graph_fm6env's geometry (the curve on canvas rows 23 .. 66), the marker from the note's operator envelope */
static void lv_fm_take(void)
{
    track_t *t = TSEL;
    uint32_t vi = t->m_vi % NVOICE, stage, seg, id = 32u + song.sel * 6u + fop_op;
    const fm6_note_t *n;
    const fm6_env_t *e;
    const uint8_t *op = fop_bytes(song.sel);
    int32_t top = 23 - GOY, bot = 66 - GOY, k, x[6], y[6], from, to, u, total, rate;
#define LFEGY(l) (bot - (int32_t)(l) * (bot - top) / 99)
#define LFEGW(r) (4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801)
    lv.n = 0;
    if (t->engine != ENGI_FM6 || !t->v[vi].active || !(n = fm6_note_of(t, &t->v[vi])) || !n->live) {
        lv.evi = 0;
        return;
    }
    e = &n->env[5u - fop_op % 6u];
    stage = e->ix < 3u ? e->ix : e->down ? 4u : 3u;   /* 4: held at the last level, 3: released */
    if (lv.epid != id || lv.evi != vi + 1u || lv.estg != stage)
        lv.t0 = 0;
    lv.epid = (uint8_t)id, lv.evi = (uint8_t)(vi + 1u), lv.estg = (uint8_t)stage;
    x[0] = 12, y[0] = LFEGY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++)
        x[k + 1] = x[k] + LFEGW(op[FP_R1 + k]), y[k + 1] = LFEGY(op[FP_L1 + k]);
    x[5] = 228, x[4] = x[5] - LFEGW(op[FP_R1 + 3]), y[4] = y[3], y[5] = y[0];
    if (stage == 4u) {
        lv.x = (int16_t)(x[3] + lv_creep(x[4] - x[3] - 1)), lv.y = (int16_t)y[3];
    } else {
        seg = stage < 3u ? stage : 4u;
        /* the same log-level targets as fm6_env_advance */
        from = (((fm6_scaleout(op[FP_L1 + (stage ? stage - 1u : 3u)]) >> 1) << 6) + e->outlevel - 4256);
        to = (((fm6_scaleout(op[FP_L1 + stage]) >> 1) << 6) + e->outlevel - 4256);
        from = (from < 16 ? 16 : from) << 16, to = (to < 16 ? 16 : to) << 16;
        if (e->ix >= 4u) u = 65536;
        else if (to != from)
            u = clamp((int32_t)(((int64_t)e->level - from) * 65536 / ((int64_t)to - from)), 0, 65536);
        else {                                       /* the level holds: the stage's static count runs down */
            rate = clamp(op[FP_R1 + stage] + e->rscale, 0, 99);
            total = rate < 77 ? FM6_STATICS[rate] : 20 * (99 - rate);
            if (!stage && !op[FP_L1] && rate < 77) total /= 20;
            u = total ? clamp(65536 - (int32_t)((int64_t)e->statc * 65536 / total), 0, 65536) : 65536;
        }
        lv.x = (int16_t)(x[seg] + (int32_t)(((int64_t)(x[seg + 1] - x[seg]) * u) >> 16));
        lv.y = (int16_t)(y[seg] + (int32_t)(((int64_t)(y[seg + 1] - y[seg]) * u) >> 16));
    }
    lv.n = 1;
#undef LFEGY
#undef LFEGW
}

/* ---- LFO and INSERT: graph_lfo's wave at its pixel x, graph_insert's sweep: one playhead over two cycles */
static int32_t lv_lfo_y(const track_t *t, int l2, int32_t x)
{
    uint32_t r = l2 ? P_L2RATE : P_LRATE, s = l2 ? P_L2SYNC : P_LSYNC, ph = (uint32_t)t->p[r + 2u] << 25;
    int32_t cy = graph_ht / 2 - 1, a = 38 * graph_ht / 100, uni = t->p[s + 2u] != 0, z = uni ? cy + a : cy;
    int32_t w = lfo_wave_of(t->p[r + 1u], l2 ? t->lfo2.rnd : t->lfo_rnd, ph + (uint32_t)x * LV_LFO_STEP);
    if (t->p[r + 1u] == 4)
        w = (int32_t)((x / 20 * 2654435761u) >> 16) - 32768;
    return uni ? z - (w + 32768) * a / 32768 : cy - w * a / 32768;
}
static int32_t lv_sweep_x(uint32_t rel, uint32_t who)   /* the phase since the start -> the panel's x */
{
    uint32_t dx = rel / LV_LFO_STEP + lv_cyc(rel, who, 2u) * (PANEL_W / 2u);
    return PANEL_X0 + (int32_t)(dx < PANEL_W ? dx : PANEL_W - 1u);
}
static void lv_lfo_take(const track_t *t, const page_t *pg)
{
    int l2 = lv_lfo2_page(pg);
    uint32_t ph0 = (uint32_t)t->p[(l2 ? P_L2RATE : P_LRATE) + 2u] << 25, ph = l2 ? t->lfo2.ph : t->lfo_ph;
    lv.pv = l2 && !lv_l2_on(t);                      /* nothing reads LFO 2: its phase stands still; the preview runs at the same rate */
    if (lv.pv)
        ph = lv_uiph(1u + song.sel, (uint64_t)(t->p[P_L2SYNC] ? lv_sync_inc((uint32_t)t->p[P_L2SYNC]) : LFO_INC[t->p[P_L2RATE] & 127]) * FS / (1000u * CTL), ph);
    lv.x = (int16_t)lv_sweep_x(ph - ph0, (uint32_t)l2 + 1u);
    lv.y = (int16_t)lv_lfo_y(t, l2, lv.x - PANEL_X0);
    lv.n = 1;
}
static void lv_ins_take(const track_t *t)
{
    int32_t ty = t->p[P_ITYPE], cy = graph_ht / 2, a = 38 * graph_ht / 100, w;
    uint32_t ph, tr;
    lv.x = (int16_t)lv_sweep_x(ins[song.sel % NLIVE].ph, 3u);   /* (the part by its index) */
    ph = (uint32_t)(lv.x - PANEL_X0) * LV_LFO_STEP, tr = pf_tri(ph);
    w = ty == IT_CHORUS ? osc_sine(ph) * 2 : ty == IT_FLANGER ? (int32_t)((tr * tr) >> 16) * 2 - 65536 : (int32_t)tr * 2 - 65536;
    lv.y = (int16_t)(cy - (w >> 1) * a / 32768 * t->p[P_IB] / 127);
    lv.n = 1;
}

/* ---- CYCLE: graph_cycle's geometry; the latest note's real generator, or a PREVIEW of its rise and fall */
typedef struct { int32_t top, base, x0, n, cw, rw, fw; } lv_cy_t;
static void lv_cy_geom(const track_t *t, lv_cy_t *g)
{
    const int big = cur_page()->id[0] == XW_BASE + X_CYCURVE;
    uint64_t dr, df;
    uint32_t r = (uint32_t)XV(t, X_CYRISE) & 127u, f = (uint32_t)XV(t, X_CYFALL) & 127u;
    g->top = CY_TOP, g->base = CY_BASE, g->x0 = PANEL_X0 + 2, g->n = big || XV(t, X_CYMODE) ? 1 : 3;
    if (XV(t, X_CYSYNC))
        dr = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)r)] + 2u, df = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)f)] + 2u;
    else
        dr = (1ull << 40) / (ENV_LIN[r] ? ENV_LIN[r] : 1u), df = (1ull << 40) / (ENV_LIN[f] ? ENV_LIN[f] : 1u);
    g->cw = XV(t, X_CYMODE) && !big ? (PANEL_W - 4) * 2 / 3 : (PANEL_W - 4) / g->n;
    g->rw = clamp(big ? g->cw / 2 : (int32_t)((uint64_t)g->cw * dr / (dr + df)), 3, g->cw - 3);
    g->fw = g->cw - g->rw;
}
static void lv_cycle_take(const track_t *t)
{
    uint32_t pi = song.sel % NLIVE, vi = trk[pi].m_vi % NVOICE, st, phase;
    int32_t lin, i, u, y01, cyc, c = clamp(XV(t, X_CYCURVE), -64, 63) * 512;
    lv_cy_t g;
    lv.pv = !trk[pi].cy || !trk[pi].v[vi].active;
    if (!lv.pv) {
        st = cy_st[pi][vi], lin = clamp(cy_lin[pi][vi], 0, 1 << 24);
    } else {                                         /* its own clock at the set RISE and FALL (ONCE: a second of rest) */
        uint32_t r = (uint32_t)XV(t, X_CYRISE) & 127u, f = (uint32_t)XV(t, X_CYFALL) & 127u, q = beat_samples();
        uint32_t kr = XV(t, X_CYSYNC) ? esync_one(q, (int32_t)r, 0) : ENV_LIN[r], kf = XV(t, X_CYSYNC) ? esync_one(q, (int32_t)f, 0) : ENV_LIN[f];
        uint32_t rise = (uint32_t)(((1ull << 24) * CTL * 1000u) / ((uint64_t)FS * (kr ? kr : 1u))) + 1u;
        uint32_t fall = (uint32_t)(((1ull << 24) * CTL * 1000u) / ((uint64_t)FS * (kf ? kf : 1u))) + 1u;
        uint32_t period = rise + fall + (XV(t, X_CYMODE) ? 1000u : 0u), pos = (uint32_t)((uint64_t)lv_uiph(4u + song.sel, (1ull << 32) / period, 0) * period >> 32);
        st = pos < rise ? 1u : pos < rise + fall ? 2u : 0u;
        lin = st == 1u ? (int32_t)((uint64_t)pos * (1u << 24) / rise) : st == 2u ? (1 << 24) - (int32_t)((uint64_t)(pos - rise) * (1u << 24) / fall) : 0;
    }
    phase = st == 1u ? (uint32_t)lin : st == 2u ? (2u << 24) - (uint32_t)lin : 2u << 24;
    lv_cy_geom(t, &g);
    cyc = (int32_t)lv_cyc(phase, 4u + pi * NVOICE + vi, (uint32_t)g.n);
    i = st == 1u ? (int32_t)(((int64_t)g.rw * lin) >> 24) : st == 2u ? g.rw + (int32_t)(((int64_t)g.fw * ((1 << 24) - lin)) >> 24) : g.cw;   /* the px in the cycle */
    lv.x = (int16_t)(g.x0 + cyc * g.cw + i);
    if (i <= g.rw)                                   /* the row of graph_cycle's line at that px: y = x + c (x^2 - x) on the rise, 1 - that on the fall */
        u = i * 32768 / g.rw, y01 = u + ((c * ((u * u >> 15) - u)) >> 15);
    else
        u = (i - g.rw) * 32768 / g.fw, y01 = 32768 - (u + ((c * ((u * u >> 15) - u)) >> 15));
    lv.y = (int16_t)(g.base - clamp(y01, 0, 32768) * (g.base - g.top) / 32768);
    lv.n = 1;
}

/* ---- the others' taps */
static void lv_filt_take(const track_t *t)           /* the latest note's effective cutoffs (t->lvc, LVC2(t): Q8 dial steps >> 7) while a voice sounds */
{
    uint32_t i, on = 0;
    for (i = 0; i < NVOICE; i++)
        on |= t->v[i].active && t->v[i].stage != 4u;
    lv.fc = on && ENGINES[t->engine % NENGINES]->ufilt ? gf_cut_hz(40 + 198 * (int32_t)t->lvc) : 0u;
    lv.fc2 = on && twin_on(t) && t->p[P_TWTYPE] != TT_COMB ? gf_cut_hz(40 + 198 * (int32_t)LVC2(t)) : 0u;
}
static int32_t lv_voice_lvl(const voice_t *v)        /* a voice's envelope level, 0 .. 1000 (0: not sounding) */
{
    return !v->active || v->stage < 1u || v->stage > 3u ? 0 : (int32_t)(((int64_t)clamp(v->env, 0, 1 << 24) * 1000) >> 24);
}

/* ---- graph_signature: take the positions (every 2nd or 4th frame), 0 when nothing live is on the page */
static uint32_t live_sig(const page_t *pg, const track_t *t)
{
    uint32_t h = LV_H0, i, k;
    if (TIDE_AMPDEST_FM6(pg, t))                     /* (the operator envelope's picture, live or not: it follows the operator, the stage, the patch) */
        h = (h ^ ((fop_op + 1u) * 40503u + (fop_stg + 1u) * 7919u + fm6_pgen[song.sel % NTRK] * 2246822519u)) * 16777619u;
    if ((ui.frame % LV_EVERY || song.cpu_q8 > LV_BUSY) && lv.kind == live_kind(pg, t))
        return lv.sig | (h ^ LV_H0) ? lv.sig ^ h : 0u;
    k = lv.kind = live_kind(pg, t);
    lv.sig = lv.n = lv.pv = lv.fc = lv.fc2 = lv.ch = 0;
    if (k == LV_ENV)
        lv_env_take(t, pg);
    else if (k == LV_FMENV)
        lv_fm_take();
    else if (k == LV_LFO)
        lv_lfo_take(t, pg);
    else if (k == LV_INS)
        lv_ins_take(t);
    else if (k == LV_CYCLE)
        lv_cycle_take(t);
    else if (k == LV_FILT || k == LV_TWIN)
        lv_filt_take(t);
    else if (k == LV_MOD)                            /* (the bars move in strips, live_strip: the page is not drawn again for them) */
        for (i = 0; i < 4u; i++) {
            const int16_t *p = &t->p[MSLOT_ID(mod_page_base(pg) + i)];
            lv.mv[i] = p[0] && p[1] && p[2] && !mod_dst_why(t, p[1]) ? clamp(lv_tsrc(t, (uint32_t)p[0], mulq15(t->lfo_val, t->lfo_fade)), -32767, 32767) : INT32_MIN;
        }
    else if (k == LV_CHORD)
        for (i = 0; i < NVOICE; i++)
            lv.ch = (lv.ch ^ ((uint32_t)lv_voice_lvl(&t->v[i]) / 32u | (uint32_t)t->v[i].note << 8 | (uint32_t)(t->v[i].stage == 3u) << 16)) * 16777619u;
    if (!lv.pv)
        lv.uid = 0;
    if (k != LV_LFO && k != LV_INS && k != LV_CYCLE)
        lv.who = 0;
    if (k == LV_NONE)
        return h ^ LV_H0 ? h : 0u;
    lv.sig = ((uint32_t)(uint16_t)lv.x | (uint32_t)(uint16_t)lv.y << 12 | lv.n << 28) ^ lv.fc ^ lv.fc2 * 2654435761u ^ lv.ch;
    lv.sig = (lv.sig + 1u) * 2654435761u ^ k;       /* (never 0 on a live page: the overlay going away is a redraw) */
    return lv.sig ^ h;
}

/* ---- drawing, over the picture just drawn (canvas rows of graph_ht, cv_oy GOY) */
static void lv_filt_draw(const track_t *t)           /* MULTI FILTER: graph_filt's response at the live cutoff */
{
    int32_t md = clamp(t->p[P_FREEC], 0, UF_HP), res = clamp(t->p[P_FREEB], 0, 127);
    int32_t wb = md <= 64 ? md << 4 : ((UF_HP - md) * 65) >> 2, wl = md < 64 ? 1024 - wb : 0, wh = md > 64 ? 1024 - wb : 0;
    int32_t s24 = t->p[P_FREED] == 2, k1 = s24 ? UF_K24A : UF_K(res), k2 = UF_K24B(res), px = 0, py = 0;
    uint32_t i, f = GF_F0;
    if (clamp(t->p[P_FREEA], 0, UF_STEPS) >= UF_STEPS && md <= 0)   /* (the bypass: graph_filt draws a flat line and no cutoff) */
        return;
    cv_rect(PANEL_X0 + gf_x(lv.fc << 8), 4, 1, 92, T_ACCENT);
    for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
        int32_t db = svf_db10(f, lv.fc, wl, wb, wh, k1, k1), x = PANEL_X0 + 4 * (int32_t)i, y;
        if (s24)
            db += svf_db10(f, lv.fc, wl, wb, wh, k2, k2);
        y = 30 - clamp(db, -420, 180) * 3 / 20;
        if (i)
            cv_line_t(px, py, x, y, T_ACCENT, 1);
        px = x, py = y;
    }
}
static void live_draw(const page_t *pg, const track_t *t)
{
    int32_t x;
    if (!lv.sig || lv.kind != live_kind(pg, t))
        return;
    cv_oy = GOY;
    if (LV_LINE[lv.kind][1]) {                       /* the playhead kinds */
        if (lv.n) {
            if (lv.kind == LV_ENV) {                 /* the curve played so far, lit (the text tone where the accent is the part colour) */
                lv_eg_t g;
                lv_eg(t, env_of(pg), &g);
                lv_eg_lit(&g, lv.x, T_ACCENT != T_THEME ? T_ACCENT : T_TEXT);
            }
            lv_mark();
        }
        if (lv.pv) {                                 /* PREVIEW: the caption row, no ground of its own */
            int32_t o = cv_oy;
            cv_oy = 0;
            cv_text(grid_on ? UT_GR_NOTE_X : PANEL_X0 + 2, grid_on ? UT_TY(S, UT_GR_NOTE_BASE - UT_GR_Y) : 94 + o, &AF_S, "PREVIEW", T_MID);
            cv_oy = o;
        }
    } else if (lv.kind == LV_FILT) {
        if (lv.fc)
            lv_filt_draw(t);
    } else if (lv.kind == LV_TWIN) {                 /* a tick per filter, a cap at the top for filter 1, at the bottom for filter 2 */
        if (lv.fc) {
            x = PANEL_X0 + gf_x(lv.fc << 8);
            cv_rect(x, 4, 1, 58, T_ACCENT), cv_rect(x - 2, 4, 5, 3, T_ACCENT);
        }
        if (lv.fc2) {
            x = PANEL_X0 + gf_x(lv.fc2 << 8);
            cv_rect(x, 4, 1, 58, T_ACCENT), cv_rect(x - 2, 59, 5, 3, T_ACCENT);
        }
    }
}

/* MATRIX: under each acting row, in the 2 px gap below it, a bar from the left (from the middle for a bipolar source) over a RAISE track, below the source's column
 * (x 34 .. 93). graph_mod_new draws them with the page (live_mod, per band: cv_oy = - the band's first screen row); while the page stands still they move alone, as a 96 x 2
 * strip each (live_strip, from draw_graph's early return): a page redraw is 320 k basic blocks, a strip 100 */
static int32_t lv_mq_of(int32_t v) { return v == INT32_MIN ? 0xFFFF : (v + 32768) >> 9; }
static void lv_bar(const track_t *t, uint32_t base, uint32_t k, int32_t y)
{
    int32_t v = lv.mv[k], w;
    uint32_t s = (uint32_t)t->p[MSLOT_ID(base + k)];
    lv.mq[k] = lv_mq_of(v);
    if (v == INT32_MIN)
        return;
    cv_rect(34, y, 60, 2, T_RAISE);
    if (MS_BIPOLAR(s) && MS_GAIN(t->p, s) == s)
        w = v * 30 / 32767, cv_rect(w < 0 ? 64 + w : 64, y, w < 0 ? -w : w ? w : 1, 2, T_ACCENT);
    else if ((w = clamp(v, 0, 32767) * 60 / 32767))
        cv_rect(34, y, w, 2, T_ACCENT);
}
static void live_mod(const track_t *t, int32_t y0)
{
    uint32_t k;
    for (k = 0; lv.sig && lv.kind == LV_MOD && k < 4u; k++) {
        int32_t y = MX_Y0 + (int32_t)k * MX_PITCH + MX_H;
        if (y >= y0 && y + 2 <= y0 + (int32_t)cv_h)
            lv_bar(t, mod_page_base(cur_page()), k, y);
    }
}
static void live_strip(const page_t *pg, const track_t *t)
{
    uint32_t k;
    for (k = 0; lv.sig && lv.kind == LV_MOD && pg->graph == GR_MOD && k < 4u; k++) {
        int32_t y = MX_Y0 + (int32_t)k * MX_PITCH + MX_H;
        if (lv_mq_of(lv.mv[k]) == lv.mq[k])
            continue;
        cv_begin(96, 2, T_BG);
        cv_oy = -y;
        lv_bar(t, mod_page_base(pg), k, y);
        cv_oy = 0;
        cv_blit(0, (uint32_t)y);
    }
}

/* CHORD: in the area's canvas (screen rows), over kb_draw's keys (chord_kb): in each key a voice sounds, a bar from its bottom as high as the voice's envelope level; the keys are
 * the two octaves from the C below the lowest sounding note (chord_live_keys), or the last chord's (chord_keys) */
static void live_chord(const track_t *t)
{
    static const uint8_t IX[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   /* a white key's index in the octave; a black key's: the white key it follows */
    uint32_t set[4], lo = 128u, root, i, j;
    int32_t pitch = UT_A_KB_W + KB_GAP, bh = UT_A_KB_H * KB_BLACK_PCT / 100, bw = (pitch / 14 - KB_GAP) * KB_BLACK_W_NUM / KB_BLACK_W_DEN;
    if (!lv.sig || lv.kind != LV_CHORD)
        return;
    if (chord_sounding(set)) {
        for (i = 0; i < 128u && lo == 128u; i++)
            lo = (set[i >> 5] >> (i & 31u)) & 1u ? i - i % 12u : 128u;
    } else {
        (void)chord_keys(t, &lo, &root);
    }
    for (j = 0; j < NVOICE; j++) {
        int32_t lvl = lv_voice_lvl(&t->v[j]), d = (int32_t)t->v[j].note - (int32_t)lo, s = d % 12, o = 7 * (d / 12), blk = 0x54A >> (s & 15) & 1, x, w, h;
        if (!lvl || d < 0 || d >= (int32_t)CHORD_KB_SPAN)
            continue;
        if (blk) {                                   /* a black key: its bar inside the key, 2 px narrower, in its upper part */
            x = UT_A_KB_X + (IX[s] + o + 1) * pitch / 14 - KB_GAP / 2 - bw / 2 + 2, w = bw - 4, h = 1 + lvl * (bh - 4) / 1000;
        } else {                                     /* a white key: the middle of its part under the black keys */
            int32_t i0 = IX[s] + o;
            x = (UT_A_KB_X + i0 * pitch / 14 + UT_A_KB_X + (i0 + 1) * pitch / 14 - KB_GAP - 3) / 2, w = 3, h = 1 + lvl * (UT_A_KB_H - bh - 8) / 1000;
        }
        if (w >= 1)
            cv_rect(x, UT_A_KB_Y + (blk ? bh : UT_A_KB_H) - 3 - h, w, h, t->v[j].stage == 3u ? T_MID : T_INK);
    }
}
