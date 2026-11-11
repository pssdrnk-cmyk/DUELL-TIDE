# soundlab metrics: every figure of `PREFIX.metrics.json`, its definition and the old script it comes from

Lane B's reference (lane C's README links here). Names are JSON paths. `mono` = (L + R) / 2. `bar range` = `--from BAR --to BAR`
(default: bar 0 to the end of the file, tail included). A figure that cannot be measured is `null`.
Old scripts: DM `work/b9/dub/dubmetrics.py`, SP `space.py`, RU `rules.py`, M18 `work/b9/sound/measure18.py`, RB `work/b9/revoice/B/{metrics,rules}.py`,
AN `work/reel-2026-11-10/sound-check/tools/analyse.py`, QU `work/audit/sound-vs-base-2026-11-09/tools/quality.py`, CK `work/b9/knobs/click/analyse_click.py`.
All of these were checked against the old numpy code on the same WAVs (LUFS, bands, side/mid, bed, rubs, hits, whistle, timbre, rhythm, attack: equal to the printed digits;
true peak within 0.001 dB). All 18 multis of `work/b9/sound/wav18` (new, PLAY) were run through M18 and soundlab: LUFS, true peak, side/mid, <120 Hz, bed, silence agree in all 18; the rub differs in M04 (50.79 old, 50.62 new) and M06 (68.10, 67.30) only because the old script stopped counting at the last log event and soundlab counts to the end of the render (the old code with a closing event gives 50.62 and 67.30). The low-band onsets differ by which bars are counted (soundlab counts every bar of the range).

## input

`PREFIX.wav` (16-bit PCM, mono or stereo, any RIFF layout) and `PREFIX.json`: `sample_rate`, `bpm`, `bars`, `voices[]` {`t0` note-on, `t1` note-off, `t_end` voice end (optional),
`part`, `note`, `vel`} in seconds, `events[]` {`t`, `kind` containing "time" (e.g. `fx_time`), optional `sample`}, optional `bar_starts[]` (seconds), optional `control` (prefix of the control render).
Bars start at k * 240 / bpm unless `bar_starts` is given. Without `t_end` the ALL mode equals REL and simultaneous notes stop at note-off.

## level (`level.*`)

| name | definition | from |
|---|---|---|
| `lufs` | ITU-R BS.1770 K-weighted, 400 ms blocks every 100 ms, gates -70 absolute and -10 relative | RU lufs |
| `true_peak_dbfs` | 4x oversampled (Kaiser sinc) peak, dBFS | RU true_peak |
| `sample_peak_dbfs`, `rms_dbfs`, `crest_db` | peak, 10 log10 of the mean square of both channels, difference | AN |
| `dynamic_range_db` | 95th over 10th percentile of the 20 ms RMS frames | AN dyn_db |
| `silence_audio_pct` | share of 20 ms frames more than 30 dB under the 95th-percentile frame | AN silent_pct |
| `clipped_samples`, `dc_dbfs` | samples at +-32767, larger channel mean | QU |

## spectrum (`spectrum.*`), of the whole mono range, unwindowed power spectrum

| name | definition | from |
|---|---|---|
| `bands_pct.{lt60,60_150,150_400,lt120,lt150,lt250,lt300,gt2k,gt4k,gt6k}` | share of the energy in the band of all energy above 20 Hz (`lt60` = 20..60 Hz; `gt6k` = 6..22 kHz) | DM band_pct |
| `reel_bands_pct` | five bands 0-120, 120-500, 0.5-2k, 2-6k, above 6k, share of all energy | AN bands |
| `centroid_hz`, `centroid_oct_re_c4` | power-weighted mean frequency of the range (all bins), and log2 of it over 261.6 Hz | AN bands |
| `centroid_bar_{min,max}_hz`, `centroid_bar_range_oct` | per bar, Hann window, f >= 20 Hz; the range as log2(max / min) = octaves of tone movement | DM bars_trace |
| `level_bar_range_db`, `side_mid_bar_range_db` | range over the bars of the bar's RMS dBFS and of its side/mid | DM, SP |
| `per_bar.*` | the three series, one value per bar (null for a bar under 4096 samples) | |
| `whistle.spike_db` | highest bin 2..12 kHz (every 7th) over the median of the 74 bins around it, 32768-sample windows, max over windows. Rule 6 limit 12 | RU spike |
| `whistle.narrow_frames_pct`, `narrow_longest_s` | 4096-sample frames whose strongest 2..12 kHz bin is within 25 dB of the frame's strongest and over 12 dB above its neighbours; longest run | RB hf |

**The whistle figures cannot tell a held partial (organ, bell) from a resonance whistle.** Every tonal sound flags (reports DUB-REVIEW, REVOICE-B, B9-LOWEND), so `rules` never fails rule 6 on them.

## time (`time.*`)

| name | definition | from |
|---|---|---|
| `bed.{p1,p2,both}_mean`, `*_max` | voices sounding (note-on to voice end), time-weighted mean and most at once | M18 bed, AN nv |
| `bed.*_silence_pct`, `silence_notes_pct` | share of the time with no voice sounding (rule 3: at most 5 %) | M18 |
| `bed.fullest_set_p*` | notes sounding when the total first reached its maximum | |
| `low_onsets_per_beat` | below 120 Hz (brick-wall), per beat of the bar: share of bars in which the band starts there (rise of 6 dB within 100 ms, above -45 dBFS) | M18 low_onsets |
| `sub.level_dbfs`, `sub.mod_depth_std_over_mean` | below 100 Hz: RMS, and std / mean of its 100 ms RMS (thump depth) | RU |
| `onsets_per_bar.{p1,p2,all}` | onset groups (notes of a part within 5 ms) a bar | AN rhythm |

