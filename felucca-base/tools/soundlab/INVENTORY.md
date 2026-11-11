# soundlab inventory: every old sound-check script and what replaces it

Written by lane C, 11 Nov 2026, from reading the scripts (paths relative to the workspace root; all under work/
unless noted). "SL" = soundlab. Status: **covered** = the spec's interface does it; **GAP** = the spec lacks it (sent to lanes A / B, see the end).

Common facts the old scripts share (they matter for "same numbers"):

- Render = `reelrender.c` (copies in reel, `b9/dub`, `b9/revoice/B`; a patched one in `b9/knobs/click`). Output: 16-bit stereo WAV at 44100 Hz + a `.log` text file with lines `ON t part voice note vel`, `REL`, `END`, `NV t part count` (voice count per block), `BAR k sample`, `CHG`, `BG` / `BP1` / `BP2` (parameter dumps).
  `quality.py` is the exception: raw int32 dumps at 48000 Hz from `sound_audit`.
- Env hooks that became options: `RR_KEYS` (hold C4 Eb4 G4), `RR_SHIFT -12` (G3 chord), `RR_DRY` (all sends 0), `RR_SOLO=1|2`, `RR_UNCAP`, `RR_T0` (start TIME index), `RR_CHG` / `RR_CHG_AT` (TIME change), `RR_DMDEP`, `RR_SUB`.
- `tide` mode = TIDE on, STYLE 0..5 (`0 DUB 1 DEEP 2 MINIMAL 3 AMBIENT 4 PEAK 5 DRONE`, `-1` = DUB / DRONE), first 4 bars skipped.

## 1. `audit/sound-vs-base-2026-11-09/tools` (stock 1.1 vs b5 sound audit)

| Script | Does | Replaced by |
|---|---|---|
| `run.sh`, `run2.sh` | Build `sound_audit` for a snapshot, dump every sound (`SOUND_DUMP`) to raw int32 | `soundlab render --sound N` (loop over N; GAP: `--sound all` would help) |
| `quality.py` | Per sound: peak, RMS, clipped samples, DC (dBFS), click = worst 2 ms 2nd-difference spike vs median loud block | `measure`: true peak, RMS; **GAP**: clipped-sample count, DC, and a "2nd-difference click" figure (the `--fx-time-step` jump metric is a different, ±5 ms first-difference test) |
| `pair.py`, `defdiff.py`, `fixpatch.py` | Pair sounds between two builds by name; diff sound definitions; patch a generated file | `compare.py A B` (pairs by metric file / folder). The definition diff is not a measurement: dropped (use `git diff`). `fixpatch.py` was a one-off: dropped |

## 2. `reel-2026-11-10/sound-check/tools` (the reel, 18 multis)

| Script | Does | Replaced by |
|---|---|---|
| `reelrender.c`, `build.sh`, `run.sh` | Render K, TIDE, bars, STYLE, to WAV + log; variants play / dry / tide / solo 1 / solo 2 | `render --multi N [--tide ... --style ...]`; **GAP**: `--dry` (sends 0), `--solo 1|2`, `--uncap` |
| `analyse.py` | The big one, per multi: RMS, peak, crest, dynamics, band shares (<120, 120-500, .5-2k, 2-6k, >6k), centroid, flatness, flux, side dB, wet %, silent % (and dry), voices a block (mean / max), steals, held steals, notes per onset, onsets a bar, gate / sound steps, velocity spread, per part: odd / even harmonic dB, harmonic centroid movement % (20 vs 150 ms), attack ms, blip % (short notes), 16th-off share, IOI, swing | `measure`. Covered: level, bands, centroid, side, wet / tail, silence, voices, onsets a bar, attack, note length. **GAP**: per-part (solo) numbers: odd / even harmonic dB, movement % inside a note, blip %, steals / held steals, flatness, flux, crest, velocity spread. Without `render --solo` the per-part figures can't exist |
| `score.py` | "Chiptune signs" per multi from numbers.json (no bed, gaps, mono, dry, ...) | `rules` (10 rules); the chiptune signs are a thin wrapper: dropped, the numbers they read are in `measure` |
| `table.py`, `params.py`, `pnames.py` | Print the numbers table; dump parameter values per part from the log; P_* id -> name from `core.h` | `compare.py` / `sweep` tables. **GAP (small)**: `render --dump-params` (BG / BP1 / BP2 as `PREFIX.params.tsv`, names resolved in C) |
| `pictures.py` | Spectrogram and envelope strip per multi (matplotlib) | Dropped: no plotting dependency. If wanted later, a PGM / CSV spectrogram export is the way |

## 3. `b9/dub` (M01 / M02 dub space, the biggest metric set)

