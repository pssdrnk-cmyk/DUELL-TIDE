/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE: the plug between DUELL and TIDE, the generative auto-morph (the fork's own code; JAM until 2026-11-01) (docs/design/MIGRATION_1_1_PLAN.md on branch jam, §3).
 * Every place where a DUELL file calls into TIDE is a hook below, marked "TIDE hook" at its call site, so the next
 * upstream merge finds them all with one grep. (Until b10 a build with DUELL_TIDE=0 compiled plain DUELL, checked by jam/check_off.sh; that build is retired:
 * TIDE is the only firmware, and the hooks are plain code.) Included once, at the end of core.h. */
#ifndef TIDE_H
#define TIDE_H


#define UP_NUSER 4u
#define PS_NUSER 4u
#define UP_EMPTY_USER(k) sl_empty_user(k)           /* U1 (upreset.c): slot k is an empty USER voice slot */
#define UP_PTR_EMPTY(k) (UP_SLOTS + 1u + (k) - UP_USER0)   /* t->user of a part pointed at the empty USER slot k (SAVE offers it): past UP_SLOTS, so it is no record */
#define UP_REC_USER(t) ((t)->user && (t)->user <= UP_SLOTS)   /* the part plays a stored record (a pointer at an empty USER slot still plays its factory sound); plain arithmetic: every TU that includes morph.c has it */

/* H2 (seq.c, audio path): a step as it plays, the Original's or the SEQ MORPH's variation (tide_seq.c). The
 * compound literal is the variation's room, alive for the enclosing block. The editor and pages read the Original
 * (seq_steps), never this */
static const step_t *tide_play_step(const track_t *t, uint32_t idx, step_t *buf);
#define TIDE_PLAY_STEP(t, idx) tide_play_step((t), (idx), &(step_t){.n = 0})
/* H2b (seq.c, audio path): ARP SEQ with no key held plays on PLAY from a stand-in, the part's last chord or C4 (the steps as written) */
static int tide_seq_ghost(const track_t *t);
static uint32_t tide_seq_chord(const track_t *t, uint32_t *list, uint32_t n);
#define TIDE_SEQ_GHOST(t) tide_seq_ghost(t)
/* H2c (seq.c arp_step): a SEQ arp is the part's sequencer, so a plain STOP stops it too, also on a chord HOLD keeps (owner, 2026-11-04: "sound is even playing
 * when the stop button is pressed"): no step while the transport is stopped and no key is down; the latched chord stays for PLAY (STOP ALL ends it) */
static int tide_arp_paused(const track_t *t);
#define TIDE_ARP_PAUSED(t) tide_arp_paused(t)
static uint32_t tide_seq_voice(const track_t *t, uint32_t *list, uint32_t n);   /* (seq_voicelead.c: the chord bed's ARC voicing on the arp's chord) */
#define TIDE_SEQ_CHORD(t, list, n) tide_seq_voice((t), (list), tide_seq_chord((t), (list), (n)))
/* H3 (seq.c, audio path): VARY BARS' bar count since PLAY and the bar turning in this block; the patterns' completed loops (the
 * phrase lifecycle, seq_rhythm.c). Counted here, read by tide_seq.c and the pages */
static volatile uint8_t sqm_bar, sqm_bar_turn, sqm_bar_soon;   /* (soon: the bar's last 32nd, the bar an ARP SEQ step plays in: its clock runs a little ahead of the beat's; MR4) */
static volatile uint8_t tide_seq_loops[NTRK];
/* H3c (perform.c perf_filter, audio path): ARC's master filter (seq_arc.c tarc_flt_of, main loop), K3 FILTER's value while the player's K3 is at rest: 0 none */
static volatile int8_t tarc_flt;
/* H3b (seq.c, audio path): POLY (seq_poly.c): the loop's length now, the step after seq_idx, the arp step k, an arp step started */
static void tide_poly_start(void);
static uint32_t tide_slen(const track_t *t);
static uint32_t tide_seq_next(track_t *t);
static int tide_seq_wrapping(const track_t *t, uint32_t len);
static uint32_t tide_arp_idx(const track_t *t, uint32_t k, uint32_t len);
static void tide_arp_tick(track_t *t);
static int tide_poly_on(const track_t *t);
static uint32_t tide_play_idx(const track_t *t);
static uint32_t tide_play_len(const track_t *t);
static uint32_t tide_swing_par(const track_t *t);
#define TIDE_SWING_PAR(t) tide_swing_par(t)   /* (review FR1: in a stretch a step swings by the full loop's step parity) */
#define TIDE_PLAY_IDX(t) tide_play_idx(t)   /* (the pages: the step playing now, the loop's length it plays) */
#define TIDE_PLAY_LEN(t) tide_play_len(t)
#define TIDE_SLEN(t) tide_slen(t)
#define TIDE_SEQ_NEXT(t, len) tide_seq_next(t)
#define TIDE_SEQ_RELEN(t, len) ((len) = tide_slen(t))   /* (a stretch began or ended at this step) */
#define TIDE_ARP_IDX(t, k, len) tide_arp_idx((t), (k), (len))
#define TIDE_ARP_TICK(t) tide_arp_tick(t)
#define TIDE_SEQ_START() (sqm_bar = sqm_bar_turn = sqm_bar_soon = 0, tide_poly_start(), tide_seq_ghost_start())     /* (the phrase from the top; MIDI CONTINUE keeps it) */
static void tide_seq_ghost_start(void);
#define TIDE_BEAT_BLOCK(adv) (sqm_bar_turn = (uint8_t)(beat_n == 3u && beat_pos + (adv) >= beat_samples()), sqm_bar_soon = (uint8_t)(beat_n == 3u && beat_pos + (adv) + (beat_samples() >> 3) >= beat_samples()))
#define TIDE_BEAT() do { if (!beat_n && song.playing) sqm_bar++; } while (0)
#define TIDE_SEQ_LOOP(t, len) do { if (tide_seq_wrapping((t), (len)) && (t)->seq_pos < 0x7FFFFFFFu) tide_seq_loops[trk_index(t)]++; } while (0)   /* (the startup sentinel is no loop) */
/* H3c (seq.c, audio path): the chance rolls the TIDE page shows (tide_seq.c): a step's (seq_play) and the SEQ arp's PROB (arp_fire); each returns the miss / hit it is given */
static int tide_chance_roll(const track_t *t, uint32_t idx, uint32_t chance, int miss);
static int tide_aroll(const track_t *t, int hit);
#define TIDE_CHANCE(t, idx, s, miss) tide_chance_roll((t), (idx), step_chance(s) < 100u, (miss))
#define TIDE_APROB(t, hit) tide_aroll((t), (hit))
static uint32_t tide_arp_out(const track_t *t, uint8_t *out, uint32_t m, uint32_t *vel);
#define TIDE_ARP_OUT(t, out, m, vel) tide_arp_out((t), (out), (m), (vel))   /* (seq.c arp_seq_notes: TIDE's dynamics on an ARP SEQ step, D1; a thinned chord's two voices, MR4) */
/* H20 (seq.c): live recording into an ARP SEQ part (audit liverec L2, R2) and the MIDI hooks of review FR8. TIDE_LIVEREC: the sections of seq.c only TIDE has (rec_put, arp_dur, rec_arp_note ..); a hook that returns, returns from the function it stands in */
#define TIDE_LIVEREC 1
/* b10 round 8 (tide_take.c, work/b10/specs/PERF_PHASE1.md): TAKE + ANSWER. tk_fresh_v: the newest edit is a take or a roll (ui.c clears it when a load or a lock edit takes the undo copy); tk_said: a roll
 * wrote its own header message (ui_input.c morph_act then leaves "ROLLED A" out) */
static uint8_t tk_fresh_v __attribute__((section(".pool"))), tk_reset_v __attribute__((section(".pool"))), tk_roll_mask __attribute__((section(".pool")));   /* (the pool: zero at power-on, and the RAM rule's .bss line has no room)
 */
static uint16_t tk_roll_seed __attribute__((section(".pool")));   /* (tk_reset_v: a multi / project loaded: the history of takes is of another music: tide_take.c take_poll drops it) */
static uint32_t project_key(void);                   /* (multi.c) */
static uint32_t take_undo(int32_t dir);
#define TIDE_TAKE_STALE() (tk_fresh_v = 0)
#define TIDE_TAKE_UNDO(s) take_undo((s) < 0 ? -1 : 1)
#define TIDE_TAKE_ROLL(mask, seed) (tk_roll_mask |= (uint8_t)(mask), tk_roll_seed = (uint16_t)(seed))   /* (morph_do, any context: take_poll writes the answer again in the main loop) */
#define TIDE_REC_VARS static volatile uint8_t tide_rec_replace; static uint32_t rec_key[NSHOWN][4];   /* (tide_rec_replace: TIDE SETTINGS REC, 0 ADD, 1 REPLACE; rec_key: the keys of an armed ARP SEQ part that went straight to the voices) */
static int arp_rec_fire(track_t *t);
#define TIDE_REC_ARP_ON(t) (rec_on(t) && (t)->p[P_AMODE] != AM_SEQ_ID)   /* (the arp records what it plays, except on an ARP SEQ part) */
#define TIDE_ARP_SEQ_PRE(t) do { \
        if ((t)->p[P_AMODE] == AM_SEQ && !(mods_off & MODS_SEQ))   /* b7 (review B2): the step's locks and automation on the step the arp plays (seq_play leaves them) */ \
            motion_step((t), TIDE_ARP_IDX((t), (t)->arp_idx, seq_len(t)), &motion, 1u); \
        if ((t)->p[P_AMODE] == AM_SEQ && arp_rec_fire(t)) \
            return;                                     /* live recording put a key into this step already: it sounds, the step does not play again */ \
    } while (0)
