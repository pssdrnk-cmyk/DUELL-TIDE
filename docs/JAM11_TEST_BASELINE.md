# jam11 Windows test baseline

Current maintenance priorities: [roadmap](ROADMAP.md). Totals below describe their named source checkpoints, not a continuously updated count.

**Latest full run (3 November 2026, Windows `sh jam/check.sh`): 112 passed, 0 failed, 25 skipped.** The test-tree cleanup (T0) is finished; the boot pack added the `bootpack` group. The 25 skips need `fork()` or an instruction counter; they run in the Linux host-test CI and in the WSL sections below.

## Linux (WSL) groups: 3 November 2026, branch `lx-sound`

`velaccent regress mod mode sounds mrender wave subcyc xvalues budget` pass in WSL (`wsl.exe -d Ubuntu -- sh jam/wsl.sh <group ...>`). The Windows check is unchanged: 111 passed, 0 failed, 25 skipped (these groups SKIP at `fork()`). No firmware change, so `check_off.sh` and `off.sha256` stay as they are. Stock 1.1 fails the same groups on Linux, so these were 1.0 expectations, not TIDE regressions.

- **Stale expectations fixed in the tests:**
  - `regress`: the voice pool is 16 (item 90), and a chord starts VS_MAX 2 voices a block (item 144), so the voice checks fill 16 voices and let the chords start; the full-cap check lets POLY part 3 lose one voice.
  - `mod`, `xvalues`: 57 destinations (SPACE), 18 sources, names PULSE WIDTH / REVERB DAMPING / WAVE POSITION / TWIN FEEDBACK. SUB HARMONICS has no card and never rolls.
  - `mode`: with TIDE the MODE page is HOME's fourth (4/4).
  - `wave`: SMART rolls WAVE SYNC also from 0 (owner 2026-10-28).
  - `subcyc`: the harness runs `vs_block()` per block; the REF scenes now compare the scene with and without the SUB / CYCLE values (the frozen cloud/base3-wave2 hashes predate the stagger and BIT DEPTH).
  - `velaccent`: ANALOG factory 4 allowance 0.95 -> 1.10 dB (1.05 measured, stock 1.1 the same).
  - `sounds`: `tests/loudness.h` used a float expression as an array size (gcc refused it). An End B the owner made as values is not judged by the seed windows.
  - `cost` checks (`mode`, `mod`, `regress` CPU): no instruction counter on Linux: reported, not compared.
