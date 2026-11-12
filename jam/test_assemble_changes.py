#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Self-test of jam/assemble_changes.py on a made-up CHANGELOG / ROADMAP: two fragments for one heading both land, in file-name order, above the
placeholder; a bold roadmap label works; an unknown target writes nothing; the fragments are deleted; a failing doc check puts the files back."""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import assemble_changes as A  # noqa: E402

bad = 0
CHANGELOG = "# Changelog\n\n## 1.0.0-b10 (in progress)\n\n- old one\n- (new lines for the next beta go here; x)\n\n## 1.0.0-b9\n\n- b9 line\n"
ROADMAP = "# Roadmap\n\n## Later, in order\n\n1. first\n\n**Repository and CI**\n- row a\n- row b\n\n**Other**\n- other row\n\n## Released\n\n- done\n"


def expect(what, ok):
    global bad
    if not ok:
        bad += 1
        print("assemble_changes: FAIL", what)


def tree(frags, check_ok=True):
    root = tempfile.mkdtemp()
    os.makedirs(os.path.join(root, "docs"))
    os.makedirs(os.path.join(root, "changes"))
    os.makedirs(os.path.join(root, "jam"))
    open(os.path.join(root, "CHANGELOG-TIDE.md"), "w", newline="").write(CHANGELOG)
    open(os.path.join(root, "docs", "ROADMAP.md"), "w", newline="").write(ROADMAP)
    open(os.path.join(root, "jam", "check_docs.py"), "w").write("import sys\nsys.exit(%d)\n" % (0 if check_ok else 1))
    open(os.path.join(root, "changes", "README.md"), "w").write("# not a fragment\n")
    for name, body in frags.items():
        open(os.path.join(root, "changes", name + ".md"), "w").write(body)
    return root


def read(root, p):
    return open(os.path.join(root, p), newline="").read()


A_ = "## changelog\n### 1.0.0-b10\n- from a\n  continued\n\n## roadmap\n### Repository and CI\n- a row from a\n### Later, in order\n2. second from a\n"
B_ = "## changelog\n### 1.0.0-b10\n- from b\n### 1.0.0-b9\n- late b9 line\n"
root = tree({"b-branch": B_, "a-branch": A_})
quiet = []
rc = A.assemble(root, say=quiet.append)
cl, rm = read(root, "CHANGELOG-TIDE.md"), read(root, "docs/ROADMAP.md")
expect("it succeeds", rc == 0)
expect("two fragments for one heading both land, file-name order, above the placeholder",
       cl.index("- old one") < cl.index("- from a") < cl.index("  continued") < cl.index("- from b") < cl.index("- (new lines for the next beta"))
expect("a continuation line stays with its bullet", "- from a\n  continued\n- from b" in cl)
expect("another section gets its line at its end", cl.index("- b9 line") < cl.index("- late b9 line") and cl.endswith("- late b9 line\n"))
expect("a bold roadmap label gets the row at the end of its bullets, before the blank line", "- row b\n- a row from a\n\n**Other**" in rm)
expect("a ## roadmap section gets the line at its end (before the next ##)", "1. first\n2. second from a\n\n**Repository" not in rm and rm.index("2. second from a") < rm.index("## Released"))
expect("the fragments are deleted, README stays", sorted(os.listdir(os.path.join(root, "changes"))) == ["README.md"])

root = tree({"x": "## changelog\n### 9.9.9\n- nowhere\n", "y": B_})
rc = A.assemble(root, say=quiet.append)
expect("an unknown target refuses the whole run", rc == 1 and read(root, "CHANGELOG-TIDE.md") == CHANGELOG and os.path.exists(os.path.join(root, "changes", "x.md")) and os.path.exists(os.path.join(root, "changes", "y.md")))

root = tree({"x": "### no part\n- l\n"})
expect("a malformed fragment refuses", A.assemble(root, say=quiet.append) == 1)

root = tree({"a": A_}, check_ok=False)
rc = A.assemble(root, say=quiet.append)
expect("a failing doc check puts both files back and keeps the fragments", rc == 1 and read(root, "CHANGELOG-TIDE.md") == CHANGELOG and read(root, "docs/ROADMAP.md") == ROADMAP and os.path.exists(os.path.join(root, "changes", "a.md")))

root = tree({"a": A_})
rc = A.assemble(root, dry=True, say=quiet.append)
expect("--dry-run writes and deletes nothing", rc == 0 and read(root, "CHANGELOG-TIDE.md") == CHANGELOG and os.path.exists(os.path.join(root, "changes", "a.md")))
rc = A.assemble(root, keep=True, say=quiet.append)
expect("--keep writes but leaves the fragments", rc == 0 and "- from a" in read(root, "CHANGELOG-TIDE.md") and os.path.exists(os.path.join(root, "changes", "a.md")))

root = tree({})
expect("no fragments: nothing to do, exit 0", A.assemble(root, say=quiet.append) == 0)

print(f"assemble_changes: {'FAIL' if bad else 'ok'} ({bad} failed)")
sys.exit(1 if bad else 0)
