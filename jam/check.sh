#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM: the whole check in one command (Git Bash on Windows; also Linux / macOS).
#   sh jam/check.sh [test ...]        (default: the device build, every host test group, the patcher's tests)
# Each host test group runs on its own with a time limit, so a slow or stuck one cannot stall the rest. On Windows a group
# that needs fork() (jam/wincompat/sys/wait.h) or another POSIX-only part stops there with exit code 77 and "skipped" in its output; host_tests.sh turns that into a "   SKIP" line and this script reports SKIP, never PASS.
# Any other non-zero exit is a FAIL even without a FAIL line. Each line ends with the group's seconds. The summary and every group's output land in the log folder (outside the repository).
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO/jam/env.sh"
LOG="${JAM_CHECK_LOG:-$JAM_WORK/check-$(basename "$REPO")}"   # (one log folder per checkout: work/check-DUELL, work/check-duell-jam11)
LIMIT="${JAM_TEST_LIMIT:-900}"
mkdir -p "$LOG"
cd "$REPO" || exit 1

groups="${*:-$(sed -n 's/^want="\${\*:-\([^"]*\)}"$/\1/p' host_tests.sh)}"
pass=0 fail=0 skip=0 failed=""

echo "== device build"
if (cd felucca-base && python3 tools/build.py > "$LOG/build.log" 2>&1); then
    grep -E "^  ok    (image|\.ram_text)" "$LOG/build.log"
    echo "   PASS"
else
    tail -20 "$LOG/build.log"
    echo "   FAIL (device build)"
    exit 1
fi

for g in $groups; do
    if [ "$g" = '$EMU' ]; then
        if command -v emcc >/dev/null 2>&1 || [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
            g=emu
        else
            echo "SKIP    emu (SDK unavailable)"; skip=$((skip + 1)); continue
        fi
    fi
    out="$LOG/$g.log"
    t0=$(date +%s)
    timeout "$LIMIT" sh host_tests.sh "$g" > "$out" 2>&1
    rc=$?
    secs=$(( $(date +%s) - t0 ))
    # The exit code decides first: host_tests.sh exits non-zero for any failed group (a crash included, even with no FAIL line, or right after a "0 FAIL" summary),
    # and reports a Windows "needs fork()" stop (exit 77 from the shim) as a "   SKIP" line with exit 0. So: timeout, non-zero = FAIL, then a FAIL line that
    # slipped past a zero exit (not the runner's "   FAIL", not a "   PASS (detected ... first: x: FAIL ...)" mutation line, not a "0 FAIL" summary), then SKIP, else PASS.
    real=$(grep -E "FAIL|Assertion failed" "$out" | grep -vE "^   FAIL|^   PASS|(^|[^0-9])0 FAIL" | head -1)
    if [ $rc -eq 124 ]; then
        echo "TIMEOUT $g (over ${LIMIT} s)"; fail=$((fail + 1)); failed="$failed $g"
    elif [ $rc -ne 0 ] || [ -n "$real" ] || grep -q "^   FAIL" "$out"; then
        echo "FAIL    $g (${secs} s)"; fail=$((fail + 1)); failed="$failed $g"
    elif grep -q "^   SKIP" "$out"; then
        # a group whose checks could not run here (no Chrome, no update package, no banks, no fork() ...) says "   SKIP <why>": never PASS
        echo "SKIP    $g ($(grep -m1 "^   SKIP" "$out" | sed 's/^   SKIP //')) (${secs} s)"; skip=$((skip + 1))
    else
        echo "PASS    $g (${secs} s)"; pass=$((pass + 1))
    fi
done

if [ -z "${*:-}" ]; then
    if python3 jam/test_jam_dev.py > "$LOG/jam_dev.log" 2>&1; then
        echo "PASS    jam_dev"; pass=$((pass + 1))
    else
        echo "FAIL    jam_dev"; fail=$((fail + 1)); failed="$failed jam_dev"
    fi
fi

echo "== $pass passed, $fail failed, $skip skipped (logs: $LOG)"
[ -n "$failed" ] && echo "   failed:$failed"
[ "$fail" -eq 0 ]
