# DUELL with TIDE roadmap

Updated 2 November 2026. **Current priority: improve memory use and clean up the inherited DUELL/Felucca code.** The owner does not currently save: every visible voice/pattern slot already contains curated material. The useful storage request is a few genuinely free user slots for both voices and patterns, keeping the existing library available. General save/backup improvements remain a separate backlog, not a prerequisite to memory or code cleanup. Musical expansion is deferred. The DUELL 1.1 migration is complete.

This is the current delivery/status document. Detailed [musical proposals](design/TIDE_MUSICAL_DESIGN.md) and the [historical roadmap and receipts](history/TIDE_ROADMAP_2026-11-02.md) are references, not competing work queues. Keep the [test baseline](JAM11_TEST_BASELINE.md) as the record of test results.

The [agent action plan](ACTION_PLAN.md) records the first completed supervised batch from pinned source `7c22b13`: lossless factory-voice packing and one shared knob-navigation helper are applied locally; buffer and free-slot investigations are reviewed. The combined candidate saves 1,936 B application flash and is not installed. Musical work and general save/backup improvements remain deferred or separate as described below.

## Baseline and evidence

| Item | Verified state |
|---|---|
| Active checkout | `work/wt/duell-jam11`, branch `jam11`; shared with another workflow |
| Installed source | `e176f6e` (firmware through `7535916`); installed 2 November 2026 |
| Installed content | R0, M1, M2a, S0a/S0b, DSP arithmetic fixes, TIDE startup-write fix |
| Package SHA-256 | `ba8fdaf9252a0cf64f5230472ce10e738801b22926b2619e46cb81216648a8aa` |
| Device/data evidence | FM-1_901 / DUELL 1.1; reboot verified; all 16 archived objects equal after restoring current music; separate TIDE preferences are outside that archive |
| Owner playback report | "sounds OK I guess"; basic playback observation, no controlled A/B comparison |
| Newer committed source | `dd2acd5`: patch rename/recategorization and editor FM6 PUT preserve saved End B; subsequent test repairs at `379e844` and `52c6604`; not part of the installed package |
| Local maintenance candidate | `7c22b13` plus the reviewed, uncommitted P1/P3 patch; package SHA-256 `4852f39142d3f9b3de6acd961aee6df96208f5f780d6685632edadc7d0731612`; source manifest and retained artifacts at `work/outputs/maintenance/run-20261102-1922/` |
| Concurrent work | Existing test repairs and documentation edits were preserved. Recheck status and file ownership before each new code change. |

Installation evidence is retained at `work/backups/s0-e176f6e-2026-11-02/INSTALLATION.md`, with the exact package, pre-flash archives, initial readback and final comparison. Preserve these files. Do not infer an installation from a newer branch HEAD or a mutable build output.

### Installed memory budget

These are linked-build figures for the installed package, not runtime CPU/stack measurements. A newer source candidate needs its own build and delta.

| Region | Used | Physical space left | Required reserve | Space after reserve |
|---|---:|---:|---:|---:|
| Application flash | 579,728 B | 1,836 B | Fits 581,564 B slot | 1,836 B |
| Ordinary RAM | 67,936 B | 5,792 B | 4,096 B | 1,696 B |
| Pool + range Z | 350,796 B | 17,844 B | 8,192 B | 9,652 B |
| Hot audio code | 11,048 B | 1,464 B to `boot_info` | 228 B | 1,236 B |
| Range X | 31,584 B | 928 B | Closure/placement checks | 928 B |

S0a/S0b recovered 6,688 B of pool against the previous installed build. **Flash remains the constraint.** Do not count RAM savings as flash savings. Commit `dd2acd5` reports a 579,856 B candidate (+128 B); that report is separate from the retained installed-build measurement above.

## Engineering work, in order

| ID | Work | State and completion condition |
|---|---|---|
| E0 | Consolidate administration | Documentation reorganization completed in this pass: one current roadmap; musical concepts and historical receipts separated. No firmware cleanup is implied. |
| E1 | Find useful memory savings | Active review across inherited DUELL/Felucca code, not only TIDE. Start with the measured inventory below; distinguish working RAM from program storage. Trace temporary-buffer lifetimes and identify removable duplication. Report measured savings separately from estimates. |
| E2 | Simplify inherited code in focused pieces | First local slice complete: three knob-navigation paths share the GLO page/history/detent-suppression transition; target selection and ordering stay at their callers. Focused input checks pass and the combined build saves another 48 B. Further cleanup remains evidence-led. |
| E3 | Check the code being changed (former T0) | Existing test repair continues independently. Run or repair the checks needed for each memory/cleanup change; do not require the whole legacy suite to be repaired first. `backup`, `patch` and `patchraw` now have recorded repairs. Keep unrelated inherited failures visible and do not weaken checks to hide defects. |
| E4 | Implement the best supported memory reduction | First local slice complete: lossless factory-pair packing saves 1,888 B net application flash; combined with E2, 1,936 B. No persistent RAM increase or approximation. OTA buffer reuse remains an estimated 512 B pool saving, unimplemented and parked. Compare further candidates separately. |
| E5 | Retire obsolete workspace artifacts | Inventory exists in `work/housekeeping-2026-11-02/REPORT.md`; it is dated evidence, not deletion authority. Identify owners and active consumers; retain packages, backups, listening references and necessary logs. Archive eligible managed worktrees through their lifecycle tools. No broad deletion or worktree removal is part of this pass. |
| U1 | A few free voice and pattern slots | Owner request, under feasibility review. Study four user voice slots and four user pattern slots as an initial size, not an agreed limit or implemented capacity. Preserve all current factory entries and existing user records; show unused user slots clearly. Measure code/RAM/storage cost and handle slot numbering, editor/backup and existing-record compatibility before changing a layout. This is separate from improving the existing overwrite workflow. |