| Script | Does | Replaced by |
|---|---|---|
| `reelrender.c`, `build.sh`, `render.sh`, `tail.sh`, `try.sh` | Render M01 M02 in variants full / dry / d (delay only) / r (reverb only) / s1dry / s2dry / s1d; one-shot tails (stop transport after N bars, ring out 8 s) | `render`; **GAP**: `--fx dry|delay|reverb|full` (return = fx variant minus dry), `--solo`, `--ringout SEC` (stop notes after the last bar, keep rendering). `try.sh` / `variant*.py` build variants of `factory_multis.py`: that is a build step (regenerate multis, rebuild), not a metric: dropped; a recipe in the README says "build once per tree, compare.py the two" |
| `dubmetrics.py` | Stab (first note of part 1) decay to -20 dB (ms), stab centroid, stab >2 kHz share, delay return vs dry, reverb return vs dry, repeat levels (1st / 2nd re dry stab), loss per repeat, **HF (>2 kHz) drop per repeat**, time to -40 dB after the first echo, reverb vs stab at .5 / 1 / 2 / 3 / 4 s, delay return <150 / <300 Hz %, energy after the stab (dry / full), full vs dry RMS, band shares <60 / 60-150 / 150-400, per-bar centroid range (octaves), per-bar dB range | `measure` Space block. Covered by spec: tail energy, time to -40 dB, repeats above -40 dB, HF drop per repeat. **GAP**: stab decay -20 dB (ms), stab centroid, return-vs-dry levels (needs the fx variants), delay-return low share, per-bar centroid / level ranges |
| `space.py` | Gap fill (level before the next onset re the stab), quietest 5 % of 50 ms frames, loudest-to-gap range, side/mid dB and L/R corr of mix / dry / echo return / reverb return / parts, air >6 kHz %, part 2 vs stab dB, per-bar side/mid range | `measure`: side/mid, L/R corr, silence. **GAP**: gap fill, 5th / 95th percentile frame range, air >6 kHz, per-bar side/mid range, return-only side/mid (needs fx variants) |
| `rules.py` | LUFS (BS.1770, 400 ms blocks, gates -70 / -10), true peak 4x, bed notes avg / silence, pitch sets, minor-2nd pairs between parts, low band shares (<60, 60-120, 120-250), sub (<100 Hz) level and 100 ms std/mean (thump depth), spike above 2 kHz (max over 0.5 s windows of a bin vs the median of +-40 bins), side/mid | `measure` + `rules`. Covered. Detail for B: keep this `spike` definition **and** the revoice-B one (below), the two differ |
| `trace.py` | Which BG / BP parameters move over the 16 bars | **GAP (small)**: `render --dump-params` per bar (see above) |
| `table.py` | Before / after table of the dubmetrics keys for M01 / M02 play / tide | `compare.py` |
| `variant.py`, `variant3.py`, `factory_multis*.py` | Edit `factory_multis.py` to try a variant | Not measurement: dropped (build step) |

## 4. `b9/revoice/A`, `b9/revoice/B`

| Script | Does | Replaced by |
|---|---|---|
| `A/measure.py` | One line per tag from analyse.py's json: rms, peak, bed mean / max, silent %, side, fx %, <120 %, 120-500 %, centroid, per part nv, odd/even, move %, blip % | `measure` + `compare.py` (per-part items: GAP above) |
| `A/pitchsets.py` | Per part pitch classes, the sounding set at each note-on, minor-2nd pairs **within** a part (w) and **across** parts (x); modes REL / END | `measure` Pitch. **GAP**: "within" pairs as well as "across"; list of the fullest sounding sets |
| `A/run.sh`, `A/build.sh`, `A/apply_after.py` | Render / build / apply an edit | `render`; build and edits are not measurements |
| `B/metrics.py` | Checklist row from analyse json + wavs; K-weighted LUFS with **2 s** gated blocks (frequency domain) | `measure` LUFS. Note: BS.1770 is 400 ms blocks (dub `rules.py`); the 2 s one was a shortcut. Use 400 ms; numbers differ by ~0.1-0.3 LU |
| `B/rules.py` | Pitch sets and rubs (rule 1), true peak and LUFS (rule 7), **whistle test** = strongest bin >2 kHz within 25 dB of the frame's strongest bin AND >12 dB over its +-300 Hz neighbours, longest run in s (rule 6) | `rules`. Report **both** the `spike` dB (dub) and this longest-run seconds |
| `B/render.sh`, `B/build.sh`, `B/reelrender.c`, `B/parts.py`, `B/apply_proposals*.py` | M09-M16 batch render, per-part summary, apply proposals | `render` loop; `parts.py` = `compare.py`; proposals are edits: dropped |

