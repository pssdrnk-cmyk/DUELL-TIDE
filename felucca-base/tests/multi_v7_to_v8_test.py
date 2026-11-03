#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/multi_v7_to_v8.py (MU_VER 7 -> 8) checked against what DUELL 1.1 did itself, and the committed test scenes against the tool.

  python3 tests/multi_v7_to_v8_test.py        (from felucca-base, after host_tests.sh has made build/gen; host_tests.sh runs it as `multiv8`)

1. the rules as numbers: SWING 0..100 -> 50..75 %, the FM6 8-to-7 packing (upreset.c upf_pk_get's order), a record that is not version 7 is refused;
2. 1.1's own conversion: the sixteen factory multis of v1.0 (version 7, git tag v1.0) converted by the tool against those of v1.1 (version 8, tag v1.1), field by field.
   Equal: the header, the globals, every part's engine, preset, AMOUNT, flags, End B block (the firmware's roll), values, extension slots, steps and FM6 patch, the automation,
   XFADE, SPLIT. Left out, because 1.1 normalizes them itself at every load (so a record holds either form and loads the same): the matrix slots (P 49..72) and the five
   retired depth cards (P 5, 114..118; mod.c mod_legacy_move, mod_preselect), G_SHPITCH (a stored BOTH reads UP, project.c), G_SWING (1.1 clamps an old 15 or 17 into 50..75:
   straight; the tool keeps the nearest swing instead);
3. the committed scenes (tests/scenes, tests/scenes/spare) are version 8 and are the tool's output from the version 7 scenes of v1.1, byte for byte."""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import multi_v7_to_v8 as mv

fails = 0
SKIP_P = set(range(49, 73)) | {5, 114, 115, 116, 117, 118}
G_SHPITCH_AT = 17                                       # its place in MU_GLOBALS (id 26)


def ok(c, what):
    global fails
    print("multi_v7_to_v8: %-110s %s" % (what, "ok" if c else "FAIL"))
    fails += not c


# the tags are not on every clone (the GitHub fork has none): the commits they name, which jam11 contains, are the fallback
TAG_COMMIT = {"v1.0": "e1ef5a74ac70a68f8b7e2bebee31e03f1ffc3a12", "v1.1": "974d87509e7e89db0738c731c8a657968e60e498"}


def git_show(ref, path):
    r = subprocess.run(["git", "show", "%s:felucca-base/%s" % (ref, path)], cwd=ROOT, capture_output=True)
    if r.returncode:
        r = subprocess.run(["git", "show", "%s:felucca-base/%s" % (TAG_COMMIT[ref], path)], cwd=ROOT, capture_output=True)
    if r.returncode:
        sys.exit("multi_v7_to_v8_test: git show %s:%s failed (the tags v1.0 and v1.1, or their commits, are needed)" % (ref, path))
    return r.stdout


def parse_v8(b):
    """version 8 record -> (globals, parts, tail): the fields of multi.c's record comment"""
    ng, nx = b[2], len(mv.xb_defs())
    pos, parts = mv.MU_G_AT + 2 * ng, []
    glob = [b[mv.MU_G_AT + 2 * i:mv.MU_G_AT + 2 * i + 2] for i in range(ng)]
    for _ in range(mv.NSHOWN):
        p = {"head": b[pos:pos + 4]}
        nb = b[pos + 4]
        p["bv"] = b[pos + 5:pos + 5 + nb]
        pos += 5 + nb
        p["vals"] = b[pos:pos + mv.NP]
        p["x"] = b[pos + mv.NP:pos + mv.NP + nx]
        pos += mv.NP + nx
        p["map"] = b[pos:pos + 8]
        pos += 8
        n = sum(bin(x).count("1") for x in p["map"])
        p["steps"] = b[pos:pos + mv.MU_STEP * n]
        pos += mv.MU_STEP * n
        p["fm6"] = b[pos:pos + 112] if p["head"][3] & 1 else b""
        pos += len(p["fm6"])
        parts.append(p)
    return glob, parts, b[pos:]


def unpk87(d):
    """upreset.c upf_pk_get: 112 bytes -> 128 7-bit values"""
    acc = n = 0
    out = []
    for x in d:
        acc |= x << n
        n += 8
        while n >= 7 and len(out) < 128:
            out.append(acc & 127)
            acc >>= 7
            n -= 7
    return bytes(out)


def main():
    # 1. the rules
    ok([mv.swing(v) for v in (0, 1, 2, 15, 17, 50, 98, 100, 120)] == [50, 50, 51, 54, 54, 63, 75, 75, 75], "SWING 0..100 -> 50..75 %, the nearest")
    pat = bytes((i * 37 + 5) & 127 for i in range(128))
    ok(len(mv.pk87(pat)) == 112 and unpk87(mv.pk87(pat)) == pat, "FM6 patch 128 -> 112 bytes and back")
    try:
        mv.parse_v7(b"\x08" + bytes(40))
        ok(False, "a version 8 record is refused by the v7 parser")
    except mv.V7Error:
        ok(True, "a version 8 record is refused by the v7 parser")

    # 2. against 1.1's own converted factory multis
    names = ["tools/factory_multis/M%02d.bin" % i for i in range(1, 17)]
    new, _ = mv.convert([git_show("v1.0", n) for n in names])
    for n, b in zip(names, new):
        ref = git_show("v1.1", n)
        (g1, p1, t1), (g2, p2, t2) = parse_v8(b), parse_v8(ref)
        bad = [k for k in range(20) if b[k] != ref[k]] and ["header"]
        bad += ["global %d" % i for i in range(len(g1)) if g1[i] != g2[i] and i not in (1, G_SHPITCH_AT)]
        for pi, (a, r) in enumerate(zip(p1, p2)):
            bad += ["P%d %s" % (pi + 1, k) for k in ("head", "bv", "x", "map", "steps", "fm6") if a[k] != r[k]]
            bad += ["P%d value %d" % (pi + 1, i) for i in range(mv.NP) if a["vals"][i] != r["vals"][i] and i not in SKIP_P]
        bad += ["tail"] if t1 != t2 else []
        ok(len(b) == len(ref) and not bad, "%s v1.0 -> as v1.1 has it (%d bytes)%s" % (Path(n).name, len(b), " " + ", ".join(bad[:6]) if bad else ""))

    # 3. the committed scenes are the tool's output
    sc = sorted((ROOT / "tests" / "scenes").glob("*.bin")) + sorted((ROOT / "tests" / "scenes" / "spare").glob("*.bin"))
    rel = [f.relative_to(ROOT).as_posix() for f in sc]
    old = [git_show("v1.1", r) for r in rel]
    ok(all(o[0] == 7 for o in old) and all(f.read_bytes()[0] == 8 for f in sc), "%d scenes: version 7 in v1.1, version 8 here" % len(sc))
    conv, _ = mv.convert(old)
    for f, c in zip(sc, conv):
        ok(f.read_bytes() == c, "%s == the tool's output" % f.relative_to(ROOT / "tests" / "scenes").as_posix())

    print("multi_v7_to_v8: %s" % ("PASS" if not fails else "%d FAILED" % fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
