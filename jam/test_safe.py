#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests of jam/safe.sh (the guard in front of every rm -rf / rsync --delete of the jam scripts) and a lint of the repository's own WSL calls.

The incident of 2026-11-12: `wsl.exe bash -c 'D=/x; rm -rf "$D"/*'` runs through WSL's own shell (without -e), so $D was empty and the command was `rm -rf /*`; it emptied a
home directory. safe.sh refuses an empty, `/`, $HOME or `..` path, and a path that is not strictly inside its root. The lint fails on a repository script that hands shell
syntax to `wsl.exe` in a quoted string without `-e` (use a script file: `wsl.exe -d Ubuntu -- sh /mnt/c/.../script.sh`).
"""
import os, re, subprocess, sys, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SAFE = ROOT / "jam" / "safe.sh"
SH = "sh"


def sh(code, home="/home/someone"):
    env = dict(os.environ, HOME=home)
    p = subprocess.run([SH, "-c", f'. "{SAFE.as_posix()}"; {code}'], capture_output=True, text=True, env=env)
    return p.returncode, p.stderr


class SafeUnder(unittest.TestCase):
    def ok(self, root, path, home="/home/someone"):
        return sh(f'safe_under "{root}" "{path}"', home)[0] == 0

    def test_inside_is_allowed(self):
        self.assertTrue(self.ok("/home/someone/duell-build", "/home/someone/duell-build/b10-x"))
        self.assertTrue(self.ok("/home/someone/duell-build/", "/home/someone/duell-build/b10-x/deep/er"))
        self.assertTrue(self.ok("/w/felucca-base/build", "/w/felucca-base/build/check/abc1234"))

    def test_empty_variable_cases_are_refused(self):
        for root, path in (("", "/x/y"), ("/x", ""), ("", ""), ("/home/someone/duell-build", ""), ("/home/someone/duell-build", "/home/someone/duell-build/"),
                           ("/home/someone/duell-build", "/home/someone/duell-build"), ("/home/someone/duell-build", "/")):
            self.assertFalse(self.ok(root, path), f"{root!r} {path!r}")

    def test_home_and_root_are_never_removed_or_a_root(self):
        self.assertFalse(self.ok("/home/someone/duell-build", "/home/someone"))
        self.assertFalse(self.ok("/home/someone/duell-build", "/home/someone/"))
        self.assertFalse(self.ok("/home/someone", "/home/someone/x"))      # a root may not be $HOME
        self.assertFalse(self.ok("/", "/x"))
        self.assertFalse(self.ok("/", "/home"))
        self.assertFalse(self.ok("/home/someone/duell-build", "/home/someone/duell-build/../.."))
        self.assertFalse(self.ok("/home/someone/duell-build", "/home/someone/duell-build/a/../../.."))
        self.assertFalse(self.ok("/home/someone/duell-build/..", "/home/someone/x"))

    def test_outside_and_prefix_lookalikes_are_refused(self):
        self.assertFalse(self.ok("/home/someone/duell-build", "/home/someone/duell-build2/x"))
        self.assertFalse(self.ok("/home/someone/duell-build", "/home/someone/other/x"))
        self.assertFalse(self.ok("/home/someone/duell-build", "/etc/passwd"))

    def test_unset_home_is_safe(self):
        env = {k: v for k, v in os.environ.items() if k != "HOME"}
        p = subprocess.run([SH, "-c", f'. "{SAFE.as_posix()}"; safe_under "/a/b" "/a/b/c"'], capture_output=True, text=True, env=env)
        self.assertEqual(p.returncode, 0)

    def test_safe_rm_removes_only_inside(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t).resolve()
            root = t / "build"
            (root / "x" / "y").mkdir(parents=True)
            (t / "keep").mkdir()
            self.assertEqual(sh(f'safe_rm "{root.as_posix()}" "{(root / "x").as_posix()}"')[0], 0)
            self.assertFalse((root / "x").exists())
            rc, err = sh(f'safe_rm "{root.as_posix()}" "{(t / "keep").as_posix()}"')
            self.assertNotEqual(rc, 0)
            self.assertIn("refusing", err)
            self.assertTrue((t / "keep").exists())
            rc, _ = sh(f'D=; safe_rm "{root.as_posix()}" "$D"/*')              # the incident's shape: an empty variable and a glob
            self.assertNotEqual(rc, 0)
            self.assertTrue(root.exists())

    def test_wslmirror_refuses_a_destination_outside_duell_build(self):
        # the guard line is in the script and uses safe_under with the duell-build root
        s = (ROOT / "jam" / "wslmirror.sh").read_text(encoding="utf-8")
        self.assertIn('safe_under "$HOME/duell-build" "$DST"', s)
        self.assertLess(s.index('safe_under "$HOME/duell-build" "$DST"'), s.index("rsync -a --delete"))


class NoShellSyntaxThroughWsl(unittest.TestCase):
    """a quoted command string handed to wsl.exe without -e is re-parsed by WSL's shell: variables set inside it are empty where they are used"""
    BAD = re.compile(r"""wsl(?:\.exe)?\s+(?!.*\s-e\s)(?:-d\s+\S+\s+)?(?:--\s+)?(?:bash|sh)\s+-l?c\s+['"]""")

    def test_repository_scripts(self):
        bad = []
        for p in list((ROOT / "jam").glob("*")) + [ROOT / "host_tests.sh"]:
            if p.suffix not in (".sh", ".py", ".md", "") or not p.is_file() or p.name == "test_safe.py":
                continue
            for i, ln in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if self.BAD.search(ln) and not ln.lstrip().startswith("#"):
                    bad.append(f"{p.name}:{i}: {ln.strip()[:120]}")
        self.assertEqual(bad, [], "wsl.exe given a quoted shell string without -e (run a script file instead)")

    def test_lint_catches_the_incident_shape(self):
        self.assertTrue(self.BAD.search("""wsl.exe bash -c 'D=/x; rm -rf "$D"/*'"""))
        self.assertTrue(self.BAD.search("""wsl -d Ubuntu -- sh -c 'cd ~/x && ls'"""))
        self.assertFalse(self.BAD.search("""wsl.exe -d Ubuntu -e bash -c 'D=/x; echo "$D"'"""))
        self.assertFalse(self.BAD.search("""wsl.exe -d Ubuntu -- sh jam/wsl.sh multi"""))


if __name__ == "__main__":
    unittest.main(verbosity=1)
