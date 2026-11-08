#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Document checks that need no build: python3 jam/check_docs.py (from the repository root).

1. The STYLE tables of README.md and docs/TIDE_GUIDE.md equal STY[] in tide_ui.c (the new-unit default is STY[0]).
2. No document that ships names a workspace path (`work/...`) or a local drive path.
3. docs/PATCH_NUMBERS.md lists exactly the factory sounds the generator builds (DS_N): the count, and each number's name.
4. The printed cheat sheet's TIDE rows (the TIDE SETTINGS rows and the AUTO TIDE knobs) carry the same names as docs/TIDE_GUIDE.md and tide_ui.c.
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
NAMES = ["DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK", "DRONE"]


def code_styles():
    text = open(os.path.join(SRC, "tide_ui.c"), encoding="utf-8").read()
    m = re.search(r"STY\[6\]\[9\]\s*=\s*\{(.*?)\};", text, re.S)
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
    if rel.startswith(("work/", "docs/audits/", "felucca-base/build", "felucca-base/node_modules")) or "/history/" in rel:   # docs/audits: internal audit records, they name workspace paths
        continue
    for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
        if re.search(r"`work/|[(]work/|C:[/]Users|C:[^/a-z ]Users", line):
            bad.append("%s:%d: names a workspace or local path" % (rel, n))



# 3. PATCH_NUMBERS = the generator's sound list (the same one that writes DS_N into sounds.c)
def generated_sounds():
    import importlib.util
    tools = os.path.join(ROOT, "felucca-base", "tools")
    sys.path.insert(0, tools)
    spec = importlib.util.spec_from_file_location("gen_duell_sounds", os.path.join(tools, "gen_duell_sounds.py"))
    gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gen)
    sounds, lst, _engines, _cats, _idname = gen.build(gen.load_data(os.path.join(tools, "duell_sounds.py")))
    return [sounds[g]["name"] for g in lst]


try:
    names = generated_sounds()
except Exception as e:  # the generator must run: it is the source of the count
    names = []
    bad.append("docs/PATCH_NUMBERS.md: the sound generator did not run (%s)" % e)
if names:
    rows = {}
    for line in open(os.path.join(ROOT, "docs", "PATCH_NUMBERS.md"), encoding="utf-8"):
        m = re.match(r"\|\s*(\d+)\s*\|\s*(.+?)\s*\|\s*$", line)
        if m:
            rows[int(m.group(1))] = m.group(2)
    if len(rows) != len(names):
        bad.append("docs/PATCH_NUMBERS.md: %d rows, the generator builds %d sounds (DS_N)" % (len(rows), len(names)))
    for n, name in enumerate(names, 1):
        if rows.get(n) != name:
            bad.append("docs/PATCH_NUMBERS.md: #%d is %r, the generator says %r" % (n, rows.get(n), name))
            break
    for path, pat in (("docs/PATCH_NUMBERS.md", r"The (\d+) factory sounds"), ("docs/DOCS_INDEX.md", r"The (\d+) factory sounds")):
        m = re.search(pat, open(os.path.join(ROOT, path), encoding="utf-8").read())
        if not m or int(m.group(1)) != len(names):
            bad.append("%s: says %s factory sounds, the generator builds %d" % (path, m.group(1) if m else "no count for the", len(names)))

# 4. the cheat sheet's TIDE rows carry the guide's names
def text_of(html):
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", html)).strip()


def cheat_names(section_title):
    html = open(os.path.join(ROOT, "felucca-base", "web", "cheatsheet", "index.html"), encoding="utf-8").read()
    m = re.search(r"<h2>%s</h2>(.*?)</section>" % re.escape(section_title), html, re.S)
    if not m:
        return None
    out = set()
    for g in re.findall(r'<div class="g">(.*?)</div>', m.group(1), re.S):
        for part in text_of(g).split(" / "):
            out.add(part.strip())
    return out


guide = open(os.path.join(ROOT, "docs", "TIDE_GUIDE.md"), encoding="utf-8").read()
sec3 = guide[guide.index("## 3. TIDE SETTINGS"):guide.index("## 4. MODE")]
guide_rows = set(re.findall(r"^\| \*\*([A-Z0-9+ ]+)\*\* \|", sec3, re.M))
src = open(os.path.join(SRC, "tide_ui.c"), encoding="utf-8").read()
code_rows = set(re.findall(r'"([A-Z ]+)"', re.search(r"JL_NAME\[JL_N\]\s*=\s*\{(.*?)\};", src, re.S).group(1)))
if guide_rows != code_rows:
    bad.append("docs/TIDE_GUIDE.md: TIDE SETTINGS rows %s, tide_ui.c JL_NAME says %s" % (sorted(guide_rows), sorted(code_rows)))
cheat_rows = cheat_names("TIDE: TIDE SETTINGS")
if cheat_rows is None:
    bad.append("cheat sheet: no TIDE SETTINGS section")
else:
    cheat_rows -= {"K1 picks the row"}
    # the cheat sheet may group rows on one line ("TURNS / POLY / SYNC / ARC"), never rename or drop one
    if cheat_rows != code_rows:
        bad.append("cheat sheet: TIDE SETTINGS rows %s, the rows are %s" % (sorted(cheat_rows), sorted(code_rows)))
guide_knobs = set(re.findall(r"^\| [1-4] \| \*\*([A-Z]+)\*\* \|", guide[guide.index("## 2. AUTO TIDE"):guide.index("## 3. TIDE SETTINGS")], re.M))
cheat_knobs = cheat_names("TIDE: AUTO TIDE")
if cheat_knobs is None:
    bad.append("cheat sheet: no AUTO TIDE section")
else:
    got = {re.sub(r"^K[1-4] ", "", c) for c in cheat_knobs if re.match(r"K[1-4] [A-Z]+$", c)}
    if got != guide_knobs:
        bad.append("cheat sheet: AUTO TIDE knobs %s, the guide says %s" % (sorted(got), sorted(guide_knobs)))

for b in bad:
    print("FAIL", b)
print("docs check: %s" % ("FAILED (%d)" % len(bad) if bad else "ok"))
sys.exit(1 if bad else 0)
