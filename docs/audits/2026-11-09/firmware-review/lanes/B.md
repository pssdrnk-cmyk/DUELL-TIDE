# Lane B: sequencer, arp, transport and recording
Reviewed: `seq.c` (read fully), `midi_clock.c`, `tide_seq.c`, `seq_poly.c`, `seq_morph.c`, `seq_rhythm.c`, `seq_phrase.c`, `seq_evolve.c`, `seq_voicelead.c`, `seq_answer.c`, `seq_arc.c`, `motion.c`, `patslot.c` (read fully); `seq_dialogue.c` (read, not traced in depth); `chord.c` (only its scale path); `ui.c` pat_load / load_begin / pslot_save_ui, `ui_input.c` rec_tap, play_press / play_hold, tap tempo, `tide_ui.c` tide_tl_keep, tide_bass_part (the trace's hand-offs).
Not reviewed: `cycle.c` and `click.c` past their headers, `slicer.c` (8 lines of constants, nothing to review), `midi_grid.c`, `midi_control.c`, `ui_events.c`, the UNDO swap in `ui.c` past load_begin, the TIDE page's display code.

## What is solid
- Stopping never leaves a note hanging. PLAY/STOP (`seq_stop`), STOP ALL, an external Stop (0xFC), a fixed clock silent for 500 ms, a MODE change and a MIDI-ring overflow all end the sequence notes, end the arp notes (or the gates run on in real time), and clear the hold, skip and `rec_key` state. Start, Continue and a master tempo change keep each track's phase (`clock_rescale`).
- Arp mode / HOLD / HOLD ALL changes are handled in one place (`events_block`, seq.c:1981-2014) and leave no note sounding. The ARP SEQ stand-in pauses on an internal STOP and stops with an external one.
- Live recording (b3's R2): armed ARP SEQ keys go to the voices and are marked. The marks are cleared at PLAY, STOP ALL and on a ring overflow, and a key-up always ends its own voice.
- TIDE never writes the stored pattern except through KEEP (`seq_morph.c:468`). Every edit path is built from the Original. The motif anchors and the ±2-step cap (`rhythm_far`) hold in `rhythm_mutate` and `phrase_order`. The other edit paths only rest a hit or repeat it in place. A load resets the overlay (`tide_loaded` → `seqm_home`).
- `patslot.c`: every stored record is checked on load (LEN / DIV / SWING / GATE ranges, names, at most 64 steps, every compact step, exact object length). An object that can't be read from flash is never replaced by the defaults (`ps_take`). A save into another object writes the waiting one first, so no SAVE is lost.

## Findings (most important first, at most 15)

### B1. With swing, a POLY stretch of 15 or 13 steps leaves the part late for good  (P1, CONFIRMED, functional)
- **What the player meets:** a swung pattern on a sequencer part, from the part's SWG or from TEMPO SWING. Factory patterns DEEP SHUFFLE 60 %, HOUSE STAB 56 %, DEEP 332 54 % and DOT DELAY 52 % (`engines.c:172-206`) are swung. Play it under a style with POLY on (DUB, MINIMAL, AMBIENT, PEAK) in WANDER / JOURNEY. When POLY picks an odd length (GENTLE 15, WIDE 15 or 13), the part comes back from the stretch late against the other part, the click and an external kick:
  - GENTLE 15: 8 × the swing offset late. At SWG 60 that is about 1.6 sixteenths; at 56 about 1 sixteenth.
  - WIDE: 4 × the offset.
  - It stays late until STOP / PLAY, and each further odd stretch adds to it.
  - During the stretch, every other pass plays its steps off the grid.
- **Why (code):**
  - A step's length comes from the parity of the short loop's index: `step_samples(t, period, t->seq_idx)` (`seq.c:717-721`, called at `seq.c:1607`). Even steps are long, odd steps are short.
  - In a 15-step loop, step 14 and the next step 0 are both even, so one pass lasts 15p + sw, not the grid's 15 positions.
  - The stretch ends by counting steps, not time. `poly_ref` wraps (`seq_poly.c:62-73`), then `poly_len = 0` (`:46-50`), then `seq_idx = 0`. `seq_pos` keeps the accumulated surplus and nothing re-locks it to the transport (`seq.c:1613`).
  - Arithmetic for GENTLE (SPAN 8 full loops = 128 steps, `:42`): 8 passes of 15 steps (each 15p + sw) plus steps 0-7 (8p) = 128p + 8sw, against the other part's 128p.
  - Even lengths (14, 12) and ARP SEQ parts are not affected: the arp swings on `arp_idx` parity, which counts continuously (`seq.c:693`).
  - Every POLY test runs SWG 50 (`tide_*_test.c`, `song.g[G_SWING] = 50`).
- **Fix sketch:** while a stretch runs, swing a sequencer step by the full loop's step parity (`poly_ref`; for an even P_SLEN this is the global 16th parity), not by `seq_idx`. A TIDE hook in `step_samples` passes `idx ^ ((seq_idx ^ poly_ref) & 1)` for the playing step. About 30 B flash, 0 RAM; it touches the audio interrupt (one compare per step boundary). `rec_note` and the NUDGE paths use the same helper, so they follow.
- **Test:** `tidesteps` (or the POLY part of `tidecontrols`): run a forced 15-step stretch at SWG 60 and assert that the part's step-0 onset after the stretch lands on the same sample as the partner part's. Add one SWG-60 case to `swing` (which audit finding 24 says isn't in the default list yet).
- **Known?** Not recorded. Related to ROADMAP "By ear: polymeter (12 / 16, 15 / 16) against a steady 4 / 4" (that row assumes straight timing). The TIDE-morph report's finding 5 (POLY on the bass) is a separate issue.

### B2. On an ARP SEQ part, knob locks and automation follow the sequencer's clock, not the arp's steps  (P2, CONFIRMED, functional)
- **What the player meets:** M02 DUB CHORD (both parts are ARP SEQ). Arm REC and turn a knob on what you hear as step 5: the lock is stored on the sequencer's step 5. Then:
  - **You press a key:** the arp's pattern restarts at step 1 on the next ARP RATE step, while the sequencer runs on, so the lock now colours another note.
  - **ARP RATE is not DIV:** the locks run at DIV and the notes at ARP RATE.
  - **POLY shortens the arp's loop:** the locks keep the full loop.
  
  The PARAMETER LOCKS list and the step marks show the stored steps, which no longer match what plays.
- **Why (code):**
  - `seq_play` applies the step's locks and automation with the sequencer's index and returns only afterwards for AM_SEQ (`seq.c:1531` `motion_step(t, idx, …)`, then `:1532-1535`). `idx` is `seq_idx`, stepping at P_SDIV. For ARP SEQ, `tide_slen` keeps the full length (`seq_poly.c:36`), while the arp plays `tide_arp_idx` with the POLY base (`:97`).
  - Recording uses the same index: `motion_knob_lock` stores at `t->seq_idx % len` (`motion.c:324-325`), `motion_capture` at `seq_idx` / `seq_pos` (`motion.c:361-362`).
  - The arp restarts on a first key: `arp_add` sets `arp_idx = 0xFFFFFFFF` and waits for the next RATE boundary (`seq.c:238-240`).
  - Live *notes* on ARP SEQ already go to the arp's step (`rec_arp_note`, `seq.c:918-933`). The locks were left on the old path.
- **Fix sketch:** for an AM_SEQ part, move `motion_step` out of `seq_play` into `arp_fire` on the arp's step index: `tide_arp_idx(t, arp_idx, len)` after `TIDE_ARP_TICK`, with `play` = the PROB roll. Record locks at that same index (the next step past the half, as `rec_arp_note`). About 60-90 B flash, 0 RAM. It touches the audio interrupt (moved, not added). The seq-side call stays for non-ARP-SEQ parts.
- **Test:** `liverec` (or `tidesteps`): ARP SEQ part, a lock on step 5, a key pressed at sequencer step 9; assert that the locked value sounds on the arp's step 5. Also a case with ARP RATE 1/8 and DIV 1/16.
- **Known?** Not recorded. The liverec audit (L2, R2) fixed notes only.

### B3. VOIC re-voicing moves a chord's out-of-scale tone onto the scale, even with OFF-SCALE PLAY  (P3, CONFIRMED, functional)
- **What the player meets:**
  - **Setup:** SCALE set (not CHROMATIC), OFF-SCALE KEYS PLAY, and a stored 3- or 4-note chord step with an out-of-scale upper tone (a 7th, a borrowed chord).
  - **Trigger:** under WANDER / JOURNEY (MUSIC), any end with VOIC ≠ 0 plays that tone snapped to the scale. The stored pattern plays it as written.
  - **Result:** the chord's colour changes from bar to bar. The design rule says "No dial moves a note to another pitch class" (`seq_morph.c:9`).
- **Why (code):** `voicelead_walk` builds each upper candidate from `note_snap_to(trk, s->note[id], 1u)` (forced snap, `seq_voicelead.c:42`) and writes the best one (`:57`), even the "unchanged" candidate. Playback honours OFF-SCALE PLAY (`seq.c:1427`, `note_snap` not forced). Under SNAP (the default) both paths give the same note, so only PLAY users meet it.
- **Fix sketch:** take `base = s->note[id]`, and accept a candidate when it equals the original pitch class or is in the scale. About 10 B. Main loop only.
- **Test:** `tidevoicelead`: OFF-SCALE PLAY, C major, step {C E G Bb} with VOIC +60; assert that the pitch class Bb is still in the end.
- **Known?** Not recorded.

### B4. The pitch walk's "nothing under C3" guard reads an ARP SEQ step's offset, not the note it plays  (P3, CONFIRMED, functional)
- **What the player meets:** on an ARP SEQ part whose sound is not in the BASS category but is played low (hold a C2 on a KEYS or PAD sound; the stand-in remembers it), PITCH walks that part's line below C3. TIDE_GUIDE promises "never a bass note under C3".
- **Why (code):** `sqm_pitch_voice` refuses `s->note[hi] < 48` (`seq_evolve.c:30`). On ARP SEQ the stored note is an offset around 60: the arp plays `held + note[0] - 60` (`seq.c:362, 366`). So 60 passes the test whatever the held key. Only `tide_bass_part` (the sound's category, `seq_evolve.c:79`, `tide_ui.c:418-421`) protects such a part.
- **Fix sketch:** for AM_SEQ parts, test `lowest stand-in note + note[hi] - 60 < 48`, from `tide_chord` (the remembered chord). About 30 B. Main loop only. It can share B1's bass mask from the tide-morph audit (brief B1).
- **Test:** `tidemotif` / the pitch-walk part of `tidesteps`: an ARP SEQ part, non-bass sound, stand-in chord {36}; assert that `sqm_pitch_walk` returns len.
- **Known?** Not recorded. Related to tide-morph finding 5 (POLY / VOIC on the bass, brief B1). Fold it into that brief.

### B5. A big tempo jump on the internal clock (tap tempo) fires skipped steps at once and can knock an arp off the grid  (P3, CONFIRMED, functional)
- **What the player meets:** tap a new tempo far from the old one while playing (the first two taps can jump from 120 to 220 BPM).
  - The sequencer plays the steps it "owes" in one block: a flam of two or three steps.
  - An arp or ARP SEQ part whose leftover exceeds a whole new step restarts its phase from 0, so it is off the transport grid against the sequencer parts until a new phrase or STOP / PLAY.
  - A slow TEMPO knob turn (1 BPM a detent) never shows it.
- **Why (code):**
  - The internal clock keeps `seq_pos` / `arp_pos` / `clk_pos` in samples across a BPM change. Nothing rescales them: `clock_rescale` is called only for external pulses and AUTO unlock (`midi_clock.c:115, 205`). Tap tempo writes `song.g[G_BPM]` directly (`ui_input.c:1109`).
  - `seq_tick` then fires every step whose new length fits (`seq.c:1606-1627`).
  - `arp_step` drops the leftover: `if (t->arp_pos >= duration) t->arp_pos = 0` (`seq.c:701-702`).
- **Fix sketch:** in `events_block`, on the internal clock while playing, remember `beat_samples()`. When it changes, call `clock_rescale(old, new)`. The phase is then kept as it is for an external clock. About 24 B flash, 4 B RAM. One compare per block in the audio interrupt.
- **Test:** `swing` / `seqtime`-style host check: play at 120, set 220 mid-step; assert at most one step fires in the next block, and the arp's next onset sits on the 1/16 grid.
- **Known?** Not recorded.

### B6. ARP SEQ puts notes on the scale differently from the keys and the sequencer  (P3, CONFIRMED, functional)
- **What the player meets:**
  - **The rule differs:** the keys and plain sequencer steps take the *nearest* scale note (a tie goes down) and leave notes alone with OFF-SCALE PLAY. ARP SEQ always moves *down* and ignores OFF-SCALE PLAY.
  - **Where you hear it:** on scales with a gap of 3+ semitones (PENTA, MINOR PENTA, BLUES, HARMONIC MINOR, …) the same pattern plays different notes as an ARP SEQ part and as a sequencer part. For example, in C harmonic minor an offset that lands on Bb gives Ab on ARP SEQ and B on the keys.
- **Why (code):** `arp_seq_notes` uses `scale_snap` (down only, any OFF-SCALE setting, `seq.c:147-153, 367-368`). The keys and steps use `note_snap` (`seq.c:158-171, 180, 1427`). The code comment calls this deliberate (`seq.c:146, 295`). The guide doesn't say so, and the brief's "the same for keys, arp, sequence and TIDE edits" doesn't hold.
- **Fix sketch:** an owner choice. Either `note_snap_to(t, x, 1u)` in `arp_seq_notes` (same nearest rule; 0-4 B; it changes how existing ARP SEQ patterns sound on gapped scales), or one line in the guide. The audio interrupt is touched only by the code fix.
- **Test:** `scale` / `arpseq` host group: C MPEN, offset +6 on ARP SEQ against the same note on a key.
- **Known?** Not recorded.

## Roadmap suggestions
1. **B1 first:** a timing bug that breaks the "groove locked again afterwards" promise in `seq_poly.c`'s own header, and a cheap fix. Ship it with a swung POLY host case. Re-check by ear on the unit with DEEP SHUFFLE (by-ear row).
2. **B2 next:** it decides whether knob recording is usable on the default multi M02. Brief it alongside the liverec R2 limits row.
3. **Fold B4 into tide-morph brief B1** (one bass mask), and B3 into the "pattern polish" row.
4. **B5:** a small standalone fix (reuse `clock_rescale`); low risk.
5. **B6:** owner question only (keep down-snap and document it, or unify). No code until answered.
