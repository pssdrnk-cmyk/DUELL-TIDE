#!/bin/sh
# CPU budget proxy (host, WSL/Linux): the per-block cost of M16 STRESS TEST, alone (Z) and with TIDE's sequencers playing in automatic MUSIC WNDR/JRNY (T).
# Usage (from Git Bash/PowerShell):  wsl.exe -d Ubuntu --cd <worktree> -- sh felucca-base/tools/cpu_budget.sh [TREE] [SECTIONS]
#   TREE      the checkout to measure (default: this one); SECTIONS default "ZT"; env BARS (16), TURNS (1), POLY (1), JRNY (0: WANDER, 1: JOURNEY)
# There is no instruction counter in WSL2 (no perf events, ptrace single-step runs 13 k steps/s), so the unit is the BASIC BLOCK: gcc -fsanitize-coverage=trace-pc
# counts each one run, exactly and repeatably. tests/cpu_scenarios_test.c (the project's own CPU model, macOS instruction counts) is rebuilt around that counter
# by cpu_budget_patch.py; the firmware is compiled unchanged. Conversion to device %: see work/outputs/cpu-budget-2026-11-03/REPORT.md.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TREE="${1:-$(cd "$HERE/../.." && pwd)}"
SEC="${2:-ZT}"
cd "$TREE/felucca-base" || exit 1
[ -x "$HOME/venv/bin/python3" ] && PATH="$HOME/venv/bin:$PATH"
mkdir -p build/gen build/gen_test build/cb
[ -f build/gen/duell_sounds.h ] || python3 tools/build.py --gen-only > build/cb/gen.log 2>&1 || { tail build/cb/gen.log; exit 1; }
python3 tools/gen_duell_multis.py build/gen/duell_multis.h > /dev/null || exit 1
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode > /dev/null || exit 1
python3 tools/gen_midi_map.py build/gen/midi_map_defaults.h > /dev/null || exit 1
FV=$(sed -n 's/^#define DUELL_VERSION_STR "\(.*\)"/\1/p' build/gen/duell_version.h)
DEFS=""
grep -q 'sqm_auto;' firmware/src/seq_morph.c && DEFS="$DEFS -DHAVE_SQM_AUTO"
grep -q 'sqm_turns;' firmware/src/*.c firmware/src/*.h 2>/dev/null && DEFS="$DEFS -DHAVE_TURNS"
grep -q 'sqm_poly;' firmware/src/*.c firmware/src/*.h 2>/dev/null && DEFS="$DEFS -DHAVE_POLY"
python3 tools/cpu_budget_patch.py tests/cpu_scenarios_test.c build/cb/cpuscen_bb.c || exit 1
printf 'unsigned long long bbc_ext;\nvoid __sanitizer_cov_trace_pc(void) { bbc_ext++; }\n' > build/cb/bbc.c
cc -O2 -c -o build/cb/bbc.o build/cb/bbc.c || exit 1
cc -O1 -fno-builtin -D_FORTIFY_SOURCE=0 -fsanitize-coverage=trace-pc -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 $DEFS \
   -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests "-DFELUCCA_VERSION=\"$FV\"" -o build/cb/cpuscen_bb build/cb/cpuscen_bb.c build/cb/bbc.o -lm || exit 1
echo "# tree $TREE  commit $(git -C "$TREE" rev-parse --short HEAD 2>/dev/null || echo ?)$(git -C "$TREE" diff --quiet HEAD 2>/dev/null || echo +dirty)  defs:$DEFS"
CPU_SCEN="$SEC" build/cb/cpuscen_bb
