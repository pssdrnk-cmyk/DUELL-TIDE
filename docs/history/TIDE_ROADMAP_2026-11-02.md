# TIDE musical refinement roadmap

Updated: 2 November 2026. Status: **S0a/S0b merged into jam11 and installed with the DSP fixes at e176f6e; final readback matches all 16 backup objects after restoring the current session. R0, M1 and M2a remain installed; M1 host A/B accepted. Device listening, clock, CPU and S0 playback-save acceptance remain pending. M7–M10 (syncopation and polymeter, question and answer, buildup/breakdown, smoother transitions) remain owner concepts, with a feasibility study of space and controls. M7d adds polymeter; automatic length policy and resource cost remain pending.**

## Purpose and accepted baseline

Make two-part TIDE sessions develop with more space, continuity and direction while preserving the owner's patterns and sounds. The intended setting is house and dub techno, with the FM-1 supporting a TR-6S and live playing.

The owner confirms the current TIDE implementation works on hardware. The DUELL 1.1 port is complete; this is a subsequent refinement programme, not unfinished migration work. The owner subsequently authorized integration, GitHub push and flashing; the combined source `1196a82` was installed on 1 November 2026. On 2 November, the owner authorized the S0/DSP checkpoint `e176f6e`, which is now installed with verified user-data readback. See the integration and device-installation receipts below. New hardware comparisons concern changes made by this programme, not re-approval of the existing implementation.

Source inspected: `jam11`, through `08a5ac7`, plus current working-tree changes. While this roadmap was being written, the branch advanced to `fb9d62d`, changing the showcase to M04 DUB CHORD, 16-bar travel and WAIT 2 bars; the scene list below reflects that update. The branch is shared and may advance again: capture a fresh source revision and diff before each implementation slice. Preserve concurrent edits. The audit's latest inspected target log reported 577,692 / 581,564 B (3,872 B free); this is a historical measurement, not a budget for subsequently changed source.

Already present: bounded sound walks; 16/32/64/128-bar phrase tiers; rhythm movement, repeats and rests; scale-degree pitch changes; bass-preserving upper-voice placement; peer contrast; KEEP and combined undo. Improve these mechanisms rather than adding another generative engine.

## Delivery order and status

| Stage | Outcome | Dependency | State |
|---|---|---|---|
| R0 | Reliable WAIT edits and takeover of either part | None | Implemented, host-tested and included in the verified combined emulator/package build; installed, with device behavior acceptance pending |
| S0 | Room in flash and RAM for M2-M4 | None; before or alongside M2-M4 | S0a/S0b merged into `jam11` and installed at `e176f6e` (2 November 2026), with exact final archive readback; playback-save and device CPU acceptance pending; S0c-S0e planned |
| T0 | Repair the host test baseline: 63 of 121 groups fail on plain 1.1 | None; before M3 and M4 | Planned (owner, 2 November 2026); `patslot` partly repaired with S0a |
| M0 | Reproducible musical reference and pitch diagnostic | R0 for moving sessions | Done (owner, 1 November 2026); its results are not yet recorded in this roadmap |
| M1 | ARP SEQ-aware space between parts | M0 | Implemented, software-verified, owner host A/B accepted and installed; device listening comparison pending |
| M2 | Smoother chord voicing over the loop join | M0 | M2a implemented, host-tested, integrated and installed; musical listening comparison pending |
| M3 | Less repetitive JRNY destinations | M0 | Planned |
| M4 | Longer passages with restraint, development and recovery | M1-M3 accepted | Planned |
| M5 | Transition loudness tuning where recordings demonstrate a problem | M0; final check after M4 | Evidence-dependent |
| M7 | Syncopation and polymeter: off-beats, pushes and unequal looping lengths | M1 | Proposed by the owner, 2 November 2026; M7d automatic length policy uncosted |
| M8 | Question and answer: alternating turnarounds, turn-taking, echoes | M7 | Proposed by the owner, 2 November 2026 |
| M9 | Buildup, peak, stop, breakdown; harshness drives busyness | M7, M8, S0; overlaps M4 | Proposed by the owner, 2 November 2026; relation to M4 open |
| M10 | Smoother sound transitions: no stairs, staggered groups, switches on the beat | M0; measure first | Proposed by the owner, 2 November 2026 |
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

## Owner musical concepts — 2 November 2026

The owner listens to TIDE as a background next to a TR-6S and wants **syncopation and polymeter**, **question-and-answer** between the two parts, and **buildup and breakdown**. The owner clarified that polymeter is included as well: one part loops over a shorter length so its notes keep changing position against the beat. The stages M7, M8 and M9 below explain each idea musically first, then record what the code does today, what it needs and how to judge it. They follow the rules above. The exceptions the owner asked for are named where they apply. Nothing in them adds a page, control, saved field or mode: they are automatic MUSIC WNDR/JRNY behaviour, with automatic length changes still requiring the M7d study.

