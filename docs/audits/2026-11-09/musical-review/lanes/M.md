# Lane M: the 18 factory multis as a set
Looked at: the b6 records as published (`jam11` fd4b6c6, worktree `work/wt/mus-M`): `tools/factory_multis.py` (M01, M05-M09 entries and the order notes), `tools/drone_pack.py` (M17, M18), `gen_factory_multis dump` / `loud`, the TIDE STYLE code (`tide_ui.c`: STYLE is global, only M17/M18 force DRONE). Rendered every multi on the host with a new tool `../tools/mlane.c` (built on `tests/tide_roles_test.c` the way `tide_long.c` is; header has the build line): PLAY alone 8 bars, TIDE 64 bars under STYLE DUB (M17/M18 DRONE) with TARGET BOTH / ROLL MUSIC / PITCH OFF, once with a C minor chord (C4 Eb4 G4) held where a part needs keys and once with no key held; idle solos of 9 multis; per-bar low band for the TIDE runs. Data: `../data/` (`table.md`, `idle_*`, `tide_*`, `nokeys_*`, `bars_summary.txt`, `solo_idle_*`, `dump.txt`). Measured: notes per part on a 16-step grid, register, chord width, rhythm overlap; mix level (K-weighted) and energy share under 120 Hz / 120 Hz-2 kHz / over 2 kHz (4th-order splits).
Not looked at: 128-bar runs, styles other than the default, PITCH ON, the spec's planned reel rendered (only read), CPU, the unit itself. Per-part spectra under TIDE (MORPH / JRNY re-apply LEVEL, so a mute does not hold there; solos are PLAY alone only). The cause of M01's low band with keys held (M4) is not traced.

## The set table (TIDE 64 bars; held / classic-arp parts with C4 Eb4 G4 held; a held part's register is the held chord)

