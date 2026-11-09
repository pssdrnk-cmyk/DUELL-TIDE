# Musical review 2026-11-09: tasks

Briefs for the recommendations in [`REPORT.md`](REPORT.md), in the recommended order. Each is written for a builder who takes it cold. The measurements behind every brief are in the lane file it names; read that finding first. Line numbers are for `fd4b6c6` (1.0.0-b6 on `jam11`).

**Rules for every builder:**
- **Two errors equals agent death:** a second real mistake ends your run, and a fresh agent takes over.
- Work on your own branch off `jam11`; one writer per branch. Stop your own servers and loops before you report.
- **Measure before and after with the same tool**, against the pass mark written in the brief. The tools are in the workspace, each with its build line in the header:
  - `work/audit/musical-review-2026-11-09/tools/mlane.c`: every multi's notes, register and energy bands (`run_all.sh`, `run_nokeys.sh`, `table.py`).
  - `work/audit/musical-review-2026-11-09/lanes/S-tools/style_long.c` and `style_stats.py`: a multi under a style for 128 bars; one row changed with `SL_R<i>`, the session seed with `SL_SEED`.
  - `work/audit/musical-review-2026-11-09/tools/tide_rows.c` with `run_rows.sh`, `ana_rows.py`: one row changed at a time, per-bar notes, level, ARC phase.
- Run only the host groups the brief names; CI runs the rest on the pull request. Locally: the device build (image size, pool tail ≥ 8,192 B, RAM ≤ 73,728 B, hot-code check) and `check_docs.py`.
- Run everything in WSL from `felucca-base`; regenerate the tables first with `~/venv/bin/python tools/build.py --gen-only`.
- A change the owner can see (a page, a row) is shown as a picture before tests and paperwork.
- A factory record change regenerates the factory tables and updates the pins that hold factory data (`level`, `deadline`, `tideroles`, `benchrun`): run those groups for any record you touch.
- Add a CHANGELOG-TIDE line in the player's words. Sizes are estimates until built.
- Model: Sonnet unless the brief says Opus.

---

<a id="mr1"></a>
## MR1. DUB plays WANDER; AMBIENT without POLY; three guide lines (S1, S2, S9, M9; HIGH, XS)

**Why:** DUB is the default style and uses JOURNEY. Measured on M02 / M09 / M16 over 128 bars: brightness over 8-bar windows ×6.99 / 3.23 / 2.51, 51 / 43 / 47 % of bars dulled in a crossing, 8 sound pairs. The same style with WANDER: ×2.11 / 1.14 / 2.22, 7 / 0 / 18 % dulled, the multi's own sounds kept, and more pattern movement (M02: 1.27 hits added a bar against 0.63). AMBIENT with POLY GENTLE shifts 30 / 24 of 126 bars against the beat, against its "no groove tricks". Trace: [lanes/S.md](lanes/S.md) S1, S2, S9.

**Steps:**
1. Before: run `style_long` for M02, M09, M16 under DUB and AMBIENT; keep the tables.
2. `tide_ui.c:152`, the `STY` table: DUB's mode `GM_JOURNEY` → `GM_WANDER`; AMBIENT's POLY `1` → `0` (`:155`).
3. Settings stored on a unit: a stored STYLE DUB with the old nine rows must not turn into CUSTOM at the next boot. Read `tide_settings` / the stored-style check near `tide_ui.c:176-180` and make an old DUB (JOURNEY) read as DUB with the new rows. The same for a stored AMBIENT.
4. Docs: `TIDE_GUIDE.md` and `TIDE_QUICK_TABLE.md` style tables and the "Defaults with no saved settings" line (`TIDE_GUIDE.md:89`); the dub recipe says WASH OFF (`:106`); one line "with no notes coming in, TIDE plays in C minor; send or hold one note to set its key" (M9); JOURNEY described as the mode for AMBIENT, PEAK and by hand.
5. After: the same runs.

**Acceptance:**
- M02 under DUB: 8-bar brightness ≤ 2.5, dulled bars ≤ 10 %, 8-bar level range ≤ 3 dB.
- AMBIENT against AMBIENT with POLY OFF by hand: 0 of 126 bars differ.
- A unit with STYLE DUB stored still shows DUB after the update (host check in `tidecontrols` or `mode`).
- `tidecontrols`, `mode`, `tidecrossings` pass; the groups that pin a default-style render are updated on purpose and listed in the report.
- Two bytes of data; flash unchanged apart from step 3.

**Owner questions:** report question 1 (default yes).

---