#define TIDE_REC_REPLACE_PASS(t, idx, len, k) do { \
        (k) = ((idx) + 2u * (len) - 2u) % (len);        /* REPLACE's pass: the playhead enters step idx, the mark of the step two behind it goes (rec_put) */ \
        (t)->rec_wr[((k) >> 5) & 1u] &= ~(1u << ((k) & 31u)); \
    } while (0)
#define TIDE_REC_ARP_RELEASE(t) do { \
        if (arp_mode(t) == AM_SEQ_ID) {                 /* an ARP SEQ part's hold runs on the arp's steps (rec_arp_note) */ \
            if ((t)->rh_ties && (t)->arp_pos < 0x8000000u && TIDE_ARP_IDX((t), (t)->arp_idx, seq_len(t)) == (t)->rh_last && \
                before_half((t)->arp_pos, arp_dur(t))) \
                (t)->step[(t)->rh_last] = (t)->rh_bak; \
            return; \
        } \
    } while (0)
#define TIDE_REC_INPUT_ON(t, note, vel) do { \
        tide_ghost_real(t);                             /* b7 (MULTIS): a real note ends the AUTO TIDE ghost chord on this part before it starts */ \
        if (rec_arp_keys(t)) {                          /* R2: the key sounds as played and goes into the pattern the part plays (the stand-in runs on) */ \
            rec_key_set((t), (note), 1); \
            trk_note_on((t), (note), (vel)); \
            rec_arp_note((t), (note), (vel)); \
            return; \
        } \
        rec_key_set((t), (note), 0); \
    } while (0)
