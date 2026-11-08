#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Self-test of tools/hot_reach.py (CPU audit K6) on a hand-made disassembly; host group hotreach (host_tests.sh).

Functions: fm1_alnk0_irq (flash, root) calls direct_fn and tail-calls tail_fn; direct_fn calls leaf_fn; fm1_c1_main (flash, root) calls nothing;
inram_fn is placed in RAM (a root of its own); ptr_fn is in flash and only its address is taken (not followed: finding H1, asserted here so a
change of that is noticed)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hot_reach  # noqa: E402

FLASH, RAM = 0x02000000, 0x01000000
NL, TAB = chr(10), chr(9)


def fn(name, addr, lines):
    out = [f"{addr:08x} <{name}>:", f"{name}:"]
    for i, text in enumerate(lines):
        out.append(f"  {addr + 4 * i:x}:{TAB}ea 80 00 10 {TAB}{text}")
    return NL.join(out)


def call(kind, name, addr):
    return f"{kind} 100 <{name}: {addr:x}>"


def disassembly():
    return NL.join([
        fn("fm1_alnk0_irq", FLASH + 0x000, [call("call", "direct_fn", FLASH + 0x100), call("goto", "tail_fn", FLASH + 0x200), "return"]),
        fn("fm1_c1_main", FLASH + 0x040, ["nop", "return"]),
        fn("direct_fn", FLASH + 0x100, [call("call", "leaf_fn", FLASH + 0x300), "return"]),
        fn("tail_fn", FLASH + 0x200, ["nop", "return"]),
        fn("leaf_fn", FLASH + 0x300, ["nop", "return"]),
        fn("ptr_fn", FLASH + 0x400, ["nop", "return"]),
        fn("inram_fn", RAM + 0x10, ["nop", "return"]),
    ]) + NL


bad = 0


def expect(what, ok):
    global bad
    if not ok:
        bad += 1
        print("hotreach: FAIL", what)


dis = disassembly()
base = {"fm1_alnk0_irq": "root, event"}
errs, notes = hot_reach.check(dis, dict(base))
named = " ".join(errs)
expect("a flash function called directly and not allowed fails", "direct_fn" in named)
expect("a function reached through that one (leaf) fails too, with the chain", "leaf_fn" in named and "leaf_fn <- direct_fn <- fm1_alnk0_irq" in named)
expect("a tail-call branch (goto) is followed", "tail_fn" in named)
expect("a function whose address is only taken is not followed (finding H1, known gap)", "ptr_fn" not in named)
expect("a function in RAM is not an error", "inram_fn" not in named)
expect("fm1_c1_main (a root, in flash) is flagged when not allowed", "fm1_c1_main" in named)
full = dict(base, direct_fn="event", tail_fn="event", leaf_fn="event", fm1_c1_main="core 1 entry")
errs, notes = hot_reach.check(dis, full)
expect("every flash function allowed: passes", not errs)
expect("the note counts them", any("allow-listed" in n for n in notes))
errs, notes = hot_reach.check(dis, dict(full, inram_fn="was in flash"))
expect("a stale entry (now in RAM) is a hard error naming it", len(errs) == 1 and "inram_fn" in errs[0] and "in RAM now" in errs[0])
errs, notes = hot_reach.check(dis, dict(full, nowhere_fn="other variant"))
expect("an entry not in this build is a note, not an error", not errs and any("nowhere_fn" in n for n in notes))
errs, notes = hot_reach.check(dis.replace("fm1_c1_main", "renamed_main"), full)
expect("a missing root fails loudly", len(errs) == 1 and "fm1_c1_main" in errs[0])
print(f"hotreach: {'FAIL' if bad else 'ok'} ({bad} failed)")
sys.exit(1 if bad else 0)
