# DUELL with TIDE roadmap

Updated 3 November 2026 (evening: status reconciled with the builds flashed that day) after the full firmware source audit at `6af479d` and the owner's storage clarification. **Audit complete; the verified bounded corrections F01–F04 are done and flashed (branch `polish-all`, `187e9d0`); the test-tree cleanup is done.** The storage goal is a small convenience: 4–8 clear USER destinations for new voices and patterns, with factory material still accessible. Existing overwrite saving remains acceptable. Exact restart continuation of live morphed sounds is not required; current audio polish is accepted. The first memory-saving milestone is complete in source. Further memory work should serve a measured feature budget, and broad cleanup/storage work is not an indefinite gate to musical work. The DUELL 1.1 migration is complete.

**Parallel work:** the test-tree cleanup (T0) is finished: full `sh jam/check.sh` is **112 pass / 0 fail / 25 skip** (the 25 skips need `fork()` or an instruction counter and pass in the Linux host-test CI; the boot pack added the `bootpack` group). Two firmware changes are in progress in other worktrees: the MULTIS preview (U3) and the GLO/MIXER part-select fix. Each firmware change still needs its affected checks.

This is the current delivery/status document. Detailed [musical proposals](design/TIDE_MUSICAL_DESIGN.md) and the [historical roadmap and receipts](history/TIDE_ROADMAP_2026-11-02.md) are references, not competing work queues. Keep the [test baseline](JAM11_TEST_BASELINE.md) as the record of test results.

The [maintenance action plan](ACTION_PLAN.md) records the first completed batch from pinned source `7c22b13`: lossless factory-voice packing and one shared knob-navigation helper are now committed as `61eb641` and `89a9a77`; buffer and free-slot investigations are reviewed. The combined change saved 1,936 B flash in its own build; both commits are part of every package flashed since. These historical build measurements do not describe a newer package.

## Baseline and evidence

| Item | Verified state |
|---|---|
| Active checkout | `work/wt/duell-jam11`, branch `jam11` (integration branch; firmware work runs in other worktrees such as `multis-preview` and `glo-mixer-part`) |
| Installed source | `187e9d0` (branch `polish-all` = jam11 `2d3e619` with the boot pack `ecd2918`, F01–F04 and the lx-sound/lx-parts test work); flashed 3 November 2026 19:42. Before it: boot pack `ecd2918`, flashed 18:31; before that `e176f6e` (2 November) |
| Installed content | R0, M1, M2a, S0a/S0b, DSP arithmetic fixes, TIDE startup-write fix, boot pack (LZMA RAM images), F01–F04 audit fixes, the shortened GLO > MIDI LAYER note |
| Package SHA-256 | `7795e77798d053177a4d0b099f0754701e16e308c92fbe22fd54913abac41b30` (`duell-1.1.fwsc`, `work/checkpoints/DUELL-2026-11-03-polish-187e9d0/`); `e176f6e` was `ba8fdaf9252a0cf64f5230472ce10e738801b22926b2619e46cb81216648a8aa` |
| Device/data evidence | FM-1_901 / DUELL 1.1. `e176f6e`: reboot verified; all 16 archived objects equal after restoring current music. Boot pack: ABOUT shows RAM CODE OK 57 KB. Pre-flash backups `work/backups/DUELL-backup-2026-11-03-1831.json` and `-1941.json`. Separate TIDE preferences are outside that archive |
| Owner playback report | "sounds OK I guess"; basic playback observation, no controlled A/B comparison |
| Newer committed source | `dd2acd5`: patch rename/recategorization and editor FM6 PUT preserve saved End B; subsequent test repairs at `379e844` and `52c6604`; not part of the installed package |
| Maintenance source and retained candidate | P1/P3 committed as `61eb641` / `89a9a77`. Retained package was built from `7c22b13` plus that reviewed patch: SHA-256 `4852f39142d3f9b3de6acd961aee6df96208f5f780d6685632edadc7d0731612`; source manifest and artifacts at `work/outputs/maintenance/run-20261102-1922/`. Not installed. |
| Review source | `6af479d`, branch `jam11`; clean at the start of this documentation review. Later commits do not establish a newer installation. |
| Concurrent work | Claude owns test-tree cleanup. Preserve that work and recheck status/file ownership before a firmware change. |

