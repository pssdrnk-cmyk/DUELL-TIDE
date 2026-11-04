# TIDE musical design notes

Status: planning proposals reviewed against source `6af479d` on 3 November 2026. Active priorities and delivery order live in [the roadmap](../ROADMAP.md). No new musical firmware is implemented by this review. Since then (branch `integ`, 4 November 2026) M3 (JRNY memory and role families), M8a TURNS, M7d POLY, M7a/b SYNC, M8b/c ANSWER (LEAD / ECHO), M4/M9 ARC and STYLES have been built; see the [TIDE guide](../TIDE_GUIDE.md). The separate test-tree cleanup is finished; no test changes are assigned here.

These notes preserve the owner's musical concepts, including polymeter and CHANGE/ENERGY/ARC, and their open decisions. Recheck affected source before implementation. Resource descriptions below identify work/cost risks, not measured allocations or proof that the queue fits. The installed package (`187e9d0`) has about 24.9 KB flash free after the boot pack (`ecd2918`); any newer candidate needs its own linked budget. Prior estimates and delivery receipts remain in the [historical snapshot](../history/TIDE_ROADMAP_2026-11-02.md).

M4 and M9 describe one future arrangement feature: use M9's musical intent with M4's restraint/recovery safeguards and one musical-time planner. Keep the alternative sketches below as design input, not two implementation tasks. Drop ownership, full-stop enablement and final phase policy remain open. M3's four-entry destination history is ephemeral session state. The owner does not require exact live-morph restart continuation. Saving stays simple: 4–8 USER destinations, existing overwrite behaviour acceptable, stopped saves sufficient. TIDE configuration backup is useful; future field growth is compatibility bugfixing. Current audio polish is accepted, so M5/M10 only become work for a demonstrated defect/regression or chosen feature change.

## Idea — broader musical roles for the two-part palette

Owner suggestion, 3 November 2026; an idea for discussion, with no implementation or priority assigned. Two parts are plenty; there is no desired track-count expansion.

Explore more roles within the existing palette, such as **dub atmosphere**, **techno rumble** and **polyrhythmic patterns**. A role could describe sound, rhythmic behaviour and use of space together. The two parts could take complementary roles, for example a low rumble beneath a sparse dub atmosphere. Exact behaviour and presentation remain open.

## M3 — JRNY destination memory

Current behavior: `tide_journey()` excludes the reached/left destinations where possible and the other part's originals/targets. It searches within the current engine/category, then widens its category fallback. It does not switch engines. Preserve that scope.

Feasibility: counting the 128 authored records in `tools/duell_sounds.py` gives 66 engine/category groups; 61 contain four sounds or fewer, accounting for 99 voices. Endpoint/peer exclusions reduce availability further. History is a modest repetition refinement, not a guarantee of a wide palette. Do not widen categories merely to fill a history ring.

### M3a: avoid premature returns

Add a four-entry recent destination ring per part, updated only when a new target is successfully adopted. Store destination identity with engine context; keep the ring ephemeral. Clear a part's history on explicit patch/multi replacement and HOME; pause/resume does not clear it. KEEP rebases and clears that part's history. Capture candidate availability before adding more scoring policy.

Candidate policy: apply existing hard exclusions, then prefer candidates outside recent history within the existing category/fallback stages. If the palette is too small, relax history from oldest first before excluding all valid candidates. Keep the peer's currently used targets protected. With no valid target, use the existing roll fallback. Maintain stochastic choice and current complement bias; do not select only the most different sound every time.

Tests: one/two/many candidates; all candidates in history; peer exclusions; successful versus failed adoption; patch/engine changes; repeatable seeded selection and independence between parts.

Acceptance: with at least five eligible candidates in a stable palette, avoid a return within four adopted targets. Small palettes remain alive and bounded, without stalls or unwanted category jumps merely to satisfy history. A long listening pass should feel less repetitive without losing the source's character.

## M4 — shape longer passages

Existing phrase tiers increase development opportunities at 16/32/64/128 bars. Existing rhythm mutation already moves, repeats or drops attacks, changes ordering and occasionally moves scale notes. Add restraint and recovery through this machinery rather than layering another random sequencer over it.

