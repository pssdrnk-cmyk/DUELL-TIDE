/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's sequencer side (tide.h H1-H3), included at the end of seq.c: SEQ MORPH (seq_morph.c) with the phrase lifecycle
 * (seq_rhythm.c, seq_phrase.c), the scale-degree walk (seq_evolve.c), voice leading (seq_voicelead.c) and the peer
 * prediction (seq_dialogue.c), from the JAM fork (branch jam, 71654ba). 1.1 has no held-note continuations (the fork's
 * step_t.cont): a hold is a tie here, so no step is a continuation of the one before.
 * Contexts: tide_play_step in the audio path; everything else in the main loop. */
static uint32_t step_cont(const step_t *s) { (void)s; return 0u; }   /* (no per-note continuations in 1.1: ties instead) */
static const step_t *seq_play_step(const track_t *t, uint32_t idx, step_t *buf);   /* (seq_dialogue.c reads it before seq_morph.c has it) */
#include "seq_morph.c"
#include "seq_poly.c"
#include "seq_answer.c"
#include "seq_arc.c"

static const step_t *tide_play_step(const track_t *t, uint32_t idx, step_t *buf)
{
    return seq_play_step(t, idx, buf);
}

/* ARP SEQ on PLAY with no key held (owner, 2026-11-01: it was silent; the steps are offsets on held keys): the arp plays from a
 * stand-in, the part's last chord held on the SEQ arp, else C4, the steps as written. Not while recording (it would record its
 * own pattern), nor stopped. A key held plays as ever */
static uint8_t tide_chord[NSHOWN][4], tide_chord_n[NSHOWN];
static int tide_seq_ghost(const track_t *t)
{
    return song.playing && !song.rec && trk_index(t) < NSHOWN && arp_mode(t) == AM_SEQ_ID;
}
static uint32_t tide_seq_chord(const track_t *t, uint32_t *list, uint32_t n)
{
    uint32_t pi = trk_index(t), i;
    if (pi >= NSHOWN || arp_mode(t) != AM_SEQ_ID)
        return n;
    if (n) {                                         /* keys held: remembered (4 at most, as arp_seq_notes plays) */
        for (i = 0; i < n && i < 4u; i++)
            tide_chord[pi][i] = (uint8_t)list[i];
        tide_chord_n[pi] = (uint8_t)i;
        return n;
    }
    if (!tide_seq_ghost(t))
        return 0;
    if (!tide_chord_n[pi]) {
        list[0] = 60;
        return 1;
    }
    for (i = 0; i < tide_chord_n[pi]; i++)
        list[i] = tide_chord[pi][i];
    return i;
}
/* PLAY: a stand-in arp starts with the sequencer, its first step on the first block */
static void tide_seq_ghost_start(void)
{
    uint32_t k;
    for (k = 0; k < NSHOWN; k++)
        if (!trk[k].nheld && arp_mode(&trk[k]) == AM_SEQ_ID) {
            trk[k].arp_pos = 0xFFFFFFF;
            trk[k].arp_idx = 0xFFFFFFFFu;
        }
}