Installation evidence for `e176f6e` is retained at `work/backups/s0-e176f6e-2026-11-02/INSTALLATION.md`, with the exact package, pre-flash archives, initial readback and final comparison; the 3 November packages are in `work/checkpoints/DUELL-2026-11-03-bootlz-ecd2918/` and `DUELL-2026-11-03-polish-187e9d0/` (with `.INSTALL.md`). Preserve these files. Do not infer an installation from a newer branch HEAD or a mutable build output.

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

## What the instrument already supports

| Capability | Useful behaviour and boundary |
|---|---|
| Timed evolution | ONCE, LOOP, DRIFT, WNDR and JRNY move SOUND, SEQ or BOTH over 1–64 bars, with endpoint WAIT and manual takeover. WNDR develops bounded variations of the source. |
| Two-part coordination | Peer-attack prediction uses playback timing; phrase edits alternate between parts; sound choices favour contrast. This is timing/parameter-based coordination, without sustained-harmony or effects-tail analysis. Purposeful shared attacks remain musically useful. |
| JRNY palette | Factory destinations within the current engine; original-category preference with category fallback and exclusions for the current/peer targets. No cross-engine journey and no recent-destination ring. |
| Chord movement | M2a improves whole-loop upper-voice movement in ordinary chord sequences. It does not rewrite ARP SEQ's held chord, infer a chord progression or coordinate harmony between parts. Listening comparison remains open. |
| Polymeter | Per-part lengths already drive ordinary and ARP SEQ playback. Try owner-set 12/16 and 15/16 cycles against a steady 4/4 reference before adding automatic length changes. Physical clock/listening acceptance remains open. |
| Capture | KEEP rebases the heard sound/pattern for subsequent saving. Factory fallbacks obscure unused USER addresses; U1 should expose a few clear save destinations while retaining factory access. Stopped saving is sufficient. Exact live-session morph restart continuity is outside the owner's requirements. |

Source boundaries: `morph_glide.c`, `tide_ui.c`, `seq_rhythm.c`, `seq_phrase.c`, `seq_dialogue.c`, `seq_voicelead.c`, `seq.c` and `morph_complement.c` under `felucca-base/firmware/src/`. The [current limits](KNOWN_LIMITS.md) distinguish the TIDE fork from inherited DUELL notes.

### MULTI contents and pattern replacement (reviewed 3 November 2026)

**Current behaviour:** a MULTI recalls both parts' sounds, patterns, arp settings, mixer, effects and sound-morph End B. Every load replaces both working patterns, including with empty patterns, and clears the previous undo copy. Loading while playing keeps the transport running. Saved P01–P32 pattern slots are separate and are not erased by the load. **SEQ KEEPS RUNNING currently protects patch changes, not MULTI loads.** The owner reported M07 clearing the playing pattern; U2 below records the requested correction as planned, not implemented.

This is the compiled factory set checked at `bee34f9`, not a device readback; saved user overwrites can differ. “Empty” means no stored note steps, even when a classic arp supplies repeating notes from played keys. “8 / 6” means an eight-step loop with six populated note steps. SEQ uses the stored pattern as the arp; where no arp is listed, it is off. Delay and reverb columns name the stored effect types; chorus and per-part sends are also stored.

