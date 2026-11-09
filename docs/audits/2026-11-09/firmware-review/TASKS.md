# Firmware review 2026-11-09: tasks

Briefs for the findings in [`REPORT.md`](REPORT.md), in the recommended order. Each is written for a builder who takes it cold. The full trace of every finding (trigger, state, wrong result, with line numbers) is in the lane file named in the brief; read that section before you touch the code. Line numbers are for `fd4b6c6` (1.0.0-b6 on `jam11`).

**Rules for every builder:**
- **Two errors equals agent death:** a second real mistake ends your run, and a fresh agent takes over.
- Work on your own branch off `jam11`; one writer per branch. Stop your own servers and loops before you report.
- **Reproduce first:** write the host check named under Test, see it fail on `jam11`, then fix. A check that passes before the fix is the wrong check.
- Run only the host groups the brief names; CI runs the rest on the pull request. Locally: the device build (image size, pool tail ≥ 8,192 B, RAM ≤ 73,728 B, hot-code check) and `check_docs.py`.
- Run everything in WSL from `felucca-base`; regenerate the tables first with `~/venv/bin/python tools/build.py --gen-only`.
- TIDE-only changes go under `#if DUELL_TIDE`; the plain build only has to compile.
- Add a CHANGELOG-TIDE line in the player's words. Sizes below are estimates until built: report the measured flash and RAM.
- Model: Sonnet unless the brief says Opus.

---

<a id="fr1"></a>
## FR1. A swung part comes back on the grid after a POLY stretch (B1; P1, S, Opus)

**Why:** a sequencer step's length comes from the parity of the short loop's index (`step_samples(t, period, t->seq_idx)`, `seq.c:717-721`, called at `seq.c:1607`). In a 15- or 13-step POLY loop, the last step and the next step 0 are both even, so every pass is one swing offset too long. The stretch ends by step count (`seq_poly.c:41-50`, `:62-73`) and nothing re-locks `seq_pos`, so the part stays late: GENTLE 15 ends 8 offsets late (about 1.6 sixteenths at SWG 60), WIDE 4 offsets. Every POLY test runs SWG 50. Trace: [lanes/B.md](lanes/B.md) B1.

**Steps:**
1. Test first: in the POLY part of `tidecontrols` (or `tidesteps`), force a 15-step stretch on part 1 at SWG 60 with part 2 on a straight 16-step pattern. Assert that part 1's first step-0 onset after the stretch falls on the same sample as part 2's. It must fail on `jam11`.
2. While a stretch runs, swing a sequencer step by the full loop's step parity (`poly_ref[pi]`), not by `seq_idx`: a TIDE hook in `step_samples`' callers that passes `idx ^ ((t->seq_idx ^ poly_ref[pi]) & 1)` for the playing step. `rec_note` and the NUDGE paths use the same helper, so they follow; check them.
3. Add one SWG 60 case without POLY to the `swing` group, and a 13-step WIDE case.

**Acceptance:**
- The new checks pass; `tidecontrols`, `tidesteps`, `swing`, `regress` pass.
- With SWG 50 the rendered output is bit-identical to before (the hook is a no-op).
- About 30 B flash, 0 RAM. One compare per step boundary in the audio interrupt.

**Owner questions:** none.

---

<a id="fr2"></a>
## FR2. The keys' MIDI out always sends its note-offs (A1, A2, A5; P2, S)

**Why:** three ordinary cases leave a DAW or second synth with hanging notes while the FM-1 itself is silent. Trace: [lanes/A.md](lanes/A.md) A1, A2, A5.
- **A1:** `key_tone` (`seq.c:1057-1073`) returns early when another key already holds the note, before it sets the key's `kb_mo` bit. The first key's release skips the note (the second holds it); the second key's release has no `kb_mo` bit, so no note-off goes out.
- **A2:** `mode_apply` (`seq.c:1662-1686`) marks every held key `KB_SILENT` and sends nothing; the key-up then skips `key_off` (`seq.c:1307-1308`).
- **A5:** `key_off` reads the MIDI channel at release time (`seq.c:1127`), so a channel changed under a held key sends the note-off on the new channel. The arp path already remembers its channel (`mo_ch`, `seq.c:99-104`).

