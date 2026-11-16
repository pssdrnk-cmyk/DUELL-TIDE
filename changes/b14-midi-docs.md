## roadmap
### Ideas, not decided
- **MIDI extras** (B14 MIDI lane, 16 November: what TIDE 1.0.0 cannot do yet; flash rough estimates, not builds; the ones over ~100 B need USB 48 kHz OFF, Later 37):
 FM-1 as the clock master (clock, Start / Stop / Continue to USB MIDI out, ~150-250 B); MIDI thru, TRS in to USB out (~40-80 B, ~100-140 B with an ON / OFF on GLO > MIDI);
 the knobs send their CCs (the map's reserved "also send out" bit, ~400-600 B); program change out on a load (~80-120 B); Song Position Pointer, a DAW starting mid-song lands on the right step (~150-250 B);
 poly aftertouch as the AT source (~20-40 B); TIDE's ghost chord sent to MIDI out like the arp's notes (~40-80 B). Not possible: MPE (several KB and RAM), a MIDI OUT jack (the TRS jack is input only), a USB keyboard straight into the FM-1 (it is a USB device, not a host).
