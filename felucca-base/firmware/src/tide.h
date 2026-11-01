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
/* H9 (morph.c's MBV block, main loop): End B's MORPH TO target on FM6 saved with patches, multis and the project (morph_tide.c) */
#define TIDE_MBV_FMT_N 3u
#define TIDE_MBV_FLAGS MBV_FMT
#define TIDE_MBV_SIZE(b, avail, len) tide_mbv_size((b), (avail), (len))
#define TIDE_MBV_MAKE(pi, out, n) tide_mbv_make((pi), (out), (n))
#define TIDE_MBV_LOAD(pi, b, len) tide_mbv_load((pi), (b), (len))
/* H10-H17 (the pages, the knobs, MIDI, the storage; main loop): tide_ui.c (end of ui_input.c), tide_store.c (felucca.c, after
 * upreset.c and project.c). Prototypes for the DUELL files that call them before they are compiled */
static void graph_tide(void);
static void graph_tidelist(void);
static void tide_columns(void);
static uint32_t tide_sig(void);
static uint32_t tide_list_sig(void);
static void tide_knob(uint32_t slot, int32_t s);
static int tide_reset(uint32_t k);
static void tide_link(uint32_t mask, uint32_t v);
static void tide_midi(uint32_t kind, uint32_t mask, uint32_t v, uint32_t pi);
static int tide_list_on(void);
static void tide_list_oct(uint32_t oct);
static int tide_store_load(uint8_t *b, uint32_t n);
static void tide_store_mark(const uint8_t *b, uint32_t n);
static void tide_store_poll(void);
static const char *tide_tl_keep(track_t *t, uint64_t was, uint32_t len);   /* (tide_ui.c) */
static void tl_undo_take(track_t *t);                    /* (ui_tools.c, compiled after tide_ui.c) */
static void tl_unlock(track_t *t, uint64_t was, uint32_t len);
#define TIDE_TL_KEEP , TL_KEEP                           /* SEQ held + G4; labelled below the tools grid */
#define TIDE_SAVE_ERROR(rc) ((rc) == PT_FULL ? "STORE FULL" : "SAVE ERROR")
#define TIDE_THEME_F(f) ((f) >= 120u ? 127u : (f) & ~7u)
static int tide_foot_parts(void);
static void tide_part_name(uint32_t pi, char *b);
static void tide_roll_ghosts(const track_t *t, uint32_t base, uint32_t ncol);
static uint32_t tide_roll_sig(const track_t *t);
#define TIDE_FOOT_NAME(t, b) do { if (tide_foot_parts()) tide_part_name(trk_index(t), (b)); else sound_label((t), (b)); } while (0)
#define TIDE_TL_DO(t, p, was, len) do { if ((p) == TL_KEEP) return tide_tl_keep((t), (was), (len)); } while (0)
#define OBJ_TIDE (OBJ_PATCHX0 + 15u)                     /* TIDE's settings: patch chunk 21's object (storage.c, tide_store.c) */
#define TIDE_PT_NCH 21u                                  /* the patch store's chunks with TIDE: 126 saved patches at most (plan §4.5) */
#define TIDE_MG_N 5u                                     /* ui.mg's gestures: MORPH's, GLO's and GLIDE's (tide_ui.c TIDE_G_GLIDE) */
#define TIDE_PAGE(pg) ((pg)->scope == SC_TIDE)
#define TIDE_LINK(kind, mask, v) do { if ((kind) == MQ_FADER) tide_link((mask), (v)); } while (0)   /* LINK: the SEQ fader follows */
#define TIDE_MQ_ROUTE(kind, mask, v, pi) ((kind) >= MQ_GLIDE ? (tide_midi((kind), (mask), (v), (pi)), 1) : 0)   /* GLIDE, the SEQ fader */
#define TIDE_MQ_KINDS , MQ_GLIDE, MQ_SEQF
#define TIDE_MQ_MERGE(kind) || (kind) == MQ_SEQF
#define TIDE_KNOB(pg, slot, steps) (TIDE_PAGE(pg) ? (tide_knob((slot), (steps)), 1) : 0)
#define TIDE_RESET_CASE case SC_TIDE: return tide_reset(k);
#define TIDE_LIST_ON() tide_list_on()
#define TIDE_LIST_OCT(oct) tide_list_oct(oct)
#define TIDE_GRAPH_CASES case GR_TIDE: cv_oy = 0; graph_tide(); break; case GR_TIDELIST: cv_oy = 0; graph_tidelist(); break;
#define TIDE_GRAPH_SIG(pg, h) do { if ((pg)->graph == GR_TIDELIST) return ((h) ^ tide_list_sig()) * 16777619u; \
                                   if (TIDE_PAGE(pg)) (h) = ((h) ^ tide_sig() ^ (uint32_t)morph.target * 0x85EBCA77u) * 16777619u; } while (0)
#define TIDE_COLUMNS() do { if (TIDE_PAGE(cur_page())) { tide_columns(); return; } } while (0)
#define TIDE_HOME_NAME "MORPH"                           /* MENU > CONTROL > HOME OPENS: the first page of the round (MORPH, as stock; TIDE is the round's 4th) */

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
#define TIDE_MBV_FMT_N 0u
#define TIDE_MBV_FLAGS 0u
#define TIDE_MBV_SIZE(b, avail, len) (len)
#define TIDE_MBV_MAKE(pi, out, n) (n)
#define TIDE_MBV_LOAD(pi, b, len) ((void)0)
#define TIDE_TL_KEEP
#define TIDE_SAVE_ERROR(rc) "SAVE ERROR"
#define TIDE_THEME_F(f) (f)
#define TIDE_FOOT_NAME(t, b) sound_label((t), (b))
#define TIDE_TL_DO(t, p, was, len) ((void)0)
#define TIDE_PT_NCH 22u
#define TIDE_MG_N 4u
#define TIDE_PAGE(pg) 0
#define TIDE_LINK(kind, mask, v) ((void)0)
#define TIDE_MQ_ROUTE(kind, mask, v, pi) 0
#define TIDE_MQ_KINDS
#define TIDE_MQ_MERGE(kind)
#define TIDE_KNOB(pg, slot, steps) 0
#define TIDE_RESET_CASE
#define TIDE_LIST_ON() 0
#define TIDE_LIST_OCT(oct) ((void)0)
#define TIDE_GRAPH_CASES
#define TIDE_GRAPH_SIG(pg, h) ((void)0)
#define TIDE_COLUMNS() ((void)0)
#define TIDE_HOME_NAME "MORPH"

#endif
#endif