Step numbers below count a straight 16-step bar from 1: the beats are steps 1, 5, 9 and 13 (code index `i % 4 == 0`); the off-beat 8ths are 3, 7, 11 and 15; the 16ths either side of an off-beat 8th are 2, 4, 6, 8, 10, 12, 14 and 16. Unequal lengths or divisions need the actual shared-clock phase described in M7d rather than this local-index shorthand.

The ideas build on each other. Syncopation decides *where* in the bar a part leans; polymeter lets its motif rotate against that bar through unequal loop lengths. Question-and-answer decides *which part* speaks in which bar. Buildup and breakdown decide *how much* is said over 32–64 bars. Implement and listen to separate slices in that order.

## M7 — syncopation and polymeter

### What it means musically

Syncopation is weight placed where the listener does not expect it: on the weak 16ths between the beats, or one 16th *before* a beat so the note pushes into the bar line instead of landing on it. In house and dub techno the kick owns the four beats, so a chord or stab that avoids them and sits on the "and" (the off-beat 8th) or the "a" (the 16th just before a beat) makes the groove lean forward and leaves the kick room. Three figures matter most here:

- **The off-beat stab.** Chords on steps 3, 7, 11 and 15, against the kick: the classic dub-chord placement. It sounds relaxed and spacious.
- **The push (anticipation).** A hit on step 4, 8, 12 or 16, one 16th before a beat. It sounds urgent and drives forward, especially before the bar line (step 16 into step 1). One pushed chord per bar or two is enough; every chord pushed sounds like the pattern has slipped a 16th.
- **The dotted cross-rhythm.** Hits every three 16ths (steps 1, 4, 7, 10, 13, 16) pull against the four-beat kick. If this figure restarts every 16 steps, its placement repeats each bar. Continuing a cycle across bar lines creates the longer rotation covered separately in M7d.

Two things limit it. The beats stay audible as reference: syncopation only sounds like syncopation against something steady, here the TR-6S and the pattern's own anchor hits. And a fully syncopated pattern stops sounding syncopated; it sounds like a shifted pattern. The aim is a pull towards the off-beats, not a complete move.

### What the code does today

- The older SYNC dial (shift chosen hits one 16th later or earlier, `seq_morph.c`) is forced to 0 in MUSIC WNDR/JRNY (`seq_morph.c` dial roll; `rhythm_begin()` in `seq_rhythm.c`). The cumulative phrase overlay replaced it.
- `rhythm_mutate()` moves one non-anchor hit by one 16th. The direction comes from momentum and from `phrase_direction()`, which only avoids the other part's predicted attacks. It does not know whether the destination is on a beat, an off-beat 8th or a push, so a move away from the off-beat is as likely as a move towards it, and syncopation appears only by chance.
- The beats (steps 1, 5, 9, 13) never move or drop. Keep this: they are the reference that makes the rest syncopated.
- Only the older DENS dial weights new hits towards the off-beat 8th (weight 3 on step 3/7/11/15, 2 on other 16ths).

### What is needed

M7a, metric weighting of moves: when `rhythm_mutate()` and `phrase_direction()` choose between the two neighbouring 16ths, weight the destination by its place in the beat: off-beat 8th highest, push (step 4/8/12/16) next, other weak 16ths next, a beat never (anchors are already fixed; an ordinary hit should also not be moved *onto* a beat by this preference). Keep the existing peer-avoidance as a stronger factor: syncopating into the other part's attack is still a collision. Keep it a soft preference with the existing stochastic direction so the phrase still drifts.

M7b, a cap on pushes: at most one or two pushed hits per bar per part, and never push a hit that already ties or slides. This keeps the push an accent instead of a shift.

M7c, only if M7a is accepted: a rare dotted cross-rhythm opportunity during development stretches (M9), built from the existing move/repeat operations, not a new pattern generator. Reject it if it sounds like a different pattern instead of the same motif rotating.

Resource estimate for M7a/b: a weight table and a few comparisons; no new state. M7c has its own estimate below; M7d is not yet costed.

Tests: a straight four-on-the-floor chord pattern drifts towards off-beats over many loops; a pattern already on off-beats stays there; anchors never move; the push cap holds; ties/slides/ratchets untouched; peer collisions still decrease (M1 fixture); seeded repeatability.

Acceptance: over a long WNDR pass the share of hits on off-beat 8ths and pushes rises, while the motif stays recognisable and the beats remain present. Listening: the groove leans forward and sits better against the TR-6S kick. Reject if it sounds as if the pattern has slipped out of time.

### M7d — unequal looping lengths (polymeter)

One part keeps a shorter repeating motif while the peer or TR-6S supplies the steady 4/4 reference. Both use the same clock. At straight 16th-note spacing, 12 steps against 16 meet again after 48 steps (three 4/4 bars); 15 against 16 meet after 240 steps (15 bars). This is distinct from off-beat emphasis and remains a requested outcome even if M7c is dropped.

Existing playback already uses each track's `P_SLEN`: ordinary `seq_tick()` wraps its index at that length, and ARP SEQ indexes the pattern using that length (`seq.c`). Begin with owner-set unequal lengths in the M0 fixture and trace actual onsets across the full joint cycle. This baseline does not establish automatic length behaviour or device acceptance.

