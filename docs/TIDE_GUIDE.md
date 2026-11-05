# DUELL with TIDE: a player's guide

Five minutes. This guide describes the development line, `jam11`, as the source has it. The published beta `beta-2026-11-04` is a little older; the list at the end says what came after it.

The stable build `stable-2026-11-03` has an earlier TIDE. Its TIDE SETTINGS page is still called MORPH SETTINGS. It lacks the STYLE, TURNS, POLY, SYNC and ARC rows, and it has no JRNY role families or memory. The [README](../README.md#builds-stable-and-beta) lists the builds.

Values marked *start* are suggestions for dub and house techno backgrounds, not defaults. Defaults are named as such.

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

The screen shows the SOUND and SEQ faders with the glide's path, the 16 steps as they play, and a status line. With both parts the faders sit in two columns, SOUND and SEQ, a row per part. In the steps a hit is a bar (a chord taller), a varied hit is in the accent colour, a hit the variation drops is a dim outline, a tie is a short bar, and a chance step is striped until it has rolled, then solid if it sounded or an outline if it did not. The glide only moves while the transport plays. STOP pauses it; PLAY carries on from there.

| Knob | Name | What it does to the sound | Start |
|---|---|---|---|
| 1 | **GLIDE** | Twist right: go to B. Twist left: go to A. It starts on the next bar. **Twist two detents again while it runs and it stops where it is**; the sound stays as it is. | Right |
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
| JRNY | Travels through factory sounds of the part's own engine (FM6 too); it never changes engine. It stays in the starting sound's role family: pads and keys; bass; lead and pluck; hits (PERC); textures and drones (eight marked sounds); the other FX, one-shots (a USER record in FX counts as one). A sound with no other in its family rolls a new End B of itself, it does not leave its role. Never the other part's sound, and it skips the last 4 destinations where it can. Each part also gets its own pattern wander. |

Where two very different sounds cross, a short filter dip hides the change (WASH, below).

**OCT- + knob**: K1 goes back to your original sound and pattern (the glide stops), K3 and K4 reset TIME and WAIT to the values of the STYLE on TIDE SETTINGS (the style you last chose, DUB if none: 16 bars, 2 bars).

## 3. TIDE SETTINGS (set and forget)

K1 picks the row. **OCT+** the next value, **OCT-** the one before (knobs 2 to 4 turn it too, one step a click). A mark at the right edge shows where the six visible rows are in the list. Saved over a power cycle (about 2 s after the change while stopped; changes made during a running set are written at its next STOP, so switching the unit off mid-set loses them).

| Row | Values (default first) | What it does | Start |
|---|---|---|---|
| **STYLE** | DUB, DEEP, MINIMAL, AMBIENT, PEAK, CUSTOM | The top row. One pick sets MODE, TIME, WAIT and the rows VARY BARS, TURNS, POLY, SYNC, ARC and WASH together. TARGET, ROLL, ROLL ARP and ROLL PITCH stay yours. Changing any of those nine by hand turns STYLE into CUSTOM and keeps your values. Step onto a style and back to CUSTOM, and CUSTOM brings your values back (until a power cycle). TURNS, POLY, SYNC and ARC act only in automatic MUSIC WNDR / JRNY with SEQ in TARGET. Otherwise their values are dim, and knob 1's line says what is missing. The presets are in the table below. | DUB |
| **TARGET** | SND+SEQ, SOUND, SEQ | What the glide moves: the sound, the pattern, or both together. SND+SEQ links the two faders; the MORPH fader moved by hand drags the SEQ fader with it. | SND+SEQ |
| **VARY BARS** | EVERY 4TH, EVERY BAR | When the pattern plays its variation: on every 4th bar of the phrase (EVERY 4TH), or every bar. | EVERY 4TH |
| **ROLL** | MUSIC, SMART, WILD | How a new End B is made. MUSIC turns five dials (bright, movement, shape, space, character) inside the ranges where that kind of sound actually lives, so a brighter end is not a louder one. SMART is DUELL's own safe roll. WILD drops the safety rules. | MUSIC |
| **ROLL ARP** | OFF, ON | OFF: rolls leave the arp's rate, gate and mode alone. ON: they may move them (never turning the arp into SEQ, so a TIDE part is never silenced). | OFF |
| **ROLL PITCH** | OFF, ON | OFF: nothing that moves pitch changes, which keeps drones in tune. ON: small detune and, on TRIO, interval jumps. Only ON lets the automatic pattern edits walk a note one scale step. They walk the top note of a chord or a lead note, never a bass note under C3 or in a part whose sound is in the BASS category, and nothing on SCALE CHR. The chord voicing of VOIC does not need PITCH. | OFF |
| **WASH** | OFF, LOW, MID, HIGH (default OFF) | The breath of a trip: filter dip, send swell and a wash where sounds cross. LOW is half, MID is full, HIGH one and a half. | LOW (try it; OFF is the default) |
| **TURNS** | TOGETHER, TAKE TURNS, LEAD | Only in automatic WNDR / JRNY with both parts. TOGETHER: both parts vary on bars 4 and 8. TAKE TURNS: part 2 varies on bars 2 and 6, part 1 on 4 and 8, so each variation is answered two bars later. LEAD: part 2 leads bars 1-2 of every 4 and part 1 bars 3-4 (on some 8-bar phrases the two swap); a lead or pluck line answers by playing thinner and softer on the other part's bars (beats, held notes, the motif's own hits and about half of the rest stay), while a pad, a bass or a chord part keeps its pattern whole. Playback only: KEEP stores the whole pattern. A stored older ECHO value reads as LEAD. With VARY BARS ALL, TAKE TURNS plays like TOGETHER (every bar varies); LEAD keeps TAKE TURNS' two-bar offset for part 2's variation bars. | LEAD |
| **POLY** | OFF, GENTLE, WIDE | In automatic WNDR / JRNY one part's loop is now and then shorter, so its motif drifts against the beat, then plays whole again at a phrase. GENTLE: 15 or 14 of 16 steps, rarely. WIDE: 12 to 15, more often. Your stored pattern is never changed. | GENTLE |
| **SYNC** | OFF, SOFT, PUSH | In automatic WNDR / JRNY the pattern edits that move a hit by a 16th lean towards the "and" (the off-beat 8th) and allow an occasional push into the beat (SOFT one a bar, PUSH two). The beats never move and a hit never lands on one. With any SYNC a hit stays within two 16ths of its place, and the motif's own hits (an offbeat skank's "and"s) are never moved or dropped. | SOFT |
| **ARC** | OFF, RISE, DROP | In automatic WNDR / JRNY the tide follows a shape of 32 to 64 bars: it rises (more syncopation and repeats, faster turns, brighter sounds), holds a peak, then eases into a thinned breakdown with softer sounds and recovers. DROP also drops by itself on about half of the arcs (both parts start no notes for two bars). The status line shows the phase. STOP and PLAY only pause the arc: PLAY goes on where it was. A load (multi, project, pattern) or a K1 stop starts it again at RISE. **Hand drop:** hold OCT- and turn K2 on the AUTO TIDE page; the drop lands at the next 4-bar phrase (a second turn cancels it). | RISE |

