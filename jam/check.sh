#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM: the whole check in one command (Git Bash on Windows; also Linux / macOS).
#   sh jam/check.sh [test ...]        (default: the device build, every host test group, the patcher's tests)
# Each host test group runs on its own with a time limit, so a slow or stuck one cannot stall the rest. On Windows a group
# that needs fork() (jam/wincompat/sys/wait.h) or another POSIX-only part stops there with exit code 77 and "skipped" in its output; host_tests.sh turns that into a "   SKIP" line and this script reports SKIP, never PASS.
# Any other non-zero exit is a FAIL even without a FAIL line. Each line ends with the group's seconds. The summary and every group's output land in the log folder (felucca-base/build/check/<sha>, git-ignored).
#   Fast mode (jam/fastcheck.py, see jam/README.md "Fast tests"): -j N (groups at a time), --all (the default list; implied by -j), --changed [BASE] (only the groups
#   whose sources changed since BASE, default fork/b9), --shard I/N, --host-only (no device build, no jam tests), --no-cache, --out FILE, --summary FILE, --with-slow (--changed otherwise leaves out the groups whose last recorded run took over 60 s and lists them as NOT RUN at the end).
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO/jam/env.sh"
# one log folder per commit inside the checkout's build/ (felucca-base/build/check/<short sha>); the three newest are kept, the rest removed (sh jam/prune.sh clears more)
LOG="${JAM_CHECK_LOG:-$REPO/felucca-base/build/check/$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo nogit)}"
LIMIT="${JAM_TEST_LIMIT:-900}"
mkdir -p "$LOG"
cd "$REPO" || exit 1
if [ -z "${JAM_CHECK_LOG:-}" ]; then
    . "$REPO/jam/safe.sh"
    ls -dt "$REPO"/felucca-base/build/check/*/ 2>/dev/null | tail -n +4 | while read -r d; do safe_rm "$REPO/felucca-base/build/check" "${d%/}"; done
fi

JOBS=0 CHANGED="" SHARD="" HOSTONLY=0 NOCACHE="" OUTF="" SUMF="" ALL=0 FAST=0 WITHSLOW=""
while [ $# -gt 0 ]; do
    case "$1" in
        -j) JOBS="$2"; FAST=1; shift 2 ;;
        -j[0-9]*) JOBS="${1#-j}"; FAST=1; shift ;;
        --all) ALL=1; FAST=1; shift ;;
        --changed=*) CHANGED="${1#--changed=}"; FAST=1; shift ;;
        --changed) FAST=1; shift
            CHANGED="fork/b9"
            # (the next word is the BASE if it names a commit and is not a group of host_tests.sh)
            if [ $# -gt 0 ] && case "$1" in -*) false ;; esac && git rev-parse --verify -q "$1^{commit}" >/dev/null 2>&1 \
                && ! sed -n 's/^want="\${\*:-\([^"]*\)}"$/\1/p' host_tests.sh | tr ' ' '\n' | grep -qxF -- "$1"; then CHANGED="$1"; shift; fi ;;
        --shard) SHARD="$2"; FAST=1; shift 2 ;;
        --host-only) HOSTONLY=1; shift ;;
        --no-cache) NOCACHE=1; shift ;;
        --with-slow) WITHSLOW=1; FAST=1; shift ;;
        --out) OUTF="$2"; shift 2 ;;
        --summary) SUMF="$2"; shift 2 ;;
        *) break ;;
    esac
done
[ "$FAST" = 1 ] && [ "$JOBS" = 0 ] && JOBS=""
groups="${*:-$(sed -n 's/^want="\${\*:-\([^"]*\)}"$/\1/p' host_tests.sh)}"
pass=0 fail=0 skip=0 failed=""

device_build() {
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
    return 1
fi

    return 0
}
if [ "$HOSTONLY" = 1 ]; then    # (no device build: only the generated headers the host groups include, as jam/wsl.sh does)
    (cd felucca-base && python3 tools/build.py --gen-only > "$LOG/gen.log" 2>&1) || { tail -20 "$LOG/gen.log"; echo "   FAIL (build.py --gen-only)"; exit 1; }
fi
if [ "$FAST" = 0 ]; then
    [ "$HOSTONLY" = 1 ] || device_build || exit 1
else
    : > "$LOG/device.out"
    wantdev=1
    if [ -n "$CHANGED" ]; then   # --changed: the device build only when firmware or tools changed
        wantdev=0; { git diff --name-only "$CHANGED"; git ls-files -o --exclude-standard; } 2>/dev/null | grep -q -E '^felucca-base/(firmware|tools)/' && wantdev=1
    fi
    if [ "$HOSTONLY" = 0 ] && [ "$wantdev" = 1 ]; then
        # the device build is independent of the host groups: it runs beside them
        ( device_build > "$LOG/device.out" 2>&1; echo $? > "$LOG/device.rc" ) &
        devpid=$!
    fi
fi

if [ "$FAST" = 1 ]; then
    # parallel groups with a compile cache; the verdicts are decided as below (jam/fastcheck.py)
    fa="--log $LOG"
    [ -n "$JOBS" ] && fa="$fa -j $JOBS"
    [ -n "$CHANGED" ] && fa="$fa --changed $CHANGED"
    [ -n "$SHARD" ] && fa="$fa --shard $SHARD"
    [ -n "$NOCACHE" ] && fa="$fa --no-cache"
    [ -n "$WITHSLOW" ] && fa="$fa --with-slow"
    [ -n "$OUTF" ] && fa="$fa --out $OUTF"
    [ -n "$SUMF" ] && fa="$fa --summary $SUMF"
    HT_REALCC="$(command -v cc)"; export HT_REALCC
    # shellcheck disable=SC2086
    if [ -n "${*:-}" ]; then python3 jam/fastcheck.py $fa "$@" > "$LOG/fast.out" 2>&1; else python3 jam/fastcheck.py $fa > "$LOG/fast.out" 2>&1; fi
    cat "$LOG/fast.out"
    pass=$(grep -c '^PASS ' "$LOG/fast.out"); skip=$(grep -c '^SKIP ' "$LOG/fast.out")
    fail=$(grep -c -E '^(FAIL|TIMEOUT) ' "$LOG/fast.out")
    failed=$(sed -n -E 's/^(FAIL|TIMEOUT) +([^ ]+).*/\2/p' "$LOG/fast.out" | tr '\n' ' '); [ -n "$failed" ] && failed=" $failed"
    if [ -n "${devpid:-}" ]; then
        wait "$devpid"; echo "== device build (ran beside the groups)"; cat "$LOG/device.out"
        if [ "$(cat "$LOG/device.rc" 2>/dev/null || echo 1)" != 0 ]; then fail=$((fail + 1)); failed="$failed device_build"; fi
    fi
