# Lane D: screens and controls
Reviewed: read fully: `ui_input.c`, `ui.c`, `ui_name.c`, `ui_layer.c`, `ui_perform.c`, `ui_menu.c` (input and menu parts; ABOUT / TEST drawing skimmed), `menu_items.c`, `ui_events.c`, `ui_tools.c`, `ui_guide.c`, `ui_fm6op.c`, `tide_ui.c`. Skimmed: `ui_live.c` (overlay drawing only), `ui_boxes.c` (drawing only). Followed out of the lane where a trace needed it: `upreset.c` (slot_of / slot_home / patch_save_do / up_put), `multi.c` (project_load, project_save_as), `morph.c` (morph_do, morph_view_fm, morph_roll_seed), `morph_glide.c` (glide_start / launch / stop, sqm_auto), `seq_morph.c:314-320`, `midi_learn.c:310-450`, `main.c:255-295` (ui_input is called many times a frame), `ui_draw.c` (confirm_text / draw_confirm, AMOUNT dim at :2597).
Not reviewed: the rest of `ui_draw.c`, `ui_graph.c`, `ui_scope.c`, `gfx.c`, `icons.c`, `lcd.c` (pixel layout, out of scope); `midi_learn.c:1-290` (map table); the editor side of MENU_SET / LEARN_SET.

## What is solid
- Dialog and NAME exits: OCT- / OCT+ act on release and only for a press that began there (`oct_taps`); a page button, HOME or HOME held cancel without writing; PLAY in NAME stops a transport so the name can be saved; ERASE DATA's dialog cancels on every way out. No dialog or overlay was found that cannot be left.
- Flash writes from the UI all re-check the transport at write time (`project_save_as` multi.c:590, `up_put` upreset.c:1089, `patch_save_do` :1654, `name_ok` ui_name.c:281); STORE FULL reaches the screen from `up_put`'s PT_FULL through `TIDE_SAVE_ERROR`. M17 / M18 are refused at every UI save entry (SAVE held, MULTIS K3 + OCT+, `project_save_as`).
- Swallowing of leftover detents: ROLL / GLIDE gestures (`gesture_det`), OCT- reset (`kr_on`, 300 ms), the DUCK and module jumps (`glo_knob_jump`), a layer letting go (`LY_QUIET_MS`), dialogs (`enc_drop`). Knob acceleration (`accel`) is correct, including its 24-bit wrap.
- List bounds after a list shrinks: `pat_pick`, `pl_cursor` (pending place re-checked against the new total), `preset_visible`, `ev_row` (EV_ROWS = 3 + 128 + 256 is exact for `ev_list`), `menu_move`, `cursor_fix`. FAV removed or TYPE changed while browsing ends on "--" and a step goes to the first / last entry, as designed.
- TIDE SETTINGS: XFADE / SPLIT are dim and inert outside LAYER (`jl_needs` and `mode_knob` agree), SPLIT's first step out of OFF lands on F#4 for knobs and OCT+, OCT- + K3 / K4 on AUTO TIDE say "TIME 16" / "WAIT 2" (audit finding 29 is fixed for TIDE), the STYLE / CUSTOM / drone-multi keep logic stores the kept rows. The FM6 operator cards show stored + morph delta and a turn moves the shown value by one (no jump).

