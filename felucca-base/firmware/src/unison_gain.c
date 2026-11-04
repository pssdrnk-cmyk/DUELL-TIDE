/* SPDX-License-Identifier: GPL-3.0-only */
/* FM6 owns its amplitude: normalize the voices actually running, once per block.
 * Keep the installed six-voice gain (2/5); one remaining voice uses ordinary unity; 7 and 8 voices go on by 1 / sqrt(n). */
/* the amplitude of n unison voices a note, n = 0..8, as a share of VOICE_FS (FM6 normalises by the voices running: below; voice.c mono_play gives PHYS the same by its UNISON voices) */
static const uint16_t unison_amp[FM6_POLY + 1u] = {VOICE_FS,VOICE_FS,VOICE_FS*7071/10000,VOICE_FS*5774/10000,VOICE_FS/2,VOICE_FS*4472/10000,VOICE_FS*2/5,VOICE_FS*378/1000,VOICE_FS*3536/10000};
static __attribute__((always_inline)) inline uint16_t fm6_unison_gain(const track_t *t)
{
    uint32_t n = 0;
    if (t->p[P_VOICE] != V_UNISON) return VOICE_FS;
    for (uint32_t i = 0; i < FM6_POLY; i++) n += !!(t->v[i].active && t->v[i].stage);
    return unison_amp[n];
}

/* Keep the scan and cache write out of the pinned RAM block. */
static __attribute__((noinline)) void fm6_unison_sync(const track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk);
    if (tr < NLIVE) fm6_eff[tr].ug = fm6_unison_gain(t);
}
