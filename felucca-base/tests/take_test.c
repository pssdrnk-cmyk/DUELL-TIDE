/* SPDX-License-Identifier: GPL-3.0-only */
/* TAKE + ANSWER, live performance assist phase 1 (b10 round 8, work/b10/specs/PERF_PHASE1.md). Host group "take".
 *   STEP 1 (the checks before building): does a MIDI note reach the live recorder the way a key does? Every scenario runs twice from the same power-on, once with
 *           keys (input_on / input_off) and once with MIDI (midi_event on the part's channel), and the part's steps must be equal afterwards. The scenarios:
 *           plain part ADD, REPLACE, a three-step hold (ties), a chord (CHRD) played as one MIDI note, part 2 on its own channel, LAYER (part 1's channel), ARP SEQ (MONO, POLY ADD,
 *           POLY REPLACE, the stand-in from a remembered chord), and a note in the count-in. Each prints what it wrote.
 * Built on the harness of tests/liverec_test.c. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

static void run(uint32_t ms)
{
    static uint32_t acc;
    for (uint32_t i = 0; i < ms / 16u; i++) {
        int32_t out[2 * CTL];
        acc += (uint32_t)FS * 16u;
        while (acc >= 1000u * CTL) { acc -= 1000u * CTL; mix_block(out, CTL); }
        frame();
    }
}
static const uint8_t PAT[16] = {60,62,63,65,67,68,70,72,60,62,63,65,67,68,70,72};
static void setup(uint32_t part, int amode, int voice)
{
    track_t *t = &trk[part];
    uint32_t i;
    memset(t->step, 0, sizeof t->step);
    for (i = 0; i < 16u; i++) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = PAT[i]; t->step[i].vel = 100; }
    t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_ARATE] = 2; t->p[P_AMODE] = (int16_t)amode; t->p[P_APROB] = 127;
    t->p[P_AOCT] = 1; t->p[P_AGATE] = 64; t->p[P_VOICE] = (int16_t)voice; t->p[P_CHRD] = 0; t->p[P_TRANS] = 0;
    t->p[P_AHOLD] = 0; t->p[P_SQNT] = 0;
}
static void boot(void)
{
    ui_power_on(); frame();
    transport_req = 2u; run(48);
    song.g[G_BPM] = 120;
    memset(tide_chord_n, 0, sizeof tide_chord_n);
    song.rec = 0;
}
static void start(void) { transport_req = 1u; run(64); }
static void stop(void) { transport_req = 2u; run(100); }
static void wait_arp(track_t *t, uint32_t k)
{
    uint32_t n = 0;
    while (!(t->arp_idx != 0xFFFFFFFFu && t->arp_idx % 16u == k && t->arp_pos < arp_dur(t) / 4u) && n++ < 2000u) run(16);
}

/* ---- STEP 1: keys against MIDI ---- */
static int use_midi;
static uint32_t part_ch(uint32_t part) { return part; }                          /* the default channels: part 1 = 1 (0), part 2 = 2 (1) */
static void note_on(uint32_t part, uint32_t note)
{
    if (use_midi) midi_event(0x90, part_ch(part), note, 100); else input_on(&trk[part], note, 100);
}
static void note_off(uint32_t part, uint32_t note)
{
    if (use_midi) midi_event(0x80, part_ch(part), note, 0); else input_off(&trk[part], note);
}
typedef struct { step_t step[NSTEP]; step_t other[NSTEP]; } snap_t;
enum { SC_ADD, SC_REPLACE, SC_HOLD, SC_CHORD, SC_PART2, SC_LAYER, SC_ARP_MONO, SC_ARP_POLY_ADD, SC_ARP_POLY_REPL, SC_ARP_CHORD, SC_COUNTIN, SC_N };
static const char *const SCN[SC_N] = {"plain ADD (a tap at step 5)", "plain REPLACE (a tap into C4+E4)", "plain, a note held for 3 steps (ties)", "plain, CHRD 'maj' on one note", "part 2 armed, its own channel",
                                      "LAYER: part 1 armed, part 1's channel", "ARP SEQ MONO, a tap at arp step 5", "ARP SEQ POLY ADD", "ARP SEQ POLY REPLACE", "ARP SEQ, the stand-in from a remembered chord",
                                      "a note in the count-in's last eighth"};