#define TIDE_REC_INPUT_OFF(t, note) do { \
        if (rec_key_set((t), (note), 0)) {              /* (it never reached the arp: no arp_remove) */ \
            trk_note_off((t), (note)); \
            return; \
        } \
    } while (0)
#define TIDE_MIDI_NOTE_HELD(t, x) midi_note_held((t), (x))
#define TIDE_REC_ARP_KEYS(t) rec_arp_keys(t)
#define TIDE_REC_RESET(t, i) do { \
        (t)->rec_wr[0] = (t)->rec_wr[1] = (t)->arp_skip = 0; \
        if ((i) < NSHOWN) \
            memset(rec_key[i], 0, sizeof rec_key[i]); \
    } while (0)
#define TIDE_STEP_MOTION(t) ((t)->p[P_AMODE] != AM_SEQ)      /* (b7, review B2: an ARP SEQ part's locks play on the arp's steps) */
#define TIDE_REC_HOLD_ON(t) (arp_mode(t) != AM_SEQ_ID)       /* (an ARP SEQ part's holds and marks run on its arp steps) */
#define TIDE_REC_KEYS_CLEAR() memset(rec_key, 0, sizeof rec_key)
#define TIDE_MIDI_SEL_POLL() do { if (song.sel != midi_sel_was) midi_sel_change(); } while (0)
#define TIDE_FORGET_MIDI(keep) (!(keep))
/* H13 (ui_input.c, main loop): once a frame, after morph_poll: GLIDE, the SEQ MORPH's ends, the settings */
static void tide_poll(void);
static void tide_ghost_poll(void);   /* (tide_seq.c, b7: the ghost chord of a part that waits for keys under AUTO TIDE; called by tide_poll) */
static void tide_ghost_stop(void);   /* (seq.c seq_stop: the ghost chords end) */
static void tide_ghost_real(track_t *t);   /* (seq.c input_on: a real note starts on t: the ghost chord ends first) */
#define TIDE_POLL() tide_poll()
#define TIDE_GHOST_STOP() tide_ghost_stop()
/* H4-H8 (morph.c, main loop unless marked): morph_music.c, morph_fm_shape.c, morph_complement.c and morph_tide.c are included
 * by morph.c; these are the lines morph.c runs for them */
