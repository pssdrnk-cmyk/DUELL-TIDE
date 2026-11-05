#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
[ -x "$HOME/venv/bin/python3" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
cd "$(dirname "$0")/../.." || exit 1
BARS=8 sh felucca-base/tools/cpu_budget.sh "$PWD" Z 2>&1 | grep -E "^Z LAYER|rror" | head -2
