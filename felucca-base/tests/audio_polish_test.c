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


static void audio_blocks(unsigned count)
{
    for(unsigned j=0;j<count;j++) { int32_t out[2*CTL]={0}; mix_block(out,CTL); }
}
/* FM6 UNISON: one voice ends (a refusal, a steal, a staggered release). The voices still sounding must not step to the new gain at the
 * block line: the block ramps from the gain of the block before to the new one. Three renders of one note from the same state:
 * S1 the count steady (all running), S2 steady one voice fewer, S3 the change itself. S3 starts like S1 and ends like S2. */
static unsigned running(const track_t *t)
{
    unsigned n=0;
    for(unsigned i=0;i<FM6_POLY;i++) n+=!!(t->v[i].active && t->v[i].stage);
    return n;
}
static void unison_ramp(void)
{
    track_t *t=&trk[0];
    ui_power_on(); frame();
    song.playing=song.rec=0; transport_req=0; gl.on=gl.armed=0; morph_musical=0;
    host_preset(t,ENGI_FM6,0);
    t->p[P_VOICE]=V_UNISON; t->p[P_AMODE]=AM_OFF;
    t->p[P_DIST]=t->p[P_DLY]=t->p[P_REV]=t->p[P_CHOR]=0;
    trk[1].p[P_MUTE]=1;
    memset(mxs,0,sizeof mxs); memset(mxv,0,sizeof mxv); memset(fm6_xt,0,sizeof fm6_xt);
    trk_note_on(t,60,100); audio_blocks(40);
    unsigned all=running(t);
    REQUIRE("unison: several voices run",all>=3);
    unsigned slot=0; while(slot<FM6_POLY && !(t->v[slot].active && t->v[slot].stage)) slot++;
    unsigned last=FM6_POLY; for(unsigned i=0;i<FM6_POLY;i++) if(i!=slot && t->v[i].active && t->v[i].stage) last=i;
    REQUIRE("unison: a voice to end",slot<FM6_POLY && last<FM6_POLY);
    if(slot>=FM6_POLY || last>=FM6_POLY) return;
    voice_t *v=&t->v[slot];
    fm6_note_t *n=fm6_note_of(t,v), saved=*n;
    vmod_t m; memset(&m,0,sizeof m);
    m.amp0=m.amp1=32767; m.pitch16=60*256; m.shape=64<<8;
    int32_t s1[CTL],s2[CTL],s3[CTL];
    uint8_t keep_act=t->v[last].active, keep_stage=t->v[last].stage;
    /* S1: steady */
    memset(s1,0,sizeof s1); fm6_block(t); fm6_block(t); *n=saved; fm6_render(t,v,s1,CTL,&m);
    /* S2: steady, one voice fewer */
    t->v[last].active=0; t->v[last].stage=0;
    memset(s2,0,sizeof s2); fm6_block(t); fm6_block(t); *n=saved; fm6_render(t,v,s2,CTL,&m);
    /* S3: from the all-running block to the count that has just dropped */
    t->v[last].active=keep_act; t->v[last].stage=keep_stage;
    fm6_block(t); fm6_block(t);
    t->v[last].active=0; t->v[last].stage=0;
    memset(s3,0,sizeof s3); fm6_block(t); *n=saved; fm6_render(t,v,s3,CTL,&m);
    double a1=0,a2=0,a3=0,b1=0,b2=0,b3=0;
    for(unsigned i=0;i<8;i++) { a1+=labs(s1[i]); a2+=labs(s2[i]); a3+=labs(s3[i]); b1+=labs(s1[CTL-1-i]); b2+=labs(s2[CTL-1-i]); b3+=labs(s3[CTL-1-i]); }
    printf("unison %u -> %u voices: first 8 |x| %.0f / %.0f / %.0f, last 8 %.0f / %.0f / %.0f\n",all,all-1,a1,a2,a3,b1,b2,b3);
    REQUIRE("unison: the gain differs between the counts",fabs(a1-a2)>0.05*a1 && fabs(b1-b2)>0.05*b1);
    REQUIRE("unison: a voice ending does not step the others (block starts at the old gain)",fabs(a3-a1)<0.5*fabs(a2-a1));
    REQUIRE("unison: .. and ramps to the new one by the block's end",fabs(b3-b2)<0.5*fabs(b1-b2));
}

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
/* WASH OFF (the default): nothing measured, nothing generated, the block untouched, the states idle */
static void wash_off(void)
{
    int32_t b[CTL];
    for(unsigned i=0;i<CTL;i++) b[i]=(int32_t)(i*997u%20001u)-10000;
    wash_setup(); morph_wlvl=0;
    morph_wash(0,b,CTL);
    REQUIRE("wash OFF: no power measured",mxv[0].wnow==0 && mxv[0].wref==((int64_t)1<<44));
    REQUIRE("wash OFF: no noise generated",mxv[0].wr==0 && mxv[0].wy[0]==0);
}
int main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    ui_power_on(); frame();
    wash_cap(); wash_off(); unison_ramp();
    printf("audiopolish: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
