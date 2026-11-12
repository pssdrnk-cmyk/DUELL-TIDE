<!-- SPDX-License-Identifier: GPL-3.0-only -->
# The NOTES budget, made honest as far as the host goes (b10 round 6, ROADMAP Later 22)

Findings 5, 6 and 34 of the [full audit](audits/2026-11-08/full/AUDIT_REPORT.md) and brief K9 of the [CPU audit](audits/2026-11-08/cpu/TASKS.md). The owner runs no sweeps, so this is everything the host can say. **The host figures are lower bounds**: the ratio method (below) gave the unit's blend a seventeenth and `fm6_xblock` a fifth to a tenth of what the unit measured. No free headroom is claimed from any of them.

Method: as `tests/tide_cost_probe.c`: a stage is timed with the time stamp counter and scaled by SAT and a three-band EQ, whose cost the table has from the unit (COST-CAL: 28.3 and 83.6; `k` = 3.3 host instructions a tick here). Code from an earlier commit is timed the same way in the parent's tree (a scene in both, interleaved, the fastest of 25 runs; noise about 2 ticks = 8 instructions). `sh host_tests.sh budgetcal` prints the rest.

## What was wrong in the model, and is fixed

**FELUCCA_MOVECOST (round 5, on in the rc3.1 package) took "at rest" to mean "the fader is still".** Three holes, all found by reading `morph_xfer.c` and `morph.c` against `bd_part`:

| Case | The code really does | The model charged (MOVECOST on, before) |
|---|---|---|
| Fader parked inside the arc (5 % .. 95 %) | the valley runs (`x->d`), the wash makes noise (`morph_wash`: `move`) | nothing |
| Fader parked between its ends | both shapes of a cross-faded choice are made (`xf.on` while 0 < weight < 1) | nothing (BD_FADE) |
| A hand or MIDI jump of more than 12 steps | the transition trails the fader for ~1.1 s (`morph_xstep`: `tp` moves `XSTEP` a block) | nothing (`bd_mv` ignores steps over 12) |
| WASH on, nothing moving | the wash's power meter runs on every block of the part | nothing |

Fix (`budget.c` `bd_where`): from the plan and the fader's weight, not from the fader's last step. *Moving* = `bd_mv`, or the transition's place trails the fader; *mid* (BD_FADE) = the weight is strictly between the ends; *arc* (valley, wash, INSERT dip) = the plan has a transition and the weight or the place is inside the arc. At rest with WASH on, the meter is charged: `BD_TIDE_WASH_REST` 6 (host 2). Without MOVECOST nothing changes (both always charged; byte-identical caps and hashes). Flash +132 B (579,156 -> 579,288 B). Tests: `movecost` (the WHERE rows), `budgetcal` (d). The factory multis' caps do not move (all 24 identical at rest, parked and moving: the TIDE items lower a cap by a note for 14 of 1,058 part loads, none of the factory multis).

## Findings

| # | Finding | Host result | Decision |
|---|---|---|---|
| 5 | per-sample work added after the 26 October COST-CAL has no term | A/B against each commit's parent: SVF precision (`85fb25d`) **+5 instructions a sample and filter stage** (the commit message says 4 ALU ops); master knee (`1eef615`) **+4** a sample; delay low cut (`67c2829`) and HALL / PLATE pre-delay (`4ff3788`): under 8, below the probe's resolution | **Not folded into the table.** The PEAK allowance (30 + 0.74 x the model) was fitted on bb730b0 (5 November 13:45), which has all four commits, so adding them counts them twice and takes notes away the sweep says are safe. They go in together with a re-fit of `BUDGET_PEAK` (K9). Written into the comment at the top of `budget.c` |
| 6 | the budget is recomputed in the audio ISR at block 0 of every half | `part_dl` x2 + `global_cost_dl`: 400 .. 1,050 instructions, **3 .. 7 us warm** (0.1 .. 0.25 % of a half) on the 24 factory multis, worst M01 | Stage-probe timer added (`st_budget_us` in the console `cpu`, test-mode builds; `jam/measure/stage_probe.py` prints it as `bud`). **No snapshot until the unit has measured it**: warm 7 us, cold flash (1,388 B of `bd_part` after a half of audio) and the unit/host factors seen elsewhere (5 .. 17) make 15 .. 120 us possible, either side of the audit's 20 us |
| 34 | guessed constants, the blend charged per voice, the wash meter uncharged | blend of a hand-moved fader: 15 .. 25 instructions a sample for every factory morph (4 notes; the budget charges 16 x 4 = 64), ~70 us a block on the unit (BIG STAB). The wash meter at rest: 2 instructions. `BD_TIDE_FM6` 76 stays (it is the unit's: BIG STAB cap 3 -> 2, 0 missed in 20 s) | meter charged (`BD_TIDE_WASH_REST`). `BD_MORPH` / `BD_FADE` unchanged: the blend is a cost of the part, not of a voice, but no host figure can set its size and the sweep's fit contains it |

The canvas bands of rc5 (two passes for pictures over 62 rows) cost the **main loop**, not the audio budget: the ISR preempts it, so they change how many UI frames fit in the 10 % above a 90 % peak, not the NOTES. SP5 builds main-loop code for size only; code the interrupt reaches keeps `-Os` (`size_fns.py`, `isr_reach`).

## Only the unit can confirm (in this order)

1. `st_budget_us` on the 16-multi GLIDE sweep (`python jam/measure/stage_probe.py SLOT --com COMx --glide`): over 20 us -> publish the figures from `budget_poll` as a snapshot (invalidate on a load); under it, close finding 6.
2. A hand jump 0 -> 127 on M02 part 1 in a MOVECOST build: `bdmv`, `valley`, `need` and `st_morph_us` in the `cpu` words during the 1.1 s that follow (the model now charges it; the old build did not).
3. COST-CAL again for the rows of finding 5 (filter 12 / 24 dB, idle with the knee), then K9: refit `BUDGET_PEAK` and fold the four commits into the table at the same time.
4. The blend as a part cost (`morph_begin` for an ANALOG, a WAVE and a PHASE morph, hand-moved, 4 notes) against `BD_MORPH` x voices; the FM6 numbers are the only ones the unit has.
5. UI frame rate on the wave and MORPH pages at 90 % audio peak, with the canvas bands (rc5) and without.
6. Whether `BD_TIDE_VALLEY` 66 and `BD_TIDE_WASH` 60 (host 40 and 33, x1.5) hold: they pass the host rank check, but the unit/host factor of control code is 5 .. 17.