| M | sounds (part 1 / part 2) | BPM | kind 1 / 2 | register 1 (p10-med-p90) | register 2 | onsets/bar 1 / 2 | chord width 1 / 2 | reg. overlap (st) | rhythm Jaccard | <120 Hz / mid / >2 kHz % | level vs median dB: alone / TIDE / no keys | parts sounding, no keys |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| M01 BASIC CHORD | CHORD ECHO / DUST BED | 122 | ARP SEQ / held | C4-G4-C5 | (held) | 4.0 / 0 | 4 / - | - | - | 20.4 / 70 / 9.5 | +0.2 / +0.2 / +0.1 | **1** |
| M02 DUB CHORD | BIG STAB / TIDAL SUB | 117 | ARP SEQ / ARP SEQ | C4-G4-G5 | C3-C3-F3 | 2.0 / 3.6 | 4 / 1 | -7 | 0.03 | 10.2 / 88 / 1.7 | +1.8 / -3.2 / -0.4 | 2 |
| M03 CELLAR TAPE | PLUCK BASS / CLAVICLE | 122 | ARP SEQ / steps | C2-C2-F2 | Eb4-Ab4-Eb5 | 4.1 / 2.1 | 1 / 4 | -22 | 0.02 | 2.4 / 97 / 1.0 | +0.9 / -3.9 / -1.1 | 2 |
| M04 NIGHT ORGAN | PEDAL BASS / DRAWBARS | 121 | ARP SEQ / ARP SEQ | C4-C4-C4 | C3-E4-G4 | 3.5 / 2.1 | 1 / 4 | 0 | **0.59** | **30.5** / 69 / 0.3 | +4.4 / -0.2 / +2.6 | 2 |
| M05 ORGAN SKANK | ORGAN SKANK / AIR SHIMMER | 123 | ARP SEQ / ARP SEQ | C4-Eb4-Bb4 | Ab4-Eb5-Bb5 | 4.5 / 2.4 | 4 / 4 | 2 | 0.00 | 0.6 / 64 / 35.2 | -1.0 / +0.2 / +3.1 | 2 |
| M06 WARM ECHO | WARM WASH / TAPE PLUCK | 118 | ARP SEQ / ARP SEQ | Bb3-G4-C5 | G4-C5-F5 | 1.0 / 4.3 | 3 / 1 | 5 | 0.25 | 0.1 / 96 / 4.0 | -0.3 / +1.2 / +4.1 | 2 |
| M07 MINIMAL 12 | DUB BLIP / DEEP BLUE | 124 | ARP SEQ / ARP SEQ | Bb4-C5-C5 | G3-D4-G4 | 5.3 / 2.6 | 1 / 3 | -3 | 0.13 | 0.2 / 91 / 8.6 | -1.8 / -1.8 / +1.1 | 2 |
| M08 TINE DUB | TINE ROOM / SLOW TIDE | 120 | ARP SEQ / ARP SEQ | C4-G4-C#5 | C4-G4-G5 | 4.0 / 2.1 | 4 / 3 | **13** | 0.03 | 1.0 / 93 / 5.8 | -0.1 / +2.4 / +5.2 | 2 |
| M09 DEEP 332 | SOUL STAB / FOG BANK | 124 | ARP SEQ / ARP SEQ | C4-Bb4-Eb5 | C4-C4-G4 | 3.8 / 2.0 | 4 / 1 | **7** | 0.21 | 0.2 / 94 / 6.3 | -0.8 / -2.9 / -0.1 | 2 |
| M10 PAD SPLIT | EVOLVING PAD / EMBER GLOW | 97 | held / ARP SEQ | (held) | Bb5-Eb6-D7 | 0 / 1.3 | - / 3 | - | - | 0.7 / 97 / 2.1 | +6.0 / +2.3 / **-14.9** | **1** |
| M11 WHISPER TIDE | DIGITAL PAD / WHISPER PAD | 77 | steps / arp RND | Bb2-F3-C4 | C4-G4-G5 | 0.5 / 1.0 | 2 / 1 | 0 | 1.00 | 2.8 / 96 / 1.1 | +6.8 / +2.3 / +0.4 | **1** |
| M12 TINE STRINGS | STRING MACH / TINE ROOM | 77 | steps / ARP SEQ | A3-Bb3-A4 | Eb4-G4-C5 | 0.5 / 2.2 | 1 / 1 | 6 | 0.06 | 0.3 / 96 / 3.9 | -2.3 / -3.8 / -0.9 | 2 |
| M13 COLD CHAPEL | EVOLVING PAD / SINGING BOWL | 71 | steps / arp UP | A2-F3-A3 | C5-Eb5-G5 | 0.5 / 2.0 | 2 / 1 | -15 | 0.50 | 3.5 / 90 / 6.9 | +5.4 / +3.2 / +1.2 | **1** |
| M14 EERIE SIGNAL | VESPERS / JAZZ CLUB | 120 | held / arp | (held) | C4-Eb4-G4 | 0 / **11.7** | - / 1 | - | - | 0.2 / 98 / 1.2 | +3.1 / +5.2 / -6.8 | **1** |
| M15 HAZY TENSOR | LADDER BASS / GEARBOX | 117 | ARP SEQ / arp | G2-G2-G3 | C4-Eb4-G4 | 4.7 / **18.4** | 1 / 1 | -5 | 0.29 | **48.7** / 50 / 0.8 | -5.5 / -12.4 / -10.6 | **1** |
| M16 FINAL REEL | BRASS PAD / STRING MACH | 71 | steps / ARP SEQ | G2-C3-E4 | C5-C5-G5 | 0.5 / 0.5 | 1 / 1 | -8 | 1.00 | 16.3 (alone: **66**) / 82 / 1.7 | -7.4 / -5.1 / -2.2 | 2 |
| M17 DUNE HORN | HORN SWELL / VOID SHIMMER | 60 | ARP SEQ / held | C4-Eb4-Ab4 | (held) | 1.0 / 0 | 3 / - | - | - | 0.0 / 100 / 0.4 | +1.2 / -2.9 / -6.2 | **1** |
| M18 VOID DEEP | VOID SHIMMER / SOLAR WIND | 60 | held / ARP SEQ | (held) | C4-G4-F5 | 0 / 0.4 | - / 3 | - | - | 0.0 / 98 / 1.6 | +2.0 / -2.6 / -8.3 | **1** |

