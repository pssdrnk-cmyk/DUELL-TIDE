/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE SMOOTH TRANSITIONS: how a part crosses the middle of its morph when the two ends differ in what cannot blend (the
 * operator structure of FM6, a wave, a filter type, a model, a sub's wave: MK_STEP values). Before this, every one of them
 * flipped inside one 32-sample block at the fader's middle while the voices sounded: a discontinuity (a click) and a jump.
 * Measured over every pair of factory sounds JOURNEY can pick (636 pairs, docs/design/TIDE_ROADMAP.md): nearly all switch
 * something; 36 of the 80 FM6 pairs change the algorithm.
 *
 * The idea: a structural switch is made only while what it changes cannot be heard, and the sound around it moves smoothly
 * into and out of that state, as one arc over the whole travel (XW0 .. XW1, 5 .. 95 %: a glide spends its time on the change,
 * not on a slice of it). Everything runs on one transition place per part, tp (the B weight, Q15), which follows the fader no
 * faster than XSTEP a block (the arc in 1.1 s: a hand or a MIDI jump still gets a whole transition); with no voice sounding tp is
 * the fader itself. The switched values sound from tp's side (morph.c morph_apply), which changes on a new note inside the
 * closed stretch (XS0 .. XS1, 42 .. 58 %; a fresh attack carries it), at the stretch's edge without one.
 *
 *   FM6, the harmonic valley (the part's operator structure differs). In phase modulation the carrier's amplitude does not
 *     depend on the modulation index (the energy moves between the carrier and its sidebands: Chowning 1973), so fading the
 *     modulators keeps the loudness and narrows the spectrum, as a closing filter would, without a filter's colour.
 *       XW0 .. XD1   the A structure; its modulators fade to silence (a raised cosine on their linear amplitude, the index)
 *       XD1 .. XD2   the bridge: algorithm 32 (every operator a carrier, no feedback); the modulators silent, so each sounding
 *                    operator is a pure sine; A's carriers fade out and B's in, equal-power (sin / cos: uncorrelated partials;
 *                    Fink, Holters, Zoelzer 2016); a carrier both ends share at the same frequency stays at full level, one
 *                    that only detunes glides; a leaving and an arriving carrier at one frequency cross-fade linearly, in phase (correlated);
 *                    a shared carrier at another ratio (measured: never under a semitone, 35 of 72 by octaves) hands over
 *                    to a stand-in, an operator silent in the bridge made a copy of it (its envelope where it is) at the B
 *                    frequency: the two cross-fade equal-power, then the carrier, silent, takes the copy's state, phase
 *                    included, in one block (the same sine: their gains sum to a constant). Without a free operator it
 *                    fades out, is retuned, fades in (a dip)
 *       XD2 .. XW1   the B structure; its modulators fade in
 *     Each operator is retuned (eng_fm6.c fm6_xtune) only while it is silent: msfa caches an operator's frequency at the note-on;
 *     and at the phase a fresh note would give it against the others (fm6_xphase), which decides whether operators of one
 *     frequency, a detuned pair or a sideband folded over 0 Hz add or cancel (measured: holes of 10 .. 25 dB without it).
 *   Every engine, the filter dip (a voice value that cannot blend differs): subtractive, the part's own multimode filter (every
 *     engine has it, voice.c uf_run): its effective cutoff (the dial, the envelope's and LFO's routes, the key) goes down in log
 *     frequency toward 1.5 times the highest sounding note (a chord keeps its notes, loses its overtones) over 5 .. 40 %, stays
 *     closed to 60 %, opens to 95 % (a raised cosine), RES held to half while closed (a switch through a resonant filter rings).
 *     A step after the filter cannot be hidden in it: where a value of the filters themselves switches (SLOPE, MODE, filter 2's
 *     TYPE), or MODE is not a low-pass, the valley's own filter after the voices does it (morph_valley: two one-poles, the dry
 *     share faded in, RMS make-up up to +9 dB, the sweep held where it would take more than 6 dB, 1/24 octave a block at most).
 *   The swell: the delay and reverb sends rise with a transition's dip (3/5) and with GLIDE's movement (half), up to 3/4 of the
 *     way to full, so the tails carry the old sound while the new one comes up.
 *   GLIDE's movement (morph_glide.c glide_motion): every trip the filter breathes down and back (morph_xcut) and the sends swell.
 *   The wash (morph_wash): band-limited noise in the sound's register fills what a movement takes while a note is held.
 *   The INSERT, the insert dip (its type or a control differs): its wet mix eases to dry over 25 .. 40 %, back over 60 .. 75 %, the type
 *     and controls switch while it is dry (its own fade, fx.c track_insert, is 2.9 ms, and a control jumped on the old effect
 *     while it still sounded), then the new effect's wet eases in; the dry sound goes on.
 *   Values that only shape the next notes (the arp, voice and glide mode, the chord) switch at the middle as before.
 *
 * Contexts: morph_xplan (the main loop, ~25 times a second, never while a transition runs) works out what differs and, for FM6,
 * each operator's part and the ends' patches; morph_xstep (the audio ISR, morph.c morph_begin, a block) moves tp and sets the
 * levels; morph_xpost (after the blend) writes what the arc moves; morph_xcut (per voice) dips the cutoff; morph_valley (the
 * audio ISR, fx.c part_voice) filters where the part's own filter cannot. Nothing here runs while the ends do not differ: the sound is
 * then bit for bit what it was. Included by morph.c */

/* The arc spans the whole travel, so a glide of any length spends its time on the change, not on a slice of it (a 1-bar glide:
 * the bridge ~0.6 s, the filter closed ~0.4 s; 8 bars: ~5 s and ~3 s). The places, as the B weight: */