<a id="mr2"></a>
## MR2. Every part plays with no notes in; the first start moves to BASIC CHORD (M1, M3; HIGH, M)

**Why:** with AUTO TIDE running and no key held, one part is silent in M01, M10, M11, M13, M14, M15, M17, M18 (`data/nokeys_*.txt`): those parts are HOLD ALL with no steps, or a classic arp, and only ARP SEQ parts get TIDE's ghost chord on PLAY (`tide_seq_chord`, `tide_seq.c`). PAD SPLIT is 14.9 dB under the set, the drones 6 to 8 dB. A new unit starts on M02 DUB CHORD (`TIDE_FIRST_MULTI 1u`, `tide.h:179`), a stab over a bass line with 20 % of its energy under 120 Hz; M01 BASIC CHORD has 0.1 %. Trace: [lanes/M.md](lanes/M.md) M1, M3.

**Steps, part A (records; b7):**
1. Before: `run_nokeys.sh`; keep `nokeys_levels.txt`.
2. M01 DUST BED: one tied step-sequencer note, as the roles spec's Appendix A writes it (`steps="n60 - - … ."`, `P_AMODE` OFF), in `tools/factory_multis.py`.
3. M17 and M18 VOID SHIMMER (`tools/drone_pack.py`): ARP SEQ, one step `0` tied over the pattern, GATE 127, at a rate that makes one pattern 2 or 4 bars. A held chord must still set the key.
4. `TIDE_FIRST_MULTI` → `0u` (M01), once step 2 is in. Update the guide's first-start paragraph (`TIDE_GUIDE.md:114`) and QUICKSTART.
5. After: `run_nokeys.sh` and `run_all.sh`.

**Steps, part B (rule; b8, Opus):**
6. While AUTO TIDE plays and a part would wait for keys (HOLD with no steps, or a classic arp with nothing held), give that part the ghost chord, as ARP SEQ parts get it. Main loop, in `tide_seq.c`; a key or MIDI note takes over at once and the ghost notes end without a hung voice. Covers the owner's records M10, M11, M13, M14, M15 without editing them.
7. Host check in `tidecontrols`: M10 with no key, PLAY: both parts start notes within 4 bars; a key press replaces the ghost chord; STOP ends everything.

**Acceptance:**
- Part A: in the no-key run both parts sound in M01, M17, M18, and each is within ±3 dB of the no-key median. The first-start multi has ≤ 3 % of its energy under 120 Hz.
- Part B: both parts sound in at least 16 of 18 multis; every multi within ±3 dB of the median (today 10 of 18).
- The factory-data groups pass (see the rules). Part B about 60-100 B flash; the audio interrupt is not touched.

**Owner questions:** report questions 2 and 5 (default yes to both).

---

<a id="mr3"></a>
## MR3. The multis' data pass: low end before level, clear lanes, spares, clock, swing (M2, M6, M7, M8, M10, M11; S-M)

**Why:** Trace: [lanes/M.md](lanes/M.md), the set table and findings M2, M6-M8, M10, M11. This rides with the roles rows T8 to T10, which already rewrite the reel; read `docs/ROADMAP.md` "Content" and the roles spec first and fold these in, don't build a second reel.

**Steps:**
1. Before: `run_all.sh` and `solo.sh`; keep `table.md`.
2. **M16 FINAL REEL (M2):** BRASS PAD's steps an octave up (the owner's default; the other way is `P_EQLG -12` on part 1). Only then trim its level. It is an owner record: do it only on the owner's yes (question 6).
3. **M15 HAZY TENSOR, M14 EERIE SIGNAL (M2, M7):** to the spares, as the spec's Q1 asks (question 7).
4. **D46:** rewrite the ROADMAP row's default to "register first, then level", and take the level numbers for the multis with an arp part (M10, M11, M13) with a C minor chord held (`idle_*.txt`), not with no keys.
5. **M08 TINE DUB, M09 DEEP 332 (M6):** the pads' steps an octave down (C3 dyads, as the spec writes FOG BANK: `n48+55`).
6. **M17, M18 (M8):** step rates halved (DUNE HORN 1/4 → 1/2, VOID DEEP 1/2 → 1/1), on the owner's yes (question 9).
7. **M05 ORGAN SKANK (M11):** arp swing 56 on part 1.
8. After: the same runs.