else
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
fi

full=0; [ -z "${*:-}" ] && [ -z "$SHARD" ] && [ -z "$CHANGED" ] && [ "$HOSTONLY" = 0 ] && full=1
jamtools=$full
[ -n "$CHANGED" ] && [ "$HOSTONLY" = 0 ] && git diff --name-only "$CHANGED" -- jam | grep -qv ccwrap && jamtools=1
if [ "$jamtools" = 1 ]; then
    for t in jam_dev music_revision8; do          # (jam/test_jam_dev.py, jam/test_music_revision8.py: the flash / backup / restore tool and its music checks; CI runs both too)
        if python3 "jam/test_$t.py" > "$LOG/$t.log" 2>&1; then
            echo "PASS    $t"; pass=$((pass + 1))
        else
            echo "FAIL    $t"; fail=$((fail + 1)); failed="$failed $t"
        fi
    done
fi

if [ "$full" = 1 ] || { [ -n "$CHANGED" ] && [ "$HOSTONLY" = 0 ]; }; then
    if python3 jam/check_docs.py > "$LOG/check_docs.log" 2>&1; then          # (the STYLE tables against tide_ui.c, no workspace paths in shipped documents; CI jam-tools runs it too)
        echo "PASS    check_docs"; pass=$((pass + 1))
    else
        head -5 "$LOG/check_docs.log"
        echo "FAIL    check_docs"; fail=$((fail + 1)); failed="$failed check_docs"
    fi
fi

echo "== $pass passed, $fail failed, $skip skipped (logs: $LOG)"
[ -n "$failed" ] && echo "   failed:$failed"
[ "$FAST" = 1 ] && grep '^== NOT RUN' "$LOG/fast.out"
[ "$fail" -eq 0 ]
