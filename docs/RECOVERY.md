# Recovery: if a flash is interrupted or fails

**DUELL** with TIDE: pssdrnk's fork of DUELL 1.1, MultiMorphing Bitimbral Synthesizer by OSCILL/R

Plain version: **DUELL has run on two FM-1 units (the author's) and more than 100 people have installed it through the Firmware Hub; TIDE has run on one, the fork owner's. The way back to the official firmware (V15) has worked for the author, and nobody can promise that a failed flash is fixable.** You install at your own risk. This page says what we know, what we do not know, and what to try if you end up in this situation anyway.

## What is known
- **The update screen.** While the loader writes the new firmware the unit shows DUELL, WRITING, a percent number, a bar of 20 segments and DO NOT SWITCH OFF. If the screen stays dark but the installer's bar moves, the write is running; the screen is only a guest of the loader and never stops it.
- **Update mode by itself.** If the firmware dies twice in a row, each time within its first 30 seconds after power-on, the unit goes into update mode by itself on the second failure and waits for the installer (the screen stays dark). Open the installer page, connect, press Install: the new write resumes. This is the way back from a firmware that does not start.
- **The second core.** If the second core does not start or misbehaves, DUELL plays on one core. A crash or hang with the second core in use makes the next start use one core as well, and the start after that tries two cores again. Nothing needs to be done.
- The FM-1 has a **single-bank layout**: there is one copy of the firmware, not a spare. We know of **no rollback** built into the unit.
- The installer works in two steps. Step 1: the running firmware stages a small **update loader** (nothing is written to the main firmware yet; if the page says "stopped" or "lost" here, it also says nothing was written). Step 2: the loader writes the new firmware. The unit **stays in update mode until a write has finished**, so pressing Install again resumes it. This comes from Felucca's installer code, which DUELL keeps.
- The installer can write the official firmware (V15) back, if you supply M-VAVE's own file.

## What is not known
- Whether DUELL behaves the same on every unit. It has run on the author's two units and many installs through the Firmware Hub.
- Whether the **return to the official firmware (V15)** works on every unit. It has worked on the author's unit with this installer's return button; the unit booted normally on V15.
- What happens if power or USB is lost in the middle of step 2 on your particular unit. Felucca's author points to an external recovery tool (FM-1-transporter) for a unit that no longer starts. DUELL's author owns no such tool. The fork owner has used one once, on one unit, with success (see "Hardware recovery" below); nobody else has reported trying it.
- Whether every unit behaves alike.

## What to try, in this order
1. **Do not panic and do not keep pulling cables.** Let the page show its message.
2. **Replug.** Unplug the USB cable, plug it back in (data cable, straight into the computer, no hub), reload the installer page and press Install again.
3. **Re-enter update mode.** If the unit does not respond normally, the installer detects an unit already waiting in update mode and resumes the write. If the page cannot see the unit, try another USB port, another data cable, another browser tab with no other MIDI program open. Switch the unit off and on.
4. **Re-flash the official firmware.** On the installer page use the amber RETURN TO OFFICIAL FIRMWARE card ("Go back to the official firmware") with your own copy of the official V15 `FM-1.fwsc` from <https://www.m-vave.com/download>. If the unit is in update mode and you already saved a backup, tick "Resume an interrupted return".
   While it runs, the page shows an amber banner "RETURNING TO THE OFFICIAL FIRMWARE" and the progress "Writing official firmware" only in the RETURN card; the DUELL install card stays empty and is locked. If you see that banner, **it is not installing DUELL again. Do not unplug and do not close the page until it says DONE.** If it fails, the message stays in the RETURN card: press the same button again.
5. **Ask for help** with an issue on [this fork's GitHub](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues/new/choose): say which build you installed (stable or beta, and its tag), which step stopped, the page's message, and what the screen shows. Do not post your device's data.
6. If none of this brings the unit back (black screen, and no MIDI device appears on the computer), the last way is a hardware recovery: see "Hardware recovery" below, or ask M-VAVE support. We cannot promise either.

## Hardware recovery: a unit that no longer starts
This is the one recovery that has been done on a real unit. On 2026-10-21 the fork owner's FM-1 stopped during an install of another custom firmware (not DUELL or TIDE): black screen, and no USB device on the computer. It was brought back to M-VAVE's official V15 with the community **FM-1 Transporter** firmware on a **Raspberry Pi Pico H**, using the **MvaveFM1Unbricker** scripts by Quixotic7 to talk to it. It worked once, on one unit; treat it as a method to try, not a promise, and read those tools' own pages first ([FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter)).

What the fork owner did:
1. **Wiring.** Cut one end off a USB-A to USB-C cable and strip it. Tape off the **red** wire (VBUS; it stays unconnected). Connect **translucent (GND)** to Pico pin 3, **white** to pin 1 (GP0) and **green** to pin 2 (GP1). Dupont jumper wires wrapped onto the cable wires were enough.
2. **Charge the FM-1 first.** A flat battery makes every later step look like a failure.
3. **Dump before you fix.** Let the toolkit read the whole flash into a `.bin` file and keep it before it writes anything.
4. **Read the answers with care.** The `info` command can report "no UBOOT" while the Pico is still pulsing. Run it again without power-cycling the unit before you decide it is dead.
5. **Write official V15** with the toolkit. Check your copy of `FM-1.fwsc` against the V15 SHA-256 that the installer expects; the file comes from <https://www.m-vave.com/download>.

**Install again safely afterwards.** Start from stock V15, with the battery full, the cable straight into the computer (no hub), and the computer kept awake until the page says Done. Back up before every later flash. Keep the Pico and the cable: that is your insurance if a flash ever fails to start.

## The backup page
The installer page and the editor's Backup page (<https://pssdrnk-cmyk.github.io/DUELL-TIDE/editor/backup.html>) save a file with your patches, multis, patterns, user wavetables and settings (TIDE's own settings are in the settings from the next build on; a backup made before that has none, and restoring it leaves the unit's TIDE settings as they are). Keep a backup before every flash.

**Going back to plain DUELL 1.1 is not supported.** A TIDE patch whose End B is an FM6 MORPH TO target (the destination sound with another algorithm) loses that End B on plain DUELL: plain DUELL does not read the record, the slot shows its factory sound, and the record is gone for good at the next write of that chunk. TIDE's settings are zeroed at plain DUELL's next settings save and start on DUB when TIDE is installed again. Back up before you go back, and expect to restore the patches with the editor on TIDE only.

## Your saved data
A flash (also going back to the official firmware and installing DUELL again) keeps your saved data, so the unit comes back as it was left. To start from scratch use MENU > SYSTEM > ERASE DATA on the unit, or the installer's optional "Erase my saved data after installing" box ([INSTALL.md](INSTALL.md), "Keep or erase your data"). Back up first; it cannot be undone.

## Before you flash
Follow the checklist in [INSTALL.md](INSTALL.md): data cable, no hub, back up, never interrupt, replug afterwards. **You install at your own risk.**
