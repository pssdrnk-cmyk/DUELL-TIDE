# Sound references after DUELL 1.1 (T0)

The twelve sound groups (`reverb halltail locut shimmer stereochorus comp master bassmono eqon dlyev mxstep modinert`) compared bit-exact hashes of renders made before 1.1. DUELL 1.1 changed the sound on purpose, so those hashes could not match. This page records how the new references were chosen.

## Method

1. Stock DUELL 1.1 (tag `v1.1`) and jam11 give the same hashes in all twelve groups. So the differences come from 1.1 itself, not from TIDE.
2. DUELL 1.0 (tag `v1.0`) passes every sound hash except comp / master / bassmono. Their busy two-part render was already stale in 1.0.
3. Each hashed render was dumped from 1.0 and from 1.1 and compared as audio. `jam/soundref/dmp.h` is force-included into the tests and the hash loops are edited to `h ^= (uint32_t)DMP(x)`. `jam/soundref/acmp.c` then compares the two dumps segment by segment and channel by channel. It reports RMS level, peak, the residual (rms(1.1 - 1.0) relative to the 1.0 rms), the largest 100 ms envelope difference (only in windows above -60 dBFS), the band energy in five bands (<250 Hz, 250 Hz-2 kHz, 2-6 kHz, 6-12 kHz, >12 kHz), and the tail end (the last |x| >= 4 LSB).
4. Every difference was checked against 1.1's source. The changes are in the single 1.1 commit (`16d0223`), each with its own comment in fx.c / fx_bus.c / mod.c / ui_draw.c.

## Results (1.0 -> 1.1)

| Group | Level | Residual | 100 ms env | Bands | Cause in 1.1 (intended) |
|---|---|---|---|---|---|
| reverb (SPRING / ROOM hash) | 0.00 dB | -53.5 dB | 1.2 dB | 0.0 dB | REVERB_FIX: comb damping and delay feedback rounded to the nearest (`mulq15r`); comb write and all-pass towards zero (`sh_tz`) |
| reverb (ROOM vs reference) | - | - | - | - | The test's reference model now uses the 1.1 ROOM. It is bit-exact |
| halltail (5 models) | 0.00 dB | -44.7 dB | see note | 0.0 dB | REVERB_FIX. Note: 1.0 holds a stuck tail at -58 dBFS for ~10 s (102 windows). 1.1 ends that tail in exact zeros, which is the bug the fix was made for |
| locut (LOCUT 0, 6 models) | -0.04 dB | -20.4 dB | 2.5 dB | 0.1 dB | REVERB_FIX (ROOM), the SHIM rework, and RESO's in-loop soft limit (`RS_KNEE`). SPRING, HALL and PLATE are unchanged |
| shimmer (bus A/B) | -0.1..-0.2 dB | -10 dB | 4.6 dB | 2.5 dB | SHIM: octave-up line and return low-passed (`sh_lp`, `sh_rlp`), feedback `SH_AMT` 13100 -> 11000 (-1.5 dB), gates `SH_GATE` 24 -> 4 and `SH_OGATE` 6 -> 2. The tails end ~0.1 s sooner |
| shimmer (whole mix) | -0.14 dB | -10.9 dB | 9.9 dB (late tail) | 0.2 dB | the same; the halo decays faster at the lower feedback |
| stereochorus | 0.00 dB | -66 dB | 0.0 dB | 0.0 dB | delay feedback rounding, 1.1 level stages |
| comp / master / bassmono | 0.00 dB | -43 dB | 0.0 dB | 0.0 dB | mode_test.c already holds the 1.1 value (its items 127 / 144 / 148: the 1.1 reverb and feedback rounding, VS_MAX 3 -> 2) |
| dlyev | 0.00 dB | -61 dB | 0.0 dB | 0.0 dB | delay feedback rounded to the nearest (`mulq15r`) |
| mxstep (SPREAD rest) | 0.00 dB | -65 dB | 0.0 dB | 0.0 dB | BIT DEPTH: the level stages keep the signal's two low bits |

No firmware bug was found. Every audible difference is a 1.1 change that 1.1's source documents.

## Non-audio checks in the same groups (1.1 UI / list changes)

- shimmer: 1.1 (item 125) draws the MODULES notice and returns before the cards, so the card cache keeps its old content. The test now marks the cache empty before the draw and checks that no card was drawn.
- eqon: 1.1's grid card cache is a hash, so the cards are read from the drawn texts (`UI_TEXT_CAPTURE`, as select_test.c). 1.1 writes the names in full: LOW / MID / HIGH FREQUENCY.
- mxstep: the matrix name is "AMP ENV / SUSTAIN" (1.1, owner 2026-10-25: the category and the item in full).
- modinert: 1.1 also removed FILTER ENV's TIMBRE depth (`MD_E2SHP`) from the destination list.
- stereochorus: COST is report-only when the host has no instruction counter (as in locut). It failed on 1.0 too.

Each changed constant keeps its old value in a comment, next to a pointer to this page.

## Reproduce

```sh
git worktree add --detach ../sound-v10 v1.0    # and ../sound-v11 v1.1; python3 tools/build.py in each felucca-base
# in each felucca-base: edit the hash loops to h ^= (uint32_t)DMP(x), then
cc <host_tests.sh CC flags> -include jam/soundref/dmp.h -o d_x tests/x_test.c -lm
DMP_FILE=v10-x.bin ./d_x      # comp / stereochorus: also DMP_SYNC=1 (they fork); no arguments except for reverb
cc -O2 -o acmp jam/soundref/acmp.c -lm && ./acmp v10-x.bin v11-x.bin   # ENVW=1 lists the windows that differ by > 6 dB
```
