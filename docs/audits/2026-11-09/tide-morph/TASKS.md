# TIDE and morph audit 2026-11-09: tasks

Briefs for the findings in `REPORT.md`, in the order of what the player gains per cost. Each one is written for a builder who takes it cold.

**Rules for every builder:**
- **Two errors equals agent death:** a second real mistake ends your run, and a fresh agent takes over.
- Work on your own branch off `jam11` and stop your own servers and loops before you report.
- An audio change is measured before and after with the same script, against the pass mark written here.
- The tools are in `work/audit/tide-morph-2026-11-09/tools/`. Each file's header gives its build line.
- `tide_long.c` includes `tr_lib.c`, which is `tests/tide_roles_test.c` with its main renamed:
  ```
  sed "s/^int main(void)/int tr_main(void)/" tests/tide_roles_test.c > build/audit/tr_lib.c
  ```
- Run everything in WSL, from `felucca-base`. Regenerate the tables first with `~/venv/bin/python tools/build.py --gen-only` (the system python3 lacks fontTools, so `ui_icons.h` goes missing).

Sizes below are estimates until built. Every brief checks the device build: image size, pool tail ≥ 8,192 B, RAM ≤ 73,728 B. b5 leaves 4,556 B of flash, and b6 is spending most of it.

<a id="j2"></a>
## J2. JOURNEY started away from A: don't swap the heard end (P2, S, Sonnet)

**Why:** `glide_start` replaces End B with a new factory sound whatever the fader is (`morph_glide.c:168-171`). With the fader near or at B, the sound swaps in one block, switches and FM6 algorithm included. Rendered on M02 under DUB from fader 100 and 127: the worst jump goes from −59 to −21 dBFS (`data/long/f1_fader*.txt`).

**Steps:**
1. In `glide_start`, call `tide_journey` for part k only when `mph[k].fader == 0`. Otherwise the first trip goes to the B that is there, and JOURNEY picks from the first arrival on.
2. Add a host check, in `tests/tide_crossings_test.c` or a new scene of `tide_controls_test.c`: PLAY with TIDE idle and the fader at 100, then JOURNEY GLIDE TO B. End B's sound is unchanged in the launch bar, and the worst second difference over the 120 blocks after the launch stays within 6 dB of a fader-0 launch.

**Acceptance:**
- The new check passes.
- `tidecrossings`, `tidecontrols`, `tidejrnymemory` and `glide` pass.
- About 16 B of flash. Main loop only.

**Owner questions:** none. The sound only stops jumping.

<a id="d1"></a>
## D1. Dynamics on ARP SEQ parts (P2, S, Sonnet)

**Why:** `seq_play_step` sets the TIDE velocity and clears the accent (`seq_morph.c:327-329`). The ARP SEQ path then plays `accent ? 127 : arp_vel(t)` (`seq.c:375`). So on ARP SEQ parts (M02, the first-start multi) ARC and LEAD don't change the level, and accents get quieter.

**Steps:**
1. Add a TIDE hook in `arp_seq_notes` (`seq.c:353-377`). When `seq_play_step` scaled the step (`sqm_auto`, `sqm_dyn`, `sqm_follows`), the velocity becomes `arp_vel(t)` (or 127 for an accent) × the same `VS` factor. Otherwise it stays as today. The plain build keeps today's line (`#else`).
2. In `seq_play_step`, keep the accent information for ARP SEQ: clear `SF_ACCENT` only for non-arp parts, or pass the factor through `buf->vel` and read it in the hook.
3. Extend `tests/tide_motif_test.c` `t_dynamics` with a part on `P_AMODE = AM_SEQ_ID`. Read the velocity `arp_seq_notes` returns, not `s->vel`.

**Acceptance:**
- On M02 under PEAK, 128 bars, the mean voice-start velocity at ARC PEAK is at least 10 % over the mean in BREAK. Read it off the voices: add the velocity to `tide_roles_test.c`'s note scan, or use `tide_long.c`.
- Accented steps at ARC OFF still start at 127.
- `tidemotif`, `tideturns`, `tidedialogue` and `regress` pass.
- About 60 B of flash. One multiply per arp step in the interrupt.

**Owner questions:** none. This is what the guide already promises.

<a id="b1"></a>
## B1. Keep the bass locked: no POLY, no VOIC octave, and POLY ends in BREAK (P2, S, Sonnet)

**Why:**
- POLY shortens any part's loop, the bass included (`seq_poly.c:44, 53`).
- VOIC moves single off-beat notes an octave with no bass guard (`seq_morph.c:184-189`).
- A running POLY stretch cancels ARC's breakdown thinning: `!poly_len[pi]` at `seq_morph.c:309`, and `tarc_hold` (`seq_arc.c:49`) holds no stretch back in BREAK.

