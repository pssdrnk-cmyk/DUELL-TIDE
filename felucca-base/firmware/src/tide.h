/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE: the plug between DUELL and TIDE, the generative auto-morph (the fork's own code; JAM until 2026-11-01) (docs/design/MIGRATION_1_1_PLAN.md on branch jam, §3).
 * Every place where a DUELL file calls into TIDE is a hook below, marked "TIDE hook" at its call site, so the next
 * upstream merge finds them all with one grep. DUELL_TIDE=0 (env DUELL_TIDE=0 for tools/build.py) builds plain DUELL:
 * each hook expands to exactly the code that stood there before, and no TIDE file is compiled. That build must equal
 * the fixes-only baseline byte for byte. Included once, at the end of core.h. */
#ifndef TIDE_H
#define TIDE_H

#ifndef DUELL_TIDE
#define DUELL_TIDE 1
#endif

#if DUELL_TIDE

/* H2 (seq.c, audio path): a step as it plays, the Original's or the SEQ MORPH's variation (tide_seq.c). The
 * compound literal is the variation's room, alive for the enclosing block. The editor and pages read the Original
 * (seq_steps), never this */
static const step_t *tide_play_step(const track_t *t, uint32_t idx, step_t *buf);
#define TIDE_PLAY_STEP(t, idx) tide_play_step((t), (idx), &(step_t){.n = 0})
/* H3 (seq.c): the transport starts (seq_start), and the block's beat advance (events_block, audio path) */
static void tide_seq_start(void);
static void tide_beat_block(uint32_t beat_adv);
#define TIDE_SEQ_START() tide_seq_start()
#define TIDE_BEAT_BLOCK(adv) tide_beat_block(adv)
/* H13 (ui_input.c, main loop): once a frame, after morph_poll: GLIDE, the SEQ MORPH's ends, the settings */
static void tide_poll(void);
#define TIDE_POLL() tide_poll()

#else

#define TIDE_PLAY_STEP(t, idx) (&seq_steps(t)[(idx)])
#define TIDE_SEQ_START() ((void)0)
#define TIDE_BEAT_BLOCK(adv) ((void)(adv))
#define TIDE_POLL() ((void)0)

#endif
#endif
