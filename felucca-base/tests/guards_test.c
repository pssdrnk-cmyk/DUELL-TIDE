/* SPDX-License-Identifier: GPL-3.0-only */
/* FELUCCA_GUARDS (b10 round 7, ROADMAP Later 14): morph_xrest and vs_block run only when their inputs are live. Built twice by host_tests.sh (group "guards": -DFELUCCA_GUARDS=0 and 1); the
 * two builds print the same renders hash and the group compares them, so the guard changes no sample. Scenes (every one from a fresh power-on):
 *   MULTIS   each of the 24 factory multis: an 8-note burst on part 1 and 3 notes on part 2 (the note-start queue fills and drains), the MORPH fader moved in steps and jumped, 200 nblk
 *   NOMORPH  a part with no morph for 300 nblk, then a morph, a fader move and the morph taken away again (the plan's state must be cleared by morph_xrest once it is gone)
 *   QUEUE    a burst on a POLY part, a note-off of a waiting note, STOP ALL, a UNISON part: the queue's counts
 * and the operation counts: how often vs_block and morph_xrest were called and how often they ran their body (off: every call). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
static unsigned gd_vs_call, gd_vs_run, gd_xr_call, gd_xr_run;
#define GD_COUNT(c) ((c)++)
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static uint64_t hsh = 1469598103934665603ull;
static int32_t ob[2 * CTL];
static unsigned nblk;
static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
    song.playing = song.rec = 0; transport_req = 0; gl.on = gl.armed = 0;
}
static void run(unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        mix_block(ob, CTL);
        for (unsigned j = 0; j < 2u * CTL; j++)
            hsh = (hsh ^ (uint32_t)ob[j]) * 1099511628211ull;
        nblk++;
        if (i % 4u == 3u)
            frame();
    }
}
int main(void)
{
    unsigned fails = 0;
    setvbuf(stdout, 0, _IONBF, 0);
    for (unsigned i = 0; i < 24u; i++) {
        power();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        project_load(MIDX(i));
        frame();
        for (unsigned q = 0; q < 8u; q++) trk_note_on(&trk[0], 40 + 3 * q, 100);
        for (unsigned q = 0; q < 3u; q++) trk_note_on(&trk[1], 52 + 4 * q, 100);
        run(40);
        for (unsigned f = 0; f < 128u; f += 9u) { morph_do(MQ_FADER, 3u, f); run(6); }
        morph_do(MQ_FADER, 3u, 0u); run(10);
        morph_do(MQ_FADER, 3u, 127u); run(60);
        for (unsigned q = 0; q < 8u; q++) trk_note_off(&trk[0], 40 + 3 * q);
        run(40);
    }
    power();
    host_preset(&trk[0], 0, 0);
    trk[1].p[P_MUTE] = 1;
    trk_note_on(&trk[0], 48, 100); trk_note_on(&trk[0], 55, 100);
    run(300);
    morph_ready(0);
    morph_do(MQ_FADER, 1u, 0u); run(10);
    morph_do(MQ_FADER, 1u, 60u); run(80);
    mph[0].nid = mph[0].fnid = 0;                       /* the morph taken away with the state of a crossing standing */
    run(80);
    {
        unsigned left = mxs[0].d | mxs[0].df | mxs[0].dub | mxs[0].dins | mxs[0].bv | mxs[0].need | mxs[0].snd | mxs[0].sounding | morph_cdd[0] | fm6_xt[0].on;
        printf("guards: after the morph is taken away the crossing state of part 1 is %s\n", left ? "NOT cleared (FAIL)" : "cleared");
        fails += left != 0;
    }
    power();
    for (unsigned q = 0; q < 12u; q++) trk_note_on(&trk[0], 36 + 2 * q, 100);
    run(1);
    trk_note_off(&trk[0], 36 + 2 * 11);                 /* a waiting note never starts */
    run(10);
    trk_note_on(&trk[0], 70, 100); trk_note_on(&trk[0], 72, 100); trk_note_on(&trk[0], 74, 100);
    run(1);
    trk_all_off(&trk[0]);
    run(10);
    trk[1].p[P_VOICE] = V_UNISON;
    trk_note_on(&trk[1], 60, 100); trk_note_on(&trk[1], 64, 100);
    run(20);
    power();                                            /* bursts of every size after the queue has drained (the counts of the block before must be reset: a stale count holds back a note that starts at once) */
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
    trk[0].p[P_MUTE] = trk[1].p[P_MUTE] = 0;
    trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = AM_OFF;
    for (unsigned r = 0; r < 12u; r++) {
        unsigned nn = 1u + (r * 5u) % 7u, g = 0;
        track_t *tt = &trk[r & 1u];
        events_block(CTL);                              /* the block's head: vs_block ran; the notes below arrive after it, as keys and MIDI do inside events_block */
        for (unsigned q = 0; q < nn; q++) trk_note_on(tt, 40 + 2 * q + r, 100);
        for (unsigned i = 0; i < NVOICE; i++) g += tt->v[i].active && tt->v[i].gate;
        hsh = (hsh ^ g) * 1099511628211ull;             /* (the voices started at once: at most VS_MAX; the two builds must agree) */
        run(1 + r % 3u);
        for (unsigned q = 0; q < nn; q++) trk_note_off(tt, 40 + 2 * q + r);
        run(5);
    }
    printf("guards: GUARDS=%d renders hash %016llx over %u nblk\n", FELUCCA_GUARDS, (unsigned long long)hsh, nblk);
    printf("guards: vs_block: %u calls, %u ran their body (%.1f %%); morph_xrest: %u calls (a part with no morph, a block), %u ran (%.1f %%)\n", gd_vs_call, gd_vs_run,
           gd_vs_call ? 100.0 * gd_vs_run / gd_vs_call : 0.0, gd_xr_call, gd_xr_run, gd_xr_call ? 100.0 * gd_xr_run / gd_xr_call : 0.0);
    if (!FELUCCA_GUARDS && (gd_vs_run != gd_vs_call || gd_xr_run != gd_xr_call)) { printf("guards: FAIL off: every call must run\n"); fails++; }
    if (FELUCCA_GUARDS && !(gd_vs_run < gd_vs_call / 4u && gd_xr_run < gd_xr_call / 4u)) { printf("guards: FAIL on: the guards should skip most calls\n"); fails++; }
    printf("guards: %s\n", fails ? "FAIL" : "all ok");
    return fails ? 1 : 0;
}