**Steps:**
1. Publish `sqm_bassm` (1 bit per part) from the main loop where the category is already read: `sqm_thin_build`, `seq_answer.c`. Use `tide_bass_part` (`tide_ui.c:418`).
2. In `tide_poly_wrap`, `ok &= !((sqm_bassm >> pi) & 1u)`.
3. In `sqm_build`, skip the VOIC octave move when the part is bass or the note is under C3, the pitch walk's rule (`seq_evolve.c:30`). `sqm_build` needs the part index.
4. Add `ph == AP_BREAK` to `tarc_hold`, and end a running stretch at its next loop start when `!calm`.
5. Guide: say in one line that the bass part keeps its loop length and its octave.

**Acceptance:**
- In `tide_roles_test.c` over M01..M16, under STYLE DUB and MINIMAL: no bar where a bass part has `poly_len != 0`, and no bass note start outside the Original's pitch classes and octaves. Add both checks.
- No BREAK bar with a POLY stretch.
- `tidemotif`, `tideturns` and `tidephrase` pass.
- About 65 B of flash and 1 B of RAM.

**Owner questions:** none.

<a id="x1"></a>
## X1. A crossing dip of musical length, not 90 % of the trip (P2, M, Opus: audio)

**Why:** the transition dip runs from 5 % to 95 % of the fader's travel and sits at the floor from 40 % to 60 % (`morph_xfer.c:56-62, 366-370`). Its length therefore scales with TIME. On DUB (16 bars) a trip is about 14 bars dull, and on AMBIENT (64 bars) about 58.
- **Rendered (`data/long_summary.txt`):** mid-trip bars are 1.5-5 dB quieter than the ends and ×0.35-0.6 as bright, on M01, M02, M08, M12 and M13. WANDER styles stay at ×0.86-0.95.
- **The guide** calls it "a short filter dip" (`TIDE_GUIDE.md:49`).

**Steps:**
1. In the main loop (`glide_poll` or `morph_xpoll`), compute per part the arc edges for the running trip: down over at most 2 bars before the switch window (42-58 %), the floor over at most 1 bar around it, and up over at most 2 bars after. Convert bars to fader share from TIME. For TIME ≤ 4 bars the edges equal today's constants. Store them in `mxs[pi]` (8 B).
2. `morph_xarc` reads the per-part edges instead of `XW0`, `XW1`, `XLP0` and `XLP1`. A hand-moved fader keeps today's constants: no glide running means the default edges.
3. Keep `XS0` / `XS1` (the switch side window) inside the floor. `_Static_assert` it, or clamp at run time.
4. Guide line 49: say how long the dip is.

**Acceptance:**
- **Before/after with `tools/tide_long.c`:** 16 multis under their own STYLE, 128 bars. The mid-trip versus end brightness ratio is at least ×0.8 on M01, M02, M08, M12 and M13, and the level dip is at most 1.5 dB. The WANDER multis don't change by more than 0.5 dB.
- `tidecrossings` passes, so the switch flips still land in the floor.
- `glidecpu` and `audiopolish` pass.
- No click: the worst second difference in the bar where the switch flips stays within 6 dB of the run's median. Add that measure to `tide_long.c`.
- About 60 B of flash and 8 B of RAM. The interrupt cost doesn't change: two loads in place of two constants.

**Owner questions:** none. If the dip turns out to mask switch clicks less well on some pair, keep a minimum floor of 1 bar.

<a id="j1"></a>
## J1. JOURNEY picks destinations near the current sound in level and colour (P1, M, Opus: musical design)

**Why:** this is the biggest player-facing issue. JOURNEY (the default STYLE DUB) picks by family, attack class, memory and a contrast pick only (`tide_ui.c:455-499`). Over 128 bars the mix's 8-bar level moves 5-15 dB on 11 of 16 multis under DUB (`data/long_windows.txt`): M02 DUB CHORD goes from −26.2 to −11.6 LUFS and turns from BIG STAB + TIDAL SUB into BELL STAB + RUBBER BAND. Under WANDER the same multis move 1.5-4.4 dB.
- **The destinations really differ in context:** loaded by hand into M02 they span 9 dB (`data/long/swap_*.txt`).
- **On one note they don't:** as single held notes they sit within 3.4 dB, so TRIM levels each sound for one note and not for a pattern.

**Steps:**
1. **Measure.** A gen-time host tool (extend `tests/sound_audit.c`, or a new `tests/jrny_ctx.c`) renders every factory sound in one fixed context: a 2-bar chord stab pattern C3-E3-G3 on beats 1 and 2+, 120 BPM, 8 bars, part 1, STEREO. It writes the K-weighted level (dB, int8) and the brightness (log2 of the centroid, uint8) per sound into `build/gen/duell_jrny_ctx.h`. The build regenerates it like `duell_sounds.h`. Keep it deterministic.
2. **Pick.** In `tide_journey`, after the class and memory filter, rank the candidates by `|Δlevel| + w × |Δbright|` against the sound the part plays now (the end it is leaving). Keep the best 3, or all if fewer, then let today's random pick and `sound_complement_pick` choose among them. This keeps surprise and keeps the parts apart in colour.
3. A USER record has no table row: use its factory parent's row if it has one, else today's pick.
4. Re-run `tools/tide_long.c` for all 16 multis under DUB and AMBIENT.

