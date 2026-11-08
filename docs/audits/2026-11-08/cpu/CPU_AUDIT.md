# CPU handling audit: DUELL / TIDE on the FM-1

Date: 8 November 2026. Author: Fable (main session). Scope: how the firmware predicts, measures and reacts to audio CPU load; whether the GLIDE fix of 6 November (fix/w16-glide-cpu, flashed as TIDE 1.0.0-b2 = fb0848c) is the right kind of fix; what the state of the art on this hardware looks like (Ravel_FM1 1.16 to 1.22, DUB-1 notes, Teensy/Daisy meters); what must change so this is never a surprise again.

Sources read: `audio.c`, `budget.c`, `budget.h`, `budget_feat.c`, `voice.c`, `fx.c` (ovl_step, mix_part), `twocore_run.c`, `morph.c`, `eng_fm6.c` on 817658d (b1) and fix/w16-glide-cpu d399a72; `work/CPU_FIX_HANDOFF.md`; Codex's `CPU_SKIPS_AND_MEASUREMENT_2026-11-06.md`; `work/measure/cpu-multis/*.jsonl`; `docs/TESTING.md` "CPU on the unit"; `docs/ROADMAP.md`; Ravel_FM1 release notes 1.18 to 1.22.

## 1. Verdict

The firmware has a good **open-loop** budget (a static cost table, calibrated on the unit on 26 October, solved per settings once a UI frame) and a precise **measurement** (TIMER4 per I2S half), but almost nothing connects the two at run time. The reactions to a measured overload are blunt: kill voices, or switch the delay events off. The GLIDE failure of 6 November was the predictable result: a **transient** cost (a morph rebuild and an FM6 crossing, both bursts) in a model that only knows **steady-state** per-sample costs, hidden by a meter that shows a one-second average (69 %) while single halves ran to 118 %.

The b2 fix is sound as containment and partly as a real cost cut (SRAM placement, no redundant ARP preparation on the bridge, blend every other block while a GLIDE runs). It is **not** the architecture fix: the burst still lands on core 0 inside the audio interrupt, while core 1 idles 60 to 90 us a block, and the budget still protects by lowering the note count whenever the fader moves. About 3 misses per FM6 transition remain (ROADMAP, owner accepted).

Grade: measurement B, static model B, run-time control D, two-core use D, process C. The sections below say what to do about each.

## 2. What was conceptually wrong

