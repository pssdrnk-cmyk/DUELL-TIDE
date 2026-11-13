#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The hot-code placement check (CPU audit K6; run by tools/build.py after every RAM-placement link, DUELL_RAMPLACE=1).

  tools/hot_reach.py [build/felucca.dis [build/felucca.elf]]      (a look by hand: prints the result of the check on a disassembly; the ELF
next to it is read when there is one)

Rule: a function that the audio block reaches (per block or per sample) must run from on-chip RAM (the hot block, range X or range Z), not from
the XIP flash, whose instruction cache misses cost far more. The only way around it is a line in tools/ram_allow.py with a reason.

How it knows what the audio block reaches: the disassembly of the linked ELF (build/felucca.dis). The call graph follows every direct call
and goto (a tail call) from the roots. The roots are the audio interrupt (fm1_alnk0_irq: audio_block, mix_block and the event dispatch are
inlined into it), core 1's entry (fm1_c1_main: tc_job_run is inlined into it) and every function already placed in RAM (hot block, X, Z:
whoever put it there meant it for the audio path, and the engines are called through pointers that a call graph cannot follow). Not
followed: a computed jump. Function pointers ARE followed (review finding H1, K6 follow-up): a reached function that takes the address of a function
(an immediate "r1 = N <fn : addr>") gets an edge to it, and one that takes the address of a data object (the engine table ENGINES, an engine's
engine_t, a dispatch table) gets an edge to every function whose start address is a word of that object, and the objects whose address is a
word of it in turn (ENGINES -> ENG_WAVE -> its voice functions). The data comes from the linked ELF (tools/elfcheck.py read_elf); without one
(check(dis) alone, a look by hand) only the address-taken functions in the code are followed. The chain then reads "fn <-[ENGINES]- caller".
tests: tools/test_hot_reach.py (host group hotreach).

The graph also reaches note-on, MIDI and sequencer code, because the interrupt dispatches the events: those functions run once per event,
not per sample, and are in tools/ram_allow.py with that reason. The check cannot tell the two apart; the allow-list can."""
import bisect
import collections
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import elfcheck  # noqa: E402

XIP = 0x02000000                       # the flash is mapped from here; the app image is at 0x02000120
ROOTS = ("fm1_alnk0_irq", "fm1_c1_main")
ADDR_REF = re.compile(r"<[^:>]*:\s*([0-9a-f]+)\s*>")       # objdump's "<symbol+off : address >" after an immediate (a call / goto prints the same)


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


class Data:
    """the data of the linked ELF that holds function pointers: the objects (a symbol with a size, not a function) in the sections whose bytes the
    ELF reader kept, and a 32-bit word reader over them"""

    def __init__(self, elf):
        self.secs = sorted((elf["sections"][n][0], n, b) for n, b in elf["contents"].items() if n in elf["sections"])
        fstarts = {a for a, _ in elf["funcs"]}
        self.objs = sorted((a, sz, n) for n, a, sz, sec in elf["symbols"] if sz and sec in elf["contents"] and a not in fstarts)
        self.addrs = [o[0] for o in self.objs]
        self.fstarts = fstarts

    def words(self, a, size):
        for base, _, b in self.secs:
            if base <= a < base + len(b):
                off = a - base
                end = min(off + size, len(b)) // 4 * 4
                return struct.unpack_from(f"<{(end - off) // 4}I", b, off) if end > off else ()
        return ()

    def obj(self, a):
        """-> (start, size, name) of the data object that holds address a, or None"""
        i = bisect.bisect_right(self.addrs, a) - 1
        return self.objs[i] if i >= 0 and a < self.objs[i][0] + self.objs[i][1] else None


def reach(dis, elf=None):
    """-> (fns, start, size, parent, via): every function the roots reach by direct calls and by taken addresses; parent[f] = who first reached it
    (None for a root), via[f] = the data object the pointer came out of (only for an edge through a table)"""
    fns, start, size = parse(dis)
    order = sorted((a, n) for n, a in start.items())
    addrs = [a for a, _ in order]
    data = Data(elf) if elf else None
    fstart = {a: n for n, a in start.items() if data is None or a in data.fstarts}   # (with an ELF, a data object's label is not a function)
    roots = [r for r in ROOTS if r in start] + sorted(n for n, a in start.items() if a < XIP and n not in ROOTS)
    parent = {r: None for r in roots}
    via = {}
    todo = collections.deque(roots)

    def add(f, by, obj=None):
        if f not in parent:
            parent[f] = by
            if obj:
                via[f] = obj
            todo.append(f)

    def table(a, by, seen):
        """the address a of a data object: every function start among its words is reached, every object start among them is looked into"""
        o = data.obj(a)
        if o is None or o[0] in seen:
            return
        seen.add(o[0])
        for w in data.words(o[0], o[1]):
            if w in fstart:
                add(fstart[w], by, o[2])
            elif w != a and data.obj(w) is not None and data.obj(w)[0] == w:
                table(w, by, seen)
    seen = set()                     # (the objects looked into: once is enough, whoever reaches it first is the one named)
    while todo:
        n = todo.popleft()
        for a, text in fns.get(n, []):
            to = elfcheck.dis_target(a, text)
            if to is not None:
                i = bisect.bisect_right(addrs, to) - 1
                if i >= 0:
                    add(order[i][1], n)
            elif "<" in text:
                for m in ADDR_REF.finditer(text):
                    w = int(m.group(1), 16)
                    if w in fstart:                  # the address of a function taken (stored, passed, put in a table)
                        add(fstart[w], n)
                    elif data is not None:           # the address of a data object: its function pointers
                        table(w, n, seen)
    return fns, start, size, parent, via


def chain(parent, n, via=None):
    """the reach as text: fn <- caller <- ... (at most 8 names); a step through a data table reads fn <-[TABLE]- caller"""
    out, k = n, 1
    while parent[n] is not None and k < 8:
        out += f" <-[{via[n]}]- " if via and n in via else " <- "
        n = parent[n]
        out += n
        k += 1
    return out


def check(dis, allow=None, elf=None):
    """-> (errors, notes): a function in the flash that the audio path reaches and that the allow-list (tools/ram_allow.py) does not name"""
    if allow is None:
        import ram_allow
        allow = ram_allow.ALLOW
    fns, start, size, parent, via = reach(dis, elf)
    errors, notes = [], []
    missing = [r for r in ROOTS if r not in start]
    if missing:
        return [f"hot placement check: root {missing[0]} is not in the disassembly (renamed? update ROOTS in tools/hot_reach.py)"], notes
    flash = sorted(n for n in parent if start.get(n, 0) >= XIP)
    for n in flash:
        if n in allow:
            continue
        errors.append(
            f"hot code in flash: {n} ({size[n]} B) sits in the XIP flash but the audio path reaches it ({chain(parent, n, via)}).\n"
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
    elf_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(path), "felucca.elf")
    elf = elfcheck.read_elf(open(elf_path, "rb").read()) if os.path.exists(elf_path) else None
    errs, nts = check(open(path).read(), elf=elf)
    for x in nts:
        print("  ok   ", x)
    for x in errs:
        print("  FAIL ", x)
    sys.exit(1 if errs else 0)
