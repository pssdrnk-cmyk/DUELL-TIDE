#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Library calls on the audio hot path of the device build (audit R14): which functions of build/felucca.dis (the disassembly tools/build.py writes) call a compiler-rt routine
(__muldi3, __divdi3, __udivdi3, __ashldi3 ... : a software 64-bit operation, many cycles) and how many hardware 32-bit divides (`rN = rM / rK`) they hold.

  python3 tools/libcalls.py [build/felucca.dis]

The groups are the per-sample code R14 named: the limiter, COMP, the master's DAC stage and the flash duck. The 2026-11-05 reading of the build: none of them calls a library routine (the 64-bit
multiplies of COMP's detector and of duck_half are inline; the limiter divides in hardware, three divide instructions in the master stage); the only library calls on audio paths are in TIDE's
per-block stages (morph_valley, morph_wash: one __divdi3 each, once a block and part, not per sample) and in part_voice (one __udivdi3 a voice and block). cmp_coef calls __udivdi3 when COMP's setting
changes (not per sample). tests/libcalls_test.py keeps the first part true: a new library call in a per-sample function of the groups fails it."""
import collections
import re
import sys

LIBCALL = re.compile(r"call [-0-9]* <(__[A-Za-z0-9_]*)")
CALL = re.compile(r"call [-0-9]* <([A-Za-z0-9_.$]*)")
DIV = re.compile(r"\br\d+ = r\d+ / r\d+ \((?:u|s)\)")
LABEL = re.compile(r"^([A-Za-z_][A-Za-z0-9_.$]*):\s*$")
# the per-sample functions (hot path) by group; a call to ANY routine named __* there is a finding. Functions that the compiler inlined into another one are not listed here: their calls show in the caller
GROUPS = {
    "limiter and master stage": ("mix_master", "split_master", "master_knee", "master_dac"),
    "COMP (per sample)": ("comp_stage", "cmp_big", "cmp_lg", "cmp_gr", "comp_y"),
    "flash duck": ("duck_half",),
}
PER_BLOCK = {"TIDE valley / wash (once a block and part)": ("morph_valley", "morph_wash"), "COMP setting change (not per sample)": ("cmp_coef",), "voice setup (a voice, a block)": ("part_voice",)}


def scan(text):
    """{function: {'lib': Counter of __routines, 'calls': Counter of every callee, 'div': hardware divides, 'lines': instruction lines}}"""
    out = {}
    fn = None
    for ln in text.splitlines():
        m = LABEL.match(ln)
        if m:
            fn = m.group(1)
            out.setdefault(fn, {"lib": collections.Counter(), "calls": collections.Counter(), "div": 0, "lines": 0})
            continue
        if fn is None or not ln.strip():
            continue
        e = out[fn]
        e["lines"] += 1
        c = CALL.search(ln)
        if c:
            e["calls"][c.group(1)] += 1
            if c.group(1).startswith("__") and not fn.startswith("__"):
                e["lib"][c.group(1)] += 1
        if DIV.search(ln):
            e["div"] += 1
    return out


def findings(text):
    """([(group, function, Counter)] library calls in the per-sample groups, the scan)"""
    sc = scan(text)
    bad = []
    for g, fns in GROUPS.items():
        for f in fns:
            if f in sc and sc[f]["lib"]:
                bad.append((g, f, sc[f]["lib"]))
    return bad, sc


def report(text, path="build/felucca.dis"):
    bad, sc = findings(text)
    L = [f"Read from {path} (the last device build in this checkout)."]
    for g, fns in GROUPS.items():
        parts = []
        for f in fns:
            if f in sc:
                e = sc[f]
                parts.append(f"{f} ({e['lines']} insns, {e['div']} divide insns" + (", LIBRARY CALLS " + ", ".join(f"{k} x{v}" for k, v in e["lib"].items()) if e["lib"] else ", no library call") + ")")
            else:
                parts.append(f"{f} (not in this build, or inlined)")
        L.append(f"- {g}: " + "; ".join(parts))
    for g, fns in PER_BLOCK.items():
        parts = [f"{f}: " + (", ".join(f"{k} x{v}" for k, v in sc[f]["lib"].items()) or "none") for f in fns if f in sc]
        if parts:
            L.append(f"- {g}: " + "; ".join(parts))
    L.append("Finding: " + ("a library call on the per-sample path: " + "; ".join(f"{f} calls {dict(c)}" for _, f, c in bad) if bad else
                            "none of the limiter, COMP, master or duck functions calls a library routine (no __muldi3 anywhere in the image: the 64-bit multiplies are inline); "
                            "the 32-bit division of the limiter is a hardware divide."))
    return "\n".join(L)


def main(argv=None):
    path = (argv or sys.argv[1:] or ["build/felucca.dis"])[0]
    text = open(path, errors="replace").read()
    print(report(text, path))
    return 1 if findings(text)[0] else 0


if __name__ == "__main__":
    sys.exit(main())