#define XW0 1638u                                        /* the arc: from 5 % .. */
#define XW1 31130u                                       /* .. to 95 % (each end itself, untouched) */
#define XD1 11469u                                       /* FM6: the bridge from 35 % (A's modulators silent) .. */
#define XD2 21299u                                       /* .. to 65 % (B's come in) */
#define XC 16384u                                        /* the middle */
#define XLP0 13107u                                      /* the filter closed from 40 % .. */
#define XLP1 19661u                                      /* .. to 60 % */
#define XS0 13763u                                       /* the switched values change side from 42 % .. */
#define XS1 19005u                                       /* .. to 58 %, on a note (morph_xpick): inside the dips' floor, a margin either side */
#define XI0 8192u                                        /* the insert dry from 25 % (dry through the whole closed stretch) .. */
#define XI1 24576u                                       /* .. to 75 % */
#define XZ 160u                                          /* a silent operator's plateau around its switch (~0.5 %: a few blocks at XSTEP) */
#define XSTEP ((uint32_t)((XW1 - XW0) * CTL + FS * 11u / 10u - 1u) / (FS * 11u / 10u))   /* the arc in no less than 1.1 s (a jump of the fader) */
#define XTP_MAX (XZ / 3u)                                /* tp's largest step a block in the arc, whatever drives it: a silent operator's plateau (XZ) is
                                                          * then three blocks at least, and a retune needs two in a row (eng_fm6.c fm6_xtune: mute = now & before).
                                                          * GLIDE's fader moves in steps of 258 (1 / 127) as often as the main loop gets round to it: a step
                                                          * could skip a plateau, the operator kept its old frequency (the audit's follow-up: two octaves). The
                                                          * arc at this pace is ~0.4 s, inside the fastest trip (a bar at 240 BPM, 1 s) */
#define XN_LP 1u                                         /* need: a voice value switches (the spectral valley) */
#define XN_FM 2u                                         /* need: the FM6 operator structure differs (the harmonic valley) */
#define XN_INS 4u                                        /* need: the INSERT's type or a control differs (the insert dip) */
#define XN_FILT 8u                                       /* need: a value of the filters themselves switches (SLOPE, filter 2's TYPE): the valley's filter,
                                                          * after the voices, hides it; the part's own would switch inside itself */
enum { XR_OTHER, XR_KEPT, XR_GLIDE, XR_LEAVE, XR_ARRIVE, XR_RETUNE };   /* an operator's part in the bridge */

typedef struct {
    uint16_t tp;                                         /* the transition's place (the B weight it sounds at), Q15 */
    uint8_t side;                                        /* the switched values sounding: 0 A's, 1 B's */
    uint8_t need;                                        /* XN_*: from the plan */
    uint8_t planned;                                     /* the plan is made (for the ends as they were then) */
    uint8_t role[6];                                     /* FM6: each operator's XR_* */
    uint8_t alg[2], fb[2];                               /* .. the ends' algorithms and feedback */
    uint8_t car[2];                                      /* .. the operators that stay carriers through the valley (feedback carriers fade as modulators) */
    uint8_t glide;                                       /* .. the operators that only detune */
    uint8_t rsub;                                        /* .. the retuned carriers with a stand-in */
    uint8_t coh;                                         /* .. leaving and arriving carriers paired at one frequency: in phase, a linear cross-fade */
    uint8_t sfor[6];                                     /* .. each stand-in's carrier + 1 (0: not a stand-in) */
    uint8_t mprev;                                       /* .. the operators silent the block before */
    uint16_t lfloor;                                     /* the spectral valley: its floor, log2 Hz Q12 (morph_xstep: the highest sounding note: every note keeps its fundamental) */
    uint16_t d;                                          /* .. its depth now, Q15 (0: off) */
    uint16_t dins;                                       /* the insert dip's depth now, Q15 (0: off) */
    uint16_t df;                                         /* the part's own filter's dip now, Q15 (0: off; the cutoff, RES held down, DUB THROW) */
    uint16_t dub;                                        /* DUB THROW's depth now, Q15 */
    uint8_t filt;                                        /* the dip is the part's multimode filter (its MODE low-pass), else the valley's own */
    uint8_t lp;                                          /* the part's multimode filter is a low-pass (MODE): GLIDE's breath may close it */
    uint8_t vf;                                          /* .. and runs already, as the NOTES budget counts it (CUT not open, at either end): the voices' own
                                                          * filter may move; else the valley after the voices does it (switching a bypassed filter on in every voice
                                                          * costs 1.8 % a voice the budget never planned: twelve FM6 voices went past the core on the unit) */
    uint16_t bv;                                         /* GLIDE's breath in the valley (the voices' filter bypassed), Q15 */
    uint8_t snd;                                         /* a note is held (the wash plays only then) */
    uint8_t sounding;                                    /* actual audio state; a silent part's queries use the requested side */
    uint16_t motion;                                     /* GLIDE's movement now, Q15 (main loop, morph_glide.c glide_motion): the breath, the swell */
    uint32_t age;                                        /* the newest voice's age (voice.c vage) the block before: a new note */
    uint16_t plan_ms;                                    /* (morph_xpoll: the last plan, fm1_ms) */
    uint32_t esig;                                       /* (.. the ends it was made for: morph_xesig) */
    uint8_t follow;                                      /* GLIDE moves this part's sound (morph_glide.c glide_motion): tp is the fader itself */
} mxs_t;
static mxs_t mxs[NSHOWN];

/* ---------------------------------------------------------------- the curves (Q15) --- */
static const uint16_t XCOS[17] = {32767, 32610, 32138, 31357, 30274, 28898, 27246, 25330, 23170, 20788, 18205, 15447, 12540,
                                  9512, 6393, 3212, 0};   /* cos(pi / 2 x), x = 0, 1/16 .. 1 */
static int32_t xcos(int32_t x)                           /* cos(pi / 2 x), x Q15 in 0 .. 32768 */
{
    uint32_t u = (uint32_t)clamp(x, 0, 32768), i = u >> 11, f = u & 2047u;
    return i >= 16u ? 0 : XCOS[i] + (((int32_t)XCOS[i + 1u] - XCOS[i]) * (int32_t)f >> 11);
}
static int32_t xrise(int32_t x) { int32_t c = xcos(x); return 32768 - (c * c >> 15); }   /* a raised cosine, 0 -> 1: sin^2(pi / 2 x) */
static int32_t xpos(uint32_t tp, uint32_t a, uint32_t b) { return tp <= a ? 0 : tp >= b ? 32768 : (int32_t)(((tp - a) << 15) / (b - a)); }
/* log2 of a linear gain g (Q15), Q24 (g 0 .. 32768; g < 4: -14 octaves) */
static int32_t xlog2(int32_t g)
{
    int32_t e = 0, f;
    uint32_t x = (uint32_t)g;
    if (g >= 32768)
        return 0;
    if (g < 4)
        return -(14 << 24);
    while (x < 16384u)
        x <<= 1, e++;
    f = (int32_t)(x - 16384u) << 1;                      /* (x / 16384 - 1), Q15 */
    return ((-e - 1) * (1 << 24)) + (int32_t)(((int64_t)f * (44128 - ((11358 * f) >> 15))) >> 6);   /* log2(1 + f) ~ f (1.3466 - 0.3466 f) */
}

/* ---------------------------------------------------------------- the plan (main loop) --- */
static int morph_xnote(uint32_t id)                      /* a value that only shapes the next notes: it switches at the middle as ever */
{
    return id == P_AMODE || id == P_ARATE || id == P_AOCT || id == P_APROB || id == P_AORDER || id == P_AHOLD || id == P_AGATE ||
           id == P_VOICE || id == P_GLMODE || id == P_CHRD || id == P_VOIC || id == P_LTRIG || id == P_DKSRC ||
           (id >= P_ITYPE && id <= P_IMIX);              /* (the INSERT: its own dip, XN_INS) */
}
static uint32_t morph_fbop(uint32_t alg)                 /* the operator with feedback in an algorithm (6: none) */
{
    uint32_t k;
    for (k = 0; k < 6u; k++)
        if ((FM6_ALG[alg & 31u][k] & 0xc0u) == 0xc0u)
            return k;
    return 6u;
}
static void morph_fm6_at(const morph_part_t *mp, uint32_t tr, uint8_t *p, int32_t w, uint32_t side);
/* part pi's plan: what differs between the ends (as morph_apply gives them at 0 and 32768) and, for FM6, each operator's part.
 * Written under the lock (the ISR reads it); fm6_xt's patches only while no transition runs */
static void morph_xplan(uint32_t pi)
{
    int16_t a[EXT_P_LEN], b[EXT_P_LEN];
    uint8_t pa[FP_SIZE + 1u], pb[FP_SIZE + 1u];
    morph_part_t *mp = &mph[pi];
    const track_t *t = &trk[pi];
    mxs_t *x = &mxs[pi];
    uint32_t j, need = 0, k, fm = morph_isfm(mp) && pi < NLIVE;
    uint8_t role[6] = {0}, alg[2] = {0}, fb[2] = {0}, car[2] = {0}, glide = 0, rsub = 0, coh = 0, sfor[6] = {0};
    mxf_t xf;
    if ((!mp->nid && !mp->fnid) || morph_stale(t, mp)) {
        MORPH_LOCK();
        x->need = 0;
        x->planned = 1;
        MORPH_UNLOCK();
        return;
    }
    memcpy(a, t->p, sizeof a);
    memcpy(b, t->p, sizeof b);
    memset(&xf, 0, sizeof xf);
    morph_apply(mp, a, 0, 0, &xf);
    memset(&xf, 0, sizeof xf);
    morph_apply(mp, b, 32768, 0, &xf);
    for (j = 0; j < mp->nid; j++) {
        uint32_t id = mp->ent[j].id;
        if ((mp->ent[j].kind & 0x3Fu) != MK_STEP || a[id] == b[id])
            continue;
        if (id >= P_ITYPE && id <= P_IC)
            need |= XN_INS;
        else if (!morph_xnote(id) && !(fm && (id == P_E0 || id == P_E3)))
            need |= XN_LP | (id == P_FREED || id == P_FREEC || id == P_TWTYPE ? XN_FILT : 0u);
    }
    if (fm) {
        int32_t dt[6];
        uint32_t fop[2];
        memcpy(pa, fm6_patch[pi], sizeof pa);
        memcpy(pb, fm6_patch[pi], sizeof pb);
        morph_fm6_at(mp, pi, pa, 0, 0u);
        morph_fm6_at(mp, pi, pb, 32768, 1u);
        fm6_cross_macros(pi, pa, a, &alg[0], &fb[0], dt);
        fm6_cross_macros(pi, pb, b, &alg[1], &fb[1], dt);
        for (j = 0; j < 2u; j++) {
            fop[j] = fb[j] ? morph_fbop(alg[j]) : 6u;
            car[j] = (uint8_t)(fm6_carriers(alg[j]) & ~(fop[j] < 6u ? 1u << fop[j] : 0u));   /* (a carrier with feedback fades as a modulator) */
        }
        if (alg[0] != alg[1] || fb[0] != fb[1])
            need |= XN_FM;
        for (k = 0; k < 6u; k++) {
            const uint8_t *oa = pa + k * FP_OP, *ob = pb + k * FP_OP;
            int same = oa[FP_MODE] == ob[FP_MODE] && oa[FP_FC] == ob[FP_FC] && oa[FP_FF] == ob[FP_FF];
            uint32_t ca = (car[0] >> k) & 1u, cb = (car[1] >> k) & 1u;
            if (!same || oa[FP_DET] != ob[FP_DET])
                need |= XN_FM;
            if (ca && cb)
                role[k] = !same ? XR_RETUNE : oa[FP_DET] != ob[FP_DET] ? XR_GLIDE : XR_KEPT;
            else
                role[k] = ca ? XR_LEAVE : cb ? XR_ARRIVE : XR_OTHER;
            if (role[k] == XR_GLIDE)
                glide |= (uint8_t)(1u << k);
        }
        for (k = 0; k < 6u; k++)                         /* an arriving carrier at a leaving one's frequency: the two in phase (eng_fm6.c fm6_xphase) */
            if (role[k] == XR_ARRIVE)
                for (j = 0; j < 6u; j++) {
                    const uint8_t *oj = pa + j * FP_OP, *ok = pb + k * FP_OP;
                    if (role[j] == XR_LEAVE && !((coh >> j) & 1u) && oj[FP_MODE] == ok[FP_MODE] && oj[FP_FC] == ok[FP_FC] && oj[FP_FF] == ok[FP_FF]) {
                        coh |= (uint8_t)(1u << j | 1u << k);
                        break;
                    }
                }
        for (k = 0; k < 6u; k++)                         /* a stand-in for each retuned carrier: an operator silent through the bridge */
            if (role[k] == XR_RETUNE)
                for (j = 0; j < 6u; j++)
                    if (role[j] == XR_OTHER && !sfor[j]) {
                        sfor[j] = (uint8_t)(k + 1u);
                        rsub |= (uint8_t)(1u << k);
                        break;
                    }
    }
    MORPH_LOCK();
    x->need = (uint8_t)need;
    memcpy(x->role, role, sizeof role);
    memcpy(x->alg, alg, sizeof alg);
    memcpy(x->fb, fb, sizeof fb);
    memcpy(x->car, car, sizeof car);
    x->glide = glide;
    x->rsub = rsub;
    x->coh = coh;
    memcpy(x->sfor, sfor, sizeof sfor);
    if (fm && !fm6_xt[pi].on) {
        fm6_xpitch_pack(&fm6_xt[pi].pa, pa);
        fm6_xpitch_pack(&fm6_xt[pi].pb, pb);
    }
    x->planned = 1;
    MORPH_UNLOCK();
}
/* the main loop, every pass: each part's plan again every 40 ms while its tp is outside the window (the ends of a JOURNEY are
 * replaced at the fader's ends, so the plan is new long before the next crossing) */
/* the ends part pi's plan is for: which entry each end is (history, MORPH TO's sound, roll seed and settings), the sound */
static uint32_t morph_xesig(uint32_t pi)
{
    const morph_part_t *mp = &mph[pi % NSHOWN];
    uint32_t e, h = 2166136261u ^ mp->fser ^ (uint32_t)mp->eng << 8 ^ (uint32_t)mp->nid << 16;
    for (e = 0; e < 2u; e++) {
        uint32_t t = mp->top[e];
        h = (h ^ (t | (uint32_t)mp->n[e] << 8 | (uint32_t)mp->tok[e][t] << 16)) * 16777619u;
        h = (h ^ (mp->seed[e][t] | (uint32_t)mp->rcfg[e][t] << 16 | (uint32_t)mp->ramt[e][t] << 24)) * 16777619u;
    }
    return h;
}
static void morph_xpoll(void)
{
    uint32_t pi;
    for (pi = 0; pi < NSHOWN; pi++) {
        mxs_t *x = &mxs[pi];
        uint32_t sig = morph_xesig(pi);
        if (x->planned && sig == x->esig && (uint16_t)(fm1_ms - x->plan_ms) < 40u)
            continue;                                    /* (new ends: planned again at once; JRNY at 240 BPM, a bar a trip, has no 40 ms to spare) */
        if (x->planned && x->tp > XW0 && x->tp < XW1)
            continue;                                    /* (a transition runs on the plan it began with) */
        x->plan_ms = (uint16_t)fm1_ms;
        x->esig = sig;
        morph_xplan(pi);
    }
}

/* ---------------------------------------------------------------- a block (audio ISR) --- */
/* operator k's level (linear Q15) at tp, and when it plays B's frequency */
static int32_t morph_xgain(const mxs_t *x, uint32_t k, uint32_t tp)
{
    uint32_t r = x->role[k];
    if (tp < XD1)                                        /* the A structure: its modulators fade out */
        return (x->car[0] >> k) & 1u ? 32768 : 32768 - xrise(xpos(tp, XW0, XD1 - XZ));
    if (tp >= XD2)                                       /* the B structure: its modulators fade in */
        return (x->car[1] >> k) & 1u ? 32768 : xrise(xpos(tp, XD2 + XZ, XW1));
    if (r == XR_KEPT || r == XR_GLIDE)                   /* the bridge */
        return 32768;
    if ((x->rsub >> k) & 1u)                             /* a retuned carrier with a stand-in: out (A), then the stand-in's state at once (B) */
        return tp < XD2 - XZ ? xcos(xpos(tp, XD1 + XZ, XD2 - 2u * XZ)) : 32768;
    if (x->sfor[k])                                      /* .. its stand-in (B): in, equal-power, then silent */
        return tp < XD2 - XZ ? xcos(32768 - xpos(tp, XD1 + XZ, XD2 - 2u * XZ)) : 0;
    if ((x->coh >> k) & 1u) {                            /* (one frequency, in phase: correlated, so the amplitudes add: linear) */
        int32_t p = xrise(xpos(tp, XD1 + XZ, XD2 - XZ));
        return r == XR_LEAVE ? 32768 - p : p;
    }
    if (r == XR_LEAVE)                                   /* (other frequencies: uncorrelated, so the powers add: equal-power) */
        return xcos(xpos(tp, XD1 + XZ, XD2 - XZ));
    if (r == XR_ARRIVE)
        return xcos(32768 - xpos(tp, XD1 + XZ, XD2 - XZ));
    if (r == XR_RETUNE)                                  /* out, retuned at the middle, in */
        return tp < XC ? xcos(xpos(tp, XD1 + XZ, XC - XZ)) : xcos(32768 - xpos(tp, XC + XZ, XD2 - XZ));
    return 0;
}
static uint32_t morph_xtunes(const mxs_t *x, uint32_t k, uint32_t tp)
{
    uint32_t r = x->role[k];
    return tp >= ((x->rsub >> k) & 1u ? XD2 - XZ : r == XR_ARRIVE ? XD1 : r == XR_LEAVE ? XD2 : XC);
}

/* part pi's arc at rest (morph.c morph_begin: no morph on the part, or another sound's): what the valley, the wash and the voices'
 * dip read is cleared, so nothing of a transition or a glide stays on a sound that does not move (the audio ISR) */
static uint16_t morph_cdd[NSHOWN];                      /* the filter dip: its depth (Q15); the voices' effective cutoff, routes and key included,
                                                          * goes that share of the way down to the floor (morph_xcut) */
static __attribute__((noinline)) void morph_xrest(uint32_t pi)
{
    mxs_t *x = &mxs[pi % NSHOWN];
    x->d = x->df = x->dub = x->dins = x->bv = 0;
    x->need = 0;
    x->snd = 0;
    x->sounding = 0;
    morph_cdd[pi % NSHOWN] = 0;
    if (pi < NLIVE && fm6_xt[pi].on) {
        fm6_xt[pi].on = 0;
        fm6_eff[pi].ok = 0;
    }
}
/* A new Original owns a new plan. Drain stand-ins before another plan may
 * reuse their mapping; the caller holds MORPH_LOCK, outside the audio ISR. */
static void morph_xreset(uint32_t pi)
{
    morph_xrest(pi);
    mxs[pi % NSHOWN].planned = 0;
    mxs[pi % NSHOWN].tp = 0;
    mxs[pi % NSHOWN].side = 0;
    if (pi < NLIVE) {
        fm6_xt_t *x = &fm6_xt[pi];
        x->tune = x->glide = 0;
        x->mute = 63u;
        if (trk[pi].engine == ENGI_FM6) {
            fm6_xblock(&trk[pi], pi);  /* release borrowed slots */
            fm6_xblock(&trk[pi], pi);  /* restore their saved states */
        }
        x->mute = x->dirty = 0;
        fm6_eff[pi].ok = 0;
    }
}
/* a dip's depth at tp (Q15): down over a .. XLP0, its floor to XLP1, up to b (a raised cosine: no corner in the move) */
static int32_t morph_xarc(uint32_t tp, uint32_t a, uint32_t b)
{
    return tp <= a || tp >= b ? 0 : tp < XLP0 ? xrise(xpos(tp, a, XLP0)) : tp <= XLP1 ? 32768 : 32768 - xrise(xpos(tp, XLP1, b));
}
static int32_t morph_xdepth(uint32_t tp) { return morph_xarc(tp, XW0, XW1); }
#define XBR_Q8 4634                                      /* GLIDE's breath at its deepest: two octaves of the dial (2317 Q8 steps an octave) */
/* WASH (TIDE LIST; tide_ui.c keeps it in the settings): 0 OFF (the default since 2026-11-01, owner), 1 LOW, 2 MID, 3 HIGH. It scales GLIDE's movement
 * (the breath, the swell: XWL, x 0 .. 1.5), a transition's swell, and the wash's fill (15 dB under the part's level at rest at MID,
 * 3 dB a step: XWS). OFF: plain glides, a transition still smoothed (its filter dip, the INSERT's, FM6's valley) */
static uint8_t morph_wlvl = 0u;
static const uint16_t XWL[4] = {0u, 16384u, 32768u, 49152u};
static const uint8_t XWS[4] = {0u, 6u, 5u, 4u};          /* the fill's level: the level at rest >> XWS (18, 15, 12 dB under it; owner, 2026-10-29: 6 dB less, "too much") */
/* the audio ISR, per voice and block (voice.c track_render, voice_chain.c): the effective cutoff uc (Q8 dial) of part t through
 * GLIDE's breath (down by up to two octaves with the movement, never under the floor: 1.5 times the highest note) and the
 * transition's dip (that share of the way to the floor). The floor in the dial: step 0 is 30 Hz (log2 4.907, Q12 20098), a step
 * 0.11048 octave. In the flash: its callers run in RAM, whose room is the pool's */
static __attribute__((noinline)) int32_t morph_xcut(const track_t *t, int32_t uc)
{
    uint32_t pi = t->mwpi % NSHOWN;
    const mxs_t *x = &mxs[pi];
    int32_t fl;
    if (!t->mwon || (!morph_cdd[pi] && !(x->motion && x->lp && x->vf)))
        return uc;
    fl = (((int32_t)x->lfloor - 20098) * 2317) >> 12;
    if (x->motion && x->lp && x->vf && uc > fl) {
        int32_t d = uc - (int32_t)(((int32_t)x->motion * XBR_Q8) >> 15);
        uc = d > fl ? d : fl;
    }
    return morph_cdd[pi] && uc > fl ? uc - (int32_t)(((int64_t)(uc - fl) * morph_cdd[pi]) >> 15) : uc;
}
/* the switched values' side: a new note in the closed stretch takes them over toward where tp goes (a fresh attack carries the
 * change, as a player would make it); with no note they change at the stretch's edge; with nothing sounding at the middle */
static uint32_t morph_xpick(const mxs_t *x, uint32_t tp, uint32_t w, uint32_t sound, uint32_t note)
{
    if (!sound)
        return tp >= XC;
    if (tp >= XS1 || tp <= XS0)
        return tp >= XS1;
    if (note)
        return w > tp ? 1u : w < tp ? 0u : tp >= XC;
    return x->side;
}
/* before the blend (morph.c morph_begin): the values the arc writes into t->p after it (morph_xpost) join the blend's list as
 * values it wrote, so morph_end puts them back; m: the list's length, returned grown. In the flash: morph_begin is in range Z,
 * whose room is the pool's */
static __attribute__((noinline)) uint32_t morph_xreg(uint32_t pi, const int16_t *p, uint8_t *tid, uint8_t *chk, int16_t *tlive, uint32_t m)
{
    const mxs_t *x = &mxs[pi];
    uint8_t ids[4];
    uint32_t n = 0, k;
    if (x->dins)
        ids[n++] = P_IMIX;
    if (x->df && x->filt)
        ids[n++] = P_FREEB;
    if (x->dub) {
        ids[n++] = P_DLY;
        ids[n++] = P_REV;
    }
    for (k = 0; k < n; k++) {
        uint32_t j;
        for (j = 0; j < m && tid[j] != ids[k]; j++)
            ;
        if (j < m) {                                     /* (the blend moves it already: one entry, always kept (chk). Two made the block's
                                                          * cache check fail on the second, its live value already written: the blend again
                                                          * every block; independent audit 2026-10-29) */
            chk[j] = 1;
            continue;
        }
        if (m >= MV_N)
            break;
        tid[m] = ids[k];
        chk[m] = 1;
        tlive[m++] = p[ids[k]];
    }
    return m;
}
static __attribute__((noinline)) void morph_xpost(track_t *t, uint32_t pi)
{
    const mxs_t *x = &mxs[pi];
    int16_t *p = t->p;
    int32_t df = x->df;
    if (x->dins)                                         /* the insert's wet eases to dry */
        p[P_IMIX] = (int16_t)((p[P_IMIX] * (32768 - (int32_t)x->dins)) >> 15);
    morph_cdd[pi] = 0;
    if (df && x->filt) {
        /* the cutoff down toward 1.5 times the highest note: the effective one (morph_xcut): a filter envelope that opens a dark
         * patch's filter would otherwise keep it open */
        morph_cdd[pi] = (uint16_t)df;
        if (p[P_FREEB] > 30)                             /* RES held to 30 while it closes (the damping of the old law's half, 64; audit P07): a switch through a resonant filter rings */
            p[P_FREEB] = (int16_t)(p[P_FREEB] - (((p[P_FREEB] - 30) * df) >> 15));
    }
    if (x->dub) {                                        /* the swell: the delay and reverb sends, up to 3/4 of the way to full */
        p[P_DLY] = (int16_t)(p[P_DLY] + (((127 - p[P_DLY]) * (int32_t)x->dub * 3) >> 17));
        p[P_REV] = (int16_t)(p[P_REV] + (((127 - p[P_REV]) * (int32_t)x->dub * 3) >> 17));
    }
}
/* before part t's block: tp follows the fader, the switched values' side, the valleys' settings. Returns 1 when the blend must
 * be made again (the side changed, or the insert dip moves) */
static __attribute__((noinline)) int morph_xstep(track_t *t, morph_part_t *mp, uint32_t pi)
{
    mxs_t *x = &mxs[pi];
    uint32_t w = (uint32_t)morph_wb(mp->fader), tp = x->tp, k, sound = 0, high = 0, side, age = 0, held = 0;
    for (k = 0; k < NVOICE; k++)
        if (t->v[k].active) {
            sound = 1;
            held |= t->v[k].gate;                        /* (a note held: the wash's gate; a release is the note going, not a gap to fill) */
            if (t->v[k].note > high)
                high = t->v[k].note;
            if (t->v[k].age - x->age < 0x80000000u && t->v[k].age != x->age)
                age = t->v[k].age - x->age > age ? t->v[k].age - x->age : age;   /* (the newest past the one seen: a note started) */
        }
    if (!x->planned || !x->need || !sound || x->follow) {   /* (GLIDE: its trip is the transition's time; the 1.1 s is for a jump of a hand or MIDI) */
        tp = w;                                          /* (nothing to hide, or nothing heard: at once) */
    } else if (tp != w) {
        uint32_t up = w > tp, lo = up ? tp : w, hi = up ? w : tp;
        if (hi <= XW0 || lo >= XW1) {
            tp = w;                                      /* (outside the window: the fader's place) */
        } else {
            if (up && tp < XW0)
                tp = XW0;
            if (!up && tp > XW1)
                tp = XW1;
            tp = up ? (tp + XSTEP < w ? tp + XSTEP : w) : (tp > w + XSTEP ? tp - XSTEP : w);
            if (up ? tp >= XW1 : tp <= XW0)
                tp = w;                                  /* (through the window: on to the fader) */
        }
    }
    if (x->planned && x->need && sound) {               /* (no step past XTP_MAX in or across the arc: see XTP_MAX) */
        uint32_t o = x->tp, lo = o < tp ? o : tp, hi = o < tp ? tp : o;
        if (!(hi <= XW0 || lo >= XW1))
            tp = tp > o + XTP_MAX ? o + XTP_MAX : tp + XTP_MAX < o ? o - XTP_MAX : tp;
    }
    x->tp = (uint16_t)tp;
    side = morph_xpick(x, tp, w, sound, age != 0u);
    x->age += age;
    /* the dip: the part's own multimode filter where its MODE is a low-pass (every engine has it, voice.c uf_run), else the
     * valley's filter (fx.c morph_valley); DUB THROW on any transition */
    k = x->need ? (uint32_t)morph_xdepth(tp) : 0u;
    x->lp = (uint8_t)(t->p[P_FREEC] <= 32 && ENGINES[t->engine]->ufilt);
    {                                                    /* (as voice.c vc_filter: the bypass is MODE LP, CUT open, no KEY TRK; the budget takes the dearer end) */
        const int8_t *e0 = morph_cur(mp, 0), *e1 = morph_cur(mp, 1);
        x->vf = (uint8_t)(!(t->p[P_FREEA] >= UF_STEPS && t->p[P_FREEC] <= 0 && !t->p[P_KTRK]) ||
                          !(e0[P_FREEA] >= UF_STEPS && e0[P_FREEC] <= 0 && !e0[P_KTRK]) ||
                          !(e1[P_FREEA] >= UF_STEPS && e1[P_FREEC] <= 0 && !e1[P_KTRK]));
    }
    x->filt = (uint8_t)(x->lp && x->vf && !(x->need & XN_FILT));
    x->bv = (uint16_t)(x->lp && x->vf ? 0u : x->motion);
    x->d = (uint16_t)(x->need & XN_LP && !x->filt ? k : 0u);
    x->snd = (uint8_t)(held != 0u);
    x->sounding = (uint8_t)sound;
    {
        /* the swell (the sends): GLIDE's movement at half, a transition's dip at 3/5, the larger */
        uint32_t df = x->need & XN_LP ? k : 0u, dub = x->motion >> 1, di = x->need & XN_INS ? (uint32_t)morph_xarc(tp, XI0, XI1) : 0u;
        k = (k * 3u / 5u * XWL[morph_wlvl & 3u]) >> 15;
        dub = k > dub ? k : dub;
        k = x->need ? (uint32_t)morph_xdepth(tp) : 0u;
        if ((df >> 7) != (x->df >> 7u) || (dub >> 7) != (x->dub >> 7u) || di != x->dins)
            side |= 2u;                                  /* (the blend again: a value the arc writes moved; 1/256 steps for the slow ones) */
        x->df = (uint16_t)df, x->dub = (uint16_t)dub, x->dins = (uint16_t)di;
    }
    if (sound)                                           /* the floor: 1.5 times the highest note (a chord keeps its notes, loses its overtones), 120 Hz .. 2 kHz (log2 Hz, Q12) */
        x->lfloor = (uint16_t)clamp(35968 + ((int32_t)high - 69) * 4096 / 12 + 2396, 28290, 44916);
    if (pi < NLIVE && (x->need & XN_FM)) {
        fm6_xt_t *f = &fm6_xt[pi];
        int on = sound && tp > XW0 && tp < XW1; /* silence needs no harmonic bridge or stand-ins */
        if (on) {
            uint32_t now = 0, tune = 0, alg = tp < XD1 ? x->alg[0] : tp < XD2 ? 31u : x->alg[1], fb = tp < XD1 ? x->fb[0] : tp < XD2 ? 0u : x->fb[1];
            int ch = alg != f->alg || fb != f->fb;
            for (k = 0; k < 6u; k++) {
                int32_t g = morph_xgain(x, k, tp);
                f->lv[k] = xlog2(g);                     /* (after the envelope: no OL quantising, nothing clamped in the note) */
                now |= (uint32_t)(g < 4) << k;
                tune |= morph_xtunes(x, k, tp) << k;
            }
            f->alg = (uint8_t)alg;
            f->fb = (uint8_t)fb;
            f->ser = (uint8_t)(f->ser + ch);
            f->tune = (uint8_t)tune;
            f->mute = (uint8_t)(now & x->mprev);
            f->glide = x->glide;
            f->gv = (uint16_t)xpos(tp, XD1, XD2);
            x->mprev = (uint8_t)now;
            for (now = 0, k = 0; k < 6u; k++)            /* the stand-ins: copies from XD1 (silent there) to XD2 - XZ (their carrier silent) */
                now |= (uint32_t)(tp >= XD1 && tp < XD2 - XZ && x->sfor[k]) << k;
            f->ser = (uint8_t)(f->ser + (now != f->xs));
            f->xs = (uint8_t)now;
            memcpy(f->sfor, x->sfor, sizeof x->sfor);
        }
        if (on != f->on) {
            f->on = (uint8_t)on;
            fm6_eff[pi].ok = 0;                          /* (the algorithm and feedback as the patch has them again, or the bridge's) */
            x->mprev = 0;
        }
    }
    if ((side & 1u) != x->side) {
        x->side = (uint8_t)(side & 1u);
        return 1;
    }
    return (int)(side >> 1);
}

/* ---------------------------------------------------------------- the spectral valley (audio ISR, fx.c part_voice) --- */
typedef struct {
    int32_t y[2][2];                                     /* the two one-poles, for the block (0) and SPREAD's side buffer (1) */
    int64_t pin, pout;                                   /* the mean squares before and after, smoothed (~23 ms) */
    int32_t g;                                           /* the make-up now, Q12 */
    int32_t m, a;                                        /* the filtered share and the coefficient of the block before, Q15 */
    int32_t lc;                                          /* the cutoff now (log2 Hz, Q12): the sweep's, held where the make-up runs out */
    int64_t wref, wnow;                                  /* the wash: the part's level at rest (~90 ms) and now (~10 ms), mean squares */
    int32_t wg, wy[3];                                   /* .. its gain (Q12) and its noise's three one-poles (two at the top, one below) */
    uint32_t wr;                                         /* .. its noise's state (xorshift) */
    int64_t qin, qout;                                   /* the mean squares before and after, smoothed (~6 ms: the hold's) */
    uint8_t run;                                         /* the states are this part's last block's */
} mxv_t;
static mxv_t mxv[NLIVE];
#define XV_LTOP 57207                                    /* log2 16 kHz, Q12: where the sweep begins */
#define XV_LW (-52335)                                   /* log2 (2 pi / FS), Q12 (FS 44100) */
_Static_assert(FS == 44100, "morph_xfer.c: XV_LW is for 44.1 kHz");
static int32_t xexp2(int32_t x)                          /* 2^x, x Q12 (-24 .. 7), Q16 */
{
    int32_t i = x >> 12, f = x & 4095, m = 65536 + ((f * (2690 + ((1407 * f) >> 12))) >> 8);   /* 2^f ~ 1 + f (0.6565 + 0.3435 f) */
    return i >= 0 ? m << i : m >> -i;
}
static uint32_t xisqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
/* part pp's voices (b, and SPREAD's side buffer s or 0) through the valley at its depth now; at depth 0 and after it, untouched */
/* TIDE: the wash. While a part moves (GLIDE's movement, a transition) and its notes sound, a soft noise bed in the sound's own
 * register (one-poles from 1.5 times the highest note to two octaves above it, 12 dB / octave above) fills what the movement takes: the level it
 * would fall below (15 dB under the part's level at rest at MID: a bed, not a second sound), and a little more through a transition's floor (24 dB under it at
 * the floor) where a structural change is made. It goes where the part goes: through its sends, the reverb and delay carry it.
 * Silent at rest and between notes (a rest is a rest, a release is the note going): it comes and goes over ~50 ms */
static __attribute__((noinline)) void morph_wash(uint32_t pp, int32_t *b, uint32_t n)
{
    mxv_t *v = &mxv[pp];
    const mxs_t *x = &mxs[pp];
    int64_t p = 0, want = 0, pn = 0;
    int32_t g1 = 0, dg, wl, wh, al, ah, i;
    int move = x->snd && (x->motion || (x->need && x->tp > XW0 && x->tp < XW1));
    int32_t nz[64];
    if (n > 64u || (!morph_wlvl && !v->wg))             /* (WASH OFF, the default, and no wash left to fade: nothing to measure or make; switched on, the level at rest is learnt again in ~90 ms) */
        return;
    for (i = 0; i < (int32_t)n; i++)
        p += (int64_t)b[i] * b[i] >> 8;
    v->wnow += (p - v->wnow) >> 4;
    if (x->snd && !move)
        v->wref += (p - v->wref) >> 7;                  /* (the level at rest: what the movement is measured against) */
    if (!move && !v->wg)
        return;
    if (!v->wr)
        v->wr = 0x9E3779B9u ^ pp;
    wl = xexp2(x->lfloor + XV_LW), wh = xexp2(x->lfloor + 2 * 4096 + XV_LW);   /* (two octaves wide: a soft band, not a hiss) */
    al = (int32_t)(((int64_t)wl << 15) / (65536 + wl)), ah = (int32_t)(((int64_t)wh << 15) / (65536 + wh));
    for (i = 0; i < (int32_t)n; i++) {                   /* white (xorshift32), low-passed at the top, the low part taken away */
        int32_t w;
        v->wr ^= v->wr << 13, v->wr ^= v->wr >> 17, v->wr ^= v->wr << 5;
        w = (int32_t)(v->wr >> 16) - 32768;
        v->wy[0] += (int32_t)(((int64_t)ah * (w - v->wy[0])) >> 15);
        v->wy[2] += (int32_t)(((int64_t)ah * (v->wy[0] - v->wy[2])) >> 15);   /* (12 dB / octave above the band: no edge) */
        v->wy[1] += (int32_t)(((int64_t)al * (v->wy[2] - v->wy[1])) >> 15);
        nz[i] = v->wy[2] - v->wy[1];
        pn += (int64_t)nz[i] * nz[i] >> 8;
    }
    if (move && morph_wlvl) {
        int64_t fill = (v->wref >> XWS[morph_wlvl & 3u]) - v->wnow;   /* (to 9 dB under the level at rest at MID: WASH) */
        want = fill > 0 ? fill : 0;
        if (x->need)
            want += ((v->wref >> (XWS[morph_wlvl & 3u] + 3u)) * morph_xdepth(x->tp)) >> 15;   /* (a transition's floor: 24 dB under it at MID) */
        want = want > (v->wref >> XWS[morph_wlvl & 3u]) ? v->wref >> XWS[morph_wlvl & 3u] : want;
    }
    if (want > 0 && pn > 0) {                            /* the gain: the noise's level to the wanted one, Q12, at most x 16 */
        int64_t r = (want << 10) / pn;                  /* (Q10 of the power ratio) */
        r = r > ((int64_t)256 << 10) - 1 ? ((int64_t)256 << 10) - 1 : r;   /* (just under x 16: r << 14 stays below 2^32, else it wraps to 0 and the wash fades out where it is most wanted) */
        g1 = (int32_t)xisqrt((uint32_t)(r << 14));      /* (sqrt of Q24: Q12) */
    }
    if (g1 != v->wg)                                     /* (in and out slowly, ~50 ms each way: a bed that comes and goes, not a burst) */
        g1 = v->wg + (g1 - v->wg) / 64 + (g1 > v->wg ? 1 : -1);
    dg = (g1 - v->wg) / (int32_t)n;
    for (i = 0; i < (int32_t)n; i++) {
        v->wg += dg;
        b[i] += (int32_t)(((int64_t)nz[i] * v->wg) >> 12);
    }
    v->wg = g1;
}
/* the part rendered nothing this block (fx.c part_voice): the valley and the wash at rest, from silence next time */
static __attribute__((noinline)) void morph_vrest(uint32_t pp)
{
    mxs[pp % NSHOWN].sounding = 0;
    mxv_t *v = &mxv[pp % NSHOWN];
    v->run = 0;
    v->m = 0;
    v->wg = 0;
    v->wy[0] = v->wy[1] = v->wy[2] = 0;
}
static __attribute__((noinline)) void morph_valley(uint32_t pp, int32_t *b, int32_t *s, uint32_t n)
{
    mxv_t *v = &mxv[pp];
    const mxs_t *x = &mxs[pp];
    int32_t d = x->d, dd = d > x->bv ? d : x->bv, m1, a1, w, g1, dm, da, dg, m, a, g;
    int64_t pin = 0, pout = 0;
    uint32_t i, c;
    if (!dd && !v->m) {
        v->run = 0;
        morph_wash(pp, b, n);
        return;
    }
    m1 = dd * 3 > 32768 ? 32768 : dd * 3;                /* (the filtered share comes in first, then the sweep goes down) */
    if (!v->run)
        v->lc = XV_LTOP;
    /* the cutoff: the sweep's (from 16 kHz down to the floor and back, by d), but it goes down only while the filter takes
     * less than 6 dB, within what the make-up can give back (9 dB; a sound whose energy lies well above its notes, noise or a bright pluck,
     * stops higher: a switch inside such a sound is masked by its own high band, and boosting a rumble 15 dB would be a jump
     * of its own), and backs up 1/24 octave a block past 9 dB */
    w = XV_LTOP - (int32_t)(((int64_t)(XV_LTOP - x->lfloor) * d) >> 15);
    if (x->bv) {                                         /* GLIDE's breath here (the voices' filter bypassed): two octaves down from the top at its deepest, not under the floor */
        int32_t wb = XV_LTOP - (int32_t)(((int32_t)x->bv * 8192) >> 15);
        wb = wb > (int32_t)x->lfloor ? wb : (int32_t)x->lfloor;
        w = wb < w ? wb : w;
    }
    if (w >= v->lc)
        v->lc = w - v->lc > 171 ? v->lc + 171 : w;       /* (at most 1/24 octave a block either way: faster than the arc, no jump) */
    else if (v->qout > 0 && v->qin > v->qout * 4)
        v->lc = v->qin > v->qout * 8 && v->lc + 171 < XV_LTOP ? v->lc + 171 : v->lc;
    else
        v->lc = v->lc - w > 171 ? v->lc - 171 : w;
    w = xexp2(v->lc + XV_LW);                            /* 2 pi fc / FS, Q16 */
    a1 = (int32_t)(((int64_t)w << 15) / (65536 + w));   /* a one-pole's coefficient ~ 1 - e^-w, Q15 */
    if (!v->run) {                                       /* starting: from the signal, no transient; the make-up at unity */
        for (c = 0; c < 2u; c++)
            v->y[c][0] = v->y[c][1] = c ? (s ? s[0] : 0) : b[0];
        v->m = 0;
        v->a = a1;
        v->g = 4096;
        v->pin = v->pout = v->qin = v->qout = 0;
        v->run = 1;
    }
    m = v->m, a = v->a, g = v->g;
    dm = (m1 - m) / (int32_t)n, da = (a1 - a) / (int32_t)n;
    for (c = 0; c < 2u; c++) {
        int32_t *p = c ? s : b, y0 = v->y[c][0], y1 = v->y[c][1], mm = m, aa = a;
        if (!p)
            continue;
        for (i = 0; i < n; i++) {
            int32_t xi = p[i], o;
            mm += dm, aa += da;
            y0 += (int32_t)(((int64_t)aa * (xi - y0)) >> 15);
            y1 += (int32_t)(((int64_t)aa * (y0 - y1)) >> 15);
            o = xi + (int32_t)(((int64_t)mm * (y1 - xi)) >> 15);
            if (!c) {
                pin += (int64_t)xi * xi >> 8;
                pout += (int64_t)o * o >> 8;
            }
            p[i] = o;
        }
        v->y[c][0] = y0, v->y[c][1] = y1;
    }
    v->pin += (pin - v->pin) >> 5;                       /* (~23 ms) */
    v->pout += (pout - v->pout) >> 5;
    v->qin += (pin - v->qin) >> 3;                       /* (~6 ms) */
    v->qout += (pout - v->qout) >> 3;
    g1 = 4096;
    if (v->qout > 0 && v->qin > v->qout) {               /* RMS make-up, as much as the filter took, at most +9 dB (x 2.82) */
        int64_t pi_ = v->qin, po = v->qout, r;
        while (pi_ >= (int64_t)1 << 38)                  /* (the ratio's numerator in range) */
            pi_ >>= 1, po >>= 1;
        r = po > 0 ? (pi_ << 24) / po : (int64_t)8 << 24;
        g1 = (int32_t)xisqrt((uint32_t)(r > (int64_t)8 << 24 ? (int64_t)8 << 24 : r));
        g1 = g1 > 11585 ? 11585 : g1;
    }
    g1 = 4096 + (int32_t)(((int64_t)(g1 - 4096) * m1) >> 15);
    dg = (g1 - g) / (int32_t)n;
    for (c = 0; c < 2u; c++) {
        int32_t *p = c ? s : b, gg = g;
        if (!p)
            continue;
        for (i = 0; i < n; i++) {
            gg += dg;
            p[i] = (int32_t)(((int64_t)p[i] * gg) >> 12);
        }
    }
    v->m = m1, v->a = a1, v->g = g1;
    morph_wash(pp, b, n);
}
