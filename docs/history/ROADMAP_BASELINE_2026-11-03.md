# Roadmap baseline and evidence, as of 3–4 November 2026

*Historical: the roadmap status before the 4 November beta and its release candidate. Superseded by the "Status" section of [the roadmap](../ROADMAP.md). Links are as they were.*

Updated 4 November 2026 (status of branch `integ`, HEAD `3803fbc`, added to the 3 November evening reconciliation of the flashed builds) after the full firmware source audit at `6af479d` and the owner's storage clarification. **Audit complete; the verified bounded corrections F01–F04 are done and flashed (branch `polish-all`, `187e9d0`); the test-tree cleanup is done.** The storage goal is a small convenience: 4–8 clear USER destinations for new voices and patterns, with factory material still accessible. Existing overwrite saving remains acceptable. Exact restart continuation of live morphed sounds is not required; current audio polish is accepted. The first memory-saving milestone is complete in source. Further memory work should serve a measured feature budget, and broad cleanup/storage work is not an indefinite gate to musical work. The DUELL 1.1 migration is complete.

**Parallel work:** the test-tree cleanup (T0) is finished: full `sh jam/check.sh` is **112 pass / 0 fail / 25 skip** (the 25 skips need `fork()` or an instruction counter and pass in the Linux host-test CI; the boot pack added the `bootpack` group). **Not flashed, on branch `integ` since `187e9d0`:** U1 USER slots, U2 B factory sequences (M16 is CELLAR TAPE), U3 MULTIS / PATTERNS preview, 32 more factory sounds (#129–#160), ten new factory patterns, JRNY role families and a four-target memory, the TIDE SETTINGS rows STYLE, TURNS (with LEAD / ECHO), POLY, SYNC and ARC, live ENV / LFO graphs, and the CHORD and GLO/MIXER part-select fixes. The Windows runner now has a real `fork()` (`jam/wincompat`), so its skip count is lower than the 25 above; no new full run is recorded here. Each firmware change still needs its affected checks.

This is the current delivery/status document. Detailed [musical proposals](../design/TIDE_MUSICAL_DESIGN.md) and the [historical roadmap and receipts](TIDE_ROADMAP_2026-11-02.md) are references, not competing work queues. Keep the [test baseline](../JAM11_TEST_BASELINE.md) as the record of test results.

The [maintenance action plan](ACTION_PLAN.md) records the first completed batch from pinned source `7c22b13`: lossless factory-voice packing and one shared knob-navigation helper are now committed as `61eb641` and `89a9a77`; buffer and free-slot investigations are reviewed. The combined change saved 1,936 B flash in its own build; both commits are part of every package flashed since. These historical build measurements do not describe a newer package.

## Baseline and evidence

| Item | Verified state |
|---|---|
| Development branch | `integ` (the integration branch: `jam11` plus the work since `187e9d0`; firmware work runs in other branches) |
| Installed source | `187e9d0` (branch `polish-all` = jam11 `2d3e619` with the boot pack `ecd2918`, F01–F04 and the lx-sound/lx-parts test work); flashed 3 November 2026 19:42. Before it: boot pack `ecd2918`, flashed 18:31; before that `e176f6e` (2 November) |
| Installed content | R0, M1, M2a, S0a/S0b, DSP arithmetic fixes, TIDE startup-write fix, boot pack (LZMA RAM images), F01–F04 audit fixes, the shortened GLO > MIDI LAYER note |
| Package SHA-256 | `7795e77798d053177a4d0b099f0754701e16e308c92fbe22fd54913abac41b30` (`duell-1.1.fwsc`, a workspace file outside this repository); `e176f6e` was `ba8fdaf9252a0cf64f5230472ce10e738801b22926b2619e46cb81216648a8aa` |
| Device/data evidence | FM-1_901 / DUELL 1.1. `e176f6e`: reboot verified; all 16 archived objects equal after restoring current music. Boot pack: ABOUT shows RAM CODE OK 57 KB. Pre-flash backups a workspace file outside this repository and `-1941.json`. Separate TIDE preferences are outside that archive |
| Owner playback report | "sounds OK I guess"; basic playback observation, no controlled A/B comparison |
| Newer committed source | `dd2acd5`: patch rename/recategorization and editor FM6 PUT preserve saved End B; subsequent test repairs at `379e844` and `52c6604`; not part of the installed package |
| Maintenance source and retained candidate | P1/P3 committed as `61eb641` / `89a9a77`. Retained package was built from `7c22b13` plus that reviewed patch: SHA-256 `4852f39142d3f9b3de6acd961aee6df96208f5f780d6685632edadc7d0731612`; source manifest and artifacts at a workspace file outside this repository. Not installed. |
| Review source | `6af479d`, branch `jam11`; clean at the start of this documentation review. Later commits do not establish a newer installation. |
| Concurrent work | Test-tree and firmware work run in parallel branches. Recheck status and file ownership before a firmware change. |

Installation evidence for `e176f6e` is retained at a workspace file outside this repository, with the exact package, pre-flash archives, initial readback and final comparison; the 3 November packages are in a workspace file outside this repository and `DUELL-2026-11-03-polish-187e9d0/` (with `.INSTALL.md`). Preserve these files. Do not infer an installation from a newer branch HEAD or a mutable build output.

### Installed memory budget

These are linked-build figures for the `e176f6e` package, not runtime CPU/stack measurements, and are not re-measured for the later packages except where the flash line says so. The boot pack (`ecd2918`) moved the RAM load images into one LZMA stream: the application image fell 578,468 → 556,620 B (decoder included), **24,944 B free** in the slot; the `187e9d0` package's `felucca.bin` is 556,620 B. Its RAM, pool, hot-code and range-X figures are not recorded here; a newer candidate needs its own build and delta.

**Memory names.** All DUELL docs use these five names, and only these five (see `felucca-base/firmware/app.ld` and `app_ram.ld`):

| Name | What it holds | Linker |
|---|---|---|
| **Flash** | The firmware's code and constants; the 581,564 B slot | `.text`, `.rodata` |
| **RAM** | Ordinary variables | `.data`, `.bss` |
| **Pool** | Large buffers: delay line, screen canvas, performance capture, engine memory. Includes range Z, part 2's voice-path code placed at the pool's end | `.pool`, `.ramz` |
| **Hot code** | Audio functions copied to fast RAM; room is counted up to `boot_info` | `.ram_text` (`.hotx.*`) |
| **Range X** | Fast RAM holding part 2's engine render code | `.ramx` |

"Hot-code move" (checkpoint folders `…-hotroom`) is the change that moved `eq_run`/`ins_swept` from hot code to range X.

| Region | Used | Physical space left | Required reserve | Space after reserve |
|---|---:|---:|---:|---:|
| Flash | 579,728 B | 1,836 B | Fits 581,564 B slot | 1,836 B |
| RAM | 67,936 B | 5,792 B | 4,096 B | 1,696 B |
| Pool | 350,796 B | 17,844 B | 8,192 B | 9,652 B |
| Hot code | 11,048 B | 1,464 B to `boot_info` | 228 B | 1,236 B |
| Range X | 31,584 B | 928 B | Closure/placement checks | 928 B |

S0a/S0b recovered 6,688 B of pool against the previous installed build. **Flash remains the constraint.** Do not count RAM savings as flash savings. Commit `dd2acd5` reports a 579,856 B candidate (+128 B); that report is separate from the retained installed-build measurement above.

The retained maintenance candidate leaves **3,644 B of flash**. Small additions are plausible individually; U1 and the musical backlog have no combined measured budget. RAM headroom and hot-code placement are separate constraints, and none of these figures establishes real-device CPU capacity. Preserve the existing reserves and measure each coherent candidate before integrating another feature.

