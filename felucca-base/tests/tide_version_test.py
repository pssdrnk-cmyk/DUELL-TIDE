#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/build.py tide_id (./host_tests.sh tideversion): the id ABOUT shows next to the version. It is TIDE_VERSION (1.0.0-b1), with a
trailing "+" unless the tree is exactly that release: tracked files clean and HEAD at the tag tide-<version>. Made in a throw-away repository."""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build  # noqa: E402

bad = 0


def check(name, got, want):
    global bad
    if got != want:
        bad += 1
        print(f"FAIL {name}: got {got!r}, wanted {want!r}")


def git(cwd, *a):
    env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@example.invalid", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@example.invalid")
    subprocess.run(["git", *a], cwd=cwd, env=env, check=True, capture_output=True)


def exits(f):
    try:
        f()
    except SystemExit:
        return True
    return False


os.environ.pop("DUELL_TIDE_ID", None)
with tempfile.TemporaryDirectory() as t:
    root = Path(t)
    (root / "felucca-base").mkdir()
    (root / "felucca-base" / "a.txt").write_text("a\n")
    (root / "TIDE_VERSION").write_text("1.0.0-b1\n")
    git(root, "init", "-q")
    git(root, "add", "-A")
    git(root, "commit", "-q", "-m", "one")
    build.SRC = (root / "felucca-base").resolve()
    check("version read", build.tide_version(), "1.0.0-b1")
    check("no tag yet", build.tide_id(), "1.0.0-b1+")
    git(root, "tag", "tide-1.0.0-b1")
    check("clean, at the tag", build.tide_id(), "1.0.0-b1")
    (root / "felucca-base" / "untracked.txt").write_text("x\n")
    check("an untracked file does not count", build.tide_id(), "1.0.0-b1")
    (root / "felucca-base" / "a.txt").write_text("changed\n")
    check("tracked file changed", build.tide_id(), "1.0.0-b1+")
    git(root, "commit", "-q", "-a", "-m", "two")
    check("HEAD past the tag", build.tide_id(), "1.0.0-b1+")
    (root / "TIDE_VERSION").write_text("1.0.0\r\n")
    check("CRLF file, release version, no tag", build.tide_id(), "1.0.0+")
    git(root, "tag", "tide-1.0.0")
    git(root, "tag", "-f", "tide-1.0.0")
    check("the tag moved to HEAD, file changed and uncommitted", build.tide_id(), "1.0.0+")
    git(root, "commit", "-q", "-a", "-m", "three")
    git(root, "tag", "-f", "tide-1.0.0")
    check("release", build.tide_id(), "1.0.0")
    # a snapshot of the tree inside another repository: that repository's tag and commit say nothing
    sub = Path(t) / "outer" / "snap"
    (sub / "felucca-base").mkdir(parents=True)
    (sub / "TIDE_VERSION").write_text("1.0.0\n")
    git(root / "outer", "init", "-q")
    git(root / "outer", "add", "-A")
    git(root / "outer", "commit", "-q", "-m", "snap")
    git(root / "outer", "tag", "tide-1.0.0")
    build.SRC = (sub / "felucca-base").resolve()
    check("inside another repository", build.tide_id(), "1.0.0+")
    # no git repository at all
    with tempfile.TemporaryDirectory() as t2:
        (Path(t2) / "felucca-base").mkdir()
        (Path(t2) / "TIDE_VERSION").write_text("2.1.3-b12\n")
        build.SRC = (Path(t2) / "felucca-base").resolve()
        check("no git", build.tide_id(), "2.1.3-b12+")
        os.environ["DUELL_TIDE_ID"] = "x.y"
        check("DUELL_TIDE_ID overrides", build.tide_id(), "x.y")
        os.environ["DUELL_TIDE_ID"] = "01234567890"
        check("DUELL_TIDE_ID over 10 chars stops the build", exits(build.tide_id), True)
        del os.environ["DUELL_TIDE_ID"]
        for v in ("1.0", "v1.0.0", "1.0.0-beta1", "1.0.0-b", "", "100.0.0"):
            (Path(t2) / "TIDE_VERSION").write_text(v + "\n")
            check(f"bad TIDE_VERSION {v!r}", exits(build.tide_version), True)
        (Path(t2) / "TIDE_VERSION").write_text("10.10.10-b10\n")
        check("a version too long for ABOUT with its +", exits(build.tide_id), True)
        (Path(t2) / "TIDE_VERSION").unlink()
        check("no TIDE_VERSION file", exits(build.tide_version), True)

# the repository's own file is valid
build.SRC = Path(__file__).resolve().parents[1]
check("the repository's TIDE_VERSION", bool(build.tide_version()), True)
print("tideversion: %s" % ("FAIL" if bad else "PASS"), "(%d failed)" % bad if bad else "")
sys.exit(1 if bad else 0)