**Dynamics.** No setting: in automatic WNDR / JRNY the hits play an eighth louder late in ARC's rise and at its peak and a quarter softer in its drop and breakdown (ARC on), and a quarter softer on a LEAD answer's bars (the two together: softer still). An accent counts as velocity 127 there and loses its accent. Playback only: KEEP stores the pattern's own velocities.

**The motif is kept.** The automatic pattern edits never drop or move the pattern's anchors: its beats, the hits on the place in the beat most of its hits take (an offbeat skank's "and"s), and more of its hits in order until three fifths are anchored (at least two). A hit never sits more than two 16ths from its stored step, so a skank keeps its own shape. The phrase gaps and moves, SYNC, LEAD and ARC's thinning all leave the anchors alone. A sustained part holds still: a pattern with a single hit (a drone or a texture) is not varied at all, and a pad of two chords a bar never gets a repeat added by the phrase edits, not even at ARC's peak.

**Loading.** A multi, project or pattern load starts as written: both loaded parts go back to End A with no variation and no pitch edits from before, ARC starts again at RISE, and a running trip begins again from A (MODE and the auto state stay).

**Changing settings while a tide plays.** There is no need to stop it; each change lands at its own moment:

| When | Settings |
|---|---|
| At once (next step or bar) | VARY BARS, WASH, TURNS, POLY, SYNC. ARC switched on starts a RISE; switched off it ends the arc. |
| At the next turn (when the trip arrives) | MODE, WAIT, TIME (also when a STYLE sets it; the page shows the new TIME at once, the bar count goes on with the trip that runs), and ROLL, ROLL ARP and ROLL PITCH (they shape the next end that is rolled or picked). |
| At the next start (knob 1) | TARGET: what the tide moves is fixed when a glide starts. |

Defaults with no saved settings (a new unit, or after ERASE DATA): STYLE DUB, that is MODE JRNY, TIME 16, WAIT 2, VARY EVERY 4TH, TURNS LEAD, POLY GENTLE, SYNC SOFT, ARC RISE, WASH OFF; and TARGET SND+SEQ, ROLL MUSIC, ROLL ARP OFF, ROLL PITCH OFF. An older saved record loads as STYLE CUSTOM with its values kept and the rows it lacks at TURNS TAKE TURNS, POLY GENTLE, SYNC SOFT, ARC OFF.

**STYLE sets** (MODE, TIME, WAIT, VARY BARS, TURNS, POLY, SYNC, ARC, WASH):

| Style | Nine rows |
|---|---|
| DUB | JRNY, 16, 2, EVERY 4TH, LEAD, GENTLE, SOFT, RISE, OFF |
| DEEP | WNDR, 32, 4, EVERY 4TH, TAKE TURNS, OFF, SOFT, RISE, LOW |
| MINIMAL | WNDR, 8, OFF, EVERY BAR, LEAD, WIDE, PUSH, OFF, OFF |
| AMBIENT | JRNY, 64, 8, EVERY 4TH, TOGETHER, GENTLE, OFF, OFF, HIGH |
| PEAK | JRNY, 8, OFF, EVERY BAR, LEAD, GENTLE, PUSH, DROP, OFF |

