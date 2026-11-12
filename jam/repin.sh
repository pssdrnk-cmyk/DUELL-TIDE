#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Re-pin test hashes, but only after the change that moved them is proven by reverting it. See jam/repin.py and jam/README.md ("Fast tests").
#   sh jam/repin.sh CAUSE_COMMIT [group ...]
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/env.sh" 2>/dev/null || true
exec python3 "$here/repin.py" "$@"
