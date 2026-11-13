# Live-record audit tasks R1 to R3: briefs for the agent that does them

Companion to `LIVEREC_AUDIT.md` (same folder; findings L1-L4, evidence in `probe/`). Each brief stands on its own.
The ROADMAP rows link here by anchor (`TASKS.md#r1`).

## Rules for every task

- **Tree:** firmware is `felucca-base/` in the DUELL checkout. Branch from `integ` (408160b or later, worktree
  `work/wt/integ`); one branch per task, `fix/liverec-R<n>-<slug>`, in its own worktree `work/wt/liverec-R<n>`.
- **Sound:** playback of every existing pattern is unchanged; only what REC writes, and whether the stand-in keeps
  playing while REC is armed, change. `sh jam/check_off.sh` must stay PASS (all code is under `DUELL_TIDE`; if a
  change lands in plain-DUELL code, stop and report instead of re-pinning).
- **Build and test (WSL Ubuntu, venv `~/dv`):** `cd felucca-base && python3 tools/build.py --gen-only`, then
  `sh host_tests.sh <groups>`; full suite before handing back. Device image in Git Bash:
  `. jam/env.sh; cd felucca-base; python tools/build.py` (report its size; app slot 581,564 B, about 2.4 KB free at
  b2, so keep each task small). LF line endings.
- **Probe:** `work/audit/liverec-audit-2026-11-08/probe/liverec_probe.c` reproduces every finding; its
  `build.sh` shows the compile line. R3 turns it into a real test group.
- **Never** flash, touch COM ports, push, tag or merge. Commit with the configured identity. CHANGELOG-TIDE entry
  under the next beta heading.

---

## R1. Stop the pattern damage; sequences keep playing while REC is armed <a id="r1"></a>

**Priority** P1. **Size** S. **Findings** L1, L2 (guard part). **Safe for b3:** yes (no playback change).

**Problem.** On an ARP SEQ part (P_AMODE == AM_SEQ_ID, 15) recording writes the arp's transposed output into the
pattern the arp reads, so a held key on a MONO sound walks the pattern up to note 127 in 3 loops, a POLY sound
gets inert extra notes, a firm key accents every step. To avoid the stand-in recording itself, `tide_seq_ghost`
stops the stand-in whenever any part is armed, so REC silences both parts' sequences.

**Files.** `firmware/src/seq.c` `arp_chord` (~324-338), `arp_seq` (~373-388), `arp_chord_off` / `arp_off`
(~311-322, ~399-408), the classic-arp record at ~611-614, `input_on` (~825-842), `cin_keep` (~808);
`firmware/src/tide_seq.c` `tide_seq_ghost` (~45-48); `firmware/src/seq_dialogue.c` `phrase_available` (~26-36);
`firmware/src/ui_input.c` `rec_tap` (~445-464).