| Multi | BPM | Part 1: sound, pattern and arp | Part 2: sound, pattern and arp | Delay / reverb |
|---|---:|---|---|---|
| M01 REESE SWAMP | 124 | REESE BASS: empty | SQUELCH: 8 / 6; SEQ 1/16 | Digital / Room |
| M02 NIGHT ORGAN | 121 | PEDAL BASS: 8 / 5; SEQ 1/8 | DRAWBARS: 8 / 2; SEQ 1/8 | Digital / Room |
| M03 PAD SPLIT | 97 | EVOLVING PAD: empty | EMBER GLOW: empty | Random / Resonator |
| M04 DUB CHORD | 117 | BIG STAB: 16 / 2; SEQ 1/16 | TIDAL SUB: 16 / 4; SEQ 1/16 | Digital / Plate |
| M05 WHISPER TIDE | 77 | DIGITAL PAD: empty; notes latch | WHISPER PAD: empty; random arp 1/1, two octaves | Random / Plate |
| M06 TINE STRINGS | 77 | STRING MACH: empty | TINE ROOM: empty | Digital / Plate |
| M07 FINAL REEL | 71 | BRASS PAD: empty; power chords | STRING MACH: empty; transpose +12 | Ping-pong / Shimmer |
| M08 HAZY TENSOR | 117 | LADDER BASS: empty | GEARBOX: empty; random arp 1/16 triplets | Octave-up / Plate |
| M09 ACID ETHER | 120 | BREADBIN: empty; up/down arp 1/8, two octaves | LADDER BASS: 16 / 13; SEQ 1/16 | Ping-pong / Hall |
| M10 WIRED SNAIL | 120 | SFORZANDO: empty; transpose -24 | GLASS SHARDS: 16 / 16 chromatic; also up/down arp 1/16, three octaves | Ping-pong / Hall |
| M11 EERIE SIGNAL | 120 | VESPERS: empty; diatonic triads | JAZZ CLUB: 16 / 16 chromatic; also random arp 1/8 | Ping-pong / Hall |
| M12 DEAD BUFFER | 120 | FROZEN AIR: empty; power chords | MODEM: 16 / 16 chromatic; also up/down arp 1/16, three octaves | Ping-pong / Hall |
| M13 COLD CHAPEL | 71 | EVOLVING PAD: empty; notes latch | SINGING BOWL: empty; up arp 1/2; latched power chords | Random / Resonator |
| M14 GARGLE ACID | 127 | GROWL BASS: empty; transpose -24 | SQUELCH: 16 / 10; SEQ 1/16 | Digital / Plate |
| M15 WOOD & SKIN | 107 | MARBLE RUN: empty; up arp 1/8, two octaves | TABLA TALK: 16 / 9; SEQ 1/16 | Digital / Room |
| M16 STRESS TEST | 137 | PAN WHISTLE: empty | DIGITAL PAD: empty; up arp 1/16, two octaves | Random / Shimmer |

M03, M05, M06, M07, M08, M13 and M16 have empty patterns on both parts. M10–M12 each contain the same rising C4–D#5 chromatic pattern on part 2, accented every fourth step, alongside their classic arp settings. All sixteen records store LAYER mode, no keyboard split (including M03 despite its name), straight global swing (50%) and no automation/parameter-lock events. MODE RECALL controls whether stored mode/crossfade settings are applied.

**Evidence and limits:** `multi.c:mu_unpack` clears steps absent from the record's map; `project.c:project_restore_runtime` copies those steps into the live tracks and clears undo. The inventory loaded all sixteen records through the current host UI harness and independently compared all 32 parts' pattern data with `tools/factory_multis/M01.bin`–`M16.bin`. A focused M01 → M07 probe reproduced replacement with TIDE on/off and SEQ FOLLOWS PATCH/KEEPS RUNNING: both live patterns became empty, transport stayed playing, all host pattern-slot bytes stayed unchanged, and saved P32 reloaded its six notes. This verifies source/host behaviour, not physical flash, audio or clock timing. Workspace evidence: `work/audit/m07-pattern-2026-11-03/REPORT.md`, `FACTORY_MULTIS.md`, `inventory.json` and the probe sources. No firmware correction or installation was performed in this investigation.

**U2 planned direction (owner follow-up):** adding appropriate sequences to the factory multis without them is the preferred content direction. The keep-current-patterns load option remains a separate possible improvement. Neither is implemented; sequence additions and the U3 preview below can be developed as bounded changes without requiring that load option first.