The last documented broad test run is at `bd339c9`: **65 pass / 57 fail (including an FM6 timeout) / 12 skip**. The S0 merge separately reports **65 / 58 / 11**. These are revision-specific snapshots, not current whole-suite totals. Later individual repairs do not justify inventing a new aggregate. See [the baseline](JAM11_TEST_BASELINE.md); use affected checks for the next coherent change.

### Memory and inherited-code review: first findings

The local linked firmware was inspected during source review at `52c6604`; object sizes were checked against the current definitions. Artifact SHA-256: `d33ad8c98e07ff4fd7c4eb4c9c38ab05d9fa440b88451d41d945db430b1f3b1b`. This locates review targets; it is not a new clean-build or hardware measurement.

| Area | Observed allocation / structure | Practical next check |
|---|---|---|
| Delay (`fx.c`) | 131,072 B working RAM; both sides already share this line | Trace duration/bandwidth needs before changing allocation. A smaller line with different processing could change the sound; it is not a free saving. |
| Screen canvas (`gfx.c`) | 59,520 B working RAM, 240 × 124 RGB565 pixels | Investigate drawing smaller strips with the same result. Direct pixel access in UI modules must be accounted for before changing the canvas layout. |
| Performance capture (`perform.c`) | 32,768 B working RAM | Establish when it must stay live and which effects depend on it. Removing or shrinking it would change available performance behavior. |
| Engine state (`engines.c`) | 25,776 B working RAM for two 12,888 B regions | Engine memory is already shared within each part; PHYS determines the largest region. Check actual state requirements, not a fictitious separate allocation per engine. |
| Temporary buffers (`ota.c`, `project.c`, `storage.c`) | Update staging has a 1,024 B header, two 512 B blocks and a decode buffer; project staging and storage staging are 3,840 B each | Trace overlapping use and every update/error path before considering shared space. Project, multi and editor backup already share some staging. Do not assume two similarly sized arrays are redundant. |
| Factory content (`sounds.c`, `multi.c`) | 9,642 B sound parameter pairs + 9,625 B multi records in program storage | Try lossless packing with bounded decoding; the previous audit's compression figures are gross estimates, before decoder overhead and runtime scratch. |
| FM6 lookup (`fm6_core.c`, `gen_tables.py`) | 4,100 B in program storage | A 257-entry candidate would remove 3,072 B of table data, but affects pitch/gain math. Treat it as an approximation requiring validation, not code deletion. |
| UI/input/editor (`ui_graph.c`, `ui_input.c`, `editor.c`) | Large handlers; much of the firmware is compiled as one ordered set of included C files | Review responsibilities, shared-state access and duplicated branches. Large compiled functions are leads for inspection, not proof of waste. Moving code into separate files alone need not save memory. |

The review and planning passes changed no firmware. The subsequent supervised batch now meets the first action-plan milestone locally. From the freshly built `7c22b13` baseline, application flash falls **579,856 → 577,920 B**, leaving **3,644 B** of the 581,564 B slot. Ordinary RAM remains 67,936 B; pool+Z 350,796 B; X 31,584 B; hot audio code 11,048 B. Placement, reserves, call reach and X closure pass. The off build falls 556,112 → 554,144 B; the original off hash was reproduced before recording the explained shared-code change.

All original factory pairs and metadata were compared with the retained baseline header; actual C decoding matches all 128 voices. Strict `bank`, `tidekeep` (72 checks), `tidecontrols` (121), `knobjump` (6) and `sidechain` (17) pass on the combined source. The exact retained package passes the pinned CPU emulator's boot/LCD/MIDI-note/bounded-audio smoke. Peak stack, real-device CPU/UI/clock/audio and installation are not established by these checks. Receipt: `work/outputs/maintenance/run-20261102-1922/SUPERVISOR.md`.

For U1, source inspection confirms factory voices are compiled separately (`sounds.c`); the 128 visible slots currently show a stored record or fall back to a factory sound (`upreset.c`). Additional user entries need not remove factory content, but numbering and physical record capacity must be reconciled. Patterns are 32 prepopulated entries in three 11/11/10-slot objects (`patslot.c`); simply raising the count would not safely preserve the stored layout. A free user area must be an explicit feature, not a relabelled factory slot. No capacity increase or layout change has been implemented.

