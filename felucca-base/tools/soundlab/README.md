# soundlab: the one sound-check tool

Render a factory multi or sound on the host through the real firmware DSP, measure it, check the musical rules, compare two builds.
It replaces the ~80 one-off scripts of b8 / b9 (`INVENTORY.md` lists each and what took its place). **Do not write a new render or metric script: add an option or a metric here.**
C for everything numeric, stdlib-only Python only for tables (`compare.py`). Audit tool, not firmware: no device build changes.

## Build

From the repo root (`DUELL/` or a worktree): `. jam/env.sh && sh felucca-base/tools/soundlab/build.sh` (same compiler and flags as `host_tests.sh`; writes `felucca-base/build/host/soundlab`).
One build per tree. To compare two firmware versions, build each worktree and run its own binary; the commit goes into `PREFIX.json`.

## The 4 commands (+ compare)

```
soundlab render --multi N | --sound N  [--keys none|chord:C3,Eb3,G3|note:C3] [--bars 16] [--bpm B]
                [--tide off|evolve|travel] [--style NAME] [--morph a|mid|b|0..127]
                [--knob SET:K:DETENTS ...] [--fx-time-step BAR:+1] --out PREFIX
soundlab measure PREFIX [--from BAR --to BAR]     -> PREFIX.metrics.json + one summary line
soundlab rules   PREFIX                           -> PASS / FAIL / UNSURE per measurable rule of work/b9/MUSICAL_RULES_B9.md
soundlab sweep   --multi N --knob SET:K [--points -32,-16,0,16,32]   -> table, delta vs the 0 point
python3 felucca-base/tools/soundlab/compare.py A B   (two metrics files or two folders) -> Markdown before / after table
```

`render` writes `PREFIX.wav` (stereo, 16-bit, 44100 Hz) and `PREFIX.json` (commit, settings, voice log `t0 t1 part note vel sound`, events, `cpu_note_caps`, `voices_max`).
Renders are deterministic: same tree + same options = identical bytes. `multi` N is 1-based (M02 = `--multi 2`). Knobs: SET = AUTO|STD|DUB|..., K = 2..4, DETENTS = steps from the stored value.
The options added by lanes A / B beyond this list (`--dry`, `--solo`, `--fx`, `--ringout`, `--g NAME=VAL`, `--dump-params`) are in `soundlab --help`.

## Metrics (`PREFIX.metrics.json`; all dB are dBFS or ratios as stated)

| Group | Metric | Definition |
|---|---|---|
| Level | LUFS | ITU-R BS.1770: K-weighting, 400 ms blocks, gates -70 absolute / -10 relative. Factory window: median of the 18 +-3 dB (rule 7) |
| | true peak | 4x oversampled max, dBFS (rule 7: <= -1) |
| | RMS | whole-render RMS, dBFS (the old "level" of the reel tables) |
| Spectrum | centroid | energy-weighted mean frequency, Hz and octaves re 1 kHz |
| | band shares | % of energy in <60, 60-150, 150-400 Hz, >2 kHz, >4 kHz (of the 20 Hz-up spectrum). "<120 Hz %" of the old tables is 20-120 Hz |
| | odd-harmonic share | energy of odd vs even harmonics of the played note: high = square / pulse character (rule 6) |
| | whistle test | narrowest strong peak above 2 kHz vs its neighbours, dB (and how long it persists). **Cannot tell held partials from a resonance**; read it with the voice log |
| Time | onsets a bar | note starts a bar (from the voice log) |
| | low-band onsets per beat | share of bars in which the <120 Hz band starts (rise >= 6 dB in 100 ms, above -45 dBFS) on each of the 4 beats. Rule 8: not on every beat |
| | note length, gate | from the voice log: median note length (s), length / step |
| | attack sharpness | time from onset to 90 % of the note's peak (2 ms RMS), ms |
| | silence share | % of 50 ms frames below -60 dBFS (rule 3: <= 5 %) |
| | simultaneous notes (bed) | mean voices sounding, from the voice log (rule 3: >= 2.5 for a bed) |
| Space | wet / tail | energy after the note-offs vs during the notes, %; echo / reverb share |
| | tail -40 | seconds from the last note-off until the tail is 40 dB down |
| | echo repeats | repeats above -40 dB; loss per repeat (dB); **HF drop per repeat** = change of the >2 kHz share (dB re total) from one repeat to the next (negative = darker, rule 4) |
| | side / mid | 10 log10(E_side / E_mid), dB (rule 4: -8 to -11 or wider; very negative = mono); L / R correlation |
| Pitch | per part median, p90, highest | MIDI note numbers from the voice log |
| | rub % | share of the both-parts-sounding time in which a note of part 1 and one of part 2 are a minor 2nd or minor 9th apart (1 or 13 semitones). **REL** ignores release tails, **ALL** counts them; the b9 tables used ALL |
| Change | sweep delta | each metric minus its value at the 0 point |
| | click | max sample-to-sample jump, either channel, in the 5 ms window (+-220 samples) around a `--fx-time-step`, dBFS, minus the same window of a control render. Click = changed window above -40 dBFS and control not |