Medians M01-M16 (K-weighted, mix): PLAY alone -20.4, TIDE -16.2, TIDE with no key -19.1 LUFS. "Reg. overlap": semitones the two parts' p10..p90 spans share (negative = a gap). Rhythm Jaccard: shared / all 16th steps hit, bars where both play. Swing: every record stores SWING 50 (dump); only M09's stab has arp swing 54. Every multi plays in C (ghost chord C4 minor; SCALE / ROOT are the unit's, CHROMATIC by default). Full table with steps-sounding columns: `../data/table.md`.

## What is good (keep)
- Six grooves keep the low end clean under TIDE: under 120 Hz M05 0.6 %, M06 0.1 %, M07 0.2 %, M08 1.0 %, M09 0.2 %, M12 0.3 % (the spec's 4.3 bound is about 3 %).
- The chord part and the second part interlock in M02, M03, M05, M08: they share 0.0-0.14 steps a bar (Jaccard ≤ 0.03); M06, M07, M09, M12 ≤ 0.25.
- The grooves' levels sit together PLAY alone: M01, M03, M05-M09 within -1.8..+0.9 dB of the median.
- Chord parts sit in the chord lane: every stab / skank's p10 is C4 and p90 at most C5-G5; no note anywhere under C2 (CELLAR TAPE's C2 line is the floor, by design).
- TIDE keeps each multi's harmony: the pitch-class sets after 64 bars equal PLAY alone's in all 18 (PITCH OFF).
- M17 / M18 are clean drones: 0.0 % under 120 Hz, 98-100 % in the mids.

## Findings (most valuable first)

### M1. Left alone (no notes coming in), 8 of the 18 multis play only one part  (HIGH value, MEASURED)
- **What you hear / meet:** press PLAY with AUTO TIDE and send no notes: M01's dust bed, M10's pad, M11's and M13's arps, M14's pad and arp, M15's arp, and the drone beds of M17 and M18 stay silent. In M10 only a few high EMBER GLOW notes are left; the drones become one thin moving line. M01 is the multi the records call "the first multi to try".
- **Evidence:** `nokeys_*.txt` (64 bars, DUB / DRONE): one part with any note in M01, M10, M11, M13, M14, M15, M17, M18. Levels against the no-key median (-19.1 LUFS): M10 -14.9, M15 -10.6, M18 -8.3, M14 -6.8, M17 -6.2 dB. Why: those parts are HOLD ALL with no steps, or a classic arp, and only ARP SEQ parts get the C4 ghost chord on PLAY (`tide_seq_chord`). The drone guide says "hold a chord" (drones TASKS.md:23), so the drones do it on purpose, but TIDE is meant to run by itself.
- **Recommendation:** records first. M01 DUST BED gets one tied step-sequencer note, as the spec's Appendix A already writes it (`steps="n60 - - … ."`, P_AMODE OFF). This can ship ahead of T8 on today's record. M17 VOID SHIMMER and M18 VOID SHIMMER: ARP SEQ, one step `0` tied over the whole pattern at RATE 2BAR or 4BAR, GATE 127. The ghost chord then plays them, and a held chord still sets the key. The owner records M10, M11, M13, M14, M15 can't be edited (D33). For them, either a small main-loop rule (while AUTO TIDE plays and no key is held, a part that would wait for keys gets the ghost chord; `tide_seq.c`, estimated 60-100 B, outside the audio interrupt; OPINION estimate), or the spec's Q1 moves M14 and M15 to SPARE anyway.
- **How to accept it:** in the no-key run, both parts sound in 16 or more of 18 multis, and each multi is within ±3 dB of the median (today 10 of 18 are, and M10 is 14.9 dB under).
- **Owner's call?** yes: "When no notes come in, should the pads and arps of your own multis (PAD SPLIT, WHISPER TIDE, COLD CHAPEL, EERIE SIGNAL, HAZY TENSOR) play the C minor chord by themselves?" Default: yes, by the ghost-chord rule; the records stay as they are.

### M2. D46's planned fix (trim M15 and M16 up) would pour bass into the mix  (HIGH, MEASURED)
- **What you hear / meet:** FINAL REEL and HAZY TENSOR feel quiet, so D46 says turn them up. But what they hold is low: their energy is the brass pad at G2-E3 and the ladder bass at G2-G3. Turning them up mostly turns up the octave the Volca and the kick use.
- **Evidence:** idle solos: M16 BRASS PAD alone has 71 % of its energy under 120 Hz (`solo_idle_15_1.txt`), the whole multi 66 %. M15 LADDER BASS alone has 41 %, and the multi under TIDE 48.7 %. M16 trimmed to the median (+7.4 dB) would put its under-120 Hz band at about -22 dB, against about -25.7 dB for M02 DUB CHORD, which plays a sub line. That is about 3.5 dB more low end than the multi that carries a bass. M15 trimmed would have about 1.4 dB more.
- **Recommendation:** fix the register, then set the level. M16: BRASS PAD's steps an octave up (owner record: owner's call), or P_EQLG -12 on part 1 as DUST BED has. Then trim. M15: SPARE as the spec's Q1 proposes (also see M5). Change D46's default to "M16: lift the pad an octave or cut its lows, then level; M15: spare".
- **How to accept it:** M16's share under 120 Hz drops from 66 % to 3 % or less (PLAY alone) before it's trimmed into ±3 dB of the median.
- **Owner's call?** yes: "FINAL REEL's brass pad sits in the bass octave: move it up an octave (default), or only cut its lows?"

