#!/bin/sh
[ -x "$HOME/venv/bin/python3" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
cd "$(dirname "$0")/.." || exit 1
python3 tools/build.py --gen-only > /dev/null 2>&1
cd .. || exit 1
sh host_tests.sh mrender > /dev/null 2>&1
cd felucca-base && ./build/host/multi_render_test tests/multi_render_golden.txt 2>&1 | grep -a "M16\|S17\|changed"
