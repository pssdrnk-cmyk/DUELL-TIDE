#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""The unit measurement of TIDE's audio DSP (audit A01) and of PHYS per model (audit Q14): runs the two bench suites on a bench build (DUELL_BENCH, the `tide` words) and writes the report.
`jam/jam_dev.py measure` is the owner's one command (build, install, run, report, back to the normal firmware); this file is its engine and works alone as well:

  python3 tools/tide_measure.py run --port COM5 --out DIR [--only TIDE|PHYS]      run the suites TIDE-DSP and PHYS-MODELS on the unit (bench_run.py), then report
  python3 tools/tide_measure.py report RUNDIR [RUNDIR ...] [--out report.md]      the report of runs already made (the folders hold results.csv)
  python3 tools/tide_measure.py run --sim build/host/bench_sim --out DIR          the same against the host twin (a test of the kit: its timer figures are not the unit's)

The report (text, markdown tables) says, for A01: what each TIDE stage costs per block and part, on core 0 (part 1) and on core 1 (part 2), as the unit's own clock saw it, in microseconds
and in the budget's unit (host instructions a sample), next to the estimate in firmware/src/budget.c (BD_TIDE_*, a host-derived guess until this report) with the verdict "holds" or
"too low: raise to N"; the scenes with every stage running against the same scene with TIDE idle (CPU on core 0, core 1's busy share, late / very late / silent blocks: the deadline);
for Q14: the cost of one more PHYS note by model against the table's 406 and against the host ratios the host probe expects (tests/phys_cost_probe.c). Nothing here changes a charge:
the report PROPOSES numbers. Units: a block is 32 samples = 725.6 us; 1 us of a stage per block = 4.522 host instructions a sample (3281.25 = a whole half buffer)."""
import argparse
import csv
import glob
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "firmware", "src")
sys.path.insert(0, HERE)

TICKS_PER_US = 24.0                                    # TC_TICKS: TIMER4 (twocore.h)
BLOCK_US = 32 / 44100 * 1e6                            # one block of 32 samples
INSTR_PER_US = 3281.25 / BLOCK_US                      # a stage that takes 1 us a block costs this many host instructions a sample (3281.25 = 100 % of a half buffer)
PCT_INSTR = 32.8125                                    # host instructions a sample per 1 % of a half buffer (the CPU column)
PHYS_CHARGE = 406                                      # budget.c BD_ENG for PHYS (the unit's COST-CAL row for MODAL, 405.5)
PHYS_HOST = {"MODAL": 1.00, "STRING": 0.48, "MEMB": 0.72, "SYMP": 0.86}   # tests/phys_cost_probe.c, table 2 (the dearest sound of each model against the dearest MODAL sound), 2026-11-05;
PHYS_AUDIT = {"SYMP": 0.65, "STRING": 0.45}            # .. and the audit's wall-clock ratios (lane Q): SYMP 0.65 x MODAL, a plain string 0.35 .. 0.5 x
SAFETY = 1.15                                          # the margin the proposed figures carry over the unit's mean (the budget table's own are the unit's mean, margin 1.00 for the caps)


def budget_estimates(path=None):
    """{'VALLEY': 66, 'WASH': 60, 'DIP': 1, 'FM6': 7}: the estimates in budget.c (BD_TIDE_*), host instructions a sample"""
    text = open(path or os.path.join(SRC, "budget.c"), encoding="utf-8").read()
    out = {}
    for k in ("VALLEY", "WASH", "DIP", "FM6"):
        m = re.search(r"#define BD_TIDE_%s (\d+)u" % k, text)
        if m:
            out[k] = int(m.group(1))
    return out


def load(paths):
    """the rows of results.csv of the given folders (or files), a folder name added: [dict]"""
    rows = []
    for p in paths:
        f = p if os.path.isfile(p) else os.path.join(p, "results.csv")
        with open(f, newline="", encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                r["_run"] = os.path.basename(os.path.dirname(os.path.abspath(f)))
                rows.append(r)
    return rows


def num(r, k, default=0.0):
    try:
        return float(r.get(k) if r.get(k) not in (None, "") else default)
    except ValueError:
        return default


def by_test(rows):
    out = {}
    for r in rows:
        out.setdefault(r.get("test", ""), []).append(r)
    return out


def voices(rows):
    """the voices that sounded in a test's CPU window (the runner's `vox` column, the mean over the repeats): None when the rows carry no such column"""
    v = [num(r, "vox", None) for r in rows if r.get("vox") not in (None, "")]
    return sum(v) / len(v) if v else None


def silent_tests(T, prefix="TIDE."):
    """the tests of a run in which not one voice sounded (the chord never reached a part: the unit's MIDI IN listens to other channels, 2026-11-05): every figure of them is the idle unit's"""
    return sorted(t for t, R in T.items() if t.startswith(prefix) and voices(R) == 0)


def stage(rows, part, st):
    """one stage of one part over the repeats of a test: calls, calls that did the work, mean and longest in us, host instructions a sample of the mean"""
    sfx = {"valley": "v", "wash": "w", "fm6": "f"}[st]
    n = sum(num(r, f"p{part}_{sfx}n") for r in rows)
    act = sum(num(r, f"p{part}_{sfx}a") for r in rows)
    tot = sum(num(r, f"p{part}_{sfx}s") for r in rows)
    mx = max([num(r, f"p{part}_{sfx}m") for r in rows] + [0.0])
    mean = tot / act / TICKS_PER_US if act else None
    return {"n": int(n), "act": int(act), "mean_us": mean, "max_us": mx / TICKS_PER_US if act else None, "instr": mean * INSTR_PER_US if mean is not None else None}


def fmt(v, d=1):
    return "-" if v is None else f"{v:.{d}f}"


def verdict_for(measured, est, what, why="the stage did not run in the scene"):
    """('holds'|'TOO LOW: raise to N'|'not measured', proposed N)"""
    if measured is None:
        return f"not measured ({why})", None
    prop = int(math.ceil(measured * SAFETY))
    if est is None:
        return f"no estimate in budget.c; proposal {prop}", prop
    if measured <= est:
        return f"holds (the estimate {est} is {est / measured:.1f} x the unit's mean)" if measured else "holds", prop
    return f"TOO LOW: the unit's mean {measured:.0f} is over the estimate {est}; raise {what} to {prop}", prop


def analyse_tide(rows, est):
    T = by_test(rows)
    quiet = silent_tests(T)
    res = {"stages": [], "scenes": [], "notes": [], "proposal": {}, "silent": quiet}
    two = any(r.get("tc") == "2" or num(r, "tide_two") == 1 for r in rows)      # (the runner's own column: core 1 played part 2 in a test)
    res["two_cores"] = two

    def add(label, test, part, st, per=None, sub=None):
        if test not in T:
            return None
        s = stage(T[test], part, st)
        m = s["instr"]
        if sub is not None and m is not None:                 # valley with the wash inside it: the wash's own mean is taken out
            w = stage(T[test], part, "wash")
            m = max(m - (w["instr"] or 0.0), 0.0)
        core = "core 0" if part == 1 else ("core 1" if two else "core 0, one core")
        res["stages"].append({"stage": label, "test": test, "part": part, "core": core, "act": s["act"], "n": s["n"], "mean_us": s["mean_us"], "max_us": s["max_us"], "silent": test in quiet,
                              "instr": m, "per": per, "per_voice": (m / per if (m is not None and per) else None)})
        return m
    v1 = add("valley, SPREAD off", "TIDE.VAL.P1", 1, "valley")
    v2 = add("valley, SPREAD off", "TIDE.VAL.P2", 2, "valley")
    vs = add("valley, SPREAD on (both buffers)", "TIDE.VAL.SPR.P2", 2, "valley")
    w1 = add("wash MID alone (GLIDE's breath)", "TIDE.WASH.P1", 1, "wash")
    w2 = add("wash MID alone (GLIDE's breath)", "TIDE.WASH.P2", 2, "wash")
    vw1 = add("wash MID on top of the valley", "TIDE.VALWASH.P1", 1, "wash")
    vw2 = add("wash MID on top of the valley", "TIDE.VALWASH.P2", 2, "wash")
    f1 = add("FM6 bridge (fm6_xblock), a part with 4 notes", "TIDE.X.FM6.W", 1, "fm6", per=4)
    f2 = add("FM6 bridge (fm6_xblock), a part with 4 notes", "TIDE.X.FM6.W", 2, "fm6", per=4)
    sc = {}
    for test in ("TIDE.X.ANALOG", "TIDE.X.ANALOG.W", "TIDE.GLIDE.W", "TIDE.HEAVY.BASE", "TIDE.HEAVY.ON", "TIDE.M16.BASE", "TIDE.M16.ON", "TIDE.BASE"):
        if test not in T:
            continue
        R = T[test]
        sc[test] = {"avg": sum(num(r, "avg") for r in R) / len(R), "peak": max(num(r, "peak") for r in R), "miss": sum(num(r, "miss") for r in R),
                    "c1": sum(num(r, "c1") for r in R) / len(R), "c1pk": max(num(r, "c1pk") for r in R), "late": sum(num(r, "tclate") for r in R),
                    "vlate": sum(num(r, "tcvlate") for r in R), "silent": sum(num(r, "tcsilent") for r in R), "own": sum(num(r, "tcown") for r in R),
                    "verdict": ",".join(sorted({r.get("verdict", "") for r in R})), "status": ",".join(sorted({r.get("status", "") for r in R})), "reps": len(R)}
        res["scenes"].append((test, sc[test]))
    # the verdicts against the estimates (the dearer of the two cores decides)
    vals = [x for x in (v1, v2) if x is not None]
    worst = [x for x in (vs, v1, v2) if x is not None]
    res["valley"] = {"measured": max(vals) if vals else None, "spread": vs, "measured_worst": max(worst) if worst else None}
    def why_not(tests):
        q = [t for t in tests if t in quiet]
        return f"no voice sounded in {', '.join(q)}: the notes did not reach the parts" if q else "the stage did not run in the scene"
    res["valley"]["verdict"], res["proposal"]["VALLEY"] = verdict_for(res["valley"]["measured_worst"], est.get("VALLEY"), "BD_TIDE_VALLEY",
                                                                       why_not(("TIDE.VAL.P1", "TIDE.VAL.P2", "TIDE.VAL.SPR.P2")))
    wvals = [x for x in (w1, w2, vw1, vw2) if x is not None]
    res["wash"] = {"measured": max(wvals) if wvals else None}
    res["wash"]["verdict"], res["proposal"]["WASH"] = verdict_for(res["wash"]["measured"], est.get("WASH"), "BD_TIDE_WASH",
                                                                   why_not(("TIDE.WASH.P1", "TIDE.WASH.P2", "TIDE.VALWASH.P1", "TIDE.VALWASH.P2")))
    fvals = [x for x in (f1, f2) if x is not None]
    fper = max(fvals) / 4.0 if fvals else None
    res["fm6"] = {"measured": fper}
    res["fm6"]["verdict"], res["proposal"]["FM6"] = verdict_for(fper, est.get("FM6"), "BD_TIDE_FM6 (a note)", why_not(("TIDE.X.FM6.W",)) if "TIDE.X.FM6.W" in quiet else
                                                                 "the bridge did not run in TIDE.X.FM6.W: the ROLL seed gave no algorithm change (gen_measure_suites.py SEED_FM6; try another on the twin)")
    # the dip: the CPU of the real crossing and GLIDE scenes over the idle scene (core 0, both parts; below the noise of the readout it is not a number)
    if "TIDE.BASE" in sc and "TIDE.GLIDE.W" in sc and "TIDE.BASE" not in quiet and "TIDE.GLIDE.W" not in quiet:   # (a silent reference is not one: the scene's own notes would be the difference)
        d = (sc["TIDE.GLIDE.W"]["avg"] - sc["TIDE.BASE"]["avg"]) * PCT_INSTR
        res["dip"] = {"glide_scene_delta_instr": d}
    # the deadline: the heavy scenes with every stage against the same without
    dl = []
    for a, b in (("TIDE.HEAVY.BASE", "TIDE.HEAVY.ON"), ("TIDE.M16.BASE", "TIDE.M16.ON")):
        if a in sc and b in sc:
            x, y = sc[a], sc[b]
            added = {k: y[k] - x[k] for k in ("miss", "late", "vlate", "silent", "own")}
            ok = all(v <= 0 for v in added.values()) and y["status"] in ("DONE", "")
            dl.append({"base": a, "on": b, "d_avg_points": y["avg"] - x["avg"], "d_peak_points": y["peak"] - x["peak"], "added": added, "inside": ok, "valid": a not in quiet and b not in quiet,
                       "c1_busy": (x["c1"], y["c1"]), "c1_peak": (x["c1pk"], y["c1pk"])})
    res["deadline"] = dl
    if quiet:
        n_all = sum(1 for t in T if t.startswith("TIDE."))
        res["notes"].append(f"NO VOICE SOUNDED in {len(quiet)} of {n_all} TIDE tests ({', '.join(quiet[:6])}{', ..' if len(quiet) > 6 else ''}): the chord never reached the parts, so the stages did not run and every CPU figure "
                            "here is the idle unit's, NOT a measurement. The unit's MIDI IN listens to the channels of GLO > MIDI (the bench build puts them on 1 and 2 for a session since 2026-11-05); "
                            "run the measurement again with a build that has that")
    missing = [t for t in ("TIDE.VAL.P1", "TIDE.VAL.P2", "TIDE.WASH.P2", "TIDE.X.FM6.W", "TIDE.HEAVY.ON") if t not in T]
    if missing:
        res["notes"].append("tests not in the run: " + ", ".join(missing))
    for t, R in T.items():
        if t.startswith("TIDE.") and any(r.get("status") not in ("DONE", "") for r in R):
            res["notes"].append(f"{t}: status {','.join(sorted({r.get('status', '') for r in R}))} ({R[0].get('why', '')})")
    if two is False and rows:
        res["notes"].append("the unit ran ONE core in these tests (tide_two 0): part 2's figures are core 0's")
    fm_ran = [x for x in res["stages"] if x["stage"].startswith("FM6") and x["act"]]
    if "TIDE.X.FM6.W" in T and not fm_ran and "TIDE.X.FM6.W" not in quiet:
        res["notes"].append("FM6's bridge did not run in TIDE.X.FM6.W although voices sounded (the ROLL seed gave no algorithm change): the FM6 figure is not measured")
    return res


def analyse_phys(rows):
    """one more PHYS note by model: the K3 against the K1 test of each factory sound, one core, both parts the same sound: (avg3 - avg1) % x 32.8125 / (2 parts x 2 notes).
    A sound counts only if its voices sounded (the runner's `vox`: more voices with 3 keys than with 1, and some with 1): a pair in which nothing sounded has a difference of 0 whatever the
    model costs (2026-11-05: the first unit run, the unit's MIDI IN on other channels than the test's, printed 'dearest 2, mean 1')"""
    T = by_test(rows)
    out, bad = {}, {}
    for t in sorted(T):
        m = re.fullmatch(r"PHYS\.(\d+)\.(\w+)\.K1", t)
        if not m or t[:-1] + "3" not in T:
            continue
        a1 = sum(num(r, "avg") for r in T[t]) / len(T[t])
        a3 = sum(num(r, "avg") for r in T[t[:-1] + "3"]) / len(T[t[:-1] + "3"])
        v1, v3 = voices(T[t]), voices(T[t[:-1] + "3"])
        if v1 is not None and v3 is not None and not (v1 > 0 and v3 > v1):
            bad.setdefault(m.group(2), []).append(int(m.group(1)))
            continue
        out.setdefault(m.group(2), []).append((int(m.group(1)), (a3 - a1) * PCT_INSTR / 4.0))
    res = {"models": {}, "note": "", "silent": bad}
    dear_modal = max((c for _, c in out.get("MODAL", [])), default=None)
    for model in ("MODAL", "STRING", "MEMB", "SYMP"):
        L = out.get(model, [])
        if not L and not bad.get(model):
            continue
        if not L:
            res["models"][model] = {"n": 0, "silent": len(bad[model]), "dearest": None, "mean": None, "ratio": None, "host_ratio": PHYS_HOST.get(model), "proposal": None}
            continue
        dear = max(c for _, c in L)
        mean = sum(c for _, c in L) / len(L)
        res["models"][model] = {"n": len(L), "silent": len(bad.get(model, [])), "dearest": dear, "mean": mean, "ratio": dear / dear_modal if dear_modal else None,
                                "host_ratio": PHYS_HOST.get(model), "proposal": int(math.ceil(dear * SAFETY / 5.0) * 5)}
    res["dear_modal"] = dear_modal
    return res


def report_text(rows, est=None, stamp="", build="", libcalls=None, velocity=None):
    est = est or budget_estimates()
    L = [f"# TIDE audio DSP and PHYS per model: the unit's own figures  {stamp}".rstrip(), ""]
    if build:
        L += [f"Build: {build}", ""]
    L += ["Units: a block is 32 samples = 725.6 us; 1 us of a stage per block = 4.522 host instructions a sample (the budget's unit; 3281.25 = a whole half buffer). Figures are means of the calls that",
          "did the stage's work (the unit's TIMER4, 24 ticks a us), the longest single call beside them. Nothing in the firmware was changed by this report: the numbers below are PROPOSALS.", ""]
    tide = analyse_tide(rows, est)
    if tide["stages"] or tide["scenes"]:
        L += ["## A01  TIDE's audio DSP", ""]
        L += ["| stage | part (core) | calls that worked / all | mean us a block | longest us | host instr a sample |", "|---|---|---|---|---|---|"]
        for s in tide["stages"]:
            L.append(f"| {s['stage']} | {s['part']} ({s['core']}) | {s['act']} / {s['n']}{' (no voice sounded)' if s.get('silent') else ''} | {fmt(s['mean_us'], 2)} | {fmt(s['max_us'], 1)} | {fmt(s['instr'], 1)}"
                     + (f" ({fmt(s['per_voice'], 1)} a note)" if s["per_voice"] is not None else "") + " |")
        L += ["", "Against the estimates in firmware/src/budget.c (host-derived, host instructions a sample, charged to a part with a MORPH):", ""]
        for key, label, unit in (("valley", "BD_TIDE_VALLEY", "worst of both cores, SPREAD on included"), ("wash", "BD_TIDE_WASH", "worst of both cores"), ("fm6", "BD_TIDE_FM6", "a note")):
            e = est.get({"valley": "VALLEY", "wash": "WASH", "fm6": "FM6"}[key])
            L.append(f"- {label} = {e}: the unit's mean {fmt(tide[key]['measured'], 1)} ({unit}): {tide[key]['verdict']}")
        if "dip" in tide:
            L.append(f"- the voices' dip: BD_TIDE_DIP = {est.get('DIP')} a voice; it is below what the CPU readout resolves (the GLIDE scene against the idle scene: {tide['dip']['glide_scene_delta_instr']:+.0f} host instr a sample "
                     "for both parts, dip and wash together, with the readout's own spread of about 5 points = 160); not a number of its own")
        L += ["", "### Scenes (core 0's CPU as the CPU bar and ABOUT read it; core 1 and the two-core runner's counters)", "",
              "| scene | avg % | peak % | missed halves | core 1 busy / peak % | late | very late | silent | own (core 0 played part 2) | verdict |", "|---|---|---|---|---|---|---|---|---|---|"]
        for t, s in tide["scenes"]:
            L.append(f"| {t} | {s['avg']:.1f} | {s['peak']:.1f} | {s['miss']:.0f} | {s['c1']:.1f} / {s['c1pk']:.1f} | {s['late']:.0f} | {s['vlate']:.0f} | {s['silent']:.0f} | {s['own']:.0f} | {s['verdict']} |")
        L += ["", "### The two-core deadline (every TIDE stage running against the same scene idle)", ""]
        if not tide["deadline"]:
            L.append("- the HEAVY / M16 scene pairs were not in the run.")
        for d in tide["deadline"]:
            L.append(f"- {d['on']} against {d['base']}: core 0 {d['d_avg_points']:+.1f} points average, {d['d_peak_points']:+.1f} peak; core 1 busy {d['c1_busy'][0]:.1f} -> {d['c1_busy'][1]:.1f} %, peak {d['c1_peak'][0]:.1f} -> "
                     f"{d['c1_peak'][1]:.1f} %; added missed halves {d['added']['miss']:+.0f}, late {d['added']['late']:+.0f}, very late {d['added']['vlate']:+.0f}, silent {d['added']['silent']:+.0f}: "
                     + ("NOT MEASURED: no voice sounded in these scenes, so nothing was running that could be late" if not d.get("valid", True) else
                        "INSIDE the deadline (nothing added)" if d["inside"] else "NOT inside: the model's TIDE term needs the numbers above, and the caps need a look"))
        for n in tide["notes"]:
            L.append(f"- note: {n}")
        props = ", ".join(f"BD_TIDE_{k} {v}" for k, v in tide["proposal"].items() if v is not None)
        L += ["", "Proposal for budget.c (only where the verdict says TOO LOW, or to replace the estimate by the unit's number; one commit, the budget and deadline tests re-pinned with the reason): "
              + (props or "none (no stage was measured)") + ".", ""]
    phys = analyse_phys(rows)
    if phys["models"]:
        L += ["## Q14  PHYS: the cost of one more note by model", "",
              f"One more note (3 keys against 1, one core, both parts the same factory sound), host instructions a sample; the budget charges {PHYS_CHARGE} for every PHYS note (the dearest, MODAL). "
              "'Host' is the ratio to MODAL's dearest sound on the host (tests/phys_cost_probe.c); the audit's own wall-clock ratios were SYMP 0.65 and STRING 0.35 to 0.5.", "",
              "| model | sounds | dearest | mean | ratio to MODAL's dearest (unit) | host expects | proposal (dearest x 1.15, up to 5) |", "|---|---|---|---|---|---|---|"]
        for model, m in phys["models"].items():
            sd = f" (+ {m['silent']} with no voice sounding: left out)" if m.get("silent") else ""
            if m["dearest"] is None:
                L.append(f"| {model} | 0{sd} | not measured | - | - | {fmt(m['host_ratio'], 2)} | - |")
            else:
                L.append(f"| {model} | {m['n']}{sd} | {m['dearest']:.0f} | {m['mean']:.0f} | {fmt(m['ratio'], 2)} | {fmt(m['host_ratio'], 2)} | {m['proposal']} |")
        if any(m.get("silent") for m in phys["models"].values()):
            L += ["", "A pair whose notes did not sound (no voices, or not more with 3 keys than with 1) is not a measurement: the unit played nothing, so 3 keys cost what 1 key costs. "
                  "Nothing here is to be taken from a model with 'not measured'."]
        L += ["", "The charges are NOT changed by this report. If the unit agrees with the host (STRING and SYMP clearly under MODAL), a per-model row in budget.c BD_ENG for PHYS is the change; "
              "it gives STRING and SYMP a third note on one core.", ""]
    if libcalls:
        L += ["## R14  library calls in the device build", "", libcalls, ""]
    if velocity:
        L += ["## T17  key velocity", "", velocity, ""]
    return "\n".join(L).rstrip() + "\n"


def package_loader(path):
    """the update loader a package stages (ota.bin, UFW entry 100): {'crc': its data CRC16 (what `bench updl` checks the staged copy against), 'len', 'marker': Felucca's own loader}.
    Raises ValueError when the file is no package of ours. A unit that installed this package through the running normal firmware has this loader staged at 0xE0000"""
    import fm1_install as fi
    raw = open(path, "rb").read()
    try:
        img = fi.logical_image(raw)
    except Exception as e:                                   # (too short ..)
        raise ValueError(f"{path}: not an FM-1 package ({e})")
    if len(img) < 0x400:
        raise ValueError(f"{path}: not an FM-1 package (too short)")

    def jl(b):                                               # the UFW header cipher, key 0xFFFF (ota.c ota_jl_enc)
        b, k = bytearray(b), 0xFFFF
        for i in range(len(b)):
            b[i] ^= k & 0xFF
            k = ((k << 1) ^ (0x1021 if k & 0x8000 else 0)) & 0xFFFF
        return bytes(b)
    h = bytearray(img[:0x400])
    h[:0x40] = jl(h[:0x40])
    n = int.from_bytes(h[8:10], "little")
    for i in range(min(n, 11)):
        e = jl(h[0x40 + i * 0x50:0x40 + (i + 1) * 0x50])
        if int.from_bytes(e[0:2], "little") == 100:
            off, ln = int.from_bytes(e[8:12], "little"), int.from_bytes(e[12:16], "little")
            ota = img[off:off + ln]
            if len(ota) < 0x80 or ln != int.from_bytes(ota[8:12], "little") + 0x20:
                break
            return {"crc": int.from_bytes(ota[2:4], "little"), "len": ln - 0x20, "marker": fi.LOADER_MARK in ota}
    raise ValueError(f"{path}: no update loader in this package (no ota.bin entry)")


def preflight(bench_main, out, port=None, sim=None, runs=None, suite="TIDE-DSP"):
    """Before the long suites: one 6-second test (TIDE.BASE) that must make voices sound. A unit whose MIDI IN does not listen to the test's channels plays nothing, and an hour
    of 'measurement' is the idle unit's. -> (ok, text); ok False also when the run produced no result"""
    d = os.path.join(out, "preflight")
    argv = ["--suite", suite, "--only", "TIDE.BASE", "--repeat", "1", "--out", d, "--runs", runs or os.path.join(out, "runs"), "--no-caffeinate"]
    argv += ["--sim", sim] if sim else ["--port", port] if port else []
    try:
        bench_main(argv)
    except SystemExit as e:
        return False, f"the preflight test did not run: {e}"
    try:
        rows = load([d])
    except OSError:
        return False, "the preflight test left no result"
    if not rows:
        return False, "the preflight test left no result"
    v = voices(rows)
    if v is None:
        return True, "preflight: the build reports no voice count; going on"
    if v <= 0:
        return False, ("preflight: no voice sounded when the chord was played (voices 0.0). The unit did not take the notes of the test: its MIDI IN listens to the channels of GLO > MIDI, and this bench "
                       "build did not put them on 1 and 2 for the session (a build before 2026-11-05 does not); nothing here would be measured")
    return True, f"preflight: {v:.1f} voices sounding in the test chord"


def run_suites(port, out, only=None, sim=None, runs=None, bench_main=None):
    """run the suites TIDE-DSP and PHYS-MODELS with bench_run.main; the run folders (results.csv inside) are returned"""
    if bench_main is None:
        import bench_run
        bench_main = bench_run.main
    folders = []
    ok, text = preflight(bench_main, out, port, sim, runs)
    print(text)
    if not ok:
        return folders
    for name, key in (("TIDE-DSP", "TIDE"), ("PHYS-MODELS", "PHYS")):
        if only and only.upper() != key:
            continue
        d = os.path.join(out, name)
        argv = ["--suite", name, "--out", d, "--runs", runs or os.path.join(out, "runs"), "--no-caffeinate"]
        argv += ["--sim", sim] if sim else ["--port", port] if port else []
        rc = bench_main(argv)
        folders.append((name, d, rc))
    return folders


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run the suites on the unit (or the twin), then report")
    r.add_argument("--port", help="the unit's serial port (default: found by itself)")
    r.add_argument("--sim", help="the host twin instead of a unit (a test of the kit)")
    r.add_argument("--out", required=True)
    r.add_argument("--only", choices=("TIDE", "PHYS"))
    q = sub.add_parser("report", help="the report of runs already made")
    q.add_argument("runs", nargs="+")
    q.add_argument("--out")
    a = ap.parse_args(argv)
    if a.cmd == "run":
        os.makedirs(a.out, exist_ok=True)
        folders = run_suites(a.port, a.out, a.only, a.sim)
        runs = [d for _, d, _ in folders if os.path.exists(os.path.join(d, "results.csv"))]
        if not runs:
            print("no results: nothing to report", file=sys.stderr)
            return 1
        text = report_text(load(runs), stamp=os.path.basename(a.out))
        path = os.path.join(a.out, "report.md")
        with open(path, "w", encoding="utf-8", newline="\n") as o:
            o.write(text)
        print(text)
        print(f"-> {path}")
        return 0 if all(rc == 0 for _, _, rc in folders) else 1
    runs = []
    for p in a.runs:
        runs += glob.glob(p)
    text = report_text(load(runs))
    if a.out:
        with open(a.out, "w", encoding="utf-8", newline="\n") as o:
            o.write(text)
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