**Steps:**
1. Tests first, in `midi_out_seq_test.c` (three cases, each must fail on `jam11`): SNAP + C major, keys C then C#, release C then C#; a held key across a `duell_mode_req` change; a held key across `midi_pch[0]` changing. Each expects exactly one 0x80 for every 0x90 in `midi_out_q`, on the channel of the note-on.
2. A1: in `key_tone`'s early-return path, still set the key's `kb_mo` bit when the out condition holds.
3. A2: in `mode_apply`, for each held key whose `kb_mo` bit is set, send 0x80 for `kb_chord[k][0..kb_chn[k])` on that key's channel before clearing it; clear the bit.
4. A5: store the channel used at `key_tone` per key (27 B RAM) and use it in `key_off` and in step 3. If RAM is short, reuse step 3's flush when `midi_ch_sync` changes a channel instead.

**Acceptance:**
- The three checks pass; `midiout`, `mode`, `scale`, `chord` pass.
- About 70 B flash, ≤ 27 B RAM. Audio interrupt, event paths only.

**For the b7 play-mode builder (fix/b7-playmode):** land this before the try-out goes to the owner. Also know that `mode_apply` silences held keys by design: with the part selection picking the play mode, a part change under a held chord cuts the chord. Say so in the try-out's note to the owner.

**Owner questions:** none.

---

<a id="fr3"></a>
## FR3. A save from the web editor keeps End B (E1; P2, S)

**Why:** `ED_UP_STORE` (`editor.c:826`) calls `up_store`, which expects its caller to have set `up_bv_in` (`upreset.c:1167`). Only the unit's own `up_patch_save_part` does (`upreset.c:1633-1642`), so the editor's save writes no End B (`PTF_MB` missing, `upreset.c:812`) and every load of that slot rolls a new random B. `web/EDITOR_PROTOCOL.md:277` promises End B is kept. The existing test (`tests/editor_test.c:1496-1503`) compares `up_store()` with `UP_STORE`, the same function, through a view that doesn't decode End B. Trace: [lanes/E.md](lanes/E.md) E1.

**Steps:**
1. Test first, in `editor_test` item 159: with the fader in the middle and a rolled End B with one hand-set value, compare the raw stored bytes (`up_raw_get`) of `up_patch_save_part` and of an `UP_STORE` for the same part; assert `PTF_MB` is set and that loading the slot gives the same End B (`morph_sig`). It must fail on `jam11`.
2. Move the End B lines of `up_patch_save_part` (`morph_ready`, the `ref` build, `morph_bv_make`, set and clear `up_bv_in` / `up_bv_n`) into one helper and call it around `up_store` in `ED_UP_STORE` for `song.sel % NTRK`.

**Acceptance:**
- The new check passes; `editor`, `patch`, `storage` pass.
- About 30-40 B flash, 0 RAM. Main loop only.

**Owner questions:** none.

---

<a id="fr4"></a>
## FR4. GLIDE on BOTH moves each part from where it is (C1; P2, S)

**Why:** a trip has one start, read from the selected part (`gl.from[l] = GLIDE_LANE[l].get(gl.pi)`, `morph_glide.c:146`), and every step writes one value to all parts in the mask (`GLIDE_LANE[l].set(gl.pi, gl.mask, v)`, `:352-356`). A part whose fader was elsewhere jumps to the selected part's place in one block, switch values included; the AUTO TIDE page draws both paths from `gl.from` (`tide_ui.c:693-696`), so the jump isn't shown. Trace: [lanes/C.md](lanes/C.md) C1.

**Steps:**
1. Test first, in the `glide` group: BOTH launch, GLIDE TO B, faders at 0 (part 1) and 100 (part 2). Assert part 2's fader never drops below 100 on the way, and both arrive at 127 together. It must fail on `jam11`.
2. Keep the start per part (`uint8_t from[GLIDE_LANES][NSHOWN]`, +2 B RAM), compute `v` per part, and set each part alone (`set(k, 1u << k, v)`).
3. Draw each part's path from its own start on the AUTO TIDE page.
4. Build in the same branch as the planned J2 and X1 (same launch code; briefs in `docs/audits/2026-11-09/tide-morph/TASKS.md`).

**Acceptance:**
- The new check passes; `glide`, `tidecrossings`, `tidecontrols` pass with equal faders unchanged (bit-identical render).
- About 30 B flash, 2 B RAM. Main loop only.

**Owner questions:** none.

---

<a id="fr5"></a>
## FR5. A factory multi leaves ROLL ARP, ROLL PITCH and ROLL TYPE alone (C3; P2, XS)

