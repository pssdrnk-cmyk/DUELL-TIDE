/* SPDX-License-Identifier: GPL-3.0-only */
/* B15 fx lane: the send buses (chorus, delay, reverb) and the master, timed and rendered on the host with the effect settings of M07 HSE CHICAGO
 * and M19 ELEC CRISP (tools/factory_multis.py slots 5 and 6), on music-like sends (stabs, a bass, a pad, a loud stretch, silence for the tails).
 *   fx_cost_bench SCENE OUT.raw      SCENE 7 or 19; OUT.raw: the master's output (int32 L R, the whole render) for the before / after compare
 * Prints the host cycles a block (rdtsc, median and 95th percentile) of mix_sends (the buses), the reverb's share (a second pass with the
 * reverb's send at 0) and mix_master. Not a CI test: the lane's measuring tool (work/b15/cpu/fx/REPORT.md) */
#define main hostsim_main
#include "hostsim.c"
#undef main
static inline uint64_t rdtsc_(void) { uint32_t lo, hi; asm volatile("lfence; rdtsc" : "=a"(lo), "=d"(hi) :: "memory"); return (uint64_t)hi << 32 | lo; }

static uint32_t xs = 0x2545F491u;
static int32_t rnd(void) { xs ^= xs << 13; xs ^= xs >> 17; xs ^= xs << 5; return (int32_t)xs; }

typedef struct { double ph[4], inc[4], env; } osc_t;
static void trig(osc_t *v, const double *hz, int k) { for (int j = 0; j < 4; j++) { v->inc[j] = j < k ? hz[j] / FS : 0; } v->env = 1.0; }
static double saw(osc_t *v, double dec)
{
    double s = 0;
    for (int j = 0; j < 4; j++) {
        if (v->inc[j] == 0) continue;
        v->ph[j] += v->inc[j];
        if (v->ph[j] >= 1) v->ph[j] -= 1;
        s += 2 * v->ph[j] - 1;
    }
    v->env *= dec;
    return s * v->env;
}

#define NSTAT 3
static uint64_t *tim[NSTAT];
static int cmp64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }
static void stat(const char *what, uint64_t *t, uint32_t nb)
{
    uint64_t s = 0;
    for (uint32_t i = 0; i < nb; i++) s += t[i];
    qsort(t, nb, sizeof t[0], cmp64);
    printf("  %-26s mean %7.0f  median %7llu  p95 %7llu  max %7llu cycles/block\n", what, (double)s / nb,
           (unsigned long long)t[nb / 2], (unsigned long long)t[nb * 95 / 100], (unsigned long long)t[nb - 1]);
}

static void setup(int scene)
{
    host_tracks_init();
    rev_clear();
    if (scene == 7) {                                   /* M07 HSE CHICAGO: 123 BPM, 8D delay FDBK 40 MIX 60 DIGI COLR 30, ROOM SIZE 50 DAMP 61 */
        song.g[G_BPM] = 123, song.g[G_DTIME] = 15, song.g[G_DFDBK] = 40, song.g[G_DMIX] = 60, song.g[G_DMODE] = DMD_DIGITAL, song.g[G_DCOLOR] = 30;
        song.g[G_RTYPE] = 0, song.g[G_RSIZE] = 50, song.g[G_RDAMP] = 61;
    } else {                                            /* M19 ELEC CRISP: 125 BPM, 1/8 PING FDBK 40 MIX 64 COLR 30, HALL SIZE 96 DAMP 64 */
        song.g[G_BPM] = 125, song.g[G_DTIME] = 1, song.g[G_DFDBK] = 40, song.g[G_DMIX] = 64, song.g[G_DMODE] = DMD_PING, song.g[G_DCOLOR] = 30;
        song.g[G_RTYPE] = 2, song.g[G_RSIZE] = 96, song.g[G_RDAMP] = 64;
    }
    if (getenv("CRATE"))                                /* (the chorus's RATE / DEPTH: the multis keep the defaults; CRATE=127 CDEPTH=127: the worst case of the control-rate LFO) */
        song.g[G_CRATE] = (int16_t)atoi(getenv("CRATE"));
    if (getenv("CDEPTH"))
        song.g[G_CDEPTH] = (int16_t)atoi(getenv("CDEPTH"));
    if (getenv("FX_SHOW") && scene)
        printf("  chorus RATE %d DEPTH %d, LOW CUT %d, delay COLR %d\n", song.g[G_CRATE], song.g[G_CDEPTH], song.g[G_RLOCUT], song.g[G_DCOLOR]);
}

