/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's sequencer side (tide.h H1-H3), included at the end of seq.c: SEQ MORPH (seq_morph.c) with the phrase lifecycle
 * (seq_rhythm.c, seq_phrase.c), the scale-degree walk (seq_evolve.c), voice leading (seq_voicelead.c) and the peer
 * prediction (seq_dialogue.c), from the JAM fork (branch jam, 71654ba). 1.1 has no held-note continuations (the fork's
 * step_t.cont): a hold is a tie here, so no step is a continuation of the one before.
 * Contexts: tide_play_step in the audio path; everything else in the main loop. */
static uint32_t step_cont(const step_t *s) { (void)s; return 0u; }   /* (no per-note continuations in 1.1: ties instead) */
static const step_t *seq_play_step(const track_t *t, uint32_t idx, step_t *buf);   /* (seq_dialogue.c reads it before seq_morph.c has it) */
/* The chance rolls the TIDE page shows: a bit a step of the last roll's miss (a step's CHANCE in seq_play, or, on an ARP SEQ part, the arp's PROB
 * in arp_fire), set or cleared where the sequencer rolls, cleared at PLAY. A step has rolled in the pass when it is at or behind the playhead:
 * the page reads the bit then, and shows the step undecided before. Audio path writes, main loop reads */
static volatile uint32_t tide_miss[NSHOWN][2];
#include "seq_morph.c"
#include "seq_poly.c"
#include "seq_answer.c"
#include "seq_arc.c"

static void tide_miss_put(const track_t *t, uint32_t idx, int miss)
{
    uint32_t pi = trk_index(t);
    if (pi < NSHOWN && idx < NSTEP)
        tide_miss[pi][idx >> 5] = (tide_miss[pi][idx >> 5] & ~(1u << (idx & 31u))) | (uint32_t)(miss != 0) << (idx & 31u);
}
static int tide_chance_roll(const track_t *t, uint32_t idx, uint32_t chance, int miss)
{
    if (chance && arp_mode(t) != AM_SEQ_ID)               /* (an ARP SEQ part's steps do not play: its rolls are the arp's) */
        tide_miss_put(t, idx, miss);
    return miss;
}
static int tide_aroll(const track_t *t, int hit)
{
    tide_miss_put(t, tide_arp_idx(t, t->arp_idx, tide_poly_full(t)), !hit);
    return hit;
}

static const step_t *tide_play_step(const track_t *t, uint32_t idx, step_t *buf)
{
    return seq_play_step(t, idx, buf);
}

/* ARP SEQ on PLAY with no key held (owner, 2026-11-01: it was silent; the steps are offsets on held keys): the arp plays from a
 * stand-in, the part's last chord held on the SEQ arp, else C4 as the part's CHRD builds it (audit G08: always one note), the steps as written.
 * Also while recording (an ARP SEQ part's arp never records: seq.c rec_arp_on), not stopped. A key held plays as ever */
static uint8_t tide_chord[NSHOWN][4], tide_chord_n[NSHOWN];
/* b7 (MR2 part B), one byte of state: bit p the ghost chord sounds on part p; bit 2 + p part p was played by hand since PLAY; bit 4 tide_ghost_poll's own
 * input_on is running; bit 5 AUTO TIDE plays (once a frame: glide_tide_on() with the transport running) */
static uint8_t tide_gh;
#define TIDE_GH_ON(p) ((tide_gh >> (p)) & 1u)
#define TIDE_GH_HAND(p) ((tide_gh >> (2u + (p))) & 1u)
#define TIDE_GH_IN 0x10u
#define TIDE_GH_AUTO 0x20u
/* b7 (musical review MR2 part B): while AUTO TIDE plays, a classic arp with nothing held gets the ghost chord too, and (tide_ghost_poll) a HOLD ALL part without steps
 * latches it: the owner's pad and arp multis then play with no key down. ARP SEQ parts get it on every PLAY, as before */
static int tide_seq_ghost(const track_t *t)
{
    uint32_t m = (uint32_t)arp_mode(t);
    return song.playing && trk_index(t) < NSHOWN && m && (m == AM_SEQ_ID || (tide_gh & TIDE_GH_AUTO));
}
static int tide_arp_paused(const track_t *t)
{
    return !song.playing && !t->arp_phys && trk_index(t) < NSHOWN && arp_mode(t) == AM_SEQ_ID && !clock_src();
}
static uint32_t tide_seq_chord(const track_t *t, uint32_t *list, uint32_t n)
{
    uint32_t pi = trk_index(t), i;
    if (pi >= NSHOWN || !arp_mode(t))
        return n;
    if (n) {                                         /* keys held: remembered (4 at most, as arp_seq_notes plays) */
        for (i = 0; i < n && i < 4u; i++)
            tide_chord[pi][i] = (uint8_t)list[i];
        tide_chord_n[pi] = (uint8_t)i;
        return n;
    }
    if (!tide_seq_ghost(t))
        return 0;
    if (!tide_chord_n[pi]) {                         /* no chord yet: C4, as the part's CHRD builds it (one note with CHRD OFF) */
        uint8_t o[CHORD_MAX];
        int32_t r;
        uint16_t mk;
        n = chord_make(t, 60, o, &r, &mk);
        for (i = 0; i < n; i++)
            list[i] = o[i];
        return n;
    }
    for (i = 0; i < tide_chord_n[pi]; i++)
        list[i] = tide_chord[pi][i];
    return i;
}
/* PLAY: a stand-in arp starts with the sequencer, its first step on the first block */
static void tide_seq_ghost_start(void)
{
    uint32_t k;
    tide_gh &= ~0x0Cu;                                  /* (b7: a part played by hand is the ghost chord's again at PLAY) */
    for (k = 0; k < NSHOWN; k++)
        if (!trk[k].arp_phys && (arp_mode(&trk[k]) == AM_SEQ_ID || (arp_mode(&trk[k]) && !trk[k].nheld))) {   /* (b7: a classic arp with nothing latched too: AUTO TIDE's ghost chord) */
            trk[k].arp_pos = 0xFFFFFFF;
            trk[k].arp_idx = 0xFFFFFFFFu;
        }
}