**Acceptance:**
- M16: ≤ 3 % of its energy under 120 Hz PLAY alone, then within ±3 dB of the median.
- M08, M09: the parts' p10..p90 registers overlap by ≤ 0 semitones; under 120 Hz stays ≤ 3 %.
- No factory part above 8 onsets a bar over 64 bars of its default.
- With keys held, every multi within ±3 dB of the median.
- At 122 BPM, DUNE HORN's step is about 1 s and VOID DEEP's loop about 31 s.
- The factory-data groups pass.

**Owner questions:** report questions 6, 7, 9 (defaults yes).

---

<a id="mr4"></a>
## MR4. An arc you can hear, on the 8-bar grid (R2, R3, S3; HIGH, M, Opus)

**Why:**
- **Grid (R3):** phase lengths are counted in single 4-bar phrases (RISE 4-8, PEAK 1, BREAK 1-4, RECOVER 1-2; `tarc_begin`, `seq_arc.c:24-31`). Of 147 phase changes over 16 multis, 68 fall on bar 4 or 12 of a 16; the peak lasts 4 bars.
- **Thinning (R2):** the breakdown drops half of the optional hits and leaves a pattern of 2 hits or fewer whole (`seq_answer.c:37-41`); on factory chord parts nearly every hit is an anchor. Measured over 16 multis: BREAK −3 % onsets, −0.74 LU against ARC OFF. A DROP's two-bar silence fired on 2 of 16 multis in 128 bars.
- **Drops (S3):** `tarc_rnd` hashes `morph.seed` (`seq_arc.c:13`), which only a ROLL sets, so the arc is the same every session; with today's seed PEAK's first two arcs go to BREAK: no DROP before bar 184 in 12 of 12 sessions.

Trace: [lanes/R.md](lanes/R.md) R2, R3; [lanes/S.md](lanes/S.md) S3, S4. Build after D1 (dynamics on ARP SEQ parts; `docs/audits/2026-11-09/tide-morph/TASKS.md#d1`), which gives the level half.

**Steps:**
1. Before: `tide_rows` with ARC OFF / RISE / DROP on M02, M08, M09; `style_long` PEAK on M02 and M09 over 12 seeds (`SL_SEED`).
2. Grid: count in pairs of phrases. RISE 2-4 × 8 bars, PEAK 1-2 × 8, BREAK 1-2 × 8 (a DROP is its first 2 bars), RECOVER 1 × 8. Every phase starts on a bar ≡ 0 (mod 8); one arc is 40 to 64 bars.
3. Thinning: in BREAK and DROP both parts play their anchors and beats only (no optional hits), and a chord part plays two voices of four (lowest and top) through the same playback mask, so KEEP and the stored pattern are untouched. A bass part and a pad part keep to the existing rules (and to B1).
4. Drops: under ARC DROP the first arc after PLAY always drops (`tarc_next_of`, `:18`); mix the session seed (`morph.gen`, `morph_new_seed`, `morph.c:2566`) into `tarc_rnd`.
5. After: the same runs.

**Acceptance:**
- 100 % of phase starts on a bar ≡ 0 (mod 8); PEAK ≥ 8 bars.
- BREAK onsets ≤ 0.65 × RISE on M02, M08, M09; after D1, BREAK ≥ 3 LU under PEAK there.
- PEAK style on M02 and M09: a DROP by bar 64 in 12 of 12 seeds; the phase sequences differ between seeds.
- `tidemotif`, `tideturns`, `tidedialogue`, `tidecontrols` pass (ARC pins updated on purpose).
- About 150-250 B flash. Main loop builds the mask; the audio interrupt only reads it, as now.

**Owner questions:** none.

---

<a id="mr5"></a>
## MR5. JOURNEY keeps the multi's level and colour (R1, S5, M4; HIGH under JOURNEY, M, Opus)

**Why:** under JOURNEY (TARGET BOTH) the mix sits above the same multi with its own sounds (TARGET SEQ) by a mean of +6.2 (M05), +6.8 (M06), +4.5 (M07), +6.5 (M08), +3.5 (M09), +5.8 (M14), +6.4 LU (M16), for 89 to 122 of 128 bars; on M08 it jumps −22.7 → −14.1 LUFS at bar 8, from part 2's pad. J1's guard (`tide_ui.c:456-530`) compares factory sounds with each other, not with the part as the multi plays it; `morph_to_sound` keeps `P_LEVEL` but takes the destination's TRIM, switches and routes (`morph_tide.c:132-138`), so a multi's own EQ and send settings may be lost. Brightness still swings ×7 on M02 (`JCTX_MIN 2`, `tide_ui.c:468`, always lets a second, far candidate in). With a chord held, M01 wanders into the low band in 27 of 64 bars (M4, cause not traced). **The cause is not proven: start with the probe.** With MR1 in, this no longer touches the default style. Trace: [lanes/R.md](lanes/R.md) R1; [lanes/S.md](lanes/S.md) S5; [lanes/M.md](lanes/M.md) M4.

