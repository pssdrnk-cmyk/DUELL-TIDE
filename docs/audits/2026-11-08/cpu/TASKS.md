# CPU audit tasks K1 to K12: briefs for the agent that does them

Companion to `CPU_AUDIT.md` (same folder). Each brief is complete on its own: an agent that has read only this section and the files it names can do the task. The ROADMAP rows link here by anchor (`docs/audits/2026-11-08/cpu/TASKS.md#k3`).

## Rules for every task

- **Tree:** the firmware is `felucca-base/` inside the DUELL checkout. Start from the branch the ROADMAP names as current (today `fix/w16-glide-cpu` c180282 in worktree `work/wt/fix-w16-glide-cpu`; after K1, `integ`). One branch per task, named `fix/cpu-K<n>-<slug>`.
- **Sound:** unchanged unless the brief says so. A rewrite of an existing path must be bit-exact; prove it with the existing host groups.
- **Never lower a CPU reading by shedding notes, shortening releases or engaging LIGHT FX.** That is the owner's rule from the 6 November fix.
- **One writer per file.** Before editing a firmware file, append a claim line to `work/CPU_FIX_HANDOFF.md` LOG (`YYYY-MM-DD HH:MM who: claims <files> for K<n>`); check the LOG for another agent's claim first.
- **Build and test:** host suite from the worktree root: WSL `cd felucca-base && python3 tools/build.py --gen-only`, then `sh host_tests.sh [groups]`. Device images in Git Bash: `. jam/env.sh; cd felucca-base; python tools/build.py` (normal) and the same with `DUELL_TEST_MODE=1 FELUCCA_UAC=0` (test mode). App slot 581,564 B; `tools/build.py check()` enforces the 8,192 B range-Z/pool tail. `sh jam/check_off.sh` must stay PASS where the brief says "off byte-identical".
- **Report:** sizes of both images, the host groups run and their result, what the unit still has to confirm (the agent never flashes, never touches COM ports, never pushes, never tags). Commit with the configured identity, never `-c user.*`. CHANGELOG-TIDE entry under the next beta heading when the user can notice the change.
- **Paths with the user's home name and e-mail addresses stay out of committed files.**
- **Unit measurement** (owner's session only): `python work/measure/cpu-multis/cpu_multis.py 2 --glide --gtime 0 --hold 20 --com COM21` against the test-mode image; acceptance for anything that touches the audio path is 0 missed and 0 shed over the 16-multi glide sweep, raw peak recorded.

---

## K1. Merge the flashed b2 into integ and jam11 <a id="k1"></a>

**Priority** P1. **Size** S. **Why:** the unit runs fb0848c (TIDE 1.0.0-b2); it is in neither `integ` (817658d) nor `jam11` (81a7665). An agent branching from either reintroduces the GLIDE overrun.

**Steps**
1. In `work/wt/integ`: `git merge --no-ff fix/w16-glide-cpu` (c180282 or later). Conflicts are expected only in `docs/ROADMAP.md` and `CHANGELOG-TIDE.md`: keep both sides' rows.
2. Run the full host suite on the merge; expected 171 passed, 0 failed, 3 skipped as on 9178086 (`work/verification/cpu-fix-9178086/host-check.out` is the reference). The four groups that had stale expectations (budget A01, deadline TIDE, partindep, wave) were fixed in 069c65e and d399a72; they must pass.
3. `sh jam/check_off.sh`: the plain-DUELL image must be byte-identical to `off.sha256`. **Known failure:** the round-2 audit found the drones commit 36f03cd changed the plain image without re-pinning. Decide with the owner: re-pin (if the change is the drones' bank data only, confirm by diffing the two images' bank sections) or exclude the drones from the plain build. Do not re-pin silently; write the reason in the commit message.
4. Fast-forward `jam11` to the same commit (the fork's public branch), push `integ` and `jam11` to `pssdrnk-cmyk/DUELL-TIDE` **only when the owner says push** (the agent reports ready-to-push).
5. Tag `tide-1.0.0-b2` on fb0848c's merge (annotated, message: the unit figures from commit 243f3cd of the workspace repo).
6. ROADMAP: tick the "Merge the flashed b2" row; `docs/TESTING.md` and `CHANGELOG-TIDE.md`: "not flashed yet" lines corrected (lane D of the round-2 audit lists them).

**Acceptance:** `git merge-base --is-ancestor fb0848c integ` and the same for `jam11` return true; host suite green; check_off PASS with a documented pin.

---

## K2. CPU truth in every build <a id="k2"></a>

**Priority** P1. **Size** S. **Files:** `firmware/src/audio.c`, `firmware/src/core.h`, `firmware/src/ui_menu.c` (cpu_meter_open / cpu_meter_poll, lines ~30-60), `firmware/src/console.c` (`cpu` command, lines ~380-390), `firmware/src/bench.c` (lines 912, 925, 1441, 1490 read `cpu_pk`), `work/measure/cpu-multis/cpu_multis.py`, `glide_watch.py`.

**Problem.** The ISR publishes the *render* time (`shed_check` returns `all - t5_nested_ticks`) to `cpu_sum`, `cpu_pk`, `song.cpu_q8`. The DMA deadline is the *elapsed* time including the TIMER5 work nested in the ISR. The raw elapsed peak exists only under `DUELL_TEST_MODE` (`cpu_raw_pk`). Three readers clear `cpu_pk` (ui_menu.c 37/54, console.c 386, bench.c 912/1441), so a peak can be consumed by one and lost to the others. `cpu_miss` increments once per half from `(us > CPU_BUDGET_US) | late`, where `late` is a two-state half-selector compare that returns to the same state after an even number of missed periods.

**Steps**
1. `audio.c`: keep `us` (render) for `cpu_sum`/`song.cpu_q8` (the display's history and `ovl_step`'s calm rule stay comparable with the past). Add `cpu_el_pk` (elapsed us, every build): `raw = (fm1_ticks() - t0) / FM1_TICKS_PER_US` before the subtraction. Remove the `DUELL_TEST_MODE` guard around `cpu_raw_pk`, rename to `cpu_el_pk`, update console.c.
2. Snapshot with one owner: a small struct `cpu_snap { uint32_t seq, sum, n, pk, el_pk, miss, halves; }` filled by **one** place, `cpu_meter_poll` in ui_menu.c, once per UI frame: it reads and clears the ISR counters under `fm1_irq_off()` (as console.c 383 does) and bumps `seq`. Console `cpu`, ABOUT, TEST and bench read the snapshot only and never clear anything. Bench builds that need their own window (bench.c 912/1441) take a `cpu_snap` copy and difference against it rather than clearing.
3. Misses: count DMA periods, not halves. The HAL has `fm1_audio_free_half()`; add to `hal/fm1_audio.h` a read of the DMA's period counter if the ALNK0 block exposes one (look at how `fm1_audio_pending()` reads its status register in `hal/fm1_audio.h`); if there is none, keep the selector compare but also count `us > 2 * CPU_BUDGET_US` as 2 misses, etc. (`miss += us / CPU_BUDGET_US` when `us > CPU_BUDGET_US`). Document which the chip allowed.
4. Tools: `cpu_multis.py` and `glide_watch.py` read `cpu_budget_us` from the probe (the console prints it as `cpu_budget_us`) instead of the fixed 2902; a missing counter is an error, not 0; print `el_pk` beside `pk`. They import from an absolute `probe-testmode` path: make the path an argument with that default.

**Tests:** `tests/ovl_test.c` and `tests/glide_cpu_test.c` still pass bit-exact (`ovl`, `glidecpu` groups); a new assertion in `ui_test.c` or a tiny `cpusnap_test.c`: two readers polling the snapshot see the same `pk` for the same `seq`, and a peak published between a read and the owner's clear is not lost (simulate with the host ISR hook `AUDIO_DUCK_IDLE` or by calling the owner's poll from the test). Host groups `ui`, `console`, `bench*` green.

**Acceptance:** the console's `cpu` and ABOUT show `el_pk` in a normal build; `cpu_pk` is written by the ISR and cleared by exactly one function; sizes reported; off byte-identical (this code is in both builds, so the plain image changes: re-pin with the reason "K2 meter").

---

## K3. The ease ladder <a id="k3"></a>

**Priority** P1. **Size** M. **Depends on** K2 (drives from `el` not `us`). **Files:** `firmware/src/audio.c` (shed_check 164-172, shed_released, shed_voice, the ISR 251-257), `firmware/src/fx.c` (ovl_step ~626-650, `OVL_LITE` uses), `firmware/src/morph.c` (~1642 `rskip`), `firmware/src/fm6_xfer.c` (`fm6_xblock` 94/149, `fm6_xtune` 61), `firmware/src/morph_xfer.c` (`morph_wash` 668, `morph_valley` 776), `firmware/src/voice.c` (`trk_nvoice` 141), `firmware/src/budget.c` (nothing changes; `bnotes[]` stays the model's number).

**Problem.** Two reactive rules exist: two halves in a row over 85 % set `SHED_HELD` (kill the quietest released voice, else the youngest held one), one half over 100 % sets `SHED_FAST` (kill two released tails); the same 85 % rule engages `ovl_lite` (delay RANDOM/OCT events off). Nothing cheaper is tried first, and new notes are still admitted while shedding.

**Design.** One controller `ease_step(uint32_t el_ticks)` called where `shed_check` is, owning a level `ease` 0..5 and the existing `shed_over` history:
- **Engage:** two halves in a row over `SHED_TICKS` (85 % of elapsed): `ease` up by one, at most once per half. A half over `SHED_FULL_TICKS` (a real miss): `ease` jumps to at least 4.
- **Release:** `song.cpu_q8 < OVL_CALM_Q8` (70 %) for `OVL_CALM_HALVES` (~3 s) in a row: `ease` down by one, the calm counter restarts.
- **Levels** (each includes the ones below it):
  1. transition rate: `morph.c` blend every 4th block (the `rskip` toggle becomes a 2-bit counter: skip 3, blend 1) while `morph_gliding` **or** the fader moves; `fm6_xblock`'s per-voice `fm6_xtune` only on even blocks (keep the last tuning on odd ones: it already holds state in `fm6_xt[]`); `morph_wash`/`morph_valley` coefficient updates (not the per-sample filter) every other block.
  2. admission: `trk_nvoice` returns `b - 1` (never below 1) while `ease >= 2`. No sounding voice is touched.
  3. LIGHT FX: `ovl_lite = 1` (today's `ovl_step` engaged state).
  4. released tails: today's `shed_released(SHED_FAST_N)` once on entering the level, then once per half while it lasts.
  5. held notes: today's `shed_voice`, only while `ease == 5` and only after a half that actually missed (`el > SHED_FULL_TICKS`).
- `ovl_step` is folded in: `ovl_lite = ease >= 3`. `ovl_count` counts entries into level 3, so ABOUT's LIGHT FX figure keeps its meaning. Console `cpu` prints `ease` and a per-level entry counter.
- All of it in flash code except the per-half `ease_step` (a few compares) and the reads of `ease` in the render (one byte).

**Tests:** new host group `ease` (`tests/ease_test.c`, register in `host_tests.sh` and the CI shard list): feed `ease_step` a sequence of elapsed ticks (e.g. 30 halves at 60 %, 2 at 90 %, 10 at 60 %, 1 at 110 %, 2000 at 50 %) and assert the level sequence, that levels 1-3 call no `voice_kill` (count via `shed_count`), that level 5 needs a miss, and the release timing. `ovl_test.c`'s bit-exact reference (`-DOVL_NOGUARD`) must still hold with the guard compiled out. `glidecpu`, `glide`, `tidemotif`, `twocore` groups green. Render tests (`check.sh` sound groups) bit-exact at `ease 0`.

**Acceptance:** suite green; both images built; the owner's unit sweep: 0 missed on the 16-multi glide sweep **and** `shed` delta 0, and on the formerly failing BIG STAB glide the console shows `ease` reaching 1 or 2 and no higher. Then K9 may lower `BUDGET_PEAK`.

**Owner question before starting:** level 2 ("no new note for a moment") acceptable live? If no, skip level 2 and renumber.

**Answered (owner, 2026-11-08):** yes, acceptable live; all five levels kept. As built (1.0.0-b4) level 2 is gentler than "no new note": a new POLY note takes over the part's oldest released voice instead of a free one (the old tail stops when the new note starts); with none released it takes a free voice, so no note is dropped and no held note is cut. The release rule was changed in review: one level down after ~1 s with no half over 80 % (the 70 %-average rule never let go of a scene at 70 .. 85 %).

---

## K4. Buses as a second core-1 job <a id="k4"></a>

**Priority** P2. **Size** M. **Files:** `firmware/src/twocore_run.c` (the runner: `tcr_begin`, `tcr_part2`, the deadline table `c1f..tg`, the floor buckets), `firmware/src/twocore.c` (the mailbox words: a second job kind needs `arg` to carry it), `firmware/src/fx.c` (`mix_block`: part 1, `tcr_part2`, `mix_gather`, `fx_buses` 709, master), `docs/TWOCORE_STAGE1.md`, `tests/twocore_sim.c`, `tests/deadline_test.c`.

**Problem.** Core 0 does part 1, gather, sends, buses (7 % of a half STEREO, 15 % DUEL), master, and bus B in DUEL. Core 1 does part 2 and then waits 60-90 us per 725 us block (the stage probe `cpu` st_* figures). The GLIDE bursts land on core 0.

**Design (stage 2 of the two-core plan).** After core 1 reports `done` for part 2, core 0 posts a second job "buses of this block" when (a) the mix is not SPLIT/DUEL with bus B (stage 1 keeps bus B on core 0), (b) core 1's recent maximum run time (the two 128-block buckets already kept for the floor) plus the bus cost (`budget.c` bus rows via `global_cost_dl`) fits before the block's `lim`. Core 1 runs `fx_buses` on the gathered sends into its own wet buffer; core 0 does gather and master, waits for the bus job's `done` with the same take-back rules as part 2 (yld: core 0 runs the buses itself; not done by `lim`: buses silent this block, a pause). Inputs to the bus job are the sends after gather, so the job is posted after gather, and the master waits for it: measure that the wait does not exceed what was saved (the probe gives both).

**Steps:** 1 extend the job encoding (a job kind bit in `arg`, the hash covers it); 2 `tc_job_run` dispatches by kind; 3 the runner's second post/wait with its own counters (`tcs.bus_in_time`, `bus_late`, `bus_silent`, `bus_own`); 4 the budget's two-core rule in `budget_solve2` moves the bus cost from "both paths" to core 1's path when the bus job is enabled (gate it with the same `bd_two_now()`); 5 `twocore_sim.c` grows a bus job with the same invariants (no overlap of writers, deterministic output equal to the one-core render); 6 console `tc` prints the new counters.

**Tests:** `twocore` (sim, invariant-only in CI), `deadline`, `budget` groups; a render comparison: two-core sim output == one-core output for 16 factory multis (the existing twocore check does this for part 2; extend it).

**Acceptance:** sim green; the unit's stage probe shows core 0's bus stage at ~0 in STEREO and the GLIDE sweep misses not worse; `bus_silent` 0 over the sweep. Owner confirms core 1 may take the buses before the task starts.

---

## K5. Silent bus and master skip <a id="k5"></a>

**Priority** P2. **Size** S. **Depends on** R08 (exact-zero tails, merged 4 Nov: `4714ee9`). **Files:** `firmware/src/fx.c` (`fx_buses` 709, the per-bus state structs `fx`/`fx_b`, `pacc_on[]` 1410 which already records which sends a part wrote, the master stage after gather), `firmware/src/fx_bus.c`.

**Design.** Per bus (chorus, delay, reverb) and per side (A/B): a `rest` counter. When no part wrote that send this block (`pacc_on` bits 1..3 are 0 for every live part) **and** the bus's state is at exact zero (the delay line's write head has written zeros for one full line length; the reverb combs and allpasses report zero: add an `is_zero` per structure that is cheap: a flag set by the stage when its output block was all zero and cleared on any nonzero input), the bus is skipped and its wet buffer is left at zero. Rest engages after ~1 s (44 blocks... use `FS / CTL` blocks) of zero so a fading tail is never cut. Same for the master's limiter and EQ: skipped when the mix buffer is all zero and the limiter's gain has recovered to unity. Nothing is stored; the skip is invisible except in CPU.

**Tests:** `tests/hall_tail_test.c`, `dly_*`, `ovl_test.c`, the reverb/delay render groups bit-exact (a skipped stage must produce exactly the zeros it would have produced: that is what R08 guarantees). New assertion: after 12 s of silence the bus stage is not called (count calls in the test build). The bench row `bus/STEREO/nothing_fed` should fall to ~0 on the host; report the before/after.

**Acceptance:** render groups bit-exact; host bench idle figure down by the bus cost; sizes; off byte-identical (plain DUELL also gains: re-pin with the reason).

---

## K6. Hot-code placement check <a id="k6"></a>

**Priority** P2. **Size** S. **Files:** `tools/ram_fns_z.py` (GROUPS dict, line 27; `morph_apply_at` appended at 41), `tools/ram_fns_x.py`, `tools/ram_place.py`, `tools/build.py` (`check()` at ~507; it already has the disassembly `dis` and symbols `syms`).

**Design.** A new check in `build.py check()`: walk the call graph from `audio_block` (and `tc_job_run` for core 1) in the disassembly, depth-first, following direct calls only; every function reached that is **not** in range X or Z and **not** in `tools/ram_allow.py` (a new list: `{"name": "reason"}`) fails the check with the list of offenders and their sizes. Seed the allow-list with what is in flash today after inspecting it: anything called only on a note-on, a load or a mode change (e.g. `voice_alloc`'s patch setup, `morph_begin`'s rebuild if it stays in flash on purpose), with a one-line reason each. Everything per sample or per block must be in RAM or allow-listed with a reason that says "per block, X us measured, accepted".

**Steps:** 1 the graph walk (the disassembly format is pi32v2 objdump; `build.py` already parses call targets for its reach checks: reuse that parser); 2 the allow-list; 3 run on the current image, list offenders, place or allow each with the owner's agent; 4 a `--placement` report mode that prints the per-block set with sizes and where each lives, for the ROADMAP's memory table.

**Tests:** `tools/build.py check()` on both images passes; a deliberate test (temporarily remove `fm6_xblock` from the Z group) fails with the right name.

**Acceptance:** the check is on in `build.py` for every image; the allow-list has a reason per entry; the report lists the range-Z free space above the 8,192 B reserve.

---

## K7. FM6 crossing: shared modulation once per part per block <a id="k7"></a>

**Priority** P2. **Size** M. **Files:** `firmware/src/fm6_core.c` (`fm6_note_compute` ~566: recomputes `pmd`, `senslfo`, pitch-LFO, `amd`, `amod`, the carrier mask per voice), `firmware/src/eng_fm6.c` (`fm6_render` ~641, the bridge call), `firmware/src/fm6_xfer.c` (`fm6_xblock`, `fm6_xtune`).

**Design (Codex's item C, 6 Nov).** The values that depend only on the effective patch, the algorithm and the part's LFO value and delay are computed once per part per block into a small struct and passed to `fm6_note_compute`; the per-voice part (`fm6_penv_get`, envelopes, frequency, live level, velocity and key scaling) stays per voice. **Do not hoist the whole `pitch_mod`**: it has per-note inputs. Under K3's ease level 1, `fm6_xblock` calls `fm6_xtune` on even blocks only.

**Tests:** `fm6*` groups (`fm6_live_ol_test.c`, `dx7compat` when banks are present, the FM6 render fixtures) bit-exact; `glidecpu`. Host bench `voice/FM6` row before/after (expect a few % down per voice). No sound change.

**Acceptance:** bit-exact renders; bench row down; sizes; the unit's stage probe on a BIG STAB glide shows the voice stage's +35..45 % during the crossing reduced (report the number).

---

## K8. Morph snapshot prepared off the ISR <a id="k8"></a>

**Priority** P3. **Size** L. **Depends on** K3 (level 1 is the stand-in until this lands) and K4's job kind (if core 1 does the preparation). **Files:** `firmware/src/morph.c` (`morph_begin`, `morph_apply_at`, the blend ~1600-1660), `firmware/src/morph_glide.c` (`glide_poll`), `firmware/src/budget.c` (the `bd_mv` charge can then shrink), `firmware/src/twocore_run.c` (optional: the preparation as a core-1 job kind).

**Design (Codex's item D, larger form).** Today every fader step inside the render rebuilds the blended parameter set (`morph_apply_at`, 1 KB of code, ~70 us a block on part 1). Instead: the main loop (or core 1 while idle) prepares the blend for the **next** fader position into a double-buffered snapshot with a generation number; the render checks "is a snapshot for my fader value ready?" and swaps a pointer, else keeps the current one for this block (the fader is at most one block late, as the every-other-block blend already is). Ownership: one writer (the preparer), one reader (the render); the generation is written last; a live edit of either end invalidates both snapshots and forces a synchronous rebuild once (today's path), then the preparer takes over again. The FM6 bridge's `fm6_xt[]` tables follow the same scheme.

**Steps:** 1 define `mph_snap_t` and the two buffers per shown part (RAM: measure, the budget is tight: the blend's output already exists once; the second copy is the cost); 2 the preparer in the main loop at UI-frame rate for a moving fader (fader steps arrive at the knob's rate; a GLIDE's steps are known a block ahead from `gl`); 3 the render's swap; 4 the fallback; 5 if core 1: a job kind "prepare snapshot" posted when core 1 has slack (K4's fit test).

**Tests:** `glide`, `glidecpu`, `morph*` groups bit-exact at rest and at every fader position (the snapshot content must equal the synchronous blend for the same fader value: a test that compares the two); `twocore` sim if core 1 is used. Host bench: `morph_begin`'s per-block cost in the render ~0 while gliding.

**Acceptance:** bit-exact; render-side morph cost removed from the stage probe's part 1 figure on the unit; `BD_TIDE_FM6`/`bd_mv` charge revisited (K9).

---

## K9. Lower BUDGET_PEAK to the measured residual <a id="k9"></a>

**Priority** P3. **Size** S. **Depends on** K3 measured on the unit. **Files:** `firmware/src/budget.h` (`BUDGET_PEAK 18`, `BUDGET_LIMIT 72`, the static assert sum 90), `tests/budget_test.c`, `tests/cpu_scenarios_test.c`, `docs/TESTING.md`.

**Steps:** 1 with K3 on the unit, run the 16-multi sweep plus the glide sweep and record, per multi, the raw elapsed peak against the model's prediction (`CPU_SCEN=S build/host/cpu_scenarios_test` gives the model); 2 the residual = max over scenes of (peak - model), excluding scenes where `ease` reached 1 (the ladder covers those); 3 set `BUDGET_PEAK` to that residual rounded up to the next 2, `BUDGET_LIMIT` to 90 minus it; 4 update the budget test expectations (A01's totals change) and TESTING.md's figures; 5 re-run COST-CAL (`bench_runs/cost_merged`) **with transitions** (glide on) so the table's per-engine rows include the crossing cost on the FM6 row.

**Acceptance:** the sweep stays at 0 missed with the new numbers; the NOTES shown on the TIDE page for the factory multis are listed before/after in the commit message.

---

## K10. Dearer part to core 1 by measured cost <a id="k10"></a>

**Priority** P3. **Size** M. **Depends on** K4. **Files:** `twocore_run.c`, `budget.c` (`budget_solve2`'s two-core rule assumes part 2 on core 1), `fx.c` (`mix_block` order).

**Design (Ravel 1.18).** At each half's first block, choose which part core 1 plays: the one with the higher `bd` cost at its current settings (part_fixed + part_cost x its live voices), hysteresis of 10 % so it does not flip every half; the choice is part of the job's snapshot so core 1 never renders the wrong part. The runner's take-back renders whichever part was posted. The budget's two-core rule becomes symmetric (each part on its own core, whichever).

**Tests:** `twocore` sim with alternating choices equals the one-core render; `deadline`, `budget` groups.

**Acceptance:** sim green; on the unit, a DUEL multi with a heavy part 1 and light part 2 shows core 0's part stage down and core 1's up in the probe.

---

## K11. TIDE-only larger half (option, costed) <a id="k11"></a>

**Priority** P3. **Size** S to cost, M to ship. **Files:** `core.h` (`HALF_FRAMES 128`), `audio.c` (`abuf` sized `2 * HALF_WORDS`), `usb.c` (4 uses of HALF_FRAMES: the UAC packetisation assumes the half), `twocore_run.c` (`TCR_HALF_TICKS`, `TCR_BLOCKS`), `fx.c` (1 use), `hal/fm1_audio.h` (the DMA setup takes the half length).

**Question to answer first (no code):** RAM for a 256-frame double buffer (`abuf` doubles: +1 KB if HALF_WORDS is 2 x 128 x 4 B... compute from the real type), the UAC path's assumption, and whether the half length can change at run time (it cannot while the DMA runs: it would be a mode set at TIDE start with the audio restarted, or a build-time option for a "TIDE background" image). Latency: 5.8 ms per half means ~6-12 ms key-to-sound; acceptable for background, not for keyboard play. Write the answer into `CPU_AUDIT.md` section 4's row, then the owner decides whether to ship it as a setting on the TIDE SETTINGS page (the only page allowed new settings).

**Acceptance of the costing:** a short note with RAM delta, code delta, latency, and the list of code that assumes 128.

---

## K12. Process: CPU before a flash <a id="k12"></a>

**Priority** P3. **Size** S. **Files:** `felucca-base/CONTRIBUTING.md`, `docs/TESTING.md` ("CPU on the unit"), `work/CPU_FIX_HANDOFF.md` (point to the new section), the PR template if one exists.

**Write down, as rules:** 1 every change that adds per-sample or per-block work states its steady charge in `budget.c` **and** its transient profile (fader move, load, crossing, mode change) in the commit message; 2 before a flash it is measured on the unit with the test-mode stage probe on a scene that provably activates the path (`tide_measure.py` preflight), transitions included, both directions, raw elapsed peak recorded; acceptance 0 missed over the glide sweep and no new shed or LIGHT FX engagement; 3 `glidecpu` and `cpu_budget.sh` gate regressions on the host (screening, not acceptance); 4 the CHANGELOG line of a flashed build names the unit figures; 5 the flashed commit is merged to `integ` and pushed the same day; 6 new per-block code goes to range Z (K6's check enforces it).

**Acceptance:** the two docs have the section; the handoff file links it; the ROADMAP's "Working standards" section gains one line pointing there.
