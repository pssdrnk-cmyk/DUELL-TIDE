#!/bin/sh
# TIDE's separability check: DUELL_TIDE=0 must build plain DUELL (1.1 + the always-on fixes) byte for byte.
#   sh jam/check_off.sh            build with DUELL_TIDE=0 and compare the package with jam/off.sha256
# off.sha256 holds the package hash of the last commit that changed DUELL's own behaviour (the fixes). A TIDE change
# must never change it; a new always-on fix updates it in the same commit, with the reason in the message.
# MENU > SYSTEM > ABOUT shows the build date (__DATE__): it is pinned to the day the first baseline was recorded,
# so the hash compares across days.
set -u
export DUELL_BUILD_DATE="Nov  1 2026"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO/jam/env.sh"
want="$(cut -c1-64 "$REPO/jam/off.sha256")"
cd "$REPO/felucca-base" || exit 1
if ! DUELL_TIDE=0 python3 tools/build.py > "$JAM_WORK/check-off.log" 2>&1; then
    tail -20 "$JAM_WORK/check-off.log"
    echo "FAIL    DUELL_TIDE=0 does not build (log: $JAM_WORK/check-off.log)"
    exit 1
fi
got="$(sha256sum build/duell-1.1.fwsc | cut -c1-64)"
if [ "$got" = "$want" ]; then
    echo "PASS    DUELL_TIDE=0 is byte-identical ($got)"
else
    echo "FAIL    DUELL_TIDE=0 differs: $got, expected $want (jam/off.sha256)"
    exit 1
fi
