# TIDE musical refinement roadmap

Updated: 2 November 2026. Status: **R0, M1 and M2a integrated and pushed; combined firmware installed and user data verified. M1 host A/B accepted; device listening, clock and CPU acceptance remain pending.**

## Purpose and accepted baseline

Make two-part TIDE sessions develop with more space, continuity and direction while preserving the owner's patterns and sounds. The intended setting is house and dub techno, with the FM-1 supporting a TR-6S and live playing.

The owner confirms the current TIDE implementation works on hardware. The DUELL 1.1 port is complete; this is a subsequent refinement programme, not unfinished migration work. The owner subsequently authorized integration, GitHub push and flashing; the combined source `1196a82` was installed on 1 November 2026. See the integration and device-installation receipts below. New hardware comparisons concern changes made by this programme, not re-approval of the existing implementation.

Source inspected: `jam11`, through `08a5ac7`, plus current working-tree changes. While this roadmap was being written, the branch advanced to `fb9d62d`, changing the showcase to M04 DUB CHORD, 16-bar travel and WAIT 2 bars; the scene list below reflects that update. The branch is shared and may advance again: capture a fresh source revision and diff before each implementation slice. Preserve concurrent edits. The audit's latest inspected target log reported 577,692 / 581,564 B (3,872 B free); this is a historical measurement, not a budget for subsequently changed source.

Already present: bounded sound walks; 16/32/64/128-bar phrase tiers; rhythm movement, repeats and rests; scale-degree pitch changes; bass-preserving upper-voice placement; peer contrast; KEEP and combined undo. Improve these mechanisms rather than adding another generative engine.

## Delivery order and status

| Stage | Outcome | Dependency | State |
|---|---|---|---|
| R0 | Reliable WAIT edits and takeover of either part | None | Implemented, host-tested and included in the verified combined emulator/package build; installed, with device behavior acceptance pending |
| S0 | Room in flash and RAM for M2-M4 | None; before or alongside M2-M4 | Planned; findings below (1 November 2026) |
| M0 | Reproducible musical reference and pitch diagnostic | R0 for moving sessions | Done (owner, 1 November 2026); its results are not yet recorded in this roadmap |
| M1 | ARP SEQ-aware space between parts | M0 | Implemented, software-verified, owner host A/B accepted and installed; device listening comparison pending |
| M2 | Smoother chord voicing over the loop join | M0 | M2a implemented, host-tested, integrated and installed; musical listening comparison pending |
| M3 | Less repetitive JRNY destinations | M0 | Planned |
| M4 | Longer passages with restraint, development and recovery | M1-M3 accepted | Planned |
| M5 | Transition loudness tuning where recordings demonstrate a problem | M0; final check after M4 | Evidence-dependent |
| M6 | Combined listening and device acceptance | Accepted individual slices | Planned |

Implement and review one slice at a time. Keep or revert each musical change on its own merits. Do not accumulate every feature and only then listen. No calendar estimate is assigned before M0 establishes the amount of work and available memory.

## Rules that apply throughout

- Keep the current pages, labels, controls and defaults unless a later owner request changes them. No new menu, save format, global mode or external dependency is planned.
- Apply new musical policy to automatic MUSIC WNDR/JRNY where relevant. Preserve ONCE/LOOP, manual SMART/WILD rolls, PITCH OFF, transport and existing scope of KEEP/UNDO. Loop voicing refines the existing MUSIC/PITCH-gated voicing path.
- Preserve note count and bass identity for voicing; preserve scale, original beat anchors, protected ties/slides/ratchets and existing density bounds for phrase changes. A scale permits notes; it does not establish the intended chord progression. Do not infer new harmony from a scale alone.
- Build completed candidates with the existing checks: 228 B hot headroom, 8 KiB pool reserve, 4 KiB ordinary RAM reserve, call reach and range-X closure. Record flash/RAM/hot deltas against the immediately preceding accepted source. No reserve reductions to fit a feature.
- New planning and scoring stay in the main loop with bounded work, fixed-size state and no allocation. ISR changes, if required to expose timing, must be small and measured. Do not put searching or voicing optimization in the ISR.
- Saved legacy records continue to load. Session history and arrangement state remain ephemeral. If exact restart continuity later becomes a requirement, design it separately.
- Use focused tests for new behavior, then the existing affected groups. Compare known failures with the same baseline. A host render is useful evidence; physical sound and CPU claims require device evidence.

## R0 — close the functional defects first

### R0a: WAIT changed to OFF during a hold

Reproduction: launch a repeating glide with WAIT enabled, reach an endpoint hold, then turn WAIT to OFF. `glide_progress()` divides by the now-zero hold duration. The audit reproduced this with UBSan.

