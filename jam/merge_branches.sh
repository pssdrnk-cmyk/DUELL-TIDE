#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Merge branches in order; generated files that conflict are regenerated, hand-written conflicts stop with a list. See jam/merge_branches.py.
#   sh jam/merge_branches.sh [--no-check] [--hand-from REF] BASE branch [branch ...]      |      sh jam/merge_branches.sh --continue BASE
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh" 2>/dev/null || true
exec python3 "$here/merge_branches.py" "$@"
