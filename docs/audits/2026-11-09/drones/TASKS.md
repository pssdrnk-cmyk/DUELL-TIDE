# Built-in drone multis (b6), 9 November 2026

**Owner, 9 November:** the drone scenes go into the firmware, not a file to import. "It can be 2 or 4, no one asked for 8, and yes, find room for it." The factory multis M01 to M16 stay as they are.

**What went wrong before:** in b4 the eight drone multis were built as a file pack (`packs/drones/`), with no download, no release-notes line, and the roadmap still calling them an idea. So the owner never had them. This brief replaces the file pack.

Rule for every agent on this: **2 errors equals agent death.** A second real mistake ends your run, and a fresh agent takes over from your WIP commit.

## D1: four drone multis as M17 to M20, in ROM

### What the player gets

- The MULTIS list goes on past M16 to **M17 to M20**, four drone scenes, one per theme:

  | Slot | Name | Part 1 | Part 2 | Theme |
  |---|---|---|---|---|
  | M17 | DUNE HORN | HORN SWELL | VOID SHIMMER | Desert |
  | M18 | NEON ROOF | NEON BRASS | RAIN STRINGS | Neon city |
  | M19 | VOID DEEP | VOID SHIMMER | SOLAR WIND | Space |
  | M20 | IRON YARD | IRON WORKS | SAND THROAT | Cyberpunk |

  Each one keeps the file pack's settings exactly: LAYER, both parts latched (HOLD ALL, arp OFF), the pack's part levels, a 60 BPM ping-pong echo, and a big HALL. The source is `felucca-base/tools/drone_pack.py`, the `DRONES` rows with those names.
- **They just work.** Pick M17 to M20, hold a chord around C3, and press PLAY with AUTO TIDE on. TIDE plays them as STYLE DRONE with no trip to TIDE SETTINGS (see "STYLE" below).
- **They are factory only.** They load, play and preview like M01 to M16. Their row shows the same factory marking as an unchanged factory slot. SAVE onto M17 to M20 is refused, with the same kind of message the SAVE pages already use for a refused target, pointing to M01 to M16; the save picker stops at M16. ERASE on them says ALREADY FACTORY. An edited drone multi is saved into M01 to M16 like any other.
- **MIDI program change** 16 to 19 loads M17 to M20 (today it stops at 15: `midi_control.c:626`).

### Why ROM-only (the room)

- **Flash for user data is full.** `storage.c:22-49` and `fm1_flash.h:38-53`: FL_DATA 0x97000 to 0xDFFFF is all taken (multis, patches, wavetables, velocity, pattern slots). Above it are the OTA staging, the autosave, the FM6 voice bank, the stock firmware's never-written areas, and the settings. There's no free A/B pair for writable multis 17 to 20, and taking one would mean moving another store (a format change, which "How we work" forbids).
- **The image has room.** The four records are 463 to 630 bytes each, about 2 KB together (from `packs/drones/drones.duell-multis.json`, the `size` field). Factory multis already live in ROM (`multi.c:40-45`, `duell_multis.h`), and an empty slot shows its ROM copy. M17 to M20 are ROM copies with no flash slot behind them.
- **RAM:** about 34 bytes a slot (`mu_names` 13, `mu_vp` 8, `graph_pname` 13). That's about 140 bytes for four, inside the b5 pool tail of 9,276 bytes. Measure it anyway (see "Acceptance").
- If the device build shows the image or RAM can't take four, ship **two** (M17 DUNE HORN, M18 VOID DEEP) and say so in the report. Don't cut anything else to make room.

### How (the builder's choices, within these limits)

