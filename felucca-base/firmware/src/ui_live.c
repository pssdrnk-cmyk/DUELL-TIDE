/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE LIVE GRAPHS (the animation DUELL JAM 1.0 had on its sound-design pages, ported to 1.1's page and picture layout): over the pictures of
 * ui_graph.c, what the selected part is doing now. Only an overlay: every picture is drawn as before, nothing it shows is taken away.
 *   ENV     GR_ADSR (AMP ENV, ENV 2 and their DEST pages): one playhead, the latest note's: a line and a dot travelling along the curve with
 *           its envelope (up the attack, down the decay, creeping along the sustain while held, down the release), the curve already played lit.
 *           A release that begins before the sustain continues from the observed point (the lit path leaves the picture's release there).
 *           ENV 2 only while the audio ticks it. An engine with its own envelopes (FM6) has no AMP ENV playhead
 *   FM6     OP ENV (GR_FMENV) and AMP DEST (its picture is the same operator envelope, tide.h TIDE_AMPDEST_FM6): the selected operator's real
 *           envelope stage and level in fm6_note_t, read only
 *   LFO     GR_LFO: the playhead on the audio phase; an LFO 2 nothing reads has a silent UI clock (PREVIEW)
 *   CYCLE   GR_CYCLE: the latest note's real rise / fall position and value; without a sounding note a silent UI preview
 *   FILTER  GR_FILT, the MULTI FILTER: the response at the latest note's effective cutoff (voice.c t->lvc: the dial plus every route and KEY TRK)
 *   TWIN    the TWIN pictures: a tick at each filter's effective cutoff (t->lvc, t->lvc2), while a voice sounds
 *   INSERT  PHASER FLANGER CHORUS: the sweep's playhead (fx.c ins[].ph) on graph_insert's two cycles
 *   MOD     the matrix: under each row that acts, its source now (mod.c mod_tsrc: the latest note's for a per-voice source)
 *   CHORD   the keyboard: in each sounding key a bar at its voice's envelope level (releasing: shorter lived, dimmer)
 * Hard rule: viewing a graph never ticks a generator, never adds a route: the sources are read from the audio's state (a torn read draws one
 * frame wrong, nothing else); the only audio-side code is the two bytes lvc / lvc2 it writes for the FILTER and TWIN ticks (never read by it).
 * The positions are taken every 2nd frame (30 a second) for the envelopes, every 4th (15 a second) for the pictures that cost more to draw (LV_MASK), and the panel is drawn again only when one moved (live_sig, in
 * graph_signature); MENU > ANIM OFF (PREF_ANIM_OFF) takes none: the pictures are static.
 * Included by ui_graph.c after the pictures: the geometry below is theirs (graph_adsr, graph_lfo, graph_cycle, graph_filt, graph_insert) */

enum { LV_NONE, LV_ENV, LV_LFO, LV_FILT, LV_CHORD, LV_TWIN, LV_INS, LV_MOD, LV_CYCLE, LV_FMENV };
#define LV_LFO_STEP (0xFFFFFFFFu / (PANEL_W / 2u))   /* graph_lfo: two cycles over the panel */

static struct {
    uint32_t sig, kind;
    uint32_t n;                                      /* a live / preview marker is present */
    int16_t ex, ey;                                  /* ENV, OP ENV: the playhead as shown */
    int16_t elvl, erel, erelx;                       /* .. the last observed level, the release's starting level and x */
    uint8_t evi, estg, ecrawl, epid;                 /* .. voice (+1), last stage, sustain creep, part / envelope identity */
    int16_t lx, ly;                                  /* LFO, INSERT, CYCLE: the playhead */
    uint32_t ph, odd, lfo;                           /* .. the phase last seen, which of the two cycles, whose (lv_sweep) */
    uint32_t preview_ph, preview_ms, preview_id;     /* UI-only clock for an unused LFO 2 / CYCLE; never writes audio state */
    uint32_t fc, fc2;                                /* FILTER, TWIN: the live cutoffs (gf_cut_hz), 0 none */
    int32_t mv[4];                                   /* MOD: the four rows' sources (Q15), INT32_MIN: the slot does nothing */
} lv;

/* mod.c mod_tsrc runs in RAM (range X, tools/ram_fns_x.py): flash code may enter it only through a pointer */
static int32_t (*volatile const lv_tsrc)(const track_t *, uint32_t, int32_t) = mod_tsrc;

/* what the audio reads (voice.c track_render: l2on, e2on), by the same terms; mod.c mod_begin leaves its flags e2 / l2 for the next look (its own on closes after the block) */
static int lv_l2_on(const track_t *t) { return t->mdw.l2 || (t->p[P_TWTYPE] && XV(t, X_LFO2)); }
static int lv_e2_on(const track_t *t) { return t->mdw.e2 || t->p[P_E2FLT] || (t->p[P_TWTYPE] && XV(t, X_ENV2)); }
static int lv_lfo2_page(const page_t *pg)            /* graph_lfo's test */
{
    return (pg->id[0] >= P_L2RATE && pg->id[0] <= P_L2FADE) || (pg->id[0] >= P_L2SYNC && pg->id[0] <= P_L2POL) || pg->id[0] == XW_BASE + X_LFO2;
}
/* graph_filt's kinds: 1 TWIN FILTER, 2 TWIN MOD, 3 TWIN CHAIN, 4 MULTI FILTER 1/2, 2/2 with filter 2 running, else 0 */
static uint32_t lv_filt_kind(const page_t *pg, const track_t *t)
{
    uint32_t id0 = pg->id[0];
    return id0 == P_TWTYPE ? 1u : id0 == XW_BASE + X_KTRK2 ? 2u : id0 == XW_BASE + X_ROUTE ? 3u : (id0 == P_FREEA || id0 == P_KTRK) && twin_on(t) ? 4u : 0u;
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
        if ((id0 == P_ATK || id0 == P_ED_PIT) && ENGINES[t->eng_req % NENGINES]->ownenv)   /* (FM6's AMP DEST draws its operator envelope; AMP ENV has no envelope of its own to follow) */
            return id0 == P_ED_PIT && t->eng_req % NENGINES == ENGI_FM6 && fop_ok() ? LV_FMENV : LV_NONE;
        return LV_ENV;
    case GR_LFO:
        return LV_LFO;
    case GR_CYCLE:
        return LV_CYCLE;
    case GR_FILT:
        k = lv_filt_kind(pg, t);
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

/* ---- ENV: graph_adsr's geometry (graph_ht rows, x 12 .. 226) */
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
/* a release can begin during the attack or the decay: its path starts at the observed point */
static void lv_eg_view(const track_t *t, uint32_t e0, lv_eg_t *g)
{
    lv_eg(t, e0, g);
    if (lv.kind == LV_ENV && lv.n && lv.estg == 3u) {
        g->x3 = lv.erelx, g->rel = lv.erel;
        g->r = clamp(g->r, 1, 227 - g->x3);          /* RELEASE may change under a running morph */
    }
}
/* the curve as graph_adsr draws it, up to x xmax, in colour c (the part already played, lit) */
static void lv_eg_curve(const lv_eg_t *g, int32_t xmax, uint16_t c)
{
    int32_t i, px, py, e = 32768, ax = xmax < g->x1 ? xmax : g->x1;
#define LEGY(lvl) (g->bot - (lvl) * (g->bot - g->top) / 1000)
    if (ax > g->x3) ax = g->x3;
    cv_line_t(g->x0, g->bot, ax, g->bot - (ax - g->x0) * (g->bot - g->top) / g->a, c, 2);
    px = g->x1, py = g->top;
    for (i = 1; i <= g->d && g->x1 + i <= xmax && g->x1 + i <= g->x3; i++) {
        e = (e * (32768 - 150733 / g->d)) >> 15;
        cv_line_t(px, py, g->x1 + i, LEGY(g->sus + ((1000 - g->sus) * e >> 15)), c, 2);
        px = g->x1 + i, py = LEGY(g->sus + ((1000 - g->sus) * e >> 15));
    }
    if (g->x3 > g->x1 + g->d && xmax > g->x1 + g->d)
        cv_line_t(px, py, xmax < g->x3 ? xmax : g->x3, LEGY(g->sus), c, 2);
    if (xmax <= g->x3) return;
    px = g->x3, py = LEGY(g->rel), e = 32768;
    for (i = 1; i <= g->r && g->x3 + i <= xmax; i++) {
        e = (e * (g->r > 4 ? 32768 - 150733 / g->r : 0)) >> 15;
        cv_line_t(px, py, g->x3 + i, LEGY(g->rel * e >> 15), c, 2);
        px = g->x3 + i, py = LEGY(g->rel * e >> 15);
    }
#undef LEGY
}

/* the place on the curve of a level lvl (0..1000) in stage (1 attack, 2 decay / sustain, 3 release); held at the sustain, crawl px along it */
static int lv_env_at(const track_t *t, uint32_t e0, uint32_t stage, int32_t lvl, int32_t crawl, int16_t *xo, int16_t *yo)
{
    lv_eg_t g;
    int32_t x, i, e = 32768, r;
    lv_eg(t, e0, &g);
    if (stage == 1u) {
        x = g.x0 + g.a * lvl / 1000;
    } else if (stage == 2u) {                        /* decay: the step of the drawn curve that has fallen to lvl; then along the sustain */
        for (i = 1; i < g.d; i++) {
            e = (e * (32768 - 150733 / g.d)) >> 15;
            if (g.sus + ((1000 - g.sus) * e >> 15) <= lvl)
                break;
        }
        x = g.x1 + i;
        if (i >= g.d - 1)
            x = g.x1 + g.d + (g.sus && g.x3 > g.x1 + g.d ? (crawl < g.x3 - g.x1 - g.d - 1 ? crawl : g.x3 - g.x1 - g.d - 1) : 0);
    } else if (stage == 3u) {                        /* release from its observed starting level */
        r = clamp(g.r, 1, 227 - lv.erelx);
        i = 0;
        if (lv.erel && lvl < lv.erel)
            for (i = 1; i < r; i++) {
                e = (e * (r > 4 ? 32768 - 150733 / r : 0)) >> 15;
                if ((lv.erel * e >> 15) <= lvl)
                    break;
            }
        x = lv.erelx + i;
    } else {
        return 0;
    }
    *xo = (int16_t)x;
    *yo = (int16_t)(g.bot - lvl * (g.bot - g.top) / 1000);
    return 1;
}
static void lv_env_take(const track_t *t, const page_t *pg)
{
    uint32_t e0 = env_of(pg), e2 = e0 == P_E2ATK, vi = t->m_vi % NVOICE, stg, id = 1u + song.sel * 2u + e2;
    const voice_t *v = &t->v[vi];
    int32_t lvl;
    int16_t x, y;
    lv.n = 0;
    if ((e2 && !lv_e2_on(t)) || !v->active) {        /* (ENV 2 is ticked only while something reads it) */
        lv.evi = 0;
        return;
    }
    stg = e2 ? v->stage2 : v->stage;
    lvl = (int32_t)(((int64_t)clamp(e2 ? v->env2 : v->env, 0, 1 << 24) * 1000) >> 24);
    if (lv.epid != id || lv.evi != vi + 1u || (stg == 1u && lv.estg != 1u))   /* a new note or view */
        lv.ecrawl = 0, lv.evi = (uint8_t)(vi + 1u), lv.estg = 0, lv.elvl = (int16_t)lvl;
    else if (stg == 2u && lv.ecrawl < 250u && !(ui.frame & 7u))
        lv.ecrawl++;                                 /* (held: creeping along the sustain, a pixel every 4th look: 7 redraws a second, not 30) */
    lv.epid = (uint8_t)id;
    if (stg == 3u && lv.estg != 3u) {
        lv_eg_t g;
        lv_eg(t, e0, &g);
        lv.erel = (int16_t)(lv.elvl > lvl ? lv.elvl : lvl);
        lv.erelx = (int16_t)g.x3;
        if (lv.estg == 1u || lv.erel > g.sus + 10) {
            lv_env_at(t, e0, lv.estg == 1u ? 1u : 2u, lv.erel, 0, &x, &y);
            lv.erelx = x;
        }
    }
    lv.estg = (uint8_t)stg;
    if (!lv_env_at(t, e0, stg, lvl, lv.ecrawl, &x, &y))
        return;
    lv.ex = x, lv.ey = y, lv.elvl = (int16_t)lvl;
    lv.n = 1;
}

/* ---- FM6: graph_fm6env's geometry (the 76 px card: the curve on canvas rows 23 .. 66), the playhead from the note's operator envelope */
typedef struct { int32_t x[6], y[6]; uint8_t level[4], rate[4]; } lv_fm_t;
static void lv_fm_geom(lv_fm_t *g)
{
    const uint8_t *op = fop_bytes(song.sel);
    int32_t top = 23 - GOY, bot = 66 - GOY, k;
#define LFEGY(l) (bot - (int32_t)(l) * (bot - top) / 99)
#define LFEGW(r) (4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801)
    for (k = 0; k < 4; k++)
        g->level[k] = op[FP_L1 + k], g->rate[k] = op[FP_R1 + k];
    g->x[0] = 12, g->y[0] = LFEGY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++)
        g->x[k + 1] = g->x[k] + LFEGW(op[FP_R1 + k]), g->y[k + 1] = LFEGY(op[FP_L1 + k]);
    g->x[5] = 228, g->x[4] = g->x[5] - LFEGW(op[FP_R1 + 3]), g->y[4] = g->y[3], g->y[5] = g->y[0];
#undef LFEGY
#undef LFEGW
}
/* the same log-level targets as fm6_env_advance, read in the UI only */
static int32_t lv_fm_target(const fm6_env_t *e, uint32_t level)
{
    int32_t v = ((fm6_scaleout((int32_t)level) >> 1) << 6) + e->outlevel - 4256;
    return (v < 16 ? 16 : v) << 16;
}
static void lv_fm_take(void)
{
    track_t *t = TSEL;
    uint32_t vi = t->m_vi % NVOICE, op = 5u - fop_op % 6u, stage, segment, id = 32u + song.sel * 6u + fop_op;
    fm6_note_t *n;
    const fm6_env_t *e;
    lv_fm_t g;
    int32_t from, to, u, total, rate;
    lv.n = 0;
    if (t->engine != ENGI_FM6 || !t->v[vi].active || !(n = fm6_note_of(t, &t->v[vi])) || !n->live) {
        lv.evi = 0;
        return;
    }
    e = &n->env[op];
    stage = e->ix < 3u ? e->ix : e->down ? 4u : 3u;   /* 4: held at the last level, 3: released */
    if (lv.epid != id || lv.evi != vi + 1u || lv.estg != stage)
        lv.ecrawl = 0;
    else if (stage == 4u && lv.ecrawl < 250u && !(ui.frame & 7u))
        lv.ecrawl++;
    lv.epid = (uint8_t)id, lv.evi = (uint8_t)(vi + 1u), lv.estg = (uint8_t)stage;
    lv_fm_geom(&g);
    if (stage == 4u) {
        lv.ex = (int16_t)(g.x[3] + clamp(lv.ecrawl, 0, g.x[4] - g.x[3] - 1));
        lv.ey = (int16_t)g.y[3];
    } else {
        segment = stage < 3u ? stage : 4u;
        from = lv_fm_target(e, g.level[stage ? stage - 1u : 3u]);
        to = lv_fm_target(e, g.level[stage]);
        if (e->ix >= 4u) u = 65536;
        else if (to != from)
            u = clamp((int32_t)(((int64_t)e->level - from) * 65536 / ((int64_t)to - from)), 0, 65536);
        else {
            rate = clamp(g.rate[stage] + e->rscale, 0, 99);
            total = rate < 77 ? FM6_STATICS[rate] : 20 * (99 - rate);
            if (!stage && !g.level[0] && rate < 77) total /= 20;
            u = total ? clamp(65536 - (int32_t)((int64_t)e->statc * 65536 / total), 0, 65536) : 65536;
        }
        lv.ex = (int16_t)(g.x[segment] + (int32_t)(((int64_t)(g.x[segment + 1] - g.x[segment]) * u) >> 16));
        lv.ey = (int16_t)(g.y[segment] + (int32_t)(((int64_t)(g.y[segment + 1] - g.y[segment]) * u) >> 16));
    }
    lv.n = 1;
}

/* ---- LFO: graph_lfo's wave at its pixel x (0 .. PANEL_W - 1) */
static int32_t lv_lfo_y(const track_t *t, int l2, int32_t x)
{
    uint32_t r = l2 ? P_L2RATE : P_LRATE, s = l2 ? P_L2SYNC : P_LSYNC, ph = (uint32_t)t->p[r + 2u] << 25;
    int32_t cy = graph_ht / 2 - 1, a = 38 * graph_ht / 100, uni = t->p[s + 2u] != 0, z = uni ? cy + a : cy;
    int32_t w = lfo_wave_of(t->p[r + 1u], l2 ? t->lfo2.rnd : t->lfo_rnd, ph + (uint32_t)x * LV_LFO_STEP);
    if (t->p[r + 1u] == 4)
        w = (int32_t)((x / 20 * 2654435761u) >> 16) - 32768;
    return uni ? z - (w + 32768) * a / 32768 : cy - w * a / 32768;
}
/* a sweep drawn as two cycles from phase ph0 (graph_lfo, graph_insert): the playhead's x at phase ph. who: whose phase it is (1 LFO 1, 2 LFO 2,
 * 3 the INSERT, 4.. the CYCLE of a note): another one starts on the first cycle */
static int16_t lv_sweep(uint32_t ph, uint32_t ph0, uint32_t who)
{
    uint32_t dx;
    if (lv.lfo != who)
        lv.lfo = who, lv.odd = 0;
    else if (ph - ph0 < lv.ph - ph0)                 /* (a cycle went by since the last look: the other half of the picture) */
        lv.odd ^= 1u;
    lv.ph = ph;
    dx = (ph - ph0) / LV_LFO_STEP + lv.odd * (PANEL_W / 2u);
    return (int16_t)(dx < PANEL_W ? dx : PANEL_W - 1u);
}
static uint32_t (*volatile const lv_sync_inc)(uint32_t) = lfo_sync_inc;
static void lv_lfo_take(const track_t *t, const page_t *pg)
{
    int l2 = lv_lfo2_page(pg);
    uint32_t ph = l2 ? t->lfo2.ph : t->lfo_ph;
    lv.n = 1;
    if (l2 && !lv_l2_on(t)) {                        /* nothing reads LFO 2: its phase stands still. The preview has its own clock (fm1_ms) at the same rate and never writes audio state */
        uint32_t id = 1u + song.sel, now = fm1_ms, dt, inc;
        if (lv.preview_id != id)
            lv.preview_id = id, lv.preview_ms = now, lv.preview_ph = ph;
        dt = now - lv.preview_ms;
        inc = t->p[P_L2SYNC] ? lv_sync_inc((uint32_t)t->p[P_L2SYNC]) : LFO_INC[t->p[P_L2RATE] & 127];
        lv.preview_ph += (uint32_t)((uint64_t)inc * (dt > 1000u ? 1000u : dt) * FS / (1000u * CTL));
        lv.preview_ms = now;
        ph = lv.preview_ph;
    } else {
        lv.preview_id = 0;
    }
    lv.lx = lv_sweep(ph, (uint32_t)t->p[(l2 ? P_L2RATE : P_LRATE) + 2u] << 25, (uint32_t)l2 + 1u);
    lv.ly = (int16_t)lv_lfo_y(t, l2, lv.lx);
}

/* ---- INSERT: graph_insert's sweep (PHASER a triangle, FLANGER the squared triangle, CHORUS a sine) at its pixel x, as deep as DEPTH */
static void lv_ins_take(const track_t *t)
{
    int32_t ty = t->p[P_ITYPE], cy = graph_ht / 2, a = 38 * graph_ht / 100, w;
    uint32_t ph, tr;
    lv.lx = lv_sweep(ins[song.sel % NLIVE].ph, 0u, 3u);   /* (the part by its index) */
    ph = (uint32_t)lv.lx * LV_LFO_STEP, tr = pf_tri(ph);
    w = ty == IT_CHORUS ? osc_sine(ph) * 2 : ty == IT_FLANGER ? (int32_t)((tr * tr) >> 16) * 2 - 65536 : (int32_t)tr * 2 - 65536;
    lv.ly = (int16_t)(cy - (w >> 1) * a / 32768 * t->p[P_IB] / 127);
    lv.n = 1;
}

/* ---- CYCLE: graph_cycle's geometry */
typedef struct { int32_t top, base, x0, n, cw, rw, fw; } lv_cy_t;
static void lv_cy_geom(const track_t *t, lv_cy_t *g)
{
    const int big = cur_page()->id[0] == XW_BASE + X_CYCURVE;
    int32_t w = PANEL_W - 4;
    uint64_t dr, df;
    uint32_t r = (uint32_t)XV(t, X_CYRISE) & 127u, f = (uint32_t)XV(t, X_CYFALL) & 127u;
    g->top = 8, g->base = 92, g->x0 = PANEL_X0 + 2, g->n = big || XV(t, X_CYMODE) ? 1 : 3;
    if (XV(t, X_CYSYNC)) {
        dr = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)r)] + 2u;
        df = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)f)] + 2u;
    } else {
        dr = (uint64_t)((1ull << 40) / (ENV_LIN[r] ? ENV_LIN[r] : 1u));
        df = (uint64_t)((1ull << 40) / (ENV_LIN[f] ? ENV_LIN[f] : 1u));
    }
    g->cw = XV(t, X_CYMODE) && !big ? w * 2 / 3 : w / g->n;
    g->rw = big ? g->cw / 2 : (int32_t)((uint64_t)g->cw * dr / (dr + df));
    g->rw = clamp(g->rw, 3, g->cw - 3);
    g->fw = g->cw - g->rw;
}
/* a UI-only preview for a CYCLE nothing sounds (not routed, or no note): its own clock at the set rise and fall, ONCE with a rest of a second */
static void lv_cycle_preview(const track_t *t, uint32_t *st, int32_t *lin, int32_t *value)
{
    uint32_t id = 4u + song.sel, now = fm1_ms, r = (uint32_t)XV(t, X_CYRISE) & 127u, f = (uint32_t)XV(t, X_CYFALL) & 127u;
    uint32_t kr = XV(t, X_CYSYNC) ? esync_one(beat_samples(), (int32_t)r, 0) : ENV_LIN[r];
    uint32_t kf = XV(t, X_CYSYNC) ? esync_one(beat_samples(), (int32_t)f, 0) : ENV_LIN[f];
    uint32_t rise = (uint32_t)(((1ull << 24) * CTL * 1000u) / ((uint64_t)FS * (kr ? kr : 1u))) + 1u;
    uint32_t fall = (uint32_t)(((1ull << 24) * CTL * 1000u) / ((uint64_t)FS * (kf ? kf : 1u))) + 1u;
    uint32_t period = rise + fall + (XV(t, X_CYMODE) ? 1000u : 0u), pos;
    int32_t x, c = clamp(XV(t, X_CYCURVE), -64, 63) * 512;
    if (lv.preview_id != id)
        lv.preview_id = id, lv.preview_ms = now, lv.preview_ph = 0;
    lv.preview_ph = (lv.preview_ph + now - lv.preview_ms) % period;
    lv.preview_ms = now, pos = lv.preview_ph;
    *st = pos < rise ? 1u : pos < rise + fall ? 2u : 0u;
    *lin = *st == 1u ? (int32_t)((uint64_t)pos * (1u << 24) / rise) :
           *st == 2u ? (1 << 24) - (int32_t)((uint64_t)(pos - rise) * (1u << 24) / fall) : 0;
    x = *lin >> 9;
    *value = *st == 1u ? cy_curve(x, c) : *st == 2u ? 32768 - cy_curve(32768 - x, c) : 0;
}
/* the real state of the latest note's cycle, or the preview */
static void lv_cycle_take(const track_t *t)
{
    uint32_t pi = song.sel % NLIVE, vi = trk[pi].m_vi % NVOICE, st, phase, who = 4u + pi * NVOICE + vi;
    int32_t lin, value;
    lv_cy_t g;
    lv.n = 0;
    if (!trk[pi].cy || !trk[pi].v[vi].active) {
        lv_cycle_preview(t, &st, &lin, &value);
    } else {
        lv.preview_id = 0;
        st = cy_st[pi][vi];
        lin = clamp(cy_lin[pi][vi], 0, 1 << 24);
        value = cy_y[pi][vi];
    }
    phase = st == 1u ? (uint32_t)lin : st == 2u ? (2u << 24) - (uint32_t)lin : 2u << 24;
    lv_cy_geom(t, &g);
    if (lv.lfo != who)
        lv.odd = 0;
    else if (phase < lv.ph)
        lv.odd = (lv.odd + 1u) % (uint32_t)g.n;
    lv.lfo = who, lv.ph = phase;
    lv.lx = (int16_t)(g.x0 + (int32_t)(lv.odd % (uint32_t)g.n) * g.cw +
        (st == 1u ? (int32_t)(((int64_t)g.rw * lin) >> 24) :
         st == 2u ? g.rw + (int32_t)(((int64_t)g.fw * ((1 << 24) - lin)) >> 24) : g.cw));
    lv.ly = (int16_t)(g.base - value * (g.base - g.top) / 32768);
    lv.n = 1;
}