- **A — Keep current patterns:** provide an explicit keep-current-patterns MULTI load choice while retaining full setup recall. Preserve both parts' steps and timing; define the matching arp/ARP SEQ, automation/locks and TIDE sequence-state policy so new sound settings cannot silently disable or rewrite the retained groove. Resolve its relationship to SEQ KEEPS RUNNING. Full recall must still restore deliberate empty patterns; do not silently reinterpret every empty record as “keep.”
- **B — Give every factory multi an appropriate sequence (preferred):** first add tailored arrangements to **M03 PAD SPLIT, M05 WHISPER TIDE, M06 TINE STRINGS, M07 FINAL REEL, M08 HAZY TENSOR, M13 COLD CHAPEL and M16 STRESS TEST**, whose two stored patterns are empty. Each multi should offer a useful sequenced arrangement; both parts need not contain notes when one has a deliberate live-keyboard or classic-arp role. Then assess all sixteen; retain useful existing sequences and review M10–M12's shared chromatic line for musical suitability. Choose per-part roles, pitches/chords, register, rhythm, length, tempo, gate, arp mode and hold together; avoid adding the same generic line everywhere. Judge the parts together and across the playable keyboard, including transposition, effects and morph behaviour. Verify PLAY starts the intended arrangement and that keyboard input/ARP SEQ transposition remain useful. M16 must retain its stress-test purpose. Develop reviewable revised factory copies and retain original records; preserve user overwrites. This route still replaces the current patterns on MULTI load, so the replacement behaviour must remain explicit.

For either route, verify the affected load/save and panel/editor/MIDI paths, preserve saved P01–P32 and format compatibility, coordinate affected tests with Claude, and measure the linked memory budget. Route A needs live transition checks; route B needs actual audition/comparison and device playback before musical acceptance. Record the selected route and source/device evidence when implementation starts.

**U3 — MULTIS browser with sound and pattern preview (owner proposal; planned):** reduce the current five-row list to three entries at the top, with the selected entry centred where possible. Underneath, reuse the compact two-part pattern layout from the owner's HOME page 3 design, with each part's sound name visible in its part colour and a small ARP/SEQ mode indicator. Empty patterns must explicitly show NO SEQUENCE while still identifying an active classic arp. Locate the exact HOME page 3 reference before fixing dimensions; it was not identifiable in the inspected checkout. Keep labels and both previews legible on the 240 × 240 display and retain clear LOAD/SAVE controls; validate the final arrangement in a rendered screen before implementation acceptance.

The preview follows the highlighted slot's actual stored record or factory fallback **before LOAD**, without changing the current sounds, patterns, transport, morph state or saved data. A stored user overwrite must preview its own contents. Distinguish the browsing selection from the currently loaded multi. Reuse existing drawing/decoding support with bounded memory; refresh preview data when the selection or record changes rather than reading/decoding every frame. Check scrolling, end slots, empty/damaged records, long sound names, overwrite refresh and the running-transport flow. Inspect the screen in the application/emulator, measure firmware memory and check physical readability/playback before device acceptance. This preview makes replacement visible; U2 still governs sequence content or preservation.

## Engineering work, in order