### M4a: a bounded phrase policy

The initial restraint/recovery sketch is a 64-bar policy: 16 bars settle, 16 develop, 16 open, 16 recover. Compare it with the current behaviour before choosing lengths/weights, then reconcile it with M9 in the same planner. Derive phase from accumulated musical time shared by both parts, not main-loop wall time; only selected parts receive edits. STOP freezes it, resume preserves it, and a genuinely new WNDR/JRNY session resets it. WAIT bars count as musical time without extra arc-triggered edits during a hold; existing phrase development during WAIT must be accounted for explicitly rather than assuming it already pauses.

- Settle: mostly retain the motif; allow small timbral motion.
- Develop: favor one eligible rhythm/voicing change at a time.
- Open: modestly favor existing brightness/motion and permitted density changes; the peer receives a complementary opportunity, not the same push.
- Recover: lower mutation frequency, remove optional added repeats where possible, and ease timbral offsets inward while retaining the evolved motif.

Recovery is not HOME: do not restore the original patch, overwrite the audible endpoint, or erase the accumulated melody. Generate new silent-end destinations with gentler deltas. PITCH OFF still prevents scale-degree edits. Keep the present rare pitch-mutation rate initially; change one musical variable at a time.

M4b, only if M4a is accepted: introduce small, bounded variation in phase lengths/choices from the existing seed stream, isolated so it does not perturb unrelated random decisions. Avoid a recognizably identical 64-bar script.

Tests: phase boundaries at short/long TIME/WAIT values; pause/resume; mode/target changes; parts with different pattern lengths; AMOUNT zero; protected notes; long-session state bounds. Record density, pitch spread and timbral dial ranges across at least 256 bars to detect accumulation or saturation.

Acceptance: audible contrast between active and restrained stretches, no endpoint jumps, motif still recognizable, no progressive thinning or permanent maximum brightness. Prefer the simpler current policy if the new arc sounds imposed.

## M5 — loudness and transition calibration

Status after owner clarification, 3 November: conditional only. Existing audio polish is accepted; this section is a method for an identified issue, not an outstanding general sound-polish milestone or a prerequisite to other work.

Measure M0 pairs and any outliers discovered in M1-M4. Use identical note/velocity/gate input for source/target comparisons. Inspect short-window RMS and peaks along the transition, plus integrated loudness for sufficiently long material where the existing tools support it. Listen to bass weight, sustain, transient impact and effects tails; brightness changes can resemble level changes.

Treat roughly 3 dB of unexplained sustained level change in a matched role as an investigation trigger, not an automatic normalization target. Expected envelope evolution, deliberate silence or the designed structural valley is not by itself a fault. Check WASH OFF first (current default), then LOW/MID/HIGH separately.

Fix a demonstrated cause at its narrowest point: incorrect unison scaling, crossing gain, or a specific preset's trim/envelope. Do not add an automatic compressor, blanket per-frame gain normalization or another wash layer. Any trim changes need independent audition and a check of their effect outside TIDE; prefer TIDE-specific correction when the source sound is already correct at rest.

Acceptance: the identified unwanted step/dip is reduced without pumping, clipped peaks, disappearing bass or altered at-rest sounds outside the authorized correction. If no clear outlier exists, complete M5 with measurements and no code change.

## Owner musical concepts — 2 November 2026

The owner listens to TIDE as a background next to a TR-6S and wants **syncopation and polymeter**, **question-and-answer** between the two parts, and **buildup and breakdown**. The owner clarified that polymeter is included as well: one part loops over a shorter length so its notes keep changing position against the beat. The stages M7, M8 and M9 below explain each idea musically first, then record what the code does today, what it needs and how to judge it. They follow the rules above. The exceptions the owner asked for are named where they apply. Nothing in them adds a page, control, saved field or mode: they are automatic MUSIC WNDR/JRNY behaviour, with automatic length changes still requiring the M7d study.

Step numbers below count a straight 16-step bar from 1: the beats are steps 1, 5, 9 and 13 (code index `i % 4 == 0`); the off-beat 8ths are 3, 7, 11 and 15; the 16ths either side of an off-beat 8th are 2, 4, 6, 8, 10, 12, 14 and 16. Unequal lengths or divisions need the actual shared-clock phase described in M7d rather than this local-index shorthand.