/* ---- FILTER, TWIN: the latest note's effective cutoffs while a voice sounds (t->lvc, t->lvc2: Q8 dial steps >> 7) */
static void lv_filt_take(const track_t *t)
{
    uint32_t i, on = 0;
    for (i = 0; i < NVOICE; i++)
        on |= t->v[i].active && t->v[i].stage != 4u;
    lv.fc = on && ENGINES[t->engine % NENGINES]->ufilt ? gf_cut_hz(40 + 198 * (int32_t)t->lvc) : 0u;
    lv.fc2 = on && twin_on(t) && t->p[P_TWTYPE] != TT_COMB ? gf_cut_hz(40 + 198 * (int32_t)t->lvc2) : 0u;
}

/* ---- MOD: the source of each of the page's four rows that acts (graph_mod_new's "on"), Q15 as mod_tsrc gives it */
static void lv_mod_take(const track_t *t, const page_t *pg)
{
    uint32_t k, base = mod_page_base(pg);
    for (k = 0; k < 4u; k++) {
        const int16_t *p = &t->p[MSLOT_ID(base + k)];
        lv.mv[k] = p[0] && p[1] && p[2] && !mod_dst_why(t, p[1]) ? clamp(lv_tsrc(t, (uint32_t)p[0], mulq15(t->lfo_val, t->lfo_fade)), -32767, 32767) : INT32_MIN;
    }
}

