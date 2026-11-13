# What TIDE changes from DUELL 1.1

Moved here from the README.

**MORPH**, HOME's first page, is the hands-on page: knob 1 the morph fader (A to B), knobs 2 to 4 the macros of the loaded multi's genre (a single sound: FILTER, RESONANCE, ECHO), over a live picture of the sound; hold OCT+ and click knob 1 for a new random End B. **AUTO TIDE** runs the tide: knob 1 glides to B or to A, knob 2 picks the AUTO TIDE mode (EVOLVE or TRAVEL), knob 3 the TIME of a trip (1 to 64 bars), knob 4 the WAIT at each end, and ALGORITHM picks PART 1, both parts (the middle, playing the PLAY MODE row's LAYER, STEREO or DUEL) or PART 2. **TIDE SETTINGS** says how it behaves, in twelve rows: STYLE, MODE (the tide's mode, the same as knob 2 on AUTO TIDE), TARGET, ARC, KNOBS (which genre set knobs 2 to 4 play on HOME) and AMOUNT (how far a roll goes), then REC (ADD or REPLACE for live recording), ANSWER and ANSWER ON (what the other part is given when a take closes, and whether it is written), PLAY MODE, XFADE and SPLIT (the output mode, LAYER / STEREO / DUEL, and its two settings). VARY BARS, POLY, OFFBEAT and WASH have no rows: STYLE sets them. Every new End B is rolled MUSIC (ROLL TYPE is gone); ROLL ARP and ROLL PITCH have no row either (OFF unless a unit had them on). Every row, its values and when a change lands are in the [TIDE guide](TIDE_GUIDE.md). The STYLE presets:

| STYLE | AUTO TIDE MODE | TIME | WAIT | VARY BARS | POLY | OFFBEAT | ARC | WASH |
|---|---|---|---|---|---|---|---|---|
| **DUB** | EVOLVE | 16 bars | 2 bars | EVERY 4TH | GENTLE | SOFT | RISE | OFF |
| **DEEP** | EVOLVE | 32 bars | 4 bars | EVERY 4TH | OFF | SOFT | RISE | LOW |
| **MINIMAL** | EVOLVE | 8 bars | OFF | EVERY BAR | WIDE | PUSH | OFF | OFF |
| **AMBIENT** | TRAVEL | 64 bars | 8 bars | EVERY 4TH | OFF | OFF | OFF | HIGH |
| **PEAK** | TRAVEL | 8 bars | OFF | EVERY BAR | GENTLE | PUSH | DROP | OFF |
| **DRONE** | EVOLVE | 64 bars | OFF | EVERY 4TH | OFF | OFF | OFF | HIGH |

A new unit starts on DUB, so DUB's row (bold) is the default. A unit that had TIDE settings saved before STYLE existed keeps them and shows CUSTOM; the values it lacked load as POLY GENTLE, OFFBEAT SOFT and ARC OFF. A saved STYLE stays that style after an update (a b6 DUB reads as DUB with EVOLVE; a saved MODE of ONCE, LOOP, DRIFT or WANDER opens as EVOLVE, JOURNEY as TRAVEL). Only a hand edit of MODE, TIME, WAIT or ARC turns STYLE into CUSTOM.

Also new: 43 more factory sounds (#129 to #171: three of them FM6, the last eight drones) and ten chord and pad patterns for dub techno and deep house; four USER slots for patches (U1 to U4) and for patterns (P29 to P32); MULTIS and PATTERNS lists with a preview of the picked entry; live graphs on the ENV, LFO, CYCLE, filter, INSERT, MATRIX and CHORD pages; SEQ MORPH (a pattern has an End A and an End B too); ARP SEQ on PLAY; KEEP in the SEQ tools. Fixes that also hold with TIDE off (clock phase, FM6, UNISON, PHYS, CHORD keys, MODE / SPLIT) are in the [CHANGELOG](../CHANGELOG.md). To make room, the fonts are Huffman-coded and UI render paths no page reaches are gone; building with `DUELL_TIDE=0` gives DUELL 1.1 without any TIDE code, and `jam/check_off.sh` checks that.

DUELL 1.1 itself (the parts and modes, the ten engines, the factory content, effects, MIDI and the CC map) is in the [DUELL 1.1 reference](DUELL_1_1_REFERENCE.md).