Syncopation decides *where* in the bar a part leans; polymeter lets its motif rotate against that bar through unequal loop lengths. Question-and-answer decides *which part* speaks in which bar. Buildup and breakdown decide *how much* is said over longer stretches. These are distinct listening goals. The roadmap orders existing-length evaluation first, then the small turnaround experiment, metric weighting and conditional later form; implementation need not follow this document's section numbers.

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
- Original hits at local indexes divisible by four are protected from these phrase moves/gaps. On straight 16-step sixteenth-note loops they coincide with beats; with other divisions/lengths they are motif anchors, not a universal shared-beat map.
- Only the older DENS dial weights new hits towards the off-beat 8th (weight 3 on step 3/7/11/15, 2 on other 16ths).

### What is needed

M7a, metric weighting of eligible moves: softly favour off-beat 8ths and occasional anticipations while preserving source anchors and stochastic drift. Use actual playback division/phase for the weights. The smallest candidate may support straight sixteenth-note 16-step loops first and leave other grids unchanged; declare that boundary explicitly. Retain M1's preference for space, but evaluate deliberate shared attacks as part of the groove rather than making collision avoidance an absolute rule. A beat can be a musically useful destination.

M7b, a cap on pushes: at most one or two pushed hits per bar per part, and never push a hit that already ties or slides. This keeps the push an accent instead of a shift.

M7c, only if M7a is accepted: a rare dotted cross-rhythm opportunity during development stretches (M9), built from the existing move/repeat operations, not a new pattern generator. Reject it if it sounds like a different pattern instead of the same motif rotating.

Cost boundary: reuse existing move choices and main-loop timing information; do not search in the audio interrupt. The push cap and shared-phase handling may need bounded state. Code/state cost is unmeasured; M7c and automatic M7d have no measured budget.

Verification when implemented: use patterns with eligible non-anchor hits and compare their shared-clock onset distribution with the baseline; a stable offbeat motif should retain its feel. An anchor-only four-on-the-floor pattern has no eligible hits for a move-only policy, so it must not be the claimed demonstration. Existing DENS additions are separate from these moves. Cover anchor/continuation/ratchet protection, push bounds, seeded repeatability, M1 spacing and unchanged behaviour on explicitly unsupported grids.

Acceptance: over a long WNDR pass the share of hits on off-beat 8ths and pushes rises, while the motif stays recognisable and the beats remain present. Listening: the groove leans forward and sits better against the TR-6S kick. Reject if it sounds as if the pattern has slipped out of time.

### M7d — unequal looping lengths (polymeter)

One part keeps a shorter repeating motif while the peer or TR-6S supplies the steady 4/4 reference. Both use the same clock. At straight 16th-note spacing, 12 steps against 16 meet again after 48 steps (three 4/4 bars); 15 against 16 meet after 240 steps (15 bars). This is distinct from off-beat emphasis and remains a requested outcome even if M7c is dropped.

Existing playback already uses each track's `P_SLEN`: ordinary `seq_tick()` wraps its index at that length, and ARP SEQ indexes the pattern using that length (`seq.c`). Begin with owner-set unequal lengths in the M0 fixture and trace actual onsets across the full joint cycle. This baseline does not establish automatic length behaviour or device acceptance.

Automatic shortening is a separate bounded prototype: decide allowed lengths, change/return boundaries and enablement before implementation. Preserve stored steps outside the shorter window, protected ties/slides/ratchets, manual takeover and KEEP/UNDO meanings. Do not reset the shorter loop at each shared bar. Reuse existing length controls for the baseline; a new control is not assumed.

Review M7 metric weighting against actual playback phase, division and swing: a local `i % 4` index is not always a beat in the shared bar. Preserve original motif anchors without forcing them onto global downbeats. M1 peer prediction follows each actual cycle; M8 turn-taking and the M9 arc follow shared musical bars.

