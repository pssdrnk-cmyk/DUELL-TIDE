#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM: list (and with --yes remove) the leftovers of earlier test runs, which grow without limit:
#   sh jam/prune.sh          show what would go
#   sh jam/prune.sh --yes    remove it
# - the work folder's old per-checkout log folders (check*, check-off.log) untouched for over 7 days: jam/check.sh now keeps its logs in
#   felucca-base/build/check/<short sha> of the checkout (the three newest)
# - this checkout's felucca-base/build/*_mut copies of the firmware and tests (the self-checks remove them after a clean run)
# - this checkout's felucca-base/build/check/ folders beyond the three newest
# Nothing outside build/ and the work folder's check* logs is touched.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO/jam/env.sh"
. "$REPO/jam/safe.sh"
go=0; [ "${1:-}" = "--yes" ] && go=1
n=0
drop() { [ -e "$1" ] || return 0; n=$((n + 1)); if [ $go -eq 1 ]; then { safe_rm "$REPO/felucca-base/build" "$1" || safe_rm "${JAM_WORK:-}" "$1"; } && echo "removed  $1"; else echo "would remove  $1"; fi; }   # (only inside this checkout's build/ or the work folder)
if [ -n "${JAM_WORK:-}" ]; then for d in $(find "$JAM_WORK" -maxdepth 1 -name "check*" -mtime +7 2>/dev/null); do drop "$d"; done; fi      # (older than a week: the work folder is shared by every checkout)
for d in "$REPO"/felucca-base/build/*_mut; do drop "$d"; done
ls -dt "$REPO"/felucca-base/build/check/*/ 2>/dev/null | tail -n +4 | while read -r d; do drop "${d%/}"; done
[ $go -eq 1 ] || echo "(nothing removed: run again with --yes)"
