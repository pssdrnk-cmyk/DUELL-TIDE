#!/bin/sh
[ -x "$HOME/venv/bin/python3" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
cd "$(dirname "$0")/.." || exit 1
python3 tools/build.py --gen-only > /dev/null 2>&1
cd .. || exit 1
for s in "$@"; do
  echo "== slot $s PLAY"; SLOT=$s PLAY=${PLAY:-1} BARS=${BARS:-8} sh felucca-base/tools/cpu_budget.sh "$PWD" Z 2>&1 | grep -E "^Z LAYER|rror"
done
