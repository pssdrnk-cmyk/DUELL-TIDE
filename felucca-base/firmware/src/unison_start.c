/* SPDX-License-Identifier: GPL-3.0-only */
/* After SLOOP 2.8.4 / Clipper 1.3 (isod89): spread a fresh unison voice by one
 * time offset, keeping its oscillators' relative timing. Called at note start,
 * never per sample; sounding voices keep their phase on a re-strike. */
static uint32_t uni_phase(uint32_t r, uint32_t base, uint32_t rate)
{
    return base ? (uint32_t)(((uint64_t)r * rate) / base) : r;
}

static void unison_spread(track_t *t, voice_t *v, uint32_t r)
{
    const engine_t *e = ENGINES[t->engine];
    const int16_t *p = t->p;
    uint32_t base = pitch_inc((uint32_t)clamp(v->pitch16, 0, 2047));
    if (e == &ENG_FM6) {
        fm6_note_t *nt = fm6_note_of(t, v);
        uint32_t tr = (uint32_t)(t - trk), k;
        int32_t lf, f0;
        if (!nt || tr >= NLIVE) return;
        lf = fm6_note_logfreq(v->pitch16 + (fm6_eff[tr].p[FP_TRNSP] - 24) * 16, v->fine);
        f0 = fm6_freq(lf);
        if (f0 <= 0) return;
        for (k = 0; k < 6u; k++) {
            int32_t fk = fm6_freq((nt->fixed >> k) & 1u ? nt->base[k] : lf + nt->base[k]);
            nt->op[k].phase = (int32_t)((uint32_t)nt->op[k].phase + uni_phase(r >> 8, (uint32_t)f0, (uint32_t)fk));
        }
        return;
    }
    v->ph[0] += r;
    if (e == &ENG_ANALOG) {
        uint32_t rate = cents_inc(v->pitch16, p[P_E1], 0);
        if (p[P_E0] == AW_SUB) rate >>= 1;
        else if (p[P_E0] == AW_SYNC)
            rate = pitch_inc((uint32_t)clamp(v->pitch16 + p[P_E1] * 3, 0, 2047));
        v->ph[1] += uni_phase(r, base, rate);
    } else if (e == &ENG_TRIO) {
        v->ph[1] += uni_phase(r, base, cents_inc(v->pitch16 + p[P_E1] * 16, p[P_E3], 0));
        v->ph[2] += uni_phase(r, base, cents_inc(v->pitch16 + p[P_E2] * 16, -p[P_E3], 0));
    } else if (e == &ENG_PHASE) {
        v->ph[1] += uni_phase(r, base, pitch_inc((uint32_t)clamp(v->pitch16 + p[P_E4] * 16 / 100, 0, 2047)));
        v->ph[2] += r >> 1;
    } else if (e == &ENG_WAVE) {
        v->ph[1] += r;
        v->s[4] = (int32_t)v->ph[0];
        v->s[5] = (int32_t)v->ph[1];
    } else if (e == &ENG_NOISE || e == &ENG_LOFI) {
        v->s[2] = (int32_t)(((uint32_t)v->s[2] ^ r) & (e == &ENG_LOFI ? 0x7FFFu : 0xFFFFFFFFu));
        if (!v->s[2]) v->s[2] = 1;
    }
}
