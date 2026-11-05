/* SPDX-License-Identifier: GPL-3.0-only */
/* The part EQ when a band's gain moves a step (fx.c track_eq: MORPH and TIDE move P_EQLG / P_EQMG / P_EQHG in whole dB, morph.c T_W): the new
 * coefficients used to replace the old at once, a step in the filter's response on a loud low tone (audit R15: MID -43 dB, just audible on a quiet
 * passage). They now glide over the block (four steps). On hostsim.c as eqon_test.c: a full-level 60 Hz tone (20000) through each band, the gain
 * stepped +1 dB every 100 ms from +1 to +12. The click is the largest third difference of the output in the blocks of a step (a smooth gain
 * change adds almost nothing to it: the tone's own is under 1 LSB). Before the glide: LOW 28, MID 197, HIGH 8970 LSB; after, 6, 8 and 197 (the bounds below: 16, 32, 256).
 *   build/host/eqglide_test   (host_tests.sh "eqglide") */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdio.h>
#include <math.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("eqglide: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
/* the output of one run (1.4 s) in y; the band's gain steps +1 dB at every 100 ms from the first, from +1 to +12 */
static int32_t y[2u * FS];
static void render(int band)
{
    static int32_t buf[CTL];
    uint32_t nb = (uint32_t)(1.4 * FS / CTL), k, i, t = 0, per = FS / 10u / CTL;
    track_t *tr = &trk[0];
    int pgain = band == 0 ? P_EQLG : band == 1 ? P_EQMG : P_EQHG;
    host_tracks_init();
    for (k = 0; k < nb; k++) {
        int g = 1 + (int)(k / per);
        tr->p[pgain] = (int16_t)(g > 12 ? 12 : g);
        if (band == 0)
            tr->p[P_EQLF] = 0;
        for (i = 0; i < CTL; i++, t++)
            buf[i] = (int32_t)(20000 * sin(2 * M_PI * 60.0 * t / FS));
        if (EQ_ON(tr))
            track_eq(tr, buf, CTL, 0);
        memcpy(y + k * CTL, buf, sizeof buf);
    }
}
static double d3(uint32_t i)
{
    return fabs((double)y[i] - 3.0 * y[i - 1] + 3.0 * y[i - 2] - y[i - 3]);
}
/* the click: the largest third difference in the 6 blocks from each step on (the steps at 100 ms, 200 ms ... 1.1 s), in LSB (the 60 Hz tone's own is
 * under 1; a step of the output of d gives up to 8 d) */
static double click(int band)
{
    uint32_t per = FS / 10u / CTL, k, i, nb = (uint32_t)(1.4 * FS / CTL);
    double ref = 1, worst = 0;
    render(band);
    for (i = per * CTL / 2u; i < per * CTL; i++)
        ref = d3(i) > ref ? d3(i) : ref;
    for (k = 1; k * per < nb - 6u; k++)
        for (i = k * per * CTL; i < (k * per + 6u) * CTL; i++)
            worst = d3(i) > worst ? d3(i) : worst;
    (void)ref;
    return worst;
}
int main(void)
{
    static const char *const nm[3] = {"LOW", "MID", "HIGH"};
    static const int lim[3] = {16, 32, 256};
    int b;
    for (b = 0; b < 3; b++) {
        double c = click(b);
        char w[128];
        snprintf(w, sizeof w, "%s: a 1 dB step's click, third difference %.1f LSB (before the glide 28 / 197 / 8970), at most %d", nm[b], c, lim[b]);
        check(w, c <= lim[b]);
    }
    printf(bad ? "eqglide: FAIL\n" : "eqglide: ok\n");
    return bad != 0;
}
