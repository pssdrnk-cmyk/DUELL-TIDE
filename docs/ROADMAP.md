# DUELL with TIDE roadmap

Updated 8 November 2026. This page says where TIDE stands, what comes next, in which order, and what waits on a decision. What is already built is in the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md), one section per build. The roadmap before this reorganisation, with its full evidence and receipts, is [history/ROADMAP_2026-11-08.md](history/ROADMAP_2026-11-08.md); older ones are in [history/](history/). The audits of 8 November are in [audits/2026-11-08/](audits/2026-11-08/): the [live-record audit](audits/2026-11-08/liverec/LIVEREC_AUDIT.md), the [CPU audit](audits/2026-11-08/cpu/CPU_AUDIT.md) with its task briefs K1 to K12 ([TASKS.md](audits/2026-11-08/cpu/TASKS.md)), and the [round-2 code audit](audits/2026-11-08/full/AUDIT_REPORT.md) with its findings 1 to 39.

## Where things stand

| | |
|---|---|
| **Stable** | `stable-2026-11-03` (`187e9d0`), validated by the fork owner. The first versioned stable will be `1.0.0`. |
| **Newest beta** | `1.0.0-b2` (tag `tide-1.0.0-b2`, `0d2fcb9`), a GitHub pre-release since 8 November: the GLIDE CPU fix and eight drones (#164 to #171). The owner's unit runs the build just before the tag (`36f03cd`, ABOUT `1.0.0-b2+`); the tagged build adds only the every-block blend for a hand-moved morph. |
| **Older betas** | `1.0.0-b1` (`817658d`) and `beta-2026-11-04` (`77a6df2`), both GitHub pre-releases. |
| **Development** | b3 is in pull requests against `jam11`, not merged yet: [#1](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/1) b3 part 1, [#2](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/2) PLAY MODE / XFADE / SPLIT rows, [#3](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/3) REC ADD / REPLACE and ARP SEQ keys, [#4](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/4) the `emu` CI job. Each PR runs every host test group on GitHub. Built and reviewed, waiting for #1 to #3: the POLY restart fix (`fix/b3-f36`), the `uartflood` test (`fix/b3-felucca264`), the `tcwrap` test (`probe/a1`). In its review round: the CPU meter (`fix/b3-k2`). |
| **Flash** | 580,204 B of the 581,564 B slot (build of `fix/b3-rec-modes`): **1,360 B free**. Every addition is measured; simplifications pay for features. Keep 0xEA000..0xFBFFF for future pattern / multi slots. |
| **CPU** | About 3 missed audio blocks per FM6 crossing during a GLIDE, accepted for the betas. Five multis peak at raw 91 to 101 % in the 16-multi GLIDE sweep. `1.0.0` needs 0 missed blocks over that sweep. |

## How we work

- **One audio change per beta.** A change that can alter the sound gets a beta of its own, so a problem points at one change.
- **Measure before and after.** An audio-path change is measured on the unit before and after, with the same script, against a pass mark written down first.
- **Builder and four reviewers.** Each change is built by one agent on its own branch, then read by separate reviewers: correctness, quality and cohesion, tests, docs. The coordinator decides the fixes; a verification review checks them.
- **Pull requests.** Every change goes to GitHub as a pull request against `jam11`; GitHub runs the full host suite on it. Locally only the device build (size, hot-code check), `jam/check_off.sh` and `check_docs.py`. Builders work in parallel; merges go one at a time, with the owner's OK.
- **Never flash red.** A tagged beta is never flashed while the full suite or `jam/check_off.sh` is red. The tag comes before the build; the flashed commit is merged and pushed the same day.
- **Plain DUELL stays identical.** `DUELL_TIDE=0` must match `jam/off.sha256`; re-pin it only for an explained shared-code change. The `partindep` pins are never regenerated.
- **Formats and user data are kept** (patches, patterns, multis, settings, KEEP / UNDO) unless the owner decides otherwise.
- **No new pages.** TIDE options are rows on TIDE SETTINGS with good defaults; restyles keep every fader bar, note and shown value.
- **Technical choices are the coordinator's**, with audio quality first, then risk and effort. The owner decides what is heard, seen and touched.
- **Anything unfinished gets a row here at once.**
- **Pushes and releases need the owner's OK.**

## Next: 1.0.0-b3, changes that cannot alter the sound

| # | Item | What it does | State |
|---|---|---|---|
| 1 | Clean baseline | CI on macOS 15; docs truth pass; `swing` / `duck` in the test list; the `emu` CI job; sound numbers #1 to #163 pinned; the measurement kit in `jam/measure/`; `check_docs.py` in `check.sh` and CI; b1 and b2 GitHub releases. | **Done** 8 November. |
| 2 | CPU and skip counter on the main screens (K2) | The raw CPU peak and a count of missed blocks in the header or footer of the main screens, in every build; `cpu_miss` counts every missed DMA period; one owner of the peak window, so ABOUT, the console and the screens never clear each other's peak. Reads and draws only. | Built and reviewed (`fix/b3-k2`, +392 B); the header look is being redesigned to match the capsules. |
| 3 | Hot code placement check (K6) | The build fails when a function that runs every audio block lands in slow flash instead of RAM range Z (or is not on an allow-list with a reason). Build time only; no image change. | Built (`fix/b3`). |
| 4 | OCT- + K3 / K4 say what they set | On the TIDE page, holding OCT- and turning K3 or K4 resets TIME or WAIT to the STYLE's value; it will show "TIME 16" / "WAIT 2" like K1's BACK TO ORIGINAL. About 40 B. | Built (`fix/b3`). |
| 5 | PATCHES order and numbers | The list: every factory sound (#1 to #171), then your saved sounds (one saved over #13 too), then the empty USER slots last (owner, 8 November); MIDI program numbers unchanged. The footer and the TIDE page number the factory sounds past #124 (they showed the name alone). | Built (`fix/b3`), reviewed. |
| 6 | Felucca 1.7.0 check | #258, #264, #269 and #282 are all in our tree ([comparison](audits/2026-11-08/felucca-264-check.md)); Felucca's clock-under-CC-flood test is ported as host group `uartflood`. No firmware change. | Done (`fix/b3-felucca264`), reviewed. |
| 7 | Small fixes with no sound change | `settings_persist.c` comment on object 48 (finding 33, D48: object 48 stays); the restore confirm says how many patches lose their FM6 MORPH TO target when the unit may be plain DUELL, since INFO cannot tell it from an older TIDE build (finding 14), plus one line in `jam_dev.py restore`'s help; three UI lint scenes (DAMAGED record, ABOUT with the TIDE id, a PATCHES row past #163; finding 31). | Built (`fix/b3`). |
| 7a | Live recording (live-record audit R1, R4, R5) | REC no longer silences the ARP SEQ parts, and an ARP SEQ part's arp no longer records its own notes into its pattern; once armed, a REC tap disarms on every page (on HOME, unarmed, REC stays HOLD and says how to record: owner, 8 November); PRESETS loads sounds on STEP and PARAMETER LOCKS. Host group `liverec`. | Done: `integ` `455e2bb`. |
| 7b | HOME without the MODE page | PLAY MODE, XFADE and SPLIT are the last three rows of TIDE SETTINGS; HOME's round is MORPH, TIDE SETTINGS, AUTO TIDE (owner, 8 November). | Built (`fix/b3-mode-rows`). |
| 7c | Keys record into ARP SEQ patterns; REC row | Keys sound as played and go into the pattern the part plays (a key sets the step's note); REC row ADD / REPLACE on TIDE SETTINGS. | Built (`fix/b3-rec-modes`); waits for the unit test. |
| 7d | POLY restart (finding 36) | On an ARP SEQ part in a shortened POLY loop, releasing all keys and playing again restarted the loop part-way in; it now starts at its first step. Host group `f36`. | Built (`fix/b3-f36`), reviewed. |
| 8 | Unit measurements | The 16-multi GLIDE sweep reading both `cpu_pk_us` and `cpu_el_pk_us` (K2); each drone with its 3-note chord held while `glide 1 0` runs. Sets the acceptance numbers for b4. | Needs the unit on USB. |
| 9 | Cut b3 | `TIDE_VERSION` 1.0.0-b3, CHANGELOG section, tag, build, flash, GitHub pre-release. | After 2 to 8 and the merges. |

## After b3, one audio change per beta

| Beta | Item | What it does | Spec |
|---|---|---|---|
| `1.0.0-b4` | Ease ladder (K3) | When the measured CPU runs hot, back off step by step instead of killing a note: 1 slower transitions (morph blend every 4th block, FM6 retune every other block), 2 no new note for a moment (owner: acceptable live), 3 lighter effects, 4 released tails, 5 held notes only after a real miss. Hysteresis: on after two halves over 85 %, off after ~3 s under 70 %. About 300 B. | Brief K3 |
| `1.0.0-b5` | Effect buses on core 1 (K4) | Core 1 also runs the sends, chorus, delay and reverb when its measured time fits the deadline; today's one-core path stays as the fallback, bit-exact. Saves 7 to 15 % on core 0 in the scenes that burst. | Brief K4 |
| then | Hand-moved morph saving | A hand or MIDI fader move blended every other block like a GLIDE (about 70 us a block saved), with a sample check of the hand-moved morph. | Finding 12, D44 |
| then | Transition costs only while moving | The engine cross-fade (BD_FADE), valley and wash charged only while a transition runs, as b2 does for FM6: more notes at rest. | Owner, 6 November |
| then | FM6 crossing fixes | The deferred stand-in drain cleared only when the bridge stays off (suspected, needs a test that reproduces it first); the crossing charge leads the crossing instead of lagging one UI frame. Then K7: the crossing's shared modulation computed once per part per block. | Findings 8, 9; brief K7 |
| then | Silent bus and master skip (K5) | A bus at exact zero is not run; the limiter and EQ rest after ~1 s of silence. About 6 % at idle. | Brief K5 |
| then | Felucca #265 | Signed DSP arithmetic and the FM6 phase wrap without undefined behaviour, bit-exact. | Felucca v1.7.0 |

**Before `1.0.0`:** 0 missed blocks over the 16-multi GLIDE sweep (owner, 8 November); the budget made honest: the per-sample work added after the 26 October cost calibration (SVF precision, delay low cut and knee, master knee, HALL / PLATE pre-delay) re-measured with `jam_dev.py measure` and charged, `bd_part` timed inside the audio interrupt and moved out if it costs over ~20 us, the blend charged as a part cost (findings 5, 6, 34); then `BUDGET_PEAK` lowered to the measured residual, only after the ease ladder is measured on the unit (K9).

## Planned, not yet scheduled

**CPU and audio engine**
- Effect code from slow flash: [analysis](audits/2026-11-08/flash-hot/REPORT.md) done. Of the 116 functions, 50 run per sample. Plan: first a test-mode probe that times one function from flash and from RAM (no sound change); only if it shows at least 0.5 % of a half, move ~0.9 KB (`dly_wr`, `master_knee`, `acc_sum`, `cho_zrun`, `fx_swing`, `rev_front`, `hall_gains`) and later ~1 KB more, as its own beta measured on the unit. Expected gain 1 to 3 %, not enough alone for `1.0.0`.
- The budget model (`bd_part`, `bd_global`, `bd_slope`, `part_dl`, 2.1 KB) runs from flash at the start of every half: run it only when its inputs change. Larger gain than the above, but it changes behaviour; needs its own design and test.
- K8: the next fader position's blended parameters prepared outside the audio interrupt (main loop or idle core 1) and swapped in by pointer. Large; needs one-writer rules.
- K10: the dearer part (by measured cost) on core 1, not always part 2.
- K11: a TIDE-only 5.8 ms half for background use, costed first (keyboard play keeps 2.9 ms).
- K12: the "prove the CPU cost before flashing" process written into CONTRIBUTING and TESTING.
- Two readers of the arp step ignore a POLY stretch (`ui_draw.c` ~1901, `seq_dialogue.c` ~44, found in the finding 36 review): check whether the display or TURNS can disagree with what plays.
- Code nits with no measured effect (finding 35): `DUELL_PROBE_STAGES` defined outside `#if DUELL_TIDE`; `morph_xside` into RAM only if the unit shows its cost; `glide_stop` lags one poll.

**TIDE interface**
- LFO DEPTH destinations shown as "LFO1/2 LVL"; the mod wheel keeps driving MORPH.
- A legend while OCT- is held on the TIDE page (C9, not built).

**Content**
- Roles spec T4: generated notes stay in the Original's register lane (`fix/roles-T4-lane` `7f6bbb9`, +288 B, not merged).
- Roles spec T7: CELLAR TAPE's bass comes back with rewritten records.
- Roles spec T8 to T10: the new reel of multis, together with the `tideroles` levels (43 of 338 checks red: M04 +3.8, M10 +5.3, M12 -3.1, M15 -7.2, M16 -9.2 dB from the median; finding 25, D46).
- Factory bank: offer the re-authored End Bs and tempo-synced slow motion upstream.

**Live recording** ([audit](audits/2026-11-08/liverec/LIVEREC_AUDIT.md))
- Keys record into an ARP SEQ pattern: built, see b3 row 7c (R2). Documented limits: a 2-step pattern in REPLACE splits a chord across the step boundary, and the key's step is predicted from the POLY stretch before the next arp step runs (it can land one step off at a stretch change). [Brief R2](audits/2026-11-08/liverec/TASKS.md#r2)
- Plain DUELL's `rec_note` stays under `#else`: deliberate, a shared `rec_put` would change the `DUELL_TIDE=0` binary.
- Named `tide.h` hooks for the raw `#if DUELL_TIDE` blocks in `seq.c`; a `seq_len()` helper; one "past half step" helper.
- Dead `SC_MODE` code under TIDE (flash to buy back, with the MODE page's `CG3` cards).
- A small per-row kind table for TIDE SETTINGS (replaces the row comparisons in `tide_ui.c`).

**Storage and backups**
- U4, 24 multi slots: tested on `wip-24-multis` (`21ca3a7`), its own rollout with a format bump. At the rebase: renumber its backup ids (41 is now the FM6 voice bank), archive VERSION 6 against 5, update `erase_test.c` and `xb_test.c`, measure the flash first (keep 3 KB headroom; only 2.4 KB is free today, so it needs savings first).
- Patch capacity in the docs: INFO says 128 slots, TIDE guarantees 126 worst-case records; replace "every slot always fits" with exact wording.
- Pattern save: document the exact visible entry point (saving needs STOP) before any live test.

**Repository and CI**
- The `emu` CI job: fixed in [#4](https://github.com/pssdrnk-cmyk/DUELL-TIDE/pull/4) (emsdk sourced under bash) and gating once merged.
- Rename `jam11` to `tide`.
- Gate the release channels by CI too (a Stable / Beta release is offered only from a green commit), from the 4 November decisions.
- Merge upstream's emulator commits.
- Test quality notes left from finding 38: split the FM6 budget check by crossing state, one generated multi-name table, the `F_NONE` hang exemption if it flakes.
- Workspace clean-up: retire merged worktrees (owner's OK).

**Upstream (plain DUELL)**
- Small single-finding pull requests, in order of value: FM6 re-strike, arp grid and swing direction, delay clicks, glide, steady tempo, then the smaller fixes, the PHYS tuning fix and the doc corrections. The owner opens every pull request.
- Live REC on an ARP SEQ part (live-record audit R1) has the same bug in plain DUELL; the b3 fix is TIDE-only. Offer it upstream as its own change.
- First the owner raises three items with the author: the voice and render restructuring, fixed knob columns, the bus B pointers.
- Felucca's "return to official V15" installer path (with a backup first).
- When merging Felucca code: its engines are now MIT (Felucca-Engines) and its hardware layer MPL-2.0 (felucca-bsp); check our headers. Their second core is a different design: read it for K4 / K10, don't swap it in.
- OSCILL/R's wordmark stays on the editor header until the author agrees to a replacement; no Firmware Hub entry yet.

**Memory savings, when a feature needs room** (estimates until measured; none is free)

| Where | Size | Note |
|---|---|---|
| FM6 lookup table | 4,100 B flash | A 257-entry table frees ~3 KB but changes pitch / gain math: an approximation to validate by ear. |
| Factory multi records | ~9.6 KB flash | Packing not measured yet (sound pairs saved 1,888 B the same way). |
| Delay line | 131 KB pool | A smaller line changes the sound. |
| Screen canvas | 59.5 KB pool | Drawing in smaller strips; every direct pixel access must be checked. |
| Performance capture | 32 KB pool | Check when it must stay live. |

## Waiting on the owner

| Question | Default if you don't mind |
|---|---|
| D43: when a crossing starts under a held chord, the chord stays whole (the budget plans for it from the start). | Stays whole; decided after the drones are measured under a GLIDE. |
| D46: the multis that are too loud or too quiet are trimmed by ear on the unit. | Trim M15, M16 up and M10 down by the numbers, then listen. |
| Try on the unit: keys recording into an ARP SEQ part, and REC ADD / REPLACE (b3). | Nothing to decide; say if the nearest-step rule feels wrong. |
| Merge PRs #1 to #4 once GitHub is green. | Merge in order #1, #2, #3, #4, then the three waiting branches. |
| Reverb FREEZE (below): build it? | Not built until you say. |
| Drone scenes: delivered as a file pack, replacing multis, or by packing the multi records to make room? | File pack (no factory multi changes). |
| Which of the ideas below, if any. | None until chosen. |

**By ear or eye on the unit** (no homework: noted when the owner meets them in use): the drones; the U2 multi sequences; the MULTIS / PATTERNS preview's readability; polymeter (12 / 16, 15 / 16) against a steady 4 / 4; whether TURNS' alternating bars improve the interplay.

## Ideas, not decided

- **MORPH card waves.** The MORPH card's moving lines become the part's real output, like an oscilloscope: a held note stands still, a chord beats, L and R as two lines, faint trails when the sound moves. Plan: [design/WAVE_DISPLAY_PLAN.md](design/WAVE_DISPLAY_PLAN.md) (branch `plan/wave-displays`); an emulator-only WAVE LAB page is on that branch for trying it. The audio tap is its own beta, measured on the unit. Owner choices in the plan (silence, trails, the bass line).
- **From Niagara Drift** (an FM-1 drone firmware; checked against our code 8 November, estimates until built): **reverb FREEZE** on HALL / ROOM (the tail holds and the notes can stop; a block-rate gain change with a limiter, ~200-400 B, no CPU while off; risks: fixed-point overflow and the release click) fits best. **AGE** (slow tape-like wear, ~300-600 B) needs an offset layer on top of the saved sound, not checked yet. Gesture keys and a free-time motion loop do not fit the flash, the keys or the no-new-pages rule.
- **Drone scenes:** multis built around the eight drones, one per theme.
- **Keep current patterns on MULTI load** (U2 route A): an explicit load that keeps both playing patterns.
- **Musical evaluations kept from the earlier plan** (no work scheduled): M2a chord movement; M5 / M10a level and slow-glide behaviour only for a found defect; M7d polymeter first by hand; CHANGE / ENERGY rows only with useful behaviour. Details in [design/TIDE_MUSICAL_DESIGN.md](design/TIDE_MUSICAL_DESIGN.md).

## Decided against, or kept as is

- WAVE table quality stays (flash). SHIM's PITCH DOWN and MOD stay; cut only if a TIDE feature cannot fit, and then the owner is asked.
- No real chord steps in ARP SEQ; no cross-engine JRNY; no automatic harmonic rewriting, loudness normalisation or harshness classification.
- Exact live-morph continuation after a restart is not required.
- Going back to plain DUELL 1.1 is documented, not supported.
- No DRONE row: HOLD already latches the notes (owner, 8 November).
- LADDER's bass loss, filter 1 without self-oscillation and LOFI's aliasing stay as character.

## Versions and releases

TIDE has its own version in [`TIDE_VERSION`](../TIDE_VERSION); DUELL's `VERSION` (1.1) is not touched. ABOUT shows both.

| Version | Means |
|---|---|
| `1.0.0-b1`, `1.0.0-b2`, ... | A beta: a build flashed to a unit for testing. |
| `1.0.0` | The first build the owner has validated: the stable one. |
| `1.0.1` / `1.1.0` | A fix / a feature on top of a stable; its betas are `1.0.1-b1` / `1.1.0-b1`. |

Tag, GitHub release and CHANGELOG section share the name; the tag has `tide-` in front (`tide-1.0.0-b2`). The older dated tags stay as they are.

**To cut a beta:** set `TIDE_VERSION` and start its CHANGELOG section, commit; tag before building (`git tag -a tide-1.0.0-b3`), because `tools/build.py` shows the plain version only for a clean tree at the tag and adds `+` otherwise; build with `tools/build.py --release` in `felucca-base`; flash and test; push the tag; make a GitHub **pre-release** with `duell-tide-beta-<version>.fwsc` and its `.sha256`, "Build `abc1234`" and the known issues in the notes; push `jam11` (or run Host tests by hand) so Pages offers the new beta.

**To cut a stable:** the same with `1.0.0`, a normal release marked Latest with `duell-tide-stable-1.0.0.fwsc`, and the `stable` branch moved to it. The site (`felucca-base/web/releases.py`) offers a beta only while it is newer than the stable one.