**Acceptance:**
- Under DUB and under each multi's own JOURNEY style, the 8-bar level range over 128 bars is at most 6 dB on every multi except M10 (near-silent in this harness; check it by ear on the unit), and the brightness range at most ×3.
- `tidejrnyfamily` and `tidejrnymemory` pass: family, class and memory still hold.
- No family is left with fewer than 2 candidates where it had 2 or more.
- About 342 B of flash (2 B × 171 sounds) plus ~60 B of code. 0 RAM. Main loop only.

**Owner question (ask once):** ROADMAP "Decided against" lists "loudness normalisation or harshness classification". This brief changes which sound JOURNEY travels to, not the sound itself. Does that decision cover it? Default if the owner doesn't mind: build it, since it keeps every sound as designed.

<a id="t2x"></a>
## T2X. Widen sound-audit T2 to every FM6 MUSIC roll, and test the default roll (P2, S, with T2)

**Why:** at AMOUNT 40 (what a factory load sets), FM6 MUSIC rolls change the level by −12 to +8 dB, and only 70 % stay within ±4.5 dB (`data/roll_music_a40.txt`). The other five engines stay at 100 %. The cause is the envelopes and `morph_fm_shape` (`data/roll_music_a40_why.txt`). [T2](../sound-vs-base-2026-11-09/TASKS.md#t2) covers three factory End Bs only. The `rolls` group measures SMART (`tests/roll_loudness.c:65`, cfg `MC_STEREO`), not the default MUSIC.

**Steps:**
1. Add an env switch to `tests/roll_loudness.c`, as `tools/roll_music.c` does (`MC_MUSIC` unless `ROLL_SMART` is set). The `rolls` group then runs MUSIC at AMOUNT 20 and 40, and SMART at 20 as today.
2. Give T2's clamp, on the SHAPE dial's decay for FM6 and on `morph_fm_shape`, this acceptance: MUSIC FM6 at AMOUNT 40 has 90 % within ±4.5 dB and 99 % within ±8 dB.

**Acceptance:**
- The `rolls` group passes with MUSIC at AMOUNT 40 included.
- The other engines don't change.
- Tests only, plus T2's own firmware cost.

**Owner questions:** none. SMART at AMOUNT 40 also fails the targets (53.9 % within ±4.5 dB, `data/roll_smart_a40.txt`): note it in KNOWN_LIMITS, no fix proposed.

<a id="r1"></a>
## R1. Free the dead SEQ MORPH history (P3, S, Sonnet)

**Why:** `SQM_HIST 4u /* rolls UNDO can go back over */` (`seq_morph.c:25`). Only `b[top]` is read; UNDO left in b5.

**Steps:**
1. Set `SQM_HIST` to 1u, as morph.c's `MORPH_HIST` did.
2. Drop `q->n` if nothing reads it.
3. Fix the comments at `seq_morph.c:25, 243`.

**Acceptance:**
- RAM down by 276 B. Check the pool tail and RAM in the device build.
- `tidesteps`, `tidekeep`, `tidemotif` and `tidephrase` pass.
- Multi-render golden unchanged.

<a id="c1"></a>
## C1. Two small interrupt savings in `morph_xstep` (P3, S, Sonnet)

**Steps:**
1. Delete `morph_xfer.c:521`, a dead store (`k` is overwritten at `:534`).
2. At `:522`, compare the INSERT dip as `(di >> 7) != (x->dins >> 7) || !di != !x->dins`, so its end still lands exactly at 0.

**Acceptance:**
- `tidecrossings` and `glidecpu` pass.
- `tests/ease_test.c`: blends counted over 1.5 s after a hand jump 0 → 127 with an INSERT type difference drop by at least 3 times.
- Audio of an INSERT crossing within 1 LSB of the old one, except during the dip's own ramp.

<a id="w1"></a>
## W1. WASH-on smoothness (P3, S, Sonnet; WASH is OFF by default)

**Steps:**
1. **Breath and swell on stop.** In `glide_motion`, when `on` is 0, let `mxs[k].motion` decay (`motion -= motion / 8 + 1` per pass) instead of zeroing it (`morph_glide.c:256`).
2. **The two dips.** In `morph_xcut` (`morph_xfer.c:389-394`), compute the breath and the transition dip from the undipped cutoff and take the lower, as the valley does (`:696`).
3. **The level at rest.** Let `wref` follow slowly during motion (`>> 10`), or relearn it at each arrival (`:619-620`).

**Acceptance:**
- WASH MID, an ANALOG low-pass pad, a hand touch at 50 % of a trip: the voices' effective cutoff moves at most 4 dial steps per block, where it can jump about 18 today.
- `tidecrossings` and `glidecpu` pass.
- About 50 B of flash.

## Not briefed yet (measure first; see ROADMAP_ROWS)

- The pattern-polish items F2, F6, F7, F9, F10, F11 and F12 in `lanes/tide-music.md`. Each lane section names its measurement.
- Morph lane F6 (a core-1 race on the overrun path) and F8 (FM6 stairs at TIME 64).
- The doc pass for REPORT finding 13. Docs only; fold it into the next beta's docs commit.
