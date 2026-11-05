#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Host test of tools/libcalls.py (audit R14): the scan finds a library call (__muldi3, __divdi3, ...) in a function of the disassembly, counts hardware divides, tells the per-sample groups
from the per-block ones; and, when build/felucca.dis exists (a device build was made in this checkout), the limiter, COMP, master and duck functions of the real image hold no library call."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import libcalls as lc  # noqa: E402

bad = 0


def check(name, ok):
    global bad
    print(f"libcalls: {name:<110} {'ok' if ok else 'FAIL'}")
    bad += not ok


DIS = """
comp_stage:
 2016000:    10 16             \tr0 = r1
 2016002:    f4 e1 20 11       \tr1 = r2 / r1 (u)
 2016006:    a3 ea 32 70       \tcall -3743644 <__muldi3 : 1c7070e >
 201600a:    a3 ea 32 70       \tcall -3743644 <cmp_big : 2016100 >

morph_wash:
 2017000:    a3 ea 32 70       \tcall -3743644 <__divdi3 : 1c7070e >
 2017004:    f4 e1 60 33       \tr3 = r6 / r3 (s)

__divdi3:
 1c7070e:    a3 ea 32 70       \tcall -3743644 <__udivdi3 : 1c70608 >
"""
sc = lc.scan(DIS)
check("a library call is found in the function that makes it, with its count", sc["comp_stage"]["lib"] == {"__muldi3": 1} and sc["morph_wash"]["lib"] == {"__divdi3": 1})
check("an ordinary call is a call but no library call; a library routine's own calls are not findings", sc["comp_stage"]["calls"]["cmp_big"] == 1 and not sc["__divdi3"]["lib"])
check("hardware divides are counted (unsigned and signed)", sc["comp_stage"]["div"] == 1 and sc["morph_wash"]["div"] == 1)
bad_found, _ = lc.findings(DIS)
check("a library call in COMP is a finding; the same call in a per-block TIDE stage is not", [f for _, f, _ in bad_found] == ["comp_stage"])
check("the report names the finding", "calls {'__muldi3': 1}" in lc.report(DIS) or "__muldi3" in lc.report(DIS))
dis = os.path.join(ROOT, "build", "felucca.dis")
if os.path.exists(dis):
    found, scan = lc.findings(open(dis, errors="replace").read())
    check("the device build (build/felucca.dis): no library call in the limiter, COMP, master and duck functions", not found and all(f in scan for f in ("comp_stage", "mix_master", "duck_half")))
    check("the device build: no __muldi3 anywhere in the image (the 64-bit multiplies are inline)", not any("__muldi3" in e["calls"] for e in scan.values()))
else:
    print("libcalls: SKIP the real image: no build/felucca.dis (make a device build first: tools/build.py)")
print(f"libcalls: {'FAIL' if bad else 'ok'}")
sys.exit(1 if bad else 0)
