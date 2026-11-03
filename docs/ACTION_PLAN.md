# Maintenance action plan for agent handoff

Prepared 2 November 2026; status reconciled 3 November 2026. **First supervised batch complete, committed and flashed (part of every package since `ecd2918`, 3 November).** Three GPT-6 Luna agents delivered P1/P2/P4; the P2 agent then scoped and implemented P3. The coordinator reviewed, corrected and checked P1/P3, initially applying them locally; subsequent commits are `61eb641` (packing) and `89a9a77` (navigation). The [roadmap](ROADMAP.md) owns product priorities and the musical delivery order. Detailed batch briefs below are retained for continuity, not an instruction to launch another agent batch.

**Current state:** the test-tree cleanup is finished (full check 112 pass / 0 fail / 25 skip, see the [test baseline](JAM11_TEST_BASELINE.md)). Future implementation uses the affected checks; whole-tree cleanup is no longer a gate.

## Outcome and scope

Create useful application-flash headroom, simplify one inherited responsibility, and establish a compatible way to offer genuinely empty user voice and pattern slots while retaining the factory library. Keep sound, controls, saved records, KEEP/UNDO and manual takeover intact.

The first delivery milestone, **one integrated, checked change with a positive net application-flash saving**, is achieved in source. The full read-only firmware audit is complete; findings and coverage are retained at `work/audit/firmware-review-2026-11-03/AUDIT.md`, with correction candidates in roadmap A1. P7/U1 is a small planned convenience: 4–8 clear voice/pattern save destinations using existing addresses/overwrite behaviour while keeping factory access. It is not a gate to musical work. Exact live-morph restart preservation is not required. Current audio polish is accepted. TIDE configuration backup across flashes is a useful bounded follow-up; future settings-field growth is compatibility bugfixing. Further compaction should serve a selected feature's measured budget.

## Starting point and dispatch rules

- Target: `work/wt/duell-jam11`, branch `jam11`; observed HEAD `7c22b13e1973d9388a0f2e57bffe78f694b599b6`. This is a planning reference, not an instruction to reset a newer checkout.
- At preparation, README and several documentation files had existing edits; the reorganized roadmap/design/history were untracked, as was `felucca-base/nul.exe`. Preserve them. Test repair is an existing independent workflow; `backup`, `patch`, `patchraw` and `bank` have recorded focused passes. See the [test baseline](JAM11_TEST_BASELINE.md).
- At preparation the installed package was `e176f6e` / firmware `7535916`. Since then `ecd2918` (boot pack) and `187e9d0` (`polish-all`, F01–F04) were flashed on 3 November; the roadmap records the current installed package. Branch HEAD, test repairs and a candidate build do not change installation status.
- Before dispatch, the coordinator checks current HEAD, diff and active file ownership, and gives every agent an exact common source revision. Include any relevant uncommitted source explicitly; do not silently omit it. Supply this plan and the current roadmap when they are not yet part of that revision.
- Reuse a suitable isolated checkout; create a managed worktree through the app's worktree tools only when needed. Do not have multiple agents edit or build in the shared `jam11` checkout. Give each candidate separate build output and log paths. Do not reuse or reset another workflow's worktree.
- The completed batch used three agent lanes plus the coordinator. The test-tree cleanup is finished; the historical batch ownership is not current authority. Publishing, installing and destructive cleanup remain separate actions.
- These are task briefs, not replacement engineering instructions. Read applicable owner/project instructions. No new agent-instruction files are needed.

## Queue and dependencies

| ID | Roadmap | Deliverable | Ready when | State |
|---|---|---|---|---|
| P0 | E1/E3 | Pinned source and comparable baseline | Coordinator starts | Complete: `7c22b13`; on/off builds retained; old off hash reproduced |
| P1 | E1/E4 | Lossless factory-voice packing candidate | P0 | Committed `61eb641`: 1,888 B net TIDE-on flash saved in retained build; exhaustive C equivalence passes |
| P2 | E1 | Temporary-buffer lifetime decision | P0 | Reviewed: project/storage overlap rejected; OTA reuse could save 512 B pool RAM, estimate only |
| P3 | E2/E3 | One focused UI/input simplification | P0; ownership clear | Committed `89a9a77`: shared GLO knob-jump helper; 48 B additional flash saved in retained build; input checks pass |
| P4 | U1 | Free-slot design and measured feasibility | P0; a lane becomes free | Reviewed proposal: FACTORY/USER groups, unchanged record IDs; no feature implemented |
| P5 | E4/E3 | Next supported memory reduction | P2 decision; coordinator selects it | Parked: OTA-path verification cost for 512 B RAM is not the immediate flash priority |
| P6 | E1–E4 | Integrated maintenance candidate and receipt | Each selected implementation passes its own checks | Complete: 1,936 B flash saved in its build, all measured RAM/code regions unchanged; flashed since as part of `ecd2918` and later packages |
| P7 | U1 | 4–8 clear user save destinations | Smallest compatible UI/address choice; measured budget | Planned convenience; no capacity migration/full browser redesign required; stopped saves sufficient |
| P8 | E5 | Reviewed retirement list | Maintenance work no longer needs the artifacts | Parked; inventory only |