**Steps:**
1. Probe (no firmware change): on M08 part 2, log which parameters differ between the multi's part and the part after the first JOURNEY move (TRIM, EQ, sends, level), and which one carries the 8 dB. The same on M01 with C4 Eb4 G4 held for the low band. Write the result into the pull request before any fix.
2. Fix by what the probe shows: keep the multi's mix values (TRIM, EQ, sends) through a JOURNEY move, or offset the destination by the measured difference against the original as the multi plays it.
3. Colour: at most half an octave of brightness from the original, or `JCTX_MIN` 1 (the original itself as the fallback).
4. Before and after: `tide_rows` JOURNEY against TARGET SEQ on all 16 multis; `style_long` DUB-with-JOURNEY on M02.

**Acceptance:**
- JOURNEY minus own-sound: mean within ±2 LU on all 16 multis; no multi with more than 16 of 128 bars above +3 LU.
- M02 under JOURNEY: 8-bar brightness ≤ 2.5.
- M01 under JOURNEY with keys held: 0 bars with over 10 % of the energy under 120 Hz.
- `tidejrnymemory`, `tidecrossings`, `tidecontrols` pass. About 50-150 B flash, main loop only.

**Owner questions:** none.

---

<a id="mr6"></a>
## MR6. Motifs stay whole (S6, R9; MEDIUM, S)

**Why:** DEEP 332's five-hit stab plays whole in 13 of 126 bars under DUB and as 3 hits in 69: the anchor floor allows 3 of 5 (`sqm_anchor`, `seq_rhythm.c`; the rule `max(2, 3/5)`). M02's bass has one free hit, its pickup on step 14, and every gap edit takes it: −81 onsets in 128 bars, gone in about 2 bars of 3. Trace: [lanes/S.md](lanes/S.md) S6; [lanes/R.md](lanes/R.md) R9.

**Steps:**
1. Before: `style_long` DUB on M09 (hits per bar, part 1); `tide_rows` DUB on M02 (part 2 rests).
2. For a figure of 5 hits or more, the kept share goes to 4 of 5.
3. A part with at most one non-anchor hit gets no phrase gaps outside ARC's breakdown. For a bass part, put this into the bass mask of brief B1 (`docs/audits/2026-11-09/tide-morph/TASKS.md#b1`) if that is being built; don't build two masks.
4. After: the same runs.

**Acceptance:**
- M09 under DUB: the whole figure in ≥ 40 % of bars; bars with 3 hits ≤ 25 %.
- M02 part 2: ≤ 16 rests over 128 bars outside BREAK.
- `tidemotif`, `tideroles` pass. About 20 B flash, main loop.

**Owner questions:** none.

---

<a id="mr7"></a>
## MR7. TIDE comes back after a hand (R8; MEDIUM, S-M)

**Why:** a hand on the MORPH fader stops that lane until AUTO TIDE is started again (`glide_touched`, `morph_glide.c:325-345`); with BOTH a touch on either part stops both. The automatic pattern edits stop with it (`sqm_auto` needs the glide, `:278`), and ARC loses its place (`tarc.run = 0`, `seq_arc.c:55-61`). Read in the code, not rendered. Trace: [lanes/R.md](lanes/R.md) R8.

**Steps:**
1. Test first, in `glide` / `tidecontrols`: a scripted fader move at bar 20 of a WANDER glide; today the glide is off at bar 40.
2. After the hand lets go, the lane resumes from where the fader is, at the next 4-bar phrase after 8 untouched bars. A second touch waits again. STOP, a load, HOME and K1 behave as today.
3. While waiting, `sqm_auto` stays on (the pattern keeps evolving) and ARC keeps its phase and count.
4. The AUTO TIDE page shows the waiting state in the existing status line (no new element); show the owner the picture first.

**Acceptance:**
- The glide moves again by bar 32 in the scripted run; the ARC phase sequence equals a run with no touch.
- `glide`, `tidecontrols`, `tidecrossings` pass. About 80-120 B flash, main loop (`glide_poll`).

**Owner questions:** report question 4 (default yes).

---

<a id="mr8"></a>
## MR8. A shorter TIDE SETTINGS page (R4, R5, R6, R7, R10, R11; HIGH for the page, M, Opus)