**Built (2 November 2026): POLY.** TIDE SETTINGS row POLY: OFF / GENTLE (the default) / WIDE (`seq_poly.c`). In automatic MUSIC WNDR/JRNY (`sqm_auto`, set from `glide_poll`) one part's loop is shortened to len-1 or len-2 (GENTLE: 15 or 14 of 16) or len-1 to len-4 (WIDE: 15 to 12), for 8 (GENTLE) or 4 (WIDE) loops, then plays whole for at least 16 / 8 loops. The full loop is the reference: every change happens where it wraps (its first step), so the return re-locks the groove with no drift; inside a stretch the short loop restarts by itself, not at the shared bars. Chosen from the seed (`sqm[].tseed`, `morph.seed`) and the loop count, only at a phrase's first loop (a multiple of 4), never before the 8th loop after PLAY, the parts in turn and never both at once, a loop under 8 steps never. ARP SEQ is covered through `arp_idx`; the peer prediction (M1) returns no attack while a part is shortened.
KEEP / UNDO policy: the shortened length is **performance only**. `P_SLEN` and the stored steps are never written, so KEEP stores the steps at the stored length (the variation as it plays, as before) and UNDO has nothing to undo about the length. Not captured. STOP/PLAY starts every part whole; POLY OFF, the glide ending (a hand taking over), a recording or a changed stored length bring the part back at the next full loop. WAIT does not change it (the flag stays set while the glide holds).

Acceptance: the motif audibly rotates against the steady beat and reunites at the expected joint cycle, without clock drift, lost notes or forced bar resets. Compare 12/16 and 15/16 first without swing, then with swing and ARP SEQ; verify STOP/PLAY and manual length edits. Record software onset traces and owner listening separately. Automatic policy and resource cost remain pending and are excluded from the estimates below.

## M8 — question and answer between the two parts

### What it means musically

Call and response is a conversation: one part plays a short phrase that sounds open (a question), and the other part replies with a phrase that sounds closed (an answer). In a 4-bar phrase the usual shape is: bars 1–2 part 1 asks while part 2 stays in the background, bars 3–4 part 2 answers while part 1 steps back. Over the next phrase the roles can swap. What makes it audible:

- **Taking turns.** The asking part is fuller and more active for its two bars; the answering part waits with sparse hits or long held notes, then becomes the active one. If both parts are equally busy all the time there is no conversation, only two monologues.
- **Turnarounds that alternate.** The last bar of a phrase (the turnaround) is where a loop usually varies. If part 1 varies at bar 2 and part 2 at bar 4, the variations themselves become a question and its reply.
- **The answer echoes the question.** The reply reuses the question's rhythm (or a recognisable part of it), shifted into the space the question left, so the listener hears that the two belong together. An unrelated reply sounds like coincidence.
- **Open and closed endings.** A rhythmic question can end on a push or offbeat and an answer near a beat. Pitch-based tension/resolution depends on the sounding harmony; a selected scale's root or fifth alone does not establish a cadence. Begin with rhythmic relationships.

The two parts still share the groove: the beats stay, and they may play together on downbeats. Turn-taking is about who leads, not about silencing one part.

### What the code does today

- `phrase_peer_attack()` predicts the other part's attacks and the gap/repeat/direction choices prefer the space between them (M1). This is avoidance: the parts do not talk at the same moment. It does not decide who leads, and the parts do not respond to each other's material.
- `rhythm_poll()` alternates which part receives a development edit (`rhythm.turn`). These are pattern edits every two loops, not bar-by-bar turns in a phrase.
- VARY BARS 4 + 8 plays the variation on every fourth bar, but `sqm_bar_turn` is one shared offset (`seq_play_step()` in `seq_morph.c`), so both parts vary on the same bars 4 and 8, together.
- There is no notion of a 2- or 4-bar phrase, no open/closed ending, and no reuse of one part's rhythm by the other.

### What is needed

M8a, alternating turnarounds (smallest, try first): in automatic MUSIC WNDR/JRNY with both parts selected, try part 2 variation on bars 2/6 and part 1 on 4/8. Use shared musical bars even when loop lengths differ. VARY ALL, a single selected part, ONCE/LOOP and manual playback retain their behaviour. The rule is small but its linked cost and integration are unmeasured; a global change to `seq_play_step()` would affect more than this scope. Alternating variation bars are an experiment in interplay, not sufficient proof of call-and-response. **Built (session `tide-turns`):** TIDE SETTINGS row TURNS: TOGETHER / TAKE TURNS (default). In automatic MUSIC WNDR/JRNY with both parts selected, part 2's turnaround is offset by two shared bars (`seq_play_step`); the offset counts the shared `sqm_bar`, so it holds for any lengths and DIVs. VARY ALL, one selected part, ONCE/LOOP, manual playback, stopped and external clock are unchanged.