Design: turning WAIT off exits the hold immediately at its current endpoint and begins the already planned return trip from progress zero. It must neither regenerate the destination a second time nor jump a fader. Handle the OCT-minus reset of WAIT through the same rule. While paused, remove the hold but keep the return trip paused until PLAY.

Files: `morph_glide.c`, WAIT controls in `tide_ui.c`, `tide_controls_test.c`.

Acceptance: regular knob edit and reset both work during running and paused holds; no zero division; no duplicate arrival/roll; first resumed fader movement begins from the held endpoint. Existing WAIT timings still pass.

### R0b: takeover of the other part during a BOTH glide

Reproduction: launch on BOTH with Part 1 as the reference, select Part 2 and move its fader. The glide compares only Part 1, then overwrites Part 2. The audit observed Part 2 set to 100, then forced back to 49.

Design: track expected fader values for every controlled part and lane. Detect a manual edit or load on either part. Retain the existing lane-wide takeover policy: the touched SOUND or SEQ lane stops across the original glide mask; the untouched lane continues. This avoids introducing independent glide schedulers. A panel K1 stop still stops the whole glide. Do not infer a manual touch from the glide's own queued updates.

Acceptance: symmetric Part 1/Part 2 cases; SOUND, SEQ and linked BOTH targets; movement and endpoint WAIT; panel/MIDI intervention and patch load; switching the selected part alone never cancels a glide. Where a load leaves the same fader value, use an explicit action/invalidation signal if value comparison cannot detect it.

## M0 — reference scenes and pitch diagnosis

Build a small reusable fixture set around actual playback behavior:

| Scene | Question |
|---|---|
| Current M04 DUB CHORD showcase; retain M02 NIGHT ORGAN as an additional ARP SEQ fixture | Does PLAY develop a useful unattended two-part passage? |
| Sparse dub stabs against a sustained pad | Does one part leave room without flattening the groove? |
| Dense paired stabs with several shared attacks | Does collision-aware development help? |
| Ordinary three/four-note chord progression | Are inner transitions and the last-to-first join smoother? |
| Unequal pattern lengths/divisions and nonzero swing | Is timing prediction correct beyond a 16-step straight loop? |
| FM6 structural transition and PHYS sustained notes | Is apparent pitch movement intended or a tuning defect? |

Use 120 and 128 BPM for the principal musical comparisons. Use 240 BPM only as a stress case. Include representative low, middle and upper notes/chords. Fix source data and seeds, or capture/replay generated decisions, so A/B differences come from the candidate policy. Match playback volume for preference comparisons and retain unmatched recordings for loudness diagnosis.

Each comparison records source revision, scene, tempo, root/scale, mode, target, TIME/WAIT, ROLL/PITCH/WASH, duration and seed/decision trace. Capture part stems and the mix where existing tooling supports it. Keep short loops for voicing and 128-bar sessions for development. Collect hardware recordings when practical; do not build a large new analysis framework.

### Pitch diagnostic, distinct from changing the composition

There is no newly established tuning fault on jam11. Measure before changing constants:

- For a known harmonic reference voice, compare steady endpoint pitch with the intended note. Use a proposed +/-2-cent tolerance for clean sustained references, subject to the estimator's resolution; exclude intentionally detuned or inharmonic voices from that assertion.
- Check both directions through FM6 crossings, held notes versus retriggered notes, fixed-frequency versus ratio operators, and KEEP at A/midpoint/B. Compare operator frequencies with the intended patch rather than assuming every operator follows concert pitch.
- Revisit the old JAM branch's FM BASS 2 / SLAP DOCTOR stress case on jam11 only if equivalent source sounds can be identified: one-bar travel at 240 BPM with sparse control polling, including the historical 55-audio-block cadence. This is a historical regression hypothesis, not proof of a current defect. Existing synthetic crossing tests do not establish that exact case.
- Measure PHYS PLUCK/HARP at low/middle/high registers after settling. Separate attack pitch drift from sustained tuning and deliberate detune.

Deliverable: reference recordings/traces plus a short list of demonstrated problems. Open a focused tuning fix only if a current discrepancy is reproduced; do not apply generic retuning to make heterogeneous presets look uniform.

## M1 — ARP SEQ-aware dialogue

Current gap: `phrase_peer_attack()` returns no competing attack for a SEQ-arp peer. Its timing model otherwise uses SDIV/SSWING and sequencer position. Simply removing that guard would predict the wrong clock: ARP SEQ uses ARATE/ASWING and arp phase/index.

### M1a: predict what can actually attack

Introduce a bounded main-loop timing adapter for ordinary sequences and SEQ arps. Snapshot the minimal phase/index/transport state consistently. Reuse current period/swing calculations; predict the next destination step against the peer's actual cycle. Do not advance the live RNG to predict probability. Classify chance-zero/rest/tie as no attack and chance-eligible notes as possible attacks, including the arp's PROB. Define a coincidence window of one audio block for the first implementation and verify its musical usefulness.