#define TIDE_PART_FIELDS uint8_t tok[2][MORPH_HIST];    /* MORPH TO: the factory sound each end is (index + 1), 0 not one */
#define TIDE_CFG_BITS() (MM_ON() && (MM_LOCK || !(morph.cfg & MC_WILD)) ? MC_MUSIC : 0u)   /* the next roll MUSICAL (TIDE LIST's ROLL) */
#define TIDE_BLK_CFG_OK(c) (!((c) & ~(MC_SET | MC_STEREO | MC_PART | MC_MUSIC)))   /* a saved roll's cfg byte: MUSICAL too */
#define TIDE_ROLL(mp, pi, x, end, fend, seed, amount, cfg, ndep) tide_roll((mp), (pi), (x), (end), (fend), (seed), (amount), (cfg), (ndep))
#define TIDE_TOK_SET(mp, x, k, v) ((mp)->tok[(x)][(k)] = (uint8_t)(v))
#define TIDE_TOK_COPY(mp, x, k, t) ((mp)->tok[(x)][(k)] = (mp)->tok[(x)][(t)])
#define TIDE_REBASE_END(pi, mp, e) do { (mp)->tok[(e)][0] = 0; memset(mm_dv[(pi) % NSHOWN][(e)], 0, sizeof mm_dv[0][0]); \
                                       memset(mm_dir[(pi) % NSHOWN], 0, sizeof mm_dir[0]); mm_loads[(pi) % NSHOWN] += !(e); } while (0)   /* (WANDER from this sound's own dials) */
