# RAMPLACE: part 2's voice code in unused RAM (two-core stage 1a)

> Historical, superseded by [TWOCORE_STAGE1.md](../TWOCORE_STAGE1.md). Notes on the RAM-placement stage of the two-core work (developer builds); the second core now ships in every build.

Branch `stage1a` (local, from `cloud/base` 85dee78). Design: a workspace file outside this repository 3.2 item 3, 3.3, 5.2 stage 1; sizes from `docs/RAM_SIZING_STUDY.md`. Developer builds only; everything still runs on core 0, core 1 is not started.

## Build

`DUELL_RAMPLACE=1 DUELL_BENCH=1 DUELL_TEST_MODE=1 BENCH_TAG=RAM1 tools/build.py` (needs DUELL_HOTRAM 3, the default). Without `DUELL_RAMPLACE` nothing of it is built: the normal package is byte for byte cloud/base's (`duell-0.2.25-beta.fwsc` 54507df4...).

Link-time only, as `tools/hot_fns.py`: `tools/ram_place.py` renames the listed functions' sections in `felucca_hot.o` (`.text.f` -> `.ramx.f` / `.ramz.f`), `firmware/app_ram.ld` links them; the C is not changed. `-DDUELL_RAMPLACE=1` adds `src/ramplace.c` (copy and check), two `#if` blocks in `main.c` (`fm1_cstart`), the PC window in `hal/fm1_guard.h` and ABOUT's RAM CODE line (`ui_menu.c`). Its variables are in `.bss.ramplace` so LLVM's GlobalMerge merges the other globals as before: `tools/ram_place.py --compare REF.o NEW.o` shows all 87 moved functions (and the 31 hot ones) with the same object code and relocations as the same build without RAMPLACE.

## The ranges (RAM1 build)

| Range | Where | Size | What | Rule |
|---|---|---|---|---|
| X | 0x01C00000..0x01C077A8 (of ..0x01C07F00) | 30,632 B of 32,512 | the render side of PHYS, ANALOG, TRIO, PHASE, LOFI, VOICE, NOISE (`tools/ram_fns_x.py`, 39 functions) | entered through engine_t pointers only; calls only RAM |
| Z | 0x01C6CE6C..0x01C6FEF4 (after .pool, below the stack guard 0x01C74000) | 12,424 B, 16,652 B left | FM6 / WAVE (what is not hot), WHEEL, part_voice and the LFOs, mod matrix, morph_begin, memset, memcpy, division helpers, part_rand, PHYS's phys_slot_of / phys_reset (`tools/ram_fns_z.py`, 48 functions) | reaches and is reached by everything |
| hot code | unchanged | 12,284 B, 228 B free | | |

Pool: 351,988 B of 368,640 (the 8 KiB pool headroom rule counts Z). Flash 461,612 B.

## Start-up and the window

`fm1_cstart`: right after the boot guard (a start that dies in the copy counts as a failed one) `ramplace_copy` copies X and Z from their load images; before it only `fm1_p33_read`, `fm1__p33_op`, `fm1_enter_uboot` run (flash only). After the other copies `ramplace_boot` compares both ranges word for word with the flash; a difference: copy and compare again, twice; still wrong: update mode (there is no flash copy to run instead: every caller jumps to the RAM address). ABOUT re-checks once a second. PC limit window 1: `_pc_ram_lo` (range X's start) to the end of the hot code (`_rt_end`); window 0 stays the flash. The window now includes the data and stacks between range X and the hot code (there are only two windows).

## Checks (build.py; tests/elfcheck_test.py on fake layouts)

- Object (`ram_place.check_object`): every listed function has its own section; no function in two lists or in the hot set; nothing in X calls or takes the address of code outside RAM; no flash code calls into X.
- After the link (`elfcheck.check_ramplace`): X inside 0x01C00000..0x01C07F00, Z between the pool's end and the stack guard, no overlap with data / pool / .noinit / hot code; the sections hold exactly the lists; no interrupt / USB / MIDI / update code; `_rx_*` / `_rz_*` bound the sections and the load images in felucca.bin equal the sections; `fm1_cstart` copies before anything that can reach X or Z code (every call before the copy followed through the disassembly); the PC limit is loaded from `_pc_ram_lo` and the window covers range X, range Z and the hot code; no direct call from range X leaves RAM (also the unannotated `call N` inside `if { }` bodies: target = address + 4 + N), none through a pointer, none from the flash into X; hot code headroom at least 228 B.
- `ram_place.reach_check`: every call relocation of the object within +-4 MiB at the linked addresses (largest RAM-flash distance 0x3D205A).

## Not done here

Core 1, its stack, the job runner, bench rows (the bench of `wip/bench0226` is another repository and was not ported), range Y, part effects and bus B (stage 2). Tables (SINE, PITCH_OCT ...) stay in the flash.

## Since boot pack `ecd2918` (3 November 2026)

The load images of X, Z, `.ram_text` and `.data` are no longer stored raw after `.text`: `tools/build.py` writes them as one LZMA stream with a header (addresses, lengths, word hashes) at `_boot_pack`. `ramplace.c` (`bp_unpack`) decodes it into the pool before anything can run X or Z code, checks every section hash and copies it; `ramplace_check` (boot and ABOUT) compares X and Z with their hashes instead of with flash images. `elfcheck` decodes the pack and compares it with the linked sections. The text above describes the stage 1a design and sizes; where it says "load images in felucca.bin" or "compares with the flash", read the pack. Image 578,468 -> 556,620 B (24,944 B free in the slot). Host group: `bootpack`.
