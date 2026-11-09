# Flash space search (S1), 9 November 2026

Read-only search for 4-8 KB of flash that can be freed with nothing the player hears or uses lost and no saved-data format change. Measured on the b5 head (`work/wt/audit-tide`, 4a79206, TIDE device build: image **577,008 B** of the 581,564 B slot, 4,556 B free). The probe builds ran in a throwaway copy without `.git` (`work/wt/probe-space-s1`, deleted afterwards); the copy reproduced 577,008 B exactly before any change.

## Result

**About 8.9 KB found** across five candidates that leave the player's sound and data unchanged, plus one owner-call option of 3.8 KB more.

| # | Candidate | Saves (flash) | What the player could notice | Risk | Effort | How measured |
|---|---|---:|---|---|---|---|
| 1 | Factory multi records: the per-part value blocks as a shared template + bitmap + changed bytes | **~3,050 B** (b6, 18 records; 2,850 B on b5's 16) | Nothing (records decode byte for byte) | Low | Medium | Modelled on the real records (build/gen/duell_multis.h) |
| 2 | Keycap pills (KC_DATA) Huffman-coded with the fonts' existing coder | **~2,500 B** (4,941 -> 2,431 B data) | Nothing (same pixels); UI drawing does a little more work | Low (+516 B .bss if given its own cache slot) | Low-medium | Encoded with tools/gen_aa_font.py huff_pack |
| 3 | Event-rate audio-path code (allow-listed "event rate" functions) built for size, as the UI already is | **1,780 B** (577,008 -> 575,228) | Nothing audible; note-on / MIDI event handling a little slower inside the audio interrupt | Medium: CPU, needs the unit sweep | Low | Probe build A |
| 4 | Factory pattern table (PATTERNS) trimmed to each pattern's length, 2 B a step | **~900 B** (2,304 -> ~1,400 B) | Nothing (the same steps) | Low | Low | Computed from engines.c (32 patterns, 542 steps used of 1,024) |
| 5 | Parameter descriptors: the `unit` string pointer as a 1-byte index | **~870 B** (223 descriptors x 4 B) | Nothing | Low | Low-medium | 9 param_desc_t tables, 4,460 B |
| | **Total, 1-5** | **~9,100 B** | | | | |
| O1 | USB audio resampler FIR (UAC_FIR) as int16 instead of int32 | 3,840 B | USB audio to the computer differs by about 1 LSB (Q30 -> Q15 taps); bit-exact recordings change | Owner call | Low | Table size, value range (max 1,071,286,587: does not fit int16 losslessly) |

Taking 1, 2, 4 and 5 (no CPU question) gives **~7.3 KB**. Adding 3 after a unit CPU sweep gives **~9.1 KB**.

## What is already done (not repeated)

- DUELL 1.1 / `up/space-savings`: the boot pack (LZMA, 21.6 KB back), Huffman fonts and icons (AF_*, AI12, AI16), packed factory sound pairs (DS_PACKED), dead render paths. All in the b5 tree.
- `fix/flash-savings` (merged): minsize for the TIDE main-loop files (-2.5 KB).
- Pages probe (8 Nov, `pages-space-2026-11-08`): SEQUENCER HELP, MODE row, big MORPH font, mv field. Taken, -4.8 KB. The leftover `SC_MODE` ROADMAP item measured 4 B there: nothing left.
- `flash-hot-2026-11-08` is about CPU (which code runs from RAM), not space.

## Where the flash goes (b5, llvm-nm of build/felucca.elf)

504.7 KB of named flash symbols; 133.9 KB of it is tables. The biggest tables and how compressible they are (zlib -9, as a guide only):

| Table | Bytes | zlib | State |
|---|---:|---:|---|
| AF_S/M/L/T/P/N_DATA (fonts) | 39,780 | 91-100 % | Already Huffman |
| DM_REC (factory multis) | 9,974 (b6: 11,065) | 49 % | **Raw: candidate 1** |
| DS_PACKED (factory sounds) | 9,894 | 70 % | Already packed (pairs) |
| UAC_FIR | 7,680 | 93 % | Already half (phase symmetry); option O1 |
| KC_DATA (keycaps) | 4,941 | 32 % | **Raw 4-bit: candidate 2** |
| FM6_EXP2 | 4,100 | 100 % | Left out: a smaller table changes the FM6 pitch / gain math (ROADMAP) |
| AI12 / AI16 (icons) | 5,647 | 94-96 % | Already Huffman |
| DS_SOUNDS | 2,736 | 64 % | Small gain, not worth a format |
| TP (param descriptors) | 2,540 | 28 % | Candidate 5 (with the other 8 desc tables) |
| PATTERNS | 2,304 | 22 % | **Candidate 4** |
| SINE | 2,050 | 99 % | Already a quarter wave |
| VAL_SHOWN, FM6_FACTORY | 1,380 / 1,024 | 46-47 % | Too small |

Nothing built only for test or bench is in the release: no `bench*`, `test_*`, `con_*` (console) or probe symbols. The editor protocol (45 symbols, 19.6 KB) serves the web editor and stays. The build is one translation unit (felucca.c), so LTO would gain nothing.

## Candidates in detail

### 1. Factory multi records (~3.05 KB)
Each record holds, per part, a 155-byte value block (P_COUNT values + the NX slots, each +64). Over the 36 parts of b6's 18 records, a part differs from the per-index most common value in 46 bytes on average (62 at most). Storing a 155-byte template once, then per part a 20-byte bitmap and the differing bytes, takes 5,580 -> 2,525 B. The header, End B blocks (2,114 B), steps, FM6 patches and automation stay as they are, so the upside is not counted.
- Decode: `df_rec(k)` rebuilds slot k's record into a RAM buffer before mu_unpack, the same staging the flash path uses (proj_wire / mu_buf, 3,840 B). No new RAM. Callers: multi.c mu_note1 (boot), multi_preview, project_load, project_rename (renaming an unchanged factory slot stores its record), the editor's layout dump (tests/editordump.c reads DM_REC: point it at the decoder).
- Proof: a host test that every decoded record equals tools/factory_multis/Mnn.bin (and tools/drone_multis/M17, M18) byte for byte. That is the whole risk, and it is checkable.
- Generic LZMA was measured too: 9,974 -> 6,200 B per record with the existing boot decoder, but its probability table needs ~8 KB RAM at run time, which is not there. Not proposed.

### 2. Keycaps (~2.5 KB)
KC_DATA is 29 pills, 13 px tall, 4 bits a pixel, raw. The fonts' canonical Huffman coder (gen_aa_font.py huff_pack, decoder gfx.c cv_alpha_hc, already in the image) packs it to 2,753 B as it is, and to **2,431 B** when the generator swaps value 5 (the pill fill, 66 % of the pixels) with 15 so the fill becomes a run. kc_ramp then swaps the same two ramp entries back.
- Code: cv_keycap calls cv_alpha_hc instead of cv_alpha (a few bytes); kc_t.off stays u16.
- RAM / CPU: the Huffman lookup cache has HC_FONTS = 8 slots and already 10 codes (9 font faces + icons); a miss rebuilds 256 entries on the main loop. Either raise HC_FONTS to 9 (+516 B .bss; b5 uses 68,968 of 73,728 B .data+.bss, check the RAM rules before taking it) or give the keycaps the code of the face they appear next to. Main loop only, no audio cost.
- Proof: a host render of every pill, before and after, pixel-identical (tests/ui_render.c already draws the hint pills).

### 3. Event-rate code for size (1,780 B, probe A)
tools/size_fns.py builds the main-loop files with minsize but keeps every function an interrupt can reach at -Os. 177 of those are allow-listed in tools/ram_allow.py as "event rate" (arp, sequencer steps, note-on, MIDI, locks): they already run from flash, once per event. Probe A marked them minsize: image 577,008 -> 575,228 B. Range Z, range X, the hot block and RAM are unchanged.
- The hot-placement check then flags 40 small helpers that were inlined before and now are calls (arp_hold, arp_mode, ...). Same event paths: they need allow-list lines with the same reason.
- Risk: the note-on / event handling inside the audio interrupt gets somewhat slower (minsize trades speed for size). The b5 sweep's worst block is ~90 %. **Needs the unit CPU sweep**, with a chord-heavy and arp-heavy scene (many events in one block). Keep out the 116 "mix or effect stage" entries: they are per block.

### 4. Factory patterns (~900 B)
PATTERNS (engines.c) is 32 x 72 B: name pointer, len, div, 32 notes, 32 flags, swing, gate. Only 542 of the 1,024 step slots are used (lengths 8 to 32, most 16). Note offsets span -7..+24, flags 3 bits. Stored as len steps of 2 bytes (or one byte offset+5 bits and flags 3 bits: about 1.4 KB saved, slightly more work), with an offset table. Two readers (patslot.c ps_default, the name lookup). Proof: a host test that every pattern decodes to the old table's steps.

### 5. Parameter descriptors (~870 B)
param_desc_t is 20 bytes: label, fmt, min, max, def, names, unit. 223 descriptors in 9 flash tables (4,460 B). `unit` is a string pointer used at 7 sites. A uint8_t index into a small unit-string table makes the struct 16 bytes. Proof: tests/editordump.c's layout.json unchanged byte for byte, and the ui render goldens unchanged.

### O1. UAC_FIR int16 (3,840 B, owner call)
The 44.1 -> 48 kHz resampler of the USB audio path holds 80 x 24 int32 taps (Q30), already halved by symmetry. int16 taps halve it again but change the USB output by about one LSB. Not "nothing the player hears" in the strict sense: recordings over USB are no longer bit-identical to b5. Ask the owner.

### Looked at, not proposed
- FM6_EXP2 (4,100 B): any smaller form changes FM6 math (ROADMAP: validate by ear).
- Per-block "mix or effect stage" code to minsize: CPU in every block.
- DS_SOUNDS, VAL_SHOWN, FM6_FACTORY: under 1 KB each to gain.
- Fonts / icons further: already Huffman, ~93-100 % under zlib.

## Notes for whoever builds these
- Each one changes the image layout a little and so the audio interrupt's inlining. Run the hot-code check and, for anything that lands, the unit CPU sweep as usual.
- None changes a saved format, a backup, or a byte any store writes.
- Order by value per risk: 1, 2, 4, 5 (no CPU question), then 3 with the sweep, O1 only if the owner agrees.
