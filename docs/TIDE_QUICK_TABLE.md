# TIDE settings at a glance

One page: what each TIDE setting does, in short. The full detail is in the [TIDE guide](TIDE_GUIDE.md), section 2 (AUTO TIDE) and 3 (TIDE SETTINGS). These rows work the same on 1.0.0-b2 and later; DRONE is new in b4.

**Where:** HOME's round is MORPH → TIDE SETTINGS → AUTO TIDE. On AUTO TIDE, K1 starts or stops the glide and K2 is the mode. On TIDE SETTINGS, K1 picks a row and OCT+ / OCT- (or K2..K4) change it.

## The tide mode (AUTO TIDE, K2): what happens at each arrival

| Mode | In short | Good for |
|---|---|---|
| ONCE | One trip from A to B, then it stays. | A single build or transition. |
| LOOP | Back and forth between the same A and B. | A steady two-state groove. |
| DRIFT | The end it just left is rolled anew around your sound. | Small, endless variations of one sound. |
| WANDER | Walks a little further from your sound each trip, always measured from your original, so it can't run away. | Slow evolution that stays recognisable. |
| JOURNEY | Travels through other factory sounds of the same engine and the same role (pad stays pad, bass stays bass). | Long sets that change sound over time. **Default.** |

TIME (K3) is how many bars one trip takes; WAIT (K4) is how many bars it rests at each arrival.

## TIDE SETTINGS rows

| Row | Values (default **bold**) | In short |
|---|---|---|
| **STYLE** | **DUB**, DEEP, MINIMAL, AMBIENT, PEAK, DRONE, CUSTOM | A preset for the whole tide: sets the mode, TIME, WAIT, VARY BARS, TURNS, POLY, SYNC, ARC and WASH in one go. Change any of those by hand and STYLE reads CUSTOM. |
| **VARY BARS** | **EVERY 4TH**, EVERY BAR | How often the pattern plays its variation: every 4th bar of the phrase, or every bar. |
| **ROLL** | **MUSIC**, SMART, WILD | How a new destination sound (End B) is made. MUSIC: musical dials inside the range that sound lives in. SMART: DUELL's own safe roll. WILD: no safety rules. |
| **TURNS** | TOGETHER, TAKE TURNS, **LEAD** | How the two parts share the variations. TOGETHER: both vary on the same bars. TAKE TURNS: one varies, the other answers two bars later. LEAD: the parts take turns leading, the other plays thinner and softer meanwhile. |
| **POLY** | OFF, **GENTLE**, WIDE | Now and then one part's loop gets a little shorter (15 or 14 of 16 steps; WIDE: 12 to 15), so its motif drifts against the beat, then lines up again at a phrase. Your stored pattern is never changed. |
| **SYNC** | OFF, **SOFT**, PUSH | Pattern edits lean hits towards the off-beat 8th, with an occasional push into the beat (SOFT one a bar, PUSH two). Beats never move. |
| **ARC** | OFF, **RISE**, DROP | A 32 to 64 bar shape: builds up (more syncopation, brighter), peaks, eases into a breakdown, recovers. DROP also drops out (no new notes for two bars) on about half of the arcs. Hand drop: hold OCT- and turn K2 on AUTO TIDE. |

TURNS, POLY, SYNC and ARC work only while the tide runs in WANDER or JOURNEY with ROLL MUSIC and the pattern in TARGET (BOTH or SEQ). Otherwise their values are dim and the top line says what's missing.

## The six STYLEs

| Style | Mode | TIME | WAIT | VARY | TURNS | POLY | SYNC | ARC | WASH | Feel |
|---|---|---|---|---|---|---|---|---|---|---|
| DUB | JOURNEY | 16 | 2 | 4TH | LEAD | GENTLE | SOFT | RISE | OFF | Dub-techno: slow journey, call and answer. |
| DEEP | WANDER | 32 | 4 | 4TH | TAKE TURNS | OFF | SOFT | RISE | LOW | Long, deep, stays close to your sound. |
| MINIMAL | WANDER | 8 | OFF | BAR | LEAD | WIDE | PUSH | OFF | OFF | Busy, shifting, polymetric. |
| AMBIENT | JOURNEY | 64 | 8 | 4TH | TOGETHER | GENTLE | OFF | OFF | HIGH | Very slow, washy, no groove tricks. |
| PEAK | JOURNEY | 8 | OFF | BAR | LEAD | GENTLE | PUSH | DROP | OFF | Fast changes, pushes, drops. |
| DRONE | WANDER | 64 | OFF | 4TH | TOGETHER | OFF | OFF | OFF | HIGH | For a held drone chord: always moving, slowly, around your own sound; nothing rhythmic. Since 1.0.0-b4. |
