# Lane C: MORPH, JOURNEY, TIDE control, parameters and sound loading
Reviewed: `morph_glide.c` (read fully); `morph.c` (read: the blend `morph_build` / `morph_apply_at` / `morph_begin`, the rebase and load paths `morph_rebase_part` / `morph_ready` / `morph_load_part(_v)` / `morph_bv_make` / `morph_bv_load` / `morph_blk_make`, EDITABLE B `morph_b_mode` / `morph_b_set` / `morph_pin_load`, the queue `morph_do` / `morph_push` / `morph_pop`; the roll internals skimmed); `morph_tide.c` (read fully); `morph_music.c` (head and bands read); `tide.h` (read fully); `multi.c` (`project_load`, `mu_unpack` read); `favorites.c`, `category.c` (read); `sounds.c` (head read); `mod.c` (inert / offered / sources / gain read, `mod_begin` not read); `params.c` (`param_fit` and the desc lookups only); `budget_feat.c` (read). Followed out of the lane: `tide_ui.c` (TIDE SETTINGS rows, STYLE, launch, `tide_home`, TIDE store fields), `ui.c` `undo_swap`, `ui_input.c` `morph_act` / `morph_poll` / `morph_settings` / `load_undo_gate`, `midi_control.c` CC1 / CC70 / CC121, `upreset.c` `up_patch_save_part`.
Not reviewed: `morph_xfer.c`, `morph_complement.c`, `morph_fm_shape.c` (covered by the 9 November audit), `engines.c`, `budget.c` (the ease ladder), `jrny_ctx.h`, `tide_journey`'s pick (audit J1), the rest of `params.c` and `mod.c` (`mod_begin`, LFO sync and retrigger: not traced, no claim either way).

## What is solid
- **Hand over a GLIDE:** a knob, CC 70, the mod wheel, a reset or a load each changes the fader or `mm_loads`. `glide_touched` (`morph_glide.c:134-140`) then stops that lane at the next pass, also during WAIT. A move made while stopped is caught at PLAY. A move between launch and the first downbeat is absorbed (`glide_begin_lanes` at `:291`, `:314`). STOP pauses to the sample, a STOP before a queued PLAY cancels the launch (`:287`).
- **What plays is what is shown:** the ISR reads `mp->fader` every block (`morph.c:1635`); the cached blend lags by at most 1-3 blocks while moving and catches up (`:1647-1656`).
- **An ordinary edit mid-morph** is an offset on both ends for continuous values (`morph.c:1481-1483`), and the player's own choice wins on switches and faded choices (`:1485-1489`, `:1513-1514`). At 127 an editable value goes to End B (`morph_b_mode`, `:2146-2152`), and a shared value says so. A hand-set B value survives SAVE (`morph_bv_make` stores pins, `:2381-2389`).
- **TIDE SETTINGS rows:** the `JL_V` / `JL_V0` name table lines up with every row's values. STYLE writes exactly the nine rows it names and leaves TARGET / ROLL / ARP / PITCH / REC alone (`tide_ui.c:150-165`). CUSTOM keeps and restores the hand values. The nine rows, STYLE and REC persist through `TS_VAR` with a range check on read (`tide_ui.c:981-984`, `:1031-1033`). The drone multis' STYLE swap is kept out of the store (`:1050-1054`).
- **Stored values are fitted on load:** patches (`upreset.c:206`, `:677`), projects (`project.c:272`, `:342`), locks (`motion.c`), and End B values decode within their stored bit range (`wv_val`). Multi records reject out-of-range bytes (`multi.c:288-345`). Favourites index safely: rows 14 and 15 are excluded, WAVE has its own bytes (`favorites.c:19-24`).

## Findings (most important first, at most 15)

