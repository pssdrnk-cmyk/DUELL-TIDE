#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Merge the changelog / roadmap fragments in changes/ into CHANGELOG-TIDE.md and docs/ROADMAP.md (at RC time), then delete them.

  python3 jam/assemble_changes.py [--dry-run] [--keep] [--no-check] [--root DIR]      (from the repository root; test: jam/test_assemble_changes.py)

Why: every branch used to edit the same few lines of CHANGELOG-TIDE.md and ROADMAP.md, so every integration had a conflict there. A branch now writes
changes/<branch>.md instead (file names never clash) and nobody else touches those two files until the assembly.

A fragment (changes/README.md says the same):
    ## changelog
    ### 1.0.0-b10
    - **New (...):** one line a bullet (a bullet may continue on lines that start with a space).
    ## roadmap
    ### Repository and CI
    - one line a bullet
"### <name>" names the target. In CHANGELOG-TIDE.md it is a "## <name>" section (the part after the name, such as " (in progress)", does not matter). In
ROADMAP.md it is a "## <name>" section or a bold label line "**<name>**" (its bullets end at the next blank line). The lines are added at the END of the
target, in the order of the file names and then of the fragment (so two fragments for one heading both land, the one whose file name sorts first on top);
in a CHANGELOG section they go above the placeholder line "- (new lines for the next beta go here ...)". A fragment with an unknown target (or one that is
malformed) stops the whole run before anything is written (nothing is dropped silently). Rows that already
exist (to strike or reword) are not fragment material: edit them in your branch as before.
After writing, jam/check_docs.py runs on the result; if it fails the two files are put back and the fragments stay. --dry-run only reports,
--keep leaves the fragments in place."""
import glob
import os
import re
import subprocess
import sys

TARGETS = {"changelog": "CHANGELOG-TIDE.md", "roadmap": os.path.join("docs", "ROADMAP.md")}
PLACEHOLDER = "- (new lines for the next beta go here"


def parse(path):
    """-> {'changelog'|'roadmap': [(target name, [lines])]}; raises ValueError on a malformed fragment"""
    out, part, cur = {}, None, None
    for n, ln in enumerate(open(path, encoding="utf-8").read().splitlines(), 1):
        m = re.match(r"## (changelog|roadmap)\s*$", ln, re.I)
        if m:
            part, cur = m.group(1).lower(), None
            out.setdefault(part, [])
            continue
        m = re.match(r"### (.+?)\s*$", ln)
        if m:
            if part is None:
                raise ValueError(f"{path}:{n}: '### {m.group(1)}' before a '## changelog' or '## roadmap' line")
            cur = (m.group(1), [])
            out[part].append(cur)
            continue
        if ln.strip() == "":
            continue
        if cur is None:
            raise ValueError(f"{path}:{n}: text outside a '### <target>' part: {ln[:50]}")
        cur[1].append(ln.rstrip())
    return out


def find_target(lines, name):
    """-> (first line index of the target, index after its last line to append at); None if absent"""
    want = name.lower()
    for i, ln in enumerate(lines):
        low = ln.lower()
        m = re.match(r"## (.+?)\s*$", ln)
        if m and (m.group(1).lower() == want or m.group(1).lower().startswith(want + " (")):
            j = i + 1
            while j < len(lines) and not lines[j].startswith("## "):
                j += 1
            while j > i + 1 and lines[j - 1].strip() == "":
                j -= 1
            return i, j
        if low.startswith("**" + want + "**") or low.startswith("**" + want + " ("):
            j = i + 1
            while j < len(lines) and lines[j].strip() != "" and not lines[j].startswith("#"):
                j += 1
            return i, j
    return None


def assemble(root, check=True, keep=False, dry=False, say=print):
    """-> 0 ok / 1 refused. Writes CHANGELOG-TIDE.md and docs/ROADMAP.md under root and deletes the fragments"""
    files = sorted(f for f in glob.glob(os.path.join(root, "changes", "*.md")) if os.path.basename(f).lower() != "readme.md")
    if not files:
        say("assemble_changes: no fragments in changes/")
        return 0
    text = {k: open(os.path.join(root, p), encoding="utf-8", newline="").read() for k, p in TARGETS.items()}
    nl = {k: ("\r\n" if "\r\n" in v else "\n") for k, v in text.items()}
    lines = {k: v.splitlines() for k, v in text.items()}
    added, errors = {k: 0 for k in TARGETS}, []
    for f in files:
        try:
            frag = parse(f)
        except ValueError as e:
            errors.append(str(e))
            continue
        for part, items in frag.items():
            for name, new in items:
                if not new:
                    continue
                hit = find_target(lines[part], name)
                if hit is None:
                    errors.append(f"{os.path.basename(f)}: no '{name}' in {TARGETS[part]}")
                    continue
                i, j = hit
                if part == "changelog":
                    for k in range(i + 1, j):
                        if lines[part][k].startswith(PLACEHOLDER):
                            j = k
                            break
                lines[part][j:j] = new
                added[part] += len(new)
    if errors:
        for e in errors:
            say("assemble_changes: " + e)
        say("assemble_changes: nothing written")
        return 1
    say(f"assemble_changes: {len(files)} fragment(s): {added['changelog']} line(s) into {TARGETS['changelog']}, {added['roadmap']} into {TARGETS['roadmap']}")
    if dry:
        return 0
    for k, p in TARGETS.items():
        open(os.path.join(root, p), "w", encoding="utf-8", newline="").write(nl[k].join(lines[k]) + (nl[k] if text[k].endswith(("\n", "\r\n")) else ""))
    if check:
        r = subprocess.run([sys.executable, os.path.join(root, "jam", "check_docs.py")], cwd=root, capture_output=True, text=True)
        if r.returncode:
            for k, p in TARGETS.items():
                open(os.path.join(root, p), "w", encoding="utf-8", newline="").write(text[k])
            say("assemble_changes: check_docs failed on the result; the two files are back as they were, the fragments stay:")
            say((r.stdout + r.stderr).strip())
            return 1
    if not keep:
        for f in files:
            os.remove(f)
        say(f"assemble_changes: {len(files)} fragment(s) deleted")
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    root = a[a.index("--root") + 1] if "--root" in a else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sys.exit(assemble(root, check="--no-check" not in a, keep="--keep" in a, dry="--dry-run" in a))
