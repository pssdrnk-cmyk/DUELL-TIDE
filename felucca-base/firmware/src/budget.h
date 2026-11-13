/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL NOTES budget (docs/CPU_PLAN.md section 5; budget.c): how many notes each part may play at once, worked out from what is switched
 * on, so the predicted worst case stays under BUDGET_LIMIT of the audio time budget (one core). All costs are in 1/100 of a percent of the
 * time budget of a half buffer (10000 = the whole budget), the scale of tests/cpu_bench.c and tests/bench_baseline.txt (3281.25 host
 * instructions a sample = 100 %). The functions are pure: the same settings give the same numbers, whenever and in whatever order they
 * are called. Unity build: budget.c is included after fx.c (it reads the master and bus settings), voice.c only reads bnotes[]. */
#ifndef DUELL_BUDGET_H
#define DUELL_BUDGET_H

/* TIDE 1.0.0-b6 N1, "give the notes back" (owner, 9 November: in steps, each kept only if the unit's GLIDE sweep stays clean: 0 missed, 0 voices cut). Since b4 the ease
 * ladder (audio.c, K3) backs off before it cuts sound, and the b4 / b5 sweeps at 72 were clean (worst per-second peak 84 .. 85 % render, 90 .. 91 % elapsed). The limit is a
 * step, DUELL_NOTES_STEP: 72 (b1 .. b5), 76, 80 or 84 (the b0 value, where the factory multis missed and shed before the ladder existed). A test-mode build picks one
 * (tools/build.py: env DUELL_NOTES_STEP, test-mode builds only; work/measure/b6-notes/UNIT_PLAN.md); every other build has the default, 72 (b6: 76 was tried on the unit, 9 November, and cut one voice on M14 EERIE SIGNAL at 90 %; b6 keeps b5's 72).
 * BUDGET_PEAK stays 90 - BUDGET_LIMIT: the room up to 90 % for the real peak over the model (the unit's fit, below: peak = 30 + 0.74 x the
 * model, i.e. 83 % at 72, 86 at 76, 89 at 80, 92 at 84; the GLIDE sweep at 72 ran 1 .. 2 points over the fit, 85 %, and 6 more elapsed, 91 %) */
#ifndef DUELL_NOTES_STEP
#define DUELL_NOTES_STEP 72     /* b6 default: b5's 72 (the unit's GLIDE sweep at 76 cut a voice on M14: work/measure/b6-integ-aefe5c0); the step stays for a later finer try */
#endif
_Static_assert(DUELL_NOTES_STEP == 72 || DUELL_NOTES_STEP == 76 || DUELL_NOTES_STEP == 80 || DUELL_NOTES_STEP == 84, "budget.h: DUELL_NOTES_STEP is 72, 76, 80 or 84");
#define BUDGET_PEAK (90u - BUDGET_LIMIT)   /* (b6 N1: 90 - the step; 18 at 72) CPU fix (wave 14, 2026-11-05; owner: "gotta conserve the cpu"): the points of a half's time the real per-second PEAK runs above the model's worst case, so that the peak stays under
                                 * about 90 % (BUDGET_LIMIT + BUDGET_PEAK = 90; it was 6). Calibration, the unit's own numbers (work/measure/cpu-multis/sweep_k0.jsonl and sweep_k1.jsonl: the 16 factory multis of
                                 * bb730b0, PLAY alone and a held Cm7, the console's `cpu` = the longest half of each second, 12 s each) against this model at the same voices (the busier core): the per-second peak
                                 * max = 30 + 0.74 x the model's peak, rms 9 points over 23 runs (the model's voices cost 0.74, the rest of the audio interrupt 30 more); the one outlier is FM6 BIG STAB (DUB CHORD, +24:
                                 * a chord hit costs its voices a second time, the note-on and the attack). At the old limit (84) the multis whose voices filled caps of 76 .. 80 ran peaks of 85 .. 109 % with 1 .. 8
                                 * missed halves and 2 .. 33 voices shed in 15 s (DUB CHORD, WHISPER TIDE, FINAL REEL, EERIE SIGNAL, DEAD BUFFER). At 72 the fitted peak of every factory multi is 83 or less. A part has
                                 * 1 or 2 notes fewer than at 84 (tests/budget_test.c prints them) */
#define BUDGET_LIMIT ((uint32_t)DUELL_NOTES_STEP)   /* (b6 N1: the step above; 72 in b1 .. b5, was 84, with an allowance of 6; 81 and 88 before) the share of the time budget the worst case may use (percent), the margin below applied to every cost (DUELL 1.1 item 132, owner: 80 -> 90; item 133: 88 with the
                                 * margin 1.15). DUELL 1.1 item 137 (owner: the model charges the true measured cost, margin 1.00; cap = the most notes whose true model load + 6 points of measured
                                 * steady peak over the average (HANDOFF_V2 item 130) fits, for core 0 and core 1): core 0 / one core: 90 - 6 = 84; core 1: block 0's job (posted ~0.27 block into
                                 * the half, the part's true cost) + 6 points must end before the runner's wait limit, which keeps its 1.15 table (BUDGET_DL_MARGIN): over the 19,472 setups of
                                 * tests/deadline_test.c with both parts at their caps that holds up to 81 (least slack 8.9 points; 82: 3.5, 84: -5.3 = the job past the wait limit = a silent part-2
                                 * block + pause). So 81: true peak <= 87 % on core 0. DUELL 1.1 item 142 (owner-approved, HANDOFF_V2 item 139): the wait limit is the later of the 1.15 one
                                 * and the true-cost one less TCR_SLACK (twocore_run.c tcr_deadline); core 1's job k (every block) + 6 points then ends before it at the caps up to 86
                                 * (tests/deadline_test.c CORE 1: least slack after 6 points 84: 10.9, 85: 6.9, 86: 2.9, 87: -1.1; after 13 points at 84: 3.9), so core 0's own rule binds:
                                 * 90 - 6 = 84. Predicted true peak <= 90 % on core 0. Read only by budget.c budget_solve2 (both rules); the runner's deadline does not read it */
