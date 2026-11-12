#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Self-test of tools/hot_reach.py (CPU audit K6) on a hand-made disassembly; host group hotreach (host_tests.sh).

Functions: fm1_alnk0_irq (flash, root) calls direct_fn and tail-calls tail_fn; direct_fn calls leaf_fn; fm1_c1_main (flash, root) calls nothing;
inram_fn is placed in RAM (a root of its own); ptr_fn is in flash and only its address is taken (an immediate in fm1_alnk0_irq: followed since
finding H1); eng_fn is in flash and reached only through a table: fm1_c1_main takes the address of the table ENGINES (a data object in .text),
whose word points to the data object ENG_X, whose words are eng_fn and the RAM function eng_ram_fn (found with the ELF only; the same disassembly
without one does not know the table). A made-up ELF dict carries the sections, symbols and bytes."""
import os
import struct
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
        fn("fm1_alnk0_irq", FLASH + 0x000, [call("call", "direct_fn", FLASH + 0x100), call("goto", "tail_fn", FLASH + 0x200),
                                            call("r1 = 7", "ptr_fn", FLASH + 0x400), "return"]),
        fn("fm1_c1_main", FLASH + 0x040, [call("r2 = 9", "ENGINES", FLASH + 0x800), "return"]),
        fn("direct_fn", FLASH + 0x100, [call("call", "leaf_fn", FLASH + 0x300), "return"]),
        fn("tail_fn", FLASH + 0x200, ["nop", "return"]),
        fn("leaf_fn", FLASH + 0x300, ["nop", "return"]),
        fn("ptr_fn", FLASH + 0x400, ["nop", "return"]),
        fn("inram_fn", RAM + 0x10, ["nop", "return"]),
        fn("eng_fn", FLASH + 0x500, ["nop", "return"]),
        fn("eng_ram_fn", RAM + 0x20, ["nop", "return"]),
        fn("other_fn", FLASH + 0x600, ["nop", "return"]),
        "ENGINES:", "ENG_X:",                                 # (the objdump labels of the two data objects: not functions with an ELF)
    ]) + NL


def elf():
    """the made-up ELF: .text at FLASH, ENGINES (one word: &ENG_X) at +0x800, ENG_X (eng_fn, eng_ram_fn, a number) at +0x810"""
    text = bytearray(0x900)
    struct.pack_into("<I", text, 0x800, FLASH + 0x810)
    struct.pack_into("<III", text, 0x810, FLASH + 0x500, RAM + 0x20, 123456)
    funcs = [(FLASH + a, FLASH + a + 8) for a in (0x0, 0x40, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600)] + [(RAM + 0x10, RAM + 0x18), (RAM + 0x20, RAM + 0x28)]
    syms = [("ENGINES", FLASH + 0x800, 4, ".text"), ("ENG_X", FLASH + 0x810, 12, ".text"), ("table_unused", FLASH + 0x820, 4, ".text")]
    return {"sections": {".text": (FLASH, len(text))}, "contents": {".text": bytes(text)}, "symbols": syms, "funcs": sorted(funcs)}


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
expect("a function whose address is only taken in the code is followed (finding H1)", "ptr_fn" in named and "ptr_fn <- fm1_alnk0_irq" in named)
expect("without an ELF a table is not known: eng_fn is not reached", "eng_fn" not in named)
expect("a function in RAM is not an error", "inram_fn" not in named)
expect("fm1_c1_main (a root, in flash) is flagged when not allowed", "fm1_c1_main" in named)
errs, notes = hot_reach.check(dis, dict(base), elf())
named = " ".join(errs)
expect("with the ELF a flash function reached only through the engine table is flagged (finding H1)", "eng_fn" in named)
expect("... naming the way: the table's object and the function that took its address", "eng_fn <-[ENG_X]- fm1_c1_main" in named)
expect("a RAM function in the same table is no error", "eng_ram_fn" not in named)
expect("a flash function that nothing points to stays unreached", "other_fn" not in named)
expect("an object in the table that is not a function is not an error", "ENG_X (" not in named and "ENGINES (" not in named)
full = dict(base, direct_fn="event", tail_fn="event", leaf_fn="event", fm1_c1_main="core 1 entry", ptr_fn="event")
errs, notes = hot_reach.check(dis, full)
expect("every flash function allowed (no ELF): passes", not errs)
errs, notes = hot_reach.check(dis, full, elf())
expect("... with the ELF the table's function is still not allowed: fails, only eng_fn", len(errs) == 1 and "eng_fn" in errs[0])
full["eng_fn"] = "event"
errs, notes = hot_reach.check(dis, full, elf())
expect("allowed through the table too: passes", not errs)
expect("the note counts them", any("allow-listed" in n for n in notes))
errs, notes = hot_reach.check(dis, dict(full, inram_fn="was in flash"))
expect("a stale entry (now in RAM) is a hard error naming it", len(errs) == 1 and "inram_fn" in errs[0] and "in RAM now" in errs[0])
errs, notes = hot_reach.check(dis, dict(full, nowhere_fn="other variant"))
expect("an entry not in this build is a note, not an error", not errs and any("nowhere_fn" in n for n in notes))
errs, notes = hot_reach.check(dis.replace("fm1_c1_main", "renamed_main"), full)
expect("a missing root fails loudly", len(errs) == 1 and "fm1_c1_main" in errs[0])
print(f"hotreach: {'FAIL' if bad else 'ok'} ({bad} failed)")
sys.exit(1 if bad else 0)
