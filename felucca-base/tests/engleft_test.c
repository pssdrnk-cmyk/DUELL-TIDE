/* SPDX-License-Identifier: GPL-3.0-only */
/* Small engine fixes of lane P / Q / S of the 2026-11-04 audit that need no test of their own. build/host/engleft_test (host_tests.sh engleft):
 *   P13  the velocity accent in UNISON (core.h ACCENT_VEL): the accent reads the key's velocity; UNISON lowers the voice's own velocity (vel * 36 / 100), which never
 *        reached the accent's 111, so a hard key never opened the filter there */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("engleft: %-110s %s\n", what, ok ? "ok" : "FAIL");
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
static track_t *fresh(const char *eng)
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
        host_preset(&trk[k], k ? 0u : eng_by_name(eng), 0);
    for (k = 1; k < NPART; k++)
        trk[k].p[P_MUTE] = 1;
    song.sel = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    return t;
}

/* P13: the velocity the accent reads (voice.c track_render, voice_chain.c, chain_f2.c all use ACCENT_VEL) for a key at vel */
static uint32_t accent_vel(uint32_t mode, uint32_t vel, uint32_t *own)
{
    track_t *t = fresh("ANALOG");
    t->p[P_VOICE] = (int16_t)mode;
    trk_note_on(t, 48, vel);
    *own = t->v[0].vel;
    return ACCENT_VEL(&t->v[0]);
}

int main(void)
{
    uint32_t own, v;
    char s[200];
    v = accent_vel(V_MONO, 127, &own);
    snprintf(s, sizeof s, "P13 MONO, key 127: the voice's vel %u, the accent reads %u (above 110)", own, v);
    check(s, own == 127u && v == 127u);
    v = accent_vel(V_UNISON, 127, &own);
    snprintf(s, sizeof s, "P13 UNISON, key 127: the voice's vel %u (lowered for the level), the accent reads %u (above 110)", own, v);
    check(s, own < 111u && v == 127u);
    v = accent_vel(V_UNISON, 100, &own);
    snprintf(s, sizeof s, "P13 UNISON, key 100: the accent reads %u (no accent)", v);
    check(s, v == 100u);
    printf("engleft: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