/* ---- CHORD: a voice's envelope level, 0..1000 (0: not sounding) */
static int32_t lv_voice_lvl(const voice_t *v)
{
    if (!v->active || v->stage < 1u || v->stage > 3u)
        return 0;
    return (int32_t)(((int64_t)clamp(v->env, 0, 1 << 24) * 1000) >> 24);
}

/* ---- graph_signature: the positions now (taken every 2nd frame); 0 when nothing live is on the page */
#define LV_H0 2166136261u
/* the frames between two looks, minus 1 (a mask of ui.frame): the envelopes' pictures are cheap to draw again (about 14 k basic blocks), so they move at 30 a second; the others
 * redraw 66 .. 150 k a time (a line a pixel of the wave, the keyboard's keys): they are looked at every 4th frame (15 a second) */
static const uint8_t LV_MASK[10] = {1, 1, 3, 3, 3, 3, 3, 1, 3, 1};   /* LV_NONE ENV LFO FILT CHORD TWIN INS MOD CYCLE FMENV */
static uint32_t live_sig(const page_t *pg, const track_t *t)
{
    uint32_t h = LV_H0, i;
    if (TIDE_AMPDEST_FM6(pg, t))                     /* (the operator envelope's picture, the live kind or not: it follows the operator, the stage, the patch) */
        h = (h ^ ((fop_op + 1u) * 40503u + (fop_stg + 1u) * 7919u + fm6_pgen[song.sel % NTRK] * 2246822519u)) * 16777619u;
    if ((ui.frame & LV_MASK[lv.kind % 10u]) && lv.kind == live_kind(pg, t))
        return lv.sig | (h ^ LV_H0) ? lv.sig ^ h : 0u;
    lv.kind = live_kind(pg, t);
    if (lv.kind != LV_LFO && lv.kind != LV_CYCLE)
        lv.preview_id = 0;
    if (lv.kind != LV_LFO && lv.kind != LV_INS && lv.kind != LV_CYCLE)
        lv.lfo = 0;
    lv.sig = 0;
    if (lv.kind == LV_NONE)
        return h ^ LV_H0 ? h : 0u;
    if (lv.kind == LV_FMENV) {
        lv_fm_take();
        if (lv.n)
            lv.sig = (uint32_t)(uint16_t)lv.ex | (uint32_t)(uint16_t)lv.ey << 10;
    } else if (lv.kind == LV_ENV) {
        lv_env_take(t, pg);
        if (lv.n)
            lv.sig = (uint32_t)(uint16_t)lv.ex | (uint32_t)(uint16_t)lv.ey << 10 | (uint32_t)(lv.estg == 3u) << 24 | (uint32_t)(uint16_t)lv.erelx << 25;
    } else if (lv.kind == LV_CYCLE) {
        lv_cycle_take(t);
        if (lv.n)
            lv.sig = (uint32_t)(uint16_t)lv.lx | (uint32_t)(uint16_t)lv.ly << 10;
    } else if (lv.kind == LV_LFO) {
        lv_lfo_take(t, pg);
        lv.sig = (uint32_t)(uint16_t)lv.lx | (uint32_t)(uint16_t)lv.ly << 10;
    } else if (lv.kind == LV_FILT || lv.kind == LV_TWIN) {
        lv_filt_take(t);
        lv.sig = lv.fc ^ lv.fc2 * 2654435761u;
    } else if (lv.kind == LV_INS) {
        lv_ins_take(t);
        lv.sig = (uint32_t)(uint16_t)lv.lx | (uint32_t)(uint16_t)lv.ly << 10;
    } else if (lv.kind == LV_MOD) {
        lv_mod_take(t, pg);                          /* (the bars move in strips, live_strip: the page is not drawn again for them) */
    } else if (lv.kind == LV_CHORD) {
        for (i = 0; i < NVOICE; i++)
            lv.sig = (lv.sig ^ ((uint32_t)lv_voice_lvl(&t->v[i]) / 32u | (uint32_t)t->v[i].note << 8 | (uint32_t)(t->v[i].stage == 3u) << 16)) * 16777619u;
    }
    lv.sig = (lv.sig + 1u) * 2654435761u ^ lv.kind;   /* (a state with nothing to show: still not 0 on a live page, so that a lapse is a redraw) */
    return lv.sig ^ h;
}