| ID | Work | State and completion condition |
|---|---|---|
| A0 | Full firmware source audit | Complete: three GPT-6 Luna agents plus coordinator reviewed all 164 tracked firmware files / 66,036 lines; reconciled 2,867 function/macro/assembly units. Reports, exact coverage and findings are retained in `work/audit/firmware-review-2026-11-03/AUDIT.md`. Four isolated host probes verify narrow source conditions; no firmware build/test-tree/device change or physical acceptance. |
| A1 | Correct verified control/note/persistence edges | F01–F04 done and flashed 3 November 2026 19:42 as `polish-all` `187e9d0` (not yet on `jam11`): F01 extended editor lock operations (MOTION ops 9..11) no longer swallowed by a query guard (`61d3551`); F02 ARP note 0 no longer collides with the empty sentinel (`f0fa950`); F03 the last 16 USB MIDI-out slots are reserved for note-offs and CCs (`1cf1bbb`); F04 failed TIDE preference writes are paced (`ddb6b09`). F05, the rarer core-job wrap/sentinel issue, is not done. Lower-priority undo/developer-parser/UI-consistency items remain in the audit, not a broad refactor queue. |
| E0 | Consolidate administration | Documentation reorganization completed in this pass: one current roadmap; musical concepts and historical receipts separated. No firmware cleanup is implied. |
| E1 | Find useful memory savings | First flash-saving milestone achieved. Select another bounded candidate when U1 or a chosen musical slice needs measured space; factory-multi packing is an unmeasured follow-up. Distinguish flash from RAM and estimates from linked savings. |
| E2 | Simplify inherited code in focused pieces | First local slice complete: three knob-navigation paths share the GLO page/history/detent-suppression transition; target selection and ordering stay at their callers. Focused input checks pass and the combined build saves another 48 B. Further cleanup remains evidence-led. |
| E3 | Check the code being changed (former T0) | Done: the test-tree cleanup is complete (full check 112 pass / 0 fail / 25 skip; see [the baseline](JAM11_TEST_BASELINE.md)). Use the affected checks for each firmware change and keep any new failure visible. |
| E4 | Implement the best supported memory reduction | First local slice complete: lossless factory-pair packing saves 1,888 B net application flash; combined with E2, 1,936 B. No persistent RAM increase or approximation. OTA staging now shares one 1 KiB pool space for the UFW header and the two 512 B blocks: pool -1,024 B, image +36 B (host group `otareuse`; project and storage staging not touched). Compare further candidates separately. |
| E5 | Retire obsolete workspace artifacts | Inventory exists in `work/housekeeping-2026-11-02/REPORT.md`; it is dated evidence, not deletion authority. Identify owners and active consumers; retain packages, backups, listening references and necessary logs. Archive eligible managed worktrees through their lifecycle tools. No broad deletion or worktree removal is part of this pass. |
| U1 | 4–8 clear user save destinations | **Implemented on branch `user-slots`, not flashed:** voice slots 125–128 (records 124–127) and patterns P29–P32 (slots 28–31) are USER slots `U1`–`U4` (`USER EMPTY` until saved); the displaced factory sounds/patterns stay as read-only places after them (`#125`–`#128`, `P29`–`P32`). Plan: bounded planned convenience. Prefer four initial voice and pattern destinations; allow up to eight if the same simple approach fits. Reuse existing record addresses and overwrite flow, label unused USER rows clearly, and retain access to compiled factory material. A full new browser/protocol or physical capacity increase is not required. Preserve occupied records; verify stopped save/load/reboot and existing editor compatibility. This is not a gate to musical work. |
| U2 | Add appropriate factory sequences; consider pattern-preserving recall | Owner-requested; planned, not implemented. Preferred direction: tailored sequenced arrangements for the seven multis with both patterns empty, retaining deliberate live/arp roles, useful existing sequences, originals and user overwrites. Review all sixteen for musical suitability. The keep-current-patterns load option remains a separate possible improvement. Full recall still replaces patterns. Complete the source, memory, interaction and actual listening/device checks detailed above. |
| U3 | Show sounds and patterns while browsing MULTIS | Owner-requested; in progress (worktree `multis-preview`). Three list rows above a two-part preview based on HOME page 3, with sound names, ARP/SEQ indicators and explicit NO SEQUENCE states. Preview the highlighted record without loading it. Locate the exact design reference, verify layout and browsing flows, and measure memory before integration; physical readability/playback acceptance remains required. |

The latest full run is **112 pass / 0 fail / 25 skip** (`sh jam/check.sh` on Windows; the 25 skips run in the Linux host-test CI), recorded in [the baseline](JAM11_TEST_BASELINE.md). Earlier snapshots (80/34/22 at `4443c13`, and the `bd339c9`/S0 figures below) are history. The totals are revision-specific.

### Memory and inherited-code review: first findings

The local linked firmware was inspected during source review at `52c6604`; object sizes were checked against the current definitions. Artifact SHA-256: `d33ad8c98e07ff4fd7c4eb4c9c38ab05d9fa440b88451d41d945db430b1f3b1b`. This locates review targets; it is not a new clean-build or hardware measurement.

