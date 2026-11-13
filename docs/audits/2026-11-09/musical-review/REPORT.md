# Musical review of TIDE and the factory multis, 9 November 2026

Reviewed: 1.0.0-b6 (`fd4b6c6`). Three reviewers measured what the FM-1 plays: every factory multi for 64 bars, four multis under all six styles for 128 bars, and every TIDE SETTINGS row changed one at a time (about 400 host renders of the real firmware path). Nothing was heard on the unit and nothing in the firmware was changed. The coordinator checked the code behind the findings marked ✔.

**The yardstick:** the FM-1 is the chord and pad bed in a DAWless house / dub-techno setup, behind a drum machine, a bass synth and a lead. It plays and evolves by itself and is picked up by hand at will. So: leave the low end and the lead's room free, restrained density, movement in the algorithm, few settings.

Files: the lane reports with all tables are in [`lanes/`](lanes/) (M the multis, S the styles, R the rows). The briefs are in [`TASKS.md`](TASKS.md). The render tools and data stay in the workspace (`work/audit/musical-review-2026-11-09/`).

## Verdict

**The foundation is right; the defaults and the long shape are not yet.**

What already serves the setup, by the numbers:
- No style gets too busy or too loud to sit behind drums and bass (at most 8.6 hits a bar against the pattern's own 7.0), and TIDE never seems switched off (never more than 8 bars without a change).
- The motif and each part's register lane survive 128 bars; the two parts land on the same 16th only once in ten bars on M02; the harmony of all 18 multis is intact after 64 bars.
- Six groove multis keep the low end clean (under 1 % of their energy below 120 Hz), and the chord parts sit in the chord lane.
- The six styles are measurably different from each other; none is redundant.
- WANDER is a steady bed: 2 to 4 dB of slow level movement, brightness within about ×2.

What falls short, as the owner meets it:

1. **The default changes the sound too much.** A new unit plays DUB, and DUB uses JOURNEY: every 35 seconds each part becomes another factory sound, brightness swings sevenfold, half the bars sound dulled, and on half the multis the bed ends up 4 to 7 dB louder than the multi was set. The same style with WANDER keeps the sounds, moves the pattern more, and has none of this. It is one byte.
2. **The long shape isn't heard.** ARC's breakdown removes 3 % of the notes and under 1 dB. Its phases change on bar 28 or bar 100 instead of on 8s and 16s, and the peak lasts 4 bars. PEAK, the style that promises drops, has none in the first 6 minutes, and the arc is the same every night.
3. **Left alone, 8 of the 18 multis play only one part.** With no notes coming in, the pads and arps that wait for keys stay silent, including the dust bed of BASIC CHORD and the beds of both drones. TIDE is meant to run by itself.
4. **The low end is not always left free.** The first three multis after BASIC CHORD all carry a bass line (a new unit starts on one). FINAL REEL's brass pad has 71 % of its energy under 120 Hz, and the planned level fix would turn that up. In TINE DUB and DEEP 332 the pad sits on top of the stab.
5. **Half the TIDE SETTINGS rows change nothing you can hear.** TURNS is identical across its three values on 15 of 16 multis. OFFBEAT's PUSH equals SOFT. POLY fires once in four minutes or not at all. ROLL ARP and ROLL PITCH change nothing in the default mode. And ROLL TYPE set to SMART or WILD silently switches ARC and the rhythm rows off.
6. **TIDE thins the motif more than it should.** DEEP 332's five-hit stab figure is heard whole in 10 % of bars under DUB; M02's bass loses its pickup note in two bars of three.
7. **Touch the fader and TIDE stops for good.** A hand sweep freezes the evolving bed until AUTO TIDE is started again, and ARC starts over.

## Findings and recommendations

Value: HIGH = heard in the first minutes of normal use, or decides whether a multi or style is worth having. Size is flash.

### Defaults and first impression

| ID | What you meet | Recommendation | Size | Brief |
|---|---|---|---|---|
| S1 ✔ HIGH | DUB (the default) changes each part's sound every 35 s; brightness ×7; 51 % of bars dulled. | DUB plays WANDER. Measured with that one change: brightness ×2.1, 7 % dulled, level range 2.3 dB, and more pattern movement. JOURNEY stays in AMBIENT, PEAK and on the MODE row. | 1 byte | [MR1](TASKS.md#mr1) |
| S2 ✔ MEDIUM | AMBIENT ("no groove tricks") plays polymeter: a fifth of its bars shift. | AMBIENT's POLY off. | 1 byte | [MR1](TASKS.md#mr1) |
| M3 HIGH | A new unit starts on DUB CHORD: a bass line under a stab (20 % of the energy under 120 Hz, more when the bass part's sound changes). The next two multis carry a bass too. | The first start goes to BASIC CHORD (0.1 % under 120 Hz), once both its parts play alone (M1). In the new reel no two bass-carrying multis sit next to each other, none in the first two places. | data | [MR2](TASKS.md#mr2) |
| S9 LOW | The guide's dub recipe says WASH LOW, the style has WASH OFF; the difference is not measurable. | Keep the row, fix the guide. | docs | [MR1](TASKS.md#mr1) |

### Playing by itself

| ID | What you meet | Recommendation | Size | Brief |
|---|---|---|---|---|
| M1 HIGH | With no notes coming in, one part is silent in M01, M10, M11, M13, M14, M15, M17, M18. PAD SPLIT drops 15 dB; the drones become one thin line. | BASIC CHORD's dust bed and the two drone beds get a tied step so they sound on PLAY. For the other five, a part that waits for keys plays TIDE's own chord while AUTO TIDE runs and no key is held. | data + ~60-100 B | [MR2](TASKS.md#mr2) |
| R8 MEDIUM | A hand on the MORPH fader stops TIDE until it is started again; with BOTH a touch on either part stops both; the pattern edits stop with it and ARC starts over. | TIDE resumes by itself from where the hand left the fader, at the next phrase after 8 untouched bars; ARC keeps its place. | ~80-120 B | [MR7](TASKS.md#mr7) |
| M8 MEDIUM | The drones were made at 60 BPM; on the drum machine's clock at 122 they move twice as fast. | Halve their step rates (data), so they move as made on a house clock. | data | [MR3](TASKS.md#mr3) |

### The long shape

| ID | What you meet | Recommendation | Size | Brief |
|---|---|---|---|---|
| R2 ✔ HIGH | ARC's breakdown: 3 % fewer notes, 0.7 dB quieter. The peak is no denser or louder. | In the breakdown and the drop both parts thin to their anchors, and a chord part plays two voices of four. Land with D1 (dynamics on ARP SEQ parts), which gives the level half. | ~100-200 B | [MR4](TASKS.md#mr4) |
| R3 ✔ HIGH | ARC changes phase on 4-bar marks: 68 of 147 changes fall on bar 4 or 12 of a 16; the peak lasts 4 bars. | Count the arc in 8-bar units: every phase starts on a multiple of 8, the peak lasts at least 8 bars. | ~20 B | [MR4](TASKS.md#mr4) |
| S3 ✔ MEDIUM | PEAK has no drop in the first 6 minutes (12 of 12 sessions), and the arc's sequence is identical every session. | Under ARC DROP the first arc always drops; the arc's dice use the session seed. | ~20 B | [MR4](TASKS.md#mr4) |
| S4 MEDIUM | The ARC peak is not louder than the breakdown on ARP SEQ multis. | None new: it is D1. Re-measure after D1 and MR4. | - | - |

### The low end and the lanes

| ID | What you meet | Recommendation | Size | Brief |
|---|---|---|---|---|
| M2 HIGH | FINAL REEL and HAZY TENSOR are quiet, and the roadmap's plan is to turn them up (D46). But FINAL REEL's brass pad is 71 % under 120 Hz: turning it up gives more low end than the multi that carries a bass. | Register first, level second: the brass pad an octave up (or its lows cut), then trim. HAZY TENSOR to the spares. Rewrite D46's default. | data | [MR3](TASKS.md#mr3) |
| M6 MEDIUM | In TINE DUB and DEEP 332 the pad covers the same octave as the stab (13 and 7 semitones of overlap). | The pads' steps an octave down (C3, still above the bass band). | data | [MR3](TASKS.md#mr3) |
| M7 MEDIUM | HAZY TENSOR's arp plays 18 hits a bar over a bass; EERIE SIGNAL's 12. Too busy and too drum-like behind a drum machine. | Both to the spares, as the roles spec already asks (its Q1). | data | [MR3](TASKS.md#mr3) |
| R1 HIGH | Under JOURNEY the bed sits 4 to 7 dB above the multi's own level on M05, M06, M07, M08, M09, M14, M16 for most of a set; on TINE DUB it jumps 8.6 dB at the first crossing. Known item J1, measured worse than recorded: its guard compares factory sounds with each other, not with the part as the multi plays it. | A probe to find which values carry the jump, then keep the multi's mix values through a JOURNEY move. With MR1 this leaves the default and matters in AMBIENT, PEAK and hand-picked JOURNEY. | ~50-150 B | [MR5](TASKS.md#mr5) |
| S5 MEDIUM | Under JOURNEY the brightness still swings ×7 on M02 (a dull stab turns into bells). | With MR5: at most half an octave of brightness from the original, or the original itself as the fallback. | few B | [MR5](TASKS.md#mr5) |
| M4 MEDIUM | With a chord held, BASIC CHORD under JOURNEY wanders into the low band in 27 of 64 bars; cause not traced. | Probe with MR5. Not met under WANDER. | - | [MR5](TASKS.md#mr5) |
| M10 LOW | With keys held, WHISPER TIDE, COLD CHAPEL and PAD SPLIT come out 5 to 7 dB loud; D46 measured them without keys. | Measure the level trims with the chord held. | data | [MR3](TASKS.md#mr3) |

### The motif

| ID | What you meet | Recommendation | Size | Brief |
|---|---|---|---|---|
| S6 MEDIUM | DEEP 332's five-hit stab figure plays whole in 13 of 126 bars under DUB; mostly 3 of 5. | For figures of 5 hits or more the kept share goes from 3 of 5 to 4 of 5. | few B | [MR6](TASKS.md#mr6) |
| R9 MEDIUM | M02's bass loses its pickup note (the turnaround) in about 2 bars of 3: every gap edit takes the one free hit. | A part with one free hit gets no gaps; a bass part gets none (into the planned bass brief B1). | ~10 B | [MR6](TASKS.md#mr6) |

### The settings page

| ID | What you meet | Recommendation | Size | Brief |
|---|---|---|---|---|
| R4 ✔ HIGH | TURNS: its three values play the same on 15 of 16 multis. There is no call and answer. | Remove the row. | saves flash | [MR8](TASKS.md#mr8) |
| R7 ✔ MEDIUM | ROLL TYPE set to SMART or WILD silently switches TURNS, POLY, OFFBEAT and ARC off. ROLL ARP and ROLL PITCH change nothing under JOURNEY or WANDER on any factory multi. | Take the gate off ROLL TYPE. Move ROLL ARP and ROLL PITCH off the page. | ~10 B | [MR8](TASKS.md#mr8) |
| R5 MEDIUM | OFFBEAT: 39 / 41 / 41 % of hits on the off-beat for OFF / SOFT / PUSH. The off-beat feel comes from the patterns. | STYLE sets it; no row. | saves flash | [MR8](TASKS.md#mr8) |
| R6 MEDIUM | POLY: 0 to 24 bars in 128 inside a shortened loop; GENTLE and WIDE take the same time. | STYLE sets it, on the motif part only; no row. | saves flash | [MR8](TASKS.md#mr8) |
| R10, R11 LOW | WASH and VARY BARS barely differ between their values under WANDER / JOURNEY. | STYLE sets them; no rows. | saves flash | [MR8](TASKS.md#mr8) |
| R12 LOW | KEEP stores the full pattern, not the breakdown you heard. | Keep as is. | - | - |

The page would go from STYLE plus eleven rows to: STYLE, MODE, TARGET, ARC, ROLL TYPE, then REC, PLAY MODE, XFADE, SPLIT. Today's code-review item "TURNS dimmed unless both parts glide" (FR11) falls away if TURNS goes.

### The set

| ID | What you meet | Recommendation | Brief |
|---|---|---|---|
| M5 MEDIUM | 7 of 18 multis are slow pads or drones. Near-twins: WHISPER TIDE / COLD CHAPEL, DUNE HORN / VOID DEEP, BASIC CHORD / TINE DUB. Missing: a texture bed that plays by itself, an answer part, a keys house stab, a sparse minimal figure. | The roles spec's new reel (T8 / T9) covers the answer part and the pulse. Add two with existing sounds: a sparse blip figure with a tape-hiss swell, and SUNDAY KEYS on the house stab pattern with a held haze. | [MR9](TASKS.md#mr9) |
| M11 LOW | No groove multi has swing (all store 50). | Swing 56 on ORGAN SKANK's stab, as the spec writes it. | [MR3](TASKS.md#mr3) |
| M9 LOW | With no notes in, every multi plays in C; to change key you send one note. The guide doesn't say so. | One line in the guide. | [MR1](TASKS.md#mr1) |
| S7 LOW | House has no style of its own. | Not needed once DUB plays WANDER; only if that is declined. | - |
| S8 LOW | PEAK changes sound every 15 s. | Leave; re-measure after X1 and MR5. | - |

### What the algorithm lacks (ideas, not scheduled)

1. **Chord-rhythm vocabulary.** A small table of one- and two-bar cells (the dub off-beat skank with a throw on the last "and", the house 3-3-2 stab, a push into the downbeat, a half-time chord) that TIDE picks variations from, instead of random gaps and moves. About 200 B.
2. **Voicing that breathes with the arc.** On 16-bar boundaries the chord part opens its voicing in the rise and peak and closes it in the breakdown; the top voice up an octave at the peak. No scale needed, no change of chord. About 150-250 B.
3. **A real call and answer** in place of TURNS: one part plays its variation in bar 2 of each pair while the other drops to its anchors. About 80-120 B.

Number 2 pairs with MR4 and is the one a listener would notice first.

## Recommended order

1. **[MR1](TASKS.md#mr1)**, two bytes and three doc lines: DUB plays WANDER, AMBIENT without POLY. The largest gain for the smallest change in this review. b7.
2. **[MR2](TASKS.md#mr2)**: every part plays with no notes in, then the first start moves to BASIC CHORD. b7 for the three factory records, b8 for the rule.
3. **[MR3](TASKS.md#mr3)**: the data pass over the multis (low end before level, pads out of the stab's octave, spares, drone rates, swing). With the roles rows T8 to T10, which already rewrite the reel.
4. **[MR4](TASKS.md#mr4)**: an arc you can hear, on the 8-bar grid. After D1. b8.
5. **[MR6](TASKS.md#mr6)**: motifs stay whole. With the planned bass brief B1.
6. **[MR8](TASKS.md#mr8)**: the shorter settings page. Its picture goes to the owner first.
7. **[MR7](TASKS.md#mr7)**: TIDE comes back after a hand.
8. **[MR5](TASKS.md#mr5)**: JOURNEY keeps the multi's level and colour. Behind MR1, which takes JOURNEY out of the default.
9. **[MR9](TASKS.md#mr9)**: the two missing multis, with T9.

## Owner questions

Each has a default that applies if unanswered.

1. **DUB keeps its sounds.** A new unit's DUB moves the settings of the multi's own two sounds and the pattern (WANDER), instead of travelling through other factory sounds (JOURNEY). Default: **yes**.
2. **The first thing a new unit plays** is the off-beat chord with a dust bed and no bass line (BASIC CHORD), not DUB CHORD with its bass. Default: **yes**.
3. **A shorter TIDE SETTINGS page:** TURNS, ROLL ARP and ROLL PITCH go; OFFBEAT, POLY, WASH and VARY BARS are set by STYLE and are no longer rows. Default: **yes**; you see the page before it is built in.
4. **TIDE comes back by itself** 8 bars after you let go of the MORPH fader. Default: **yes**.
5. **Your own multis left alone:** when no notes come in, the pads and arps of PAD SPLIT, WHISPER TIDE, COLD CHAPEL, EERIE SIGNAL and HAZY TENSOR play TIDE's chord by themselves. Default: **yes**; the records stay as they are.
6. **FINAL REEL's brass pad** sits in the bass octave: move it up an octave. Default: **yes** (the other way is to cut its lows only).
7. **EERIE SIGNAL and HAZY TENSOR** move to the spares (kept; one line brings them back). Default: **yes** (this is the roles spec's open question).
8. **WHISPER TIDE and COLD CHAPEL** play nearly the same when left alone. Default: **keep both**; the two new multis wait for a free place.
9. **The drones on the drum machine's clock:** slow their steps so they move as made at 120 BPM. Default: **yes**.

## What was not covered

- Everything is measured on the host, at the default seeds; nothing was judged by ear or on the FM-1.
- R1's cause (which values carry JOURNEY's level jump) is not proven; MR5 starts with that probe. M4's cause is not traced.
- Hand takeover, KEEP and HOME were read in the code, not rendered.
- Not looked at: the multis under styles other than their default, PITCH ON, MINIMAL / AMBIENT / PEAK as the base for the row tests, FM6 detail, CPU.