/* ---- drawing, over the picture just drawn (canvas rows of graph_ht, cv_oy GOY) */
static void lv_label(const char *s)               /* the caption row (graph_cap), without a ground of its own: what is under it stays */
{
    int32_t o = cv_oy;
    cv_oy = 0;
    cv_text(grid_on ? UT_GR_NOTE_X : PANEL_X0 + 2, grid_on ? UT_TY(S, UT_GR_NOTE_BASE - UT_GR_Y) : 94 + o, &AF_S, s, T_MID);
    cv_oy = o;
}
static void lv_filt_draw(const track_t *t)
{
    int32_t md = clamp(t->p[P_FREEC], 0, UF_HP), res = clamp(t->p[P_FREEB], 0, 127);   /* (graph_filt's MULTI FILTER) */
    int32_t wb = md <= 64 ? md << 4 : ((UF_HP - md) * 65) >> 2, wl = md < 64 ? 1024 - wb : 0, wh = md > 64 ? 1024 - wb : 0;
    int32_t s24 = t->p[P_FREED] == 2, k1 = s24 ? UF_K24A : UF_K(res), k2 = UF_K24B(res), z = 30, px = 0, py = 0;
    uint32_t i, f = GF_F0;
    if (clamp(t->p[P_FREEA], 0, UF_STEPS) >= UF_STEPS && md <= 0)   /* (the bypass: graph_filt draws a flat line and no cutoff) */
        return;
    cv_rect(PANEL_X0 + gf_x(lv.fc << 8), 4, 1, 92, T_ACCENT);
    for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
        int32_t db = svf_db10(f, lv.fc, wl, wb, wh, k1, k1), x = PANEL_X0 + 4 * (int32_t)i, y;
        if (s24)
            db += svf_db10(f, lv.fc, wl, wb, wh, k2, k2);
        y = z - clamp(db, -420, 180) * 3 / 20;
        if (i)
            cv_line_t(px, py, x, y, T_ACCENT, 1);
        px = x;
        py = y;
    }
}
static void live_draw(const page_t *pg, const track_t *t)
{
    if (!lv.sig || lv.kind != live_kind(pg, t))
        return;
    cv_oy = GOY;
    if (lv.kind == LV_FMENV) {
        if (lv.n) {
            cv_rect(lv.ex, 23 - GOY - 2, 1, 46, T_ACCENT);
            cv_rrect(lv.ex - 3, lv.ey - 3, 7, 7, 3, T_TEXT, T_BG);
        }
    } else if (lv.kind == LV_ENV) {
        if (lv.n) {                                  /* the curve played so far lit, the playhead line, the dot */
            lv_eg_t g;
            lv_eg_view(t, env_of(pg), &g);
            lv_eg_curve(&g, lv.ex, T_ACCENT != T_THEME ? T_ACCENT : T_TEXT);   /* (a palette whose accent is the part colour: the text tone, so that the played part still reads) */
            cv_rect(lv.ex, g.top - 2, 1, g.bot - g.top + 4, T_ACCENT);
            cv_rrect(lv.ex - 3, lv.ey - 3, 7, 7, 3, T_TEXT, T_BG);
        }
    } else if (lv.kind == LV_CYCLE || lv.kind == LV_LFO || lv.kind == LV_INS) {
        if (lv.n) {
            int32_t x = lv.kind == LV_CYCLE ? lv.lx : PANEL_X0 + lv.lx;
            cv_rect(x, 6, 1, lv.kind == LV_CYCLE ? 88 : graph_ht - 12, T_ACCENT);
            cv_rrect(x - 3, lv.ly - 3, 7, 7, 3, T_TEXT, T_BG);
        }
        if (lv.preview_id)
            lv_label("PREVIEW");
    } else if (lv.kind == LV_FILT) {
        if (lv.fc)
            lv_filt_draw(t);
    } else if (lv.kind == LV_TWIN) {                 /* the ticks, a cap at the top for filter 1, at the bottom for filter 2 */
        if (lv.fc) {
            cv_rect(PANEL_X0 + gf_x(lv.fc << 8), 4, 1, 58, T_ACCENT);
            cv_rect(PANEL_X0 + gf_x(lv.fc << 8) - 2, 4, 5, 3, T_ACCENT);
        }
        if (lv.fc2) {
            cv_rect(PANEL_X0 + gf_x(lv.fc2 << 8), 4, 1, 58, T_ACCENT);
            cv_rect(PANEL_X0 + gf_x(lv.fc2 << 8) - 2, 59, 5, 3, T_ACCENT);
        }
    }
}