| # | Finding | Evidence | Consequence |
|---|---|---|---|
| C1 | **The cost model is steady state; the failures are transients.** budget.c prices work per sample per voice. A fader move rebuilds a morph (`morph_begin`, ~70 us a block on part 1 measured), an FM6 crossing runs both operator sets per voice (+35..45 % on the voice stage measured), reblends cost +4..23 % a block on the host. None of these existed in the table before 6 November. | handoff, glide_live_1: pk 118 %, cpu 69 %; budget.c `BD_TIDE_FM6 76` added 6 Nov | Every TIDE motion feature (GLIDE, WASH, VALLEY, ARC, TURNS) is a transient generator. Each one shipped was a new way to miss blocks that no test could see. |
| C2 | **The measurement feeds a display, not a controller.** `song.cpu_q8` is a 32-half IIR average; `cpu_pk` is read and cleared by the UI; `cpu_miss` counts. The only consumers that act are `shed_check` (two halves over 85 %: kill a voice; one half over 100 %: kill two released tails) and `ovl_step` (delay events off). Nothing lowers admission of new notes from measured load; the NOTES number comes from the model alone. | audio.c 164..172, fx.c ovl_step, voice.c trk_nvoice reads `bnotes[]` only | The unit knows its real cost every 2.9 ms and ignores it until it is too late to do anything but cut sound. |
| C3 | **The reaction ladder is upside down.** On measured overload the firmware kills voices first (released tails, then held notes), then lightens the delay. Ravel's rule is "nothing playing is cut off": ease quality in steps, keep the notes. We have cheap, inaudible easings that are never used: blend every 4th block instead of every 2nd, defer a crossing retune by a block, skip a WASH/VALLEY update, admit no new note for one half. | audio.c shed_voice; fx.c ovl_step; morph.c `rskip` | Shedding is audible (83 shed voices in the 300 s log). The owner heard thinning, which is why fb0848c had to narrow the charge to "while moving". |
| C4 | **The meter reads render time; the deadline is elapsed time.** `shed_check` returns `all - t5_nested_ticks`; the UI, the average and `cpu_pk` all use that. The DMA does not subtract the TIMER5 work (keys, UART, USB) nested in the ISR. The raw elapsed peak exists only in test-mode builds (`cpu_raw_pk`). | audio.c 172, 280..291; fix branch `DUELL_TEST_MODE` block | The number the owner sees is lower than the number that misses blocks. The 1-s average hides a 118 % half completely. |
| C5 | **Core 1 is a part-2 renderer, not a second core.** Core 0: part 1, gather, sends, buses (7 % STEREO / 15 % DUEL), master, bus B. Core 1: part 2 only, then waits. The probe shows core 1 idle 60 to 90 us of every 725 us block (8 to 12 %) while core 0 spikes. | twocore_run.c header; ROADMAP "Two-core balancing"; Ravel 1.18/1.19 | The chip has the headroom the GLIDE needs; it is on the wrong core. |
| C6 | **One flat reserve for scene-dependent error.** `BUDGET_LIMIT 72 + BUDGET_PEAK 18 = 90`. The 18 points are "the real per-second peak over the model", one number for every scene, taken from the worst factory multi on 5 November. Scenes with 64-bit stages (PHYS, SHIM, RANDOM delay) run further over the model than FM6/ANALOG scenes (TESTING.md says so). | budget.h; TESTING.md "The model and the unit" | Light scenes lose notes they could play; heavy transient scenes still miss. A flat margin cannot be both safe and generous. |
| C7 | **Hot code in flash.** On this chip code run from XIP flash waits on the flash. `morph_apply_at` (1,078 B), `fm6_xblock`, `fm6_xtune`, `fm6_xphase`, `morph_xstep` (1,896 B), `morph_wash`, `morph_valley` were in flash at b1 while their callers were in SRAM. Range Z has ~1.7 KB free above the 8 KB reserve. | Codex doc A0/A, retained ELF map; `ram_fns_z.py` | Each new TIDE stage lands in flash by default and costs more than its instruction count says. The fix moved `morph_apply_at` and the FM6 bridge into RAM (6b6678f); nothing enforces it for the next stage. |
| C8 | **Buses run when silent.** 21 % of M16's cost at one key was the sends: "the buses run whether fed or not". A silent part already writes nothing (INSERT and EQ rest), so the gap is the buses and the master. R08 (exact-zero tails) was merged on 4 November as the prerequisite; the skip itself is not done. | TESTING.md "Where 0.2.23's M16 cost went"; fx.c 786, 1277, 1400; audit R08 | ~6 % idle cost (audit figure) that TIDE's quiet passages pay for nothing. Ravel 1.19 rests the BED and its SPACE after 4 s of silence. |
| C9 | **Housekeeping.** The unit runs fb0848c; that commit is in neither `integ` nor `jam11`. Two test-expectation commits (069c65e, d399a72) and five ROADMAP commits sit on fix/w16-glide-cpu after it. The handoff's last step ("merge into integ, push the fork") did not happen. | `git merge-base` today | The flashed build is unreproducible from the public branches; the next agent branches from b1 and reintroduces the bug. |

## 3. Was the b2 fix the best available?

What it did, by commit: probe and per-stage timing (76c1f31, b129c68, 60d0f9a: test-mode only, good); charge End B and the wash, 32-bit wash (d13f5ab: containment); SRAM for `morph_apply_at`, cached WASH coefficients, exact no-ARP bridge path (9178086: real cost cut, bit-exact); blend every other block while moving, FM6 bridge into RAM, charge the crossing (6b6678f: half amortisation, half containment); charge only while the fader moves, ~1 s hold (fb0848c: narrower containment after the owner heard thinning).

Judgement:
- **Right:** measured per stage on the unit before changing anything (the first time this project did that for a burst); kept the one-writer handoff between Claude and Codex; bit-exact rewrites verified by the host suite; no sound change beyond the half-rate blend.
- **Good enough for b2, not for 1.0:** the crossing charge of 76 (half an FM6 voice per voice) is a measured number but a steady-state way of paying a burst: the notes number drops for the whole glide plus a second. The half-rate blend is the right idea (amortise) applied once; the FM6 crossing itself still runs at full rate on core 0.
- **Missing:** Codex's C and D (compute shared FM6 modulation once per part per block; stop rebuilding unchanged patch data on every fader step) were "measure before undertaking" and were not undertaken. D in its larger form (prepare the next-fader snapshot outside the render, publish atomically) is the real fix for C1 and is a design change the handoff explicitly deferred.
- **Alternative not taken:** run the morph rebuild and the FM6 crossing preparation on core 1 while it waits (C5). That removes the burst from core 0 without lowering any cap.