**Batch receipt:** `work/outputs/maintenance/run-20261102-1922/SUPERVISOR.md`, with the exact source manifest, patch, measured on/off artifacts and focused logs. The combined TIDE-on flash is 577,920 B, leaving 3,644 B; RAM, pool, range X and hot code are unchanged. Strict `bank`, `tidekeep`, `tidecontrols`, `knobjump` and `sidechain` checks pass; pinned package CPU-emulator smoke passes. These are software results, not a hardware acceptance or installation.

**Next selection:** choose bounded corrections from the completed firmware audit's verified A1 findings. Keep a P7 brief to four initial save destinations per kind, up to eight if the same approach fits, with simple labels and factory access. Existing overwrite saving is acceptable; preserve occupied records and old editor compatibility. Do not require a new grouped-editor protocol, physical capacity expansion, exact live End-B continuation, or live-save workflow. P5 remains parked. TIDE configuration archive/restore can be a separate small change; settings-record extension should preserve old fields and default only missing new fields.

P1's positive net saving is established; do not reopen it as an uncompleted experiment. Compare factory-multi packing or another supported candidate only if a selected feature needs more room. P5 remains parked because its RAM estimate does not address the flash constraint.

## P0 — coordinator: pin the baseline

1. Record current source revision, relevant dirty changes and existing worker ownership. Resolve overlapping write ownership before dispatch, without discarding concurrent work.
2. Make one production TIDE-on build of the pinned source using the existing Windows/Git Bash environment. Retain its build log, ELF/package and package SHA-256 under a unique evidence directory.
3. Record flash, RAM, pool, range X and hot-code use, physical headroom and enforced reserves. Use the build's placement/call-reach/range-X checks. The installed figures are historical comparison only.
4. Pass agents the source identity, baseline receipt and allowed file set. A worker may investigate while the baseline builds; candidate measurements must use comparable flags/toolchain and the same source starting point.

**Done:** all workers can identify what their before/after numbers compare. No broad-suite repair or device session is required for this step.

## P1 — agent: recover flash through lossless factory-voice packing

**Goal:** reduce the linked firmware in flash without changing any factory sound or its identity.

**Write ownership:** `felucca-base/tools/gen_duell_sounds.py`, `felucca-base/firmware/src/sounds.c`, and a narrowly scoped packing-equivalence test. Coordinate any existing test-file edit with its current owner. Generated headers are build outputs. Keep authored `duell_sounds.py` recipes and saved-record formats unchanged. Request a scope adjustment before touching the common runner, loader interfaces or other firmware modules.

**Work:**

1. Trace every reader of `DS_PAIR`, especially `ds_put()` and `ds_value()`, and the generator's parameter/default validation. Count only data present in the linked target: the inactive classic `DS_ENDB` data is not an available saving in the TIDE build.
2. Prototype one simple bounded representation for the sparse parameter/value pairs. Estimate packed payload, index metadata, decoder code and scratch separately; implement the best justified option only. Decoder work belongs to loading/querying factory data, not the audio ISR.
3. Preserve engine/index identity, browse order, names, categories, flags, signed values, extension IDs, arp/key filtering and End-B seed/amount behavior. Do not reorder or remove factory entries.
4. Compare baseline and candidate target builds. Reject a candidate whose decoder/metadata consumes the apparent saving or whose runtime/state cost defeats the purpose.

**Acceptance:**

- Compare every decoded entry against the original representation using a reference independent of the new decoder. Check normal, arp-only and key-only loads, lookup defaults and extension values. Exhaustive comparison is practical for this finite bank.
- Run the existing `bank` group, generator validation (`--check`) and the focused equivalence check. Run applicable TIDE factory-selection/loading checks if those call paths change. Classify inherited failures rather than rewriting their expected sound.
- Net application-flash saving is measured after decoder/index overhead; all memory regions/reserves still pass. Report added load-time work and scratch. Factory data equality is software evidence, not a new hardware listening result.

**Stop:** return one coherent candidate, or a rejection with measured reasons. Factory-multi packing (`multi.c`, `gen_duell_multis.py`) is a follow-up option, not part of this first patch. No DSP/table approximation, cache framework or content removal.

