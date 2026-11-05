/* Uses the existing firmware/UI host harness; no simulated DSP replacement. */
#define UI_TEST_NO_MAIN 1
#define CROSSINGS 1
#ifndef HOST_MIX_FOLLOWS_MASTER
#define HOST_MIX_FOLLOWS_MASTER 1
#endif
#include "ui_test.c"

static unsigned checks, failures;
#define REQUIRE(msg,expr) do { checks++; if (!(expr)) { failures++; printf("FAIL: %s (%s:%d)\n",msg,__FILE__,__LINE__); } } while(0)
static uint64_t audio_hash=1469598103934665603ull;
static void audio_blocks(unsigned count)
{
    for(unsigned j=0;j<count;j++) {
        int32_t out[2*CTL]={0};
        mix_block(out,CTL);
        for(unsigned k=0;k<2*CTL;k++) audio_hash=(audio_hash^(uint32_t)out[k])*1099511628211ull;
    }
}
static track_t *fresh(unsigned engine)
{
    ui_power_on(); frame();
    song.playing=song.rec=0; transport_req=0;
    gl.on=gl.armed=0; morph_musical=0;
    host_preset(&trk[0],engine,0);
    track_t *t=&trk[0];
    t->p[P_VOICE]=V_POLY; t->p[P_AMODE]=AM_OFF;
    t->p[P_DIST]=t->p[P_DLY]=t->p[P_REV]=t->p[P_CHOR]=0;
    t->p[P_ITYPE]=0;
    trk[1].p[P_MUTE]=1;
#ifdef CROSSINGS
    memset(mxs,0,sizeof mxs); memset(mxv,0,sizeof mxv); memset(fm6_xt,0,sizeof fm6_xt);
#endif
    return t;
}

static void at_rest(void)
{
    static const unsigned engines[]={0,ENGI_FM6};
    static const unsigned notes[]={36,60,84};
    audio_hash=1469598103934665603ull;
    for(unsigned e=0;e<sizeof engines/sizeof engines[0];e++) for(unsigned n=0;n<3;n++) {
        track_t *t=fresh(engines[e]);
        t->p[P_ATK]=32+n*24; t->p[P_DEC]=48; t->p[P_SUS]=96; t->p[P_REL]=40;
        morph_ready(0); morph_do(MQ_FADER,1u,0u);
        trk_note_on(t,notes[n],100);
        audio_blocks(160);
        trk_note_off(t,notes[n]); audio_blocks(40);
    }
    printf("AT_REST_HASH=%016llx\n",(unsigned long long)audio_hash);
}

