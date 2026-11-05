/* SPDX-License-Identifier: GPL-3.0-only */
/* PLAY STOP while TIDE plays: nothing may keep sounding (owner, build 77a6df2: "sound is even playing when the stop button is pressed").
 * A short press of PLAY / STOP while running stops at once (a plain STOP: the sequencer's and the arp's notes get their note-offs and ring out with
 * their own release and FX tails only); a double press is STOP ALL (every voice faded out). After the stop request is taken:
 *   NOTE-ON  no voice starts (voice.c vage, one count a voice_start)
 *   GATE     no voice keeps its gate (a note held: sustaining), and no FM6 carrier keeps its key, a few blocks after the request; no gated voice without an owner (the sequencer, the arp, a key) while playing
 *   VOICES   every voice has ended within its envelope release (the formula of env_tick; FM6: within 44 s); after STOP ALL within 0.2 s
 *   LEVEL    the output is under -70 dBFS within the last voice + the FX tail of the same multi let go the same way (twice, 3 s at least); after STOP ALL within 0.5 s
 * Setups: the factory multis under TIDE (STYLE DUB: JRNY, WAIT 2, LEAD, POLY GENTLE, SYNC SOFT, ARC RISE; PEAK: ARC DROP, LEAD; 1 and 16 bar trips; TIDE idle),
 * stopped at many moments after PLAY (mid step, mid glide, in WAIT, on the bar line), by a plain STOP and by STOP ALL. The failing cases are counted by
 * cause. STOPHANG_V=1 lists every failing case; STOPHANG_CASES=n sets the stop moments per setup (10); STOPHANG_MULTIS=0 skips the sweep of all sixteen multis.
 * Keys: a chord played and lifted with the PLAY STOP press (M02, M04: HOLD ARP latches it on a SEQ arp; M14 EERIE SIGNAL: a latched pad). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include <math.h>

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

#define SEC ((uint32_t)(FS / CTL))                      /* control blocks a second */
static double rms_db(double acc, uint32_t n) { return acc > 0 && n ? 10.0 * log10(acc / n / (32768.0 * 32768.0)) : -200.0; }

/* ------------------------------------------------------------------ the engine: frames and audio together */
static uint32_t macc;                                    /* the audio blocks that fall in a 16 ms frame */
static double blk_db;                                    /* the last block's level */
static uint32_t blk_n;                                   /* control blocks rendered since the setup */
/* a gated voice whose note nothing accounts for (the sequencer's notes, the arp's, a held key): a hung note, whose note-off nobody will send */
static uint32_t orphans, orphan_k, orphan_note;
static int accounted(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < t->seq_n && i < 4u; i++)
        if (t->seq_notes[i] == note)
            return 1;
    if ((ARP_UP(t) && t->arp_note == note) || ((t->arp_nz & 1u) && note == 0u))
        return 1;
    for (i = 0; i < 3u; i++)
        if (t->arp_ch[i] == note || ((t->arp_nz >> (i + 1u)) & 1u && note == 0u))
            return 1;
    for (i = 0; i < t->nheld && i < 16u; i++)
        if (t->held[i] == note)
            return 1;
    return 0;
}
static void check_orphans(void)
{
    uint32_t k, i;
    for (k = 0; k < NSHOWN; k++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[k].v[i];
            if (v->active && v->gate && !accounted(&trk[k], v->note)) {
                if (!orphans) { orphan_k = k; orphan_note = v->note; }
                orphans++;
            }
        }
}
static uint32_t key_chord, key_on_blk, key_off_blk;      /* the player's keys: a chord down from block key_on_blk to key_off_blk */
static void block(void)
{
    static int32_t out[2 * CTL];
    uint32_t i;
    double e = 0;
    fm1_in.notes = blk_n >= key_on_blk && blk_n < key_off_blk ? key_chord : 0u;
    mix_block(out, CTL);
    while (wave_service())
        ;
    if (!fm1_in.notes && !kb_prev)                       /* (a key down owns its voices: kb_note, the chord tables) */
        check_orphans();
    for (i = 0; i < CTL; i++) { double l = out[2 * i], r = out[2 * i + 1]; e += l * l + r * r; }
    blk_db = rms_db(e, 2u * CTL);
    blk_n++;
    macc += 1000u * CTL;
    if (macc >= (uint32_t)FS * 16u) {                    /* a 16 ms main-loop pass: the UI, TIDE's poll (glide, SEQ MORPH) */
        macc -= (uint32_t)FS * 16u;
        frame();
    }
}
static void run_ms(uint32_t ms)
{
    uint32_t n = (uint32_t)((uint64_t)ms * FS / 1000u / CTL), i;
    for (i = 0; i < n; i++)
        block();
}
static void tap_play(void)                               /* the PLAY STOP button, as a hand: down, a frame, up, a frame (ui_input.c play_press) */
{
    fm1_in.buttons |= 1u << panel.btn[B_PLAY];
    host_pressed |= 1u << panel.btn[B_PLAY];
    frame();
    fm1_in.buttons &= ~(1u << panel.btn[B_PLAY]);
    frame();
}

