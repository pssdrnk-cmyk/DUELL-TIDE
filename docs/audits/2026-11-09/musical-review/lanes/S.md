# Lane S: the TIDE styles
Looked at: the six styles of 1.0.0-b6 (`fork/jam11` fd4b6c6, worktree `work/wt/mus-S`), rendered on the host. Four multis: M02 DUB CHORD (first start), M09 DEEP 332 (swung sequence), M16 FINAL REEL (pads), M17 DUNE HORN (drone). Each one ran under all six styles and as its Original (PLAY alone, TIDE idle) for 128 bars: 28 runs. Then 27 single-row variants (one row changed by hand, on M02 / M09 / M16), PEAK and DUB over 12 sessions (seeds), and PEAK for 640 bars.
Tool: `S-tools/style_long.c`. It is `tide_long.c` (tide-morph audit) plus, per bar, each part's notes with their 16th position in the bar. It runs the same way as the unit: the style's nine rows, TARGET BOTH, ROLL MUSIC, PITCH OFF, glide launched with PLAY. Env `SL_R<i>` changes one row, `SL_SEED` sets the session seed. Analysis: `S-tools/style_stats.py` → `S-data/style_stats.txt` (all tables below). Bars 0-1 are left out.
Not looked at: M18; the multis with a held chord (M11, M13-M15); PITCH ON; TARGET SOUND/SEQ; the unit. M17 under a style other than DRONE is a harness artefact (the drone hook rewrites the rows after launch), and the unit never plays it that way, so it is left out. Minutes are given for 124 BPM: 1 bar = 1.94 s.

