#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Worktrees and fork branches: what can go. See jam/cleanup.py and jam/README.md ("Cleanup").
#   sh jam/cleanup.sh [--apply] [--into "fork/jam11 fork/b9"] [--no-fetch] [--no-pr] [--from-refs refs/backup/DATE]
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh" 2>/dev/null || true
exec python3 "$here/cleanup.py" "$@"
