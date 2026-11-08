/* Live recording (audit liverec 2026-11-08, work/audit/liverec-audit-2026-11-08): REC armed while PLAY runs.
 *   H1   both ARP SEQ parts keep playing while REC is armed (L1)
 *   H2   POLY, a key held a loop: the pattern is as it was (L2)
 *   H2b  a firm key (velocity 120) sets no accent on any step (L2)
 *   H3   MONO, a key held three loops: no climb to 127 (L2)
 *   H4   a key pressed mid-pattern: no rotated copy (L2)
 *   H5   arp OFF (the plain path): a tap at step 5 adds the note to step 5 (control; L3: a long hold ties)
 *   R4   REC on HOME: armed, a tap disarms and leaves HOLD alone (L6); unarmed it stays HOLD and says how to record (L5)
 * Built from the probe work/audit/liverec-audit-2026-11-08/probe/liverec_probe.c (same harness as ui_test.c). */
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
#define LOOP_MS 2000u   /* 120 bpm, 16 x 1/16 = 1 bar = 2000 ms */

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
/* how many times the arp's note changes over ms */
static uint32_t changes(track_t *t, uint32_t ms)
{
    uint32_t i, last = 0, cnt = 0;
    for (i = 0; i < ms / 16u; i++) {
        run(16);
        if (t->arp_note && t->arp_note != last) cnt++;
        last = t->arp_note;
    }
    return cnt;
}
static void start(void) { transport_req = 1u; run(64); }
static void stop(void) { transport_req = 2u; run(100); }
static int same(const track_t *t, const step_t *was) { return !memcmp(t->step, was, sizeof t->step); }

static void h1(void)
{
    uint32_t a0, a1;
    boot(); setup(0, AM_SEQ_ID, V_POLY); setup(1, AM_SEQ_ID, V_POLY);
    start();
    CHECK("H1: before arming, both parts play", changes(&trk[0], 1000) >= 4u && changes(&trk[1], 1000) >= 4u);
    song.rec = 1; run(400);
    a0 = changes(&trk[0], 2000); a1 = changes(&trk[1], 2000);
    CHECK("H1: part 1 armed: its stand-in keeps playing", a0 >= 8u);
    CHECK("H1: part 1 armed: part 2's stand-in keeps playing", a1 >= 8u);
    song.rec = 3;
    CHECK("H1: both armed: both keep playing", changes(&trk[0], 1000) >= 4u && changes(&trk[1], 1000) >= 4u);
    song.rec = 0;
    stop();
}
static void h2(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    boot(); setup(0, AM_SEQ_ID, V_POLY);
    start();
    memcpy(was, t->step, sizeof was);
    song.rec = 1; run(100);
    input_on(t, 64, 100);
    run(LOOP_MS);
    input_off(t, 64); run(200); song.rec = 0;
    CHECK("H2: POLY, E4 held a loop while armed: the pattern is as it was", same(t, was));
    stop();
}
static void h2b(void)
{
    track_t *t = &trk[0];
    uint32_t i, acc = 0;
    boot(); setup(0, AM_SEQ_ID, V_POLY);
    start();
    song.rec = 1; run(100);
    input_on(t, 64, 120);
    run(LOOP_MS);
    input_off(t, 64); run(200); song.rec = 0;
    for (i = 0; i < 16u; i++) acc += !!(t->step[i].flags & SF_ACCENT);
    CHECK("H2b: velocity 120 held a loop: no step accented", acc == 0u);
    stop();
}
static void h3(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    uint32_t l, ok = 1;
    boot(); setup(0, AM_SEQ_ID, V_MONO);
    start();
    memcpy(was, t->step, sizeof was);
    song.rec = 1; run(100);
    input_on(t, 64, 100);
    for (l = 0; l < 3u; l++) { run(LOOP_MS); ok &= same(t, was); }
    input_off(t, 64); run(200); song.rec = 0;
    CHECK("H3: MONO, E4 held three loops: every step's note unchanged after each loop (no climb)", ok);
    CHECK("H3: ... and after release", same(t, was));
    stop();
}
static void h4(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    boot(); setup(0, AM_SEQ_ID, V_POLY);
    start();
    song.rec = 1;
    while (t->seq_idx != 5u) run(16);
    memcpy(was, t->step, sizeof was);
    input_on(t, 64, 100);
    run(LOOP_MS);
    input_off(t, 64); run(200); song.rec = 0;
    CHECK("H4: a key pressed at step 5 of an ARP SEQ pattern: no rotated copy of the arp's notes", same(t, was));
    stop();
}
static void h5(void)
{
    track_t *t = &trk[0];
    step_t was[NSTEP];
    uint32_t i, other = 1, l, tail = 1;
    boot(); setup(0, 0, V_POLY);
    start();
    song.rec = 1;
    while (t->seq_idx != 5u) run(16);
    memcpy(was, t->step, sizeof was);
    input_on(t, 64, 100); run(300); input_off(t, 64); run(LOOP_MS); song.rec = 0;
    {
        const step_t *s = &t->step[5];
        uint32_t has = 0;
        for (i = 0; i < s->n && i < 4u; i++) has |= s->note[i] == 64u;
        CHECK("H5 (arp OFF): the tap at step 5 adds 64 to step 5", has);
    }
    for (i = 0; i < 16u; i++)
        if (i != 5u && i != 6u && memcmp(&t->step[i], &was[i], sizeof was[i])) other = 0;
    CHECK("H5 (arp OFF): no other step changes but the tie into 6", other);
    setup(0, 0, V_MONO); song.rec = 1;                   /* L3: a key held longer than the loop fills it with ties */
    input_on(t, 64, 100);
    for (l = 0; l < 3u; l++) run(LOOP_MS);
    input_off(t, 64); song.rec = 0;
    for (i = 0; i < 16u; i++) tail &= t->step[i].time == ST_TIE || t->step[i].note[0] == 64u || t->step[i].note[0] == 0u;
    CHECK("H5 (arp OFF, L3): a key held for whole loops leaves ties and that one note", tail);
    stop();
}

