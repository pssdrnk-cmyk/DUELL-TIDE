/* SPDX-License-Identifier: GPL-3.0-only */
/* FM6's exp2 lookup (fm6_core.c fm6_mant / fm6_exp2, the table FM6_EXP2 from tools/gen_tables.py) against the double-precision exp2, over the whole input range
 * (b11 room lane, item 10): the table has 513 entries (a point every 2^15 of the Q24 fraction) with the interpolation between them; the worst relative error of the
 * mantissa must stay under 1e-6 (0.03 LSB at full scale). Prints the worst error, the input it sits at, and the same for fm6_exp2 over its integer range.
 *   build/host/fm6_exp2_test            (host_tests.sh "fm6exp2") */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define EXP2_LIMIT 1e-6

int main(void)
{
    double worst = 0, worst_e = 0;
    uint32_t at = 0;
    int32_t at_e = 0;
    uint32_t f;
    int32_t x;
    int bad = 0;
    for (f = 0; f < (1u << 24); f++) {                       /* every Q24 fraction: the mantissa 2^(f / 2^24) as Q30 */
        double ex = ldexp(exp2((double)f / 16777216.0), 30);
        double got = (double)fm6_mant((int32_t)f);
        double e = fabs(got - ex) / ex;
        if (e > worst) {
            worst = e;
            at = f;
        }
    }
    for (x = 0; x < (6 << 24); x += 4099) {        /* fm6_exp2: Q24 in, Q24 out, up to 2^6 (above it saturates by design: fm6_exp2 shifts at most 2^5) */
        double ex = ldexp(exp2((double)x / 16777216.0), 24);
        double got;
        double e;
        if (ex >= 2147483647.0)
            continue;
        got = (double)fm6_exp2(x);
        if (ex < 16777216.0)                                    /* (below 1.0 the Q24 output has too few bits for a relative figure: the rounding alone is 6e-8 at 1.0, more below) */
            continue;
        e = fabs(got - ex) / ex;
        if (e > worst_e) {
            worst_e = e;
            at_e = x;
        }
    }
    printf("fm6_exp2: table %u entries; mantissa worst relative error %.3g at f=%u (limit %.0e); fm6_exp2 worst %.3g at x=%d\n", (unsigned)(sizeof FM6_EXP2 / sizeof FM6_EXP2[0]), worst, at, EXP2_LIMIT, worst_e, at_e);
    if (worst >= EXP2_LIMIT) {
        printf("fm6_exp2: FAIL mantissa error %.3g over the limit\n", worst);
        bad = 1;
    }
    if (worst_e >= EXP2_LIMIT) {
        printf("fm6_exp2: FAIL fm6_exp2 error %.3g over the limit\n", worst_e);
        bad = 1;
    }
    printf("fm6_exp2: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
