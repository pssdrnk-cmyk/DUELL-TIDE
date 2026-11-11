#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The emu_smoke group (host_tests.sh emu_smoke): web/emu/emu_run.c (the emulator's device, built natively) plays web/emu/scripts/emu_smoke.txt twice (boot, TIDE SETTINGS, the KNOBS row,
# SAVE > MULTIS on M03, GLO); both runs must give the same screens (CRC of each 240 x 240 frame), and each screen's CRC must be one of those stored in tests/emu_smoke_ref.txt.
# A line of the reference is "NAME crc [crc ...]": more than one CRC when machines draw a screen differently (a compiler's float rounding). Pages with no live waveform only.
#   tests/emu_smoke.sh [--write-ref | --add-ref]      from felucca-base/   (--write-ref: replace the reference; --add-ref: add this machine's CRCs next to the stored ones)
set -e
mkdir -p build/emu
cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/emu_run web/emu/emu_run.c -lm || { echo "FAIL emu_smoke: emu_run does not build"; exit 1; }
rm -rf build/emu/smoke_a build/emu/smoke_b
build/emu/emu_run web/emu/scripts/emu_smoke.txt --out build/emu/smoke_a || { echo "FAIL emu_smoke: the script did not run"; exit 1; }
build/emu/emu_run web/emu/scripts/emu_smoke.txt --out build/emu/smoke_b || { echo "FAIL emu_smoke: the second run"; exit 1; }
cmp -s build/emu/smoke_a/screens.txt build/emu/smoke_b/screens.txt || { echo "FAIL emu_smoke: two runs of the same script give different screens"; exit 1; }
cmp -s build/emu/smoke_a/tide_settings.png build/emu/smoke_b/tide_settings.png || { echo "FAIL emu_smoke: the PNGs differ"; exit 1; }
case "${1:-}" in
--write-ref) cp build/emu/smoke_a/screens.txt tests/emu_smoke_ref.txt; echo "wrote tests/emu_smoke_ref.txt"; exit 0 ;;
--add-ref)
    awk 'NR==FNR { n[$1] = $0; next } { split(n[$1], a, " "); have = 0; for (i = 2; i <= length(a); i++) if (a[i] == $2) have = 1; print $1 " " (n[$1] == "" ? "" : substr(n[$1], length($1) + 2)) (have || n[$1] == "" ? "" : " ") (have ? "" : $2) }' \
        tests/emu_smoke_ref.txt build/emu/smoke_a/screens.txt | sed 's/  */ /g; s/ $//' > build/emu/ref_new.txt
    cp build/emu/ref_new.txt tests/emu_smoke_ref.txt; echo "added this machine's CRCs to tests/emu_smoke_ref.txt"; exit 0 ;;
esac
bad=0
record=0
[ "${CI:-}" = true ] && [ "${EMU_SMOKE_STRICT:-}" != 1 ] && record=1     # TEMPORARY: on CI (GitHub sets CI=true) only print each screen's CRC and never fail on an unknown one, until the Linux CRCs are in the reference (then remove this line)
while read -r name crc; do
    echo "emu_smoke: screen $name CRC $crc"
    [ $record = 1 ] && continue
    if ! grep -E "^$name( |$)" tests/emu_smoke_ref.txt | grep -qw "$crc"; then
        echo "FAIL emu_smoke: screen $name has CRC $crc, the reference has: $(grep -E "^$name " tests/emu_smoke_ref.txt | cut -d' ' -f2-)"
        bad=1
    fi
done < build/emu/smoke_a/screens.txt
if [ $bad = 1 ]; then
    echo "emu_smoke: if this machine draws these screens differently (not a UI change): tests/emu_smoke.sh --add-ref; a UI change on purpose: --write-ref; say which in the commit"
    exit 1
fi
echo "emu_smoke: deterministic, $(wc -l < tests/emu_smoke_ref.txt) screens equal to a stored CRC"
