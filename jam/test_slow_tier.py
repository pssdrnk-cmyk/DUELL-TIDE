#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests of the slow tier of `check.sh --changed` (jam/fastcheck.py split_slow): groups whose last recorded run took over SLOW_S seconds are left out of a
--changed run (and listed as NOT RUN); a group never timed is not slow; the limit is the recorded time, not the group's name."""
import sys, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fastcheck as F  # noqa: E402


class SlowTier(unittest.TestCase):
    def test_split(self):
        t = {"tcmeas": 410, "twocore": 213, "regress": 10, "edge": 60, "just_over": 61}
        fast, slow = F.split_slow(["regress", "tcmeas", "edge", "just_over", "never_timed", "twocore"], t)
        self.assertEqual(fast, ["regress", "edge", "never_timed"])
        self.assertEqual(slow, ["tcmeas", "just_over", "twocore"])

    def test_no_times_means_nothing_is_slow(self):
        fast, slow = F.split_slow(["a", "b"], {})
        self.assertEqual((fast, slow), (["a", "b"], []))

    def test_limit_is_a_parameter_and_default_is_60(self):
        self.assertEqual(F.SLOW_S, 60)
        self.assertEqual(F.split_slow(["a"], {"a": 30}, 20), ([], ["a"]))


if __name__ == "__main__":
    unittest.main()
