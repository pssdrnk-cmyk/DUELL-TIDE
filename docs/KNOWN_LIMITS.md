# Known limits: DUELL 1.1

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

Current fork-specific limits are listed first. The inherited DUELL notes follow. [KNOWN_ISSUES.md](KNOWN_ISSUES.md) retains older 1.0 details; its slot-capacity and free-flash claims are not current TIDE guarantees. See [the roadmap](ROADMAP.md) for source, installed-package and measurement evidence.

## TIDE limits reviewed 4 November 2026

- **JRNY stays in the part's own engine.** It never changes engine (FM6 included). It picks within the starting sound's role family (pads, keys and the rest; bass; lead and pluck; perc and FX) and falls back to the engine's other sounds only when the family is empty. It skips a part's last four destinations where it can; a small family (55 of the 66 engine and category groups still have four sounds or fewer) can repeat.
- **The parts coordinate timing and colour.** Attack prediction and parameter-based contrast do not analyse sustained harmony or effects tails. M2's upper-voice improvement applies to ordinary chord sequences, not ARP SEQ's held chord.
- **Unequal lengths are available; automatic polymeter is not.** Existing part lengths can rotate against a shared 4/4 reference. Automatic shortening/return policy and physical clock/listening acceptance remain open.
- **USER slots reuse the existing addresses; they add no capacity.** Voice slots 125 to 128 (U1 to U4) and pattern slots P29 to P32 show `USER EMPTY` until saved; the factory sounds and patterns they stand in for are read-only places after them. TIDE's store still has 21 chunks with six worst-case records each: 126 records, despite 128 addresses, so a USER slot on a full store can refuse a new save (STORE FULL). The 32 patterns share three byte-limited 11/11/10-slot objects. A record already in a USER address is kept and shown as USER.
- **Exact live-morph continuation is not promised.** Retained readback found Part 2's live End B changed after restart; the owner does not require complex continuation of that state. Intentional saved records still need to retain their contents. TIDE preferences already persist between normal boots, but their separate record is outside the current editor backup; archive/restore between flashes is a planned bounded improvement.
- **Expansion has a small measured program budget.** The installed package (`187e9d0`, boot pack included) leaves about 24.9 KB of flash (556,620 of 581,564 B; the boot pack stores the RAM images as one LZMA stream). The source since then (USER slots, previews, live graphs, 32 sounds, the TIDE rows) has not been measured against that figure here; new candidates need fresh measurements. Software/emulator evidence does not establish physical audio, MIDI clock or CPU performance.

## Inherited DUELL 1.1 limits

- **Units tested.** DUELL has run on two FM-1 units, the author's, and more than 100 people have installed it through the Firmware Hub. Cables and hubs are untested.
- **The way back (V15)** has worked for the author. There is no known rollback beyond it. See [RECOVERY.md](RECOVERY.md).
- **Pattern slots share storage.** The 32 patterns are stored in groups: one storage object holds 11 slots in 3,584 bytes, so the 11 slots of a group share that room.
- **Hidden destinations.** LFO DEPTH and DEPTH 2 do not show as matrix destinations (the depth cards of the pages move them). Two factory patches, #28 TORCH SONG and #108 YO YA, still carry an aftertouch route to the hidden DEPTH 2, so their LFO 2 does nothing without aftertouch.
- **KEY VELOCITY is on by default.** A very hard press can come out quiet; the feel has not been measured. You can switch it off in MENU > CONTROL. AFTERTOUCH is on by default too.
- **The editor has no MIDI MAP tab.** Assign CCs with MIDI LEARN on the unit.
- **SUB HARMONICS is switched off.** The HARM card is not on the SUB OSC page; CC17 has no effect.
- **No menu switch and no notice for the second core.** The fallback to one core is automatic and silent.
- **No error screen in the update loader.** The update screen shows progress only; if a write fails the installer page tells you.
- **MIDI** is not tested with many devices.
- **Heavy multis** can miss an audio block very rarely (a short click). The NOTES budget limits the notes per part to avoid it; the CPU figures in the documents are a computer model, the unit can read higher.
