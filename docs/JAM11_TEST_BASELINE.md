# jam11 Windows test baseline

Current maintenance priorities: [roadmap](ROADMAP.md). Totals below describe their named source checkpoints, not a continuously updated count.

## T0 progress: 2 November 2026, UI groups (branch `t0-ui`)

Repaired test-side, now PASS: `select`, `morphui`, `perflevelmut`, `motion`, `fuzz`, `xbmut`. `mod` and `level` compile and run with no failed check up to their first `fork()` (SKIP on Windows). Causes: TIDE's HOME round (MORPH SETTINGS in MORPH SET's place, then AUTO TIDE; ALGORITHM selects BOTH there), cards read from the drawn texts (`ui.col` is a hash since 1.1), item 160's third BALANCE rule, the 1.1 header capsule's A / B letter, PT_VER 4 / PROJ_REV 8 / MU_VER 8 layouts, End B kept as values as the B end sounds it, TIDE's MUSIC roll AMOUNT.

Still FAIL:
- `ui` compiles and runs (`8099bdf`) but its lint finds real screens, not stale names: ENGINE FM6's FEEDBACK label off its card (uitest item 158 says the same), TIDE's splash / ABOUT credit line over the waves, the retired SCALE layer's scenes, menu INFO / TEST bottom rows, AUTO TIDE's 'A', 1487 alignment and 36 box findings. Needs owner / firmware decisions.
- `editb`: one mutant needs `midi_control_test.c` to compile (`tests/scenes.h` mu_put_rec, the t0-scenes groups); the others match once morph_test's 2 known roll failures are not counted.
- Not started: `uitest`, `pages` (its card reads and the 1.0 card-width model of section (a)), `perform`, `step3`, `engine`, `scope2`, `velo`, `pan`, `dx7import`.

No firmware bug found in these groups.

## T0 progress: 2 November 2026, later

Repaired test-side (T0, roadmap): `backup`, `patch`, `patchraw`, `bank` now PASS. They rewrite the 1.0 record checks for PT_VER 4 (End B as an MBV block; values lossless only inside their range; SWING 50..75), TIDE's 126-patch store, 1.1's PATCHES knobs (K3 SAVE, K4 FAV, no per-slot RESTORE) and S0a's one pattern object (a backup LIST writes a waiting pattern save). Shared helper: `felucca-base/tests/endb_store.h`. `bank` no longer reads `tests/factory_bank_records.txt` (PT_VER 3, refused by 1.1): it saves and reloads every factory sound and compares the sound.

Real bug found and fixed: RENAME / recategorise and the editor's FM6 PUT dropped a saved patch's End B (`dd2acd5`).

`morph` moved from SKIP to FAIL when the runner began listing every failed check (`e10bbc0`): its two roll checks already fail at `8506f2c`.

## Broad-run snapshot: 2 November 2026, `bd339c9`

`sh jam/check.sh`, 600 s per group (full run at `75a987a`, the groups changed after it re-run at `bd339c9`). **65 passed, 57 failed (fm6 a timeout), 12 skipped.** Before this round (`8506f2c`): 58 / 65 / 11.

