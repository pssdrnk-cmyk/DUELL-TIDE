# Flash space tasks (from S1, 9 November 2026)

Measured on the b5 head (577,008 B of 581,564 B). Details and numbers: [REPORT.md](REPORT.md). Each task is TIDE-only, changes no saved format, no backup and nothing the player hears or sees. Each ends with a device build (image, RAM, pool tail before and after), the hot-code check clean, and targeted host tests. Rule for every agent: 2 errors equals agent death.

## SP1: factory multi records as template + diff (~3.05 KB on b6)

- **Generator** (tools/gen_duell_multis.py): split each record into its parts (tools/factory_multis.py parts_of). Emit one 155-byte template of the per-part value block (P_COUNT values + NX, the per-index most common byte over all parts), and per part a 20-byte bitmap (bit i: byte i differs) plus the differing bytes. Leave everything else in the record as it is.
- **Firmware** (multi.c): `df_rec(k, buf)` rebuilds slot k's record into the staging buffer and returns its length. mu_note1, multi_preview, project_load and project_rename read through it instead of `DM_REC + DM_AT[..]`. No new RAM: use the buffer the flash path already decodes from (mu_buf / proj_wire). Check that no caller holds a pointer into mu_buf across the call.
- **Tests:** every decoded record equals tools/factory_multis/Mnn.bin and tools/drone_multis/M17, M18 byte for byte (multi_test). tests/editordump.c gets its names through the decoder, and layout.json is unchanged. multi, release, editorjson, mrender and budget pass.
- **Accept:** about 3 KB less flash, the same records, no RAM change.

## SP2: keycaps Huffman-coded (~2.5 KB)

- **Generator** (tools/gen_aa_keycaps.py): swap pixel values 5 (fill) and 15 before packing. Pack each pill with gen_aa_font.huff_pack and emit KC_HC. kc_t.off stays u16.
- **Firmware** (gfx.c): cv_keycap draws with cv_alpha_hc. kc_ramp swaps ramp entries 5 and 15 so the colours are unchanged.
- **Cache:** HC_FONTS (8) already serves 10 codes. Either raise it to 9 (+516 B .bss: check the RAM rules) or reuse a font's code if it packs nearly as well. Measure which costs less.
- **Tests:** a host render of all 29 pills in every state, pixel-identical to b5 (tests/ui_render.c). The ui and uitest groups pass.
- **Accept:** about 2.5 KB less flash, identical pixels.

## SP3: factory pattern table trimmed (~0.9 KB)

- engines.c PATTERNS: store each pattern's steps up to its length, 2 bytes a step (note offset and flags), behind a small offset table. The name, len, div, swing and gate are kept.
- **Readers:** patslot.c (ps_default / pat_make_step) and any name lookup.
- **Tests:** every pattern decodes to the old table's notes and flags (a host test that keeps the old table as a reference). The patslot and select groups pass.

## SP4: param_desc_t unit as an index (~0.87 KB)

- core.h param_desc_t: replace `const char *unit` with `uint8_t unit` (0 = none) into a small static string table; the struct goes from 20 to 16 bytes. Update the 7 readers and every PD / PE initialiser macro.
- **Tests:** web/editor/layout.json and the ui goldens unchanged. editorjson, ui, uitest, editor and menuproduct pass.

## SP5: event-rate code built for size (1.78 KB), with the unit CPU sweep

- tools/size_fns.py: also mark minsize the functions tools/ram_allow.py lists as "event rate". Never the "mix or effect stage" entries: those run every block.
- Add allow-list lines (same reason, event rate) for the about 40 helpers that minsize stops inlining. The build names them.
- **Unit sweep before it lands:** chord-heavy and arp-heavy scenes plus the factory multi sweep. 0 missed blocks and 0 cut voices; worst block not above b5's.
- **Accept:** about 1.8 KB less flash, the sweep clean.

## Owner question (not a task yet)

- O1: USB audio resampler taps as int16 (-3.84 KB). USB recordings change by about 1 LSB; not bit-identical to b5. Only on the owner's yes.
