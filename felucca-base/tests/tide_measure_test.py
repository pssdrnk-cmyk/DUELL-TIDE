#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Host test of the measurement kit (audit A01 / Q14): tools/gen_measure_suites.py, the suites TIDE-DSP and PHYS-MODELS, the bench words `tide ...` and the stage timers (src/bench.c, run
here through the host twin build/host/bench_sim --serve), bench_run.py's `report tide` columns and tools/tide_measure.py's analysis. The twin's clock does not move, so its timer figures are 0:
what is checked is the structure (which stage ran how often in which scene), the suites and the analysis on rows made up with known figures."""
import io
import math
import os
import re
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import bench_run as br  # noqa: E402
import gen_measure_suites as gen  # noqa: E402
import tide_measure as tm  # noqa: E402

SIM = os.path.join(ROOT, "build", "host", "bench_sim")
bad = 0


def check(name, ok):
    global bad
    print(f"tidemeasure: {name:<120} {'ok' if ok else 'FAIL'}")
    bad += not ok


# ---- the suites: generated, current, parseable, within the console's limits
for name, tests in (("TIDE-DSP", gen.tide_tests()), ("PHYS-MODELS", gen.phys_tests())):
    path = os.path.join(ROOT, "tools", "bench_suites", name + ".txt")
    text = open(path, encoding="utf-8").read()
    check(f"{name}: the file is what tools/gen_measure_suites.py writes (run it after a change)", "\n".join(tests) + "\n" in text and "\r" not in text)
    sname, parsed = br.parse_suite(text)
    check(f"{name}: parses, {len(parsed)} tests, names unique", sname == name and len(parsed) == len({t.name for t in parsed}) >= 16)
    words = [w for t in parsed for w, _ in br.setup_words(t)]
    check(f"{name}: every console word under the console's 64 characters (the longest {max(len('bench ' + w) for w in words)})", all(len("bench " + w) < 64 for w in words))
    check(f"{name}: at most 40 events a test (the lean margin of the 256-event queue)", all(len(t.events) <= 40 for t in parsed))
parsed = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "TIDE-DSP.txt")).read())[1]
check("TIDE-DSP: every test reads the stage timers (report tide)", all(t.report_tide for t in parsed))
check("TIDE-DSP: the real crossings and the heavy scenes play part 2 on its own channel (DUEL: LAYER does not give it its own fader)",
      all(any(w == "mode DUEL" for w, _ in br.setup_words(t)) for t in parsed if t.name.startswith(("TIDE.X.", "TIDE.HEAVY"))))
ph = br.parse_suite(open(os.path.join(ROOT, "tools", "bench_suites", "PHYS-MODELS.txt")).read())[1]
check("PHYS-MODELS: 12 factory PHYS sounds x (1 key, 3 keys), one core", len(ph) == 24 and all(any(w == "core1 off" for w, _ in br.setup_words(t)) for t in ph))
check("PHYS-MODELS: the models of the sounds are 6 MODAL, 3 STRING, 2 MEMB, 1 SYMP", sorted(m for _, _, m in gen.phys_sounds()) == ["MEMB"] * 2 + ["MODAL"] * 6 + ["STRING"] * 3 + ["SYMP"])
check("budget.c carries the four TIDE estimates", set(tm.budget_estimates()) == {"VALLEY", "WASH", "DIP", "FM6"})


# ---- the analysis on rows made up with known figures
def row(test, **kw):
    r = {"test": test, "rep": "1", "verdict": "PASS", "status": "DONE", "avg": "50", "peak": "55", "miss": "0", "c1": "20", "c1pk": "30", "tclate": "0", "tcvlate": "0", "tcsilent": "0", "tcown": "0",
         "tide_two": "1", "why": ""}
    r.update({k: str(v) for k, v in kw.items()})
    return r


T = tm.TICKS_PER_US
rows = [row("TIDE.VAL.P1", p1_vn=1000, p1_va=1000, p1_vs=int(10 * T * 1000), p1_vm=int(14 * T)),                  # 10 us a block  = 45.2 instr
        row("TIDE.VAL.P2", p2_vn=1000, p2_va=1000, p2_vs=int(12 * T * 1000), p2_vm=int(15 * T)),                  # 12 us = 54.3 (core 1 dearer)
        row("TIDE.VAL.SPR.P2", p2_vn=1000, p2_va=1000, p2_vs=int(20 * T * 1000), p2_vm=int(25 * T)),              # 20 us = 90.4 : over the estimate 66
        row("TIDE.WASH.P2", p2_vn=1000, p2_va=1000, p2_ws=int(8 * T * 1000), p2_wa=1000, p2_wn=1000),            # 8 us = 36.2
        row("TIDE.X.FM6.W", p1_fn=500, p1_fa=500, p1_fs=int(3 * T * 500), p2_fn=500, p2_fa=500, p2_fs=int(4 * T * 500)),   # 4 us = 18.1 a part with 4 notes = 4.5 a note
        row("TIDE.HEAVY.BASE", avg=80), row("TIDE.HEAVY.ON", avg=84, tclate=1)]
an = tm.analyse_tide(rows, {"VALLEY": 66, "WASH": 60, "DIP": 1, "FM6": 7})
check("analysis: a stage's mean is ticks / calls that worked / 24 ticks a us (10 us a block = 45.2 host instructions a sample)",
      abs(an["stages"][0]["mean_us"] - 10.0) < 1e-6 and abs(an["stages"][0]["instr"] - 45.22) < 0.05)
PROP = math.ceil(20 * tm.INSTR_PER_US * tm.SAFETY)
check(f"analysis: SPREAD on 90 instructions is over the estimate 66: TOO LOW, proposal {PROP} (x 1.15)", an["valley"]["verdict"].startswith("TOO LOW") and an["proposal"]["VALLEY"] == PROP)
check("analysis: the wash (36 instructions) and FM6's bridge (4.5 a note) hold", an["wash"]["verdict"].startswith("holds") and an["fm6"]["verdict"].startswith("holds"))
dl = an["deadline"][0]
check("analysis: a late block added in the scene with every stage is NOT inside the deadline; the points are the difference", not dl["inside"] and dl["added"]["late"] == 1 and abs(dl["d_avg_points"] - 4) < 1e-6)
txt = tm.report_text(rows, {"VALLEY": 66, "WASH": 60, "DIP": 1, "FM6": 7})
check("report: the stage table, the estimates, the deadline lines and the proposal are in the text", all(s in txt for s in ("| valley, SPREAD on", "BD_TIDE_VALLEY = 66", "NOT inside", f"BD_TIDE_VALLEY {PROP}")))
prow = []
for k, model, c1, c3 in ((0, "MODAL", 40, 40 + 4 * 328 / 32.8125), (4, "STRING", 40, 40 + 4 * 160 / 32.8125), (2, "SYMP", 40, 40 + 4 * 280 / 32.8125)):
    prow += [row(f"PHYS.{k:02d}.{model}.K1", avg=c1), row(f"PHYS.{k:02d}.{model}.K3", avg=c3)]
ap = tm.analyse_phys(prow)
check("PHYS: one more note = (K3 - K1) x 32.8125 / 4: MODAL 328, STRING 160, SYMP 280; ratios 1.00 / 0.49 / 0.85",
      abs(ap["models"]["MODAL"]["dearest"] - 328) < 0.5 and abs(ap["models"]["STRING"]["ratio"] - 0.488) < 0.01 and abs(ap["models"]["SYMP"]["ratio"] - 0.854) < 0.01)

# ---- the host twin: the real bench words and the stage timers
if os.path.exists(SIM) or os.path.exists(SIM + ".exe"):
    sim = br.SimLink(SIM)
    tmp = tempfile.mkdtemp()
    log = open(os.path.join(tmp, "console.log"), "w")
    sim.open()
    unit = br.Unit(sim, log)
    unit.ok("bench reset")
    unit.sleep(0.3)
    check("tide: the words are known and checked (a bad one is refused)", unit.cmd("bench tide wash 9").startswith("ERR") and unit.cmd("bench tide force 3 10 10").startswith("ERR")
          and unit.cmd("bench tide bogus").startswith("ERR") and unit.ok("bench tide wash 2").startswith("OK"))
    unit.ok("bench reset")
    check("tide: bench reset puts WASH back to the session's (OFF here) and clears the timers",
          br.tide_report(unit).get("tide_wash") == 0 and br.tide_report(unit).get("p1_vn") == 0)
    sim.close()
    # the suites on the twin (its keysense word is the device's only: the line is left out)
    def run_suite(name, only=None):
        text = "".join(ln for ln in open(os.path.join(ROOT, "tools", "bench_suites", name + ".txt"), encoding="utf-8") if not ln.startswith("keysense"))
        sp = os.path.join(tmp, name + ".txt")
        open(sp, "w", newline="\n").write(text)
        args = ["--sim", SIM, "--suite", sp, "--repeat", "1", "--runs", os.path.join(tmp, "runs"), "--out", os.path.join(tmp, name), "--no-caffeinate"]
        if only:
            args += ["--only", only]
        so = sys.stdout
        sys.stdout = io.StringIO()
        try:
            br.main(args)
        finally:
            sys.stdout = so
        return tm.load([os.path.join(tmp, name)])
    R = run_suite("TIDE-DSP")
    T_ = tm.by_test(R)
    s = lambda t, p, st: tm.stage(T_[t], p, st)
    check("twin: TIDE.BASE runs no stage that works (the valley and the wash are idle), the timers are counted", s("TIDE.BASE", 1, "valley")["act"] == 0 and s("TIDE.BASE", 1, "valley")["n"] > 100)
    check("twin: the forced valley works on the forced part only, in every block", s("TIDE.VAL.P1", 1, "valley")["act"] == s("TIDE.VAL.P1", 1, "valley")["n"] > 100 and s("TIDE.VAL.P1", 2, "valley")["act"] == 0
          and s("TIDE.VAL.P2", 2, "valley")["act"] > 100 and s("TIDE.VAL.P2", 1, "valley")["act"] == 0)
    check("twin: the wash alone works with WASH MID and the movement forced (part 2), not with WASH OFF", s("TIDE.WASH.P2", 2, "wash")["act"] > 100 and s("TIDE.VAL.P2", 2, "wash")["act"] == 0)
    check("twin: valley and wash together", s("TIDE.VALWASH.P1", 1, "valley")["act"] > 100 and s("TIDE.VALWASH.P1", 1, "wash")["act"] > 100)
    check("twin: the real FM6 crossing runs the bridge on BOTH parts (DUEL, each on its own fader)", s("TIDE.X.FM6.W", 1, "fm6")["act"] > 100 and s("TIDE.X.FM6.W", 2, "fm6")["act"] > 100)
    check("twin: GLIDE (armed 500 ms into the run) moves both parts, the plan shows the movement", all(num > 0 for num in (tm.num(T_["TIDE.GLIDE.W"][0], "p1_motion"), tm.num(T_["TIDE.GLIDE.W"][0], "p2_motion")))
          and tm.num(T_["TIDE.GLIDE.W"][0], "tide_glide") == 1)
    check("twin: the heavy scene with every stage runs the valley, the wash and the bridge on both parts", all(s("TIDE.HEAVY.ON", p, st)["act"] > 100 for p in (1, 2) for st in ("valley", "wash", "fm6")))
    rep = tm.report_text(R)
    check("twin: the report is made from the run (stage table, scenes, deadline lines)", "## A01" in rep and "TIDE.HEAVY.ON against TIDE.HEAVY.BASE" in rep and "| TIDE.GLIDE.W |" in rep)
    P = run_suite("PHYS-MODELS", "PHYS.0*")
    check("twin: the PHYS suite runs (rows K1 and K3 of the first sounds)", len(P) >= 2 and all(r["status"] == "DONE" for r in P))
else:
    print("tidemeasure: SKIP the twin runs: build/host/bench_sim is not built (host_tests.sh tidemeasure builds it)")
print(f"tidemeasure: {'FAIL' if bad else 'ok'}")
sys.exit(1 if bad else 0)
