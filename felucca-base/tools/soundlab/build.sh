#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Builds soundlab: the firmware through the host simulation (tests/tide_roles_test.c as a library, as host_tests.sh builds its render tests) plus the tool's own C.
#   usage:  tools/soundlab/build.sh [--regen]      from any folder; on Windows first `. jam/env.sh` (the script sources it itself if cc or python3 is missing)
#   result: felucca-base/build/soundlab/soundlab   (.exe on Windows). --regen: make the generated headers again (they are made when missing).
# The compiler flags are host_tests.sh's CC (test mode on, HOST_MIX_FOLLOWS_MASTER), without its -DTIDE_ROLL_LOCK=0: the renders sound as the device does.
# Every tools/soundlab/sl_*.c is built; sl_render.c is the only one that sees the firmware.
set -e
T="$(cd "$(dirname "$0")" && pwd)"
FB="$(cd "$T/../.." && pwd)"
cd "$FB"
if ! command -v cc >/dev/null 2>&1 || ! command -v python3 >/dev/null 2>&1; then
    # shellcheck disable=SC1091
    [ -f "$FB/../jam/env.sh" ] && . "$FB/../jam/env.sh"
fi
O=build/soundlab
mkdir -p build/gen build/gen_test "$O"
if [ "${1:-}" = "--regen" ] || [ ! -f build/gen/duell_version.h ] || [ ! -f build/gen/felucca_tables.h ]; then
    python3 tools/build.py --gen-only > "$O/gen.log" 2>&1 || { tail -20 "$O/gen.log"; exit 1; }     # (the generated headers, without the device toolchain)
    REGEN=1
fi
if [ -n "${REGEN:-}" ] || [ ! -f build/gen_test/duell_multis.h ] || [ ! -f build/gen_test/midi_map_defaults.h ]; then
    python3 tools/gen_duell_multis.py build/gen/duell_multis.h >/dev/null
    python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null
    python3 tools/gen_midi_map.py build/gen/midi_map_defaults.h >/dev/null
    python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv build/gen_test/midi_map_defaults.h >/dev/null
    python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv --sym MIDI_MAP_LEGACY build/gen_test/midi_map_legacy.h >/dev/null
    python3 tools/gen_midi_map.py --sym MIDI_MAP_SHIPPED build/gen_test/midi_map_shipped.h >/dev/null
fi
sed "s/^int main(void)/int sl_unused_tr_main(void)/" tests/tide_roles_test.c > "$O/trt_lib.c"      # (the test as a library: its main renamed away)
FV=$(sed -n 's/^#define DUELL_VERSION_STR "\(.*\)"/\1/p' build/gen/duell_version.h)
SHA=$(git rev-parse --short=10 HEAD 2>/dev/null || echo unknown); git diff --quiet HEAD -- . 2>/dev/null || SHA="$SHA-dirty"
OBJS=
for f in "$T"/sl_*.c; do
    b=$(basename "$f" .c); OBJS="$OBJS $O/$b.o"
    if [ "$b" = sl_render ]; then
        cc -O2 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -I"$O" -I"$T" "-DFELUCCA_VERSION=\"$FV\"" "-DSL_COMMIT=\"$SHA\"" -c "$f" -o "$O/$b.o"
    else
        cc -O2 -Wall -I"$T" -c "$f" -o "$O/$b.o"
    fi
done
# shellcheck disable=SC2086
cc -O2 -o "$O/soundlab" $OBJS -lm
echo "built $FB/$O/soundlab ($SHA)"
