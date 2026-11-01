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
/* H3 (seq.c, audio path): VARY BARS' bar count since PLAY and the bar turning in this block; the patterns' completed loops (the
 * phrase lifecycle, seq_rhythm.c). Counted here, read by tide_seq.c and the pages */
static volatile uint8_t sqm_bar, sqm_bar_turn;
static volatile uint8_t tide_seq_loops[NTRK];
#define TIDE_SEQ_START() (sqm_bar = sqm_bar_turn = 0)     /* (the phrase from the top; MIDI CONTINUE keeps it) */
#define TIDE_BEAT_BLOCK(adv) (sqm_bar_turn = (uint8_t)(beat_n == 3u && beat_pos + (adv) >= beat_samples()))
#define TIDE_BEAT() do { if (!beat_n && song.playing) sqm_bar++; } while (0)
#define TIDE_SEQ_LOOP(t, len) do { if ((t)->seq_idx + 1u == (len) && (t)->seq_pos < 0x7FFFFFFFu) tide_seq_loops[trk_index(t)]++; } while (0)   /* (the startup sentinel is no loop) */
/* H13 (ui_input.c, main loop): once a frame, after morph_poll: GLIDE, the SEQ MORPH's ends, the settings */
static void tide_poll(void);
#define TIDE_POLL() tide_poll()
/* H4-H8 (morph.c, main loop unless marked): morph_music.c, morph_fm_shape.c, morph_complement.c and morph_tide.c are included
 * by morph.c; these are the lines morph.c runs for them */
#define TIDE_PART_FIELDS uint8_t tok[2][MORPH_HIST];    /* MORPH TO: the factory sound each history entry is (index + 1), 0 not one */
#define TIDE_CFG_BITS() (morph_musical && !(morph.cfg & MC_WILD) ? MC_MUSIC : 0u)   /* the next roll MUSICAL (TIDE LIST's ROLL) */
#define TIDE_BLK_CFG_OK(c) (!((c) & ~(MC_SET | MC_STEREO | MC_PART | MC_MUSIC)))   /* a saved roll's cfg byte: MUSICAL too */
#define TIDE_ROLL(mp, pi, x, end, fend, seed, amount, cfg, ndep) tide_roll((mp), (pi), (x), (end), (fend), (seed), (amount), (cfg), (ndep))
#define TIDE_TOK_SET(mp, x, k, v) ((mp)->tok[(x)][(k)] = (uint8_t)(v))
#define TIDE_TOK_COPY(mp, x, k, t) ((mp)->tok[(x)][(k)] = (mp)->tok[(x)][(t)])
#define TIDE_REBASE_END(pi, mp, e) do { (mp)->tok[(e)][0] = 0; memset(mm_dv[(pi) % NSHOWN][(e)], 0, sizeof mm_dv[0][0]); \
                                       memset(mm_dir[(pi) % NSHOWN], 0, sizeof mm_dir[0]); } while (0)   /* (WANDER from this sound's own dials) */
#define TIDE_FACTORY_AMOUNT(a) (morph_musical ? MM_AMOUNT : (a))   /* a factory sound's B: MUSICAL at MUSICAL's AMOUNT (no DS_ENDB) */
#define TIDE_FACTORY_CFG() (morph_musical ? MC_MUSIC : 0u)
#define TIDE_BUILD_KIND(mp, id, kind) do { if ((id) == P_TRIM && ((mp)->tok[0][(mp)->top[0]] || (mp)->tok[1][(mp)->top[1]])) \
                                              (kind) = MK_CONT; } while (0)   /* (MORPH TO: two sounds' loudness blends, no step halfway) */
#define TIDE_BUILD_FNID(mp) ((mp)->fnid |= (uint8_t)((mp)->tok[0][(mp)->top[0]] | (mp)->tok[1][(mp)->top[1]]))   /* (MORPH TO's FM6 flip) */
#define TIDE_FM6_TARGET(mp, tr, p, w) tide_fm6_target((mp), (tr), (p), (w))   /* (audio ISR) */
#define TIDE_UNDO(pi, x) morph_fmto_sync((pi), (x))
#define TIDE_MORPH_PROTOS static void tide_fm6_target(const morph_part_t *mp, uint32_t tr, uint8_t *p, int32_t w);

#else

#define TIDE_PLAY_STEP(t, idx) (&seq_steps(t)[(idx)])
#define TIDE_SEQ_START() ((void)0)
#define TIDE_BEAT_BLOCK(adv) ((void)(adv))
#define TIDE_BEAT() ((void)0)
#define TIDE_SEQ_LOOP(t, len) ((void)0)
#define TIDE_POLL() ((void)0)
#define TIDE_PART_FIELDS
#define TIDE_CFG_BITS() 0u
#define TIDE_BLK_CFG_OK(c) ((c) < 32u)
#define TIDE_ROLL(mp, pi, x, end, fend, seed, amount, cfg, ndep) 0
#define TIDE_TOK_SET(mp, x, k, v) ((void)0)
#define TIDE_TOK_COPY(mp, x, k, t) ((void)0)
#define TIDE_REBASE_END(pi, mp, e) ((void)0)
#define TIDE_FACTORY_AMOUNT(a) (a)
#define TIDE_FACTORY_CFG() 0u
#define TIDE_BUILD_KIND(mp, id, kind) ((void)0)
#define TIDE_BUILD_FNID(mp) ((void)0)
#define TIDE_FM6_TARGET(mp, tr, p, w) ((void)0)
#define TIDE_UNDO(pi, x) ((void)0)
#define TIDE_MORPH_PROTOS

#endif
#endif
