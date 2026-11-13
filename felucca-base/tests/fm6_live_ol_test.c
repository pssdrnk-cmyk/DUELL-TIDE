/* SPDX-License-Identifier: GPL-3.0-only */
/* FM6 live operator level (fm6_core.c fm6_live_level): an OL the morph moves under a sounding note is a gain over the
 * note-on envelope. From JAM's jam_ready_test.c test_fm (branch jam, 71654ba), split out for the 1.1 port. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("fm6liveol: FAIL %s\n", s); bad++; } } while (0)
static void fm_compute(fm6_note_t *n, const uint8_t *p, int32_t *out)
{
    int32_t dt[6] = {0};
    fm6_lfot_t lt;
    memset(out, 0, 3 * FM6_N * sizeof *out);
    fm6_lfo_terms(&lt, p, 0, 0);
    fm6_note_compute(n, p, out, &lt, fm6_note_logfreq(60 * 16, 0), 31, 0, dt, 0, 0);
}
/* The pre-optimization renderer: even without an ARP it prepared temporary pitch/gate arrays for the bridge.
 * Keep this reference separate so the comparison covers the resulting audio and note state, not just array values. */
static void legacy_bridge_render(track_t *t, voice_t *v, int32_t *out, const vmod_t *m)
{
    fm6_note_t *n = fm6_note_of(t, v);
    uint32_t tr = (uint32_t)(t - trk), i;
    const uint8_t *p;
    int32_t bus[3 * FM6_N], lf, fb, lvl, k, a, da, dt2[6], ul[6];
    if (!n || !n->live)
        return;
    p = fm6_eff[tr].p;
    if (!v->gate && n->down)
        fm6_note_key(n, p, 0);
    lf = fm6_note_logfreq(m->pitch16 + (p[FP_TRNSP] - 24) * 16, m->fine);
    fb = clamp(fm6_eff[tr].fb + ((m->shape - (64 << 8)) >> 11), 0, 7);
    lvl = clamp(t->p[P_E2], -64, 63) * 24 + clamp(m->cutoff >> 8, -150, 150) * 16;
    if (m->amask || fm6_xt[tr].on) {
        if (!fm6_note_compute(n, p, bus, &fm6_lfot[tr], lf, fm6_eff[tr].alg, fb,
                              atg_fm6(m, fm6_eff[tr].dt, dt2, ul), lvl * 65536, ul))
            return;
    } else if (!fm6_note_compute(n, p, bus, &fm6_lfot[tr], lf, fm6_eff[tr].alg, fb,
                                 fm6_eff[tr].dt, lvl * 65536, 0)) {
        return;
    }
    k = fm6_eff[tr].ug;
    a = mulq15(m->amp0, fm6_eff[tr].ugp);
    da = (mulq15(m->amp1, k) - a) >> FM6_LG_N;
    for (i = 0; i < FM6_N; i++) {
        int32_t x = bus[i] - n->dc;
        n->dc += x >> 10;
        a += da;
        out[i] += (int32_t)(((int64_t)x * a) >> 26);
    }
}

