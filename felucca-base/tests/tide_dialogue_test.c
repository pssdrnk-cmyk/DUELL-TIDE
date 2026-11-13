/* SPDX-License-Identifier: GPL-3.0-only */
/* Compare predictions with real seq_tick/arp_step MIDI note-on traces. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(msg, x) do { checks++; if (!(x)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define BLOCKS 24000u
static uint8_t onset[2][BLOCKS], predicted[2][BLOCKS], visited[2][BLOCKS], future[2][BLOCKS];

static void setup(unsigned a, unsigned b, unsigned bpm, unsigned swing)
{
    ui_power_on(); frame();
    gl.on = gl.armed = 0; transport_req = 0; song.playing = song.rec = 0;
    mods_off = 0; duell_mode = DM_STEREO; usb.config = 1;
    memset(&midi_clock, 0, sizeof midi_clock);
    song.g[G_CLOCK] = 0; /* internal clock */ song.g[G_BPM] = bpm; song.g[G_SWING] = 50;
    midi_pch[0] = 0; midi_pch[1] = 1;
    memset(sqm, 0, sizeof sqm); memset(&rhythm, 0, sizeof rhythm);
    for (unsigned p = 0; p < 2; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = p ? 7 : 9; t->p[P_AMODE] = (p ? b : a) ? AM_SEQ : AM_OFF;
        t->p[P_ARATE] = p ? 3 : 2; t->p[P_SDIV] = p ? 2 : 3;
        t->p[P_ASWING] = swing; t->p[P_SSWING] = swing;
        t->p[P_APROB] = 127; t->p[P_AOCT] = 1; t->p[P_AGATE] = 64;
        t->p[P_SQNT] = 1; t->p[P_MUTE] = 0; t->p[P_LEVEL] = 100;
        t->nheld = 0; t->seq_active = 1;
        for (unsigned i = 0; i < (unsigned)t->p[P_SLEN]; i++) {
            t->step[i].time = i % 3 == 1 ? ST_REST : ST_NOTE;
            t->step[i].n = t->step[i].time == ST_NOTE;
            t->step[i].note[0] = 60 + i;
        }
    }
    seq_start(); mo_r = mo_w;
}
static void traces(unsigned a, unsigned b, unsigned bpm, unsigned swing, unsigned external)
{
    setup(a, b, bpm, swing);
    song.g[G_CLOCK] = external; midi_beat_samples = (uint32_t)FS * 60u / bpm;
    uint32_t fraction = 0, pulse = 0;
    if (external >= 2) {
        song.g[G_CLOCK] = external == 3 ? 2 : 1;
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_clock.mode = (uint8_t)song.g[G_CLOCK];
        fm1_ms = 10; midi_clock_transport(0xFA, 10); midi_clock_pulse(10);
        duell_mode_req = duell_mode;
    }
    memset(onset, 0, sizeof onset); memset(visited, 0, sizeof visited);
    for (unsigned block = 0; block < BLOCKS; block++) {
        uint32_t old[2];
        for (unsigned p = 0; p < 2; p++) old[p] = trk[p].p[P_AMODE] == AM_SEQ ? trk[p].arp_idx : trk[p].seq_idx;
        if (external >= 2) {
            fm1_ms = 10u + (uint32_t)((uint64_t)(block + 1u) * CTL * 1000u / FS);
            while ((uint64_t)(pulse + 1u) * (uint32_t)FS * 60u <= (uint64_t)(block + 1u) * CTL * bpm * 24u) {
                pulse++;
                uint32_t ms = 10u + (uint32_t)((uint64_t)pulse * 60000u / (bpm * 24u));
                if (external == 3) { uint32_t keep = fm1_ms; fm1_ms = ms; um_byte(0xF8); fm1_ms = keep; }
                else midi_enqueue(0xFu | 0xF8u << 8, 1);
            }
            events_block(CTL);
        } else {
            uint32_t advance = CTL;
            if (external) {
                fraction += MIDI_CLOCK_GRID * CTL;
                advance = fraction / midi_beat_samples;
                fraction %= midi_beat_samples;
            }
            for (unsigned p = 0; p < 2; p++) seq_tick(&trk[p], advance);
            for (unsigned p = 0; p < 2; p++) {
                uint32_t n = external ? midi_grid_advance(&midi_arp_grid[p],advance,(uint32_t)trk[p].p[P_ARATE]) : advance;
                arp_step(&trk[p], n, n);
            }
        }
        song.tick++;
        while (mo_r != mo_w) {
            uint32_t event = midi_out_q[mo_r++ % MQ];
            if (((event >> 8) & 0xF0u) == 0x90u && (event >> 24)) onset[(event >> 8) & 1u][block] = 1;
        }
        for (unsigned p = 0; p < 2; p++) {
            uint32_t idx = trk[p].p[P_AMODE] == AM_SEQ ? trk[p].arp_idx : trk[p].seq_idx;
            if (idx != old[p] && block) {
                visited[p][block] = 1;
                predicted[p][block] = phrase_peer_attack(p, idx % (uint32_t)trk[p].p[P_SLEN]);
                future[p][block] = phrase_peer_attack(p, (idx + 1u) % (uint32_t)trk[p].p[P_SLEN]);
            }
        }
    }
    unsigned mismatch = 0, bounded = 0, n = 0, previous[2] = {0,0};
    for (unsigned p = 0; p < 2; p++) for (unsigned i = 2; i + 1 < BLOCKS; i++) if (visited[p][i]) {
        unsigned real = onset[p ^ 1u][i - 1] || onset[p ^ 1u][i] || onset[p ^ 1u][i + 1];
        if (predicted[p][i] != real) mismatch++;
        if (previous[p] && future[p][previous[p]] != real) mismatch++;
        if (external >= 2 && i >= 3 && i + 3 < BLOCKS) {
            unsigned near = 0;
            for (unsigned j = i - 3; j <= i + 3; j++) near |= onset[p ^ 1u][j];
            bounded += predicted[p][i] ? !near : onset[p ^ 1u][i];
            if (previous[p]) bounded += future[p][previous[p]] ? !near : onset[p ^ 1u][i];
        }
        previous[p] = i;
        n++;
    }
    printf("trace arp=%u/%u BPM=%u swing=%u ext=%u: %u visits, %u mismatches\n", a,b,bpm,swing,external,n,mismatch);
    if (external >= 2) {
        printf("  wire timestamp uncertainty: %u contradictions outside 3 blocks / at exact coincident dispatch\n", bounded);
        CHECK("wire-clock predictions respect millisecond timestamp uncertainty", n > 50 && bounded == 0);
    } else CHECK("predictions match actual fired onsets within one block", n > 50 && mismatch == 0);
}