Recognize held chords and the TIDE no-key stand-in. Handle stopped transport, REC disabling the stand-in, muted or level-zero peer, disabled ARP/SEQ modules, and phase resets. At extreme/unsupported periods, fall back to today's independent evolution. If reading or exposing the chord availability would add disproportionate ISR state, begin with the confirmed no-key playing path and document the remaining limitation.

### M1b: use prediction as a soft preference

Feed the timing result into existing direction/gap/repeat choices. Prefer an eligible neighboring space over a collision, but retain shared downbeats and allow intentional unison rhythm. Keep alternating which part gets a development opportunity. Never thin a pattern below existing density bounds or change its actual clock to manufacture space.

Tests: both ordinary/SEQ-arp directions; ARATE different from SDIV; unequal lengths; swing; phase wrap and STOP/PLAY; no-key/held-key/recording; probability 0/full; muted/disabled peer. Compare prediction with actual fired onset traces, not just hand-calculated indexes.

Acceptance: eligible offbeat collisions decrease in the deliberately clashing fixture; no change is required where the groove already has space. No missing anchors, systematic starvation or increased total attack density. Listening should retain drive and produce audible answers. Reject if numerical collision reduction makes the passage weaker.

## M2 — connect chord voicings across the loop join

Current behavior: `sqm_voicelead()` walks forward from an empty previous-chord state. It protects the bass and chooses nearby upper-note octaves. It does not score closure back to the first chord, and its current invocation is gated by a nonzero VOIC dial, MUSIC and PITCH.

### M2a: evaluate cyclic cost

Keep the existing constraints and trigger conditions. Score all eligible chord-to-chord links, including last-to-first across intervening rests. Begin with a bounded second pass seeded by the last eligible chord; retain it only if the whole-cycle score improves. Never iterate to an unbounded fixed point. If that fails the fixtures, consider a bounded candidate search with an explicit work/scratch budget before adopting a more complex solver.

Use deterministic tie breaking. Reject an improvement that breaks bass identity, pitch classes, note count, register limits, octave displacement bounds, duplicate/crossed voices or protected ties/slides. Do not change a held chord across the loop boundary merely to improve the score. Leave single-note lines unchanged in this stage.

Tests: an obviously awkward last-to-first join, already smooth loop, rests at either end, one chord, triads/four-note chords, changing chord sizes, ties across the join, and repeated endpoint regeneration. Measure total movement and closure movement separately so a better join cannot conceal much worse internal motion.

Acceptance: the difficult join improves without worsening whole-cycle cost; already good passages remain stable. Audition several repeats rather than a one-way pass.

Important playback boundary: ARP SEQ takes `step.note[0] - 60` as a transposition of the held chord; it does not play the stored step's upper chord voices. Validate this improvement on ordinary chord sequencing. Do not claim it improves ARP SEQ voicings unless recordings show a relevant effect. A different arp voicing model is outside this roadmap.

## M3 — JRNY destination memory

Current behavior: `tide_journey()` excludes the reached/left destinations where possible and the other part's originals/targets. It searches within the current engine/category, then widens its category fallback. It does not switch engines. Preserve that scope.

### M3a: avoid premature returns

Add a four-entry recent destination ring per part, updated only when a new target is successfully adopted. Store destination identity with engine context; keep the ring ephemeral. Clear a part's history on explicit patch/multi replacement and HOME; pause/resume does not clear it. KEEP rebases and clears that part's history. Capture candidate availability before adding more scoring policy.

Candidate policy: apply existing hard exclusions, then prefer candidates outside recent history within the existing category/fallback stages. If the palette is too small, relax history from oldest first before excluding all valid candidates. Keep the peer's currently used targets protected. With no valid target, use the existing roll fallback. Maintain stochastic choice and current complement bias; do not select only the most different sound every time.

Tests: one/two/many candidates; all candidates in history; peer exclusions; successful versus failed adoption; patch/engine changes; repeatable seeded selection and independence between parts.

Acceptance: with at least five eligible candidates in a stable palette, avoid a return within four adopted targets. Small palettes remain alive and bounded, without stalls or unwanted category jumps merely to satisfy history. A long listening pass should feel less repetitive without losing the source's character.

## M4 — shape longer passages

Existing phrase tiers increase development opportunities at 16/32/64/128 bars. Existing rhythm mutation already moves, repeats or drops attacks, changes ordering and occasionally moves scale notes. Add restraint and recovery through this machinery rather than layering another random sequencer over it.

### M4a: a bounded phrase policy

