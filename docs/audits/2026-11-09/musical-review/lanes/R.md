# Lane R: what each TIDE row and function does to the music
Looked at: 1.0.0-b6 (`fork/jam11` fd4b6c6, worktree `work/wt/mus-R`). I built a new host tool, `tools/tide_rows.c`, on the same harness as `tide_long.c` / `tide_roles_test.c`. It plays a factory multi from power-on under STYLE DUB with one row changed and reads, bar by bar, every note each part starts: its step, its real 16th in the bar, its pitch and velocity. It also logs level (LUFS-like), brightness, the ARC phase, the POLY loop length, sqm_lead and the thin masks. 128 bars per run.
- Every row's values on M02 DUB CHORD (ARP SEQ), M08 TINE DUB, M07 MINIMAL 12 (a 12-step motif) and M06 WARM ECHO. That is VARY, TURNS, POLY, OFFBEAT, ARC, WASH, ROLL TYPE / ARP / PITCH, the modes ONCE / LOOP / DRIFT / WANDER, TIME 8 / 64, WAIT OFF / 4, and TARGET SOUND / SEQ.
- ARC, OFFBEAT, TURNS, POLY and VARY on all 16 multis.
- ROLL and PITCH also under DEEP (WANDER).
- Each multi under TARGET SEQ, to give its own sound's level.
- 340 runs; numbers in `data/R_ana.txt`. Scripts: `tools/run_rows.sh`, `sum_rows.py`, `ana_rows.py`, `map_rows.py`.
- Code read: seq_answer, seq_arc, seq_poly, morph_glide (hand takeover), morph_tide (morph_to_sound), tide_ui (STYLE, jl_needs, journey, home, keep), seq_evolve (pitch walk).

Not looked at: hand takeover and keys played during TIDE, KEEP and HOME were read in the code, not rendered. Not looked at: XFADE / SPLIT / PLAY MODE / REC, STYLE MINIMAL, AMBIENT and PEAK as bases, FM6 detail, CPU. Velocity is read off the voices. On ARP SEQ parts (nearly every factory multi) the TIDE dynamics don't reach them yet (known D1), so loudness judgements of ARC and LEAD are made with D1 still open. One harness seed: the arc plan and the coin flips are the same in every run.

## What is good (keep)
- **The motif survives.** Over 128 bars of DUB, every part keeps its register lane. On M02 the chord part stays at 60-82 and the bass at 48-60 (65 at most with ARC DROP). Pitch classes never grow (M02 4 and 2, M08 6 and 3). The two parts start on the same 16th only 13 times in 128 bars on M02 (0.1 a bar), so they don't pile up.
- **The modes are clearly different, and each does what its name says.** ONCE, LOOP and DRIFT change the pattern only on bar 4 of every 4 (M02 bass: 31 of 31 changed bars, 23 for LOOP, 20 for DRIFT). WANDER keeps the multi's level: within 1.5 LU of the multi's own sound on M02 / M06 / M07 / M08. TIME is a real control: TIME 64 against 16 gives a per-bar similarity of 0.46-0.73.
- **The pads stay still, as designed.** M08's pad part changes in at most 5 of 128 bars under any row (0-3 under most).
- **HOME is clean (read).** OCT- + K1 stops the glide, clears JOURNEY's memory and rebases sound and pattern on the Original (`tide_ui.c:44-54`).
- **ARC is the only rhythm row with a large effect:** ARC OFF against RISE changes ≥10 % of a part on 14 of 16 multis (mean similarity 0.71).

## Findings (most valuable first, at most 12)