`rules` maps these to rules 1-10: 1 rubs, 3 bed + silence, 4 repeats + side, 6 odd share + whistle, 7 LUFS / peak (End A, mid, End B), 8 low-band onsets, 9 repeat 1 / 4 / 7 under `--tide travel`. Rules 2, 5, 10 are for a human: `UNSURE`.
A render is the unit's behaviour including the NOTES budget, so voices are capped as on the device. Host CPU is a lower bound, not headroom.

## Recipes

Every recipe: render, `measure`, then read the line named. `C` = `soundlab`; `P` = a prefix such as `/tmp/m02`.

1. **Drone low end (why M17 / M18 have no bass).** `C render --multi 17 --keys none --out P && C measure P`: read the band shares <60 and 60-150. Chord held: `--keys chord:G3,Bb3,D4`. Gate used: >= 20 % under 150 Hz, LUFS +-1.5 dB, true peak < 0 dBFS.
2. **Dub space (does M02 sound dubby).** `C render --multi 2 --keys chord:C4,Eb4,G4 --bars 16 --out P && C measure P`: wet / tail, tail -40, echo repeats, HF drop per repeat, side / mid. For the delay alone use `--fx delay` and `--dry` for the dry stem (return = difference). Ring-out: `--ringout 8`.
3. **Rubs (minor-second clashes between parts).** `C measure P` and read rub % in REL and ALL; `C rules P` rule 1 shows the pitch sets of both parts.
4. **Sharpness / register (is a lead too bright or a bare square).** Read odd-harmonic share, whistle test, attack sharpness, centroid and per-part p90 / highest note. A bare square or a persistent narrow peak fails rule 6.
5. **Knob strength (does a genre knob do enough).** `C sweep --multi 2 --knob DUB:4`: the table is each metric at -32 ... +32 detents and its delta vs 0. A knob that moves no metric by more than the metric's noise (re-render the 0 point twice: it is exactly equal, so any delta is real) is too weak.
6. **TIME click.** `C render --multi 2 --keys chord:C4,Eb4,G4 --bars 4 --fx-time-step 2:+1 --out P_chg`, a control with the same start TIME and no step (`P_ctl`), then `C measure P_chg` (it reads the control from `--control P_ctl` or the same-named file): click dB and the excess over control.
7. **The 18-multi table.** For N in 1..18 and keys `none` / `chord:G3,Bb3,D4` / `--tide evolve --style <suggested>` (`0 0 1 1 1 4 4 1 2 2 2 2 4 3 1 3 5 5` = DUB DUB DEEP ...): render + measure into one folder per tag, then `compare.py folder_before folder_after`. Do not write a new loop script: `sh felucca-base/tools/soundlab/table18.sh TAG` if lane A shipped it, else a 5-line shell loop.
8. **Before / after a change.** Build the baseline tree and the changed tree (each its own `soundlab`), render the same options into two folders, `compare.py before after`. Same options + same seed = every difference is the change. Report numbers with the commit from the JSON.
9. **TIDE-proof (rule 9).** `--tide travel --style <its style> --bars 16` and for End A / mid / End B `--morph a|mid|b`; `C rules P` must stay PASS for rules 1, 4, 7.

## Tests and checks

- `./host_tests.sh soundlab`: M02 for 2 bars rendered twice must give bit-identical WAV and JSON; `measure` must match `tests/soundlab_ref.txt` within 0.1. If you change firmware sound on purpose, re-generate the reference (`tests/soundlab_ref.sh --write`) and say so in the commit.
- Reproduction of old answers (M17 under 150 Hz, M02 HF drop per repeat, K4 click) is documented in `REPRODUCE.md`.

## Rules for the next lane

- Add a metric: C in `sl_metrics.c`, one line here, a number in the reference. Add a render option: `sl_render.c` + this README.
- Never say "free headroom" from a host CPU figure. Never hand the owner WAVs to audition: accept by metrics, the owner hears it on the device.
- LF line endings only. No new dependencies: nothing that needs numpy or matplotlib.
