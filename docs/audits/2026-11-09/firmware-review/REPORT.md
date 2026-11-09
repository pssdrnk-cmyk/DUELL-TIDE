# Firmware review, 9 November 2026: functional and code

Reviewed: DUELL with TIDE 1.0.0-b6 as published (`fd4b6c6` on `jam11`), read as the TIDE build. Six reviewers each traced one area of the code by reading it; nothing was built, rendered or run on the FM-1. The coordinator re-read the code behind the nine findings marked ✔ below. Everything already on the ROADMAP, in KNOWN_ISSUES / KNOWN_LIMITS or in the audits of 8 and 9 November was left out.

Files: the lane reports with the full traces are in [`lanes/`](lanes/) (A keys and MIDI, B sequencer, C MORPH and loading, D screens, E saved data, F audio core). The briefs are in [`TASKS.md`](TASKS.md), the rows are in the [ROADMAP](../../../ROADMAP.md).

## Verdict

**No way to lose saved data in normal use, no crash, no hanging note on the FM-1 itself.** The storage commit order, the editor's transfers, the update path, the two-core hand-off, MIDI parsing and voice stealing were traced and are sound (each lane lists what it found correct, so nobody audits it again).

**35 new findings: 1 P1, 11 P2, 23 P3.** They cluster in four places:

1. **Timing under TIDE.** A swung pattern comes back late from a POLY stretch of odd length and stays late (the one P1). Knob recording on ARP SEQ parts follows the wrong clock.
2. **The other part gets hurt.** Things done to one part or to BOTH change the part you didn't touch: a GLIDE on BOTH snaps the other fader, a saved-sound load cuts the other part's held notes, a fast PRESETS turn lands on the other part.
3. **End B is fragile outside the unit's own SAVE.** The web editor's save drops it; load UNDO and AUTO TIDE's HOME re-roll it; loading a factory multi switches ROLL ARP / ROLL PITCH off for good.
4. **MIDI out from the keys.** The FM-1 itself never hangs, but a DAW or second synth is left with hanging notes in three ordinary cases. The b7 play-mode try-out turns one of them into an everyday path.

The rest are small screen-and-control slips (stray letters in RENAME, OCT- acting twice, wrong cancel message) and words that don't match the unit (ERASE DATA erases the FM6 bank and the user tables without saying so).

## Findings

✔ = the coordinator re-read the code and agrees. Size is flash. "Int." = touches the audio interrupt.

### P1