### R1. JOURNEY turns the bed up by 4-7 dB on half the multis and keeps it there  (HIGH value, MEASURED)
- **What you hear / meet:** with STYLE DUB (the default), M05, M06, M07, M08, M09, M14 and M16 sit about 4-7 dB above the multi as written for most of the set. On M08 TINE DUB the jump comes at bar 8, at the first crossing: −22.7 → −14.1 LUFS. It is part 2's pad: with ALGORITHM on part 2 alone, the level goes −20.1 → −12.0 and stays. The bed ends up louder than the owner set it, against the drums and bass.
- **Evidence:** per-bar LUFS of JOURNEY (TARGET BOTH) minus the same multi under TARGET SEQ (its own sounds, same pattern edits), over 128 bars:

  | Multi | Mean | Bars above +3 LU |
  |---|---|---|
  | M05 | +6.2 | 117 |
  | M06 | +6.8 | 119 |
  | M07 | +4.5 | 108 |
  | M08 | +6.5 | 118 |
  | M09 | +3.5 | 89 |
  | M14 | +5.8 | 122 |
  | M16 | +6.4 | 109 |
  | M01, M12, M13 | +2.0 to +2.5 | 40-53 |
  | M02, M03, M04, M11, M15 | −1.5 to +0.9 | 0-7 |

  It is biased loud, not random. J1's guard is in b6 (`tide_ui.c:456-530`, nearest to the Original's factory row), so this is **known J1 measured worse than recorded**. The guard compares the factory sounds with each other, not with the multi's part as it plays. `morph_to_sound` keeps P_LEVEL (MF_NEVER) but takes the destination's TRIM, switches and routes (`morph_tide.c:132-138`). The multis' own EQ and send settings (for example P_EQLG −12 on M01's DUST BED) are then likely lost. Cause not proven.
- **Recommendation:** a brief to find which values carry the jump (TRIM, EQ, sends). Then keep the multi's mix values through a JOURNEY move, or offset the destination by the measured difference against the Original as the multi plays it. Code, main loop, ≈50-150 B. No interrupt work.
- **How to accept it:** JOURNEY minus own-sound, mean within ±2 LU on all 16 multis, and no multi with more than 16 of 128 bars above +3 LU.
- **Owner's call?** no.

### R2. ARC's breakdown and peak are not heard  (HIGH value, MEASURED)
- **What you hear / meet:** with ARC RISE (DUB), the "thinned breakdown" plays almost the same notes as the rise. On M02 the skank stays whole and the bass loses one hit of four. The peak is no denser or louder. Next to JOURNEY's 7-15 dB crossings (J1 / X1), the arc's own shape is lost.
- **Evidence:** all 16 multis, mean per phase:

  | Phase | Onsets a bar (both parts) | Against ARC OFF, same bars |
  |---|---|---|
  | RISE | 6.42 | −0.18 LU, +0.30 onsets |
  | PEAK | 6.26 | −0.37 LU, +0.11 onsets |
  | BREAK | 5.79 | **−0.74 LU, −0.19 onsets (−3 %)** |
  | RECOVER | 6.08 | −0.02 LU, +0.11 onsets |

  The thin mask drops half of the *optional* hits (`seq_answer.c:37-41`), and a pattern of 2 hits or fewer is left whole. On factory chord parts nearly every hit is an anchor, so little goes. ARC DROP's two-bar silence fired on only 2 of 16 multis in 128 bars (M03 bars 76-77, M04 bar 81). The quieter-breakdown half waits on D1 (ARP SEQ dynamics).
- **Recommendation:** in BREAK and DROP, thin both parts to their anchors and beats (no optional hits). On chord parts also play two voices instead of four (the low and the top), by the same playback-mask route, so KEEP is untouched. Code in `seq_answer.c` / `seq_morph.c`, ≈100-200 B. The interrupt only reads the mask, as now. Land it with D1.
- **How to accept it:** BREAK onsets ≤ 0.65 × RISE, and BREAK ≥ 3 LU under PEAK on M02, M08 and M09 (after D1).
- **Owner's call?** no.

