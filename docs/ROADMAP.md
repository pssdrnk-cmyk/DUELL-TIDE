# DUELL with TIDE roadmap

Updated 5 November 2026. This is the current delivery and status document. The [musical proposals](design/TIDE_MUSICAL_DESIGN.md) and the [historical roadmap and receipts](history/TIDE_ROADMAP_2026-11-02.md) are references; the earlier baseline and evidence tables are kept in [history/ROADMAP_BASELINE_2026-11-03.md](history/ROADMAP_BASELINE_2026-11-03.md).

## Status

| Item | State |
|---|---|
| **Stable** | `stable-2026-11-03` (`187e9d0`): validated by the fork owner on the unit. It is from before the versions (the first versioned stable will be `1.0.0`). Package SHA-256 `7795e777…1b30`. |
| **Beta (last dated one)** | `beta-2026-11-04` (`77a6df2`): flashed to the owner's unit 2026-11-04, under test. U1–U3, the 32 extra sounds, new patterns and multi sequences, JRNY role families, live graphs, the TIDE SETTINGS rows STYLE, TURNS (LEAD / ECHO), POLY, SYNC and ARC. Package SHA-256 `d94ac77c…5d7c`. |
| **Next beta: `1.0.0-b1`** | The next build flashed (tag `tide-1.0.0-b1`, made when it is flashed; the file `TIDE_VERSION` holds `1.0.0-b1`). On `jam11`, the release candidate, for the owner's test: the TIDE step bars show what plays (`tidesteps`), STOP ends a latched arp (`stophang`), an FM6 voice under MORPH ends after its release (also in plain DUELL), the review fixes (a TIME change lands at the next turn, ARC 32–64 bars, PATTERNS preview = LOAD, …), the SAVE > MULTIS / PATTERNS redesign and a UI polish pass, and the rebuilt installer site with Stable / Beta channels. |
| **In progress** | The arp timing, TIDE page truth, FM6 re-strike and JRNY roles fixes are on the owner's unit (`6fbd939`, 4 November 2026). Merged since, not yet in any build: the delay-time glide and steadier clock tempo; the ENV SYNC release end; the load undo and list acceleration; guide and QUICKSTART corrections; MIDI Start on the first clock, arp velocity, the arp on the grid, truthful swing and PLAY waiting for the master's bar; the TIDE motif, resets and velocity shape; TIDE settings in every backup and STORE FULL; delay times on the grid and RESO in tune; GLIDE that follows its display; ECHO folded into LEAD; FM6 with 8 voices, UNISON level, DTUN and OKS; RES over the whole knob and chorus in chorus range; flat chord names; the master soft knee, SPACE 3 dB gentler, COMP, silent tails, the SVF low end and the CUT slew; dead code out and the TIDE core simplified; CI. Still to do: the rest of the planned tables below. |
| **Tests** | The last full `sh jam/check.sh` recorded here ran on the release candidate before the merges above: **143 pass / 0 fail / 1 skip** (benchrun needs termios; it runs in the Linux CI). Web: installer state, site build, install flow (headless, fake unit), editor web tests pass. `jam/check_off.sh`: DUELL_TIDE=0 matches `jam/off.sha256`, which every shared-code change re-pins on purpose (now `baff39b9…`). |
| **Flash** | Development line (5 November 2026) 576,204 B of the 581,564 B slot: **5,360 B free**. RAM .data+.bss 68,432 of 73,728 B; pool 322,880 of 368,640 B. Keep 0xEA000..0xFBFFF for future pattern / multi slots. |
| **CPU (host proxy)** | Within +116 basic blocks a half of the 3 November measurement on the worst scene (DUEL, 3 keys, both sequencers); the threshold for a closer look is about +1,000. Check ABOUT on the unit after flashing: code layout can move the device figure. |
| **GLIDE CPU fix (6 November)** | Active on `fix/w16-glide-cpu`: correct the shared blender's SRAM placement, cache WASH coefficients and remove redundant FM6 bridge preparation. Preserve calibrated b1 note capacity; the provisional extra 25% capacity margin is withdrawn. Software checks and both b2 images are being prepared. The owner's full multi/GLIDE sweep and normal-image acceptance remain pending; host timings do not establish the device fix. |
| **Development branch** | `jam11` (integration happens on a local `integ` branch and is pushed when it passes). Feature work runs in its own branches. Planned: rename `jam11` to `tide` (see [Planned work after the 4 November decisions](#planned-work-after-the-4-november-2026-decisions)). |
| **Parked as its own rollout** | U4, 24 multi slots: a tested implementation waits on branch `wip-24-multis` (see the engineering table). It follows the release candidate. |

Every flashed build has a section in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md); the README's [Builds](../README.md#builds-stable-and-beta) table says which is stable and which is beta.

## Versions

TIDE has its own version, `MAJOR.MINOR.PATCH`, in one line of the file [`TIDE_VERSION`](../TIDE_VERSION) at the root of the repository. DUELL's own `VERSION` (1.1, OSCILL/R's) is not touched. MENU > SYSTEM > ABOUT shows both: DUELL 1.1 and TIDE's version beside the date.

| Version | Means |
|---|---|
| `1.0.0-b1`, `1.0.0-b2`, ... | A **beta**: one per build flashed to a unit for testing. The number counts up until the build is validated. |
| `1.0.0` | The first build the fork owner has validated on the unit: the **stable** one. |
| `1.0.1` | A fix on top of a stable build. |
| `1.1.0` | A new feature; its betas are `1.1.0-b1`, ... |