/* one render: secs seconds; nor: the reverb's send at 0 (its cost by difference); out: the master output (or NULL) */
static void render(int scene, uint32_t secs, int nor, int noc, FILE *out, uint32_t *nbp)
{
    static int32_t o[2 * CTL];
    osc_t pia = {0}, bas = {0}, pad = {0};
    const double c7[2][4] = {{196.0, 233.08, 261.63, 311.13}, {207.65, 261.63, 311.13, 349.23}};   /* (M07: G Bb C Eb / Ab C Eb F) */
    const double b7[4] = {65.41, 130.81, 65.41, 116.54};
    const double pd[2][4] = {{349.23, 440.0, 523.25, 659.26}, {233.08, 293.66, 349.23, 440.0}};
    double bpm = scene == 7 ? 123 : 125, s16 = FS * 60.0 / bpm / 4;
    uint32_t nb = secs * FS / CTL, b, i, smp = 0;
    setup(scene);
    for (b = 0; b < nb; b++) {
        double loud = (b * CTL >= 8u * FS && b * CTL < 12u * FS) ? 2.6 : 1.0;   /* 8 .. 12 s: loud, into the limiter */
        int quiet = b * CTL >= (secs - 6u) * FS;                                  /* the last 6 s: silence, the tails ring out and stop */
        for (i = 0; i < CTL; i++, smp++) {
            uint32_t st = (uint32_t)(smp / s16), ph = (uint32_t)fmod(smp, s16);
            if (!quiet && ph == 0) {
                uint32_t bar = (st / 16) & 1;
                if ((st & 3) == 2)
                    trig(&pia, c7[bar], 4);
                if ((st & 3) != 0 && (st * 7 % 5) != 0)
                    trig(&bas, &b7[st & 3], 1);
                if ((st & 31) == 0)
                    trig(&pad, pd[(st / 32) & 1], 4);
            }
            double p = saw(&pia, 0.99985) * 2600 * loud, q = saw(&bas, 0.9996) * 5200 * loud, r = saw(&pad, 0.999995) * 1500 * loud;
            int32_t ip = (int32_t)p, iq = (int32_t)q, ir = (int32_t)r, nz = rnd() >> 26;   /* (a little noise: the low bits move) */
            if (quiet) ip = iq = ir = nz = 0;
            if (scene == 7) {                           /* the sends in Q15 of a part's level: piano CHOR 40 DLY 34 REV 26 (SPREAD 50), bass REV 6 */
                mix_l[i] = ip + (ip >> 2) + iq + nz, mix_r[i] = ip - (ip >> 2) + iq - nz;
                send_c[i] = noc ? 0 : ip * 40 / 127, send_d[i] = ip * 34 / 127, send_r[i] = nor ? 0 : ip * 26 / 127 + iq * 6 / 127;
            } else {                                    /* M19: bass CHOR 20 DLY 40 REV 20, strings CHOR 80 DLY 20 REV 70 */
                mix_l[i] = iq + ir + (ir >> 2) + nz, mix_r[i] = iq + ir - (ir >> 2) - nz;
                send_c[i] = noc ? 0 : iq * 20 / 127 + ir * 80 / 127, send_d[i] = iq * 40 / 127 + ir * 20 / 127, send_r[i] = nor ? 0 : iq * 20 / 127 + ir * 70 / 127;
            }
        }
        uint64_t t0 = rdtsc_();
        mix_sends(CTL);
        uint64_t t1 = rdtsc_();
        mix_master(o, CTL, 0, MIX_MG());
        uint64_t t2 = rdtsc_();
        tim[0][b] = t1 - t0, tim[1][b] = t2 - t1, tim[2][b] = t2 - t0;
        if (out)
            fwrite(o, sizeof o[0], 2 * CTL, out);
    }
    *nbp = nb;
}

int main(int argc, char **argv)
{
    int scene = argc > 1 ? atoi(argv[1]) : 7;
    uint32_t secs = 30, nb, k;
    FILE *f = argc > 2 ? fopen(argv[2], "wb") : NULL;
    for (k = 0; k < NSTAT; k++)
        tim[k] = calloc(secs * FS / CTL + 1, sizeof(uint64_t));
    printf("fx_cost_bench: M%02d, %u s, host cycles a block of %u\n", scene, secs, CTL);
    render(scene, secs, 0, 0, f, &nb);
    if (f) fclose(f);
    stat("sends (mix_sends)", tim[0], nb);
    stat("master (mix_master)", tim[1], nb);
    stat("sends + master", tim[2], nb);
    render(scene, secs, 1, 0, NULL, &nb);
    stat("sends, reverb send 0", tim[0], nb);
    render(scene, secs, 1, 1, NULL, &nb);
    stat("sends, rev+chorus 0", tim[0], nb);
    return 0;
}
