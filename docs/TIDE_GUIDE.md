# DUELL with TIDE: a player's guide

Five minutes. This guide describes the development line, `jam11`, as the source has it. It is the betas `1.0.0-b1` to `1.0.0-b6` and what is merged after them for `1.0.0-b7` (the shorter TIDE SETTINGS page is from there). The older beta `beta-2026-11-04` is a little different; the list at the end says what came after it. The version of your unit is in MENU > SYSTEM > ABOUT, beside the date.

The stable build `stable-2026-11-03` has an earlier TIDE. Its TIDE SETTINGS page is still called MORPH SETTINGS. It lacks STYLE and ARC, and it has no JOURNEY role families or memory. The [README](../README.md#builds-stable-and-beta) lists the builds.

Values marked *start* are suggestions for dub and house techno backgrounds, not defaults. Defaults are named as such.

## The idea

Every sound has two ends, **A** (your sound) and **B** (a variation of it), and a fader between them. TIDE moves that fader by itself, bar by bar, while the music plays, and also moves the pattern the same way. Over minutes the part evolves; nothing needs your hands. You can touch it whenever you like.

HOME's round, one tap of **HOME** each: **MORPH → TIDE SETTINGS → AUTO TIDE**. **ALGORITHM** picks PART 1, both parts or PART 2 on all of them.

**ALGORITHM and the play mode** (1.0.0-b7). On MORPH, TIDE SETTINGS, AUTO TIDE and in PERFORM it has three positions, a click each: PART 1, the middle, PART 2. The middle selects both parts and plays the mode the **PLAY MODE** row of TIDE SETTINGS is set to, and it is named after it: PART 1, LAYER, PART 2 (or PART 1, STEREO, PART 2; PART 1, DUEL, PART 2).

| Position | The keys play | The knobs, the fader and the glide act on |
|---|---|---|
| **PART 1** | part 1 alone, on every key, whatever PLAY MODE says | part 1 |
| **the middle: LAYER** | both parts on every key (with SPLIT set: below the split part 1, from it part 2) | both parts, each from its own value |
| **the middle: STEREO** | with SPLIT set: below the split part 1, from it part 2; SPLIT OFF: the last single part you picked | both parts |
| **the middle: DUEL** | as STEREO, with part 1 hard left and part 2 hard right | both parts |
| **PART 2** | part 2 alone, on every key | part 2 |

In the middle the header's title is LAYER, STEREO or DUEL, in the theme colour, and both part numbers light. Changing the PLAY MODE row also moves ALGORITHM to the middle, so you hear the new mode at once. A multi saved in LAYER loads in the middle. Nothing new is stored: the middle is the BOTH of before. **With PLAY MODE on LAYER, a move between a side and the middle lets go of the notes you hold** (a short dip): a held chord stops, play it again. With STEREO or DUEL the held notes stay. The edit pages still pick PART 1 or PART 2 only.

On these three pages a small dim pill beside part 1's capsule shows the busiest audio block of the last 4 seconds, in % of its time. A red badge beside part 2's capsule shows audio blocks missed recently (a click or dropout was possible); it goes away after a minute with no new miss.

## 1. MORPH (the hand page)

| Knob | What it does |
|---|---|
| 1 | **MORPH**, the fader from A to B. Two steps a click: a full sweep is half the turning of 1.0.0-b6. Moving it by hand holds that part's glide; TIDE takes it back after 8 untouched bars, at the next phrase (see "Your hand on the MORPH fader", section 3). |
| 2 | **FILTER**: the part's cutoff, the same as knob 1 of QUICK EDIT (hold EDIT). |
| 3 | **ECHO**: the part's delay send. Dim when the delay can't be heard (GLO > MODULES SENDS OFF, or the delay's MIX at 0); knob 1's line says which. |
| 4 | **LENGTH**: the release of the amp envelope, as the ENV page shows it (FM6 and ENV SYNC have their own values). |

These are the sound's own settings: they record into the pattern with REC armed, MIDI LEARN picks them, SAVE keeps them, and turning them never holds TIDE's glide (only the MORPH fader does). With ALGORITHM in the middle (both parts) every knob moves each part from its own value by the same amount, so neither part jumps.

**Hold OCT+ and click knob 1** (either way): End B becomes a new random roll; the fader stays where it is and the octave doesn't move. A roll replaces its end; there is no UNDO. How far a roll goes is the AMOUNT row of TIDE SETTINGS.

**Hold OCT- and turn a knob:** knob 1 goes back to your original sound and pattern (the glide stops, as OCT- + K1 on AUTO TIDE); knobs 2 to 4 go back to the value the loaded sound had.

The fader's position (A, 1 to 126, B) sits beside knob 1's label; the slim row under the picture shows knobs 2 to 4 as FLT, ECHO and LEN, each with its value, and the footer shows both parts' patches. Most of the page shows the sound itself as waves, like an oscilloscope: a held note stands still, a chord beats, the newest wave in front, the older ones fainter behind it; at rest the lines lie flat. ALGORITHM picks whose: PART 1, PART 2, or both (the middle: one above the other). **Hold SAVE** saves the patch; with both parts selected each part goes over its own slot.

## 2. AUTO TIDE (the page you live on)

The screen shows the SOUND and SEQ faders with the glide's path, the 16 steps as they play, and a status line. The page always shows both parts: the faders sit in two columns, SOUND and SEQ, a row per part, and the steps in two rows. The part the knobs act on (the selected part) is drawn in full colour with its number in a filled pill and a small bar beside its steps; the other part is dimmed. In the steps a hit is a bar (a chord taller), a varied hit is in the accent colour, a hit the variation drops is a dim outline, a tie is a short bar, and a chance step is striped until it has rolled, then solid if it sounded or an outline if it did not. The glide only moves while the transport plays. STOP pauses it; PLAY carries on from there.

| Knob | Name | What it does to the sound | Start |
|---|---|---|---|
| 1 | **GLIDE** | Twist right: go to B. Twist left: go to A. It starts on the next bar. **Twist two detents again while it runs and it stops where it is**; the sound stays as it is. | Right |
| 2 | **MODE** (K2) | What happens at each arrival (below). The MODE row of TIDE SETTINGS is the same setting. | WANDER (default, from STYLE DUB) |
| 3 | **TIME** | How many bars one trip takes: 1, 2, 4, 8, 16, 32 or 64. Longer trips also breathe more: the filter dips and the sends swell more over a long trip (full depth from 16 bars). Default 16. | 16 or 32 |
| 4 | **WAIT** | Bars to hold at each arrival before the next trip: OFF, 1, 2, 4, 8 or 16. Default 2. | 2 to 8 |

**MODE**

| Mode | Heard as |
|---|---|
| ONCE | One trip, then stillness. |
| LOOP | Back and forth between A and B. |
| DRIFT | The end it left is rolled again from your sound: new twists around the same centre. |
| WANDER | Walks away from your sound, a little further each trip; the steps grow with the phrase (16, 32, 64, 128 bars). Always computed from your original, so it can't run away. |
| JOURNEY | Travels through factory sounds of the part's own engine (FM6 too); it never changes engine. It stays in the starting sound's role family: pads and keys; bass; lead and pluck; hits (PERC); textures and drones (eight marked sounds); the other FX, one-shots (a USER record in FX counts as one). It also keeps the starting sound's attack: a pad (ATK 60 or more) goes to other pads, a stab (SUS 20 or less) to other stabs, keys to keys; where the family has no other sound of that attack it takes any sound of the family. A sound with no other in its family rolls a new End B of itself, it does not leave its role. Never the other part's sound, and it skips the last 4 destinations where it can. Of those, it keeps to the 2 to 4 that play nearest your sound in loudness and brightness, so the background keeps its level. Each part also gets its own pattern wander. |

Where two very different sounds cross, a short filter dip hides the change (the STYLE's wash, below). On a trip of 8 bars or more the sound closes over 2 bars, stays closed for 1 and opens over 2 (on a 1 to 4 bar trip, across the trip); FM6 sounds keep their sparkle through it. JOURNEY is the mode of STYLE AMBIENT and PEAK, or yours by hand; DUB, DEEP, MINIMAL and DRONE WANDER.

**OCT- + knob**: K1 goes back to your original sound and pattern (the glide stops), K2 drops the ARC at the next phrase (see ARC), K3 and K4 reset TIME and WAIT to the values of the STYLE on TIDE SETTINGS (the style you last chose, DUB if none: 16 bars, 2 bars). The page says what it set: TIME 16 or WAIT 2 (WAIT OFF).

## 3. TIDE SETTINGS (set and forget)

Ten rows: STYLE, MODE, TARGET, ARC, ROLL TYPE and AMOUNT for the tide, then REC, PLAY MODE, XFADE and SPLIT. K1 picks the row. **OCT+** the next value, **OCT-** the one before (knobs 2 to 4 turn it too, one step a click; on AMOUNT, PLAY MODE, XFADE and SPLIT a knob moves by the clicks you turn, while OCT+ and OCT- step AMOUNT by 5 %, XFADE by 10 % and SPLIT by an octave, and out of SPLIT OFF the first OCT+ lands on MIDDLE, the next on the middle key's note). Six rows show at a time; a mark at the right edge shows where they are in the list. A value that does nothing in the present setup is dim, and knob 1's line says why. Saved over a power cycle (about 2 s after the change while stopped; changes made during a running set are written at its next STOP, so switching the unit off mid-set loses them).

| Row | Values (default first) | What it does | Start |
|---|---|---|---|
| **STYLE** | DUB, DEEP, MINIMAL, AMBIENT, PEAK, DRONE, CUSTOM | The top row. One pick sets MODE, TIME, WAIT and ARC, and the feel of the pattern that has no row of its own (below). TARGET and ROLL TYPE stay yours. Changing MODE, TIME, WAIT or ARC by hand turns STYLE into CUSTOM and keeps your values and the feel of the style you came from. Step onto a style and back to CUSTOM, and CUSTOM brings your values back (until a power cycle). The presets are in the table below. | DUB |
| **MODE** | ONCE, LOOP, DRIFT, WANDER, JOURNEY (default WANDER) | The AUTO TIDE mode, the same setting as K2 on AUTO TIDE (section 2). The default comes from STYLE DUB: WANDER. | WANDER |
| **TARGET** | BOTH, SOUND, SEQ | What the glide moves: the sound, the pattern, or both together. BOTH links the two faders; the MORPH fader moved by hand drags the SEQ fader with it. | BOTH |
| **ARC** | OFF, RISE, DROP | In automatic WANDER / JOURNEY with the pattern in TARGET the tide follows a long shape made of 8-bar phrases: it rises (more syncopation and repeats, brighter sounds), holds a peak, then eases into a breakdown, and recovers. One arc is 40 to 64 bars in 8-bar steps: RISE 16 to 32 bars, PEAK 8 to 16, BREAK 8 to 16 (a DROP is its first 8: two silent bars, then thinned), RECOVER 8. In the breakdown both parts keep their main hits, chords play two voices and everything is softer. A chord part's voicing opens at the peak and closes in the breakdown (octaves only, no new notes). DROP drops on the first arc after PLAY (about 40 seconds in), then on about half of the arcs, and the shape differs from night to night. The status line shows the phase. STOP and PLAY only pause the arc: PLAY goes on where it was. A load (multi, project, pattern) or a K1 stop starts it again at RISE. **Hand drop:** hold OCT- and turn K2 on the AUTO TIDE page; the drop lands at the next 8-bar step (a second turn cancels it). Dim with ONCE, LOOP or DRIFT, or TARGET SOUND. | RISE |
| **ROLL TYPE** | MUSIC, SMART, WILD | How a new End B is made. MUSIC turns five dials (bright, movement, shape, space, character) inside the ranges where that kind of sound actually lives, so a brighter end is not a louder one. SMART is DUELL's own safe roll. WILD drops the safety rules. It no longer switches the rhythm off: ARC and the pattern's movement act with any ROLL TYPE. Dim under JOURNEY, which picks factory sounds instead of rolling (knob 1's line: NOT JRNY). | MUSIC |
| **AMOUNT** | 0 to 100 % | How far a roll may go, the selected part's (with BOTH each part moves by the same steps). *Start: leave it.* The musical roll has its own sensible scale. A sound or multi load keeps your AMOUNT. Not part of a STYLE. It still shapes a roll in JOURNEY (OCT+ and knob 1 on the MORPH page), so it is never dim. Until 1.0.0-b7 it was knob 4 of the MORPH page. | 15 % |
| **REC** | ADD, REPLACE | Live recording (REC armed while PLAY runs), not part of any STYLE: a STYLE pick leaves it and changing it does not turn STYLE into CUSTOM. ADD: a note you play into a step that already holds notes is added to them (a chord of up to 4; a MONO, LEGATO or UNISON part keeps one note per step). REPLACE: the first note you play into a step in a pass clears that step's old notes and takes their place; more notes played into the same step in that pass (a chord played together) are added; steps you do not play into stay as they are (nothing is erased as the playhead passes). A held note still ties the following steps in both. On an ARP SEQ part a key sets the step's note, in both modes: its steps hold one note each, so a second key into the same step takes its place. Saved with the other settings. | ADD |
| **PLAY MODE** | LAYER, STEREO, DUEL | The output mode. LAYER: both parts on the keys. STEREO: both parts, one stereo field. DUEL: part 1 left, part 2 right. The header says PLAY MODE and the value. It names ALGORITHM's middle; changing it moves ALGORITHM to the middle. A change lets go of held notes. | LAYER |
| **XFADE** | 0 to 100 % | The velocity crossfade between the parts. LAYER only. | 0 |
| **SPLIT** | OFF, MIDDLE, then a note | The key split point: below it part 1, from it part 2. MIDDLE splits the keys in the middle wherever OCT- / OCT+ put them (the 14th of the 27 keys starts part 2: F#4 at octave 0; MIDI notes split at the note that key plays now). Acts in ALGORITHM's middle, in LAYER, STEREO and DUEL (with one part selected it says 2 PARTS ONLY). Out of OFF the first turn right is MIDDLE, the next the note it stands for. | OFF |

**What STYLE sets without a row.** These were rows until `1.0.0-b6`; now each STYLE has its own values (the table below) and they act in automatic WANDER / JOURNEY:

- **Vary bars** (EVERY 4TH or EVERY BAR): how often the pattern plays its variation.
- **Turns** (TOGETHER, TAKE TURNS, LEAD): a stored value that plays nothing since `1.0.0-b7`. The two parts answer each other anyway (call and answer, below).
- **Poly** (OFF, GENTLE, WIDE): now and then one part's loop is a little shorter, so its motif drifts against the beat, then lines up again at a phrase. Your stored pattern is never changed. Never on a bass part; a shortened loop ends when ARC's breakdown begins.
- **Offbeat** (OFF, SOFT, PUSH): the pattern edits that move a hit by a 16th lean towards the "and" (the off-beat 8th), with an occasional push into the beat. The beats never move.
- **Wash** (OFF, LOW, MID, HIGH): the breath of a trip: filter dip, send swell and a wash where sounds cross.

**ROLL ARP and ROLL PITCH** have no row since `1.0.0-b7` (both OFF by default: rolls leave the arp and the pitch alone, which keeps drones in tune). A unit that had one switched ON keeps it, and a learned MIDI CC can still switch them.

**No notes coming in.** With no notes coming in TIDE plays its own chord on C; send or hold one note to set its key. A part that waits for keys (a pad, HOLD ALL, an arp with nothing held) plays TIDE's own chord while AUTO TIDE runs; a key or MIDI note takes over and the part is yours until the next PLAY.

**Call and answer.** No setting. In WANDER and JOURNEY, with both parts gliding, the parts answer each other: in bars 1 to 3 of every 4 one part steps back (main hits only, chords in two voices, softer) while the other plays; they swap each bar, and bar 4 both play. A bass or a pad is never pushed back; next to one, the other part steps back on bar 3. Nothing steps back while ARC's breakdown thins.

**Your hand on the MORPH fader.** Move the MORPH fader (the MORPH page, the mod wheel or CC 70) and TIDE holds only the part you moved. The other part, the pattern's own movement and ARC go on. 8 bars after you let go, at the next phrase, TIDE takes the part back from where you left it (AUTO TIDE says HAND  BACK IN n BARS). A sound load, STOP, HOME or a K1 stop end it as before.

**Dynamics.** No setting: in automatic WANDER / JOURNEY the hits play an eighth louder late in ARC's rise and at its peak and a quarter softer in its drop and breakdown (ARC on), and softer on a part that steps back in call and answer (the two together: softer still). ARP SEQ parts, the factory chord parts, follow it too, and keep their accent. An accent counts as velocity 127 there and loses its accent. Playback only: KEEP stores the pattern's own velocities.

**The motif is kept.** The automatic pattern edits never drop or move the pattern's anchors: its beats, the hits on the place in the beat most of its hits take (an offbeat skank's "and"s), and more of its hits in order until three fifths are anchored (at least two). A hit never sits more than two 16ths from its stored step, so a skank keeps its own shape. A figure of 5 to 7 hits keeps 4 of every 5 in place (3 of 5 for busier grooves); a figure with one free hit, and the bass, keep every hit outside ARC's breakdown. The exception is a chord part's variation bar: there the rhythm cell decides (below). The phrase gaps and moves, the offbeat lean, the answers and ARC's thinning all leave the anchors alone. On a chord part's variation bar the part plays a rhythm cell from a small house and dub-techno vocabulary (the skank with a throw, 3-3-2, the Detroit two-hit and more), the one nearest its own pattern, with the pattern's own chords. With OFF-SCALE PLAY, VOIC keeps a chord's out-of-scale note; VOIC never touches the bass or a note under C3. A sustained part holds still: a pattern with a single hit (a drone or a texture) is not varied at all, and a pad of two chords a bar never gets a repeat added by the phrase edits, not even at ARC's peak.

**Loading.** A multi, project or pattern load starts as written: both loaded parts go back to End A with no variation and no pitch edits from before, ARC starts again at RISE, and a running trip begins again from A (MODE and the auto state stay).

**Changing settings while a tide plays.** There is no need to stop it; each change lands at its own moment:

| When | Settings |
|---|---|
| At once (next step or bar) | STYLE's pattern feel (vary bars, turns, poly, offbeat, wash), REC (the next note you record), PLAY MODE, XFADE, SPLIT. ARC switched on starts a RISE; switched off it ends the arc. |
| At the next turn (when the trip arrives) | MODE, WAIT, TIME (also when a STYLE sets it; the page shows the new TIME at once, the bar count goes on with the trip that runs), and ROLL TYPE (it shapes the next end that is rolled). |
| At the next start (knob 1) | TARGET: what the tide moves is fixed when a glide starts. |

Defaults with no saved settings (a new unit, or after ERASE DATA): STYLE DUB, that is MODE WANDER, TIME 16, WAIT 2, ARC RISE (and DUB's feel: vary every 4th bar, poly GENTLE, offbeat SOFT, wash OFF); and TARGET BOTH, ROLL TYPE MUSIC, REC ADD, ROLL ARP and ROLL PITCH OFF. A STYLE saved by an earlier build loads as the same style with this build's values (DUB saved by `1.0.0-b6` played JOURNEY and now plays WANDER). An older record without a STYLE loads as CUSTOM with its values kept.

**STYLE sets** (MODE, TIME, WAIT, vary bars, poly, offbeat, ARC, wash):

| Style | Eight values |
|---|---|
| DUB | WANDER, 16, 2, EVERY 4TH, GENTLE, SOFT, RISE, OFF |
| DEEP | WANDER, 32, 4, EVERY 4TH, OFF, SOFT, RISE, LOW |
| MINIMAL | WANDER, 8, OFF, EVERY BAR, WIDE, PUSH, OFF, OFF |
| AMBIENT | JOURNEY, 64, 8, EVERY 4TH, OFF, OFF, OFF, HIGH |
| PEAK | JOURNEY, 8, OFF, EVERY BAR, GENTLE, PUSH, DROP, OFF |
| DRONE | WANDER, 64, OFF, EVERY 4TH, OFF, OFF, OFF, HIGH |

DUB keeps the multi's own sounds and walks them (WANDER); JOURNEY, which travels through other factory sounds, is the mode of AMBIENT and PEAK, or yours by hand on the MODE row.

The built-in drone multis M17 DUNE HORN and M18 VOID DEEP (SAVE > MULTIS, 1.0.0-b6) each have one part on a slow sequence over the held chord (DUNE HORN: the horn swells once a bar, root, root, fourth, root; VOID DEEP: the solar wind turns every 8 to 16 seconds, root, a tone up, the fourth) while the other part holds the chord; DRONE's WANDER keeps them sparse (never more than one new note a bar, never under the chord you hold). They play as DRONE without a trip here: while one of them is the multi loaded, STYLE shows DRONE and its values are DRONE's; loading any other multi brings back the STYLE and values you had, and the saved STYLE never changes (a row you change while a drone multi is loaded lasts until then).

DRONE is for a held chord of one of the factory drone sounds (#164 to #171) with HOLD on: WANDER keeps the sound and slowly moves it between its two ends over 64 bars, never waits at an end, and leaves out everything rhythmic (poly, offbeat and ARC OFF) that a held chord would not use; wash HIGH gives the slow breath. AMBIENT is JOURNEY: it hops from sound to sound, with no groove tricks (poly OFF). CUSTOM moved from 5 to 6 in `1.0.0-b4`; an old 5 reads as CUSTOM unless its values are DRONE's.

Dub-techno recipe: STYLE DUB, ALGORITHM LAYER (by hand: **AUTO TIDE** WANDER, TIME 16, WAIT 2 to 4; **SETTINGS** TARGET BOTH, ARC RISE, ROLL TYPE MUSIC; DUB's wash is OFF, so the sounds keep their own level and space). Press PLAY, twist K1 right, walk away.

## 4. MODE (output)

DUEL (part 1 left, part 2 right, effects separate), STEREO, LAYER. This is stock DUELL, not TIDE. With TIDE there is no MODE page: PLAY MODE, XFADE and SPLIT are the last three rows of TIDE SETTINGS (REC sits between ROLL TYPE and PLAY MODE). PLAY MODE is LAYER, STEREO or DUEL. XFADE (0 to 100 %) acts only in LAYER: in DUEL and STEREO its value is dim, knob 1's line says LAYER ONLY, and turning does nothing. SPLIT (OFF, MIDDLE, then a note) acts in ALGORITHM's middle in any PLAY MODE; with one part selected XFADE and SPLIT say 2 PARTS ONLY. PLAY MODE is what ALGORITHM's middle plays (section 1). They are not part of STYLE (changing them does not turn it into CUSTOM), and they are kept as before (settings and multis). MENU > CONTROL > HOME OPENS = SETTINGS opens TIDE SETTINGS with the PLAY MODE row picked; MULTI LOAD KEEPS PLAY MODE (or SETS PLAY MODE) says whether a multi load changes these three rows. The MODE row near the top of TIDE SETTINGS and the AUTO TIDE page's K2 (ONCE, LOOP, DRIFT, WANDER, JOURNEY) are the tide's mode, a different setting. Dub chords on two parts: DUEL if you mix outside, STEREO for a single stereo out.

## First start

A new unit (or after ERASE DATA) comes up as multi **M02 DUB CHORD** with STYLE DUB (AUTO TIDE on WANDER). Its two 16-step patterns play on PLAY with no key held (ARP SEQ). Press PLAY. With no notes coming in TIDE plays its own chord on C; send or hold one note to set its key. With REC armed the pattern keeps playing; the keys sound as you play them and go into the pattern (the REC row of TIDE SETTINGS chooses ADD or REPLACE for ordinary parts).

## When the unit is busy

With heavy sounds and a moving morph the audio can get close to its limit. TIDE then backs off by itself, in steps, before it cuts anything. First a glide or a crossing moves in slightly coarser steps. Then a new note may take over a note that is already fading: the old tail stops when the new note starts. Then the delay's RANDOM and OCT UP play as the plain delay. Only if that is not enough do fading notes end early, and a held note only after the audio really missed a block. When the load is lower again it steps back one step at a time: about 1 s each for the first steps, about 3 s from LIGHT FX up (longer if the unit needed LIGHT FX again right after it ended, so the delay does not keep switching). Only LIGHT FX can be seen, in ABOUT; the other steps show nowhere. Nothing is stored and there is nothing to set.

## KEEP

In the SEQ tools, hold SEQ and press G4 (KEEP): what you hear now becomes the part's own, sound and pattern, with one UNDO. Use it to freeze a good moment, then start the next tide from there.

SAVE stores the sound as you loaded it, not the one JOURNEY (or MORPH TO) moved the part to. When you hold SAVE on a part that plays such a moved sound, the dialog says so above its question: "SAVES THE LOADED SOUND", "KEEP FIRST = AS HEARD". To save what you hear, answer NO (OCT-), do KEEP, then hold SAVE again. With BOTH the note shows when either part is moved. A part that was not moved, or that you kept, shows no note.

---

# New since the stable build

In the betas (`beta-2026-11-04` and `1.0.0-b1` to `1.0.0-b5`), not in the stable build `stable-2026-11-03`.

| Feature | What it does |
|---|---|
| **JOURNEY families and memory** | JOURNEY stays on the part's engine, picks within a role family and the starting sound's attack (pad, stab or keys; families: pads and keys; bass; lead and pluck; hits; textures and drones; one-shot FX; a USER record uses its own category) and skips the part's last four destinations where it can. |
| **43 more factory sounds** | #129 to #171 (dub, deep house: pads, chords, stabs, plucks, textures; three of them FM6; the last eight, #164 to #171, are drones, new in `1.0.0-b2`), so JOURNEY has more to travel between. They are read-only places after the 128 in PATCHES; the names are in [PATCH_NUMBERS](PATCH_NUMBERS.md). |
| **Factory patterns and multis** | Ten patterns are now chords and pads for dub techno and deep house (some with their own swing and gate); seven factory multis that had no sequence have one; CELLAR TAPE was M03 (since `1.0.0-b7` M03 is JUNO DEEP). A pattern slot you never saved over takes the new pattern at power-on. |
| **USER slots** | The last 4 patch slots and 4 pattern slots (P29 to P32) are USER slots (U1 to U4), `USER EMPTY` until you save into them. The factory sounds and patterns they stand in for stay listed, read-only; SAVE offers USER 1 for a read-only place. In the PATCHES list the factory sounds come first (#1 to #171), then the sounds you saved, then the empty USER slots last. |
| **MULTIS / PATTERNS previews** | On the SAVE pages: three list rows with the picked entry opened, showing both parts (sound names, ARP / SEQ / OFF, the first 16 steps) before you load. LOAD is already picked when the page opens, so OCT+ loads the picked entry (K3 picks SAVE). A broken record reads DAMAGED. |
| **Live graphs** | ENV, LFO, CYCLE, filter, INSERT, MATRIX and CHORD pages show a moving marker (about 15 a second, paused above 80 % CPU; MENU > ANIMATION OFF keeps them still). An LFO 2 or CYCLE nothing reads is captioned PREVIEW. |
| **GLO part select** | On GLO pages (MIXER first), ALGORITHM picks PART 1 or PART 2. |
| **CHORD page** | Loading a sound no longer turns the chord TYPE / VOICING off; the page says NEEDS POLY or SET A SCALE when the keys cannot play the chord. |

Settings that survive a power cycle stay compatible: an older saved record loads with the new rows at their defaults.

# New since the beta of 2026-11-04

Merged on the development line, in `1.0.0-b1` and after it (the source is the reference).

| Feature | What it does |
|---|---|
| **TURNS has three values** | TOGETHER, TAKE TURNS, LEAD. ECHO is folded into LEAD (it played like it); a stored ECHO reads as LEAD and STYLE PEAK uses LEAD. LEAD answers only on lead and pluck lines. (Since `1.0.0-b7` STYLE sets it; there is no row.) |
| **The motif is kept** | The automatic edits leave a pattern's anchors alone and a hit never moves more than two 16ths; see TIDE SETTINGS. |
| **Dynamics** | Louder in ARC's late rise and peak, softer in its drop and breakdown and on a LEAD answer's bars. No setting. |
| **Loads start as written** | A multi, project or pattern load drops the old variation and pitch walk, puts the parts at A and starts ARC again at RISE. |
| **ARC keeps its place** | STOP and PLAY only pause the arc; a load or a K1 stop starts it again. |
| **PITCH leaves the bass alone** | No note under C3 and no BASS-category sound is walked, and nothing moves on SCALE CHR. The chord voicing of VOIC works with PITCH OFF. |
| **The register stays** | WNDR, JRNY and TIDE's pattern edits (VOIC, voice leading, PITCH) never put a note outside the register of the pattern you wrote: from its lowest note up to its highest plus an octave, and a pattern that lives above C3 never gets a note under C3 (a skank at C4 stays a skank; a bass stays a bass and goes no lower). |
| **TIDE settings in every backup** | The TIDE SETTINGS rows ride in the settings record, so a backup holds them and a restore brings them back. A backup made by an older build holds none and leaves the unit's rows as they are. |
| **Load undo** | OCT- held and a turn of PRESETS undoes the last sound load (again: redo). Not on the TIDE pages, where OCT- plus a knob is TIDE's own reset. |
| **Chord names** | Flats in a flat key (C minor's DIA7 reads Ebmaj7, Abmaj7, Bb7), a stack of 4ths reads m7sus or msus, and DIA7 on a scale of six notes is a triad. |

# New in 1.0.0-b7

| Feature | What it does |
|---|---|
| **MORPH is the hands-on page** | Knob 1 MORPH at half the turning, knob 2 FILTER, knob 3 ECHO, knob 4 LENGTH, on the slim row under the wave picture. With both parts selected (ALGORITHM's middle) every knob moves each part by the same amount. A new End B: hold OCT+ and click knob 1. AMOUNT is a TIDE SETTINGS row now; the new-A roll is gone from the page. |
| **ALGORITHM's middle is the play mode** | PART 1, the middle, PART 2 on the HOME pages and PERFORM: the middle plays both parts in the PLAY MODE row's mode and the header names it (LAYER, STEREO or DUEL); PART 1 and PART 2 play one part on every key. Changing PLAY MODE moves ALGORITHM to the middle. |
| **SPLIT MIDDLE** | A SPLIT value after OFF: the keys split in the middle wherever OCT- / OCT+ put them. With SPLIT set, STEREO and DUEL split the keys too. |
| **A shorter TIDE SETTINGS page** | STYLE, MODE, TARGET, ARC, ROLL TYPE, AMOUNT, then REC, PLAY MODE, XFADE, SPLIT. VARY BARS, POLY, OFFBEAT and WASH are STYLE's values (no rows); ROLL ARP and ROLL PITCH have no row (OFF by default; a unit that had them ON keeps them). The tide's MODE is a row too. A saved STYLE stays that style. |
| **ROLL TYPE no longer switches the rhythm off** | ARC and the pattern's movement act in WANDER and JOURNEY whatever ROLL TYPE is. ROLL TYPE is dim under JOURNEY, where it does nothing. |
| **DUB walks your sounds** | STYLE DUB plays WANDER: the multi's own sounds stay and evolve, and the level and brightness stay steadier. AMBIENT plays without the shortened loops (poly OFF). |
| **An arc you can hear** | ARC's phases fall on 8-bar boundaries (an arc is 40 to 64 bars), the breakdown keeps each part's main hits, plays chords in two voices and drops the level by 2 to 5 dB, the first arc after PLAY drops under ARC DROP, and ARP SEQ parts follow ARC's dynamics too. In the breakdown the parts thin by voices and level; their number of hits drops less. |
| **Call and answer** | In WANDER and JOURNEY the two parts take turns bar by bar: one plays out while the other steps back to its main hits; both play the fourth bar. A bass or pad is never pushed back. TURNS plays nothing now. |
| **The bass is left alone** | No POLY and no VOIC on the bass part, a shortened loop ends when ARC's breakdown begins. |
| **Motifs stay whole** | A figure of 5 to 7 hits keeps 4 of 5 in place; a figure with one free hit keeps all of them outside the breakdown. |
| **The chord part breathes** | Its voicing opens at ARC's peak and closes in the breakdown (octaves only), and its variation bar plays a house or dub-techno rhythm cell. |
| **The dip is short** | The crossing between two sounds closes over 2 bars, stays closed for 1 and opens over 2. A part's mixer level stays yours: JOURNEY never changes it, and a part the multi turned down stays quieter on the sounds JOURNEY travels to. JOURNEY started with the fader away from A no longer swaps the sound you hear. Not as bright as hoped: the blend between two sounds still dulls mid-trip. |
| **The reel: fifteen new multis** | M01 BASIC CHORD (dub techno), M02 DUB CHORD (dub techno, rebuilt over a held pad), M03 JUNO DEEP and M04 LATE RHODES (deep house), M05 ORGAN HOUSE and M06 PIANO HOUSE (classic house), M07 DETROIT STAB and M08 HI-TECH SOUL (Detroit), M09 HYPNO 303 (acid techno, 128 BPM; was MINIMAL 12 until `1.0.0-b8`), M10 SEVEN LOOP (hypnotic techno), M11 ACID LINES (Chicago acid house, 122 BPM), M12 ROBOT FUNK and M13 DEEP SEA ARP (electro), M14 WARPED TAPE (IDM), M15 BERLIN RUN (Berlin school), M16 TWO LOOPS (ambient). A genre each; all play both parts on PLAY with no key held. Hold a key and a multi moves by its distance from C; JUNO DEEP, PIANO HOUSE, HYPNO 303 and ACID LINES are written note by note and stay in their key (a key you play sounds on top). STYLE suggestions: DUB for M01 and M02, DEEP for M03 to M05, M08 and M15, PEAK for M06, M07 and M13, MINIMAL for M09 to M12, AMBIENT for M14 and M16 (DUB, DEEP and MINIMAL keep a multi's own two sounds). |
| **Beds that play themselves** | BASIC CHORD's crackle bed, DUNE HORN's and VOID DEEP's shimmer sound on PLAY with no key held. |
| **TIDE comes back after your hand** | A hand on the MORPH or SEQ fader holds only that part; 8 bars after you let go TIDE takes it back from where you left it, and the other part, the pattern's movement and ARC never stop. AUTO TIDE says HAND  BACK IN n BARS. |
| **No keys, still music** | With no notes coming in TIDE plays its own chord on C; send or hold one note to set its key. A part that waits for keys plays TIDE's chord while AUTO TIDE runs; a key takes the part over until the next PLAY. |