/* Ask two steps ahead, then compare with the destination's actual note-on.
 * This also covers nudges, whose note-on need not coincide with seq_idx. */
static void offset_trace(unsigned detail, unsigned external)
{
    static uint8_t note[2][BLOCKS], ask[2][BLOCKS], target[2][BLOCKS];
    setup(detail == 3, detail == 3, 128, 64);
    song.g[G_CLOCK] = external;
    midi_beat_samples = (uint32_t)FS * 60u / 128u;
    memset(note,0,sizeof note); memset(target,0,sizeof target);
    for (unsigned p=0;p<2;p++) {
        track_t *t=&trk[p];
        for (unsigned i=0;i<(unsigned)t->p[P_SLEN];i++) {
            t->step[i].time=ST_NOTE; t->step[i].n=1;
            if (detail==1) { t->p[P_SQNT]=0; step_set_nudge(&t->step[i],((int)i%3-1)*3); }
            if (detail==2) step_set_ratchet(&t->step[i],2+i%3);
        }
        if (detail==3) input_on(t,60,100);
    }
    mo_r=mo_w;
    uint32_t fraction=0;
    for (unsigned block=0;block<BLOCKS;block++) {
        uint32_t old[2];
        for (unsigned p=0;p<2;p++) old[p]=detail==3?trk[p].arp_idx:trk[p].seq_idx;
        uint32_t adv=CTL;
        if (external) { fraction+=MIDI_CLOCK_GRID*CTL; adv=fraction/midi_beat_samples; fraction%=midi_beat_samples; }
        for (unsigned p=0;p<2;p++) seq_tick(&trk[p],adv);
        for (unsigned p=0;p<2;p++) {
            uint32_t n=external?midi_grid_advance(&midi_arp_grid[p],adv,(uint32_t)trk[p].p[P_ARATE]):adv;
            arp_step(&trk[p],n,n);
        }
        while(mo_r!=mo_w) {
            uint32_t e=midi_out_q[mo_r++%MQ];
            if (((e>>8)&0xF0u)==0x90u && (e>>24)) note[(e>>8)&1u][block]=(uint8_t)(e>>16);
        }
        for (unsigned p=0;p<2;p++) {
            uint32_t idx=detail==3?trk[p].arp_idx:trk[p].seq_idx;
            if (idx!=old[p] && block) {
                uint32_t d=(idx+2u)%(uint32_t)trk[p].p[P_SLEN];
                target[p][block]=(uint8_t)(60+d);
                ask[p][block]=phrase_peer_attack(p,d);
            }
        }
    }
    unsigned checked=0,mismatch=0;
    for(unsigned p=0;p<2;p++) for(unsigned b=1;b+1<BLOCKS;b++) if(target[p][b]) {
        unsigned j=b+1;
        while(j+1<BLOCKS && note[p][j]!=target[p][b]) j++;
        if(j+1>=BLOCKS) continue;
        unsigned real=note[p^1u][j-1]||note[p^1u][j]||note[p^1u][j+1];
        if (ask[p][b]!=!!real) mismatch++;
        checked++;
    }
    printf("offset detail=%u ext=%u: %u destinations, %u mismatches\n",detail,external,checked,mismatch);
    CHECK("nudge/ratchet/held-key prediction matches fired destinations",checked>50 && !mismatch);
}