Prototype a 64-bar policy: 16 bars settle, 16 develop, 16 open, 16 recover. These are internal phases, not new UI modes. Compare with the current behavior before choosing final lengths/weights. Derive phase from accumulated musical time shared by both parts, not main-loop wall time; only selected parts receive edits. STOP freezes it, resume preserves it, and a genuinely new WNDR/JRNY session resets it. WAIT bars count as musical time without triggering extra edits during a hold.

- Settle: mostly retain the motif; allow small timbral motion.
- Develop: favor one eligible rhythm/voicing change at a time.
- Open: modestly favor existing brightness/motion and permitted density changes; the peer receives a complementary opportunity, not the same push.
- Recover: lower mutation frequency, remove optional added repeats where possible, and ease timbral offsets inward while retaining the evolved motif.

Recovery is not HOME: do not restore the original patch, overwrite the audible endpoint, or erase the accumulated melody. Generate new silent-end destinations with gentler deltas. PITCH OFF still prevents scale-degree edits. Keep the present rare pitch-mutation rate initially; change one musical variable at a time.

M4b, only if M4a is accepted: introduce small, bounded variation in phase lengths/choices from the existing seed stream, isolated so it does not perturb unrelated random decisions. Avoid a recognizably identical 64-bar script.

Tests: phase boundaries at short/long TIME/WAIT values; pause/resume; mode/target changes; parts with different pattern lengths; AMOUNT zero; protected notes; long-session state bounds. Record density, pitch spread and timbral dial ranges across at least 256 bars to detect accumulation or saturation.

Acceptance: audible contrast between active and restrained stretches, no endpoint jumps, motif still recognizable, no progressive thinning or permanent maximum brightness. Prefer the simpler current policy if the new arc sounds imposed.

## M5 — loudness and transition calibration

Measure M0 pairs and any outliers discovered in M1-M4. Use identical note/velocity/gate input for source/target comparisons. Inspect short-window RMS and peaks along the transition, plus integrated loudness for sufficiently long material where the existing tools support it. Listen to bass weight, sustain, transient impact and effects tails; brightness changes can resemble level changes.

Treat roughly 3 dB of unexplained sustained level change in a matched role as an investigation trigger, not an automatic normalization target. Expected envelope evolution, deliberate silence or the designed structural valley is not by itself a fault. Check WASH OFF first (current default), then LOW/MID/HIGH separately.

Fix a demonstrated cause at its narrowest point: incorrect unison scaling, crossing gain, or a specific preset's trim/envelope. Do not add an automatic compressor, blanket per-frame gain normalization or another wash layer. Any trim changes need independent audition and a check of their effect outside TIDE; prefer TIDE-specific correction when the source sound is already correct at rest.

Acceptance: the identified unwanted step/dip is reduced without pumping, clipped peaks, disappearing bass or altered at-rest sounds outside the authorized correction. If no clear outlier exists, complete M5 with measurements and no code change.

## M6 — accept the combined result

Run the focused regression groups for changed code, including `tidecontrols`, `tidecrossings`, `tidekeep` and relevant sequence/clock tests. Use strict sanitizers for DSP/fixed-point changes. Check TIDE-off against the accepted off baseline, explaining intentional shared changes. Build the production target and review every resource check and delta; do not infer device CPU from host elapsed time.

Walk through affected controls in the emulator. Then, when a new package is explicitly requested for device testing, compare the accepted baseline and candidate on the owner's FM-1, with a backup and the normal installer procedure. Test a representative two-part passage and a demanding multi, with TR-6S clock and live intervention. Report CPU/UI responsiveness and any clicks from the unit separately from host metrics.

Final musical session: at least 10 minutes of MUSIC WNDR and JRNY with PITCH both off/on, TARGET SOUND/SEQ/BOTH, normal TIME/WAIT, and intermittent hand takeover. Use shorter diagnostic cases for extreme settings. Confirm KEEP/save/reload and undo still preserve their established meanings. Session-only history/arc state need not survive a power cycle.

Each accepted stage records: source checkpoint, exact change, resource delta, tests actually run, A/B listening result, and any remaining limitation. Update this roadmap's state rather than creating duplicate status documents. Local commits may be made when requested; publishing and flashing remain separately authorized actions.

## S0 — make room in flash and RAM

Measured on the M1a candidate (`work/roadmap-m1-build`, image 578,216 B, before M2): flash **3,348 B** free of 581,564; ordinary RAM **1,712 B** above the 4 KiB reserve; pool + range Z **2,964 B** above the 8 KiB reserve; hot audio code 1,240 B above the 228 B reserve; range X 928 B. That is not enough for M2-M4. Earlier compaction is already in the build: every font face Huffman-packed, the splash transparent-run encoding, the size policy for TIDE and five UI files, MORPH_HIST cut from 9 to 3, the font decoder cache moved to ordinary RAM.