### R3. ARC changes phase on 4-bar marks, not on 8 / 16-bar boundaries  (HIGH value, MEASURED)
- **What you hear / meet:** the breakdown starts at bar 28, the next rise at bar 100, and the peak lasts only 4 bars (RISE 6 phrases, PEAK 1, BREAK 4, RECOVER 1). A house / dub-techno player expects these moves on 8 or 16.
- **Evidence:** of 147 phase changes over 16 multis, 54 fall on a multiple of 8 and 68 fall on bar 4 or 12 of a 16. The other 25 come one bar before such a mark. Phase lengths are counted in single 4-bar phrases: RISE 4-8, PEAK 1 (or 1-2 with DROP), BREAK 1-4, RECOVER 1-2 (`seq_arc.c:24-31`).
- **Recommendation:** count in pairs of phrases (8 bars): RISE 2-4 × 8, PEAK 1-2 × 8, BREAK 1-2 × 8 (the DROP its first 2 bars), RECOVER 1 × 8. That gives an arc of 40-64 bars that starts on a multiple of 8. Data and arithmetic in `tarc_begin`, ≈20 B, main loop.
- **How to accept it:** 100 % of phase starts on bar ≡ 0 (mod 8), and PEAK ≥ 8 bars.
- **Owner's call?** no.

### R4. TURNS changes nothing a player can hear  (HIGH value, MEASURED)
- **What you hear / meet:** TOGETHER, TAKE TURNS and LEAD sound the same on 15 of 16 multis. There is no call and answer.
- **Evidence:** on all 16 multis, TAKE TURNS against LEAD gives similarity 1.00 on 15 (M06 0.93). TOGETHER against LEAD stays ≥ 0.90 on all 16 (mean 0.96). LEAD acts only on a lead / pluck line (`seq_answer.c:22-25`). sqm_lead is set on M06 (part 2) and M07 (part 1) only, and M07's mask is empty, so in practice it acts on M06 alone. There it is ~10 % fewer onsets, with flat velocity (D1). In WANDER / JOURNEY the pattern edits fall evenly on bars 1-4 of the phrase (M02 bass changed bars by position 24 / 30 / 26 / 23), so the variation-bar offset TURNS shifts is not audible. Builds on today's item "TURNS shown live with one part".
- **Recommendation:** take TURNS off the page and play LEAD in every rhythmic style. If a real call and answer is wanted, see "missing" no. 3.
- **How to accept it:** the row is gone and the STYLE table loses a column, or (with no. 3) TOGETHER against the answering value < 0.85 on ≥ 8 multis.
- **Owner's call?** yes: "TURNS does nothing you can hear. Remove the row? (default: remove)".

### R5. OFFBEAT: the three values are nearly the same  (MEDIUM value, MEASURED)
- **What you hear / meet:** the off-beat feel comes from the patterns themselves (the skanks). The row barely adds to it, and PUSH equals SOFT.
- **Evidence:** share of all onsets on the "and" (16 multis × 128 bars): OFF 39.3 %, SOFT 40.9 %, PUSH 41.2 %. PUSH against SOFT gives similarity 0.98, with ≥10 % change on 1 of 16 multis. OFF against SOFT: 0.89, with ≥10 % change on 7 of 16. The skank "and"s are anchors and are never moved (`seq_rhythm.c`).
- **Recommendation:** two values, OFF / ON (ON = SOFT), set by STYLE. Or drop the row and let STYLE set it. The rename to SYNC is already planned. Data / UI only, a few bytes.
- **How to accept it:** one fewer value (or row). No change in the notes for DUB.
- **Owner's call?** yes: "OFFBEAT: keep OFF / ON only? (default: yes)".

### R6. POLY is too rare to hear, and GENTLE and WIDE take the same time  (MEDIUM value, MEASURED)
- **What you hear / meet:** in a 4-minute stretch (128 bars at 122 BPM) a part drifts once for 8 bars, or not at all.
- **Evidence:** bars inside a shortened loop, per 128 bars: GENTLE 0-24 (0 on M06, M12, M16), WIDE 0-28 (0 on M12). WIDE's 40 % chance comes with stretches of half the length (`seq_poly.c:39-55`: GENTLE 15 % × 8 loops, WIDE 40 % × 4). OFF against GENTLE changes ≥10 % of a part on 3 of 16 multis, WIDE against GENTLE on 4 of 16. When it fires on the bass it drifts against the kick (known B1).
- **Recommendation:** fold into STYLE as OFF / ON, with ON on the motif part only (never the bass or pad, B1). Or drop the row. Data, ≈10 B.
- **How to accept it:** the row gone or two-valued, and with ON ≥ 1 stretch per 64 bars on the motif multis (M06, M07, M09).
- **Owner's call?** yes: "POLY: keep it only as part of STYLE? (default: yes)".

