# Claude: DUELL 1.1 UI cleanup and byte accounting

## Starting point and ownership

- Code baseline: **`f2ff36f`**, branch `jam11`. This contains Codex integration `6bb1e67`, Claude KEEP/storage `4f29aa8`, compaction `21cbbe5`, and the finished UI feedback stage.
- Active checkout: `work/wt/duell-jam11`.
- Use an isolated branch/copy from this commit. Do not change `jam11` directly. Do not reset the existing `tide-keep` worktree: it has unrelated test artifacts/changes. The original sibling `DUELL` checkout is the old JAM branch, not the current firmware.
- **Claude owns:** UI rendering/layout/font cleanup (`ui_draw.c`, `ui_graph.c`, `ui_layer.c`, `ui_boxes.c`, `ui_menu.c`, `ui_tokens.h`, `gfx.c`, `icons.c`, UI-only helpers and font generators).
- **Codex owns:** TIDE morph/planner/control code, FM6/audio/sequence/storage, integration and packaging. Do not edit `morph*.c`, `seq*.c`, `tide_ui.c`, `tide.h`, DSP/engine files, storage, global compiler/RAM placement policies. Propose any needed interface change first in the result file.
- Owner wants implementation to keep moving. Memory is diagnostic during development; release bounds stay enforced. No flash/install, no new approval round about save compatibility, no broad regression sweep.

## Task

Investigate whether DUELL 1.1 kept redundant old rendering paths alongside the replacement templates. Deliver one coherent cleanup with measured savings and the same visible behavior. The owner specifically wants code/layout/data consolidation, not arbitrary feature removals. More bytes do not establish better visual quality, but coexistence alone does not establish dead code.

Start with `ui_draw.c` template selection and `ui_graph.c:draw_graph`:

1. The old `graph_mod` fallback and newer `graph_mod_new` coexist. MATRIX pages in `params.c` use `SC_TRACK/GR_MOD`, and normal selection sets template 4, whose early return calls `graph_mod_new`. Check all virtual pages, quick layers, TEST mode and direct callers before removing the old path/function.
2. The ENGINE fallback calls `graph_engine`, while normal ENGINE selection chooses template 5 or 15 and has newer engine pictures/list handling. Prove which fallback is still reachable.
3. AREA list/STEP/ARP renderers retain old geometry branches and switch entries. Map valid **graph + template** pairs; all template numbers being live does not mean every combination is live.
4. Look for duplicated font/data/layout tables and shared painters that can be consolidated without appearance changes. Several card variants already share `card_paint`; do not assume all are independent implementations.

Remove only branches whose callers and selector invariants you can explain. Keep all controls, overlays, module-off behavior, engine pages, UI notices, hardware menus, sequencer ties and TIDE hooks. Do not trade DSP performance for flash.

## Already done; do not repeat

Evidence is under the sibling `work/audit` directory:

- `STOCK-1.0-VS-1.1-SPACE.md`: stock app growth 76,344 B; UI code +25,734, fonts +20,497, factory data +11,474, remainder +18,639.
- `DEEP-UI-COMPACTION-REVIEW.md`: all nine font faces Huffman-packed, packed per-phase metrics, lossless splash RLE, missing cold UI minsize entries and `morph_roll_from`. Pixel/metrics equivalence was checked.
- Broad noinline trial saved 1,296 B but was not adopted; narrow UI helper trial saved 48 B; sharing `cv_text_on` saved zero. Global merge-functions broke required RAM function identity and was rejected. Do not disable placement checks or repeat these trials without new evidence.
- Every current UI file is already in `size_fns.py`'s size policy, with ISR reachability exclusion. Adding the whole gfx/UI family again will not produce a new saving.

## Baseline state and pitfalls

- Core scoped checks passed: KEEP/storage 70, crossings 562, controls 22, EQ 13. UI-stage checks: KEEP/storage **72/72**, MIDI/PERFORM/UI **24/24**, full ASan/UBSan. Native captures inspected for SEQ TOOLS, STEP, splash/About.
- The current UI has visible G4 KEEP, STORE FULL, variation outlines, live selected-part destination footer, quantized theme motion.
- First fitting fallback reuses the L alphabet font for splash/About text instead of SVG bitmap lettering. Branding/credits/animation/startup controls remain; TIDE-off retains original artwork. Exact `f2ff36f` baseline: **581,332 / 581,564 B**, **232 B free**, all ELF/RAM checks passed. Measurement: `work/audit/jam11-measurements/tide-ui-baseline-f2ff36f/result.json`; SHA-256 `2e5fa2eb2762a51a491c9ad769e701886fa2197c1ff628bc550c451520777136`. More headroom is wanted; if renderer savings permit restoring the bitmap artwork, report that as a separate option.
- Font X/N/B are sparse numeric roles; do not use them for arbitrary alphabet text. L supports the alphabet.
- Drawing coordinates are mixed: `box_grid` uses area-local coordinates and does not own/blit a canvas; its caller `draw_layer` owns the buffer/blit. Many graph functions use screen coordinates with `cv_oy`. Do not start another canvas inside `layer_seq` or draw its hint with unadjusted screen coordinates.
- Existing broader reference failures: erase/menu 65; morph 2; web editor 16; librarian 2; old editor-round test throws in `MockUnit.deviceMenu`. These predate this integration. Do not weaken tests or turn this cleanup into fixing all of them. POSIX-only `twin_test` is unavailable in the native Windows harness.
- UI native captures: `felucca-base/build/tide-{seq-tools,step,splash,about,perform}.ppm`. The test's optional screenshot writer swaps the LCD RGB565 bytes before exporting.

## Proportionate validation and delivery

Use a before/after target measurement with the same flags and a unique label; record per-function/data savings and RAM changes. Render only the affected UI flows and compare their output or inspect captures. One coherent candidate and relevant checks, not repeated full suites.

From the `work` directory, the existing helpers are:

```
duell-win-venv/Scripts/python.exe audit/crossings_host_build.py <your-checkout-relative-to-work> candidate tide_controls_test.c
```

Set `DUELL_TEST_SCREENSHOT=build/tide-perform.ppm` for the capture flow. Target helper (existing Windows compiler needs elevated execution):

```
DUELL_MEASUREMENTS_DIR=<absolute separate artifact directory>
duell-win-venv/Scripts/python.exe audit/stock_compare_build.py 1.1 <your-checkout-relative-to-work> <unique-label>
```

Reuse prepared generated headers or generate them with the project's tools; `build/gen` must match the packed-font source. Keep artifacts out of tracked source. Preserve each file's existing line endings; check the diff for whole-file newline churn.

Write `CLAUDE-UI-COMPACTION-RESULT.md` with baseline/commit, changed files, reachability proof, measured flash/RAM delta, checks actually run, and remaining concerns; provide the commit/patch. Stop after that coherent pass so Codex can integrate it. Keep `MIGRATION_1_1_PLAN.md` as the canonical project status; avoid editing it concurrently with Codex.
