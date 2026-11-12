#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests of jam/check_groups.py: the real host_tests.sh passes, and each way of losing a group from the default list fails by name."""
import re, subprocess, sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_groups as cg  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
with open(ROOT / "host_tests.sh", encoding="utf-8", newline="") as _f:
    REAL = _f.read()


def _w(p, s):
    with open(p, "w", encoding="utf-8", newline="") as fh:
        fh.write(s)


def drop_from_want(text, name):
    return re.sub(r'^(want="\$\{\*:-.*?) %s( .*)$' % re.escape(name), r"\1\2", text, count=1, flags=re.M)


class CheckGroups(unittest.TestCase):
    def test_real_file_is_fine(self):
        self.assertEqual(cg.problems(REAL), [])

    def test_dropping_take_fails_and_names_it(self):
        t = drop_from_want(REAL, "take")
        self.assertNotEqual(t, REAL)
        p = cg.problems(t)
        self.assertEqual(len(p), 1)
        self.assertIn("group take ", p[0])

    def test_script_exit_code_and_message(self):
        with tempfile.TemporaryDirectory() as d:
            f = Path(d) / "host_tests.sh"
            _w(f, drop_from_want(REAL, "take"))
            r = subprocess.run([sys.executable, str(ROOT / "jam" / "check_groups.py"), str(f)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn("FAIL group take", r.stdout)
            _w(f, REAL)
            self.assertEqual(subprocess.run([sys.executable, str(ROOT / "jam" / "check_groups.py"), str(f)], capture_output=True, text=True).returncode, 0)

    def test_new_case_nobody_runs(self):
        t = REAL.replace("\n  sat)     ", "\n  newgroup) echo hi ;;\n  sat)     ", 1)
        self.assertNotEqual(t, REAL)
        p = cg.problems(t)
        self.assertEqual(len(p), 1)
        self.assertIn("group newgroup ", p[0])

    def test_marked_but_in_the_list(self):
        t = REAL.replace("  # not default: soundlab - builds tools/soundlab", "  # not default: soundlab - builds tools/soundlab\n  # not default: take - by mistake", 1)
        p = cg.problems(t)
        self.assertEqual(len(p), 1)
        self.assertIn("group take is marked", p[0])

    def test_marker_needs_a_reason_and_a_case(self):
        p = cg.problems(REAL.replace("# not default: soundlab - builds tools/soundlab", "# not default: soundlab -", 1))
        self.assertTrue(any("group soundlab " in x for x in p))
        p = cg.problems(REAL.replace("# not default: soundlab - builds", "# not default: soundlabb - builds", 1))
        self.assertTrue(any("names no case" in x for x in p) and any("group soundlab " in x for x in p))

    def test_typo_in_the_list(self):
        t = REAL.replace(" tidestore take ", " tidestore tkae ", 1)
        self.assertTrue(any("tkae" in x for x in cg.problems(t)))
        self.assertTrue(any("group take " in x for x in cg.problems(t)))


if __name__ == "__main__":
    unittest.main()