/* ------------------------------------------------------------------ one setup */
enum { ST_DUB, ST_PEAK, ST_IDLE };                       /* TIDE: STYLE DUB, STYLE PEAK, TIDE idle (no glide) */
typedef struct { uint32_t slot, style, time; const char *name; uint32_t keys; } setup_t;   /* keys: 0 none, 1 a chord played 0.3 .. 2.5 s, 2 held until the stop */

static void setup_run(const setup_t *s)
{
    uint32_t k, i;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(s->slot);
    song.master_q12 = 2048;                              /* (felucca_init: the default MASTER) */
    if (getenv("STOPHANG_PLAY"))
        printf("  loaded slot %u: HOLD %d %d, AMODE %d %d, VOICE %d %d, mode %d, G_CLOCK %d\n", s->slot, (int)trk[0].p[P_AHOLD], (int)trk[1].p[P_AHOLD], (int)trk[0].p[P_AMODE], (int)trk[1].p[P_AMODE], (int)trk[0].p[P_VOICE], (int)trk[1].p[P_VOICE], (int)duell_mode, (int)song.g[G_CLOCK]);
    for (i = 0; i < 40; i++)
        block();
    macc = 0;
    gl.on = gl.armed = 0;
    transport_req = 0;
    song.rec = 0;
    mods_off = 0;
    for (k = 0; k < NTRK; k++)
        if (k < NSHOWN)
            trk[k].p[P_MUTE] = 0;
    morph.target = MT_BOTH;
    if (s->style != ST_IDLE) {
        tide_style_apply(s->style == ST_PEAK ? STY_PEAK : STY_DUB);
        gl.time = (uint8_t)s->time;                      /* 0: 1 bar a trip, 4: 16 bars */
        glide_launch(1u, 3u, 0);                         /* the TIDE page's K1: PLAY starts with it */
    } else {
        sqm_auto = 0;
        transport_req = 1u;                              /* plain PLAY */
    }
    blk_n = 0;
    key_on_blk = key_off_blk = 0;
    if (s->keys) {
        key_chord = 0;
        for (k = 0; k < 27u; k++)
            if (kb_map(&trk[0], k) == 60u || kb_map(&trk[0], k) == 64u || kb_map(&trk[0], k) == 67u)
                key_chord |= 1u << k;
        key_on_blk = SEC * 3u / 10u;
        key_off_blk = s->keys == 1 ? SEC * 5u / 2u : 0xFFFFFFFFu;
    }
}

/* ------------------------------------------------------------------ the oracle: what a patch rings out with when its notes are simply let go */
typedef struct { double voice_s, tail_s, pred_s; } free_t;
static uint32_t voices_alive(void)
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            n += trk[k].v[i].active != 0;
    return n;
}
static uint32_t voices_gated(void)
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            n += trk[k].v[i].active && trk[k].v[i].gate;
    return n;
}
/* the seconds a voice of track t takes to go from full to nothing in its release now (env_tick stage 3: x (1 - esk[2] / 65536) a block, down to 2^-12) */
static double rel_s(const track_t *t)
{
    double k = (double)t->esk[2] / 65536.0;
    return k <= 0.0 ? 1e9 : log(4096.0) / -log(1.0 - (k > 0.999 ? 0.999 : k)) * CTL / FS;
}
static free_t free_tail(uint32_t slot)
{
    static const uint8_t CH1[3] = {48, 55, 64}, CH2[3] = {60, 67, 72};
    free_t f = {0, 0, 0};
    setup_t s = {slot, ST_IDLE, 0, "free"};
    uint32_t i, b, lim = 60u * SEC, v_end = 0, l_end = 0, hold = 3u * SEC;
    setup_run(&s);
    transport_req = 0;
    block();
    for (i = 0; i < 3; i++) { trk_note_on(&trk[0], CH1[i], 100); trk_note_on(&trk[1], CH2[i], 100); }
    for (b = 0; b < hold; b++)
        block();
    for (i = 0; i < 3; i++) { trk_note_off(&trk[0], CH1[i]); trk_note_off(&trk[1], CH2[i]); }
    f.pred_s = rel_s(&trk[0]) > rel_s(&trk[1]) ? rel_s(&trk[0]) : rel_s(&trk[1]);
    for (b = 0; b < lim; b++) {
        block();
        if (voices_alive()) v_end = b + 1u;
        if (blk_db > -70.0) l_end = b + 1u;
    }
    f.voice_s = (double)v_end * CTL / FS;
    f.tail_s = (double)l_end * CTL / FS;
    return f;
}

