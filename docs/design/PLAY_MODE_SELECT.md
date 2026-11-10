# PLAY MODE SELECT and SPLIT MIDDLE (1.0.0-b7, owner 9 November)

> An experiment. Compile flag `TIDE_PLAYSEL` (core.h; 1 in TIDE builds, 0 = b6's PART 1 / BOTH / PART 2, nothing else changes). No new setting, no new page, no format change. DUEL is the MODE `DM_SPLIT` (parts hard left / right), named DUEL on the screen.

ALGORITHM on the HOME pages (MORPH, TIDE SETTINGS, AUTO TIDE) and PERFORM, left to right, a detent a step, stopping at the ends:

| Position | Output MODE (PLAY MODE row) | Knobs, fader, morph, glide edit | Keys play |
|---|---|---|---|
| PART 1 | LAYER becomes STEREO (same mix); STEREO / DUEL stay | part 1 | part 1, all keys (SPLIT not used) |
| LAYER | LAYER | both parts (b6's BOTH) | both parts; with SPLIT: lower part 1, upper part 2 |
| STEREO | STEREO | both parts | with SPLIT: lower part 1, upper part 2; SPLIT OFF: the last single part |
| DUEL | DUEL | both parts | as STEREO, the parts hard left / right |
| PART 2 | as PART 1 | part 2 | part 2, all keys |

- The position is read from what is stored, nothing new: PART 1 / PART 2 = BOTH off and the part; LAYER / STEREO / DUEL = BOTH on and the MODE. A multi or project keeps its MODE and BOTH bytes as before and loads unchanged: BOTH always loads, the MODE only with MULTI LOAD SETS PLAY MODE (MENU > CONTROL), as in b6, so a load can land on another position (saved on DUEL, loaded while in LAYER: LAYER). An old multi with BOTH off and MODE LAYER shows LAYER and plays as in b6. The PLAY MODE row and a MODE CC still set the MODE alone.
- A move between positions with a different MODE lets the held notes go (as a PLAY MODE change always did, ~50 ms dip).
- The edit pages still pick PART 1 / PART 2 only.

SPLIT MIDDLE: a SPLIT value after OFF (stored 129: 0 OFF, 1..128 note + 1, 129 was free; a b6 build reads 129 from the settings or a multi as OFF).

| | MIDDLE |
|---|---|
| Keys | 27 keys (F3 .. G5 at octave 0): key 14 (F#4 at octave 0) is the middle key and starts part 2, 13 keys below, 14 from it up. OCT- / OCT+ move the split with the keys: the same physical keys always split |
| MIDI notes | the note under the middle key now: F#4 (note 66) + 12 per octave shifted |
| SPLIT row | OFF, MIDDLE, then the notes; out of MIDDLE the first step right lands on the note it stands for now |
| MIDI CC / typed SPLIT line | 0 OFF, 1 MIDDLE (was note 1, C#-1), 2..127 the note |