### R7. The ROLL rows do nothing in the default mode, and ROLL TYPE secretly switches off the rhythm rows  (MEDIUM value, MEASURED + READ)
- **What you hear / meet:** on DUB (JOURNEY), SMART and WILD sound the same, and ROLL ARP and ROLL PITCH change nothing. Setting ROLL TYPE to SMART or WILD quietly turns TURNS, POLY, OFFBEAT and ARC off.
- **Evidence:**
  - JOURNEY: SMART and WILD give identical output, line for line, on M02 / M06 / M07 / M08.
  - ROLL ARP ON: identical to OFF on 4 multis under JOURNEY and 3 under WANDER (dLUFS 0.00).
  - ROLL PITCH ON: identical notes under JOURNEY and WANDER. The power-on SCALE is CHR (mask fff, printed by the tool), no factory multi sets one, and the walk returns on CHR (`seq_evolve.c:79`).
  - The gate: `jl_needs` asks for "ROLL TYPE MUSIC" (`tide_ui.c:186`). Under WANDER, ROLL TYPE does matter: against MUSIC, the per-bar level differs on average by 1.3-2.6 LU with SMART and 2.8-5.7 LU with WILD, and WILD's brightness differs by 550 % on average on M02.
- **Recommendation:** gate TURNS / POLY / OFFBEAT / ARC on the mode only, not on ROLL TYPE (≈10 B). Move ROLL ARP and ROLL PITCH off TIDE SETTINGS (MENU, or the defaults only). Dim ROLL TYPE while JOURNEY runs, as AMOUNT already is. This is in addition to the planned rename.
- **How to accept it:** with ROLL SMART on DUB, ARC still changes notes (similarity against MUSIC ≥ 0.95). Two fewer rows on the page.
- **Owner's call?** yes: "Move ROLL ARP and ROLL PITCH off the TIDE page? (default: yes)".

### R8. Grab the fader and TIDE stops for good; the arc starts over  (MEDIUM value, READ)
- **What you hear / meet:** a hand on the MORPH fader stops that lane until K1 is twisted again (`morph_glide.c:325-345`). On TARGET BOTH a touch on either part stops both. All automatic pattern edits stop with it (sqm_auto needs the glide, `morph_glide.c:278`). ARC loses its place: tarc.run = 0, so the next start begins at RISE (`seq_arc.c:55-61`). A quick hand sweep in a set freezes the evolving bed.
- **Evidence:** read only; not rendered.
- **Recommendation:** after the hand lets go, the tide resumes by itself from where the hand left it, at the next 4-bar phrase after 8 untouched bars, and ARC keeps its place. Main loop in `glide_poll`, ≈80-120 B. No interrupt work.
- **How to accept it:** a scripted fader touch at bar 20 shows the glide moving again by bar 32, with the ARC phase sequence unchanged against a run with no touch.
- **Owner's call?** yes: "Should TIDE come back by itself 8 bars after you let go of the fader? (default: yes)".