## Findings (most important first, at most 15)
### D1. RENAME in the SAVE dialog types stray letters into the name  (P2, CONFIRMED, functional)
- **What the player meets:** hold SAVE, then turn knob 2 (RENAME) by an ordinary twist of two or more clicks. The NAME screen opens, and the rest of the twist adds a character at the end of the name ("DEEP PADA", or "+" when turned left), which then cycles with each further click. A name that is already 12 long shows "NAME FULL" instead. If the player presses OCT+ without looking, the patch is saved under the wrong name.
- **Why (code):** the dialog opens NAME on any K2 detent read (`ui_input.c:2250-2253`) and swallows nothing after it. `ui_input` runs many times a frame (`main.c:269, 286`), so the next reads of the same twist reach `name_input` → `nm_knob(1, s)` (`ui_name.c:339-341`). With the cursor at the end (`name_open` sets `cur = len`, :130), `nm_knob` inserts a space and turns it into `NM_SET[s]` (`ui_name.c:261-266`). Compare the K3 SAVE and DUCK jumps, which drop their leftovers (`enc_drop`, `kr_on`).
- **Fix sketch:** when NAME opens from the dialog, set `kr_on[1]` / `kr_t[1]` (or a 300 ms K2 quiet in `name_input`), as `glo_knob_jump` does. About 12 B, main loop only.
- **Test:** `ui` group (`tests/ui_test.c`): SAVE held, K2 turned 3 detents over 3 `ui_input` passes; the name stays as it was.
- **Known?** not recorded.

### D2. OCT- held as a modifier still acts as OCT- when let go, where it has no modifier meaning  (P3, CONFIRMED, functional)
- **What the player meets:** the load undo (OCT- held + a PRESETS turn) does not work on the TIDE pages, in PERFORM or on PATTERNS / MULTIS / TOOLS (documented). Trying it anyway gives more than "nothing": on AUTO TIDE and in PERFORM the sound loads and, on letting go of OCT-, the keyboard drops an octave. On PATTERNS / MULTIS / TOOLS the page jumps to HOME. On TIDE SETTINGS the picked row steps one value back, and the same happens after OCT- held + knob 2-4 (the "Hold OCT- and turn the knob" reset the guide teaches): the knob steps the value, the release steps it back, so a right turn does nothing and a left turn moves two values.
- **Why (code):** only the undo branch marks OCT- as used (`oct_mod = 1`, `ui_input.c:2346-2350`); the plain PRESETS branch (:2351-2355) does not. AUTO TIDE and PERFORM are not blocked in `oct_rel` (:2093), so the release moves the octave (:1576-1580). On the action pages and TIDE SETTINGS the press is recorded by `oct_taps` (:2091) and the release is dispatched as a tap (:2315-2326: `go_main` at :2321, `TIDE_LIST_OCT` at :2325). `tide_reset` returns 0 on TIDE SETTINGS (`tide_ui.c:359-360`), and `oct_pend` is cleared there one pass after the press, so the knob turns act normally.
- **Fix sketch:** with OCT- held, any PRESETS turn sets `oct_mod` and clears the pending OCT- tap in `oct_taps` (a "used" bit). The same for a knob turned on TIDE SETTINGS while OCT- is held. About 30-40 B, main loop.
- **Test:** `ui` group: OCT- down, PRESETS turn, OCT- up on AUTO TIDE (`song.octave` unchanged), on MULTIS (page unchanged), on TIDE SETTINGS (row value unchanged).
- **Known?** not recorded. Related to QUICKSTART:178 (the pages where the undo does not work).

### D3. SAVE held with BOTH refuses "SAME SLOT" for two sounds from the read-only places  (P3, CONFIRMED, functional)
- **What the player meets:** load a multi whose parts both play factory sounds from #125 to #171, for example M17 DUNE HORN (HORN SWELL #165, VOID SHIMMER #168). On MORPH with ALGORITHM on BOTH, hold SAVE: the screen says SAME SLOT and nothing is saved. TIDE_GUIDE:26 promises "with ALGORITHM on BOTH each part goes over its own slot". Saving one part at a time works.
- **Why (code):** both parts' `slot_home` resolve to `slot_user()`, the first empty USER slot (`upreset.c:1596-1599`, `:1579-1588`), so the check `slot_home(&trk[0]) == slot_home(&trk[1])` (`ui_input.c:851-853`) fires. `patch_save_do` would already have given part 2 the next USER slot, because it calls `slot_home` again after part 1 is saved (`upreset.c:1659-1660`). The dialog text (`ui_draw.c:3268-3275`) shows the same two slots.
- **Fix sketch:** in the BOTH case, pick two distinct empty USER slots at ask time, keep them for the dialog and for `patch_save_do`, and refuse only when fewer than two are free ("NEEDS 2 USER SLOTS"). About 60 B.
- **Test:** `ui` group: both parts on #165 / #168, BOTH, SAVE held → dialog "U1 + U2", OCT+ → both saved.
- **Known?** not recorded.

