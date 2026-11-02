# DUELL 1.1 migration — completed baseline; status updated 2 November 2026

The implementation and integration onto DUELL 1.1 are complete, and the owner confirms TIDE works on hardware. This document retains migration evidence. The pending statements and resource measurements below refer to their named historical checkpoints. Current installed/source state, memory figures and engineering priorities live in [the roadmap](../ROADMAP.md).

R0, M1 and M2a were installed at `1196a82` on 1 November; the S0/DSP checkpoint `e176f6e` was installed on 2 November. Engineering maintenance now has priority over musical expansion. Detailed [musical proposals](TIDE_MUSICAL_DESIGN.md) remain available separately.

Historical M1/M2 installed build: **579,832 / 581,564 B application flash (1,732 B free)**; ordinary RAM **5,808 B free**, pool including range Z **11,156 B free**, hot audio RAM **1,468 B free**. These figures were superseded by S0/DSP. Original [integration and installation receipts](../history/TIDE_ROADMAP_2026-11-02.md#m1--m2-integration-for-github) are retained; use the roadmap for the current budget.

## Retained implementation snapshot — before owner hardware confirmation

**Historical stage: tested Claude fixes merged; TIDE/TIDE SET restyle built for owner emulator review. The complete implementation fits; nothing from this implementation has been flashed.** Continue in `work/wt/duell-jam11`, branch `jam11`. The older revision 3.5 plan below is historical, not the current build status.

## Completed

- Integrated S1 pages, GLIDE, sound/sequence journeys, full crossings/wash, FM6 live tuning, Claude KEEP/storage, combined undo, and the six always-on fixes. Owner accepts TIDE-only saves for exact FM6 recall.
- UI feedback: G4 KEEP hint below SEQ TOOLS; STORE FULL in both save flows; STEP variation outlines and refresh; two-part destination footer on TIDE/TIDE SET (selected part only on MORPH/MODE); quantized theme updates. TIDE splash/About uses the existing L alphabet font; credits, animation and controls remain. TIDE-off keeps the original bitmap artwork.
- Cold morph compiler policy (`946afe7`) saves 304 B without changing function bodies. Compact FM6 tuning endpoints (`be070fb`) recover 576 B of pool including alignment for 16 B flash cost.
- Claude UI compaction `50cfdac` merged at `0736b23`: removes superseded renderers and unreachable layout paths. It saves 4,240 B flash, 1,024 B ordinary RAM and 1,024 B pool. Claude's pixel comparisons match all 2,514 TIDE-on and 2,469 TIDE-off cases. Result and proof: `work/handover-2026-11-01/CLAUDE-UI-COMPACTION-RESULT.md` and `ui-compact-proof/`. Separate Claude worktrees are preserved; the UI freeze is over.
- Retired hardware scope capture is now host-only (`FELUCCA_HOST_SCOPE` in `tests/hostsim.c`). The old settings bit remains reserved/ignored. This removes obsolete production source while preserving metronome test capture; it yields no further image reduction after compiler dead-code elimination.

## Historical restyle measurements

The final restyle build with the merged functional fixes measures: **577,116 / 581,564 B**, **4,448 B flash free**, SHA-256 `ca7963ad7f34f5bb05cf1c44fbf8650d5f489e1201a604e5e07e40ee9fc0d0a7`. All target ELF, startup, RAM placement, call reach and range-X closure checks pass (`errors: []`). Evidence: `work/audit/jam11-measurements/tide-restyle-final/`. The earlier fresh-asset build is retained in `tide-ui-release-generated/`. Production bounds remain unchanged.

| Memory area | Used | Space remaining |
|---|---:|---:|
| Ordinary RAM | 67,872 B | 1,760 B after the 4 KiB reserve |
| Pool plus range Z | 357,480 B | 2,968 B after the 8 KiB reserve |
| Range X | 31,584 B | 928 B |
| Hot audio plus flash driver | 13,968 B | 1,468 B physical; 1,240 B after the 228 B reserve |

TIDE-off also passes target checks: **556,920 B**, app SHA-256 `0a35ee3e6e530c6047ce69ae0df75440b79dda493c9d02960829b2ec3b5b9777`. `jam/off.sha256` now records package SHA-256 `1efb2d84c26cf436194b725d36dd2364bb80aedcf911e076dd19c23ab505bb7e` for the intentionally updated shared baseline (font/compiler/UI cleanup and retired scope). This supersedes the fixes-only hash; it does not claim byte identity with untouched upstream 1.1. Evidence: `tide-ui-off-current/` and `tide-restyle-off/` (same app bytes). Claude also ran `jam/check_off.sh` successfully on `516b700`. The package hash was calculated from the checked app and unchanged loader; the shell wrapper itself has not been run in this stage.

## Verification and limits

- Focused host checks: KEEP/storage 72/72; crossings 566/566; MIDI/PERFORM/UI 32/32; EQ 13/13, with ASan/UBSan. The restyle controls, crossings and KEEP groups pass under ASan/UBSan. The shared UI suite reports the same 13 failures documented on Claude's baseline (`tide-restyle-uitest.log`). ANALOG and FM6 at-rest samples match the reference (`d3d0b151f5d71383`). Native SEQ TOOLS, STEP, splash/About and PERFORM captures were inspected. These establish host behavior, not physical audio or MIDI.
- The affected legacy `click_test.c` cannot compile on this branch: it uses the removed `chain` global and POSIX two-argument `mkdir` on Windows. Capture hooks remain available in hostsim; no test was weakened. Log: `work/audit/tide-scope-click.log`.
- Existing unrelated host-suite failures remain: erase/menu 65, morph 2, web editor 16, librarian 2, and the old editor-round MockUnit.deviceMenu exception. Claude's UI batch reproduces the same failures before/after its patch. Old `ui_render` also fails to compile on its baseline. No broad rerun is required before hardware audition.

## Evening fixes and restyle / owner emulator review

- Claude `tide-fix` `516b700` merged at `2f37ef2`; the separate worktree is untouched. HOME now opens MORPH, then TIDE SET, MODE, TIDE. The chain filter path now applies the crossing/GLIDE dip; GLIDE follows PLAY with pause/resume; both TIDE footer rows identify their part. Claude's tested target was 576,880 B; the restyle adds 236 B.
- Applied the reviewed restyle draft from `work/handover-2026-11-01/tide-restyle-wip/`, with its tuple typo removed. TIDE/TIDE SET use the existing template-9 knob line and card/list style. All six settings are visible. Retained the GLIDE progress bar as a thin strip above the cards, in addition to its status and moving fader knobs. Kept sound/sequence lanes, notes/chords/ties, variation outlines, playheads, part selection and both footer rows. Card lifting is limited to actual TIDE pages, excluding overlays.
- Rendered and inspected selected-part/BOTH views, populated and varied note patterns, running/paused GLIDE, and the last selected setting. Evidence: `build/host-review/tide-restyle/` and `work/audit/tide-restyle-preview.png`. Final label and playhead spacing is in `tide-restyle-final-shots.log`.
- Rebuilt the current emulator at `http://127.0.0.1:8765/?build=tide-restyle-966027`. The build now fingerprints the WASM and worklet URLs, avoiding reuse of old assets after the page loads a new build. Browser source includes real TIDE settings storage. The previously observed browser smoke limitations (two GUIDE expectations and the old heavy-song fixture) remain; no new full emulator sweep or physical validation is claimed.

## Historical next steps / handoff (superseded by the roadmap)

1. **Owner review first:** inspect the restyled emulator, the HOME order, two footer rows and retained motion/note/progress displays. No further design change or firmware write is required for this review.
2. The old `tide-device-test-a5b768d` package is superseded by the evening functional fixes and restyle; retain it as historical evidence. After owner review, regenerate a checked package from the accepted source before requesting authorization to flash. Actual FM-1 audio/CPU/GLIDE/KEEP and TR-6S clock testing remains outstanding.
3. Further compaction is optional until hardware findings justify it. Retain the current font-based branding; bitmap restoration has not been measured against the merged budget. USB audio and the full crossings/wash remain enabled.

Build helpers: `work/audit/stock_compare_build.py 1.1 wt/duell-jam11 <unique-label>` (`DUELL_REGENERATE=1` regenerates assets, `DUELL_TIDE_OVERRIDE=0` disables TIDE, `DUELL_MEASUREMENTS_DIR` selects external output); `crossings_host_build.py wt/duell-jam11 candidate <test.c>`. Windows target tools need approved execution. `work/audit/tide-crossings` is an older evidence snapshot, not the active source.

---

# JAM on DUELL 1.1 — migration plan

Historical snapshot: **revision 3.5, before the integration and fitting work above (2026-11-01)**: see **`docs/design/HANDOVER_2026-11-01.md`**. Steps 0, 1, 2a and
S1 part 1 are committed on `jam11` (`1c04831`). The S1 UI is on `jam11-s1-ui-wip` (`54e8727`): it compiles, but the link
overflows the flash slot by 2,484 B, and about 9-10 KB more are needed with S2. Codex is investigating why it wasn't
foreseen and where to compact. The handover's tables supersede the space figures below where they differ.

### Historical state at revision 3.5 (2026-11-01)

| What | State |
|---|---|
| FM-1 | `DUELL 1.1` + font compaction (8-slot) + hot-room placement, package `work/checkpoints/DUELL-2026-11-01-1.1-fonts8-hotroom/duell-1.1-fonts8-hotroom.fwsc`, SHA-256 `7de3761b…1cdff867`. INFO answers |
| Flash history today | 09:0x plain 1.1 → 09:35 back to JAM 1.0 + restore (another session) → 09:40 fonts8 → plain 1.1 → fonts8 (owner's A/B comparison). Backups before each flash in `work/backups/` |
| Unit settings | still holds JAM 1.0's [15][23] = 0x69 (NOTES LEFT OFF) unless the owner ran ERASE DATA; ERASE DATA is planned at the first `jam11` flash |
| `jam11` worktree (`work/wt/duell-jam11`) | step 0: `1d29995` Windows toolchain, `0c48565` fonts (8-slot), `ac0fee1` hot room, `aa55e22` `jam/` tools, `33ea81a` test baseline. Step 1: `d445608` D (FM6), `b6b669e` E, `67ea2ec` F, `baeec20` B + D (PHYS/WAVE), `1c56523` C, `a227e4a` A; `e9a5495` LF fix for three tools files |
| Branch `jam` (`71654ba`) | untouched reference |
| Audits | `work/audit/MIGRATION_1_1_AUDIT.md` (rev 2, answered here); `work/audit/DUELL-1.1-COMPACTION.md` (font compaction, adopted as 8-slot); re-audit of rev 3.2: `work/audit/MIGRATION_1_1_REAUDIT_3_2.md` (go for step 0 and fixes; §4.9 before S1) |
| Hot-room build (adopted) | `work/checkpoints/DUELL-2026-11-01-1.1-fonts8-hotroom/` (fonts 8-slot + hot room, SHA-256 `7de3761b…1cdff867`), built and confirmed here, **flashed 2026-11-01 ~09:52 (backup `DUELL-backup-2026-11-01-0950.json`), INFO answers; owner: "plays as before" (by ear; 1.1 retired the CPU bar, so the number is only on MENU > SYSTEM > ABOUT, not yet compared). M16 STRESS TEST on this build (owner): about 70 % CPU, peak 80 %, the UI a bit slower, no crash or dropout; key count not noted; no fonts-only figure yet to compare against** |
| Open | `jam_dev` test fixtures for 1.1 formats; whether to flash the fixes build (Raqm-correct); step 2; §4.9 before S1

## 0. Summary in plain words

Start a new branch from DUELL 1.1 and bring over only the part of JAM that plays by itself:

1. **The journey:** GLIDE with ONCE / LOOP / DRIFT / WNDR / JRNY, TIME and WAIT, the MUSICAL sound morph,
   MORPH TO (JRNY's move to another sound), the **smooth crossings** between unlike sounds, and SEQ MORPH.
2. **The two JAM pages** as they are: JAM (both parts) and JAM LIST.
3. **Fixes that keep the journey clean:** FM6 live operator level, ANALOG slope-fade restart, the external MIDI
   clock, PHYS tuning, unison start/level.

JAM is reached through one switch (`DUELL_JAM`) and a small set of hooks. With the switch off, the build must equal
the fixes-only baseline. That's how "separable in code" is proven.

The hard part is **space**. 1.1 already fills the flash and the fast audio memory. The port goes in slices, with
the basic journey first and each slice measured. The optional extras (contrast, voice leading, dialogue) come last,
and only if they fit.

### 0.1 Owner decisions (2026-11-01)

| Question | Decision |
|---|---|
| JRNY between unlike sounds (audit F1) | **Bring back the smooth crossings** (`morph_xfer.c`): FM6 bridge, valleys, cut handoff. The wash is the first piece to cut if space runs out (§4.8). |
| JAM settings storage | **Own storage object** in patch chunk 21's sector pair; **126 own saved patches** is fine (audit F5). |
| USB audio | **Keep it.** USB audio (about 9 KB) is not a source of space. |
| Held notes in FM6 WNDR shape-follow (audit F2) | **Use 1.1's ties** (`ST_TIE`). |
| Old data on the unit | Nothing needs keeping: ERASE DATA after the first `jam11` flash. |

## 1. Reference points

| Name | Commit / file | What |
|---|---|---|
| Base | `654a89e` | DUELL 1.0 as forked |
| JAM source | `71654ba` (branch `jam`) | reference for the port; never changed |
| Target | **`974d87509e7e89db0738c731c8a657968e60e498`** (tag `v1.1`, includes the 1.1 code commit `16d0223`) | branch `jam11`, worktree `work/wt/duell-jam11` |
| **Installed now** | 1.1 + font compaction (8-slot) + hot-room placement, `work/checkpoints/DUELL-2026-11-01-1.1-fonts8-hotroom/` (SHA-256 `7de3761b…1cdff867`) | the unit's current state; step 0's two optimizations |
| Rollback: fonts only | `work/checkpoints/DUELL-2026-11-01-1.1-fonts8/` (SHA-256 `606e5a50…e55b02`) | 1.1 + font compaction |
| Rollback: plain 1.1 | `work/checkpoints/DUELL-2026-11-01-plain-1.1/duell-1.1.fwsc`, SHA-256 `f3787beb…d7c16584b` (`SHA256SUMS` beside it, read-only file; with its build log and after-flash backup) | immutable copy (audit F8). Not the release package: the release's hash is `f8f8cdee…`. Different build, same source. |
| Rollback: last JAM | `work/checkpoints/DUELL-2026-10-31-2040-sound-complement/`; JAM backup `work/backups/DUELL-backup-2026-11-01-0900.json` | the last JAM that was played |

## 2. Scope

### 2.1 Keep: the journey (behind `DUELL_JAM`)

| Feature | Source (jam) | Runs in | Slice |
|---|---|---|---|
| GLIDE: ONCE/LOOP/DRIFT/WNDR/JRNY, TIME, WAIT | `morph_glide.c` | main loop | S1 |
| MUSICAL morph (dial model) | `morph_music.c`, `morph_fm_shape.c` | main loop | S1 |
| MORPH TO + target FM6 patch adoption | `morph.c` (`morph_to_sound`, `morph_to_of`, `morph_fmto`, `fm6_slot_patch`, `tok[][]`) **and** the consumer: `morph.c:1435-1444` (copy of the target's non-blended FM6 fields) and `morph_fm6:1455` (`mxs[].side`) (audit F1) | target building: main loop; the consumer `morph_fm6`: audio path (range Z) | S1 |
| Smooth crossings | `morph_xfer.c`: FM6 bridge (`eng_fm6.c`), valleys (`fx.c` mix block), cut handoff (`voice.c` `morph_xcut`), wash (`morph_wlvl`, `XWL`, `mxs[].motion/follow` used by `glide_motion`) | **audio path** + main loop | S2 |
| JAM and JAM LIST pages, settings, MIDI GLIDE, KEEP | `ui_jam.c` + a complete KEEP action (§3.1 H14) | UI | S1 |
| SEQ MORPH (A/B pattern ends, dials) | `seq_morph.c` | ends in main loop; played-step read **in the audio path** | S3 |
| Phrase lifecycle / rhythm / scale evolution | `seq_rhythm.c`, `seq_phrase.c`, `seq_evolve.c` | main loop | S3 |
| Optional extras | `morph_complement.c` (contrast), `seq_voicelead.c`, `seq_dialogue.c` | main loop | S4, only if space allows |

### 2.2 Keep: fixes (always on, own commits)

| Fix | Source (jam) | Why kept | Tests |
|---|---|---|---|
| **E. FM6 live operator level** (audit F6) | `fm6_core.c` `fm6_live_level` replaces 1.1's `fm6_note_ol` (`fm6_core.c:496-517`, still destructive in 1.1) | JRNY moves operator levels | `jam_ready_test.c:test_fm` cases (repeated zero/nonzero OL, silent operators, key/velocity attenuation, all-zero envelopes), split into their own group |
| **F. ANALOG slope-fade restart** (audit F6) | `voice.c` (3 lines, `71654ba`) | spike 28.2 → 1.9 dB in a held-chord morph | its `reliability_test.c` case; recheck against the S2 crossings |
| **A. External MIDI clock** | `midi_grid.c`, `midi_clock.c`, `seq.c` remainders / `midi_grid_reset`, the TRS-polling part of `timer5.c` (UART polled while nested in audio), **`hal/fm1_uart.h` UART DMA fix** (audit F7.6) | TR-6S clock | `clockdrift`, `clockguard`, `uarttimer`, `uartdma`. These are software checks; a TR-6S tempo hold on the unit is the real test |
| **B. PHYS tuning** | `phys_dsp.c`, `phys_symp.c` | PLUCK +56 / HARP +31 cents | `physintune` |
| **C. Unison start/level** | `unison_start.c` (`voice.c`), `unison_gain.c` (`eng_fm6.c`), NOISE seeds (`eng_noise.c`) | level jumps | `unisonlevel` (normal + zero DETUNE) |
| **D. Defined arithmetic** | `phys_symp.c`, `eng_wave.c` | UB | strict runs |

1.1 already keeps `fm1_ms` running during the render, so that part of the old `timer5.c` isn't needed (audit F7.6).
Fix order by risk: E, F (journey correctness) before A-D.

### 2.3 Drop

| Dropped | Effect |
|---|---|
| Clipper bank (192 slots) | 128 slots |
| Live graphs, JAM footer/page restyling beyond the two pages | 1.1's design |
| Screen reader, Frenkieboy boot logo | — |
| SEQ COPY/PASTE, REVERSE holds, independent POLY held notes (`seq_hold.c`, `seq_slot_meta.c`) | whole-chord holds of shape-follow become ties (§4.3) |
| Mixer `mix_gain_q12`, FM6 AMP ENV routing (`voice_env.c`), NOISE clock | 1.1 has its own mixer fix and FM6 AMP ENV |

## 3. The plug

1. `DUELL_JAM` (default 1 on `jam11`; 0 for the proof build).
2. `jam.h` in `felucca-base/firmware/src/` holds every hook JAM offers to DUELL's files. With `DUELL_JAM == 0` each
   hook is an empty inline function or gives the plain value.
3. JAM code lives in its own files, included where today's unity build includes them, inside `#if DUELL_JAM`. MORPH
   TO, MUSICAL glue and the FM6 target consumer go into a new `morph_jam.c`.
4. Hook lines in DUELL files are marked `/* JAM hook */`. The aim is few, small hooks, but **the count isn't an
   acceptance rule** (audit F7).

### 3.1 Hooks (checked against 1.1 where the audit did; re-grep in each slice)

| # | DUELL file | Hook |
|---|---|---|
| H1 | `seq.c` | `#include "seq_morph.c"` |
| H2 | `seq.c` | played-step adapter at the six playback reads of `seq_steps(t)`: **1.1 `seq.c:327, 1145, 1229, 1270, 1325, 1330`**. Not at the stopped preview (`seq.c:1219`): preview plays the Original, as JAM does today. Not a global `seq_steps` replacement, because the editor/UI show the Original. Keep `motion_step` / chance ordering intact. **Audio path.** |
| H3 | `seq.c` | VARY BARS bar counter: reset on PLAY, bar-turn flag per block |
| H4 | `morph.c` | includes of `morph_music.c`, `morph_fm_shape.c`, `morph_jam.c`, `morph_xfer.c` (S2), `morph_complement.c` (S4) |
| H5 | `morph.c` | **MUSIC at the roll operation, not in `morph_roll_step`** (§4.1, audit F3) |
| H6 | `morph.c` | factory End B when a sound loads (§4.2) |
| H7 | `morph.c` | `tok[2][MORPH_HIST]` beside `rdep`; truthful depth (§4.1) |
| H8 | `morph.c` | FM6 target consumer: non-blended FM6 fields of a MORPH TO target, `mxs[].side` (audit F1) |
| H9 | `fx.c`, `voice.c`, `eng_fm6.c` | S2 crossings: valley/wash in the mix block, `morph_xcut` in the filter cut, FM6 bridge. **Audio path, hot block** (§4.8) |
| H10 | `ui_layer.c` | `#include "ui_jam.c"`; JAM, JAM LIST in the HOME round |
| H11 | `ui_graph.c` | JAM/JAM LIST draw dispatch and redraw signatures; SEQ MORPH ghost steps |
| H12 | `ui_draw.c` | footer: both parts on JAM / MORPH |
| H13 | `ui_input.c` | JAM knobs, OCT−+knob reset, JAM LIST OCT±, LINK, MIDI routes, and **one `jam_poll()`** in the main loop covering `glide_poll`, **`seqm_poll`** (rebuilds ends after TRANS/SHIFT/edits; audit F7) and the settings lifecycle (load at boot, dirty/debounce save, `erase_busy`) |
| H14 | `ui_tools.c` | **a complete KEEP**: 1.1 has no `TL_KEEP` (`ui_tools.c:20`), so add the tool entry, KEEP's sound half (`jam_keep_sound`), `seqm_keep` (materialize the pattern) and the pattern undo (audit F7) |
| H15 | `ui_perform.c` | GLIDE TO A/B on PERFORM |
| H16 | `midi_control.c` | MIDI GLIDE zones |
| H17 | `storage.c`, `upreset.c`, `erase_data.c` | `OBJ_JAM` on object 48's sector pair; `PT_NCH` 21 (§4.5) |

## 4. Decisions inside the port

### 4.1 MUSICAL roll vs 1.1's chained ROLL (audit F3)

In 1.1, `morph_end_pure` masks `cfg` to `MC_SET | MC_STEREO` (`morph.c:1903`), so `MC_MUSIC` never reaches
`morph_roll_step`. After the step it always copies `fst` and runs `morph_fm_roll` (`:1905-1907`). `morph_cfg_of`
(`:1714`) doesn't carry MUSIC either. So a branch inside `morph_roll_step` can't keep JAM's behaviour.

Plan: decide MUSIC **at the roll operation** (`morph_roll_from` level). A MUSICAL roll:
- passes the flag down, sets and resets `mm_x` around the build, and produces both the ordinary end and the FM6 end
  JAM's way (SHAPE coordinate, `morph_fm_shape.c`; no `morph_fm_roll`);
- always starts from the Original, never from `st`;
- publishes truthful metadata: a WANDER end depends on `mm_walk` / `mm_x` / `mm_dv`, not only Original + seed, so
  its depth must **not** claim that the seed regenerates it. Use a depth value (or flag) that `morph_blk_make` won't
  try to regenerate (no production caller found in 1.1, but keep it honest). Exact-value saves (`morph_bv_make`)
  carry the end as values;
- the amount-zero branch of `morph_roll_from` must not overwrite a MUSICAL entry's metadata with the old one.

SMART/WILD (MUSIC off) stay exactly 1.1. FM6 envelope: 1.1 adds carrier AMP ENV offsets (`fm6_env_offsets`). JAM's
SHAPE also moves carrier rates, so the port must define one order (SHAPE moves the patch, AMP ENV offsets apply on
top, once) and test that no envelope move is applied twice.

Tests: repeated WNDR arrivals, two UNDOs after MUSICAL and after SMART, amount zero, saved exact End B reload,
settings restore, first cold-boot settings pass.

### 4.2 Factory End B (resolves the contradiction the audit found)

JAM builds leave out `DS_ENDB` (9,180 B) and `DS_ENDB_AT` (258 B). The only production reader is `morph_rebase_part`
(audit). So in JAM builds a **factory** sound's B is always rolled: MUSICAL when MUSIC is on (JAM's default), SMART
(as in 1.0) when it's off. 1.1's exact factory End B exists only in plain builds. **Saved user End B (exact values)
is kept in every build.** This is the one 1.1 feature JAM gives up; it goes on the cheat sheet.

### 4.3 Held notes → 1.1 ties (audit F2)

The kept code reads `cont` (`seq_morph.c:64, 219`, `seq_evolve.c:29, 48, 67`) and **writes** it (`seq_morph.c:123,
225`, `seq_rhythm.c:86`). 1.1's `step_t` has no `cont`. `glide_shape_follow` turns on `sqm_shape_hold` for FM6
WNDR+BOTH, which copies onset notes into the following cells marked as continuations. A stub would turn those into
new attacks, or into silence (probability 101).

Plan: shape-follow writes **`ST_TIE` cells** (1.1's whole-step tie) instead of continuation cells, for whole-chord
holds only. Reads of `cont` become "is this cell an `ST_TIE` following a note". Independent per-voice holds stay
dropped. Tests: onset count, sustained duration, missed-onset behaviour, before and after.

### 4.4 Smooth crossings (owner decision, audit F1)

`morph_xfer.c` comes back in slice S2: FM6 bridge, valleys, cut handoff and the wash with its JAM LIST row WASH LEVEL.
It's measured as one slice. If it doesn't fit, cut in this order: (1) the wash and WASH LEVEL, removing the whole
wash path including `glide_motion`'s use of `XWL` / `mxs[].motion/follow`, not just `xrise`; (2) move crossing audio
functions out of the hot block into flash (works, costs CPU; measure load); (3) ask the owner. Acceptance: forward and
backward FM6, filter-type and insert transitions, compared with JAM 1.0 renders of the same pairs.

### 4.5 Settings storage: `OBJ_JAM` (owner decision, audit F5)

- **Where:** object 48's A/B pair, 0xC4000 / 0xC5000, the last patch chunk. `PT_NCH` 22 → 21.
- **Cost:** chunks take a new record only under `PT_K` = 6 records (`upreset.c:865`, `pt_put_enc:926-929`), so
  **at most 126 distinct saved patches, whatever their size**. Owner accepted. All 128 factory slots stay
  browsable. SAVE on a 127th distinct slot shows STORE FULL. Change the `_Static_assert` with this exact reason, and
  add a test that the 127th save fails cleanly.
- **Format:** `"JAM"` magic, version byte, fields (MODE, TIME, TARGET, WAIT, VARY BARS, MUSIC/SMART, WASH, JAM LIST
  rows). 3,840 B of room.
- **Lifecycle** (audit answer 1): load at boot with defaults on a missing/invalid record; dirty flag + debounce
  before `st_save` (no write per knob turn); skip while `erase_busy`; ERASE DATA already covers object 48 (audit).
- **Plain 1.1 on a JAM unit:** `pt_scan_img` rejects the non-`PT` payload without clearing it (audit). Saving patches
  on plain 1.1 may overwrite it, which only loses JAM's settings.
- **Backup:** the editor backup doesn't list object 48. JAM settings are outside backups (accepted, documented).
- First `jam11` boot: **ERASE DATA** once, which clears 1.1's [15][22..23] bytes that still hold JAM 1.0 values (seen
  on the unit: [23] = 0x69 → NOTES LEFT OFF).

### 4.6 Saved data on the unit

Owner: nothing needs keeping. Plain 1.1 booted over JAM 1.0 data without error: 0 saved patches; the old autosave
was kept as raw data by the editor. ERASE DATA at the first `jam11` boot.

### 4.7 SEQ MORPH vs 1.1 sequencer features

- Parameter locks: separate from steps. Locks stay on their step index when SEQ MORPH moves a note (documented, tested).
- TRANS ± / SHIFT < >: change the Original; `jam_poll` → `seqm_poll` rebuilds the ends (H13). Test: no old notes
  sound after TRANS during play.
- Swing 50-75 %: dialogue uses production `step_samples`, so no separate range to fix (audit).
- STRUM: scheduled downstream of steps; it creates no extra step records (audit). Nothing to change.

### 4.8 Space (measured plain 1.1, corrected per audit F4)

| Region | plain 1.1 | Usable for JAM | What JAM needs (estimate, from JAM 1.0 symbols; real numbers come from each slice's link) |
|---|---|---|---|
| App flash slot (581,564 B) | 574,280 B (566,456 B with font compaction) | 7,284 B; **15,108 B** with font compaction; + 9,438 B from `DS_ENDB`/`_AT` = **~24,500 B** | S1+S3 ~15-17 KB, S2 crossings: unmeasured (a few KB), S4 extras ~2-3 KB → likely fits; measure each slice |
| Call reach (hot block → end of `.text`) | 0x3fe228 (0x3fc398 with fonts) | 7,640 B; **15,464 B** with fonts (+ whatever is removed) | grows with flash; check each slice |
| Hot block (`.ram_text`) | 12,172 B (11,180 B with hot room) | 112 B; **1,104 B** with hot room (physical room to boot_info minus `HOT_HEADROOM` 228, `elfcheck.py:245, 392-393`) | S2 crossings + fix C: unmeasured |
| Range X | 30,276 B (31,268 B with hot room) | 2,236 B; **1,244 B** with hot room | fix B adds `px_atan2t`, `px_atan` (JAM 1.0's list); measure |
| Pool | 8,344 B free (9,880 B with the font cache in `.bss`) | 152 B; **1,688 B** with fonts (8,192 B reserve, `POOL_TAIL_MIN`) | SEQ MORPH buffers `sqm_buf` 576 + `rhythm_map` 128 + `rhythm` 20 = **724 B** → fits with fonts; `.bss` still preferred |
| `.data` + `.bss` | 59,632 of 73,728 B (63,760 B with the 8-slot font cache) | **5,872 B** with fonts (73,728 − 63,760 − the 4,096 B reserve, `tools/build.py:460`; re-audit) | see the RAM ledger, §4.9 |

**Lossless font compaction (measured 2026-11-01; `work/audit/DUELL-1.1-COMPACTION.md`, rebuilt and confirmed
here).** Two files: `tools/gen_aa_font.py` Huffman-codes the faces T, N, X, B and P too (as M and L already are; S
labels and D digits stay plain), and `gfx.c` moves the decoder cache from `.pool` to `.bss`. Glyph pixels and
metrics are identical (the audit compared all 1,392 glyph entries and 11,136 render cases). Use the **8-slot
variant** (`work/audit/compact11/DUELL-1.1-lossless-size-8slots.patch`, `HC_FONTS` 3 → 8): with 3 slots, 8 Huffman
faces share them and the last slot is rebuilt (256 entries) on every face change while a page draws. 8 slots
remove that for 2.6 KB of ordinary RAM.

| | plain 1.1 | + fonts, 3-slot cache | + fonts, 8-slot cache (chosen) |
|---|---|---|---|
| App image / free | 574,280 / 7,284 B | 566,456 / **15,108 B** | 566,456 / **15,108 B** |
| Call reach margin | 7,640 B | 15,464 B | 15,464 B |
| Pool + Z free (8,192 reserve) | 8,344 B (152 spare) | 9,880 B (**1,688 spare**) | 9,880 B (**1,688 spare**) |
| `.data` + `.bss` (of 73,728; 4,096 B must stay free) | 59,632 B | 61,184 B | 63,760 B (**5,872 B usable**) |
| Hot block | 12,172 B (112 usable) | unchanged | unchanged |

With `DS_ENDB` left out too (§4.2): **about 24.5 KB of flash for JAM**, against the 15-17 KB estimate for S1+S3
plus S2 and S4. **Checked on the unit 2026-11-01:** the owner flashed the 8-slot build, plain 1.1, then the 8-slot build
again, and found drawing "feels the same" (package `work/checkpoints/DUELL-2026-11-01-1.1-fonts8/`, SHA-256
`606e5a50…e55b02`; superseded on the unit by the hot-room build). That's a judgement by feel, not a timing measurement. Recheck once
`jam_poll` runs in the same main loop. Goes into step 0 as its own commit.

**Hot-room placement (measured 2026-11-01; `work/audit/DUELL-1.1-FAST-RAM.md`, rebuilt and confirmed here on top
of the font compaction).** `tools/hot_fns.py` and `tools/ram_fns_x.py` move `eq_run` (214 B) and `ins_swept` (778 B)
from the hot block into spare range X, in two-core builds only. Both stay in on-chip RAM, and the compiled code is
unchanged (same IR hash per the audit); only the placement moves. Measured: hot audio 12,172 → **11,180 B** (29
functions), 1,332 B physical room, **1,104 B usable**; range X 30,276 → 31,268 B (**1,244 B free**); image, pool and
call reach unchanged. All build checks pass (closure, start-up copy, PC window, reserves). Flashed 2026-11-01. RAM placement
can change timing. **On the unit (owner, 2026-11-01):** plays as before by ear; M16 STRESS TEST about 70 % CPU,
peak 80 % (MENU > SYSTEM > ABOUT; 1.1 has no CPU bar), UI a bit slower under that load, no dropout or crash. No
fonts-only figure was taken; **the owner judged the comparison unnecessary and adopts it.** Goes into step 0 as its own commit
after that check.

**Build environment (found in step 0).** The step-0 build is 112 B smaller than the flashed hot-room package
(566,344 vs 566,456 B), with identical sources. Cause: today's flashed builds (plain 1.1, fonts8, hot room) were made
with `JAM_WORK=C:/...` exported before sourcing `env.sh`. Git Bash splits PATH at the drive colon, so the venv and
FriBiDi dropped out, Pillow ran without Raqm ("Raqm layout was requested, but Raqm is not available" in those logs),
and several glyphs were drawn differently (e.g. face L's digit advance 12 px instead of 13; S/M/L/T/N/X/keycap data
sizes differ). Every JAM 1.0 build and the step-0 build have Raqm. `jam11`'s `env.sh` now normalizes `JAM_WORK`. The
owner's drawing-speed and CPU judgements on those builds still hold (same code paths), but the unit's text rendering
differs slightly from the branch until the step-0 package is flashed. Today's rollback checkpoints stay as they are
(they are what was flashed). Figures above come from the non-Raqm builds unless marked; the step-0 baseline is the
reference from here on.

Space plan, in order:
1. **Pool:** with the font cache out of the pool there are 1,688 spare bytes, enough for SEQ MORPH's 724 B. Still
   prefer `.bss` for main-loop buffers. Check the closure (no range-X/Z code reads them) in the real link.
2. **Flash:** the font compaction (above) and leaving out `DS_ENDB` (§4.2). Keep USB audio (owner).
3. **Flash, if still short:** slice S4 (extras) only if it fits; then the splash logo (~6 KB, plain splash text
   instead); then the 1.1 first-start guide (~1.2 KB). Each one is an owner decision when reached.
4. **Hot block:** with the hot-room placement (below) there are 1,104 B usable. Crossings and fix C go into the hot
   block only as far as they fit; the rest stays in flash. Measure the CPU load on the unit (BENCH or load figures)
   for each one.
5. Keep every existing check: `HOT_HEADROOM` 228, the 8 KiB pool reserve, call reach, range X closure.

AGENTS.md's "hot code ≤ 12,032 B" rule is replaced by "the build's own checks pass (`HOT_HEADROOM`, pool reserve,
call reach), and each slice reports its deltas against the phase-1 baseline". AGENTS.md is updated in phase 0.

### 4.9 Before S1 (re-audit, `work/audit/MIGRATION_1_1_REAUDIT_3_2.md`)

1. **Full FM6 destination: persistence and KEEP (P1).** 1.1's MBV block saves 65 blended FM6 bytes (packed into 57,
   `morph.c:2139-2150`, `morph_bv_make:2282`, `morph_bv_load:2327`). A JRNY destination with another algorithm,
   frequency mode/coarse/fine or detune lives in JAM's `morph_fmto`, selected by `tok`, and doesn't round-trip through
   MBV. `jam_keep_sound` copies only `morph_view`'s ordinary parameters. To specify before S1: how saved patches,
   multis and the autosave keep the target structure (metadata or a format extension, with stock MBV records still
   loading), and KEEP adopting the effective FM6 patch before it rebases. Re-measure record maxima and chunk capacity
   instead of assuming 577 B / 6 per chunk. Tests: algorithm, frequency fields, all blended bytes and the rendered
   output across save/load and KEEP at A, the midpoint and B. The generative session and history aren't saved.
2. **RAM ledger (P2).** Usable: `.data`+`.bss` **5,872 B**, pool **1,688 B**, range X **~906 B** after fix B's
   ~338 B (reference figures), hot **1,104 B** for changes to existing hot functions and fix C only. Old JAM state
   to place: SEQ MORPH ends 2,304 B (2 parts × 2 ends × 64 steps × 9 B), `sqm` control/history 504 B, SEQ scratch
   576 + rhythm 148 B, FM6 targets 620 B, bridge state 712 B, `mxs` 128 B, `mxv` 208 B, transition-plan scratch ~936 B
   (`EXT_P_LEN` 156), plus target-building, KEEP, GLIDE and fix state. Record persistent state and main-loop scratch
   separately, choose a region for each, overlay scratch only with checked lifetimes, keep the 4 KiB `.bss` and
   8 KiB pool reserves, and budget S1-S3 together before S1/S2 spend the room.
3. **Ties (P2).** Stock `seq_step` checks `SEQ_MISS`/chance before `ST_TIE` (`seq.c:1151-1157`), and the old
   shape-follow writes probability 101. Generated ties get normal chance, with no stale notes or ratchets. The H2
   adapter also serves `next_tie`'s lookup (stock reads the Original). Sequence chords already start together, and
   STRUM is keyboard/MIDI only, so no strummed-sequence behaviour is promised. Tests: chance-0 onset, normal onset,
   gate/swing, successive ties, a variation change at a tie boundary.
4. Re-audit answers adopted: MUSICAL ends are saved as exact values with `rdep >= 2` (no new depth enum); the named
   crossing helpers (4,100 B: `morph_xstep` 1,914, `fm6_xblock` 730, ...) are flash code in JAM 1.0 and stay there, so
   the 1,104 hot bytes aren't spent on them; `morph_fm6` stays in range Z.

### 4.10 S1 design (2026-11-01, answers §4.9; the feature is now called **TIDE**, JAM before)

**Name.** Owner: "we are not jamming": the generative auto-morph is **TIDE** (it moves by itself between two shores, A
and B, never quite the same, always back; a hand can still push it). Pages TIDE and TIDE LIST; switch `DUELL_TIDE`;
code files `tide.h`, `tide_seq.c`, `tide_ui.c`, `morph_tide.c`. The workshop names stay (`jam/` tools, branches
`jam`, `jam11`).

**Roll (H5).** In `morph_roll_from`, before `morph_end_pure`: `TIDE_ROLL(...)` (off: `0`). When the end's cfg has
`MC_MUSIC` (bit 64; `morph_cfg_of` adds it via `TIDE_CFG_BITS`, off `0u`), `tide_roll` builds the end from the
**Original** (never `st`) with the dials (`mm_x` set and reset around it) and the FM6 end by `morph_fm_shape`
(no `morph_fm_roll`), publishes `rdep = 2` (values, not regenerable from a seed) and skips 1.1's amount-zero
metadata branch. SMART/WILD and UNDO stay 1.1's.

**Factory End B (H6).** `morph_rebase_part`: with `DUELL_TIDE` the `DS_ENDB` table is left out; a factory sound's B
is a MUSICAL roll at `MM_AMOUNT` (MUSIC on) or a SMART roll at the category amount (MUSIC off). A saved user B
(MBV) loads 1.1's way.

**FM6 destination, persistence (H8, §4.9.1).** The target's structure (algorithm, ratios, detune, ...) is held in
`morph_fmto[pi][x]` and adopted at the transition's side by `morph_fm6`. Saved: a new MBV flag bit **MBV_FMT** with
3 bytes after the FM6 bytes: the factory index + 1 of the target and a 16-bit checksum of the target's non-blended
bytes. On load (`morph_bv_load`, also multis and the autosave, which use the same block) the target patch is
rebuilt from its SLOT (`fm6_slot_patch`) and kept only when its checksum matches (a changed user voice bank then
loads the blended bytes only, as 1.1 does). Stock MBV records load unchanged. Under `DUELL_TIDE` only; `MBV_MAX` +3,
re-check `PT_WORST` and `PT_K` in the build (`_Static_assert`s stay).

**KEEP (H14, §4.9.1).** A new tool entry TL_KEEP (1.1 has none) runs: the sound half (`tide_keep_sound`: the
effective values at the fader, and on FM6 the effective patch, non-blended bytes from the sounding side, written as
the part's own patch before the rebase), then from S3 the pattern half and its undo.

**RAM (§4.9.2).** Measured per slice from the link (`.data`+`.bss`, pool, X, Z, hot) against step 1. S1's own state:
`morph_fmto` 2 × 2 × FP_SIZE, `tok` 2 × MORPH_HIST per part, GLIDE state, the dials (`mm_*`). The SEQ MORPH ends
(2,304 B) go to `.bss` in S3; the crossing state (S2) after that. A slice that would leave less than the reserves
stops and reports.

**Disabled adapters in S1.** GLIDE's SEQ lane, SEQ MORPH queries, the wash/crossing terms and the contrast pick are
stubs that report "none", so S1 plays the sound journey only.

## 5. Phases and slices

Each step is one or a few commits, builds, passes its tests, and records its space deltas here.

| Step | Work | Done when |
|---|---|---|
| **0 Base** | `jam11` from `974d875`; Windows toolchain lines; font compaction (8-slot); hot-room placement; `jam/` tools (`env.sh` finds and normalizes `JAM_WORK`); AGENTS.md space rule; Windows baseline test list | **Done 2026-11-01:** `1d29995`, `0c48565`, `ac0fee1`, `aa55e22`, `33ea81a`. AGENTS.md updated. Build: image 566,344 B, hot 11,180 B, `.data`+`.bss` 63,760 B, call reach 0x3fc328. Baseline (`docs/JAM11_TEST_BASELINE.md`): 47 pass, 63 fail, 11 skip. Most failures are upstream 1.1's own tests, which no longer compile against its firmware; `jam_dev`'s mock fixtures are 1.0-format (the tool itself works on the 1.1 unit) |
| **1 Fixes** | E, F, then A, B, C, D, each re-applied on 1.1 with its tests | **Committed 2026-11-01**, each test failing on stock 1.1 and passing with the fix. D (FM6, `d445608`): defined arithmetic in `fm6_core.c` (stock stops under UBSan in `fm6_scale_vel`). E (`b6b669e`): `fm6liveol` 36 checks, stock fails 9. F (`67ea2ec`): `slopehandoff`. B (`baeec20`, with D's PHYS SYMP / WAVE part): `physintune`, stock PLUCK +57 cents, HARP +20..+34; one conflict with 1.1's own PHYS change (BRIGHT/DAMP from the key), both kept. C (`1c56523`): `unisonlevel` + zero DETUNE, stock fails 11 (TRIO +8.4 dB, NOISE +9.2, FM6 mean +7.2 over POLY). A (`a227e4a`, clock parts only, JAM's sequencer changes left out): `clockguard`, `clockdrift`, `uartdma`, `uarttimer`, all four fail on stock (a division by zero, Felucca #282, guarded in `fx.c beat_samples`; ARP and sequence phase lost on tempo changes; UART CON0 width; the timer module). Space after step 1: image **568,588 B** (+2,244 over step 0), `.data`+`.bss` 63,792 B, hot **10,976 B** (−204), range X **31,584 B** (928 free), call reach 0x3fcb88. Not in step 1: JAM's two-core fault watch in `main.c` (`tc_closed`/`tc_watch`), not in the plan's scope. **Regression:** `check-duell-jam11-fixes` 56/56 pass (baseline 47 + `fm6liveol slopehandoff signedfm physintune unisonlevel clockguard clockdrift uartdma uarttimer`); package checkpoint `work/checkpoints/DUELL-2026-11-01-jam11-step1-a227e4a/` |
| **2 Plug** | `DUELL_JAM`, `jam.h`, hook lines with empty stubs | **Split (2026-11-01).** **2a done** (uncommitted, regression running): `jam.h` (included at the end of `core.h`), `DUELL_JAM` passed by `tools/build.py`, stubs `jam_seq.c` (H1, end of `seq.c`) and `jam_ui.c` (end of `ui_input.c`); hooks H2 at all six played-step reads of 1.1's `seq.c` **including `next_tie`** (§4.9.3), H3 in `seq_start` and after `beat_adv` in `events_block`, H13 `JAM_POLL()` after `morph_poll()` in `ui_input`. **`DUELL_JAM=0` is byte-identical to step 1** (package SHA-256 `b081f24e…` both); the stubbed `DUELL_JAM=1` build is the same size (stubs inlined away). **2b, with the §4.9 design before S1:** the morph hooks (H4-H8: roll operation, factory End B, `tok`, the FM6 target consumer), storage (H17, `OBJ_JAM`) and UI hooks (H10-H12, H14-H16), because their form depends on that design. Same rule: `DUELL_JAM=0` stays byte-identical to `b081f24e…` |
| **S1 Journey** | GLIDE, MUSICAL (§4.1), MORPH TO + FM6 target consumer, full FM6 destination persistence and KEEP (§4.9), JAM + JAM LIST pages, `OBJ_JAM`, `jam_poll`, PERFORM/MIDI hooks; `DS_ENDB` out; **explicit disabled adapters** for the sequence (S3) and bridge (S2) state so S1 can be auditioned on its own | generative acceptance (§6) for sound, **without** structural-switch acceptance (that is S2); emulator shows both pages; space within the §4.9 ledger. The S1 audition is not the finished smooth journey |
| **S2 Crossings** | `morph_xfer.c` (§4.4) | forward/backward structural transitions compared with JAM 1.0; space within limits, or the §4.4 cut order applied |
| **S3 Sequence** | SEQ MORPH, rhythm/phrase/evolve; ties (§4.3); played-step adapter (H2); buffers in `.bss` | sequence acceptance (§6); pool reserve kept |
| **S4 Extras** | contrast, voice leading, dialogue | only if space allows; each one alone |
| **6 Release check** | full build, package smoke, all space checks, emulator walk-through | numbers recorded; owner review |
| **7 Flash** | §7 | unit boots, JAM plays, owner listens |

Audition on the unit after S1 and after S2 (owner-asked flashes, same procedure): the journey is judged by ear,
not only by tests.

## 6. Tests

- Windows with `jam/env.sh` (`JAM_WORK` explicit). Compare against step 0's baseline list, never "all green".
- **Split old groups by kept behaviour** (audit): `jamready` keeps its FM6 (fix E) and GLIDE checks, minus
  continuation/REVERSE. `recallenv` keeps exact recall, minus AMP DEST/AENV routing. `seqfeatures` is mostly
  dropped features: keep only what tests kept code. No useful assertion is deleted to go green, and every removal is
  listed in its commit.
- **Generative acceptance:** A→B→A repeated travel; only the silent end is replaced; exploration continues (not a
  fixed two-patch loop); independent sound and sequence targets; WAIT, stop, resume, manual takeover; forward and
  backward structural transitions; bounds over all allowed engines/categories; exact recall of saved End B.
- Fix groups: `physintune`, `unisonlevel`, `clockguard`, `clockdrift`, `uarttimer`, `uartdma`, fix E's FM6 cases,
  fix F's case. Strict (ASan/UBSan) for B, C, D, E and the played-step adapter.
- 1.1's own new tests (swing, STRUM, locks) keep passing.
- `DUELL_JAM=0` compared with step 1 at every later step.

## 7. Flash procedure

1. Unit charged, data cable straight into the PC, PC kept awake. Flash only when the owner asks in that task.
2. Backup: `"$JAM_WORK/duell-win-venv/Scripts/python.exe" jam/jam_dev.py backup` (system `python` lacks mido).
3. `jam_dev.py flash PACKAGE.fwsc` (backs up again, runs `fm1_install.py`). Never interrupt.
4. First `jam11` boot: INFO answers; MENU > ERASE DATA once.
5. Check: HOME round JAM, JAM LIST, MODE, MORPH; JRNY with TIME and WAIT; TR-6S clock holds tempo for several
   minutes; JAM settings survive a power cycle.
6. Way back: the immutable plain-1.1 package or the JAM checkpoint (§1), through the same installer.

## 8. Upstream (later, owner-authorized only)

Fixes A-F could be offered upstream after the port, one per PR, after asking on an issue whether PRs are welcome.
This plan doesn't authorize sending or publishing anything (audit).

## 9. Audit response

| Finding | Status in revision 3 |
|---|---|
| F1 crossings + FM6 target consumer | accepted: S2 brings `morph_xfer.c` back (owner); consumer is H8, in S1 |
| F2 `cont` writes, shape-follow | accepted: ties (owner), §4.3 |
| F3 MUSIC hook level, metadata | accepted: §4.1 |
| F4 112 B hot, 152 B pool, 724 B SEQ buffers | accepted: §4.8, buffers to `.bss` |
| F5 126-record limit | accepted by owner, §4.5 |
| F6 FM6 live level, ANALOG slope fade | accepted: fixes E and F |
| F7 `seqm_poll`, complete KEEP, no fixed hook count | accepted: H13, H14, §3 |
| F8 immutable rollback | done: §1 checkpoint with SHA-256 |
| Space audits (2026-11-01) | `work/audit/DUELL-1.1-COMPACTION.md` (fonts) and `work/audit/DUELL-1.1-FAST-RAM.md` (hot room): both rebuilt and confirmed here, flashed, accepted by the owner (§4.8); fonts adopted with an 8-slot cache instead of 3 |
| Answers 1-8 | folded into §2.2 (UART DMA), §3.1 H2 (line numbers, preview), §4.2, §4.5 lifecycle, §5 step 2 |

Open questions for the next audit pass (revision 3.2):
1. §4.1: is a dedicated depth value/flag for MUSICAL ends the cleanest way to stop seed regeneration, or should
   MUSICAL ends always be stored as values?
2. §4.3: does `ST_TIE` on a whole-chord step keep every voice of a chord, including under STRUM and chance?
3. §4.8: which crossing functions (S2) are hot in JAM 1.0, and how many bytes does each add, now against 1,104 B usable?
4. §4.8: with `eq_run` and `ins_swept` in range X (1,244 B left), do fix B's `px_atan2t` / `px_atan` and any S2
   range-X additions still fit together?
5. §4.8: the 8-slot font cache (2.6 KB of `.bss`) plus SEQ MORPH buffers if moved to `.bss`: is the ordinary RAM
   budget (~10 KB free) still comfortable with the S1-S3 state?