### R9. M02's bass loses its pickup in most bars  (MEDIUM value, MEASURED)
- **What you hear / meet:** on DUB CHORD the bass's 4th hit (step 14, F, the turnaround) is gone in about 2 of 3 bars, and the line flattens to C-C-C.
- **Evidence:** M02 part 2 over 128 bars: +7 onsets, −81. The step-14 hit is missing in, for example, bars 3-4, 64-67 and 100-106, in RISE as well. With ARC OFF it is still −55. The three other hits are anchors (beats, 3/5 rule), so every gap edit takes the one free hit.
- **Recommendation:** a part with at most one non-anchor hit gets no phrase gaps, or a bass-family part gets none (put it into B1's bass mask). ≈10 B, main loop.
- **How to accept it:** M02 part 2 rests ≤ 16 over 128 bars outside BREAK.
- **Owner's call?** no.

### R10. WASH is hardly a row  (LOW value, MEASURED)
- **What you hear / meet:** the notes are identical. LOW and HIGH are a slight darkening, not new space.
- **Evidence:** mean per-bar level change against OFF: LOW 0.1-0.2 LU, HIGH 0.2-0.6 LU. Mean brightness −5 to −13 % with HIGH (M07 1298 → 1134 Hz). The deepest per-bar dip goes 1.5 LU lower on M02. The send swell itself was not separated in this harness.
- **Recommendation:** let STYLE set it (it already does: DEEP LOW, AMBIENT and DRONE HIGH) and drop the row, after X1 and W1 land.
- **How to accept it:** one row fewer.
- **Owner's call?** yes (in the same question as R5 / R6: "fewer rows, STYLE sets them").

### R11. VARY BARS barely matters in WANDER / JOURNEY  (LOW value, MEASURED)
- **What you hear / meet:** under DUB, EVERY BAR and EVERY 4TH are close. Under ONCE / LOOP / DRIFT it works as named.
- **Evidence:** 16 multis, EVERY BAR against EVERY 4TH: mean similarity 0.94, ≥10 % change on 2 of 16. Under ONCE, M02's bass changes only on bar 4 of 4 (31 of 31).
- **Recommendation:** let STYLE set it (MINIMAL and PEAK EVERY BAR) and drop the row.
- **How to accept it:** one row fewer.
- **Owner's call?** yes (the same question).

### R12. KEEP keeps the full pattern, not the breakdown or the drift you heard  (LOW value, READ)
- **What you hear / meet:** KEEP in a breakdown, a LEAD answer or a POLY stretch stores the whole pattern at its stored length and velocities (`tide_tl_keep`, `seq_answer.c:4`). The sound half is kept as heard.
- **Evidence:** read only; the guide says so.
- **Recommendation:** keep as is (it's safe). Optional: during BREAK, KEEP could apply the thin mask. ≈30 B.
- **How to accept it:** n/a.
- **Owner's call?** no.

## What is missing in the algorithm (at most three, no new pages)
1. **Chord-rhythm vocabulary for chord parts.** A small table of 1- or 2-bar cells: the dub off-beat skank with a delay throw on bar 4's last "and", the house 3-3-2 stab, a push into the downbeat, a half-time chord. JOURNEY / WANDER would pick a variation of a chord part from these instead of random gap and move edits. About 16 cells × 4 B + ≈120 B code in `seq_rhythm.c`, main loop.
2. **Harmonic breathing without needing a SCALE.** On 16-bar boundaries the chord part moves its voicing: spread / open in RISE and PEAK, close and low in BREAK, the top voice up an octave at the PEAK. Optionally a 4-bar move to the chord a fourth up in its own quality, which stays clear of the bass machines' root only if the owner's key allows: an owner's-call flag, default off. In `seq_voicelead.c` (VOIC exists), ≈150-250 B.
3. **A real call and answer to replace TURNS.** A motif part plays its variation on bar 2 of each 2-bar pair, and in that bar the other part drops to its anchors. For every role except pad and bass, reusing the thin mask (`seq_answer.c`), ≈80-120 B.

## If I could change three things
1. R1: make JOURNEY keep the multi's level. This is the first thing anyone mixing other gear notices.
2. R2 + R3: an arc on the 8 / 16-bar grid with a breakdown that really thins (after D1).
3. R4-R7, R10, R11: cut TIDE SETTINGS to STYLE, TARGET, ARC, ROLL TYPE (and REC / PLAY MODE / XFADE / SPLIT). TURNS, ROLL ARP and ROLL PITCH go. OFFBEAT, POLY, WASH and VARY become STYLE's job. The gate on ROLL TYPE goes.
