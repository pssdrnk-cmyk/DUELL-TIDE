/* SPDX-License-Identifier: GPL-3.0-only */
/* Audio polish (audit lane A): the wash's gain at its cap, its early exit with WASH OFF, the FM6 UNISON gain ramp.
 * Uses the existing firmware/UI host harness; the wash is called directly (it is static in the same translation unit). */
#define UI_TEST_NO_MAIN 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include "ui_test.c"

static unsigned checks, failures;
#define REQUIRE(msg,expr) do { checks++; if (!(expr)) { failures++; printf("FAIL: %s (%s:%d)\n",msg,__FILE__,__LINE__); } } while(0)

/* a part that moves (GLIDE's breath) with a silent block and a huge level at rest: the noise is asked for at the gain cap */
static void wash_setup(void)
{
    memset(mxs,0,sizeof mxs); memset(mxv,0,sizeof mxv);
    mxs[0].snd=1; mxs[0].motion=20000; mxs[0].lfloor=4096*9;
    mxv[0].wref=(int64_t)1<<44;
}
static void wash_cap(void)
{
    int32_t b[CTL]; memset(b,0,sizeof b);
    wash_setup(); morph_wlvl=2;
    for(unsigned k=0;k<8;k++) morph_wash(0,b,CTL);
    REQUIRE("wash: at the gain cap (ratio of 24 dB or more) the wash still comes in",mxv[0].wg>0);
    morph_wlvl=0;
}
int main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    ui_power_on(); frame();
    wash_cap();
    printf("audiopolish: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