Automatic shortening is a separate bounded prototype: decide allowed lengths, change/return boundaries and enablement before implementation. Preserve stored steps outside the shorter window, protected ties/slides/ratchets, manual takeover and KEEP/UNDO meanings. Do not reset the shorter loop at each shared bar. Reuse existing length controls for the baseline; a new control is not assumed.

Review M7 metric weighting against actual playback phase, division and swing: a local `i % 4` index is not always a beat in the shared bar. Preserve original motif anchors without forcing them onto global downbeats. M1 peer prediction follows each actual cycle; M8 turn-taking and the M9 arc follow shared musical bars.

Acceptance: the motif audibly rotates against the steady beat and reunites at the expected joint cycle, without clock drift, lost notes or forced bar resets. Compare 12/16 and 15/16 first without swing, then with swing and ARP SEQ; verify STOP/PLAY and manual length edits. Record software onset traces and owner listening separately. Automatic policy and resource cost remain pending and are excluded from the estimates below.

## M8 — question and answer between the two parts

### What it means musically

Call and response is a conversation: one part plays a short phrase that sounds open (a question), and the other part replies with a phrase that sounds closed (an answer). In a 4-bar phrase the usual shape is: bars 1–2 part 1 asks while part 2 stays in the background, bars 3–4 part 2 answers while part 1 steps back. Over the next phrase the roles can swap. What makes it audible:

- **Taking turns.** The asking part is fuller and more active for its two bars; the answering part waits with sparse hits or long held notes, then becomes the active one. If both parts are equally busy all the time there is no conversation, only two monologues.
- **Turnarounds that alternate.** The last bar of a phrase (the turnaround) is where a loop usually varies. If part 1 varies at bar 2 and part 2 at bar 4, the variations themselves become a question and its reply.
- **The answer echoes the question.** The reply reuses the question's rhythm (or a recognisable part of it), shifted into the space the question left, so the listener hears that the two belong together. An unrelated reply sounds like coincidence.
- **Open and closed endings.** With pitch, a question ends away from home (the 2nd, 4th or 6th degree, or the top of the chord) and the answer ends at home (the root, or the 5th). Rhythmically, a question can end on a push or an off-beat; an answer ends on or near a beat.

The two parts still share the groove: the beats stay, and they may play together on downbeats. Turn-taking is about who leads, not about silencing one part.

### What the code does today

- `phrase_peer_attack()` predicts the other part's attacks and the gap/repeat/direction choices prefer the space between them (M1). This is avoidance: the parts do not talk at the same moment. It does not decide who leads, and the parts do not respond to each other's material.
- `rhythm_poll()` alternates which part receives a development edit (`rhythm.turn`). These are pattern edits every two loops, not bar-by-bar turns in a phrase.
- VARY BARS 4 + 8 plays the variation on every fourth bar, but `sqm_bar_turn` is one shared offset (`seq_play_step()` in `seq_morph.c`), so both parts vary on the same bars 4 and 8, together.
- There is no notion of a 2- or 4-bar phrase, no open/closed ending, and no reuse of one part's rhythm by the other.

### What is needed

M8a, alternating turnarounds (smallest, try first): give each part its own bar offset in the VARY BARS 4 + 8 rule, so part 2 varies on bars 2 and 6 and part 1 on bars 4 and 8. Each part's variation is then followed two bars later by the other's. About one line plus tests. VARY BARS ALL is unchanged.

M8b, turn-taking within a phrase: from the shared bar counter, assign each 4-bar phrase a leading part for bars 1–2 and the other for bars 3–4 (swap the order every other phrase from the seed). In its following bars the non-leading part plays a thinned version of its current pattern: the existing gap rules applied as a per-bar playback mask (keep the beats and long notes; drop some optional hits), not as stored edits. The leading part may use its current variation. Only parts selected for TIDE take part; a single selected part plays as today.

M8c, echo and endings, only if M8b is accepted: when the leading role changes, give the new leader a bias to reuse the previous leader's last-bar rhythm on its own free steps (through the existing move/repeat operations). With PITCH on and MUSIC voice leading available, bias the leader's last note of bar 2 away from the root and the answer's last note of bar 4 towards the root or 5th of the selected scale, without inventing harmony (see the rule on scales above). ARP SEQ plays the held chord, so the pitch part does not apply there.

Resource estimate: M8a none; M8b a few bytes of phrase state plus a mask path in the step read (check the ISR cost: `seq_play_step()` runs in the audio interrupt); M8c depends on S0.

Tests: per-part VARY offsets with different pattern lengths and DIVs; STOP/PLAY keeps whatever bar-count behaviour VARY BARS has today; the mask never removes beats, ties or slides; one selected part; WAIT holds; external clock; KEEP stores the varied pattern, not the mask.

Acceptance: listening on a two-part scene, the parts audibly take turns and one sounds like it replies to the other. Turnarounds no longer collide. Reject if the thinned part sounds like it is dropping out by mistake, or if the drive suffers.

