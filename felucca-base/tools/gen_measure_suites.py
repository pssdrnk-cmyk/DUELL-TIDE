#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Writes the two bench suites of the measurement kit (tools/tide_measure.py, `jam_dev.py measure`, docs/TESTING.md):

  tools/bench_suites/TIDE-DSP.txt      audit A01: what TIDE's audio DSP (the spectral valley, the wash, the voices' dip, FM6's bridge) costs on the unit, on core 0 and on core 1,
                                       and whether a heavy TIDE scene stays inside the two-core deadline. Needs a bench build with the `tide` words (bench.c bench_con_tide).
  tools/bench_suites/PHYS-MODELS.txt   audit Q14: the cost of one more PHYS note by model (MODAL, STRING, MEMB, SYMP): every factory PHYS sound with 1 and 3 keys, one core, as COST-CAL1.

  python3 tools/gen_measure_suites.py            write both
  python3 tools/gen_measure_suites.py --list     the tests and the minutes, nothing written

A test is the COST-CAL kind (tools/cost_defs.py): both parts the same cleaned sound (`sound N ENGINE DEAR|k`), LAYER, the NOTES budget held open, MODULES SENDS off, no master stage,
KEY SENSE off, a chord held and the CPU measured over most of it. TIDE-DSP runs on both cores (the bench build's default), PHYS-MODELS on one (`tune core1 off`, the budget's table is the
one-core row). `report tide` makes the runner read the stage timers after the result. The TIDE stages are driven in two ways: `tide force PART VALLEY% MOTION%` holds the valley's depth
and GLIDE's movement at fixed values (a fixed load: the cost of the stage itself, per part), and a REAL crossing (a MORPH ROLL and the fader's ramp, or a GLIDE trip) shows the stages as
they come in a scene; the report says how many calls really did the stage's work (`act`) so a scene that did not reach a stage is not judged on it."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import duell_sounds as DS  # noqa: E402  (the factory sounds' definitions: which PHYS sound has which model)

PREAMBLE = ["mode LAYER", "mods ALL ON", "mods SENDS OFF", "master COMP 0", "master WIDTH 100", "master BM 0", "keysense OFF", "budget OPEN"]
PHYS_MODELS = {0: "MODAL", 1: "STRING", 2: "MEMB", 3: "SYMP"}
# the ROLL seeds of the real crossings (found on the host twin: the stage ran; tests/tide_measure_test.py checks them there)
SEED_X = 5
SEED_FM6 = 11


def phys_sounds():
    """[(index, name, model)] of the factory PHYS sounds in the order the unit's `sound 1 PHYS k` counts them (idx)"""
    out = []
    for s in sorted((s for s in DS.SOUNDS if s["engine"] == "PHYS"), key=lambda s: s["idx"]):
        out.append((s["idx"], s["name"], PHYS_MODELS[int(s["p"].get("E1", 0))]))
    return out


def test(name, text, setup, events, measure, tail=0.5, settle=1.5, repeat=2, report_tide=True, one_core=False, mode="LAYER"):
    L = [f"test {name}  {text}"]
    L += [f"mode {mode}"] + [w for w in setup if w.startswith("sound")] + PREAMBLE[1:]      # (the order of COST-CAL1: mode, the sounds, the switches, then what the test sets)
    L += [w for w in setup if not w.startswith("sound")]
    if one_core:
        L.append("tune core1 off")
    L += events
    L += [f"measure {measure[0]} {measure[1]}", f"tail {tail}", f"settle {settle}", "expect report"]
    if report_tide:
        L.append("report tide")
    L.append(f"repeat {repeat}")
    L.append("")
    return L


def chord(notes, t0, t1, ch=1, both=False):
    """the chord on part 1's channel (LAYER: both parts sound it), or `both`: on each part's own channel (DUEL), then the release"""
    L = [f"at {t0}  chord {' '.join(notes)} vel 100 ch {ch}"]
    if both:
        L.append(f"at {t0}  chord {' '.join(notes)} vel 100 ch 2")
    return L + [f"at {t1}  release"]


