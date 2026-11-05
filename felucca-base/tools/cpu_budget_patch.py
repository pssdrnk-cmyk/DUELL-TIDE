#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the CPU-budget harness: tests/cpu_scenarios_test.c plus a basic-block counter and sections Z (M16 as the owner runs it) and T (M16 with TIDE's
sequencers playing in automatic MUSIC WNDR/JRNY). Usage: cpu_budget_patch.py SRC.c OUT.c   (see cpu_budget.sh)"""
import sys

src = open(sys.argv[1]).read()
CH = chr(92) + 'n'


def rep(old, new, what):
    global src
    if old not in src:
        sys.exit('cpu_budget_patch: anchor not found: ' + what)
    src = src.replace(old, new, 1)


rep('static uint64_t instr_now(void)\n', 'static uint64_t instr_now_orig(void)\n', 'instr_now')
rep('static uint64_t ovh;', 'static uint64_t ovh;\nextern uint64_t bbc_ext;                         /* basic blocks run (bbc.c, __sanitizer_cov_trace_pc) */\nstatic uint64_t instr_now(void) { return bbc_ext + 1; }\nstatic uint64_t g_mb;\n', 'ovh')
rep('    c = instr_now() - i0 - ovh;\n', '    c = instr_now() - i0 - ovh;\n    if (c > g_mb) g_mb = c;\n', 'blk')

code = r'''
/* ---- cpu-budget: section Z (the existing S run, M16 STRESS TEST played from the keys) and T (the same with TIDE playing). Units: basic blocks per half (BB), one block = 128 / 4 frames */
static uint32_t g_tide, g_bars = 16, g_t_turns = 1, g_t_poly = 1, g_t_jrny;
static void tide_setup(void)
{
    uint32_t p, i;
    for (p = 0; p < 2u; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = 16;
        t->p[P_SDIV] = 2;
        t->p[P_ARATE] = 2;
        t->p[P_AMODE] = AM_OFF;
        t->p[P_APROB] = 127;
        t->p[P_AGATE] = 64;
        t->p[P_SQNT] = 1;
        t->p[P_MUTE] = 0;
        t->nheld = 0;
        t->seq_active = 1;
        for (i = 0; i < 16u; i++) {
            t->step[i].time = i % 3u == 1u ? ST_REST : ST_NOTE;
            t->step[i].n = t->step[i].time == ST_NOTE;
            t->step[i].note[0] = (uint8_t)(60 + (i * 5u) % 12u - (p ? 12u : 0u));
            t->step[i].vel = 100;
        }
    }
#ifdef HAVE_SQM_AUTO
    sqm_vary_all = 0;
#ifdef HAVE_TURNS
    sqm_turns = (uint8_t)g_t_turns;
#endif
#ifdef HAVE_POLY
    sqm_poly = (uint8_t)g_t_poly;
#endif
#endif
    for (p = 0; p < 2u; p++) {
        seqm_set_amount(p, 80);
        seqm_roll(p);
        seqm_set_fader(p, 127);
    }
    seqm_poll();
    seq_start();
    song.playing = 1;
    gl.on = 3, gl.mode = g_t_jrny ? GM_JOURNEY : GM_WANDER, gl.mask = 3, gl.armed = 0;
    morph_musical = 1;
}
typedef struct { double avg; uint64_t pkh, pkb, ml_max, ml_n; double ml_avg; uint32_t vb; uint64_t hist[8]; } zres_t;
static zres_t zrun(uint32_t mode, uint32_t nk, uint32_t tide)
{
    static const uint8_t KN[8] = {60, 64, 67, 71, 74, 77, 62, 69};
    zres_t r;
    uint64_t sum = 0, mlsum = 0, h, c;
    uint32_t i, n, keys = 0, nh, bpm;
    memset(&r, 0, sizeof r);
    power();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(15);
    duell_mode_req = (uint8_t)mode;
    ovl_lite = 0;
    frame();
    for (i = 0; i < 40; i++) { half(0); while (wave_service()) ; frame(); }
    morph_set_target(1);
    morph_set_fader(0);
    for (i = 0; i < nk; i++) {
        keys |= 1u << key_of_note(KN[i]);
        if (mode != DM_LAYER)
            midi_in_event(0x09u | 0x91u << 8 | (uint32_t)KN[i] << 16 | 110u << 24);
    }
    fm1_in.notes = keys;
    if (tide)
        tide_setup();
    for (i = 0; i < 120; i++) { half(0); while (wave_service()) ; if (tide) tide_poll(); }
    bpm = song.g[G_BPM] >= 40 && song.g[G_BPM] <= 300 ? (uint32_t)song.g[G_BPM] : 137u;
    nh = (uint32_t)((uint64_t)g_bars * 4u * 60u * FS / (bpm * HALF_FRAMES));
    g_mb = 0;
    for (n = 0; n < nh; n++) {
        uint32_t v = voices_busy();
        if (tide) {
            uint64_t m0 = instr_now();
            tide_poll();
            c = instr_now() - m0 - ovh;
            mlsum += c; r.ml_n++;
            if (c > r.ml_max) r.ml_max = c;
        }
        h = half(0); sum += h; if (h > r.pkh) r.pkh = h;
        r.hist[h * 8u / 80000u > 7u ? 7u : h * 8u / 80000u]++;
        r.vb = v > r.vb ? v : r.vb;
    }
    fm1_in.notes = 0;
    r.avg = (double)sum / nh;
    r.pkb = g_mb;
    r.ml_avg = r.ml_n ? (double)mlsum / r.ml_n : 0;
    (void)bpm;
    return r;
}
static void zsection(const char *tag, uint32_t tide)
{
    static const uint32_t MZ[2] = {DM_LAYER, DM_SPLIT};
    uint32_t m, nk;
    for (m = 0; m < 2u; m++)
        for (nk = tide ? 0u : 1u; nk <= 3u; nk++) {
            zres_t r = zrun(MZ[m], nk, tide);
            printf("%s %-5s %u keys: avg_half %.0f  peak_half %.0f  peak_block %llu  main_loop_poll avg %.0f max %llu  voices %u  halves>=40k:%llu >=50k:%llu >=60k:%llu@NL@",
                   tag, m ? "DUEL" : "LAYER", nk, r.avg, (double)r.pkh, (unsigned long long)r.pkb, r.ml_avg, (unsigned long long)r.ml_max, r.vb,
                   (unsigned long long)(r.hist[4] + r.hist[5] + r.hist[6] + r.hist[7]), (unsigned long long)(r.hist[5] + r.hist[6] + r.hist[7]), (unsigned long long)(r.hist[6] + r.hist[7]));
        }
}
'''
code = code.replace('project_load(15);', 'if (g_stress) scene_load(15, "M16_STRESS_TEST"); else project_load(g_slot);')
code = code.replace('static uint32_t g_tide,', 'static uint32_t g_slot = 15, g_play, g_stress, g_tide,')
TS = '    if (tide)' + chr(10) + '        tide_setup();'
code = code.replace(TS, TS + chr(10) + '    if (g_play) { seq_start(); song.playing = 1; }   /* PLAY: the stored arrangement (U2 B) */')
code = code.replace('nk = tide ? 0u : 1u;', 'nk = tide || g_play ? 0u : 1u;')
code = code.replace('@NL@', CH)
rep('int main(void)\n', code + '\nint main(void)\n', 'main')
main_hook = '''    if (strchr(sec, 'Z') || strchr(sec, 'T')) {   /* cpu-budget */
        if (getenv("BARS")) g_bars = (uint32_t)atoi(getenv("BARS"));
        if (getenv("TURNS")) g_t_turns = (uint32_t)atoi(getenv("TURNS"));
        if (getenv("POLY")) g_t_poly = (uint32_t)atoi(getenv("POLY"));
        if (getenv("JRNY")) g_t_jrny = (uint32_t)atoi(getenv("JRNY"));
        if (getenv("SLOT")) g_slot = (uint32_t)atoi(getenv("SLOT")) - 1u;
        g_stress = !getenv("SLOT");                 /* (no SLOT: the old STRESS TEST, as before) */
        if (getenv("STRESS")) g_stress = (uint32_t)atoi(getenv("STRESS"));   /* 1: the old M16 STRESS TEST (tests/scenes/M16_STRESS_TEST.bin), the proxy's 70 % calibration point */
        if (getenv("PLAY")) g_play = (uint32_t)atoi(getenv("PLAY"));
        if (strchr(sec, 'Z')) zsection("Z", 0);
        if (strchr(sec, 'T')) zsection("T", 1);
        return 0;
    }
'''
rep("    if (strchr(sec, 'Q')) {", main_hook + "    if (strchr(sec, 'Q')) {", 'Q')
rep('#include <string.h>', '#include <string.h>\n#include <stdlib.h>', 'string.h')
open(sys.argv[2], 'w').write(src)