P4 recommends separate FACTORY and USER browsing while retaining existing record IDs/formats: study voice IDs 124–127 and pattern P29–P32 as four initially visible USER rows, alongside every existing saved record. Factory loads use compiled data independently. An occupied proposed USER row stays occupied; four blank rows are not guaranteed on a populated store. This is a reviewed design proposal, not an implemented feature or an agreed final UI/capacity. See the P4 receipt in the batch evidence directory.

### Separate storage backlog — outside the current focus

| Concern | Current evidence | Required next result |
|---|---|---|
| Live End-B restart preservation | Both installations changed Part 2's current-session End B on reboot; a music-only restore recovered it | Reproduce with a fixed current-music record; trace export, autosave, boot import and rebase order. Fix the demonstrated cause separately from patch-record rewriting. |
| Saved End B during record rewrites | `dd2acd5` repairs rename/recategorization and editor FM6 PUT; newer than installed firmware | Review current regression evidence and include in the next checked candidate. Do not label it installed. |
| TIDE preferences backup | `tide_store.c` stores object 48 independently; `editor_backup.c` does not archive it | Specify compatible backup/restore coverage, including safe absence in older archives, before extending preferences. |
| Settings record growth | `tide_store_load()` rejects a record shorter than the requested field count | Preserve existing fields and default only newly absent fields in any future extension; cover old, new, malformed and unsupported versions. No new fields in this maintenance pass. |
| Logical versus physical patch capacity | INFO advertises 128 slots; TIDE uses 21 chunks × 6 worst-case records = 126 guaranteed records | Reconcile actual capacity/failure behavior and documentation. Keep the accepted format/layout unless a separate change is justified; remove blanket "every slot always fits" claims only with precise replacement wording. |
| Pattern save entry points | Panel code distinguishes pattern SAVE from MULTI SAVE; MULTI SAVE is disabled while playing. Owner reports their sequence-save workflow requires stopping. S0's lower storage layer handles pending saves independently | Document the exact visible entry point before prescribing a live test. Cover stopped save/load and lower-layer pending-write integrity in software; do not require an inaccessible panel action for acceptance. |

Relevant source: `patslot.c` (one object buffer + metadata; three 11/11/10-slot flash objects), `storage.c` (A/B sector commit), `project.c` (current-music serialization/autosave), `upreset.c` and `up_fm6.c` (patch records), `editor_backup.c` and `jam/jam_dev.py` (archive boundary), `tide_store.c` (separate preferences). These files are under `felucca-base/firmware/src/` except the host helper. Production and host-only pattern storage differ; ordinary host RAM tests alone do not establish real-flash behavior.

## Working standards for this programme

- Follow the owner's engineering standards. Inspect the affected source, instructions and concurrent diff before changing code; do not add another instruction file or duplicate policy document.
- Prefer existing components, fixed-size bounded state and clear ownership. Main-loop planning stays out of the audio ISR. Buffer sharing requires a demonstrated lifetime/exclusion contract and recovery behavior.
- Preserve formats, user data, KEEP/UNDO and manual takeover unless the owner authorizes a change. Treat ephemeral JOURNEY history and intentional saved records as different lifetimes; capturing every random destination is not a requirement.
- Use the narrow existing checks that cover each change. Storage changes need meaningful failure/round-trip coverage; DSP changes need defined arithmetic and relevant strict fixtures. Preserve unrelated baseline failures visibly.
- After a coherent firmware change, build the affected target, record flash/RAM/hot deltas and verify placement, call reach, range-X closure and existing reserves. Do not lower reserves to fit a feature. Update the TIDE-off baseline only for explained shared-code changes.
- Keep source verification, emulator checks, installation/readback and owner hardware observations distinct. No exhaustive owner checklist for a documentation or memory-accounting change.
- Each completed entry needs a source revision, relevant evidence, remaining limitation and installation state. Keep detailed logs outside this roadmap. Publishing, another flash and destructive cleanup require their applicable authorization.

## Deferred musical work

| Existing ID | Status |
|---|---|
| M0 | Owner marked complete; original reference evidence has not been linked |
| R0 / M1 / M2a | Implemented and installed. M1 host A/B accepted; M2 ordinary-chord listening comparison remains open. No new whole-device re-acceptance requested here. |
| M3 | Recent JRNY destination memory; proposed ephemeral state |
| M4 / M9 | Overlapping longer-form development/arrangement concepts; keep one eventual policy; owner decisions remain open |
| M5 / M10 | Loudness/transition measurement before any smoothing or calibration change |
| M7 / M8 | Syncopation, polymeter, turn-taking and echoes; deferred proposals |
| CHANGE / ENERGY / ARC | Owner-requested intent controls; exact open choices retained with the design notes; settings compatibility first |
| M6 | Combined device/music acceptance when relevant changed behavior is ready |

See [the design notes](design/TIDE_MUSICAL_DESIGN.md) for details and parked decisions. Their estimates are provisional. Resume these features after the maintenance baseline is dependable and the owner chooses to return to them.