static track_t *bridge_pair(uint32_t *carrier)
{
    uint8_t p[FP_SIZE + 1u];
    uint32_t alg = 0, k, side;
    track_t *t;
    morph_part_t *mp;
    ui_power_on(); frame();
    song.playing = song.rec = 0; transport_req = 0;
    gl.on = gl.armed = 0; morph_musical = 0;
    host_preset(&trk[0], ENGI_FM6, 0);
    t = &trk[0];
    t->p[P_VOICE] = V_POLY; t->p[P_AMODE] = AM_OFF;
    t->p[P_E0] = t->p[P_E1] = t->p[P_E3] = t->p[P_E4] = t->p[P_E5] = t->p[P_E6] = 0;
    memset(mxs, 0, sizeof mxs); memset(mxv, 0, sizeof mxv); memset(fm6_xt, 0, sizeof fm6_xt);
    while (alg < 31u && (fm6_carriers(alg) & (fm6_carriers(alg) - 1u)))
        alg++;
    *carrier = 0;
    while (!((fm6_carriers(alg) >> *carrier) & 1u))
        (*carrier)++;
    fm6_unpack(FM6_INIT, p);
    p[FP_ALG] = (uint8_t)alg; p[FP_FB] = 0; p[FP_OKS] = 1;
    p[FP_LPMD] = 25; p[FP_LPMS] = 3; p[FP_LAMD] = 35;
    for (k = 0; k < 6u; k++) {
        uint8_t *op = p + k * FP_OP;
        op[FP_MODE] = 0; op[FP_FC] = 1; op[FP_FF] = 0; op[FP_DET] = 7;
        op[FP_R1] = op[FP_R1 + 1] = op[FP_R1 + 2] = 99;
        op[FP_R1 + 3] = 20;
        op[FP_L1] = op[FP_L1 + 1] = op[FP_L1 + 2] = 99; op[FP_L1 + 3] = 0;
        op[FP_OL] = (uint8_t)(k == *carrier ? 99 : 0);
        op[FP_AMS] = (uint8_t)(k & 3u);
    }
    fm6_set_patch(0, p); fm6_env_look(0, FM6_LOOK_NEW);
    mp = morph_ready(0);
    for (side = 0; side < 2u; side++) {
        memcpy(morph_fmto[0][side], p, FP_SIZE);
        morph_fmto[0][side][*carrier * FP_OP + FP_FC] = (uint8_t)(1u + side);
        mp->tok[side][mp->top[side]] = (uint8_t)(side + 1u);
        morph_fmto_k[0][side] = (uint8_t)(side + 1u);
    }
    morph_build(mp); mp->fser++; mp->cvalid = 0;
    morph_xplan(0);
    CHECK("bridge comparison fixture plans a retune with a stand-in", (mxs[0].need & XN_FM) && ((mxs[0].rsub >> *carrier) & 1u));
    morph_do(MQ_FADER, 1u, 0u);
    trk_note_on(t, 60, 100); trk_note_on(t, 67, 80);
    return t;
}

typedef struct {
    unsigned calls, bridge, standin, audible_release, off, arp;
    int samples_equal, state_equal;
} bridge_result_t;

static void compare_bridge_block(track_t *t, uint32_t mask, unsigned block, bridge_result_t *r)
{
    uint32_t i;
    morph_begin(t, 1); fm6_block(t);
    for (i = 0; i < FM6_POLY; i++) {
        voice_t *v = &t->v[i];
        fm6_note_t *n = fm6_note_of(t, v), before, candidate;
        int32_t actual[FM6_N] = {0}, expected[FM6_N] = {0};
        vmod_t m;
        unsigned sounding = 0;
        if (!v->active || !n || !n->live)
            continue;
        memset(&m, 0, sizeof m);
        m.pitch16 = (int32_t)v->note * 16; m.fine = (block & 1u) ? 31 : -17;
        m.shape = 64 << 8; m.cutoff = 64 << 8;
        m.amp0 = 27000; m.amp1 = 30000;
        m.amask = mask; m.adelta = (block & 1u) ? 96 : -64;
        m.again = block % 3u ? 19000 : 0;
        before = *n;
        fm6_render(t, v, actual, FM6_N, &m); candidate = *n;
        *n = before;
        legacy_bridge_render(t, v, expected, &m);
        r->samples_equal &= !memcmp(actual, expected, sizeof actual);
        r->state_equal &= !memcmp(n, &candidate, sizeof candidate);
        *n = candidate;
        for (unsigned j = 0; j < FM6_N; j++)
            sounding |= actual[j] != 0;
        r->calls++;
        if (fm6_xt[0].on) {
            r->bridge++;
            r->standin += n->xs != 0;
            r->arp += mask != 0;
            r->audible_release += !v->gate && !n->down && sounding;
        } else {
            r->off++;
        }
    }
    morph_end(t);
}

