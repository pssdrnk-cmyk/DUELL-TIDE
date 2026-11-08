# Live recording while a sequence plays: audit 2026-11-08

Scope: REC armed while PLAY runs (`seq.c` rec_note / rec_hold / rec_release, arp recording, `tide_seq.c` stand-in,
`ui_input.c` rec_tap). Tree: `work/wt/integ` 408160b (= b3 baseline). Read-only; the probe ran in a throwaway
worktree `work/wt/probe-liverec` (detached 408160b, only the probe file added). Briefs: [TASKS.md](TASKS.md).
ROADMAP rows: [ROADMAP_ROWS.md](ROADMAP_ROWS.md).

## Verdict

Live recording works for a part whose arp is OFF (steps play directly): notes land on the nearest step, overdub
adds, held notes tie. It is **broken on ARP SEQ parts**, which is how the default multi (M02 DUB CHORD) and the
factory multis with sequences play ("plays on PLAY with no key held", TIDE_GUIDE). On those parts:

1. Tapping REC **silences both parts' sequences**, not only the armed one (L1).
2. Holding a key while armed **records the arp's transposed output back into the pattern that drives it**: on a
   MONO sound the pattern climbs every loop until every step is note 127 (destroyed in 3 loops); on a POLY sound
   it appends notes the ARP SEQ never plays, and a firm key (velocity > 110) accents every step (L2).
3. The write goes to the sequencer's step cursor while the arp plays from its own cursor, which restarts at
   step 1 on a key press: the recorded notes land rotated (L2).

So what the owner noticed ("weird or broken") is real. Nothing here is a crash; the damage is to the pattern in
RAM (saved only if the user saves the multi / pattern afterwards). A "recording silences the stand-in" test exists
(`tests/tide_controls_test.c:423`), so finding 1 was a deliberate guard against finding 2, applied too widely.

## Findings