### M3. The first sound a new unit plays is a bass line under a stab, and slots 2 to 4 all carry a bass  (HIGH, MEASURED)
- **What you hear / meet:** a new unit starts on M02 DUB CHORD. Its TIDAL SUB line at C3 adds 20 % of the energy under 120 Hz (TIDAL SUB alone 26 %). Under TIDE DUB, JOURNEY roams that part through LADDER BASS, BEHIVE and SQUELCH: 36 of 64 bars have over 10 % under 120 Hz. Step forward and M03 (PLUCK BASS at C2) and M04 (PEDAL BASS, 30.5 % under 120 Hz, parts on the same steps: Jaccard 0.59) follow. The owner has a Volca for this.
- **Evidence:** `idle_1`, `solo_idle_1_2`, `bars_summary.txt` (M02 36/64 bars, M04 63/64 bars over 20 %); table rows M02-M04. M01 BASIC CHORD has 0.1 % under 120 Hz PLAY alone and is +0.2 dB from the median.
- **Recommendation:** the spec's Q3 is right by these numbers: the first start goes to the off-beat skank with no bass (`TIDE_FIRST_MULTI` to M01, C6). Do it together with M1's DUST BED fix, or the first start plays one part. In the spec's reel the bass-carriers sit at M03, M04, M07; keep them apart and never in the first two slots.
- **How to accept it:** the first-start multi has 3 % or less under 120 Hz, both parts sounding with no keys; no two bass-carrying multis next to each other.
- **Owner's call?** no (Q3 is already the spec's open question; this is its evidence).

### M4. With a chord held, BASIC CHORD wanders into the low band; with no keys it doesn't  (MEDIUM, MEASURED; cause not traced)
- **What you hear / meet:** M01 under DUB with the owner holding a C minor chord: in 27 of 64 bars over 10 % of the energy is under 120 Hz (max 57 %). These are mostly bars with SILK CHORUS + TAPE HISS and LOW STAB + MOOR WIND loaded. Played the same way with no key held, JOURNEY visits the same sounds and the run stays at 0.4 %.
- **Evidence:** `bars_0.txt` against `nokeys_0.txt` (and the `m01b.sh` sound counts). PLAY alone it is 0.1 %. JOURNEY's context (`jrny_ctx.h`) holds level and brightness, no low band.
- **Recommendation:** a probe first (code reading, about 30 min): which part sounds the lows when keys are held (the LAYER keys sounding on part 2's noise sounds, or SILK CHORUS's SUB LEVEL under the held notes). If it's the sound, give JOURNEY a 171-bit "carries lows" table from the same generator (about 22 B) and skip those sounds from a part whose Original has none. TIDE lane may overlap.
- **How to accept it:** M01 under DUB, keys held, 64 bars: 0 bars over 10 % under 120 Hz.
- **Owner's call?** no.