**Why:** measured over 16 multis × 128 bars under DUB:
- **TURNS:** TAKE TURNS against LEAD identical on 15 of 16; TOGETHER against LEAD ≥ 0.90 similar on all. LEAD acts only on a lead or pluck sound playing a line (`seq_answer.c:22-25`): M06 alone in practice.
- **OFFBEAT:** 39.3 / 40.9 / 41.2 % of hits on the off-beat for OFF / SOFT / PUSH.
- **POLY:** 0 to 24 of 128 bars inside a shortened loop; GENTLE and WIDE spend the same time.
- **WASH, VARY BARS:** 0.1-0.6 LU and 0.94 similarity between their values.
- **ROLL ARP, ROLL PITCH:** output identical to OFF under JOURNEY and WANDER on every multi tried (SCALE is CHR at power-on, and the pitch walk returns on CHR, `seq_evolve.c:79`).
- **The gate:** `jl_needs` (`tide_ui.c:186`) switches TURNS, POLY, OFFBEAT and ARC off unless ROLL TYPE is MUSIC, and the page only says so in knob 1's line.

Trace: [lanes/R.md](lanes/R.md) R4-R7, R10, R11.

**Steps:**
1. **Picture first:** render the page as it would be (STYLE, MODE, TARGET, ARC, ROLL TYPE, REC, PLAY MODE, XFADE, SPLIT) and the AUTO TIDE page, and show the owner before building further.
2. Remove the TURNS row and its logic (the variation-bar offset, `sqm_turns`); every rhythmic style plays what LEAD plays today. The `STY` table loses that column.
3. OFFBEAT, POLY, WASH and VARY BARS stay as STYLE's values (the table keeps them) and are no longer rows. POLY applies to a motif part only, never a bass or pad (with B1). CUSTOM keeps MODE, TIME, WAIT and ARC as its hand values.
4. ROLL ARP and ROLL PITCH leave the page; their stored values keep working (defaults OFF), reachable from MENU if the owner wants them kept.
5. Take ROLL TYPE out of `jl_needs`: the rhythm side acts in WANDER / JOURNEY whatever ROLL TYPE is. Dim ROLL TYPE while JOURNEY runs, where it does nothing.
6. Stored settings: a unit's stored rows for the removed rows are ignored without turning its STYLE into CUSTOM; the settings record keeps its layout (formats are kept).
7. Docs: `TIDE_GUIDE.md`, `TIDE_QUICK_TABLE.md`, QUICKSTART. Drop the planned rename of the SYNC and ROLL rows from the ROADMAP (the rows are gone), and FR11's "TURNS dimmed" step.

**Acceptance:**
- The owner's OK on the picture.
- DUB's notes are unchanged against before on M02, M08, M09 (bit-identical render, apart from POLY's motif-only rule where it applies).
- With ROLL TYPE SMART under DUB, ARC still changes notes (similarity against MUSIC ≥ 0.95).
- A unit with stored settings shows its STYLE after the update.
- `tidecontrols`, `mode`, `ui`, `tideturns` (reduced) pass. Flash goes down; report by how much.

**Owner questions:** report question 3 (default yes; the picture is the check).

---

<a id="mr9"></a>
## MR9. Two multis for the missing roles (M5; MEDIUM, M, Opus; with T9)

**Why:** after the roles spec's reel (T8 / T9) two roles are still missing for house and dub techno: a sparse figure with a texture, and a keys house stab with a held bed. 7 of 18 places are slow pads or drones, with near-twins (WHISPER TIDE / COLD CHAPEL; DUNE HORN / VOID DEEP). Trace: [lanes/M.md](lanes/M.md) M5.

**Steps:**
1. With existing factory sounds only: (a) DUB BLIP on 2 to 3 hits a bar + a TAPE HISS swell, for STYLE MINIMAL; (b) SUNDAY KEYS (#151) on the P04 HOUSE STAB pattern + ROTOR HAZE held at G3 + D4, for STYLE DEEP.
2. Each obeys the roles spec's bounds: ≤ 3 % of the energy under 120 Hz, the two parts' registers apart, ≤ 8 onsets a bar, both parts sounding with no key held, within ±3 dB of the set's median.
3. Where they go is the owner's choice (question 8: the default keeps WHISPER TIDE and COLD CHAPEL, so these wait for a free place or for the 24-multi row U4).

**Acceptance:** `mlane` rows for both inside the bounds of step 2; no two multis share the role pair, BPM band (±6) and chord-part register within 3 semitones. The factory-data groups pass.

**Owner questions:** report question 8.
