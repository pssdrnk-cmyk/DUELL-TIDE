# TIDE musical design notes

Status: deferred proposals, retained 2 November 2026. Active priorities and delivery state live in [the roadmap](../ROADMAP.md). No feature in this document is made active merely by being described here.

These notes preserve the owner's musical concepts, including polymeter and CHANGE/ENERGY/ARC, and their open decisions. Existing-behavior descriptions reflect the original design review; recheck affected source before implementation. Resource estimates and prior delivery receipts are retained in the [historical snapshot](../history/TIDE_ROADMAP_2026-11-02.md), not a current memory budget.

M4 and M9 overlap: retain both descriptions until the owner resolves the arrangement policy; implement one scheduler rather than two. M3's four-entry destination history is ephemeral session state, unrelated to saving generated patterns to flash. Exact session restart continuity is a separate storage concern.

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

**Built (2 November 2026 session `tide-turns`): POLY.** TIDE SETTINGS row POLY: OFF / GENTLE (the default) / WIDE (`seq_poly.c`). In automatic MUSIC WNDR/JRNY (`sqm_auto`, set from `glide_poll`) one part's loop is shortened to len-1 or len-2 (GENTLE: 15 or 14 of 16) or len-1 to len-4 (WIDE: 15 to 12), for 8 (GENTLE) or 4 (WIDE) loops, then plays whole for at least 16 / 8 loops. The full loop is the reference: every change happens where it wraps (its first step), so the return re-locks the groove with no drift; inside a stretch the short loop restarts by itself, not at the shared bars. Chosen from the seed (`sqm[].tseed`, `morph.seed`) and the loop count, only at a phrase's first loop (a multiple of 4), never before the 8th loop after PLAY, the parts in turn and never both at once, a loop under 8 steps never. ARP SEQ is covered through `arp_idx`; the peer prediction (M1) returns no attack while a part is shortened.
KEEP / UNDO policy: the shortened length is **performance only**. `P_SLEN` and the stored steps are never written, so KEEP stores the steps at the stored length (the variation as it plays, as before) and UNDO has nothing to undo about the length. Not captured. STOP/PLAY starts every part whole; POLY OFF, the glide ending (a hand taking over), a recording or a changed stored length bring the part back at the next full loop. WAIT does not change it (the flag stays set while the glide holds).

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

M8a, alternating turnarounds (smallest, try first): give each part its own bar offset in the VARY BARS 4 + 8 rule, so part 2 varies on bars 2 and 6 and part 1 on bars 4 and 8. Each part's variation is then followed two bars later by the other's. About one line plus tests. VARY BARS ALL is unchanged. **Built (session `tide-turns`):** TIDE SETTINGS row TURNS: TOGETHER / TAKE TURNS (default). In automatic MUSIC WNDR/JRNY with both parts selected, part 2's turnaround is offset by two shared bars (`seq_play_step`); the offset counts the shared `sqm_bar`, so it holds for any lengths and DIVs. VARY ALL, one selected part, ONCE/LOOP, manual playback, stopped and external clock are unchanged.

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

### Controls: what the user decides, what stays in code

The owner (2 November 2026) wants a few choices back: how much change to expect, the energy, how random, and whether buildup and breakdown phases are allowed. These are **intent** choices: they set how TIDE behaves, not individual parameters. Everything else stays automatic.

The TIDE page's four columns (GLIDE, MODE, TIME, WAIT) are full, so the new choices go into the TIDE LIST, which has six rows today (TARGET, VARY BARS, ROLL, ARP, PITCH, WASH LEVEL). The TIDE settings record stores 7 of its 32 field bytes, so three more fit (TURNS and POLY use two: fields 7 and 8; the loader accepts the 7-field record and defaults them to TAKE TURNS / GENTLE); the loader must accept the older, shorter record (keep the stored fields, default the new ones) so an update does not reset the owner's settings.

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

