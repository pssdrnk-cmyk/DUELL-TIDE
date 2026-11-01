/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's sequencer side (tide.h H1-H3), included at the end of seq.c: SEQ MORPH (seq_morph.c) with the phrase lifecycle
 * (seq_rhythm.c, seq_phrase.c), the scale-degree walk (seq_evolve.c), voice leading (seq_voicelead.c) and the peer
 * prediction (seq_dialogue.c), from the JAM fork (branch jam, 71654ba). 1.1 has no held-note continuations (the fork's
 * step_t.cont): a hold is a tie here, so no step is a continuation of the one before.
 * Contexts: tide_play_step in the audio path; everything else in the main loop. */
static uint32_t step_cont(const step_t *s) { (void)s; return 0u; }   /* (no per-note continuations in 1.1: ties instead) */
static const step_t *seq_play_step(const track_t *t, uint32_t idx, step_t *buf);   /* (seq_dialogue.c reads it before seq_morph.c has it) */
#include "seq_morph.c"

static const step_t *tide_play_step(const track_t *t, uint32_t idx, step_t *buf)
{
    return seq_play_step(t, idx, buf);
}
