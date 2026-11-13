# PLAY MODE SELECT and SPLIT MIDDLE (1.0.0-b7, owner 9 and 10 November)

> An experiment. Compile flag `TIDE_PLAYSEL` (core.h; 1 in TIDE builds, 0 = b6's PART 1 / BOTH / PART 2 with the row's mode always). No new setting, no new page, no format change. DUEL is the MODE `DM_SPLIT` (parts hard left / right), named DUEL on the screen.

ALGORITHM on the HOME pages (MORPH, TIDE SETTINGS, AUTO TIDE) and PERFORM: three positions, a detent a step, stopping at the ends (owner, 10 November: "1-layer-2 when changed in the menu then 1-stereo-2"; the first version had five, PART 1, LAYER, STEREO, DUEL, PART 2):

| Position | What the audio plays | Knobs, fader, morph, glide edit | Keys play |
|---|---|---|---|
| PART 1 | the PLAY MODE row's mode, but LAYER as STEREO | part 1 | part 1, all keys (SPLIT not used) |
| the middle (named LAYER / STEREO / DUEL by the row) | the row's mode | both parts (b6's BOTH) | LAYER: both parts; STEREO / DUEL with SPLIT: lower part 1, upper part 2; SPLIT OFF: the last single part |
| PART 2 | as PART 1 | part 2 | part 2, all keys |

- **Nothing new is stored.** The position is BOTH (song.both) and the part (song.sel), as in b6. The PLAY MODE row's byte (duell_mode_req) is saved as ever. The mode the audio uses is derived from them (seq.c MODE_WANT: LAYER with one part selected plays as STEREO, the part alone on every key), so a LAYER multi never plays both parts on PART 1.
- **The row picks the middle:** a change of the PLAY MODE row (ui_input.c mode_knob) also sets BOTH, so it is heard at once. A MODE CC sets the mode alone. A multi saved in LAYER loads in the middle (multi.c: LAYER after the load sets BOTH).
- **Held notes:** a change of the mode the audio plays lets every held note go (seq.c mode_apply, ~50 ms dip). With the row on LAYER that happens between a side and the middle (STEREO underneath ↔ LAYER); with STEREO or DUEL nothing changes and held notes stay.
- **XFADE and SPLIT** act in the middle only (TIDE SETTINGS says 2 PARTS ONLY with one part selected); XFADE needs LAYER, SPLIT any mode.
- **The header names the position:** in the middle on HOME's pages (not PERFORM, whose footer says ALGORITHM = ..), the title is LAYER, STEREO or DUEL in the theme colour, both part numbers lit (ui_draw.c head_ps_on). PART 1 / PART 2 keep the page's name and one lit number.
- **HOME page 1 (b7's hands-on MORPH page):** its knobs act on the selected part(s); in the middle they move both parts relatively, as BOTH did. OCT- / OCT+ held gestures and the edit pages' part selection (track_select) are unchanged.
- **Space:** the split helpers are out of line (seq.c split_note, lay_route: event rate, tools/ram_allow.py) and the factory pattern table keeps its flags as bit masks (SP3, engines.c pat_fl, 640 B). Flash with the feature: about 580 B free; RAM unchanged.
- The edit pages still pick PART 1 / PART 2 only.

SPLIT MIDDLE: a SPLIT value after OFF (stored 129: 0 OFF, 1..128 note + 1, 129 was free; a b6 build reads 129 from the settings or a multi as OFF).

| | MIDDLE |
|---|---|
| Keys | 27 keys (F3 .. G5 at octave 0): key 14 (F#4 at octave 0) is the middle key and starts part 2, 13 keys below, 14 from it up. OCT- / OCT+ move the split with the keys: the same physical keys always split |
| MIDI notes | the note under the middle key now: F#4 (note 66) + 12 per octave shifted |
| SPLIT row | OFF, MIDDLE, then the notes; out of MIDDLE the first step right lands on the note it stands for now |
| MIDI CC / typed SPLIT line | 0 OFF, 1 MIDDLE (was note 1, C#-1), 2..127 the note |