## What is good (keep)
- **No style is too busy or too loud to sit behind drums and bass.** The 8-bar onset mean peaks at 8.6 a bar, against 7.0 for the Original (M09). The mean level stays within −1.6 to +2.7 dB of the Original, except JOURNEY on the quiet M16 (+5.6 to +6.6 dB; M16's level is known, D46). Peaks stay at −1.7 dBFS or lower.
- **TIDE never seems off.** In every style and multi, the longest run where neither the notes nor the faders change is 8 bars (15 s) or less.
- **DUB keeps the dub feel.** On M02, 33 % of the hits land on quarter-note 16ths, the same as the Original (the off-beat stays off the kick). TIDE takes away more hits than it adds (0.98 against 0.46 a bar), so it makes space.
- **WANDER is a steady bed.** DEEP, MINIMAL and DRONE move 1.8-3.8 dB in level over 8-bar windows, and brightness by ×1.1-2.3. Under 27 % of bars sound dulled (on M02 / M09: 0-12 %).
- **The level half of J1 landed in b6.** M02 under DUB now moves 3.8 dB over 8-bar windows. The b5 report measured 14.6 dB.
- **DRONE on M17 is always moving.** The horn line never changes (0 % of bars, as designed), but the sound does: 2.9 dB, brightness ×1.7, 59 % of bars mid-trip, never more than 4 still bars.
- **The styles are distinct.** On 12 z-scored figures, the closest pair is DRONE–MINIMAL at 2.09. The other pairs sit at 2.3-4.6 (table under S7). None is redundant.

## Findings (most valuable first, at most 12)

### S1. The first-start style changes the sound every 35 s and dulls half the bars: DUB's JOURNEY works against a background bed  (HIGH value, MEASURED)
- **What you hear / meet:** a new unit plays M02 under DUB. Every ~18 bars (35 s) each part becomes another sound: BIG STAB → TUBE BELLS → TINE ROOM → SALOON KEYS …, TIDAL SUB → LADDER BASS → BEHIVE → SQUELCH. Brightness swings ×7, and half the bars sound dulled in a crossing.
- **Evidence (128 bars, `style_stats.txt` + `var/`):**

  | M02 / M09 / M16 | 8-bar brightness max/min | bars dulled | longest dull run | sound pairs | 8-bar level range |
  |---|---|---|---|---|---|
  | DUB as shipped (JOURNEY 16, WAIT 2) | 6.99 / 3.23 / 2.51 | 51 / 43 / 47 % | 12 / 16 / 9 bars | 8 / 8 / 8 | 3.8 / 4.5 / 8.4 dB |
  | DUB with WANDER (one byte) | 2.11 / 1.14 / 2.22 | 7 / 0 / 18 % | 2 / 0 / 1 | 1 / 1 / 1 | 2.3 / 3.3 / 3.7 dB |
  | DUB, JOURNEY at TIME 32 / WAIT 4 | 5.06 / 4.31 / 4.25 | 52 / 33 / 40 % | 25 / 16 / 9 | 5 / 5 / 5 | 3.9 / 5.9 / 8.3 dB |

  WANDER also moves the pattern more: M02 adds 1.27 and rests 1.06 hits a bar, against 0.63 and 0.99. The movement moves into the algorithm, which is where the owner wants it. A longer JOURNEY trip is no fix: it makes the dull runs longer (12 → 25 bars on M02) until X1 lands.
- **Recommendation:** STY[0] mode `GM_JOURNEY` → `GM_WANDER` (`tide_ui.c:152`, one byte). Keep DUB as the default style: its other eight rows fit (see "What is good"). JOURNEY stays in AMBIENT and PEAK, and on the MODE row for whoever wants it.
- **How to accept it:** M02 under DUB: 8-bar brightness ≤ 2.5 (from 7.0), dulled bars ≤ 10 % (from 51 %), 8-bar level range ≤ 3 dB (from 3.8).
- **Owner's call?** yes. "A new unit's DUB keeps the multi's own sounds and moves only their settings and the pattern (WANDER), instead of travelling to other sounds (JOURNEY)? Default: yes, WANDER."

### S2. AMBIENT plays polymeter tricks: POLY GENTLE moves a fifth of its bars  (MEDIUM, MEASURED)
- **What you meet:** AMBIENT promises no groove tricks. Yet now and then a part's loop runs 14-15 steps against the bar.
- **Evidence:** AMBIENT with POLY GENTLE against POLY OFF: the notes or their positions differ in 30 / 24 / 0 of 126 bars on M02 / M09 / M16. Under DUB it is 16 / 16 / 0. So the 9 November doc note matters in the numbers: about 1 bar in 5.
- **Recommendation:** STY[3] POLY 1 → 0 (one byte). That also matches TIDE_QUICK_TABLE's "no groove tricks".
- **How to accept it:** AMBIENT against AMBIENT-with-POLY-OFF: 0 of 126 bars differ.
- **Owner's call?** no.

### S3. PEAK has no drop for the first 6 minutes, and the arc is the same every night  (MEDIUM, MEASURED)
- **What you meet:** PEAK promises drops. In the first 128 bars (4.1 min) none comes, in 12 of 12 sessions on M02 and M09. The first DROP is at bar 184 (5.9 min). Over 640 bars, 5 of the 12 arcs drop. The arc's phase sequence (PEAK at bar 24, BREAK at 32, RISE again at 56 …) is identical across all 12 seeds, while the notes do differ.
- **Evidence:** `S-data/seeds/` and the 640-bar run (the ARC phase per bar). `tarc_rnd` hashes `morph.seed` (`seq_arc.c:13`). That is set only by a ROLL (`morph.c:2583`, `:2670`), not by the session seed (`morph_new_seed`, `:2566`). The identical sequences across 12 seeds show that TIDE's playing never changes it. So the dice are the same every night, and with today's seed the first two PEAK phases go to BREAK (`tarc_next_of`, `:18`).
- **Recommendation (code, main loop, ~10-20 B, no audio interrupt):** (a) under ARC DROP, the first arc after PLAY always drops (`tarc_next_of`: `tarc.cnt <= 2u ||`). (b) Mix the session seed (`morph.gen`) into `tarc_rnd`, so the arcs differ from night to night the way the notes already do.
- **How to accept it:** PEAK on M02 and M09: a DROP by bar 64 in 12/12 seeds. The phase sequences differ between seeds.
- **Owner's call?** no.

### S4. The ARC peak and breakdown barely show in level: they depend on D1  (MEDIUM, MEASURED)
- **What you meet:** DUB, DEEP and PEAK have ARC on. One arc is 48-56 bars (1.6-1.8 min): the first PEAK at bar 24 (46 s), the BREAK from bar 28-32. The breakdown thins the music: 5.0-5.9 onsets a bar against 6.2-7.0 at the peak (−16 to −20 %). But the peak is not louder than the breakdown.
- **Evidence (mean LUFS, PEAK phase − BREAK phase):** M02: DUB −0.1, DEEP +1.8, PEAK −0.2 dB. M09: −1.2, +0.8, +0.5. M16: +1.0, +1.8, +2.0. M02 and M09 are ARP SEQ multis, where ARC's dynamics don't arrive: that is known D1. Under JOURNEY the sound changes swamp the ARC as well (S1).
- **Recommendation:** none new. PEAK and DUB depend on D1 landing. After D1, re-measure. If the peak is still within 2 dB of the breakdown on M02, raise ARC's level lever.
- **How to accept it:** PEAK phase − BREAK phase ≥ +2 dB on M02 and M09 under DEEP and PEAK.
- **Owner's call?** no.

### S5. J1's colour part doesn't hold on the first-start multi: the brightness still swings ×7  (MEDIUM, MEASURED; only if DUB stays JOURNEY or for AMBIENT / PEAK)
- **What you hear:** under DUB on M02, the mix goes from ~230 Hz (BIG STAB / TIDAL SUB) to 1,000-1,700 Hz (TUBE BELLS bars 14-20 and 32-38, SALOON KEYS bars 104-110). That is a dull stab turning into ringing bells.
- **Evidence:** the per-bar brightness proxy in `S-data/m2_s0.txt`: 8-bar ×6.99. The level half of J1 works (3.8 dB, see "What is good"). `JCTX_MIN 2` (`tide_ui.c:468`) always lets a second, far candidate in.
- **Recommendation:** for the J1 owner: at most half an octave of brightness from the Original, or `JCTX_MIN` 1 (the Original itself as the fallback). This is a known item, not fixed yet. If S1 is taken, it only matters in AMBIENT and PEAK.
- **How to accept it:** M02 under DUB with JOURNEY: 8-bar brightness ≤ 2.5.
- **Owner's call?** no.

### S6. DEEP 332's swung 3-3-5-3-2 figure is rarely heard whole under TIDE  (MEDIUM, MEASURED; the pattern engine, every style)
- **What you hear:** the soul-stab comping, the multi's identity, mostly plays 3 of its 5 hits.
- **Evidence (bars by hits per bar, part 1, of 126):** Original: 5 hits in 125 bars. DUB: 3 hits in 69, the whole 5 in 13. MINIMAL: 3 hits in 65, 5 in 17. DEEP: 3 hits in 54, 5 in 28. AMBIENT (no ARC): 3 hits in 30, 5 or more in 75. The anchor floor allows 3 of 5 (`max(2, 3/5)`, tide_roles_test). The pad part is never touched. Rests outnumber adds in every style (1.4-2.0 against 0.6-1.1 a bar).
- **Recommendation:** for STAB / MOTIF roles of 5 hits or more, the floor goes to 4/5 (code: the anchor rule `sqm_anchor`, a few bytes). The other choice is fewer rests in DUB and MINIMAL. Check with the multis lane: this may overlap.
- **How to accept it:** M09 under DUB: the whole figure (5 hits) in ≥ 40 % of bars (from 10 %). Bars with 3 hits ≤ 25 % (from 55 %).
- **Owner's call?** no.

### S7. No two styles are too alike, but one house style is missing  (LOW, MEASURED + OPINION)
- **Evidence (distance on 12 z-scored figures, mean over M02 / M09 / M16):**

  | | DUB | DEEP | MINIMAL | AMBIENT | PEAK | DRONE |
  |---|---|---|---|---|---|---|
  | DUB | 0 | 3.93 | 4.56 | 3.08 | 4.31 | 4.02 |
  | DEEP | | 0 | 3.32 | 3.65 | 3.92 | 2.33 |
  | MINIMAL | | | 0 | 3.55 | 3.59 | 2.09 |
  | AMBIENT | | | | 0 | 3.93 | 3.25 |
  | PEAK | | | | | 0 | 3.51 |

  DRONE on a groove multi plays like a slower MINIMAL / DEEP (2.09 / 2.33). It earns its place on M17 / M18, where it is set by itself.
- **Owner's list against the six:** Dub → DUB / DEEP. Ambient → AMBIENT / DRONE. Techno → MINIMAL / PEAK. **House** has no style: busy chord stabs that keep the sound, push the off-beat and build. Acid needs a squelch sound, not a style row. Berlin school is a sequencer arp idea, a ROLL ARP matter: OPINION.
- **Recommendation:** if S1 makes DUB WANDER, no new style is needed now. If DUB keeps JOURNEY, add HOUSE as a table entry: WANDER, 16, WAIT 2, BAR, LEAD, POLY OFF, PUSH, RISE, WASH OFF. That is 9 bytes plus a name, and it is in effect DUB-with-WANDER with VARY BAR and PUSH.
- **How to accept it:** HOUSE's distance to every other style ≥ 2.0 on the same 12 figures.
- **Owner's call?** yes, only if S1 is declined. "Add a HOUSE style (a busy, steady stab bed)? Default: no, while DUB plays WANDER."

### S8. PEAK changes sound every 15 s  (LOW, MEASURED + OPINION)
- **What you meet:** JOURNEY with TIME 8 and WAIT OFF gives 17 sound pairs in 128 bars, a new one every ~7.5 bars (14.5 s). 41-63 % of bars sit in a dulled crossing.
- **Evidence:** `style_stats.txt`. With TIME 16, the pairs drop to 9, but the 8-bar level range grows (M02 3.4 → 6.0 dB, brightness 2.6 → 5.4), so it is no clean byte fix.
- **Recommendation:** leave the row. PEAK depends on X1 and S5. Re-measure after them: if the dulled bars are still over 30 %, try TIME 16.
- **How to accept it:** PEAK: dulled bars ≤ 30 % on M02 and M09.
- **Owner's call?** no.

### S9. DUB's WASH OFF against the recipe's WASH LOW makes no audible difference: fix the doc, not the row  (LOW, MEASURED)
- **Evidence:** DUB with WASH LOW on M02 / M09 / M16: dulled bars 48 / 42 / 44 % against 51 / 43 / 47 %. Brightness ×6.39 against 6.99. Level within 0.2 dB. WASH doesn't fill the crossing dip (X1's job).
- **Recommendation:** keep WASH OFF in DUB (no wash cost). Change the TIDE_GUIDE dub recipe to WASH OFF (already listed as doc item 13).
- **How to accept it:** the doc and the row agree.
- **Owner's call?** no.