M8b, turn-taking within a phrase: from the shared bar counter, assign each 4-bar phrase a leading part for bars 1–2 and the other for bars 3–4 (swap the order every other phrase from the seed). In its following bars the non-leading part plays a thinned version of its current pattern: the existing gap rules applied as a per-bar playback mask (keep the beats and long notes; drop some optional hits), not as stored edits. The leading part may use its current variation. Only parts selected for TIDE take part; a single selected part plays as today.

M8c, rhythmic echo, only if M8b is accepted: when the leading role changes, try a recognizable part of the previous leader's last-bar rhythm on the new leader's eligible steps through existing move/repeat operations. Preserve each part's own pitches and register. Automatic pitch cadences are deferred: neither scale membership nor M2's within-part octave choices provides the required chord context. ARP SEQ's held-chord behaviour remains unchanged.

Cost boundary: all three slices add code and need individual linked measurements. For M8b, calculate follower masks in the main loop and publish bounded state; `seq_play_step()` in the audio interrupt should only do a cheap lookup/bit test. No extra full-pattern buffers. Earlier S0 savings do not establish capacity for M8c.

Verification when implemented: per-part VARY offsets with different pattern lengths/DIVs; existing STOP/PLAY bar-count behaviour; protected notes and live keys; single-part selection; WAIT; external clock; unchanged VARY ALL and manual modes. Define KEEP/UNDO capture semantics before adding masks: show whether thinning is performance-only or captured, and do not claim a saved result reproduces a playback mask it omits. Preserve sounding-note releases and effects tails rather than muting the post-effects output.

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
- **Character needs calibration on actual sounds.** Cutoff, resonance, drive, FM modulation/feedback, ratios and envelope shape are possible proxies, not universal harshness or pleasantness rules. A NOISE patch can be soft; a simple FM ratio can be bright. Preserve the owner's harshness/busyness link as a bounded musical bias, with independent density limits and M5 level checks.
- **The arc varies.** Not every buildup reaches a stop; lengths vary from the seed (as M4b proposes) so the cycle is not predictable.

### What the code does today

- `morph_complement.c` already measures four qualities from parameters (no audio analysis): brightness (LP cutoff), movement (filter LFO depth), space (reverb+delay+chorus) and envelope length (including FM6 carrier rates). JRNY's `sound_complement_pick()` uses them to prefer a sound that contrasts with the other part.
- WNDR walks brightness and other dials independently of the sequence.
- The phrase overlay has fixed density bounds: `phrase_repeat()` permits the original attack count plus `max(floor(original/4), 1)`; `phrase_gap()` stops at `ceil(original/2)` and never below one attack. Sparse patterns therefore do not have a strict 25% addition cap. Endpoint DENS edits can also affect the heard result; these overlay limits are not a complete audible-density model.
- The phrase tiers (16/32/64/128 bars) make larger changes more likely in longer phrases, but there is no shared tension and no stop or breakdown.

### What is needed

M9a, a calibrated character proxy: first check whether existing brightness/movement/envelope measures distinguish the intended factory/user scenes. Add resonance/drive or FM-specific measures only when they improve that ordering. Do not label an entire engine harsh or perform real-time spectral analysis. If the proxy is unreliable, use the arrangement phase to bias sound and rhythm directly instead of claiming to measure perceived harshness.