**Steps**
1. A helper in seq.c, `static int rec_arp_on(const track_t *t) { return rec_on(t) && t->p[P_AMODE] != AM_SEQ_ID; }`
   (use the raw P_AMODE as `seq_play` does, so MODULES' SEQ OFF does not change which path records). Use it in
   `arp_seq`, `arp_chord`, the classic arp record (~613), `arp_off` and the `arp_chord_off(t, rec_on(t))` call:
   an ARP SEQ part's arp never records. Classic arps still record what they play (documented behaviour).
2. `input_on` already skips `rec_note` when `arp_mode(t)` is on; leave it. Result: keys on an armed ARP SEQ part
   transpose as when not armed and write nothing (answer B of Q1, until R2).
3. `tide_seq_ghost`: drop `!song.rec`. The stand-in can no longer record itself (step 1), so it plays while armed.
   `phrase_available` (seq_dialogue.c:30): drop the `if (!song.rec) return 1;` special case accordingly, so the
   prediction treats an armed ARP SEQ part like an unarmed one (read the function first; keep its held-key branch
   for the case without a stand-in).
4. `rec_tap`: when the armed part (song.sel) has P_AMODE == AM_SEQ_ID, `ui_message("ARP SEQ: KNOBS RECORD")`
   (knob moves still record parameter locks via motion.c; keys do not). Check the message fits the toast width the
   other REC messages use. Under `DUELL_TIDE` only if ui_input.c's REC path is plain-DUELL code (check_off).
5. Tests: `tests/tide_controls_test.c:423-424` expects silence while recording: change it to "seq ghost: recording:
   keeps playing, the pattern unchanged" (assert arp_note changes and a step hash is unchanged after 2 loops).
   `tests/tide_dialogue_test.c:218-219` ("REC disables the stand-in"): change to the new rule. Add probe cases H1,
   H2b, H3 as asserts (both parts keep playing armed; MONO + E4 held 3 loops leaves note[0] of all 16 steps as
   before; velocity 120 sets no SF_ACCENT) in the group R3 creates, or in `tide_controls` if R3 is not done yet.

**Acceptance.** Probe H1: both parts change note while armed. H2/H2b/H3/H4: the step dump after the hold equals
the dump before. H5 (arp OFF) unchanged: tap at step 5 adds 64 to step 5. Full host suite green, check_off PASS,
image size reported.

---

## R2. Keys record into an ARP SEQ pattern, heard as played <a id="r2"></a>

**Priority** P2. **Size** M. **Depends on** R1, and owner answer **A** to Q1 of `LIVEREC_AUDIT.md` (if the owner
answers B, this task is dropped and R1 is the final behaviour). **Finding** L2. **Sound:** playback unchanged;
new behaviour only while armed.

**Behaviour.** On an ARP SEQ part, armed and playing: the pattern keeps playing from its stand-in (or the last held
chord); a key sounds directly (no transpose) and is added to the step nearest on the arp's grid; when the
stand-in later plays that step it reproduces the key's pitch.

**Files.** `firmware/src/seq.c` `input_on` / `input_off` (~825-900), `rec_note` (~718-766), `rec_hold` /
`rec_release` (~769-805), `arp_fire` (~578-616), `arp_step` (~626-688), `arp_seq_notes` (~341);
`firmware/src/tide_seq.c` `tide_seq_chord` (~53-78, `tide_chord`), `tide_arp_idx` (seq_poly.c ~94-119);
`firmware/src/core.h` track_t (`rskip*`, `rh_*` fields ~614-625).

**Steps**
1. `input_on`: if `rec_on(t) && t->p[P_AMODE] == AM_SEQ_ID` (and not in the count-in), do not `arp_add`; call
   `trk_note_on` and a new `rec_arp_note(t, note, vel)`. `input_off` mirrors it (`trk_note_off`, the hold's
   release). Keep `nheld` / `arp_phys` untouched so the stand-in keeps running (`tide_seq_chord` returns the
   stand-in only when no key is held: check that the bypassed key is not counted as held there).
2. `rec_arp_note`: root = the lowest note `tide_seq_chord` would hand the arp now (remembered chord, else C4's
   chord_make root, else 60). `stored = clamp(note - root + 60, 0, 127)`. Step index = `tide_arp_idx(t, arp_idx, len)`
   for the arp step playing, or the next one when `arp_pos` is past half of the current arp step's duration
   (`period + ±swing` exactly as `arp_step` computes it, RATE = P_ARATE). MONO / LEGATO / UNISON: replace note[0];
   POLY: an empty step gets the note as note[0], a filled one appends (inert in ARP SEQ, as today). Velocity:
   s->vel = max, SF_ACCENT above 110, as `rec_note`.
3. Double trigger: when the key went into the *next* arp step, that step must not fire the same note again when
   the arp reaches it in this pass. Add an arp-side skip (one byte idx + up to 4 notes, like `rskip`; reuse the
   `rskip` fields if `seq_play`'s AM_SEQ early return guarantees they are unused on such a part, and say so in a
   comment).
4. Holds: a key held across arp steps turns the following arp steps into ST_TIE (`arp_seq_tie` already honours
   ties). Drive the existing `rh_*` state from `arp_fire` for AM_SEQ parts instead of `seq_tick`'s `rec_hold`
   call (that call must then skip AM_SEQ parts so the sequencer cursor never writes ties into an ARP SEQ pattern).
   Same stop rule: not more ties than `len - 1`.
5. POLY + TIDE stretch (`tide_poly_on`, seq_poly.c): write through the same index mapping the arp reads
   (`tide_arp_idx`), and check finding 36 of the round-2 audit (arp_idx restart mid-stretch) cannot misplace the
   write; if it can, end the stretch on arm (seq_poly.c already ends a stretch on `song.rec`, audit C10: confirm).
6. Tests (in R3's group): stand-in from C4, ARATE == SDIV 1/16: tap E4 at arp step 5 → step 5 note[0] (MONO) = 64
   and afterwards the stand-in plays 64 there; remembered chord E4 (root 64): tap G4 → stored 63, plays 67; tap
   past the middle of a step → next step, played once (count note-ons); hold E4 3 steps → 2 TIEs, then release
   early in the third → that step restored; ARATE 1/8 with SDIV 1/16 → index from the arp grid; MONO hold 3 loops
   → no drift (values identical after loops 1, 2, 3).

**Acceptance.** All step-6 tests pass; R1's tests still pass; host suite green; check_off PASS; image size
reported (expected a few hundred bytes; stop and report if over 1 KB). Owner checks on the unit: M02, arm part 1
on STEP, play a few notes over the running pattern, disarm: the pattern plays them back as played.

---

## R3. Live-record test group and docs <a id="r3"></a>

**Priority** P2 (the test group with R1; docs after R1 or R2). **Size** S. **Findings** L3, L4.

**Steps**
1. Move `work/audit/liverec-audit-2026-11-08/probe/liverec_probe.c` into `felucca-base/tests/liverec_test.c`:
   turn the prints into CHECKs (counted failures, exit code as the other `*_test.c`), keep H5 (arp OFF) as the
   regression net for the plain path, add R1's / R2's cases as they land. Register group `liverec` in
   `host_tests.sh` (its default list and the CI shard that runs `tide_controls`).
2. `docs/QUICKSTART.md:47` REC paragraph: one sentence for ARP SEQ parts (R1 only: "on a part whose pattern plays
   without keys (ARP SEQ) the pattern keeps playing and the keys transpose it; REC records knob moves there";
   with R2: "the keys add notes to it, as you play them"), and one for L3: "a key held for a whole loop fills the
   pattern with that note".
3. `docs/TIDE_GUIDE.md`: in the M02 / "Press PLAY" paragraph (~101), the same ARP SEQ sentence.
4. `CHANGELOG-TIDE.md` under the next beta: **Fixed:** "REC no longer stops the sequences, and no longer rewrites
   an ARP SEQ pattern from the arp's own notes" (+ R2's **New** line if done).
5. `jam/check_docs.py` must stay green.

**Acceptance.** `sh host_tests.sh liverec` green on WSL and in CI; docs say what the firmware does.

---

## R4. REC on HOME <a id="r4"></a>

**Priority** P2. **Size** S. **Finding** L5. **Depends on** owner answer to Q2 of `LIVEREC_AUDIT.md`.
**Safe for b3:** yes (UI only).

**Files.** `firmware/src/ui_input.c` `rec_is_hold` (~414-417), `rec_hold_tap` (~431-444), `rec_tap` (~445-464);
the HOME page's family in `firmware/src/ui.c` / page tables (find HOME's `page_t` and whether `ui.perf` covers it);
`tests/ui_test.c` / `tests/perform_test.c` REC cases (grep `rec_tap`, `HOLD ON`).