/* MOD: under each acting row, in the 2 px gap below it, a bar from the left (from the middle for a bipolar source) over a RAISE track, below the source's column
 * (x 34 .. 93). graph_mod_new draws them with the page (live_mod, per band: cv_oy = - the band's first screen row); while the page stands still they move alone, as a
 * 96 x 2 strip each (live_strip, from draw_graph's early return): the rows' text is not drawn again for a bar (a page redraw is 320 k basic blocks, a strip 100) */
static int32_t lv_mq[4];                             /* what the screen shows of each bar: its source in steps of 512 (Q15), 0xFFFF: no bar */
static int32_t lv_mq_of(int32_t v) { return v == INT32_MIN ? 0xFFFF : (v + 32768) >> 9; }
static void lv_bar(const track_t *t, uint32_t base, uint32_t k, int32_t y)
{
    int32_t v = lv.mv[k], w;
    uint32_t s = (uint32_t)t->p[MSLOT_ID(base + k)];
    lv_mq[k] = lv_mq_of(v);
    if (v == INT32_MIN)
        return;
    cv_rect(34, y, 60, 2, T_RAISE);
    if (MS_BIPOLAR(s) && MS_GAIN(t->p, s) == s) {
        w = v * 30 / 32767;
        cv_rect(w < 0 ? 64 + w : 64, y, w < 0 ? -w : w ? w : 1, 2, T_ACCENT);
    } else {
        w = clamp(v, 0, 32767) * 60 / 32767;
        if (w)
            cv_rect(34, y, w, 2, T_ACCENT);
    }
}
static void live_mod(const track_t *t, int32_t y0)
{
    uint32_t k, base = mod_page_base(cur_page());
    if (!lv.sig || lv.kind != LV_MOD)
        return;
    for (k = 0; k < 4u; k++) {
        int32_t y = MX_Y0 + (int32_t)k * MX_PITCH + MX_H;
        if (y >= y0 && y + 2 <= y0 + (int32_t)cv_h)
            lv_bar(t, base, k, y);
    }
}
static void live_strip(const page_t *pg, const track_t *t)
{
    uint32_t k, base = mod_page_base(pg);
    if (!lv.sig || lv.kind != LV_MOD || pg->graph != GR_MOD)
        return;
    for (k = 0; k < 4u; k++) {
        int32_t y = MX_Y0 + (int32_t)k * MX_PITCH + MX_H;
        if (lv_mq_of(lv.mv[k]) == lv_mq[k])
            continue;
        cv_begin(96, 2, T_BG);
        cv_oy = -y;
        lv_bar(t, base, k, y);
        cv_oy = 0;
        cv_blit(0, (uint32_t)y);
    }
}