/* ---- R4: REC on HOME ---- */
static void to_home(void) { go_main_to(HP_MORPH); frames(600); }
static void r4(void)
{
    track_t *t = &trk[0];
    boot(); setup(0, AM_SEQ_ID, V_POLY);
    song.sel = 0;
    go_page(GR_ROLL); frame();                           /* STEP */
    start();                                             /* (stopped, STEP arms step recording and says KEYS WRITE STEPS) */
    press(B_REC); frames(600);
    CHECK("R4: REC on STEP arms the selected part", song.rec == 1u);
    CHECK("R4: ... on an ARP SEQ part the toast says the knobs record", msg_is("ARP SEQ: KNOBS RECORD"));
    to_home();
    CHECK("R4: HOME is where the test is", at_home());
    t->p[P_AHOLD] = 0;
    press(B_REC); frames(600);
    CHECK("R4 (L6): armed, REC on HOME disarms", song.rec == 0u);
    CHECK("R4 (L6): ... and leaves HOLD alone", t->p[P_AHOLD] == 0);
    CHECK("R4 (L6): ... and says REC OFF", msg_is("REC OFF"));
    stop();
    song.rec = 0;
    press(B_REC); frames(600);

    CHECK("R4: not armed, REC on HOME is the HOLD switch", song.rec == 0u && t->p[P_AHOLD] != 0);
    CHECK("R4 (L5): ... and the toast says how to record", msg_is("HOLD ON: SEQ+REC=REC"));
    press(B_REC); frames(600);
    CHECK("R4: the next tap: HOLD OFF", t->p[P_AHOLD] == 0 && msg_is("HOLD OFF: SEQ+REC=REC"));
    CHECK("R4 (L5): the toasts fit the header (message font, or the smaller one)", text_w(&AF_M, "HOLD OFF: SEQ+REC=REC") <= 150 && text_w(&AF_M, "ARP SEQ: KNOBS RECORD") <= 150);
    printf("liverec: toast widths  M %d / S %d px (HOLD OFF: SEQ+REC=REC), M %d / S %d px (ARP SEQ: KNOBS RECORD), header 150 px\n",
           text_w(&AF_M, "HOLD OFF: SEQ+REC=REC"), text_w(&AF_S, "HOLD OFF: SEQ+REC=REC"),
           text_w(&AF_M, "ARP SEQ: KNOBS RECORD"), text_w(&AF_S, "ARP SEQ: KNOBS RECORD"));
    /* another part armed, not the selected one: the tap behaves as unarmed (the header ring shows the other part) */
    song.rec = 2u; song.sel = 0; t->p[P_AHOLD] = 0;
    press(B_REC); frames(600);
    CHECK("R4: part 2 armed, part 1 selected: REC on HOME is HOLD, the arming stays", song.rec == 2u && t->p[P_AHOLD] != 0);
    song.rec = 0; t->p[P_AHOLD] = 0;
    /* SEQ held + REC arms from HOME and starts a stopped transport */
    to_home(); song.sel = 0;
    btn_down(B_SEQ); frames(600);
    press(B_REC); frames(600);
    btn_up(B_SEQ); frames(600);
    CHECK("R4 (L5): SEQ held + REC on HOME arms the selected part", (song.rec & 1u) && t->p[P_AHOLD] == 0);
    CHECK("R4 (L5): ... and starts the stopped transport", song.playing || transport_req == 1u);
    song.rec = 0;
    stop();
}