| Area | Observed allocation / structure | Practical next check |
|---|---|---|
| Delay (`fx.c`) | 131,072 B pool; both sides already share this line | Trace duration/bandwidth needs before changing allocation. A smaller line with different processing could change the sound; it is not a free saving. |
| Screen canvas (`gfx.c`) | 59,520 B pool, 240 × 124 RGB565 pixels | Investigate drawing smaller strips with the same result. Direct pixel access in UI modules must be accounted for before changing the canvas layout. |
| Performance capture (`perform.c`) | 32,768 B pool | Establish when it must stay live and which effects depend on it. Removing or shrinking it would change available performance behavior. |
| Engine state (`engines.c`) | 25,776 B pool for two 12,888 B regions | Engine memory is already shared within each part; PHYS determines the largest region. Check actual state requirements, not a fictitious separate allocation per engine. |
| Temporary buffers (`ota.c`, `project.c`, `storage.c`) | Update staging has a 1,024 B header, two 512 B blocks and a decode buffer; project staging and storage staging are 3,840 B each | Trace overlapping use and every update/error path before considering shared space. Project, multi and editor backup already share some staging. Do not assume two similarly sized arrays are redundant. |
| Factory content (`sounds.c`, `multi.c`) | Before P1: 9,642 B sound pairs; P1 packs that payload to 7,553 B. Multi records were 9,625 B at the inventory checkpoint | Sound packing is complete, with 1,888 B net linked saving. Investigate multis separately if more flash is needed; gross compression estimates exclude decoder/scratch overhead. |
| FM6 lookup (`fm6_core.c`, `gen_tables.py`) | 4,100 B in program storage | A 257-entry candidate would remove 3,072 B of table data, but affects pitch/gain math. Treat it as an approximation requiring validation, not code deletion. |
| UI/input/editor (`ui_graph.c`, `ui_input.c`, `editor.c`) | Large handlers; much of the firmware is compiled as one ordered set of included C files | Review responsibilities, shared-state access and duplicated branches. Large compiled functions are leads for inspection, not proof of waste. Moving code into separate files alone need not save memory. |

The review and planning passes changed no firmware. The subsequent supervised batch now meets the first action-plan milestone locally. From the freshly built `7c22b13` baseline, flash falls **579,856 → 577,920 B**, leaving **3,644 B** of the 581,564 B slot. RAM remains 67,936 B; pool 350,796 B; range X 31,584 B; hot code 11,048 B. Placement, reserves, call reach and X closure pass. The off build falls 556,112 → 554,144 B; the original off hash was reproduced before recording the explained shared-code change.

All original factory pairs and metadata were compared with the retained baseline header; actual C decoding matches all 128 voices. Strict `bank`, `tidekeep` (72 checks), `tidecontrols` (121), `knobjump` (6) and `sidechain` (17) pass on the combined source. The exact retained package passes the pinned CPU emulator's boot/LCD/MIDI-note/bounded-audio smoke. Peak stack, real-device CPU/UI/clock/audio and installation are not established by these checks. Receipt: `work/outputs/maintenance/run-20261102-1922/SUPERVISOR.md`.

For U1, factory voices are compiled separately (`sounds.c`); the 128 visible slots show a stored record or a factory fallback (`upreset.c`). Patterns are 32 prepopulated entries in three 11/11/10-slot objects (`patslot.c`). The owner accepts existing overwrite saving and only wants a few obvious places for new material. Reusing 4–8 existing destinations with separate factory access is preferable to increasing counts or migrating layouts. No capacity increase or layout change has been implemented.

P4's earlier FACTORY/USER browsing proposal is a reference, not a requirement for a large UI/editor redesign. It studies voice IDs 124–127 and pattern P29–P32 as four initial USER destinations; the exact 4–8 addresses/labels should follow the smallest compatible implementation. Occupied destinations remain occupied and retain the existing overwrite choice; do not move/erase data merely to make them blank. No feature is implemented yet. See the P4 receipt in the batch evidence directory.

### Storage priorities and parked observations

| Concern | Current evidence | Required next result |
|---|---|---|
| Live End-B restart preservation | Earlier readback found Part 2's live destination changed after restart; raw evidence is retained | Parked by the owner: no requirement to preserve exact ongoing morph state or add complex session capture. Do not make it a prerequisite. Ordinary intentional saved-record integrity still applies. |
| Saved End B during record rewrites | `dd2acd5` already repairs rename/recategorization and editor FM6 PUT; newer than last recorded installation | Source-complete bookkeeping, not a new feature queue. Retain the existing fix and stopped-save restrictions; no live-save workflow or additional project is requested. |
| TIDE preferences backup | Settings already persist between boots in object 48; the editor archive omits that object | Useful bounded follow-up: archive/restore the TIDE configuration between flashes, accept older archives without it, and preserve existing fields. Do not turn this into a full live-morph/session capture. |
| Settings record growth | Current seven-field record works; the loader rejects a record shorter than a future requested count | Treat as compatibility bugfixing when adding fields: preserve stored fields and default only missing new fields. Cover old/current/malformed/unsupported records. No record redesign or new settings fields are requested now. |
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

