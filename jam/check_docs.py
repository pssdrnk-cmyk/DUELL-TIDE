#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Document checks that need no build: python3 jam/check_docs.py (from the repository root).

1. The STYLE tables of README.md and docs/TIDE_GUIDE.md equal STY[] in tide_ui.c (the new-unit default is STY[0]).
2. No document that ships names a workspace path (`work/...`) or a local drive path.
Exit 0 when all hold; each failure is one line."""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "felucca-base", "firmware", "src")
bad = []

# the names of each STY column, in the order of the C table (mode, time idx, wait idx, vary, turns, poly, sync, arc, wash)
MODE = {"GM_ONCE": "ONCE", "GM_LOOP": "LOOP", "GM_DRIFT": "DRIFT", "GM_WANDER": "WNDR", "GM_JOURNEY": "JRNY"}
TIME = ["1", "2", "4", "8", "16", "32", "64"]
WAIT = ["OFF", "1", "2", "4", "8", "16"]
VARY = ["EVERY 4TH", "EVERY BAR"]
TURNS = ["TOGETHER", "TAKE TURNS", "LEAD"]
POLY = ["OFF", "GENTLE", "WIDE"]
SYNC = ["OFF", "SOFT", "PUSH"]
ARC = ["OFF", "RISE", "DROP"]
WASH = ["OFF", "LOW", "MID", "HIGH"]
NAMES = ["DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK"]


def code_styles():
    text = open(os.path.join(SRC, "tide_ui.c"), encoding="utf-8").read()
    m = re.search(r"STY\[5\]\[9\]\s*=\s*\{(.*?)\};", text, re.S)
    rows = re.findall(r"\{([^{}]*)\}", m.group(1))
    out = []
    for r in rows:
        v = [x.strip() for x in r.split(",")]
        out.append([MODE[v[0]], TIME[int(v[1])], WAIT[int(v[2])], VARY[int(v[3])], TURNS[int(v[4])],
                    POLY[int(v[5])], SYNC[int(v[6])], ARC[int(v[7])], WASH[int(v[8])]])
    return out


def norm(cell):
    return re.sub(r"\s*bars?$", "", cell.replace("**", "").strip())


def doc_styles(path):
    """rows of the table whose first cell is a STYLE name: README has nine columns, the guide one cell of nine values"""
    found = {}
    for line in open(os.path.join(ROOT, path), encoding="utf-8"):
        if not line.startswith("|"):
            continue
        cells = [norm(c) for c in line.strip().strip("|").split("|")]
        if cells[0] not in NAMES:
            continue
        vals = cells[1:]
        if len(vals) == 1:
            vals = [norm(c) for c in vals[0].split(", ")]
        found[cells[0]] = vals
    return found


want = code_styles()
for path in ("README.md", "docs/TIDE_GUIDE.md"):
    got = doc_styles(path)
    for i, name in enumerate(NAMES):
        if name not in got:
            bad.append("%s: no STYLE row for %s" % (path, name))
        elif got[name] != want[i]:
            bad.append("%s: STYLE %s is %s, tide_ui.c says %s" % (path, name, got[name], want[i]))

# no workspace or local paths in the documents that ship (history pages are exempt)
for path in glob.glob(os.path.join(ROOT, "**", "*.md"), recursive=True):
    rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
    if rel.startswith(("work/", "felucca-base/build", "felucca-base/node_modules")) or "/history/" in rel:
        continue
    for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
        if re.search(r"`work/|[(]work/|C:[/]Users|C:[^/a-z ]Users", line):
            bad.append("%s:%d: names a workspace or local path" % (rel, n))

for b in bad:
    print("FAIL", b)
print("docs check: %s" % ("FAILED (%d)" % len(bad) if bad else "ok"))
sys.exit(1 if bad else 0)
