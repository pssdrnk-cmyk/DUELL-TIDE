#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The emu_smoke group (host_tests.sh emu_smoke): web/emu/emu_run.c (the emulator's device, built natively) plays web/emu/scripts/emu_smoke.txt twice (boot, TIDE SETTINGS, the KNOBS row,
# SAVE > MULTIS on M03, GLO); both runs must give the same screens (CRC of each 240 x 240 frame), equal to tests/emu_smoke_ref.txt. Pages with no live waveform only.
#   tests/emu_smoke.sh [--write-ref]      from felucca-base/
set -e
mkdir -p build/emu
cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/emu_run web/emu/emu_run.c -lm || { echo "FAIL emu_smoke: emu_run does not build"; exit 1; }
rm -rf build/emu/smoke_a build/emu/smoke_b
build/emu/emu_run web/emu/scripts/emu_smoke.txt --out build/emu/smoke_a || { echo "FAIL emu_smoke: the script did not run"; exit 1; }
build/emu/emu_run web/emu/scripts/emu_smoke.txt --out build/emu/smoke_b || { echo "FAIL emu_smoke: the second run"; exit 1; }
cmp -s build/emu/smoke_a/screens.txt build/emu/smoke_b/screens.txt || { echo "FAIL emu_smoke: two runs of the same script give different screens"; exit 1; }
cmp -s build/emu/smoke_a/tide_settings.png build/emu/smoke_b/tide_settings.png || { echo "FAIL emu_smoke: the PNGs differ"; exit 1; }
if [ "${1:-}" = "--write-ref" ]; then cp build/emu/smoke_a/screens.txt tests/emu_smoke_ref.txt; echo "wrote tests/emu_smoke_ref.txt"; exit 0; fi
if ! cmp -s build/emu/smoke_a/screens.txt tests/emu_smoke_ref.txt; then
    diff build/emu/smoke_a/screens.txt tests/emu_smoke_ref.txt || true
    echo "FAIL emu_smoke: a screen differs from tests/emu_smoke_ref.txt (a UI change on purpose: tests/emu_smoke.sh --write-ref, and say so in the commit)"; exit 1
fi
echo "emu_smoke: deterministic, $(wc -l < tests/emu_smoke_ref.txt) screens equal to the stored hashes"
