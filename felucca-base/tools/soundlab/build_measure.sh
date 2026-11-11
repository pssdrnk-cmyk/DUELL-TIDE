#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Builds the stand-alone metrics tool (lane B): build/host/soundlab_measure.  From the worktree root:  . jam/env.sh && sh felucca-base/tools/soundlab/build_measure.sh
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$HERE/../../build/host}"
mkdir -p "$OUT"
cc -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter -I"$HERE" -o "$OUT/soundlab_measure" \
    "$HERE/sl_measure_main.c" "$HERE/sl_cmd.c" "$HERE/sl_metrics.c" "$HERE/sl_notes.c" "$HERE/sl_rules.c" "$HERE/sl_dsp.c" "$HERE/sl_json.c" -lm