**Why:** `project_load` sets `morph.cfg = set[2]` for every multi (`multi.c:751`); the byte is 0 in all factory records, so ROLL ARP and ROLL PITCH go OFF and WILD reads SMART, and `morph_settings` saves that (`ui_input.c:769-791`). The guide says these rows stay the player's (`TIDE_GUIDE.md:59`). b6 gave AMOUNT this rule already (`multi.c:752-756`). Trace: [lanes/C.md](lanes/C.md) C3.

**Steps:**
1. Test first, in `tidecontrols` or `mode_test`: set ROLL PITCH ON and ROLL TYPE WILD, load factory M02, assert `jl_get(JL_PITCH) == 1` and `jl_get(JL_ROLL) == 2`.
2. Under `DUELL_TIDE`: `if (!fact) morph.cfg = set[2];`. A saved user multi keeps restoring its own byte.

**Acceptance:** the check passes; `tidecontrols`, `mode`, `multi` pass. About 8 B.

**Owner questions:** none.

---

<a id="fr6"></a>
## FR6. Loading a saved sound doesn't cut the other part's held notes (F1; P2, M, Opus)

**Why:** every store read calls `tc_flash_park` (`storage_hw.c:7-12`), which sets `tc.park` until the main loop's next `tc_watch` pass (`twocore.c:122`, `:127-139`). The audio interrupt reads `tc.park` as "one core" (`twocore_run.c:565-575`), sets `bd_one_now`, and `voice_trim_now` fades every voice above the one-core NOTES number (`voice.c:328-350`, `:143`). The faded notes are gone, not paused. The park is only needed while a flash window has the XIP map off, and those windows run with interrupts off, so no job can start during one. Trace: [lanes/F.md](lanes/F.md) F1.

**Steps:**
1. Test first, in the two-core simulation (`tcstage1`, the `park` scene): two-core NOTES above the one-core number, a held chord over the one-core number on part 1, then a store read. Assert `tcs.trims == 0` and the voice count unchanged. It must fail on `jam11`.
2. Don't hold `tc.park` across blocks for a read, program or erase: inside each IRQ-off window in `st_read` / `st_prog` / `st_erase`, check `tc_closed()`; if a job is still open (the fault path's very-late job), wait up to `TC_PARK_US` as now, then `tc_fail(TCR_FLASH)`. Keep `tc.park` for the update path if it needs it; keep the trim for real pauses and faults.
3. Fix the comment at `budget.c:356`.
4. Unit check (test-mode build): a 6-note held chord on part 1 of a two-core-heavy multi, load a USER sound on part 2 ten times; the missed-block counter and the two-core fault counters stay at 0, and the trim counter stays at 0.

**Acceptance:**
- The new check passes; `tcstage1` and the two-core groups pass; the `park_stuck` scene keeps its FLASH strike.
- No change to rendered audio in any pinned scene.
- Within ±20 B flash, 0 RAM. The audio interrupt's code is unchanged.

**Owner questions:** none.

---

<a id="fr7"></a>
## FR7. Small control fixes (D1, D2, D6, D7, D9; P2-P3, S)

**Why:** five slips in the button and knob handling, all main loop. Trace: [lanes/D.md](lanes/D.md).

**Steps** (one commit each, a `ui` group check first for each):
1. **D1, RENAME types stray letters:** the SAVE dialog opens NAME on a knob 2 detent and swallows nothing (`ui_input.c:2250-2253`); the rest of the turn reaches `nm_knob` (`ui_name.c:339-341`). When NAME opens from the dialog, set `kr_on[1]` / `kr_t[1]` as `glo_knob_jump` does. Check: SAVE held, K2 three detents over three `ui_input` passes, the name is unchanged.
2. **D2, OCT- acts twice:** only the undo branch sets `oct_mod` (`ui_input.c:2346-2350`). With OCT- held, any PRESETS turn sets `oct_mod` and clears the pending OCT- tap; the same for a knob turned on TIDE SETTINGS with OCT- held. Check: OCT- down, PRESETS turn, OCT- up leaves `song.octave` unchanged on AUTO TIDE, the page unchanged on MULTIS, the row value unchanged on TIDE SETTINGS.
3. **D7, the sound lands on the other part:** drop the pending place at the top of `track_select` (`ui.c:2030-2042`; `pl_pend_on = 0` or `pl_commit()`). Check: accelerated PRESETS step, ALGORITHM detent, 200 ms of polls, part 2's sound unchanged.
4. **D6, SEQ + REC:** remember at REC's press that the SEQ layer was down and let `rec_is_hold` (`ui_input.c:414-422`) honour it. Check: SEQ down, REC down, both up in one pass arms `song.rec` and leaves HOLD alone.
5. **D9, NOT SAVED:** print it only for the save kinds and NAME; "CANCELLED" otherwise (`ui_input.c:2079-2085`, `:2176-2181`).

