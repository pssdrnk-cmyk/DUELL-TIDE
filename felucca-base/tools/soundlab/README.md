# soundlab: the one sound-check tool

Render a factory multi or sound on the host through the real firmware DSP, measure it, check the musical rules, compare two builds.
It replaces the ~80 one-off scripts of b8 / b9 (`INVENTORY.md` lists each and what took its place). **Do not write a new render or metric script: add an option or a metric here.**
C for everything numeric, stdlib-only Python only for tables (`compare.py`). Audit tool, not firmware: no device build changes.

## Build

From the repo root (`DUELL/` or a worktree): `. jam/env.sh && sh felucca-base/tools/soundlab/build.sh` (same compiler and flags as `host_tests.sh`; writes `felucca-base/build/soundlab/soundlab` (`.exe` on Windows; run it after `. jam/env.sh`, or it exits 127 with no output: the DLLs).
One build per tree. To compare two firmware versions, build each worktree and run its own binary; the commit goes into `PREFIX.json`.

## The 4 commands (+ compare)

```
soundlab render --multi N | --sound N  [--keys none|chord:C3,Eb3,G3|note:C3] [--bars 16] [--bpm B]
                [--tide off|evolve|travel] [--style NAME] [--morph a|mid|b|0..127]
                [--knob SET:K:DETENTS ...] [--fx-time-step BAR:+1] --out PREFIX
soundlab measure PREFIX [--from BAR --to BAR]     -> PREFIX.metrics.json + one summary line
soundlab rules   PREFIX                           -> PASS / FAIL / UNSURE per measurable rule of work/b9/MUSICAL_RULES_B9.md
soundlab sweep   --multi N --knob SET:K [--points -32,-16,0,16,32]   -> table, delta vs the 0 point
soundlab atlas   [--only multis|sounds|patterns] | --find "pad dark sub"   -> the measured profile of every sound (below)
python3 felucca-base/tools/soundlab/compare.py A B   (two metrics files or two folders) -> Markdown before / after table
```

`render` writes `PREFIX.wav` (stereo, 16-bit, 44100 Hz) and `PREFIX.json` (commit, settings, voice log `t0 t1 part note vel sound`, events, `cpu_note_caps`, `voices_max`).
Renders are deterministic: same tree + same options = identical bytes. `multi` N is 1-based (M02 = `--multi 2`). Knobs: SET = AUTO|STD|DUB|..., K = 2..4, DETENTS = steps from the stored value.
The options added by lanes A / B beyond this list (`--dry`, `--solo`, `--fx`, `--ringout`, `--g NAME=VAL`, `--dump-params`) are in `soundlab --help`.
Every metric, its JSON path and the old script it comes from: `METRICS.md`. Proof that soundlab gives the old lanes numbers: `REPRODUCE.md`.
Known limits: per-part figures (steals, odd/even per part) use the mix, not stems (`--solo` stems are not wired into `measure` yet), and the voice log has no voice ids.
Old "chord held" renders (`RR_SHIFT -12`) held only G3: use `--keys chord:G3` to compare; a full chord picks an octave that reaches every note (`keys_octave` in the JSON). `--keys` notes are what SOUNDS (a part TRN is compensated): `--keys-raw` presses the keys as they sound with TRN 0 so TRN shows, `--keys-turn MS` presses the keys one after another in the order given (PRIO, arp ORDER).
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

1. **Drone low end (why M17 / M18 have no bass).** `C render --multi 17 --bars 16 --out P && C measure P`: read `spectrum.bands_pct.lt150` (and lt60). Held: `--keys chord:G3` (the old figure) or `chord:G3,Bb3,D4`. Gate used: >= 20 % under 150 Hz, LUFS +-1.5 dB, true peak < 0 dBFS.
2. **Dub space (does M02 sound dubby).** `C render --multi 2 --bars 16 --solo 1 --fx delay --ringout 8 --out P && C measure P`: `space.echo.hf_drop_db_per_repeat` (negative = darker), `time_to_minus40db_s`, repeats above -40 dB; `--fx full` for the mix, `side_mid_db` for width. M02 gave -1.7 dB per repeat (REPRODUCE.md).
3. **Rubs (minor-second clashes between parts).** `C measure P` and read rub % in REL and ALL; `C rules P` rule 1 shows the pitch sets of both parts.
4. **Sharpness / register (is a lead too bright or a bare square).** Read odd-harmonic share, whistle test, attack sharpness, centroid and per-part p90 / highest note. A bare square or a persistent narrow peak fails rule 6.
5. **Knob strength (does a genre knob do enough).** `C sweep --multi 2 --knob DUB:4`: the table is each metric at -32 ... +32 detents and its delta vs 0. A knob that moves no metric by more than the metric's noise (re-render the 0 point twice: it is exactly equal, so any delta is real) is too weak.
6. **TIME click.** `C render --multi 2 --bars 4 --keys chord:C4,Eb4,G4 --g DTIME=9 --fx-time-step 2:+1 --control --out P`, then `C measure P`: `click.steps[]` has `window_db`, `control_window_db`, `excess_db`, `click`. Loop i over 0..16 with +1 and -1 for the 32-change set (REPRODUCE.md: 0 clicks).
7. **The 18-multi table.** For N in 1..18 and keys `none` / `chord:G3,Bb3,D4` / `--tide evolve --style <suggested>` (`0 0 1 1 1 4 4 1 2 2 2 2 4 3 1 3 5 5` = DUB DUB DEEP ...): render + measure into one folder per tag, then `compare.py folder_before folder_after`. Do not write a new loop script: `sh felucca-base/tools/soundlab/table18.sh TAG` if lane A shipped it, else a 5-line shell loop.
8. **Before / after a change.** Build the baseline tree and the changed tree (each its own `soundlab`), render the same options into two folders, `compare.py before after`. Same options + same seed = every difference is the change. Report numbers with the commit from the JSON.
9. **TIDE-proof (rule 9).** `--tide travel --style <its style> --bars 16` and for End A / mid / End B `--morph a|mid|b`; `C rules P` must stay PASS for rules 1, 4, 7.

## Atlas: look here before rendering

`felucca-base/tools/soundlab/atlas/` holds the measured profile of every factory sound (171: C3 note, C minor chord, End A, End B), multi (18: no key and under its STYLE, per part) and pattern (notes, register, density): JSON per item, `INDEX.md`, and one self-contained `atlas.html` (sortable, filterable, no libraries).
`soundlab atlas [--only multis|sounds|patterns]` rebuilds it (run it after any sound change; about 7 minutes on this PC, one render at a time). `soundlab atlas --find "pad dark sub"` or `--find "centroid<500 sub>10%"` prints the matches with their numbers.
Tags come from the numbers (thresholds in `atlas.py`, `tags_of`). Sounds, from the C3 held note (envelope figures of the held bar only; `decay_ms` stops near 1500 = never fell 20 dB): sustained = decay >= 1400 ms or attack >= 100 ms. **stab** not sustained, decay < 400 ms; **pluck** not sustained, longer; **pad** sustained, attack >= 100 ms; **held** sustained, faster attack; **bass** centroid < 250 Hz and >= 50 % under 150 Hz; **lead** sustained, not bass, centroid >= 600 Hz; **texture** >= 10 partials within 30 dB and >= 20 % above 4 kHz; **square** odd-harmonic share >= 70 %. All: **dark** centroid < 500 Hz, **bright** > 1500 Hz, **sub** >= 10 % under 60 Hz, **wet** tail after note-off >= 30 %, **dry** < 5 %. Multis add **bed** (>= 2.5 notes) and **busy** (>= 8 onsets a bar). Patterns: **sparse** density < 0.3, **busy** > 0.6, **bassline** lowest note <= -7 semitones, **wide** span >= 12, **tied**, **swung**.

## Genre targets

`genres.json`: per genre (dub-techno, deep-house, classic-house, detroit, acid, electro, idm, berlin-school, ambient, drone) numeric target ranges, each with `source` (file:section of work/b9/research/) and `basis` (sourced / example / rule / measured). Where no source gave a number the metric is absent and `rules --genre NAME PREFIX` prints it UNSURE; never fill a gap from memory.
`soundlab rules --genre dub-techno PREFIX` checks the measured values against the ranges. Add a number only with its source line. Each genre also has a `measured` band: soundlab's own numbers over its factory multis in rc5 (PLAY alone, 4 bars); outside it `rules --genre` prints DRIFT (a regression signal, not a genre failure). Refill it with `soundlab atlas --only multis` after an approved sound change.

## Dead parameters

`soundlab deadparams [--engine NAME]` renders every parameter of every engine at its min and max (a held C3, then a C minor chord, then in each context where it could act: an LFO / ENV 2 / insert / EQ band / arp / voice mode / twin filter switched on, the wave slot at each value) and says DEAD (bit-identical), NEAR (< 0.5 dB and < 1/12 octave), LIVE or UNTESTED (sequencer, matrix, sidechain). `soundlab params` lists the descriptors. The table with the reason for each dead one: `DEADPARAMS.md`. About 8 minutes for all 10 engines.

## Emulator sessions (emu_run)

`sh web/emu/emu_run.sh --run web/emu/scripts/NAME.txt --out DIR` (from `felucca-base/`) plays a script on the emulator's device (`web/emu/felucca_web.c` built natively, the same sources the browser runs; CI checks wasm = native): `boot`, `button HOME|SAVE|GLO|OCTUP.. [tap|down|up]`, `enc K1..K4|SELECT N`, `keys C4 Eb4 G4|off`, `master V`, `midi S D1 D2`, `wait MS`, `shot NAME` (DIR/NAME.png, 240 x 240, and its CRC in `DIR/screens.txt`), `rec NAME|off` (DIR/NAME.wav + .json: `soundlab measure DIR/NAME` reads it). Header of `web/emu/emu_run.c` has the full list.
Navigation: HOME cycles MORPH (K2-K4 the genre knobs), TIDE SETTINGS (K1 picks the row, OCT+/OCT- change it), AUTO TIDE; SAVE opens MULTIS (K1 the slot, OCT+ loads); a page persists, so do not press SAVE twice. Demos in `web/emu/scripts/`: `genre_knobs.txt` (K2-K4 down and up on M02 M03 M07 M09 M12 M17), `tide_settings.txt`, `multis_pages.txt` (M01-M18 page and 2 s of audio each), and one per b9 feature: `knobs_row.txt` (KNOBS row, 8 sets), `tide_modes.txt` (EVOLVE / TRAVEL), `usb_audio.txt` (MENU > SYSTEM > USB AUDIO; the menu opens by holding HOME on a page other than HOME), `delay_time_glide.txt` (TIME stepped in the DUB set, audio), `m18_growl.txt` (M18 AMB GROWL, audio). SP3 / SP4 and ROLL TYPE have no screen of their own (ROLL TYPE is gone from the KNOBS row demo). Group `emu_smoke` checks 6 screens against `tests/emu_smoke_ref.txt` (`tests/emu_smoke.sh --write-ref` after a UI change on purpose). The audio is the device's output at MASTER `master V`; its level is not soundlab's.

## Tests and checks

- `./host_tests.sh soundlab`: M02 for 2 bars rendered twice must give bit-identical WAV and JSON; `measure` must match `tests/soundlab_ref.txt` within 0.1. If you change firmware sound on purpose, re-generate the reference (`tests/soundlab_ref.sh --write`) and say so in the commit.
- Reproduction of old answers (M17 under 150 Hz, M02 HF drop per repeat, K4 click) is documented in `REPRODUCE.md`.

## Rules for the next lane

- Add a metric: C in `sl_metrics.c`, one line here, a number in the reference. Add a render option: `sl_render.c` + this README.
- Never say "free headroom" from a host CPU figure. Never hand the owner WAVs to audition: accept by metrics, the owner hears it on the device.
- LF line endings only. No new dependencies: nothing that needs numpy or matplotlib.
