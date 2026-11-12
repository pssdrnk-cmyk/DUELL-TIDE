/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE TRAVEL's level match (b10 round 2, ROADMAP Later 26): every factory multi's part as the multi plays it, in the pattern
 * tests/jrny_ctx.c measures the factory sounds in, so a TRAVEL destination can be set to the part's level (morph_tide.c
 * morph_to_sound). The part's own TRIM lifts or lowers a sound the multi re-voiced (M06's darker MELLOW EP +5 dB, M19's sparse
 * HARM BASS +12 dB): the factory sound's level does not say how loud the part is, this does.
 * The context, as jrny_ctx.c: the multi loaded (MODE RECALL ON, End A), the globals at their defaults (the sound table's), STEREO,
 * the part alone (the other's LEVEL 0), its LEVEL at the default, CHORD and the arp off, transport stopped; the part's Original's
 * class picks the pattern (2 a bass: C2 on 1 and 3, 1.5 16ths; 1 a pad, ATK 60 or more: C3 E3 G3 on 1 and 3, 1.5 beats; 0 else:
 * the chord on the off-beat 16th, 45 % gate), 120 BPM, 4 bars, velocity 100, measured over bars 2..4 on the mix before MASTER,
 * K-weighted (loudness.h lz_kw). One byte a part: round(2 x (dB + 37)), half dB (the TRIM's step), 0..255.
 *   check (host_tests.sh jrnymu): a fresh run must equal firmware/src/jrny_mu.h byte for byte
 *   write:  build/host/jrny_mu gen > firmware/src/jrny_mu.h   (after a factory multi or one of its sounds changes)
 * Needs fork (Linux / WSL / macOS). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#include <math.h>
#include "loudness.h"

#define JM_BPM 120.0
#define JM_BARS 4u
static void pump(void)
{
    static uint32_t n;
    fm1_ms += 3u;
    while (wave_service())
        ;
    if (!(++n & 7u))
        morph_xpoll();
}
static double measure(uint32_t slot, uint32_t p)
{
    static const uint8_t CH[3] = {48, 52, 55}, BASS[1] = {36};
    const double bar = 4.0 * 60.0 / JM_BPM;
    const uint8_t *nt;
    uint32_t nn, ctx, b, i, c, nb = (uint32_t)(JM_BARS * bar * FS) / CTL, b0 = (uint32_t)(bar * FS) / CTL, down = 0;
    int32_t out[2 * CTL];
    lz_kw_t kw;
    track_t *t = &trk[p];
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 4096;
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    for (i = 0; i < 40; i++) {
        memset(out, 0, sizeof out);
        mix_block(out, CTL);
        pump();
    }
    morph_set_fader(0);
    song.playing = song.rec = 0;
    transport_req = 0;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    duell_mode_force(DM_STEREO);
    trk[p ^ 1u].p[P_LEVEL] = 0;
    t->p[P_LEVEL] = TP[P_LEVEL].def;
    t->p[P_CHRD] = 0;
    t->p[P_AMODE] = 0;
    nt = ds_cat_of(t->eng_req, t->preset) == CAT_BASS ? BASS : CH;
    nn = nt == BASS ? 1u : 3u;
    ctx = nt == BASS ? 2u : ds_value(ds_get(t->eng_req, t->preset), P_ATK, TP[P_ATK].def) >= 60 ? 1u : 0u;   /* (morph_tide.c jctx_cx: the Original's class) */
    lz_kw_init(&kw);
    for (b = 0; b < nb; b++) {
        double tm = (double)b * CTL / FS, beat = fmod(tm, bar) / (bar / 4.0), st = beat * 4.0, ph = fmod(st, 4.0);
        uint32_t on = ctx == 1u ? (beat < 1.5) || (beat >= 2.0 && beat < 3.5) : ctx == 2u ? fmod(st, 8.0) < 1.5 : ph >= 2.0 && ph < 2.45;
        if (on != down) {
            for (i = 0; i < nn; i++)
                on ? input_on(t, nt[i], 100) : input_off(t, nt[i]);
            down = on;
        }
        pump();
        memset(out, 0, sizeof out);
        mix_block(out, CTL);
        if (b >= b0)
            for (i = 0; i < CTL; i++)
                for (c = 0; c < 2u; c++)
                    lz_kw_push(&kw, c, (double)((c ? mix_r[i] : mix_l[i]) + wet[i]) / 32767.0);
    }
    return lz_kw_db(&kw);
}

int main(int argc, char **argv)
{
    static double got[DF_N][2];
    uint32_t s, p, bad = 0, gen = argc > 1 && !strcmp(argv[1], "gen");
    for (s = 0; s < DF_N; s++) {                       /* (forked: each part from a fresh power-on) */
        pid_t pid[2];
        int fd[2][2];
        for (p = 0; p < 2u; p++) {
            if (pipe(fd[p]))
                return 2;
            fflush(stdout);
            if (!(pid[p] = fork())) {
                double r = measure(DF_SLOT[s], p);
                close(fd[p][0]);
                _exit(write(fd[p][1], &r, sizeof r) == (ssize_t)sizeof r ? 0 : 2);
            }
            close(fd[p][1]);
        }
        for (p = 0; p < 2u; p++) {
            int st = 0;
            if (read(fd[p][0], &got[s][p], sizeof got[0][0]) != (ssize_t)sizeof got[0][0])
                got[s][p] = -200.0, bad++;
            close(fd[p][0]);
            waitpid(pid[p], &st, 0);
            bad += st != 0;
        }
    }
#define JM_V(db) ((uint32_t)((db) < -37.0 ? 0 : (db) > 90.0 ? 255 : lround(2.0 * ((db) + 37.0))))
    if (gen) {
        printf("/* SPDX-License-Identifier: GPL-3.0-only */\n"
               "/* GENERATED by tests/jrny_mu.c (build/host/jrny_mu gen > firmware/src/jrny_mu.h); host_tests.sh jrnymu checks that a fresh run equals it.\n"
               " * Each factory multi's two parts as the multi plays them, in their Original's class pattern (tests/jrny_ctx.c's): one byte a part,\n"
               " * round(2 x (dB + 37)), half dB: what TRAVEL sets a destination's level to (morph_tide.c morph_to_sound). Rows in DF_SLOT order. */\n"
               "#ifndef JRNY_MU_H\n#define JRNY_MU_H\n#define JRNY_MU_N %uu\nstatic const uint8_t JRNY_MU[JRNY_MU_N][2] = {\n", (unsigned)DF_N);
        for (s = 0; s < DF_N; s++)
            printf("    {%3u, %3u},   /* M%02u: %.1f / %.1f dB */\n", JM_V(got[s][0]), JM_V(got[s][1]), (unsigned)DF_SLOT[s] + 1u, got[s][0], got[s][1]);
        printf("};\n#endif\n");
        return bad ? 1 : 0;
    }
    printf("jrnymu: %u factory multis, both parts, rendered in their class's pattern (%u failed)\n", (unsigned)DF_N, (unsigned)bad);
    if (JRNY_MU_N != DF_N) {
        printf("jrnymu: FAIL firmware/src/jrny_mu.h has %u rows, the factory multis %u: regenerate it (build/host/jrny_mu gen)\n", (unsigned)JRNY_MU_N, (unsigned)DF_N);
        return 1;
    }
    for (s = 0; s < DF_N; s++)
        for (p = 0; p < 2u; p++) {
            if (JM_V(got[s][p]) != JRNY_MU[s][p]) {
                printf("jrnymu: FAIL M%02u part %u: fresh %u, committed %u\n", (unsigned)DF_SLOT[s] + 1u, (unsigned)p + 1u, JM_V(got[s][p]), JRNY_MU[s][p]);
                bad++;
            }
            if (got[s][p] < -60.0) {
                printf("jrnymu: FAIL M%02u part %u is silent in the pattern\n", (unsigned)DF_SLOT[s] + 1u, (unsigned)p + 1u);
                bad++;
            }
        }
    printf("jrnymu: %s\n", bad ? "FAIL" : "ok (the committed table equals a fresh run)");
    return bad ? 1 : 0;
}