### C1. GLIDE on BOTH snaps the other part's fader to the selected part's position  (P2, CONFIRMED, functional)
- **What the player meets:** select PART 2, move its MORPH fader to about 100 (by hand, CC 70 or the mod wheel on part 2's channel), keep part 1 at A. Select BOTH, then GLIDE TO B (AUTO TIDE K1, PERFORM GLIDE TO or MIDI) in any mode. Part 2's fader jumps from 100 to 0 in one block at the trip's start and then travels A→B with part 1, so part 2's sound falls back to A. Every switch-type value of part 2 flips at once: the same hard switch the 9 November audit measured for J2, here on a part that wasn't touched. On AUTO TIDE, part 2's path is drawn from part 1's start, so the screen never shows the jump. The same happens with GLIDE TO A when the parts sit at different points.
- **Why (code):**
  - The trip has one start, read from the selected part only: `gl.from[l] = GLIDE_LANE[l].get(gl.pi)` (`morph_glide.c:146`).
  - Each part's own position is recorded only to detect a hand (`glide_seen`, `:125-133`).
  - The first pass computes `v` from `gl.from` (`:348-350`). It sees `gl.last[l][k] != v` for the other part and writes `v` to every part of the mask: `GLIDE_LANE[l].set(gl.pi, gl.mask, v)` → `gl_sound_set` → `morph_do(MQ_FADER, mask, v)` (`:352-356`, `:40`, `morph.c:2660-2663`).
  - Switch values follow the side at once (`morph.c:1511-1516`). The page draws `gl.from[c]` for both parts (`tide_ui.c:693-696`).
- **Fix sketch:** keep `from` per part (`uint8_t from[GLIDE_LANES][NSHOWN]`, +2 B RAM) and set each part on its own path (`set` with `1u << k`). About 30 B flash, main loop only, no audio-interrupt change. Alternative: refuse BOTH launches whose faders differ and say "FADERS DIFFER" (smaller, but less musical).
- **Test:** `glide` host group: BOTH launch with the faders at 0 / 100. Assert part 2 never drops below 100 on the way to B, and the page path starts at each part's own fader.
- **Known?** Not recorded. Related to audit 9 November J2 (fader not at A at a JOURNEY launch). J2's fix (replace End B only at fader 0) does not cover this, because the jump happens in every mode.

### C2. Load UNDO / REDO, and AUTO TIDE's HOME, give a saved USER patch a random End B  (P2, CONFIRMED, functional)
- **What the player meets:** load a USER patch whose End B you designed (rolled and kept, or set by hand at the B end), then load another sound. OCT- held + a PRESETS turn: the USER patch comes back with the right End A, but End B is a new random roll, and your hand-set B values are gone. REDO and UNDO again roll yet another B. OCT- + K1 on AUTO TIDE (home) does the same to a USER patch on the parts it homes. The guide promises the undo "undoes the last sound load" (`TIDE_GUIDE.md:157`).
- **Why (code):**
  - `undo_swap` restores `eng_req`, `preset`, `user` and `p[]`, then calls `morph_rebase(t)` (`ui.c:1396`). It never sets `morph_ldv` / `morph_ld` / `morph_ld_n`, which only a real patch load sets (`upreset.c:1309-1319`).
  - In `morph_rebase_part`, a USER patch (`UP_REC_USER(t)`) fails the factory branch (`morph.c:1875`) and lands in "none saved: a fresh roll at the current AMOUNT" with `morph_new_seed()` (`:1893-1894`). The pins were just cleared (`:1857`).
  - `tide_home` calls `morph_rebase_part(k)` the same way (`tide_ui.c:44-54`).
  - Factory sounds aren't hit: their B is re-made from `ds_seed`.
- **Fix sketch:** when the restored part plays a stored record (`UP_REC_USER`), re-read its MBV block from the record and set `morph_ldv` before the rebase (`upreset.c` already decodes it for `up_load`). About 40-60 B flash, main loop only. Cheaper alternative: keep the pre-load End B (P_COUNT + NX + NFM + pins, ~250 B RAM) in the undo copy. RAM is tight, so re-reading the record is preferred.
- **Test:** `undo` / `storage` host group: save a USER patch with End B rolled and one value pinned. Load another sound, undo, then compare `morph_cur(mp, 1)` and the pins with the saved ones. Same for `tide_home`.
- **Known?** Not recorded. KNOWN_LIMITS "exact live-morph continuation is not promised" covers restarts, not an undo or HOME of a saved record.

### C3. Loading a factory multi turns ROLL ARP and ROLL PITCH off, and ROLL TYPE WILD into SMART, for good  (P2, CONFIRMED, functional)
- **What the player meets:** set TIDE SETTINGS ROLL PITCH ON (or ROLL ARP ON, or ROLL TYPE WILD). Load any factory multi (SAVE > MULTIS, or a MIDI program change in MULTI LOAD). The rows now read OFF / OFF / SMART, the next rolls and the pitch walk follow that, and the change is saved into the settings, so it survives a power cycle. ROLL TYPE MUSIC is not affected (its bit is TIDE's own). The guide presents these rows as the player's ("TARGET, ROLL, ROLL ARP and ROLL PITCH stay yours", `TIDE_GUIDE.md:59`) and says a load keeps "MODE and the auto state" (`:79`). A saved user multi restores whatever MORPH SET byte it was saved with, which is DUELL's multi format.
- **Why (code):**
  - `project_load` sets `morph.cfg = set[2]` for every multi, factory too (`multi.c:751`). `set[2]` is record byte 6 (`multi.c:382`), and it is 0 in all 16 factory records (`tools/factory_multis/M01..M16.bin` byte 6).
  - `morph_settings` writes `morph.cfg` back into the settings bytes, so the change persists (`ui_input.c:769-791`).
  - The ROLL row reads WILD from `MC_WILD` but MUSIC / SMART from `morph_musical` (`tide_ui.c:225`). Clearing `MC_WILD` with `morph_musical` at 0 therefore shows SMART.
  - b6 already made factory multis keep the player's AMOUNT (`multi.c:752-756`); `morph.cfg` was not given the same rule.
- **Fix sketch:** under `DUELL_TIDE`, `if (!fact) morph.cfg = set[2];` (the AMOUNT rule of b6). About 8 B flash, main loop only. Owner choice: also ignore it for user multis, since in TIDE these are SETTINGS rows.
- **Test:** `tidecontrols` or `mode_test`: set ROLL PITCH ON and WILD, load factory M02, and assert `jl_get(JL_PITCH) == 1` and `jl_get(JL_ROLL) == 2`.
- **Known?** Not recorded.

### C4. A patch SAVE after JOURNEY or ROLL A stores the starting sound as End A, not the A you hear  (P3, CONFIRMED, functional)
- **What the player meets:** run JOURNEY. After an arrival at B, End A becomes a new factory sound (the end left). Stop at A, hear sound X, hold SAVE. The saved patch is the sound you loaded at the start (its name too), with the current End B. X is lost; reloading gives the old sound at A. The same goes for an End A made by ROLL A. KEEP (SEQ + G4) does freeze what is heard, but nothing on SAVE says the two differ.
- **Why (code):** `up_patch_save_part` stores `t->p` (the Original) plus `morph_bv_make` (End B against the Original) (`upreset.c:1633-1641`, `morph.c:2360-2405`). End A (`morph_cur(mp, 0)`) and the fader are never written. `morph_to_sound` puts JOURNEY's new sound into End A's slot (`morph_tide.c:147-166`).
- **Fix sketch:** the smallest change is a message. When `tide_sound_moved(pi)` is true at SAVE, say "SAVES THE START SOUND: KEEP FREEZES" (~30 B). Saving what is heard would be KEEP's job, a design change for the owner.
- **Test:** none needed for the message. If the behaviour changes: `storage` group, save after a JOURNEY arrival and compare the reloaded A with the heard A.
- **Known?** Not recorded as such. Related to KNOWN_LIMITS "exact live-morph continuation is not promised", which is about restarts, not SAVE.

### C5. MOD_INERT_TABLE says ANALOG has no inert engine slot, but the unit hides PW unless WAVE is PWM  (P3, CONFIRMED, docs)
- **What the player meets:** the table (`docs/MOD_INERT_TABLE.md:21`, ANALOG row, "E slots with no effect: none") tells an editor user that every ANALOG engine slot is a destination. The unit and the editor's `MXH` drop PW (E4) for every WAVE but PWM and show a stored route dim with WAVE.
- **Why (code):** `mod_e_inert`: `return k == 4u && p[P_E0] != 4;` (`mod.c:215`, audit P09). This feeds `mod_dst_why` → "WAVE" (`mod.c:935-936`) → `mod_dst_offered` (`:959-967`). The table is still headed 0.2.25.
- **Fix sketch:** the ANALOG row's E-slot cell becomes "PW with any WAVE but PWM". Docs only.
- **Test:** `modinert` already renders it; `check_docs.py` doesn't read the table.
- **Known?** Not recorded.

## Roadmap suggestions
- b7, with J2: C1 (per-part glide start, ~30 B). It touches the same launch path as J2 and X1. Build both in one branch, and run the `glide` / `tidecrossings` groups with the faders apart.
- b7: C3 (one line beside b6's AMOUNT rule), and C5 into the docs-fixes row (Later 7).
- After b7: C2. Re-read the record's End B on UNDO and HOME; it needs the storage tests.
- C4: a SAVE message only; ask the owner whether SAVE should ever store the heard End A (it overlaps KEEP).
- Drop, at no flash cost: `morph_pack` / `morph_unpack` (`morph.c:2709-2767`) have no caller (the compiler already drops them, so source only). Their comments still describe the project block as live.