## parts (`parts.p1.*`, `parts.p2.*`), from the voice log; timbre from the mix

`notes_per_onset`, `note_len_median_ms` (note-on to note-off), `gate_median_16ths`, `sound_median_16ths` (to voice end), `blip_pct` (sounding length under half the gap to the next onset),
`gap_fill_median`, `share_on_offbeat_16ths_pct`, `ioi_1_16th_pct`, `swing_pct`, `vel_*`, `jump_*` (lowest note of consecutive onsets): AN rhythm.
`attack_ms`: onset to 90 % of the peak of a 2 ms RMS envelope, median of the first 40 onsets (AN attack_ms).
`harmonics_within_30db`, `odd_even_db` (harmonics 3,5,7,9,11 over 2,4,6,8,10 of the lowest note at 60 ms), `odd_harmonic_share_pct`, `fundamental_share_db`, `harmonic_centroid`,
`tone_move_pct` (centroid at 300 ms against 60 ms), `env_move_pct` (150 ms against 20 ms), `held_centroid_cv_pct`: AN timbre / movement.
The old scripts used the part alone; the mix is used here, so **only onsets where the other part is silent are used when there are any** (`timbre_from_clean_onsets`); otherwise all onsets, and the figure is contaminated by the other part.

## pitch (`pitch.*`)

`p1/p2.{median,p90,highest,lowest,set}` of the note-ons. `rubs_REL`, `rubs_ALL`: REL counts a note from note-on to note-off, ALL to the voice end (release tails; the old logs' END).
`minor2_or_9_pct` = share of the time both parts sound in which a note of each is 1 or 13 semitones apart (M18 rub; this is the figure the 18-multi table used);
`minor2_or_maj7_any_octave_pct` = any pair a minor 2nd / major 7th apart at any octave (RB rubs); `both_parts_sounding_pct` of the whole range; `within_p1_pct`, `within_p2_pct` = the same 1 / 13 rub inside one part.
`minor2_pitchclass_pairs`: pitch-class pairs of the two sets a semitone apart (RU minor2nd).

## space (`space.*`), of the mix

`side_mid_db` (10 log10 of side over mid energy; rule 4: -11 or wider), `lr_correlation` (np.corrcoef), `air_gt6k_pct`.
`hits.*`: the hits of part 1 (else part 2), notes within 400 samples: `decay_to_minus20db_ms_median` (5 ms RMS frames to 10 % of the peak, DM stab_decay), `centroid_hz_median`, `rolloff85_hz_median`,
`lt150_pct_median`, `lt300_pct_median`, `gt2k_db_re_total_median`, `centroid_range_oct` over the first 250 ms of each hit (DM stab_spec); `gap_fill_db_re_hit_median` (last 150 ms before the next hit against the hit's first 200 ms, SP gaps);
`energy_after_first_200ms_pct` (DM tail_share).
`after_noteoff.*`: share of the energy and the level (dB) while no key is held, against while a key is held (note-on to note-off, all parts).
`echo.*`: the hit with the longest quiet gap after it (at least 1 s); the delay time is the dotted eighth of the bpm or `--delay-ms`. `repeat_level_db_re_hit[]`, `repeat_gt2k_db_re_window[]`, `repeat_lt300_pct[]` in 100 ms windows
at k * time (DM tail), `repeats_above_minus40db`, `hf_drop_db_per_repeat` = (3rd - 1st) / 2 of the >2 kHz share, `time_to_minus40db_s` (100 ms RMS falls 40 dB under the first repeat's window), `autocorrelation_lag_ms`, `tail_level_db_re_hit_at_0.5_1_2_3_4s`.
**These need room after the hit: render a hit followed by 4 s of silence.** They are of the mix, with the other parts and the reverb in the windows.
`quietest_5pct_frame_dbfs`, `loud_to_gap_range_db`: 50 ms frames (SP frames).

## quality (`quality.*`), QU

`second_difference_click_db` (worst 2 ms block of the second difference over the median loud block), `end_cut_dbfs` (level of the 5 ms before the signal ends in digital zero), `tail_last_200ms_dbfs`, `gt16k_db_re_total` (1..6 s), `lr_correlation_uncentered`.

## click (`click.*`), CK, only when `events` has a delay-TIME step

Per step: `window_db` = largest sample-to-sample jump of either channel within +-5 ms of the step, `control_window_db` the same in the control render (`--control`, `PREFIX.json` `control`, or `PREFIX_ctl`),
`excess_db`, `pre_identical` (the two renders equal before the step), `click` = changed window above -40 dBFS and the control not above it. `any_click`.

## rules (`soundlab rules`), PASS / FAIL / UNSURE

1 rub 1 / 13 semitones (ALL) at most 1 % PASS, 5 % UNSURE; 2 registers 7+ semitones apart or onset density 3x apart PASS, same register and density FAIL; 3 notes sounding 2.5 or more and no-note time 5 % or less; 4 side/mid -11 or wider and the HF per repeat falling (UNSURE without repeats);
5 centroid range 0.1 oct, level range 1 dB or tone move 10 %; 6 whistle spike 12 dB or less PASS, above UNSURE (never FAIL), a square-like lead (odd/even over 10 dB, lowest note 52 or more) FAIL;
7 LUFS in `--lufs-window` (default -24.1 .. -18.1), true peak -1 or less, no clipped sample; 8 FAIL only when all four beats have a low onset in 75 % of the bars; 9 rules 1, 4, 7 of a 16-bar render with TIDE on (else UNSURE); 10 UNSURE (needs the other multis: `compare.py`).
