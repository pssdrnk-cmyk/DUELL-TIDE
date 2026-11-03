# DUELL-TIDE: a player's guide

Five minutes. Written from the firmware source at `30212a1` (pages and rows as they are in the code now). Values marked *start* are suggestions for dub and house techno backgrounds, not defaults. Defaults are named as such.

## The idea

Every sound has two ends, **A** (your sound) and **B** (a variation of it), and a fader between them. TIDE moves that fader by itself, bar by bar, while the music plays, and also moves the pattern the same way. Over minutes the part evolves; nothing needs your hands. You can touch it whenever you like.

HOME's round, one tap of **HOME** each: **MORPH → MORPH SETTINGS → AUTO TIDE → MODE**. **ALGORITHM** picks PART 1, BOTH or PART 2 on all of them.

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
| JRNY | Travels through other factory sounds of the same engine and kind, never the other part's. Also crosses engines (FM6 included) and gives each part its own pattern wander. |

Where two very different sounds cross, a short filter dip hides the change (WASH LEVEL, below).

**OCT- + knob**: K1 goes HOME (your original sound and pattern, the glide stops), K3 resets TIME to 8, K4 resets WAIT to OFF (these resets are not the defaults).

## 3. MORPH SETTINGS (set and forget)

K1 picks the row. **OCT+** the next value, **OCT-** the one before. Saved over a power cycle (after about 2 s, while stopped).

| Row | Values (default first) | What it does | Start |
|---|---|---|---|
| **TARGET** | BOTH, SOUND, SEQ | What the glide moves: the sound, the pattern, or both together. BOTH links the two faders; the MORPH fader moved by hand drags the SEQ fader with it. | BOTH |
| **VARY BARS** | 4 + 8, ALL | When the pattern plays its variation: on every 4th bar of the phrase (4 + 8), or every bar. | 4 + 8 |
| **ROLL** | MUSIC, SMART, WILD | How a new End B is made. MUSIC turns five dials (bright, movement, shape, space, character) inside the ranges where that kind of sound actually lives, so a brighter end is not a louder one. SMART is DUELL's own safe roll. WILD drops the safety rules. | MUSIC |
| **ARP** | OFF, ON | OFF: rolls leave the arp's rate, gate and mode alone. ON: they may move them (never turning the arp into SEQ, so a TIDE part is never silenced). | OFF |
| **PITCH** | OFF, ON | OFF: nothing that moves pitch changes. ON: small detune and, on TRIO, interval jumps. Keeps drones in tune when OFF. | OFF |
| **WASH LEVEL** | OFF, LOW, MID, HIGH (default OFF) | The breath of a trip: filter dip, send swell and a wash where sounds cross. LOW is half, MID is full, HIGH one and a half. | LOW (try it; OFF is the default) |

Defaults with no saved settings: TARGET BOTH, MODE JRNY, TIME 16, WAIT 2, VARY 4 + 8, ROLL MUSIC, ARP OFF, PITCH OFF, WASH OFF.

Dub-techno recipe: **AUTO TIDE** JRNY (or WNDR), TIME 16, WAIT 2 to 4, ALGORITHM BOTH. **SETTINGS** TARGET BOTH, VARY 4 + 8, ROLL MUSIC, ARP OFF, PITCH OFF, WASH LOW. Press PLAY, twist K1 right, walk away.

## 4. MODE (output)

DUEL (part 1 left, part 2 right, effects separate), STEREO, LAYER. This is stock DUELL, not TIDE. Dub chords on two parts: DUEL if you mix outside, STEREO for a single stereo out.

## First start

A new unit (or after ERASE DATA) comes up as multi **M04 DUB CHORD** with AUTO TIDE on JRNY. Its two 16-step patterns play on PLAY with no key held (ARP SEQ). Press PLAY.

## KEEP

In the SEQ tools, hold SEQ and press G4 (KEEP): what you hear now becomes the part's own, sound and pattern, with one UNDO. Use it to freeze a good moment, then start the next tide from there.

---

# Coming in the next build (not final)

Not on the unit yet. These are in flight on separate branches or are proposals. Names, values and defaults will change, and some may not ship. This section is updated after they merge.

| Feature | What it will do | State |
|---|---|---|
| **TIDE SETTINGS** | The MORPH SETTINGS page is renamed TIDE SETTINGS. Same rows. | Built (branch `tide-turns`) |
| **TURNS** | New row. TOGETHER / TAKE TURNS (default) / LEAD / ECHO. TAKE TURNS: in automatic WNDR/JRNY with both parts, part 2 varies on bars 2 and 6, part 1 on 4 and 8, so each variation is answered two bars later. TOGETHER: both on 4 and 8. Only in automatic WNDR/JRNY with both parts. LEAD and ECHO are the ANSWER rows below. | Built, not merged |
| **POLY** | New row: OFF / GENTLE (default) / WIDE. One part now and then plays a shorter loop (GENTLE: 15 or 14 of 16 steps; WIDE: 12 to 15), so its motif drifts against the beat, then plays whole again at a phrase. Your stored pattern is never changed. | Built, not merged |
| **SYNC** | New row: OFF / SOFT / PUSH. Movable hits lean towards the "and" of the beat and sometimes push a 16th ahead of it; the beats stay put. | Proposal only |
| **ANSWER** | TURNS gains two values, LEAD and ECHO: LEAD (one part leads for two bars while the other plays a thinned version, then they swap) and ECHO (the answer slowly copies the call's rhythm into the gaps). | Proposal only |
| **ARC** | New row: OFF / RISE / DROP. A 30 to 60 bar shape: build, peak, optional 2-bar drop, sparse breakdown, recovery. **Drop gesture:** hold OCT- and turn K2 on the TIDE page (AUTO TIDE), and the drop lands at the next 4-bar phrase (a second turn cancels it). Sound bias is brightness only at first. | Proposal only |
| **STYLES** | New top row on TIDE SETTINGS: DUB / DEEP / MINIMAL / AMBIENT / PEAK / CUSTOM. One pick sets MODE, TIME, WAIT, VARY BARS, TURNS, POLY, SYNC, ARC and WASH together (TARGET, ROLL, ARP, PITCH stay yours). DUB equals today's defaults and is the first-start style. Hand-editing any of those rows turns it into CUSTOM. Ships after SYNC, ANSWER and ARC. | Proposal only |
| **MULTIS / PATTERNS previews** | On the SAVE pages: three list rows with the picked entry opened, showing both parts (sound names, ARP/SEQ/OFF, the first 16 steps) before you load. A broken record reads DAMAGED. | Built, not merged |
| **USER slots** | The last 4 patch slots and 4 pattern slots become USER slots (U1 to U4), empty until you save into them. The factory sounds/patterns they displace stay listed after them, read-only. | In progress |
| **GLO part select** | On GLO pages (MIXER first), ALGORITHM now picks PART 1 or PART 2. | Built, not merged |

Settings that survive a power cycle will stay compatible: an older saved record loads with the new rows at their defaults.