/* ---- R5: PRESETS loads on STEP and PARAMETER LOCKS (owner 2026-11-08) ---- */
static void r5(void)
{
    track_t *t = &trk[0];
    static const uint32_t G[2] = {GR_ROLL, GR_EVENTS};
    static const char *const N[2] = {"STEP", "PARAMETER LOCKS"};
    uint32_t k, pre, locks, armed_locks;
    step_t was[NSTEP];
    for (k = 0; k < 2u; k++) {
        char b[96];
        boot(); setup(0, 0, V_POLY); song.sel = 0;
        go_page(G[k]); frames(100);
        memcpy(was, t->step, sizeof was);
        pre = t->preset; locks = motion.count;
        turn(EN_PRESET, 1); frames(200);
        snprintf(b, sizeof b, "R5: PRESETS on %s loads the next sound", N[k]);
        CHECK(b, t->preset != pre);
        snprintf(b, sizeof b, "R5: ... on %s the steps and the parameter locks are unchanged", N[k]);
        CHECK(b, same(t, was) && motion.count == locks);
    }
    boot(); setup(0, 0, V_POLY); song.sel = 0;
    go_page(GR_PATS); frames(100);
    pre = t->preset;
    turn(EN_PRESET, 1); frames(200);
    CHECK("R5: PRESETS on PATTERNS still does nothing", t->preset == pre);
    /* armed and playing, from STEP: the load records no lock and leaves the recording on */
    boot(); setup(0, 0, V_POLY); song.sel = 0;
    go_page(GR_ROLL); frames(100);
    start(); song.rec = 1; run(200);
    memcpy(was, t->step, sizeof was);
    pre = t->preset; armed_locks = motion.count;
    turn(EN_PRESET, 1); run(400);
    CHECK("R5: armed and playing, PRESETS on STEP loads the sound", t->preset != pre);
    CHECK("R5: ... records no parameter lock, keeps REC armed and the transport running", motion.count == armed_locks && song.rec == 1u && song.playing);
    song.rec = 0;
    stop();
}

int main(void)
{
    h1(); h2(); h2b(); h3(); h4(); h5(); r4(); r5();
    printf("liverec: %u checks, %u FAIL\n", checks, failures);
    return failures ? 1 : 0;
}
