# Lane A: keys, voices and MIDI
Reviewed: `midi_control.c` (read fully), `midi_uart.c` (fully), the MIDI parts of `usb.c` (rings, `midi_enqueue`, `midi_out_event`, `midi_in_event`, `ep1_take`: fully), `midi_learn.c` (map edit, sanitize, learn/clear: fully; the page's UI part skimmed), `midi_map.h` (fully), `voice.c` (allocation, stealing, mono/legato/unison, note-on stagger, note-off, engine switch: lines 100-960 read; render side skimmed), `unison_gain.c` / `unison_start.c` (fully), `sense.c` (the event step `sense_gate` / `sk_taken` and `sense_vel` read; calibration skimmed), `panel.c` (fully), `ui_kb.c` (fully: drawing only). Followed into lane-B/other files where the traces went: `seq.c` key path (`key_on`/`key_tone`/`key_off`/`keyboard_block`, `input_on`/`input_off`, `arp_add`/`arp_remove`, `mo_*`, `mode_apply`, `stop_all`, `events_block` panic and MIDI drain), `ui.c` `pc_recall`, `upreset.c` patch load, `ui_input.c` mode knob.
Not reviewed: `voice_chain.c`, `velocap.c` (developer capture build only), `perform.c` beyond its key ownership (its effects are master stages), `sense.c` velocity curves and calibration walk, the key scan and debounce themselves (they live in `hal/fm1_input.h`, outside the lane), `midi_clock.c` (lane B).

## What is solid
- MIDI input parsing: USB checks CIN against the status nibble and that data bytes are 7-bit (`usb.c:834-835`); DIN keeps running status across real-time bytes, cancels it on system common / SysEx, ignores SysEx data (`midi_uart.c:32-68`). Every index from a MIDI byte is bounded (channel `& 0x0F`, note/CC `& 0x7F`, `midi_morph_fired[ch]`, `midi_tide_fired[ch & 15]`, program change queue and `pc_recall` range checks). Ring overflow on either port panics every part and forgets all MIDI ownership, so a lost note-off can't hang a note (`seq.c` events_block `midi_in_overflow`).
- Panel keys to voices: each key stores the notes it started and its part (`kb_chord`, `kb_chn`, `kb_trk`, LAYER's `kb_chn2`), so OCT-/OCT+, TRN, part select and a sound load with keys held all release correctly. MIDI notes store their owning part (`midi_sel_on`), so a part-channel change or ROUT switch releases correctly (`midi_route_block`); CC120/123, the pedal, note-on velocity 0 and repeated note-ons are handled.
- `mode_apply` lets every note go on both parts at a MODE change, so no internal voice hangs across DUEL / STEREO / LAYER (the MIDI-out side is A2 below).
- Voice allocation: the same note reuses its voice, stealing takes the oldest released voice and then the oldest held one but never the lowest held note (the bass rule, `voice.c:403-466`). A note-off releases every voice of that note across all 8 (`voice.c:808-838`), so a shrinking NOTES budget or a POLY/MONO change with keys held can't strand a voice. The start limit's queue is cancelled by the note-off and emptied by every panic.
- MIDI LEARN map: `map_set` and `map_sanitize` (stored maps) keep a learned line's track below `NTRK` (2) and refuse locked numbers and action codes for learned lines, so the ISR's `&trk[rule]` (`midi_control.c:513`) can't go out of bounds.

## Findings (most important first, at most 15)
### A1. A note sent to the DAW never ends when two keys play the same note  (P2, CONFIRMED, functional)
- **What the player meets:** with USB MIDI going to a DAW or another synth, a scale set (OFF-SCALE is SNAP by default), press C, then C# (it snaps to C as well), let go of C first, then C#. The FM-1 stops the note, but the DAW gets a note-on for C and no note-off: the note hangs there. The same happens without a scale when the same pitch is held from two key positions (hold F4, press OCT+, press the low F key, let go of the high one first).
- **Why (code):** `key_tone` (`seq.c:1057-1073`) returns early when another key already holds the note (`midi_local_held`), before `kb_mo |= 1u << k`, so the second key is marked as having sent nothing. `key_off` of the first key (`seq.c:1125-1138`) skips that note (`midi_local_held`: the second key holds it), with both its `input_off` and its MIDI note-off, and clears its own `kb_mo` bit. `key_off` of the second key then ends the voice, but its `kb_mo` bit is clear, so no note-off goes out. The arp/sequencer MIDI out (`mo_bits`) is not affected.
- **Fix sketch:** in `key_tone`'s early-return path, still set the key's `kb_mo` bit when the out condition holds, so whichever key is released last sends the one note-off. About 8-16 B; runs in `keyboard_block` (audio interrupt), no measurable cost.
- **Test:** `midi_out_seq_test.c` or `scale_test.c`: SNAP + major scale, keys C then C#, release C then C#; expect exactly one 0x90 and one 0x80 for note 60 in `midi_out_q`. Same with the OCT+ case.
- **Known?** not recorded.

### A2. Held keys leave notes hanging on the DAW when the MODE changes  (P2, CONFIRMED, functional)
- **What the player meets:** hold a chord with USB MIDI going out, then load a multi that has another MODE (or turn the mode control, or send a learned MODE CC). The FM-1 silences the chord as designed, but the DAW never gets note-offs for it: every held note hangs on the receiving side until it gets an all-notes-off.
- **Why (code):** `mode_apply` (`seq.c:1662-1686`) sets `kb_note[k] = KB_SILENT` and `kb_chn[k] = 0` for every held key, but leaves `kb_mo` alone and sends nothing. At the key-up, `keyboard_block` skips a `KB_SILENT` key (`seq.c:1307-1308`), so `key_off` and its note-off (`seq.c:1135-1136`) never run. The arp's notes are fine (`arp_forget` -> `mo_flush`). Triggers: `multi.c:746`, `ui_input.c:912` and `:998`, `ui_guide.c:93`, `midi_control.c:483`.
- **Fix sketch:** in `mode_apply`, for each held key whose `kb_mo` bit is set, send 0x80 for `kb_chord[k][0..kb_chn[k])` on `trk_midi_ch(kb_trk[k])` before clearing it, then clear `kb_mo`. About 40 B; runs in events_block (audio interrupt), only at a mode change.
- **Test:** `mode_test.c`: hold a key (MIDI out on), switch MODE, release; expect a note-off for each note-on in `midi_out_q`.
- **Known?** not recorded. **Matters more for b7:** ROADMAP's try-out "the part selection also picks how the keys play" (`fix/b7-playmode`) goes through this same mode change, so every part change with keys held will hit A2, and will also silence the held chord (`mode_apply` is built to do that). The b7 builder should know about both.

### A3. With ARP KEEPS RUNNING, a sound load makes the arp play on after you let go of a MIDI keyboard  (P2, CONFIRMED, functional)
- **What the player meets:** GLO > MODULES ARP = KEEPS RUNNING, arp on (no HOLD), hold a chord on an external MIDI keyboard or from the DAW, change the sound (PRESETS, or a program change from the DAW), let go. The arp keeps cycling the released chord until STOP ALL, a multi load, or pressing and releasing each of those notes again. Panel keys work correctly in the same situation.
- **Why (code):** the panic for the load (`ui.c:1636-1637`, `upreset.c:1270-1271`) reaches `events_block` (`seq.c:1962-1976`), which calls `midi_forget_track(i)` first, wiping who holds which MIDI note, and then keeps `held[]` / `nheld` / `arp_phys` because of `panic_keep`. The later MIDI note-off finds no owner (`midi_control.c:247`, `261`), so `midi_release` -> `input_off` -> `arp_remove` never runs. The arp keeps stepping while `nheld > 0` (`seq.c:670`). Panel keys still release, because `kb_chn` survives the panic.
- **Fix sketch:** when the keep branch applies, skip `midi_forget_track(i)` (keep the ownership and the MIDI chords, so the note-offs reach `arp_remove`); forget only in the else branch. Roughly 0-10 B; events_block (audio interrupt).
- **Test:** `modules_test.c`, or `midi_control_test.c` with `mods_keep = MODK_ARP`: MIDI note-on 60/64, patch-load panic with keep, note-offs; expect `nheld == 0`.
- **Known?** not recorded.

### A4. Same note from the keys and from MIDI on one part: HOLD stops replacing the chord, and turning HOLD off leaves the arp running  (P3, CONFIRMED, functional)
- **What the player meets:** an arp part receives MIDI (DAW or controller) and you play along on the keys. If a key plays a note that a MIDI note already holds, the arp's count of held keys stays one too high from then on. With HOLD, the next chord adds to the latched one instead of replacing it, up to 16 notes. Switching HOLD off then doesn't drop the latched chord: the arp plays on with nothing held, until STOP ALL or a load.
- **Why (code):** `midi_play` skips a note any key or MIDI note holds (`midi_control.c:183`), but `key_tone` checks only other keys (`seq.c:1060`), so the key calls `input_on` -> `arp_add` (`arp_phys++`, `seq.c:224`). At the key's release, `input_off` returns early because MIDI still holds the note (`seq.c:1031-1032`), so `arp_remove` and its `arp_phys--` never run. With `arp_phys` stuck above 0, `arp_add` never replaces the latch (`seq.c:216`), and the HOLD-off drop needs `!t->arp_phys` (`seq.c:2004`). A related drift: in LAYER with SPLIT, a MIDI note routed only to part 2 is still released on part 1 too (`midi_control.c:116-118`; the owner is always part 1), which decrements part 1's count and can end a part-1 sequencer voice of the same pitch.
- **Fix sketch:** add `!midi_note_held(t, x)` to `key_tone`'s skip test, mirroring `midi_play` (~8 B). For the split case, call part 1's `input_off` only when `lay_route(note) & 1` (~16 B). Both in events_block (audio interrupt).
- **Test:** `chordkeep_test.c` or `midi_control_test.c`: MIDI on 60, key on 60, key off, MIDI off; expect `arp_phys == 0`.
- **Known?** not recorded.

### A5. Changing a part's MIDI channel while holding a key sends the note-off on the new channel  (P3, CONFIRMED, functional)
- **What the player meets:** hold a key, change PART 1 CH in GLO > MIDI, let go: the receiving synth keeps the note, because its note-off arrives on the other channel.
- **Why (code):** `key_off` reads the channel at release time (`seq.c:1127`, used at `:1136`). The arp/sequencer path remembers the channel its notes went out on (`mo_ch`, `seq.c:99-104`, "no hanging notes if the channel is changed"); the keys don't.
- **Fix sketch:** a byte per key with the channel used at `key_tone` (27 B RAM, ~20 B flash), or reuse A2's flush when `midi_ch_sync` changes a channel. Fold it into the same change as A1/A2.
- **Test:** `midi_out_seq_test.c`: key on, `midi_pch[0] = 3`, key off; the 0x80 is on channel 1.
- **Known?** not recorded.

### A6. MIDI IN = SEL: a bend held across a part change leaves the old part out of tune  (P3, CONFIRMED, functional)
- **What the player meets:** MENU > MIDI > MIDI IN set to SEL. Bend the wheel, switch part while it's bent, let the wheel return. The new part is centred, but the old part stays bent, on its keys, arp and sequencer too, until a sound load on it or a mode change.
- **Why (code):** bend goes to `midi_targets(ch)` = the parts with notes owned on that channel plus the channel's current part (`midi_control.c:43-46`, `61-67`). In SEL that's the newly selected part, so the centring bend never reaches the old one: its `midi_bend_target` (`midi_control.c:51`) keeps the bent value, and `voice.c:1264` glides to it. Only `midi_forget_track` resets it (`midi_control.c:294`). MIDI channel aftertouch and the wheel's matrix value (`mod_midi`) probably stay on the old part the same way (SUSPECTED; check `mod_midi`'s per-track store).
- **Fix sketch:** with ROUT SEL, when `song.sel` changes in events_block, zero the old part's bend target unless it still owns MIDI notes (~30 B, audio interrupt, only at a part change).
- **Test:** `midi_control_test.c`: G_ROUTE 1, bend max, `song.sel = 1`, bend centre; expect `midi_bend_target[0] == 0`.
- **Known?** not recorded.

### A7. `midi_play` repeats its chord-keeping code for part 2  (P3, SUSPECTED size, backend)
- **What the player meets:** nothing; flash only.
- **Why (code):** `midi_control.c:155-242`: the free-slot search, the STRUM chord set-up and the plain chord record are written out twice, once for part 1 and once for LAYER's part 2, near-identical.
- **Fix sketch:** one helper `mchord_keep(t, id, ch, note, vel, nn, n, sb)` called twice. Estimated 120-200 B saved (estimate: measure with `tools/size_fns.py`). Event path, not per sample.
- **Test:** `chord_test.c`, `strum_test.c`, `mode_test.c` (LAYER) unchanged.
- **Known?** not recorded.

## Roadmap suggestions
1. One "keys' MIDI out" change for A1 + A2 + A5 (about 70 B), before or together with b7's playmode try-out: that try-out turns A2 into an everyday path. Extend `midi_out_seq_test.c` with the three cases.
2. A3 as its own small fix (a branch move in events_block) with a `modules_test` case; b7 or the next beta.
3. Tell the b7 playmode builder that `mode_apply` silences held keys: with part select picking the mode, a part change while a chord is held cuts the chord. Owner decision if it should carry over.
4. A4 and A6 together later (cheap, rare paths).
5. A7 only when flash is needed.
