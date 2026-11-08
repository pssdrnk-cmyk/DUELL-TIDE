#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The hot-code placement check (CPU audit K6; run by tools/build.py after every RAM-placement link, DUELL_RAMPLACE=1).

  tools/hot_reach.py [build/felucca.dis]      (a look by hand: prints the result of the check on a disassembly)

Rule: a function that the audio block reaches (per block or per sample) must run from on-chip RAM (the hot block, range X or range Z), not from
the XIP flash, whose instruction cache misses cost far more. The only way around it is a line in tools/ram_allow.py with a reason.

How it knows what the audio block reaches: the disassembly of the linked ELF (build/felucca.dis). The call graph follows every direct call
and goto (a tail call) from the roots. The roots are the audio interrupt (fm1_alnk0_irq: audio_block, mix_block and the event dispatch are
inlined into it), core 1's entry (fm1_c1_main: tc_job_run is inlined into it) and every function already placed in RAM (hot block, X, Z:
whoever put it there meant it for the audio path, and the engines are called through pointers that a call graph cannot follow). Not
followed: calls through a function pointer, and any function whose address is only taken (a dispatch table, a literal-pool word, inline asm,
a computed jump). A function reached only that way and not placed in RAM is invisible to this check (review finding H1: a new engine voice
function left in flash and reached only through the engine table passes silently; ROADMAP: K6 follow-up). tests: tools/test_hot_reach.py (host group hotreach).

The graph also reaches note-on, MIDI and sequencer code, because the interrupt dispatches the events: those functions run once per event,
not per sample, and are in tools/ram_allow.py with that reason. The check cannot tell the two apart; the allow-list can."""
import bisect
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import elfcheck  # noqa: E402

XIP = 0x02000000                       # the flash is mapped from here; the app image is at 0x02000120
ROOTS = ("fm1_alnk0_irq", "fm1_c1_main")


def parse(dis):
    """objdump -d text -> ({function: [(address, text)]}, {function: start}, {function: bytes}). The labels that start with '.' (the
    jump tables .GJT..., local labels) belong to the function before them"""
    fns, cur = {}, None
    for ln in dis.splitlines():
        m = elfcheck.DIS_LABEL.match(ln)
        if m:
            if not m.group(1).startswith("."):
                cur = fns.setdefault(m.group(1), [])
            continue
        m = elfcheck.DIS_LINE.match(ln)
        if m and cur is not None:
            cur.append((int(m.group(1), 16), m.group(3)))
    start = {n: ins[0][0] for n, ins in fns.items() if ins}
    size = {n: ins[-1][0] + 4 - ins[0][0] for n, ins in fns.items() if ins}
    return fns, start, size


def reach(dis):
    """-> (fns, start, size, parent): every function the roots reach by direct calls; parent[f] = who first reached it (None for a root)"""
    fns, start, size = parse(dis)
    order = sorted((a, n) for n, a in start.items())
    addrs = [a for a, _ in order]
    roots = [r for r in ROOTS if r in start] + sorted(n for n, a in start.items() if a < XIP and n not in ROOTS)
    parent = {r: None for r in roots}
    todo = collections.deque(roots)
    while todo:
        n = todo.popleft()
        for a, text in fns.get(n, []):
            to = elfcheck.dis_target(a, text)
            if to is None:
                continue
            i = bisect.bisect_right(addrs, to) - 1
            if i >= 0 and order[i][1] not in parent:
                parent[order[i][1]] = n
                todo.append(order[i][1])
    return fns, start, size, parent


def chain(parent, n):
    out = []
    while n is not None and len(out) < 8:
        out.append(n)
        n = parent[n]
    return " <- ".join(out)


def check(dis, allow=None):
    """-> (errors, notes): a function in the flash that the audio path reaches and that the allow-list (tools/ram_allow.py) does not name"""
    if allow is None:
        import ram_allow
        allow = ram_allow.ALLOW
    fns, start, size, parent = reach(dis)
    errors, notes = [], []
    missing = [r for r in ROOTS if r not in start]
    if missing:
        return [f"hot placement check: root {missing[0]} is not in the disassembly (renamed? update ROOTS in tools/hot_reach.py)"], notes
    flash = sorted(n for n in parent if start.get(n, 0) >= XIP)
    for n in flash:
        if n in allow:
            continue
        errors.append(
            f"hot code in flash: {n} ({size[n]} B) sits in the XIP flash but the audio path reaches it ({chain(parent, n)}).\n"
            f"         Why it matters: it would run every audio block from slow flash (instruction-cache misses, a longer worst block).\n"
            f"         Fix: place it in range Z (add {n} to a group in tools/ram_fns_z.py; mind the 8,192 B reserve), or, if it runs only\n"
            f"         on an event (a note-on, a load, a mode change), add it to tools/ram_allow.py with a one-line reason.")
    ram_now = sorted(n for n in allow if n in start and start[n] < XIP)
    gone = sorted(n for n in allow if n not in start)
    allowed = [n for n in flash if n in allow]
    notes.append(f"hot placement: {len(parent)} functions reached from the audio interrupt, core 1 and the RAM-placed code; "
                 f"{len(flash)} in flash, {len(allowed)} allow-listed ({sum(size[n] for n in allowed)} B), {len(flash) - len(allowed)} not")
    if ram_now:                                          # stale: it was moved to RAM, the line no longer says anything true (a hard error so the list cannot rot)
        errors.append(f"hot placement: {len(ram_now)} allow-list entries are in RAM now, remove them from tools/ram_allow.py: {', '.join(ram_now[:6])}"
                      + (" ..." if len(ram_now) > 6 else ""))
    if gone:
        notes.append(f"hot placement: {len(gone)} allow-list entries are not in this build (another variant, or gone): {', '.join(gone[:6])}"
                     + (" ..." if len(gone) > 6 else ""))
    return errors, notes


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "felucca.dis")
    errs, nts = check(open(path).read())
    for x in nts:
        print("  ok   ", x)
    for x in errs:
        print("  FAIL ", x)
    sys.exit(1 if errs else 0)