#define BUDGET_MARGIN 100u      /* every cost of the table below times this (percent) for the NOTES caps. DUELL 1.1 item 137: 115 -> 100, the table is the unit's own COST-CAL measurement (item 116) and
                                 * the raw model was >= the measured average in every held-out run and >= the measured steady peak in every clean POLY-MAX row (item 130) */
#define BUDGET_DL_MARGIN 115u   /* DUELL 1.1 item 137: the two-core runner's deadline and wait limit (twocore_run.c tcr_deadline, through part_dl / global_cost_dl) keep the
                                 * 1.15 table they always had, so they are the same bit for bit (tests/deadline_test.c); only the block-0 take-over gets a true-cost second chance (tcr_deadline_true)
                                 * and (item 142) the wait limit is the later of the 1.15 one and the true-cost one less TCR_SLACK */
#define NOTES_MAX 10u           /* the most notes a part is ever given; a part's own NVOICE voices cap it further (DUELL 1.1 item 90: each part has its own 8, no shared pool) */
#define BUDGET_HO 66u           /* DUELL 1.1 item 90, the two-core rule: the hand-over's cost on each core, host instructions a sample (2 %: the one-engine tests of the owner's BENCHM1 read
                                 * 1.5 to 2.0 points more on core 0 with core 1 on than BENCHC1 without it). DUELL 1.1 item 132 (owner): NOT CHARGED any more (COST-CAL's estimate was
                                 * -28, i.e. nothing): budget.c no longer reads it; kept as the figure tools/cost_fit.py fits and reads. The runner's deadline never read it */

/* what limits a part's number (part_notes_reason): the item whose removal would give the part the most notes */
enum { NR_NONE,                 /* nothing: the part has all it can have (NOTES_MAX, the pool) */
       NR_ENGINE,               /* the engine itself: its voice cost, or its own voice limit (PHYS 3, VOICE 4) */
       NR_FILTER,               /* the multimode filter's 24 dB slope (a voice) */
       NR_INSERT,               /* the part's INSERT */
       NR_SAT,                  /* the part's SAT */
       NR_EQ,                   /* the part's EQ bands */
       NR_FX,                   /* the effects: chorus, delay mode, reverb type, SHIM, COMP, WIDTH, BASS MONO (the buses of DUEL twice) */
       NR_OTHER,                /* the other part's notes */
       NR_MISC,                 /* the ducker, the matrix, the LFOs, the arp, the morph */
       NR_N };

static uint32_t part_cost(const track_t *t);    /* ONE voice of the part t (engine, filter slope, morph extras) */
static uint32_t part_fixed(const track_t *t);   /* the part's own cost whatever it plays (INSERT, SAT, EQ bands, ducker, matrix, LFOs, arp) */
static uint32_t global_cost(void);              /* the idle mix, the sends and the master stages of the mode */
static void part_dl(const track_t *t, uint32_t *fixed, uint32_t *voice);   /* item 137: part_fixed and part_cost with BUDGET_DL_MARGIN (1.15), for the two-core runner's deadline and wait limit only
                                                 * (item 142: both from one bd_part) */
static uint32_t global_cost_dl(void);           /* .. and global_cost */
static uint32_t part_notes(int part);           /* the budget's number of notes for the part, 1 .. NOTES_MAX (0: a part that never sounds) */
static uint8_t part_notes_shown(int part);      /* what the UI shows: the same number, with the engine's own voice limit */
static uint8_t part_notes_reason(int part);     /* NR_*: what limits it */
static void budget_poll(void);                  /* main loop, every frame: hands the numbers to the note-on path (voice.c trk_nvoice) */
static uint32_t bd_two_now(void);               /* DUELL 1.1 item 90: 1 = the two-core rule is in force (core 1 up, no fault, jobs not paused), 0 = the one-core rule */

/* THE DEADLINE'S COST ITEMS, HANDED OVER BY THE MAIN LOOP (b11, ported from DUELL 1.2 by OSCILL/R, GPL-3.0: budget.h / budget.c bd_dl_publish, twocore_run.c tcr_costs). Working them out in the
 * audio interrupt at every half's first block cost 113 to 127 us there on this firmware (the b11 sweep's st_budget_us, all 26 multis; 1.2 measured 171 to 188 us on its own), the largest single
 * piece of the block that starts a half late. budget_poll (main loop, every frame) calls bd_dl_publish: the five items (part_dl of both parts, global_cost_dl) into the copy the audio side is NOT
 * reading, then one byte says which copy is the new one. The runner (twocore_run.c tcr_deadline) reads the copy that byte names: the main loop never interrupts the audio interrupt, so it always
 * reads a whole copy. The items are an estimate for the deadline (a 15 % margin in them); after a knob or a fader moves they are at most one main-loop pass old (the old way: one half).
 * A LOAD (a sound, an engine, a multi, a project: ui.c load_begin, project.c project_restore_runtime) clears bd_dl_ok, so the runner works the items out itself, as before, until the next
 * publish: a load can change them by more than the margin, and the main loop's next pass may be up to a frame and a redraw away (ui_input and the editor also run in the frame's wait).
 * Before the first publish (and in host tests that never run the main loop's pass) the runner works them out itself too */
typedef struct { uint32_t c1f, c1v, c2f, c2v, g; } bd_dl_t;
static bd_dl_t bd_dl_buf[2];
static volatile uint8_t bd_dl_i, bd_dl_ok;
static void bd_dl_publish(void);

#endif