### D4. AMOUNT is dimmed during JOURNEY but still shapes rolls  (P3, CONFIRMED, functional)
- **What the player meets:** with AUTO TIDE running JOURNEY (STYLE DUB's default), the MORPH page draws AMOUNT dim, "where it does nothing" (TIDE_GUIDE:24). K4 still changes it. It also still decides how far a hand ROLL (K2, active during JOURNEY) goes, and how far JOURNEY's own roll goes for a sound with no other in its family.
- **Why (code):** the dim is `gl.on && gl.mode == GM_JOURNEY` (`morph_glide.c:194`, `ui_draw.c:2597`). K4 has no guard (`ui_input.c:757-758`). The hand roll and JOURNEY's fallback (`tide_ui.c:524-525` → `gl_sound_roll`, `morph_glide.c:42-48`) both go through `MQ_ROLL` → `morph_roll_seed`, which uses the part's AMOUNT (`morph.c:2098-2101`), and the MUSIC roll takes it as its scale (`morph_tide.c:81`).
- **Fix sketch:** either drop the dim (0 B, TIDE_GUIDE:24 reworded), or dim only when the part's family has other sounds and no hand roll is possible. The first is the honest small fix.
- **Test:** `ui` group's MORPH-page dim case; docs check.
- **Known?** corrects ROADMAP "MORPH c3 ... K4 dim in JRNY, where it does nothing" and TIDE_GUIDE:24. Hand-off: lane C (morph) for whether JOURNEY should ignore AMOUNT instead.

### D5. TURNS looks active when only one part glides  (P3, CONFIRMED, functional)
- **What the player meets:** on TIDE SETTINGS, TURNS is drawn in full colour and knob 1's line names nothing missing whenever the mode is WANDER / JOURNEY with MUSIC and SEQ in TARGET. When the glide was started on PART 1 or PART 2 alone, TURNS (TAKE TURNS, LEAD) does nothing. The guide says "only ... with both parts"; the screen does not.
- **Why (code):** `jl_needs` checks mode, ROLL TYPE and TARGET only (`tide_ui.c:190-197`). TURNS needs `sqm_auto == 3` (`seq_morph.c:314, 320`), and `sqm_auto` is `gl.mask` (`morph_glide.c:278`), the parts the glide was launched on.
- **Fix sketch:** for `JL_TURNS`, return "BOTH PARTS" when `(gl.on ? gl.mask : (song.both ? 3 : 0)) != 3`. About 24 B; `tests/mode_test.c` already checks `jl_dimmed`.
- **Test:** `mode` group: TURNS dimmed with a PART 1 glide, live with a BOTH glide.
- **Known?** not recorded.

### D6. "SEQ + REC" from HOME toggles HOLD when SEQ is let go first or together  (P3, CONFIRMED, functional)
- **What the player meets:** on HOME, REC is HOLD, and the message itself says "HOLD ON: SEQ+REC=REC". Pressing SEQ and REC as a chord and lifting both together (or SEQ first) toggles HOLD instead of arming recording.
- **Why (code):** REC's tap fires on its release (`btn_hold`, `ui_input.c:2070`). `rec_is_hold` decides by `ui.layer == LAYER_SEQ` at that moment (:414-422). The SEQ release earlier in the same pass has already closed the layer (`layer_gesture` / `layer_show`, :2126-2127, `ui_layer.c:340-347`) before `rec_tap` runs (:2152).
- **Fix sketch:** remember at REC's press that the SEQ layer was down (one bit in `ui.rec_t0`) and let `rec_is_hold` honour it. About 16 B.
- **Test:** `ui` group (the liverec cases): SEQ down, REC down, both up in one pass → `song.rec` armed, HOLD unchanged.
- **Known?** not recorded; related to liverec L5 (REC = HOLD on HOME).

### D7. A fast PRESETS turn followed quickly by ALGORITHM loads the sound into the other part  (P3, CONFIRMED, functional)
- **What the player meets:** with KNOB ACCELERATION on (the default), a fast PRESETS turn loads only after 150 ms of rest. Turning ALGORITHM to the other part within that time loads the waited-for sound into the newly selected part.
- **Why (code):** the pending place (`pl_step`, `ui.c:1905-1910`) is dropped by `page_entered`, `list_set` and `load_begin`, but not by `track_select` (`ui.c:2030-2042`). `pl_poll` then commits into `TSEL` (`ui.c:1880-1888` → `preset_go` → `sl_load`, "slot k -> the selected part").
- **Fix sketch:** `pl_commit()` (or `pl_pend_on = 0`) at the top of `track_select`. About 6 B.
- **Test:** `ui` group: accelerated PRESETS step, ALGORITHM detent, 200 ms of polls → part 2's sound unchanged.
- **Known?** not recorded.

### D8. With BOTH, MORPH's fader and AMOUNT knobs jump the other part to the shown part's value  (P3, CONFIRMED, functional)
- **What the player meets:** on MORPH (and PERFORM's K1) with ALGORITHM on BOTH, the first click of K1 or K4 sets both parts to the selected part's value plus one step. A part 2 left elsewhere (a glide on one part, a different AMOUNT per part from the settings or a saved multi) jumps.
- **Why (code):** the new value is computed from `song.sel`'s part only (`ui_input.c:749, 758`; `ui_perform.c:164`) and written absolutely to every part in the mask (`morph.c:2659-2665`).
- **Fix sketch:** apply the step relatively per part (`morph_fader_of(pi) + d` for each `pi` in the mask), about 40 B. Or keep it as a deliberate "BOTH = same value" and say so in TIDE_GUIDE §1. Owner's call: plain DUELL's MT_BOTH behaved the same way.
- **Test:** `ui` group: parts at fader 0 / 90, BOTH, K1 +1 → 1 / 91 (if relative is chosen).
- **Known?** not recorded.

### D9. Cancelling any dialog by a page button or HOME says "NOT SAVED"  (P3, CONFIRMED, functional)
- **What the player meets:** "CLEAR PART 1 SEQUENCE?", "INITIALIZE SOUND?", "REPLACE PART 1 SEQUENCE?" or "RESTORE ALL PATCHES?" cancelled by HOME or a page button shows NOT SAVED, though nothing was being saved.
- **Why (code):** `ui_input.c:2079-2085` and `:2176-2181` print "NOT SAVED" for every `ui.confirm` kind.
- **Fix sketch:** "NOT SAVED" only for the save kinds (CF_SAVE_OVER, CF_OVR_*, CF_HOLD_*) and NAME, "CANCELLED" otherwise. About 20 B.
- **Test:** `ui` group message check.
- **Known?** not recorded.

## Roadmap suggestions
- Next beta, one small UI-fixes change (all main loop, about 200 B together): D1 first (it can save a wrong name), then D2, D7, D6, D9.
- With the TIDE docs pass (ROADMAP "Later" 7): D4 (drop the dim, reword TIDE_GUIDE:24) and D5 (TURNS needs both parts).
- D3 with the USER-slot work (U4 / patch capacity wording), since it changes what SAVE offers.
- D8 needs the owner's yes before any change (relative steps against "both the same").
- Nothing to drop from the existing UI rows.