## M9 — buildup, peak, stop and breakdown

### What it means musically

Techno is arranged in tension and release. A **buildup** raises tension gradually: the sound gets brighter, rougher and more resonant, and the part gets busier — more hits, repeats, ratchets, syncopation — so every bar feels a little more urgent. It ends at a **peak**: the densest, harshest bar. Then comes the **stop**, a sudden drop: the FM-1 falls silent for a bar or two and only the effects tails and the TR-6S remain. The silence is what makes the peak feel like a peak. After it, the **breakdown**: the opposite colour, a soft, warm, consonant sound with slow attacks, long release and more space (reverb/delay), and a sparse sequence — few, long notes, the beats and a rare extra hit — so the music breathes. From there the next buildup starts.

The owner's specific link: **harshness and busyness go together.** When the sound is harsh it should become gradually busier; when it turns pleasant and harmonic the sequence becomes rare. Sound and rhythm are no longer moved independently but by one shared tension.

Musical details that make it work:

- **The buildup is gradual and long** (16–32 bars), the stop is abrupt and short (1–2 bars), and the breakdown sits between (8–16 bars). An even split sounds mechanical.
- **The stop falls on a phrase boundary** (the end of an 8- or 16-bar phrase) and does not cut a sounding note's release or the effects tails. It is a musical rest, not a mute.
- **The motif survives.** After the breakdown the evolved phrase is still recognisable: the breakdown thins it and the next buildup fills it again from there. It does not restart from the original pattern.
- **Both parts move with the arc, not identically.** During the buildup the call-and-response (M8) can quicken (turns every bar instead of every two bars); in the breakdown one part may hold a long chord while the other plays the rare notes.
- **"Harsh" and "harmonic" are sound qualities, not loudness.** Harsh: high cutoff, high resonance, drive, strong FM modulation and feedback, short percussive envelopes, noise. Harmonic and pleasant: low cutoff, low resonance, simple whole-number FM ratios, low modulation, long envelopes, more reverb. A brighter sound also sounds louder; M5's loudness checks apply.
- **The arc varies.** Not every buildup reaches a stop; lengths vary from the seed (as M4b proposes) so the cycle is not predictable.

### What the code does today

- `morph_complement.c` already measures four qualities from parameters (no audio analysis): brightness (LP cutoff), movement (filter LFO depth), space (reverb+delay+chorus) and envelope length (including FM6 carrier rates). JRNY's `sound_complement_pick()` uses them to prefer a sound that contrasts with the other part.
- WNDR walks brightness and other dials independently of the sequence.
- Phrase density is bounded: `phrase_repeat()` adds at most a quarter more hits than the original, and `phrase_gap()` removes no more than half. These bounds are the same at every moment.
- The phrase tiers (16/32/64/128 bars) make larger changes more likely in longer phrases, but there is no shared tension and no stop or breakdown.

### What is needed

M9a, a harshness score: extend the parameter measures with resonance, drive and, for FM6, modulator levels/feedback and ratio simplicity; NOISE engines count as harsh. Usable for the current sound (WNDR) and JRNY candidates.