1. **Two counts, not one.** Keep `MU_SLOTS` (16) as the *stored* slot count. It sets `MU_NOBJ`, the flash objects, and `OBJ_PATCH0 == OBJ_MULTI0 + MU_NOBJ` (`multi.c:62`, `:104`; `storage.c:53-59`), and none of those may change. Add a *shown* count (for example `MU_SHOWN` = 20) for every list, label, picker, preview and program change. Go through every `MU_SLOTS` use (`grep -rn MU_SLOTS felucca-base/firmware/src felucca-base/tests`: bench.c, console.c, midi_control.c, multi.c, ui.c, ui_draw.c, ui_graph.c, ui_input.c, ui_menu.c, ui_name.c, and the tests) and decide for each one whether it means stored or shown. The `slot % MU_SLOTS` guards are the trap: M17 would wrap to M01. Write the decision per call site into the commit message.
2. **Bitmasks:** `mu_used`, `mu_fact` and `mu_bad` are `uint16_t` (`multi.c:89`, `:106`, `:378`). Widen them to `uint32_t`. Slots 16 to 19 have `mu_used` and `mu_fact` set at boot and never `mu_bad`.
3. **Records:** generate the four records with the same machinery as the factory multis and the pack (`tools/factory_multis.py`, `tests/gen_factory_multis.c`, `tools/drone_pack.py`) into `duell_multis.h` with `DF_SLOT` 16 to 19. Lift `_Static_assert(DF_N <= MU_SLOTS)` (`multi.c:105`) to the shown count. Loading reads the ROM record directly; there's no flash read for 16 to 19.
4. **Saving is refused at every entry:** the SAVE > MULTI picker, the name page (`ui_name.c:123`), `project_save_as` and its callers (`ui_input.c:2210`), the console (`console.c:355`), bench (`bench.c:2431`), and the editor's "Save to unit" (it refuses slots past 16 with the unit's own reply).
5. **STYLE:** a multi doesn't store TIDE's STYLE (`packs/drones/README.md` step 4). For M17 to M20 the player shouldn't have to pick it. While one of M17 to M20 is the loaded multi, TIDE plays STYLE DRONE. Loading any other multi returns to the STYLE saved in TIDE SETTINGS, and the saved STYLE value itself never changes. TIDE SETTINGS shows DRONE while a drone multi is loaded. Mark it the way the page already marks a value that comes from somewhere else, or, if no such marking exists, just show DRONE (no new page, no new row). This costs no CPU in the audio interrupt: it's read where STYLE is read today.
6. **The web editor:** the MULTIS tab lists M17 to M20 as factory, read-only (Recall and Load from unit work; Save to unit and Import into them don't). Backup files don't change: nothing is stored for 17 to 20, so the backup format stays the same (`web/editor/backup.js`, `duellbackup.js`).
7. **The file pack:** remove `packs/drones/` and `tools/drone_pack.py`'s pack output, or keep the tool only as the record source for step 3. Nothing shipped should point the player at importing a file.

### Docs (in the same change)

- CHANGELOG-TIDE under 1.0.0-b6: "Four drone scenes built in as M17 to M20 (DUNE HORN, NEON ROOF, VOID DEEP, IRON YARD), factory only, TIDE plays them as STYLE DRONE."
- QUICKSTART, TIDE_GUIDE, SPEC (the multi list), PATCH_NUMBERS if it lists multis, EDITOR_LIBRARIAN (MULTIS tab), and the cheatsheet's MIDI program change line (`web/cheatsheet/index.html:129`: 0 to 19).
- `check_docs.py` passes.

### Tests

- Host: M17 to M20 load and match their ROM records byte for byte. SAVE, rename and ERASE on them are refused. M01 to M16 save, load and erase exactly as before. A b5 flash image's multi objects read back unchanged (no format change). Program change 16 to 19 loads them, and 20 and above are ignored. The STYLE override is on while a drone multi is loaded, off after another multi loads, and the saved STYLE never changes.
- Budget and CPU tests: add M17 to M20 to the factory-multi sweeps (`budget_test.c`, `bench_sim.c:813`, `cpu_scenarios_test.c`). Each holds a 3-note chord with 0 missed blocks on the host, as the drone sounds did in b2.
- On the unit (the release sweep, not homework for the owner): each of M17 to M20 with a held 3-note chord and AUTO TIDE on, 0 missed and 0 shed.

### Acceptance

- Device build: the image size and the RAM pool tail before and after go in the report, and the hot-code check is clean. PR CI is green.
- Look first: one emulator picture of the MULTIS list scrolled to M17 to M20, and one of the SAVE refusal, shown to the owner before the paperwork.
- Size: roughly 2 KB ROM and 140 B RAM; one builder, one review pass (correctness), no extra rounds.
- Depends on nothing else in b6. It touches multi.c and the UI list code, so merge after the MORPH page work, or rebase onto it.
