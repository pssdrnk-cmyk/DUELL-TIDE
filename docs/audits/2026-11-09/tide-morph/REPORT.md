# TIDE and the morph: design and quality audit (2026-11-09)

Tree: `work/wt/audit-tide`, detached at `4a79206` (b5 head `3fbbc71` plus two ROADMAP-only commits; firmware identical). Read-only: no firmware changed on any branch, nothing pushed. Auditor: Claude (Opus 5.5), plus two read-only reviewer lanes whose top claims I re-read in the code before using them (`lanes/morph-engine.md`, `lanes/tide-music.md`). The measurement tools I wrote are in `tools/`, their outputs in `data/`.

## Verdict

**The engine is good; JOURNEY, the default mode of the default style, is not yet the best we can have.**

- **What works well, measured:**
  - WANDER holds a set steady: 1.5–4.4 dB of slow level movement over 128 bars on every multi tried.
  - The MUSIC roll stays within ±2 dB on five of six engines at AMOUNT 20 and 40.
  - Ends are exact, the motif anchors hold, and the 13 TIDE and morph host groups I ran pass (`data/host_groups_1.txt`, `data/host_groups_2.txt`).
- **What falls short, as a player meets it:**
  - **JOURNEY loudness and colour.** It picks the next sound by family and attack only. On the first-start multi M02 DUB CHORD with STYLE DUB, the mix wanders over **14.6 dB** in 128 bars, and a dub chord and sub bass turn into FROST BELLS and BEHIVE.
  - **The "short filter dip" isn't short.** It covers 90 % of every JOURNEY trip and sits at its deepest for the middle 20 %. On AMBIENT (64-bar trips) the sound is dulled for about 58 bars of each 64.
  - **Dynamics don't reach ARP SEQ parts.** The ARC peak, the breakdown and LEAD answers don't change the level of those parts, and accents get quieter. M02 is such a multi.
- **Not a problem today:**
  - **Clicks.** None are measured in normal trips. One real hard switch exists: starting JOURNEY with the fader not at A. It was rendered, and it's cheap to fix.
  - **CPU.** No proposal below adds audio-interrupt work beyond one multiply per arp step; two save some.
- **Cost of the top five fixes:**
  - Flash: about 600 B.
  - RAM: about 6 B extra, against 276 B freed by finding 8.
  - None of them adds a setting or a page.

## How it was measured

