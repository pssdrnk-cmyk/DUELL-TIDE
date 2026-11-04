# DUELL with TIDE: a player's guide

Five minutes. Written from the firmware source (pages and rows as they are in the code). It describes the **beta** line, `jam11`. The stable build `stable-2026-11-03` has an earlier TIDE: its TIDE SETTINGS page is still called MORPH SETTINGS and lacks the STYLE, TURNS, POLY, SYNC and ARC rows, and it has no JRNY role families or memory (see the end; [README](../README.md#builds-stable-and-beta) lists the builds). Values marked *start* are suggestions for dub and house techno backgrounds, not defaults. Defaults are named as such.

## The idea

Every sound has two ends, **A** (your sound) and **B** (a variation of it), and a fader between them. TIDE moves that fader by itself, bar by bar, while the music plays, and also moves the pattern the same way. Over minutes the part evolves; nothing needs your hands. You can touch it whenever you like.

HOME's round, one tap of **HOME** each: **MORPH → TIDE SETTINGS → AUTO TIDE → MODE**. **ALGORITHM** picks PART 1, BOTH or PART 2 on all of them.

## 1. MORPH (the hand page)

| Knob | What it does |
|---|---|
| 1 | The fader, A to B. Moving it by hand stops any glide on it. |
| 2 | ROLL: left makes a new A, right a new B. One click, a new random end. |
| 3 | UNDO the last roll of the end you rolled. |
| 4 | AMOUNT: how far a roll may go. *Start: leave it.* The musical roll has its own sensible scale. |

## 2. AUTO TIDE (the page you live on)

The screen shows the SOUND and SEQ faders with the glide's path, the 16 steps playing, and a status line. The glide only moves while the transport plays. STOP pauses it; PLAY carries on from there.

| Knob | Name | What it does to the sound | Start |
|---|---|---|---|
| 1 | **GLIDE** | Twist right: go to B. Twist left: go to A. It starts on the next bar. **Twist again while it runs and it stops where it is**; the sound stays as it is. | Right |
| 2 | **MODE** | What happens at each arrival (below). | JRNY (default) |
| 3 | **TIME** | How many bars one trip takes: 1, 2, 4, 8, 16, 32 or 64. Longer trips also breathe more: the filter dips and the sends swell more over a long trip (full depth from 16 bars). Default 16. | 16 or 32 |
| 4 | **WAIT** | Bars to hold at each arrival before the next trip: OFF, 1, 2, 4, 8 or 16. Default 2. | 2 to 8 |

**MODE**

| Mode | Heard as |
|---|---|
| ONCE | One trip, then stillness. |
| LOOP | Back and forth between A and B. |
| DRIFT | The end it left is rolled again from your sound: new twists around the same centre. |
| WNDR | Walks away from your sound, a little further each trip; the steps grow with the phrase (16, 32, 64, 128 bars). Always computed from your original, so it can't run away. |
| JRNY | Travels through factory sounds of the part's own engine (FM6 too); it never changes engine. It stays in the starting sound's role family: pads, keys and the rest; bass; lead and pluck; perc and FX. Another sound of that engine only if the family has none left. Never the other part's sound, and it skips the last 4 destinations where it can. Each part also gets its own pattern wander. |

Where two very different sounds cross, a short filter dip hides the change (WASH LEVEL, below).

**OCT- + knob**: K1 goes HOME (your original sound and pattern, the glide stops), K3 resets TIME to 8, K4 resets WAIT to OFF (these resets are not the defaults).

## 3. TIDE SETTINGS (set and forget)

K1 picks the row. **OCT+** the next value, **OCT-** the one before. Saved over a power cycle (after about 2 s, while stopped).

| Row | Values (default first) | What it does | Start |
|---|---|---|---|
| **STYLE** | DUB, DEEP, MINIMAL, AMBIENT, PEAK, CUSTOM | The top row. One pick sets MODE, TIME, WAIT and the rows VARY BARS, TURNS, POLY, SYNC, ARC and WASH LEVEL together (TARGET, ROLL, ARP and PITCH stay yours). Changing any of those nine by hand turns it into CUSTOM and keeps your values; CUSTOM itself changes nothing. Table below. | DUB |
| **TARGET** | BOTH, SOUND, SEQ | What the glide moves: the sound, the pattern, or both together. BOTH links the two faders; the MORPH fader moved by hand drags the SEQ fader with it. | BOTH |
| **VARY BARS** | 4 + 8, ALL | When the pattern plays its variation: on every 4th bar of the phrase (4 + 8), or every bar. | 4 + 8 |
| **ROLL** | MUSIC, SMART, WILD | How a new End B is made. MUSIC turns five dials (bright, movement, shape, space, character) inside the ranges where that kind of sound actually lives, so a brighter end is not a louder one. SMART is DUELL's own safe roll. WILD drops the safety rules. | MUSIC |
| **ARP** | OFF, ON | OFF: rolls leave the arp's rate, gate and mode alone. ON: they may move them (never turning the arp into SEQ, so a TIDE part is never silenced). | OFF |
| **PITCH** | OFF, ON | OFF: nothing that moves pitch changes. ON: small detune and, on TRIO, interval jumps. Keeps drones in tune when OFF. | OFF |
| **WASH LEVEL** | OFF, LOW, MID, HIGH (default OFF) | The breath of a trip: filter dip, send swell and a wash where sounds cross. LOW is half, MID is full, HIGH one and a half. | LOW (try it; OFF is the default) |
| **TURNS** | TOGETHER, TAKE TURNS, LEAD, ECHO | Only in automatic WNDR / JRNY with both parts. TOGETHER: both parts vary on bars 4 and 8. TAKE TURNS: part 2 varies on bars 2 and 6, part 1 on 4 and 8, so each variation is answered two bars later. LEAD: part 2 leads bars 1-2 of every 4 and part 1 bars 3-4, and the other part plays a thinned version of its pattern (beats, held notes and about half of the other hits stay). ECHO: as LEAD, and the answer also leans its hits into the places the call left free. Playback only: KEEP stores the whole pattern. | LEAD |
| **POLY** | OFF, GENTLE, WIDE | In automatic WNDR / JRNY one part's loop is now and then shorter, so its motif drifts against the beat, then plays whole again at a phrase. GENTLE: 15 or 14 of 16 steps, rarely. WIDE: 12 to 15, more often. Your stored pattern is never changed. | GENTLE |
| **SYNC** | OFF, SOFT, PUSH | In automatic WNDR / JRNY the pattern edits that move a hit by a 16th lean towards the "and" (the off-beat 8th) and allow an occasional push into the beat (SOFT one a bar, PUSH two). The beats never move and a hit never lands on one. | SOFT |
| **ARC** | OFF, RISE, DROP | In automatic WNDR / JRNY the tide follows a shape of 36 to 64 bars: it rises (more syncopation and repeats, faster turns, brighter sounds), holds a peak, then eases into a thinned breakdown with softer sounds and recovers. DROP also drops by itself on about half of the arcs (both parts start no notes for two bars). The status line shows the phase. **Hand drop:** hold OCT- and turn K2 on the AUTO TIDE page; the drop lands at the next 4-bar phrase (a second turn cancels it). | RISE |

Defaults with no saved settings (a new unit, or after ERASE DATA): STYLE DUB, that is MODE JRNY, TIME 16, WAIT 2, VARY 4 + 8, TURNS LEAD, POLY GENTLE, SYNC SOFT, ARC RISE, WASH OFF; and TARGET BOTH, ROLL MUSIC, ARP OFF, PITCH OFF. An older saved record loads as STYLE CUSTOM with its values kept and the rows it lacks at TURNS TAKE TURNS, POLY GENTLE, SYNC SOFT, ARC OFF.

**STYLE sets** (MODE, TIME, WAIT, VARY BARS, TURNS, POLY, SYNC, ARC, WASH LEVEL):

| Style | Nine rows |
|---|---|
| DUB | JRNY, 16, 2, 4 + 8, LEAD, GENTLE, SOFT, RISE, OFF |
| DEEP | WNDR, 32, 4, 4 + 8, TAKE TURNS, OFF, SOFT, RISE, LOW |
| MINIMAL | WNDR, 8, OFF, ALL, LEAD, WIDE, PUSH, OFF, OFF |
| AMBIENT | JRNY, 64, 8, 4 + 8, TOGETHER, GENTLE, OFF, OFF, HIGH |
| PEAK | JRNY, 8, OFF, ALL, ECHO, GENTLE, PUSH, DROP, OFF |

Dub-techno recipe: STYLE DUB, ALGORITHM BOTH (by hand: **AUTO TIDE** JRNY or WNDR, TIME 16, WAIT 2 to 4; **SETTINGS** TARGET BOTH, VARY 4 + 8, ROLL MUSIC, ARP OFF, PITCH OFF, WASH LOW). Press PLAY, twist K1 right, walk away.

## 4. MODE (output)

DUEL (part 1 left, part 2 right, effects separate), STEREO, LAYER. This is stock DUELL, not TIDE. Dub chords on two parts: DUEL if you mix outside, STEREO for a single stereo out.

## First start

A new unit (or after ERASE DATA) comes up as multi **M04 DUB CHORD** with AUTO TIDE on JRNY and STYLE DUB. Its two 16-step patterns play on PLAY with no key held (ARP SEQ). Press PLAY.

## KEEP

In the SEQ tools, hold SEQ and press G4 (KEEP): what you hear now becomes the part's own, sound and pattern, with one UNDO. Use it to freeze a good moment, then start the next tide from there.

---

# New since the stable build

In the beta, not in the stable build `stable-2026-11-03`.

| Feature | What it does |
|---|---|
| **JRNY families and memory** | JRNY stays on the part's engine, picks within a role family (pads, keys and the rest; bass; lead and pluck; perc and FX) and skips the part's last four destinations where it can. |
| **32 more factory sounds** | #129 to #160 (dub, deep house: pads, chords, stabs, plucks, textures), so JRNY has more to travel between. They are read-only places after the 128 in PATCHES. |
| **Factory patterns and multis** | Ten patterns are now chords and pads for dub techno and deep house (some with their own swing and gate); seven factory multis that had no sequence have one; M16 is CELLAR TAPE. A pattern slot you never saved over takes the new pattern at power-on. |
| **USER slots** | The last 4 patch slots and 4 pattern slots (P29 to P32) are USER slots (U1 to U4), `USER EMPTY` until you save into them. The factory sounds and patterns they stand in for stay listed after them, read-only; SAVE offers USER 1 for a read-only place. |
| **MULTIS / PATTERNS previews** | On the SAVE pages: three list rows with the picked entry opened, showing both parts (sound names, ARP / SEQ / OFF, the first 16 steps) before you load. A broken record reads DAMAGED. |
| **Live graphs** | ENV, LFO, CYCLE, filter, INSERT, MATRIX and CHORD pages show a moving marker (about 15 a second, paused above 80 % CPU; MENU > ANIMATION OFF keeps them still). An LFO 2 or CYCLE nothing reads is captioned PREVIEW. |
| **GLO part select** | On GLO pages (MIXER first), ALGORITHM picks PART 1 or PART 2. |
| **CHORD page** | Loading a sound no longer turns the chord TYPE / VOICING off; the page says NEEDS POLY or SET A SCALE when the keys cannot play the chord. |

Settings that survive a power cycle stay compatible: an older saved record loads with the new rows at their defaults.