### M5. The ambient end is crowded with look-alikes, and dub / house roles are missing  (MEDIUM, MEASURED + READ)
- **What you hear / meet:** 7 of 18 slots are slow pads or drones (M10-M13, M16-M18, stored at 60-97 BPM) in a house / dub-techno set. Pairs that play nearly the same thing:
  - **M11 WHISPER TIDE / M13 COLD CHAPEL:** with no keys, both play one pad dyad every 2 bars (0.5 onsets/bar, 2 notes, A2/Bb2 to A3/C4), both stored at 71/77 BPM.
  - **M17 DUNE HORN / M18 VOID DEEP:** both use VOID SHIMMER, both at 60 BPM, each one slow line in C4-C5.
  - **M01 BASIC CHORD / M08 TINE DUB:** the same four off-beat MIN7 hits (steps 3, 7, 11, 15) from C4, at 122/120 BPM.

  Missing: a texture bed that plays by itself (DUST BED is the only texture and is silent, M1); an answer part (notes only in the bar's second half: none on the reel); a piano / keys house stab; a sparse minimal figure (M07's blip plays 5.3 onsets a bar).
- **Evidence:** table rows; `nokeys_10`, `nokeys_12`; records in `factory_multis.py` and `drone_pack.py`.
- **Recommendation:** the spec's reel (T8/T9) adds TINE DUB with the ISLAND STEEL answer, DX LOOP (pulse + TAPE HISS air) and two PEAK stabs. The numbers support Q1 = yes. Still missing after T9: a sparse figure + texture and a keys house stab. With factory sounds: DUB BLIP on 2-3 hits a bar + TAPE HISS swell (MINIMAL), and SUNDAY KEYS (#151) on P04 HOUSE STAB + ROTOR HAZE held at G3+D4 (DEEP). These would take M11's or M13's slot (owner's call) or M18's (M17 stays as the drone).
- **How to accept it:** no two multis share the role pair, BPM band (±6) and chord-part register within 3 semitones; ambient / drone slots ≤ 5.
- **Owner's call?** yes: "WHISPER TIDE and COLD CHAPEL play almost the same when left alone: keep both (default), or make room for a minimal-texture multi?"