M9b, harshness drives busyness (the owner's link; can be tried before the full arc): set the phrase density bounds from the current harshness — a harsh sound may reach more added repeats and the occasional ratchet; a soft sound is held towards fewer, longer notes. This replaces the fixed bounds above and is a deliberate change to the rule "preserve existing density bounds", asked for by the owner.

M9c, the arc: one tension value shared by both parts, advanced by musical bars (not wall time) through build → peak → stop → breakdown → next build. During the build the WNDR walk is biased towards the harsh side and the M7 syncopation weighting is strongest; at the peak the density bound is highest; the stop is a phrase-aligned rest of the TIDE parts' sequences (notes release naturally, tails ring); in the breakdown JRNY scoring prefers soft, harmonic candidates (adding a sign from the arc to `sound_candidate_score()`) and the density bound drops below today's minimum. STOP of the transport freezes the arc, resume continues it, a new WNDR/JRNY session starts it fresh. WAIT bars count as musical time.

The sudden stop is a deliberate exception to the "no endpoint jumps" acceptance of M4; that rule still applies to the sound glides themselves.

Limits: JRNY stays within the current engine and category (see M3), so the soft breakdown sound must come from the same palette; crossing categories for breakdowns would be a separate owner decision. Long sustained breakdown chords depend on the sound's own envelope. M9c needs S0 for flash room.

Relation to M4: M9 covers the same ground as M4a's settle/develop/open/recover arc and is the owner's more specific version of it. Whether M9 replaces M4 or the two are combined is an open owner decision; until then, design them together and do not build both.

Tests: harshness ordering on known presets (e.g. a resonant acid sound above a soft pad); busyness tracks harshness in both directions; arc boundaries at different TIME/WAIT values and pattern lengths; stop aligned to phrase ends and never cutting a release; STOP/PLAY and resume; one or two selected parts; AMOUNT zero; KEEP during each phase; long-session bounds over at least 256 bars (no progressive thinning, no permanent maximum brightness or density).

Acceptance: a 10-minute session has audible buildups, peaks, clean stops and calmer breakdowns; harsh stretches are busier and soft stretches sparse; the motif carries across; the TR-6S stays in front during stops. Reject if the arc sounds scripted, if stops sound like dropouts, or if the background starts drawing more attention than the instruments it supports.

## M10 — smoother sound transitions

### What it means musically

A sound change in a background should feel like the same instrument slowly turning into another, not like a sequence of small edits. Three things make a change sound smooth to a listener:

- **Continuous motion.** A filter that opens over 16 bars should sweep, not climb a staircase. On a held pad or a dub chord with long release, every small jump in cutoff, envelope or send is audible as a tiny tick or a sudden change of colour, and dozens of them over a glide sound restless.
- **An order in which things change.** When a sound designer moves from one patch to another by hand, they rarely move everything at once. The brightness usually leads (it is what the ear follows), the envelope shape follows, and the space (reverb, delay, chorus) trails, so the old sound's tail is still in the room while the new colour arrives. Everything moving in lockstep on one curve sounds mechanical and makes the middle of the trip a muddy average of the two sounds.
- **Changes that land with the music.** Switches that cannot blend (arp mode, voice mode, a chord) are least noticeable when they coincide with a musical boundary, a new bar or phrase, rather than at an arbitrary moment in the middle of a note.

Loudness belongs here too: a sound that becomes brighter seems louder, one that closes or thins seems to drop away. M5 already covers measured level problems; M10 must not introduce new ones.

### What the code does today

- Structural switches are already hidden carefully (`morph_xfer.c`): FM6 algorithm changes cross a bridge of pure sines with equal-power fades; wave, filter type and model switches happen while the filter valley is closed; the insert fades to dry while its type changes; the sends swell so the tails carry the old sound. Keep all of this.
- Continuous values blend linearly between the two ends at the fader's position (`morph_apply`). The fader has 128 positions (`morph_wb`), the ends are stored as whole-number parameter values, and the result is written once a block with no interpolation between fader positions. The part filter's dial has 82 positions (`UF_STEPS`) and is read without its own smoothing (`voice.c`, `ucut0`). On a long glide this most likely produces audible stairs; measure it before changing anything.
- Every continuous value follows the same S-curve (`glide_ease`) at the same time.
- Values that only shape the next notes (arp, voice and glide mode, chord) switch at the fader's middle (`morph_xfer.c` header).
- JRNY stays within one engine, so the transition never has to cross engines.

### What is needed

M10a, measure first: render slow glides on the host (1, 16 and 64 bars) between pairs with large cutoff, envelope and send differences, and record the effective cutoff, levels and send amounts every block. Count the steps and their size; listen for them on a sustained chord. If no audible stairs exist, record that and skip M10b.

M10b, continuous values without stairs: interpolate the blend weight between fader positions every block (the glide knows its exact progress, finer than 128 positions), and give the cutoff dial sub-step resolution through the morph path so a slow sweep is continuous. Keep the exact endpoints: fader 0 is End A and 127 is End B bit for bit, and a sound at rest is unchanged. Manual knob turns keep their current response.

M10c, staggered groups: inside the existing travel, give each group its own portion of the curve — brightness and filter lead (for example 0–70 % of the trip), envelope and shape in the middle (15–85 %), space and effects trail (30–100 %). Each group still starts at its End A and ends at its End B, so the arrival is identical; only the path differs. The structural valley/bridge timings stay where they are, and the groups must agree with them (the filter must be closed when a switch happens). Try the order and spans by listening; reject it if the middle of the trip sounds less coherent than today.

M10d, musical switch timing: move the switches that only shape the next notes from the exact middle to the first bar (or note-on) boundary inside the hidden stretch, so they arrive with the music. Fall back to the middle when there is no clock or no boundary in time.

Not in M10: crossing engines (JRNY keeps its engine; a cross-engine change would need two engines sounding at once, with CPU and voice cost, and is a separate owner decision); automatic loudness matching (M5, evidence-based).

Resource estimate: M10b a per-part fractional weight and a few bytes of cutoff state, plus per-block work in the audio ISR that must be measured against the hot headroom; M10c a small table of group spans; M10d a few bytes of pending-switch state.

Tests: endpoints bit-exact at fader 0 and 127; at-rest sounds unchanged; monotone sweeps with no step larger than a set threshold on long glides; every structural switch still inside its hidden stretch with groups staggered; manual knob edits and pinned B values still work; STOP/PLAY pause and resume mid-trip; hand takeover mid-glide; external clock; CPU cost of the per-block interpolation.

Acceptance: a 16- and a 64-bar glide on a sustained chord sound like one continuous change with no ticks or stair-steps; the old sound's space audibly carries into the new colour; no new clicks, level dips or CPU problems on the unit. Reject a group order that makes the middle of the trip sound like neither sound.

## Feasibility — space and controls for M3 and M7–M10 (2 November 2026)

### Room available today

Measured on the current base (`work/s0-build/base-build.log`, 2 November 2026 07:17): image 579,916 of 581,564 B, so **1,648 B of flash free**; ordinary RAM 1,712 B above the 4 KiB reserve; pool + range Z 2,964 B above the 8 KiB reserve; hot audio code 1,240 B above the 228 B reserve; range X 928 B. The S0b candidate in the same folder brings the image to 578,988 B (**+928 B**, 2,576 B free). S0 work is in progress there (S0a pattern-slot tests were failing at 07:35); this study does not touch it.

RAM, pool and range Z are not the constraint for the costed proposals below: each needs well under 200 B of state. **Flash is.** M7d is uncosted; its timing implications must be checked separately. The hot audio block matters only for M8b and M10b among the costed proposals, which change code the audio interrupt runs.

### Estimated cost per stage

Estimates come from the sizes of comparable functions in the current image (`felucca.dis`): `rhythm_mutate` 586 B, `phrase_peer_attack` 1,842 B, `sound_axis` + `sound_candidate_score` 656 B, `tide_journey` 1,444 B, `morph_apply_at` 1,082 B, `morph_xstep` 1,994 B; all TIDE rhythm/phrase/sound/sequence-morph code together is about 9.4 KB. These are ranges for planning, not measurements; each slice records its real delta.

| Stage | Flash | RAM | Hot/ISR | Notes |
|---|---|---|---|---|
| M3 destination memory | 300–500 B | ~20 B | none | ring of four per part |
| M7a+b syncopation weights, push cap | 200–400 B | 0 | none | inside `rhythm_mutate`/`phrase_direction` |
| M7c dotted cross-rhythm | 300–500 B | 0–4 B | none | only if M7a accepted |
| M7d polymeter | unmeasured | unmeasured | to study | existing unequal lengths first; automatic changes excluded from subtotal |
| M8a alternating turnarounds | 50–100 B | 1–2 B | ~20 B | `seq_play_step()` runs in the ISR |
| M8b turn-taking mask | 600–1,000 B | 8–16 B | 50–150 B | per-bar playback mask in the step read |
| M8c echo and endings | 800–1,500 B | ~16 B | none | needs PITCH; not for ARP SEQ |
| M9a harshness score | 400–700 B | 0 | none | extends `sound_axis` |
| M9b harshness drives busyness | 200–400 B | 0 | none | density bounds from harshness |
| M9c the arc (build/peak/stop/breakdown) | 1,000–2,000 B | 16–32 B | none | replaces M4a if chosen |
| M10b no stairs | 300–600 B | 8–16 B | 100–300 B + CPU | per-block interpolation; defeats the blend cache while gliding |
| M10c staggered groups | 300–600 B | 0 | 50–150 B | |
| M10d switches on the beat | 200–400 B | ~4 B | none | |
| New TIDE LIST rows (three, see below) | 500–900 B | 3 B | none | labels, handlers, store fields |
| **Costed stages, excluding M7d** | **about 5.2–9.6 KB** | **~100–150 B** | **~250–650 B** | |

### How to make the room

- **S0b** (size policy for `multi.c` and more of `morph.c`): measured +928 B.
- **S0c** (smaller FM6 pitch table): about 3 KB; needs a tolerance in FM6 tests and the M0 pitch check.
- **S0e** (factory data packing): unmeasured; the factory sounds and multis are 19 KB, so a tighter encoding may give several KB.
- S0a and S0d free pool, not flash.
- Owner-decided drops from 1 November, if still needed: splash logo about 6 KB, S font Huffman about 1.8 KB, extras about 2.1 KB, the 1.1 guide about 1.2 KB.

With S0b + S0c the flash free is about 5.6 KB: enough for M3, M7a–c, M8a/b, M9a/b, M10b/d and the three rows (about 3–5 KB), not for everything. M7d is excluded until measured. M8c and M9c, the two largest costed musical pieces, need S0e or one of the drops. M10b additionally needs a measured CPU budget on the unit: the blend is cached today while the fader stands still, and interpolating every block during a glide removes that saving.

Feasible order by room: S0b → M3 → M7a/b → M8a → (S0c) → M10a measurement → M10b/d → M9a/b → M8b → rows → (S0e or a drop) → M9c → M8c → M10c.

### Controls: what the user decides, what stays in code

The owner (2 November 2026) wants a few choices back: how much change to expect, the energy, how random, and whether buildup and breakdown phases are allowed. These are **intent** choices: they set how TIDE behaves, not individual parameters. Everything else stays automatic.

The TIDE page's four columns (GLIDE, MODE, TIME, WAIT) are full, so the new choices go into the TIDE LIST, which has six rows today (TARGET, VARY BARS, ROLL, ARP, PITCH, WASH LEVEL). The TIDE settings record stores 7 of its 32 field bytes, so three more fit; the loader must accept the older, shorter record (keep the stored fields, default the new ones) so an update does not reset the owner's settings.

User decisions (TIDE LIST rows, all automatic MUSIC WNDR/JRNY behaviour):

- **CHANGE — SLIGHT / MEDIUM / BOLD** (default MEDIUM = today). How much each trip changes: SEQ AMT (today fixed at 50 with no knob), how often phrase edits happen, and how many edits a trip may carry. The sound's AMOUNT on the MORPH page stays as it is; CHANGE scales the sequence and phrase side, AMOUNT the sound side.
- **ENERGY — DEEP / EVEN / DRIVE** (default EVEN = today). The character of the background: DEEP leans to sparse, on-beat-and-off-beat placement, darker and softer sounds; DRIVE to busier, more syncopated patterns (M7 pushes), brighter and harsher sounds (M9b). It biases the existing walks; it does not fix a sound. If "upbeat / downbeat" was meant as the feel (on-beat versus off-beat placement) rather than energy, this row is where that choice would live; to be confirmed with the owner.
- **ARC — OFF / BUILD / FULL** (default to be decided by listening). Whether buildup and breakdown phases are allowed: OFF = today's even evolution; BUILD = gradual buildups and easing back, no stop; FULL = buildup, peak, stop and breakdown (M9c).
- **Randomness — the existing ROLL row (MUSIC / SMART / WILD)** already chooses how free the changes are. No new row; its labels and help text should say so.

Shown, not chosen: the TIDE status line can show where the arc is ("BUILD 12/24", "PEAK", "BREAK 3/8") in the same way it shows "WNDR TO B 3/8" now. It costs little and lets the owner anticipate a stop while playing.

Code-only (no control; tuned by listening):

- M3 destination memory; M7 metric weights and push cap; M8 turnaround offsets, turn-taking, echo and open/closed endings; M9 harshness score, phase lengths and their variation, stop placement on phrase boundaries; M10 smoothing, group order and switch timing.
- These read the three choices above where it makes sense (CHANGE scales edit counts, ENERGY biases syncopation and harshness, ARC enables M9c), but expose no separate settings.

This changes the earlier rule "no new menu, save format, global mode" for these three rows only, at the owner's request. Each row still needs its own listening check that its settings sound clearly different and that the default matches today's behaviour.

### Open decisions — parked 2 November 2026

The owner parked these for a later decision. Claude's recommendations, with the reasons, so the decision can be made without redoing the discussion:

1. **Who owns the drop.** Recommended: the buildup runs automatically, reaches PEAK and holds it, shown on the status line; the owner fires the stop with one gesture; if nothing happens within about 8 bars it stops by itself at the next phrase boundary. Reason: an automatic drop will eventually land on the owner's own S-1 line or SP-404 fill, and an arrangement decision belongs to the performer.
2. **ENERGY and ARC.** Recommended: keep both, as *average* (ENERGY) and *swing* (ARC): dub techno is low energy with a big swing, peak-time techno high energy with a small one. Merge them if listening shows their combinations do not sound clearly different.
3. **"Upbeat / downbeat".** Recommended: read it as energy, and let syncopation follow ENERGY (more drive = more pushes and off-beats) instead of being its own setting; a separate feel setting mostly produces combinations that sound wrong.
4. **CHANGE.** Recommended: per trip and still cumulative, with no pull home (the owner's earlier decision); SLIGHT = small steps that add up slowly. HOME remains the way back.
5. **Space.** Recommended: keep the splash logo; do S0b, then S0c, then measure S0e; weigh the logo against M9c/M8c only if the arc still feels missing after the cheaper stages and S0e is small.
6. **Order.** Recommended: M10a/b, M8a, M7, M8b, M9b, then M9c. M10b improves every glide, M8a is almost free, M9c should sit on accepted layers.
7. **Row count.** Recommended test for every row: would the owner change it during a set? If not, it is tuning and belongs in code. Cut ROLL WILD or CHANGE BOLD if they sound alike. M7d automatic polymeter enablement and allowed lengths remain open; do not silently tie loop-length changes to ENERGY or assume another row is approved.
8. **A launched buildup and drop** (owner idea, 2 November 2026): start a buildup by hand during play that runs over a set number of bars, drops and recovers, without a new screen. Placement candidates: OCT+ held + K1 on the TIDE page (beside OCT- + K1 = HOME), GLIDE A + GLIDE B pressed together on PERFORM, or a sixth MODE; undecided.

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

### S0a and S0b — 2 November 2026

Isolated checkpoint: branch `s0-room`, from `jam11` `8506f2c` (firmware source as `a41d1bf`). It was not flashed or merged at this checkpoint; the subsequent integration and installation are recorded below.

- **S0b:** `tools/size_fns.py` takes `multi.c` and the whole of `morph.c`. Its audio-rate code (`morph_apply`, `morph_cur`, `morph_begin`, ...) is reached from the audio interrupt, so `isr_reach` keeps it at -Os; 1,344 functions built for size (1,224 before), hot audio code unchanged. The single names of `morph_xfer.c` and `eng_fm6.c` stay listed. The main-loop morph steps during a glide (`morph_do`, `morph_ramp`, `morph_build`) are now built for size; no CPU measurement on the unit yet.
- **S0a:** `patslot.c` keeps ONE pattern object in RAM (`ps_buf`, 3,584 B) plus every slot's name, LEN and flags (`ps_meta`, 480 B) in place of all three objects (10,752 B). A LOAD reads its object into RAM; while RAM holds another object's waiting SAVE, the slot is read from the flash for that load only (no write). An object that was missing or damaged is its defaults, built again when needed. Owner rule: a SAVE into a different object (P01-11, P12-22, P23-32) while another object's SAVE still waits during playback writes the waiting one at once, with the flash duck's short dip; the owner accepted it as rare and barely audible. Format, flash map and the editor backup are unchanged.
- **Date pin:** MENU > SYSTEM > ABOUT shows `__DATE__`, so `jam/check_off.sh` failed on every day after its baseline was recorded. `build.py` now takes `DUELL_BUILD_DATE` and `check_off.sh` pins it to `Nov  1 2026`. Proven: the date pin alone, on today's date, reproduces the old `6232c047...` exactly.

| Region | `a41d1bf` | `s0-room` | Delta |
|---|---:|---:|---:|
| Application flash (image) | 579,916 B | 579,712 B | -204 B (S0b -928, S0a +724) |
| Pool + range Z | 357,484 B | 350,796 B | -6,688 B (free to the stack guard 11,156 -> 17,844 B) |
| Ordinary RAM | 67,920 B | 67,936 B | +16 B |
| Hot audio code | 11,044 B | 11,048 B | +4 B |

TIDE-off baseline moves to `b531ace3...` (shared code, as S0 expects). Verified: device build with all placement/reach checks, packaged-firmware CPU-emulator smoke (boot, LCD, MIDI note, finite audio; package `a6a1aa72...`), `check_off` PASS on the new baseline, and the full host suite on a clean `a41d1bf` snapshot against the candidate: the same 58 pass / 63 fail / 11 skip groups and no differing check line in any group. `patslot` was repaired enough to run: the stale per-slot ERASE checks are gone (1.1 removed that UI), its pattern generator keeps SWING in 1.1's 50..75, and seven S0a checks are new (all pass). Its remaining 22 failures are old UI-flow checks and fail identically on the baseline (T0). Not established: CPU on the unit, the dip on the unit, a real-flash LOAD while a SAVE waits.

Flash is still the limit: 1,852 B free. S0c (smaller FM6 pitch table, about 3 KB) is the next flash step.

### S0/DSP integration and device installation — 2 November 2026

S0a/S0b were merged at `254a501`. The owner authorized flashing checkpoint `e176f6e` (firmware through `7535916`, followed by a test-only commit). The retained production build passes all placement, reserve and call-reach checks: image **579,728 B, 1,836 B flash free**, ordinary RAM 67,936 B, pool + range Z 350,796 B, hot audio 11,048 B with 1,464 B headroom. Against the previously installed M1/M2 build, flash is -104 B, ordinary RAM +16 B, pool -6,688 B and hot audio +4 B. This checkpoint also contains the shared DSP arithmetic fixes and the TIDE settings startup-write fix; M7–M10 are not implemented by it.

Exact package SHA-256 `ba8fdaf9252a0cf64f5230472ce10e738801b22926b2619e46cb81216648a8aa`. This package passed the pinned CPU-emulator boot/LCD/MIDI/audio smoke; five revision-8 backup-validator tests passed. The connected FM-1 identified as FM-1_901 / DUELL 1.1 with 128 patch slots and 10 named engines. Two fresh pre-flash captures matched all 16 validated objects. The normal installer completed at 100%, rebooted and verified the same model and DUELL identity.

Initial readback matched 15/16 objects; only Part 2's live End-B destination in current music differed after restart, as in the previous installation. A validated music-only restore recovered the saved session. Final capture matches **all 16 objects byte-for-byte**. Evidence, exact package, backups and `INSTALLATION.md` are retained in `work/backups/s0-e176f6e-2026-11-02/`. These are installation and data-preservation results. Device listening, TR-6S clock, CPU/UI behavior, and pattern save/load during playback (including the cross-object dip) remain pending. The existing backup protocol does not separately archive TIDE preferences. No push or release was performed for this checkpoint.

## T0 — repair the host test baseline

On 1 November 2026 (`docs/JAM11_TEST_BASELINE.md`) 63 of 121 host test groups failed on plain 1.1 before any TIDE change: upstream's own tests use names 1.1 removed (`CF_ERASE_PSLOT`, `ps_erase`, `up_xb_in`, `ui.plk_src`) or check 1.0 behaviour (SWING from 0, old argument counts). Comparing against that list catches new breakage only where a test still runs; the pattern slots, multis, backup, persist, project, UI and editor have no working net. Repair one group at a time, test-side only (no firmware change to make a test pass, unless the test found a real bug: then that fix is its own always-on change with an `off.sha256` update). Order by what TIDE and S0 touch: `patslot` (22 UI-flow checks left), `backup` (does not compile), `persist`, `multi`, `project`, `ui`/`uitest`, `morphui`, `perform`, then the rest. Each repaired group moves from FAIL to PASS in `JAM11_TEST_BASELINE.md`.

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