The search covered all of DUELL, not only TIDE. Owner decisions (1 November 2026): the pattern-slot RAM mirror is not needed; code compaction is covered by the existing test groups. Each slice below is its own measured candidate with flash/RAM/pool/hot deltas. Changes to shared DUELL code intentionally move the TIDE-off image: record its new hash as the off baseline.

- **S0a — pattern slots from flash.** `patslot.c` keeps all three PTS3 objects in the pool (`ps_bank`, 10,752 B) so the list never reads flash. Read slots from flash when needed instead; keep one object-sized buffer for a SAVE/ERASE whose flash write still waits until nothing plays (`ps_poll`). Expected saving about 7 KB of pool. Format, slot layout and flash map unchanged. Tests: save/load/erase of slots, first-start defaults, SAVE while playing, power cut during a write (A/B sector copy), list browsing.
- **S0b — complete the size policy.** `multi.c` is `#include`d by `project.c`; `tools/size_fns.py` reads only the listed files' text, so multi's functions (about 3.2 KB of code) are never built for size. Add it. Extend the `morph.c` selection beyond its current eight main-loop functions; `isr_reach` keeps everything the interrupts reach at the existing policy. Expected 0.7-1.9 KB of flash.
- **S0c — smaller FM6 pitch table.** `FM6_EXP2` is 1,025 entries (4,100 B) with linear interpolation; 257 entries keep the error far below one cent. About 3 KB of flash. Not bit-identical: adapt the affected FM6 tests to a tolerance and repeat M0's pitch check.
- **S0d — share the update buffers.** `ota_stage` and `ota_dec` (about 2.7 KB of pool) can share memory with the save/load staging (`proj_wire_u`); a firmware update never runs alongside a save. Assert the exclusion in code.
- **S0e — factory data packing.** Factory sounds and multis (`DS_PAIR` 9,642 B, `DM_REC` 9,625 B) are read only at load. Measure what a tighter encoding saves before deciding.

Not pursued: strings (19.5 KB; duplicates already merged, the longest are licence credits that must stay); the second effects bus and engine memory (real per-part needs); drawing the keycap pills instead of storing them (`KC_DATA`, 4.9 KB; changes their look); broad no-inline of UI helpers (about 1.3 KB measured earlier, at a drawing-speed cost).

Suggested order: S0b and S0a first (largest, simplest), then S0c, S0d, S0e as M2-M4 need the room.

## First implementation slice

### Local progress — 1 November 2026

Starting checkpoint: `e6e1500` on `jam11`. The initial R0 fixes and basic tests were already present in `9c9e253`; they are not new work from this slice. Existing migration-plan edits and a concurrent `ui_draw.c` change are preserved.

R0a follow-up: regression tests reproduced that WAIT OFF during a paused endpoint hold did not remove the hold until playback resumed. The main-loop STOP path now releases that hold without regenerating the destination or moving the endpoint. Tests cover both the WAIT knob and OCT-minus reset, a sustained pause, and a return trip resuming near progress zero. Before the fix: 66 checks, two failures; after: `tidecontrols`, `tidekeep` and `tidecrossings` pass with the existing strict sanitizer builds.

Production build verification uses an isolated snapshot of `e6e1500` plus this slice, excluding concurrent edits: the shared output directory rejected generated-header writes. Candidate image 577,676 B; ordinary RAM 67,904 B (5,824 B free); pool including range Z 357,484 B (11,156 B free); hot audio code 11,044 B with 1,468 B headroom. All build placement, reserve, call-reach and range-X closure checks pass. Build evidence is in `work/roadmap-r0-build/build.log` beside the worktrees. Hardware and emulator interaction acceptance of this follow-up remain pending.

Measured against a fresh unchanged `e6e1500` build (`baseline-build.log`): image +16 B (577,660 to 577,676 B); ordinary RAM, pool, hot audio code and hot headroom unchanged. The isolated output directory's normal package is the baseline after this comparison; the candidate is explicitly retained as `work/roadmap-r0-build/candidate.fwsc` and `candidate-app.bin`.

Remaining R0 work: broaden running WAIT edit/reset coverage and verify no duplicate destination generation; verify symmetric takeover across lanes, holds, MIDI and same-value patch loads. Do not mark R0 complete from the existing basic Part 2 SOUND takeover test. M0 reference capture follows R0; no new musical policy is implemented yet.

### R0 completion — 1 November 2026, later

Source: `c46ccf2` plus the uncommitted R0a follow-up above.