Dub-techno recipe: STYLE DUB, ALGORITHM BOTH (by hand: **AUTO TIDE** JRNY or WNDR, TIME 16, WAIT 2 to 4; **SETTINGS** TARGET SND+SEQ, VARY EVERY 4TH, ROLL MUSIC, ROLL ARP OFF, ROLL PITCH OFF, WASH LOW). Press PLAY, twist K1 right, walk away.

## 4. MODE (output)

DUEL (part 1 left, part 2 right, effects separate), STEREO, LAYER. This is stock DUELL, not TIDE. Dub chords on two parts: DUEL if you mix outside, STEREO for a single stereo out.

## First start

A new unit (or after ERASE DATA) comes up as multi **M02 DUB CHORD** with AUTO TIDE on JRNY and STYLE DUB. Its two 16-step patterns play on PLAY with no key held (ARP SEQ). Press PLAY.

## KEEP

In the SEQ tools, hold SEQ and press G4 (KEEP): what you hear now becomes the part's own, sound and pattern, with one UNDO. Use it to freeze a good moment, then start the next tide from there.

---

# New since the stable build

In the beta, not in the stable build `stable-2026-11-03`.

| Feature | What it does |
|---|---|
| **JRNY families and memory** | JRNY stays on the part's engine, picks within a role family (pads and keys; bass; lead and pluck; hits; textures and drones; one-shot FX; a USER record uses its own category) and skips the part's last four destinations where it can. |
| **35 more factory sounds** | #129 to #163 (dub, deep house: pads, chords, stabs, plucks, textures; three of them FM6), so JRNY has more to travel between. They are read-only places after the 128 in PATCHES. |
| **Factory patterns and multis** | Ten patterns are now chords and pads for dub techno and deep house (some with their own swing and gate); seven factory multis that had no sequence have one; M16 is CELLAR TAPE. A pattern slot you never saved over takes the new pattern at power-on. |
| **USER slots** | The last 4 patch slots and 4 pattern slots (P29 to P32) are USER slots (U1 to U4), `USER EMPTY` until you save into them. The factory sounds and patterns they stand in for stay listed after them, read-only; SAVE offers USER 1 for a read-only place. |
| **MULTIS / PATTERNS previews** | On the SAVE pages: three list rows with the picked entry opened, showing both parts (sound names, ARP / SEQ / OFF, the first 16 steps) before you load. A broken record reads DAMAGED. |
| **Live graphs** | ENV, LFO, CYCLE, filter, INSERT, MATRIX and CHORD pages show a moving marker (about 15 a second, paused above 80 % CPU; MENU > ANIMATION OFF keeps them still). An LFO 2 or CYCLE nothing reads is captioned PREVIEW. |
| **GLO part select** | On GLO pages (MIXER first), ALGORITHM picks PART 1 or PART 2. |
| **CHORD page** | Loading a sound no longer turns the chord TYPE / VOICING off; the page says NEEDS POLY or SET A SCALE when the keys cannot play the chord. |

Settings that survive a power cycle stay compatible: an older saved record loads with the new rows at their defaults.

# New since the published beta

Merged on the development line, in no published build yet (the source is the reference).

| Feature | What it does |
|---|---|
| **TURNS has three values** | TOGETHER, TAKE TURNS, LEAD. ECHO is folded into LEAD (it played like it); a stored ECHO reads as LEAD and STYLE PEAK uses LEAD. LEAD answers only on lead and pluck lines. |
| **The motif is kept** | The automatic edits leave a pattern's anchors alone and a hit never moves more than two 16ths; see TIDE SETTINGS. |
| **Dynamics** | Louder in ARC's late rise and peak, softer in its drop and breakdown and on a LEAD answer's bars. No setting. |
| **Loads start as written** | A multi, project or pattern load drops the old variation and pitch walk, puts the parts at A and starts ARC again at RISE. |
| **ARC keeps its place** | STOP and PLAY only pause the arc; a load or a K1 stop starts it again. |
| **PITCH leaves the bass alone** | No note under C3 and no BASS-category sound is walked, and nothing moves on SCALE CHR. The chord voicing of VOIC works with PITCH OFF. |
| **TIDE settings in every backup** | The TIDE SETTINGS rows ride in the settings record, so a backup holds them and a restore brings them back. A backup made by an older build holds none and leaves the unit's rows as they are. |
| **Load undo** | OCT- held and a turn of PRESETS undoes the last sound load (again: redo). Not on the TIDE pages, where OCT- plus a knob is TIDE's own reset. |
| **Chord names** | Flats in a flat key (C minor's DIA7 reads Ebmaj7, Abmaj7, Bb7), a stack of 4ths reads m7sus or msus, and DIA7 on a scale of six notes is a triad. |
