<p align="center"><img src="docs/duell-tide-cover.jpg" alt="DUELL with TIDE: DUELL 1.1 by OSCILL/R, TIDE by pssdrnk" width="960"></p>

# DUELL with TIDE

**DUELL 1.1 for the M-VAVE FM-1, plus TIDE: a morph that plays and evolves by itself.**

**Stable: [`stable-2026-11-03`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/stable) · Beta: [`beta-2026-11-04`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/beta-2026-11-04)** ([what that means](#builds-stable-and-beta))

[Try it in the browser](https://pssdrnk-cmyk.github.io/DUELL-TIDE/emu/) · [What changed](#what-changed-from-duell-11) · [Install](#install) · [All documents](docs/DOCS_INDEX.md) · [Issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues)

> **TIDE is by pssdrnk, not by OSCILL/R.** DUELL with TIDE is pssdrnk's fork of [DUELL](https://github.com/oscillr/DUELL) 1.1, which is made by **OSCILL/R**. The fork is not made, affiliated with or endorsed by OSCILL/R or M-VAVE. Please don't send TIDE problems to the DUELL author: use this fork's [issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues). DUELL and its credit belong to OSCILL/R; the code stays GPL-3.0-only.

> **Use at your own risk.** TIDE has run on **one FM-1, the fork owner's**. It has releases (stable and beta, above) but no wide testing. The FM-1 has a single copy of its firmware and no known rollback; a failed write can leave it dark, and then only a hardware recovery helps ([docs/RECOVERY.md](docs/RECOVERY.md)). Back up first. No warranty.

## Builds: stable and beta

| Channel | Build | Date | Status |
|---|---|---|---|
| **Stable** | [`stable-2026-11-03`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/stable-2026-11-03) (`187e9d0`), branch [`stable`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/stable) | 2026-11-03 | **Tested and validated** on the fork owner's FM-1. Build this one unless you want to help test. |
| **Beta** | [`beta-2026-11-04`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/beta-2026-11-04) (`77a6df2`), on branch `jam11` | 2026-11-04 | **Under test.** Adds STYLE, ARC, SYNC, TURNS LEAD / ECHO, POLY, the USER slots, 32 more sounds, new patterns and multi sequences, list previews, JRNY role families and live graphs. Known issues in this build: the SAVE > MULTIS page layout, the TIDE page step bars not always matching what plays, and a latched arp playing on after STOP (use STOP ALL). All three are fixed on `jam11` for the next beta. |

`jam11` (this page) is the development line: everything below describes the **beta**. A build becomes stable only after the fork owner has played and validated it on the unit. To build the stable one, check out the `stable` branch first (see [Install](#install)).

## The idea

DUELL's MORPH glides a sound between two versions of itself, End A and End B, with one fader. **TIDE moves that fader for you**, in time with the music: from A to B and back, each trip a little different, like a tide between two shores. The pattern can glide with the sound.

It is made for playing the FM-1 as a **background**: set a tide going, then play your drum machine, bass or sampler over it. Nothing has to be managed live, and a hand on the knob can still take over at any time.

## What it looks like

| AUTO TIDE | TIDE SETTINGS | MORPH, mid-glide |
|:---:|:---:|:---:|
| <img src="docs/tide/auto-tide-both-parts.png" width="260" alt="AUTO TIDE page with both parts gliding: WNDR to B, the SOUND and SEQ faders of parts 1 and 2 in two columns, both parts' steps, MODE WNDR, TIME 16 bars, WAIT 2 bars"> | <img src="docs/tide/morph-settings.png" width="260" alt="TIDE SETTINGS page: STYLE, TARGET BOTH, VARY BARS 4 + 8, ROLL MUSIC, ARP, PITCH; the rest of the rows scroll below"> | <img src="docs/tide/morph-gliding.png" width="260" alt="MORPH page while a tide plays: the fader at 64, ROLL, UNDO and AMOUNT 40 %"> |
| The glide's state on knob 1's line (here *WNDR to B*), with both parts: the SOUND and SEQ faders in two columns, a row per part, each part's steps as they play and the sounds they have travelled to, and MODE / TIME / WAIT on knobs 2 to 4. | STYLE on top, then what the tide moves and how it changes. Knob 1 picks the row, OCT+ / OCT- change it. | DUELL's own MORPH page follows the glide, so you see where the sound is between A and B. |

*Screens rendered from the firmware's own drawing code (GREY palette); try them live in the [web emulator](https://pssdrnk-cmyk.github.io/DUELL-TIDE/emu/).*

## Play a tide in four steps

1. Press **HOME** until **AUTO TIDE** shows. HOME's round is now **MORPH → TIDE SETTINGS → AUTO TIDE → MODE**.
2. Pick a **MODE** with knob 2, a trip length (**TIME**, in bars) with knob 3 and a rest at each end (**WAIT**) with knob 4. Turn **ALGORITHM** to BOTH to glide both parts at once.
3. Twist **knob 1** right (to B) or left (to A) and press **PLAY**. While the music plays, the glide starts on the next bar.
4. Play over it. **STOP** pauses the glide and PLAY goes on from there. Twist knob 1 while it glides and it stops where it is; the sound stays.

On the very first start TIDE comes up ready: multi **M04 DUB CHORD**, AUTO TIDE on **JRNY**, both parts, 16 bars, WAIT 2. Just press PLAY.

## What changed from DUELL 1.1

### New: AUTO TIDE and TIDE SETTINGS (in HOME's round)

**AUTO TIDE** runs the tide: knob 1 glides to B or to A, knob 2 picks the MODE (ONCE, LOOP, DRIFT, WNDR, JRNY), knob 3 the TIME of a trip (1 to 64 bars), knob 4 the WAIT at each end, and ALGORITHM picks PART 1, PART 2 or BOTH. **TIDE SETTINGS** says how it behaves: STYLE, TARGET, VARY BARS, ROLL, ARP, PITCH, WASH LEVEL, TURNS, POLY, SYNC and ARC. Every row, its values and when a change lands are in the [TIDE guide](docs/TIDE_GUIDE.md); the STYLE presets are:

| STYLE | MODE | TIME | WAIT | VARY BARS | TURNS | POLY | SYNC | ARC | WASH |
|---|---|---|---|---|---|---|---|---|---|
| **DUB** | JRNY | 16 bars | 2 bars | 4 + 8 | LEAD | GENTLE | SOFT | RISE | OFF |
| **DEEP** | WNDR | 32 bars | 4 bars | 4 + 8 | TAKE TURNS | OFF | SOFT | RISE | LOW |
| **MINIMAL** | WNDR | 8 bars | OFF | ALL | LEAD | WIDE | PUSH | OFF | OFF |
| **AMBIENT** | JRNY | 64 bars | 8 bars | 4 + 8 | TOGETHER | GENTLE | OFF | OFF | HIGH |
| **PEAK** | JRNY | 8 bars | OFF | ALL | LEAD | GENTLE | PUSH | DROP | OFF |

A new unit starts on DUB, so the bold values above are DUB's. A unit that had TIDE settings saved before STYLE existed keeps them and shows CUSTOM; the rows it lacked load as TURNS TAKE TURNS, POLY GENTLE, SYNC SOFT and ARC OFF.

### New: sounds, patterns and saving

- **32 more factory sounds**, #129 to #160 (dub, deep house: pads, chords, stabs, plucks, textures), so JRNY has more to travel between. They are read-only places after the 128 (SAVE offers USER 1).
- **Ten factory patterns** are now chords and pads for dub techno and deep house (some carry their own swing and gate). A pattern slot you never saved over takes the new pattern at power-on; a saved one is never touched. Seven factory multis that had no sequence now have one, and M16 is CELLAR TAPE.
- **USER slots**: the last four patch slots (U1 to U4) and pattern slots (P29 to P32) are empty places for your own work (`USER EMPTY` until saved). The factory sounds and patterns they stand in for stay listed after them, read-only.
- **MULTIS and PATTERNS** are plain lists; the picked entry's preview sits in a panel at the bottom of the page: both parts' sound names with ARP / SEQ / OFF and their first 16 steps (MULTIS), the pattern's 16 steps (PATTERNS), or NO SEQUENCE. The loaded entry has a star, and OCT+ runs the action (LOAD or SAVE) whose card shows its key. A record that does not load reads DAMAGED.
- **Live graphs**: the ENV, LFO, CYCLE, filter, INSERT, MATRIX and CHORD pages show a moving marker (about 15 a second; paused above 80 % CPU; MENU > ANIMATION OFF makes them still). An LFO 2 or CYCLE that nothing reads is marked PREVIEW.

### New: in the sequencer and tools

- **SEQ MORPH**: a pattern has an End A and an End B too, and the tide moves the notes with the sound.
- **ARP SEQ on PLAY**: with no key held, PLAY plays the part's pattern from the last chord held on it (or from C4).
- **KEEP** (in the SEQ tools): makes the sound and pattern you hear right now the part's own, to save or to start the next tide from.

### Fixed (also in a build without TIDE)

- **MODE / SPLIT**: turning left at OFF stays OFF (it jumped back to the middle key).
- **External MIDI clock**: tempo changes keep the phase; the progress is exact.
- **FM6**: a live operator level acts as a gain over the note-on envelope.
- **UNISON**: every oscillator of an extra voice is spread; the FM6 level follows the voices running.
- **PHYS**: the string loop is in tune.
- **CHORD keys**: loading a sound no longer turns the chord TYPE / VOICING off; the page says NEEDS POLY or SET A SCALE when the keys cannot play the chord.
- **GLO pages**: ALGORITHM picks the part (MIXER included).
- **FM6 under MORPH**: a voice ends after its release even while the morph raises an operator level (it used to keep running, silent).
- Undefined arithmetic removed from MORPH, FM6, PHYS and WAVE. Each fix has a host test.

### Under the hood

To make room for TIDE in the FM-1's flash, the fonts are Huffman-coded and UI render paths no page reaches are gone. Building with `DUELL_TIDE=0` gives DUELL 1.1 without any TIDE code; `jam/check_off.sh` checks that.

## Install

Two ways to put TIDE on an FM-1. Either way, [back up first](docs/RECOVERY.md) and use a **data** USB cable straight into the computer (no hub). The links on OSCILL/R's site (installer, emulator, editor, releases) install and show **plain DUELL**, not TIDE. To try TIDE without a unit, use [this fork's emulator](https://pssdrnk-cmyk.github.io/DUELL-TIDE/emu/).

**1. The installer page** (Chrome or Edge): <https://pssdrnk-cmyk.github.io/DUELL-TIDE/install/>. It offers the **Stable** and the **Beta** channel, taken from this fork's [GitHub releases](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases), saves a backup of the unit first, then writes the package. Steps, the checklist and the way back: [docs/INSTALL.md](docs/INSTALL.md).

**2. Build it yourself:**
1. Pick the build: `git checkout stable` for the validated one, or stay on `jam11` for the beta ([Builds](#builds-stable-and-beta)).
2. Build the package as in [felucca-base/BUILDING.md](felucca-base/BUILDING.md): `python3 tools/build.py` in `felucca-base`. TIDE is on by default. The result is `felucca-base/build/duell-1.1.fwsc`.
3. Install it with `python3 ../jam/jam_dev.py flash build/duell-1.1.fwsc` (needs `pip install mido python-rtmidi`). It saves a backup of the unit first, then writes the package with `tools/fm1_install.py`. Keep the computer awake.

While it writes, the unit shows the update screen (WRITING, a percent number, DO NOT SWITCH OFF). Don't unplug until the installer says it is done, then switch the unit off and on once.

TIDE reports itself as DUELL 1.1; the AUTO TIDE page in HOME's round shows that TIDE is on the unit.

**The way back:** install the original DUELL 1.1 package from [OSCILL/R's releases](https://github.com/oscillr/DUELL/releases) the same way (or with their installer page), or return to M-VAVE's official firmware (V15) as in [docs/INSTALL.md](docs/INSTALL.md). Your backup restores with `jam_dev.py restore` or the editor's Backup page.

---

# DUELL 1.1 reference

*From here on: DUELL 1.1 by OSCILL/R, as in the upstream README. TIDE leaves all of it as it is.*

<p align="left"><img src="docs/duell-deck.png" alt="DUELL 1.1 by OSCILL/R" width="640"></p>

## What is in it

### Two parts, three modes
Part 1 and Part 2 each have their own sound, effects, EQ, ducker and arp.

| Mode | What you get |
|---|---|
| **DUEL** | Dual mono. Part 1 hard left, part 2 hard right, isolated, effects included. |
| **STEREO** | Both parts in one stereo field, for headphones. |
| **LAYER** (the default) | Both parts on the keys at once, with velocity crossfade and key split. The heart of DUELL. |

### MORPH
Glide between two versions of the same patch with one fader. End A is the sound; End B is rolled by a smart randomizer from your starting point (ROLL, UNDO, AMOUNT). Up to 32 values of End B can be set by hand and stay when it is saved.

### Ten engines
ANALOG, PHASE, LOFI, VOICE, TRIO, WHEEL, PHYS, NOISE, FM6 and WAVE (with three user wavetables made from a WAV file in the editor). Every engine has the same filter section:
- **MULTI FILTER**: the multimode filter (CUT, RES, MODE, SLOPE, key tracking, envelope and LFO depth).
- **TWIN FILTER**: a second filter with the types OFF, LP, BP, HP, NOTCH, LADDER and COMB, its own cutoff (or an offset in whole semitones from the MULTI FILTER with LINK), resonance, DRIVE, SERIES or PARALLEL routing and a BLEND that reads MULTI, BOTH or TWIN.
- A **SUB OSC** layer (sine, triangle, square or pulse, an octave or two down), **WAVE STEPS**, and a **CYCLE** envelope that can run as a modulation source.

### Factory content
- **128 patches** (with TIDE: 160, see above), sorted by category (24 BASS, 19 LEAD, 22 PAD, 14 PLUCK, 16 KEYS, 12 PERC, 21 FX). The bank is levelled with each patch's TRIM. New in 1.0: **#25 DRIVEN SAW**, a driven lead that uses both filters in series. The list of numbers is [docs/PATCH_NUMBERS.md](docs/PATCH_NUMBERS.md).
- **16 MULTI slots**: 15 factory multis (REESE SWAMP, NIGHT ORGAN, PAD SPLIT, DUB CHORD, WHISPER TIDE, TINE STRINGS, FINAL REEL, HAZY TENSOR, ACID ETHER, WIRED SNAIL, EERIE SIGNAL, DEAD BUFFER, COLD CHAPEL, GARGLE ACID, WOOD & SKIN), their levels balanced and each with its own End B, and M16 CELLAR TAPE (dub techno: an offbeat plucked sub under minor-ninth chord stabs). Every factory multi now has a sequence or a deliberate live part; the former M16 STRESS TEST is a test scene (tests/scenes/M16_STRESS_TEST.bin).
- **32 patterns**, P01 to P32, all overwritable. Upstream fills every one with a default; with TIDE, P29 to P32 are empty USER slots (see above).
- Save any patch into any of the 128 slots; RESTORE brings the slot's factory sound back (with TIDE, the last four slots are USER slots that hold no factory sound).

### Level
A part has two levels, as on a hardware synth. The sound has a **TRIM** (its own loudness, saved with the patch) and the mixer has a **LEVEL** (your volume for the part: loading a sound does not change it, a multi remembers it). The MASTER's top is 0 dB.

### Effects
Per part: distortion, INSERT (including TAPE, VINYL, FOLD, HARD, CRUSH), EQ, a ducker with sidechain from MIDI. Shared: chorus (stereo), a delay with four modes (DIGI, PING, RAND, OCT+), six reverbs (ROOM, SPRING, HALL, SHIM, RESO, PLATE) with a low cut, BASS MONO, a master compressor and a stereo WIDTH on the GLO pages.

### Keys
Key velocity and aftertouch (KEY SENSE): the menu has **AFTERTOUCH** (on by default) and **KEY VELOCITY** (on by default; a very hard press can come out quiet, and you can switch it off; the method is Velo's, see [NOTICE.md](NOTICE.md)).

### Two cores
Part 2 plays on the chip's second core. It starts at every power-on and needs nothing from you. If the second core does not answer, misreads or stalls, DUELL plays on **one core** for the rest of that session without a break in the music; after a crash or hang that involved the second core, the next start uses one core too and the one after tries two again. The unit also keeps a limit on the number of notes per part (the NOTES budget) so that it does not crackle. There is no menu switch or on-screen notice for the second core yet.

### MIDI
A MIDI channel for each part. Clock in (AUTO, INT, USB or TRS), program change, bank select, pitch bend, mod wheel, sustain pedal and the CC map below. MIDI LEARN teaches a CC to any other parameter. Not tested with a wide range of devices.

### USB, the editor and the backup
The unit shows up as **DUELL** (USB-MIDI; USB audio input as set in the menu). The web editor, <https://oscillr.github.io/DUELL/editor/> (Chrome or Edge, unit connected; with a TIDE unit use [this fork's copy](https://pssdrnk-cmyk.github.io/DUELL-TIDE/editor/)), edits both parts live: all parameters, the ENGINE selector, the patch library for the 128 slots (export, import, drag between slots, DX7 `.syx` import), the MULTIS, a piano roll for the patterns, wavetable files and a Backup page that saves and restores everything (patches, multis, patterns, wavetables, settings). The editor has **no MIDI MAP tab yet**; assignments are made on the unit with MIDI LEARN. Try the editor without a unit with `?mock=1` on its address.

## MIDI CC map

One map for the whole unit. A CC on a part's channel moves that part; "both parts" rows answer on any channel. Generated from [docs/MIDI_MAP_DEFAULTS.csv](docs/MIDI_MAP_DEFAULTS.csv): **70** direct CCs, **23** locked and input numbers, 35 unused.

<details>
<summary><b>Direct CCs</b> (70 rows)</summary>

| CC | On the unit | MIDI standard name | Scope |
|---|---|---|---|
| 5 | GLIDE | Portamento Time | the part of the channel |
| 7 | LEVEL (mixer level of the part) | Channel Volume | the part of the channel |
| 10 | PAN | Pan | the part of the channel |
| 12 | INSERT A | Effect Control 1 | the part of the channel |
| 13 | INSERT B | Effect Control 2 | the part of the channel |
| 14 | SUB LEVEL | Undefined | the part of the channel |
| 15 | SUB OCTAVE | Undefined | the part of the channel |
| 16 | SUB WAVE | General Purpose Controller 1 | the part of the channel |
| 17 | SUB HARMONICS (the HARM card is switched off for now: no effect) | General Purpose Controller 2 | the part of the channel |
| 18 | MULTI FILTER ENV AMT | General Purpose Controller 3 | the part of the channel |
| 19 | MULTI FILTER KEY TRK | General Purpose Controller 4 | the part of the channel |
| 20 | MULTI FILTER MODE | Undefined | the part of the channel |
| 21 | TWIN CUTOFF | Undefined | the part of the channel |
| 22 | TWIN RES | Undefined | the part of the channel |
| 23 | TWIN FILTER ENV AMT | Undefined | the part of the channel |
| 24 | TWIN FILTER KEY TRK | Undefined | the part of the channel |
| 25 | TWIN FILTER TYPE | Undefined | the part of the channel |
| 26 | TWIN DRIVE | Undefined | the part of the channel |
| 27 | TWIN BLEND | Undefined | the part of the channel |
| 28 | TWIN ROUTE (SERIES / PARALLEL) | Undefined | the part of the channel |
| 29 | TWIN LINK | Undefined | the part of the channel |
| 30 | LFO 1 to TWIN CUTOFF | Undefined | the part of the channel |
| 31 | LFO 2 to TWIN CUTOFF | Undefined | the part of the channel |
| 34 | SPREAD | LSB for CC 2 | the part of the channel |
| 35 | SATURATION | LSB for CC 3 | the part of the channel |
| 36 | INSERT C | LSB for CC 4 | the part of the channel |
| 41 | MULTI FILTER SLOPE | LSB for CC 9 | the part of the channel |
| 44 | E1 (engine value 1) | LSB for CC 12 | the part of the channel |
| 45 | E2 (engine value 2) | LSB for CC 13 | the part of the channel |
| 46 | E3 (engine value 3) | LSB for CC 14 | the part of the channel |
| 47 | E4 (engine value 4) | LSB for CC 15 | the part of the channel |
| 48 | E5 (engine value 5) | LSB for CC 16 | the part of the channel |
| 49 | E6 (engine value 6) | LSB for CC 17 | the part of the channel |
| 50 | E7 (engine value 7) | LSB for CC 18 | the part of the channel |
| 51 | E8 (engine value 8) | LSB for CC 19 | the part of the channel |
| 52 | ENV 2 ATTACK | LSB for CC 20 | the part of the channel |
| 53 | ENV 2 DECAY | LSB for CC 21 | the part of the channel |
| 54 | ENV 2 SUSTAIN | LSB for CC 22 | the part of the channel |
| 55 | ENV 2 RELEASE | LSB for CC 23 | the part of the channel |
| 70 | MORPH FADER | Sound Controller 1 (Sound Variation) | the part of the channel |
| 71 | MULTI FILTER RESONANCE | Sound Controller 2 (Timbre / Harmonic Intensity) | the part of the channel |
| 72 | AMP RELEASE | Sound Controller 3 (Release Time) | the part of the channel |
| 73 | AMP ATTACK | Sound Controller 4 (Attack Time) | the part of the channel |
| 74 | MULTI FILTER CUTOFF | Sound Controller 5 (Brightness) | the part of the channel |
| 75 | AMP DECAY | Sound Controller 6 (Decay Time) | the part of the channel |
| 76 | LFO 1 RATE | Sound Controller 7 (Vibrato Rate) | the part of the channel |
| 77 | LFO 1 to PITCH | Sound Controller 8 (Vibrato Depth) | the part of the channel |
| 78 | LFO 1 FADE | Sound Controller 9 (Vibrato Delay) | the part of the channel |
| 79 | AMP SUSTAIN | Sound Controller 10 (no default) | the part of the channel |
| 80 | CHORUS RATE | General Purpose Controller 5 | both parts (any channel) |
| 81 | CHORUS DEPTH | General Purpose Controller 6 | both parts (any channel) |
| 82 | DELAY TIME | General Purpose Controller 7 | both parts (any channel) |
| 83 | DELAY FEEDBACK | General Purpose Controller 8 | both parts (any channel) |
| 85 | DELAY COLOUR | Undefined | both parts (any channel) |
| 86 | DELAY MIX | Undefined | both parts (any channel) |
| 87 | REVERB SIZE | Undefined | both parts (any channel) |
| 89 | REVERB DAMP | Undefined | both parts (any channel) |
| 90 | REVERB LOW CUT | Undefined | both parts (any channel) |
| 91 | REVERB send | Effects 1 Depth (Reverb Send) | the part of the channel |
| 92 | DELAY send | Effects 2 Depth | the part of the channel |
| 93 | CHORUS send | Effects 3 Depth (Chorus Send) | the part of the channel |
| 94 | DETUNE | Effects 4 Depth | the part of the channel |
| 95 | INSERT MIX | Effects 5 Depth | the part of the channel |
| 102 | MORPH ROLL: 0 to 42 rolls End A, 43 to 84 nothing, 85 to 127 rolls End B | Undefined | the part of the channel |
| 103 | MORPH AMOUNT | Undefined | the part of the channel |
| 114 | CYCLE MODE | Undefined | the part of the channel |
| 115 | CYCLE RISE | Undefined | the part of the channel |
| 116 | CYCLE FALL | Undefined | the part of the channel |
| 117 | CYCLE CURVE | Undefined | the part of the channel |
| 118 | CYCLE SYNC | Undefined | the part of the channel |

</details>

**Input numbers:** CC 1, 2, 4, 11 and 64 are inputs (mod wheel, breath, foot, expression, sustain pedal) that you can use as sources in the mod matrix; they cannot be assigned to parameters. CC 1 also moves the morph fader.

**Locked numbers** (the MIDI standard's own): 0 and 32 bank select, 6 and 38 data entry, 96 to 101 data increment / decrement and NRPN / RPN select, 120 to 127 the channel mode messages (all sound off, reset all controllers, local control, all notes off, omni, mono, poly).

**Unused:** 8, 56 to 63 and the others not listed.

**Learn-only parameters** have no CC of their own: the matrix choices and amounts, EQ, ARP and SEQ set-up, DUCK, the LFOs beyond the vibrato rows, the voice set-up, the master settings, UNDO, the four MORPH switches (TARGET, SMART, ARP, PITCH) and the settings that belong to both parts (BALANCE, XFADE, SPLIT, MODE). Teach a CC to one with MIDI LEARN on the unit.

## What is not in 1.1
The editor's MIDI MAP tab, switching multis with preset changes, the SUB HARMONICS card (switched off). The full list is in [docs/KNOWN_LIMITS.md](docs/KNOWN_LIMITS.md).

## Build and test

Building a device image is described in [felucca-base/BUILDING.md](felucca-base/BUILDING.md); a normal build has the two-core firmware and the update screen. Host tests: `./host_tests.sh` on Linux or macOS, `sh jam/check.sh` in Git Bash on Windows (see [docs/TESTING.md](docs/TESTING.md) and [CONTRIBUTING.md](CONTRIBUTING.md)).

## Read more

| I want to... | Read |
|---|---|
| Install or update | [docs/INSTALL.md](docs/INSTALL.md) |
| Play it | [docs/QUICKSTART.md](docs/QUICKSTART.md) |
| Play TIDE | [docs/TIDE_GUIDE.md](docs/TIDE_GUIDE.md) |
| Fix a problem | [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) |
| Recover from a failed flash | [docs/RECOVERY.md](docs/RECOVERY.md) |
| Know the limits | [docs/KNOWN_LIMITS.md](docs/KNOWN_LIMITS.md), [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) |
| See what changed | [CHANGELOG.md](CHANGELOG.md), [RELEASE_NOTES.md](RELEASE_NOTES.md) (DUELL 1.1's own notes, by OSCILL/R) |
| Read the full design | [SPEC.md](SPEC.md) |
| Find any other document | [docs/DOCS_INDEX.md](docs/DOCS_INDEX.md) (every document, its audience and whether it is current) |
| Help build it | [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md) |
| Report a bug | TIDE: this fork's [issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues/new/choose). Plain DUELL: OSCILL/R's [bug report form](https://github.com/oscillr/DUELL/issues/new/choose) |

## Credits and licence

DUELL is a fork of **Felucca** by Leo Kuroshita (@kurogedelic), Hügelton Instruments, and is licensed under the **GPL-3.0-only** ([LICENSE](LICENSE)). The whole list of thanks and of third-party parts is in [CREDITS.md](CREDITS.md), [NOTICE.md](NOTICE.md) and [felucca-base/LICENSING.md](felucca-base/LICENSING.md); the upstream commit is in [UPSTREAM_COMMIT.txt](UPSTREAM_COMMIT.txt). The DUELL logo and artwork are the work of OSCILL/R and are not part of the GPL licence; the code and the splash graphic as shipped in the firmware are GPL-3.0-only. DUELL is an independent project and is not affiliated with M-VAVE.

DUELL with TIDE (the TIDE pages, the glide and SEQ MORPH, and the fixes listed above) is a fork of DUELL 1.1 under the same GPL-3.0-only licence. It is not affiliated with OSCILL/R or M-VAVE.
