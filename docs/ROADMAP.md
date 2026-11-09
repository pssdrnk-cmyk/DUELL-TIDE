# DUELL with TIDE roadmap

Updated 9 November 2026. What is already built is in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md). Older roadmaps are in [history/](history/); the audits of 8 November in [audits/2026-11-08/](audits/2026-11-08/).

State words on this page: Released in bN, Ready (built and tested, waiting for a release), Built (waits for the FM-1 test), In progress, Planned (brief written, not built), Next, Later, Waiting on you.

## Now: 1.0.0-b6 (b5 folded in)

b5 was never published: its changes ship inside b6 as one release (owner, 9 November: "let's move to b6"). The b6 build is put together on one branch; the rows below say what is in it.

| What | State |
|---|---|
| The MORPH page's waves move like the first WAVE LAB stack again (13 lines a part scrolling back every picture, full height, fading toward the back), at today's size (from b5) | Built (in the b6 build), waits for your look and the CPU test on the FM-1. The owner, 9 November: "the animation quality got worse, the front lines moves a lot and the ones behind barely"; "the initial version looked quite cool when we called it wave lab" |
| MORPH page: an older saw line no longer crosses the newest one on note changes (from b5) | Built (in the b6 build, with the motion fix), waits for the FM-1 test |
| PART 1 / PART 2 / BOTH on HOME's pages (from b5) | Ready (in the b6 build) |
| The new flashing screen (from b5) | Ready (in the b6 build) |
| More free memory (EQ design moved out of fast RAM, 1,068 B; from b5) | Ready (in the b6 build) |
| Factory sounds and multis stop resetting AMOUNT, the two drone scenes too (MORPH c3) | Ready (in the b6 build) |
| Pattern notes stay in a sensible range (roles T4) | Ready (in the b6 build) |
| More free flash: factory multis stored as template + changes (2,776 B), keycaps compressed (2,032 B); nothing you hear or see changes | Ready (in the b6 build) |
| Two drone scenes built in (M17 DUNE HORN, M18 VOID DEEP, played as STYLE DRONE; [brief](audits/2026-11-09/drones/TASKS.md)) | Built (in the b6 build), waits for the FM-1 test |
| Give back the notes the first beta's CPU fix took (most factory multis play 1 or 2 notes fewer per part since b1; b4's ease ladder now catches the peaks) | Built (in the b6 build at step 76), waits for the FM-1 test, which picks the step 76 / 80 / 84 |
| FM6 sounds no longer get louder at the morph's End B (FRETLESS, ISLAND STEEL, MALLET WOOD) | Built (in the b6 build), waits for the FM-1 test |
| JOURNEY travels to sounds near the current one in level and colour (the default set moves up to 15 dB today; [J1](audits/2026-11-09/tide-morph/TASKS.md#j1)) | Built (in the b6 build), waits for the FM-1 test |
| Reverb on the second core, to free CPU (K4) | Built, not in the b6 build yet: needs its own FM-1 sweep with K4 on and off |
| JOURNEY's crossing dip lasts a few bars, not 90 % of every trip ([X1](audits/2026-11-09/tide-morph/TASKS.md#x1); [audit 9 November](audits/2026-11-09/tide-morph/REPORT.md)) | Planned: ~60 B, 8 B RAM; before/after with `tide_long` |
| JOURNEY doesn't jump the sound when started with the fader away from A ([J2](audits/2026-11-09/tide-morph/TASKS.md#j2)) | Planned: ~16 B, main loop |
| TIDE's louder peak and softer breakdown reach ARP SEQ parts, M02 included; accents kept ([D1](audits/2026-11-09/tide-morph/TASKS.md#d1)) | Planned: ~60 B, one multiply per arp step |
| The bass keeps its loop and octave: no POLY, no VOIC octave on a bass part; POLY ends in ARC's breakdown ([B1](audits/2026-11-09/tide-morph/TASKS.md#b1)) | Planned: ~65 B, 1 B RAM |
| CPU test of b6 on your FM-1 (the 16 multis, the drones, the notes step; MORPH on screen) | Next. b5's baseline (9 November, before the motion fix): no missed blocks and no voices cut in all 16 multis, busiest block 83 % (M05). The sweep starts with one warm-up scene and checks MORPH is on screen before M01 (in the b5 run M01 came first after boot, ran off the MORPH page and cut 2 voices; on MORPH it was clean at 75 %) |

b6 goes out after the CPU test and your OK on the FM-1. Room left on the b6 build (with the motion fix and JOURNEY): 8,060 B of flash, RAM 69,624 of 73,728 B (4,104 B headroom against the 4,096 B rule).

## Later, in order

1. Hand-moved morph saving: a hand or MIDI fader move blended every other block like a GLIDE (about 70 us a block saved). Finding 12, D44.
2. Transition costs only while moving: the engine cross-fade, valley and wash charged only while a transition runs, so more notes play at rest.
3. FM6 crossing fixes (findings 8, 9; brief K7) and Felucca #265 (signed DSP arithmetic, bit-exact).
4. Free 276 B of RAM: the SEQ MORPH history UNDO no longer reads ([R1](audits/2026-11-09/tide-morph/TASKS.md#r1)). When a feature needs RAM.
5. Two small interrupt savings in the crossing step ([C1](audits/2026-11-09/tide-morph/TASKS.md#c1)), with "Transition costs only while moving".
6. WASH on: no jump when a glide is stopped by hand, one dip instead of two stacked ([W1](audits/2026-11-09/tide-morph/TASKS.md#w1)).
7. Docs fixes from the TIDE and morph audit: `TIDE_GUIDE.md:49` how long the crossing dip is; `TIDE_GUIDE.md:23` the MORPH knob 3 text (the wave picture is shipped); the DUB recipe's WASH LOW against STYLE DUB's WASH OFF; AMBIENT "no groove tricks" against POLY GENTLE; WANDER "a little further each trip"; the bass promise worded for POLY and VOIC too, once B1 lands; the dynamics paragraph, once D1 lands.
8. Factory pattern table trimmed (~0.9 KB; [SP3](audits/2026-11-09/space/TASKS.md)).
9. Parameter units stored as an index (~0.87 KB; [SP4](audits/2026-11-09/space/TASKS.md)).
10. Event-rate code built for size (1.78 KB; [SP5](audits/2026-11-09/space/TASKS.md)); needs the FM-1 CPU sweep.
11. FM6 MUSIC rolls at AMOUNT 40 leave the loudness window (all FM6 rolls, not just End B); needs its own change ([finding 7](audits/2026-11-09/tide-morph/REPORT.md)).
12. Before `1.0.0`: 0 missed blocks over the 16-multi GLIDE sweep (owner, 8 November); the CPU budget re-measured and made honest (findings 5, 6, 34; K9).

## Released

| Version | Date | What you got |
|---|---|---|
| 1.0.0-b4 | 8 November | No notes cut when the CPU runs hot (ease ladder), less CPU while a part is silent, STYLE DRONE, screen cleanups |
| 1.0.0-b3 | 8 November | CPU and missed-block readout, patch list order, live recording into ARP SEQ, HOME without the MODE page |
| 1.0.0-b2 | 8 November | Second beta |
| 1.0.0-b1 | 5 November | First beta |

1.0.0-b5 was not released separately: its changes are in 1.0.0-b6.

Stable: `stable-2026-11-03` (`187e9d0`), the last build you validated. The first numbered stable will be `1.0.0`.

**Quick reference:** [TIDE settings at a glance](TIDE_QUICK_TABLE.md).

## How we work

- **Betas carry what is ready.** Several audio changes may ship in one beta (owner, 8 November: delivery first); each one is still measured on the unit before it ships.
- **Measure before and after.** An audio-path change is measured on the unit before and after, with the same script, against a pass mark written down first.
- **Builder and four reviewers.** Each change is built by one agent on its own branch, then read by separate reviewers: correctness, quality and cohesion, tests, docs. The coordinator decides the fixes; a verification review checks them.
- **Pull requests.** Every change goes to GitHub as a pull request against `jam11`; GitHub runs the full host suite on it. Locally only the device build (size, hot-code check) and `check_docs.py`. Builders work in parallel; merges go one at a time, with the owner's OK.
- **Never flash red.** A tagged beta is never flashed while the full suite is red. The tag comes before the build; the flashed commit is merged and pushed the same day.
- **Plain DUELL is no longer kept identical** (owner, 8 November: no upstream merging). Shared code may change when that is simpler or smaller; the `DUELL_TIDE=0` build should still compile, but `jam/check_off.sh` is no longer a gate. The `partindep` pins are never regenerated.
- **Formats and user data are kept** (patches, patterns, multis, settings, KEEP / UNDO) unless the owner decides otherwise.
- **No new pages.** TIDE options are rows on TIDE SETTINGS with good defaults; restyles keep every fader bar, note and shown value.
- **Technical choices are the coordinator's**, with audio quality first, then risk and effort. The owner decides what is heard, seen and touched.
- **Anything unfinished gets a row here at once.**
- **Pushes and releases need the owner's OK.**
- **Look first:** a visible change is shown to the owner on the unit (or emulator) and approved before tests and paperwork.

## Planned, not yet scheduled

**Screens** (redundancy audit 8 November, all 433 screens rendered)
- TIDE rows SYNC and ROLL renamed so they don't share words with tempo sync and MORPH's ROLL. (The rest of this audit was released in b4.)

**One view for both parts** (owner, 8 November): AUTO TIDE and the MORPH / wave page always show both parts, with the selected part clearly marked (full colour, the other dimmed, plus a cue that works on MONO and GREY). The wave page and AUTO TIDE do it in b5 (see Now).

**RAM room** (b5, Ready): pool tail 9,276 B against the 8,192 B rule; a test-mode build keeps the lowered 7,936 B rule. The next change that grows range Z or the pool measures against that.

**MORPH** ([review](audits/2026-11-08/morph/REPORT.md): keep the core and the page, no big rework)
- c3 (b6, Ready): factory loads stop resetting AMOUNT; K4 dim in JRNY, where it does nothing. Branch `fix/b6-morph-c3` `148d84b`, tests pass; rebase onto the reworked MORPH page before b6.
- c2: a hand fader resting in the middle (40-60 %) between ends that differ in a switch stays dulled, FM6 on pure sines: after about 1 s at rest, settle onto the nearer side (about 100 B, audio interrupt; listen on the unit first).

**CPU and audio engine**
- Effect code from slow flash: [analysis](audits/2026-11-08/flash-hot/REPORT.md) done. Of the 116 functions, 50 run per sample. Plan: first a test-mode probe that times one function from flash and from RAM (no sound change); only if it shows at least 0.5 % of a half, move ~0.9 KB (`dly_wr`, `master_knee`, `acc_sum`, `cho_zrun`, `fx_swing`, `rev_front`, `hall_gains`) and later ~1 KB more, as its own beta measured on the unit. Expected gain 1 to 3 %, not enough alone for `1.0.0`.
- The budget model (`bd_part`, `bd_global`, `bd_slope`, `part_dl`, 2.1 KB) runs from flash at the start of every half: run it only when its inputs change. Larger gain than the above, but it changes behaviour; needs its own design and test.
- K8: the next fader position's blended parameters prepared outside the audio interrupt (main loop or idle core 1) and swapped in by pointer. Large; needs one-writer rules.
- K10: the dearer part (by measured cost) on core 1, not always part 2.
- K11: a TIDE-only 5.8 ms half for background use, costed first (keyboard play keeps 2.9 ms).
- K12: the "prove the CPU cost before flashing" process written into CONTRIBUTING and TESTING.
- Two readers of the arp step ignore a POLY stretch (`ui_draw.c` ~1901, `seq_dialogue.c` ~44, found in the finding 36 review): check whether the display or TURNS can disagree with what plays.
- Code nits with no measured effect (finding 35): `DUELL_PROBE_STAGES` defined outside `#if DUELL_TIDE`; `morph_xside` into RAM only if the unit shows its cost; `glide_stop` lags one poll.

**TIDE interface**
- LFO DEPTH destinations shown as "LFO1/2 LVL"; the mod wheel keeps driving MORPH.
- A legend while OCT- is held on the TIDE page (C9, Later).

**Content**
- Roles spec T4 (b6, Ready): branch `fix/b6-roles-t4` (review MERGE, +316 B, audio interrupt untouched); merge it for real into b6, never as a patch of the diff against b5/integ (the branch predates the flash screen). Cleanups then: TIDE_GUIDE.md:153 wording (the floor is min(lowest, C3)), the `t_lane` edge check is met for free in the bass scene, the unused `sqm_note` (seq_morph.c:89) and its comment at :60.
- Roles spec T7: CELLAR TAPE's bass comes back with rewritten records.
- Roles spec T8 to T10: the new reel of multis, together with the `tideroles` levels (43 of 338 checks red: M04 +3.8, M10 +5.3, M12 -3.1, M15 -7.2, M16 -9.2 dB from the median; finding 25, D46).

**Live recording** ([audit](audits/2026-11-08/liverec/LIVEREC_AUDIT.md))
- Documented limits of the keys-record-into-ARP-SEQ pattern (Released in b3): a 2-step pattern in REPLACE splits a chord across the step boundary, and the key's step is predicted from the POLY stretch before the next arp step runs (it can land one step off at a stretch change). [Brief R2](audits/2026-11-08/liverec/TASKS.md#r2)
- Plain DUELL's `rec_note` stays under `#else`: deliberate, a shared `rec_put` would change the `DUELL_TIDE=0` binary.
- Named `tide.h` hooks for the raw `#if DUELL_TIDE` blocks in `seq.c`; a `seq_len()` helper; one "past half step" helper.
- Dead `SC_MODE` code under TIDE (flash to buy back, with the MODE page's `CG3` cards).
- A small per-row kind table for TIDE SETTINGS (replaces the row comparisons in `tide_ui.c`).

**Hot code check (K6)**
- K6 follow-up (review finding H1): `tools/hot_reach.py` does not follow function pointers or address-taken functions, so a new engine voice function left in flash and reached only through the engine table passes silently. Cheap fix: add an edge for every function whose address is a literal-pool word or in a data pointer table of a reached function, or name the engine entry points as roots. The self-test is `tools/test_hot_reach.py` (host group `hotreach`).
- K6 follow-up: 135 allow-list reasons in `tools/ram_allow.py` say "mix or effect stage ... not yet reviewed". They are per block, not per event; once each is reviewed, relabel it "per block, not yet reviewed" (or move the function to RAM). The classification is in [flash-hot REPORT](audits/2026-11-08/flash-hot/REPORT.md).

**Storage and backups**
- U4, 24 multi slots: tested on `wip-24-multis` (`21ca3a7`), its own rollout with a format bump. At the rebase: renumber its backup ids (41 is now the FM6 voice bank), archive VERSION 6 against 5, update `erase_test.c` and `xb_test.c`, measure the flash first (keep 3 KB headroom; 4,556 B is free on the b5 build).
- Patch capacity in the docs: INFO says 128 slots, TIDE guarantees 126 worst-case records; replace "every slot always fits" with exact wording.
- Pattern save: document the exact visible entry point (saving needs STOP) before any live test.

**Repository and CI**
- The `emu` CI job: fixed in [#4](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/4) (emsdk sourced under bash; merged 8 November), and the job gates.
- Rename `jam11` to `tide`.
- Gate the release channels by CI too (a Stable / Beta release is offered only from a green commit), from the 4 November decisions.
- Retire the plain build (Later): `jam/check_off.sh` is already no longer a gate (owner, 8 November). Still open: drop `DUELL_TIDE=0`, `jam/check_off.sh`, `jam/off.sha256` and the tideoff test groups, and fold the `#if DUELL_TIDE` hooks into plain code where that saves flash or clarity. One step per beta.
- Test quality notes left from finding 38: split the FM6 budget check by crossing state, one generated multi-name table, the `F_NONE` hang exemption if it flakes.
- Workspace clean-up: retire merged worktrees (owner's OK).

**Memory savings, when a feature needs room** (estimates until measured; none is free)

| Where | Size | Note |
|---|---|---|
| FM6 lookup table | 4,100 B flash | A 257-entry table frees ~3 KB but changes pitch / gain math: an approximation to validate by ear. |
| Factory multi records | ~9.6 KB flash | Packing not measured yet (sound pairs saved 1,888 B the same way). |
| Delay line | 131 KB pool | A smaller line changes the sound. |
| Screen canvas | 59.5 KB pool | Drawing in smaller strips; every direct pixel access must be checked. |
| Performance capture | 32 KB pool | Check when it must stay live. |

## Waiting on you

| Question | Default if you don't mind |
|---|---|
| D43: when a crossing starts under a held chord, the chord stays whole (the budget plans for it from the start). | Stays whole; decided after the drones are measured under a GLIDE. |
| D46: the multis that are too loud or too quiet are trimmed by ear on the unit. | Trim M15, M16 up and M10 down by the numbers, then listen. |
| Try on the unit: keys recording into an ARP SEQ part, and REC ADD / REPLACE (Released in b3, not yet tried on the unit). | Nothing to decide; say if the nearest-step rule feels wrong. |
| Reverb FREEZE (below): build it? | Later: not built until you say. |
| Drone scenes: delivered as a file pack, replacing multis, or by packing the multi records to make room? | Decided: built in, 2 multis in ROM (owner, 9 November); no file pack. |
| Which of the ideas below, if any. | None until chosen. |

**By ear or eye on the unit** (no homework: noted when the owner meets them in use): the drones; the U2 multi sequences; the MULTIS / PATTERNS preview's readability; polymeter (12 / 16, 15 / 16) against a steady 4 / 4; whether TURNS' alternating bars improve the interplay.

## Ideas, not decided

- **Wave card L/R lines and the deep-bass line:** about 4 KB more RAM. The candidates are PERFORM capture (8 KB = 93 ms shorter capture) or a canvas drawn in strips. Owner decision when wanted.
- **From Niagara Drift** (an FM-1 drone firmware; checked against our code 8 November, estimates until built): **reverb FREEZE** on HALL / ROOM (the tail holds and the notes can stop; a block-rate gain change with a limiter, ~200-400 B, no CPU while off; risks: fixed-point overflow and the release click) fits best. **AGE** (slow tape-like wear, ~300-600 B) needs an offset layer on top of the saved sound, not checked yet. Gesture keys and a free-time motion loop do not fit the flash, the keys or the no-new-pages rule.
- **Keep current patterns on MULTI load** (U2 route A): an explicit load that keeps both playing patterns.
- **Musical evaluations kept from the earlier plan** (no work scheduled): M2a chord movement; M5 / M10a level and slow-glide behaviour only for a found defect; M7d polymeter first by hand; CHANGE / ENERGY rows only with useful behaviour. Details in [design/TIDE_MUSICAL_DESIGN.md](design/TIDE_MUSICAL_DESIGN.md).

## Decided against, or kept as is

- WAVE table quality stays (flash). SHIM's PITCH DOWN and MOD stay; cut only if a TIDE feature cannot fit, and then the owner is asked.
- No real chord steps in ARP SEQ; no cross-engine JRNY; no automatic harmonic rewriting, loudness normalisation or harshness classification.
- JOURNEY may choose its next sound by level and brightness; the sounds themselves stay unchanged. This doesn't conflict with 'no loudness normalisation' (owner, 9 November).
- Exact live-morph continuation after a restart is not required.
- Going back to plain DUELL 1.1 is documented, not supported.
- No upstream pull requests: TIDE goes its own way (owner, 8 November).
- No DRONE row: HOLD already latches the notes (owner, 8 November).
- LADDER's bass loss, filter 1 without self-oscillation and LOFI's aliasing stay as character.
- USB resampler taps as int16 (-3.8 KB, USB audio about 1 LSB different): not done, audio quality first (9 November).

## Versions and releases

TIDE has its own version in [`TIDE_VERSION`](../TIDE_VERSION); DUELL's `VERSION` (1.1) is not touched. ABOUT shows both.

| Version | Means |
|---|---|
| `1.0.0-b1`, `1.0.0-b2`, ... | A beta: a build flashed to a unit for testing. |
| `1.0.0` | The first build the owner has validated: the stable one. |
| `1.0.1` / `1.1.0` | A fix / a feature on top of a stable; its betas are `1.0.1-b1` / `1.1.0-b1`. |

Tag, GitHub release and CHANGELOG section share the name; the tag has `tide-` in front (`tide-1.0.0-b2`). The older dated tags stay as they are.

**To cut a beta:** set `TIDE_VERSION` and start its CHANGELOG section, commit; tag before building (`git tag -a tide-1.0.0-bN`), because `tools/build.py` shows the plain version only for a clean tree at the tag and adds `+` otherwise; build with a normal `python tools/build.py` in `felucca-base` (b2 did it so; `--release` would name the file `duell-<DUELL version>-beta.fwsc`, so copy `build/duell-1.1.fwsc` to `duell-tide-beta-<version>.fwsc`); flash and test; push the tag; make a GitHub **pre-release** with `duell-tide-beta-<version>.fwsc` and its `.sha256`, "Build `abc1234`" and the known issues in the notes; push `jam11` (or run Host tests by hand) so Pages offers the new beta.

**To cut a stable:** the same with `1.0.0`, a normal release marked Latest with `duell-tide-stable-1.0.0.fwsc`, and the `stable` branch moved to it. The site (`felucca-base/web/releases.py`) offers a beta only while it is newer than the stable one.