### S10. Pacing at 120-128 BPM  (LOW, MEASURED: for the docs and as context)
- One leg (A → B) and the rest at the end:
  - DUB: 16 + 2 bars = 34-36 s.
  - DEEP: 32 + 4 = 68-72 s.
  - MINIMAL and PEAK: 8 + 0 = 15-16 s, so the fader never rests.
  - AMBIENT: 64 + 8 = 2.3-2.4 min.
  - DRONE: 64 + 0 = 2.0-2.1 min. M17's own 60 BPM gives 4.3 min.
- One ARC is 1.6-1.8 min. The longest "nothing changes" stretch is ≤ 15 s in every style.
- For a 5-7 minute dub-techno track: DEEP and AMBIENT give one or two legs, the right scale. DUB gives four to six legs a track, fine under WANDER (S1) and restless under JOURNEY. MINIMAL's 16-bar round trip is steady because WANDER keeps the sound (level 1.8-2.4 dB). No change to TIME / WAIT recommended beyond S1.
- **Owner's call?** no.

## If I could change three things
1. DUB plays WANDER (S1, one byte): the first start keeps its sounds, and brightness swings ×2 instead of ×7.
2. AMBIENT POLY OFF (S2, one byte): the calm style stops moving a fifth of its bars against the beat.
3. PEAK's first arc always drops, and the arcs follow the session seed (S3, ~20 B): PEAK keeps its promise within the first 2 minutes.
