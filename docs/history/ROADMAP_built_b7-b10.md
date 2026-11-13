# Built in b7 to b10 and the finished Later rows (moved out of ROADMAP.md on 12 November 2026)

The roadmap's "On the unit" tables for b7, b8, b9, b10 (rc2) and rc3, and the struck-through Later rows, kept word for word. The rows that were only partly done stay on the live [roadmap](../ROADMAP.md). What is built is in the [CHANGELOG-TIDE](../../CHANGELOG-TIDE.md).

---

## On the unit: built in b7

On the unit since b10-rc2 (12 November).

Six builder lanes made these overnight (9 to 10 November); each is measured on the host; all of it has been on the unit since b10-rc2. State words in brackets are honest: what is only partly done says so.

| What | State |
|---|---|
| A sound saved from the web editor keeps its End B ([FR3](../audits/2026-11-09/firmware-review/TASKS.md#fr3)) | Built |
| Small control fixes: RENAME no longer types stray letters, OCT- held as a modifier doesn't also act on release, a fast PRESETS turn can't land on the other part, SEQ + REC arms REC however the buttons are lifted, NOT SAVED only for saves ([FR7](../audits/2026-11-09/firmware-review/TASKS.md#fr7)) | Built |
| Words that match the unit: ERASE DATA names the FM6 bank and the user tables (AMOUNT not dimmed in JOURNEY: done with page 1 in b7; the TURNS step falls away: the row is gone) ([FR11](../audits/2026-11-09/firmware-review/TASKS.md#fr11)) | Built |
| ALGORITHM on HOME: PART 1, the middle, PART 2. The middle plays both parts in the PLAY MODE row's mode and the header names it (LAYER, STEREO, DUEL); PART 1 / PART 2 play one part on every key; changing PLAY MODE moves ALGORITHM to the middle. With PLAY MODE on LAYER, moving between a side and the middle cuts a held chord (the mode change silences held keys by design) ([design](../design/PLAY_MODE_SELECT.md)) | Built (b7/playsel, on b7), on the unit since b10-rc2 |
| SPLIT MIDDLE: the keys split in the middle and the split follows OCT- / OCT+; with SPLIT set, STEREO and DUEL split the keys too | Built (b7/playsel, on b7), on the unit since b10-rc2 |
| Factory pattern table packed: its accent / slide / tie flags as bit masks (SP3, 640 B), the room for the two above | Built (b7/playsel) |
| STYLE DUB keeps the multi's own sounds (WANDER, not JOURNEY); AMBIENT without POLY; a saved STYLE stays that style ([MR1](../audits/2026-11-09/musical-review/TASKS.md#mr1)). M02: brightness swing 2x not 7x, dulled bars 7 % not 51 %. The first start stays M02 DUB CHORD | Built |
| A shorter TIDE SETTINGS page: nine rows, TURNS / ARP / PITCH / VARY / WASH / POLY / SYNC rows gone, MODE a row, ROLL TYPE no longer switches the rhythm off ([MR8](../audits/2026-11-09/musical-review/TASKS.md#mr8)) | Built. One mark not met: ARC under ROLL SMART is only 0.67 to 0.82 like MUSIC (not 0.95), because ROLL TYPE's own pattern roll differs; that is what ROLL TYPE is for |
| TIDE comes back by itself 8 bars after you let go of the MORPH or SEQ fader, only the part you moved is held, ARC keeps its place ([MR7](../audits/2026-11-09/musical-review/TASKS.md#mr7)) | Built |
| JOURNEY doesn't jump the sound when started with the fader away from A ([J2](../audits/2026-11-09/tide-morph/TASKS.md#j2)) | Built |
| JOURNEY's crossing dip lasts a few bars, not most of every trip ([X1](../audits/2026-11-09/tide-morph/TASKS.md#x1)) | Partly: the dip is short (2 bars closing, 1 closed, 2 opening); the brightness mark is not met (brightness x0.48 to x0.70 against x0.8), the blend between the two sounds itself is dull mid-trip. M16's switch bar is louder than the median (a note attack, listen on the unit) |
| JOURNEY keeps the multi's level ([MR5](../audits/2026-11-09/musical-review/TASKS.md#mr5)) | Partly: level kept on M05, M06, M16 (22 bars over 3 LU) and M08 (3 dB under its own sounds); the colour is not done (see Later) |
| TIDE's louder peak and softer breakdown reach ARP SEQ parts, M02 included; accents kept ([D1](../audits/2026-11-09/tide-morph/TASKS.md#d1)) | Built: M02 peak 112, breakdown 62 (was flat at 100) |
| An arc you can hear: ARC on the 8-bar grid, the first arc after PLAY drops, a different arc each session, a breakdown that thins ([MR4](../audits/2026-11-09/musical-review/TASKS.md#mr4)) | Partly: grid, drops and level done; the breakdown thins by voices (two of four) and level, not by onsets (M02 0.92 of the rise's hits, the mark was 0.65). Fewer onsets would mean dropping non-beat anchors, a decision for you (below) |
| Call and answer in place of TURNS: the two parts take turns bar by bar in WANDER and JOURNEY | Built; by voices 14 of 16 multis alternate, by hits 6 of 16 |
| The bass keeps its loop and octave: no POLY, no VOIC octave on a bass part; POLY ends in ARC's breakdown; the under-C3 guard on ARP SEQ parts ([B1](../audits/2026-11-09/tide-morph/TASKS.md#b1)) | Built |
| Motifs stay whole: a five-hit figure keeps four, a line's one free note is not taken ([MR6](../audits/2026-11-09/musical-review/TASKS.md#mr6)) | Built (M09 whole figure in 72 % of the bars, was 18 %) |
| The chord bed's voicing opens at the peak and closes in the breakdown; a chord-rhythm vocabulary of 12 house and dub-techno cells for the variation bar | Built |
| VOIC keeps an out-of-scale chord tone with OFF-SCALE PLAY (the VOIC part of [FR13](../audits/2026-11-09/firmware-review/TASKS.md#fr13)) | Built; the tap-tempo part of FR13 is not (see Later) |
| A swung pattern is back on the grid after a POLY stretch ([FR1](../audits/2026-11-09/firmware-review/TASKS.md#fr1)); a MIDI keyboard into the arp ([FR8](../audits/2026-11-09/firmware-review/TASKS.md#fr8)); GLIDE on BOTH moves each part from where its fader is ([FR4](../audits/2026-11-09/firmware-review/TASKS.md#fr4)); a factory multi leaves ROLL ARP, ROLL PITCH and ROLL TYPE as you set them ([FR5](../audits/2026-11-09/firmware-review/TASKS.md#fr5)) | Built (in the b7 changelog) |
| BASIC CHORD's dust bed and the two drone beds play with no notes coming in; every part that waits for keys plays TIDE's chord under AUTO TIDE ([MR2](../audits/2026-11-09/musical-review/TASKS.md#mr2), both parts) | Built. Level with no keys: M14 is +4.4 dB over the median, M15 -11.8, M16 -3.7, M08 +3.2 (the old M14 and M15 are replaced) |
| The multis' data pass ([MR3](../audits/2026-11-09/musical-review/TASKS.md#mr3)): pads an octave down in TINE DUB and DEEP 332, drones at a house clock and 5 dB louder, swing on ORGAN SKANK, FINAL REEL's brass keeps its octave with a gentle low cut, PAD SPLIT and WHISPER TIDE 5 to 6 dB quieter with a chord held | Built (register overlap of TINE DUB / DEEP 332 not re-measured) |
| The reel: fifteen new factory multis as one curated programme, a genre each (M01 BASIC CHORD (dub techno), M02 DUB CHORD (dub techno, rebuilt over a held pad), M03 JUNO DEEP and M04 LATE RHODES (deep house), M05 ORGAN HOUSE and M06 PIANO HOUSE (classic house), M07 DETROIT STAB and M08 HI-TECH SOUL (Detroit), M09 HYPNO 303 (hypnotic techno), M10 TWO 303s (acid), M11 ACID LINES (acid), M12 ROBOT FUNK and M13 DEEP SEA ARP (electro), M14 WARPED TAPE (IDM), M15 BERLIN RUN (Berlin school), M16 TWO LOOPS (ambient)). The night's six genre scenes, ORGAN SKANK, WARM ECHO, TINE DUB, DEEP 332 and the owner's four pads left the set (git history, fa8d5c0) | Built on the computer: notes, rhythm cells, registers and levels match the design; End B set by hand. Not on the unit yet: the CPU sweep decides (heaviest by the model: M06 PIANO HOUSE, M04 LATE RHODES, M05 ORGAN HOUSE), and the ear decides the organ, the piano, the acid line and the tape wobble |
| HOME page 1 is the hands-on page: K1 MORPH at half the turning, K2 FILTER, K3 ECHO, K4 LENGTH on the slim row (b8: K3 RESONANCE, K4 ECHO, no LENGTH); BOTH moves each part by the same steps; OCT+ held + K1 rolls a new End B; OCT- + a knob puts it back; AMOUNT is a TIDE SETTINGS row, not dimmed in JOURNEY | Built (host tests, device build); on the unit since b10-rc2 |
| Loading one of your own sounds, a multi or a pattern no longer cuts notes the other part is holding ([FR6](../audits/2026-11-09/firmware-review/TASKS.md#fr6)). Second-core code: its own FM-1 check. b8. | Built |
| Pattern polish: a big tap-tempo jump doesn't flam or knock the arp off the grid (the rest of [FR13](../audits/2026-11-09/firmware-review/TASKS.md#fr13)). | Built |
| Load UNDO (OCT- held + PRESETS) and AUTO TIDE's HOME bring your saved sound back with its own End B ([FR10](../audits/2026-11-09/firmware-review/TASKS.md#fr10)) | Built |
| Knob locks recorded on an ARP SEQ part play on the step the arp plays ([FR9](../audits/2026-11-09/firmware-review/TASKS.md#fr9)) | Built |
| SAVE held with BOTH on two sounds from the read-only places saves each into its own empty USER slot ([FR12](../audits/2026-11-09/firmware-review/TASKS.md#fr12)) | Built |
| After a save cut short by a power cut, RESTORE no longer brings the old sound back ([FR14](../audits/2026-11-09/firmware-review/TASKS.md#fr14)) | Built |
| tools/build.py --release refuses to build with a developer switch on ([FR15](../audits/2026-11-09/firmware-review/TASKS.md#fr15)) | Built |

The owner's answers to the musical review (9 November) are built in: the default for each of the nine questions, with the first start staying M02 DUB CHORD and FINAL REEL's brass pad keeping its octave.

## On the unit: built in b8

On the unit since b10-rc2 (12 November).

A polish beta on top of b7 (details in the [CHANGELOG-TIDE](../../CHANGELOG-TIDE.md#100-b8)); all of it has been on the unit since b10-rc2.

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

The sound and knobs beta on top of b8 (details in the [CHANGELOG-TIDE](../../CHANGELOG-TIDE.md#100-b9)); released as 1.0.0-b9 (11 November) and on the unit since then.

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

- **COLR LFO (CLFO):** a new DELAY page knob, 0..15, swings the echo's colour darker and brighter at a slow 0.25 Hz; 0 is off; DUB BASIC uses 5, DUB CHORD 2. Multis store it.
- **ELEC GRT and AMB DRF:** ELEC's GRT also moves the cutoff, so the odd harmonics span about 10 dB (was 2 dB); AMB's DRF reaches its full filter wobble in about 9 detents (twice as fast).
- **Six new factory multis, M19 to M24:** TECH HYPNO, DUB HOUSE, HSE LOFI, TECH CINEMA, AMB TECHNO and TECH MINIMAL (24 factory multis in all).
- **Developer tools:** `factory_multis.py verify` and `drone_pack.py verify` fail on any byte drift from the committed records M01 to M24; `multi_patch.py keep-amount` is removed.
- **Developer tests:** a generated table of the factory multis' names (`build/gen/multi_names.h`) that the C tests read, so a rename cannot break them; `tests/bench_run_test.py` fails on a suite that spells a factory name.

## Built in b10-rc3, waits for the FM-1

- **ARC first beat:** the first beat of a new ARC phase plays at the new phase's loudness (louder peak, softer breakdown), not the old level.
- **Hot-code check:** it now follows function pointers, so an engine voice function left in flash is no longer invisible; 15 engine hooks are allow-listed as event rate. No image changes.

## Done in b10 (the finished Later rows)

Numbers as they were on the roadmap.

1. ~~ARP SEQ parts drift against the beat on the internal clock: the arp's step period and the sequencer's beat round differently, 12 to 14 samples a bar on M02 at 117 BPM, about 107 ms after 400 bars (found by the ARC lane with a host probe). Not measured under an external clock, where both follow the clock grid and probably do not drift.~~ Built in b8 (DRIFT: arp, tracks and click run through one exact beat accumulator); checked on 12 November (b10 round 2) with `arpdrift`: M02 at 117 BPM, 400 bars, drift 1 and 0 samples at every row (a constant, no growth); the external-clock path is unchanged (`clockdrift` passes).
2. ~~A new ARC phase's first downbeat still plays with the old phase's level and voices (the main loop sets the levers after the bar turns; the drop's silence is armed ahead and is exact). One step per phase change; about 40 B to arm the levers a bar ahead.~~ Built in b8 (A2, the levers armed a unit ahead, `tarc_lev` bits 3-5); the b10 probe (`arc_levers` in `tidesteps`) reads 0 late levers on M02 and M06. The loudness step was the part left: see 25.
3. ~~`factory_multis.py build` no longer reproduces the committed End B AMOUNT bytes~~ Closed (b10 tools lane): the drift was already gone since b8 (the sounds' End B amounts match the shipped records; a rebuild from source gives zero byte diff on M01..M24, also after a record is damaged first). `multi_patch.py keep-amount` is retired, and `factory_multis.py verify` / `drone_pack.py verify` (in the `sounds` group) fail the build if a rebuild ever differs from the committed records again.
5. ~~Cleanup after TURNS~~: `sqm_turns` and `tarc_fast` were removed in b8 (`5a3e086`); checked again on 12 November (b10 round 2): no symbol of them is in the rc2 image.
6. ~~TRAVEL keeps the multi's colour~~ b10 tide3 (`fork/b10-tide3`): a part as a factory multi loaded it travels only to sounds in its Original's brightness octave whose level its TRIM can reach; if there is none, it travels between its own two ends. 8-bar brightness under TRAVEL against EVOLVE alone: M02 x2.6 / x2.6 (the rest is EVOLVE's own), M12 x1.3 / x1.3 (was x2.1, and half as bright), M13 x1.4 / x1.2 (was x1.9, its mid-trip dips gone). A sound loaded alone walks as J1.
11. ~~FM6 crossing fixes (findings 8, 9; brief K7) and Felucca #265 (signed DSP arithmetic, bit-exact).~~ Checked and built in b10 round 4 (`fw4`): finding 8 was built already (`budget.c` charges the crossing when the glide or fader arms, `bd_mv` with `XN_FM`); finding 9 fixed (`fm6_block`: a stand-in drain waits while a new crossing runs and still runs after it; probe in `tidecrossings`); K7 built (the part's LFO terms once a block and part, `fm6_lfo_terms`; bit-exact, 6,144 term combinations equal the old formulas; +36 B with finding 9 and 152 B less hot code, the saving per voice is about 0.15 % of an FM6 voice block, not the few % the brief expected); Felucca #265 is in since `6bef0a7` (the DC block, the FM6 velocity and LFO scaling, the phase additions, PHYS out; `ubsanrender` passes).
13. ~~Two small interrupt savings in the crossing step ([C1](../audits/2026-11-09/tide-morph/TASKS.md#c1)), with "Transition costs only while moving".~~ Built in b10 round 5 with Later 10: step 1 (the dead `k` store) is a 0 B cleanup; step 2 (the INSERT dip in 1/256 steps) is under the same switch. Host: with an INSERT difference forced on M02, a hand jump 0 -> 127 blends 278 times in 1.5 s instead of 351 (1.26 times fewer; the brief's 3 times is not reached: the dip's ramp is steep).
14. ~~WASH on: no jump when a glide is stopped by hand, one dip instead of two stacked ([W1](../audits/2026-11-09/tide-morph/TASKS.md#w1)).~~ Built in b10 round 4 (`fw4`, W1 steps 1 and 2): a stop fades the breath and the swell by time (1/32 of what is left a millisecond; per pass would be gone in a millisecond, the passes run far faster than the audio blocks) and the breath and the transition dip no longer stack (the lower of the two); the effective cutoff after a stop moves at most 1 dial step a block (it jumped 18); WASH off unchanged. +132 B. Step 3 (the wash level at rest, `wref`) left as it is: letting it follow during motion would pull the fill down toward the dipped level.
15. ~~Docs fixes from the TIDE and morph audit still open (the dip's length, the DUB recipe's wash, the bass promise and the dynamics paragraph were written in the b7 guide pass): `TIDE_GUIDE.md:23` the MORPH knob 3 text (the wave picture is shipped); EVOLVE "a little further each trip"; `MOD_INERT_TABLE.md:21` (ANALOG: PW is offered only with WAVE PWM), and `INSTALL.md:60` / `QUICKSTART.md:41` (ERASE DATA erases the FM6 voice bank and the user tables), with FR11.~~ Built in b10 chores, after checking each line against the source. Already right, no edit: ERASE DATA lists the FM6 bank and the user tables (`INSTALL.md:117`, `QUICKSTART.md:119`; `erase_data.c`); the MORPH knob 3 text (`TIDE_GUIDE.md:320`, RESONANCE); the ANALOG PW row (`MOD_INERT_TABLE.md:21`, `eng_analog.c:274`, `ui_draw.c` hides PW unless WAVE PWM). Fixed: EVOLVE wording (`web/cheatsheet/index.html`, `web/landing.html`; the step grows with the phrase, `morph_music.c:15-16`, `:162`, `:171`); the 160 figure (`DUELL_1_1_REFERENCE.md:28`).
16. ~~Factory pattern table trimmed~~ (SP3): built in b9.
17. ~~Parameter units stored as an index~~ (SP4): built in b9.
19. ~~FM6 MUSIC rolls at AMOUNT 40 leave the loudness window~~ ([finding 7](../audits/2026-11-09/tide-morph/REPORT.md)): built in b10 (`morph_fm_shape.c`: the roll moves FM6's carrier envelopes once, through the AMP ENV knobs; 99.7 % within +-4.5 dB, was 69.7 %).
23. ~~AUTO TIDE's step display can show one hit ahead that then doesn't play~~ (picture only): built in b10 (b10/fw-b). Cause: the page's redraw hash (`tide_sig`) did not see `morph.seed`; a ROLL sets the seed at once and CALL AND ANSWER's listening bars (and POLY) read it from the audio side, while the ends' key (`tseed`) follows from the main loop, so a look-ahead drawn just before the roll stayed on the screen until the playhead moved. Reproduced only with the old M09 MINIMAL 12 record (reel commit `2de0f78`, gone with the reel's re-voice `e2f8c07`); setting the old seed back reproduced the old view in all 4 cases. The seed is in the hash now (+4 B); `tidesteps` probes it (a seed alone changed 6,426 steps with both parts leading; the hash stayed the same 0 times, 103,451 before) and plays three more 12-step ARP SEQ loops.
25. ~~ARC's loudness step (sqm_dyn) still follows the main loop's pass, so a new phase's first beat keeps the old loudness (B8-ARC).~~ Built in b10 round 2: `sqm_dyn_nx` is armed with the levers and the audio side takes it from the unit's first bar (`tarc_levers`, `sqm_dyn_of`); the `arc_levers` probe read the step late at 4 of 11 unit starts before (first at bar 16), 0 after. About +40 B.
27. README's second picture (`docs/tide/morph-settings.png`) still shows the retired ROLL MUSIC / ARP / PITCH rows: shoot a new one of TIDE SETTINGS with the emulator (b9 chores). **Done in b11:** re-shot from the emulator, the scroll mark now shows 12 rows (PR 56).
33. The `budget` group (item 142 reference hash, A01 rules) and the `deadline` group (TIDE scene) only run with the MOVECOST switch off, but the shipped build has it on (`tools/build.py` default 1): built with `-DFELUCCA_MOVECOST=1` they fail 3 and 1 checks because their expectations are the off-state's (a part with WASH on at rest pays only the meter; valley, wash and cross-fade are charged only in a transition). Give them an ON build in `host_tests.sh` with ON expectations (re-pin through `jam/repin.sh`); found in b10 round 6. **Done in b11:** `budget` and `deadline` now build the shipped MOVECOST=1 configuration; the OFF state stays covered by `movecost` (PR 57).
28. Stale comment: `ui_input.c` above `part_select` still describes MORPH SET's SMART / WILD roll knobs, retired in b9 (b9 chores). **Done in b11:** the comment above `part_select` in `ui_input.c` is fixed (PR 61).
30. The store's cross-version test has no record written by b8 rc1 / rc2 (B9-STORE); add one to the fixtures. **Done in b11:** the b8 rc1 / rc2 store records are fixtures in `storexver`; the 5 records with ANALOG WAVE 3 / 7 / 8 are the known B9-STORE ambiguity, pinned (PR 59).