**Acceptance:** the five checks pass; `ui`, `liverec`, `mode` pass. About 100-120 B flash together.

**Owner questions:** none.

---

<a id="fr8"></a>
## FR8. Held notes from MIDI, the keys and the arp stay counted (A3, A4, A6; P2-P3, S)

**Why:** three bookkeeping gaps. Trace: [lanes/A.md](lanes/A.md) A3, A4, A6.
- **A3:** with ARP = KEEPS RUNNING, the load's panic calls `midi_forget_track(i)` before the keep branch (`seq.c:1962-1976`), so the later MIDI note-offs find no owner and `arp_remove` never runs: the arp plays on.
- **A4:** `key_tone` checks only other keys before `input_on` (`seq.c:1060`), `midi_play` checks both (`midi_control.c:183`); a key on a note MIDI already holds raises `arp_phys` and never lowers it (`seq.c:1031-1032`). In LAYER with SPLIT a MIDI note routed only to part 2 is still released on part 1 (`midi_control.c:116-118`).
- **A6:** with MIDI IN = SEL, the centring bend after a part change goes to the new part only (`midi_control.c:43-67`); the old part's `midi_bend_target` stays bent.

**Steps** (a check first for each, in `midi_control_test.c` / `modules_test.c` / `chordkeep_test.c`):
1. A3: in the keep branch, skip `midi_forget_track(i)`. Check: `mods_keep = MODK_ARP`, MIDI on 60 / 64, load panic, note-offs, `nheld == 0`.
2. A4: add `!midi_note_held(t, x)` to `key_tone`'s skip test; call part 1's `input_off` only when `lay_route(note) & 1`. Check: MIDI on 60, key on 60, key off, MIDI off, `arp_phys == 0`.
3. A6: with ROUT SEL, when `song.sel` changes in `events_block`, zero the old part's bend target unless it still owns MIDI notes. Check whether channel aftertouch and `mod_midi` stay on the old part the same way, and treat them the same if so. Check: `G_ROUTE` 1, bend max, `song.sel = 1`, bend centre, `midi_bend_target[0] == 0`.

**Acceptance:** the checks pass; `midicontrol`, `modules`, `chordkeep`, `mode` pass. About 60 B flash. Audio interrupt, event paths only.

**Owner questions:** none.

---

<a id="fr9"></a>
## FR9. Knob locks on an ARP SEQ part follow the arp's steps (B2; P2, M, Opus)

**Why:** `seq_play` applies a step's locks and automation with the sequencer's index and only then returns for ARP SEQ (`seq.c:1531-1535`); recording stores at `seq_idx` too (`motion.c:324-325`, `:361-362`). The arp's pattern restarts on a first key (`seq.c:238-240`), can run at another rate, and shortens under POLY, so the lock ends up on another note than the one it was recorded on. Live notes already use the arp's step (`rec_arp_note`, `seq.c:918-933`). Both parts of M02, the first-start multi, are ARP SEQ. Trace: [lanes/B.md](lanes/B.md) B2.

**Steps:**
1. Test first, in `liverec` (or `tidesteps`): an ARP SEQ part, a lock on step 5, a key pressed at sequencer step 9; assert the locked value sounds on the arp's step 5. A second case with ARP RATE 1/8 and DIV 1/16.
2. For an `AM_SEQ` part, move `motion_step` out of `seq_play` into `arp_fire`, on `tide_arp_idx(t, arp_idx, len)` after `TIDE_ARP_TICK`, with `play` = the PROB roll. The seq-side call stays for other parts.
3. Record locks and motion at that same index (the next step past the half, as `rec_arp_note` does).
4. Check the PARAMETER LOCKS list and the step marks show the step that plays.

**Acceptance:**
- The checks pass; `liverec`, `tidesteps`, `motion`, `regress` pass; non-ARP-SEQ parts render bit-identical.
- About 60-90 B flash, 0 RAM. Audio interrupt: moved work, not added.