| # | Pri | Finding | Evidence | Fix (brief) |
|---|---|---|---|---|
| L1 | P1 | `tide_seq_ghost` tests `!song.rec` (any part armed), so arming part 1 also stops part 2's stand-in; the armed part's own stand-in stops too, so a sequence goes quiet the moment REC is tapped. `seq_dialogue.c:30` `phrase_available` has the same global test. | Probe H1: before arming both parts change note 8 times a second; armed: 0 and 0 over 4 s; after disarm 8 and 9. `tide_seq.c:45-48`. | [R1](TASKS.md#r1) |
| L2 | P1 | On an ARP SEQ part (AMODE 15) `arp_seq` calls `rec_note(t, out[i])` with `out[i]` = held key + step offset (`seq.c:381-386`), and `arp_seq_notes` reads the same `t->step[]` as offsets (`note[0] - 60`, `seq.c:341-364`). The write uses `seq_idx`/`seq_pos`/SDIV (`seq.c:718-727`); the arp reads `arp_idx`, reset to the first step on a new key (`seq.c:237-239`). | Probe H3 (MONO, E4 held 3 loops): note[0] 60 62 63 65 … → 92 94 64 66 … → 124 126 96 … → all 127; heard a rising staircase. H2 (POLY): 12 of 16 steps get a second note, playback unchanged (inert). H2b: velocity 120 sets SF_ACCENT on 16/16 steps, so all play at 127 afterwards. H4: key at step 5 → step k receives arp step (k-6) mod 16. | Guard [R1](TASKS.md#r1), real fix [R2](TASKS.md#r2) |
| L3 | P3 | Arp OFF, any voice mode: a key held longer than one loop turns the whole pattern into that note + TIEs (`rec_hold` stops at `rh_ties + 1 >= len`). Stable, by design, but undocumented and easy to do by accident. | Probe H5 MONO: after each of 3 loops note[0] = 0 except step 7 = 64 (ties elsewhere). | Doc line only, in [R3](TASKS.md#r3) |
| L4 | P3 | QUICKSTART's REC paragraph (`docs/QUICKSTART.md:47`) does not say what REC does on an ARP SEQ part; TIDE_GUIDE does not mention REC at all. | Grep of docs/*.md, CHANGELOG-TIDE.md. | [R3](TASKS.md#r3) |

| L5 | P2 | "REC doesn't work on HOME" (owner, 2026-11-08). By design since 0.2.9: outside the sequencer pages (STEP, SEQUENCER HELP, PARAMETER LOCKS) and the SEQ layer, a tap of REC is the HOLD switch (`ui_input.c:409-416` rec_is_hold, `rec_hold_tap` 431-444), toast HOLD ON / HOLD OFF. On an ARP SEQ part playing its stand-in, HOLD changes nothing audible, so REC looks dead. Holding SEQ and tapping REC arms from any page (`ui.layer == LAYER_SEQ`), but nothing on HOME says so. | Code reading; QUICKSTART:47 documents it, the screen does not. | Owner question Q2, [R4](TASKS.md#r4) |

Control (probe H5, arp OFF, POLY): a 300 ms tap at step 5 adds 64 to step 5 and ties into step 6 (held into it,
as documented); no other step changes. The arp-OFF path is sound.

Classic arps (UP, DOWN, CHORD …) recording what the arp plays (`seq.c:611-614`, `335`) was not probed in depth:
there the steps play directly, so recording the arp's output is the documented "what plays back is what was
heard". It does not feed back into the arp. Not a finding.

## Design for the real fix (L2, task R2)

The rule the user can rely on: **on an ARP SEQ part, while armed, the pattern keeps playing and a key adds that
note to it, heard as played.** Concretely:

- The armed part's stand-in keeps playing (R1 already removes the REC exception).
- While the part is armed and playing, a key does **not** transpose the pattern (no `arp_add`): it sounds directly
  (`trk_note_on`) and is recorded as a step offset relative to the stand-in's root, so playback reproduces it:
  `stored = key - root + 60`, `root` = the lowest note of the chord the stand-in plays (`tide_chord`, else 60).
- The step written is the arp's: index `tide_arp_idx(t, arp_idx, len)`, nearest by `arp_pos` against the ARP RATE
  period (the arp's swing), not the sequencer's cursor.
- `arp_seq` / `arp_chord_off` / `arp_off` stop calling `rec_note` / `rec_release` on AMODE 15 (that is R1's guard,
  kept).
- The just-played key must not also be fired by the arp when it reaches that step in the same pass (an `rskip`
  equivalent on the arp path), and held keys tie by setting the following arp steps to ST_TIE (the existing
  `rec_hold` logic driven from `arp_fire` instead of `seq_tick`).
- MONO / LEGATO parts replace the step's note[0]; POLY appends (ARP SEQ plays only note[0] today, so for POLY the
  first note written into an empty step is what plays; appended notes stay inert, as now. Real chord steps in ARP
  SEQ are out of scope: ROADMAP "No real chord steps in ARP SEQ").

The owner question below decides whether this design or the guard alone ships.

## Owner question (player-facing)

**Q1.** When you record on a part that plays its pattern without keys (most factory multis), what should the keys
do while REC is on?
- **A (recommended):** the pattern keeps playing unchanged, and each key you play is added to it as a new note,
  sounding as you played it.
- **B:** keys keep transposing the pattern as they do when not recording, and REC records only knob moves
  (parameter locks) on such a part; the display says KEYS DON'T RECORD ON ARP SEQ.

R1 (the guard, sequences keep playing) is needed either way; R2 only with answer A.

**Q2.** On HOME, a tap of REC is the HOLD switch, so it does not start recording. What should it do?
- **A (recommended):** stay HOLD (nothing you use live is lost), but the tap's message says how to record:
  HOLD ON · SEQ+REC RECORDS. 
- **B:** REC starts recording on HOME too; HOLD stays on REC on the sound pages only.

Task R4 does either answer.

## Evidence files

- `probe/liverec_probe.c`: the host probe (ui_test.c harness, `UI_TEST_NO_MAIN`), H1-H5.
- `probe/liverec_probe.out`: its output (all numbers quoted above).
- `probe/build.sh`: the WSL build (`~/dv`, `tools/build.py --gen-only`, the gen steps of `host_tests.sh` 247-255,
  `cc … tests/liverec_probe.c`).
