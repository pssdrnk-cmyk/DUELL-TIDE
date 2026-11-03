/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tool (not firmware, not in any test list): listening WAVs for TURNS and POLY through the real firmware path (events_block, mix_block),
 * both parts, 16 bars at 124 BPM, the click as the steady reference (the TR-6S's place). Per feature a before and an after:
 *   turns_before_together.wav   / turns_after_take_turns.wav   (the variation fader at the top: the turnaround bars differ)
 *   poly_before_off.wav         / poly_after_wide.wav          (the fader at A: only the loop length differs)
 *   build: cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -DFELUCCA_VERSION=\"x\" -o build/host/tide_turns_render tests/tide_turns_render.c -lm
 *   run:   build/host/tide_turns_render OUTDIR */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

#define BPM 124u
#define BARS 16u
static const uint8_t STAB[16] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0};   /* part 1: off-beat chords (steps 7, 11, 15) */
static const uint8_t STABE[16] = {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};   /* SYNC demo: chords on beats 1 and 3 and the E of 2 and 4, to be pulled to the and */
static uint32_t g_edit, g_sync = 1;                      /* g_edit: the phrase edits run (rhythm_poll), the SYNC demo's pattern */
static const uint8_t LINE[16] = {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0};   /* part 2: a bass line against them */

static void setup(uint32_t fader, uint32_t seed)
{
    uint32_t p, i;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    gl.on = gl.armed = 0;
    transport_req = 0;
    song.playing = song.rec = 0;
    mods_off = 0;
    duell_mode = DM_STEREO;
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = BPM;
    song.g[G_SWING] = 50;
    click_mode = CLICK_ON;
    memset(sqm, 0, sizeof sqm);
    memset(&rhythm, 0, sizeof rhythm);
    for (p = 0; p < 2; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        host_preset(t, 0u, p ? 3 : 8);   /* (PLUCK for the chords, a bass for the line) */
        t->p[P_REV] = 0;
        t->p[P_CHOR] = 0;
        t->p[P_DLY] = 0;
        t->p[P_SLEN] = 16;
        t->p[P_SDIV] = 2;
        t->p[P_AMODE] = AM_OFF;
        t->p[P_SQNT] = 1;
        t->p[P_MUTE] = 0;
        t->p[P_LEVEL] = 100;
        t->p[P_SGATE] = p ? 80 : 40;
        t->seq_active = 1;
        for (i = 0; i < 16u; i++)
            if (p ? LINE[i] : (g_edit ? STABE[i] : STAB[i])) {
                t->step[i].time = ST_NOTE;
                t->step[i].n = p ? 1 : 3;
                t->step[i].note[0] = (uint8_t)(p ? 36 + (i % 4u) * 2u : 60 + (i % 3u) * 3u);
                t->step[i].note[1] = (uint8_t)(p ? 0 : t->step[i].note[0] + 4u);
                t->step[i].note[2] = (uint8_t)(p ? 0 : t->step[i].note[0] + 7u);
                t->step[i].vel = 100;
            }
    }
    sqm_vary_all = 0;
    morph.seed = (uint16_t)seed;
    for (p = 0; p < 2; p++) {
        seqm_set_amount(p, 100);
        seqm_roll(p);
        seqm_set_fader(p, fader);
    }
    seqm_poll();
}
static uint32_t render(const char *path, uint32_t turns, uint32_t poly, uint32_t fader, uint32_t seed, uint32_t write)
{
    static int32_t ob[2 * CTL];
    uint32_t n = BARS * 4u * (uint32_t)FS * 60u / BPM, t, i, short_blocks = 0, peak = 0;
    FILE *f = write ? fopen(path, "wb") : NULL;
    setup(fader, seed);
    sqm_turns = (uint8_t)turns;
    sqm_poly = (uint8_t)poly;
    sqm_auto = 3;
    sqm_sync = (uint8_t)g_sync;
    if (f)
        wav_hdr(f, n);
    transport_req = 1;
    for (t = 0; t < n; t += CTL) {
        events_block(CTL);
        sqm_auto = 3;
        if ((g_edit || turns >= 2u) && (t / CTL) % 64u == 0u) {
            if (g_edit)
                rhythm_poll(3);                /* (the main loop's pass) */
            sqm_thin_build();
        }
        if (f)
            mix_block(ob, CTL);
        short_blocks += poly_len[0] || poly_len[1];
        for (i = 0; f && i < CTL; i++) {
            uint32_t a = (uint32_t)(ob[2u * i] < 0 ? -ob[2u * i] : ob[2u * i]);
            peak = a > peak ? a : peak;
            if (f)
                wav_put(f, ob[2u * i], ob[2u * i + 1u]);
        }
    }
    if (f)
        fclose(f);
    if (f)
        printf("%s: peak %u, %u%% of the blocks with a shortened loop\n", path ? path : "(search)", peak, short_blocks * 100u / (n / CTL));
    return short_blocks;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    if (argc > 2 && !strcmp(argv[2], "answer")) {
        char q[512];
        g_edit = 1;
        g_sync = 1;
        snprintf(q, sizeof q, "%s/answer_before_take_turns.wav", dir);
        render(q, 1, 0, 127, 5, 1);
        snprintf(q, sizeof q, "%s/answer_after_echo.wav", dir);
        render(q, 3, 0, 127, 5, 1);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[2], "sync")) {
        char q[512];
        g_edit = 1;
        g_sync = 0;
        snprintf(q, sizeof q, "%s/sync_before_off.wav", dir);
        render(q, 1, 0, 0, 5, 1);
        g_sync = 2;
        snprintf(q, sizeof q, "%s/sync_after_push.wav", dir);
        render(q, 1, 0, 0, 5, 1);
        return 0;
    }
    char path[512];
    uint32_t seed, found = 0;
    /* a seed with a WIDE stretch inside the 16 bars (the first can start at the 8th loop) */
    for (seed = 1; seed < 400u && !found; seed++)
        if (render(NULL, 1, 2, 0, seed, 0) > 0u)
            found = seed;
    printf("seed %u\n", found);
    snprintf(path, sizeof path, "%s/turns_before_together.wav", dir);
    render(path, 0, 0, 127, found, 1);
    snprintf(path, sizeof path, "%s/turns_after_take_turns.wav", dir);
    render(path, 1, 0, 127, found, 1);
    snprintf(path, sizeof path, "%s/poly_before_off.wav", dir);
    render(path, 1, 0, 0, found, 1);
    snprintf(path, sizeof path, "%s/poly_after_wide.wav", dir);
    render(path, 1, 2, 0, found, 1);
    return 0;
}