/* ------------------------------------------------------------------ one stop */
enum { C_NOTEON, C_GATE, C_ORPHAN, C_VOICES, C_LEVEL, C_N };
static const char *const CAUSE[C_N] = {"note-on after the stop", "voice still gated", "hung note (gated, nothing owns it)", "voice alive past the release", "level over -70 dBFS past the tail"};
static uint32_t fails[C_N], cases, bad_cases, verbose;
typedef struct { uint32_t noon, gate, orph, vend, lvl; double v_s, l_s, bound_v, bound_l; uint32_t gated_blocks; } res_t;

static uint32_t track_alive(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[k].v[i].active != 0;
    return n;
}

/* a released FM6 voice (gate 0) whose carriers' envelopes still hold the key (down): the note-off never reached its operators */
static uint32_t fm6_keyed(uint32_t k)
{
    uint32_t i, o, n = 0;
    if (!ENGINES[trk[k].engine]->ownenv)
        return 0;
    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &trk[k].v[i];
        fm6_note_t *nt;
        uint32_t car;
        if (!v->active || v->gate)
            continue;
        nt = fm6_note_of(&trk[k], v);
        if (!nt || !nt->live)
            continue;
        car = fm6_carriers(fm6_eff[k].alg);
        for (o = 0; o < 6u; o++)
            if (((car >> o) & 1u) && nt->env[o].down && nt->env[o].ix < 4u)
                n++;
    }
    return n;
}
static void dump_voices(uint32_t b)
{
    uint32_t kk, ii, o;
    for (kk = 0; kk < NSHOWN; kk++)
        for (ii = 0; ii < NVOICE; ii++) {
            const voice_t *v = &trk[kk].v[ii];
            if (!v->active)
                continue;
            printf("  dump +%us trk %u eng %u voice %u note %u stage %u gate %u env %d esk2 %u (formula %.2f s)\n", b / SEC, kk, trk[kk].engine, ii, v->note, v->stage, v->gate,
                   (int)v->env, (unsigned)trk[kk].esk[2], rel_s(&trk[kk]));
            if (ENGINES[trk[kk].engine]->ownenv) {
                fm6_note_t *n = fm6_note_of(&trk[kk], v);
                uint32_t car = fm6_carriers(fm6_eff[kk].alg);
                for (o = 0; o < 6u; o++) {
                    const uint8_t *op = fm6_eff[kk].p + o * FP_OP;
                    if (((car >> o) & 1u) && n)
                        printf("     op%u CAR ix %u down %u level %d gain_out %d | R %u %u %u %u L %u %u %u %u\n", o + 1, n->env[o].ix, n->env[o].down, (int)n->env[o].level,
                               (int)n->op[o].gain_out, op[FP_R1], op[FP_R1 + 1], op[FP_R1 + 2], op[FP_R1 + 3], op[FP_L1], op[FP_L1 + 1], op[FP_L1 + 2], op[FP_L1 + 3]);
                }
            }
        }
}

