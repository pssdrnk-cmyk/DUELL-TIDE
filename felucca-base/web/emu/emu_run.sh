#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Builds web/emu/emu_run.c (the scripted emulator session runner) natively: build/emu/emu_run. Run after one ./build.sh (build/gen).
#   sh web/emu/emu_run.sh [--run SCRIPT.txt --out DIR]
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu_run: build/gen is missing: run tools/soundlab/build.sh once (or ./build.sh)"; exit 1; }
mkdir -p build/emu
cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/emu_run web/emu/emu_run.c -lm
if [ "${1:-}" = "--run" ]; then shift; build/emu/emu_run "$@"; fi
