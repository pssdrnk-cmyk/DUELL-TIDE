# TIDE settings at a glance

One page: what each TIDE setting does, in short. The full detail is in the [TIDE guide](TIDE_GUIDE.md), section 2 (AUTO TIDE) and 3 (TIDE SETTINGS). This is the page of 1.0.0-b7 and later: ten rows (AMOUNT came from the MORPH page). Up to 1.0.0-b6 VARY BARS, TURNS, POLY, OFFBEAT, WASH, ROLL ARP and ROLL PITCH were rows too; DRONE is new in b4.

**Where:** HOME's round is MORPH → TIDE SETTINGS → AUTO TIDE. On MORPH, K1 is the fader, K2 FILTER, K3 RESONANCE, K4 ECHO; OCT+ held and a click of K1 rolls a new End B. On AUTO TIDE, K1 starts or stops the glide and K2 is the mode. On TIDE SETTINGS, K1 picks a row and OCT+ / OCT- (or K2..K4) change it. On all three, ALGORITHM picks PART 1, the middle (both parts, in the PLAY MODE row's mode, named LAYER, STEREO or DUEL) or PART 2.

With no notes coming in TIDE plays in C minor; send or hold one note to set its key.

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
| **KNOBS** | **AUTO**, STD, DB, HS, TC, AC, EL, AM | Which set K2-K4 play on HOME page 1; AUTO reads the multi's 2-letter name prefix. New End Bs are always MUSIC. See the sets in the [TIDE guide](TIDE_GUIDE.md). |
| **AMOUNT** | 0 to 100 % (**15 %**) | How far a roll may go (the selected part's; BOTH moves each part by the same steps). Not part of a STYLE. |
| **REC** | **ADD**, REPLACE | Live recording into a step that holds notes: add to them, or take their place. Not part of a STYLE. |
| **PLAY MODE** | **LAYER**, STEREO, DUEL | The output mode; It is what ALGORITHM's middle plays; changing it moves ALGORITHM to the middle (held notes let go). |
| **XFADE** | **0** to 100 % | Velocity crossfade between the parts (LAYER only). |
| **SPLIT** | **OFF**, MIDDLE, then a note | Key split point: below it part 1. MIDDLE = the middle of the keys, moving with OCT- / OCT+. Acts in ALGORITHM's middle. |

ARC works while the tide runs in EVOLVE or TRAVEL with the pattern in TARGET (BOTH or SEQ), with any KNOBS set. Otherwise its value is dim and the top line says what's missing.

**The pattern's feel, set by STYLE (no rows):** *vary bars* (the variation every 4th bar or every bar), *turns* (how the two parts share the variations; they also take turns by themselves, one part playing a phrase and the other answering), *poly* (now and then one loop a little shorter, so its motif drifts against the beat), *offbeat* (hits lean towards the off-beat 8th), *wash* (filter dip and send swell where sounds cross).

## The six STYLEs

| Style | Mode | TIME | WAIT | VARY | TURNS | POLY | OFFBEAT | ARC | WASH | Feel |
|---|---|---|---|---|---|---|---|---|---|---|
| DUB | EVOLVE | 16 | 2 | 4TH | LEAD | GENTLE | SOFT | RISE | OFF | Dub-techno: your own sounds walk, call and answer. |
| DEEP | EVOLVE | 32 | 4 | 4TH | TAKE TURNS | OFF | SOFT | RISE | LOW | Long, deep, stays close to your sound. |
| MINIMAL | EVOLVE | 8 | OFF | BAR | LEAD | WIDE | PUSH | OFF | OFF | Busy, shifting, polymetric. |
| AMBIENT | TRAVEL | 64 | 8 | 4TH | TOGETHER | OFF | OFF | OFF | HIGH | Very slow, washy, no groove tricks. |
| PEAK | TRAVEL | 8 | OFF | BAR | LEAD | GENTLE | PUSH | DROP | OFF | Fast changes, pushes, drops. |
| DRONE | EVOLVE | 64 | OFF | 4TH | TOGETHER | OFF | OFF | OFF | HIGH | For a held drone chord: always moving, slowly, around your own sound; nothing rhythmic. Since 1.0.0-b4. |

Move the MORPH fader by hand and TIDE lets you play; 8 bars after you let go it carries on by itself.
