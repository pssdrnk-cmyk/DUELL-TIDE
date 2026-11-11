#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The soundlab group (host_tests.sh soundlab): builds tools/soundlab, renders M02 for 2 bars twice (WAV and JSON must be byte-identical),
# measures it (the stored numbers in tests/soundlab_ref.txt within 0.1), and runs the atlas builder twice on 3 sounds (identical output).
# usage: tests/soundlab_test.sh [--write-ref]      from felucca-base/
set -e
sh tools/soundlab/build.sh > build/soundlab_build.log 2>&1 || { tail -20 build/soundlab_build.log; echo "FAIL soundlab: does not build"; exit 1; }
S=build/soundlab/soundlab; [ -x $S.exe ] && S=$S.exe; S="$(pwd)/$S"
D=build/soundlab/test; rm -rf $D; mkdir -p $D
$S render --multi 2 --bars 2 --out $D/a > $D/a.log 2>&1 || { cat $D/a.log; echo "FAIL soundlab: render"; exit 1; }
$S render --multi 2 --bars 2 --out $D/b > $D/b.log 2>&1 || { cat $D/b.log; echo "FAIL soundlab: render 2"; exit 1; }
cmp -s $D/a.wav $D/b.wav || { echo "FAIL soundlab: two renders of M02 differ (WAV)"; exit 1; }
cmp -s $D/a.json $D/b.json || { echo "FAIL soundlab: two renders of M02 differ (JSON)"; exit 1; }
$S measure $D/a > $D/m.log 2>&1 || { cat $D/m.log; echo "FAIL soundlab: measure"; exit 1; }
if [ "${1:-}" = "--write-ref" ]; then python3 tests/soundlab_ref.py $D/a.metrics.json tests/soundlab_ref.txt --write; exit 0; fi
python3 tests/soundlab_ref.py $D/a.metrics.json tests/soundlab_ref.txt || exit 1
python3 tools/soundlab/tests/selftest.py $S $D/self > $D/self.log 2>&1 || { grep FAIL $D/self.log | head; echo "FAIL soundlab: selftest"; exit 1; }
echo "soundlab: deterministic render, reference numbers within 0.1, selftest ok"