- Running WAIT edits (`test_wait_edit_running`): on a DRIFT glide held at B, WAIT up keeps the hold. WAIT OFF, by knob and by OCT-minus reset, ends it once: the return starts from the endpoint near progress zero, End A is not rolled again, and the return arrives at A and turns.
- Symmetric takeover (`test_takeover_symmetric`): a BOTH glide launched from either part, the other part touched on the SOUND or SEQ lane, moving and held. The touched lane stops, the other lane goes on, and the hand's value and the launcher's position stay. All 8 cases passed without a code change.
- Routes (`test_takeover_routes`): selecting the other part cancels nothing. A MIDI MORPH on the other part under TARGET BOTH also moves its SEQ fader through LINK, so both lanes stop (established LINK behaviour); under TARGET SOUND it stops the SOUND lane. A MIDI SEQ fader stops only the SEQ lane. A patch load on the other part stops the SOUND lane. A JOURNEY's own arrivals never stop it.
- Defect found and fixed: a patch load on either part during a hold at A left the fader at 0, so value comparison missed it and the glide then moved the new sound. `morph_rebase_part` now counts loads per part (`mm_loads`, through the TIDE-only `TIDE_REBASE_END` hook), and the glide's SOUND lane compares that count as well as the fader value. KEEP and HOME rebase too, so they count as touches.
- Tests: `tidecontrols` 121 checks, 0 failures (the same-value load cases failed before the fix); `tidecrossings` and `tidekeep` pass. `morph` (needs fork()) and `morphui` (4 FAILs) fail identically on the baseline and the candidate.
- Build (isolated snapshots; baseline = `c46ccf2` + R0a follow-up): image 577,736 to 577,832 B (+96); ordinary RAM 67,904 to 67,920 B (+16); pool + range Z 357,484 B, hot audio code 11,044 B and 1,468 B hot headroom unchanged. Every placement, reserve, call-reach and range-X check passes. `DUELL_TIDE=0` is byte-identical to `jam/off.sha256` (`6232c047…`).

Remaining for R0 acceptance: an emulator walkthrough of WAIT edits during a hold, then the device comparison when a package is requested. M1 is assigned to Codex; this branch's next local candidates are the M0 fixtures, then M2 or M3.

R0a and R0b, with focused regressions, then M0 reference scenes. The first musical change is M1's correct ARP SEQ timing prediction and gentle use of it for space. M2 and M3 remain independent candidates until heard. M4 waits until the individual improvements are accepted; M5 changes code only when recordings justify it.

### M1 implementation and software verification — 1 November 2026

Owner explicitly selected M1. **Implementation and software checks are complete; owner host A/B listening accepted; testing on the unit remains pending.** The initial internal-clock-only slice was extended before delivery; external clocks and held keys are now supported.

Scope: bounded main-loop onset prediction for ordinary sequences and SEQ arps, using SDIV/SSWING or ARATE/ASWING and the actual phase/index. External divisions use the same fractional grids as playback, converted to common Q8 musical time. The internal arp's overshoot discard and odd-length swing parity are retained. Ordinary nudges and ratchets are included. A short existing publication guard copies phase, fractional carry and held-key metadata; all searching runs after interrupts resume. Held-chord eligibility uses the existing key mapping without updating chord memory. REC disables only the no-key stand-in; a held chord can still compete. Stopped, muted, zero-level and disabled-module peers do not compete; ARP TARGET modulation without note-ons is excluded. Other arp modes and anomalous/extreme periods retain independent evolution.

The existing direction, gap and repeat choices consume the result. There is no new UI, mode, clock, persistent record, dependency, or ISR search. Existing protection of original anchors, ties/slides/ratchets, density limits and alternation remains in place. ONCE/LOOP, manual rolls, PITCH OFF, KEEP/UNDO and transport code were not changed.

Probability follows actual playback: ordinary CHANCE 0 suppresses attacks; SEQ arp playback ignores step CHANCE and compares APROB inclusively, so APROB 0 still permits 1/128. This slice does not change those existing semantics and never draws the live RNG.

Verification:
- `tidedialogue`: **429 checks, 0 failures**, strict ASan/UBSan. Eighteen internal/external-grid traces cover 8,493 step visits, with zero current/next-step mismatches within one block. Six additional nudge/ratchet/held-key traces cover 2,587 fired destinations, with zero mismatches. Unequal 9/7-step patterns, distinct ARATE/SDIV, odd wraps, swing 50/67/75, 120/128 BPM and 240 BPM stress are included.
- USB/TRS pulse-input traces cover another 1,291 visits through the real clock handlers. USB fixtures have zero nominal one-block mismatches. The swung mixed TRS fixture has **19 nominal one-block mismatches** because interpolation uses whole-millisecond timestamps and tempo estimates change with pulse quantization. Across these wire-input traces, no predicted attack lies outside the measured three-block uncertainty bound, and no prediction misses an exactly coincident dispatch. This is explicitly weaker than exact future block prediction; no sub-millisecond wire precision is claimed.
- Deliberately clashing, symmetric fixture: 64 eligible moves across both parts, 64 collisions under the existing independent direction versus 0 under dialogue, with unchanged attack count and every shared beat anchor retained. This numerical result does not establish a musical preference.
- `tidecontrols`, `tidekeep`, `tidecrossings`, `clockguard`, `clockdrift` pass. The cleaned standalone timing test passes again after removing the temporary listening command from the tracked test.
- Production image **577,832 to 579,528 B (+1,696 B)**. Ordinary RAM **67,920 B**, pool plus range Z **357,484 B**, hot audio code **11,044 B**, hot headroom **1,468 B**, all unchanged. All image/reserve/placement/call-reach/range-X checks pass. `DUELL_TIDE=0` package is byte-identical to `jam/off.sha256` (`6232c047c076ea52f836b1fffc25fb7c4706db8f8d856791f2d9a10e8b0ee9f3`).
- Pinned CPU-emulator package smoke passes boot, LCD, MIDI note-on/off and finite bounded audio. This does not prove device CPU, sound or MIDI.