All runs are host renders of the real firmware path (`mix_block`, the main loop between blocks). Details are in `data/`. The tools are:
- **`tools/tide_long.c` (new):** a factory multi played the way the unit runs TIDE: the style's nine rows, TARGET BOTH, ROLL MUSIC, PITCH OFF, GLIDE launched with PLAY. It runs for 128 bars and measures, bar by bar: K-weighted level, peak, a brightness proxy, a jump proxy, both faders and both sounds' names. It's built on `tests/tide_roles_test.c`'s harness.
- **Its two analysis scripts:** `long_windows.py` averages over 8 bars, so the pattern's own bar-to-bar dynamics drop out. `long_summary.py` compares mid-trip bars with bars at the ends.
- **`tools/roll_music.c`:** `tests/roll_loudness.c` with one change, so it measures the MUSIC roll (the player's default). The shipped tool passes cfg `MC_STEREO` only, which makes it a SMART roll.
- **`tests/jrny_pair_audit.c` (existing, in no test list):** 524 JOURNEY pairs, measured with the fader parked at 32, 64 and 96.
- **The sound audit of the same day** (`../sound-vs-base-2026-11-09/data/sound_audit_b5-d4b3c36.txt`) gives each sound's single-note level.

## Findings, ranked by what the player gains per cost

Status words: **NEW**, **KNOWN** (with a link), **EXTENDS** (a known item that is wider than recorded). Evidence is CONFIRMED (read in the code and, where it says so, rendered) or SUSPECT (the mechanism was read, its effect wasn't measured).

### 1. JOURNEY wanders in level and colour: the default set doesn't stay a background (NEW, P1)

**What the player hears:** M02 DUB CHORD, STYLE DUB (the first-start state). Over 128 bars:
- Part 1 goes BIG STAB → DUB DRAWBAR → FROST BELLS → BELL STAB → TUBE BELLS → GLASS HALL → SALOON KEYS → TINE ROOM.
- Part 2 goes TIDAL SUB → LADDER BASS → BEHIVE → RUBBER BAND → TIDAL SUB → FOLD BASS → LADDER BASS → SQUELCH.
- The 8-bar level runs from −26.2 (DUB DRAWBAR + LADDER BASS) to −11.6 LUFS (BELL STAB + RUBBER BAND): **14.6 dB**. Brightness changes up to ×9.

Anyone mixing other gear under it has to ride the fader.

**Evidence:**
- **Under its own STYLE (`data/long_windows.txt`), 8-bar level range over 128 bars:**

  | Style | Mode | Multis |
  |---|---|---|
  | DUB | JOURNEY | M01 7.8, M02 14.6, M08 7.4 dB |
  | AMBIENT | JOURNEY | M12 10.4, M15 11.5, M16 8.4 dB |
  | DEEP / MINIMAL | WANDER | M04 3.8, M05 2.5, M06 4.0, M07 1.8, M09 3.8 dB |

- **Same multis under DEEP (WANDER):** M01 2.6, M02 3.4, M05 2.5, M08 1.5, M12 4.4 dB (`data/long/deep_*.txt`). DEEP has ARC RISE like DUB, so ARC is not the cause.
- **Under DUB:** 11 of 16 multis move 5 dB or more, M10 not counted (it is near-silent in this harness).
- **The destinations really differ in that context.** I loaded them into M02 by hand, through the PATCHES-browser path (`set_engine_of` + `apply_preset_to`, no TIDE). They range from −26.4 to −17.3 LUFS (9 dB) and from 298 to 1,601 Hz brightness (`data/long/swap_*.txt`). Played as single held C3 notes, the same sounds sit within 3.4 dB of each other (sound audit: BIG STAB −16.9, FROST BELLS −17.9, BELL STAB −17.9, DUB DRAWBAR −18.3). The factory TRIM levels each sound on one held note, which doesn't carry over to a chord pattern: a bell rings through the gaps, a stab doesn't.
- **The pick reads only the family, the attack class, the memory and a contrast pick.** `tide_journey`, `tide_ui.c:455-499`; the class is `tide_cls`, `:455`. Nothing in the pick reads level or brightness.

**Fix (brief J1):** let JOURNEY prefer the candidates nearest the current sound in a gen-time table: each factory sound's level and brightness, measured in a fixed chord-pattern context. Pick randomly among the nearest 3, so it still surprises.
- **Cost:** flash ~342 B of table (2 B × 171) plus ~60 B of code. Main loop only, 0 RAM, no audio-interrupt work.
- **Note (owner question in J1):** ROADMAP "Decided against" lists "loudness normalisation or harshness classification". This changes which sound is chosen and leaves the sound itself alone; the owner should say whether that decision covers it.

### 2. The crossing dip spans the whole trip, not "a short dip" (NEW, P2)

**What the player hears:** on every JOURNEY trip between sounds that differ in any switch (most pairs), the filter closes from 5 % of the trip, is fully down from 40 % to 60 %, and reopens by 95 %. On DUB (16 bars) that's about 14 dull bars per trip, 3 of them at the floor. On AMBIENT (64 bars) it's about 58 dull bars, 13 at the floor. The floor is 1.5 times the highest note.

**Evidence:**
- The arc is `XW0` 5 % … `XW1` 95 % with the floor `XLP0` 40 % … `XLP1` 60 % (`morph_xfer.c:56-62`, `morph_xarc` `:366-370`). `tp` follows the fader (`morph_xstep`, `:467-498`), so the dip scales with TIME.
- With WASH OFF (DUB's value) the dip stays: "OFF: plain glides, a transition still smoothed (its filter dip …)" (`:374`).
- **Rendered (`data/long_summary.txt`), mid-trip bars against end bars:**

  | Style | Mode | Multis |
  |---|---|---|
  | DUB | JOURNEY | M01 −2.0 dB ×0.57 brightness; M02 −1.5 dB ×0.35; M08 −2.5 dB ×0.41 |
  | AMBIENT | JOURNEY | M12 −4.9 dB ×0.55; M13 ×0.48 |
  | DEEP | WANDER | M04 ×0.95, M05 ×0.92, M09 ×0.86 (no dip: MUSIC rolls set no switches) |

- With the fader parked, 175 of 524 JOURNEY pairs dip more than 4 dB under the quieter end (median 7.2 dB, worst 38 dB) (`data/jrny_pairs_all.txt`). The parked case is KNOWN as MORPH W1 / c2 ([ROADMAP MORPH c2](../../wt/audit-tide/docs/ROADMAP.md)). The moving case is this finding.
- The guide calls it "a short filter dip" (`docs/TIDE_GUIDE.md:49`).

**Fix (brief X1):** cap the dip by musical time, not by a share of the trip. A main-loop helper sets each part's arc edges from TIME and BPM (at most about 2 bars down, the floor about 1 bar around the switch point 42-58 %); `morph_xarc` reads them. Short TIMEs (≤ 4 bars) keep today's shape.
- **Cost:** ~60 B flash, 8 B RAM. The interrupt reads two fields instead of two constants.
- **Accept by:** `tidecrossings` and `glidecpu` still pass, and the switch flips still land inside the floor.

### 3. TIDE's dynamics never reach ARP SEQ parts and take their accents away (NEW, P2; from the music lane, re-read)

**What the player hears:** on an ARP SEQ part (M02 DUB CHORD's two patterns play this way), the ARC peak isn't louder, and the breakdown and LEAD answers aren't softer. In every phase with dynamics on, an accented step drops from 127 to the arp's velocity (100 with no key held).

**Evidence:**
- `seq_play_step` writes `buf->vel` and clears `SF_ACCENT` (`seq_morph.c:327-329`).
- The ARP SEQ path reads the step through the same hook, then takes `*vel = (s->flags & SF_ACCENT) ? 127u : arp_vel(t);` (`seq.c:357, 375`). `arp_vel` is the key velocity or 100 (`seq.c:300`).
- The guarding test runs a plain-sequencer part (`tests/tide_motif_test.c`, lane note).
- CONFIRMED in code, not rendered.

**Fix (brief D1):** in `arp_seq_notes`, take the velocity from the TIDE step when dynamics apply, and keep the accent as 127 × the factor.
- **Cost:** ~60 B flash, 0 RAM, one multiply per arp step in the interrupt.

### 4. Starting JOURNEY with the fader not at A swaps the sound in one step (NEW, P2; from the morph lane, rendered by me)

**What the player hears:** park the fader at B or near it (after a ONCE trip, a hand move, PERFORM's B key), then start GLIDE TO B. End B is replaced at once by a new factory sound while you're hearing it, so the switches and FM6's algorithm flip in one block.

**Evidence:**
- `glide_start` calls `tide_journey` whatever the fader is (`morph_glide.c:168-171`).
- `morph_ramp` smooths only the blended values (`morph.c:1985-1992`). Switch values take the side's value at once (`morph.c:1515`).
- **Rendered:** M02 under DUB, PLAY with TIDE idle and both faders at 100 or 127, GLIDE at bar 4. Within the same bar the sounds become DUB DRAWBAR + RUBBER BAND, and the worst sample-to-sample jump rises from about −59 dBFS (the fader-0 launch) to −24 / −21 dBFS within 8-15 blocks (`data/long/f1_fader*.txt`). From 127 the fader then has nowhere to glide: the change is the swap itself.

**Fix (brief J2):** replace End B at launch only when the fader is at 0. Otherwise the first trip goes to the B that's there.
- **Cost:** ~16 B flash, main loop.
- **It also removes the main trigger of the frozen-plan case** (lane F2, `morph_xfer.c:274-277`).

### 5. The bass isn't fully protected: POLY shortens it, VOIC moves its notes an octave (NEW, P2; music lane F3 + F4, re-read)

**What the player hears:** with POLY on (DUB, MINIMAL, AMBIENT, PEAK), the bass part takes its turn at a 12-15-step loop and drifts against an external kick for 4-8 loops. The VOIC dial also moves single off-beat bass notes an octave up or down. The guide's bass protection is worded for the pitch walk only ("never a bass note under C3 or in a part whose sound is in the BASS category", `TIDE_GUIDE.md:64, 152`). A player reads it as "TIDE leaves my bass alone", but POLY and VOIC still act on the bass.

**Evidence:**
- POLY: `int ok = mode && len >= 8u && ((sqm_auto >> pi) & 1u) && song.playing && !song.rec;` has no role test (`seq_poly.c:44`). `tide_bass_part` exists (`tide_ui.c:418-421`) but only the pitch walk uses it (`seq_evolve.c:77`).
- VOIC: `out[i].note[0] = sqm_note(out[i].note[0] + (d[SD_VOIC] > 0 ? 12 : -12));` for any single-note off-beat hit (`seq_morph.c:184-189`), with no bass guard.

**Fix (brief B1):** one bass mask, set from the main loop, read by both.
- **Cost:** ~55 B flash, 1 B RAM, one bit test per loop in the interrupt.

### 6. A POLY stretch cancels ARC's breakdown thinning (NEW, P3; music lane F5, re-read)

- **Evidence:** thinning is skipped while a stretch runs: `!poly_len[pi]` (`seq_morph.c:309`). New stretches are held only at PEAK and DROP (`seq_arc.c:49`), and a running stretch ignores that (`seq_poly.c:46-51`).
- **Fix (in brief B1):** add BREAK to the hold, and end a running stretch at its next loop start. ~10 B.

### 7. FM6 MUSIC rolls at AMOUNT 40 leave the loudness window (EXTENDS known T2)

**What the player hears:** on FM6, a ROLL, a DRIFT or a WANDER step at AMOUNT 40 changes the level by −12 to +8 dB. Only 70 % of rolls stay within ±4.5 dB. Every other engine stays at 100 %, within −4.1 to +3.0 dB (`data/roll_music_a40.txt`). AMOUNT 40 is what a factory load sets (MORPH c3).

**Evidence:**
- The per-group breakdown (`data/roll_music_a40_why.txt`) puts it on the envelopes (the SHAPE dial's decay) and on the FM6 bytes (`morph_fm_shape`). Examples: HAN SOLO −12.0 dB, back to +0.2 without the FM6 bytes; TEMPLE BLOCK −8.1, which the envelopes take to −20.2 when reverted.
- [Sound audit T2](../sound-vs-base-2026-11-09/TASKS.md#t2) covers three factory End Bs only. Its clamp should cover every FM6 MUSIC roll.
- **Test gap:** the `rolls` group measures SMART (cfg `MC_STEREO`, `tests/roll_loudness.c:65`), not the default MUSIC. At AMOUNT 40, SMART itself fails the owner's own targets: 53.9 % within ±4.5 dB (`data/roll_smart_a40.txt`).

**Fix:** widen T2's acceptance to `roll_music` at AMOUNT 40, and add the MUSIC variant to the `rolls` group (brief T2X, tests only).

### 8. SEQ MORPH still keeps a 4-deep history nobody reads (NEW, P3; music lane F8, re-read)

- **Evidence:** `#define SQM_HIST 4u /* rolls UNDO can go back over */` (`seq_morph.c:25`). Only `b[top]` is read (`:210, 262, 268`).
- **Fix (brief R1):** `SQM_HIST 1u` frees **276 B of RAM** (3 × 46 B × 2 parts) and a little flash. The sound side made the same change in b5.

### 9. Wasted interrupt work in `morph_xstep` (NEW, P3; morph lane F5, re-read)

- **Evidence:**
  - `k = x->need ? (uint32_t)morph_xdepth(tp) : 0u;` (`morph_xfer.c:521`) is a dead store: `k` is overwritten by the loop at `:534` before anything reads it.
  - The INSERT dip's trigger compares `di != x->dins` exactly, where the other two compare at 1/256 (`:522`). After a hand jump with an INSERT difference, that forces the full blend nearly every block for about 0.4 s (lane estimate, not measured).
- **Fix (brief C1):** delete the dead line, and compare `di >> 7` while keeping the exact landing at 0.
- **Cost:** −8 to +6 B. Saves interrupt time.

### 10. WASH-on rough edges (NEW, P3; morph lane F3, F4, F7; WASH is OFF by default)

- **F3:** a hand touch, K1 or a load during a trip drops the breath and swell to 0 in one pass (`morph_glide.c:256, 281, 323`). The voices' filter, which `morph_xfer.c:390-392` doesn't slew, opens by up to 2 octaves in one block. Fix: let `motion` decay (~24 B).
- **F4:** the breath and the transition dip stack in `morph_xcut` (`morph_xfer.c:389-394`), while the valley takes the larger of the two (`:696`). Fix: take the lower cutoff (~16 B).
- **F7 (SUSPECT):** with WAIT OFF the wash never relearns its level at rest (`:619-620`).
- **Status:** all CONFIRMED in code, not rendered. Brief W1.

### 11. More pattern-logic items, lower priority (NEW; music lane, read by the lane, spot-checked by me)

- **F2 (SUSPECT size):** with VARY EVERY 4TH, bars 1-3 jump two generations at every second arrival (`seq_morph.c:288, 397-421`). They skip the variation bar 4 has been growing into. Measure first (lane M2).
- **F6:** phrase edits are counted in loops (`seq_arc.c:119`), so an 8-step pattern changes twice as often and on any bar.
- **F7:** on a 1/8 grid the motif rules count steps, not 16ths (`seq_rhythm.c:24-41`).
- **F9:** DENS can retrigger a pad of two chords a bar (`seq_morph.c:120`).
- **F10:** with ROLL PITCH ON the top voice drifts with no pull home (`seq_evolve.c:3-4`).
- **F11 (SUSPECT):** JOURNEY's attack class reads FM6's AMP ENV offsets, not the patch's own attack (`tide_ui.c:469, 475`).
- **F12:** WANDER + BOTH ties FM6 hits over up to 7 free steps (`morph_glide.c:99-103`), filling the room left for other gear.
- **Status:** each is 4-100 B. They're grouped in ROADMAP_ROWS as one "pattern polish" row, with no brief until measured.

### 12. Rare or unproven engine items (morph lane, SUSPECT)

- **F6:** core 1 can still be inside part 2's `morph_apply` when the main loop rewrites `mph[1]`, after an overrun (`twocore_run.c:693`). Only `morph_xreset` guards it (`morph_xfer.c:353`). A morphed value could then stick in part 2's saved patch.
- **F8:** possible audible stairs in FM6 levels at TIME 64.
- **Status:** a row each, measure first (lane M5).

### 13. Docs that don't match the code (NEW, P3)

- `TIDE_GUIDE.md:49` says "a short filter dip"; the dip spans 90 % of a trip (finding 2).
- `TIDE_GUIDE.md:23`: MORPH knob 3 "Nothing for now (a picture of the sound's wave is planned)". The picture shipped in b5.
- `TIDE_GUIDE.md` dub-techno recipe: "WASH LOW" by hand, while STYLE DUB sets WASH OFF (`tide_ui.c:152`).
- TIDE_QUICK_TABLE says AMBIENT has "no groove tricks", yet it sets POLY GENTLE.
- "WANDER: a little further each trip": the dials bounce at AMOUNT's edge (`morph_music.c:86-89`), and the step size comes from the phrase boundary crossed.
- The bass protection is described only for the pitch walk; POLY and VOIC aren't mentioned (finding 5). The dynamics paragraph doesn't hold on ARP SEQ (finding 3).
- Stale comments: `seq_morph.c:25, 243` (UNDO).

## Already known, not repeated

These were confirmed where touched, but not re-reported:
- MORPH W1 / c2: a parked fader stays dull.
- c3: AMOUNT in JRNY.
- UNDO removed in b5.
- "Transition costs only while moving" (ROADMAP Later 2).
- Full audit 2026-11-08 findings 4, 6, 7, 8, 9, 12, 34, 35, 36.
- CPU audit K7 and K8 (FM6 crossing cost, morph snapshot off the interrupt).
- Sound audit T2 (FRETLESS / ISLAND STEEL End B), extended by finding 7.
- `tideroles` levels (D46).
- Roles T4 and `sqm_note`.
- The SYNC / ROLL row names.

## Keep as is

- **Ends:** every end is computed from the Original, so a long set can't run away (`morph_music.c:16-18`, `seq_morph.c:11-13`). End A and End B are exact at 0 / 127.
- **Arrivals:** an end is replaced only while it's silent (except finding 4).
- **The blend cache** and its range-edge check.
- **The valley's safety:** 1/24-octave slew, +9 dB make-up cap.
- **The motif:** one shared floor (`sqm_anchor`) and the ±2-step cap.
- **LEAD** answers only on lead / pluck lines.
- **ARC** pauses on STOP.
- **JOURNEY's role logic:** engine kept, never the other part's sound, a 4-deep memory, the FM6 key kept. Finding 1 adds to it; nothing replaces it.
- **The MUSIC roll** on the five non-FM6 engines: ±2 dB at AMOUNT 40, 1,500 rolls.
- **WANDER's steadiness.**

## What was run

| What | Result |
|---|---|
| `sh host_tests.sh` glide, morph, tidecrossings, tidesteps, tidemotif, tideturns, tidedialogue, tidephrase, tidejrnyfamily, tidejrnymemory, tidecontrols, tidekeep, tidevoicelead (WSL, tables regenerated with `~/venv/bin/python tools/build.py --gen-only`) | all PASS |
| `jrny_pair_audit 4` | 524 pairs, 189 fail (175 dips) |
| `roll_music` at AMOUNT 20 and 40, MUSIC and SMART; ROLL_WHY at 40 | see finding 7 |
| `tide_long`, 16 multis × (own STYLE, DUB) × 128 bars; 5 under DEEP; 6 hand-load swaps on M02; 3 F1 launches | see findings 1, 2, 4 |

**Not run:**
- Device build and anything on the unit. Proposals are costed by estimate; each brief measures its own size.
- `tideroles`: known red, D46.
- The music lane's probes M1-M5 (in `lanes/tide-music.md`).