/* stop_ms after PLAY's request; all: STOP ALL. Returns the figures */
static res_t one_stop(const setup_t *s, uint32_t stop_ms, int all, const free_t *fr)
{
    res_t r = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t b, k, lim = 45u * SEC, v_end[NSHOWN] = {0, 0}, l_end = 0, vage0, started = 0, dump = getenv("STOPHANG_DUMP") != 0;
    double rmax[NSHOWN] = {0, 0}, fx;
    setup_run(s);
    orphans = 0;
    { uint32_t v0 = vage; run_ms(stop_ms); if (getenv("STOPHANG_PLAY")) printf("  play: %s %u ms: %u note-ons, playing %d, arp modes %d %d, nheld %u %u, song.rec %d\n", s->name, stop_ms, (unsigned)(vage - v0), (int)song.playing, (int)trk[0].p[P_AMODE], (int)trk[1].p[P_AMODE], trk[0].nheld, trk[1].nheld, (int)song.rec); }
    if (key_off_blk > blk_n)
        key_off_blk = blk_n;                            /* the hand lifts with the press */
    tap_play();                                         /* STOP (the request is taken in the next block) */
    if (all) {
        run_ms(60);
        tap_play();                                     /* the second press within 300 ms: STOP ALL */
    }
    if (transport_req != 2u && transport_req != 4u && song.playing)
        printf("stophang: NOTE: no stop request after the press (%s %u ms)\n", s->name, stop_ms);
    block();                                            /* the request taken here (events_block) */
    block();
    if (song.playing)
        r.noon += 1000u;                                /* (the transport must have stopped) */
    vage0 = vage;
    for (b = 0; b < lim; b++) {
        block();
        if (vage != vage0) { started += vage - vage0; vage0 = vage; }
        for (k = 0; k < NSHOWN; k++) {
            double x = rel_s(&trk[k]);
            if (x > rmax[k]) rmax[k] = x;
            if (track_alive(k)) v_end[k] = b + 1u;
            if (b > 2u && fm6_keyed(k))
                r.gated_blocks++;
        }
        if (voices_gated() && b > 2u) r.gated_blocks++;
        if (dump && (b == SEC || b == 3u * SEC || b == 8u * SEC))
            dump_voices(b);
        if (dump && (b == SEC || b == 3u * SEC))
            printf("  state +%us: nheld %u %u arp_phys %u %u hold %d %d amode %d %d kb_prev %x notes %x playing %d seq_n %u %u arp_note %u %u\n", b / SEC, trk[0].nheld, trk[1].nheld, trk[0].arp_phys, trk[1].arp_phys, (int)trk[0].p[P_AHOLD], (int)trk[1].p[P_AHOLD], (int)trk[0].p[P_AMODE], (int)trk[1].p[P_AMODE], (unsigned)kb_prev, (unsigned)fm1_in.notes, (int)song.playing, trk[0].seq_n, trk[1].seq_n, trk[0].arp_note, trk[1].arp_note);
        if (blk_db > -70.0) l_end = b + 1u;
    }
    r.orph = orphans != 0;
    if (orphans && verbose)
        printf("stophang: hung note: %s stop at %u ms: track %u note %u gated with no owner (%u blocks)\n", s->name, stop_ms, orphan_k, orphan_note, orphans);
    r.noon += started;
    r.gate = r.gated_blocks != 0;
    for (k = 0; k < NSHOWN; k++) {
        double alive = (double)v_end[k] * CTL / FS;
        /* a classic voice rings out with its ADSR release (the formula, 25 % and 0.2 s over); an FM6 voice with its operators' (R4, whatever the patch has: only a voice that
         * never ends in 45 s is held) */
        double lim_s = all ? 0.2 : ENGINES[trk[k].engine]->ownenv ? 44.0 : rmax[k] * 1.25 + 0.2;   /* STOP ALL: every voice faded out at once (~3 ms) */
        if (alive > r.v_s) r.v_s = alive;
        if (lim_s > r.bound_v) r.bound_v = lim_s;
        r.vend |= alive > lim_s;
    }
    r.l_s = (double)l_end * CTL / FS;
    fx = fr->tail_s > fr->voice_s ? (fr->tail_s - fr->voice_s) * 1.5 + 1.0 : 1.0;       /* the effects' tail of the multi let go (measured), 1.5 x + 1 s */
    fx = fx > 3.0 ? fx : 3.0;
    r.bound_l = r.v_s + 2.0 * fx;                       /* the output follows the last voice by the effects' tail (twice the measured one: the stop leaves more in the lines than the test chord) */
    if (all)                                            /* STOP ALL: the effect lines flushed with a ~22 ms fade (fx.c snd_panic) */
        r.bound_l = 0.5;
    r.lvl = r.l_s > r.bound_l;
    return r;
}

static void report(const setup_t *s, uint32_t stop_ms, int all, const res_t *r, const free_t *fr)
{
    uint32_t c[C_N] = {r->noon != 0, r->gate, r->orph, r->vend, r->lvl}, k, any = 0;
    cases++;
    for (k = 0; k < C_N; k++) {
        fails[k] += c[k];
        any |= c[k];
    }
    bad_cases += any;
    if (any && verbose)
        printf("stophang: FAIL %-22s stop at %5u ms %-8s: note-ons %u, gated for %u blocks, voices alive %.2f s (release allows %.2f), level over -70 dB %.2f s (allowed %.2f)\n",
               s->name, stop_ms, all ? "STOP ALL" : "STOP", r->noon, r->gated_blocks, r->v_s, r->bound_v, r->l_s, r->bound_l);
}