### M6. In TINE DUB and DEEP 332 the pad sits right on top of the stab  (MEDIUM, MEASURED)
- **What you hear / meet:** in M08 the SLOW TIDE fifths cover C4-G5, the same octave as the TINE ROOM stabs (C4-C#5). In M09 FOG BANK holds C4-G4 under SOUL STAB at C4-Eb5. The stab loses its edge to the pad; M07, where DEEP BLUE sits at G3-G4, has the gap.
- **Evidence:** register overlap p10..p90: M08 13 semitones, M09 7 (M07 -3, M03 -22). The pads are ARP SEQ on the C4 ghost with offsets 0 / 7.
- **Recommendation:** data. On today's records, write the pads' steps an octave down (`-12A … -5A`); the spec already writes DEEP 332's FOG BANK as absolute C3 dyads (n48+55) for T8, and the spec's TINE DUB has no pad. C3 is 131 Hz, above the 120 Hz band.
- **How to accept it:** overlap ≤ 0 semitones in both; under 120 Hz stays ≤ 3 %.
- **Owner's call?** no.

### M7. HAZY TENSOR and EERIE SIGNAL are too busy and too drum-like to sit behind the TR-6S  (MEDIUM, MEASURED)
- **What you hear / meet:** with a chord held, HAZY TENSOR's GEARBOX arp plays 18.4 onsets a bar (more than every 16th) over a ladder bass at G2. EERIE SIGNAL's JAZZ CLUB arp plays 11.7 a bar. Each loses one of its parts without keys (M1).
- **Evidence:** table; the spec's busiest role (PULSE) is 6-8 a bar; M15 also 48.7 % under 120 Hz (M2).
- **Recommendation:** the spec's Q1 (EERIE SIGNAL, HAZY TENSOR to SPARE) is backed by these numbers: do it in T9.
- **How to accept it:** no factory part above 8 onsets a bar over 64 bars of its default.
- **Owner's call?** yes, it is Q1: "Move EERIE SIGNAL and HAZY TENSOR to the spares (kept, one line brings them back)?" Default: yes.

### M8. Under a house clock the drones play twice as fast as they were made  (MEDIUM, READ + arithmetic)
- **What you hear / meet:** M17 / M18 were made at 60 BPM ("a 1/4 step is a second", `drone_pack.py`). With the FM-1 on the TR-6S clock at 122 BPM, DUNE HORN swells every 0.49 s instead of 1 s, and VOID DEEP's three turns take 15.7 s instead of 32 s. The stored tempo is the one thing the clock replaces; the arp rates follow it.
- **Evidence:** `drone_pack.py` lines 41-48 (rates 1/4 and 1/2), the record BPM 60; the clock lock is in `seq.c` (G_CLOCK / `midi_clock.lock`). 122 / 60 = 2.03.
- **Recommendation:** data: M17 RATE 1/4 → 1/2 and M18 1/2 → 1/1. At 120-124 BPM they then move as made; on the internal clock they keep 60 BPM and move half as fast. The same holds, milder, for M11-M13 and M16 (71-77 BPM): leave them.
- **How to accept it:** at 122 BPM, DUNE HORN's step about 1 s (0.98 s) and VOID DEEP's loop about 31 s.
- **Owner's call?** yes: "Do you run the drone multis on the drum machine's clock (default: slow their steps to match), or on their own 60 BPM?"

### M9. The set mixes C minor with C major and borrowed minor chords, and nothing follows the owner's key on its own  (LOW, MEASURED)
- **What you hear / meet:** with no notes in, every multi plays in C. M04's DRAWBARS (C E G) and M16's BRASS PAD (C D E G) are major, against the C-minor majority. M05, M08 and M09 step to parallel minor chords (Ebm7, Abm7: Gb, Db, Cb), which can rub against a C-minor bass line. To play in another key, the owner must send one note: the ARP SEQ parts then transpose.
- **Evidence:** pitch classes in `tide_*.txt`: E in M04, M16; F#/C# in M08, M09; F#/B in M05.
- **Recommendation:** none in data now (the parallel minors are idiomatic dub). For the guide, one line: "send or hold one note to set TIDE's key; with none it plays C minor".
- **How to accept it:** the guide names the ghost key.
- **Owner's call?** no.

### M10. With keys held, three ambient multis come out 5-7 dB loud, which D46 doesn't list  (LOW, MEASURED)
- **What you hear / meet:** play a chord into M11 WHISPER TIDE, M13 COLD CHAPEL or M10 PAD SPLIT and they jump +5.4 to +6.8 dB over the set median. D46's numbers were taken without keys for these, where their arp parts are silent.
- **Evidence:** PLAY alone with C4 Eb4 G4 held: M11 +6.8, M10 +6.0, M13 +5.4 dB (`idle_*.txt`). `gen_factory_multis loud` measures M11 and M13 with "PLAY" (no keys) at -17.4 / -20.6.
- **Recommendation:** measure D46's trims for the arp multis with the chord held (what the owner hears), then trim. M10 is already on D46.
- **How to accept it:** with keys held, every multi within ±3 dB of the median.
- **Owner's call?** no.

### M11. No groove multi has swing  (LOW, READ)
- **What you hear / meet:** every record stores SWING 50; only DEEP 332's stab has arp swing 54. Deep house stabs usually sit at 54-56.
- **Evidence:** `dump.txt`; the spec writes 56 for ORGAN SKANK.
- **Recommendation:** data: arp swing 56 on M05 ORGAN SKANK's part 1 (spec), 54 on M09's stab (as now). Leave the dub skanks straight.
- **How to accept it:** M05 P_ASWING 56 in the record.
- **Owner's call?** no.

## If I could change three things
1. Let every part play with no notes in (M1): DUST BED as one tied step, the drone beds as a tied ARP SEQ step, and a ghost-chord rule for the owner multis. Then move the first start to M01 (M3).
2. Fix the low end before levels (M2): M16's brass pad up an octave or its lows cut, M15 and M14 to the spares (Q1), D46's default rewritten; then trim by the keys-held numbers (M10).
3. Thin the ambient end and fill the missing roles (M5): one of WHISPER TIDE / COLD CHAPEL out for a sparse figure + texture multi, plus a keys house stab, on top of T8/T9; slow the drones' steps for a house clock (M8).