#define TIDE_FACTORY_AMOUNT(a) (MM_ON() ? MM_AMOUNT : (a))   /* a factory sound's B: MUSICAL at MUSICAL's AMOUNT (no DS_ENDB) */
/* the AMOUNT the load rolls factory sound (e, k)'s B at: TIDE_FACTORY_AMOUNT, or the sound's own MUSICAL AMOUNT 'tamt' (b6, sound audit T2:
 * a sound whose level rides on its decay, FRETLESS ISLAND STEEL MALLET WOOD, at 20, as SHAPE at 40 moved its level out of the End B
 * window). The part's AMOUNT knob stays TIDE_FACTORY_AMOUNT (the next ROLL's) */
#define TIDE_FACTORY_BAMT(e, k, a) (MM_ON() && ds_get((e), (k))->tamt ? (uint32_t)ds_get((e), (k))->tamt : TIDE_FACTORY_AMOUNT(a))
#define TIDE_FACTORY_CFG() (MM_ON() ? MC_MUSIC : 0u)
#define TIDE_BUILD_KIND(mp, id, kind) do { if ((id) == P_TRIM && ((mp)->tok[0][(mp)->top[0]] || (mp)->tok[1][(mp)->top[1]])) \
                                              (kind) = MK_CONT; } while (0)   /* (MORPH TO: two sounds' loudness blends, no step halfway) */
#define TIDE_BUILD_FNID(mp) ((mp)->fnid |= (uint8_t)((mp)->tok[0][(mp)->top[0]] | (mp)->tok[1][(mp)->top[1]]))   /* (MORPH TO's FM6 flip) */
#define TIDE_FM6_TARGET(mp, tr, p, w) tide_fm6_target((mp), (tr), (p), (w))   /* (audio ISR) */
/* ARP ON (WILD): a roll never turns the arp into SEQ, nor a SEQ arp into another mode. SEQ is not an arp's flavour: it decides whether
 * PLAY sounds the part at all (seq.c: the steps only as offsets on held keys), so a roll there silenced a TIDE part (owner, 2026-11-01) */
