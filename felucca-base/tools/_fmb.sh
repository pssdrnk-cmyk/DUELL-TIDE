#!/bin/sh
[ -x "$HOME/venv/bin/python3" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
cd "$(dirname "$0")/.." || exit 1
python3 tools/build.py --gen-only > /dev/null 2>&1
python3 tools/factory_multis.py build 2>&1 | tail -12