Reference comparison uses an isolated `c46ccf2` snapshot plus the accepted R0 source changes; the current branch's concurrent M2, credit/cover/web and migration-document edits are excluded and preserved. Evidence is under `work/roadmap-m1-build`: `baseline-build.log`, `candidate-final-build.log`, `final-regressions.log`, `final-onset-traces.log`, `final-dialogue-clean.log`, `off-build.log`, `package-smoke.log`, and retained `candidate.fwsc`/`candidate-app.bin`.

Listening files in `work/roadmap-m1-build/audio`: seeded 16-bar M04 at 120 BPM and M02 at 128 BPM are byte-identical between baseline and candidate. A deliberate-clash fixture using M04 sounds produces different audio while both automatic parts develop. All six raw renders are non-silent and unclipped. Root/pattern data come from the identified factory records; comparisons use MUSIC WNDR, BOTH, TIME 1 bar, WAIT OFF, PITCH OFF, WASH OFF, VARY ALL, seed `0x1234567` and the device USB reference master level. `clashing-A-then-B.wav` contains RMS-matched baseline A (32 s), 1 s silence, then candidate B (32 s); unmatched originals and `metrics.json` are retained. Rendering uses the actual firmware DSP in the existing host harness, not replacement synthesis. The separate reproducible listening driver stays in the evidence folder rather than adding a product/test-runner feature.

Strict native audio rendering found existing signed-shift UB in **both** baseline and candidate: M04 `fx_bus.c:711`, M02 `eng_wheel.c:311`. Those failures are retained in `audio/*-strict.log`; normal-production-setting host renders are supplied for comparison. They are not claimed as strict DSP sanitizer passes. The timing/control regression groups retain their strict sanitizer flags.

Owner listening result (1 November 2026): after hearing the level-matched deliberate-clash A/B, the owner preferred candidate B: "B is better, less busy I guess". Record this as a favorable but tentative subjective result for this host-rendered fixture; retain M1. It does not establish preference across every scene or device performance.

At this isolated M1 checkpoint, no firmware had been installed or published. The later authorized integration, push and installation are recorded below. M1 implementation and owner host A/B listening are complete; physical listening, clock and CPU acceptance remain pending.

### M2a local slice — 1 November 2026

Owner asked to proceed with M2. M2a is **implemented and host-tested**, not musically accepted; it was done ahead of the M0 fixtures, so the ordinary-chord listening scene is still to come.

Source checkpoint: `17d306a`. Only `seq_voicelead.c` changes in firmware; the concurrent M1 edits (`seq_dialogue.c`, `tide_dialogue_test.c`) are untouched and excluded from the build comparison.

- `sqm_voicelead` is now one bounded walk (`voicelead_walk`) run up to three times: the existing walk from no previous chord, measured; a second walk seeded with the first walk's last chord, measured; then the chosen one, written. The second is kept only when its whole-cycle movement (upper voices, chord to chord, plus last back to first) is strictly lower, so ties and already smooth loops keep today's voicing exactly. Per-chord candidates, scoring, constraints and the VOIC/MUSIC/PITCH gate are unchanged. No persistent state or pattern-sized scratch arrays; only fixed four-note temporaries, in the main loop. Worst case three times today's work.
- New `tidevoicelead` group (strict ASan/UBSan): a loop whose first chord is written an octave below the rest (inner movement 28 to 4, join 24 to 0); an already smooth four-note loop unchanged; one chord unchanged; a chord tied across the join keeps its notes; repeated generation identical. 9,000 seeded random loops over chromatic and two other scales, with rests, ties, slides, single notes, triads and four-note chords: 15–17% improve, none change without a whole-cycle gain, and bass, pitch classes, note count, register 24..108, ±12 from the source, no duplicated voice and held/slid chords hold on every loop. **9,017 checks, 0 failures.** `tidecontrols`, `tidekeep`, `tidecrossings` and `tidedialogue` pass.
- Build (isolated snapshots in `work/roadmap-m2-build`; baseline = `17d306a`): image 577,832 to 578,136 B (+304); ordinary RAM 67,920 B, pool + range Z 357,484 B, hot audio code 11,044 B and 1,468 B hot headroom unchanged. Every placement, reserve, call-reach and range-X check passes. The final combined M1/M2 measurement is recorded in the integration section below.

