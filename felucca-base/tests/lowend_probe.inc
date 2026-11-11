/* SPDX-License-Identifier: GPL-3.0-only */
/* b9 low-end restore (B9-LOWEND): `gen_factory_multis lowend [SLOT ..]` measures what a multi puts under 150 Hz and when. Included by gen_factory_multis.c before main().
 * Set up like loud() (a fresh power-on, the multi's own MODE, PLAY for a stored step pattern else C4 E4 G4 held, 10 s, 2..10 s measured), and prints:
 *   under150  the share of the mono mix's energy under 150 Hz (a 4th-order Butterworth low-pass), in %
 *   K-weighted the level, as loud() prints it
 *   b1..b4    the share of the bars (16 sixteenths at the multi's own tempo, counted from the start of the transport) with a low-band onset on that beat: a sixteenth whose
 *             low-band level is more than 6 dB over the sixteenth before it and within 30 dB of the loudest one
 *   other16ths the same share averaged over the twelve other sixteenths
 * The drum rule (the only one on the low end): no part is a kick on every beat. b1..b4 all near 1.00 with a loud under150 share is a kick; a bass line is uneven, a pad has none. */
typedef struct { double b0, b1, b2, a1, a2, x1, x2, y1, y2; } lo_bq_t;
static void lo_lp(lo_bq_t *f, double fc, double q, double fs)
{
    double w = 2.0 * M_PI * fc / fs, c = cos(w), al = sin(w) / (2.0 * q), a0 = 1.0 + al;
    memset(f, 0, sizeof *f);
    f->b0 = (1.0 - c) / 2.0 / a0;
    f->b1 = (1.0 - c) / a0;
    f->b2 = f->b0;
    f->a1 = -2.0 * c / a0;
    f->a2 = (1.0 - al) / a0;
}
static double lo_run(lo_bq_t *f, double x)
{
    double y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1, f->x1 = x, f->y2 = f->y1, f->y1 = y;
    return y;
}
#define LO_WIN 2048u
static double lo_env[LO_WIN];
static void lowend(uint32_t k, uint32_t nkeys)
{
    static const uint8_t KEYS[3] = {60, 64, 67};
    uint32_t kmask = 0, b, i, c, nb = (uint32_t)(10.0 * FS) / CTL, b0 = (uint32_t)(2.0 * FS) / CTL, pat = 0, n = 0, w = 0, wn = 0, nw, nbars, bar, s;
    double e = 0, elow = 0, efull = 0, wacc = 0, lmax = 1e-30, bpm, slot[16], other = 0;
    bq_t sh[2], hp[2];
    lo_bq_t l1, l2;
    int32_t out[2 * CTL];
    ui_power_on();
    if (!project_factory(k))
        return;
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    for (i = 0; i < nkeys; i++)
        kmask |= 1u << key_of(KEYS[i]);
    project_load(k);
    song.master_q12 = 2048;
    bpm = song.g[G_BPM] ? (double)song.g[G_BPM] : 120.0;
    nw = (uint32_t)(FS * 60.0 / bpm / 4.0);
    for (c = 0; c < 2u; c++)
        for (i = 0; i < NSTEP; i++)
            pat |= trk[c].step[i].time == ST_NOTE && trk[c].step[i].n && trk[c].p[P_AMODE] == 0;
    kw_init(&sh[0], &hp[0], FS);
    sh[1] = sh[0];
    hp[1] = hp[0];
    lo_lp(&l1, 150.0, 0.5412, FS);
    lo_lp(&l2, 150.0, 1.3066, FS);
    for (i = 0; i < 8u; i++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
    }
    if (pat)
        transport_req = 1;
    else
        fm1_in.notes = kmask;
    memset(lo_env, 0, sizeof lo_env);
    for (b = 0; b < nb; b++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
        for (i = 0; i < CTL; i++) {
            double m = (out[2u * i] + out[2u * i + 1u]) / 65534.0, lo = lo_run(&l2, lo_run(&l1, m));
            wacc += lo * lo;
            if (++wn >= nw) {
                if (w < LO_WIN)
                    lo_env[w++] = wacc / nw;
                wacc = 0, wn = 0;
            }
            if (b >= b0) {
                elow += lo * lo;
                efull += m * m;
                n++;
            }
        }
        if (b >= b0)
            for (i = 0; i < CTL; i++)
                for (c = 0; c < 2u; c++) {
                    double y = bq(&hp[c], bq(&sh[c], out[2u * i + c] / 32767.0));
                    e += y * y;
                }
    }
    fm1_in.notes = 0;
    for (i = 0; i < w; i++) {
        lmax = lo_env[i] > lmax ? lo_env[i] : lmax;
    }
    nbars = w / 16u;
    for (s = 0; s < 16u; s++)
        slot[s] = 0;
    for (bar = 0; bar < nbars; bar++)
        for (s = 0; s < 16u; s++) {
            uint32_t at = bar * 16u + s;
            double cur = lo_env[at], prev = at ? lo_env[at - 1u] : 0.0;
            if (cur > 4.0 * prev && cur > lmax * 1e-3 && cur > 1e-12)    /* (+6 dB over the sixteenth before, within 30 dB of the loudest) */
                slot[s] += 1.0;
        }
    for (s = 0; s < 16u; s++)
        slot[s] = nbars ? slot[s] / nbars : 0.0;
    for (s = 0; s < 16u; s++)
        if (s % 4u)
            other += slot[s];
    printf("M%02u %-12s %-9s under150 %5.1f %%  K-weighted %6.1f dB  low onsets per beat: b1 %.2f b2 %.2f b3 %.2f b4 %.2f  other16ths %.2f\n", k + 1u, mu_names[k],
           pat ? "PLAY" : nkeys == 1u ? "C4" : "C4 E4 G4", efull > 0 ? 100.0 * elow / efull : 0.0, -0.691 + 10 * log10(e / n + 1e-30), slot[0], slot[4], slot[8], slot[12],
           other / 12.0);
}