#define TIDE_AMODE_KEEP(v) ((v) == AM_SEQ_ID)
#define TIDE_AMODE_END(n) AM_SEQ_ID
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
static void tide_water(int whole);                       /* (tide_ui.c: AUTO TIDE's water rolling, each frame) */
static uint16_t aw_under(int32_t y);                     /* (tide_ui.c: the water's tone at row y of the lane being drawn, or the ground: under a step's corners, ui_graph.c ns_rr) */
static uint8_t aw_wet;                                   /* (the steps being drawn stand in AUTO TIDE's water) */
static void tide_columns(void);
static uint32_t tide_sig(void);
static uint32_t tide_list_sig(void);
static void tide_knob(uint32_t slot, int32_t s);
static int tide_reset(uint32_t k);
static void tide_link(uint32_t mask, uint32_t v);
static void tide_midi(uint32_t kind, uint32_t mask, uint32_t v, uint32_t pi);
static int tide_list_on(void);
static void tide_pick_mode(void);
static void tide_list_oct(uint32_t oct);
static int tide_store_load(uint8_t *b, uint32_t n);
static uint32_t tide_store_load_min(uint8_t *b, uint32_t n, uint32_t min);
static void tide_store_mark(const uint8_t *b, uint32_t n);
static void tide_store_poll(void);
static uint8_t tide_tail_new;                            /* settings_import (settings_persist.c) brought a TIDE record: tide_settings reads the fields again */
static const char *tide_tl_keep(track_t *t, uint64_t was, uint32_t len);   /* (tide_ui.c) */
static void tide_save_note(int32_t w);                      /* (tide_ui.c) */
#define TIDE_SAVE_NOTE(w) tide_save_note(w)               /* the SAVE dialog says what it saves when the sound is not the loaded one (1.0.0-b8) */
static void tl_undo_take(track_t *t);                    /* (ui_tools.c, compiled after tide_ui.c) */
static void tl_unlock(track_t *t, uint64_t was, uint32_t len);
#define TIDE_TL_KEEP , TL_KEEP                           /* SEQ held + G4; labelled below the tools grid */
#define TIDE_SAVE_ERROR(rc) ((rc) == PT_FULL ? "STORE FULL" : "SAVE ERROR")
#define TIDE_THEME_F(f) ((f) >= 120u ? 127u : (f) & ~7u)
static int tide_foot_parts(void);
static void tide_part_name(uint32_t pi, char *b);
/* H18 (project.c restore, ui.c pat_load): a multi, project or pattern was loaded into the parts of mask; the tide starts as written (D6) */
static void tide_loaded(uint32_t mask);                  /* (seq_arc.c) */
#define TIDE_LOADED(mask) tide_loaded(mask)
static int32_t tarc_bright(void);                        /* ARC's brightness lean for the next sound (seq_arc.c) */
static void tide_roll_ghosts(const track_t *t, uint32_t base, uint32_t ncol);
static uint32_t tide_roll_sig(const track_t *t);
#define TIDE_FOOT_NAME(t, b) do { if (tide_foot_parts()) tide_part_name(trk_index(t), (b)); else sound_label((t), (b)); } while (0)
static int tide_foot_two(void);
#define TIDE_FOOT_PART(sel) (tide_foot_two() ? 1u : (sel))   /* the part footer row 2 names: on TIDE and TIDE SET part 2 (row 1: part 1) */
static const char *tide_tl_swell(track_t *t, uint32_t ripple);   /* (tide_take.c: SEQ held + C4 SWELL / D4 RIPPLE, b11) */
static uint32_t tide_sw_pending(void);                    /* (tide_take.c: 1 a SWELL waits for the loop start, 2 a RIPPLE, 0 none) */
static uint32_t tl_sw_apply(track_t *t, uint32_t ripple, uint32_t seed, uint32_t live);   /* (ui_tools.c; live: the part is playing, the result is built in a scratch copy and written with the interrupt off) */
static int32_t tk_poff(const track_t *t);                /* (tide_take.c: what a step note of the part sounds at) */
#define TIDE_TL_DO(t, p, was, len) do { if ((p) == TL_KEEP) return tide_tl_keep((t), (was), (len)); if ((p) == TL_RANDOM || (p) == TL_COOK) return tide_tl_swell((t), (p) == TL_COOK); } while (0)
#define OBJ_TIDE (OBJ_PATCHX0 + 15u)                     /* patch chunk 21's object: where earlier TIDE builds kept the settings (read once, never written: tide_store.c) */
#define TIDE_PT_NCH 25u                                  /* the patch store's chunks with TIDE: 0..20 and, b12, 21..24 (objects 55..58 at 0xF4000): 150 saved patches at most (126 + 24: TIDE_PT_NOLD) */
#define TIDE_PT_NEXT(k) ((k) + 1u == TIDE_PT_NOLD * 6u ? UP_BANK : (k) + 1u)   /* (the tests: the slots a full store holds, in order: 1..126, then 129..152) */
#define TIDE_PT_END (UP_BANK + (TIDE_PT_NCH - TIDE_PT_NOLD) * 6u)
#define TIDE_PT_LAST (TIDE_PT_END - 1u)
#define TIDE_PT_NOLD 21u                                 /* the chunks an earlier build reads (0..20): slots 1..128 live there (126 records), the slots past them only in 21..24 (24) */
#define TIDE_MG_N 5u                                     /* ui.mg's gestures: MORPH's, GLO's and GLIDE's (tide_ui.c TIDE_G_GLIDE) */
#define TIDE_PAGE(pg) ((pg)->scope == SC_TIDE)
#define TIDE_WAVE_PAGE(pg) ((pg)->graph == GR_MORPH)      /* the MORPH page is a template 9 page: MORPH on one line, the part's real sound in the big area (ui_scope.c), FILTER / RESONANCE / ECHO on the slim row (1.0.0-b7) */
#define TIDE_LINK(kind, mask, v) do { if ((kind) == MQ_FADER) tide_link((mask), (v)); } while (0)   /* LINK: the SEQ fader follows */
#define TIDE_MQ_ROUTE(kind, mask, v, pi) ((kind) >= MQ_GLIDE ? (tide_midi((kind), (mask), (v), (pi)), 1) : 0)   /* GLIDE, the SEQ fader */
#define TIDE_MQ_KINDS , MQ_GLIDE, MQ_SEQF
#define TIDE_MQ_MERGE(kind) || (kind) == MQ_SEQF
#define TIDE_SCOPE_OPS {SC_TIDE, 0x0Fu, 0, tide_knob, tide_reset},   /* (ui.c SCOPE_OPS) */
#define TIDE_LIST_ON() tide_list_on()
#define TIDE_LIST_OCT(oct) tide_list_oct(oct)
#define TIDE_GRAPH_CASES
/* TIDE and TIDE SET are template 9 pages (owner, 2026-11-01: 1.1's look): the knob-1 line and the card row are ui_draw.c's; the pages draw
 * their own body (ui_graph.c draw_graph, before the area's canvas), and the card row sits TIDE_LIFT rows higher so that the footer keeps its
 * two rows (tide_foot_two: part 1 and part 2) */