## P2 — agent: decide whether temporary buffers can safely share memory

**Mode:** read-only source investigation. Write only the result receipt; no firmware or layout changes.

**Read scope:** `ota.c`, `project.c`, `storage.c`, `multi.c`, `editor_backup.c` and their callers under `felucca-base/firmware/src/`.

**Work:** trace `proj_wire_u`, `st_buf`, `ota_dec` and OTA staging arrays. For each, record size/region, readers/writers, who owns it, when its contents must survive and nested calls that may overwrite it. Include boot, autosave, backup/restore, update entry, timeout/cancel and error recovery. Account for ISR/second-core references and flash-driver requirements. “Main loop only” does not prove non-overlap when one operation calls another.

**Acceptance:** return a small lifetime/conflict table and either:

- one specific share/reuse candidate with call-path evidence, exclusion/recovery contract, estimated RAM benefit, likely flash cost and the checks needed to prove it; or
- a supported finding that the apparent duplication is necessary, with the conflicting call paths identified.

Separate estimates from linked savings. Do not propose shrinking delay, performance capture, display canvas or engine arenas as free savings. RAM recovery does not meet the flash-saving milestone.

## P3 — agent: simplify one UI/input responsibility

**Goal:** make one inherited handler easier to reason about with unchanged visible behavior. Start with control-event routing in `felucca-base/firmware/src/ui_input.c`: layer consumption, knob reset/recording and normal page editing are adjacent responsibilities with ordering constraints.

**Write ownership:** `ui_input.c` and the narrow affected input test/capture fixture after checking existing test-worker ownership. Other UI files, editor protocol, DSP/storage and TIDE planners are outside this assignment. The older `CLAUDE_UI_COMPACTION_HANDOFF.md` contains useful historical experiments, but its baseline and scheduling are not current dispatch authority.

**Work:** map the relevant event/state flow, choose one duplicated decision or cohesive routing block, and simplify it. Preserve the order in which layer input is consumed, reset turns are swallowed, automation/locks are recorded and hot-column state is updated. Preserve page navigation, live keys and TIDE takeover. Do not split the file mechanically or introduce a generic event framework. If no worthwhile local change exists, return that conclusion and one better-supported next target.

**Acceptance:**

- Explain the removed duplication or clarified responsibility and why callers retain their behavior. Exercise normal knob editing, reset by the existing button/modifier paths, and layer-consumed input without a second underlying edit. Include the affected recording and TIDE paths when touched.
- Use relevant existing input/interaction checks and a representative emulator/UI flow. Repair stale affected test assumptions only as needed; preserve unrelated failures. Request shared fixture/runner changes through the coordinator.
- Build and report every memory delta, including zero. Target unchanged or smaller flash; any growth needs an explicit trade-off before integration. This package must not be presented as memory recovery solely because a function/file became smaller.

**Stop:** one focused simplification. No page redesign, new controls, font/canvas changes or broad editor/test rewrite.

## P4 — agent: design genuinely empty voice and pattern slots

**3 November scope clarification:** the owner accepts overwriting existing sounds and wants only 4–8 clear USER destinations. The original broad investigation below is retained as evidence; do not treat full catalogue/editor redesign, exact restart continuation or additional physical storage as requirements.

**Mode:** read-only feasibility and user-flow proposal. No layout, migration, capacity or protocol changes yet.

**Starting question:** can we present the complete factory library and at least a small explicit user area, studying **four voices plus four patterns**, without losing or renumbering existing saved material? Four is a study size, not an accepted capacity limit.

**Read scope:** `sounds.c`, `upreset.c`, `up_fm6.c`, `patslot.c`, `storage.c`, panel/editor list and save callers, `editor_backup.c`, `jam/jam_dev.py`, and relevant web-editor slot handling.

**Work and acceptance:**

1. Sketch the actual browse/save/load flow: where FACTORY and USER material appear, how an unused entry is labelled, first-save behavior, and how the current library stays reachable. This is separate from merely making overwrite easier.
2. For voices, compare separating compiled-factory browsing from existing user-record presentation against extending physical records. Reconcile the 128 visible addresses with TIDE's 126 guaranteed worst-case records. Check on-wire slot fields and editor assumptions; do not simply raise a count.
3. For patterns, trace first-start prepopulation, existing 11/11/10 object boundaries, pending saves and default generation. Show how all 32 factory patterns and all existing user records survive. A populated factory slot relabelled USER does not establish a free user area.
4. Give one recommended smallest compatible design and at most one fallback. Include object/slot mapping before/after, old/new/absent-record handling, worst-case capacity/full-store behavior and backup/editor compatibility. Identify any choice requiring owner authority.
5. Separate measured current sizes from estimates for added code/RAM/record space. Specify a small proof or measurement needed before implementation, then list meaningful round-trip, migration, interrupted-write and full-store checks. Use the real panel save entry point; do not prescribe a save action unavailable during playback.

