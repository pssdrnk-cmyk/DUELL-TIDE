#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Multi record converter, version 7 -> 8 (firmware/src/multi.c MU_VER 8, ROLL CHAINED storage). DUELL 1.1 loads only version 8; it converted its
factory multis (tools/factory_multis/) but not the test scenes (tests/scenes/, tests/scenes.h), and its converter was not committed. This is one.

    tools/multi_v7_to_v8.py [--check] [FILE ...]      default: tests/scenes/*.bin and tests/scenes/spare/*.bin, in place
    tools/multi_v7_to_v8.py --out DIR FILE ...         write the converted records to DIR (same names) instead

What changes (multi.c's record comment of 1.0 and of 1.1, params.c, ext.h):
  header      byte 0 version 7 -> 8, byte 2 ng 27 -> MU_NG 18: only the globals a multi loads are stored (MU_GLOBAL, in id order)
  per part    End B: the 7-byte seed block (and the hand-set B edit list after the values) -> a length byte and morph.c's MBV block, End B as values.
              The values are made by the firmware's own code (tests/multi_v7_endb.c: the record loaded, then 1.0's morph_load_part with the seed block and
              the edits, then morph_bv_make): what 1.0 rolled at each load. A part with neither block nor edits stays without a block (a fresh roll at load, as before).
              The NX extension values: no count byte; 16 -> 28 slots (1.1's X16..X27 get their defaults, up_xb.h XB_DEFS, as 1.0's reader gave a shorter list's rest)
              steps: 7 bytes each (ps_pack_step's bytes 0..5 and 8; 6 and 7, the old drum lane, must be 0)
              FM6 patch: 128 bytes -> 112 (7-bit values packed 8 to 7, upreset.c upf_pk_put)
  SWING       G_SWING and each part's P_ASWING, P_SSWING: 0..100 -> 50..75 % (the old timing scale: old = (new - 50) * 4; since the swing fix the new value is the percentage itself). An old value that is not a multiple of 4
              goes to the nearest (reported). (1.1's own factory multis turned 15 and 17 into 50, straight; this keeps the swing as near as 1.1 can play it.)
  tail        the automation as it was, then XFADE, SPLIT: the BALANCE byte before them is gone (1.1 has no BALANCE; a non-zero one is reported)
Everything else is copied byte for byte. A record already version 8 is left as it is; any other version, or one that does not parse to its own length, stops.
The harness is built with the host test flags against build/gen and build/gen_test (host_tests.sh makes them), DUELL_TIDE=0: plain DUELL's roll, the one 1.0 and
1.1 share (tools/multi_v7_to_v8_test.py checks it against 1.1's own converted factory multis).
The harness rolls with 1.0's RES ceilings (-DMORPH_UF_RESBP=40 -DMORPH_UF_RES=60; the firmware's are 17 and 28 in the new RES law since audit P07): the record's own values are copied in the
law they were stored in, so the End B rolled from them is the one 1.0 rolled. A converted record keeps its old RES values; it is not migrated to the new law.
--check changes nothing: lists what would be converted, exit 1 if anything would."""
import argparse
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import endb
MU_G_AT, MB_N, PS_STEP, MU_STEP, FM6_PACKED, NSHOWN, NX7, NG7, NP = 20, 7, 9, 7, 128, 2, 16, 27, 127
# core.h's global ids that multi.c MU_GLOBAL stores: BPM SWING, DTIME..CDEPTH, DMRATE DMDEP, DMODE, SHMOD, RLOCUT, RTYPE,
# PINGSPREAD, SHPITCH (the ids once G_MIDI, G_SYNC, G_INFO, G_NAME, G_FREE26: core.h)
MU_GLOBALS = (0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 17, 21, 24, 25, 26)
G_SWING, P_ASWING, P_SSWING = 1, 21, 31
HARNESS = ROOT / "build" / "host" / "multi_v7_endb"


class V7Error(Exception):
    pass


def xb_defs():
    """XB_DEFS of firmware/src/up_xb.h: the stored (value + 64) defaults of 1.1's NX extension slots"""
    src = (ROOT / "firmware" / "src" / "up_xb.h").read_text(encoding="utf-8")
    d = [int(x.strip().rstrip("uU")) for x in re.search(r"#define XB_DEFS \{([^}]*)\}", src).group(1).split(",")]
    nx = int(re.search(r"#define NX (\d+)", (ROOT / "firmware" / "src" / "ext.h").read_text(encoding="utf-8")).group(1))
    if len(d) != nx or nx < NX7:
        sys.exit("multi_v7_to_v8: XB_DEFS has %d entries, NX is %d" % (len(d), nx))
    return bytes(d)


def swing(old):
    """1.0's SWING (0..100) -> 1.1's (50..75 %), the nearest 1.1 plays"""
    return min(75, 50 + (max(0, old) + 2) // 4)


def parse_v7(b):
    """version 7 record -> (header bytes 0..19, globals [27], parts [dict], automation bytes (count + events), balance, xfade, split)"""
    if len(b) < MU_G_AT or b[0] != 7:
        raise V7Error("not a version 7 record (version %d)" % (b[0] if b else -1))
    if b[1] != NP or b[2] != NG7:
        raise V7Error("np %d, ng %d: version 7 of 1.0 has %d, %d" % (b[1], b[2], NP, NG7))
    pos = MU_G_AT
    glob = [int.from_bytes(b[pos + 2 * i:pos + 2 * i + 2], "little", signed=True) for i in range(NG7)]
    pos += 2 * NG7
    parts = []
    for _ in range(NSHOWN):
        p = {"eng": b[pos], "preset": b[pos + 1], "amt": b[pos + 2], "fl": b[pos + 3]}
        if p["fl"] & ~1:
            raise V7Error("part flags 0x%02x" % p["fl"])
        pos += 4
        p["blk"] = bytes(b[pos:pos + MB_N])
        pos += MB_N
        p["vals"] = bytes(b[pos:pos + NP])
        pos += NP
        if b[pos] != NX7:
            raise V7Error("nx %d, 1.0 wrote %d" % (b[pos], NX7))
        p["x"] = bytes(b[pos + 1:pos + 1 + NX7])
        pos += 1 + NX7
        ne = b[pos]
        p["edits"] = bytes(b[pos + 1:pos + 1 + 2 * ne])
        pos += 1 + 2 * ne
        p["map"] = bytes(b[pos:pos + 8])
        pos += 8
        steps = []
        for _ in range(sum(bin(x).count("1") for x in p["map"])):
            st = bytes(b[pos:pos + PS_STEP])
            if len(st) != PS_STEP:
                raise V7Error("steps run past the end")
            if st[6] or st[7]:
                raise V7Error("a step with drum lane bytes (6, 7 not 0)")
            steps.append(st)
            pos += PS_STEP
        p["steps"] = steps
        p["fm6"] = None
        if p["fl"] & 1:
            p["fm6"] = bytes(b[pos:pos + FM6_PACKED])
            if len(p["fm6"]) != FM6_PACKED or any(x > 127 for x in p["fm6"]):
                raise V7Error("FM6 patch short or not 7-bit")
            pos += FM6_PACKED
        parts.append(p)
    if pos >= len(b):
        raise V7Error("no automation count")
    nev = b[pos]
    auto = bytes(b[pos:pos + 1 + 3 * nev])
    pos += 1 + 3 * nev
    if pos + 3 != len(b):
        raise V7Error("the record does not parse to its own length (%d != %d)" % (pos + 3, len(b)))
    return bytes(b[:MU_G_AT]), glob, parts, auto, b[pos], b[pos + 1], b[pos + 2]


def pk87(fm6):
    """upreset.c upf_pk_put: 128 7-bit bytes -> 112"""
    acc = n = 0
    out = bytearray()
    for v in fm6:
        acc |= (v & 127) << n
        n += 7
        while n >= 8:
            out.append(acc & 255)
            acc >>= 8
            n -= 8
    return bytes(out)


def build_v8(v7, defs, bv=(b"", b"")):
    """the version 8 record of parse_v7's v7, part pi's End B = bv[pi] (an MBV block; empty: none)"""
    hdr, glob, parts, auto, _bal, xf, split = v7
    out = bytearray(hdr)
    out[0], out[2] = 8, len(MU_GLOBALS)
    for g in MU_GLOBALS:
        out += (swing(glob[g]) if g == G_SWING else glob[g]).to_bytes(2, "little", signed=True)
    for p, blk in zip(parts, bv):
        vals = bytearray(p["vals"])
        for i in (P_ASWING, P_SSWING):
            vals[i] = swing(vals[i] - 64) + 64
        out += bytes([p["eng"], p["preset"], p["amt"], p["fl"], len(blk)]) + blk + vals + p["x"] + defs[NX7:] + p["map"]
        for st in p["steps"]:
            out += st[:6] + st[8:9]
        if p["fm6"] is not None:
            out += pk87(p["fm6"])
    out += auto + bytes([xf, split])
    if len(out) > 1916:
        raise V7Error("the converted record is %d bytes, over 1916" % len(out))
    return bytes(out)


def notes(v7):
    """what the conversion cannot keep exactly, as text"""
    _hdr, glob, parts, _auto, bal, _xf, _split = v7
    sw = [("SWING", glob[G_SWING])] + [("P%d %s" % (k + 1, n), p["vals"][i] - 64) for k, p in enumerate(parts) for n, i in (("A.SWG", P_ASWING), ("S.SWG", P_SSWING))]
    out = ["%s %d -> %d %% (old scale %d)" % (n, v, swing(v), (swing(v) - 50) * 4) for n, v in sw if v % 4 or not 0 <= v <= 100]
    if bal:
        out.append("BALANCE %d dropped (1.1 has none)" % (bal - 256 if bal > 127 else bal))
    return out


def run_sh(cmd):
    if os.name == "nt":                                    # (Git Bash: `cc` is a shell shim with the POSIX stand-ins, jam/env.sh; the command goes on stdin, Windows argv quoting would break the quotes)
        return subprocess.run(["sh"], cwd=ROOT, input=shlex.join(cmd), text=True).returncode
    return subprocess.run(cmd, cwd=ROOT).returncode


def compile_harness():
    HARNESS.parent.mkdir(parents=True, exist_ok=True)
    for h in ("build/gen/duell_multis.h", "build/gen_test/duell_multis.h", "build/gen/felucca_tables.h"):
        if not (ROOT / h).exists():
            sys.exit("multi_v7_to_v8: %s missing (run the device build and host_tests.sh once: sh jam/check.sh <group>)" % h)
    off = ROOT / "build" / "gen_off" / "duell_multis.h"        # (TIDE b6: the harness is plain DUELL, so its factory multis header is plain DUELL's: no M17, M18, unpacked)
    off.parent.mkdir(parents=True, exist_ok=True)
    if subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "gen_duell_multis.py"), str(off), "--test-mode"], cwd=ROOT, capture_output=True,
                      env=dict(os.environ, DUELL_TIDE="0")).returncode:
        sys.exit("multi_v7_to_v8: tools/gen_duell_multis.py with DUELL_TIDE=0 failed")
    cc = ["cc", "-O1", "-w", "-DHOST_MIX_FOLLOWS_MASTER=1", "-DDUELL_TEST_MODE=1", "-DDUELL_TIDE=0", "-Ibuild/gen_off", "-Ibuild/gen_test", "-Ibuild/gen",
          "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="x"', "-DMORPH_UF_RESBP=40", "-DMORPH_UF_RES=60", "-o", str(HARNESS), "tests/multi_v7_endb.c", "-lm"]
    if run_sh(cc):
        sys.exit("multi_v7_to_v8: tests/multi_v7_endb.c does not compile")


def blk_form(eng, blk):
    """the harness is the firmware, and morph_bv_make sets MBV_W4 in every block it makes (b9). A converted 1.0 record is a record from before b8: its block says W4 only where
    ANALOG's WAVE is in it (tools/endb.py encode), so the blocks without it stay byte for byte what 1.1 has (the committed scenes, tests/multi_v7_to_v8_test.py)"""
    if blk and blk[0] & 8 and not (eng == 0 and blk[2 + (endb.E0 >> 3)] >> (endb.E0 & 7) & 1):
        return bytes([blk[0] & ~8]) + blk[1:]
    return blk


def convert(records):
    """version 7 records (bytes) -> (version 8 records, parsed v7); the End B blocks from the harness (compiled first). V7Error on a record it cannot parse"""
    defs = xb_defs()
    parsed = [parse_v7(b) for b in records]
    compile_harness()
    h = lambda x: x.hex() or "-"
    lines = [" ".join([build_v8(v, defs).hex()] + [h(f) for p in v[2] for f in (p["blk"], p["edits"])]) for v in parsed]
    exe = HARNESS.with_suffix(".exe") if HARNESS.with_suffix(".exe").exists() else HARNESS
    r = subprocess.run([str(exe)], cwd=ROOT, input="\n".join(lines) + "\n", capture_output=True, text=True)
    outl = r.stdout.splitlines()
    if r.returncode or len(outl) != len(parsed):
        sys.exit("multi_v7_to_v8: the harness failed (%d): %s" % (r.returncode, r.stderr.strip()))
    return [build_v8(v, defs, tuple(blk_form(p["eng"], b"" if w == "-" else bytes.fromhex(w)) for p, w in zip(v[2], ln.split()))) for v, ln in zip(parsed, outl)], parsed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--out")
    a = ap.parse_args()
    sc = ROOT / "tests" / "scenes"
    files = [Path(f).resolve() for f in a.files] or sorted(sc.glob("*.bin")) + sorted((sc / "spare").glob("*.bin"))
    todo = []
    for f in files:
        b = f.read_bytes()
        if b and b[0] == 8:
            continue
        if not b or b[0] != 7:
            sys.exit("multi_v7_to_v8: %s: version %d, only 7 -> 8 is converted" % (f, b[0] if b else -1))
        todo.append(f)
    if a.check:
        for f in todo:
            print("would convert", f)
        sys.exit(1 if todo else 0)
    try:
        new, parsed = convert([f.read_bytes() for f in todo])
    except V7Error as e:
        sys.exit("multi_v7_to_v8: %s" % e)
    for f, b, v in zip(todo, new, parsed):
        old = f.stat().st_size
        (Path(a.out) / f.name if a.out else f).write_bytes(b)
        print("%s: %d -> %d bytes%s" % (f.name, old, len(b), "".join("; " + n for n in notes(v))))


if __name__ == "__main__":
    main()