#define TIDE_LIFT 8
#define TIDE_GRAPH_OWN(pg) do { if (TIDE_PAGE(pg)) { if ((pg)->graph == GR_TIDE) graph_tide(); else graph_tidelist(); return; } } while (0)
#define TIDE_CARD_LIFT() (tide_foot_two() ? TIDE_LIFT : 0)
#define TIDE_FOOT_FULL() tide_foot_two()
#define TIDE_GRAPH_SIG(pg, h) do { if ((pg)->graph == GR_TIDELIST) return ((h) ^ tide_list_sig()) * 16777619u; \
                                   if (TIDE_PAGE(pg)) (h) = ((h) ^ tide_sig() ^ (uint32_t)song.both * 0x85EBCA77u ^ (uint32_t)duell_mode_req * 0x9E3779B1u) * 16777619u; } while (0)   /* (1.0.0-b7: the MODE, AUTO TIDE names the play mode) */
#define TIDE_COLUMNS() do { if (TIDE_PAGE(cur_page())) { tide_columns(); return; } } while (0)
#ifndef TIDE_HOME_ANSWER
#define TIDE_HOME_ANSWER 1                               /* .. the answer on knob 1's line (tide_ui.c tide_answer_line; the take's header message is not repeated there, tide_take.c); 0 drops it */
#endif
#define TIDE_HOME_NAME "MORPH"                           /* MENU > CONTROL > HOME OPENS: the first page of the round (MORPH, as stock; b12, owner: AUTO TIDE is the round's 2nd, TIDE SETTINGS the 3rd) */
#define TIDE_HOME_MODE_NAME "SETTINGS"                  /* (b4, UI audit 9: HOME OPENS' second value names the page it opens, TIDE SETTINGS; MULTI LOAD keeps or sets the PLAY MODE row) */
#define TIDE_MLOAD_NAMES {"KEEPS PLAY MODE", "SETS PLAY MODE"}
#define TIDE_HOME_MODE(pg) ((pg)->graph == GR_TIDELIST)   /* HOME OPENS = MODE: TIDE has no MODE page (no PAGES row); its MODE is a row of TIDE SETTINGS, which opens with that row picked */
#define TIDE_MODE_PG(pg) 0                              /* the MODE page shown (never: TIDE has no MODE row in PAGES) */
#define TIDE_GRID3 0                                    /* (its card template 3, CG3: SCOPE_OPS has no MODE row) */
#define TIDE_PICK_MODE() tide_pick_mode()
#define TIDE_MODE_SAY "PLAY MODE "                       /* the header message of the output mode (the TIDE SETTINGS row PLAY MODE; AUTO TIDE's K2 is MODE) */
/* LIVE GRAPHS (ui_live.c, included by ui_graph.c; the animation of the envelope, LFO, CYCLE, filter, insert, matrix and chord pictures; MENU > ANIM OFF takes it away). FM6 AMP DEST shows the operator envelope (OP ENV's picture) so that it can follow it */
#define TIDE_LIVE_SIG(pg, t, h) ((h) = ((h) ^ live_sig((pg), (t))) * 16777619u)
#define TIDE_LIVE_DRAW(pg, t) live_draw((pg), (t))
#define TIDE_LIVE_MOD(t, y0) live_mod((t), (y0))
#define TIDE_LIVE_STRIP(pg, t) do { live_strip((pg), (t)); if ((pg)->graph == GR_TIDE) tide_water(0); } while (0)   /* (b12: AUTO TIDE's water) */
#define TIDE_LIVE_CHORD(t) live_chord(t)
#define TIDE_AMPDEST_FM6(pg, t) ((pg)->graph == GR_ADSR && (pg)->id[0] == P_ED_PIT && (t)->eng_req % NENGINES == ENGI_FM6 && fop_ok())
/* H18 (ui_input.c first_boot_poll): the factory multi a new unit (or one after ERASE DATA, and the emulator) starts with: M02 DUB CHORD (M04 until D31, 2026-11-05: BASIC CHORD took M01 and the curated order moved it),
 * the TIDE showcase (owner, 2026-11-01): its two 16-step SEQUENCER arps play on PLAY with no key held (ARP SEQ), AUTO TIDE set to JRNY.
 * (NIGHT ORGAN, now M04, before: 8 steps, "not a great showcase") */
