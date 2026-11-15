# Known issues: TIDE 1.0.0-b13

What is still open on the current beta, `1.0.0-b13`. [KNOWN_LIMITS.md](KNOWN_LIMITS.md) lists the design limits. Report anything else on this fork's [issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues/new/choose).

Fixed in earlier builds: see [CHANGELOG-TIDE](../CHANGELOG-TIDE.md) (every beta's fixes) and [the builds page](BUILDS.md) (which build is which).

## Open in TIDE

- **New on the unit:** the owner installed b13 on 15 November. The louder output (+8 dB at the jacks), the 303 voices, the 11 replaced sounds and the five new multis were tested on the computer before that. Whether the analog output stays clean at the new level is not checked yet.
- **MORPH TO and DUCK SRC:** a MORPH TO switches the part's DUCK SRC to the target sound's halfway, but End B does not store DUCK SRC. So a saved patch whose target ducks from another source plays the part's own source at B after loading. No factory pair shows it.
- **VOICE 2/2 page:** the cell overlaps the text beside it when GLIDE reads "54 ms". Picture only; a drawing fix is planned.
- **ACID filter drawing:** on WAVE ACID / ACSQ the filter pictures draw the resonance peak lower than you hear it (they use the old RES law). Picture only; the sound is right.
- **Delay TIME changes sweep slowly** with the delay's wobble on: a big TIME change glides for about a second (up to five for a bar's worth). Small steps settle in about a tenth of a second.
- **Live recording on an ARP SEQ part:** a key sets one note per step (no chords), a 1- or 2-step pattern in REPLACE can split a chord across steps, and at a POLY stretch change a key can land one step off.
- **The step display can show a hit that doesn't play:** seen once in tens of thousands of look-aheads in the step test (ARC DROP under WANDER). Picture only.
- **One unit:** TIDE has run on one FM-1, the fork owner's. Other units, cables and USB hubs are untested; use a data cable straight into the computer, no hub.
- **Old backups:** a backup from before `1.0.0-b1` holds no TIDE settings; restoring it leaves the unit's TIDE settings as they are.

## Open from DUELL (OSCILL/R's notes that still hold)

- **The way back to the official firmware** has worked on the author's unit a few times; other units are untested. See [RECOVERY.md](RECOVERY.md).
- **Editor, backup and restore** are tested on few units and computers; restoring a backup on another unit has not been tried. The piano roll has not been used on a real unit yet.
- **Exporting or importing all 175 patches** is tested on the computer only. Don't import a whole bank while you play.
- **Rare missed audio blocks:** switching to a very heavy multi can make a short click once in a while.
- **CPU figures in the docs are a computer model;** the unit can read higher. Trust the CPU bar on your unit.
- **Unsaved slots follow the firmware:** a slot you never saved shows the factory sound of the installed version, so a new version can show another sound there. Sounds you saved stay put. RESTORE brings a factory sound back.
- **The patch store holds 150 patches** (175 slots): at most 126 among slots 1 to 128 and 24 among slots 129 to 175. A save over the limit shows STORE FULL and changes nothing; ERASE or RESTORE a slot to make room.
- **The second core is new:** part 2 runs on it with an automatic fallback to one core; there is no on-screen notice.
- **MIDI** is untested with most controllers, sequencers and DAWs.
- **User wavetables (USR1 to USR3)** are tested on the computer and one unit. A table is reduced to 19 frames x 64 harmonics, so sharp or noisy sources lose their top; ERASE DATA erases them.
- **The web editor cannot set** the output MODE, CROSSFADE, SPLIT, the MORPH fader and ROLL, SCALE, ROOT, OFF-SCALE or the MIDI channels. Set them on the unit.