/* b7 (MR2 part B, main loop: tide_poll): while AUTO TIDE plays, a part that would wait for keys (no arp, no steps: a HOLD ALL pad or a plain key part) with no note
 * sounding on it plays TIDE's ghost chord (the part's CHRD from C4, as the ARP SEQ stand-in is built): the notes are started as keys would start them (input_on), and on a
 * HOLD ALL part latched like a chord let go. A real key or MIDI note ends the ghost chord first (tide_ghost_real, from input_on: the old notes end before the new one starts,
 * so nothing doubles) and the part is the player's until the next PLAY; STOP, the end of AUTO TIDE, and a load or STOP ALL that took the voices end it here.
 * (A classic arp with nothing held gets the stand-in from tide_seq_ghost / tide_seq_chord, which also remember the player's last chord.) */

static int tide_gh_steps(const track_t *t)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    for (i = 0; i < len && i < NSTEP; i++)
        if (sqm_hit(&t->step[i]))
            return 1;
    return 0;
}
static uint32_t tide_gh_gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate;
    return n;
}
static void tide_gh_end(track_t *t, uint32_t p)          /* the part's gated voices are the ghost chord's (a real note ends it first, tide_ghost_real) */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate)
            trk_note_off(t, t->v[i].note);
    if (hold_all_keys(t))
        t->nheld = 0;
    tide_gh &= (uint8_t)~(1u << p);
}
static void tide_ghost_real(track_t *t)                  /* input_on: a real note is about to start on t */
{
    uint32_t p = trk_index(t);
    if (p >= NSHOWN || (tide_gh & TIDE_GH_IN))
        return;
    if (TIDE_GH_ON(p))
        tide_gh_end(t, p);
    tide_gh |= (uint8_t)(4u << p);
}
static void tide_ghost_stop(void)                        /* seq_stop: the transport stopped (main loop): the ghost chords end at once */
{
    uint32_t p;
    tide_gh &= (uint8_t)~TIDE_GH_AUTO;
    for (p = 0; p < NSHOWN; p++)
        if (TIDE_GH_ON(p))
            tide_gh_end(&trk[p], p);
}
static void tide_ghost_poll(void)
{
    uint32_t p, i, n, on = song.playing && glide_tide_on();
    tide_gh = (uint8_t)(on ? tide_gh | TIDE_GH_AUTO : tide_gh & ~TIDE_GH_AUTO);
    for (p = 0; p < NSHOWN; p++) {
        track_t *t = &trk[p];
        if (TIDE_GH_ON(p)) {
            if (!on || !tide_gh_gated(t))                /* STOP, AUTO TIDE ended, or a load took the voices */
                tide_gh_end(t, p);
            continue;
        }
        if (!on || TIDE_GH_HAND(p) || arp_mode(t) || t->nheld || t->arp_phys || tide_gh_steps(t) || tide_gh_gated(t))
            continue;
        {
            uint8_t o[CHORD_MAX];
            int32_t r;
            uint16_t mk;
            n = chord_make(t, 60, o, &r, &mk);
            if (n > 4u)
                n = 4u;
            tide_gh |= TIDE_GH_IN;
            for (i = 0; i < n; i++) {
                if (duell_mode == DM_LAYER)
                    lay_input(t, o[i], 100u);            /* (the part's XFADE share rides with the voice) */
                else
                    input_on(t, o[i], 100u);
            }
            tide_gh &= (uint8_t)~TIDE_GH_IN;
            if (hold_all_keys(t))
                t->arp_phys = 0;                         /* (the notes are down no more: a latched chord, which the next key replaces) */
            if (n)
                tide_gh |= (uint8_t)(1u << p);
        }
    }
}
/* b10 round 8 (tide_take.c): the sequencer enters a step of a part whose first take is being recorded (tide.h TIDE_REC_REPLACE_PASS; the audio path: one increment) */
static void tide_take_step(track_t *t)
{
    uint32_t p = trk_index(t);
    if (p < 2u && ((tk_first >> p) & 1u) && tk_adv[p] < 255u)
        tk_adv[p]++;
}
