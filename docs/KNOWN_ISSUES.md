# Known issues: TIDE

What is known to be broken or untested. [KNOWN_LIMITS.md](KNOWN_LIMITS.md) lists the design limits; this page lists the bugs and the untested ground. It describes the current beta, `1.0.0-b13`; what earlier betas fixed is in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md), and [the builds page](BUILDS.md) says which build is which. Report anything else on this fork's [issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues/new/choose). The sections say where each note comes from: the TIDE fork, or the DUELL 1.0 notes of OSCILL/R that still hold.

## TIDE (this fork)

- **A big delay TIME change sweeps slowly (since `1.0.0-b9`).** With the delay's wobble on, a TIME change glides at most about 1/8 sample per sample (about 12 % off pitch): a dotted eighth to a quarter takes about a second, a bar's worth one and a half to five. Small steps settle in about a tenth of a second. Not yet heard on the unit.
- **`1.0.0-b13` is new on the unit.** The owner installed it from the site on 15 November; the louder output (+8 dB at the jacks: whether the analog stage stays clean at the new level), the 303 voices and the ACID filter, the 11 replaced sounds and the five new multis had been tested on the computer only before that. The rest was played on the owner's unit as test builds, the last on 14 November.
- **`1.0.0-b13`: the VOICE 2/2 cell overlaps the text beside it when GLIDE reads "54 ms"** (found by the screen test, item 156). Only the picture; a drawing fix is planned (roadmap).
- **TIDE has run on one FM-1, the fork owner's.** Other units, cables and USB hubs are untested. Use a data cable straight into the computer, no hub.
- **TIDE's settings are in the backup from `1.0.0-b1` on.** The TIDE SETTINGS rows ride in the settings record's reserved tail, so every backup (installer page, editor Backup page) holds them and a restore brings them back. A backup made by an older build has none: restoring it leaves the unit's TIDE settings as they are. Plain DUELL 1.1 zeroes that tail at its next settings save, and TIDE then starts on DUB again.
- **Live recording on an ARP SEQ part (since `1.0.0-b3`).** A key always sets the step's note (one note a step, no chords), and a 1- or 2-step pattern in REPLACE can split a chord across the step boundary. The key's step is predicted from the POLY stretch before the next arp step runs, so at a stretch change it can land one step off. Plain DUELL has the old ARP SEQ recording bug; the fix is TIDE-only.
- **The step display can blink a hit that doesn't play (seen in `1.0.0-b7`'s test).**
  - AUTO TIDE's step display can show one hit ahead that then doesn't play: the old M09 MINIMAL 12's 12-step blip (that multi is now HYPNO 303), under a style with ARC DROP (PEAK).
  - It is a picture only; the sound is right.
  - It was seen once in 22,158 look-aheads in the `tidesteps` test: its WANDER / ARC DROP scenario on factory slot 8 (part 2, step 6, three steps ahead).
  - The same class showed on the old set in the night build.
  - The scenario was moved from slot 8 to slot 6 to keep the suite green. To see it, put 8 back in `felucca-base/tests/tide_steps_test.c`.
- **Tested in software and by ear on one unit.** Host tests, the emulator and the editor's mock unit show that the code does what it says; how a tide sounds with a real MIDI clock has been judged on one unit only.

## Inherited from DUELL 1.0

- **Units tested.** DUELL has run on two FM-1 units, the author's, and more than 100 people have installed it through the Firmware Hub.
- **The way back to the official firmware** (V15) has worked on the author's unit about three times: the installer's return button wrote V15 and the unit booted normally. Other units are untested. Single-bank layout, no known rollback beyond that. See [RECOVERY.md](RECOVERY.md).
- **The editor and restore are not tested with many units.** The web editor (patch library, backup and restore, piano roll, wavetable files) and the installer's backup and restore have been used on the author's unit; other units, other computers and other USB-MIDI setups are untested, and the piano roll has not been used on a real unit yet. The backup covers patches, multis, patterns, the user wavetables, settings and the current music; restoring a backup on another unit has not been tried.
- **The 175-patch export and import are untested on hardware.** The editor moves a whole patch with PATCH_RAW (in pieces over USB-MIDI). The USB-MIDI timing of a 175-patch export or import, the flash timing of 175 writes and playing while importing have been tested on the computer only (simulated flash and unit), not measured on a real unit. Do not import a whole bank while you play.
- **Very rare missed audio blocks.** When you switch to a very heavy multi, a block of audio can be late once in a while; you may hear a short click. Normal loads of patches and multis do not do this.
- **The CPU figures in the documentation are a host model; the unit can read higher on some scenes.**
  - The percentages in TESTING.md, the bench and the changelog are instruction counts on a computer.
  - A count of the device code (not a measurement) says the part EQ costs about three times its model figure on the unit. The SHIM pitch shifter and the RANDOM delay cost about 1.5 times.
  - The flash cache adds an unknown part.
  - M16 STRESS TEST of 0.2.23 overloaded at one key on the author's unit, where the model said 78 %. 0.2.24 recalibrated it (TESTING.md "CPU on the unit").
  - Read the CPU bar on your unit, not the model.