def tide_tests():
    N4 = ["C3", "E3", "G3", "B3"]
    N6 = ["C3", "E3", "G3", "B3", "D4", "F4"]
    base = ["sound 1 ANALOG DEAR", "sound 2 ANALOG DEAR", "tide wash 0"]
    ev = chord(N4, 0.0, 5.5)
    m = (1.5, 5.5)
    T = []
    T += test("TIDE.BASE", "ANALOG x2, 4 keys, TIDE idle (nothing forced, WASH OFF): the reference of every row below", base, ev, m)
    for p in (1, 2):
        T += test(f"TIDE.VAL.P{p}", f"the valley forced on part {p} (depth 60 %, no movement), WASH OFF", base + [f"tide force {p} 60 0"], ev, m)
    T += test("TIDE.VAL.SPR.P2", "the valley on part 2 with SPREAD 64 (the second buffer), WASH OFF", base + ["set 2 SPRD 64", "tide force 2 60 0"], ev, m)
    for p in (1, 2):
        T += test(f"TIDE.WASH.P{p}", f"the wash alone on part {p} (GLIDE's breath: movement 60 %, no valley), WASH MID", base[:2] + ["tide wash 2", f"tide force {p} 0 60"], ev, m)
        T += test(f"TIDE.VALWASH.P{p}", f"the valley and the wash on part {p} (depth 60 %, movement 60 %), WASH MID", base[:2] + ["tide wash 2", f"tide force {p} 60 60"], ev, m)
    ramp = ["at 1.0  morph 0 127 over 3.0 ch 1", "at 1.0  morph 0 127 over 3.0 ch 2"]   # (DUEL: each part has its own channel and its own fader; in LAYER part 2 does not take its own)
    for w, nm in ((0, ""), (2, ".W")):
        T += test(f"TIDE.X.ANALOG{nm}", f"a real crossing on both parts (ROLL {SEED_X}, 100 %), ANALOG x2, WASH {'MID' if w else 'OFF'}",
                  base[:2] + [f"tide wash {w}", f"morph 1 ROLL {SEED_X} 100", f"morph 2 ROLL {SEED_X} 100"], chord(N4, 0.0, 6.0, both=True) + ramp, (1.0, 5.0), settle=2.0, mode="DUEL")
    T += test("TIDE.X.FM6.W", f"FM6's bridge: a real crossing on both parts (ROLL {SEED_FM6}, 100 %), FM6 x2, WASH MID",
              ["sound 1 FM6 DEAR", "sound 2 FM6 DEAR", "tide wash 2", f"morph 1 ROLL {SEED_FM6} 100", f"morph 2 ROLL {SEED_FM6} 100"], chord(N4, 0.0, 6.0, both=True) + ramp, (1.0, 5.0), settle=2.0,
              mode="DUEL")
    T += test("TIDE.GLIDE.W", "GLIDE once, 8 bars, on both parts, ANALOG x2, WASH MID (the breath, the wash and the voices' dip together)",
              base[:2] + ["tide wash 2", f"morph 1 ROLL {SEED_X} 100", f"morph 2 ROLL {SEED_X} 100", "tide glide on 3 500"], chord(N4, 0.0, 6.0), (1.0, 5.0), settle=1.5)
    heavy = ["sound 1 FM6 DEAR", "sound 2 FM6 DEAR"]
    T += test("TIDE.HEAVY.BASE", "FM6 x2 (DUEL), 6 keys each, TIDE idle: the deadline's reference", heavy + ["tide wash 0"], chord(N6, 0.0, 6.0, both=True), (1.0, 5.0), settle=2.0, mode="DUEL")
    T += test("TIDE.HEAVY.ON", "the same with every TIDE stage running (valley forced on both parts, WASH MID, a real FM6 crossing): inside the deadline?",
              heavy + ["tide wash 2", f"morph 1 ROLL {SEED_FM6} 100", f"morph 2 ROLL {SEED_FM6} 100", "tide force 1 60 60", "tide force 2 60 60"], chord(N6, 0.0, 6.0, both=True) + ramp, (1.0, 5.0), settle=2.0,
              mode="DUEL")
    M16 = ['load multi 4 "LATE RHODES"']   # (reel, 2026-11-10: M04 LATE RHODES, both parts ARP SEQ and played from the held keys; M03, loaded here until then, is JUNO DEEP: two plain patterns. The test ids keep their M16)
    T += test("TIDE.M16.BASE", "M16 LATE RHODES (LAYER), 3 keys, TIDE idle", M16 + ["tide wash 0"], chord(["C4", "E4", "G4"], 0.0, 7.0), (1.0, 7.0), settle=2.0, repeat=1)
    T += test("TIDE.M16.ON", "M16 LATE RHODES with the valley forced on both parts and WASH MID", M16 + ["tide wash 2", "tide force 1 60 60", "tide force 2 60 60"], chord(["C4", "E4", "G4"], 0.0, 7.0),
              (1.0, 7.0), settle=2.0, repeat=1)
    return T


