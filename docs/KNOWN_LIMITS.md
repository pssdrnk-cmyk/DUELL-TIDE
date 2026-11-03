# Known limits: DUELL 1.1

**DUELL**
MultiMorphing Bitimbral Synthesizer
by OSCILL/R

Current fork-specific limits are listed first. The inherited DUELL notes follow. [KNOWN_ISSUES.md](KNOWN_ISSUES.md) retains older 1.0 details; its slot-capacity and free-flash claims are not current TIDE guarantees. See [the roadmap](ROADMAP.md) for source, installed-package and measurement evidence.

## TIDE limits reviewed 3 November 2026

- **JRNY stays in the current engine.** It prefers the original category and may use the existing broader-category fallback. Small eligible palettes can repeat; recent-destination memory is proposed, not implemented.
- **The parts coordinate timing and colour.** Attack prediction and parameter-based contrast do not analyse sustained harmony or effects tails. M2's upper-voice improvement applies to ordinary chord sequences, not ARP SEQ's held chord.
- **Unequal lengths are available; automatic polymeter is not.** Existing part lengths can rotate against a shared 4/4 reference. Automatic shortening/return policy and physical clock/listening acceptance remain open.
- **USER browsing is proposed.** The 128 voice addresses currently show saved records or factory fallbacks. TIDE's store has 21 chunks with six worst-case records each: 126 records, despite 128 logical addresses. The 32 patterns share three byte-limited 11/11/10-slot objects. Clear USER rows can reuse existing addresses while factory loading stays separate, but that does not increase physical capacity or guarantee a blank row on a populated store.
- **Exact live-morph continuation is not promised.** Retained readback found Part 2's live End B changed after restart; the owner does not require complex continuation of that state. Intentional saved records still need to retain their contents. TIDE preferences already persist between normal boots, but their separate record is outside the current editor backup; archive/restore between flashes is a planned bounded improvement.
- **Expansion has a small measured program budget.** The installed package (`187e9d0`, boot pack included) leaves about 24.9 KB of flash (556,620 of 581,564 B; the boot pack stores the RAM images as one LZMA stream). New candidates need fresh measurements. Software/emulator evidence does not establish physical audio, MIDI clock or CPU performance.

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
