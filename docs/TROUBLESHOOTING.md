# Troubleshooting

Symptom, then fix. For a failed or interrupted flash read [RECOVERY.md](RECOVERY.md).

| Symptom | Fix |
|---|---|
| Installer says the browser cannot do it | Use Chrome or Edge on a computer. Safari and Firefox lack WebMIDI SysEx. |
| "MIDI (SysEx) access was not allowed" | Allow MIDI in the site settings in the address bar, then try again. |
| "FM-1 not found" | Use a **data** USB cable, straight into the computer (no hub). Close DAWs and other tabs using MIDI. Switch the unit off and on. |
| "The connected device is not an FM-1" | Unplug other MIDI devices and try again. |
| Install button stays grey | Tick all four checklist boxes. If the page says the firmware is not yet released, no package was found next to it; try again later or see the releases page. |
| "This file cannot be installed" | The file is not a DUELL/Felucca package. Rebuild with `tools/build.py --release`. Never use a patched official file. |
| It stopped halfway / "USB connection was lost" | Replug and press Install again; the unit stays in update mode and the page resumes. |
| "The update loader did not appear" | Replug the cable and press Install again. |
| Done, but the unit does not behave | Unplug and plug it in again. Check HOME (hold) > ABOUT for the version. |
| Backup says the firmware cannot make a backup | The unit runs the official or an old firmware. Nothing was saved; back up on a Felucca/DUELL build only. |
| Emulator shows old or broken saved data after an update | Download saved data first, then clear the site's browser data (cookies and site data) and reload. |
| Emulator is silent | Click the page once (browsers block audio until you do) and raise MASTER. |
| Emulator pitch sounds off | The browser plays at a rate other than 44.1 kHz; the console warns about it. |
| Hum, buzz or whine in the outputs while the USB cable is connected | See **USB noise** below. |
| The computer does not show the FM-1 as an audio input / sound card | USB AUDIO is OFF (its default): hold HOME, SYSTEM tab, USB AUDIO ON. The unit leaves the bus for a moment and comes back. |
| A sound does not change when I turn SELECT | SELECT switches sub-pages and moves rows; knobs 1-4 adjust. See [QUICKSTART.md](QUICKSTART.md). |
| MIDI notes play the wrong part | Part 1 listens on channel 1 and part 2 on channel 2. In LAYER, channel 1 plays both parts. |
| SAVE ERROR | The flash failed to write: try again, and report it if it repeats. |
| STORE FULL | With TIDE the patch store holds 126 saved patches, not 128. Saving into a 127th empty slot is refused and nothing is written. Overwriting a saved slot always works; RESTORE a saved slot to make room ([KNOWN_ISSUES.md](KNOWN_ISSUES.md)). |
| The tide does not move | It moves only while the transport plays: press PLAY (STOP pauses it). Twist knob 1 on AUTO TIDE to start a glide. If you moved the fader by hand, that stopped the glide. See [TIDE_GUIDE.md](TIDE_GUIDE.md). |
| TURNS, POLY, OFFBEAT or ARC do nothing | They act only while the tide runs by itself on a pattern: MODE EVOLVE or TRAVEL, TARGET SEQ or BOTH, the music playing. TURNS also needs ALGORITHM on BOTH. With the tide stopped, or TARGET SOUND, your pattern plays as written. |
| TIDE SETTINGS shows STYLE CUSTOM | You changed one of the rows a style sets; your values are kept. Pick a style again to set them all. |
| STOP TO SAVE | Saving needs the transport stopped. Press PLAY/STOP. |
| Anything else | TIDE, or this fork's installer, emulator and editor: open a bug report on [this fork's issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues/new/choose). A problem that plain DUELL has too: OSCILL/R's [bug report form](https://github.com/oscillr/DUELL/issues/new/choose). |

## USB noise

If you hear a hum, buzz or whine in the outputs only while the USB cable is connected, it is almost always **not** the FM-1's sound: the cable ties the unit's ground and power to the computer's, and the computer (especially a laptop on its charger) puts noise on them. The FM-1's audio itself does not depend on USB: the sound is made and sent to the jacks inside the unit, whatever the cable does. Try these, in this order:

1. **Ground loop or power noise (the usual cause).** Hum at 50 or 60 Hz, or a whine that follows the computer (the screen, the disk, moving the mouse), points to this. Power the FM-1 from a phone charger or a power bank instead of the computer, and use the computer only when you need the editor (connect, edit, unplug). Or put a **USB isolator** between the computer and the FM-1, or a **ground-loop isolator** (a 1:1 audio transformer) on the FM-1's outputs. Try a different USB port, a shorter data cable or a cable with a ferrite, and run a laptop on its battery to test. A balanced connection (DI box) to the mixer also helps.
2. **Digital noise.** A buzz at 1 kHz or a ticking that follows USB activity can be the USB bus itself (the computer sends a frame every millisecond) coupling into the outputs. **Set MENU > SYSTEM > USB AUDIO to OFF** (it is OFF by default): the FM-1 then shows up as MIDI only, no audio is streamed over the cable, and the unit's USB work is smaller. MIDI and the editor keep working. If the noise is still there with USB AUDIO OFF, the cable's ground is the cause: use the charger / power bank or an isolator as above.
3. **Not the USB at all.** Unplug the USB cable. If the noise stays, it is the audio cable, the mixer or the power supply of another device.