Fixed in this round: `patslot`, `multi`, `project`, `persist`, `sidechain` (stale tests rewritten for 1.1's formats: MU_VER 8, PROJ_REV 8, K3 SAVE, no per-slot ERASE), `erase` (a real TIDE bug: the TIDE record was rewritten at every power-on), `midimapmut` (its mutation also hit `midi_tide`); `insert` compiles again and skips at its `fork()`.

Why the rest fail, by cause:

| Cause | Groups |
|---|---|
| Fed by `tests/scenes/*.bin`, MU_VER 7 records the firmware now refuses (needs a v7 to v8 converter, none was committed upstream) | mrender loadcost cpuscen modules midictl panic |
| Stale 1.0 / pre-1.1 names or formats (removed identifiers, the edit list, old record layouts): the same rewrite as multi / project | mod ui select motion patchraw backup level fuzz xbmut velo patch bank editb pages scope2 engine morphui uitest editor step3 perform perflevelmut pan dx7import |
| Bit-for-bit hashes of 0.2.x sound ("the sources before X"): re-baselining means accepting 1.1's sound as the reference, an owner decision | reverb halltail locut shimmer stereochorus comp master bassmono eqon dlyev mxstep modinert |
| `sys/mman.h` / `sys/wait.h` missing on Windows (a shim in `jam/wincompat` would help; most would then SKIP at fork) | velaccent uf twocore tcmeas xvalues twin subcyc ownerimport |
| Web / tools | editorjson (16 inherited web-editor failures, none from TIDE since `bd339c9`), emu, converttwin, benchsim, benchrun, jam_dev (1.0 fixtures in its mock) |
| Too slow here | fm6 (over 600 s) |

TIDE-specific notes for the tests: `TIDE_FIRST_MULTI` (a new unit starts on M04), an ARP SEQ part's stand-in plays on PLAY with no key, TIDE's patch store has 21 chunks (126 saved patches, accepted, migration plan §4.5): `patch` does not know that yet.

PASS: chord strum performcap dlymod hang atgt patslot multi release menuproduct dx7compat splash physintune unisonlevel clockguard clockdrift uartdma uarttimer fm6liveol tidekeep tidecrossings tidecontrols tidedialogue tidevoicelead slopehandoff hold reset resetwalk tcstage1 bootguard tcbench poolrule polylimits deadline velomono sat eq32 ovlguard matrixpack engswitch guide guidesetup wtab wavetable project fun10 persist audio erase flashled usb elfcheck plate chorusskip sidechain tune bench benchaudio rollbrowser sliders xb midimap midimapmut ldrscreen lcdboot

FAIL: velaccent mod perform perflevelmut ui select step3 engine morphui dlyev patch patchraw bank dx7import mrender loadcost cpuscen uf pan twocore tcmeas mxstep modinert eqon level modules uitest motion backup editor editorjson midictl reverb halltail locut shimmer panic bassmono comp master stereochorus benchsim benchrun ownerimport xbmut fuzz xvalues twin subcyc pages scope2 editb velo emu converttwin jam_dev; timeout fm6

SKIP: regress perflevel morph mode sounds fmbass wave mixsplit partindep duckpart budget insert

## Later focused evidence — 2 November 2026

- S0 merge `254a501` reports 65 passed / 58 failed / 11 skipped. Its commit records an FM6 timeout and two existing morph-roll failures exposed by the improved runner. Keep that run separate from `bd339c9`'s totals.
- `backup` passed at `e176f6e` against 1.1 + TIDE + S0a (`work/check-duell-jam11/backup.log`, reviewed during installation). The older FAIL list above is the broad-run snapshot, not the status of this repaired group.
- Five revision-8 backup-helper tests and the installed package's pinned CPU-emulator smoke passed during the S0/DSP installation. Retained logs: `work/backups/s0-e176f6e-2026-11-02/`.
- `patch` repair is in progress in the shared checkout. It exposed the End-B preservation bug fixed in `dd2acd5`; do not count the whole group as PASS until its coherent run is recorded. No new aggregate has been inferred from these individual checks.

## Step 0 (history)

Step 0, commit `aa55e22` (1.1 `974d875` + Windows build, fonts 8-slot, hot room, `jam/` tools), 2026-11-01, `sh jam/check.sh` with a 600 s limit per group, Git Bash + llvm-mingw. Every later step is compared with this list, not with "all green".

Summary: 47 passed, 63 failed, 11 skipped.

Why so many fail: most of upstream 1.1's own host tests don't compile against 1.1's firmware (they use names 1.1 removed, e.g. `CF_ERASE_PSLOT`, `ui.plk_src`, old argument counts) or check 1.0 behaviour. That is upstream's state at `974d875`, not a Windows problem. `jam_dev` fails on 1.0-format fixtures (End B block, the old music object) in its mock; the tool itself backed up and flashed the real 1.1 unit today. SKIP = needs `fork()`, which Windows lacks. `fm6` exceeds 600 s here.

### PASS

chord strum performcap dlymod hang atgt release menuproduct dx7compat splash hold reset resetwalk tcstage1 
bootguard tcbench poolrule polylimits deadline velomono sat eq32 ovlguard matrixpack engswitch guide 
guidesetup wtab wavetable fun10 audio erase flashled usb elfcheck plate chorusskip tune bench benchaudio 
rollbrowser sliders xb midimap midimapmut ldrscreen lcdboot 

### FAIL

velaccent mod perform perflevelmut ui select step3 engine morphui dlyev patslot multi patch patchraw bank 
dx7import mrender loadcost cpuscen uf pan twocore tcmeas mxstep modinert eqon level modules uitest project 
motion persist backup editor editorjson midictl reverb halltail locut shimmer panic insert bassmono comp 
master stereochorus sidechain benchsim benchrun ownerimport xbmut fuzz xvalues twin subcyc pages scope2 editb 
velo emu converttwin jam_dev 

### TIMEOUT

fm6 

### SKIP

regress perflevel morph mode sounds fmbass wave mixsplit partindep duckpart budget 