## 5. `b9/sound` (18-multi table), `b9/knobs/click` (K4), `b9/probes`, `felucca-base/tests/b9_rules_probe.c`

| Script | Does | Replaced by |
|---|---|---|
| `sound/render18.sh`, `build.sh`, `fmbuild.sh`, `g12*.sh`, `lowend.sh` | Render all 18 multis in no-key / G3-chord-held / TIDE (suggested STYLE) modes; G1/G2 before / after; low-end generator | `render --multi N` x 18 with `--keys none`, `--keys chord:G3,Bb3,D4`, `--tide ... --style S`. **GAP**: a table command: `sweep` covers one knob; for "all 18" a thin `soundlab table --all` (or a README loop + `compare.py folder`) |
| `sound/measure18.py` | Per multi: bed notes (p1, p2, both), silence %, rub % (REL), side, <120 Hz %, **low-band onsets per beat** (<120 Hz, rise >= 6 dB in 100 ms, > -45 dBFS), peak; LUFS and true peak (not for the keys mode) | `measure` (all items in the spec) |
| `sound/mktable.py` | Markdown table before / after of m18 json | `compare.py` |
| `sound/fm_step2.py` | Edit `factory_multis.py` | not measurement: dropped |
| `knobs/click/clickprobe.c`, `build.sh`, `run_all.sh`, `run_dmdep0.sh` | M02 PLAY, chord held, 4 bars at 117 bpm, TIME change at sample 180960; controls with the same start TIME; DMDEP 0 arm | `render --fx-time-step BAR:+1` (+ **GAP**: a control render needs `--g NAME=VAL`, i.e. start TIME index, DMDEP override; or `--time-start I`) |
| `knobs/click/analyse_click.py`, `analyse_dmdep0.py`, `levels_probe.py` | Click = max abs first difference over +-220 samples (5 ms), both channels, dBFS; click if changed > -40 and control not; excess = changed - control | `measure` Change (the spec's "largest sample jump in a 5 ms window vs a control render") |
| `probes/probe_*.c` (drift onsets, fun10, patch, regress_info, store_backup, xver) | Firmware test probes (store, patch, regression), not sound checks, except `probe_drift_onsets.c` (onsets of drifting patterns) | Out of scope (firmware tests keep their own groups); `probe_drift_onsets` = `measure` onsets a bar |
| `felucca-base/tests/b9_rules_probe.c` (on branch `b9/lowend`, commit 0381a50; **not in fork/b9**) | Level, 4x true peak, low-band onsets, whistle test, at morph End A / middle / End B and TIDE WANDER | `render --morph a|mid|b` + `rules`; `--tide evolve|travel` (**GAP**: rule 9 names WANDER / JOURNEY; state which spec values map to which, or accept `wander|journey`) |

## 6. Sent to lanes A and B (what the spec misses)

**Lane A (render)**
1. `--dry` (all FX sends 0), `--fx dry|delay|reverb|full`, `--solo 1|2` (the other part's MIXER LEVEL 0), `--uncap` (no NOTES budget): old variants full / dry / d / r / s1dry / s2dry need them.
2. `--ringout SEC` (stop the transport after the last bar and keep rendering; `tail.sh`).
3. `--g NAME=VAL` generic global override (start TIME index `RR_T0`, `RR_DMDEP`), for the click control render and the DMDEP 0 arm.
4. `--dump-params` -> `PREFIX.params.tsv` (BG / BP1 / BP2 per bar, names).
5. `--tide` values: accept `wander|journey` as aliases (rule 9 names them); `--bpm` default = the multi's own.
6. Optional: `--sound all` / `--multi all` writing one PREFIX per item; JSON `voices[].end` (release end) so REL / ALL modes both work.

**Lane B (metrics)**
1. Whistle test twice: dub `spike` dB and revoice-B longest-run seconds (definitions above).
2. LUFS with 400 ms blocks (BS.1770); mention the 2 s variant is not used.
3. Per bar: centroid range (oct), level range (dB), side/mid range; gap fill; 5th / 95th percentile frame level; air >6 kHz; sub (<100 Hz) level and 100 ms std/mean (thump depth).
4. Stab figures from the voice log: decay to -20 dB (ms), centroid, >2 kHz share.
5. Clipped-sample count, DC, 2nd-difference click figure (`quality.py`).
6. Rubs: across parts (spec) and within a part; REL and ALL; fullest sounding sets.
7. With solo stems: odd / even harmonic dB, in-note movement %, blip %, steals / held steals, flatness, flux, crest.
8. Return-vs-dry levels and return-only side/mid, when render gives fx variants (compare PREFIX files).