**Done:** an implementable recommendation with explicit unresolved choices, or a precise feasibility blocker. General backup expansion and the live End-B reboot discrepancy remain separate tasks unless the proposed slot design actually depends on them.

## P5 / P7 — conditional implementation assignments

- **P5:** coordinator selects one P2-supported reuse candidate or a separate factory-multi packing candidate. Issue a narrow file set and acceptance contract before work starts. Buffer reuse needs real overlap/error-path coverage and a linked RAM/flash comparison; multi packing needs exact reconstruction of all current factory records and existing `multi`/`project`/`persist`/`backup` checks as affected. Keep saved formats unchanged.
- **P7:** turn the reviewed P4 recommendation into a separate feature brief, including the agreed user flow, capacity, compatibility contract and file ownership. Resolve material layout/data choices before making them. Free-slot feasibility may proceed early; implementation follows a demonstrated fitting budget. Do not bundle unrelated storage backlog fixes.

## P6 — coordinator: integrate and verify the selected candidates

Review each diff and its receipt before integration. Apply one candidate at a time to the current accepted source and check for overlap with later test repairs. Rebuild the combined result; isolated savings need not add because this firmware is largely one translation unit.

The combined candidate must retain the existing placement, call-reach and range-X checks and ordinary-RAM/pool/hot reserves. Run the union of genuinely affected checks once after coherent integration. Use representative emulator/UI flows for changed interaction. Do not require an unrelated full-suite repair or stack several musical changes into maintenance acceptance.

Shared-code changes require a TIDE-off build and comparison. `jam/check_off.sh` pins a build date and writes a shared log/output, so run it serially with retained TIDE-on artifacts. A packing/shared-UI change may legitimately change the off hash: explain the delta, verify it, and only then update the baseline through the coordinator. Never bless a new hash solely to obtain a pass.

**Done:** requested behavior preserved in the affected software checks; a measured positive flash saving is retained; package/ELF and logs identify the exact candidate; roadmap records source versus installed state. If a target/emulator check cannot run, report the actual limitation. Installation, device CPU/clock/audio and owner listening remain separate evidence; this plan does not dispatch a flash or release.

## P8 — parked housekeeping

Use `work/housekeeping-2026-11-02/REPORT.md` as a dated inventory. Return an owner/consumer check and a proposed keep/archive/remove list. Retain backups, installed/candidate packages, source-to-artifact receipts and listening references. No recursive deletion or worktree retirement is authorized by this task; use managed worktree lifecycle tools when retirement is separately agreed.

## Commands and evidence

Run from the assigned checkout in **Git Bash**, using the already installed toolchain:

```sh
. jam/env.sh
# Build and a selected existing group set; this wrapper already builds first:
JAM_CHECK_LOG="$JAM_WORK/outputs/maintenance/P1/check" sh jam/check.sh bank
# Direct build when no host group is needed:
(cd felucca-base && python3 tools/build.py)
# Validate factory recipes (this does not build or measure firmware):
(cd felucca-base && python3 tools/gen_duell_sounds.py --check)
```

Replace `P1` and the group list for the assignment; use distinct baseline/candidate paths. `sh host_tests.sh <groups>` is the existing host-only entry point when a coherent target build is already retained. Do not run the build wrapper and then repeat its target build without a reason. See [the test baseline](JAM11_TEST_BASELINE.md) for known failures, not a current all-green claim. Do not install new tooling or execute an external repository to perform these tasks.

Each agent returns **one short receipt** under `work/outputs/maintenance/<task-id>/`, outside tracked product source, plus its reviewable patch/diff:

```text
Task / result: candidate | investigation complete | rejected | blocked
Source: exact baseline revision + any included dirty source; candidate identity/diff
Files changed and behavior preserved or deliberately proposed
Measurements: before -> after -> delta for flash, RAM, pool, range X, hot code
             (use "not measured" for investigation; label estimates)
Checks: exact commands, observed pass/fail/skip, relevant log/artifact paths
Risks/choices: remaining limitation, next decision, installation state
```

No duplicate project plan or full test ledger is needed. The coordinator records accepted task outcomes in this queue and the roadmap; detailed logs remain with the receipt.

To dispatch, give an agent the common starting-point rules, its P-number section, applicable verification/evidence sections, exact checkout/revision and its file ownership. End the assignment with: **"Complete this bounded task, return the evidence and patch if applicable, and stop before the next package."**