#ifdef CROSSINGS
static void curves(void)
{
    int32_t pc=32768,pr=-1;
    unsigned ok=1;
    for(int32_t p=0;p<=32768;p++) {
        int32_t c=xcos(p),r=xrise(p);
        ok &= c>=0 && c<=32768 && c<=pc && r>=0 && r<=32768 && r>=pr;
        pc=c;pr=r;
    }
    REQUIRE("crossing curves monotonic and bounded",ok);
    for(unsigned k=2;k<=15;k++) REQUIRE("exact octave gains",xlog2(1<<k)==-((int32_t)(15-k)*(1<<24)));
}
static unsigned carrier;
static track_t *retune_pair(void)
{
    track_t *t=fresh(ENGI_FM6);
    t->p[P_E0]=0; t->p[P_E1]=t->p[P_E3]=t->p[P_E4]=t->p[P_E5]=t->p[P_E6]=0;
    morph_part_t *mp=morph_ready(0);
    unsigned alg=0;
    for(;alg<32;alg++) { unsigned m=fm6_carriers(alg); if(m && !(m&(m-1))) break; }
    REQUIRE("one-carrier algorithm exists",alg<32);
    carrier=0; while(!((fm6_carriers(alg)>>carrier)&1u)) carrier++;
    for(unsigned side=0;side<2;side++) {
        uint8_t *p=morph_fmto[0][side];
        memcpy(p,fm6_patch[0],FP_SIZE);
        p[FP_ALG]=(uint8_t)alg; p[FP_FB]=0;
        for(unsigned k=0;k<6;k++) {
            uint8_t *op=p+k*FP_OP;
            op[FP_MODE]=0; op[FP_FC]=(uint8_t)(k==carrier?1+side:1); op[FP_FF]=0;
        }
        mp->tok[side][mp->top[side]]=(uint8_t)(side+1);
        morph_fmto_k[0][side]=(uint8_t)(side+1);
    }
    morph_build(mp); mp->fser++; mp->cvalid=0;
    morph_xplan(0);
    REQUIRE("planner detects FM6 structural retune",mxs[0].need&XN_FM);
    REQUIRE("carrier uses retune role",mxs[0].role[carrier]==XR_RETUNE);
    REQUIRE("retuned carrier gets a free-operator stand-in",(mxs[0].rsub>>carrier)&1u);
    return t;
}
static void key_layout(void)
{
    retune_pair(); morph_part_t *mp=&mph[0];
    mxs[0].sounding=1; /* active crossing: side must be independent of smoothing bits */
    for(unsigned side=0;side<2;side++) for(unsigned r=0;r<32768;r+=127) {
        mxs[0].side=(uint8_t)side; mp->sm_r=(uint16_t)r;
        REQUIRE("smoothing cannot overwrite FM6 structural side",((morph_fm6(0,0)>>15)&1u)==side);
    }
    mp->sm_r=32767; mxs[0].side=0;
    REQUIRE("last smoothing interval preserves side A",!(morph_fm6(0,0)&(1u<<15)));
}
static void held_retune(void)
{
    track_t *t=retune_pair();
    morph_do(MQ_FADER,1,0); trk_note_on(t,60,110); audio_blocks(128);
    unsigned traversals=0;
    for(unsigned direction=0;direction<2;direction++) {
        unsigned target=direction?0:127,bridge=0,standin=0,monotonic=1;
        morph_do(MQ_FADER,1,target);
        unsigned before=mxs[0].tp;
        for(unsigned i=0;i<2000;i++) {
            audio_blocks(1);
            unsigned now=mxs[0].tp;
            monotonic &= direction ? now<=before : now>=before;
            before=now;
            bridge |= fm6_xt[0].on && fm6_xt[0].alg==31;
            fm6_note_t *note=0;
            for(unsigned v=0;v<FM6_POLY;v++) if(t->v[v].active) note=(fm6_note_t*)ENG_MEM(0)+v;
            if(note) standin |= note->xs!=0;
            if(!fm6_xt[0].on && now==(target?32768u:0u)) { traversals++; break; }
        }
        REQUIRE("transition progress monotonic",monotonic);
        REQUIRE("harmonic bridge reached",bridge);
        REQUIRE("stand-in actually runs on a held note",standin);
        REQUIRE("transition arrives at requested side",mxs[0].side==(direction?0:1));
        REQUIRE("FM6 bridge stops at endpoint",!fm6_xt[0].on);
        fm6_note_t *ended=(fm6_note_t*)ENG_MEM(0);
        unsigned v=0; while(v<FM6_POLY && !t->v[v].active) v++;
        const uint8_t *p=morph_fmto[0][direction ? 0 : 1];
        int32_t expected=fm6_osc_base(p+carrier*FP_OP,60+p[FP_TRNSP]-24,0);
        REQUIRE("held carrier reaches endpoint frequency",v<FM6_POLY && ended[v].base[carrier]==expected);
    }
    REQUIRE("both full traversals completed",traversals==2);
}
static void compact_tuning(void)
{
    static const int note[5]={0,36,60,84,127};
    uint8_t patch[FP_SIZE+1u]={0};
    fm6_xpitch_t packed;
    unsigned ok=1;
    for(unsigned sample=0;sample<32;sample++) {
        patch[FP_TRNSP]=(uint8_t)(sample%49u);
        for(unsigned k=0;k<6;k++) {
            uint8_t *op=patch+k*FP_OP;
            op[FP_MODE]=(uint8_t)((sample+k)&1u);
            op[FP_FC]=(uint8_t)((sample+5u*k)%32u);
            op[FP_FF]=(uint8_t)((13u*sample+19u*k)%100u);
            op[FP_DET]=(uint8_t)((sample+k)%15u);
        }
        fm6_xpitch_pack(&packed,patch);
        for(unsigned k=0;k<6;k++) for(unsigned n=0;n<5;n++) {
            int a=clamp(note[n]+patch[FP_TRNSP]-24,0,127);
            int b=clamp(note[n]+packed.transpose-24,0,127);
            ok &= fm6_osc_base(patch+k*FP_OP,a,4096)==fm6_osc_base4(packed.op[k],b,4096);
        }
    }
    REQUIRE("compact endpoints preserve ratio/fixed tuning and transpose",ok);
}
static void held_keep_retune(void)
{
    track_t *t=retune_pair();
    trk_note_on(t,60,100); audio_blocks(8);
    morph_do(MQ_FADER,1u,127u);
    for(unsigned i=0;i<2200;i++) audio_blocks(1);
    unsigned slot=0; while(slot<FM6_POLY && !t->v[slot].active) slot++;
    fm6_note_t *n=(fm6_note_t*)ENG_MEM(0)+slot;
    int32_t expected=fm6_osc_base(morph_fmto[0][1]+carrier*FP_OP,60,0);
    REQUIRE("held KEEP fixture reaches ratio B",slot<FM6_POLY && n->live && n->base[carrier]==expected);
    tide_keep_sound(0);
    REQUIRE("KEEP retains a retuned held carrier",n->live && n->base[carrier]==expected);
    audio_blocks(8);
    REQUIRE("kept held carrier stays at its committed frequency",n->live && n->base[carrier]==expected);
}
#endif
#ifdef CROSSINGS
/* the filter dip reaches the voices on 1.1's chain path too (CHAIN_ALL: every voice whose filter 1 runs), as track_render's plain path and the JAM fork's voice_chain.c */
static void chain_dip(void)
{
    track_t *t=fresh(0);                                  /* ANALOG: the multimode filter */
    const engine_t *e=ENGINES[0];
    voice_t v; vmod_t m;
    int32_t mid=UF_TOP/2;
    memset(&v,0,sizeof v); memset(&m,0,sizeof m);
    t->p[P_KTRK]=0; t->p[P_FREEC]=0;
    t->mwon=1; t->mwpi=0; morph_cdd[0]=16384;
    REQUIRE("chain path: filter 1's cutoff takes the crossing dip", morph_xcut(t,mid)<mid && chain_uc(e,t,&v,&m,mid,0)==morph_xcut(t,mid));
    m.acc1=3000; chain_uc(e,t,&v,&m,mid,0);
    REQUIRE("crossing dip: filter 2 LINK's and NOISE's cutoff (ucn) takes the dip too", m.ucn<mid-3000 && m.ucn==morph_xcut(t,mid-3000));
    memset(&m,0,sizeof m);
    REQUIRE("chain path: a dip under the open end runs filter 1", chain_f1_on(e,t,&v,&m,UF_TOP));
    morph_cdd[0]=0; t->mwon=0;
    memset(&m,0,sizeof m);
    REQUIRE("chain path: no dip, an open filter 1 stays off", !chain_f1_on(e,t,&v,&m,UF_TOP) && chain_uc(e,t,&v,&m,mid,0)==mid);
}
#endif
int main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    at_rest();
#ifdef CROSSINGS
    curves(); key_layout(); held_retune(); held_keep_retune(); compact_tuning(); chain_dip();
#endif
    printf("crossings: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