So: the best available **within the 6 November rules** (no lowered reading by shedding, no sound change, flash soon), and correctly logged as such. Not the end state.

## 4. State of the art on this hardware and in small-DSP practice

| Practice | Who | Where DUELL stands |
|---|---|---|
| Measure each stage and each core with the cycle timer; keep peak and average apart; one owner of the peak window | Teensy AudioProcessorUsage, Daisy CpuLoadMeter, Ravel bench builds (1.22.1: "time each part of a step on both cores"), DUB-1 notes | Per-stage timing exists in test mode only; the peak is cleared by three readers; the display shows the average. |
| Feedback control on measured load with an ease ladder and hysteresis; never cut what is playing | Ravel 1.17 load guard; 1.20 BUFFER SAFE ("reads two blocks at a time, allows 90 % instead of 80 %, fades far fewer voices") | We have shed (kills voices) and LIGHT FX (delay events). No admission control from measurement, no quality ladder. |
| Assign work to cores by recent measured cost; whole tracks with their effects as jobs; buses shared | Ravel 1.18 ("tracks assigned by what their voices and effects cost, then single voices"), 1.19 ("buses shared between both cores") | Fixed split: part 2 on core 1, everything else on core 0. ROADMAP row exists, not started. |
| Nothing nobody hears runs; rest after silence | Ravel: "a block that nothing heard depends on doesn't run"; BED rests after 4 s | Parts rest (INSERT, EQ). Buses and master always run. |
| Whole voice loop and transition code in RAM; build check | Ravel ("the whole voice loop and its bigger blocks run from RAM"), DUB-1 A/B tests | `ram_fns_z.py` groups exist; the TIDE additions were not in them until 6 Nov; no check that a per-block function is not in XIP. |
| Elastic buffering: let a busy block borrow from the next | Ravel 1.20 BUFFER SAFE: "a busy block may take up to twice its time without a dropout" (larger DMA buffer, more latency) | HALF_FRAMES 128 = 2.9 ms, fixed. A keyboard wants low latency; TIDE's background does not care. A TIDE-only larger half is a legitimate option nobody has costed. |
| Emulator with measured instruction timing for pre-flash screening | Ravel 1.21.1 ("fm1-emu … the FM-1's measured timing") | Our host model is instruction counts on a Mac with one calibration; the fm1-emulator uses 8 ns per instruction as policy. Screening only; the unit stays the judge (Codex's recommendation, agreed). |

## 5. Recommended design

Ordered by payoff per byte and per risk. A to D are the architecture; E to G are process.

**A. Close the loop: an ease ladder driven by the raw elapsed peak (C2, C3, C4, C6).**
- Measure the raw elapsed half (before the TIMER5 subtraction) in every build, not only test mode. Keep the render figure for the display's history, but drive control from elapsed.
- One controller where `shed_check` is, with hysteresis, stepping an ease level 0..5 on "two halves over 85 % of elapsed" and stepping back after ~3 s under 70 %:
  1. transition rate: morph blend every 4th block, FM6 crossing retune every other block, WASH/VALLEY update skipped every other block;
  2. admission: `bnotes` minus one for new notes (no sounding voice touched);
  3. LIGHT FX (today's ovl_lite);
  4. released tails (today's SHED_FAST);
  5. held notes (today's shed_voice), only when a half actually missed.
- The flat BUDGET_PEAK 18 can then come down (to the measured residual of the calibration scenes, ~6) because the ladder covers what the model does not. That is the headroom the owner is asking for, without a single killed note.
- Size: ~300 B flash, a few bytes RAM. Testable on the host with the `glidecpu` group by injecting elapsed times.

**B. Take the bursts out of the audio interrupt (C1).**
- Morph: prepare the next fader position's blended parameters in the main loop (or on core 1 while it waits), as a snapshot with a generation number; the render reads the latest complete snapshot. The ISR then swaps a pointer instead of rebuilding 1 KB. Codex's D in its larger form; it needs ownership rules for live edits (one writer, generation check, fall back to the current snapshot when the next is not ready).
- FM6 crossing: precompute the stand-in operator tables per part per block (Codex C) once, not per voice; retune every other block while ease level 1 is engaged.
- Until B lands, A's level 1 is the stand-in.

**C. Two-core balancing (C5): the ROADMAP row, made concrete.**
- Step 1 (small): give core 1 the buses (sends, chorus, delay, reverb) as a second job after part 2 when its measured recent run time plus the bus cost fits the deadline; the runner's deadline and fallback stay. Core 0 keeps gather and master. Saves 7 to 15 % on core 0 in exactly the scenes that burst.
- Step 2: assign the dearer part (by `bd` cost or measured run time; two buckets of 128 blocks exist already) to core 1 instead of always part 2.
- Step 3: the morph preparation of B on core 1's idle time.
- Each step keeps the one-core fallback bit-exact (the runner already does).

**D. Silence (C8).** After R08, skip a bus whose input and state are at exact zero; skip the master's limiter and EQ when the mix is at rest. Rest after ~1 s of zero, like Ravel's BED. ~6 % idle, more in TIDE's valleys.

**E. Measurement truth (C4; Codex, "What the readings mean").**
- Raw elapsed peak and MISSED on the main screens (ROADMAP row of 6 November), read from one snapshot that nobody clears but its owner (a sequence number and the window length).
- `cpu_miss` to count every DMA period missed, not the two-state half selector.
- `cpu_multis.py` and `glide_watch.py` to read `cpu_budget_us` from the probe and stop substituting zero for missing counters.

**F. Placement policy (C7).** `ram_fns_z.py` gets a rule and a check: every function reachable from `audio_block` per block or per sample must be in range Z, or named in an allow-list with a reason. `build.py check()` fails on a new per-block function in XIP. The 8 KB reserve stays.

**G. Process: how a feature proves its CPU cost before it is flashed.**
1. Every change that adds per-sample or per-block work states its budget charge in `budget.c` (steady) **and** its transient profile (what happens on a fader move, a load, a crossing, a mode change).
2. It is measured on the unit with the test-mode stage probe on a scene that provably activates the path (`tide_measure.py` preflight: voices sound, stage active), transitions included, both directions, raw elapsed peak recorded. Acceptance: 0 missed over the 16-multi glide sweep and no new shed or LIGHT FX engagement on the formerly failing scene (Codex's acceptance, adopted).
3. The host `glidecpu` group and `cpu_budget.sh` basic blocks gate regressions before the unit (screening, not acceptance).
4. The CHANGELOG line of a flashed build names the unit figures (as 243f3cd did).
5. The flashed commit is merged to `integ` and pushed to the fork the same day (C9).

## 6. Backlog, ordered

| Id | Pri | Task | Size | Design |
|---|---|---|---|---|
| K1 | P1 | Merge fix/w16-glide-cpu (fb0848c plus the test fixes to d399a72) into integ and jam11, push; tag tide-1.0.0-b2 | S | C9 |
| K2 | P1 | Raw elapsed peak measured in every build; MISSED counts every DMA period; one owner of the peak window; CPU and skips on the main screens | S | E |
| K3 | P1 | The ease ladder (levels 1..5) with hysteresis in place of the two shed/ovl rules; host test with injected elapsed times | M | A |
| K4 | P2 | Buses as a second core-1 job when its measured run time fits (two-core step 1) | M | C |
| K5 | P2 | Silent bus and master skip after R08; rest after ~1 s of exact zero | S | D |
| K6 | P2 | Placement check: per-block functions must be in range Z or allow-listed; the build fails otherwise | S | F |
| K7 | P2 | FM6 crossing: shared modulation once per part per block; retune every other block under ease level 1 | M | B |
| K8 | P3 | Morph snapshot prepared off the ISR (main loop or core 1), published by generation; the ISR swaps a pointer | L | B |
| K9 | P3 | BUDGET_PEAK 18 down to the measured residual once K3 is on the unit; re-run COST-CAL with transitions | S | A |
| K10 | P3 | Dearer part to core 1 by measured cost (two-core step 2) | M | C |
| K11 | P3 | TIDE-only larger half (256 frames, 5.8 ms) as a costed option for background use | S | section 4 |
| K12 | P3 | Process G written into CONTRIBUTING.md and TESTING.md: budget charge, transient profile, unit probe before flash | S | G |

## 7. Owner decisions

1. Ease ladder (K3): is level 2, "no new notes for a moment", acceptable in a live set, given that the alternative today is a killed note?
2. Two-core (K4): Felucca's rule was "core 1 untested, off by default"; DUELL runs two cores by default since 1.1. Confirm that core 1 may take the buses.
3. Latency (K11): is a 5.8 ms half acceptable when TIDE plays alone (background mode), with 2.9 ms kept for keyboard play?
4. K9 lowers the reserve only after K3 is measured on the unit. Agree that no reserve is cut before a feedback controller exists.