def phys_tests():
    T = []
    for k, name, model in phys_sounds():
        for keys, notes in ((1, ["C3"]), (3, ["C3", "E3", "G3"])):
            T += test(f"PHYS.{k:02d}.{model}.K{keys}", f"PHYS #{k} {name} ({model}), one core, {keys} keys",
                      [f"sound 1 PHYS {k}", f"sound 2 PHYS {k}"], chord(notes, 0.0, 2.4), (0.9, 2.4), tail=0.3, settle=1.2, repeat=2, report_tide=False, one_core=True)
    return T


def seconds_of(lines):
    """the minutes of a block of test lines, as bench_run would take them: (chord end + tail + settle + the runner's 0.9 s of words) x repeat, roughly"""
    tail = settle = rep = 1
    end = 0.0
    for ln in lines:
        w = ln.split()
        if not w:
            continue
        if w[0] == "at":
            end = max(end, float(w[1]))
        elif w[0] == "tail":
            tail = float(w[1])
        elif w[0] == "settle":
            settle = float(w[1])
        elif w[0] == "repeat":
            rep = int(w[1])
    return (end + tail + settle + 0.9) * rep


def main():
    suites = {"TIDE-DSP": tide_tests(), "PHYS-MODELS": phys_tests()}
    heads = {"TIDE-DSP": "# TIDE-DSP (generated by tools/gen_measure_suites.py, audit A01): what TIDE's audio DSP costs on the unit, per stage and per core, and the deadline with every stage running. "
                         "Read the run with tools/tide_measure.py (jam_dev.py measure). Needs a bench build with the `tide` words.",
             "PHYS-MODELS": "# PHYS-MODELS (generated by tools/gen_measure_suites.py, audit Q14): the cost of one more PHYS note by model, every factory PHYS sound with 1 and 3 keys, one core (as COST-CAL1). "
                            "Read the run with tools/tide_measure.py."}
    for name, tests in suites.items():
        text = "\n".join(tests).split("\n")
        blocks = ("\n".join(tests)).split("\n\n")
        mins = sum(seconds_of(b.split("\n")) for b in blocks if b.strip()) / 60.0
        n = sum(1 for ln in text if ln.startswith("test "))
        if "--list" in sys.argv:
            print(f"{name}: {n} tests, about {mins:.1f} minutes")
            continue
        path = os.path.join(HERE, "bench_suites", name + ".txt")
        with open(path, "w", newline="\n") as o:
            o.write(heads[name] + "\n" + f"suite {name}\ndiag\n\n" + "\n".join(tests) + "\n")
        print(f"gen_measure_suites: {name}: {n} tests, about {mins:.1f} minutes -> {path}")


if __name__ == "__main__":
    main()
