/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL MULTI (project.c multi.c, SAVE > MULTI, SPEC 5b.4) against simulated NOR flash:
 *   1. SAVE (KNOB 3, OCT+, the NAME screen, OCT+) of a distinct state: part 1 and part 2 on other engines (part 2 FM6 with
 *      an edited patch), a pattern on each, an automation lock, MODE LAYER, TEMPO, DLY TIME, MORPH TARGET BOTH and SMART
 *      OFF, End B rolled on both parts, the fader moved; after a power cycle (RAM gone, flash read again) LOAD (KNOB 2, OCT+)
 *      gives the same parameters, engines, steps, patch, automation, globals, MODE, TARGET, settings, End B (MU_VER 8: the
 *      values it sounded), the fader at A; the writes stay in the multi store's two ranges;
 *   2. sixteen slots, each its own, through a power cycle; rewriting one leaves its pair partner as it was; project_erase
 *      (DUELL 1.1: no ERASE card on the page) brings the factory multi back;
 *   3. a power cut during a slot write leaves both multis of the pair as they were (A/B, the commit record last);
 *   4. MU_VER 8 records: round trips, refused versions and damage, the worst multi;
 *   5. the factory multis; 6. (0.2.12) 0.2.11's six (0.2.13: M01 M02 M07..M10) played from the keys in LAYER, the transport stopped and with PLAY: both parts answer
 *      a key, follow another key, stop or stay latched on the release, and nothing plays without a key (TIDE: an ARP SEQ part's stand-in plays on PLAY).
 *   build/host/multi_test            (DUELL/host_tests.sh "multi") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include "scenes.h"                                   /* (U2 B: the STRESS TEST, once M16, is the scene tests/scenes/M16_STRESS_TEST.bin) */

static uint8_t nor[0x100000];
static long cut_bytes = -1;                         /* the power goes after that many programmed bytes */
static int dead, watch;
static uint32_t stray;                              /* (watch) bytes erased / programmed outside the multi store */
static void hit(uint32_t off, uint32_t n)
{
    if (watch && !((off >= 0x97000u && off + n <= 0x9F000u) || (off >= 0xCE000u && off + n <= 0xD6000u)))
        stray += n;
}
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead)
        return -8;
    hit(off, 4096u);
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    hit(off, n);
    for (i = 0; i < n; i++) {
        if (dead || (cut_bytes >= 0 && cut_bytes-- == 0)) {
            dead = 1;
            return -9;
        }
        nor[off + i] &= s[i];
    }
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static char nm_a[24];
static int bad;
static void ck(const char *what, int ok)
{
    printf("multi: %-104s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void power_cycle(void)                        /* the RAM is gone; the flash read again at power-on */
{
    uint32_t k;
    dead = 0;
    cut_bytes = -1;
#if DUELL_TIDE
    {   /* (TIDE's SEQ MORPH is RAM only: after a power cycle it starts as on a new unit, not from an earlier section's ends and fader) */
        static const sqm_part_t SQM0 = {0};
        for (k = 0; k < NSHOWN; k++)
            sqm[k] = SQM0;
        memset(sqm_pat, 0, sizeof sqm_pat);
    }
#endif
    ui_power_on();
    for (k = 0; k < NTRK; k++)                       /* (the patches too: another sound than any saved) */
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
/* MU_VER 8 keeps End B as the values it sounds (morph_bv_make: morph_effective_x at 127, each in its stored code): part pi's End B now in
 * those codes, to compare a saved state with a loaded one */
static void endb_codes(uint32_t pi, int16_t *c)
{
    int16_t v[EXT_P_LEN];
    uint32_t w;
    morph_effective_x(pi, 1u, v);
    for (w = 0; w < EXT_P_LEN; w++)
        c[w] = mbv_wid(w) && !mbv_never(w) ? (int16_t)wv_code(mph[pi].eng, w, v[w]) : 0;
}
static int endb_set(uint32_t pi)                     /* End B is not the Original (a factory multi's End B as values: MU_VER 8, no seed) */
{
    int16_t a[EXT_P_LEN], b[EXT_P_LEN];
    uint32_t w;
    morph_effective_x(pi, 0u, a);
    morph_effective_x(pi, 1u, b);
    for (w = 0; w < EXT_P_LEN; w++)
        if (mbv_wid(w) && !mbv_never(w) && wv_code(mph[pi].eng, w, a[w]) != wv_code(mph[pi].eng, w, b[w]))
            return 1;
    return 0;
}
static void pattern(track_t *t, uint32_t k)          /* a pattern seeded by k, every field of a step used */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        memset(s, 0, sizeof *s);
        step_clear(s);
        if ((i + k) % 3u == 2u)
            continue;                                /* (rests: the record leaves them out) */
        s->time = (i + k) % 7u == 3u ? ST_TIE : ST_NOTE;
        s->n = (uint8_t)(1u + (i + k) % 3u);
        s->note[0] = (uint8_t)(36u + (i * 5u + k * 3u) % 60u);
        s->note[1] = s->n > 1u ? (uint8_t)(s->note[0] + 4u) : 0u;
        s->note[2] = s->n > 2u ? (uint8_t)(s->note[0] + 7u) : 0u;
        s->flags = (uint8_t)((i + k) & 3u);
        step_set_ratchet(s, 1u + (i + k) % 4u);
        step_set_nudge(s, (int32_t)((i * 3u + k) % 16u) - 8);
        s->vel = (uint8_t)(20u + (i * 5u + k) % 100u);
        s->probability = (uint8_t)((i * 11u + k) % 102u);
    }
    t->p[P_SLEN] = (int16_t)(8u + (k * 7u) % 56u);
}
static void press_k(uint32_t knob)                   /* an action knob picked, OCT+ */
{
    turn(knob, 1);
    press(B_OCTUP);
}

/* 6 (DUELL 0.2.12): 0.2.11's six multis (0.2.13: M01 M02 M07..M10) are played from the keys. A multi loaded on a unit in LAYER (MODE RECALL OFF, the default), the
 * transport stopped: a key (or a chord) held for a while, then let go, the audio rendered block by block (mix_block); each part's
 * NOTE-ONS are the notes that become gated in a block (voice gate set, not set the block before). play_run returns the onsets of
 * part pi in on[pi][] (note, block), *n[pi] of them; *held1 / *held2: a part still gated after the release (latched); *late: onsets
 * after the release (from one step on); first[pi]: the block of its first onset */
#define PR_MAX 400u
typedef struct { uint8_t note[PR_MAX]; uint16_t blk[PR_MAX]; uint32_t n, first, late, gated_after; } pr_t;
static uint32_t pr_key(uint32_t note)                /* the key whose note (a power-on's octave and TRN 0) is note */
{
    uint32_t k, r = 27u;
    int16_t trn = trk[0].p[P_TRANS];                 /* (1.1, item 143: the power-on sound #1 BEHIVE carries TRANSPOSE -12; this finds the key at TRN 0) */
    trk[0].p[P_TRANS] = 0;
    for (k = 0; k < 27u && r == 27u; k++)
        if (kb_map(&trk[0], k) == note)
            r = k;
    trk[0].p[P_TRANS] = trn;
    return r;
}
static int pr_recall;                               /* (0.2.23: the load with MODE RECALL ON: the multi's split applied) */
static void play_run(uint32_t slot, uint32_t keys, uint32_t hold_blocks, uint32_t after_blocks, int play, pr_t *r)
{
    static int32_t out[2 * CTL];
    uint8_t prev[NSHOWN][128], cur[128];
    uint32_t b, pi, v, n;
    power_cycle();
    duell_mode_req = DM_LAYER;                       /* (the owner's unit: LAYER; MODE RECALL OFF, unless pr_recall) */
    if (pr_recall)
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(slot);
    ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
    memset(prev, 0, sizeof prev);
    memset(r, 0, NSHOWN * sizeof *r);
    for (pi = 0; pi < NSHOWN; pi++)
        r[pi].first = 0xFFFFu;
    for (b = 0; b < 8u; b++) {                       /* (the MODE taken, the tables built) */
        mix_block(out, CTL);
        while (wave_service())
            ;
    }
    if (play)
        transport_req = 1;
    for (b = 0; b < hold_blocks + after_blocks; b++) {
        fm1_in.notes = b < hold_blocks ? keys : 0u;
        mix_block(out, CTL);
        while (wave_service())
            ;
        for (pi = 0; pi < NSHOWN; pi++) {
            memset(cur, 0, sizeof cur);
            for (v = 0; v < NVOICE; v++)
                if (trk[pi].v[v].active && trk[pi].v[v].gate)
                    cur[trk[pi].v[v].note & 127u] = 1;
            for (n = 0; n < 128u; n++)
                if (cur[n] && !prev[pi][n]) {
                    if (r[pi].n < PR_MAX) {
                        r[pi].note[r[pi].n] = (uint8_t)n;
                        r[pi].blk[r[pi].n] = (uint16_t)b;
                    }
                    r[pi].n++;
                    if (r[pi].first == 0xFFFFu)
                        r[pi].first = b;
                    if (b >= hold_blocks + 2u * (uint32_t)(FS / CTL) / 10u)   /* (more than 1/5 s after the release: no tail of a step) */
                        r[pi].late++;
                }
            if (b == hold_blocks + after_blocks - 1u)
                for (n = 0; n < 128u; n++)
                    r[pi].gated_after += cur[n];
            memcpy(prev[pi], cur, sizeof cur);
        }
    }
    fm1_in.notes = 0;
    if (play)
        transport_req = 2;
    mix_block(out, CTL);
}
/* the onsets of a and b the same, b's notes d semitones higher (the same notes at the same blocks otherwise) */
static int pr_shifted(const pr_t *a, const pr_t *b, int32_t d)
{
    uint32_t i;
    if (a->n != b->n || !a->n)
        return 0;
    for (i = 0; i < a->n && i < PR_MAX; i++)
        if (a->blk[i] != b->blk[i] || (int32_t)b->note[i] != (int32_t)a->note[i] + d)
            return 0;
    return 1;
}
static void keys_drive(void)
{
    static pr_t r[NSHOWN], q[NSHOWN], p[NSHOWN], z[NSHOWN];
    uint32_t c4, d4, e4, g4, k, ok, sec = (uint32_t)(FS / CTL), chord;
    memset(nor, 0xFF, sizeof nor);                   /* (the factory multis, nothing saved) */
    power_cycle();
    c4 = pr_key(60); d4 = pr_key(62); e4 = pr_key(64); g4 = pr_key(67);
    chord = 1u << c4 | 1u << e4 | 1u << g4;
    if (0) {
    uint32_t c4 = pr_key(60), d4 = pr_key(62), e4 = pr_key(64), g4 = pr_key(67), k, ok, sec = (uint32_t)(FS / CTL);
    }
    ck("(the keys C4 D4 E4 G4 found on a power-on's keyboard)", c4 < 27u && d4 < 27u && e4 < 27u && g4 < 27u);
    /* the grooves (D31 slots): M02 DUB CHORD, M04 NIGHT ORGAN, M06 WARM ECHO, M07 MINIMAL 12, M09 DEEP 332 (arps on both parts; the pads of the new ones play one or two chords a bar,
     * so their part 2 only has to play): one key, 2 s, then 1 s let go, the transport stopped. P1[j]: part 1's one note at once (part 1 plays the key itself), 0: part 1 is an arp too
     * (D31: all five are arps). AMB: the pads, WHISPER TIDE, TINE STRINGS, FINAL REEL, COLD CHAPEL (their D31 slots) */
    /* (reel, 2026-11-10: the ARP SEQ pairs of the fifteen new multis: M05 ORGAN HOUSE, M07 DETROIT STAB, M09 MINIMAL 12, M13 DEEP SEA ARP (M03 and M06 are plain patterns now);
     * AMB: no factory multi has a latched pad any more, so the owner's WHISPER TIDE (the factory M11 until the reel) is the test scene tests/scenes/reel/M11_WHISPER_TIDE.bin,
     * stored in M11 for its check and erased again; M06 PIANO HOUSE is the keyed pair (both parts played from the keys, HOLD OFF), as TINE STRINGS and FINAL REEL were) */
    static const uint8_t GRV[5] = {1, 4, 6, 8, 12}, P1[5] = {0, 0, 0, 0, 0}, AMB[2] = {10, 5};
    static const char *const GN[5] = {"M02 DUB CHORD", "M05 ORGAN HOUSE", "M07 DETROIT STAB", "M09 MINIMAL 12", "M13 DEEP SEA ARP"};
    uint32_t j;
    char line[160];
    for (j = 0; j < 5u; j++) {
        k = GRV[j];
        play_run(k, 1u << c4, 2u * sec, sec, 0, r);
        ok = !song.playing && r[0].n >= 1u && r[1].n >= (k == 1u ? 4u : 1u) && r[0].late == 0u && r[1].late == 0u && !r[0].gated_after && !r[1].gated_after;
        if (k == 1u)                                 /* (M02 DUB CHORD, re-voiced with the owner's leave: its pad is one note tied over four bars, so one note-on, and it may ring after the release; until then the owner's TIDAL SUB line, 4 or more) */
            ok = !song.playing && r[0].n >= 1u && r[1].n >= 1u;
        if (P1[j])                                   /* (part 1 plays the key itself (TRN), at once, one note for the hold) */
            ok &= r[0].first <= 1u && r[0].n == 1u && r[0].note[0] == P1[j];
        else                                         /* (part 1 an arp too) */
            ok &= r[0].n >= 2u && r[0].first < sec / 2u;
        ok &= r[1].first < sec / 2u;                 /* (part 2's arp from the key-down, on the internal clock) */
        printf("multi:   M%02u C4 held 2 s, stopped: part 1 %u note-ons (first in block %u), part 2 %u (first in block %u); after the release %u / %u\n",
               k + 1u, r[0].n, r[0].first, r[1].n, r[1].first, r[0].late, r[1].late);
        snprintf(line, sizeof line, k == 1u ? "%s, LAYER, STOPPED: a key plays both parts (no PLAY; the pad may ring on)" : "%s, LAYER, STOPPED: a key plays both parts (no PLAY), the release ends both", GN[j]);
        ck(line, ok);
        if (getenv("PR_DBG")) {
            uint32_t i, jj;
            for (jj = 0; jj < 2u; jj++) {
                printf("part %u:", jj + 1u);
                for (i = 0; i < r[jj].n && i < PR_MAX; i++) printf(" %u@%u", r[jj].note[i], r[jj].blk[i]);
                printf("\n");
            }
        }
        play_run(k, 1u << d4, 2u * sec, sec, 0, q);
        ck("  another key (D4): both parts play the same figure two semitones up (follows the keys)", pr_shifted(&r[0], &q[0], 2) && pr_shifted(&r[1], &q[1], 2));
        play_run(k, 0u, 2u * sec, 0u, 1, z);
#if DUELL_TIDE
        {   /* (TIDE, owner 2026-11-01: an ARP SEQ part plays on PLAY with no key, from its last chord or C4; the other parts stay silent) */
            uint32_t pi2;
            ok = 1;
            for (pi2 = 0; pi2 < NSHOWN; pi2++)
                ok &= arp_mode(&trk[pi2]) == AM_SEQ_ID ? z[pi2].n > 0u : z[pi2].n == 0u;
        }
#else
        ok = z[0].n == 0u && z[1].n == 0u;
#endif
        play_run(k, 1u << d4, 2u * sec, sec, 1, p);
        {
            static pr_t rr[NSHOWN];
            memcpy(rr, r, sizeof rr);
#if DUELL_TIDE
            {   /* (after the release, on PLAY, the stand-in goes on from the last chord: an ARP SEQ part compares the notes while the key is held) */
                uint32_t pi2;
                for (pi2 = 0; pi2 < NSHOWN; pi2++)
                    if (arp_mode(&trk[pi2]) == AM_SEQ_ID) {
                        while (p[pi2].n && p[pi2].n <= PR_MAX && p[pi2].blk[p[pi2].n - 1u] >= 2u * sec)
                            p[pi2].n--;
                        while (rr[pi2].n && rr[pi2].n <= PR_MAX && rr[pi2].blk[rr[pi2].n - 1u] >= 2u * sec)
                            rr[pi2].n--;
                    }
            }
#endif
            ck("  PLAY: no key, nothing plays (TIDE: an ARP SEQ part's stand-in does); a key with PLAY plays what it plays stopped",
               ok && pr_shifted(&rr[0], &p[0], 2) && pr_shifted(&rr[1], &p[1], 2));
        }
    }
    /* the pads (M05 WHISPER TIDE, M06 TINE STRINGS, M07 FINAL REEL, M13 COLD CHAPEL (the owner's original, 0.2.25: the bowl an arp on the latched chord)): a chord, then let go: both parts sound from the chord and stay latched (HOLD ALL);
     * WHISPER TIDE's choir is a latched arp that goes on after the release (0.2.25: M06 is the owner's TINE STRINGS, keyed, nothing latched),
     * 0.2.25: M07 is the owner's refined FINAL REEL, HOLD OFF on both parts: keyed, nothing latched, as M06 */
    for (j = 0; j < 2u; j++) {
        k = AMB[j];
        if (k == 10u)                                /* (reel: the latched pad is a test scene, saved in M11; the flash is read again by play_run's power cycle) */
            scene_put(10, "reel/M11_WHISPER_TIDE");
        play_run(k, chord, sec, 3u * sec, 0, r);
        ok = r[0].n >= 3u && r[0].first <= 1u && r[1].n >= 1u && r[0].gated_after >= 3u && r[1].late >= 1u;
        if (k == 5u)                                 /* (reel: M06 PIANO HOUSE: both parts keyed, nothing latched (HOLD OFF), the release ends both; 0.2.25 .. b7 the owner's TINE STRINGS and FINAL REEL) */
            ok = r[0].n >= 3u && r[0].first <= 1u && r[1].n >= 3u && r[0].gated_after == 0u && r[1].gated_after == 0u && r[1].late == 0u;
        printf("multi:   M%02u C4 E4 G4 1 s, 3 s let go: part 1 %u note-ons, %u notes still gated; part 2 %u (%u after the release), %u gated\n",
               k + 1u, r[0].n, r[0].gated_after, r[1].n, r[1].late, r[1].gated_after);
        ck(k == 10u ? "WHISPER TIDE (the owner's, a test scene in M11 since the reel): a chord latches the pad, the choir picks its notes and goes on after the release"
                    : "M06 PIANO HOUSE (reel): a chord plays the piano and the strings, nothing latched (HOLD OFF), the release ends both", ok);
        play_run(k, 0u, 2u * sec, 0u, 1, z);
        {   /* (U2 B: a plain pattern plays on PLAY whatever the build; an ARP SEQ part only with TIDE's stand-in; the classic arps (M05, M13) wait for keys) */
            uint32_t pi2, st2, n2;
            ok = 1;
            for (pi2 = 0; pi2 < NSHOWN; pi2++) {
                for (n2 = st2 = 0; n2 < NSTEP; n2++)
                    st2 += trk[pi2].step[n2].time == ST_NOTE && trk[pi2].step[n2].n;
                ok &= (arp_mode(&trk[pi2]) == AM_SEQ_ID ? DUELL_TIDE && z[pi2].n > 0u : (arp_mode(&trk[pi2]) == 0u && st2) ? z[pi2].n > 0u : z[pi2].n == 0u);
            }
        }
        ck("  PLAY without a key: the stored arrangement plays (patterns; ARP SEQ with TIDE's stand-in), a classic arp stays silent", ok);
        if (k == 10u) {                              /* (the scene leaves M11: the factory M11 ACID LINES is back for the checks below) */
            power_cycle();
            ck("  (the WHISPER TIDE scene erased from M11: the factory M11 ACID LINES is back)", project_erase(10u) == 0 && project_factory(10u) && str_eq(mu_names[10], "ACID LINES"));
        }
    }
    /* U2 B (1.1): the sequenced arrangements of PAD SPLIT, WHISPER TIDE, TINE STRINGS, FINAL REEL, HAZY TENSOR, CELLAR TAPE (M10 M11 M12 M16 M15 M03 since D31). PLAY alone starts the plain-pattern parts; the ARP SEQ
     * parts follow a transposed key (D4 two semitones above C4, the same blocks); a plain pattern part does not move with the key (a live key plays on top) */
    {
        static const uint8_t SQ[5][2] = {{9, 0}, {11, 1}, {15, 1}, {14, 0}, {10, 0}};   /* slot, the part: ARP SEQ (the first four), a plain pattern (the last). (reel, 2026-11-10: M10 SEVEN LOOP's HOLLOW POP (its IRON WORKS holds one note
                                                                                         * four bars), M12 ROBOT FUNK's VOWEL PAD, M16 TWO LOOPS' EVOLVING PAD, M15 BERLIN RUN's ARP PLUCK; the plain pattern is M11 ACID LINES' SQUELCH) */
        uint32_t m, pp, n;
        for (m = 0; m < 5u; m++) {
            k = SQ[m][0];
            pp = SQ[m][1];
            play_run(k, 1u << c4, (m < 4u ? 8u : 3u) * sec, sec / 2u, 0, r);
            play_run(k, 1u << d4, (m < 4u ? 8u : 3u) * sec, sec / 2u, 0, q);
            play_run(k, 0u, (m < 4u ? 8u : 3u) * sec, 0u, 1, z);
            play_run(k, 1u << d4, (m < 4u ? 8u : 3u) * sec, 0u, 1, p);
            if (m < 4u) {
                snprintf(line, sizeof line, "U2 B: M%02u part %u (ARP SEQ): a key plays the arrangement (%u notes), D4 two semitones above C4 on the same blocks%s", k + 1u, pp + 1u, r[pp].n,
                         DUELL_TIDE ? ", PLAY with no key plays it from C4" : "");
                ck(line, trk[pp].p[P_AMODE] == 15 && r[pp].n >= 2u && pr_shifted(&r[pp], &q[pp], 2) && (!DUELL_TIDE || z[pp].n >= 2u));
            } else {
                snprintf(line, sizeof line, "U2 B: M%02u part %u (plain pattern): PLAY alone plays it (%u notes), a held key does not move its notes", k + 1u, pp + 1u, z[pp].n);
                for (n = ok = 0; n < z[pp].n && n < PR_MAX; n++) {   /* (every note PLAY alone played is among the notes with the key held: the key adds its own) */
                    uint32_t i3;
                    for (i3 = 0; i3 < p[pp].n && i3 < PR_MAX && p[pp].note[i3] != z[pp].note[n]; i3++)
                        ;
                    ok += i3 < p[pp].n && i3 < PR_MAX;
                }
                ck(line, trk[pp].p[P_AMODE] == 0 && z[pp].n >= 2u && ok == (z[pp].n < PR_MAX ? z[pp].n : PR_MAX));
            }
        }
    }
    /* M04 and M10 (reel: LATE RHODES and SEVEN LOOP; before: NIGHT ORGAN and PAD SPLIT) with MODE RECALL ON (0.2.25: the owner's saved records have no split any more, SPLIT 0: a key below F4 and one
     * from F4 up both play both parts; until 0.2.24 the split at F4 gave part 1 alone below and part 2 alone above); each part adds its own TRN */
    {
        uint32_t e4 = pr_key(64), f4 = pr_key(65), sp;
        static const uint8_t SPS[2] = {3, 9};
        for (sp = 0; sp < 2u; sp++) {
            pr_recall = 1;
            play_run(SPS[sp], 1u << e4, sec, sec / 2u, 0, r);
            play_run(SPS[sp], 1u << f4, sec, sec / 2u, 0, q);
            pr_recall = 0;
            printf("multi:   M%02u split: E4 part 1 %u / part 2 %u note-ons; F4 part 1 %u / part 2 %u\n", SPS[sp] + 1u, r[0].n, r[1].n, q[0].n, q[1].n);
            ck(sp ? "M10 SEVEN LOOP (MODE RECALL ON, no split): E4 and F4 both play both parts"
                  : "M04 LATE RHODES (MODE RECALL ON, no split): E4 and F4 both play both parts",
               r[0].n >= 1u && r[1].n >= 1u && q[0].n >= 1u && q[1].n >= 1u && mix_split == 0u);
        }
        pr_recall = 1;
        play_run(10, 1u << e4, sec, sec / 2u, 0, r);         /* (reel: M11 ACID LINES has no arp on either part: the direct path, the transport stopped; until the reel the owner's PAD SPLIT, M10) */
        play_run(10, 1u << f4, sec, sec / 2u, 0, q);
        pr_recall = 0;
        ck("M11 ACID LINES: part 1 (SQUELCH, TRN -12) plays E4 at E3, part 2 (DEEP BLUE, TRN 0) plays F4 at F4 (no TRN of part 1 on part 2)",
           trk[0].p[P_TRANS] == -12 && trk[1].p[P_TRANS] == 0 && r[0].n >= 1u && r[0].note[0] == 52u && r[1].n >= 1u && r[1].note[0] == 64u && q[1].n >= 1u && q[1].note[0] == 65u);
        play_run(2, 1u << pr_key(60), sec, sec / 2u, 0, r);   /* M03 JUNO DEEP: the bass's own TRN -12 is heard from the keys (until the reel: the owner's FINAL REEL, M16, its strings TRN +12) */
        printf("multi:   M03 key C4: part 1 note %u, part 2 note %u (TRN %d / %d)\n", r[0].note[0], r[1].note[0], trk[0].p[P_TRANS], trk[1].p[P_TRANS]);
        ck("M03 JUNO DEEP: a key plays the pad at the key and the bass (TRN -12) an octave down",
           trk[0].p[P_TRANS] == 0 && trk[1].p[P_TRANS] == -12 && r[0].n >= 1u && r[1].n >= 1u && r[0].note[0] == 60u && r[1].note[0] + 12u == r[0].note[0]);
    }
}

static void editb_worst_multi(void)
{   /* the worst multi through the real SAVE: two FM6 parts, 64 steps each, 32 values set by hand on both, a full automation: the messages */
            uint32_t i2, kept = 0, w, pi;
            int rc, amsg;
            xb_edit_t gg[XB_MAX];
            for (pi = 0; pi < NSHOWN; pi++) {
                trk[pi].eng_req = trk[pi].engine = ENGI_FM6;
                for (i2 = 0; i2 < NSTEP; i2++) {
                    step_clear(&trk[pi].step[i2]);
                    trk[pi].step[i2].note[0] = (uint8_t)(30u + (i2 + pi) % 60u);
                    trk[pi].step[i2].n = 1;
                    trk[pi].step[i2].time = ST_NOTE;
                    trk[pi].step[i2].vel = (uint8_t)(40u + i2);
                }
                morph_ready(pi);
                morph_roll_seed(pi, 1u, 777u + pi);
                mph[pi].fader = 127;
                for (w = 0; w < EXT_P_LEN && morph_edits(pi, gg) < XB_MAX; w++)
                    if (morph_b_mode(pi, w) == 1u && !pin_get(morph_pins(&mph[pi]), w))
                        morph_b_set(pi, w, mph[pi].base[w]);
                mph[pi].fader = 0;
            }
            for (i2 = 0; i2 < MOTION_MAX; i2++)
                motion.event[i2] = (motion_event_t){(uint8_t)((i2 & 1u) << 6 | (i2 >> 1) % 64u), P_REV, (int8_t)(i2 % 100u)};
            motion.count = MOTION_MAX;
            motion.on = 3;
            ck("  the worst multi (2 FM6 parts, 64 steps each, 32 + 32 values set by hand, a full automation) saves: PARAMETER LOCKS CUT (MU_VER 8: End B and its marks always fit)",
               project_save_as(13u, "WORST") == 0 && msg_is("PARAMETER LOCKS CUT"));
            project_load(13u);
            frames(50);
            for (pi = 0; pi < NSHOWN; pi++)
                kept += morph_edits(pi, gg);
            amsg = motion.count;
            ck("  .. LOAD: all 64 values set by hand are back, and the part of the automation that fitted", kept == 2u * XB_MAX && motion.count > 0u && motion.count < MOTION_MAX);
            for (pi = 0; pi < NSHOWN; pi++) {
                mph[pi].fader = 0;
                morph_roll_seed(pi, 1u, 900u + pi);          /* a roll drops the hand-set values */
            }
            for (i2 = 0; i2 < MOTION_MAX; i2++)              /* (the load emptied the automation: a full one again) */
                motion.event[i2] = (motion_event_t){(uint8_t)((i2 & 1u) << 6 | (i2 >> 1) % 64u), P_REV, (int8_t)(i2 % 100u)};
            motion.count = MOTION_MAX;
            rc = project_save_as(13u, "WORST");
            amsg = (int)motion.count == MOTION_MAX && msg_is("PARAMETER LOCKS CUT") ? amsg : -1;   /* (the count with the hand-set values, or -1) */   /* (item 138: the message of multi.c since the page became PARAMETER LOCKS) */
            motion.count = 0;
            project_load(13u);
            frames(50);
            ck("  rolled again (no values set by hand): PARAMETER LOCKS CUT again, the automation cut to what fits", rc == 0 && amsg >= 0 && motion.count > 0u && motion.count < MOTION_MAX);
            ck("  (erased again)", project_erase(13u) == 0 && project_factory(13));
}

#if DUELL_TIDE
/* TIDE 1.0.0-b6: the built-in drone multis M17 DUNE HORN and M18 VOID DEEP (ROM only, MU_ROM): they show, load and preview as the factory multis do and match
 * their ROM records byte for byte; SAVE, rename and ERASE on them are refused and write nothing; M01..M16 and the flash objects are as before; MIDI program
 * change 16 and 17 load them, 18 and up are ignored; while one is loaded TIDE plays STYLE DRONE, another multi brings the STYLE back, the saved one never changes */
static void drones(void)
{
    static const char *const DN[2] = {"DUNE HORN", "VOID DEEP"};
    static const char *const DS[2][2] = {{"HORN SWELL", "VOID SHIMMER"}, {"VOID SHIMMER", "SOLAR WIND"}};
    /* the sequences (tools/drone_pack.py, b6: owner "I'm missing some sequence from the drone multis"): the part, its RATE (params.c N_DIV: 6 1/2, 7 1/1; b7: the bed beside it is ARP SEQ too, 1/4 = 0, one note tied over its 16 steps), the 16 steps
     * (a digit: the offset from the held key, '-' a tie, '.' a rest) */
    static const uint8_t DQ[2] = {0, 1}, DQ_RATE[2] = {6, 7};
    static const char DQ_STEP[2][17] = {"0--.0--.5--.0--.", "0------.2---5--."};
    uint8_t rec[MU_REC_MAX], obj[MU_NOBJ][MU_OBJ_MAX], amt[NSHOWN], set[6];
    uint16_t objn[MU_NOBJ];
    project_t q;
    static uint8_t snap[0x100000], dmb[MU_REC_MAX + 1u];
    uint32_t k, o, cut, len, ok, style0, gen;
    char nm[PROJ_NAME_LEN + 1u];
    mu_pv_t pv;
    song.playing = 0;
    duell_mode = duell_mode_req = DM_LAYER;
    for (k = 0, ok = 1; k < DF_N; k++) {             /* SP1 (b6): the ROM holds the records packed; each rebuilds byte for byte to its source file */
        char path[64];
        FILE *f;
        snprintf(path, sizeof path, DF_SLOT[k] < MU_SLOTS ? "tools/factory_multis/M%02u.bin" : "tools/drone_multis/M%02u.bin", (unsigned)DF_SLOT[k] + 1u);
        f = fopen(path, "rb");
        len = f ? (uint32_t)fread(rec, 1, sizeof rec, f) : 0u;
        if (f)
            fclose(f);
        memset(dmb, 0xA5, sizeof dmb);
        ok &= len > MU_G_AT && len == DM_LEN[k] && len <= MU_REC_MAX && !memcmp(rec, dm_rec(k, dmb), len) && dmb[len] == 0xA5u;
    }
    ck("drones: all 18 factory records (M01-M16, M17, M18) rebuild from the packed ROM byte for byte to their .bin files, and write nothing past their length", ok && DF_N == 18u);
    {                                                /* the rebuild beside a stored partner: M02 saved, M01 empty (its factory record decodes at the end of the staging buffer) */
        char nm2[PROJ_NAME_LEN + 1u];
        uint32_t l1;
        ok = project_save_as(1u, "PARTNER") == 0;
        mu_obj_read(0u);
        len = mu_put_rec(0u, 0, &cut, 0, 0);       /* (M01 emptied: its factory multi) */
        ok &= mu_obj_write(0u, len) == 0;
        mu_obj_read(0u);
        (void)mu_rec(1u, &l1);
        mu_note(0u);
        ok &= project_factory(0u) && project_info(0u, nm) && str_eq(nm, "BASIC CHORD") && project_info(1u, nm2) && str_eq(nm2, "PARTNER") && !project_factory(1u);
        mu_obj_read(0u);
        multi_preview(0u, &pv);
        ok &= pv.state == 1u && MU_HDR + l1 <= MU_OBJ_MAX - MU_REC_MAX;
        project_load(0u);
        ok &= proj_cur == 0u;
        project_load(1u);
        ok &= proj_cur == 1u;
        ok &= project_erase(1u) == 0 && project_factory(1u);
        ck("  a factory record rebuilt beside a stored partner (M01 factory, M02 saved): both list, preview and load; the partner is untouched", ok);
    }
    for (o = 0; o < MU_NOBJ; o++) {                  /* (the store before: every object of M01..M16) */
        objn[o] = (uint16_t)mu_obj_read(o);
        memcpy(obj[o], mu_buf, objn[o]);
    }
    ck("drones: MU_SHOWN 18, MU_SLOTS (stored) still 16, the flash objects as before (OBJ_PATCH0 = OBJ_MULTI0 + 8)", MU_SHOWN == 18u && MU_SLOTS == 16u && MU_NOBJ == 8u && DF_N == 18u);
    for (k = 16u, ok = 1; k < 18u; k++) {
        ok &= project_used(k) && project_factory(k) && !multi_bad(k) && project_info(k, nm) && str_eq(nm, DN[k - 16u]) && df_of(k) == k;
        ok &= str_eq(multi_vname(k, 0), DS[k - 16u][0]) && str_eq(multi_vname(k, 1), DS[k - 16u][1]);
        ok &= multi_vmode(k, DQ[k - 16u]) == (2u | 4u) && multi_vmode(k, 1u - DQ[k - 16u]) == (2u | 4u);   /* (the MULTIS row, ui_graph.c graph_slots: "SEQ" on both: the moving part and, b7, the bed with its one tied step) */
        multi_preview(k, &pv);
        ok &= pv.state == 1u;
    }
    ck("  M17 DUNE HORN, M18 VOID DEEP: used, factory, their names, sounds (HORN SWELL + VOID SHIMMER, VOID SHIMMER + SOLAR WIND), the MULTIS row SEQ / SEQ with notes in the pattern, the preview decodes", ok);
    {   /* 1.0.0-b7: a part with the arp OFF and notes in its pattern plays the sequencer on PLAY: the MULTIS row and the preview say SEQ, not OFF (M03 JUNO DEEP, written patterns on both parts) */
        static proj_trk_t d;
        uint8_t v[4];
        memset(&d, 0, sizeof d);
        d.p[P_SLEN] = 16;
        mu_vp_set(v, &d);
        ok = v[2] == 0u && v[3] == 0u;                /* (arp OFF, no note: OFF) */
        d.step[3].time = ST_NOTE;
        d.step[3].n = 1;
        d.step[3].note[0] = 60;
        mu_vp_set(v, &d);
        ok &= v[2] == 2u && v[3] == 1u;               /* (arp OFF, a note: SEQ) */
        d.p[P_AMODE] = 1;
        mu_vp_set(v, &d);
        ok &= v[2] == (AM_SEQ_ID == 1 ? 2u : 1u);     /* (an arp: ARP) */
        d.p[P_AMODE] = AM_SEQ_ID;
        mu_vp_set(v, &d);
        ok &= v[2] == 2u;
        ok &= multi_vmode(2u, 0u) == (2u | 4u) && multi_vmode(2u, 1u) == (2u | 4u);
        multi_preview(2u, &pv);
        ok &= pv.state == 1u && pv.p[0].mode == 2u && pv.p[1].mode == 2u;
        ck("  a part with the arp OFF and notes in its pattern shows SEQ (row and preview; M03 JUNO DEEP both parts); OFF only with neither, ARP stays ARP", ok);
    }
    for (k = 16u; k < 18u; k++) {
        char path[64];
        FILE *f;
        uint32_t t;
        snprintf(path, sizeof path, "tools/drone_multis/M%u.bin", (unsigned)k + 1u);   /* (the source of the ROM record: tools/drone_pack.py) */
        f = fopen(path, "rb");
        len = f ? (uint32_t)fread(rec, 1, sizeof rec, f) : 0u;
        if (f)
            fclose(f);
        ok = len == DM_LEN[k] && !memcmp(rec, dm_rec(k, dmb), len) && rec[4] == DM_LAYER;
        ok &= mu_unpack(&q, amt, set, dm_rec(k, dmb), DM_LEN[k]);
        project_load(k);
        frame();
        for (t = 0; t < NSHOWN; t++)
            trk[t].engine = trk[t].eng_req;          /* (the engine-change fade, which audio blocks would finish) */
        ok &= proj_cur == k;
        for (t = 0; ok && t < NSHOWN; t++)
            ok &= trk[t].eng_req == q.t[t].engine && trk[t].preset == q.t[t].preset && !memcmp(trk[t].p, q.t[t].p, sizeof q.t[t].p) &&
                  !memcmp(trk[t].step, q.t[t].step, sizeof q.t[t].step);
        {                                            /* (the sequence, tools/drone_pack.py: one part ARP SEQ on 16 steps at its RATE, GATE 127, the steps' offsets; the other, b7, the bed: ARP SEQ 1/4, one tied note) */
            const track_t *s = &trk[DQ[k - 16u]], *d = &trk[1u - DQ[k - 16u]];
            uint32_t bed_ok;
            ok &= trk[0].p[P_AHOLD] == 2 && trk[1].p[P_AHOLD] == 2 && d->p[P_AMODE] == AM_SEQ_ID && song.g[G_BPM] == 60;
            ok &= s->p[P_AMODE] == AM_SEQ_ID && s->p[P_SLEN] == 16 && s->p[P_AGATE] == 127 && s->p[P_ARATE] == DQ_RATE[k - 16u];
            for (t = 0; ok && t < 16u; t++)
                ok &= DQ_STEP[k - 16u][t] == '.' ? s->step[t].time == ST_REST : DQ_STEP[k - 16u][t] == '-' ? s->step[t].time == ST_TIE :
                      s->step[t].time == ST_NOTE && s->step[t].n == 1u && s->step[t].note[0] == 60u + (uint32_t)(DQ_STEP[k - 16u][t] - '0') && !(s->step[t].flags & SF_ACCENT);
            for (bed_ok = 1, t = 0; t < 16u; t++)     /* (b7: the VOID SHIMMER bed, ARP SEQ 1/4, GATE 127: one note, root, tied over all 16 steps) */
                bed_ok &= t ? d->step[t].time == ST_TIE : d->step[t].time == ST_NOTE && d->step[t].n == 1u && d->step[t].note[0] == 60u;
            ok &= bed_ok && d->p[P_SLEN] == 16 && d->p[P_AGATE] == 127 && d->p[P_ARATE] == 0;
        }
        ck(k == 16u ? "  M17: the ROM record is tools/drone_multis/M17.bin byte for byte (LAYER); LOAD gives its engines, sounds, every value and step; HOLD ALL, 60 BPM; part 1 ARP SEQ 1/2 root root fourth root, part 2 ARP SEQ 1/4 one tied root"
                    : "  M18: the ROM record is tools/drone_multis/M18.bin byte for byte (LAYER); LOAD gives its engines, sounds, every value and step; HOLD ALL, 60 BPM; part 2 ARP SEQ 1/1 root, a tone up, fourth, part 1 ARP SEQ 1/4 one tied root", ok);
    }
    memcpy(snap, nor, sizeof snap);
    gen = multi_gen();
    ok = project_save_as(16u, "MINE") != 0 && msg_is(MU_ROM_SAY) && project_rename(17u, "MINE") != 0 && project_erase(17u) != 0 && msg_is("ALREADY FACTORY");
    ok &= project_save(17u) != 0 && !memcmp(snap, nor, sizeof snap) && multi_gen() == gen && project_info(16u, nm) && str_eq(nm, "DUNE HORN");
    ck("  SAVE, rename and ERASE on M17 / M18 refused (SAVE TO M01-M16, ALREADY FACTORY), nothing written, the names stand", ok);
    go_title("MULTIS");
    song.g[G_SLOT] = 18;
    frame();
    ui.act = 3;                                      /* (K3: SAVE picked) */
    act_do();
    ck("  the MULTIS page: SAVE on M18 says SAVE TO M01-M16, no dialog, no NAME screen, nothing written", msg_is(MU_ROM_SAY) && !ui.confirm && !name_on() &&
       !memcmp(snap + 0x97000, nor + 0x97000, 0x8000) && !memcmp(snap + 0xCE000, nor + 0xCE000, 0x8000));
    for (o = 0, ok = 1; o < MU_NOBJ; o++)
        ok &= mu_obj_read(o) == objn[o] && !memcmp(mu_buf, obj[o], objn[o]);
    ck("  every flash object of M01..M16 reads back unchanged (no format change)", ok);
    ok = project_save_as(3u, "EDITED DRONE") == 0 && project_info(3u, nm) && str_eq(nm, "EDITED DRONE") && !project_factory(3u);
    project_load(3u);
    ok &= proj_cur == 3u && project_erase(3u) == 0 && project_factory(3u) && project_info(3u, nm) && str_eq(nm, "LATE RHODES");   /* (reel, 2026-11-10: M04 is LATE RHODES) */
    ck("  an edited drone multi saves into M04 like any other, loads, and ERASE brings the factory M04 back", ok);
    proj_cur = 0;
    midi_pc(0, 16);
    pc_poll();
    ok = proj_cur == 16u;
    midi_pc(0, 17);
    pc_poll();
    ok &= proj_cur == 17u;
    project_load(1u);                                /* (M02: M01 holds the DAMAGED record of the check above) */
    midi_pc(0, 18);
    pc_poll();
    midi_pc(0, 127);
    pc_poll();
    ck("  MIDI program change 16, 17 (LAYER) load M17, M18; 18 and 127 are ignored", ok && proj_cur == 1u);
    tide_settings();
    tide_style_apply(STY_DEEP);                     /* (the player's STYLE) */
    tide_settings();
    style0 = tide_seen[TS_STYLE];
    mph[0].aset = mph[0].amount = 37u;               /* (the player's AMOUNT, knob 4: MORPH c3 keeps it over a factory load) */
    mph[1].aset = mph[1].amount = 63u;
    project_load(16u);
    tide_settings();
    ok = tide_style == STY_DRONE && tide_style_holds() && gl.mode == STY[STY_DRONE][0] && gl.time == STY[STY_DRONE][1] && str_eq(jl_vname(JL_STYLE, jl_get(JL_STYLE)), "DRONE");
    ok &= tide_seen[TS_STYLE] == STY_DEEP && style0 == STY_DEEP && tide_seen[TS_TIME] == STY[STY_DEEP][1];
    ck("  STYLE: M17 loaded, TIDE plays DRONE (its rows, TIDE SETTINGS shows DRONE); the saved STYLE stays DEEP", ok);
    ck("  M17 loaded: like every factory multi (MORPH c3), both parts keep the player's AMOUNT (37 / 63), and STYLE is still DRONE",
       mph[0].n[0] && mph[1].n[0] && mph[0].amount == 37u && mph[1].amount == 63u && tide_style == STY_DRONE);
    project_load(17u);
    tide_settings();
    ok = tide_style == STY_DRONE && tide_seen[TS_STYLE] == STY_DEEP;
    project_load(1u);
    tide_settings();
    ok &= tide_style == STY_DEEP && tide_style_holds() && gl.time == STY[STY_DEEP][1] && tide_seen[TS_STYLE] == STY_DEEP;
    ck("  M18 after it: still DRONE; M02 loaded: DEEP and its rows are back, the saved STYLE never changed", ok);
    {   /* as a player plays them (TASKS: "hold a chord around C3"): loaded in LAYER, a C major chord on the keys of a power-on keyboard (MIDI 60 64 67;
         * 48 and 52 need OCT- first) held 3 s, then let go: both parts sound it
         * (the unit's console sweep played one MIDI channel and saw part 2 at 0 voices, 9 November) */
        static pr_t dr[NSHOWN];
        uint32_t c3 = pr_key(60), e3 = pr_key(64), g3 = pr_key(67), s, sec = (uint32_t)(FS / CTL);
        for (s = 16u; s < 18u; s++) {
            play_run(s, 1u << c3 | 1u << e3 | 1u << g3, 3u * sec, sec, 0, dr);
            printf("multi:   M%02u the keys C E G (MIDI 60 64 67) held 3 s (LAYER): part 1 %u note-ons, part 2 %u; still sounding after the release (HOLD ALL): %u / %u\n",
                   s + 1u, dr[0].n, dr[1].n, dr[0].gated_after, dr[1].gated_after);
            if (getenv("DR_DBG")) {
                uint32_t pi, i;
                printf("keys %u %u %u; ", c3, e3, g3);
                for (pi = 0; pi < 2u; pi++) {
                    printf("part %u VOICE %d HOLD %d AMODE %d CHRD %d notes:", pi + 1u, trk[pi].p[P_VOICE], trk[pi].p[P_AHOLD], trk[pi].p[P_AMODE], trk[pi].p[P_CHRD]);
                    for (i = 0; i < dr[pi].n && i < PR_MAX; i++)
                        printf(" %u@%u", dr[pi].note[i], dr[pi].blk[i]);
                    printf("; ");
                }
                printf("\n");
            }
            ck(s == 16u ? "  M17 DUNE HORN from the keys (C E G, LAYER): both parts sound the chord" : "  M18 VOID DEEP from the keys (C E G, LAYER): both parts sound the chord",
               c3 < 27u && e3 < 27u && g3 < 27u && dr[0].n >= 3u && dr[1].n >= 3u);
        }
    }
    (void)amt; (void)set;
}
#endif

int main(void)
{
    static track_t ref[NSHOWN];
    static uint8_t patch[FP_SIZE + 1u], fendb[NSHOWN][NFM];
    static int16_t endb[NSHOWN][EXT_P_LEN];
    static motion_store_t mref;
    uint32_t seedb[NSHOWN], k, pi, ok;
    memset(nor, 0xFF, sizeof nor);
    power_cycle();

    /* 1: a distinct state, saved to M05 through the page */
    set_engine_of(&trk[0], 3u);                      /* part 1: engine 3, part 2: FM6 (other engines) */
    apply_preset_to(&trk[0], 1u);
    set_engine_of(&trk[1], ENGI_FM6);
    apply_preset_to(&trk[1], 2u);
    trk[0].engine = trk[0].eng_req;                  /* (no audio runs here: the engine-change fade is over; else the morph is another sound's) */
    trk[1].engine = trk[1].eng_req;
    trk[0].p[P_LEVEL] = 91;
    trk[1].p[P_PAN] = -20;
    trk[1].p[P_AMODE] = 2;                           /* (the arp is a part's parameter) */
    fm6_patch[1][3] = (uint8_t)((fm6_patch[1][3] + 17u) % 99u);   /* the FM6 patch edited: its own (SLOT OWN) */
    fm6_adopt(1);
    pattern(&trk[0], 1u);
    pattern(&trk[1], 2u);
    trk[0].p[P_M2SRC] = MS_LFO, trk[0].p[P_M2DST] = MD_RSIZE, trk[0].p[P_M2AMT] = 25;   /* 0.2.7: the new destinations travel too */
    trk[1].p[P_M6SRC] = MS_ENV2, trk[1].p[P_M6DST] = MD_EQHI, trk[1].p[P_M6AMT] = -40;
    trk[1].p[P_M8SRC] = MS_MODW, trk[1].p[P_M8DST] = MD_DMRATE, trk[1].p[P_M8AMT] = 60;
    motion.count = 1;                                /* an automation lock on part 2 */
    motion.event[0].place = (uint8_t)(1u << 6 | 5u);
    motion.event[0].param = P_ATK | MOTION_LOCK;
    motion.event[0].value = 33;
    motion.on = 2;
    song.g[G_BPM] = 137;
    song.g[G_DTIME] = 5;
    song.g[G_DMODE] = DMD_PING;
    song.g[G_DMDEP] = 37;                                /* the delay's modulation (0.2.5) travels with the multi */
    song.g[G_DMRATE] = 51;
    song.g[G_RTYPE] = 3;
    song.g[G_RLOCUT] = 77;                               /* the reverb's low cut (0.2.14) travels with the multi */
    song.g[G_SHPITCH] = SHP_DOWN;                        /* SHIM's PITCH and MOD (0.2.19) travel with the multi (0.2.25: DOWN; BOTH is gone) */
    song.g[G_SHMOD] = 37;
    frame();
    duell_mode_req = DM_LAYER;
    SEL_BOTH = 1;
    morph_set_cfg(MC_WILD, 1);                       /* WILD */
    frames(100);
    morph_roll(1);                                   /* End B rolled (both parts: TARGET BOTH) */
    morph_set_fader(70);
    frame();
    song.playing = 0;
    go_title("MULTIS");
    song.g[G_SLOT] = 11;
    frame();
    ck("SAVE > MULTI is SAVE's page, KNOB 1 M01..M16 (TIDE b6: ..M18, the two ROM drone multis), all sixteen stored slots the factory multis (0.2.23; M11 ACID LINES since the reel, 2026-11-10)",
       cur_page()->graph == GR_SLOTS && cur_page()->fam == FAM_SAVE && GP[G_SLOT].max == (int)MU_SHOWN && project_used(10) && project_factory(10) && str_eq(mu_names[10], "ACID LINES"));
#if DUELL_TIDE
    {                                                /* U3: browsing the MULTIS page (the preview) changes nothing live, decodes once per slot, and shows the record */
        uint32_t k, sum0, sum1, gen = multi_gen(), eng0 = trk[0].eng_req;
        mu_pv_t pv;
        project_capture(&proj_scratch);
        sum0 = proj_sum(&proj_scratch);
        for (k = 0; k < MU_SLOTS; k++) {
            song.g[G_SLOT] = (int16_t)(k + 1u);
            frame();
        }
        song.g[G_SLOT] = 11;
        frame();
        frame();
        project_capture(&proj_scratch);
        sum1 = proj_sum(&proj_scratch);
        ck("MULTIS preview: all sixteen slots browsed, the music, the transport and the loaded multi unchanged (no record was written: the record counter stands)",
           sum0 == sum1 && !song.playing && multi_gen() == gen);
        multi_preview(10u, &pv);
        ck("  the preview of M11 is its record: decoded, both parts' sounds named, no engine out of range", pv.state == 1u && pv.p[0].eng < NENGINES && pv.p[1].eng < NENGINES &&
           gsl_key == (10u | multi_gen() << 8));
        ck("  the music is still the one before the preview decoded (the scratch only)", trk[0].eng_req == eng0);
        {                                            /* audit C01: a record that is there but does not load (here a version 7 one over the factory M11) is DAMAGED, not the factory multi */
            uint8_t junk[40];
            uint32_t cut, len;
            mu_pv_t dp;
            memset(junk, 0, sizeof junk);
            junk[0] = 7;
            mu_obj_read(10u / 2u);
            len = mu_put_rec(10u & 1u, 0, &cut, junk, sizeof junk);
            ck("DAMAGED slot: a stored record this firmware does not load is written over the factory M11", mu_obj_write(10u / 2u, len) == 0);
            mu_note(10u / 2u);
            multi_preview(10u, &dp);
            ck("  M11 is not used, shows no factory name, is marked damaged and previews as DAMAGED (state 2)", !project_used(10) && multi_bad(10) && !mu_names[10][0] && dp.state == 2u);
            ck("  its partner M12 is still the factory multi", project_used(11) && !multi_bad(11) && project_factory(11));
            mu_obj_read(10u / 2u);                   /* (empty again: the factory M11) */
            len = mu_put_rec(10u & 1u, 0, &cut, 0, 0);
            mu_obj_write(10u / 2u, len);
            mu_note(10u / 2u);
            ck("  emptied again: the factory M11 shows", project_used(10) && !multi_bad(10) && project_factory(10));
        }
        {                                            /* the PATTERNS preview: a slot saved over previews its own contents; empty = no note; the tracks stay */
            pat_pv_t pp;
            track_t keep = trk[0];
            uint32_t i;
            ps_preview(2u, &pp);
            ck("PATTERNS preview: P03 (factory) decodes, has notes, LEN as the list says", pp.state == 1u && pp.has && pp.len == ps_len(2u));
            for (i = 0; i < NSTEP; i++)
                step_clear(&trk[0].step[i]);
            ck("  a cleared pattern saved over P03 (a user overwrite)", ps_save_trk(&trk[0], 2u, "EMPTY ONE") == 0);
            ps_preview(2u, &pp);
            ck("  previews its own contents: decoded, no note (NO SEQUENCE), the new gen", pp.state == 1u && !pp.has && pp.len == ps_len(2u) && ps_gen() != 0u);
            trk[0] = keep;
            ck("  the live track is untouched by the preview", !memcmp(&trk[0], &keep, sizeof keep));
            {                                        /* audit C1 / C2: every list entry (the slots, the empty USER slots, the read-only places P29..P32) previews what its LOAD gives */
                uint32_t k, same = 0, places = 0, users = 0;
                for (k = 0; k < PS_VSLOTS; k++) {
                    track_t t = trk[0];
                    uint32_t hits = 0;
                    ps_preview(k, &pp);
                    if (ps_load_trk(&t, k) != 0)
                        continue;
                    for (i = 0; i < NSTEP && i < (uint32_t)t.p[P_SLEN]; i++)
                        hits += t.step[i].time == ST_NOTE && t.step[i].n;
                    same += pp.state == 1u && (pp.has != 0u) == (hits != 0u) && pp.len == (uint32_t)t.p[P_SLEN] && ps_len(k) == pp.len;
                    places += k >= PS_SLOTS && pp.state == 1u && pp.has && pp.len == PATTERNS[k - PS_NUSER].len;
                    users += k >= PS_USER0 && k < PS_SLOTS && pp.state == 1u && !pp.has && !hits;
                }
                ck("PATTERNS preview = LOAD for all 36 entries: decoded, notes iff the loaded pattern has notes, LEN as LOAD sets it and as the list says", same == PS_VSLOTS);
                ck("  the read-only places P29..P32 preview their default pattern (not DAMAGED, not LEN 0)", places == PS_NUSER);
                ck("  an empty USER slot previews no note (its LOAD gives a blank pattern)", users == PS_NUSER);
                trk[0] = keep;
            }
        }
    }
#endif
    watch = 1;
    press_k(EN_K3);                                  /* SAVE (1.1: K3): a used (factory) slot: OVERWRITE M11?, then the NAME screen */
    ck("SAVE over the factory M11 asks OVERWRITE M11?", ui.confirm == CF_OVR_PROJ);
    press(B_OCTUP);
    ck("  OCT+ opens NAME (placeholder MULTI M11)", name_on() && str_eq(nm.ph, "MULTI M11"));
    str_cpy(nm.s, "DUEL JAM", sizeof nm.s);
    nm.len = nm.cur = 8;
    press(B_OCTUP);
    watch = 0;
    ck("OCT+: SAVED M11, written in the multi store's ranges only", msg_is("SAVED M11") && project_used(10) && !stray);
    for (pi = 0; pi < NSHOWN; pi++) {                /* (what was saved: the save may roll End B again from its seed) */
        ref[pi] = trk[pi];
        seedb[pi] = morph_seed_of(pi, 1);
        endb_codes(pi, endb[pi]);
        memcpy(fendb[pi], mph[pi].fh[1][mph[pi].top[1]], NFM);
    }
    memcpy(patch, fm6_patch[1], sizeof patch);
    mref = motion;
    power_cycle();
    ck("after a power cycle: M11 DUEL JAM listed, M12 and M01 still the factory multis; the state is the power-on one", project_used(10) &&
       !project_factory(10) && project_factory(11) && project_factory(0) && str_eq(mu_names[10], "DUEL JAM") && trk[0].eng_req != 3u &&
       song.g[G_BPM] != 137 && duell_mode_req != DM_LAYER);
    go_title("MULTIS");
    song.g[G_SLOT] = 11;
    frame();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;              /* (0.2.11: this test reads the stored MODE back: MODE RECALL ON; the option's own tests are below) */
    press_k(EN_K2);                                  /* LOAD (no dialog) */
    frames(50);
    for (pi = 0; pi < NSHOWN; pi++)
        trk[pi].engine = trk[pi].eng_req;            /* (the engine-change fade, which audio blocks would finish) */
    ok = 1;
    for (pi = 0; pi < NSHOWN; pi++)
        ok &= !memcmp(trk[pi].p, ref[pi].p, sizeof ref[pi].p) && trk[pi].eng_req == ref[pi].eng_req &&
              !memcmp(trk[pi].step, ref[pi].step, sizeof ref[pi].step);
    ck("LOAD: LOADED M11; both parts' engines, every parameter (sound, arp, mix, LEN .., 0.2.7's matrix DSTs) and all 64 steps equal",
       !ui.confirm && msg_is("LOADED M11") && ok);
    ck("  part 2's FM6 patch (edited) equal; the automation lock equal", !memcmp(fm6_patch[1], patch, sizeof patch) &&
       motion.count == mref.count && !memcmp(&motion.event[0], &mref.event[0], sizeof mref.event[0]) && motion.on == mref.on);
    ck("  TEMPO, DLY TIME, PING-PONG, DLY MODULATION DEPTH / RATE, REVERB TYPE; MODE LAYER; WILD; the saved TARGET BOTH loaded as the selection BOTH (1.0.0-b5)",
       song.g[G_BPM] == 137 && song.g[G_DTIME] == 5 && song.g[G_DMODE] == DMD_PING && song.g[G_DMDEP] == 37 && song.g[G_DMRATE] == 51 && song.g[G_RTYPE] == 3 && song.g[G_RLOCUT] == 77 &&
       duell_mode_req == DM_LAYER && SEL_BOTH && (morph.cfg & MC_WILD));
    ck("  SHIM's PITCH DOWN and MOD 37 (0.2.19: the ids of the old PING-PONG switch and NAME card) are back", song.g[G_SHPITCH] == SHP_DOWN && song.g[G_SHMOD] == 37);
    ok = 1;
    for (pi = 0; pi < NSHOWN; pi++)
    {
        static int16_t now[EXT_P_LEN];
        endb_codes(pi, now);
        ok &= !memcmp(now, endb[pi], sizeof now) && !memcmp(mph[pi].fh[1][mph[pi].top[1]], fendb[pi], NFM) && mph[pi].fader == 0u;
    }
    (void)seedb;
    ck("  End B of each part: the same values as it sounded when saved (MU_VER 8: values, no seed; FM6 bytes too); the fader at A", ok);
    {   /* cloud job G (EDITABLE B): the values set by hand go through a multi (the record's edit lists), with End B */
        xb_edit_t el[NSHOWN][XB_MAX], g[XB_MAX];
        uint32_t ne[NSHOWN], w, n;
        int8_t eb[NSHOWN][P_COUNT];
        for (pi = 0; pi < NSHOWN; pi++) {
            trk[pi].engine = trk[pi].eng_req;            /* (no audio runs here: the engine-change fade is over) */
            mph[pi].fader = 127;
            for (w = 0, n = 0; w < P_COUNT && n < 3u; w++)
                if (morph_b_mode(pi, w) == 1u && !pin_get(morph_pins(&mph[pi]), w)) {
                    morph_b_set(pi, w, clamp(mph[pi].base[w] + 7 + (int32_t)pi, TP[w].min < -64 ? -64 : TP[w].min, TP[w].max));
                    n++;
                }
            ne[pi] = morph_edits(pi, el[pi]);
            mph[pi].fader = 0;
        }
        ck("EDITABLE B: values set by hand on both parts (three each) before a multi SAVE", ne[0] == 3u && ne[1] == 3u);
        ck("  MULTI SAVE (M14 HAND B)", project_save_as(13u, "HAND B") == 0 && project_used(13));
        for (pi = 0; pi < NSHOWN; pi++) {
            memcpy(eb[pi], morph_cur(&mph[pi], 1), P_COUNT);
            ne[pi] = morph_edits(pi, el[pi]);
        }
        project_load(16u);                           /* another multi: the parts' B ends are other ones (the reel re-voiced: M17 DUNE HORN, a ROM drone, no value set by hand at its B end; M02, used until then, has hand-set B values now;
                                                      * M12, loaded here until then, is ROBOT FUNK now, whose End B is set value by value: it has edit lists of its own) */
        frames(50);
        ok = morph_edits(0, g) == 0u && morph_edits(1, g) == 0u;
        project_load(13u);
        frames(50);
        for (pi = 0; pi < NSHOWN; pi++) {
            n = morph_edits(pi, g);
            ok &= n == ne[pi] && n == 3u && !memcmp(eb[pi], morph_cur(&mph[pi], 1), P_COUNT);
            for (w = 0; w < n; w++)
                ok &= g[w].wid == el[pi][w].wid && g[w].val == el[pi][w].val;
        }
        ck("  LOAD M14: both parts' three values come back with End B exactly as saved (the fader at A)", ok && mph[0].fader == 0u);
        ck("  (M14 erased again: its factory multi is back)", project_erase(13u) == 0 && project_factory(13));

    }

    /* 0.2.21 (owner, 2026-10-16): a multi load sets its OWN mixer levels and the global SEND effects it stores (chorus, delay with MODE / TIME / FDBK / COLR / SPREAD /
     * MRATE / MDEPTH (CHANCE), reverb TYPE / SIZE / DAMP / LOCUT, SHIMMER PITCH / MOD, the parts' sends); it leaves the device's: MIDI (channels, routing, clock source),
     * TUNE and the master effect BASS MONO. The record stores BASS MONO in no bit (byte 5 bits 1..4 written 0) */
    {
        static const struct { uint8_t id; int16_t v, other; } GS[] = {
            {G_DTIME, 9, 3}, {G_DFDBK, 57, 11}, {G_DCOLOR, 33, 99}, {G_DMIX, 47, 5}, {G_RSIZE, 71, 20}, {G_RDAMP, 29, 80}, {G_CRATE, 31, 70}, {G_CDEPTH, 23, 3},
            {G_RTYPE, 5, 1}, {G_RLOCUT, 101, 0}, {G_PINGSPREAD, 61, 100}, {G_DMDEP, 43, 0}, {G_DMRATE, 51, 10}, {G_DMODE, DMD_RAND, DMD_DIGITAL},
            {G_SHPITCH, SHP_DOWN, SHP_UP}, {G_SHMOD, 17, 0}};
        uint32_t i;
        ok = 1;
        song.sel = 0;
        duell_mode_req = DM_LAYER;
        song.bm = 6;
        song.g[G_TUNE] = 4407; song.g[G_CLOCK] = 2; song.g[G_ROUTE] = 1;
        midi_pch[0] = 4; midi_pch[1] = 9;
        for (i = 0; i < sizeof GS / sizeof GS[0]; i++)
            song.g[GS[i].id] = GS[i].v;
        trk[0].p[P_LEVEL] = 83; trk[1].p[P_LEVEL] = 61; trk[0].p[P_TRIM] = 99;
        trk[0].p[P_CHOR] = 21; trk[0].p[P_DLY] = 31; trk[0].p[P_REV] = 41; trk[1].p[P_REV] = 71;
        ck("MULTI SAVE (M13 SENDS TST) with those", project_save_as(12u, "SENDS TST") == 0 && project_used(12));
        {
            project_t q;
            uint32_t cut;
            uint8_t blk[NSHOWN][MB_N], rec[MU_REC_MAX];
            memset(blk, 0, sizeof blk);
            project_capture(&q);
            ck("  the record holds no BASS MONO (byte 5 bits 1..4 are 0 with 125 Hz on the unit; 0.2.24: the capture writes 0 too, a unit setting)", q.bm == 0 && song.bm == 6 && mu_pack(rec, &q, &cut) > MU_G_AT && (rec[5] >> 1) == 0);
        }
        for (i = 0; i < sizeof GS / sizeof GS[0]; i++)
            song.g[GS[i].id] = GS[i].other;
        trk[0].p[P_LEVEL] = 12; trk[1].p[P_LEVEL] = 0; trk[0].p[P_TRIM] = 5;
        trk[0].p[P_CHOR] = 0; trk[0].p[P_DLY] = 0; trk[0].p[P_REV] = 0; trk[1].p[P_REV] = 0;
        song.bm = 9; song.g[G_TUNE] = 4395; song.g[G_CLOCK] = 1; song.g[G_ROUTE] = 0;
        midi_pch[0] = 2; midi_pch[1] = 3;
        project_load(12u);
        for (i = 0; i < sizeof GS / sizeof GS[0]; i++)
            ok &= song.g[GS[i].id] == GS[i].v;
        ck("0.2.21 LOAD sets every global send it stores: DLY TIME FDBK COLR MIX SPREAD MDEPTH / CHANCE MRATE MODE, CHO RATE DEPTH, REV TYPE SIZE DAMP LOCUT, SHIMMER PITCH MOD", ok);
        ck("  .. and the parts' own mixer levels (not the 12 / 0 it had), the trim and each part's CHO / DLY / REV sends",
           trk[0].p[P_LEVEL] == 83 && trk[1].p[P_LEVEL] == 61 && trk[0].p[P_TRIM] == 99 && trk[0].p[P_CHOR] == 21 && trk[0].p[P_DLY] == 31 && trk[0].p[P_REV] == 41 &&
           trk[1].p[P_REV] == 71);
        ck("  .. the device's stay: BASS MONO 9, TUNE, CLOCK source, MIDI IN routing and the parts' MIDI channels",
           song.bm == 9 && song.g[G_TUNE] == 4395 && song.g[G_CLOCK] == 1 && song.g[G_ROUTE] == 0 && midi_pch[0] == 2 && midi_pch[1] == 3);
        song.bm = 0;
        project_load(12u);
        ck("  .. and a unit with BASS MONO OFF stays OFF (the multi was saved at 125 Hz)", song.bm == 0);
        /* a 0.2.19 / 0.2.20 record with BASS MONO bits in byte 5 loads, the bits are ignored (the load, not just mu_unpack) */
        {
            static uint8_t rec2[MU_REC_MAX];
            project_t q;
            uint32_t cut, len;
            uint8_t blk[NSHOWN][MB_N];
            memset(blk, 0, sizeof blk);
            project_capture(&q);
            len = mu_pack(rec2, &q, &cut);
            rec2[5] = (uint8_t)((rec2[5] & 1u) | 5u << 1);
            mu_obj_read(13u / 2u);
            mu_put_rec(13u & 1u, 1, &cut, rec2, len);
            ok = mu_obj_write(13u / 2u, MU_HDR + mu_rd16(mu_buf + 4) + mu_rd16(mu_buf + 6)) == 0;
            mu_obj_read(13u / 2u);
            mu_note(13u / 2u);
            song.bm = 3;
            project_load(13u);
            ck("  a record saved by 0.2.19 / 0.2.20 with BASS MONO 100 Hz in byte 5 loads, the bit pattern ignored (the unit's 3 stays)", ok && project_used(13) && song.bm == 3);
        }
    }

    /* the scale is the instrument's (core.h): a load never touches the global SCALE, ROOT, OFF-SCALE. Ids 25 and 26 (once the per-part ROOT and SCALE) are the TWIN FILTER's
     * RES 2 and DRIVE (cloud job B): ordinary sound values, stored in the multi and loaded back as any value (the old "ignored on load" is gone) */
    trk[0].p[P_RES2] = 9; trk[1].p[P_DRV2] = 4;
    scale_g = 7; root_g = 5; offscale_g = OS_PLAY;
    project_save_as(15, "SCALE TST");
    trk[0].p[P_RES2] = trk[1].p[P_DRV2] = 0;
    scale_g = 3; root_g = 2; offscale_g = OS_SNAP;
    project_load(15);
    ck("a multi with RES 2 and DRIVE (ids 25 / 26) loads them back; the global scale, root and OFF-SCALE are as they were",
       trk[0].p[P_RES2] == 9 && trk[1].p[P_DRV2] == 4 && scale_g == 3 && root_g == 2 && offscale_g == OS_SNAP);
    trk[0].p[P_RES2] = trk[1].p[P_DRV2] = 0;
    scale_g = 0; root_g = 0;

    /* 2: sixteen slots, each its own; a rewrite keeps the pair partner */
    for (k = 0; k < MU_SLOTS; k++) {
        char nm2[8] = "MULTI  ";   /* ("MULTI A" ..) */
        nm2[6] = (char)('A' + k);
        song.g[G_BPM] = (int16_t)(60 + k);
        trk[0].p[P_LEVEL] = (int16_t)(10 + k);
        pattern(&trk[1], 100u + k);
        watch = 1;
        project_save_as(k, nm2);
        watch = 0;
    }
    ck("16 saves: each SAVED, nothing written outside the multi store", project_used(15) && !stray);
    power_cycle();
    ok = 1;
    for (k = 0; k < MU_SLOTS; k++) {
        static track_t r;
        char nm2[8] = "MULTI  ";
        nm2[6] = (char)('A' + k);
        pattern(&r, 100u + k);
        project_load(k);
        ok &= project_used(k) && str_eq(mu_names[k], nm2) && song.g[G_BPM] == (int16_t)(60 + k) &&
              trk[0].p[P_LEVEL] == (int16_t)(10 + k) && !memcmp(trk[1].step, r.step, sizeof r.step) &&
              trk[1].p[P_SLEN] == r.p[P_SLEN];
    }
    ck("after a power cycle: M01..M16 each load their own (name, tempo, level, part 2's pattern)", ok);
    song.g[G_BPM] = 200;
    project_save_as(6, "NEW 7");                     /* M07: its partner is M08 */
    power_cycle();
    project_load(7);
    ok = str_eq(mu_names[7], "MULTI H") && song.g[G_BPM] == 67;
    project_load(6);
    ck("M07 rewritten: M07 the new one, its pair partner M08 as it was", ok && str_eq(mu_names[6], "NEW 7") && song.g[G_BPM] == 200);
    go_title("MULTIS");
    song.g[G_SLOT] = 12;
    frame();
    ck("MULTIS: only LOAD and SAVE are actions (DUELL 1.1: no ERASE card)", act_cols() == 6u);
    ck("project_erase (no UI in 1.1; the factory multi comes back): ERASED M12", project_erase(11) == 0 && msg_is("ERASED M12"));
    power_cycle();
    ck("  after a power cycle M12 is its factory multi again (ROBOT FUNK since the reel, 2026-11-10), its partner M11 kept", project_factory(11) &&
       str_eq(mu_names[11], "ROBOT FUNK") && project_used(10) && str_eq(mu_names[10], "MULTI K"));
    go_title("MULTIS");
    song.g[G_SLOT] = 4;
    frame();
    press_k(EN_K3);                                  /* SAVE (1.1: K2 LOAD, K3 SAVE) */
    ck("SAVE over a used slot asks OVERWRITE M04?", ui.confirm == CF_OVR_PROJ);
    press(B_OCTDN);

    /* 3: a power cut during the write of M10 (the second range: object 4, 0xCE000): M09 and M10 as they were */
    song.g[G_BPM] = 222;
    cut_bytes = 1500;
    ck("a power cut during M10's write: SAVE ERROR", project_save_as(9, "CUT") != 0);
    power_cycle();
    project_load(9);
    ok = str_eq(mu_names[9], "MULTI J") && song.g[G_BPM] == 69;
    project_load(8);
    ck("  after the power comes back: M10 and its partner M09 as they were", ok && song.g[G_BPM] == 68 && str_eq(mu_names[8], "MULTI I"));

    /* 4: MIDI PROGRAM CHANGE / BANK SELECT (SPEC 4.2a) and the MOD WHEEL on the MORPH fader, through the real input path */
    {
        static const uint8_t TRS_PC[] = {0xC1u, 3u};          /* (TRS: PC 3 on channel 2) */
        uint32_t e, kk, i;
        int16_t p0[P_COUNT];
        power_cycle();
        song.g[G_ROUTE] = 0;                                  /* (ROUT CH1-4) */
        duell_mode_req = DM_LAYER;
        events_block(CTL);
        song.g[G_BPM] = 99;
        midi_in_event(0x0Cu | 0xC0u << 8 | 40u << 16);        /* PC 40: no multi */
        events_block(CTL); frame();
        ok = song.g[G_BPM] == 99;
        midi_in_event(0x0Cu | 0xC0u << 8 | 11u << 16);        /* PC 11 ch 1: M12, erased above: its factory multi (reel: ROBOT FUNK, 128) */
        events_block(CTL); frame();
        ok &= song.g[G_BPM] == 128 && proj_cur == 11u && str_eq(mu_names[11], "ROBOT FUNK");
        song.g[G_BPM] = 99;
        song.g[G_BPM] = 123;
        project_save_as(11, "PC M12");
        song.g[G_BPM] = 99;
        duell_mode_req = DM_LAYER;
        events_block(CTL);
        midi_in_event(0x0Cu | 0xC0u << 8 | 11u << 16);
        events_block(CTL); frame();
        ck("LAYER: PC 40: nothing; PC 11 ch 1 loads M12 (its factory multi, then the one saved there, TEMPO 123)", ok && song.g[G_BPM] == 123 &&
           proj_cur == 11u);

        duell_mode_req = DM_SPLIT;
        events_block(CTL);
        set_engine_of(&trk[1], ENGI_FM6);
        trk[1].p[P_LEVEL] = 55;
        memcpy(p0, trk[0].p, sizeof p0);
        e = ds_list_at(3, &kk);                          /* (program 3 = slot 3 = the factory sound at browse position 3) */
        for (i = 0; i < sizeof TRS_PC; i++)
            um_byte(TRS_PC[i]);
        events_block(CTL); frame();
        ok = e < NENGINES && trk[1].eng_req == e && trk[1].preset == kk && !trk[1].user &&   /* (DUELL: a factory sound's */
             trk[1].p[P_TRIM] == ds_value(ds_get(e, kk), P_TRIM, TP[P_TRIM].def) &&          /* TRIM is its loudness; 0.2.19: the mixer level 55 stays) */
             trk[1].p[P_LEVEL] == 55 &&
             !memcmp(trk[0].p, p0, sizeof p0) && song.sel == 0u;
        up_store_trk(&trk[0], 5u, "PC USER", 0);              /* a user sound in STORE slot 5 */
        midi_in_event(0x0Bu | 0xB1u << 8 | 0u << 16 | 0u << 24);    /* bank 0 / 1 on ch 2 */
        midi_in_event(0x0Bu | 0xB1u << 8 | 32u << 16 | 1u << 24);
        midi_in_event(0x0Cu | 0xC1u << 8 | 5u << 16);
        events_block(CTL); frame();
        ok &= trk[1].user == 6u && trk[1].eng_req == trk[0].eng_req;
        midi_in_event(0x0Cu | 0xC1u << 8 | 7u << 16);         /* bank 1 kept: slot 7 holds no saved sound, its factory sound loads */
        events_block(CTL); frame();
        {
            uint32_t k7, e7 = ds_list_at(7, &k7);
            ck("SPLIT: PC 3 ch 2 (TRS) = slot 3 on part 2 only (TRIM: the sound's; the mixer LEVEL stays); bank 1 + PC 5 = slot 5 (saved); PC 7 = slot 7 (the factory sound)",
               ok && trk[1].user == 0u && trk[1].eng_req == e7 && trk[1].preset == k7 && slot_of(&trk[1]) == 7u);
        }

        {   /* audit G13: BANK SELECT 2 (CC0 = 0, CC32 = 2) + PC n = the read-only place #129 + n, the sounds past the 128; bank 3 and a program past the last: nothing */
            uint32_t fe, fk, good = 0, last = UP_VSLOTS - UP_SLOTS - UP_NUSER - 1u;
            const uint32_t pr[3] = {0u, 17u, last};
            for (i = 0; i < 3u; i++) {
                midi_in_event(0x0Bu | 0xB1u << 8 | 0u << 16 | 0u << 24);
                midi_in_event(0x0Bu | 0xB1u << 8 | 32u << 16 | 2u << 24);
                midi_in_event(0x0Cu | 0xC1u << 8 | pr[i] << 16);
                events_block(CTL); frame();
                good += sl_fac(UP_SLOTS + UP_NUSER + pr[i], &fe, &fk) && trk[1].eng_req == fe && trk[1].preset == fk && !trk[1].user &&
                        slot_place(&trk[1]) == UP_SLOTS + UP_NUSER + pr[i];
            }
            midi_in_event(0x0Bu | 0xB1u << 8 | 32u << 16 | 2u << 24);
            midi_in_event(0x0Cu | 0xC1u << 8 | (last + 1u) << 16);   /* past the last place */
            midi_in_event(0x0Bu | 0xB1u << 8 | 32u << 16 | 3u << 24);   /* bank 3 */
            midi_in_event(0x0Cu | 0xC1u << 8 | 0u << 16);
            events_block(CTL); frame();
            ck("BANK SELECT 2 + PC n on ch 2 = the place #129 + n (first, a middle one, the last); past the last, and bank 3: nothing",
               good == 3u && last >= 31u && slot_place(&trk[1]) == UP_SLOTS + UP_NUSER + last);
            midi_in_event(0x0Bu | 0xB1u << 8 | 32u << 16 | 0u << 24);   /* back to bank 0 for the rest */
        }

        SEL_BOTH = 0;
        midi_in_event(0x0Bu | 0xB0u << 8 | 1u << 16 | 127u << 24);  /* the wheel on ch 1 */
        events_block(CTL); frame();
        ok = mph[0].fader == 127u && mph[1].fader == 0u && trk[0].mw == 127u && trk[1].mw == 0u;
        duell_mode_req = DM_LAYER;
        events_block(CTL);
        midi_in_event(0x0Bu | 0xB0u << 8 | 1u << 16 | 40u << 24);
        events_block(CTL); frame();
        ck("CC1 127 ch 1: part 1's fader 127 (part 2's not), MODW 127; LAYER: CC1 40 both faders 40, both MODW",
           ok && mph[0].fader == 40u && mph[1].fader == 40u && trk[0].mw == 40u && trk[1].mw == 40u);
    }

    /* 5: the factory multis M01..M16 (build/gen/duell_multis.h, tools/factory_multis.py; DUELL 0.2.23: all sixteen slots, M16 the STRESS TEST;
     * 0.2.24: the owner's COLD CHAPEL, ACID ETHER and GARGLE ACID of 0.2.22 back in M06, M09, M14; 0.2.25: COLD CHAPEL in M13): in ROM, shown while the slot is empty in flash */
    {
        /* (reel, 2026-11-10: the fifteen new factory multis, one curated programme (tools/factory_multis.py); M02 DUB CHORD is the owner's record as before) */
        static const char *const FN[16] = {"BASIC CHORD", "DUB CHORD", "JUNO DEEP", "LATE RHODES", "ORGAN HOUSE", "PIANO HOUSE", "DETROIT STAB",
                                           "HI-TECH SOUL", "MINIMAL 12", "SEVEN LOOP", "ACID LINES", "ROBOT FUNK", "DEEP SEA ARP", "WARPED TAPE",
                                           "BERLIN RUN", "TWO LOOPS"};
        static const char *const SN[16][2] = {{"CHORD ECHO", "DUST BED"}, {"BIG STAB", "SLOW TIDE"}, {"WARM WASH", "RUBBER BAND"},
                                              {"TINE ROOM", "AIR SHIMMER"}, {"DUB DRAWBAR", "HOUSE ORGAN"}, {"SALOON KEYS", "RAIN STRINGS"},
                                              {"LOW STAB", "STRING MACH"}, {"SUNDAY KEYS", "BELL STAB"}, {"SOUL STAB", "FOG BANK"},
                                              {"HOLLOW POP", "IRON WORKS"}, {"SQUELCH", "DEEP BLUE"}, {"LADDER BASS", "VOWEL PAD"},
                                              {"DIGI PLUCK", "PWM PAD"}, {"WAVE CHIMES", "TAPE CLOUD"}, {"ARP PLUCK", "CHOIR LOFT"},
                                              {"FROST BELLS", "EVOLVING PAD"}};
        static const uint8_t MD[16] = {DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER,
                                       DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER, DM_LAYER};   /* (0.2.25: no factory multi in DUEL; M08 MONO DUEL, the only one, is spare: the owner's HAZY TENSOR, LAYER) */
        static const uint8_t BPM[16] = {122, 117, 116, 122, 124, 124, 125, 127, 128, 130, 124, 128, 133, 114, 112, 100};   /* (reel, 2026-11-10) */
        static const uint8_t SPL[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          /* (the splits: F4 + 1, compared with the key as played) */
        static const uint8_t XF[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};   /* (reel, 2026-11-10: no factory multi sets XFADE; until then M16, the owner's FINAL REEL, had 58) */
        /* (seq.c AM_*: UP 1, RND 4, SEQ 15; D31: the six new multis are SEQ arps on both parts (BASIC CHORD's DUST BED is played live); the kept records as they were) */
        /* (reel, 2026-11-10: ARP SEQ on both parts, but M03 JUNO DEEP, M06 PIANO HOUSE and M11 ACID LINES: written notes, plain patterns on both parts, the arp OFF) */
        static const int8_t AM[16][2] = {{15, 15}, {15, 15}, {0, 0}, {15, 15}, {15, 15}, {0, 0}, {15, 15}, {15, 15}, {15, 15},
                                         {15, 15}, {0, 0}, {15, 15}, {15, 15}, {15, 15}, {15, 15}, {15, 15}};
        static uint8_t before[0x100000];
        uint32_t n, st;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ok = 1;
        for (k = 0; k < MU_SLOTS; k++)
            ok &= project_used(k) && project_factory(k) && str_eq(mu_names[k], FN[k]);
        ck("empty flash: M01..M16 are the factory multis (the reel, 2026-11-10: BASIC CHORD, DUB CHORD, JUNO DEEP .. BERLIN RUN, TWO LOOPS; the owner's DUB CHORD kept)", ok);
        ok = 1;
        ui_prefs2 |= PREF_MODE_RECALL >> 16;           /* (0.2.11: the MODE is read back from the multi) */
        for (k = 0; k < MU_SLOTS; k++) {              /* each part's engine + factory index names the intended sound */
            project_load(k);
            for (pi = 0; pi < NSHOWN; pi++)
                ok &= str_eq(ds_name(trk[pi].eng_req, trk[pi].preset), SN[k][pi]) && (trk[pi].engine = trk[pi].eng_req, endb_set(pi)) &&
                      (AM[k][pi] < 0 || trk[pi].p[P_AMODE] == AM[k][pi]);
            ok &= duell_mode_req == MD[k] && proj_cur == k && song.g[G_BPM] == BPM[k] && mix_split == SPL[k] && (XF[k] == 255u || mix_xf == XF[k]) &&
                  song.g[G_RLOCUT] == 75;   /* (the reel re-voiced: DUB CHORD rebuilt with the owner's leave, at the default 75; 0.2.25: the owner's DUB CHORD (M02 since D31) has its reverb LOW CUT at 80; until the reel his FINAL REEL (M16) had 79) */
            for (pi = 0; pi < NSHOWN && AM[k][pi] == 15; pi++) {   /* (a SEQUENCER arp: its steps, offsets within two octaves, one note a step) */
                const track_t *t = &trk[pi];
                for (n = st = 0; n < NSTEP; n++) {
                    st += t->step[n].time == ST_NOTE && t->step[n].n;
                    ok &= t->step[n].time != ST_NOTE || (t->step[n].n == 1u && t->step[n].note[0] >= 36u && t->step[n].note[0] <= 84u);
                }
                ok &= st >= 1u;                    /* (reel, 2026-11-10: a held bed or drone is one note and its ties (M01's DUST BED, M10's IRON WORKS); until then every SEQ arp had two notes or more) */
            }
            if (!ok) {
                printf("multi:   M%02u %s: %s / %s, MODE %u, BPM %d, SPLIT %u, XF %u, arps %d %d\n", k + 1u, mu_names[k], ds_name(trk[0].eng_req, trk[0].preset),
                       ds_name(trk[1].eng_req, trk[1].preset), duell_mode_req, song.g[G_BPM], mix_split, mix_xf, trk[0].p[P_AMODE], trk[1].p[P_AMODE]);
                break;
            }
        }
        ck("LOAD of M01..M16 (MODE RECALL ON): the intended sounds, MODE, TEMPO, SPLIT / XFADE, arps and SEQUENCER steps, LOW CUT 75, an End B of its own", ok);
        {   /* (reel, 2026-11-10: D31's rule for its six new multis (every step C3..C6, at most six hits a bar on a part, no part on a bass register) no longer describes the set: the reel has
             * bass lines down to F2 and busy lines on purpose (JUNO DEEP, ORGAN HOUSE, ACID LINES, ROBOT FUNK, DEEP SEA ARP, BERLIN RUN). What still must hold of every one of the fifteen:
             * each part has a pattern of at least one note, inside its own length, every stored note a playable one (C1..C8, 24..108), the reverb LOW CUT at the default and an End B of its
             * own (both checked above) */
            uint32_t m6, bad6 = 0;
            for (m6 = 0; m6 < MU_SLOTS; m6++) {
                if (m6 == 1u)                         /* (M02 DUB CHORD: the owner's record, not one of the fifteen) */
                    continue;
                project_load(m6);
                for (pi = 0; pi < NSHOWN; pi++) {
                    const track_t *t = &trk[pi];
                    uint32_t hits = 0, len = (uint32_t)t->p[P_SLEN];
                    for (n = 0; n < NSTEP; n++)
                        if (t->step[n].time == ST_NOTE && t->step[n].n) {
                            uint32_t j6;
                            hits++;
                            bad6 += n >= len;
                            for (j6 = 0; j6 < t->step[n].n; j6++)
                                bad6 += t->step[n].note[j6] < 24u || t->step[n].note[j6] > 108u;
                        }
                    bad6 += hits == 0u || len == 0u || len > NSTEP;
                    if (bad6)
                        printf("multi:   M%02u part %u: %u hits, LEN %u, bad %u\n", m6 + 1u, pi + 1u, hits, len, bad6);
                }
            }
            ok = bad6 == 0u;
        }
        ck("  the reel: the fifteen new multis (all but M02): each part a pattern of at least one note inside its LEN, every stored note a playable one (C1..C8)", ok);
        {   /* M16 STRESS TEST (0.2.23): every effect on: both inserts, SAT, the EQ, the ducker, the three sends; RANDOM delay, SHIM (its record's PITCH BOTH reads UP since 0.2.25); an arp */
            scene_load(15, "M16_STRESS_TEST");            /* (the scene is stored in slot 16 for this check, erased again below) */
            ok = song.g[G_DMODE] == DMD_RAND && song.g[G_RTYPE] == 3 && song.g[G_SHPITCH] == SHP_UP && song.g[G_CDEPTH] > 0 && trk[1].p[P_AMODE] != 0;
            for (pi = 0; pi < NSHOWN; pi++)                /* (0.2.24: the part stages are split between the parts: FLANG + SAT on part 1, EQ + ducker on part 2) */
                ok &= trk[pi].p[P_CHOR] && trk[pi].p[P_DLY] && trk[pi].p[P_REV] && trk[pi].p[P_VOICE] == V_POLY && trk[pi].p[P_FREED] == 2;
            ok &= trk[0].p[P_ITYPE] != 0 && trk[0].p[P_DIST] != 0 && trk[1].p[P_EQLG] != 0 && trk[1].p[P_DKDEP] != 0;
        }
        project_erase(15);
        ck("  the STRESS TEST scene: the 24 dB filters and three sends on both parts, insert + SAT on part 1, EQ + ducker on part 2, RANDOM delay, SHIM (the record's PITCH BOTH loads as UP, 0.2.25), part 2's arp", ok);
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        memcpy(before, nor, sizeof nor);
        ck("project_erase of an unchanged factory slot: ALREADY FACTORY, nothing written (1.1: no ERASE card)", project_erase(0) == 1 && msg_is("ALREADY FACTORY") &&
           !memcmp(before, nor, sizeof nor));
        song.playing = 0;
        song.g[G_BPM] = 151;
        project_save_as(1, "MY STAB");                /* M02 DUB CHORD overwritten (D31; M04 before, WIRED SNAIL before that) */
        power_cycle();
        ck("SAVE over M02: after a power cycle it holds MY STAB (not factory), M01 M03 M04 still factory", str_eq(mu_names[1], "MY STAB") &&
           !project_factory(1) && project_factory(0) && project_factory(2) && project_factory(3));
        project_load(1);
        ck("  and loads the saved one (TEMPO 151)", song.g[G_BPM] == 151);
        ok = project_erase(1) == 0;
        power_cycle();
        project_load(1);
        ck("project_erase of the overwrite restores DUB CHORD (after a power cycle too); it loads again", ok && project_factory(1) &&
           str_eq(mu_names[1], "DUB CHORD") && str_eq(ds_name(trk[0].eng_req, trk[0].preset), "BIG STAB") && song.g[G_BPM] != 151);
        n = project_rename(11, "STACK 2");
        power_cycle();
        ck("rename of the factory M12 (ROBOT FUNK) stores it under the new name; the music of the record is the factory one", n == 0u &&
           str_eq(mu_names[11], "STACK 2") && !project_factory(11) && project_used(11));
        project_load(11);
        ck("  and it loads LADDER BASS + VOWEL PAD as before", str_eq(ds_name(trk[0].eng_req, trk[0].preset), "LADDER BASS") &&
           str_eq(ds_name(trk[1].eng_req, trk[1].preset), "VOWEL PAD"));
    }


    /* 3b: MENU > SYSTEM > TEST (developer builds) over the factory multis. (Until 0.2.22 also the CPU TEST multis M13..M16 of these builds; since
     * 0.2.23 they are the test scenes of tests/scenes.h and every slot holds a musical factory multi, M16 the STRESS TEST) */
    {
        char b[24];
        uint32_t f0;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        /* the TEST screen: MENU > SYSTEM > TEST (K1 right on the item opens it), not a page of GLO */
        ui.menu = 1; ui.menu_sel = MI_TEST; frames(16);
        turn(EN_K1, 1);
        ck("MENU > SYSTEM > TEST: opened with K1 right; the TEST screen (GR_TEST), no parameter on its knobs", ui.menu == 3u && TEST_PAGE.graph == GR_TEST &&
           TEST_PAGE.id[0] == 0xFFu && str_eq(TEST_PAGE.title, "TEST"));
        frames(16);
        test_pick = 0xFFu;
        project_cur_slot();
        proj_cur = PROJ_NO_SLOT;
        turn(EN_K3, 1);
        ck("K3 LOAD MULTI right: the next used multi (from none: M02 after the first used M01) is loaded at once, 'LOADED M02 DUB CHORD'",
           proj_cur == 1u && msg_is("LOADED M02 DUB CHORD") && str_eq(ds_name(trk[0].eng_req, trk[0].preset), "BIG STAB") && trk[1].eng_req == 0u && !ui.confirm);
        frame();
        frames(48);
        test_multi_name(b);
        ck("  the panel's multi line follows (M02 DUB CHORD)", str_eq(b, "M02 DUB CHORD"));
        turn(EN_K3, 2);
        /* (reel, 2026-11-10: the names and engines of the new M04, M10, M11, M14, M16) */
        ck("  2 detents right: M03 skipped through, M04 (stepping), loaded: M04 LATE RHODES", proj_cur == 3u && msg_is("LOADED M04 LATE RHODES"));
        turn(EN_K3, 6);
        ck("  6 more right: M10 SEVEN LOOP (the factory multis stepped through)", proj_cur == 9u && msg_is("LOADED M10 SEVEN LOOP"));
        turn(EN_K3, 1);
        ck("  one more right: M11 ACID LINES (ANALOG SQUELCH + PHASE DEEP BLUE)", proj_cur == 10u &&
           trk[0].eng_req == 0u && trk[1].eng_req == 2u && str_eq(ds_name(trk[0].eng_req, trk[0].preset), "SQUELCH") &&
           str_eq(ds_name(trk[1].eng_req, trk[1].preset), "DEEP BLUE") && msg_is("LOADED M11 ACID LINES"));
        test_multi_name(b);
        sound_name(&trk[0], nm_a);
        ck("  the engine and patch names the panel draws follow (ANALOG / SQUELCH, M11 ACID LINES)", str_eq(b, "M11 ACID LINES") &&
           str_eq(nm_a, "SQUELCH") && str_eq(ENGINES[trk[0].eng_req]->name, "ANALOG"));
        turn(EN_K3, 40);
#if DUELL_TIDE
        ck("  many detents right: stops at the last used, M18 VOID DEEP (the built-in drone multi, TIDE b6)", proj_cur == 17u && msg_is("LOADED M18 VOID DEEP"));
        turn(EN_K3, -2);
        ck("  2 left: M16 TWO LOOPS (FM6 + WAVE)", proj_cur == 15u && trk[0].eng_req == ENGI_FM6 && str_eq(ENGINES[trk[1].eng_req]->name, "WAVE"));
        turn(EN_K3, 2);
#else
        ck("  many detents right: stops at the last used, M16 TWO LOOPS (FM6 + WAVE)", proj_cur == 15u && trk[0].eng_req == ENGI_FM6 && str_eq(ENGINES[trk[1].eng_req]->name, "WAVE"));
#endif
        f0 = (uint32_t)proj_cur;
        turn(EN_K3, 1);
        ck("  right at the end: nothing", proj_cur == f0);
        turn(EN_K3, -(int32_t)(MU_SHOWN - 14u));
        ck("  back to M14 WARPED TAPE (WAVE + PHASE)", proj_cur == 13u && str_eq(ENGINES[trk[0].eng_req]->name, "WAVE") && str_eq(ENGINES[trk[1].eng_req]->name, "PHASE"));
        turn(EN_K3, -2);
        ck("  2 more left: M12", proj_cur == 11u);
        /* K4 RESET */
        cpu_miss = 7u;
        cpu_n = 1000u;
        cpu_sum = 400000u;
        test_t = 0;
        frames(160);
        cpum.hold = 91u;
        cpum.peak = 91u;
        cpum.miss = cpu_miss;
        cpum.base = 0u;
        ck("(a state to reset: 7 missed blocks, peak hold 91 %)", test_missed() == 7u && cpum.hold == 91u);
        turn(EN_K4, 1);
        ck("K4 RESET: 'RESET'; the peak hold, the missed count and the average window are cleared (the power-on count of ABOUT stays)",
           msg_is("RESET") && test_missed() == 0u && cpum.hold < 91u && cpu_pk == 0u && cpu_miss == 7u && cpum.n0 == cpu_n && !cpu_recent());
        cpu_miss = 9u;
        frames(1200);
        ck("  a new miss after RESET is counted from zero (2) and shows as recent", test_missed() == 2u && cpu_recent());
        cpu_miss = 0u;
        turn(EN_K4, -1);
        ck("  K4 turned left resets too", msg_is("RESET") && test_missed() == 0u);
        /* K1 MORPH, K2 ROLL: the MORPH page's */
        project_load(13);
        ck("  (still on the TEST screen after a load)", ui.menu == 3u);
        SEL_BOTH = 0;
        song.sel = 0;
        turn(EN_K1, 20);
        ck("K1 MORPH: the selected part's fader moves (MORPH TARGET: selected)", morph_fader_of(0) > 0u && morph_fader_of(1) == 0u);
        SEL_BOTH = 1;
        f0 = morph_fader_of(0);
        turn(EN_K1, -5);
#if DUELL_TIDE
        ck("  MORPH TARGET BOTH: each part's fader by the same steps (1.0.0-b7; part 2 at A stays at A)", morph_fader_of(0) == f0 - 10u && morph_fader_of(1) == 0u);
#else
        ck("  MORPH TARGET BOTH: both parts' faders", morph_fader_of(0) < f0 && morph_fader_of(1) == morph_fader_of(0));
#endif
        turn(EN_K2, -MORPH_DET);
        ck("K2 ROLL left past the detent: ROLLED A (as the MORPH page)", msg_is("ROLLED A"));
        frames(600);
        turn(EN_K2, MORPH_DET);
        ck("  right: ROLLED B", msg_is("ROLLED B"));
        ck("  the TEST screen is still shown (K1..K4 never leave it, K1 left included)", ui.menu == 3u);
        press(B_OCTDN);
        ck("  OCT-: back to the menu tabs, TEST the row", ui.menu == 1u && ui.menu_sel == MI_TEST);
        /* ABOUT's first view is the splash animation: endless, frozen by ANIM OFF, gone once SELECT scrolls */
        ui.menu = 2; ui.menu_scroll = 0; about_t0 = fm1_ms;
        frames(32);
        f0 = about_step();
        frames(2000);
        ck("ABOUT, first view: the animation steps on without end (a step every 66 ms, 2 s later still running)", about_step() > f0 + 20u);
        ui_prefs |= PREF_ANIM_OFF;
        ck("  MENU > ANIM OFF: the first frame, still", about_step() == 0u);
        ui_prefs &= ~PREF_ANIM_OFF;
        ui.menu_scroll = 18;
        ck("  scrolled (SELECT): the information, no animation", about_step() == 0u);
        ui.menu = 0;
    }

    /* 4: the first start (DUELL 0.1). A unit whose settings sector holds nothing valid (erased, stock firmware's data, garbage) starts in
     * VIOLET, MODE LAYER, factory multi M01 loaded as the page's LOAD does it; the settings are then saved, so the next start is
     * an ordinary one (the default sounds are back only if nobody saved: here the saved settings win); an older / other valid
     * settings blob keeps its theme and mode, nothing is forced */
    {
        static track_t m01[NSHOWN];
        static persist_t pb;
        uint32_t m01_bpm, m01_mode, sector, i, m01_lo[2];
        int16_t g_copy[G_COUNT];
        /* the reference: what M01 loads to */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;           /* (0.2.11: the reference reads M01's stored MODE) */
        project_load(TIDE_FIRST_MULTI);                /* (the multi a new unit starts with: M01; TIDE's hook H18 may pick another) */
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        memcpy(m01, trk, sizeof m01);
        m01_bpm = (uint32_t)song.g[G_BPM];
        m01_mode = duell_mode_req;
        memcpy(g_copy, song.g, sizeof g_copy);
        ck("M01 is the factory BASIC CHORD (D31; read through the factory path)", str_eq(mu_names[0], "BASIC CHORD") && project_factory(0));

        for (sector = 0; sector < 3u; sector++) {     /* 0 erased flash, 1 garbage (stock firmware's data), 2 zeros */
            memset(nor, 0xFF, sizeof nor);
            for (i = 0; sector && i < 0x2000u; i++)
                nor[0xFC000u + i] = sector == 1u ? (uint8_t)(i * 131u + 7u) : 0u;
            power_cycle();
            memset(&settings, 0, sizeof settings);      /* (.noinit after a power-off: nothing) */
            settings.palette = 7777u;
            first_boot_pending = 0;
            memset(&persist_saved, 0, sizeof persist_saved);
            persist_boot();
            settings_init();
            ck(sector == 0u ? "first start on erased flash: counted as new (no valid settings)" :
               sector == 1u ? "  garbage in the flash: counted as new too" : "  zeros in the flash: counted as new too", first_boot_pending == 1u);
            ck("  the theme is VIOLET (the default index, UI_DEFAULT_INDEX 0)", settings.palette == UI_DEFAULT_INDEX &&
               str_eq(UI_PALETTES[settings.palette].name, "VIOLET"));
            if (autosave_boot(1))
                first_boot_pending = 0;
            ck("  no autosave on a new unit", first_boot_pending == 1u);
            ck("  (before it runs the unit has its default sounds, not M01's)", trk[0].eng_req != m01[0].eng_req || trk[0].preset != m01[0].preset);
            frames(160);
            ck("  MODE is LAYER (asked for and in its settings byte)", duell_mode_req == DM_LAYER && ui_mode_set == DM_LAYER && m01_mode == DM_LAYER);
            ck("  M01 is loaded: both parts' engines, presets and sounds as M01 holds them",
               trk[0].eng_req == m01[0].eng_req && trk[0].preset == m01[0].preset && trk[1].eng_req == m01[1].eng_req &&
               trk[1].preset == m01[1].preset && !memcmp(trk[0].p, m01[0].p, sizeof trk[0].p) && !memcmp(trk[1].p, m01[1].p, sizeof trk[1].p));
            ck("  TEMPO and the globals as M01 holds them", (uint32_t)song.g[G_BPM] == m01_bpm && !memcmp(song.g, g_copy, sizeof g_copy));   /* (item 160: no balance value any more) */
            ck("  the first-start flag is used up", first_boot_pending == 0u);
            events_block(CTL);                        /* (the audio side takes the MODE: LAYER)  */
            if (sector == 0u) {                       /* tester bug: a black key played a semitone apart on the two parts */
                uint32_t kk;
                for (kk = 0; kk < 2u; kk++) {
                    static const uint32_t KEY2[2] = {0, 7};   /* F3 / F#3, then C4 / C#4 (key 0 = F3) */
                    uint32_t j;
                    for (j = 0; j < 2u; j++) {
                        uint32_t key = KEY2[kk] + j, lo[2], n;
                        for (i = 0; i < 2u; i++)
                            lo[i] = 255u;
                        key_down(key);
                        for (n = 0; n < 400u; n++) {        /* (part 2's arp starts its notes on its clock: audio blocks) */
                            events_block(CTL);
                            for (i = 0; i < 2u; i++) {
                                uint32_t v;
                                for (v = 0; v < NVOICE; v++)
                                    if (trk[i].v[v].active && trk[i].v[v].gate && trk[i].v[v].note < lo[i])
                                        lo[i] = trk[i].v[v].note;
                            }
                        }
                        key_up(key);
                        frames(32);
                        if (j == 0u)
                            m01_lo[0] = lo[0], m01_lo[1] = lo[1];
                        else {
                            printf("multi: M01 keys %u,%u: part 1 lowest note %u -> %u, part 2 %u -> %u\n", key - 1u, key, m01_lo[0], lo[0], m01_lo[1], lo[1]);
                            ck("  M01 first boot: a key and the key above it (C, C#) are one semitone apart on BOTH parts, the same root on both (whole octaves apart)",
                               lo[0] == m01_lo[0] + 1u && lo[1] == m01_lo[1] + 1u && (m01_lo[1] + 120u - m01_lo[0]) % 12u == 0u &&
                               (lo[1] + 120u - lo[0]) % 12u == 0u && lo[0] < 128u && lo[1] < 128u);   /* (D31: the first start's DUB CHORD: BIG STAB and TIDAL SUB, whole octaves apart) */
                        }
                    }
                }
            }
            frames(2400);
            if (sector == 0u) {                       /* (0.2.11: the write waits for silence; frames() renders no audio, so the voices of the keys played above never die: let them end) */
                uint32_t k, v;
                for (k = 0; k < NTRK; k++)
                    for (v = 0; v < NVOICE; v++)
                        trk[k].v[v].active = 0;
                frames(2400);
                settings_poll();                      /* (the main loop's call: frame() makes none) */
            }
            ck("  the settings are saved (a valid record in the flash now)", (st_load(OBJ_SETTINGS, &pb, sizeof pb) > 0) && pb.favorites.factory[15][9] == DM_LAYER);
            /* the next start: an ordinary one, theme and mode as saved */
            power_cycle();
            memset(&settings, 0, sizeof settings);
            settings.palette = 7777u;
            first_boot_pending = 0;
            persist_boot();
            settings_init();
            ck("  next start: not new any more (settings valid), VIOLET and LAYER kept", first_boot_pending == 0u && settings.palette == UI_DEFAULT_INDEX &&
               ui_mode_set == DM_LAYER);
            if (sector == 2u)
                break;
        }

        /* an older or other valid settings blob keeps its theme, mode, scale and root; the unit starts as it was left: the autosave is restored
         * (RESTORE LAST ON, bit 128 clear), not M01; DUELL 0.2.2: a part muted in the autosave starts audible */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        memset(&pb, 0, sizeof pb);
        settings_export(&pb);
        pb.palette = palette_to_stored(4u);
        pb.favorites.factory[15][9] = DM_STEREO;
        pb.favorites.factory[15][26] = 1u;
        pb.favorites.factory[15][3] = 0x80u | 5u;          /* the global scale (format flag + scale 5) and root 7 */
        pb.favorites.factory[15][4] = 7u;
        pb.favorites.factory[15][30] &= (uint8_t)~128u;    /* RESTORE LAST ON (bit 7 clear, as every older blob) */
        ck("an older DUELL settings record (theme 4, STEREO, 0.1 beta's HOME view, a scale) is stored", st_save(OBJ_SETTINGS, &pb, sizeof pb) == 0);
        {
            uint32_t pass;
            power_cycle();                                 /* an autosave from an earlier session: part 1 MUTED, tempo 77 */
            host_tracks_init();
            trk[0].p[P_MUTE] = 1;
            song.g[G_BPM] = 77;
            project_capture(&proj_scratch);
            proj_wire_gen++;
            ck("  an autosave with a muted part 1 and tempo 77 is on the flash",
               proj_pack(&proj_wire, &proj_scratch) && st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) == 0);
            for (pass = 0; pass < 2u; pass++) {            /* 0: RESTORE LAST ON (the older blob), 1: OFF (bit 128 set) */
                power_cycle();
                memset(&settings, 0, sizeof settings);
                first_boot_pending = 0;
                if (pass) {
                    pb.favorites.factory[15][30] |= 128u;
                    ck("  RESTORE LAST OFF stored (a settings blob)", st_save(OBJ_SETTINGS, &pb, sizeof pb) == 0);
                    power_cycle();
                    memset(&settings, 0, sizeof settings);
                }
                persist_boot();
                settings_init();
                ck(pass ? "  OFF is read from the blob (editor id 15 value 1)" : "  the older blob reads RESTORE LAST ON (editor id 15 value 0)",
                   menu_get(MI_RESTORE) == pass && !!(ui_prefs & PREF_RESTORE_OFF) == (int)pass);
                if (autosave_boot(1))
                    first_boot_pending = 0;
                frames(300);
                if (!pass) {
                    ck("  power-on restores the autosave as left (tempo 77, no M01, not a first start)", first_boot_pending == 0u && song.g[G_BPM] == 77 &&
                       proj_cur == PROJ_NO_SLOT && !str_eq(proj_name, TIDE_FIRST_MULTI ? "DUB CHORD" : "BASIC CHORD"));
                    ck("  .. the muted part 1 starts audible (MUTE cleared at power-on)", !trk[0].p[P_MUTE] && !trk[1].p[P_MUTE]);
                    ck("  .. the theme (4), MODE (STEREO), scale 5 and root 7 stay as stored", settings.palette == palette_from_stored(pb.palette) &&
                       duell_mode_req == DM_STEREO && (ui_scale_set & 0xCFu) == (0x80u | 5u) && ui_root_set == 7u);
                } else {
                    ck("  RESTORE LAST OFF: the autosave is not restored, and it is not a first start either (no M01)", first_boot_pending == 0u &&
                       song.g[G_BPM] != 77 && !str_eq(proj_name, TIDE_FIRST_MULTI ? "DUB CHORD" : "BASIC CHORD") && proj_cur == PROJ_NO_SLOT);
                }
            }
        }
        /* a multi that holds a muted part: a LOAD keeps the stored mute (the GLO page shows MUTED) */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        memset(&settings, 0, sizeof settings);
        persist_boot();
        settings_init();
        project_load(0);
        trk[1].p[P_MUTE] = 1;
        ck("  (M01 overwritten with part 2 muted)", project_save(0) == 0 && !project_factory(0));
        project_load(0);
        ck("  a LOAD of that multi keeps its stored mute (part 2 MUTED on the GLO page)", trk[1].p[P_MUTE] == 1 && !trk[0].p[P_MUTE]);
        /* DUELL 0.2.14: the shorter record of the first DUELL builds (no user_hi) is no record any more: refused, the unit starts as a new one */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        memset(&pb, 0, sizeof pb);
        settings_export(&pb);
        pb.palette = palette_to_stored(4u);
        pb.favorites.factory[15][26] = 1u;
        ck("  the shorter older record (without the patches' favourites 32..127) is stored", st_save(OBJ_SETTINGS, &pb, (uint32_t)(sizeof pb - sizeof pb.favorites.user_hi)) == 0);
        power_cycle();
        memset(&settings, 0, sizeof settings);
        first_boot_pending = 0;
        persist_boot();
        settings_init();
        frames(100);
        ck("  and it is refused (0.2.14): the unit starts as new (its default theme, the home byte 0), nothing of the record taken over, no crash",
           settings.palette != palette_from_stored(pb.palette) && favorites.factory[15][26] == 0u && menu_get(MI_HOMEPG) == 0u);
    }


    /* 0.2.11: MENU > CONTROL > MODE RECALL (KEEP by default): a multi load leaves the unit's MODE; LOAD sets the one saved in the multi */
    {
        static const uint8_t MODES[3] = {DM_LAYER, DM_SPLIT, DM_STEREO};
        static persist_t pb;
        uint32_t m, k, pi;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ok = !(ui_prefs2 & (PREF_MODE_RECALL >> 16)) && menu_get(MI_MODERECALL) == 0u && str_eq(menu_vname(MI_MODERECALL, 0), DUELL_TIDE ? "KEEPS PLAY MODE" : "KEEPS MODE") &&
             str_eq(menu_vname(MI_MODERECALL, 1), DUELL_TIDE ? "SETS PLAY MODE" : "SETS MODE") && str_eq(MI_NAME[MI_MODERECALL], "MULTI LOAD") && MI_TAB[MI_MODERECALL] == MTAB_CONTROL;
        ck("MODE RECALL (0.2.14: MULTI LOAD KEEPS MODE / SETS MODE): KEEP by default (CONTROL tab)", ok);
        ok = 1;
        for (m = 0; m < 3u; m++)                      /* KEEP: M01, M02, M04 (all LAYER): the mode stays */
            for (k = 0; k < 3u; k++) {
                duell_mode_req = MODES[m];
                project_load(k == 0 ? 0u : k == 1 ? 1u : 3u);
                ok &= duell_mode_req == MODES[m];
                for (pi = 0; pi < NSHOWN; pi++)
                    ok &= trk[pi].eng_req < NENGINES;
            }
        ck("KEEP: a multi load leaves the MODE as it is, from LAYER, DUEL and STEREO alike", ok);
        ok = 1;
        for (m = 0; m < 3u; m++) {                    /* a multi saved in each mode: LOAD sets it, KEEP does not */
            duell_mode_req = MODES[m];
            ok &= project_save_as(8u + m, "MODE") == 0;
        }
        for (m = 0; m < 3u; m++) {
            duell_mode_req = MODES[(m + 1u) % 3u];
            project_load(8u + m);
            ok &= duell_mode_req == MODES[(m + 1u) % 3u];
        }
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        for (m = 0; m < 3u; m++) {
            duell_mode_req = MODES[(m + 1u) % 3u];
            project_load(8u + m);
            ok &= duell_mode_req == MODES[m];
            {
                uint32_t len;
                const uint8_t *r;
                mu_obj_read((8u + m) / 2u);
                r = mu_rec((8u + m) & 1u, &len);
                ok &= len > 4u && r[4] == MODES[m];   /* (the header byte 4 is the saved MODE) */
            }
        }
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        ck("a save still writes the MODE (header byte 4); LOAD applies it from any mode, KEEP does not", ok);
        {   /* XFADE, SPLIT follow the option too (saved in the multi either way). Item 157: BALANCE is no multi value: the record's byte before XFADE is a
             * spare written 0 (the record's size and layout as before), a load never reads it (item 160: the balance value itself is gone; PERFORM's SELECT moves the LEVELs) */
            uint32_t len, spare0 = 1, xf_ok = 1;
            const uint8_t *r;
            ok = 1;
            for (m = 0; m < 3u; m++) {
                duell_mode_req = MODES[m];
                mix_xf = 41; mix_split = 61;
                ok &= project_save_as(8u, "BAL") == 0;
                mu_obj_read(8u / 2u);
                r = mu_rec(8u & 1u, &len);
                spare0 &= len > 3u && r[len - 3u] == 0u;
                xf_ok &= len > 3u && r[len - 2u] == 41u && r[len - 1u] == 61u;
                mix_xf = 17; mix_split = 71;
                project_load(8u);
                ok &= mix_xf == 17 && mix_split == 71;
                ui_prefs2 |= PREF_MODE_RECALL >> 16;
                project_load(8u);
                ok &= mix_xf == 41 && mix_split == 61;
                ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
            }
            ck("XFADE / SPLIT: OFF keeps them after a load (from each mode), ON applies the multi's", ok);
            ck("  the record: the spare byte (BALANCE's until item 157) is 0, XFADE and SPLIT the last two bytes", spare0 && xf_ok);
            mix_xf = 6; mix_split = 7;
            ui_prefs2 |= PREF_MODE_RECALL >> 16;
            project_load(0);
            ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        }
        /* a MIDI program change recalls through the same project_load */
        duell_mode_req = DM_STEREO;
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        project_load(8u);
        ck("  (KEEP: also a recall by program change, the same path)", duell_mode_req == DM_STEREO);
        /* the option in the settings: an older blob (bit clear) reads KEEP; LOAD survives a save and a power cycle */
        memset(&pb, 0, sizeof pb);
        settings_export(&pb);
        pb.favorites.factory[15][24] = 0x07u;
        memset(nor, 0xFF, sizeof nor);
        ck("  an older settings blob (bits 0-2 only) is stored", st_save(OBJ_SETTINGS, &pb, (uint32_t)sizeof pb) == 0);
        power_cycle();
        memset(&settings, 0, sizeof settings);
        persist_boot();
        settings_init();
        ck("  ..reads MODE RECALL KEEP (editor id 27 value 0), the other three flags kept", menu_get(MI_MODERECALL) == 0u && (ui_prefs2 & 7u) == 7u);
        menu_put(MI_MODERECALL, 1);
        settings_export(&pb);
        ck("  LOAD is saved in bit 6 of favorites.factory[15][24]", pb.favorites.factory[15][24] == (0x47u | 0x80u));   /* (+ 0.2.14's defaults flag, bit 7) */
        memset(&favorites, 0, sizeof favorites);
        ck("  ..and imported again as LOAD", settings_import(&pb, (int)sizeof pb) && menu_get(MI_MODERECALL) == 1u);
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
    }

    {   /* DUELL 0.2.14: a multi record of firmware before (version 2 or 1, or another P_COUNT) in flash is no multi: its slot lists as empty,
         * a LOAD changes nothing and does not crash, a factory slot shows its factory multi, the next SAVE replaces the old record */
        static uint8_t old[MU_REC_MAX], keep[MU_REC_MAX];
        uint32_t len, cut = 0, o, v, ok2 = 1;
        char nm[PROJ_NAME_LEN + 1u];
        ui_power_on();
        trk[0].p[P_LEVEL] = 77;
        ok2 &= project_save_as(10u, "OLD ONE") == 0 && project_used(10u);
        for (v = 0; v < 3u; v++) {
            o = 10u / 2u;
            mu_obj_read(o);
            {
                const uint8_t *r = mu_rec(10u & 1u, &len);
                memcpy(old, r, len);
            }
            if (v == 0u) old[0] = 2u; else if (v == 1u) old[0] = 1u; else { old[0] = 3u; old[1] = (uint8_t)(P_COUNT - 1u); }
            memcpy(keep, old, len);
            mu_obj_read(o);
            ok2 &= mu_obj_write(o, mu_put_rec(10u & 1u, 1, &cut, keep, len)) == 0;
            mu_obj_read(o);
            mu_note(o);
            trk[0].p[P_LEVEL] = 55;
            /* (0.2.23: every slot has a factory multi, so the slot shows that one; a slot with none would list "--" and load nothing) */
#if DUELL_TIDE                                       /* (audit C01: a record that is there and does not load is DAMAGED, the factory multi is not shown for it) */
            ok2 &= !project_used(10u) && multi_bad(10u) && !project_info(10u, nm);
#else
            ok2 &= project_used(10u) && project_factory(10u) && project_info(10u, nm) && !strcmp(nm, "EERIE SIGNAL");
#endif
            project_load(10u);                       /* (the old record is there: DAMAGED, nothing loaded, no crash) */
            ok2 &= trk[0].p[P_LEVEL] == 55;
            trk[0].p[P_LEVEL] = 77;
        }
        ck("a multi record of version 2 / 1 or another P_COUNT in M11: no multi; the slot shows its factory multi (EERIE SIGNAL), a LOAD changes nothing", ok2);
        /* a factory slot (M01) holding such a record shows its factory multi */
        o = 0;
        mu_obj_read(o);
        {
            uint32_t l0;
            const uint8_t *r = mu_rec(0u, &l0);
            ok2 = DM_LEN[df_of(0u)] <= sizeof old;
            memmove(old, dm_rec(df_of(0u), old), DM_LEN[df_of(0u)]);   /* (packed: rebuilt into old; plain DUELL: copied from the ROM) */
            old[0] = 2u;
            (void)r;
            ok2 &= mu_obj_write(o, mu_put_rec(0u, 1, &cut, old, DM_LEN[df_of(0u)])) == 0;
        }
        mu_obj_read(o);
        mu_note(o);
#if DUELL_TIDE
        ok2 &= !project_used(0u) && multi_bad(0u) && !project_info(0u, nm);
        ck("  a factory slot (M01) holding one is DAMAGED too (audit C01: not its factory multi), a LOAD changes nothing", ok2 && (project_load(0u), 1));
#else
        ok2 &= project_used(0u) && project_factory(0u) && project_info(0u, nm) && !strcmp(nm, "BASIC CHORD");
        ck("  a factory slot (M01) holding one shows its factory multi (BASIC CHORD) and loads it", ok2 && (project_load(0u), 1));
#endif
        ok2 = project_save_as(10u, "NEW ONE") == 0 && project_used(10u) && !project_factory(10u) && project_info(10u, nm) && !strcmp(nm, "NEW ONE");
        ck("  a SAVE over the old record replaces it, the multi loads again", ok2);
        project_erase(10u);
    }
    {   /* MU_VER 8 (ROLL CHAINED storage, 2026-10-29): per part End B as values (an MBV block after its length byte), the NX slots dense
         * without a count, no edit list (the values set by hand are End B's marks), steps in 7 bytes; every other version refused */
        static project_t q, q2;
        static uint8_t rec[2048], keep[2048];
        uint8_t amt[NSHOWN], set[6];
        uint32_t cut, len, i, t, ok3, base, evs[2], p0 = MU_G_AT + 2u * MU_NG, nb0;
        ck("MU_VER is 8 and a part without End B, steps or patch is MU_PART_FIX bytes (engine, preset, AMOUNT, flags, End B's length, the values, the NX slots, the step map)",
           MU_VER == 8u && MU_PART_FIX == 4u + 1u + P_COUNT + NX + 8u && MU_STEP == 7u);
        /* (a) the music's slots through SAVE, a power cycle and LOAD */
        ui_power_on();
        for (t = 0; t < NSHOWN; t++)
            for (i = 0; i < NX; i++)
                XV(&trk[t], i) = (int16_t)(t ? 100 - 5 * (int)i : 3 * (int)i - 40);
        ok3 = project_save_as(12u, "XSLOTS") == 0;
        for (t = 0; t < NSHOWN; t++)
            for (i = 0; i < NX; i++)
                XV(&trk[t], i) = 0;
        ui_power_on();
        project_load(12u);
        for (t = 0; t < NSHOWN; t++)
            for (i = 0; i < NX; i++)
                ok3 &= XV(&trk[t], i) == (t ? 100 - 5 * (int)i : 3 * (int)i - 40);
        ck("the extension slots of both parts survive SAVE, a power cycle and LOAD (through XV)", ok3);
        project_erase(12u);
        /* (b) the record: round trip of the slots and both parts' End B blocks */
        for (t = 0; t < NSHOWN; t++) {
            trk[t].engine = trk[t].eng_req;
            morph_ready(t);
            morph_roll_seed(t, 1u, 4242u + t);
        }
        project_capture(&q);
        for (t = 0; t < NSHOWN; t++)
            for (i = 0; i < NX; i++)
                q.t[t].x[i] = (int16_t)(t ? 127 - (8 * (int)i) % 191 : -64 + (9 * (int)i) % 191);   /* (kept inside -64 .. 127) */
        len = mu_pack(rec, &q, &cut);
        ok3 = !cut && len <= MU_REC_MAX && rec[0] == 8u && q.bvn[0] && q.bvn[1] && mu_unpack(&q2, amt, set, rec, len);
        for (t = 0; ok3 && t < NSHOWN; t++)
            ok3 &= !memcmp(q2.t[t].x, q.t[t].x, sizeof q.t[t].x) && q2.bvn[t] == q.bvn[t] && !memcmp(q2.bv[t], q.bv[t], q.bvn[t]) && q2.xe_n[t] == 0u &&
                   !memcmp(q2.t[t].p, q.t[t].p, sizeof q.t[t].p) && !memcmp(q2.t[t].step, q.t[t].step, sizeof q.t[t].step);
        ck("a record with both parts' End B and all slots off their defaults: packs without a cut, unpacks equal (values, slots, steps, End B blocks)", ok3);
        memcpy(keep, rec, len);
        nb0 = rec[p0 + 4u];
        rec[0] = 7u;
        ck("  the same bytes marked version 7 (the old form, with edit lists): not a multi", !mu_unpack(&q2, amt, set, rec, len));
        rec[0] = 9u;
        ck("  marked version 9: not a multi", !mu_unpack(&q2, amt, set, rec, len));
        memcpy(rec, keep, len);
        rec[p0 + 4u] = (uint8_t)(MBV_MAX + 1u);
        ck("an End B block longer than MBV_MAX: not a multi", !mu_unpack(&q2, amt, set, rec, len));
        memcpy(rec, keep, len);
        rec[p0 + 4u] = (uint8_t)(nb0 - 1u);
        ck("  an End B length that is not its block's own: not a multi", !mu_unpack(&q2, amt, set, rec, len));
        memcpy(rec, keep, len);
        rec[p0 + 5u + nb0 + P_COUNT + 2u] = 192u;
        ck("a slot value byte above 191: not a multi", !mu_unpack(&q2, amt, set, rec, len));
        memcpy(rec, keep, len);
        rec[p0 + 5u + nb0 + 3u] = 192u;
        ck("  a parameter value byte above 191: not a multi", !mu_unpack(&q2, amt, set, rec, len));
        memcpy(rec, keep, len);
        ck("  one byte more than the record: not a multi (the length is exact)", !mu_unpack(&q2, amt, set, rec, len + 1u));
        ck("  every byte of the record cut short: not a multi (never read past the end)", ({ uint32_t c, okc = 1; memcpy(rec, keep, len);
            for (c = 0; c < len; c++) okc &= !mu_unpack(&q2, amt, set, rec, c); okc; }));
        /* (c) the worst multi: two FM6 parts with End B, 64 steps each, and a full automation */
        for (t = 0; t < NSHOWN; t++) {
            q.t[t].engine = ENGI_FM6;
            for (i = 0; i < NSTEP; i++) {
                step_clear(&q.t[t].step[i]);
                q.t[t].step[i].note[0] = (uint8_t)(30u + (i + t) % 60u);
                q.t[t].step[i].n = 1;
                q.t[t].step[i].time = ST_NOTE;
                q.t[t].step[i].vel = (uint8_t)(40u + i);
            }
        }
        for (i = 0; i < MOTION_MAX; i++)
            q.motion.event[i] = (motion_event_t){(uint8_t)((i & 1u) << 6 | (i >> 1) % 64u), P_REV, (int8_t)(i % 100u)};
        q.motion.count = MOTION_MAX;
        q.motion.on = 3;
        for (base = 0; base < 2u; base++) {
            if (base == 1u)                                  /* few steps: everything fits */
                for (t = 0; t < NSHOWN; t++)
                    for (i = 8; i < NSTEP; i++) {
                        memset(&q.t[t].step[i], 0, sizeof q.t[t].step[i]);
                        step_clear(&q.t[t].step[i]);
                    }
            len = mu_pack(rec, &q, &cut);
            ok3 = len <= MU_REC_MAX && mu_unpack(&q2, amt, set, rec, len);
            evs[base] = ok3 ? q2.motion.count : 999u;
            for (t = 0; ok3 && t < NSHOWN; t++)
                ok3 &= !memcmp(q2.t[t].step, q.t[t].step, sizeof q.t[t].step) && q2.bvn[t] == q.bvn[t] && !memcmp(q2.bv[t], q.bv[t], q.bvn[t]);
            printf("multi: worst multi, case %u (%s): %u bytes of %u, automation %u of %u events kept, cut bits %u\n", base,
                   base == 0u ? "64 steps each" : "8 steps each", len, (unsigned)MU_REC_MAX, evs[base], (unsigned)MOTION_MAX, cut);
            if (base == 0u)
                ck("  worst multi (2 FM6 parts, End B, 64 steps each, full automation): fits; the sound, steps and End B stay, the automation is cut to what fits",
                   ok3 && cut == 1u && evs[0] > 0u && evs[0] < MOTION_MAX && len + 3u > MU_REC_MAX);
            else
                ck("  with 8 steps a part: everything fits, the whole automation too", ok3 && !cut && evs[1] == MOTION_MAX);
        }
        ck("  the sound, End B and steps of the worst multi always fit (_Static_assert in multi.c)",
           MU_G_AT + 2u * MU_NG + NSHOWN * (MU_PART_FIX + MBV_MAX + NSTEP * MU_STEP + UPF_PK) + 1u + 2u <= MU_REC_MAX);
    }
#if DUELL_TIDE
    drones();                                        /* TIDE b6: M17 DUNE HORN, M18 VOID DEEP, ROM only */
#endif
    keys_drive();                                    /* 6: the factory multis played from the keys (0.2.12; 0.2.23's set) */
    editb_worst_multi();                             /* 7: cloud job G, the worst multi through SAVE (it leaves the state changed: last) */
    printf(bad ? "multi: FAILED (%d)\n" : "multi: all ok\n", bad);
    return bad ? 1 : 0;
}
