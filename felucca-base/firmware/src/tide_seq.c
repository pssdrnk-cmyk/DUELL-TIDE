/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's sequencer side (tide.h H1-H3), included at the end of seq.c. Step 2 of the migration: the hooks only, each
 * doing what DUELL does without TIDE. SEQ MORPH (seq_morph.c and its parts) comes in slice S3.
 * Contexts: tide_play_step and tide_beat_block run in the audio path; tide_seq_start where seq_start runs. */
static const step_t *tide_play_step(const track_t *t, uint32_t idx, step_t *buf)
{
    (void)buf;
    return &seq_steps(t)[idx];
}

static void tide_seq_start(void)
{
}

static void tide_beat_block(uint32_t beat_adv)
{
    (void)beat_adv;
}