int main(void)
{
    static const setup_t S[] = {
        {1, ST_DUB, 4, "M02 DUB 16bar"}, {1, ST_DUB, 0, "M02 DUB 1bar"}, {1, ST_PEAK, 0, "M02 PEAK 1bar"}, {1, ST_PEAK, 3, "M02 PEAK 8bar"}, {1, ST_IDLE, 0, "M02 TIDE idle"},
        {2, ST_DUB, 4, "M03 DUB 16bar"}, {2, ST_DUB, 0, "M03 DUB 1bar"}, {2, ST_PEAK, 0, "M03 PEAK 1bar"}, {2, ST_IDLE, 0, "M03 TIDE idle"},
        {1, ST_DUB, 0, "M02 DUB keys lifted", 1}, {1, ST_DUB, 0, "M02 DUB keys to STOP", 2}, {1, ST_IDLE, 0, "M02 idle keys", 2}, {2, ST_DUB, 0, "M03 DUB keys to STOP", 2}, {2, ST_PEAK, 0, "M03 PEAK keys lifted", 1},
        {3, ST_DUB, 0, "M04 DUB keys to STOP", 2}, {13, ST_DUB, 0, "M14 DUB keys to STOP", 2}, {4, ST_DUB, 0, "M05 DUB keys to STOP", 2}, {8, ST_DUB, 0, "M09 DUB keys to STOP", 2},
    };
    static const char *const NAMES[16] = {"M01", "M02", "M03", "M04", "M05", "M06", "M07", "M08", "M09", "M10", "M11", "M12", "M13", "M14", "M15", "M16"};
    static setup_t all16[16];
    uint32_t i, j, nst = getenv("STOPHANG_CASES") ? (uint32_t)atoi(getenv("STOPHANG_CASES")) : 10u, n16 = getenv("STOPHANG_MULTIS") && getenv("STOPHANG_MULTIS")[0] == '0' ? 0u : 16u;
    free_t fr[16];
    verbose = getenv("STOPHANG_V") != 0;
    for (i = 0; i < 16u; i++) {
        fr[i] = free_tail(i);
        printf("stophang: %s free ring-out: voices %.2f s (formula %.2f), output under -70 dBFS after %.2f s\n", NAMES[i], fr[i].voice_s, fr[i].pred_s, fr[i].tail_s);
        fflush(stdout);
    }
    for (i = 0; i < sizeof S / sizeof *S; i++) {
        uint32_t before = bad_cases, c0 = cases;
        for (j = 0; j < nst; j++) {
            /* stop moments: spread over 5 bars and then some, odd ms so that no two land on the same step, bar line or block edge */
            uint32_t ms = 137u + j * 1013u + (j * j * 71u) % 523u + j * j * j * 37u, all = j % 5u == 4u;
            res_t r = one_stop(&S[i], ms, all, &fr[S[i].slot]);
            report(&S[i], ms, all, &r, &fr[S[i].slot]);
        }
        printf("stophang: %-16s %u stops, %u failed\n", S[i].name, cases - c0, bad_cases - before);
        fflush(stdout);
    }
    if (n16)
        for (i = 0; i < 16u; i++) {
            uint32_t before = bad_cases, c0 = cases;
            all16[i].slot = i; all16[i].style = ST_DUB; all16[i].time = 0; all16[i].name = NAMES[i];
            for (j = 0; j < nst / 2u + 1u; j++) {
                uint32_t ms = 311u + j * 1777u + (j * j * 131u) % 719u + j * j * j * 53u, all = j % 4u == 3u;
                res_t r = one_stop(&all16[i], ms, all, &fr[i]);
                report(&all16[i], ms, all, &r, &fr[i]);
            }
            printf("stophang: %-16s DUB 1bar %u stops, %u failed\n", NAMES[i], cases - c0, bad_cases - before);
            fflush(stdout);
        }
    printf("stophang: %u stops, %u failed (", cases, bad_cases);
    for (i = 0; i < C_N; i++)
        printf("%s%s %u", i ? ", " : "", CAUSE[i], fails[i]);
    printf(")\n");
    if (bad_cases) {
        printf("stophang: FAIL\n");
        return 1;
    }
    printf("stophang: ok\n");
    return 0;
}
