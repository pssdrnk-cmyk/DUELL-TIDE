/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE JOURNEY's context table (audit 2026-11-09 finding 1, brief J1): every factory sound's level and brightness as it plays in
 * one fixed pattern, so JOURNEY can travel to sounds near the one it leaves (tide_ui.c tide_journey, jctx_near). A sound's TRIM
 * levels it on one held note; in a pattern a bell rings through the gaps where a stab does not, and the factory sounds of one
 * role family then differ by up to 9 dB (M02 DUB CHORD under STYLE DUB moved 14.6 dB over 128 bars, the audit's tools/tide_long.c).
 * The context: part 1 alone, STEREO, the sound as loaded (host_ds_load), 120 BPM, 4 bars, velocity 100, by the class JOURNEY keeps: a
 * stab or keys sound plays a chord C3 E3 G3 on each off-beat 16th at 45 % gate (the factory dub stab: M01, M02, M05); a pad (ATK 60 or more)
 * the chord on beats 1 and 3, held 1.5 beats; a BASS-category sound C2 on beats 1 and 3, held 1.5 16ths. Measured over bars 2..4 (2 s .. 8 s) on the mix before MASTER:
 *   level       K-weighted (tests/loudness.h lz_kw: BS.1770's pre-filter and RLB high-pass, no gate), as the audit's tide_long and
 *               tideroles 4.4 read a multi, in half dB:
 *               L = round(dB + 37), 1 dB steps, clamped to 0..31 (the 171 sounds span -36.5 .. -5.5 dB)
 *   brightness  the RMS frequency (sqrt(diff energy / energy), Hz, as loudness.h): B = round(log2(Hz) - 6), octaves from 64 Hz, 0..7
 *   one byte a sound: L << 3 | B (171 B of flash)
 * The firmware keeps the result as firmware/src/jrny_ctx.h, in DS_SOUNDS order (build/gen/duell_sounds.h: engine by engine).
 *   check (host_tests.sh jrnyctx): a fresh run must equal the committed table, byte for byte; the rows must be DS_N
 *   write:  build/host/jrny_ctx gen > firmware/src/jrny_ctx.h   (after adding, removing or re-voicing a factory sound)
 * Needs fork (Linux / WSL / macOS). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/category.c"
#include "loudness.h"
#include "../firmware/src/jrny_ctx.h"

#define JC_BPM 120.0
#define JC_BARS 4u
#define JC_JOBS 12u
typedef struct { uint8_t v; double db, hz; } jc_t;   /* v: the byte (L << 3 | B) */

/* the main loop between the audio blocks (main.c): a WAVE table build waits on it (tests/jrny_pair_audit.c pump) */
static void pump(void)
{
    static uint32_t n;
    fm1_ms += 3u;
    while (wave_service())
        ;
    if (!(++n & 7u))
        morph_xpoll();
}
static jc_t measure(uint32_t e, uint32_t k)
{
    static const uint8_t CH[3] = {48, 52, 55}, BASS[1] = {36};
    const double bar = 4.0 * 60.0 / JC_BPM;
    const uint8_t *nt;
    uint32_t nn, ctx, b, i, c, nb = (uint32_t)(JC_BARS * bar * FS) / CTL, b0 = (uint32_t)(bar * FS) / CTL, down = 0;
    int32_t out[2 * CTL];
    double px[2] = {0}, re = 0, de = 0, db, hz;
    lz_kw_t kw;
    jc_t r;
    host_tracks_init();
    song.g[G_RTYPE] = GP[G_RTYPE].def;
    duell_mode_force(DM_STEREO);
    host_ds_load(&trk[0], e, k);
    trk[0].p[P_CHRD] = 0;
    morph_ready(0);
    nt = ds_cat_of(e, k) == CAT_BASS ? BASS : CH;
    nn = nt == BASS ? 1u : 3u;
    ctx = nt == BASS ? 2u : ds_value(ds_get(e, k), P_ATK, TP[P_ATK].def) >= 60 ? 1u : 0u;   /* (the class JOURNEY keeps, tide_ui.c tide_cls: a pad's own context) */
    lz_kw_init(&kw);
    for (b = 0; b < nb; b++) {
        double t = (double)b * CTL / FS, beat = fmod(t, bar) / (bar / 4.0);   /* (the beat inside the bar, 0..4) */
        double st = beat * 4.0, ph = fmod(st, 4.0);   /* (the 16th step, and where in its beat) */
        uint32_t on = ctx == 1u ? (beat < 1.5) || (beat >= 2.0 && beat < 3.5)          /* PAD: held chords on 1 and 3, 1.5 beats */
                    : ctx == 2u ? fmod(st, 8.0) < 1.5                                  /* BASS: C2 on 1 and 3, 1.5 16ths */
                                : ph >= 2.0 && ph < 2.45;                              /* else: the dub stab, the off-beat 16th at 45 % gate (M01, M02, M05) */
        if (on != down) {
            for (i = 0; i < nn; i++)
                on ? input_on(&trk[0], nt[i], 100) : input_off(&trk[0], nt[i]);
            down = on;
        }
        pump();
        mix_block(out, CTL);
        for (i = 0; i < CTL; i++)
            for (c = 0; c < 2u; c++) {
                double x = (double)((c ? mix_r[i] : mix_l[i]) + wet[i]);
                if (b >= b0) {
                    lz_kw_push(&kw, c, x / 32767.0);
                    re += x * x;
                    de += (x - px[c]) * (x - px[c]);
                }
                px[c] = x;
            }
    }
    db = lz_kw_db(&kw);
    hz = re > 0 ? sqrt(de / re) * FS / (2 * M_PI) : 0;
    {
        long l = lround(db + 37.0), o = hz < 1.0 ? 0 : lround(log2(hz) - 6.0);
        r.v = (uint8_t)((l < 0 ? 0 : l > 31 ? 31 : l) << 3 | (o < 0 ? 0 : o > 7 ? 7 : o));
        r.db = db, r.hz = hz;
    }
    return r;
}

int main(int argc, char **argv)
{
    static jc_t got[DS_N];
    uint32_t e, k, n = 0, i, j, bad = 0, gen = argc > 1 && !strcmp(argv[1], "gen");
    uint32_t qe[DS_N], qk[DS_N];
    for (e = 0; e < NENGINES; e++)
        for (k = 0; k < ds_count(e); k++)
            qe[DS_ENG_AT[e] + k] = e, qk[DS_ENG_AT[e] + k] = k, n++;   /* (the row is the sound's DS_SOUNDS index: tide_ui.c reads it so) */
    for (i = 0; i < n; i += JC_JOBS) {                   /* (forked in batches: each sound from a fresh state) */
        pid_t pid[JC_JOBS];
        int fd[JC_JOBS][2];
        for (j = 0; j < JC_JOBS && i + j < n; j++) {
            if (pipe(fd[j]))
                return 2;
            fflush(stdout);
            if (!(pid[j] = fork())) {
                jc_t r = measure(qe[i + j], qk[i + j]);
                close(fd[j][0]);
                _exit(write(fd[j][1], &r, sizeof r) == (ssize_t)sizeof r ? 0 : 2);
            }
            close(fd[j][1]);
        }
        for (j = 0; j < JC_JOBS && i + j < n; j++) {
            int st = 0;
            if (read(fd[j][0], &got[i + j], sizeof got[0]) != (ssize_t)sizeof got[0])
                got[i + j].v = 0, got[i + j].db = -200.0, bad++;
            close(fd[j][0]);
            waitpid(pid[j], &st, 0);
            bad += st != 0;
        }
    }
    if (gen) {
        printf("/* SPDX-License-Identifier: GPL-3.0-only */\n"
               "/* GENERATED by tests/jrny_ctx.c (build/host/jrny_ctx gen > firmware/src/jrny_ctx.h); host_tests.sh jrnyctx checks that a fresh run equals it.\n"
               " * Each factory sound in DS_SOUNDS order (engine by engine), one byte L << 3 | B: its level (L = dBFS + 37, 0..31) and brightness\n"
               " * (B = log2 Hz - 6, 0..7) in its class's pattern (tests/jrny_ctx.c): what TIDE JOURNEY reads to stay near the Original (tide_ui.c jctx_near). */\n"
               "#ifndef JRNY_CTX_H\n#define JRNY_CTX_H\nstatic const uint8_t JRNY_CTX[%u] = {\n", (unsigned)n);
        for (i = 0; i < n; i++)
            printf("    %3u,   /* %u %u %s: %.1f dB, %.0f Hz */\n", got[i].v, (unsigned)qe[i], (unsigned)qk[i], ds_name(qe[i], qk[i]), got[i].db, got[i].hz);
        printf("};\n#endif\n");
        return bad ? 1 : 0;
    }
    printf("jrnyctx: %u factory sounds rendered in the chord pattern (%u failed)\n", (unsigned)n, (unsigned)bad);
    if (n != (uint32_t)(sizeof JRNY_CTX / sizeof JRNY_CTX[0])) {
        printf("jrnyctx: FAIL firmware/src/jrny_ctx.h has %u rows, the sound list %u: regenerate it (build/host/jrny_ctx gen)\n",
               (unsigned)(sizeof JRNY_CTX / sizeof JRNY_CTX[0]), (unsigned)n);
        return 1;
    }
    for (i = 0; i < n; i++)
        if (got[i].v != JRNY_CTX[i]) {
            printf("jrnyctx: FAIL %s: fresh %u, committed %u\n", ds_name(qe[i], qk[i]), got[i].v, JRNY_CTX[i]);
            bad++;
        }
    for (i = 0; i < n; i++)
        if (got[i].db < -60.0) {
            printf("jrnyctx: FAIL %s is silent in the pattern\n", ds_name(qe[i], qk[i]));
            bad++;
        }
    printf("jrnyctx: %s\n", bad ? "FAIL" : "ok (the committed table equals a fresh run)");
    return bad ? 1 : 0;
}
