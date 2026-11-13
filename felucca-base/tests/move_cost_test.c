/* SPDX-License-Identifier: GPL-3.0-only */
/* Transition costs only while moving (b10 round 5, ROADMAP Later 10 and 13 / C1). FELUCCA_MOVECOST (core.h, tools/build.py env FELUCCA_MOVECOST=1): a part's morph costs are charged
 * only while its fader moves or a GLIDE runs (budget.c bd_part / bd_mv, the rule the FM6 bridge has had since 6 November), and the INSERT dip asks for a new blend only when it moved by a
 * 1/256 step (morph_xfer.c morph_xstep). Built twice by host_tests.sh (group "movecost": the default and -DFELUCCA_MOVECOST=1).
 *   BUDGET  bd_part of a part with a morph, at rest and while moving (bd_mv): off: the same; on: the valley, the wash and the cross-fade cost nothing at rest and the same as before moving.
 *   WHERE   b10 round 6 (budget.c bd_where): "at rest" is the fader at an end with its transition arrived; a fader parked inside the arc (the valley and the wash run) or between the ends (both shapes of a
 *           cross-faded choice are made) is charged, and so is a jump (the transition's place trails the fader for ~1.1 s); the wash's meter alone is charged at rest with WASH on
 *   BLENDS  a hand jump of the fader 0 -> 127 with an INSERT difference between the ends (need XN_INS forced on the plan): the blends over 1.5 s (EASE_PROBE(0)); on: fewer (1.26 times on M02). */
#define UI_TEST_NO_MAIN 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
static unsigned ease_probe[2];
#define EASE_PROBE(k) (ease_probe[k]++)
#include "ui_test.c"

static int bad;
static void expect(const char *what, int ok)
{
    printf("movecost: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
}
int main(void)
{
    bd_part_t rest, move;
    int32_t out[2 * CTL];
    unsigned blends = 0, blk;
    ui_power_on();
    frame();
    song.playing = song.rec = 0;
    transport_req = 0;
    gl.on = gl.armed = 0;
    project_load(1u);                                   /* M02 */
    morph_ready(0);
    morph_do(MQ_FADER, 1u, 0u);
    morph_wlvl = 2;                                     /* WASH MID: the wash is charged when it runs */
    trk[0].p[P_MUTE] = 0;
    memset(out, 0, sizeof out);
    mix_block(out, CTL);
    expect("the part has a morph (the ends differ)", mph[0].nid > 0);
    bd_mv[0] = 0;
    bd_part(&trk[0], &rest);
    bd_mv[0] = 1;
    bd_part(&trk[0], &move);
    printf("movecost: FELUCCA_MOVECOST=%d: part 1 at rest: tide %u ext %u misc %u; moving: tide %u ext %u misc %u (cost units, the budget's)\n", FELUCCA_MOVECOST,
           (unsigned)rest.tide, (unsigned)rest.ext, (unsigned)rest.misc, (unsigned)move.tide, (unsigned)move.ext, (unsigned)move.misc);
    expect("moving: the valley / wash are charged (WASH MID: at least 60)", move.tide >= 60u);
#if FELUCCA_MOVECOST
    expect("on: at rest the valley and the wash's noise cost nothing (the wash's meter does: BD_TIDE_WASH_REST), the part costs less", rest.tide == BD_TIDE_WASH_REST && rest.misc < move.misc);
#else
    expect("off: at rest the part costs what it does moving (nothing changed)", rest.tide == move.tide && rest.misc == move.misc && rest.ext == move.ext);
#endif
    {   /* WHERE: the fader at an end, parked in the arc, parked between the ends but outside the arc, and jumped (tp trails) */
        uint32_t mid, arc;
        bd_part_t pk;
        mxs[0].planned = 1, mxs[0].need = XN_LP;
        bd_mv[0] = 0;
        morph_do(MQ_FADER, 1u, 0u);
        mxs[0].tp = 0;
        bd_where(0, &mid, &arc);
        expect("WHERE: the fader at an end, arrived: mid and arc are off (on) / on (off)", FELUCCA_MOVECOST ? !mid && !arc : mid && arc);
        morph_do(MQ_FADER, 1u, 64u);
        mxs[0].tp = (uint16_t)morph_wb(64u);
        bd_where(0, &mid, &arc);
        expect("WHERE: parked inside the arc: charged (mid and arc)", mid && arc);
        bd_part(&trk[0], &pk);
        expect("WHERE: parked inside the arc: the valley and the wash are charged (WASH MID: at least 60)", pk.tide >= 60u);
        morph_do(MQ_FADER, 1u, 3u);
        mxs[0].tp = (uint16_t)morph_wb(3u);
        bd_where(0, &mid, &arc);
        expect("WHERE: parked between the ends below the arc: both shapes are made (mid), no valley (arc off on)", FELUCCA_MOVECOST ? mid && !arc : mid && arc);
        morph_do(MQ_FADER, 1u, 0u);
        mxs[0].tp = (uint16_t)morph_wb(127u);
        bd_where(0, &mid, &arc);
        expect("WHERE: a jump (the transition's place trails the fader): charged", mid && arc);
        mxs[0].tp = 0;
        mxs[0].need = 0;
        bd_part(&trk[0], &pk);
        expect("WHERE: at an end, arrived, WASH MID: only the meter (BD_TIDE_WASH_REST) on (the wash 60 off)", FELUCCA_MOVECOST ? pk.tide == BD_TIDE_WASH_REST : pk.tide >= 60u);
        morph_wlvl = 0;
        bd_part(&trk[0], &pk);
        expect("WHERE: WASH OFF at rest: nothing", FELUCCA_MOVECOST ? pk.tide == 0u : 1);
        morph_wlvl = 2;
    }
    bd_mv[0] = 0;
    trk[0].nheld = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk_note_on(&trk[0], 60, 100);                      /* a note sounding: the crossing runs only then */
    mxs[0].need |= XN_INS;                              /* an INSERT difference between the ends: the insert dip runs */
    morph_wlvl = 0;
    morph_do(MQ_FADER, 1u, 0u);
    for (blk = 0; blk < 60u; blk++) {                  /* at rest first: the main loop makes the crossing's plan */
        mxs[0].need |= XN_INS;
        mix_block(out, CTL);
        if (blk % 4u == 3u)
            frame();
    }
    ease_probe[0] = 0;
    morph_do(MQ_FADER, 1u, 127u);                       /* a hand jump 0 -> 127 */
    for (blk = 0; blk < 1034u; blk++) {                 /* 1.5 s */
        mxs[0].need |= XN_INS;
        mix_block(out, CTL);
        if (blk % 22u == 21u)
            frame();                                    /* (the main loop plans the crossing) */
    }
    blends = ease_probe[0];
    printf("movecost: FELUCCA_MOVECOST=%d: blends over 1.5 s after a hand jump 0 -> 127 with an INSERT difference: %u\n", FELUCCA_MOVECOST, blends);
    expect("the jump arrived (the part's fader is 127 and blended)", mph[0].cvalid && mph[0].cfader == mph[0].fader);
#if FELUCCA_MOVECOST
    expect("on: the INSERT dip asks for fewer blends than the 351 it did (1/256 steps; the brief's 3 times is not reached on M02: the dip's ramp is steep)", blends > 0u && blends < 320u);
#else
    expect("off: the INSERT dip blends as it did (351 on M02)", blends > 330u);
#endif
    printf("movecost: %s\n", bad ? "FAIL" : "all ok");
    return bad ? 1 : 0;
}