- **A slot shows the factory sound of the installed firmware.** The 175 patch slots hold the factory sounds until you save over one. You can bring a factory sound back with RESTORE. Since `1.0.0-b12` every place in the PATCHES list is a slot you can save over and restore, the factory sounds #125 to #171 included; U1 to U4 are the four empty USER slots, after #124. The list is #1 to #124, U1 to U4, then #125 to #171. The factory sound a slot falls back to is the one at that place in the list of the firmware you run:

  - When a new version adds or sorts in sounds, a slot you never saved can show another sound than before.
  - A sound you saved stays in its slot number.
- **With TIDE the patch store holds 150 patches, not 175.** The patch list has 175 slots. With TIDE the store has 25 chunks of at most 6 records: 21 chunks (126 records) for slots 1 to 128 and 4 chunks (24 records) for slots 129 to 175, so it holds at most 150 saved patches, whatever their size, and at most 24 of them among #125 to #171.

  - Overwriting a patch already saved always works.
  - Saving into one more empty slot than a room holds is refused: the 127th among slots 1 to 128, or the 25th among slots 129 to 175.
  - The unit shows STORE FULL, writes nothing and touches nothing already saved.
  - The editor's store command reports an error.
  - ERASE or RESTORE a saved slot to make room.
  - Without TIDE the store has 22 chunks and holds all 128.

  K1 on PATCHES steps one slot a turn (faster with KNOB ACCEL ON). Use TYPE (knob 2: FAV, a category or an engine) to shorten the list. The editor's patch page lists the 175 slots with a search.
- **The second core is new.** Part 2 plays on the chip's second core from every power-on, with an automatic fallback to one core (see [RECOVERY.md](RECOVERY.md)). The two-core firmware has run on the author's unit and the unit fault tests were done; there is no menu switch or on-screen notice for it.
- **MIDI is untested with many devices.** It follows the spec and the host tests; real controllers, sequencers and DAWs are still to be tested.
- **WAVE user tables are new and tested on the computer only.**
  - The three slots (USR1 to USR3) were checked by host tests: flash places, CRC, power cuts, refusal while playing, backup round trip.
  - The editor's WAVETABLES tab was checked against a simulated unit.
  - The writes have not been tried on other units than the author's.
  - The editor tab has not been used to write a table on a real unit in a long session.
  - A user table is reduced to 19 frames x 64 harmonics with no phases.
  - A file with a very sharp or noisy sound loses its top and its character.
  - An empty slot plays a sine.
  - ERASE DATA erases the tables.
- **User sample slots:** gone since 0.2.9, with the sample engines. Slot 3's flash now holds the patch store; the WAVE user tables (0.2.14) sit in 24 KiB at the start of the old slot 1 range (0xA0000 to 0xA5FFF); the rest of the old sample flash (0xA6000 to 0xB9FFF) now holds the patch store's chunks 6 to 15 (80 KiB); 0xBA000 to 0xC7FFF (56 KiB) is free.
- **Version text:** the splash and ABOUT show the TIDE wordmark; ABOUT writes "TIDE" and its version beside the build date (`1.0.0-b13` now). A `+` after the version (`1.0.0-b13+`) means the build was not made from exactly the tagged release. DUELL's own version string (the "DUELL 1.1" of the base firmware) is still in the build, for INFO and the console. The release has no TEST screen; M16 is a normal musical multi (FINAL REEL); the former STRESS TEST is a test scene only.
- **What the web editor cannot set:** the output MODE (DUEL, STEREO, LAYER), CROSSFADE, SPLIT, the MORPH fader and its ROLL, SCALE, ROOT and OFF-SCALE, and the parts' MIDI channels. Set them on the unit.
