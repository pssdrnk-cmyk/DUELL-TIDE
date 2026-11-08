# DUELL with TIDE: full code and functional audit, round 2

Date: 2026-11-08. Auditor: Claude (Fable 5.1) as coordinator, with five Sonnet lanes (A correctness and races, B audio
quality and CPU cost, C storage and compatibility, D UI and docs drift, E tests and CI). Round 1 is
`../full-audit-2026-11-04/` (236 findings, 194 fixed by 2026-11-05); this round covers everything merged since its
baseline `d3ca0b9` and does not re-litigate what round 1 settled unless the code changed.

Audit tree: a `git archive` snapshot of **`89dfea3`** (branch `fix/w16-glide-cpu` as of 08:40) = `integ 817658d`
(tag `tide-1.0.0-b1`, public `jam11`) + the GLIDE CPU fix (`d13f5ab` .. `fb0848c`) + the eight drones `36f03cd` + ROADMAP
commits. **The unit runs `36f03cd` (TIDE 1.0.0-b2, flashed 2026-11-06 17:48).** Range audited: `d3ca0b9..89dfea3`,
377 commits, 88 firmware files (lists in `probes/`). Lane reports in `lanes/`, the shared brief in `AGENT_BRIEF.md`,
the owner's questions in `OWNER_DECISIONS.md` (D42..D48), the ROADMAP rows to land in `ROADMAP_ROWS.md`.

> **Baseline moved during the audit.** Between 08:47 and 09:46 the main session committed `069c65e` (the every-other-block
> blend only while a GLIDE runs, a fader jump is not a move, test expectations), `d399a72` (budget_test part count 1058),
> `4fe3958` (drones in PATCH_NUMBERS and the editor layout, JRNY texture count 11, `off.sha256` re-pinned), merged
> `fix/w16-glide-cpu` into **`integ` = `7746099`**, and tagged **`tide-1.0.0-b2`**. It also wrote a parallel CPU audit
> (`../cpu-audit-2026-11-08/CPU_AUDIT.md`, tasks K1..K12) whose ROADMAP rows are in. Lane E re-ran the seven red host
> groups and `check_off` on `4fe3958`: all PASS. Findings fixed by those commits are marked **fixed 2026-11-08**; the
> doc findings were re-checked against `7746099` and still stand unless marked. Nothing is pushed; the unit still runs
> `36f03cd` (ABOUT reads `1.0.0-b2+`, built before the tag existed).

## 1. Verdict

**The firmware is sound; the GLIDE CPU problem is contained, not closed; the build that is on the unit was flashed
with a red suite, and the docs describe the state of 5 November.** No P0. Three P1, two of them already fixed today.

- **Correctness (lane A): clean.** The two-core overrun hand-over is sound (the open job keeps part 2 silent, nothing
  renders twice, core 0 reads no buffer core 1 may still write). The test-mode probe is fully out of the normal image and
  its read-and-clear is under interrupts off. The every-other-block blend is per part and cannot serve a stale cache. The
  WASH coefficient cache invalidates on every input it reads. The no-ARP FM6 bridge path equals the old one. The 171
  sounds fit every table. Two bounded suspicions (a deferred FM6 drain that the same block re-arms; a POLY + ARP SEQ step
  phase after a key re-press) and two real gaps (the crossing charge lags one UI frame; nothing enforces the append-only
  numbering the drones rely on).
- **CPU (lane B): the budget is honest about TIDE's own DSP but not about the unit under GLIDE.** After the fix the
  16-multi GLIDE sweep still reads raw peaks of 91, 93, 94, 95 and 101 % (one missed block, M11) on five multis; the
  limit-72 calibration contained no GLIDE run. The NOTES cap acts only at note-on, so a held chord or drone keeps every
  note when a crossing starts, and drones under a running GLIDE were never measured. Four per-sample additions since the
  26 October cost calibration (SVF precision, delay low cut, master knee, reverb pre-delay) have no budget term, and the
  budget itself is recomputed inside the ISR at block 0 from flash code. The glide path still runs eleven functions from
  flash with 680 B of range Z to spare. The CPU audit of the same day (K2 truth, K3 ease ladder, K6 placement, K9 peak)
  is the right answer to all of this; the findings here are its evidence and acceptance criteria.