/* CHORD: in the area's canvas (screen rows, cv_oy - the area's first row), over kb_draw's keys (chord_kb): in each key a voice sounds, a bar from
 * its bottom as high as the voice's envelope level; the keys are the two octaves from the C below the lowest sounding note (chord_live_keys),
 * or the last chord's (chord_keys) */
static void live_chord(const track_t *t)
{
    static const uint8_t WHITE_IX[12] = {0, 0, 1, 0, 2, 3, 0, 4, 0, 5, 0, 6}, IS_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t set[4], lo, root, i, j;
    int32_t pitch = UT_A_KB_W + KB_GAP, bh = UT_A_KB_H * KB_BLACK_PCT / 100, bw = (pitch / 14 - KB_GAP) * KB_BLACK_W_NUM / KB_BLACK_W_DEN;
    if (!lv.sig || lv.kind != LV_CHORD)
        return;
    if (chord_sounding(set)) {
        lo = 128u;
        for (i = 0; i < 128u && lo == 128u; i++)
            if ((set[i >> 5] >> (i & 31u)) & 1u)
                lo = i - i % 12u;
    } else {
        (void)chord_keys(t, &lo, &root);
    }
    for (j = 0; j < NVOICE; j++) {
        const voice_t *v = &t->v[j];
        int32_t lvl = lv_voice_lvl(v), d = (int32_t)v->note - (int32_t)lo, semi, h, x, kw;
        if (!lvl || d < 0 || d >= (int32_t)CHORD_KB_SPAN)
            continue;
        semi = d % 12;
        if (IS_BLACK[semi]) {                        /* a black key: its bar inside the key, 2 px narrower */
            int32_t wi = (semi < 2 ? 0 : semi < 4 ? 1 : semi < 7 ? 3 : semi < 9 ? 4 : 5) + 7 * (d / 12);
            x = UT_A_KB_X + (wi + 1) * pitch / 14 - KB_GAP / 2 - bw / 2 + 2;
            kw = bw - 4;
            h = 1 + lvl * (bh - 4) / 1000;
        } else {                                     /* a white key: the middle of its lower part, 4 px under the black keys */
            int32_t wi = WHITE_IX[semi] + 7 * (d / 12), x0 = UT_A_KB_X + wi * pitch / 14, x1 = UT_A_KB_X + (wi + 1) * pitch / 14 - KB_GAP;
            kw = 3;
            x = (x0 + x1 - kw) / 2;
            h = 1 + lvl * (UT_A_KB_H - bh - 8) / 1000;
        }
        if (kw >= 1)
            cv_rect(x, UT_A_KB_Y + (IS_BLACK[semi] ? bh : UT_A_KB_H) - 3 - h, kw, h, v->stage == 3u ? T_MID : T_INK);
    }
}
