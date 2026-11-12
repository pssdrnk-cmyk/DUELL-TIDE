/* SPDX-License-Identifier: GPL-3.0-only */
/* Hand-moved morph saving (b10 round 4, ROADMAP Later 9; audit finding 12, D44). A fader moved by a hand or MIDI (not a GLIDE) used to be blended every block, about 70 us a block
 * on the unit; GLIDE alone blended every other block. FELUCCA_HANDSKIP (morph.c, tools/build.py env FELUCCA_HANDSKIP=1) gives a hand the same path: a blend every other block, the last one
 * written in between (the fader 0.73 ms behind). Off (the default): every block, as before.
 * Built twice by host_tests.sh (group "handskip": the default and -DFELUCCA_HANDSKIP=1). The fader steps once a block over two sweeps; EASE_PROBE(0) counts the blends. PASS:
 *   off: a blend every block (nothing changed); on: about half of them; both: once the fader stops, the written values are the fader's own within two blocks; the fader jump
 *   (a load, MIDI over 12 steps) still blends at once. */
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
    printf("handskip: %-96s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
}
static uint32_t sweep(uint32_t from, uint32_t to, unsigned *blends)
{
    uint32_t f = from, n = 0;
    int32_t out[2 * CTL];
    ease_probe[0] = 0;
    while (f != to) {
        f += f < to ? 1u : (uint32_t)-1;
        morph_do(MQ_FADER, 1u, f);
        memset(out, 0, sizeof out);
        mix_block(out, CTL);
        n++;
    }
    *blends = ease_probe[0];
    return n;
}
int main(void)
{
    unsigned b1, b2, bj;
    uint32_t n1, n2, nj;
    int32_t out[2 * CTL];
    ui_power_on();
    frame();
    song.playing = song.rec = 0;
    transport_req = 0;
    gl.on = gl.armed = 0;
    project_load(1u);                                   /* M02: both ends differ, so a blend has work */
    morph_ready(0);
    morph_do(MQ_FADER, 1u, 0u);
    trk[0].p[P_MUTE] = 0;
    trk[0].nheld = 0;
    trk[0].seq_active = 0;
    trk_note_on(&trk[0], 60, 100);                      /* a note sounding: the blend runs for the part that renders */
    memset(out, 0, sizeof out);
    mix_block(out, CTL);
    expect("the part has a blend to do (the ends differ)", mph[0].nid > 0);
    n1 = sweep(0u, 100u, &b1);
    n2 = sweep(100u, 0u, &b2);
    printf("handskip: FELUCCA_HANDSKIP=%d: the fader steps once a block, %u blocks up, %u blends, %u blocks down, %u blends\n", FELUCCA_HANDSKIP, (unsigned)n1, b1, (unsigned)n2, b2);
#if FELUCCA_HANDSKIP
    expect("on: a hand-moved fader blends about every other block (45..60 of 100, up and down)", b1 >= 45u && b1 <= 60u && b2 >= 45u && b2 <= 60u);
#else
    expect("off: a hand-moved fader blends every block, as before", b1 >= n1 - 1u && b2 >= n2 - 1u);
#endif
    {                                                   /* the fader stops: two quiet blocks, the written values are the fader's blend */
        unsigned b;
        ease_probe[0] = 0;
        mix_block(out, CTL);
        mix_block(out, CTL);
        b = ease_probe[0];
        expect("the fader stopped: the blend caught up within two blocks (cfader is the fader)", mph[0].cvalid && mph[0].cfader == mph[0].fader);
        (void)b;
    }
    nj = sweep(0u, 0u, &bj);                            /* (nothing: a placeholder so a jump below starts from rest) */
    (void)nj;
    morph_do(MQ_FADER, 1u, 0u);
    mix_block(out, CTL);
    mix_block(out, CTL);
    ease_probe[0] = 0;
    morph_do(MQ_FADER, 1u, 120u);                       /* a jump (a load, MIDI over 12 steps) */
    mix_block(out, CTL);
    mix_block(out, CTL);
    expect("a jump of the fader is blended within two blocks", mph[0].cfader == mph[0].fader && ease_probe[0] >= 1u);
    printf("handskip: %s\n", bad ? "FAIL" : "all ok");
    return bad ? 1 : 0;
}
