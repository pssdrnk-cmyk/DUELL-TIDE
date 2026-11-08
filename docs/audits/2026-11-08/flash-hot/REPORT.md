# Effect code from slow flash: analysis (read-only), 2026-11-08

Analyst: read-only. Tree: `work/wt/b3` (fix/b3, b613526), TIDE build, `DUELL_RAMPLACE=1 DUELL_TWOCORE=1` (the default), built here in the foreground for the map (nothing committed, `build/` only). Sizes below are `llvm-nm -S` / `felucca.dis` sizes of that build; the 116 names and their bytes (44,318 B in all) match `tools/ram_allow.py` "Mix and effect stages (116)".

## 1. Short answer

- Of the 116, **50 contain or sit inside a per-sample loop (33.6 KB), 53 run per block or per half (9.5 KB), 13 run on an event or a fade (1.2 KB)**. But "per sample" does not mean "always on": most of the big ones are gated by a model, a mode or a master setting (HALL vs PLATE, DUEL, COMP, WIDTH, PERFORM, RAND/OCT+ delay). Only about 2.9 KB of the 116 are on in every normal STEREO/LAYER scene with the sends on.
- Room: range Z 568 B above the 8,192 B pool-tail rule (8,760 B free to the guard, build line `range Z ... 28680 B (8760 B free up to the stack guard)`), hot block 388 B (`12124 B (32 functions) ... 388 B free`), range X 488 B but unusable (closed set: nothing in the flash may call into X, and everything below is called from the flash). That is 956 B without evicting anything.
- **The gain is not measured anywhere in the repo.** `docs/KNOWN_ISSUES.md`: "The flash cache adds an unknown part." The only number is anecdotal: `morph_apply_at` (1,078 B) in the flash spiked a GLIDE half to 102 % (CPU_AUDIT C7, `hot_fns.py` GROUPS[3] comment). So the expected gain below is a model with a stated range, and the plan starts with a measurement that fixes the one unknown (the cost of one cache-line fill) before any audio code moves.
- Expected gain of the proposed move (1.9 KB total, two steps): **about 0.7 to 2.9 % of a half (20 to 85 us of 2,902 us) if the code is cold once per half, up to four times that if it is evicted between the four blocks of a half; zero for whatever the cache already keeps.** That is a useful hedge on the five 91 to 101 % multis, not a fix by itself.

## 2. How the FM-1's XIP and cache work (what the repo says, what it does not)

Documented in the repo:
- Code runs from the SPI NOR flash through the SFC at `0x02000000 + off - 0x4000` (`hal/fm1_xip.h`). `tools/hot_fns.py` header: "the audio code runs from the flash (XIP) through a 32 KB instruction cache" (a hypothesis of 2026-10-15 that the bench A / D-off / C builds supported: placement moved missed halves 2 to 4 times). `docs/BENCH_GUIDE.md` (BENCH-B1): the 40 KB flash test "cannot stay in the 32 KB cache that both cores share".
- Line size is 32 B: `hal/fm1_flash.h` `fl_inval` invalidates with `flushinv` in 32 B steps.
- The official M-VAVE firmware keeps all its audio code in SRAM (hot_fns.py); Ravel runs "the whole voice loop and its bigger blocks from RAM" (CPU_AUDIT section 4).
- The audio ISR runs 4 blocks of CTL = 32 samples per half (HALF_FRAMES = 128, `audio.c`: `for (b = 0; b < HALF_FRAMES; b += CTL) audio_block`), half budget 2,902 us (`cpu_multis.py` bud). So every flash function in the per-sample path executes only 32 iterations per call: one cold pass through its code is a large share of its cost.

Not documented anywhere: the cost of one line fill, the cache's associativity, whether the SFC reads ahead, the CPU clock. My own arithmetic (not from the repo): quad SPI at 60 to 80 MHz, 32 B = 64 data clocks + about 18 command/address/dummy clocks = 0.8 to 1.4 us per line; with continuous-read mode (mode 6/7 appears in `fl_enter`) less. At roughly 150 to 200 MHz that is **C = 60 to 240 CPU cycles per line fill** (bracket used below; 120 is the middle). One percent of a half is about 4.2 to 5.6 k cycles (the host model: 33 instructions per sample = 1 %, 128 samples per half), so I use 5 k.

Model: gain per half = (flash bytes of that function that execute per half) / 32 x C x (share of halves in which the cache lost them). Consequences:
1. For code that executes fully every block, gain per byte is the same for every function: 1 KB = 32 lines = 0.4 / 0.8 / 1.5 % of a half at C = 60 / 120 / 240, if cold once per half. Ranking is therefore by **executed bytes that are on in most scenes**, not by instruction counts.
2. A function whose code is mostly rare branches (`mix_sends` 904 B, `mix_gather` 1,306 B) gains only its executed fraction.
3. The peaks are where this matters most: a GLIDE half runs extra flash code (morph, TIDE, FM6 bridge, budget) and so evicts the effects code, which the next half then re-fetches. I cannot size this without the unit.

## 3. The 116 functions