**Owner questions:** none. Lands next to the live-recording limits row (R2).

---

<a id="fr10"></a>
## FR10. UNDO and HOME bring a USER patch back with its own End B (C2; P2, S)

**Why:** `undo_swap` restores the part and calls `morph_rebase(t)` without setting `morph_ldv` / `morph_ld` (`ui.c:1396`); only a real load sets them (`upreset.c:1309-1319`). For a USER patch `morph_rebase_part` then falls to "a fresh roll" with a new seed (`morph.c:1875-1894`). `tide_home` does the same (`tide_ui.c:44-54`). Factory sounds are fine. Trace: [lanes/C.md](lanes/C.md) C2.

**Steps:**
1. Test first, in the `undo` / `storage` group: save a USER patch with a rolled End B and one pinned value, load another sound, undo; compare `morph_cur(mp, 1)` and the pins with the saved ones. The same through `tide_home`.
2. When the restored part plays a stored record (`UP_REC_USER`), re-read its End B block from the record and set `morph_ldv` before the rebase (the decode is already in `up_load`). Don't keep a RAM copy: RAM is tight.

**Acceptance:** the checks pass; `undo`, `patch`, `tidecontrols` pass. About 40-60 B flash, 0 RAM. Main loop only.

**Owner questions:** none.

---

<a id="fr11"></a>
## FR11. Words that match the unit (E2, C5, D4, D5; P2-P3, S)

**Why:** four places where the screen or the docs say something the unit doesn't do. Traces: [lanes/E.md](lanes/E.md) E2, [lanes/C.md](lanes/C.md) C5, [lanes/D.md](lanes/D.md) D4, D5.

**Steps:**
1. **E2:** ERASE DATA also erases the FM6 voice bank (`erase_data.c:64`) and the three user wavetables (`:48`). Add them to the dialog (`ui_draw.c:3322-3323`), for example "AUTOSAVE, PATCHES, MULTIS," / "PATTERNS, TABLES, FM6 BANK." within the 200 px rule; show the owner the picture before the tests. Correct `docs/INSTALL.md:60`, `docs/QUICKSTART.md:41` and the stale comment at `editor_erase.c:88`.
2. **D4:** AMOUNT still shapes hand rolls and JOURNEY's fallback roll during JOURNEY. Remove the dim (`morph_glide.c:194`, `ui_draw.c:2597`) and reword `TIDE_GUIDE.md:24`. Correct the ROADMAP's "c3 ... K4 dim in JRNY, where it does nothing".
3. **D5:** dim TURNS when the glide doesn't run on both parts: in `jl_needs` (`tide_ui.c:190-197`) return "BOTH PARTS" for `JL_TURNS` when `(gl.on ? gl.mask : (song.both ? 3 : 0)) != 3`. Extend the `jl_dimmed` check in `tests/mode_test.c`.
4. **C5:** `docs/MOD_INERT_TABLE.md:21`, ANALOG row: "PW with any WAVE but PWM"; update the table's version header.

**Acceptance:** `ui`, `mode`, `erase` pass; `check_docs.py` passes. About 40 B flash.

**Owner questions:** none (the dialog picture is shown, not asked).

---

<a id="fr12"></a>
## FR12. SAVE with BOTH from the read-only places uses two USER slots (D3; P3, S)

**Why:** both parts' `slot_home` resolve to the first empty USER slot (`upreset.c:1579-1599`), so the check at `ui_input.c:851-853` says SAME SLOT and nothing is saved, though `patch_save_do` would give part 2 the next slot (`upreset.c:1659-1660`). M17 DUNE HORN hits it. Trace: [lanes/D.md](lanes/D.md) D3.

**Steps:**
1. Test first, `ui` group: both parts on #165 / #168, BOTH, SAVE held; expect a dialog naming two different USER slots and both saved on OCT+.
2. In the BOTH case pick two distinct empty USER slots at ask time, keep them for the dialog (`ui_draw.c:3268-3275`) and for `patch_save_do`; refuse with "NEEDS 2 USER SLOTS" only when fewer than two are free.

**Acceptance:** the check passes; `ui`, `patch` pass. About 60 B. Schedule with the USER-slot / patch-capacity work.

**Owner questions:** none.

---

<a id="fr13"></a>
## FR13. Pattern polish (B3, B5, and B4 into the bass brief; P3, S)

**Why:** Trace: [lanes/B.md](lanes/B.md) B3, B4, B5.

