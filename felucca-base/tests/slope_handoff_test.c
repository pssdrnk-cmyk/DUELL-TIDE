/* SPDX-License-Identifier: GPL-3.0-only */
/* ANALOG filter SLOPE handoff (voice.c uf_run): when a morph has already faded the second stage out (v->msl), reaching
 * the 12 dB end must not start the SLOPE fade again. From JAM's reliability_test.c slope_handoff (branch jam, 71654ba);
 * in a held-chord morph the spike fell from 28.2 to 1.9 dB. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
int main(void)
{
    int16_t p[EXT_P_LEN] = {0};
    voice_t v = {0}, reference;
    int32_t b[CTL], expected[CTL];
    int same;
    p[P_FREED] = 1;
    p[P_FREEB] = 40;
    v.msl = 0x8000u | 1024u;                             /* the morph's slope weight: nearly all 24 dB */
    for (uint32_t i = 0; i < CTL; i++)
        b[i] = i < 16 ? 9000 : -9000;
    uf_run(&v, b, CTL, 30 << 8, p);
    reference = v;
    reference.uft = 1;                                   /* the reference: a part already at 12 dB, no morph */
    reference.msl = 0;
    v.msl = 0x8000u | 1023u;                             /* the morph reaches the 12 dB end */
    for (uint32_t i = 0; i < CTL; i++)
        b[i] = expected[i] = i < 16 ? 9000 : -9000;
    uf_run(&v, b, CTL, 30 << 8, p);
    uf_run(&reference, expected, CTL, 30 << 8, p);
    same = !memcmp(b, expected, sizeof b);
    printf("slopehandoff: a slope morph reaching 12 dB does not restart the 24 dB fade: %s\n", same ? "PASS" : "FAIL");
    return !same;
}
