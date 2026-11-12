#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Every host group is run by default or says why not (b11 tests lane).

host_tests.sh names its groups twice: the default list (`want="${*:-...}"`, what CI's shards and `check.sh --changed` run, jam/fastcheck.py default_groups) and the
case labels of its big `case $t in`. A group that is a case but not in the list is never run by anyone: take and answer (live assist) and guards were like that
until PR 62. This check fails when

  - a case label is neither in the default list nor marked out by a line   # not default: LABEL - reason
    (one line a label, anywhere in host_tests.sh, normally just above the case; the reason is not empty),
  - a label is marked "not default" and is in the default list (a stale marker),
  - the default list names something that is no case label (a typo that would make a run fail or skip), or lists a name twice,
  - a "not default" line names a label that is no case.
`$EMU` in the default list stands for the label `emu` (it is added only when ~/emsdk exists).

  python3 jam/check_groups.py [path/to/host_tests.sh]     (exit 0 = fine, 1 = the problems, one a line)
"""
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse(text):
    m = re.search(r'^want="\$\{\*:-([^"]*)\}"$', text, re.M)
    if not m:
        raise SystemExit("check_groups: no want=\"${*:-...}\" line in host_tests.sh")
    default = m.group(1).split()
    start = text.find("for t in $want; do case $t in")
    if start < 0:
        raise SystemExit("check_groups: no `for t in $want; do case $t in` in host_tests.sh")
    labels = []
    for lm in re.finditer(r"^  ([A-Za-z0-9_|]+)\) ", text[start:], re.M):
        labels += lm.group(1).split("|")
    marks = {}
    for mm in re.finditer(r"^[ \t]*#[ \t]*not default:[ \t]*([A-Za-z0-9_|]+)[ \t]+-[ \t]+(\S.*)$", text, re.M):
        for lab in mm.group(1).split("|"):
            marks[lab] = mm.group(2).strip()
    return default, labels, marks


def problems(text):
    default, labels, marks = parse(text)
    out = []
    seen = set()
    for d in default:
        if d in seen:
            out.append("the default list names %s twice" % d)
        seen.add(d)
    dset = {("emu" if d == "$EMU" else d) for d in default}
    lset = set(labels)
    for d in sorted(dset - lset):
        out.append("the default list names %s, which is no case of host_tests.sh" % d)
    for lab in sorted(lset - dset):
        if lab not in marks:
            out.append("group %s is a case of host_tests.sh but neither in the default list (so CI and --changed never run it) nor marked `# not default: %s - reason`" % (lab, lab))
    for lab in sorted(marks):
        if lab not in lset:
            out.append("`# not default: %s` names no case" % lab)
        elif lab in dset:
            out.append("group %s is marked `# not default` but is in the default list" % lab)
    return out


def main(argv):
    path = Path(argv[1]) if len(argv) > 1 else ROOT / "host_tests.sh"
    with open(path, encoding="utf-8", newline="") as fh:
        text = fh.read()
    probs = problems(text)
    default, labels, marks = parse(text)
    for p in probs:
        print("check_groups: FAIL " + p)
    print("check_groups: %d case labels, %d in the default list, %d marked not default%s" % (len(set(labels)), len(set(default)), len(marks), "" if not probs else ": %d problem(s)" % len(probs)))
    return 1 if probs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