Class: **S** per sample (the function is, or is called from inside, a loop over the block's samples); **B** per block or per half; **E** per event, fade or parameter change. "Active when" says how often it is on in practice. "Back-branches" is a count from the disassembly (loops, including inlined ones). Verdicts: **M1** move in step 1 (fits without evicting anything); **M2** step 2 / reserve list (needs the hot block room from evicting PERFORM's two stages, or is too small to matter alone); **L** per-sample or per-block but too big for the room, later or only with an overlay; **F** per-sample but only when a master/DUEL/feature setting is on: leave in the flash, re-check if the 16-multi sweep turns out to run it; **N** leave: event, transient (PERFORM held, fades, USB audio), UI, or a mode no factory multi uses.

Counts: S 50 (33,610 B), B 53 (9,482 B), E 13 (1,226 B). Verdicts: M1 8 (1,036 B), M2 14 (2,320 B), L 16 (9,684 B), F 21 (10,520 B), N 57 (20,758 B).

Factory multis (tools/factory_multis.py, as far as the file lists them): reverb PLATE x5, ROOM (HALL's code, short decay) x3, HALL x3, SPRING x1, none SHIM or RESO; delay DIGI or PING, one with DMDEP 17 (the modulated delay); modes LAYER (STEREO/LAYER path) except M08 MONO DUEL (spare). So the `_b` (bus B / DUEL) twins and dly_ev / rev_shimx / comp / width / bass mono are not in the 16-multi sweep unless the unit's master settings turn them on.

| # | function | B | class | active when | call site and loop evidence (map/disassembly callers; back-branches) | verdict |
|---|---|---|---|---|---|---|
| 1 | `rev_front` | 244 | S | HALL + PLATE | loop over the decimated block (nc) per call, fx_bus.c:163, noinline; callers: rev_hall,rev_hall_b,rev_plate; 1 back-branch(es) | M1 |
| 2 | `dly_wr` | 160 | S | always (all delay paths) | called once per sample from fx_buses_m, dly_mod (hot block!), dly_ev, split_buses; noinline (fx.c:442); callers: dly_ev,dly_ev_b,dly_mod; 0 back-branch(es) | M1 |
| 3 | `blk_snap` | 154 | B | always | straight line, fx.c:3471 once per block, called from the ISR; callers: fm1_alnk0_irq; 2 back-branch(es) | M1 |
| 4 | `acc_sum` | 128 | S | always | 5 loops over n; mix_gather sums the parts accumulators into mix_l/mix_r/sends (fx.c:3122); callers: mix_gather; 5 back-branch(es) | M1 |
| 5 | `master_knee` | 118 | S | always | 118 B noinline, called 2x per sample from the mix_master loop (fx.c:354-355) = 64 calls per block; callers: mix_master,split_master; 0 back-branch(es) | M1 |
| 6 | `hall_gains` | 114 | B | HALL/ROOM | 114 B per block (fx_bus.c:146; 1 loop, exp2_neg); callers: rev_hall; 1 back-branch(es) | M1 |
| 7 | `cho_zrun` | 76 | B | always | one scan loop over n at most, fx_bus.c:693, once per block (mix_sends, split_buses); callers: mix_sends,split_buses; 1 back-branch(es) | M1 |
| 8 | `fx_swing` | 42 | B | always | called per block by the chorus/delay glide set-up (fx_buses_m, dly_mod, dly_ev); callers: dly_ev,dly_ev_b,dly_mod; 0 back-branch(es) | M1 |
| 9 | `fx_buses_m` | 988 | S | always (DIGI/PING, MOD 0) | 988 B symbol called from mix_sends; chorus + delay loop over n (fx_bus.c:864); DMDEP>0 uses dly_mod (hot), RAND/OCT+ uses dly_ev; callers: mix_sends,split_buses; 1 back-branch(es) | M2 |
| 10 | `plain_fx` | 294 | B | always | 294 B, 0 loops; per part per block from part_fx (Z): SAT, INSERT, EQ crossfades (fx.c:2933); callers: part_fx; 0 back-branch(es) | M2 |
| 11 | `rev_locut` | 278 | S | always (LOW CUT default 75) | one-pole HP loop over n every block while G_RLOCUT>0 (fx_bus.c:599); callers: rev_tail; 2 back-branch(es) | M2 |
| 12 | `mix_events` | 110 | B | always | fx.c:3463 once per block (calls events_block in flash); callers: fm1_alnk0_irq; 0 back-branch(es) | M2 |
| 13 | `shed_check` | 104 | B | per half | 104 B, ISR end of each half; callers: fm1_alnk0_irq; 0 back-branch(es) | M2 |
| 14 | `ovl_step` | 88 | B | per half | 88 B from shed_check; callers: shed_check; 0 back-branch(es) | M2 |
| 15 | `fxb_pre` | 80 | B | always | 80 B per part per block from part_fx/plain_fx/spread_*; callers: part_fx,plain_fx,spread_duck; 0 back-branch(es) | M2 |
| 16 | `fxb_sends` | 66 | B | always | 66 B from part_fx (fx.c:2921); callers: part_fx; 0 back-branch(es) | M2 |
| 17 | `mix_part` | 66 | B | always | 66 B, once per part per block (ISR; tcr_play2 for part 2 when core 0 plays it); callers: fm1_alnk0_irq,tcr_play2; 0 back-branch(es) | M2 |
| 18 | `exp2_neg` | 56 | B | per voice with a multimode filter | 56 B, UF_K coefficient macro in voice.c; also hall_gains, fx_swing; callers: draw_graph,fx_swing,hall_gains; 0 back-branch(es) | M2 |
| 19 | `space_goal` | 54 | B | always | 54 B, tested every block in mix_sends (fx.c:3331); callers: mix_sends,space_step; 0 back-branch(es) | M2 |
| 20 | `cut_slew` | 50 | B | per voice with a multimode filter | 50 B, voice.c:1250, per voice per block; callers: part_voice; 0 back-branch(es) | M2 |
| 21 | `fxb_done` | 44 | B | always | 44 B per part per block; callers: part_fx,plain_fx,spread_duck; 0 back-branch(es) | M2 |
| 22 | `fxb_save` | 42 | B | always | 42 B from fxb_pre; callers: fxb_pre; 0 back-branch(es) | M2 |
| 23 | `rev_plate` | 1614 | S | PLATE | 1614 B; per sample (fx_bus.c:448); callers: rev_run; 1 back-branch(es) | L |
| 24 | `rev_hall` | 1394 | S | HALL + ROOM | 1394 B, 13 loops; per sample reverb network (fx_bus.c:194); callers: rev_run; 13 back-branch(es) | L |
| 25 | `bd_part` | 1390 | B | per half | budget model, 12 back-branches, runs 2x per half (twocore_run.c:324) and from fm1_main on a parameter change; callers: fm1_main,part_dl; 12 back-branch(es) | L |
| 26 | `mix_gather` | 1306 | S | always | 13 loops; ISR -> mix_block -> mix_gather once per 32-sample block; its sum loops are per sample; callers: fm1_alnk0_irq; 13 back-branch(es) | L |
| 27 | `mix_sends` | 904 | B | always | 7 loops, nearly all in rare branches (hold/fade/split); the common path is a run of tests and calls (fx.c:3326); callers: fm1_alnk0_irq; 7 back-branch(es) | L |
| 28 | `rev_spring` | 616 | S | SPRING | 616 B (fx_bus.c:88); callers: rev_run; 2 back-branch(es) | L |
| 29 | `rev_reso` | 596 | S | RESO only | 596 B (fx_bus.c:404); callers: rev_run; 5 back-branch(es) | L |
| 30 | `mix_master` | 570 | S | always | 570 B; the final loop runs master_out per sample (fx.c:3418+); callers: fm1_alnk0_irq; 2 back-branch(es) | L |
| 31 | `bd_global` | 430 | B | per half | global_cost_dl each half (budget.c:297); also fm1_main; callers: fm1_alnk0_irq,fm1_main; 2 back-branch(es) | L |
| 32 | `master_dac` | 270 | S | always | 270 B, 2 loops over n, ISR audio_block (audio.c:100); callers: fm1_alnk0_irq; 2 back-branch(es) | L |
| 33 | `rev_tail` | 212 | B | always (scan); model only with a send | 4 loops: the nonzero scan over n and the zero/accumulate loops (fx_bus.c:810); calls locut, byp, run; callers: fx_buses_ev,fx_buses_m,fx_buses_mod; 4 back-branch(es) | L |
| 34 | `bd_slope` | 122 | B | per half | called from bd_part (budget.c:123); callers: bd_part; 5 back-branch(es) | L |
| 35 | `part_dl` | 80 | B | per half | budget deadline: tcr_deadline at the first block of each half (twocore_run.c:324), 2 calls; callers: fm1_alnk0_irq; 0 back-branch(es) | L |
| 36 | `plate_damp` | 64 | B | PLATE | 64 B per block; callers: rev_plate,rev_plate_b; 0 back-branch(es) | L |
| 37 | `rev_run` | 62 | B | with a send | 62 B switch to the model (fx_bus.c:517); callers: rev_fadeout,rev_tail; 0 back-branch(es) | L |
| 38 | `plate_gain` | 54 | B | PLATE | 54 B per block; callers: rev_plate,rev_plate_b; 0 back-branch(es) | L |
| 39 | `rev_plate_b` | 1614 | S | DUEL only (bus B) + PLATE | bus B twin of rev_plate; 1614 B; per sample (fx_bus.c:448); callers: rev_run_b; 1 back-branch(es) | F |
| 40 | `rev_hall_b` | 1394 | S | DUEL only (bus B) + HALL + ROOM | bus B twin of rev_hall; 1394 B, 13 loops; per sample reverb network (fx_bus.c:194); callers: rev_run_b; 13 back-branch(es) | F |
| 41 | `comp_stage` | 1360 | S | master COMP engaged | 1360 B, 8 loops, 53 host insn/sample (SPEC); callers: mix_master; 8 back-branch(es) | F |
| 42 | `split_buses` | 1090 | S | DUEL only | 1090 B, 5 loops; the DUEL (dm_split) sends path (fx.c:3366); callers: mix_sends; 5 back-branch(es) | F |
| 43 | `bass_mono` | 772 | S | master BASS MONO on | 772 B, 39 host insn/sample (SPEC); callers: mix_master; 4 back-branch(es) | F |
| 44 | `rev_spring_b` | 616 | S | DUEL only (bus B) + SPRING | bus B twin of rev_spring; 616 B (fx_bus.c:88); callers: rev_run_b; 2 back-branch(es) | F |
| 45 | `rev_reso_b` | 596 | S | DUEL only (bus B) + RESO only | bus B twin of rev_reso; 596 B (fx_bus.c:404); callers: rev_run_b; 5 back-branch(es) | F |
| 46 | `space_width` | 594 | S | PERFORM SPACE off-centre | 594 B, 3 loops, master; callers: mix_master; 3 back-branch(es) | F |
| 47 | `width_stage` | 568 | S | master WIDTH != 100 | 568 B, 3 loops, 38 host insn/sample (SPEC); callers: mix_master; 3 back-branch(es) | F |
| 48 | `split_master` | 356 | S | DUEL only | 356 B, final loop per sample incl. 2x master_knee per side; callers: mix_master; 1 back-branch(es) | F |
| 49 | `rev_locut_b` | 286 | S | DUEL only (bus B) + always (LOW CUT default 75) | bus B twin of rev_locut; one-pole HP loop over n every block while G_RLOCUT>0 (fx_bus.c:599); callers: rev_tail_b; 2 back-branch(es) | F |
| 50 | `spread_fx` | 266 | B | SPREAD on | 266 B from mix_spread (hot); callers: mix_spread; 0 back-branch(es) | F |
| 51 | `rev_tail_b` | 212 | B | DUEL only (bus B) + always (scan); model only with a send | bus B twin of rev_tail; 4 loops: the nonzero scan over n and the zero/accumulate loops (fx_bus.c:810); calls locut, byp, run; callers: fx_buses_ev_b,fx_buses_mod_b,split_buses; 4 back-branch(es) | F |
| 52 | `cmp_coef` | 206 | B | COMP engaged | 206 B, 0 loops, coefficients; callers: comp_stage; 0 back-branch(es) | F |
| 53 | `mod_bus` | 158 | B | matrix routes to bus values | 158 B, only if mod_gon_any(); callers: mix_sends,split_buses; 2 back-branch(es) | F |
| 54 | `hall_gains_b` | 114 | B | DUEL only (bus B) + HALL/ROOM | bus B twin of hall_gains; 114 B per block (fx_bus.c:146; 1 loop, exp2_neg); callers: rev_hall_b; 1 back-branch(es) | F |
| 55 | `dly_side_mix` | 90 | S | PING/event delay only | 1 loop over n (mix_sends, fx.c:3372); callers: mix_sends; 1 back-branch(es) | F |
| 56 | `spread_duck` | 76 | B | SPREAD on | 76 B from mix_spread (hot); callers: mix_spread; 0 back-branch(es) | F |
| 57 | `rev_run_b` | 62 | B | DUEL only (bus B) + with a send | bus B twin of rev_run; 62 B switch to the model (fx_bus.c:517); callers: rev_fadeout_b,rev_tail_b; 0 back-branch(es) | F |
| 58 | `rev_side_mix` | 46 | S | HALL only (hl.side) | 46 B, 1 loop over n after the buses (fx.c:3370); callers: mix_sends; 1 back-branch(es) | F |
| 59 | `cmp_big` | 44 | S | COMP engaged | 44 B, 0 loops, called from comp_stage; callers: comp_stage; 0 back-branch(es) | F |
| 60 | `dly_ev` | 2600 | S | RAND/OCT+ delay mode only | 2600 B, fx_buses_ev; 5 loops over n; no factory multi uses it; the overload guard turns it into the plain delay; callers: fx_buses_ev; 5 back-branch(es) | N |
| 61 | `dly_ev_b` | 2032 | S | DUEL + RAND/OCT+ | bus B twin; callers: fx_buses_ev_b; 7 back-branch(es) | N |
| 62 | `perf_block` | 1802 | S | PERFORM FX held | 1802 B; callers: perf_master; 5 back-branch(es) | N |
| 63 | `pf_chan` | 1248 | S | PERFORM filter held | 1248 B, 9 loops; callers: perf_filter; 9 back-branch(es) | N |
| 64 | `dly_mod_b` | 958 | S | DUEL + DMDEP>0 | bus B twin of dly_mod (hot); callers: fx_buses_mod_b; 1 back-branch(es) | N |
| 65 | `spread_post` | 924 | B | SPREAD + matrix/MORPH moving gains | 924 B, 5 loops, from mix_gather; only when a gain moves; callers: mix_gather; 5 back-branch(es) | N |
| 66 | `rev_shimx` | 902 | S | SHIM + PITCH/MOD only | 902 B, called from rev_shim (hot); callers: rev_shim; 1 back-branch(es) | N |
| 67 | `rev_shimx_b` | 902 | S | DUEL only (bus B) + SHIM + PITCH/MOD only | bus B twin of rev_shimx; 902 B, called from rev_shim (hot); callers: rev_shim_b; 1 back-branch(es) | N |
| 68 | `perf_filter` | 832 | S | PERFORM filter held | 832 B; callers: perf_block; 2 back-branch(es) | N |
| 69 | `perf_harm` | 584 | S | PERFORM held | 584 B; callers: perf_block; 0 back-branch(es) | N |
| 70 | `perf_begin` | 548 | B | PERFORM active | 548 B; mix_block calls perf_tick when idle, perf_begin otherwise; callers: fm1_alnk0_irq; 4 back-branch(es) | N |
| 71 | `rev_shim_b` | 444 | S | DUEL + SHIM | bus B twin; callers: rev_run_b; 3 back-branch(es) | N |
| 72 | `uac_tap48` | 432 | S | USB audio 48k ring only | 432 B, 4 loops; callers: uac_tap; 4 back-branch(es) | N |
| 73 | `spring_glide` | 422 | S | SPRING while SIZE glides | 422 B, only on a SIZE change; callers: rev_spring; 2 back-branch(es) | N |
| 74 | `spring_glide_b` | 422 | S | DUEL only (bus B) + SPRING while SIZE glides | bus B twin of spring_glide; 422 B, only on a SIZE change; callers: rev_spring_b; 2 back-branch(es) | N |
| 75 | `perf_phaser` | 382 | S | PERFORM held | 382 B (hot_fns.py already sent it to flash on purpose); callers: perf_block; 3 back-branch(es) | N |
| 76 | `rev_room_b` | 354 | S | DUEL + SHIM | bus B twin; callers: rev_shim_b; 3 back-branch(es) | N |
| 77 | `space_vals` | 350 | B | PERFORM SPACE | from space_step; callers: space_step,split_buses; 0 back-branch(es) | N |
| 78 | `dlyev_read` | 332 | S | RAND/OCT+ only | inside the dly_ev loop (332 B); callers: dly_ev; 1 back-branch(es) | N |
| 79 | `dlyev_read_b` | 286 | S | DUEL + RAND/OCT+ | bus B twin; callers: dly_ev_b; 1 back-branch(es) | N |
| 80 | `click_render` | 270 | B | metronome | 270 B, ISR every block; returns at once when the click is idle (env<=0); callers: fm1_alnk0_irq; 1 back-branch(es) | N |
| 81 | `perf_pump` | 252 | S | PERFORM held | 252 B; callers: perf_block; 1 back-branch(es) | N |
| 82 | `space_sends` | 246 | S | PERFORM SPACE off-centre | 246 B, 2 loops; callers: mix_sends; 2 back-branch(es) | N |
| 83 | `spread_gains` | 222 | B | same | 222 B; callers: spread_post; 0 back-branch(es) | N |
| 84 | `uac_render_start` | 222 | B | USB audio | 222 B, ISR once per half if FELUCCA_UAC; callers: fm1_alnk0_irq; 2 back-branch(es) | N |
| 85 | `bd_tw_of` | 182 | B | per half, TWIN live | called from bd_part when a TWIN filter is live (budget.c:188); callers: bd_part; 1 back-branch(es) | N |
| 86 | `snd_step` | 170 | E | MODULES SENDS state | 170 B from mix_events when snd_st changes; callers: mix_events; 0 back-branch(es) | N |
| 87 | `snd_fade` | 166 | E | MODULES SENDS fade | 166 B; callers: mix_sends; 1 back-branch(es) | N |
| 88 | `pf_fade` | 162 | S | PERFORM filter | 162 B; callers: perf_filter; 1 back-branch(es) | N |
| 89 | `duck_half` | 160 | E | around a flash write | ISR, only if duck_on; callers: fm1_alnk0_irq; 1 back-branch(es) | N |
| 90 | `duck_pump` | 156 | E | DUCK SRC PUMP | 156 B, events_block; callers: events_block; 1 back-branch(es) | N |
| 91 | `uac_tap` | 150 | B | USB audio streaming only | 150 B, returns at once if !uac.feed (usb.c:1122); callers: fm1_alnk0_irq; 1 back-branch(es) | N |
| 92 | `perf_master` | 146 | S | PERFORM held | 146 B; callers: mix_master; 3 back-branch(es) | N |
| 93 | `pf_stage_coefs` | 118 | B | PERFORM filter | 118 B; callers: perf_filter; 0 back-branch(es) | N |
| 94 | `dly_slide` | 108 | S | while the delay time slides | per sample from fx_buses_m only while g != tg (a TIME/tempo change); callers: fx_buses_m,split_buses; 1 back-branch(es) | N |
| 95 | `rev_fadeout` | 104 | E | type change / BYPASS | 2 loops, once; callers: rev_byp,rev_switch; 2 back-branch(es) | N |
| 96 | `rev_fadeout_b` | 104 | E | DUEL only (bus B) + type change / BYPASS | bus B twin of rev_fadeout; 2 loops, once; callers: rev_byp_b,rev_switch_b; 2 back-branch(es) | N |
| 97 | `voice_step` | 96 | E | on a voice discontinuity | 96 B, only if (offp|offn) (voice.c:1359); callers: part_voice; 1 back-branch(es) | N |
| 98 | `dly_mode_why` | 90 | E | UI | only from mod_dst_why/card_hidden_vp (UI text); callers: card_hidden_vp,mod_dst_why; 0 back-branch(es) | N |
| 99 | `cho_zrun_b` | 86 | B | DUEL only | bus B twin; callers: split_buses; 1 back-branch(es) | N |
| 100 | `rev_byp` | 70 | B | always | first few insns only unless BYPASS; callers: rev_tail; 0 back-branch(es) | N |
| 101 | `rev_byp_b` | 70 | B | DUEL only (bus B) + always | bus B twin of rev_byp; first few insns only unless BYPASS; callers: rev_tail_b; 0 back-branch(es) | N |
| 102 | `meter_scan` | 68 | B | GLO page + SPLIT | 68 B, meter_on only; callers: fm1_alnk0_irq; 1 back-branch(es) | N |
| 103 | `f2_bypmix` | 66 | E | TWIN F2 bypass | 66 B; callers: chain_f2; 1 back-branch(es) | N |
| 104 | `mod_bus_end` | 66 | B | same | 66 B; callers: mix_sends; 1 back-branch(es) | N |
| 105 | `perf_mod_depth` | 66 | B | PERFORM held | 66 B; callers: perf_flanger,perf_phaser; 0 back-branch(es) | N |
| 106 | `space_step` | 66 | B | PERFORM SPACE | only when space_goal or sv.spc; callers: mix_sends; 0 back-branch(es) | N |
| 107 | `ph_coef` | 56 | B | PERFORM phaser | 56 B; callers: perf_phaser; 0 back-branch(es) | N |
| 108 | `fxb_mix` | 54 | S | BYPASS fade of a part FX | 54 B, 1 loop, only during the fade; callers: fxb_done; 1 back-branch(es) | N |
| 109 | `byp_scale` | 52 | E | BYPASS fades (8 blocks) | mix_sends only while the bypass gain ramps; callers: mix_sends; 1 back-branch(es) | N |
| 110 | `delay_samples` | 36 | B | always | 36 B, per block from mix_sends (also UI); callers: delay_div_shown,mix_sends,split_buses; 1 back-branch(es) | N |
| 111 | `fx_buses_ev` | 36 | B | RAND/OCT+ only | 36 B wrapper: dly_ev then rev_tail; callers: mix_sends,split_buses; 0 back-branch(es) | N |
| 112 | `rev_switch` | 22 | E | reverb type change | one block fade; callers: rev_tail; 0 back-branch(es) | N |
| 113 | `rev_switch_b` | 22 | E | DUEL only (bus B) + reverb type change | bus B twin of rev_switch; one block fade; callers: rev_tail_b; 0 back-branch(es) | N |
| 114 | `f2_bypsave` | 18 | E | TWIN F2 bypass | 18 B; callers: chain_f2; 1 back-branch(es) | N |
| 115 | `fx_buses_ev_b` | 12 | B | DUEL only | wrapper, bus B; callers: split_buses; 0 back-branch(es) | N |
| 116 | `fx_buses_mod_b` | 12 | B | DUEL + DMDEP | wrapper, bus B; callers: split_buses; 0 back-branch(es) | N |
## 4. Ranking by expected gain per byte

Gain per byte is equal for fully executed code (section 2), so the order is: always on, fully executed, per sample first; then per block; then gated code weighted by how many scenes use it.

| rank | function(s) | B | why | active in |
|---|---|---|---|---|
| 1 | `master_knee` | 118 | 64 calls per block from the mix_master loop, fully executed, tiny | every scene, STEREO/LAYER and DUEL |
| 2 | `dly_wr` | 160 | 1 call per sample in every delay path, including the one in the hot `dly_mod` (a hot-block function calling the flash per sample) | every scene with sends, all delay modes |
| 3 | `acc_sum` | 128 | the sum loops of mix_gather | every scene |
| 4 | `rev_front` + `hall_gains` | 244 + 114 | per-block loop and gains of HALL/ROOM/PLATE, fully executed | 10 of the 11 readable multis (HALL, ROOM, PLATE) |
| 5 | `cho_zrun` + `fx_swing` | 76 + 42 | per block, executed whenever the sends are on | every scene with sends |
| 6 | `fx_buses_m` | 988 | the default chorus + delay loop; the biggest always-on, fully executed item that can be moved (host share 1.3 % of mix_block samples, 2.5 % on the multis: `hot_fns.py` table) | DIGI/PING without DMDEP: 10 of 11 |
| 7 | `rev_locut` | 278 | one-pole HP loop every block (LOW CUT default 75) | every scene with a reverb send |
| 8 | the per-part chain bits `plain_fx`, `fxb_pre/done/save/sends`, `cut_slew`, `exp2_neg`, `mix_events`, `mix_part`, `space_goal`, `shed_check`, `ovl_step` | about 1,000 | per block, straight line, fully executed, called from Z or the ISR | every scene |
| 9 | the reverb models `rev_hall` 1,394, `rev_plate` 1,614, `rev_spring` 616 (+ `plate_*`, `spring_glide`, `rev_tail` 212, `rev_run` 62) | 1.4 to 1.8 KB each | the largest executed bytes after the ISR, but only one model is on, and none fits the 956 B | by reverb type |
| not movable | the ISR `fm1_alnk0_irq` (2,804 B, with mix_block and the event dispatch inlined), `mix_gather` 1,306, `mix_sends` 904, `mix_master` 570, `master_dac` 270, the budget model `bd_part` 1,390 + `bd_global` 430 + `bd_slope` 122 + `part_dl` 80 (once per half) | about 7 KB | these run in every half: the largest always-run flash block on core 0 | every scene |

Not worth moving despite "per sample": `comp_stage` 1,360 + `cmp_coef` 206, `width_stage` 568, `bass_mono` 772, `space_width` 594 (master features the user sets; which ones the owner uses live is not in the repo), the DUEL twins, `dly_ev` (2,600 B, RAND/OCT+ only), all PERFORM code (transient; `hot_fns.py` already decided `perf_phaser` stays in the flash for that reason).

## 5. Plan

### Step 0: measure the line-fill cost C (test-mode build, no audio code moves)

The repo never measured it, and the whole gain is lines x C. A 30-line test-mode-only console command `xipprobe` (under `DUELL_TEST_MODE`, never in a shipped build): for a given flash function (say `rev_hall`, 1,394 B), `fl_inval` its range (`hal/fm1_flash.h`, 32 B `flushinv`), time one call cold and one call warm with `fm1_ticks()` (24 MHz, 41.7 ns resolution), with the audio ISR masked for that call only (about 50 us: a click in a test build only), 20 repetitions. (cold - warm) / lines touched = C in ticks. Do the same on a RAM-resident function of equal size as a control. Decision rule: the whole proposal is 60 lines (1.9 KB), so its gain is 60 x C. Go if that is at least 15 us (0.5 % of the 2,902 us half), i.e. C at least 0.25 us (about 50 cycles); otherwise stop and write the number into this item's ROADMAP row.

### Step 1: no eviction, 882 B (the safe set)

| into | function | B |
|---|---|---|
| range Z (568 B above the reserve) | `dly_wr` | 160 |
| | `master_knee` | 118 |
| | `acc_sum` | 128 |
| | `cho_zrun` | 76 |
| | `fx_swing` | 42 |
| | subtotal | **524** (44 B spare) |
| hot block (388 B free) | `rev_front` | 244 |
| | `hall_gains` | 114 |
| | subtotal | **358** (30 B spare) |

How: add a group (e.g. "MIX PATH") to `tools/ram_fns_z.py` GROUPS and the two to `tools/hot_fns.py` GROUPS[3]; delete the eight lines from `tools/ram_allow.py` (the build names them). Both tools rename the sections in the object file after compilation (`.text.f` to `.ramz.f` / `.hotx.f`), so the compiled instructions are identical: placement only. All eight are `noinline` or separate symbols in the map, and every flash caller reaches RAM by a direct call (build: `641 cross between RAM and the flash`, nearest to the limit 82,882 B spare).
Expected gain: 882 B = 28 lines = **0.3 / 0.7 / 1.3 % of a half** at C = 60 / 120 / 240 cycles (cold once per half).

### Step 2: evict PERFORM's two stages from the hot block, add the delay loop (1,042 B)

Move `perf_drive` (546 B) and `perf_flanger` (478 B) out of `tools/hot_fns.py` GROUPS[3] back to the flash (frees 1,024 B; hot free becomes 30 + 1,024 = 1,054 B). Add to the hot block `fx_buses_m` 988 + `space_goal` 54 = 1,042 B (12 B spare). Why these two stages: they run only while the performer holds the PERFORM button, and `perf_phaser` was already sent to the flash for the same reason in 0.2.14 (hot_fns.py comment). Why not `rev_shim` + `rev_room` (794 B): SHIM is the dearest reverb (151 host instr/sample), so it is the one scene that can least afford the flash.
Expected extra gain: 33 lines = **+0.4 / +0.8 / +1.6 %**. Step 1 + 2 together: 60 lines, **0.7 / 1.4 / 2.9 %** (20 to 85 us per half), at C = 60 / 120 / 240.
Cost of step 2: PERFORM FLANGER and DRIVE held run from the flash. Measure with PERFORM held on the two heaviest multis; abort step 2 if the raw peak of any scene rises by more than step 2 gains.

### Not proposed
- Reverb models (1.4 to 1.8 KB each), `mix_gather`, `mix_master`: no room. If Step 0 shows a large C, the options are (a) a bigger eviction (`dly_mod` 1,466 B to the flash: only the multis with DMDEP > 0 pay), (b) a code overlay that copies the selected reverb model into a RAM slot on a type change (a new mechanism, not timing-only: a separate item), (c) lowering the 8 KB pool-tail reserve, which CPU audit section 5 F says stays.
- The budget model (`bd_part` + `bd_global` + `bd_slope` + `part_dl` = 2.1 KB, run once per half from `tcr_deadline` at the first block, `twocore_run.c:324`): mostly straight-line code with little arithmetic, so its flash cost is high compared with its own instructions. Running it only when its inputs change (the `tcr.cost_ok` flag already exists) would save more than any placement here (about 66 lines, 0.8 to 3 % of a half, on the first block of each half), but it changes when the deadline logic re-reads the voice count: a behaviour change, not timing-only. Suggest a separate ROADMAP row.

### How to measure on the unit, before and after

Kit: `jam/measure/cpu_multis.py`, `stage_probe.py`, `glide_watch.py` (copies in `work/measure/cpu-multis/`), the console `cpu` / `status` / `mload` / `glide` commands, a `DUELL_TEST_MODE=1` build (UAC off as in CPU_FIX_HANDOFF) with `DUELL_PROBE_STAGES=1`.
1. Extend the probe (test-mode only). `prb_stage_pk[5]` in `fx.c` times morph_begin / part 1 / part 2 wait. Add timers around `mix_gather`, `mix_sends` and `mix_master` in `mix_block`, and keep a sum and a count as well as the peak, so a per-stage MEAN exists (the peak alone is too noisy for a 1 % effect). A and B builds carry the identical probe.
2. Build A (b3 + probe), B (A + step 1), C (B + step 2). Flash A, B, A, B, C, so layout and thermal drift average out. Moving code shifts the flash addresses of everything after it, and `docs/BENCH_GUIDE.md` says a shift of 18 to 96 B "can move the readings by a little" (cache conflicts), so one flash per variant cannot show 1 %: repeat each build twice.
3. Scenes: the five peak multis from `work/measure/cpu-multis/sweep_glide_fix7.txt` (raw max: 11 = 101, 8 = 95, 13 = 94, 2 = 93, 6 = 91) plus one HALL, one PLATE and one SPRING multi. For each: steady (`cpu_multis.py N --hold 20`, held chord, no glide) for the mean and `cpu_raw_pk_us`; GLIDE (`--glide --gtime 0 --hold 20`) for peaks and misses; `stage_probe.py N --glide` for the per-stage numbers. 3 repetitions each.
4. Accept if: the stage means for gather + sends + master fall by at least 60 % of the Step 0 prediction (60 x C), the raw peak of the five multis rises in none, 0 new misses over the 16-multi sweep. Otherwise revert (nothing else depends on it).
5. Build checks that must stay green: the K6 hot-code check (`hot placement ... 0 not`), `elfcheck` (range Z, hot block, call reach), the 8,192 B pool tail (the build fails if broken), and a disassembly diff of the moved functions against build A (instructions equal, only addresses and relocations differ): that is the evidence that the sound cannot change.

### Risks
- **The gain may be near zero** if the cache keeps this code between halves (C small, or no thrash). Step 0 is cheap and decides it before any audio code moves. The honest prior is small: 1 to 3 % against peaks of 91 to 101 %. It does not close the 1.0.0 goal alone.
- **Headroom drops to 44 B (Z) and 30 / 12 B (hot).** The next feature that grows range Z or a hot function fails the build (enforced by `POOL_TAIL_MIN`, `elfcheck`; not silent), and it has to find its own room. Record this in the ROADMAP row and in the header of `ram_fns_z.py`.
- **Layout shift:** removing 1.9 KB from `.text` moves every later flash function; a low-associativity cache can gain or lose a few conflict misses elsewhere (hot_fns.py saw 2 to 4 times from placement in 2026-10). The A/B/A/B measurement is there to catch it; a regression means revert.
- **Step 2 hurts PERFORM:** FLANGER/DRIVE held run from the flash. Measured in step 2.
- **Timing-only but not inert:** the overload guard (`ovl_step`, `shed_check`) and the deadline fall-back react to timing, so slightly faster code moves the point where LIGHT FX or shedding engages. Sound is bit-identical for the same inputs (placement only); per the rule: own beta, unit measurement, CHANGELOG line with the unit figures.
- **Core 1:** none of the eight/ten moved functions is on core 1's path (they are core 0's mix, sends, master and chain stages); `hot_reach.py` roots include `fm1_c1_main`, so the build check decides. RAM-to-RAM calls always reach, so a core 1 caller would still be fine.
- **Boot copy:** `fm1_cstart` copies X and Z and compares word for word; about 1.9 KB more is a negligible start-up change. The app slot's free space (2,832 B) is unaffected: the code moves from `.text` to a load section of the same image.

## 6. What I did not verify
- C, the associativity, the SFC read-ahead and the CPU clock are unknown; every gain figure depends on C and is a model.
- The classes come from the map (callers, back-branch counts) and from reading the call sites in `fx.c`, `fx_bus.c`, `audio.c`, `twocore_run.c`, `budget.c`. I read the bodies of `fx_buses_m`, `rev_tail`, `rev_locut`, `rev_front` (head), `master_out`, `mix_sends`, `mix_master`, `mix_block`, `dly_wr`, `shed_check`, `click_render` (head), `uac_tap` (head), not all 116. "Active when" is taken from the guard at the call site; re-check the gate before changing a verdict.
- Which master settings (COMP, WIDTH, BASS MONO, SPACE) the owner uses live is not in the repo.
- `work/wt/b3/felucca-base/build/` was rebuilt for this analysis (TIDE on, default flags: `elfcheck` ok, 0 not allow-listed); no source file was touched.