**Answer A (keep HOLD, say how to record).** In `rec_hold_tap`, when the page is HOME, the message reads
`HOLD ON · SEQ+REC RECORDS` / `HOLD OFF · SEQ+REC RECORDS` (measure the toast width against the longest existing
toast; if it does not fit, `SEQ+REC = RECORD` as a second toast line or the shorter `HOLD ON (SEQ+REC: REC)`;
pick the one that fits and say which). Confirm on the host that holding SEQ and tapping REC on HOME arms the
selected part (song.rec bit, header REC mark) and that a stopped transport starts (rec_tap 459-460): add that as a
ui_test case.

**Answer B (REC records on HOME).** `rec_is_hold` returns 0 on HOME (and only there: PERFORM keeps its own REC,
`perform_test.c:202-212` "PERFORM's keys record nothing" must still pass). HOLD on HOME is then unreachable from REC:
say so in QUICKSTART:47 and the first-start guide page that teaches REC (CHANGELOG-TIDE line 41/86 mention it).
Tests: REC on HOME toggles song.rec, not P_AHOLD; on a sound page it still toggles HOLD.

**Both.** QUICKSTART:47 one sentence; CHANGELOG-TIDE line under the next beta; full host suite green; check_off PASS
(if ui_input.c's REC path is plain-DUELL code, put the change under `DUELL_TIDE` or stop and report).
