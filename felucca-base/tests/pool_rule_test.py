#!/usr/bin/env python3
"""The pool-tail rule: 8 KiB in every build, 4,096 B only in a DUELL_BENCH developer build, 7,936 B in a DUELL_TEST_MODE measurement build (tools/build.py pool_tail_min); the RAM headroom rule: 4,096 B, 3,840 B in a DUELL_TEST_MODE measurement build (ram_headroom_min)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import build

ok = True
def eq(name, got, want):
    global ok
    good = got == want
    ok &= good
    print(("ok   " if good else "FAIL ") + f"{name}: {got} (want {want})")

eq("normal build", build.pool_tail_min({}), 8192)
eq("RAMPLACE only", build.pool_tail_min({"DUELL_RAMPLACE": "1"}), 8192)
eq("TWOCORE without bench", build.pool_tail_min({"DUELL_TWOCORE": "1"}), 8192)
eq("DUELL_BENCH=1 without a tag", build.pool_tail_min({"DUELL_BENCH": "1"}), 8192)
eq("DUELL_BENCH=0 with a tag", build.pool_tail_min({"DUELL_BENCH": "0", "BENCH_TAG": "X"}), 8192)
eq("bench developer build", build.pool_tail_min({"DUELL_BENCH": "1", "BENCH_TAG": "BENCHT1"}), build.POOL_TAIL_MIN_BENCH)
eq("bench constant", build.POOL_TAIL_MIN_BENCH, 4096)
eq("constant", build.POOL_TAIL_MIN, 8192)
eq("test-mode measurement build", build.pool_tail_min({"DUELL_TEST_MODE": "1"}), build.POOL_TAIL_MIN_TEST)
eq("test constant", build.POOL_TAIL_MIN_TEST, 7936)
eq("DUELL_TEST_MODE=0", build.pool_tail_min({"DUELL_TEST_MODE": "0"}), 8192)
eq("bench wins over test mode", build.pool_tail_min({"DUELL_TEST_MODE": "1", "DUELL_BENCH": "1", "BENCH_TAG": "B"}), build.POOL_TAIL_MIN_BENCH)
eq("RAM headroom: normal build", build.ram_headroom_min({}), 4096)
eq("RAM headroom: test-mode measurement build", build.ram_headroom_min({"DUELL_TEST_MODE": "1"}), build.RAM_HEADROOM_MIN_TEST)
eq("RAM headroom: test constant (256 B under the rule)", build.RAM_HEADROOM_MIN_TEST, build.RAM_HEADROOM_MIN - 256)
eq("RAM headroom: DUELL_TEST_MODE=0", build.ram_headroom_min({"DUELL_TEST_MODE": "0"}), 4096)
sys.exit(0 if ok else 1)