## Musical delivery order

This is the rationalized plan, not a claim that new musical firmware is implemented or that open performance choices are settled. The source audit is complete and A1 lists bounded correction candidates; U1 is a small planned convenience, not a prerequisite to musical work. Evaluation rows can proceed in parallel without broad test cleanup as a gate. Current audio polish is accepted: M5/M10 do not create an obligatory re-polishing exercise. Retain M0 as owner-complete; R0/M1/M2a remain implemented and installed, with M1 host A/B accepted and M2's focused listening comparison optional when relevant.

| Order / existing ID | Deliverable and feasibility | Completion decision |
|---|---|---|
| Optional evaluation: M2a | Ordinary triad/four-note loop when that behaviour is being judged; ARP SEQ cannot demonstrate its benefit. | No prerequisite or mandatory listening project. Avoid expanding harmonic scope from mathematical movement alone. |
| Conditional: M5 + M10a | Current audio polish is accepted. Investigate matched-input level/slow-glide behaviour only for an identified defect/regression or a chosen transition change. | No generic recalibration/smoothing project. Keep existing protection and correct a demonstrated cause narrowly; M10b/c/d are conditional. |
| Evaluation: M7d baseline | Use existing unequal-length controls: 12/16, then 15/16; straight timing before swing/ARP SEQ. | Hear useful motif rotation and verify the full joint cycle. Automatic shortening remains uncosted and needs a separate length/return/KEEP policy. |
| First musical candidate: M8a | Alternating variation turnarounds in MUSIC WNDR/JRNY with VARY 4 + 8. Small scope, but code size is unmeasured. | Keep VARY ALL and manual/ONCE/LOOP behaviour. Retain only if alternating bars improve interplay in listening. |
| Next: M7a/b | Soft offbeat/anticipation weighting for eligible movable notes. Use actual musical phase; explicitly leave unsupported grids unchanged in an initial slice. | Preserve anchors/continuations and useful shared attacks. An anchor-only pattern cannot be made syncopated by a move-only policy. |
| Conditional: M3 | Four-target ephemeral history with bounded relaxation for small palettes. | Count eligible destinations first. 61 of the bank's 66 engine/category groups have at most four sounds (99/128 voices), before endpoint/peer exclusions; history cannot promise broad exploration there. |
| Conditional: M8b, then rhythmic M8c | Main-loop follower masks, with cheap playback lookup; rhythmic echoes only after turn-taking proves useful. | Hear a leader and an audible relationship while retaining drive, live keys, held notes and tails. Scale-root/fifth pitch cadences stay deferred without chord context. |
| Later: one M4/M9 policy | Combine longer-form development/recovery into one musical-time planner. Preserve the requested harshness/busyness link as a bounded, scene-calibrated bias. | Begin with gradual development/recovery. Full stops need an explicit performance policy, audible benefit and a measured flash/CPU budget. No second competing scheduler. |
| Alongside accepted behaviour: CHANGE / ENERGY / ARC | CHANGE controls sequence development amount/rate; ENERGY biases character; ARC enables longer form. ROLL selects generation policies. | Add rows only with their useful behaviour and compatible settings loading/backup. Current behaviour remains the reference; drop ownership and exact ARC default remain open. |
| M6 | Combined acceptance for the behaviour actually changed | Keep source tests, emulator results, installation/data readback and physical clock/audio observations distinct. |

Use the completed audit's verified, useful corrections as the first A1 candidates. Keep U1/P7 to 4–8 straightforward save destinations and TIDE configuration backup to its own bounded change. M8a remains the first small musical feature candidate; neither exact restart continuity nor a general sound-polish project is a gate. Do not bundle the queue into one build. Cross-engine JRNY, autonomous harmonic rewriting, automatic loudness normalization and universal harshness classification have no supported near-term plan. Retain existing sounds/effects/performance capture. Detailed acceptance and unresolved choices live in [the design notes](design/TIDE_MUSICAL_DESIGN.md).
