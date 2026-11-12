# TIDE roadmap

Updated 12 November 2026. What is already built is in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md). Older roadmaps are in [history/](history/); the audits of 8 November in [audits/2026-11-08/](audits/2026-11-08/); the firmware review of 9 November in [audits/2026-11-09/firmware-review/](audits/2026-11-09/firmware-review/REPORT.md); the musical review of TIDE and the multis in [audits/2026-11-09/musical-review/](audits/2026-11-09/musical-review/REPORT.md).

State words on this page: Released in bN, Ready (built and tested, waiting for a release), Built (waits for the FM-1 test), In progress, Planned (brief written, not built), Next, Later, Waiting on you.

## Now: 1.0.0-b7

The unit runs b10-rc2, flashed 12 November at 02:34 (its TIDE_VERSION string still reads 1.0.0-b9). b10-rc3 is built and tested, and waits for the FM-1.

| What | State |
|---|---|
| Reverb on the second core, to free CPU (K4) | Built, needs its own FM-1 sweep with K4 on and off |

## On the unit: built in b7

On the unit since b10-rc2 (12 November).

Six builder lanes made these overnight (9 to 10 November); each is measured on the host; all of it has been on the unit since b10-rc2. State words in brackets are honest: what is only partly done says so.

| What | State |
|---|---|
| A sound saved from the web editor keeps its End B ([FR3](audits/2026-11-09/firmware-review/TASKS.md#fr3)) | Built |
| Small control fixes: RENAME no longer types stray letters, OCT- held as a modifier doesn't also act on release, a fast PRESETS turn can't land on the other part, SEQ + REC arms REC however the buttons are lifted, NOT SAVED only for saves ([FR7](audits/2026-11-09/firmware-review/TASKS.md#fr7)) | Built |
| Words that match the unit: ERASE DATA names the FM6 bank and the user tables (AMOUNT not dimmed in JOURNEY: done with page 1 in b7; the TURNS step falls away: the row is gone) ([FR11](audits/2026-11-09/firmware-review/TASKS.md#fr11)) | Built |
| ALGORITHM on HOME: PART 1, the middle, PART 2. The middle plays both parts in the PLAY MODE row's mode and the header names it (LAYER, STEREO, DUEL); PART 1 / PART 2 play one part on every key; changing PLAY MODE moves ALGORITHM to the middle. With PLAY MODE on LAYER, moving between a side and the middle cuts a held chord (the mode change silences held keys by design) ([design](design/PLAY_MODE_SELECT.md)) | Built (b7/playsel, on b7), on the unit since b10-rc2 |
| SPLIT MIDDLE: the keys split in the middle and the split follows OCT- / OCT+; with SPLIT set, STEREO and DUEL split the keys too | Built (b7/playsel, on b7), on the unit since b10-rc2 |
| Factory pattern table packed: its accent / slide / tie flags as bit masks (SP3, 640 B), the room for the two above | Built (b7/playsel) |
| STYLE DUB keeps the multi's own sounds (WANDER, not JOURNEY); AMBIENT without POLY; a saved STYLE stays that style ([MR1](audits/2026-11-09/musical-review/TASKS.md#mr1)). M02: brightness swing 2x not 7x, dulled bars 7 % not 51 %. The first start stays M02 DUB CHORD | Built |
| A shorter TIDE SETTINGS page: nine rows, TURNS / ARP / PITCH / VARY / WASH / POLY / SYNC rows gone, MODE a row, ROLL TYPE no longer switches the rhythm off ([MR8](audits/2026-11-09/musical-review/TASKS.md#mr8)) | Built. One mark not met: ARC under ROLL SMART is only 0.67 to 0.82 like MUSIC (not 0.95), because ROLL TYPE's own pattern roll differs; that is what ROLL TYPE is for |
| TIDE comes back by itself 8 bars after you let go of the MORPH or SEQ fader, only the part you moved is held, ARC keeps its place ([MR7](audits/2026-11-09/musical-review/TASKS.md#mr7)) | Built |
| JOURNEY doesn't jump the sound when started with the fader away from A ([J2](audits/2026-11-09/tide-morph/TASKS.md#j2)) | Built |
| JOURNEY's crossing dip lasts a few bars, not most of every trip ([X1](audits/2026-11-09/tide-morph/TASKS.md#x1)) | Partly: the dip is short (2 bars closing, 1 closed, 2 opening); the brightness mark is not met (brightness x0.48 to x0.70 against x0.8), the blend between the two sounds itself is dull mid-trip. M16's switch bar is louder than the median (a note attack, listen on the unit) |
| JOURNEY keeps the multi's level ([MR5](audits/2026-11-09/musical-review/TASKS.md#mr5)) | Partly: level kept on M05, M06, M16 (22 bars over 3 LU) and M08 (3 dB under its own sounds); the colour is not done (see Later) |
| TIDE's louder peak and softer breakdown reach ARP SEQ parts, M02 included; accents kept ([D1](audits/2026-11-09/tide-morph/TASKS.md#d1)) | Built: M02 peak 112, breakdown 62 (was flat at 100) |
| An arc you can hear: ARC on the 8-bar grid, the first arc after PLAY drops, a different arc each session, a breakdown that thins ([MR4](audits/2026-11-09/musical-review/TASKS.md#mr4)) | Partly: grid, drops and level done; the breakdown thins by voices (two of four) and level, not by onsets (M02 0.92 of the rise's hits, the mark was 0.65). Fewer onsets would mean dropping non-beat anchors, a decision for you (below) |
| Call and answer in place of TURNS: the two parts take turns bar by bar in WANDER and JOURNEY | Built; by voices 14 of 16 multis alternate, by hits 6 of 16 |
| The bass keeps its loop and octave: no POLY, no VOIC octave on a bass part; POLY ends in ARC's breakdown; the under-C3 guard on ARP SEQ parts ([B1](audits/2026-11-09/tide-morph/TASKS.md#b1)) | Built |
| Motifs stay whole: a five-hit figure keeps four, a line's one free note is not taken ([MR6](audits/2026-11-09/musical-review/TASKS.md#mr6)) | Built (M09 whole figure in 72 % of the bars, was 18 %) |
| The chord bed's voicing opens at the peak and closes in the breakdown; a chord-rhythm vocabulary of 12 house and dub-techno cells for the variation bar | Built |
| VOIC keeps an out-of-scale chord tone with OFF-SCALE PLAY (the VOIC part of [FR13](audits/2026-11-09/firmware-review/TASKS.md#fr13)) | Built; the tap-tempo part of FR13 is not (see Later) |
| A swung pattern is back on the grid after a POLY stretch ([FR1](audits/2026-11-09/firmware-review/TASKS.md#fr1)); a MIDI keyboard into the arp ([FR8](audits/2026-11-09/firmware-review/TASKS.md#fr8)); GLIDE on BOTH moves each part from where its fader is ([FR4](audits/2026-11-09/firmware-review/TASKS.md#fr4)); a factory multi leaves ROLL ARP, ROLL PITCH and ROLL TYPE as you set them ([FR5](audits/2026-11-09/firmware-review/TASKS.md#fr5)) | Built (in the b7 changelog) |
| BASIC CHORD's dust bed and the two drone beds play with no notes coming in; every part that waits for keys plays TIDE's chord under AUTO TIDE ([MR2](audits/2026-11-09/musical-review/TASKS.md#mr2), both parts) | Built. Level with no keys: M14 is +4.4 dB over the median, M15 -11.8, M16 -3.7, M08 +3.2 (the old M14 and M15 are replaced) |
| The multis' data pass ([MR3](audits/2026-11-09/musical-review/TASKS.md#mr3)): pads an octave down in TINE DUB and DEEP 332, drones at a house clock and 5 dB louder, swing on ORGAN SKANK, FINAL REEL's brass keeps its octave with a gentle low cut, PAD SPLIT and WHISPER TIDE 5 to 6 dB quieter with a chord held | Built (register overlap of TINE DUB / DEEP 332 not re-measured) |
| The reel: fifteen new factory multis as one curated programme, a genre each (M01 BASIC CHORD (dub techno), M02 DUB CHORD (dub techno, rebuilt over a held pad), M03 JUNO DEEP and M04 LATE RHODES (deep house), M05 ORGAN HOUSE and M06 PIANO HOUSE (classic house), M07 DETROIT STAB and M08 HI-TECH SOUL (Detroit), M09 HYPNO 303 (hypnotic techno), M10 TWO 303s (acid), M11 ACID LINES (acid), M12 ROBOT FUNK and M13 DEEP SEA ARP (electro), M14 WARPED TAPE (IDM), M15 BERLIN RUN (Berlin school), M16 TWO LOOPS (ambient)). The night's six genre scenes, ORGAN SKANK, WARM ECHO, TINE DUB, DEEP 332 and the owner's four pads left the set (git history, fa8d5c0) | Built on the computer: notes, rhythm cells, registers and levels match the design; End B set by hand. Not on the unit yet: the CPU sweep decides (heaviest by the model: M06 PIANO HOUSE, M04 LATE RHODES, M05 ORGAN HOUSE), and the ear decides the organ, the piano, the acid line and the tape wobble |
| HOME page 1 is the hands-on page: K1 MORPH at half the turning, K2 FILTER, K3 ECHO, K4 LENGTH on the slim row (b8: K3 RESONANCE, K4 ECHO, no LENGTH); BOTH moves each part by the same steps; OCT+ held + K1 rolls a new End B; OCT- + a knob puts it back; AMOUNT is a TIDE SETTINGS row, not dimmed in JOURNEY | Built (host tests, device build); on the unit since b10-rc2 |
| Loading one of your own sounds, a multi or a pattern no longer cuts notes the other part is holding ([FR6](audits/2026-11-09/firmware-review/TASKS.md#fr6)). Second-core code: its own FM-1 check. b8. | Built |
| Pattern polish: a big tap-tempo jump doesn't flam or knock the arp off the grid (the rest of [FR13](audits/2026-11-09/firmware-review/TASKS.md#fr13)). | Built |
| Load UNDO (OCT- held + PRESETS) and AUTO TIDE's HOME bring your saved sound back with its own End B ([FR10](audits/2026-11-09/firmware-review/TASKS.md#fr10)) | Built |
| Knob locks recorded on an ARP SEQ part play on the step the arp plays ([FR9](audits/2026-11-09/firmware-review/TASKS.md#fr9)) | Built |
| SAVE held with BOTH on two sounds from the read-only places saves each into its own empty USER slot ([FR12](audits/2026-11-09/firmware-review/TASKS.md#fr12)) | Built |
| After a save cut short by a power cut, RESTORE no longer brings the old sound back ([FR14](audits/2026-11-09/firmware-review/TASKS.md#fr14)) | Built |
| tools/build.py --release refuses to build with a developer switch on ([FR15](audits/2026-11-09/firmware-review/TASKS.md#fr15)) | Built |

The owner's answers to the musical review (9 November) are built in: the default for each of the nine questions, with the first start staying M02 DUB CHORD and FINAL REEL's brass pad keeping its octave.


## On the unit: built in b8

On the unit since b10-rc2 (12 November).

A polish beta on top of b7 (details in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b8)); all of it has been on the unit since b10-rc2.

| What | State |
|---|---|
| ARP SEQ parts stay on the beat on the internal clock: on a long set the arp no longer drifts ahead of the bars and the click, at any tempo and after a tap tempo | Built |
| HOME page 1's knobs are now K2 FLT, K3 RSN (the filter's resonance, on every engine), K4 ECH; LENGTH is dropped (owner 2026-11-11); OCT- + a knob still puts it back, BOTH moves each part by the same steps; test `morphui` t_home1 | Built (host tests, device build); on the unit since b10-rc2 |
| SAVE after JOURNEY says what it saves: "SAVES THE LOADED SOUND / KEEP FIRST = AS HEARD" above its question (what SAVE stores did not change) | Built |
| ARC's new phase starts with its own levers: the breakdown's first beat is thinned, the peak's first loop gets no POLY stretch, as the rest of the phase does | Built |
| The AUTO TIDE look-ahead test is back on factory multi slot 9, also over several ARC DROP arcs: no step shown that does not play | Built (no mismatch found) |
| The ACID and ACSQ waves on the ANALOG engine play accents like a TB-303: a short sweep (about 200 ms), a level punch of about +9 dB, accents in a row stacking higher, and a soft RC slide (90 % in 53 ms, 99 % in 106 ms); checked against Open303 | Built, to set by ear on the unit |
| Three acid multis: HYPNO 303 (slot 9, square 303 against the bar), TWO 303S (slot 10, two distorted 303s left and right), ACID LINES (slot 11, an acid-house saw line) | Built |
| Under the hood: the TURNS leftovers are out, HOME's old MODE page leftover is out, and the `glidecpu` test's bound is 10 % (it failed on b8 before, not from a sound change) | Built |

## On the unit: built in b9

On the unit since b10-rc2 (12 November).

The sound and knobs beta on top of b8 (details in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b9)); released as 1.0.0-b9 (11 November) and on the unit since then.

| What | State |
|---|---|
| The delay's echo is duller and cleaner everywhere: the first repeat now leaves through the COLR filter like every later one, and the loop's low cut moves from about 165 to about 219 Hz | Built |
| A TIME change glides instead of clicking when the delay's wobble (MDEPTH) is on: the echo sweeps to the new length like a tape echo, at most about 12 % off pitch; a big step takes a second or two | Built |
| BASIC CHORD and DUB CHORD re-voiced for dub space and weight: longer, duller, wobbling echo, a plate wash on DUB CHORD, tape on the stab, and a low held minor-7 bed with its root an octave down under both | Built |
| The other 14 multis re-voiced as one curated set (house and Detroit M03 to M08; acid, electro, IDM, Berlin and ambient M09 to M16): held beds, longer wider echoes, no semitone clashes, one genre each | Built |
| The drone multis get a soft sub an octave under their notes; the "leave the low end free" cuts are undone | Built |
| The factory multis' names start with their genre: DUB, HSE, TECH, ACID, ELEC, AMB (DUB BASIC ... AMB LOOPS) | Built |
| HOME page 1's knobs 2 to 4 play a genre set (dub: SND / FDBK / TIME, house: FLT / PMP / REV, techno, acid, electro, ambient; STD for a single sound); TIDE SETTINGS' KNOBS row (AUTO reads the name's first word) replaces ROLL TYPE | Built |
| Every new End B is rolled MUSIC; ROLL TYPE (SMART, WILD) is retired | Built |
| AUTO TIDE's MODE is EVOLVE or TRAVEL; old ONCE, LOOP, DRIFT and WANDER open as EVOLVE, JOURNEY as TRAVEL | Built |
| Parameter units stored as an index, 20 to 16 bytes a descriptor (SP4); records saved before b8 load as saved (the store fix) | Built |
| Tests and tools follow the new records and widths (round 3 and 3b: multiv8, ownerimport, strum, budget and deadline re-based); the G1/G2 echo re-pins (dlyev, master, bassmono, comp, mode, pan, perflevel, reverb) | Built |
| `benchsim`: M17 DUNE HORN does not render the same forward and backward (its HORN SWELL sub); a test-side finding, the sound is right | Open: the CI lane's fix is not in this build |

Closed from "Later": the parameter units as an index (SP4, below).

## On the unit: built in b10 (rc2)

- **COLR LFO (CLFO):** a new DELAY page knob, 0..15, swings the echo's colour darker and brighter at a slow 0.25 Hz; 0 is off, and DUB BASIC and DUB CHORD use 5. Multis store it.
- **ELEC GRT and AMB DRF:** ELEC's GRT also moves the cutoff, so the odd harmonics span about 10 dB (was 2 dB); AMB's DRF reaches its full filter wobble in about 9 detents (twice as fast).
- **Six new factory multis, M19 to M24:** TECH HYPNO, DUB HOUSE, HSE LOFI, TECH CINEMA, AMB TECHNO and TECH MINIMAL (24 factory multis in all).
- **Developer tools:** `factory_multis.py verify` and `drone_pack.py verify` fail on any byte drift from the committed records M01 to M24; `multi_patch.py keep-amount` is removed.
- **Developer tests:** a generated table of the factory multis' names (`build/gen/multi_names.h`) that the C tests read, so a rename cannot break them; `tests/bench_run_test.py` fails on a suite that spells a factory name.

## Built in b10-rc3, waits for the FM-1

- **ARC first beat:** the first beat of a new ARC phase plays at the new phase's loudness (louder peak, softer breakdown), not the old level.
- **Hot-code check:** it now follows function pointers, so an engine voice function left in flash is no longer invisible; 15 engine hooks are allow-listed as event rate. No image changes.

## Tooling: faster work (owner, 11 November)

Built or being built in b9: **the sound lab** (`felucca-base/tools/soundlab/`: render, measure, sweep, rules, the sound atlas and `genres.json`; it replaces about 80 one-off scripts). Next, in order:

1. **Fast test runner:** run groups in parallel (as many as memory allows), and a `--changed` mode that runs only the groups a change can affect (a map of which source files each group compiles; today every group compiles the whole firmware from scratch). CI shards run on after a failure and list every failure in one summary. Target: the full local run under 20 minutes, a typical change checked in about 5.
2. **Re-pin helper:** given a cause commit, find the changed reference results, prove each by putting the cause back, and re-pin with the cause written in (today: slow manual work per failure).
3. **Merge helper:** merge a list of branches, regenerate the generated files (level table, factory records, reference renders) instead of hand-resolving them, then run only the affected groups.
4. **The emulator in testing:** drive the web emulator (the `emu` CI build) with scripted sessions (key presses, knob turns, page changes) to check screens and behaviour as the player meets them, with pictures for the "look first" rule, and audio captured for the sound lab.
5. **A spec template per job type** (sound, firmware, docs) with the standard checks built in.

Status of 1 to 3 (b10): built and measured (`jam/check.sh -j N | --changed`, `jam/repin.sh`, `jam/merge_branches.sh`; [jam/README.md](../jam/README.md)). Full run on WSL: 51 min serial, 26 min at `-j 2`, 8 min with a warm cache; CI shards 2.5 to 13 min. Two things stay open:
- **`--changed` cannot get under 5 minutes for a firmware edit** (16.5 min at `-j 3`: a change in `fx.c` selects 177 of 192 groups). Every group compiles the whole firmware as one unit (`tests/hostsim.c` includes every source), so any firmware file is an input of nearly every program. The fix is per-module host objects: compile each firmware file once to an object, link the groups against the objects, and let the group map name objects instead of one translation unit.
- **The merge helper's factory / drone record regeneration has not run on a real conflict.** The rc2 replay and a `golden.txt` conflict were exercised; `fork/b10-fw` (M01 / M02 records) and `fork/b10-new-multis` (M19 to M24) touch different record files and merge without a bin conflict, so the `factory_multis.py build` / `drone_pack.py` path still waits for the first conflicting pair.

## Later, in order

1. ~~ARP SEQ parts drift against the beat on the internal clock: the arp's step period and the sequencer's beat round differently, 12 to 14 samples a bar on M02 at 117 BPM, about 107 ms after 400 bars (found by the ARC lane with a host probe). Not measured under an external clock, where both follow the clock grid and probably do not drift.~~ Built in b8 (DRIFT: arp, tracks and click run through one exact beat accumulator); checked on 12 November (b10 round 2) with `arpdrift`: M02 at 117 BPM, 400 bars, drift 1 and 0 samples at every row (a constant, no growth); the external-clock path is unchanged (`clockdrift` passes).
2. ~~A new ARC phase's first downbeat still plays with the old phase's level and voices (the main loop sets the levers after the bar turns; the drop's silence is armed ahead and is exact). One step per phase change; about 40 B to arm the levers a bar ahead.~~ Built in b8 (A2, the levers armed a unit ahead, `tarc_lev` bits 3-5); the b10 probe (`arc_levers` in `tidesteps`) reads 0 late levers on M02 and M06. The loudness step was the part left: see 25.
3. ~~`factory_multis.py build` no longer reproduces the committed End B AMOUNT bytes~~ Closed (b10 tools lane): the drift was already gone since b8 (the sounds' End B amounts match the shipped records; a rebuild from source gives zero byte diff on M01..M24, also after a record is damaged first). `multi_patch.py keep-amount` is retired, and `factory_multis.py verify` / `drone_pack.py verify` (in the `sounds` group) fail the build if a rebuild ever differs from the committed records again.
4. RAM headroom: the b7 build had only about 8 B above the build's 4,096 B rule at the start of the night (4,104 B free); a change that adds RAM will fail the build. Re-measure the merged image.
5. ~~Cleanup after TURNS~~: `sqm_turns` and `tarc_fast` were removed in b8 (`5a3e086`); checked again on 12 November (b10 round 2): no symbol of them is in the rc2 image.
6. TIDE range overshoot and the tideroles allowances: decided per case in b10 round 2 (`fork/b10-tide2`). The overshoots (M01 +4, M04 +2, M07 +5, M08 +3) were a TIDE bug: on an ARP SEQ chord part, a VOIC octave on the step and PEAK's open voicing of the arp chord added up past the lane (seq_voicelead.c sqm_varp, fixed). M15's -2 was the test reading 8 bars of a 16-bar loop (fixed). Left, musical, reasons in `AL[]` of `tests/tide_roles_test.c`: the stab anchors of M01, M02, M21 (bar 4 of 4 is CALL AND ANSWER's variation bar) and M16's AIR onset (one bell moved half a note). Done when tide2 is merged.
6. ~~TRAVEL keeps the multi's colour~~ b10 tide3 (`fork/b10-tide3`): a part as a factory multi loaded it travels only to sounds in its Original's brightness octave whose level its TRIM can reach; if there is none, it travels between its own two ends. 8-bar brightness under TRAVEL against EVOLVE alone: M02 x2.6 / x2.6 (the rest is EVOLVE's own), M12 x1.3 / x1.3 (was x2.1, and half as bright), M13 x1.4 / x1.2 (was x1.9, its mid-trip dips gone). A sound loaded alone walks as J1.
7. ~~Two multis for the missing roles, a sparse figure with a texture and a keys house stab ([MR9](audits/2026-11-09/musical-review/TASKS.md#mr9))~~ b10 round 4 (`fork/b10-mr9`): M03 DUB MINIMAL (a blip figure into the echo over tape hiss, STYLE MINIMAL) and M07 HSE KEYS (parallel minor-seventh keys stabs over a held organ, STYLE DEEP), 26 multis, +764 B. Left: the roles rows T8 to T10 (the new reel and its `tideroles` levels).
8. On the next test-mode sweep on the unit: read core 1's stack peak with a TRAVEL crossing and WASH on, and record it in docs/TWOCORE_STAGE1.md (the rest of [FR15](audits/2026-11-09/firmware-review/TASKS.md#fr15)).
9. ~~Hand-moved morph saving: a hand or MIDI fader move blended every other block like a GLIDE (about 70 us a block saved). Finding 12, D44.~~ Built behind a switch in b10 round 4 (`fw4`): `FELUCCA_HANDSKIP=1 python tools/build.py` (`morph.c`) gives a hand- or MIDI-moved fader a blend every other block (host: 50 blends in 100 blocks of a stepping fader, 100 off; the fader jump still blends at once; host group `handskip` builds both). **Off by default**: the image is byte-identical; on it is 4 B smaller. Still to do: the unit sweep (a fast twist during dense play; the 0.73 ms trail listened to), then make it the default.
10. ~~Transition costs only while moving: the engine cross-fade, valley and wash charged only while a transition runs, so more notes play at rest.~~ Built behind a switch in b10 round 5 (`fw5`): `FELUCCA_MOVECOST=1 python tools/build.py` (`core.h`, `budget.c` `BD_AT_REST`): the cross-fade, valley and wash are charged only while the part's fader moves or a GLIDE runs (`bd_mv`, the FM6 bridge's rule). Host (`movecost`): M02 part 1 with WASH MID, tide 60 -> 0 and misc 103 -> 43 cost units at rest. **Off by default** (byte-identical); on, +76 B. A jump over 12 fader steps is not a move in `bd_mv` (**fixed in round 6**: `bd_where` also charges a jump, a fader parked inside the arc or between its ends, and the wash's meter; [BUDGET_HONEST](BUDGET_HONEST.md)). Turning it on re-pins `budget` (item 142 reference, A01) and `deadline` (the TIDE scene) through `jam/repin.sh`.
11. ~~FM6 crossing fixes (findings 8, 9; brief K7) and Felucca #265 (signed DSP arithmetic, bit-exact).~~ Checked and built in b10 round 4 (`fw4`): finding 8 was built already (`budget.c` charges the crossing when the glide or fader arms, `bd_mv` with `XN_FM`); finding 9 fixed (`fm6_block`: a stand-in drain waits while a new crossing runs and still runs after it; probe in `tidecrossings`); K7 built (the part's LFO terms once a block and part, `fm6_lfo_terms`; bit-exact, 6,144 term combinations equal the old formulas; +36 B with finding 9 and 152 B less hot code, the saving per voice is about 0.15 % of an FM6 voice block, not the few % the brief expected); Felucca #265 is in since `6bef0a7` (the DC block, the FM6 velocity and LFO scaling, the phase additions, PHYS out; `ubsanrender` passes).
12. Free 276 B of RAM: the SEQ MORPH history UNDO no longer reads ([R1](audits/2026-11-09/tide-morph/TASKS.md#r1)). When a feature needs RAM.
13. ~~Two small interrupt savings in the crossing step ([C1](audits/2026-11-09/tide-morph/TASKS.md#c1)), with "Transition costs only while moving".~~ Built in b10 round 5 with Later 10: step 1 (the dead `k` store) is a 0 B cleanup; step 2 (the INSERT dip in 1/256 steps) is under the same switch. Host: with an INSERT difference forced on M02, a hand jump 0 -> 127 blends 278 times in 1.5 s instead of 351 (1.26 times fewer; the brief's 3 times is not reached: the dip's ramp is steep).
14. ~~WASH on: no jump when a glide is stopped by hand, one dip instead of two stacked ([W1](audits/2026-11-09/tide-morph/TASKS.md#w1)).~~ Built in b10 round 4 (`fw4`, W1 steps 1 and 2): a stop fades the breath and the swell by time (1/32 of what is left a millisecond; per pass would be gone in a millisecond, the passes run far faster than the audio blocks) and the breath and the transition dip no longer stack (the lower of the two); the effective cutoff after a stop moves at most 1 dial step a block (it jumped 18); WASH off unchanged. +132 B. Step 3 (the wash level at rest, `wref`) left as it is: letting it follow during motion would pull the fill down toward the dipped level.
15. ~~Docs fixes from the TIDE and morph audit still open (the dip's length, the DUB recipe's wash, the bass promise and the dynamics paragraph were written in the b7 guide pass): `TIDE_GUIDE.md:23` the MORPH knob 3 text (the wave picture is shipped); EVOLVE "a little further each trip"; `MOD_INERT_TABLE.md:21` (ANALOG: PW is offered only with WAVE PWM), and `INSTALL.md:60` / `QUICKSTART.md:41` (ERASE DATA erases the FM6 voice bank and the user tables), with FR11.~~ Built in b10 chores, after checking each line against the source. Already right, no edit: ERASE DATA lists the FM6 bank and the user tables (`INSTALL.md:117`, `QUICKSTART.md:119`; `erase_data.c`); the MORPH knob 3 text (`TIDE_GUIDE.md:320`, RESONANCE); the ANALOG PW row (`MOD_INERT_TABLE.md:21`, `eng_analog.c:274`, `ui_draw.c` hides PW unless WAVE PWM). Fixed: EVOLVE wording (`web/cheatsheet/index.html`, `web/landing.html`; the step grows with the phrase, `morph_music.c:15-16`, `:162`, `:171`); the 160 figure (`DUELL_1_1_REFERENCE.md:28`).
16. ~~Factory pattern table trimmed~~ (SP3): built in b9.
17. ~~Parameter units stored as an index~~ (SP4): built in b9.
18. Event-rate code built for size (1.78 KB; [SP5](audits/2026-11-09/space/TASKS.md)); needs the FM-1 CPU sweep. **Built behind a switch in b10 round 3:** `FELUCCA_SIZE_EVENT=1 python tools/build.py` builds the 203 "event rate" functions of `tools/ram_allow.py` for size (`tools/size_fns.py`) and the 43 helpers the size build stops inlining are allow-listed; image 579,444 B against 580,992 B (-1,548 B), hot-code check ok. **Off by default, so the package is byte-identical to the build without it.** Still to do: the unit sweep (chord-heavy and arp-heavy scenes, the 16-multi GLIDE sweep; 0 missed blocks, 0 cut voices, worst block not above b5's), then make it the default.
19. ~~FM6 MUSIC rolls at AMOUNT 40 leave the loudness window~~ ([finding 7](audits/2026-11-09/tide-morph/REPORT.md)): built in b10 (`morph_fm_shape.c`: the roll moves FM6's carrier envelopes once, through the AMP ENV knobs; 99.7 % within +-4.5 dB, was 69.7 %).
20. Give notes back more finely (per multi, or a smaller step than 76): at the first step, NOTES limit 76 (18 notes over the 16 multis), the unit's sweep missed no block but cut one voice on M14 EERIE SIGNAL, busiest block 90 % (b5 83 %); M08 TINE DUB 89 %. Each try kept only if the sweep stays clean.
21. Send only the screen rows that changed, so the wave page reaches ~16 pictures a second instead of ~11 (the SPI link costs ~44 ms a full wave redraw).
22. Before `1.0.0`: 0 missed blocks over the 16-multi GLIDE sweep (owner, 8 November); the CPU budget re-measured and made honest (findings 5, 6, 34; K9). **Host part done in b10 round 6** ([BUDGET_HONEST](BUDGET_HONEST.md), `sh host_tests.sh budgetcal`): the MOVECOST rest state fixed; findings 5 and 34 measured and written into `budget.c`, not folded (the PEAK fit already contains them); finding 6 measured at 3 .. 7 us warm, a timer (`st_budget_us`) added. **Left, unit only:** the six items at the end of that file (first: `st_budget_us` on the GLIDE sweep; over 20 us -> a snapshot from `budget_poll`), then K9's `BUDGET_PEAK` re-fit together with folding the four 5 November rows in.
23. ~~AUTO TIDE's step display can show one hit ahead that then doesn't play~~ (picture only): built in b10 (b10/fw-b). Cause: the page's redraw hash (`tide_sig`) did not see `morph.seed`; a ROLL sets the seed at once and CALL AND ANSWER's listening bars (and POLY) read it from the audio side, while the ends' key (`tseed`) follows from the main loop, so a look-ahead drawn just before the roll stayed on the screen until the playhead moved. Reproduced only with the old M09 MINIMAL 12 record (reel commit `2de0f78`, gone with the reel's re-voice `e2f8c07`); setting the old seed back reproduced the old view in all 4 cases. The seed is in the hash now (+4 B); `tidesteps` probes it (a seed alone changed 6,426 steps with both parts leading; the hash stayed the same 0 times, 103,451 before) and plays three more 12-step ARP SEQ loops.
24. SAVE after TRAVEL: SAVE shows a short message when the sound you loaded and the one TRAVEL moved to differ (owner, 10 November: the default).
25. ~~ARC's loudness step (sqm_dyn) still follows the main loop's pass, so a new phase's first beat keeps the old loudness (B8-ARC).~~ Built in b10 round 2: `sqm_dyn_nx` is armed with the levers and the audio side takes it from the unit's first bar (`tarc_levers`, `sqm_dyn_of`); the `arc_levers` probe read the step late at 4 of 11 unit starts before (first at bar 16), 0 after. About +40 B.
26. TRAVEL level: b10 tide2 + tide3 (`fork/b10-tide3`): the destination plays at the part's level as the multi plays it (`firmware/src/jrny_mu.h`), and a destination it cannot reach is not picked. 22 of 24 multis inside -24.1 .. -18.1 LUFS (M19 -30.1 -> -21.8, M17 -24.3 -> -19.6). Left: M24 -17.7 (BRAZEN, ATK 43, reads quiet in the J1 table's 56 ms stab gate and plays louder in M24's full 16ths; next step: J1 measured with the part's own gate), M05 -24.2 (0.1 under: a pad to keys swap, two patterns, no match).
27. README's second picture (`docs/tide/morph-settings.png`) still shows the retired ROLL MUSIC / ARP / PITCH rows: shoot a new one of TIDE SETTINGS with the emulator (b9 chores).
28. Stale comment: `ui_input.c` above `part_select` still describes MORPH SET's SMART / WILD roll knobs, retired in b9 (b9 chores).
29. Two low cuts from before the "low end welcome" rule were kept "for another reason" and never decided: the C3 pitch-walk floor and the pad lows in M04, M12 and M16 (B9-LOWEND rows 9 and 14). Check them with the sound lab and open them unless the reason holds.
30. The store's cross-version test has no record written by b8 rc1 / rc2 (B9-STORE); add one to the fixtures.
31. USB AUDIO OFF: the web editor's mock unit (`web/editor/mockunit.js`) has no USB AUDIO item, and a Mac host seeing the MIDI-only device was never tried (Windows is proven by every flash since b9).
32. The Attack Magazine research left about 25 relevant articles listed but unread, and the Deconstructed house / nu-disco ones (listed in the b9 research index, "unread but relevant"): read them before the next sound round.
33. The `budget` group (item 142 reference hash, A01 rules) and the `deadline` group (TIDE scene) only run with the MOVECOST switch off, but the shipped build has it on (`tools/build.py` default 1): built with `-DFELUCCA_MOVECOST=1` they fail 3 and 1 checks because their expectations are the off-state's (a part with WASH on at rest pays only the meter; valley, wash and cross-fade are charged only in a transition). Give them an ON build in `host_tests.sh` with ON expectations (re-pin through `jam/repin.sh`); found in b10 round 6.

## Released

| Version | Date | What you got |
|---|---|---|
| 1.0.0-b6 | 9 November | b5's MORPH wave picture and PART 1 / PART 2 / BOTH, then the WAVE LAB stack with depth at an even pace, two drone scenes with sequences (M17, M18), AMOUNT kept on factory loads, JOURNEY close in level, notes in your register, FM6 End B levels, a 20 MHz screen link, 4.8 KB of flash back ([release](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/tide-1.0.0-b6)) |
| 1.0.0-b4 | 8 November | No notes cut when the CPU runs hot (ease ladder), less CPU while a part is silent, STYLE DRONE, screen cleanups |
| 1.0.0-b3 | 8 November | CPU and missed-block readout, patch list order, live recording into ARP SEQ, HOME without the MODE page |
| 1.0.0-b2 | 8 November | Second beta |
| 1.0.0-b1 | 5 November | First beta |

1.0.0-b5 was not released separately: its changes are in 1.0.0-b6. On the unit before the b6 tag (9 November, test-mode build, MORPH on screen): no missed audio blocks and no voices cut over the 16 multis, busiest block 85 % (b5: 83 %); the drones 87 % and 67 %.

Stable: `stable-2026-11-03` (`187e9d0`), the last build you validated. The first numbered stable will be `1.0.0`.

**Quick reference:** [TIDE settings at a glance](TIDE_QUICK_TABLE.md).

## How we work

- **Betas carry what is ready.** Several audio changes may ship in one beta (owner, 8 November: delivery first); each one is still measured on the unit before it ships.
- **Measure before and after.** An audio-path change is measured on the unit before and after, with the same script, against a pass mark written down first.
- **Builder and four reviewers.** Each change is built by one agent on its own branch, then read by separate reviewers: correctness, quality and cohesion, tests, docs. The coordinator decides the fixes; a verification review checks them.
- **Pull requests.** Every change goes to GitHub as a pull request against `jam11`; GitHub runs the full host suite on it. Locally only the device build (size, hot-code check) and `check_docs.py`. Builders work in parallel; merges go one at a time, with the owner's OK.
- **Never flash red.** A tagged beta is never flashed while the full suite is red. The tag comes before the build; the flashed commit is merged and pushed the same day.
- **Plain DUELL is no longer kept identical** (owner, 8 November: no upstream merging). Shared code may change when that is simpler or smaller; there is no plain build any more (retired in b10). The `partindep` pins are never regenerated.
- **Formats and user data are kept** (patches, patterns, multis, settings, KEEP / UNDO) unless the owner decides otherwise.
- **No new pages.** TIDE options are rows on TIDE SETTINGS with good defaults; restyles keep every fader bar, note and shown value.
- **Technical choices are the coordinator's**, with audio quality first, then risk and effort. The owner decides what is heard, seen and touched.
- **A new store keeps its sector tails erased.** Any new object in `0xE0000..0xFBFFF` leaves the last 256 bytes of each sector erased: the update loader treats a sector tail that passes its record check as a leftover update and erases the sector. This applies to U4 (24 multis) and to the reserved `0xEA000..0xFBFFF`.
- **Anything unfinished gets a row here at once.**
- **Pushes and releases need the owner's OK.**
- **Look first:** a visible change is shown to the owner on the unit (or emulator) and approved before tests and paperwork.
- **The low end is welcome** (owner, 11 November): bass and sub may be in any multi or sound. The one rule: the FM-1 does not take over the drums (no kick-like thump on every beat, no drum-kit role). Nothing else limits the low end: no thresholds under 120 Hz, no caps, no registers kept free. The "leave the low end free" marks of the 9 November musical review ([audits](audits/2026-11-09/musical-review/REPORT.md)) were a suggestion for a few edge cases, never a rule.

## Planned, not yet scheduled

**Live performance assist** (owner, 12 November: "recording and overwriting, or add complementary logic to the voice not being played ... store a melody and it'll add a complementary bass in the same key, or a question answer logic, dubbing it on the other voice"). In design: one coherent system for a DAWless live player, phased by value and cost (flash / pool room comes from the b10 screen overhaul):
- Capture what you play, loop it, overdub / replace, undo (builds on keys recording into ARP SEQ and REC ADD / REPLACE, and the live-record audit's open items).
- The part you aren't playing responds: a complementary bass in the same key from a stored melody, question / answer (extending b7's call and answer), dub-style doubling or answering on the other voice.
- Design study first (the coordinator's `PERF_ASSIST` brief); the owner picks the player-facing choices before it is built.
- **Phase 1 built in b10 round 8** (`fork/b10-perf1`, `firmware/src/tide_take.c`; owner 12 November: BASS LINE the default answer, DOUBLE a third above, no CAPTURE): a REC pass that wrote something is a **take**; the first take on an empty part sets the loop (1, 2 or 4 bars); TIDE SETTINGS `ANSWER` (OFF / BASS LINE / DOUBLE, fresh unit BASS LINE) and `ANSWER ON` (OTHER / OFF) write the answer into the other part when a take closes; OCT- held + a PRESETS turn goes back (left) / forward (right) over the last 2 takes or rolls (`UNDO TAKE n`); the existing ROLL on the answering part writes the answer again with the roll's seed, one undo step (`ROLL n  BASS LINE  A MINOR`); an armed part plays its own notes. Key: SCL, else the take's own guess (`?`). Host groups `take` and `answer`. **Costs:** flash +6.0 KB (the brief said 2.5; the pi32 code is 2 to 3 times the host's size), pool +5.7 KB, `.bss` +0. **Not tried:** the unit (REC on ARP SEQ and ADD / REPLACE are still unproven there); the two-multi renders (the emulator played one multi: peak -17 dBFS, rms -32.5 dBFS with the answer).
- **Phase 2 built in b10 round 9** (`fork/b10-perf2`): REPLY and ECHO as ANSWER choices (the loop doubles, a call is 2 bars at most, ROLL works on them: `ECHO` throws the last note 4 times, 100 / 80 / 60 / 40; `REPLY` moves the call in scale steps and ends on the tonic); the **key per multi**, `KEYS` in `tools/factory_multis.py` read by `gen_duell_multis.py` into `DF_KEY` (one byte a multi, 26 B; **not in the multi record**, which is unchanged: a user multi has no key until SCL says one; keys were read off each multi's patterns, thin beds and drones have none); the **REC state of the tide line** (red, filling with the loop; red dashes in the count-in; knob 1: `TAKE  BAR 3/4`). Flash +1.13 KB on phase 1 (the brief said 1 KB), `.bss` +0. Not asked, phase 3: the answer following every overdub pass.
- **CAPTURE: owner said no** (12 November), not later either.

**Full design overhaul: TIDE as its own instrument** (owner, 12 November: a separate, standalone topic, not now). b10 brought the TIDE colours, splash and tide line (direction A of the design consult (kept in the workspace notes, not shipped)); the shapes are still DUELL's, and the DUELL logo and artwork are not under the GPL. Three parts, in this order:
- Off the unit: the website, installer, emulator page, release and package names (`duell-tide-…`) and README say TIDE, with the credits to DUELL and Felucca kept.
- A TIDE home / performance page built around time: where the tide is, the ARC phase, bars, and live assist's take and answer on one screen.
- The shape language: water-line fills, wave-edged knob bars, a TIDE header on the global pages, one icon style, replacing DUELL's cards and bars.

Each part is proposed with close-ups and approved by the owner before it's built.

**Screens** (redundancy audit 8 November, all 433 screens rendered)
- TIDE rows SYNC and ROLL renamed so they don't share words with tempo sync and MORPH's ROLL. (The rest of this audit was released in b4.)

**One view for both parts** (owner, 8 November): AUTO TIDE and the MORPH / wave page always show both parts, with the selected part clearly marked (full colour, the other dimmed, plus a cue that works on MONO and GREY). The wave page and AUTO TIDE do it in b5 (see Now).

**RAM room** (b5, Ready): range Z has 9,140 B to the stack guard against the 8,192 B rule (b7 build); a test-mode build keeps the lowered 7,936 B rule. The next change that grows range Z or the pool measures against that.

**MORPH** ([review](audits/2026-11-08/morph/REPORT.md): keep the core and the page, no big rework)
- c3 (b6, Ready): factory loads stop resetting AMOUNT; K4 dim in JRNY (the review of 9 November found AMOUNT still shapes hand rolls and JOURNEY's fallback roll there: the dim comes out with FR11). Branch `fix/b6-morph-c3` `148d84b`, tests pass; rebase onto the reworked MORPH page before b6.
- c2: a hand fader resting in the middle (40-60 %) between ends that differ in a switch stays dulled, FM6 on pure sines: after about 1 s at rest, settle onto the nearer side (about 100 B, audio interrupt; listen on the unit first).

**CPU and audio engine**
- Move the 44 reviewed RAM candidates of `tools/ram_allow.py` (~13.7 KB, ranked in [REVIEW_2026-11-12.md](audits/2026-11-08/flash-hot/REVIEW_2026-11-12.md): the always-on per-sample stages first, by executed bytes) into the hot block / range Z as the room allows, ~~and take `morph_xrest` (92 B) and `vs_block` (168 B) out of the always-on set with a guard that skips them when their state is already at rest~~ (**built in b10 round 7**, `FELUCCA_GUARDS`, ON by default, `FELUCCA_GUARDS=0 python tools/build.py` turns it off if a glitch is reported: `morph_xrest` runs 2 times in 464 calls and `vs_block` 49 in 6,368 over the 24 factory multis and bursts; renders bit-identical with the guard on and off, group `guards`; flash +212 B on, +44 B off, RAM +0: .data+.bss 69,536 B as before). After the unit sweep: the gain of a flash line fill is still unmeasured (flash-hot REPORT section 1), so the sweep decides how many to move.
- Effect code from slow flash: [analysis](audits/2026-11-08/flash-hot/REPORT.md) done. Of the 116 functions, 50 run per sample. Plan: first a test-mode probe that times one function from flash and from RAM (no sound change); only if it shows at least 0.5 % of a half, move ~0.9 KB (`dly_wr`, `master_knee`, `acc_sum`, `cho_zrun`, `fx_swing`, `rev_front`, `hall_gains`) and later ~1 KB more, as its own beta measured on the unit. Expected gain 1 to 3 %, not enough alone for `1.0.0`.
- The budget model (`bd_part`, `bd_global`, `bd_slope`, `part_dl`, 2.1 KB) runs from flash at the start of every half: run it only when its inputs change. Larger gain than the above, but it changes behaviour; needs its own design and test.
- K8: the next fader position's blended parameters prepared outside the audio interrupt (main loop or idle core 1) and swapped in by pointer. Large; needs one-writer rules.
- K10: the dearer part (by measured cost) on core 1, not always part 2.
- K11: a TIDE-only 5.8 ms half for background use, costed first (keyboard play keeps 2.9 ms).
- K12: the "prove the CPU cost before flashing" process written into CONTRIBUTING and TESTING.
- ~~Two readers of the arp step ignore a POLY stretch~~ (`ui_draw.c` foot_playhead, `seq_dialogue.c` phrase_peer_attack): checked in b10 (b10/fw-b), no change needed. The footer's playhead already maps the arp step through `TIDE_ARP_IDX` (a stretch included; `f36` pins it equal to `tide_play_idx` over 16 arp steps inside a stretch, with a key held, none held and the arp not started); the dialogue reads the raw `arp_idx` but declines the whole prediction while either part is in a stretch (`tide_poly_on`, `f36` runs `phrase_peer_attack` with each part stretched). `P_SLEN` is 1..`NSTEP`, so `foot_playhead`'s length and `tide_poly_full`'s are the same. `f36` and `tidedialogue` PASS.
- Code nits with no measured effect (finding 35): `DUELL_PROBE_STAGES` defined outside `#if DUELL_TIDE`; `morph_xside` into RAM only if the unit shows its cost; `glide_stop` lags one poll.

**TIDE interface**
- LFO DEPTH destinations shown as "LFO1/2 LVL"; the mod wheel keeps driving MORPH.
- A legend while OCT- is held on the TIDE page (C9, Later).

**Content**
- Roles spec T4 (b6, Ready): branch `fix/b6-roles-t4` (review MERGE, +316 B, audio interrupt untouched); merge it for real into b6, never as a patch of the diff against b5/integ (the branch predates the flash screen). Cleanups then: TIDE_GUIDE.md:153 wording (the floor is min(lowest, C3)), the `t_lane` edge check is met for free in the bass scene, the unused `sqm_note` (seq_morph.c:89) and its comment at :60.
- Roles spec T7: dropped with CELLAR TAPE (replaced by M03 ACID FIELD in b7).
- Roles spec T8 to T10: the new reel of multis, together with the `tideroles` levels (43 of 338 checks red: M04 +3.8, M10 +5.3, M12 -3.1, M15 -7.2, M16 -9.2 dB from the median; finding 25, D46).

**Live recording** ([audit](audits/2026-11-08/liverec/LIVEREC_AUDIT.md))
- Documented limits of the keys-record-into-ARP-SEQ pattern (Released in b3): a 2-step pattern in REPLACE splits a chord across the step boundary, and the key's step is predicted from the POLY stretch before the next arp step runs (it can land one step off at a stretch change). [Brief R2](audits/2026-11-08/liverec/TASKS.md#r2)
- ~~Named `tide.h` hooks for the raw `#if DUELL_TIDE` blocks in `seq.c`; a `seq_len()` helper; one "past half step" helper.~~ Built in b10 round 3 (`fw3`): hooks H20 in `tide.h`, `TIDE_LIVEREC`, `seq_len`, `past_half` / `before_half`; `seq.c` keeps one `#if DUELL_TIDE` (the include of `tide_seq.c`). The TIDE image is byte-identical.
- ~~Dead `SC_MODE` code under TIDE~~: checked on 12 November (b10 round 2): nothing to buy back. The MODE page's draw, graph and knob code is under `#if !DUELL_TIDE`, and `TIDE_MODE_PG` / `TIDE_GRID3` are constant 0 in TIDE, so the compiler folds the rest; the rc2 image has no `graph_mode` and no `CG3` symbol (`mode_knob` stays: TIDE SETTINGS calls it).
- ~~A small per-row kind table for TIDE SETTINGS (replaces the row comparisons in `tide_ui.c`).~~ Built in b10 round 3 (`JL_KIND`, +4 B).

**Hot code check (K6)**
- ~~K6 follow-up (review finding H1): `tools/hot_reach.py` follows function pointers~~: built in b10 (tools-b). An edge for every address-taken function and for every function pointer in a data table a reached function points to (ENGINES -> engine_t -> note_on / desc); 15 engine hooks were allow-listed as event rate.
- ~~K6 follow-up: 135 allow-list reasons in `tools/ram_allow.py` say "mix or effect stage ... not yet reviewed" ...~~ Reviewed in b10 round 5: every reason now says per block / per sample when on / event and either "RAM candidate" (44, ~13694 B) or why it stays in the flash; list in [REVIEW_2026-11-12.md](audits/2026-11-08/flash-hot/REVIEW_2026-11-12.md). Nothing measured on the unit.

**Storage and backups**
- ~~U4, a flash slot behind every multi position (M17..M26).~~ Built in b10 (`b10/u4`): five new objects (ids 50..54) at 0xEA000..0xF3FFF, `MU_SLOTS` = `MU_SHOWN` = 26 (MR9's two multis), backup ids 42..46 (41 is the FM6 voice bank), caps 127, archive version 6. The web editor reads and writes the objects 42..46 too (Download / Rename / Import work on M17..M26 of a unit with caps bit 6).
- ~~Patch capacity in the docs: INFO says 128 slots, TIDE guarantees 126 worst-case records; replace "every slot always fits" with exact wording.~~ Built in b10 chores: the docs already say 126 (`KNOWN_ISSUES.md:47`, `EDITOR_LIBRARIAN.md:17`); "every slot always fits" is not in the docs; INFO now reports 126 with TIDE (`editor.c:585`, `upreset.c:252`); the 160 figure is fixed (`DUELL_1_1_REFERENCE.md:28`).
- ~~Pattern save: document the exact visible entry point (saving needs STOP) before any live test.~~ Built in b10 chores, with a correction: saving a pattern does not need STOP. SAVE held on PATTERNS goes to the slot confirm before the STOP check (`ui_input.c`, `patch_save_ask`); `ui_name.c:282` keeps a pattern in RAM while playing. Documented in `QUICKSTART.md:112`.

**Repository and CI**
- The `emu` CI job: fixed in [#4](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/4) (emsdk sourced under bash; merged 8 November), and the job gates.
- Rename `jam11` to `tide`.
- Gate the release channels by CI too (a Stable / Beta release is offered only from a green commit), from the 4 November decisions.
- ~~Retire the plain build~~ Done (b10 tools lane): `DUELL_TIDE=0`, `jam/check_off.sh`, `jam/off.sha256`, the `tideoff` and `multiv8` groups (and the 1.0 -> 1.1 record converter) are gone, and the `#if DUELL_TIDE` sides are folded; the TIDE image is byte for byte the one before.
- Test quality notes left from finding 38: ~~split the FM6 budget check by crossing state~~ and ~~one generated multi-name table~~ built in b10 (tools-b); the `F_NONE` hang exemption if it flakes.
- Workspace clean-up: retire merged worktrees (owner's OK).

**Memory savings, when a feature needs room** (estimates until measured; none is free)

| Where | Size | Note |
|---|---|---|
| FM6 lookup table | 4,100 B flash | A 257-entry table frees ~3 KB but changes pitch / gain math: an approximation to validate by ear. |
| Factory multi records | ~9.6 KB flash | Packing not measured yet (sound pairs saved 1,888 B the same way). |
| Delay line | 131 KB pool | A smaller line changes the sound. |
| Screen canvas | 59.5 KB pool | Drawing in smaller strips; every direct pixel access must be checked. |
| Performance capture | 32 KB pool | Check when it must stay live. |
| Bus B effect code (a second full copy of the reverbs, delays, SHIM and SPRING) | ~9 KB flash | One copy taking a bus pointer. It touches the effect loops in the audio interrupt: needs the bit-exact pins and a DUEL CPU sweep on the FM-1. 10.6 KB of `_b` symbols by the b4 sizes; re-measure ([review F2](audits/2026-11-09/firmware-review/REPORT.md)). |
| `midi_play`'s chord-keeping code, written twice | ~150 B flash | One helper called twice; unmeasured (review A7). |

## Waiting on you

| Question | Default if you don't mind |
|---|---|
| Try on the unit: keys recording into an ARP SEQ part, and REC ADD / REPLACE (Released in b3, not yet tried on the unit). | Nothing to decide; say if the nearest-step rule feels wrong. |
| D46: the levels of the multis, by ear on the unit. | The musical review's trims are in (PAD SPLIT and WHISPER TIDE 5 to 6 dB down, FINAL REEL and the drones adjusted). Listen to the set with a chord held and without keys; say which multi still sits too loud or too quiet. |

**By ear or eye on the unit** (no homework: noted when the owner meets them in use): the drones and their beds; the six electronic-genre multis; M16 FINAL REEL's switch bar on a TRAVEL; the hand-and-return of the MORPH fader; call and answer; the U2 multi sequences; the MULTIS / PATTERNS preview's readability; polymeter (12 / 16, 15 / 16) against a steady 4 / 4; the chord part's rhythm cells.

## Ideas, not decided

- Effects ideas from the sourced recipes (the b9 recipe research), each needing flash: ~~a slow LFO on the delay loop filter~~ (built in b10: COLR LFO); a noise source for the FM6 and insert sounds (Attack's dub-chord "chuff" and tape hiss, not possible today); a low cut or EQ on the reverb return; a phaser / flanger insert; a fast LFO to oscillator sync / PWM, plus tremolo / auto-pan; a two-tap delay (one synced, one free); separate L / R delay times; ducking the delay and reverb returns by the part's own notes; a per-bar delay-send ramp and a reverb accent on a phrase's last note; a slow swell LFO on a part's level; an oscillator pitch envelope. Low priority (owner, 12 November: "Effects are a bit less important I have other gear for that").
- ~~More factory multis~~ (built in b10 as M19 to M24: hypnotic techno, cinematic hypnotic techno, dub house, lo-fi house, ambient techno, minimal; researched from Attack Magazine technique articles in b9): about 0.5 KB each, room from USB AUDIO OFF and its int16 resampler taps (b9) and SP5.
- **From Niagara Drift** (an FM-1 drone firmware; checked against our code 8 November, estimates until built): **reverb FREEZE** on HALL / ROOM (the tail holds and the notes can stop; a block-rate gain change with a limiter, ~200-400 B, no CPU while off; risks: fixed-point overflow and the release click) fits best. **AGE** (slow tape-like wear, ~300-600 B) needs an offset layer on top of the saved sound, not checked yet. Gesture keys and a free-time motion loop do not fit the flash, the keys or the no-new-pages rule.
- **From the musical review** ([report](audits/2026-11-09/musical-review/REPORT.md)): the three ideas (a chord-rhythm vocabulary, voicing that breathes with the arc, a real call and answer) are built in b7.
- **Keep current patterns on MULTI load** (U2 route A): an explicit load that keeps both playing patterns.
- **Musical evaluations kept from the earlier plan** (no work scheduled): M2a chord movement; M5 / M10a level and slow-glide behaviour only for a found defect; M7d polymeter first by hand; CHANGE / ENERGY rows only with useful behaviour. Details in [design/TIDE_MUSICAL_DESIGN.md](design/TIDE_MUSICAL_DESIGN.md).

## Decided against, or kept as is

- WAVE table quality stays (flash). SHIM's PITCH DOWN and MOD stay; cut only if a TIDE feature cannot fit, and then the owner is asked.
- No real chord steps in ARP SEQ; no cross-engine JRNY; no automatic harmonic rewriting, loudness normalisation or harshness classification.
- TRAVEL may choose its next sound by level and brightness; the sounds themselves stay unchanged. This doesn't conflict with 'no loudness normalisation' (owner, 9 November).
- Exact live-morph continuation after a restart is not required.
- Going back to plain DUELL 1.1 is documented, not supported.
- No upstream pull requests: TIDE goes its own way (owner, 8 November).
- No DRONE row: HOLD already latches the notes (owner, 8 November).
- LADDER's bass loss, filter 1 without self-oscillation and LOFI's aliasing stay as character.
- Hanging notes on gear that follows the FM-1's MIDI out (two keys on one note, a MODE or channel change under a held key; [FR2](audits/2026-11-09/firmware-review/TASKS.md#fr2)) and the hard gap in a USB recording at a flash write: not fixed. The FM-1 is played DAWless, receives notes and clock only, and goes out through its jacks (owner, 9 November). The briefs stay in case that changes.
- USB resampler taps as int16 (-3.8 KB, USB audio about 1 LSB different): not done, audio quality first (9 November).
- D43: a crossing that starts under a held chord keeps the chord whole. (owner, 10 November)
- Reverb FREEZE: not now; it stays an idea. (owner, 10 November)
- An ARP SEQ pattern keeps moving off-scale notes down to the scale (keys and steps move to the nearest note); the guide says so. (owner, 10 November)
- With both parts selected, the first click of MORPH's fader or AMOUNT puts both parts on the same value; the guide says so. (owner, 10 November)
- ARC's breakdown keeps the chord parts' hits; it thins by voices and level. (owner, 10 November)
- The drone scenes are built in (2 multis in ROM, no file pack); the musical review's ideas are built. (owner, 10 November)
- A separate 303 engine ported from Open303 / X0X bass303.c (AC1): 14.8 KB of flash against 768 B free, 1.5 KB of RAM for two parts, 3.9x an ANALOG 24 dB note's CPU on the host. b8 builds the 303's accent and slide into the ANALOG engine instead (WAVE ACID / ACID SQR) (10 November).
- An acid line generator (SEQ held + A4 writes a new 303 line, AC2): not needed. Acid lines are typed in and adjusted in the sequencer, which already plays notes, accents and slides (owner, 11 November).
- Wave card L/R lines and a deep-bass band: decided against (owner, 12 November, after seeing drawn mock-ups: they clutter the wave page that the b10 overhaul just cleaned up).

## Versions and releases

TIDE has its own version in [`TIDE_VERSION`](../TIDE_VERSION); DUELL's `VERSION` (1.1) is not touched. ABOUT shows both.

| Version | Means |
|---|---|
| `1.0.0-b1`, `1.0.0-b2`, ... | A beta: a build flashed to a unit for testing. |
| `1.0.0` | The first build the owner has validated: the stable one. |
| `1.0.1` / `1.1.0` | A fix / a feature on top of a stable; its betas are `1.0.1-b1` / `1.1.0-b1`. |

Tag, GitHub release and CHANGELOG section share the name; the tag has `tide-` in front (`tide-1.0.0-b2`). The older dated tags stay as they are.

**To cut a beta:** set `TIDE_VERSION` and start its CHANGELOG section, commit; tag before building (`git tag -a tide-1.0.0-bN`), because `tools/build.py` shows the plain version only for a clean tree at the tag and adds `+` otherwise; build with a normal `python tools/build.py` in `felucca-base` (b2 did it so; `--release` would name the file `duell-<DUELL version>-beta.fwsc`, so copy `build/duell-1.1.fwsc` to `duell-tide-beta-<version>.fwsc`); flash and test; push the tag; make a GitHub **pre-release** with `duell-tide-beta-<version>.fwsc` and its `.sha256`, "Build `abc1234`" and the known issues in the notes; push `jam11` (or run Host tests by hand) so Pages offers the new beta.

**To cut a stable:** the same with `1.0.0`, a normal release marked Latest with `tide-stable-1.0.0.fwsc`, and the `stable` branch moved to it. The site (`felucca-base/web/releases.py`) offers a beta only while it is newer than the stable one, and a release only if the commit of its tag has a green `Host tests` run (`releases.py --require-green`, from `pages.yml`; the newest release that has one is offered, a red or unchecked one is skipped and logged; the releases published before the gate are listed in `web/pre_gate_releases.txt` and stay offered).