static void dialogue_choices(void)
{
    unsigned collisions=0, independent=0, moves=0;
    sqm_sync=0;   /* the M1 fixture is the OFF regression: SYNC SOFT keeps a hit on the "and" in place more often (tideturns covers SYNC) */
    for(unsigned pi=0;pi<2;pi++) for(unsigned seed=0;seed<32;seed++) {
        setup(1,1,120,50);
        for(unsigned p=0;p<2;p++) {
            track_t *t=&trk[p];
            t->p[P_SLEN]=16; t->p[P_ARATE]=2;
            t->arp_idx=t->arp_pos=0;
            memset(t->step,0,sizeof t->step);
            for(unsigned i=0;i<16;i++) if(!(i%4) || i%4==(p==pi?2u:3u)) {
                t->step[i].time=ST_NOTE; t->step[i].n=1; t->step[i].note[0]=60;
            }
            sqm_refresh(p,1);
            rhythm_reset(p);
        }
        rhythm.dir[pi]=1;
        unsigned count=phrase_count(pi,16);
        int changed=rhythm_mutate(pi,3u+seed*65536u);
        CHECK("eligible part receives a development opportunity",changed);
        moves+=changed;
        CHECK("dialogue retains total attack density",phrase_count(pi,16)==count);
        for(unsigned i=0;i<16;i++) {
            if(!(i%4)) CHECK("shared beat anchors remain",!rhythm_map[pi][i]);
            if(rhythm_offset(pi,i)) {
                unsigned dest=(unsigned)((int)i+rhythm_offset(pi,i));
                collisions+=sqm_hit(&trk[pi^1u].step[dest]);
                /* The unchanged independent preference would move right. */
                independent+=sqm_hit(&trk[pi^1u].step[i+1u]);
            }
        }
    }
    printf("clashing fixture: %u moves, independent=%u collisions, dialogue=%u; both parts exercised\n",moves,independent,collisions);
    CHECK("eligible offbeat collisions decrease without thinning",moves==64 && independent==64 && collisions==0);
}
static void guards(void)
{
    setup(0, 1, 120, 50);
    trk[0].seq_idx = trk[1].arp_idx = 0; trk[0].seq_pos = trk[1].arp_pos = 0;
    CHECK("SEQ arp onset at the shared origin", phrase_peer_attack(0, 0));
    uint32_t before = rng_state;
    for (unsigned i = 0; i < 100; i++) phrase_peer_attack(0, i % 9);
    CHECK("prediction never advances playback RNG", rng_state == before);
    song.g[G_CLOCK] = 1;
    CHECK("external-clock origin is predicted", phrase_peer_attack(0, 0));
    song.g[G_CLOCK] = 0; /* internal clock */
    trk[0].p[P_SQNT] = 0;
    CHECK("unquantized zero-nudge sequence is predicted", phrase_peer_attack(0, 0));
    trk[0].p[P_SQNT] = 1;
    trk[1].nheld = 1;
    CHECK("held-key arp is predicted", phrase_peer_attack(0, 0));
    trk[1].nheld = 0; song.rec = 1;
    CHECK("REC does not disable the stand-in (audit liverec L1)", phrase_peer_attack(0, 0));
    song.rec = 0; trk[1].p[P_MUTE] = 1;
    CHECK("muted peer", !phrase_peer_attack(0, 0)); trk[1].p[P_MUTE] = 0;
    trk[1].p[P_LEVEL] = 0;
    CHECK("silent peer", !phrase_peer_attack(0, 0)); trk[1].p[P_LEVEL] = 100;
    mods_off = MODS_ARP;
    CHECK("disabled ARP", !phrase_peer_attack(0, 0)); mods_off = MODS_SEQ;
    CHECK("disabled SEQ", !phrase_peer_attack(0, 0)); mods_off = 0;
    song.playing = 0;
    CHECK("stopped transport", !phrase_peer_attack(0, 0)); song.playing = 1;
    trk[1].step[0].time = ST_TIE;
    CHECK("tie is no attack", !phrase_peer_attack(0, 0));
    trk[1].step[0].time = ST_NOTE;
    step_set_chance(&trk[1].step[0], 0);
    CHECK("arp playback ignores step chance", phrase_peer_attack(0, 0));
    trk[1].p[P_APROB] = 0;
    CHECK("inclusive APROB zero remains a possible attack", phrase_peer_attack(0, 0));
    trk[1].arp_pos = 0xFFFFFFFu;
    CHECK("phase-reset sentinel falls back", !phrase_peer_attack(0, 0));
    setup(0, 0, 120, 50);
    trk[0].seq_idx = trk[1].seq_idx = 0; trk[0].seq_pos = trk[1].seq_pos = 0;
    step_set_chance(&trk[1].step[0], 0);
    CHECK("ordinary chance zero is no attack", !phrase_peer_attack(0, 0));
}