#define TIDE_FIRST_MULTI 1u


/* U1 (upreset.c, patslot.c, ui*.c): the last UP_NUSER voice slots of the bank and PS_NUSER pattern slots are USER slots, empty until saved. The factory
 * sounds / patterns they displace stay listed after them (voice places UP_BANK.., pattern places PS_SLOTS.., read-only). No record moves.
 * The factory sounds past the 128 of the bank (sounds.c DS_N, tools/duell_sounds.py N_BASE) follow the displaced ones (UP_NXTRA). b12: every voice place is a
 * slot (UP_SLOTS = UP_VSLOTS): the displaced #125..#128 and #129..#171 are slots 128..174, saved over and restored as any other */
#define UP_NXTRA (DS_N > UP_BANK ? DS_N - UP_BANK : 0u)
#define UP_USER0 (UP_BANK - UP_NUSER)
#define UP_VSLOTS (UP_BANK + UP_NUSER + UP_NXTRA)
#define PS_USER0 (PS_SLOTS - PS_NUSER)
#define PS_VSLOTS (PS_SLOTS + PS_NUSER)
#endif
/* the pages where the part selection has its third position, BOTH (owner 2026-11-08): HOME's MORPH page, TIDE SETTINGS and AUTO TIDE, and PERFORM, whose K1 MORPH and ROLL are the MORPH page's
 * controls (and the test build's CPU TEST page). Everywhere else the selector picks PART 1 or PART 2 and the page shows and edits that part alone, as before; BOTH stays set for these pages */
#define HOME_BOTH_PAGE(pg) ((pg)->graph == GR_MORPH || TIDE_PAGE(pg) || (pg)->scope == SC_TEST || (pg)->scope == SC_PERF)
#if TIDE_PLAYSEL
/* 1.0.0-b7 PLAY MODE SELECT: ALGORITHM's position names on those pages: PART 1, the middle named by the PLAY MODE row, PART 2 (ui_input.c play_pos; docs/design/PLAY_MODE_SELECT.md) */
enum { PLS_P1, PLS_LAYER, PLS_STEREO, PLS_DUEL, PLS_P2, PLS_N };
static const char *const PLS_NAME[PLS_N] = {"PART 1", "LAYER", "STEREO", "DUEL", "PART 2"};
static uint32_t play_pos(void);
#endif
