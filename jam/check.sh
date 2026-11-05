#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM: the whole check in one command (Git Bash on Windows; also Linux / macOS).
#   sh jam/check.sh [test ...]        (default: the device build, every host test group, the patcher's tests)
# Each host test group runs on its own with a time limit, so a slow or stuck one cannot stall the rest. On Windows a group
# that needs fork() (jam/wincompat/sys/wait.h) or another POSIX-only part stops there with exit code 77 and "skipped" in its output; host_tests.sh turns that into a "   SKIP" line and this script reports SKIP, never PASS.
# Any other non-zero exit is a FAIL even without a FAIL line. Each line ends with the group's seconds. The summary and every group's output land in the log folder (felucca-base/build/check/<sha>, git-ignored).
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO/jam/env.sh"
# one log folder per commit inside the checkout's build/ (felucca-base/build/check/<short sha>); the three newest are kept, the rest removed (sh jam/prune.sh clears more)
LOG="${JAM_CHECK_LOG:-$REPO/felucca-base/build/check/$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo nogit)}"
LIMIT="${JAM_TEST_LIMIT:-900}"
mkdir -p "$LOG"
cd "$REPO" || exit 1
if [ -z "${JAM_CHECK_LOG:-}" ]; then
    ls -dt "$REPO"/felucca-base/build/check/*/ 2>/dev/null | tail -n +4 | while read -r d; do rm -rf "$d"; done
fi

groups="${*:-$(sed -n 's/^want="\${\*:-\([^"]*\)}"$/\1/p' host_tests.sh)}"
pass=0 fail=0 skip=0 failed=""

echo "== device build"
if (cd felucca-base && python3 tools/build.py > "$LOG/build.log" 2>&1); then
    grep -E "^  ok    (image|\.ram_text)" "$LOG/build.log"
    echo "   PASS"
    # what the build's own checks measured, one row per commit in jam/size_log.csv (build.py fails a build only at the limits; this shows the margin shrinking
    # before then): free flash in the app slot, .bss headroom, pool; a warning when flash is under 2 KB or the .bss headroom under 4.5 KB (the build fails at 4 KB)
    nl=$(grep -m1 -E "^  ok    image [0-9]+ B; RAM" "$LOG/build.log")
    if [ -n "$nl" ]; then
        img=$(echo "$nl" | sed 's/.*image \([0-9]*\) B.*/\1/'); bss=$(echo "$nl" | sed 's/.*\.bss \([0-9]*\) B of.*/\1/')
        ramlen=$(echo "$nl" | sed 's/.* B of \([0-9]*\); pool.*/\1/'); pool=$(echo "$nl" | sed 's/.*pool \([0-9]*\) B of.*/\1/')
        slot=$(( $(sed -n 's/^APP_SLOT = \(0x[0-9A-Fa-f]*\).*/\1/p' felucca-base/tools/fm1pkg_make.py) ))
        sha=$(git rev-parse --short HEAD 2>/dev/null || echo nogit); git diff --quiet HEAD 2>/dev/null || sha="$sha+"
        [ -f jam/size_log.csv ] || echo "date,commit,image_B,free_flash_B,ram_B,ram_headroom_B,pool_B" > jam/size_log.csv
        grep -q "^[^,]*,$sha," jam/size_log.csv || echo "$(date +%Y-%m-%d),$sha,$img,$((slot - img)),$bss,$((ramlen - bss)),$pool" >> jam/size_log.csv
        [ $((slot - img)) -lt 2048 ] && echo "   WARN  free flash $((slot - img)) B (under 2048)"
        [ $((ramlen - bss)) -lt 4608 ] && echo "   WARN  .bss headroom $((ramlen - bss)) B (under 4608; the build fails under 4096)"
    fi
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
    for t in jam_dev music_revision8; do          # (jam/test_jam_dev.py, jam/test_music_revision8.py: the flash / backup / restore tool and its music checks; CI runs both too)
        if python3 "jam/test_$t.py" > "$LOG/$t.log" 2>&1; then
            echo "PASS    $t"; pass=$((pass + 1))
        else
            echo "FAIL    $t"; fail=$((fail + 1)); failed="$failed $t"
        fi
    done
fi

echo "== $pass passed, $fail failed, $skip skipped (logs: $LOG)"
[ -n "$failed" ] && echo "   failed:$failed"
[ "$fail" -eq 0 ]