int main(void)
{
    for (unsigned bpm = 120; bpm <= 128; bpm += 8)
        for (unsigned a = 0; a < 2; a++) for (unsigned b = 0; b < 2; b++)
            for (unsigned e = 0; e < 2; e++) traces(a,b,bpm, a == b ? 64 : 50,e);   /* (SWG 64 lands the offbeat where 67 did before the display told the truth, audit S03) */
    traces(1,1,240,75,0);
    traces(1,1,240,75,1);
    traces(0,1,120,50,2);
    traces(1,0,128,64,3);
    traces(1,1,128,64,2);
    for (unsigned d=1;d<=3;d++) for(unsigned e=0;e<2;e++) offset_trace(d,e);
    dialogue_choices();
    guards();
    setup(0,1,120,50);
    trk[0].p[P_SLEN] = trk[1].p[P_SLEN] = 8;
    trk[0].p[P_SDIV] = trk[1].p[P_ARATE] = 2;
    trk[0].seq_idx = trk[1].arp_idx = 0;
    trk[0].seq_pos = trk[1].arp_pos = 0;
    trk[1].step[2].time = ST_NOTE; trk[1].step[2].n = 1;
    trk[1].step[4].time = ST_REST;
    CHECK("direction prefers the eligible neighboring gap", phrase_direction(0,3,8,-1,1) == 1);
    printf("M1 dialogue: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