| ID | What the player meets | Size | Brief |
|---|---|---|---|
| B1 ✔ | A swung pattern (DEEP SHUFFLE, HOUSE STAB, DEEP 332, DOT DELAY, or any TEMPO SWING) under a style with POLY: after a 15- or 13-step stretch the part is late against the other part and the click, by about one to one and a half 16ths at SWG 56-60. It stays late until STOP / PLAY, and each further stretch adds to it. | ~30 B, int. | [FR1](TASKS.md#fr1) |

### P2

| ID | What the player meets | Size | Brief |
|---|---|---|---|
| A1 ✔ | Two keys that play the same note (a scale with SNAP, or an octave shift): the DAW gets a note-on and never the note-off. | ~15 B, int. | [FR2](TASKS.md#fr2) |
| A2 ✔ | Keys held across a MODE change (a multi load, the mode control): the DAW keeps every held note. b7's play-mode try-out makes this happen at every part change. | ~40 B, int. | [FR2](TASKS.md#fr2) |
| E1 ✔ | A sound saved to a slot from the web editor loses its End B: every later load rolls a new random B. The editor's own protocol text promises End B is kept, and the test that should catch it compares the function with itself. | ~40 B | [FR3](TASKS.md#fr3) |
| C1 ✔ | GLIDE with BOTH selected, the two faders at different places: the other part's fader jumps to the selected part's place in one block, switches and all. The AUTO TIDE picture doesn't show the jump. | ~30 B | [FR4](TASKS.md#fr4) |
| C3 ✔ | Loading any factory multi sets ROLL ARP and ROLL PITCH to OFF and ROLL TYPE WILD to SMART, and saves that into the settings. b6 fixed the same thing for AMOUNT only. | ~8 B | [FR5](TASKS.md#fr5) |
| F1 ✔ | Part 1 holds a big chord or a latched drone; you load one of your own saved sounds (or a multi or pattern) on part 2: part 1 loses notes at once and they don't come back while held. Factory sounds don't do it. | ~0 B | [FR6](TASKS.md#fr6) |
| D1 ✔ | SAVE held, knob 2 (RENAME) turned more than one click: the rest of the turn types a stray character into the name. OCT+ then saves the wrong name. | ~12 B | [FR7](TASKS.md#fr7) |
| A3 | ARP set to KEEPS RUNNING, a chord held on a MIDI keyboard, a sound change, keys released: the arp plays on until STOP ALL. | ~10 B, int. | [FR8](TASKS.md#fr8) |
| B2 | On an ARP SEQ part (both parts of M02) a recorded knob lock sits on the sequencer's step, not the arp's: after the next key press, a different ARP RATE or a POLY stretch it colours another note than the one you turned it on. | ~60-90 B, int. | [FR9](TASKS.md#fr9) |
| C2 | Load UNDO / REDO, and HOME on AUTO TIDE, bring a saved USER patch back with a new random End B; hand-set B values are gone. | ~40-60 B | [FR10](TASKS.md#fr10) |
| E2 | ERASE DATA also erases the FM6 voice bank and the three user wavetables. The dialog doesn't list them; INSTALL.md and QUICKSTART say the bank is kept. | ~20 B + docs | [FR11](TASKS.md#fr11) |

### P3

| ID | What the player meets | Brief |
|---|---|---|
| A4 | The same note from the keys and from MIDI on an arp part: HOLD adds to the chord instead of replacing it, and HOLD off leaves the arp running. | [FR8](TASKS.md#fr8) |
| A5 | A part's MIDI channel changed while a key is held: the note-off goes out on the new channel. | [FR2](TASKS.md#fr2) |
| A6 | MIDI IN = SEL, the wheel bent across a part change: the old part stays out of tune. | [FR8](TASKS.md#fr8) |
| A7 | Flash: `midi_play` holds its chord-keeping code twice (~120-200 B to win, unmeasured). | [ROADMAP](../../../ROADMAP.md), memory savings |
| B3 | VOIC moves a chord's out-of-scale tone onto the scale even with OFF-SCALE PLAY. | [FR13](TASKS.md#fr13) |
| B4 | PITCH can walk an ARP SEQ part below C3 when its sound isn't in the BASS category (the guard reads the step's offset, not the played note). | [FR13](TASKS.md#fr13), with tide-morph B1 |
| B5 | A big tap-tempo jump fires two or three steps at once and can leave an arp off the grid. | [FR13](TASKS.md#fr13) |
| B6 | ARP SEQ snaps notes to the scale downwards and ignores OFF-SCALE PLAY; keys and sequence steps take the nearest note. | Owner question 1 |
| C4 | SAVE after JOURNEY (or ROLL A) stores the sound you started from, not the A you hear. | Owner question 3 |
| C5 | MOD_INERT_TABLE says ANALOG has no inert slot; PW is hidden unless WAVE is PWM. | [FR11](TASKS.md#fr11) |
| D2 | OCT- held as a modifier still acts as OCT- on release where it has no modifier meaning: the octave drops on AUTO TIDE / PERFORM, the page jumps to HOME on MULTIS / PATTERNS, and on TIDE SETTINGS the guide's "hold OCT- and turn" steps the value and then back. | [FR7](TASKS.md#fr7) |
| D3 | SAVE held with BOTH refuses "SAME SLOT" when both parts play sounds from the read-only places (M17 DUNE HORN does). | [FR12](TASKS.md#fr12) |
| D4 | AMOUNT is drawn dim during JOURNEY "where it does nothing", but it still shapes hand rolls and JOURNEY's fallback roll. Corrects the b6 "c3" row. | [FR11](TASKS.md#fr11) |
| D5 | TURNS is drawn live when only one part glides, where it does nothing. | [FR11](TASKS.md#fr11) |
| D6 | SEQ + REC from HOME toggles HOLD instead of arming REC when SEQ is lifted first or together. | [FR7](TASKS.md#fr7) |
| D7 | A fast PRESETS turn followed within 150 ms by ALGORITHM loads the sound into the other part. | [FR7](TASKS.md#fr7) |
| D8 | With BOTH, the first click of MORPH's fader or AMOUNT knob sets the other part to the shown part's value. | Owner question 2 |
| D9 | Every cancelled dialog says NOT SAVED, also CLEAR SEQUENCE or INITIALIZE. | [FR7](TASKS.md#fr7) |
| E3 | A power cut in the ~50 ms of a save that moves a record, then a RESTORE of that slot: the old sound can come back at the next power-on. | [FR14](TASKS.md#fr14) |
| E4 | (Suspected) the editor's MENU tab writes the settings to flash on every request, with no rest time. | [FR15](TASKS.md#fr15) |
| E5 | A `--release` build accepts the developer switches from the environment, so keeping the serial console out depends on the builder's shell. | [FR15](TASKS.md#fr15) |
| F2 | (Suspected size) the effect code for bus B is a second full copy: ~10.6 KB by the b4 symbol sizes, ~9 KB to win. | [ROADMAP](../../../ROADMAP.md), memory savings |
| F3 | A USB recording gets a hard 17-40 ms gap with two clicks when the unit writes flash (stopped transport only); the speaker gets the soft dip. | [FR15](TASKS.md#fr15) |

## Recommendation

**b7, before the play-mode try-out goes to the owner** (about 300 B together; 4.8 KB are free):

1. [FR2](TASKS.md#fr2) keys' MIDI out (A1, A2, A5). b7's try-out sends every part change through the mode change, so A2 would hit at every part switch with a key down. The b7 builder also needs to know that the mode change silences held keys by design: under the try-out, switching part with a chord held cuts the chord.
2. [FR1](TASKS.md#fr1) swung POLY (B1): the only P1, a cheap fix, and no existing test runs POLY with swing.
3. [FR4](TASKS.md#fr4) GLIDE on BOTH (C1), in the same branch as the planned J2 and X1: same launch code.
4. [FR5](TASKS.md#fr5) factory multis keep the ROLL rows (C3): one line beside b6's AMOUNT rule.
5. [FR3](TASKS.md#fr3) editor save keeps End B (E1), with a test that really compares bytes.
6. [FR7](TASKS.md#fr7) the small control fixes (D1, D2, D6, D7, D9).
7. [FR11](TASKS.md#fr11) words (E2 dialog and docs, C5, D4, D5), into the existing docs-fixes row.

**b8:**

8. [FR6](TASKS.md#fr6) saved-sound load cuts the other part's notes (F1). It changes two-core code, so it gets its own FM-1 check and doesn't ride with b7's experiment.
9. [FR8](TASKS.md#fr8) held-note bookkeeping between MIDI, keys and the arp (A3, A4, A6).
10. [FR10](TASKS.md#fr10) UNDO / HOME keep a USER patch's End B (C2).
11. [FR9](TASKS.md#fr9) knob locks on ARP SEQ (B2), next to the live-recording limits row.
12. [FR13](TASKS.md#fr13) pattern polish (B3, B5; B4 folded into the existing bass brief).

**Before 1.0.0:** [FR14](TASKS.md#fr14) stale patch records (E3); [FR12](TASKS.md#fr12) BOTH save from the read-only places (D3), with the USER-slot work; [FR15](TASKS.md#fr15) housekeeping (E5 release guard, F3 USB dip, one erase count for E4); read core 1's stack peak on the unit with TIDE's crossings and wash running (the 1,744 B figure predates them).

**Reserve, not scheduled:** F2 (~9 KB of flash from one copy of the bus effects) is the largest reserve anyone has found, larger than every row in the space audit except the factory multis. It needs a DUEL CPU sweep, so keep it for when a feature needs the room. A7 is ~150 B on the same terms.

**One rule to add to "How we work":** a new store in `0xE0000..0xFBFFF` keeps the last 256 bytes of each sector erased (the update loader treats a sector tail that passes its record check as a leftover update). The 24-multi row (U4) and any use of the reserved range must follow it.

**Nothing to drop** from the current roadmap. One row to correct: "c3 ... K4 dim in JRNY, where it does nothing" (D4: it does something).

## Owner questions

None blocks the work above; each has a default that applies if unanswered.

1. **ARP SEQ and the scale (B6).** An ARP SEQ pattern moves off-scale notes down to the scale; keys and sequence steps move them to the nearest note. On scales with gaps (pentatonic, blues, harmonic minor) the same pattern plays different notes on the two kinds of part. Make ARP SEQ follow the keys? It would change how existing ARP SEQ patterns sound on those scales. **Default: leave it, and say so in the guide.**
2. **BOTH and the MORPH knobs (D8).** With BOTH selected, the first click of the fader or AMOUNT knob puts both parts on the same value. Should each part instead move by one step from where it is? **Default: keep "BOTH = the same value" and say so in the guide.**
3. **SAVE after JOURNEY (C4).** SAVE stores the sound you loaded, not the A that JOURNEY has moved to (KEEP freezes what you hear). **Default: SAVE shows a short message when the two differ.**

## What was not covered

- Nothing was run: every finding is from reading the code. FR1, FR4 and FR6 each include a host check that reproduces the fault first.
- Not read: the engines' voice code one by one (`eng_*.c`, `fm6_core.c`, `phys_*.c`), the reverb models line by line, `budget.c`'s ease ladder, `mod_begin` and LFO sync, the drawing code (`ui_draw.c`, `ui_graph.c`, `ui_scope.c`), the key scan in the HAL, the web editor and installer. No overflow proof at extreme resonance or feedback.