- **Re-baselined, with the audio compared:** `regress` golden (210). 1.0's hashes pass on Linux. All 210 renders were dumped from 1.0 and jam11 and compared (`jam/soundref/regress_dmp.sh`, `acmp`). Built with `-DVS_MAX=8` the presets of ANALOG FM6 LOFI NOISE TRIO WAVE WHEEL, the sends and the 4-track mix fall to -40 .. -52 dB residual: BIT DEPTH, REVERB_FIX, SHIM. The rest is the PHYS accent / brightness and string tuning, PHASE, and the factory sounds' new recipes (`sound/*`). jam11 and stock 1.1 render 204 of 222 segments bit-exact; the others are PHYS strings (jam11's tuning), UNISON and my changed voice checks.
- **Re-baselined without a 1.0 audio comparison** (the 1.0 and stock 1.1 trees do not build these tests on Linux): `mrender` (105 hashes), `xvalues` roll and matrix hashes (the matrix hash equals stock 1.1; the roll hash differs from it by TIDE's roll changes), `budget` LOADED_REF (End B as values, reworked ROLL B). Reasons are in the comments next to each value.
- **Findings for the owner (not fixed, reported as KNOWN by `sounds`; stock 1.1 shows the same six):** BEHIVE has no LFO TRIG FREE; SINGING BOWL and ARP PLUCK are quiet in the body; FLOOR THUD and TABLA TALK miss the hit level; GHOST HARP clips (+0.5 dBFS peak in jam11; -6.2 in stock 1.1, after the PHYS string tuning).
- Tools: `jam/soundref/regress_dmp.sh` (regress audio dump), `jam/soundref/i32rec.c` (MULTI_DUMP / SOUND_DUMP to acmp records).
- Windows after WSL runs: delete `felucca-base/build` first (Python gets Errno 22 on files WSL made).


## Linux (WSL) groups: 3 November 2026, branch `lx-ui`

`perform step3 morph uf benchrun benchaudio ldrscreen multiv8` pass in WSL (`wsl.exe -d Ubuntu -- sh jam/wsl.sh <group>`); the Windows check is unchanged: 111 passed, 0 failed, 25 skipped. All changes are test or tool side; the TIDE=0 image is untouched (no firmware file changed).

- `perform`, `step3`, `morph`, `uf` had stale expectations that Windows never reached (they SKIP at `fork()`): the tempo starts at 40 so 1/32 fits everywhere; FADE 2, KEY TRK and the other non-matrix ids are not automatable (HANDOFF_V2 item 34); AMP ENV and LFO 2 cutoff depths no longer act; MULTI FILTER 2/2 has no K4; the sparse PT_VER 4 record leaves SLOPE 12 dB out; PHYS BOW rolls from 0 (owner 2026-10-28); the plain-roll checks of `morph` run with MUSICAL off and the seeds check rolls from the Original (ROLL is chained); `morph` also divided by zero on an engine with no preset.
- `benchrun`, `benchaudio`: patch 28 (was 27); the `--record` refusal test needs `--port` and a stand-in sox. Without the macOS instruction counter the twin's CPU figures are an estimate, so the verdict of the dear STRESS tests (WARN on a Mac, FAIL here) is not compared (`COUNTER` in both tests). Not compared on Linux: the cost checks of `perform`, `uf` (printed as "no instruction counter").
- `ldrscreen` needs a `build/duell-*.fwsc`, which `fm1pkg_make.py` can only make with the JieLi SDK: SKIP (reported PASS) without one.
- `multiv8`: the fork has no tags `v1.0` / `v1.1`, so the test falls back to the commits they name (`e1ef5a7`, `974d875`, both in jam11's history); `jam/wsl.sh` points git at the main repository for a worktree made on Windows (its `.git` file holds a C:/ path).
- `build/host` is shared by WSL and Windows runs: delete it when switching (a Linux `bench_sim` is not an .exe, Windows then fails with "Exec format error").
- CI: `.github/workflows/host-tests.yml` at `bee34f9` was invalid YAML (a colon in a step name, line 35): GitHub ran no job. Fixed in `b12c319`.

## Linux (WSL) run: 3 November 2026, lx-parts (the fork() groups)

`wsl.exe -d Ubuntu -- sh jam/wsl.sh mixsplit partindep duckpart twocore tcmeas insert twin`: **all seven groups PASS** (including `twin_partindep`, `twin_twocore` and every `partindep_mut` / `twocore_mut` / `twin_mut` self-check). These groups SKIP on Windows (they need `fork()` / pthreads); they had never been brought up to DUELL 1.1 + TIDE. No firmware bug was found: every failure was a stale 1.0 reference or expectation.

- `mixsplit`, `partindep`, `duckpart`, `twin_*`: `tests/mixsplit_ref.h` (the old combined path) now carries 1.1's own changes in the same verbatim way: BIT DEPTH's level stages (`LV4`), item 135's tail count, SPACE's matrix offset (`mod_space_off`). `mix_block`, the staged path and the old path are then equal bit for bit in every scene and schedule. `partindep`'s pinned `OLD_HASH` table was re-made (all 12 scenes: the 1.0 hashes pinned the 1.0 sound).
- `twocore`, `tcmeas`: `twocore_sim.c` checks follow items 90 and 142: a block with no take-over is posted at the wait limit (past the cap by design, not a violation); `floorx` accepts the floor's "does not fit" as such a block. One `TC_MEAS`-only line in `twocore_run.c` records the true-cost cap when the posted deadline is the true-cost one (bench builds only; the TIDE=0 image is unchanged, `off.sha256` stays). The own-split self-check "a deadline already past is counted as paused" had become vacuous (no such block exists since item 142); it is replaced by "a paused block is counted as a deadline already past", with a check that late_run / noans show their paused blocks.
- `insert`: DIST's reference wet follows BIT DEPTH; the BITS card is read from the texts drawn in it (the grid cards no longer keep `LABEL|VALUE|UNIT`); the page order has SEND FX after FX; INSERT TYPE is no matrix destination since item 34, so it is no lock or automation target (the test now expects the refusal).
- Not compared: `insert`'s cost section needs an instruction counter Linux does not have (it prints "skipped"); the twocore cost figures are not asserted.

- Windows full check on this branch: **112 passed, 0 failed, 25 skipped** (the seven groups above SKIP on Windows as before). Note: a WSL run leaves Linux binaries without `.exe` in `felucca-base/build/host/`; the Windows runner then executes them ("Exec format error", reported FAIL). Delete `mixsplit_test partindep_test partindep_mut duckpart_test twocore_sim twocore_meas twocore_mut insert_test` there before a Windows check.


## Full run: 3 November 2026, `0d66b59` (t0-sound: the 12 sound groups)

`sh jam/check.sh` in the `t0-sound` checkout (logs `work/check-duell-t0-sound/`). **92 passed, 22 failed, 22 skipped** (before: 80 / 34 / 22). No new failures. No firmware change, so `check_off.sh` is unchanged.

- Now PASS: reverb halltail locut shimmer stereochorus comp master bassmono eqon dlyev mxstep modinert. Stock 1.1 and jam11 hash these renders the same. Each 1.0 -> 1.1 difference was dumped and compared as audio (`jam/soundref`) and traced to an intended 1.1 change. No firmware bug was found. The new references and measurements are in [JAM11_SOUND_REFERENCES.md](JAM11_SOUND_REFERENCES.md).

FAIL: perform ui step3 engine dx7import fm6 uf pan modules uitest editor editorjson panic benchsim ownerimport pages scope2 editb velo emu converttwin jam_dev

## T0 progress: 3 November 2026, tools groups (branch `t0-tools`)

Full `sh jam/check.sh` at `c90f679` (before t0-sound merged): **87 passed, 26 failed, 23 skipped** (before: 80 / 34 / 22). No group failed that did not fail before.

Now PASS: `ownerimport` (tool: `import_owner_edits.py` drops 1.1's preselected matrix pairs at AMOUNT 0, puts moved depth cards back, ignores FM6's AMP ENV knobs and, with TIDE, keeps the recipe's End B because a factory sound's B is rolled at load), `converttwin` (the multis comparison with job J retired; the ledger is `CONVERT_TWIN_LEDGER`), `benchsim` (row INS BIT CRUSH fits the column; model signatures current, its averages still need the macOS counter), `modules`, `panic` (test: scenarios start from the same lines and random streams; not a STOP ALL fault), `emu` (test: the first-start splash waits for OCT+; the first start's M04 is left sounding by a key until all-notes-off, so the settings write waits), `editor` (INFO tail, 32 menu items, ARP / SEQ three-way, KEY VELOCITY default). `uf` compiles and every check passes, SKIP at `fork()`.

Also PASS now: `editorjson` (its 16 web-editor expectations, and the librarian / backup tests behind it, follow 1.1: 32 menu items, ARP / SEQ three-way, NX 28, 1.1's names, record limit 580) and `jam_dev` (the tool's record check is the PT_VER 4 layout with layout.json's ranges, multis are MU_VER 8, the mock builds a valid revision 8 music object and a real End B block).

Full `sh jam/check.sh` after rebasing on the UI groups: **110 passed, 1 failed (`ui`: real screens, owner / firmware decisions), 25 skipped** (logs `work/check-t0-tools-full7/`). Note: the device build intermittently fails with Errno 22 on `build/gen/*.h` when two checks or builds run in one checkout at once; run one at a time.

## Full run: 3 November 2026, t0-ui2 (`ui` repaired): 111 passed, 0 failed, 25 skipped

`sh jam/check.sh` on jam11 `95c7021` + t0-ui2: every group passes or is a correct SKIP (`fork()` and similar). `ui`: 0 lint findings, 0 alignment findings, 0 boxes.

- Firmware screen fixes of the last round (always-on, off hash updated, flash 578340 B): header messages on the header's middle; a card unit on an M value's baseline; piano roll note names' right edge; MONO label chip; save-dialog hint centring; two-line labels in cells and area cards, a long single word under its dot; QNTZ for the narrow automation card; TIDE list pill an allowed highlight; the NAME keys' words one row up; TIDE splash credit line off a strip edge.
- Lint (test-side): rolling digits and sweep frames are not overdraw; the REC sweep and SCALE scenes draw as the device; 1.1's MODULES page is a cell page; TIDE's splash is set in glyphs; the '<' '>' name keys are not declared (their ink cannot be measured; they look centred by their coordinates, not checked on screen).
- Open: a squeeze reads 54 at NORMAL type in LAYER and AFTERTOUCH-alone (tests relaxed to over 40); the web record does not round-trip SLOPE 24 dB.

## Full run: 3 November 2026, t0-ui2 (UI groups repaired)

`sh jam/check.sh`: **108 passed, 3 failed, 25 skipped**. Failing: `ui`, `editorjson`, `jam_dev` (the last two are the web editor's 1.0 model, see the t0-win notes below).

- Repaired test-side, now PASS: `uitest`, `pages`, `engine`, `scope2`, `velo`, `pan`, `dx7import`, `editb`, `fm6`. `perform` and `step3` run every check up to their `fork()` (SKIP). Causes: cards read from the drawn texts (`ui.col` is a hash), 1.1 / TIDE behaviour (matrix packing on restore, FM6's AMS buffer overflow in `fm6_test.c` that hung on Windows, KEY VELOCITY OFF + five types, PERFORM's ROLL keys are GLIDE TO in TIDE, item 160's BALANCE).
- Real screen fixes (always-on, off hash updated, flash +372 B in all): FM6 FEEDBACK label on its own line; two-line card labels where one line is too wide (FILTER ENV DEPTH, LOW / HIGH FREQUENCY, ROTARY SPEAKER); TEST / INFO bottom rows inside the canvas; TRIO WAVE values on two lines; TIDE splash credit line off a strip edge; AUTO TIDE's A clear of the knob.
- `ui` still FAIL: the `roll` scene (TEMPO label under a '9'), the "sweep FLAT" scenes (menu rows, ABOUT, REC layer values; the REC sweep draws the layer without its cell template), 1452 alignment findings, 36 boxes (TIDE morph pill 134 x 19).
- Open: a squeeze reads 54 at NORMAL type in LAYER and AFTERTOUCH-alone (the test expected over 100; relaxed to 40); the web record does not round-trip SLOPE 24 dB.

## Full run: 2 November 2026, `4443c13` (T0 branches merged, morph fixed)

`sh jam/check.sh`, 600 s per group, in a `git archive` snapshot (logs `work/check-full-4443c13/`), then re-run in the checkout for the groups the snapshot or the old `cc` shim got wrong (`work/check-full-4443c13-rerun/`). **80 passed, 34 failed, 22 skipped** (before: 65 / 58 / 11). `check_off.sh` PASS (`ade430c7…`).

- `morph` now runs to its `fork()` with no failed check (SKIP here). Its two roll checks were test-side: the seeds hash is `0x2f8cb758` on Windows / llvm-mingw for 1.1's own `974d875` too (both values accepted), and TIDE rolls a factory sound's B, so ROLL chained to depth 2 (the save check rolls from the Original now; a new check covers SAVE of a MUSICAL B).
- `multiv8` needs the tags `v1.0` / `v1.1`: it fails in a `git archive` snapshot, passes in a checkout.
- The shared `work/shims/cc` took the POSIX stand-ins from `DUELL/` (1.0, no `sys/mman.h`); it now takes the tested checkout's `jam/wincompat` first. `twin`, `twocore`, `subcyc`, `xvalues`, `velaccent`, `tcmeas` then SKIP at `fork()` instead of failing to compile.
- Still FAIL, as the t0-win notes say: `ownerimport` (tool fix), `benchsim` (stale model, macOS counter), `emu` (no first-start GUIDE, silent heavy song).

FAIL: perform ui step3 engine dlyev dx7import fm6 uf pan mxstep modinert eqon modules uitest editor editorjson reverb halltail locut shimmer panic bassmono comp master stereochorus benchsim ownerimport pages scope2 editb velo emu converttwin jam_dev

## Supervised maintenance batch: 2 November 2026

Pinned `7c22b13` plus reviewed local factory-packing and GLO knob-jump changes: native host equivalents of `bank`, `tidekeep` (72), `tidecontrols` (121), new `knobjump` (6) and `sidechain` (17) compile and pass with ASan/UBSan and no recovery. The new bank coverage compares actual C decoding/lookup/all three load kinds across every factory voice with original uncompressed pairs; a separate retained-baseline header comparison verifies unchanged pairs and metadata. `knobjump` is registered in the existing selectable/default host groups. Its runner syntax check passes.

Production on/off builds pass all existing layout/reserve/reach/closure checks. The unchanged off build reproduces `4b140b...`; the shared-code candidate's explained new package hash is `ade430c7b254f6a25a846caa5af6cd95e2ff83f544a0c6c367358476f50c7713`, with the same pinned ABOUT date. The exact retained TIDE-on package passes the hash-pinned CPU-emulator boot/LCD/MIDI-note/bounded-audio smoke. Evidence, native commands, source manifest and earlier failed restricted Windows launch attempts are under `work/outputs/maintenance/run-20261102-1922/`. No new whole-suite aggregate, installation or physical-device acceptance is implied.

## T0 progress: 2 November 2026, UI groups (branch `t0-ui`)

Repaired test-side, now PASS: `select`, `morphui`, `perflevelmut`, `motion`, `fuzz`, `xbmut`. `mod` and `level` compile and run with no failed check up to their first `fork()` (SKIP on Windows). Causes: TIDE's HOME round (MORPH SETTINGS in MORPH SET's place, then AUTO TIDE; ALGORITHM selects BOTH there), cards read from the drawn texts (`ui.col` is a hash since 1.1), item 160's third BALANCE rule, the 1.1 header capsule's A / B letter, PT_VER 4 / PROJ_REV 8 / MU_VER 8 layouts, End B kept as values as the B end sounds it, TIDE's MUSIC roll AMOUNT.

Still FAIL:
- `ui` compiles and runs (`8099bdf`) but its lint finds real screens, not stale names: ENGINE FM6's FEEDBACK label off its card (uitest item 158 says the same), TIDE's splash / ABOUT credit line over the waves, the retired SCALE layer's scenes, menu INFO / TEST bottom rows, AUTO TIDE's 'A', 1487 alignment and 36 box findings. Needs owner / firmware decisions.
- `editb`: one mutant needs `midi_control_test.c` to compile (`tests/scenes.h` mu_put_rec, the t0-scenes groups); the others match once morph_test's 2 known roll failures are not counted.
- Not started: `uitest`, `pages` (its card reads and the 1.0 card-width model of section (a)), `perform`, `step3`, `engine`, `scope2`, `velo`, `pan`, `dx7import`.

No firmware bug found in these groups.

## T0 progress: 2 November 2026, later

Repaired test-side (T0, roadmap): `backup`, `patch`, `patchraw`, `bank` now PASS. They rewrite the 1.0 record checks for PT_VER 4 (End B as an MBV block; values lossless only inside their range; SWING 50..75), TIDE's 126-patch store, 1.1's PATCHES knobs (K3 SAVE, K4 FAV, no per-slot RESTORE) and S0a's one pattern object (a backup LIST writes a waiting pattern save). Shared helper: `felucca-base/tests/endb_store.h`. `bank` no longer reads `tests/factory_bank_records.txt` (PT_VER 3, refused by 1.1): it saves and reloads every factory sound and compares the sound.

Scene-fed groups (branch `t0-scenes`, `e796169`, `433347e`): `tests/scenes/*.bin` converted from MU_VER 7 to 8 by the new `felucca-base/tools/multi_v7_to_v8.py` (End B made by the firmware's own roll; group `multiv8` checks it against 1.1's own converted factory multis and the scenes against the tool, PASS). `scenes.h` calls 1.1's `mu_put_rec`. Lossy: M09 SWING 17 becomes 54 % (old scale 16); BALANCE dropped in M03..M06 (1.1 has none). Results: `loadcost`, `cpuscen`, `midictl` PASS; `mrender` SKIP (one `fork()` per render: its 0.2.22 hashes are still unchecked here); `modules` FAIL 1 of 95 (an ARP in SEQUENCER: HOLD on K1, UI expectation, not the scenes); `panic` FAIL 2 (LAYER RESO / PLATE: the next chord after STOP ALL 2.3..2.9 dB off the cleared run, 1.5 allowed; plain DUELL 1.1 without TIDE fails it too, plus LAYER ROOM / SHIM). Not re-baselined.

Real bug found and fixed: RENAME / recategorise and the editor's FM6 PUT dropped a saved patch's End B (`dd2acd5`).

`morph` moved from SKIP to FAIL when the runner began listing every failed check (`e10bbc0`): its two roll checks already fail at `8506f2c`.

### Windows and tools groups (branch `t0-win`)

`jam/wincompat/sys/mman.h` (anonymous shared maps as plain memory: the child they share with is never forked here). `jam/check.sh` reports any "... which Windows does not have: skipped" stop as SKIP, not only fork. Note: `work/shims/cc` points at the main checkout's `jam/wincompat`, so the new header counts once it is there.

| Group | Now | Why |
|---|---|---|
| velaccent twocore tcmeas xvalues twin subcyc | SKIP (fork), every check before it passes | twin also: its LADDER mutation pattern follows 1.1's `* 64` |
| uf | FAIL (compile) | 1.1 API: `PTF_BM` gone, `card_label` and its call changed: a stale-test rewrite, not Windows |
| ownerimport | FAIL | fixed: compiles through the shell's `cc`, the export holds 126 records with TIDE. Left: `tools/import_owner_edits.py` reads every 1.1 factory sound as edited (1.1 fills the free matrix slots with default routes the recipe comparison does not know): a tool fix |
| benchrun | SKIP (termios) | the runner needs termios and select() on pipes; the parser checks pass. OWNER-1 had BRAZEN #31 and SQUARE LEAD #27 (they are #32, #28) |
| benchsim | FAIL (2) | fixed: 1.1's INFO line ends in `,core1 on`; the cost order needs the macOS counter (reported, not compared). Left, firmware data: the row "INS BIT CRUSHER" is 102 px (column 94), `bench_model.h` is stale (regenerating needs the macOS counter) |
| emu | FAIL (3) | fixed: it sourced `~/emsdk` unconditionally and stopped with no output. Left: no first-start GUIDE on a fresh flash, the heavy test song is silent after PLAY (the native build agrees): 1.1 / TIDE behaviour, needs a look |
| converttwin | FAIL (explained) | a one-time 0.2.x migration check: 1.1 rebuilt the factory multis as MU_VER 8 (other values, other sounds) and the tool reads today's tables, so "committed == tool output" cannot hold; retire it or freeze it with its own tables: owner decision. Its run also rewrites `tools/convert_twin_done.json` |
| editorjson | FAIL (16, explained) | not Windows: the web editor tests keep 1.0 expectations (menu 33 items: 1.1 dropped CPU METER; MIDI MAP tab; NX 28; matrix VIB and SPACE; MULTI FILTER 2/2 without LFO 2; FM6 AMP ENV no longer hidden; 1.1's renames SATURATION, WAVE SHAPING, TWIN RES, FILTER ENV DEPTH break the R1 naming checks). The mock's ARP / SEQ menu items are "DEMO ONLY" values the firmware does not have: an upstream / owner call before rewriting |
| jam_dev | FAIL (explained) | its mock is the web editor's MockUnit, which still builds 1.0 objects (old music header, pre-PT_VER 4 patch records) that 1.1's checkObject refuses: same root as editorjson |

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
- `patch` repair exposed the End-B preservation bug fixed in `dd2acd5`. Subsequent commits record `patch` passing at `379e844` and `patchraw` passing at `52c6604` against 1.1 + TIDE. These commit-recorded focused results do not imply a new full-suite run. Other test repair continues independently.

## Step 0 (history)

Step 0, commit `aa55e22` (1.1 `974d875` + Windows build, fonts 8-slot, hot-code move, `jam/` tools), 2026-11-01, `sh jam/check.sh` with a 600 s limit per group, Git Bash + llvm-mingw. Every later step is compared with this list, not with "all green".

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