Limits: ARP SEQ plays the held chord, not stored upper voices, so this has no effect there (M02 NIGHT ORGAN). Single-note lines and chord-size changes are not linked, as before. At this isolated M2a checkpoint, no emulator, listening or device check had been performed and nothing had been flashed or published. Combined software verification and installation followed below; M2 listening acceptance remains pending.

### M1 + M2 integration for GitHub — 1 November 2026

Owner requested integration and push to the existing `pssdrnk-cmyk/DUELL-TIDE` fork. Both changes were already in the shared `jam11` checkout, so no separate topic-branch merge is needed. Remote `fork/jam11` was `876ddd5`; the R0 commit `17d306a` and this integration advance it without rewriting history. The older `fork/main` emulator redesign is superseded by the current TIDE page and is not merged over it.

Combined verification used a fresh snapshot of `17d306a` plus the exact pending M1/M2 source files and tests. Seven groups pass: `tidedialogue`, `tidevoicelead`, `tidecontrols`, `tidekeep`, `tidecrossings`, `clockguard`, `clockdrift`. The browser WASM build/note smoke and pinned CPU-emulator packaged-firmware boot/LCD/MIDI/audio smoke both pass. Plain DUELL (`DUELL_TIDE=0`) remains byte-identical to the recorded off baseline.

| Region | Used | Capacity | Free | Required free |
|---|---:|---:|---:|---:|
| Application flash | 579,832 B | 581,564 B | 1,732 B | Fits slot |
| Ordinary RAM | 67,920 B | 73,728 B | 5,808 B | 4,096 B |
| Pool including range Z | 357,484 B | 368,640 B | 11,156 B | 8,192 B |
| Hot audio RAM | 11,044 B | 12,512 B | 1,468 B | 228 B |
| Range X | 31,584 B | 32,512 B | 928 B | Fits region |

Combined image delta against R0: **+2,000 B flash**, no ordinary/pool/hot RAM growth. All reserves, placement, call reach and range-X closure checks pass. Application flash is tight; only 1,732 B remains. These are linked-build measurements, not live heap/CPU measurements or physical-unit validation.

Evidence and retained combined package: `work/merge-m1-m2-build/` (`integration-build.log`, `integration-tests.log`, `emulator-build.log`, `emulator-note.log`, `package-smoke.log`, `off-build.log`, `candidate.fwsc`). M1 retains the owner's favorable host A/B result; M2's listening comparison is still pending. No firmware write or release upload is part of this push. The repository's existing Pages workflow runs on a `jam11` push.

### Device installation — 1 November 2026

The owner authorized flashing after the integration/push. Firmware source checkpoint `1196a82`; exact combined package SHA-256 `3a07a5431e346c5ac67830d2410c715350b6296836d62b486a80c8aa9961794d`. The connected unit identified as DUELL 1.1, model FM-1_901, port DUELL 0, 128 patch slots and the expected 10 engines.

Full backups were saved before writing in `work/backups/m1-m2-1196a82/`. The Python helper's older revision-7 music checker marks revision-8 current music as raw; its bytes and CRC were retained, and the actual candidate firmware's native `proj_import` accepted the 3,840-byte, two-part, 127-parameter revision-8 record under strict ASan/UBSan. This helper mismatch remains a follow-up tooling issue; a default helper restore would skip that raw-marked record.

The normal installer wrote to 100%, rebooted, verified FM-1_901, and a fresh INFO returned DUELL 1.1 with the same slot/engine capabilities. Initial readback matched 15 of 16 archive objects; decoded current-music comparison showed only Part 2's live End-B destination differed after restart. The validated pre-flash current-music object 0 was restored through the existing music-only restore API and accepted by the unit's own validation. Final readback matches **all 16 objects byte-for-byte**, including current music, settings, empty multi objects, pattern slots and user wavetables. Both End-B blocks and amounts also match in the native decoded comparison.

Local evidence: `flash.log`, `post-flash-info.txt`, `music-validation.log`, `music-restore.log`, `final-readback-comparison.json`, `final-music-comparison.log`, pre-flash archives and the final archive under `restored/`, all in the backup folder above. This establishes physical installation, reboot/identity and user-data readback. It does not establish live audio quality, physical MIDI/clock timing or CPU performance; those acceptance checks remain pending.
