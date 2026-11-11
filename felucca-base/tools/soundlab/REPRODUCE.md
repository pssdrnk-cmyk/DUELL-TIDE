# Does soundlab give the old lanes' numbers? (11 Nov 2026, lane C; tree b9/soundlab on fork/b9 427758c, host Windows build)

Method: where the old WAV and voice log still exist, `soundlab measure` was run on them (log turned into JSON by `tests/oldlog2json.py`): that tests the metrics alone.
Where the firmware changed since, a fresh `soundlab render` on this tree was compared with the newest old report that matches this tree. Tolerances are stated per answer.

## 1. M17 (and M18) under 150 Hz: the drone-bass question

Old answer (DRONE-BASS report, b7/b8 reel render, 20 Hz and up): M17 0.0 % under 150 Hz, 42.8 % in 150-400 Hz, LUFS -19.1; M18 0.0 / 93.1 %, LUFS -18.9.

| | soundlab on the old reel WAV (b6/b7 render) | old report | tolerance | result |
|---|---|---|---|---|
| M17 under 150 Hz | 0.0 % (under 60: 0.0) | 0.0 % | +-0.5 points | reproduced |
| M17 150-400 Hz | 45.8 % | 42.8 % | +-3 points | reproduced (the report took another render of the same state) |
| M17 LUFS | -19.3 | -19.1 | +-0.3 | reproduced |
| M18 under 150 Hz / 150-400 / LUFS | 0.0 / 91.0 / -18.9 | 0.0 / 93.1 / -18.9 | same | reproduced |

After the drone sub (fresh render, 16 bars, this tree, `--keys none` / `--keys chord:G3`): M17 22 % / 33 %, M18 34 % / 27 % under 150 Hz.
The drone report (B9-DRONE, tree before the later b9 merges) gave M17 27.1 / 32.9 % and M18 25.8 / 26.0 %: the chord-held figures agree within 1.2 points, the no-key figures do not (M17 22 vs 27.1, M18 34 vs 25.8); the firmware moved between those two trees (low-end restore, re-voice). The B9-SOUND table (same tree as the old WAVs) agrees exactly with soundlab on LUFS, true peak, side/mid and bed notes for M17 and M18 (all four within 0.1).

Not comparable: the old "chord held" renders (`RR_SHIFT -12`) held only G3 (the keys span 53..79, so C3 and Eb3 never sounded). Use `--keys chord:G3` for the old figure.

## 2. M02 HF drop per repeat (B9-SOUND dub echo)

Old: repeats 1 / 2 / 3 above 2 kHz -13.9 / -15.3 / -17.4 dB re total, i.e. (-17.4 - -13.9) / 2 = **-1.75 dB per repeat** (delay-only return, part 1 alone, 16 bars + 8 s ring-out).
soundlab: `render --multi 2 --bars 16 --solo 1 --fx delay --ringout 8` then `measure`: **-1.7 dB per repeat** (echo HF). Tolerance +-0.3 dB: reproduced. With the full mix (`--fx full`): -1.5 dB.

## 3. K4 click (TIME change on M02, DMDEP 24)

Old after the delay glide (B9-DLYGLIDE, same 32-change set): 0 clicks of 32, worst changed window -38.5 dBFS, worst excess over control 1.3 dB. (Before the fix: 3 clicks, worst -23.5, excess 20.2 dB; that firmware is not in this tree, so the click itself cannot be reproduced here.)
soundlab: PLAY, `--keys chord:C4,Eb4,G4`, 4 bars, `--g DTIME=i --fx-time-step 2:+1|-1 --control` for i = 0..16 (32 changes): **0 clicks of 32, worst excess over control 1.3 dB, worst changed window -36.2 dBFS** (a pair whose control is also -36.2: excess 0). The window differs by 2.3 dB from the old -38.5 because the step falls at bar 2 (4.102 s, sample 180895) and the old render stepped at sample 180960: a step's size depends on the signal under it (the old RESULT says so). Tolerance: verdict equal, excess +-0.2 dB, changed window +-2.5 dB: reproduced.