- **Storage (lane C): no defect.** The settings tail, the one-time migration, the never-rewritten user slots, the
  numeric `-bN` ordering and the untouched loader are all as they should be. Two compatibility edges: the parked 24-multi
  branch now collides with the FM6 voice bank's backup id 41, and the editor strips FM6 MORPH TO from a restore onto the
  two older TIDE builds that do accept it.
- **UI and docs (lane D): the UI tells the truth, the documents do not.** Every round-1 UI decision is in the code (two
  detents, page-scoped OCT-, STYLE tables equal the code, LOAD picked). But CHANGELOG-TIDE says "not flashed yet" for the
  build on the unit, the README's beta is "not out yet", the ROADMAP status table is dated 5 November and lists built
  things as planned, KNOWN_LIMITS gives the old bank-2 range, the printed cheat sheet gives wrong OCT- resets, the drones
  are in one doc of nine, and `jam/check_docs.py` fails and runs nowhere.
- **Tests and CI (lane E): the suite caught everything and nobody looked.** At `89dfea3` the default list is 171 groups,
  159 PASS, 8 FAIL, 4 SKIP. Five of the eight failures came from the CPU fix (the half-rate blend changed the samples of a
  hand-moved morph; `partindep`'s "never regenerate" pins caught it), two from the drones, one is a snapshot artefact.
  `36f03cd` was flashed with three groups red and `off.sha256` unpinned. All fixed today; the lesson is a rule (D45).
  `swing` and `duck` are real tests nobody runs; `emu` has no CI job; `fix/*` branches get no CI.

What must be kept is in section 6.

## 2. Findings

Priority: P0 data loss / brick / crash / silent audio; P1 wrong behaviour a player meets (a missed block in normal use);
P2 bounded defect or clear quality problem; P3 polish. Size: S < half a day, M one to two days, L more. Lane ids point
into `lanes/`. Status is as of `integ 7746099` (2026-11-08 09:46).

| # | Pri | Finding | Fix | Size | Lane | Status |
|---|---|---|---|---|---|---|
| 1 | P1 | After the GLIDE fix the unit still peaks at raw 91 / 93 / 94 / 95 / 101 % (one miss) on M6, M2, M13, M8, M11 in the 16-multi GLIDE sweep (`work/measure/cpu-multis/sweep_glide_fix7.txt`); the limit-72 calibration used PLAY alone and a held chord, no GLIDE; the sweep reads `raw` (before the TIMER5 subtraction) while the calibration used `cpu_pk_us`, so the scales differ | Re-run the sweep reading both figures; fit a GLIDE allowance or lower the cap on those five until K3 (ease ladder) lands; decision D42 | M | B-01 | open |
| 2 | P1 | `36f03cd` and `fb0848c` went to the unit with 3 and 4 host groups red (budget, deadline, partindep, wave, xvalues, editorjson, tidejrnyfamily) and `jam/off.sha256` unpinned for the drones (the plain image changed: the shared bank); `off.history.md` had no entry | Done: `069c65e`, `d399a72`, `4fe3958`; rule: no flash of a tagged beta while the suite is red (D45) | S | E-01, E-07, B-04, C-01 | **fixed 2026-11-08** |
| 3 | P1 | The every-other-block blend (`6b6678f`) applied to a hand-moved fader too and changed the samples of a non-glide morph: `wave`, `xvalues` and the `partindep` pins (scenes 3-5) failed; the unit behaves so | Done in `069c65e` (blend skipped only while a GLIDE runs); keep the `partindep` pins, never re-base them | S | E-02, E-03 | **fixed 2026-11-08** |
| 4 | P2 | The NOTES cap acts only at the next note-on (`voice.c:312`, `:392`, `:574`); a chord or drone held when a GLIDE or crossing starts keeps every note at the higher charge; the drone probe holds a 3-note chord but never starts a glide, so drone + GLIDE is unmeasured | Measure the drones with `glide 1 0` running; then either trim held voices when the charge rises (one-block fade) or size the drones' chord from the start (D43) | S / M | B-02 | open |
| 5 | P2 | Per-sample work added after the 26 October cost calibration has no budget term, only the flat 18-point `BUDGET_PEAK`: SVF precision (`voice.c:1115`, in `uf_run`, the hottest function), `dly_wr` low cut + soft knee on every delay write (`fx.c:442`, flash), `master_knee` twice a sample + `lim_mul` (`fx.c:260`), HALL / PLATE pre-delay (`fx_bus.c:163`); `BD_DLY`, `BD_IDLE`, `BD_REV` HALL / PLATE unchanged | Re-run the COST-CAL rows with `jam_dev.py measure`; fold the deltas into `budget.c`; meanwhile say in `budget.c` which rows predate these commits (companion of K9) | M | B-05 | open |
| 6 | P2 | The budget is recomputed inside the audio ISR at block 0 of every half: `tcr_deadline` runs `bd_part` (flash, 1,388 B) which calls `morph_apply_at` per part (`twocore_run.c:320-326`, `budget.c:157-186`); never timed; lands on core 0 where the peaks are | Time it with the stage probe; if over about 20 us publish the part figures from `budget_poll` as a snapshot the ISR reads | M | B-06 | open |
| 7 | P2 | Glide and crossing path still in flash: `morph_xstep` 1,896 B, `morph_valley` 1,178, `morph_wash` 998, `morph_xcut`, `morph_xpost`, `morph_xreg`, `fm6_xphase`, `fm6_xapply`, `fm6_xlv`, `fm6_env_offsets`, `fm6_macros` (run on every `fm6_sync` rebuild, i.e. each fader step); range Z 680 B over the 8,192 B reserve, hot block 388 B, flash about 3 KB | Time `fm6_sync`'s rebuild and the valley; K6's placement check so the next stage cannot land in flash unnoticed; moving `morph_valley` needs about 500 B freed in Z | M | B-07, A-08 | open (K6) |
| 8 | P2 | The FM6 crossing charge lags the crossing by up to one UI frame: `budget_poll` (main loop) sees `fm6_xt[].on` or the fader move once a pass, the bridge turns on in the audio path, so the part is charged `BD_TIDE_FM6_IDLE` (7) while the crossing already costs 76 | Pre-charge when the glide / fader arms (`bd_mv` is set before `on`), or accept as the price of "thin only while a crossing runs" | S | A-11 | open |
| 9 | P2 | The deferred FM6 stand-in drain (`c71ecfb`) gives the stand-ins back only if the bridge is off when `fm6_block` runs it; if the new plan set `x->on` again in the same block, `drain`, `mute`, `dirty` are cleared anyway and stand-ins can stay mapped (`eng_fm6.c:567-580`, `fm6_xfer.c` `want = on ? xs : 0`); the test covers `on == 0` only | Clear `drain` only when `!fm6_xt[tr].on`, else keep it for the next block | S | A-03 | open, SUSPECT |
| 10 | P2 | Nothing enforces "append-only after #163" (`N_LATE`, `duell_sounds.py:20`): no test pins the first 163 `(engine, idx, name)`; the count literals the drones moved (`budget_test.c` 1010 -> 1058, `tide_jrny_family_test.c` 8 -> 11) were typed, not derived | Pin the 163 and `len(SOUNDS)` in the generator or a host test; derive the two counts | S | A-01, E-04, E-06 | open (counts fixed) |
| 11 | P2 | Any fader change counted as a move, a load included: 60 polls (about 1 s) of the full FM6 charge after every load of an FM6 part with a bridging plan (`budget.c:432-440`) | Done in `069c65e`: a jump over 12 steps is not a move; residual: a hand / CC sweep faster than 12 steps a poll misses at most one frame | S | B-03, A-16 | **fixed 2026-11-08** |
| 12 | P2 | After `069c65e` a hand- or MIDI-moved fader is back on the every-block blend (about 70 us a block measured on the unit); the saving helps GLIDE only, a fast twist during dense play can still overrun | One condition for both if the owner agrees (D44); the blend trails the fader by 0.73 ms either way | S | A-15 | open, decision |
| 13 | P2 | The parked 24-multi branch (`multi-sequences` 21ca3a7) takes backup object ids 41..44 for M17..M24, but id 41 is the FM6 voice bank since `ad04e1b` (`duellbackup.js:44`, `jam_dev.py:74`); archive VERSION 6 against 5; `tests/erase_test.c` and `tests/xb_test.c` assert nothing lives at 0xEA000 / 0xF0000. Flash itself is clear (no object 50..53, nothing in 0xEA000..0xFBFFF) | Renumber the multi ids or the bank's at the rebase; add the checklist to the U4 ROADMAP row | S | C-02 | open |
| 14 | P2 | A restore onto a TIDE unit older than the INFO TIDE tag (`stable-2026-11-03`, `beta-2026-11-04`) silently strips the FM6 MORPH TO target from every patch: `duellbackup.js:218` reads "no tag 44" as plain DUELL; both builds do carry `MBV_FMT` and would load the flag; the confirm text says nothing | Treat "caps bit 5, no tag 44" as unknown (no strip), or count the changed records in the confirm text; `jam_dev.py restore` has no strip at all: one line in its help | S | C-03, C-07 | open |
| 15 | P2 | `editorjson` red: `web/editor/layout.json` (163 sounds) and `docs/PATCH_NUMBERS.md` (ends at #163) not regenerated by the drones commit; the web editor did not list #164..#171 | Done in `4fe3958`; put `cp build/host/layout.json web/editor/layout.json` on the "adding a sound" checklist | S | C-01, E-05 | **fixed 2026-11-08** |
| 16 | P2 | The unit's ABOUT reads `1.0.0-b2+`: `build.py:117-133` adds `+` unless HEAD is the commit of tag `tide-<version>`, and the tag did not exist at build time; no doc says so | Tag exists now (`tide-1.0.0-b2`); the unit keeps the `+` until its next flash: one sentence in README / KNOWN_ISSUES; next beta: tag first, then build (ROADMAP:38 already says so) | S | D-01 | partly fixed |
| 17 | P2 | CHANGELOG-TIDE: "Not flashed yet" under b2 (`:13`) and b1 (`:20`), "device validation pending" (`:15`), and the b2 section omits two behaviours the unit has: the FM6 crossing charge while the fader moves or a GLIDE runs (notes thin during a crossing, `budget.c:74-75,207,439`) and the moving morph blended every other block (`morph.c:1638`) | Remove the markers, put the commit and flash date in the heading, add the two "Changed" lines | S | D-02, D-03 | open (verified at 7746099) |
| 18 | P2 | README Builds table: beta `1.0.0-b1` "not out yet", no b2 (`README.md:7,67`); ROADMAP "Updated 5 November" with "Next beta: 1.0.0-b1 ... tag made when it is flashed" (`:11`), GLIDE fix "being prepared" (`:16`), flash 5,360 B free (`:14`), tests 143 (`:13`); rows that call built things planned: K1 two detents (`:221`), OCT- reset (`:222`), "the STYLE reset itself is not built" (`:262`), "merged, not yet in a build" rows (`:170-203`) | One rewrite of the status table and a strike-through pass | M | D-04, D-05, D-06 | open |
| 19 | P2 | KNOWN_LIMITS / KNOWN_ISSUES stale or silent: bank 2 "up to program 34 = #163" (`KNOWN_LIMITS.md:15`; real: program 42 = #171, `ui.c:1962-1964`), "5.4 KB free" (`:18`), "#125 to #160" (`KNOWN_ISSUES.md:21`), "ABOUT shows DUELL 1.1" (`:31`); missing: about 3 misses per FM6 crossing in a GLIDE (ROADMAP:217 says it), the unreproduced M11 spike, notes thinning during a crossing | Four numbers, three bullets | S | D-08, C-05 | open |
| 20 | P2 | The drones are in one player doc of nine: missing from QUICKSTART (`:38` "#160" against `:41` "#129 to #163": contradicts itself), TIDE_GUIDE (`:116`), README (`:87`), SPEC (`:5`, `:458`), EDITOR_LIBRARIAN (`:16`), DOCS_INDEX (`:42`); nothing ties any doc to `DS_N` | Update the eight places; `check_docs.py` checks PATCH_NUMBERS rows = `DS_N` | S | D-09 | partly fixed (PATCH_NUMBERS) |
| 21 | P2 | The printed cheat sheet's TIDE block is wrong: "K3: TIME 8. K4: WAIT OFF" (`cheatsheet/index.html:154`; code: the STYLE's values, DUB = 16 bars / 2 bars, `tide_ui.c:310-336`), no K2 (drop), old row names (TARGET BOTH / ROLL / ARP / PITCH / WASH LEVEL against SND+SEQ / ROLL ARP / ROLL PITCH / WASH), K1 "twist again" (two detents); `landing.html` step 4 the same | Re-read the block against TIDE_GUIDE; put it under `check_docs.py` | S | D-10 | open (verified at 7746099) |
| 22 | P2 | `jam/check_docs.py` fails today (`docs/ROADMAP.md:214` names a workspace path) and is called by no `check.sh`, `host_tests.sh` or workflow | Fix the line; wire it into `jam/check.sh` and CI shard 0; name it in CONTRIBUTING | S | D-07 | open (verified at 7746099) |
| 23 | P2 | The public CHANGELOG names `cpu_multis.py --glide` (`:16`) but the script (and `glide_watch.py`, `drone_probe.py`, `stage_probe.py`) is not in the repository; the test-mode console words `mload`, `pload`, `cpu`, `glide` are documented nowhere a developer looks (TESTING, DEVELOPING silent) | Move the scripts under `jam/` next to `jam_dev.py measure`; a six-line table in TESTING.md | S | D-11, D-12 | open |
| 24 | P2 | `swing` (S03 / S09: SWG storage, display, pair timing) and `duck` are real tests with `case` entries but not in the default `want=` list, so neither `check.sh` nor CI runs them (both PASS when run); `orphans` cannot see this | Append `swing duck` at the end of `want=` (the shards of the others do not move) | S | E-08 | open (verified at 7746099) |
| 25 | P2 | `tideroles` (roles spec T2, out of the default list by design) is red: 43 of 338 checks, the 30-120 Hz band under the full band (10, M16 FINAL REEL 2.0 dB) and loudness within 3 dB of the median (12: M04 +3.8, M10 +5.3, M12 -3.1, M15 -7.2, M16 -9.2 dB), the rest motif / onset; a guard that does not guard yet | Trim the multis by the numbers or relax the bounds (D46); with T8 / T9 | M | E-15 | open, decision |
| 26 | P2 | The `emu` group (wasm against native samples) runs in no CI job: `host-tests.yml` drops `$EMU`, `pages.yml` only builds the emulator | A ninth job running `sh host_tests.sh emu` before Pages (emsdk is cached there) | M | E-09 | open |
| 27 | P2 | CI runs only on a push to `jam11` and by hand: no `pull_request` trigger, so `fix/*` branches and upstream PR candidates get no CI; sharding is round-robin by list position (a group inserted mid-list moves every later group; appending moves none); `timeout 1500` per group and 60 min per job exist | `pull_request: branches: [jam11]`; append new groups at the end | S | E-10 | open |
| 28 | P3 | Drone comments in `tools/duell_sounds.py` number them #164..#168, #170, #171, #172 (no #169); IRON WORKS is #171 | Fix three comments | S | A-02, E-20 | open |
| 29 | P3 | OCT- held + K3 / K4 reset TIME / WAIT silently (`reset_say` prints nothing, `ui_input.c:1404-1408`); the C9 legend was not built | "TIME 16" / "WAIT 2" message, about 40 B (D47) | S | D-16 | open, decision |
| 30 | P3 | Small doc leftovers: "M16 is CELLAR TAPE" (QUICKSTART:40, TESTING:43, TIDE_GUIDE:117, SPEC:5; it is M03 since D31); TIDE_GUIDE:3 stops at b1, `:62` "VARY BARS ALL" (EVERY BAR); TESTING:10 "twist knob 1 while it glides (it stops)"; INSTALL:32,52 "TIDE does not change the shown version" | Edit | S | D-13, D-14, D-15 | open |
| 31 | P3 | UI lint gaps: no scene for a DAMAGED record (row and panel), ABOUT with the TIDE id (the `ui` group compiles without `DUELL_TIDE_ID`, so `ui_menu.c:287` never draws; `1.0.0-b2+` probably fits, SUSPECT), PATCHES rows past #163, four header messages of the linted length class | Three scenes | S | D-17 | open |
| 32 | P3 | Downgrade edges to document (D11: downgrade not supported): a multi saved with a drone loads on b1 and older with the right sound but another name (`project.c:371` `preset % ds_count`); chord types 10..15 clamp to POW on `187e9d0` / `77a6df2` (`params.c:66`, `upreset.c:206`) | One KNOWN_LIMITS line each | S | C-04, C-08 | open |
| 33 | P3 | `settings_persist.c:79-80` says plain DUELL zeroing the tail makes TIDE "start from DUB again"; a migrated unit still has object 48, so the pre-migration values come back instead (harmless; keeps settings across a return to stable) | Fix the comment (D48: keep object 48) | S | C-06 | open |
| 34 | P3 | Budget constants still guessed or fitted to one scene: `BD_MORPH` 16 and `BD_FADE` 66 are the 0.2.11 guesses and the blend is a per-part cost charged per voice (`budget.c:64,77`); `BD_TIDE_FM6` 76 is half an FM6 voice fitted to BIG STAB part 1 only; the WASH power meter runs uncharged on a WASH-on part without a MORPH (`morph_xfer.c:604-620`); the FM6 cull's -66 dB is relative to a full carrier, not the note (`fm6_core.c:331`; not listened) | Measure the blend as a part cost (the unit saw about 70 us a block); sweep the FM6 sounds with a bridging plan; charge about `BD_TIDE_WASH / 8` or leave; lower `FM6_CULL` to 4096 only if a dry pad ever sounds short | S | B-08, B-11, B-10, B-09 | open |
| 35 | P3 | Code nits with no measured effect: `DUELL_PROBE_STAGES` defined only inside `#if DUELL_TIDE` (undefined elsewhere; a `-Wundef` build would warn); `morph_apply_at` (RAM) still calls `morph_xside` (flash) once per apply; `budget_poll` loops `p < NLIVE` over `mph[NSHOWN]` (both 2); `morph_gliding` is written in `glide_motion`, so a `glide_stop` lags one poll and the SEQ-only lane also charges the crossing (pessimistic, as before) | As listed; `morph_xside` into Z (about 100 B) only if the unit still shows the cost | S | A-06, A-08, A-12, A-14 | open |
| 36 | P3 | POLY + ARP SEQ: `tide_arp_idx` computes `(k - poly_base) % poly_len` with unsigned `k`; if `arp_idx` restarts at 0 mid-stretch (all keys released, then pressed) the subtraction wraps and the step phase is wrong for that stretch; the index stays in range, no overflow (`seq_poly.c:94-119`) | End the stretch when `arp_idx` restarts, or add a multiple of `poly_len` first | S | A-18 | open, SUSPECT |
| 37 | P3 | Test-tree hygiene: `fuzz` writes `felucca-base/nul.exe` into the source tree (`cc ... -o /dev/null` under llvm-mingw; the untracked file in every worktree); `jam/size_log.csv` is neither committed nor ignored; "25 groups SKIP on Windows" is stale in `host-tests.yml:1`, `DEVELOPING.md:30`, `wsl.sh:3` (3 real SKIPs since the fork shim); `dx7compat`, `ldrscreen`, `otareuse` SKIP in CI too; `multiv8` FAILs instead of SKIPping in a snapshot | `-o "$OUT/sanprobe"`; decide the CSV; three sentences; `multiv8` SKIPs without `v1.0` | S | E-11..E-14 | open |
| 38 | P3 | Test quality notes: `glidecpu` proves the 32-bit wash sample-identical to an embedded copy of the old code over 108,000 blocks (good) but tests no CPU (nothing asserts a margin): say so in its comment; the `budget` FM6 line now accepts `IDLE || full` and no longer says which applies when; the factory-multi order is hard-coded in `multi_test.c`, `release_test.c`, `tide_roles_test.c`, `bench_sim.c`, `TIDE-DSP.txt` (all right today); twocore's `F_NONE` scene hard-checks `late == 0 ...` without the `hang` exemption the generic check has (1,716 checks clean when run pinned to 2 CPUs; SUSPECT) | Split the FM6 check by `fm6_xt[].on`; one generated name table; give `F_NONE` the `hang` exemption if it flakes | S / M | E-16..E-19 | open |
| 39 | P3 | `jam11` is still the development line's name (D12 rename pending) | None until the rename | - | D-18 | open |

## 3. The next tasks, in order

Ordered for the owner. Each names the findings; sizes as above. K-numbers are the CPU audit's tasks (same day, same folder
family), which this audit confirms and does not duplicate.

| # | Task | Findings | Size | Needs owner |
|---|---|---|---|---|
| 1 | **Docs truth pass for 1.0.0-b2** in one commit: CHANGELOG markers and the two missing b2 lines, README Builds table, ROADMAP status table and strike-through, KNOWN_LIMITS / KNOWN_ISSUES numbers and three bullets, the drones in eight docs, cheat sheet TIDE block, the small leftovers; wire `check_docs.py` and make it green | 16-23, 28, 30 | M | - |
| 2 | **Unit measurement before 1.0.0** (owner's unit, test-mode image): the 16-multi GLIDE sweep reading `cpu_pk_us` and `cpu_raw_pk_us` together; each drone with its chord held while `glide 1 0` runs | 1, 4 | S | D42, D43 |
| 3 | **One blend condition** for GLIDE and hand / MIDI moves, if agreed | 12 | S | D44 |
| 4 | **Test-list hygiene**: `swing duck` appended, `emu` job, `pull_request` trigger, `nul.exe`, `size_log.csv`, the "25 SKIP" sentences, `multiv8` SKIP in snapshots | 24, 26, 27, 37 | S | - |
| 5 | **Budget honesty** (with K9): COST-CAL re-run for the post-calibration per-sample additions, `bd_part` timed in the ISR and moved to a snapshot if dear, the blend charged as a part cost | 5, 6, 34 | M | - |
| 6 | **Small code fixes with real effect**: the FM6 drain guard, the crossing pre-charge, the append-only pin and derived counts, the POLY stretch phase | 8, 9, 10, 36 | S | - |
| 7 | **Storage edges**: the restore planner's "unknown" case and `jam_dev.py` help; the U4 rebase checklist (ids, VERSION, two tests); two KNOWN_LIMITS lines; the object 48 comment | 13, 14, 32, 33 | S | D48 |
| 8 | **Placement** (K6): the per-block allow-list check; time `fm6_sync` and the valley; `morph_xside` into Z if the unit shows it | 7, 35 | M | - |
| 9 | **`tideroles` levels** with the roles tasks T8 / T9 | 25 | M | D46 |
| 10 | **Polish**: OCT- messages, lint scenes, `glidecpu` comment, FM6 budget check split, multi-name table, `F_NONE` exemption | 29, 31, 38 | S | D47 |

Rule from finding 2 (D45): a tagged beta is never flashed while the full suite or `check_off` is red; the flashed commit
is merged and pushed the same day (the CPU audit's K1 says the same).

## 4. Round-1 findings re-checked

Lanes re-checked their round-1 areas (details in `lanes/*.md` section 3). In short: A02-A06, A08-A10 fixed and still
fixed; A01 partly (FM6 bridge measured on one scene; valley, wash, dip still host estimates x 1.5); P05 and Q03 open by
decision; Q14 not fixed (PHYS charged as MODAL for every model; the unit run was too coarse). D01, D03-D09, I04 fixed;
D02 partly (finding 14); I06 partly (finding 15, now fixed). C4, C9 (reset), C13 fixed in code (finding 21, 29 for the
docs and the legend); F-03 not verifiable without network; I11 fixed in form, stale in content (finding 21); T12-T19 not
re-checked. E01-E05, E07-E12, E14, E17 fixed; E06 partly (finding 27); E13 (compile-dominated suite, 35 min on a 28-thread
PC) open and not worth doing now; E15 not re-checked.

## 5. What was run

All runs in private copies of the `89dfea3` snapshot on Windows (Git Bash, llvm-mingw); no checkout was built or tested.

| Lane | Groups | Result |
|---|---|---|
| A | twocore glidecpu tidesteps chord tidemotif | all PASS (every twocore mutant detected) |
| B | budget deadline glidecpu audiopolish | budget FAIL (4 assertions), deadline FAIL (1), glidecpu PASS, audiopolish PASS |
| C | persist tidestore backup editorjson multi patslot erase; web tests one by one; `test_site_build.py` | all PASS except editorjson (stale layout.json, then PATCH_NUMBERS); web tests PASS; 27 site tests OK; multiv8 not run (needs tags) |
| D | ui uitest uitruth guide guidesetup tidesteps livegraph morphui tideversion menuproduct patslot sounds; `python jam/check_docs.py` | all PASS, `sounds: 171 sounds, 0 FAIL`; check_docs FAIL (ROADMAP:214) |
| E | the full default list (171 groups, 35 min); `jam/check_off.sh`; the WIP patch applied; swing duck tideroles; twocore x5; then budget deadline partindep wave xvalues editorjson tidejrnyfamily + check_off on `4fe3958` | 159 PASS / 8 FAIL / 4 SKIP at 89dfea3; check_off FAIL (`3f69b964` against pinned `e51ce4b2`; the parent `babcb92` is byte-identical to the pin, so the GLIDE fix is TIDE-only and the drones alone changed the plain image); with the WIP 5 of the 8 pass; swing PASS, duck PASS, tideroles 43 of 338 FAIL; twocore 1,716 checks 0 failed; on `4fe3958` all seven PASS and check_off PASS |

Not run: benchrun / benchaudio (WSL only), the full suite on `7746099`, anything on the unit.

## 6. Keep as is

The two-core overrun policy and its mutation tests; the unity-build include order; `rskip` per part with the live-value
check; the probe compiled out of the normal image with its read-and-clear under interrupts off; the chord clamp; the
`bd_mv` idea (charge the crossing only while it can run) and the 1.15 deadline margin; the 8,192 B pool-tail check;
`glidecpu` and `fm6liveol` comparing against embedded copies of the old code; `partindep`'s "never regenerate" pins (they
caught finding 3); `check_off.sh` (it caught finding 2 in 15 s); the mutation self-checks; `jam/wincompat` (168 of 171
groups run on Windows); the settings tail design (no new object, tag + count, longer counts kept); the one-time migration
that never writes object 48; `ps_refresh` only on unflagged slots; read-only DAMAGED; `N_LATE` append-only browse order;
`releases.py`'s numeric `-bN` ordering with its tests; the untouched loader and OTA; `check_docs.py`'s STYLE-table check;
TIDE_GUIDE as the one owner of the row texts; the MULTIS / PATTERNS list with LOAD always picked; the inert-row logic and
the CUSTOM snapshot; the live graphs' 80 % gate (it reads the smoothed average, so it saves UI time and cannot make the
ISR late: do not describe it as a peak guard).

## 7. Count

| Priority | Open | Fixed 2026-11-08 | Partly fixed | Total |
|---|---|---|---|---|
| P0 | 0 | 0 | 0 | 0 |
| P1 | 1 | 2 | 0 | 3 |
| P2 | 20 | 2 | 2 | 24 |
| P3 | 12 | 0 | 0 | 12 |
| **All** | **33** | **4** | **2** | **39** |

The 39 rows group 77 lane items (A 20, B 11, C 8, D 18, E 20); the lane reports in `lanes/` keep every item with its evidence.