M9b, harshness drives busyness (the owner's link; can be tried before the full arc): set the phrase density bounds from the current harshness — a harsh sound may reach more added repeats and the occasional ratchet; a soft sound is held towards fewer, longer notes. This replaces the fixed bounds above and is a deliberate change to the rule "preserve existing density bounds", asked for by the owner.

M9c, one shared arc with M4: first try gradual development and recovery without automatic stops. The fuller owner-requested option is build → peak → optional stop → breakdown → next build, with one musical-bar phase value for both parts. The build may bias WNDR colour and permitted density; recovery eases them back without erasing the motif. A full stop is a phrase-aligned rest of selected TIDE sequences, with notes releasing naturally and tails ringing. Drop ownership/enablement must be settled before implementing that option. STOP of the transport freezes the arc, resume continues it, a new WNDR/JRNY session starts it fresh, and WAIT counts as musical time. Reuse M8 phrase timing rather than adding another scheduler.

The sudden stop is a deliberate exception to the "no endpoint jumps" acceptance of M4; that rule still applies to the sound glides themselves.

Limits: JRNY stays within the engine, with its existing category preference/fallback. A given palette may have no suitable soft breakdown sound; category/engine changes are not a free consequence of the arc. Longer gates cannot guarantee longer sustain from a decaying sound. Existing memory savings do not prove that M9c fits: require a fresh linked flash/RAM/hot-code budget and an affected real-device CPU comparison.

Relation to M4: keep one combined implementation brief, using M9's musical intent and M4's bounds/recovery. The remaining owner decisions concern phase behaviour and performance control, not whether to build two schedulers.

Tests: harshness ordering on known presets (e.g. a resonant acid sound above a soft pad); busyness tracks harshness in both directions; arc boundaries at different TIME/WAIT values and pattern lengths; stop aligned to phrase ends and never cutting a release; STOP/PLAY and resume; one or two selected parts; AMOUNT zero; KEEP during each phase; long-session bounds over at least 256 bars (no progressive thinning, no permanent maximum brightness or density).

Acceptance: a 10-minute session has audible buildups, peaks, clean stops and calmer breakdowns; harsh stretches are busier and soft stretches sparse; the motif carries across; the TR-6S stays in front during stops. Reject if the arc sounds scripted, if stops sound like dropouts, or if the background starts drawing more attention than the instruments it supports.

## M10 — smoother sound transitions

Status after owner clarification, 3 November: conditional only. Keep the current accepted sound/transition behaviour; investigate these proposals only for a demonstrated issue or an explicitly chosen transition change.

### What it means musically

A sound change in a background should feel like the same instrument slowly turning into another, not like a sequence of small edits. Three things make a change sound smooth to a listener:

- **Continuous motion.** A filter that opens over 16 bars should sweep, not climb a staircase. On a held pad or a dub chord with long release, every small jump in cutoff, envelope or send is audible as a tiny tick or a sudden change of colour, and dozens of them over a glide sound restless.
- **An order in which things change.** When a sound designer moves from one patch to another by hand, they rarely move everything at once. The brightness usually leads (it is what the ear follows), the envelope shape follows, and the space (reverb, delay, chorus) trails, so the old sound's tail is still in the room while the new colour arrives. Everything moving in lockstep on one curve sounds mechanical and makes the middle of the trip a muddy average of the two sounds.
- **Changes that land with the music.** Switches that cannot blend (arp mode, voice mode, a chord) are least noticeable when they coincide with a musical boundary, a new bar or phrase, rather than at an arbitrary moment in the middle of a note.

Loudness belongs here too: a sound that becomes brighter seems louder, one that closes or thins seems to drop away. M5 already covers measured level problems; M10 must not introduce new ones.

### What the code does today

- Structural switches are already hidden carefully (`morph_xfer.c`): FM6 algorithm changes cross a bridge of pure sines with equal-power fades; wave, filter type and model switches happen while the filter valley is closed; the insert fades to dry while its type changes; the sends swell so the tails carry the old sound. Keep all of this.
- Continuous values blend at the 128-position fader's weight (`morph_wb`), then `morph_apply_at()` rounds them back to whole parameter values. The part filter reads that cutoff into Q8 (`voice.c`, `ucut0`). This establishes quantization, not its audibility; measure slow glides before proposing a fix. Existing filter/transition processing also affects the heard result.
- Every continuous value follows the same S-curve (`glide_ease`) at the same time.
- Values that only shape the next notes (arp, voice and glide mode, chord) switch at the fader's middle (`morph_xfer.c` header).
- JRNY stays within one engine, so the transition never has to cross engines.

### What is needed

M10a, measure first: render slow glides on the host (1, 16 and 64 bars) between pairs with large cutoff, envelope and send differences, and record the effective cutoff, levels and send amounts every block. Count the steps and their size; listen for them on a sustained chord. If no audible stairs exist, record that and skip M10b.

M10b, only for a demonstrated audible staircase: preserve fractional resolution through the affected control path, including the blend, cache invalidation and DSP consumer. A finer fader weight alone is insufficient if the result is rounded back to a whole parameter step. Start with the affected cutoff/control coordinate, not every parameter. Keep the exact endpoints: fader 0 is End A and 127 is End B bit for bit, and a sound at rest is unchanged. Manual knob turns keep their current response.

M10c, staggered groups: inside the existing travel, give each group its own portion of the curve — brightness and filter lead (for example 0–70 % of the trip), envelope and shape in the middle (15–85 %), space and effects trail (30–100 %). Each group still starts at its End A and ends at its End B, so the arrival is identical; only the path differs. The structural valley/bridge timings stay where they are, and the groups must agree with them (the filter must be closed when a switch happens). Try the order and spans by listening; reject it if the middle of the trip sounds less coherent than today.

M10d, musical switch timing: move the switches that only shape the next notes from the exact middle to the first bar (or note-on) boundary inside the hidden stretch, so they arrive with the music. Fall back to the middle when there is no clock or no boundary in time.

Outside M10: cross-engine JRNY and automatic loudness matching. A continuous cross-engine blend needs a separately reviewed engine-lifetime/voice/CPU design; changing engines at silence is a different behaviour. Neither is implemented or costed by this smoothing proposal. M5 addresses demonstrated level problems independently.

Resource estimate: M10b a per-part fractional weight and a few bytes of cutoff state, plus per-block work in the audio ISR that must be measured against the hot code headroom; M10c a small table of group spans; M10d a few bytes of pending-switch state.

Tests: endpoints bit-exact at fader 0 and 127; at-rest sounds unchanged; monotone sweeps with no step larger than a set threshold on long glides; every structural switch still inside its hidden stretch with groups staggered; manual knob edits and pinned B values still work; STOP/PLAY pause and resume mid-trip; hand takeover mid-glide; external clock; CPU cost of the per-block interpolation.

Acceptance: a 16- and a 64-bar glide on a sustained chord sound like one continuous change with no ticks or stair-steps; the old sound's space audibly carries into the new colour; no new clicks, level dips or CPU problems on the unit. Reject a group order that makes the middle of the trip sound like neither sound.

### Controls: what the user decides, what stays in code

The owner (2 November 2026) wants a few choices back: how much change to expect, the energy, how random, and whether buildup and breakdown phases are allowed. These are **intent** choices: they set how TIDE behaves, not individual parameters. Everything else stays automatic.

The TIDE page's four columns (GLIDE, MODE, TIME, WAIT) are full, so the new choices go into the TIDE LIST, which has six rows today (TARGET, VARY BARS, ROLL, ARP, PITCH, WASH LEVEL). The TIDE settings record stores 7 of its 32 field bytes, so three more fit (TURNS and POLY use two: fields 7 and 8; the loader accepts the 7-field record and defaults them to TAKE TURNS / GENTLE); the loader must accept the older, shorter record (keep the stored fields, default the new ones) so an update does not reset the owner's settings.

User decisions (TIDE LIST rows, all automatic MUSIC WNDR/JRNY behaviour):

- **CHANGE — SLIGHT / MEDIUM / BOLD** (default MEDIUM = today). How much each trip changes: SEQ AMT (today fixed at 50 with no knob), how often phrase edits happen, and how many edits a trip may carry. The sound's AMOUNT on the MORPH page stays as it is; CHANGE scales the sequence and phrase side, AMOUNT the sound side.
- **ENERGY — DEEP / EVEN / DRIVE** (default EVEN = today). The character of the background: DEEP leans to sparse, on-beat-and-off-beat placement, darker and softer sounds; DRIVE to busier, more syncopated patterns (M7 pushes), brighter and harsher sounds (M9b). It biases the existing walks; it does not fix a sound. If "upbeat / downbeat" was meant as the feel (on-beat versus off-beat placement) rather than energy, this row is where that choice would live; to be confirmed with the owner.
- **ARC — OFF / BUILD / FULL** (exact default and drop policy open; OFF is the conservative starting recommendation). OFF preserves today's evolution; BUILD allows gradual development/recovery without stops; FULL adds the explicitly enabled stop/breakdown policy when implemented.
- **ROLL — MUSIC / SMART / WILD** selects generation policies. It is not a numeric randomness amount and does not replace CHANGE's amount/rate control. Retain the existing row; do not add a separate randomness control without a demonstrated musical distinction.

Shown, not chosen: an implemented arc should show its phase (for example "BUILD 12/24", "PEAK", "BREAK 3/8") on the existing status line so the owner can anticipate it. Include text/rendering cost in the measured candidate rather than declaring it free.

Code-only (no control; tuned by listening):

- M3 destination memory; M7 metric weights and push cap; M8 turnaround offsets, turn-taking, echo and open/closed endings; M9 harshness score, phase lengths and their variation, stop placement on phrase boundaries; M10 smoothing, group order and switch timing.
- These read the three choices above where it makes sense (CHANGE scales edit counts, ENERGY biases syncopation and harshness, ARC enables M9c), but expose no separate settings.

This changes the earlier rule "no new menu, save format, global mode" for these three rows only, at the owner's request. Deliver each row with the accepted behaviour it controls rather than reserving inactive UI. Each needs a listening check that choices are useful and distinct, compatible older-settings loading, and a defined backup boundary. The current settings backup omits TIDE's independent record; extra field bytes alone do not solve compatibility.

### Open decisions — parked 2 November, reviewed 3 November 2026

The owner parked these for a later decision. The earlier discussion included Claude's recommendations; the entries below distinguish those from this review's recommendations. None of the unresolved performance choices is approved by this documentation update.

1. **Who owns the drop.** Still open. Earlier recommendation: hold at PEAK, allow an owner gesture, then use an automatic timeout. Review recommendation: begin with gradual development/recovery; enable full stops only through an explicit ARC/performance policy. An owner-fired drop with an automatic timeout still permits an unsolicited stop, so that timeout must be a deliberate choice. No new gesture is selected here.
2. **ENERGY and ARC.** Recommended: keep both, as *average* (ENERGY) and *swing* (ARC): dub techno is low energy with a big swing, peak-time techno high energy with a small one. Merge them if listening shows their combinations do not sound clearly different.
3. **"Upbeat / downbeat".** Meaning remains open. ENERGY may gently bias syncopation if listening supports it, but metrical placement and average energy are distinct. Keep a separate feel control deferred without declaring its combinations musically wrong.
4. **CHANGE.** Recommended: per trip and still cumulative, with no pull home (the owner's earlier decision); SLIGHT = small steps that add up slowly. HOME remains the way back.
5. **Space.** The first lossless factory-sound packing/cleanup milestone is complete in source. Compare further lossless candidates against the chosen feature's measured budget. Keep effects, performance capture and visual assets until a concrete trade-off is justified; old S0 labels/estimates do not prove new capacity.
6. **Order.** Use the completed source audit's verified useful corrections first. U1 is a bounded 4–8-destination convenience, not a musical prerequisite. Existing-polymeter evaluation can proceed independently; M8a remains the first small musical feature candidate, then M7a/b, conditional M3/M8b and one later M4/M9 policy. M2 listening is optional when relevant; M5/M10 require a demonstrated issue or chosen change. Section numbering is not a queue.
7. **Row count.** Expose intent choices the owner needs while playing or preparing a session; leave implementation tuning in code. ROLL policy and CHANGE amount/rate have different roles even if one fixture sounds alike. M7d automatic polymeter enablement and lengths remain open; do not silently tie length changes to ENERGY or assume another row is approved.
8. **A launched buildup and drop** (owner idea, 2 November 2026): start a buildup by hand during play that runs over a set number of bars, drops and recovers, without a new screen. Placement candidates: OCT+ held + K1 on the TIDE page (beside OCT- + K1 = HOME), GLIDE A + GLIDE B pressed together on PERFORM, or a sixth MODE; undecided.