**Steps:**
1. **B3:** `voicelead_walk` builds candidates from a forced snap (`seq_voicelead.c:42`, `:57`), so with OFF-SCALE PLAY an out-of-scale chord tone is moved onto the scale. Take `base = s->note[id]` and accept a candidate that equals the original pitch class or is in the scale. Check in `tidevoicelead`: OFF-SCALE PLAY, C major, step {C E G Bb}, VOIC +60, Bb survives. ~10 B, main loop.
2. **B5:** on the internal clock nothing rescales `seq_pos` / `arp_pos` / `clk_pos` at a tempo jump (tap tempo writes `G_BPM` directly, `ui_input.c:1109`). In `events_block`, while playing on the internal clock, remember `beat_samples()` and call `clock_rescale(old, new)` when it changes. Check: play at 120, set 220 mid-step; at most one step fires in the next block and the arp's next onset is on the 1/16 grid. ~24 B flash, 4 B RAM.
3. **B4:** the pitch walk's C3 guard reads an ARP SEQ step's offset, not the played note (`seq_evolve.c:30`). Add it to the existing bass brief (tide-morph [B1](../tide-morph/TASKS.md#b1)): for `AM_SEQ` parts test `lowest stand-in note + note[hi] - 60 < 48`.

**Acceptance:** the checks pass; `tidevoicelead`, `swing`, `tidemotif` pass.

**Owner questions:** none.

---

<a id="fr14"></a>
## FR14. No stale patch record after an interrupted save (E3; P3, M, Opus, before 1.0.0)

**Why:** a save that moves a record to another chunk writes the new one, then cuts the old (`upreset.c:927-945`). A power cut between the two leaves both; the newer wins at boot. But RESTORE rewrites only the chunk the directory points to (`upreset.c:948-952`), and RESTORE ALL skips chunks with no counted record (`:1011-1012`), so the stale record stays and is adopted at the next boot; a new save into that slot starts at revision 0 and loses to it. Trace: [lanes/E.md](lanes/E.md) E3.

**Steps:**
1. Test first, `patch` group on the simulated flash: cut power after the first write of a move, boot, RESTORE the slot, boot; expect the factory sound. The same with a new save in place of RESTORE; expect the new sound.
2. In `pt_scan_img`, mark a chunk that holds a well-formed record the directory doesn't point to (a 3-byte mask). Rewrite each marked chunk once at the first quiet moment after boot (`pt_write(c, UP_SLOTS, 0)`), and have `pt_wipe` and the restore path include marked chunks.

**Acceptance:** the checks pass; `patch`, `storage`, `erase` pass. About 80-120 B flash, 3 B RAM. Main loop only; no write while the transport runs.

**Owner questions:** none.

---

<a id="fr15"></a>
## FR15. Housekeeping (E5, F3, E4; P3, S)

**Steps:**
1. **E5:** `tools/build.py --release` stops when `FELUCCA_CDC`, `DUELL_TEST_MODE`, `DUELL_BENCH`, `FELUCCA_OTA_DRYRUN` or `FELUCCA_OTA_RAMONLY` is set to 1 in the environment (`build.py:362-390`). A release-tool test: a `--release` call with `FELUCCA_CDC=1` fails. No firmware bytes.
2. **F3:** the flash-write dip fades only the DAC buffer (`audio.c:110-145`), so a USB recording gets a hard gap. Apply the duck gain to the block before `uac_tap` when `duck_on || duck_g < 32768`. Check in the duck's host group: frames written to `ua_ring` during the ducked halves fade to 0 and back. ~40-60 B, audio interrupt, only around a flash write.
3. **E4 (check first, then decide):** count flash erases during a 2 s slider drag in the editor's MENU tab (emulator, `felucca_dbg.erases`). If more than one, use the device debounce in `ed_menu_save` (`editor_menu.c:92-94`) and check how `web/editor` treats the "queued" reply. If one, close the item.
4. On the next test-mode sweep, read core 1's stack peak (`tc_stack_peak`) with a JOURNEY crossing and WASH on; record it in `docs/TWOCORE_STAGE1.md`. The 1,744 B figure predates TIDE's crossing work on core 1 (stack: 3 KB).

**Acceptance:** per step. Trace: [lanes/E.md](lanes/E.md) E4, E5; [lanes/F.md](lanes/F.md) F3.

**Owner questions:** none.