static void bridge_render_equivalence(void)
{
    for (unsigned arp = 0; arp < 2u; arp++) {
        uint32_t carrier;
        track_t *t = bridge_pair(&carrier);
        uint32_t mask = arp ? 1u << carrier : 0u;
        bridge_result_t warm = {0};
        warm.samples_equal = warm.state_equal = 1;
        for (unsigned i = 0; i < 32u; i++)
            compare_bridge_block(t, mask, i, &warm);
        CHECK("renderer matches legacy before bridge entry", warm.samples_equal && warm.state_equal && warm.off > 0u);
        for (unsigned direction = 0; direction < 2u; direction++) {
            unsigned target = direction ? 0u : 127u, released = 0, arrived = 0;
            bridge_result_t r = {0};
            r.samples_equal = r.state_equal = 1;
            if (direction)
                trk_note_on(t, 60, 100);
            morph_do(MQ_FADER, 1u, target);
            for (unsigned i = 0; i < 2000u; i++) {
                if (!released && fm6_xt[0].on && i > 64u) {
                    trk_note_off(t, 60); released = 1;
                }
                compare_bridge_block(t, mask, i, &r);
                if (!fm6_xt[0].on && mxs[0].tp == (target ? 32768u : 0u)) {
                    arrived = 1;
                    break;
                }
            }
            for (unsigned i = 0; i < 8u; i++)
                compare_bridge_block(t, mask, i, &r);
            CHECK("both crossing directions enter the bridge and run a stand-in", r.bridge > 100u && r.standin > 0u);
            CHECK("both crossing directions arrive and render after bridge exit", arrived && r.off > 0u);
            CHECK("bridge comparison includes a released voice that still sounds", released && r.audible_release > 0u);
            CHECK("ARP and bridge run together when selected", !arp || r.arp > 100u);
            CHECK("bridge renderer matches legacy samples throughout the crossing", r.samples_equal && r.calls > 200u);
            CHECK("bridge renderer matches complete legacy FM6 note state", r.state_equal);
        }
    }
}
/* K7: the LFO terms are computed once a part and block (fm6_lfo_terms); they must be the numbers the per-voice formulas gave (the old fm6_note_compute's own lines, copied here) */
static void lfo_terms_old(const uint8_t *p, int32_t lfo_val, int32_t lfo_delay, int32_t *pm, uint32_t *am)
{
    uint32_t pmd = (uint32_t)((p[FP_LPMD] % 100u) * 165u >> 6) * (uint32_t)lfo_delay;
    int32_t senslfo = FM6_PMS[p[FP_LPMS] & 7u] * (lfo_val - (1 << 23));
    int32_t pmod = (int32_t)(((int64_t)pmd * senslfo) >> 39);
    uint32_t amd = (uint32_t)((p[FP_LAMD] % 100u) * 165u >> 6), amod;
    pmod = pmod < 0 ? -pmod : pmod;
    *pm = senslfo < 0 ? -pmod : pmod;
    amod = (uint32_t)(((int64_t)amd * lfo_delay) >> 8);
    *am = (uint32_t)(((int64_t)amod * ((1 << 24) - lfo_val)) >> 24);
}
static void k7_terms(void)
{
    static const int32_t vals[] = {0, 1, 1 << 20, (1 << 23) - 1, 1 << 23, (1 << 23) + 12345, 3 << 22, (1 << 24) - 1}, dels[] = {0, 1, 1 << 12, 1 << 20, (1 << 24) - 1, 1 << 24};
    uint32_t n = 0, bad_terms = 0, pi, ai, si, vi, di;
    uint8_t p[FP_SIZE];
    memcpy(p, fm6_patch[0], sizeof p);
    for (pi = 0; pi < 4; pi++)
        for (ai = 0; ai < 4; ai++)
            for (si = 0; si < 8; si++)
                for (vi = 0; vi < sizeof vals / sizeof vals[0]; vi++)
                    for (di = 0; di < sizeof dels / sizeof dels[0]; di++) {
                        fm6_lfot_t c;
                        int32_t pm;
                        uint32_t am;
                        p[FP_LPMD] = (uint8_t)(pi == 0 ? 0 : pi == 1 ? 7 : pi == 2 ? 50 : 99);
                        p[FP_LAMD] = (uint8_t)(ai == 0 ? 0 : ai == 1 ? 9 : ai == 2 ? 60 : 99);
                        p[FP_LPMS] = (uint8_t)si;
                        fm6_lfo_terms(&c, p, vals[vi], dels[di]);
                        lfo_terms_old(p, vals[vi], dels[di], &pm, &am);
                        n++;
                        bad_terms += c.pm != pm || c.am != am;
                    }
    printf("fm6liveol: K7 LFO terms: %u combinations, %u differ\n", n, bad_terms);
    CHECK("K7: the part's LFO terms equal the per-voice formulas they replace", n > 1000 && bad_terms == 0);
}
int main(void)
{
    static const uint32_t notes[] = {24, 60, 108}, velocities[] = {1, 32, 127};
    _Static_assert(sizeof(fm6_env_t) == 24u, "the key and velocity reuse the envelope's padding");
    ui_power_on();
    frame();
    k7_terms();
    for (uint32_t a = 0; a < 3; a++)
        for (uint32_t b = 0; b < 3; b++) {
            uint8_t p[FP_SIZE];
            fm6_note_t normal, walked;
            int32_t x[3 * FM6_N], y[3 * FM6_N];
            int restored = 1, intact = 1;
            memcpy(p, fm6_patch[0], sizeof p);
            p[FP_ALG] = 31;
            for (uint32_t k = 0; k < 6; k++) {
                uint8_t *o = p + k * FP_OP;
                o[FP_R1] = o[FP_R1 + 1] = o[FP_R1 + 2] = 99;
                o[FP_L1] = o[FP_L1 + 1] = o[FP_L1 + 2] = 99;
                o[FP_L1 + 3] = 0;
                o[FP_OL] = 99;
                o[FP_RS] = 0;
                o[FP_KVS] = 7;
            }
            fm6_note_init(&normal, p, notes[a], velocities[b], 1);
            walked = normal;
            for (uint32_t i = 0; i < 100; i++) {         /* operator 1's OL toggles 99 / 0 every five blocks */
                p[FP_OL] = 99;
                fm_compute(&normal, p, x);
                p[FP_OL] = (i % 10 < 5) ? 0 : 99;
                fm_compute(&walked, p, y);
                intact &= !memcmp(normal.env, walked.env, sizeof normal.env);
                if (i % 10 >= 7)
                    restored &= !memcmp(x, y, FM6_N * sizeof *x);
            }
            CHECK("live OL does not mutate EG attenuation", intact);
            CHECK("OL returns to the same audio after repeated mute/unmute", restored);
            p[FP_OL] = 0;
            fm6_note_init(&walked, p, notes[a], velocities[b], 1);
            for (uint32_t i = 0; i < 100; i++)
                fm_compute(&walked, p, y);
            p[FP_OL] = 99;
            for (uint32_t i = 0; i < 4; i++)
                fm_compute(&walked, p, y);
            CHECK("initially silent operator keeps keyboard/velocity scaling", walked.op[0].level_in == normal.op[0].level_in);
            for (uint32_t j = 0; j < 4; j++)
                p[FP_L1 + j] = 0;
            p[FP_OL] = 0;
            fm6_note_init(&walked, p, notes[a], velocities[b], 1);
            p[FP_OL] = 99;
            for (uint32_t i = 0; i < 20; i++)
                fm_compute(&walked, p, y);
            CHECK("an all-zero EG stays inaudible when OL rises", walked.op[0].gain_out < FM6_THRESH);
        }
    bridge_render_equivalence();
    printf("fm6liveol: %d checks, %d failed\n", checks, bad);
    return bad != 0;
}