Git tag, GitHub release and CHANGELOG section have the same name. The tag is the version with `tide-` in front (`tide-1.0.0-b1`, `tide-1.0.0`). Plain `v1.0.0` would be easy to mistake for OSCILL/R's own tags `v1.0` and `v1.1`, and `tide-` also sorts the fork's tags together. The commit is not in the name: it stays in the release notes ("Build `abc1234`") and in the checkpoints. The older dated tags `stable-2026-11-03` and `beta-2026-11-04` stay as history and are not renamed.

**To cut a beta** (the coordinator does this, with the owner's OK for the push):

1. Put the new version in `TIDE_VERSION` (for example `1.0.0-b2`), start a `## 1.0.0-b2` section at the top of `CHANGELOG-TIDE.md` for the lines of this build, and commit.
2. Tag that commit before building: `git tag -a tide-1.0.0-b2 -m "TIDE 1.0.0-b2"`. `tools/build.py` shows `1.0.0-b2` in ABOUT only when the tree is clean and HEAD is that tag; in any other case it shows `1.0.0-b2+`, so a unit with a plus is a build that is not exactly the release. (`DUELL_TIDE_ID` sets any other text of up to 10 characters.)
3. Build (`tools/build.py --release` in `felucca-base`), flash, test.
4. To publish it: push the tag to the fork, make a GitHub **pre-release** named `1.0.0-b2` on it, attach the package as `duell-tide-beta-1.0.0-b2.fwsc` (and its `.sha256`), and put "Build `abc1234`" and the known issues in the notes. Then run Host tests by hand on `jam11`: Pages offers the new beta.

**To cut a stable** (the owner has validated a beta on the unit): put `1.0.0` in `TIDE_VERSION`, start a `## 1.0.0` section in the CHANGELOG, commit, tag `tide-1.0.0`, build and check the package is the one tested; move the `stable` branch to that commit; make a normal GitHub release (not a pre-release, marked Latest) named `1.0.0` with the package `duell-tide-stable-1.0.0.fwsc` and its `.sha256`. Doc links in the README and the project's pages follow the `stable` branch. The next beta is `1.0.1-b1` (a fix) or `1.1.0-b1` (a feature).

The site picks the stable and beta builds by these versions (`felucca-base/web/releases.py`): a beta is offered only while it is newer than the stable build, and `1.0.0-b2` is after `1.0.0-b1` and before `1.0.0`.

## What the instrument already supports

| Capability | Useful behaviour and boundary |
|---|---|
| Timed evolution | ONCE, LOOP, DRIFT, WNDR and JRNY move SOUND, SEQ or BOTH over 1–64 bars, with endpoint WAIT and manual takeover. WNDR develops bounded variations of the source. |
| Two-part coordination | Peer-attack prediction uses playback timing; phrase edits alternate between parts; sound choices favour contrast. This is timing/parameter-based coordination, without sustained-harmony or effects-tail analysis. Purposeful shared attacks remain musically useful. |
| JRNY palette | Factory destinations within the part's own engine, in the starting sound's role family (pads and keys; bass; lead and pluck; hits; FX and textures), with a roll of the same sound when the family has no other, and exclusions for the current/peer targets and the part's last four destinations. No cross-engine journey. |
| Chord movement | M2a improves whole-loop upper-voice movement in ordinary chord sequences. It does not rewrite ARP SEQ's held chord, infer a chord progression or coordinate harmony between parts. Listening comparison remains open. |
| Polymeter | Per-part lengths already drive ordinary and ARP SEQ playback. Try owner-set 12/16 and 15/16 cycles against a steady 4/4 reference before adding automatic length changes. Physical clock/listening acceptance remains open. |
| Capture | KEEP rebases the heard sound/pattern for subsequent saving. Factory fallbacks obscure unused USER addresses; U1 should expose a few clear save destinations while retaining factory access. Stopped saving is sufficient. Exact live-session morph restart continuity is outside the owner's requirements. |

Source boundaries: `morph_glide.c`, `tide_ui.c`, `seq_rhythm.c`, `seq_phrase.c`, `seq_dialogue.c`, `seq_voicelead.c`, `seq.c` and `morph_complement.c` under `felucca-base/firmware/src/`. The [current limits](KNOWN_LIMITS.md) distinguish the TIDE fork from inherited DUELL notes.

### MULTI contents and pattern replacement (reviewed 3 November 2026)

**Current behaviour:** a MULTI recalls both parts' sounds, patterns, arp settings, mixer, effects and sound-morph End B. Every load replaces both working patterns, including with empty patterns, and clears the previous undo copy. Loading while playing keeps the transport running. Saved P01–P32 pattern slots are separate and are not erased by the load. **SEQ KEEPS RUNNING currently protects patch changes, not MULTI loads.** The owner reported M07 clearing the playing pattern; U2 below records the requested correction as planned, not implemented.

This is the compiled factory set in the curated dub / house order of D31 (2026-11-05: the three acid multis, WIRED SNAIL, DEAD BUFFER and WOOD & SKIN left the sixteen; six new ones; the first start loads M02 DUB CHORD), not a device readback (the pattern and arp columns come from the stored records, `revise_multis dump`, after U2 B; the delay / reverb column is as checked at `bee34f9`); saved user overwrites can differ. “Empty” means no stored note steps, even when a classic arp supplies repeating notes from played keys. “8 / 6” means an eight-step loop with six populated note steps. SEQ uses the stored pattern as the arp; where no arp is listed, it is off. Delay and reverb columns name the stored effect types; chorus and per-part sends are also stored.

| Multi | BPM | Part 1: sound, pattern and arp | Part 2: sound, pattern and arp | Delay / reverb |
|---|---:|---|---|---|
| M01 BASIC CHORD | 122 | CHORD ECHO: 16 / 4; SEQ 1/16; minor sevenths, ducked on the quarters | DUST BED: empty; latched, played live | Digital / Plate |
| M02 DUB CHORD | 117 | BIG STAB: 16 / 2; SEQ 1/16 | TIDAL SUB: 16 / 4; SEQ 1/16, an octave up since D31 | Digital / Plate |
| M03 CELLAR TAPE | 122 | PLUCK BASS: 16 / 5 offbeat line; SEQ 1/16 | CLAVICLE: 16 / 2 chord stabs | Digital 4D / Plate |
| M04 NIGHT ORGAN | 121 | PEDAL BASS: 8 / 5; SEQ 1/8 | DRAWBARS: 8 / 2; SEQ 1/8 | Digital / Room |
| M05 ORGAN SKANK | 123 | ORGAN SKANK: 16 / 6; SEQ 1/16; minor sevenths | AIR SHIMMER: 16 / 2 on ties; SEQ 1/16; latched, an octave up | Digital / Spring |
| M06 WARM ECHO | 118 | WARM WASH: 32 / 2 on ties; SEQ 1/16; power chords | TAPE PLUCK: 16 / 5; SEQ 1/16 | Ping-pong / Hall |
| M07 MINIMAL 12 | 124 | DUB BLIP: 12 / 4; SEQ 1/16 | DEEP BLUE: 16 / 2 on ties; SEQ 1/16; minor triads (12 against 16) | Digital / Room |
| M08 TINE DUB | 120 | TINE ROOM: 16 / 4 on ties; SEQ 1/16; minor sevenths | SLOW TIDE: 16 / 2 on ties; SEQ 1/16; power chords | Digital / Plate |
| M09 DEEP 332 | 124 | SOUL STAB: 16 / 5; SEQ 1/16, swing 54; minor sevenths | FOG BANK: 16 / 2 on ties; SEQ 1/16 | Digital / Room |
| M10 PAD SPLIT | 97 | EVOLVING PAD: empty | EMBER GLOW: 12 / 4; SEQ 1/4 | Random / Resonator |
| M11 WHISPER TIDE | 77 | DIGITAL PAD: 8 / 4 on whole-note steps with ties; notes latch | WHISPER PAD: empty; random arp 1/1, two octaves | Random / Plate |
| M12 TINE STRINGS | 77 | STRING MACH: 4 / 2 on whole-note steps with ties | TINE ROOM: 16 / 4; SEQ 1/8 | Digital / Plate |
| M13 COLD CHAPEL | 71 | EVOLVING PAD: 8 / 4 on whole-note steps with ties; notes latch | SINGING BOWL: empty; up arp 1/2; latched power chords | Random / Resonator |
| M14 EERIE SIGNAL | 120 | VESPERS: empty; diatonic triads | JAZZ CLUB: 16 / 5; random arp 1/8 | Ping-pong / Hall |
| M15 HAZY TENSOR | 117 | LADDER BASS: 16 / 6; SEQ 1/16 | GEARBOX: empty; random arp 1/16 triplets | Octave-up / Plate |
| M16 FINAL REEL | 71 | BRASS PAD: 8 / 4 on whole-note steps with ties; power chords | STRING MACH: 8 / 2; SEQ 1/2 | Ping-pong / Shimmer |

Since U2 B every multi carries a stored sequence on at least one part; an “empty” part is left to a classic arp, chords or the played keys on purpose. M10–M12 each have their own 16-step figure on part 2 (no shared chromatic line any more). All sixteen records store LAYER mode, no keyboard split (including M03 despite its name), straight global swing (50%) and no automation/parameter-lock events. MODE RECALL controls whether stored mode/crossfade settings are applied.

**Evidence and limits:** `multi.c:mu_unpack` clears steps absent from the record's map; `project.c:project_restore_runtime` copies those steps into the live tracks and clears undo. The inventory loaded all sixteen records through the current host UI harness and independently compared all 32 parts' pattern data with `tools/factory_multis/M01.bin`–`M16.bin`. A focused M01 → M07 probe reproduced replacement with TIDE on/off and SEQ FOLLOWS PATCH/KEEPS RUNNING: both live patterns became empty, transport stayed playing, all host pattern-slot bytes stayed unchanged, and saved P32 reloaded its six notes. This verifies source/host behaviour, not physical flash, audio or clock timing. Workspace evidence: a workspace file outside this repository, `FACTORY_MULTIS.md`, `inventory.json` and the probe sources. No firmware correction or installation was performed in this investigation.

**U2 planned direction (owner follow-up):** adding appropriate sequences to the factory multis without them is the preferred content direction. The keep-current-patterns load option remains a separate possible improvement. Route B (sequence additions) and the U3 preview are implemented on `integ`; route A is not.

- **A — Keep current patterns:** provide an explicit keep-current-patterns MULTI load choice while retaining full setup recall. Preserve both parts' steps and timing; define the matching arp/ARP SEQ, automation/locks and TIDE sequence-state policy so new sound settings cannot silently disable or rewrite the retained groove. Resolve its relationship to SEQ KEEPS RUNNING. Full recall must still restore deliberate empty patterns; do not silently reinterpret every empty record as “keep.”
- **B — Give every factory multi an appropriate sequence (preferred):** first add tailored arrangements to **M03 PAD SPLIT, M05 WHISPER TIDE, M06 TINE STRINGS, M07 FINAL REEL, M08 HAZY TENSOR, M13 COLD CHAPEL and M16** (then STRESS TEST, now CELLAR TAPE), whose two stored patterns were empty. Each multi should offer a useful sequenced arrangement; both parts need not contain notes when one has a deliberate live-keyboard or classic-arp role. Then assess all sixteen; retain useful existing sequences and review M10–M12's shared chromatic line for musical suitability. Choose per-part roles, pitches/chords, register, rhythm, length, tempo, gate, arp mode and hold together; avoid adding the same generic line everywhere. Judge the parts together and across the playable keyboard, including transposition, effects and morph behaviour. Verify PLAY starts the intended arrangement and that keyboard input/ARP SEQ transposition remain useful. Done in `beta-2026-11-04` (`bc63e86`, `386e40f`); the stress test left the factory multis and is the test scene `tests/scenes/M16_STRESS_TEST.bin`. Develop reviewable revised factory copies and retain original records; preserve user overwrites. This route still replaces the current patterns on MULTI load, so the replacement behaviour must remain explicit.

For either route, verify the affected load/save and panel/editor/MIDI paths, preserve saved P01–P32 and format compatibility, coordinate the affected tests, and measure the linked memory budget. Route A needs live transition checks; route B needs actual audition/comparison and device playback before musical acceptance. Record the selected route and source/device evidence when implementation starts.

**U3 — MULTIS browser with sound and pattern preview (owner proposal; in `beta-2026-11-04` as `d5e6178` / `893ab9a`, redesigned in `4a568ff`; the text is the original brief):** reduce the current five-row list to three entries at the top, with the selected entry centred where possible. Underneath, reuse the compact two-part pattern layout from the owner's HOME page 3 design, with each part's sound name visible in its part colour and a small ARP/SEQ mode indicator. Empty patterns must explicitly show NO SEQUENCE while still identifying an active classic arp. Locate the exact HOME page 3 reference before fixing dimensions; it was not identifiable in the inspected checkout. Keep labels and both previews legible on the 240 × 240 display and retain clear LOAD/SAVE controls; validate the final arrangement in a rendered screen before implementation acceptance.

The preview follows the highlighted slot's actual stored record or factory fallback **before LOAD**, without changing the current sounds, patterns, transport, morph state or saved data. A stored user overwrite must preview its own contents. Distinguish the browsing selection from the currently loaded multi. Reuse existing drawing/decoding support with bounded memory; refresh preview data when the selection or record changes rather than reading/decoding every frame. Check scrolling, end slots, empty/damaged records, long sound names, overwrite refresh and the running-transport flow. Inspect the screen in the application/emulator, measure firmware memory and check physical readability/playback before device acceptance. This preview makes replacement visible; U2 still governs sequence content or preservation.

## Engineering work, in order

Crossed-out tasks are completed: built, and in a build or merged, with no open condition. A task whose listening or device acceptance is still open, or that has a decided change pending, stays as plain text.

| ID | Work | State and completion condition |
|---|---|---|
| ~~A0~~ | ~~Full firmware source audit~~ | Complete: three review agents plus the coordinator reviewed all 164 tracked firmware files / 66,036 lines; reconciled 2,867 function/macro/assembly units. Reports, exact coverage and findings are retained in a workspace file outside this repository. Four isolated host probes verify narrow source conditions; no firmware build/test-tree/device change or physical acceptance. |
| A1 | Correct verified control/note/persistence edges | Four edges done and flashed 3 November 2026 19:42 as `polish-all` `187e9d0` (not yet on `jam11`): extended editor lock operations (MOTION ops 9..11) no longer swallowed by a query guard (`61d3551`); ARP note 0 no longer collides with the empty sentinel (`f0fa950`); the last 16 USB MIDI-out slots are reserved for note-offs and CCs (`1cf1bbb`); failed TIDE preference writes are paced (`ddb6b09`). The rarer core-job wrap/sentinel issue is not done. Lower-priority undo/developer-parser/UI-consistency items remain in the audit, not a broad refactor queue. |
| ~~E0~~ | ~~Consolidate administration~~ | Documentation reorganization completed in this pass: one current roadmap; musical concepts and historical receipts separated. No firmware cleanup is implied. |
| E1 | Find useful memory savings | First flash-saving milestone achieved; two more merged, not yet in a build: dead code out (the 0.2.11 VOICE CAP outside bench builds, cross-part stealing, Felucca's engine preset tables: 3,372 B back) and the TIDE core's tables and helpers shared (1,444 B back, no musical change). Select another bounded candidate when U1 or a chosen musical slice needs measured space; factory-multi packing is an unmeasured follow-up. Distinguish flash from RAM and estimates from linked savings. |
| E2 | Simplify inherited code in focused pieces | First local slice complete: three knob-navigation paths share the GLO page/history/detent-suppression transition; target selection and ordering stay at their callers. Focused input checks pass and the combined build saves another 48 B. Further cleanup remains evidence-led. |
| ~~E3~~ | ~~Check the code being changed (former T0)~~ | Done: the test-tree cleanup is complete (full check 112 pass / 0 fail / 25 skip; see [the baseline](JAM11_TEST_BASELINE.md)). Use the affected checks for each firmware change and keep any new failure visible. |
| E4 | Implement the best supported memory reduction | First local slice complete: lossless factory-pair packing saves 1,888 B net application flash; combined with E2, 1,936 B. No persistent RAM increase or approximation. OTA staging now shares one 1 KiB pool space for the UFW header and the two 512 B blocks: pool -1,024 B, image +36 B (host group `otareuse`; project and storage staging not touched). Compare further candidates separately. |
| E5 | Retire obsolete workspace artifacts | Inventory exists in a workspace file outside this repository; it is dated evidence, not deletion authority. Identify owners and active consumers; retain packages, backups, listening references and necessary logs. Archive eligible managed worktrees through their lifecycle tools. No broad deletion or worktree removal is part of this pass. |
| ~~U1~~ | ~~4–8 clear user save destinations~~ | **In `beta-2026-11-04` (`a448e90`):** voice slots 125–128 (records 124–127) and patterns P29–P32 (slots 28–31) are USER slots `U1`–`U4` (`USER EMPTY` until saved); the displaced factory sounds/patterns stay as read-only places after them (`#125`–`#128`, `P29`–`P32`). Plan: bounded planned convenience. Prefer four initial voice and pattern destinations; allow up to eight if the same simple approach fits. Reuse existing record addresses and overwrite flow, label unused USER rows clearly, and retain access to compiled factory material. A full new browser/protocol or physical capacity increase is not required. Preserve occupied records; verify stopped save/load/reboot and existing editor compatibility. This is not a gate to musical work. |
| U2 | Add appropriate factory sequences; consider pattern-preserving recall | Owner-requested. **Route B in `beta-2026-11-04` (`bc63e86`, M16 `386e40f`); listening and device acceptance open.** Preferred direction was: tailored sequenced arrangements for the seven multis with both patterns empty, retaining deliberate live/arp roles, useful existing sequences, originals and user overwrites. Review all sixteen for musical suitability. The keep-current-patterns load option remains a separate possible improvement. Full recall still replaces patterns. Complete the source, memory, interaction and actual listening/device checks detailed above. |
| U3 | Show sounds and patterns while browsing MULTIS | Owner-requested. **In `beta-2026-11-04` (`d5e6178`, `893ab9a`); redesigned for the next beta after the owner's device check (`4a568ff`: a plain list, the preview in a bottom panel).** Three list rows above a two-part preview based on HOME page 3, with sound names, ARP/SEQ indicators and explicit NO SEQUENCE states. Preview the highlighted record without loading it. Locate the exact design reference, verify layout and browsing flows, and measure memory before integration; physical readability/playback acceptance remains required. |
| U4 | MULTIS: 24 multi slots (from 16) | **Planned, a separate rollout; not part of the current build.** A tested implementation is parked on local branch `wip-24-multis` (`21ca3a7`), to review and ship as its own build. Owner decision 2026-11-04: extend the multi slots to 24 as its own rollout. It changes the stored multi/backup format, so it needs a format version bump. Loading old 16-slot backups stays unchanged: slots 17–24 come up as factory content and nothing the user saved is overwritten. Also needed: support in `jam/jam_dev.py` backup/restore, in the web editor's reader and in the mock unit; `DUELL_TIDE=0` byte-identical (`jam/check_off.sh`); the flash cost measured first (the slot has about 6 KB free; keep at least 3 KB headroom); a MULTIS selector and preview that handle 24 entries; and tests. |

The Status table gives the latest full run; [the baseline](JAM11_TEST_BASELINE.md) records the earlier **112 pass / 0 fail / 25 skip** run (`sh jam/check.sh` on Windows; the 25 skips run in the Linux host-test CI). Earlier snapshots (80/34/22 at `4443c13`, and the `bd339c9`/S0 figures below) are history. The totals are revision-specific.

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

All original factory pairs and metadata were compared with the retained baseline header; actual C decoding matches all 128 voices. Strict `bank`, `tidekeep` (72 checks), `tidecontrols` (121), `knobjump` (6) and `sidechain` (17) pass on the combined source. The exact retained package passes the pinned CPU emulator's boot/LCD/MIDI-note/bounded-audio smoke. Peak stack, real-device CPU/UI/clock/audio and installation are not established by these checks. Receipt: a workspace file outside this repository.

For U1, factory voices are compiled separately (`sounds.c`); the 128 visible slots show a stored record or a factory fallback (`upreset.c`). Patterns are 32 prepopulated entries in three 11/11/10-slot objects (`patslot.c`). The owner accepts existing overwrite saving and only wants a few obvious places for new material. Reusing 4–8 existing destinations with separate factory access is preferable to increasing counts or migrating layouts. No capacity increase or layout change has been implemented.

P4's earlier FACTORY/USER browsing proposal is a reference, not a requirement for a large UI/editor redesign. It studies voice IDs 124–127 and pattern P29–P32 as four initial USER destinations; the exact 4–8 addresses/labels should follow the smallest compatible implementation. Occupied destinations remain occupied and retain the existing overwrite choice; do not move/erase data merely to make them blank. No feature is implemented yet. See the P4 receipt in the batch evidence directory.

### Storage priorities and parked observations

| Concern | Current evidence | Required next result |
|---|---|---|
| Live End B restart preservation | Earlier readback found part 2's live destination changed after restart; raw evidence is retained | Parked by the owner: no requirement to preserve exact ongoing morph state or add complex session capture. Do not make it a prerequisite. Ordinary intentional saved-record integrity still applies. |
| Saved End B during record rewrites | `dd2acd5` already repairs rename/recategorization and editor FM6 PUT; newer than last recorded installation | Source-complete bookkeeping, not a new feature queue. Retain the existing fix and stopped-save restrictions; no live-save workflow or additional project is requested. |
| TIDE preferences backup | Settings already persist between boots in object 48; the editor archive omits that object | Useful bounded follow-up: archive/restore the TIDE configuration between flashes, accept older archives without it, and preserve existing fields. Do not turn this into a full live-morph/session capture. **Decided 2026-11-04, merged, not yet in a build:** TIDE's 12 setting bytes moved into the settings record's reserved tail (map_tail bytes 32..63), so every backup holds them, and an earlier build's object 48 is read once; going back to plain DUELL is documented as unsupported rather than made to work. Still planned: the FM6 voice bank in backups. |
| Settings record growth | Current seven-field record works; the loader rejects a record shorter than a future requested count | Treat as compatibility bugfixing when adding fields: preserve stored fields and default only missing new fields. Cover old/current/malformed/unsupported records. No record redesign or new settings fields are requested now. The 12 TIDE bytes live in this record's reserved tail (decided 2026-11-04, merged, not yet in a build): fields past the ones a build knows are kept as read. |
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

## Planned work after the 4 November 2026 decisions

On 4 November 2026 the owner decided the open questions left by a full code and functional review of the release candidate and of the inherited DUELL 1.1 base. This section is the plan that follows. **Planned means decided and not built.** Crossed-out rows are merged on the integration branch and in no build yet; nothing in the tables below is in a published build. The user guides describe the firmware on the integration branch. Sound changes are heard on the unit before they are accepted. Flash is tight (about 5 KB free), so every firmware item is measured on its own and the simplifications pay for the additions. Nothing is pushed or flashed without the owner's go-ahead.

### In progress

Each runs on its own branch until it is reviewed and merged. Crossed-out rows are merged on the integration branch.

| Fix | What changes |
|---|---|
| ~~FM6 re-strike~~ | ~~FM6 pads and keys stop clicking and dipping about 20 dB on re-strike, arp and held chords. Also true for plain DUELL.~~ Merged, not yet in a build. |
| ~~Arp timing and swing direction~~ | ~~The arp stays on the grid for a whole set, and swing swings the right way.~~ Merged, not yet in a build. |
| ~~Delay-time glide, steadier clock tempo~~ | ~~TIME and TEMPO moves bend instead of clicking; the tempo and a synced delay stay steadier under an external clock.~~ Merged, not yet in a build. |
| ~~JRNY roles and level data~~ | ~~The sounds that fall in no role get one, FX are split from PERC, USER records honour the families, and level data for GHOST HARP and a few other sounds is corrected.~~ Merged, not yet in a build. |
| ~~CI~~ | ~~Green and honest: the two-core simulation checks invariants only, the runner shows the real failing line, the two unlisted JRNY groups run, the runner image and actions are pinned.~~ Merged, not yet in a build. Pages now publishes only a tested, green commit. |
| ~~ENV SYNC release end~~ | ~~A long synced release (4 bars at 69 BPM or slower) ends instead of holding a voice at -71 dB.~~ Merged, not yet in a build. |
| ~~MIDI Start and arp velocity~~ | ~~MIDI Start plays step 0 on the first clock pulse; the arp plays the key's velocity.~~ Merged, not yet in a build. |
| ~~Delay times on the grid, RESO in tune~~ | ~~A delay TIME too long for the line halves until it fits in every mode and the card shows what plays; RESO's combs are tuned for DAMP.~~ Merged, not yet in a build. |
| ~~Load undo~~ | ~~OCT- held plus a PRESETS turn undoes the last sound load; PRESETS steps like PATCHES' knob 1; long lists accelerate.~~ Merged, not yet in a build. |
| ~~Dead code and TIDE core clean-up~~ | ~~The VOICE CAP outside bench builds, cross-part stealing and Felucca's engine preset tables leave the unit's image (3,372 B); the TIDE core's tables and helpers are shared (1,444 B). No behaviour change.~~ Merged, not yet in a build. |
| ~~Filters~~ | ~~The state-variable filter keeps its precision below 100 Hz (resonance and no DC offset at a low cutoff); the CUT dial is slewed, so a sweep no longer zippers.~~ Merged, not yet in a build. |
| ~~Silent tails~~ | ~~The ROOM, RESO and delay loops fall to exact zero.~~ Merged, not yet in a build. |
| ~~Chord names~~ | ~~Flats in a flat key; a stack of 4ths is named; DIA7 on a six-note scale is a triad.~~ Merged, not yet in a build. |
| ~~TIDE page truth~~ | ~~The pages and rows show what plays: step position and length, rows that do nothing in the current mode, the CUSTOM snapshot, the ARC status text.~~ Merged, not yet in a build. |

### Planned: TIDE musical behaviour

| Item | Decision |
|---|---|
| ~~Skank drift~~ | ~~A skank hit may drift at most two 16ths from its origin.~~ Merged, not yet in a build. |
| ~~LEAD roles~~ | ~~Call and response applies to lead and pluck roles only, never to chord skanks.~~ Merged, not yet in a build. |
| ~~Bass and PITCH~~ | ~~With PITCH ON the bass never walks.~~ No note under C3 and no BASS-category sound is walked, and nothing moves on SCALE CHR. Merged, not yet in a build. |
| ~~Velocity shape~~ | ~~Automatic, with no setting: RISE louder, BREAK and the followers softer.~~ Merged, not yet in a build. |
| ARP SEQ ties | ARP SEQ honours ties (about 40 B). Real chord steps are not planned. |
| ~~Factory multis~~ | ~~About half of the factory multis leave the bass to external gear. First start loads DUB CHORD (as it does now). No acid multi stays.~~ The 16 multis are in the curated dub / house order with six replacements (D31); only four carry a bass part, the first start loads M02 DUB CHORD. Merged, not yet in a build. |
| ~~Loads~~ | ~~A multi or pattern loaded mid-session starts as written: the TIDE generation does not carry over a load.~~ Merged, not yet in a build. |
| ~~ARC and transport~~ | ~~ARC resumes where it was on STOP and PLAY (STOP and PLAY are used live to resync with other gear, and a restart would throw the tide back to its opening stage). It restarts only on a load or a K1 stop.~~ Merged, not yet in a build. |
| ~~ECHO~~ | ~~ECHO folds into LEAD: TURNS becomes TOGETHER, TAKE TURNS and LEAD; the PEAK style uses LEAD; a stored ECHO reads as LEAD.~~ Merged, not yet in a build. |
| Crossings | A LINKed filter 2 closes with filter 1 during a crossing. WASH stays. |

### Planned: TIDE interface and product

| Item | Decision |
|---|---|
| Two-core balancing (owner, 6 November) | After b2 (1.0.0-b2), a few blocks still miss during an FM6 crossing in a GLIDE, about 3 per transition. The owner accepts this for now. Today core 1 plays part 2 only. Core 0 plays part 1 and then the gather, the sends, the buses (about 7 % of a half in STEREO, 15 % in DUEL), the master, and bus B in DUEL. The unit's probe shows core 0 spiking while core 1 waits 60 to 90 us a block. Plan: hand the buses (or a whole part, by its recent measured cost) to core 1 when it has slack, keeping the runner's deadline and fallback rules (twocore_run.c, docs/TWOCORE_STAGE1.md). Ravel_FM1 1.16 to 1.19 shows the same idea on this hardware: voices by measured cost, then whole tracks and the buses as jobs. Measure it with the test-mode stage probe (`cpu`: st_* figures). It gives the headroom back without losing notes or quality. |
| Transition costs only while moving (owner, 6 November) | Use the b2 FM6 rule for the other morph costs too. The engine-choice cross-fade (BD_FADE) and TIDE's valley and wash are charged all the time while a part has a MORPH. Charge them only while a transition runs: more notes while still, thinner while moving. |
| HOME without MODE (owner, 6 November) | Remove the MODE page, currently page 4/4 of the HOME round. Its three controls move elsewhere: the output MODE (STEREO / LAYER / DUEL), XFADE and SPLIT. Where they go is still open (not decided yet). They keep their values in the settings and in each MULTI. Every piece of information the page shows stays reachable. |
| K1 stop | K1 needs two detents to stop a running tide. The header messages stay. |
| OCT- reset | OCT- plus a knob resets to the STYLE's values, on the TIDE pages only. |
| Installer and INFO | The installer backs up before every install. INFO shows a TIDE flag. The cheat sheet is finished. |
| Settings in backups | ~~The 12 TIDE setting bytes live in the settings record's reserved tail, so every backup holds them; a full patch store reports STORE FULL end to end; a restore reports every failure.~~ Merged, not yet in a build. The FM6 voice bank still goes into backups (not built yet: an archive object and a restore through the FM6B commands). Going back to plain DUELL is documented, not supported. |

### Planned: repository

| Item | Decision |
|---|---|
| Name | Rename `jam11` to `tide`. Replace OSCILL/R's wordmark on the cover until the author agrees. No Firmware Hub entry yet. Remove the internal brief files from the public repository. |
| CI | ~~Green CI gates Pages.~~ Merged, not yet in a build. Gating the channels is still to do. Device builds stay an owner step. Two-core checks in CI are invariant-only (done). |
| Branches | Fast-forward `jam11` after the current fix batch (the push needs the owner's OK at that moment). Merge upstream's emulator commits. The 24 multis (U4) come after the release candidate. |

### Planned: inherited DUELL 1.1 engines

Done in the fork first. Items that help plain DUELL are offered upstream later, one finding per change.

| Area | Decision |
|---|---|
| ~~Glide~~ | ~~GLIDE RATE and TIME do what the display says, up to 10 s.~~ Factory sounds and multis keep the glide they had; patches you saved yourself now glide for the time they show. Merged, not yet in a build. |
| ~~RES~~ | ~~Remap RES so the whole knob is useful.~~ The factory sounds, multis and the bank file are migrated to sound the same, to within a few percent; patches you saved yourself are not. The owner judges the result on the unit. LADDER's bass loss, filter 1 without self-oscillation and LOFI's aliasing stay as character. Merged, not yet in a build. |
| WAVE | Table quality stays as it is (flash is too tight). |
| ~~FM6~~ | ~~The engine cap goes to 8 voices. DTUN is centred. UNISON detune stays about plus or minus 24 cents; the comments are corrected, the range is not widened.~~ UNISON also keeps its level and velocity on FM6 and PHYS, OKS is honoured (a patch with oscillator key sync off free-runs), and an overload sheds the quietest FM6 tail. Merged, not yet in a build. |
| ~~Swing and clock~~ | ~~The displayed swing % becomes truthful. The arp starts on the transport grid. A local PLAY under an external clock waits for the master's bar.~~ Merged, not yet in a build. |
| LFO DEPTH | The destinations are shown as "LFO1/2 LVL" (not removed). The mod wheel keeps driving MORPH. |

### Planned: effects

| Area | Decision |
|---|---|
| Delay | A feedback low cut (about 150 Hz) and controlled dub runaway: FDBK up to about 105 % with a soft clip in the loop. |
| ~~Master~~ | ~~A clean soft knee replaces the always-on tanh. This changes every golden render, so it is its own change, with the golden update in the same commit and nothing else riding on it. SPACE +100 then pushes about 3 dB less.~~ The knee is exact up to the limiter's threshold (-5.2 dBFS). Merged, not yet in a build. |
| ~~Modulation ranges~~ | ~~Chorus and delay modulation ranges are remapped to chorus territory; stored sounds are migrated (to within a few percent).~~ The sweep holds its pitch swing above CRT 40 (chorus) and MRATE 44 (delay); the factory multis are migrated, patches and multis you saved are not. Merged, not yet in a build. |
| Reverb | SHIM's PITCH DOWN and MOD stay: it is a working base feature, and it is cut only if a TIDE feature really fails to fit, and then the owner is asked first. ROOM becomes a short diffuse room built on the HALL code. HALL and PLATE get a pre-delay that grows with SIZE (no new setting). |
| ~~COMP~~ | ~~The automatic makeup is halved.~~ Its ATTACK and RELEASE also follow the printed times. Merged, not yet in a build. |

### Planned: stock user experience and upstream

| Item | Decision |
|---|---|
| ~~Knob columns~~ | ~~Fix the documents rather than the knob columns.~~ QUICKSTART, VOICE_CONTROLS, the TIDE guide and the first-start guide follow the firmware. Merged, not yet in a build. |
| Load undo | ~~A load undo on OCT- plus a PRESETS turn.~~ Built page-scoped: Merged, not yet in a build. OCT- held and a PRESETS turn undoes the last load where PRESETS loads, and not on the TIDE pages, where OCT- plus a knob keeps TIDE's reset; the STYLE reset itself is not built. Saving at stop stays as it is. Tempo goes into the footer (still planned). |
| Factory bank | Re-author the 128-voice bank's End Bs and sync the slow motion, in the fork first; offer it upstream later. |
| Upstream changes | Prepared as small single-finding changes, in order of value to plain DUELL users: FM6 re-strike; arp grid and swing direction; delay clicks; glide; a steady tempo; then the smaller fixes, the PHYS tuning fix and the document corrections. The owner opens every pull request. Before any code is written, the owner raises three items with the author: the voice and render restructuring, fixed knob columns, and the bus B pointers. |

## Musical delivery order

This is the rationalized plan, not a claim that new musical firmware is implemented or that open performance choices are settled. The source audit is complete and A1 lists bounded correction candidates; U1 is a small planned convenience, not a prerequisite to musical work. Evaluation rows can proceed in parallel without broad test cleanup as a gate. Current audio polish is accepted: M5/M10 do not create an obligatory re-polishing exercise. Retain M0 as owner-complete; R0/M1/M2a remain implemented and installed, with M1 host A/B accepted and M2's focused listening comparison optional when relevant.

| Order / existing ID | Deliverable and feasibility | Completion decision |
|---|---|---|
| Optional evaluation: M2a | Ordinary triad/four-note loop when that behaviour is being judged; ARP SEQ cannot demonstrate its benefit. | No prerequisite or mandatory listening project. Avoid expanding harmonic scope from mathematical movement alone. |
| Conditional: M5 + M10a | Current audio polish is accepted. Investigate matched-input level/slow-glide behaviour only for an identified defect/regression or a chosen transition change. | No generic recalibration/smoothing project. Keep existing protection and correct a demonstrated cause narrowly; M10b/c/d are conditional. |
| Evaluation: M7d baseline | Use existing unequal-length controls: 12/16, then 15/16; straight timing before swing/ARP SEQ. | Hear useful motif rotation and verify the full joint cycle. Automatic shortening remains uncosted and needs a separate length/return/KEEP policy. |
| First musical candidate: M8a | **In `beta-2026-11-04` as the TURNS row.** Alternating variation turnarounds in MUSIC WNDR/JRNY with VARY 4 + 8. Small scope, but code size is unmeasured. | Keep VARY ALL and manual/ONCE/LOOP behaviour. Retain only if alternating bars improve interplay in listening. |
| ~~Next: M7a/b~~ | **In `beta-2026-11-04` as the SYNC row.** Soft offbeat/anticipation weighting for eligible movable notes. Use actual musical phase; explicitly leave unsupported grids unchanged in an initial slice. | Preserve anchors/continuations and useful shared attacks. An anchor-only pattern cannot be made syncopated by a move-only policy. |
| ~~Conditional: M3~~ | **In `beta-2026-11-04` (JRNY memory and role families).** Four-target ephemeral history with bounded relaxation for small palettes. | Count eligible destinations first. 61 of the 128-sound bank's 66 engine/category groups have at most four sounds (99/128 voices; 55 of 66 with the 32 added sounds), before endpoint/peer exclusions; history cannot promise broad exploration there. |
| Conditional: M8b, then rhythmic M8c | **In `beta-2026-11-04` as TURNS LEAD / ECHO.** Main-loop follower masks, with cheap playback lookup; rhythmic echoes only after turn-taking proves useful. | Hear a leader and an audible relationship while retaining drive, live keys, held notes and tails. Scale-root/fifth pitch cadences stay deferred without chord context. **Decided 2026-11-04, merged, not yet in a build:** ECHO folds into LEAD, so TURNS has three values; LEAD applies to lead and pluck roles only, never to chord skanks. |
| Later: one M4/M9 policy | **In `beta-2026-11-04` as the ARC row (RISE / DROP, with the hand drop).** Combine longer-form development/recovery into one musical-time planner. Preserve the requested harshness/busyness link as a bounded, scene-calibrated bias. | Begin with gradual development/recovery. Full stops need an explicit performance policy, audible benefit and a measured flash/CPU budget. No second competing scheduler. **Decided 2026-11-04, merged, not yet in a build:** ARC resumes where it was on STOP and PLAY and restarts only on a load or a K1 stop; a velocity shape comes with it, with no setting (louder late in the rise and at the peak, softer in the drop, the breakdown and on the followers' bars). |
| Alongside accepted behaviour: CHANGE / ENERGY / ARC | CHANGE controls sequence development amount/rate; ENERGY biases character; ARC enables longer form. ROLL selects generation policies. | Add rows only with their useful behaviour and compatible settings loading/backup. Current behaviour remains the reference; drop ownership and exact ARC default remain open. |
| M6 | Combined acceptance for the behaviour actually changed | Keep source tests, emulator results, installation/data readback and physical clock/audio observations distinct. |

Use the completed audit's verified, useful corrections as the first A1 candidates. Keep U1/P7 to 4–8 straightforward save destinations and TIDE configuration backup to its own bounded change. M8a remains the first small musical feature candidate; neither exact restart continuity nor a general sound-polish project is a gate. Do not bundle the queue into one build. Cross-engine JRNY, autonomous harmonic rewriting, automatic loudness normalization and universal harshness classification have no supported near-term plan. Retain existing sounds/effects/performance capture. Detailed acceptance and unresolved choices live in [the design notes](design/TIDE_MUSICAL_DESIGN.md).
