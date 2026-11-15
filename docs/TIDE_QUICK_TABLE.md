# TIDE settings at a glance

One page: what each TIDE setting does, in short. The full detail is in the [TIDE guide](TIDE_GUIDE.md), section 2 (AUTO TIDE) and 3 (TIDE SETTINGS). This is the page of 1.0.0-b10 and later: twelve rows (AMOUNT came from the MORPH page in b7, ANSWER and ANSWER ON in b10). Up to 1.0.0-b6 VARY BARS, TURNS, POLY, OFFBEAT, WASH, ROLL ARP and ROLL PITCH were rows too; DRONE is new in b4.

**Where:** HOME's round is MORPH → AUTO TIDE → TIDE SETTINGS.

- On MORPH: K1 is the fader; K2 to K4 the KNOBS set (a single sound: FILTER, RESONANCE, ECHO); hold OCT+ and click K1 to roll a new End B.
- On AUTO TIDE: K1 starts or stops the glide; K2 is the mode.
- On TIDE SETTINGS: K1 picks a row; OCT+ / OCT- (or K2..K4) change it.
- On all three screens: ALGORITHM picks PART 1, the middle (both parts, in the PLAY MODE row's mode, named LAYER, STEREO or DUEL) or PART 2.

With no notes coming in TIDE plays its own chord on C; send or hold one note to set its key.

## The tide mode (AUTO TIDE K2, the MODE row of TIDE SETTINGS): what happens at each arrival

| Mode | In short | Good for |
|---|---|---|
| EVOLVE | Every dial steps one position and bounces at the edges; always measured from your original, so it never drifts. | Slow evolution that stays recognisable. **Default** (STYLE DUB). |
| TRAVEL | Travels through other factory sounds of the same engine and the same role (pad stays pad, bass stays bass). | Long sets that change sound over time: STYLE AMBIENT and PEAK, or by hand. |

TIME (K3) is how many bars one trip takes; WAIT (K4) is how many bars it rests at each arrival.

## TIDE SETTINGS rows

| Row | Values (default **bold**) | In short |
|---|---|---|
| **STYLE** | **DUB**, DEEP, MINIMAL, AMBIENT, PEAK, DRONE, CUSTOM | A preset for the whole tide: sets MODE, TIME, WAIT and ARC, and the pattern's feel (below), in one go. Change MODE, TIME, WAIT or ARC by hand and STYLE reads CUSTOM. |
| **MODE** | **EVOLVE**, TRAVEL | The tide mode, the same as K2 on AUTO TIDE (table above). |
| **TARGET** | **BOTH**, SOUND, SEQ | What the glide moves: the sound, the pattern, or both. |
| **ARC** | OFF, **RISE**, DROP | A long shape in 8-bar phrases: builds up (more syncopation, brighter), peaks, eases into a breakdown where the parts thin out, recovers. DROP also drops out on some arcs; PEAK's first arc always drops. Hand drop: hold OCT- and turn K2 on AUTO TIDE. |
| **KNOBS** | **AUTO**, STD, DUB, HSE, TECH, ACID, ELEC, AMB | Which set K2-K4 play on HOME page 1; AUTO reads the tag (first word) of the multi's name. New End Bs are always MUSIC. See the sets in the [TIDE guide](TIDE_GUIDE.md). |
| **AMOUNT** | 0 to 100 % (**15 %**) | How far a roll may go (the selected part's; BOTH moves each part by the same steps). Not part of a STYLE. |
| **REC** | **ADD**, REPLACE | Live recording into a step that holds notes: add to them, or take their place. Not part of a STYLE. |
| **ANSWER** | OFF, **BASS LINE**, DOUBLE, REPLY, ECHO, CHORDS | What the other part gets when a REC take closes, always inside the part's own loop (LEN). CHORDS since 1.0.0-b13. |
| **ANSWER ON** | **OTHER**, OFF | Whether the answer is written to the other part (OFF: takes are kept, nothing is written). |
| **PLAY MODE** | **LAYER**, STEREO, DUEL | The output mode; It is what ALGORITHM's middle plays; changing it moves ALGORITHM to the middle (held notes let go). |
| **XFADE** | **0** to 100 % | Velocity crossfade between the parts (LAYER only). |
| **SPLIT** | **OFF**, MIDDLE, then a note | Key split point: below it part 1. MIDDLE = the middle of the keys, moving with OCT- / OCT+. Acts in ALGORITHM's middle. |

ARC acts in EVOLVE or TRAVEL with the pattern in TARGET (BOTH or SEQ), with any KNOBS set; with TARGET SOUND its value is dim and knob 1's line says NEEDS SEQ.

**The pattern's feel, set by STYLE (no rows):** *vary bars* (the variation every 4th bar or every bar), the two parts also take turns by themselves (one plays a phrase, the other answers), *poly* (now and then one loop a little shorter, so its motif drifts against the beat), *offbeat* (hits lean towards the off-beat 8th), *wash* (filter dip and send swell where sounds cross).

## The six STYLEs

| Style | Mode | TIME | WAIT | VARY | POLY | OFFBEAT | ARC | WASH | Feel |
|---|---|---|---|---|---|---|---|---|---|
| DUB | EVOLVE | 16 | 2 | 4TH | GENTLE | SOFT | RISE | OFF | Dub-techno: your own sounds walk, call and answer. |
| DEEP | EVOLVE | 32 | 4 | 4TH | OFF | SOFT | RISE | LOW | Long, deep, stays close to your sound. |
| MINIMAL | EVOLVE | 8 | OFF | BAR | WIDE | PUSH | OFF | OFF | Busy, shifting, polymetric. |
| AMBIENT | TRAVEL | 64 | 8 | 4TH | OFF | OFF | OFF | HIGH | Very slow, washy, no groove tricks. |
| PEAK | TRAVEL | 8 | OFF | BAR | GENTLE | PUSH | DROP | OFF | Fast changes, pushes, drops. |
| DRONE | EVOLVE | 64 | OFF | 4TH | OFF | OFF | OFF | HIGH | For a held drone chord: always moving, slowly, around your own sound; nothing rhythmic. Since 1.0.0-b4. |

Move the MORPH fader by hand and TIDE lets you play; 8 bars after you let go it carries on by itself.