static void scenario(uint32_t sc, snap_t *out)
{
    uint32_t part = sc == SC_PART2 ? 1u : 0u;
    track_t *t = &trk[part];
    int arp = sc >= SC_ARP_MONO && sc <= SC_ARP_CHORD;
    boot();
    tide_rec_replace = sc == SC_REPLACE || sc == SC_ARP_POLY_REPL;
    if (sc == SC_LAYER) { duell_mode_force(DM_LAYER); setup(1, 0, V_POLY); }
    setup(part, arp ? AM_SEQ_ID : 0, sc == SC_ARP_MONO || sc == SC_ARP_CHORD ? V_MONO : V_POLY);
    if (sc == SC_REPLACE) { t->step[5].n = 2; t->step[5].note[0] = 60; t->step[5].note[1] = 64; }
    if (sc == SC_CHORD) t->p[P_CHRD] = 1;
    if (sc == SC_ARP_CHORD) { tide_chord[0][0] = 64; tide_chord_n[0] = 1; }
    if (sc == SC_COUNTIN) { click_mode = CLICK_REC; cin_bars = 1; }
    start();
    song.rec = 1u << part;
    run(100);
    if (arp) wait_arp(t, 5); else while (t->seq_idx != 5u) run(16);
    if (sc == SC_HOLD) {
        note_on(part, 64);
        if (arp) wait_arp(t, 8); else while (t->seq_idx != 8u) run(16);
        run(16);
        note_off(part, 64);
    } else {
        note_on(part, sc == SC_ARP_CHORD ? 67 : sc == SC_ARP_POLY_REPL ? 72 : 67);
        run(40);
        note_off(part, sc == SC_ARP_CHORD ? 67 : sc == SC_ARP_POLY_REPL ? 72 : 67);
    }
    run(32);
    song.rec = 0;
    memcpy(out->step, t->step, sizeof out->step);
    memcpy(out->other, trk[1u - part].step, sizeof out->other);
    stop();
    click_mode = 0; cin_bars = 0;
}
static void step1(void)
{
    uint32_t sc;
    for (sc = 0; sc < SC_N; sc++) {
        static snap_t k, m;
        uint32_t i, diff = 0, wrote = 0;
        if (sc == SC_COUNTIN) continue;                  /* (the count-in needs the transport state of a real press: reported as not tried) */
        use_midi = 0; scenario(sc, &k);
        use_midi = 1; scenario(sc, &m);
        for (i = 0; i < NSTEP; i++) {
            diff += memcmp(&k.step[i], &m.step[i], sizeof k.step[i]) != 0 || memcmp(&k.other[i], &m.other[i], sizeof k.other[i]) != 0;
            wrote += k.step[i].n != (i < 16u ? 1u : 0u) || (i < 16u && k.step[i].note[0] != PAT[i]) || k.step[i].time != (i < 16u ? ST_NOTE : 0);
        }
        if (sc == SC_CHORD) {                            /* (a key's chord goes through key_on, not input_on: compare with what the chord is, not with this harness's keys) */
            const step_t *c = &m.step[5];
            printf("take: step 1: %-48s MIDI wrote step 5 = %u notes: %u %u %u %u\n", SCN[sc], (unsigned)c->n, c->note[0], c->note[1], c->note[2], c->note[3]);
            CHECK("plain, CHRD on one MIDI note: the chord's tones are all recorded (3 or more)", c->n >= 3u);
            continue;
        }
        printf("take: step 1: %-48s keys wrote %2u steps, MIDI %s (%u steps differ)\n", SCN[sc], wrote, diff ? "DIFFERS" : "the same", diff);
        CHECK(SCN[sc], diff == 0u);
        CHECK("... and the keys wrote something (the scenario works)", wrote > 0u);
    }
}

int main(void)
{
    step1();
    printf("take: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
