/* SPDX-License-Identifier: GPL-3.0-only */
/* Pitch modulation below the 1/16 semitone (lane P of the 2026-11-04 audit, P02; voice.c track_render, mod.c mod_voice_add / mod_pit),
 * same sources as the firmware (hostsim.c). build/host/pitchmod_test (host_tests.sh pitchmod):
 *   ENV PIT   ANALOG's sine held at a full sustain, ENV PIT depth 1 / 2 / 3 / 5: the frequency is within 1.5 cents of the
 *             depth's own offset (18.75 ct a step; it used to be whole 1/16 semitones, floored: 12.5 / 31.3 / 43.8 / 87.5 ct)
 *   MATRIX    the same offset through a matrix slot (ENV -> PITCH, amount 1 / 2 / 3 / 5): the same cents
 *   LFO       LFO 1 PIT depth 1, a sine: the mean pitch over 20 whole cycles is centred (within 1 cent), not 3.5 ct flat */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("pitchmod: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static int32_t o[2 * CTL];
static track_t *fresh(void)
{
    uint32_t k;
    track_t *t = &trk[0];
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    vage = 0;
    rng_state = 0x1234567u;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : eng_by_name("ANALOG"), 0);
    for (k = 1; k < NPART; k++)
        trk[k].p[P_MUTE] = 1;
    song.sel = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_KTRK] = 0;
    t->p[P_VOICE] = V_MONO;
    t->p[P_LD_PIT] = t->p[P_ED_PIT] = t->p[P_L2PIT] = 0;
    t->p[P_E0] = 3;                                   /* SIN, OSC A alone */
    t->p[P_E1] = t->p[P_E2] = t->p[P_E3] = t->p[P_E5] = t->p[P_E6] = t->p[P_E7] = 0;
    t->p[P_E4] = 127;
    return t;
}
/* the frequency over blocks b0..b1 of the held note 60 (rising zero crossings), in cents from 261.626 Hz */
static double cents_of(track_t *t, uint32_t skip, uint32_t n)
{
    uint32_t b, i, cross = 0;
    double first = 0, last = 0, x0 = 0, at, got;
    uint64_t pos = 0;
    trk_note_on(t, 60, 127);
    for (b = 0; b < skip; b++)
        mix_block(o, CTL);
    for (b = 0; b < n; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++, pos++) {
            double x = o[2u * i];
            if (x0 < 0 && x >= 0) {
                at = (double)pos - 1.0 + x0 / (x0 - x);
                if (!cross)
                    first = at;
                last = at;
                cross++;
            }
            x0 = x;
        }
    }
    got = (cross - 1u) * (double)FS / (last - first);
    return 1200.0 * log2(got / (440.0 * pow(2.0, -9.0 / 12.0)));
}
static double unmod;

static void env_pit(void)
{
    static const int D[4] = {1, 2, 3, 5};
    uint32_t i, route;
    for (route = 0; route < 2u; route++)
        for (i = 0; i < 4u; i++) {
            track_t *t = fresh();
            double want = 18.75 * D[i], got;
            char s[128];
            if (route) {
                t->p[P_M1SRC] = MS_ENV;
                t->p[P_M1DST] = MD_PITCH;
                t->p[P_M1AMT] = (int16_t)D[i];
            } else
                t->p[P_ED_PIT] = (int16_t)D[i];
            got = cents_of(t, 20, 400) - unmod;
            snprintf(s, sizeof s, "%s depth %d: %+.2f cents (%.2f)", route ? "MATRIX ENV -> PITCH" : "ENV PIT", D[i], got, want);
            check(s, got > want - 1.5 && got < want + 1.5);
        }
}

static void lfo(void)
{
    track_t *t = fresh();
    double hz = LFO_HZ_X100[70] / 100.0, got;
    uint32_t n = (uint32_t)(20.0 * FS / CTL / hz + 0.5);
    char s[128];
    t->p[P_LD_PIT] = 1;
    t->p[P_LWAVE] = 0;
    t->p[P_LRATE] = 70;
    got = cents_of(t, 0, n) - unmod;
    snprintf(s, sizeof s, "LFO PIT depth 1 (sine, %.1f Hz, 20 cycles): mean %+.2f cents (was -3.5 flat)", hz, got);
    check(s, got > -1.0 && got < 1.0);
}

int main(void)
{
    track_t *t = fresh();
    unmod = cents_of(t, 20, 400);
    env_pit();
    lfo();
    printf("pitchmod: %s (%d failed)\n", bad ? "FAIL" : "all ok", bad);
    return bad != 0;
}
