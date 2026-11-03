#!/bin/sh
# DUELL dub pack: build the renderer, resolve the sounds, store + render them, assemble the patch files (Git Bash on Windows, as jam/env.sh)
#   sh tools/dub_pack/build.sh OUTDIR
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BASE="$(cd "$HERE/../.." && pwd)"
. "$BASE/../jam/env.sh"
OUT="${1:?usage: build.sh OUTDIR}"
mkdir -p "$OUT"
cd "$BASE"
mkdir -p build/gen build/gen_test build/host build/dub
unset DUELL_TEST_MODE
for c in "gen_aa_font.py ui_fonts.h --preset barlow" "gen_aa_icons.py ui_icons.h" "gen_aa_keycaps.py ui_keycaps.h" "gen_ui_palettes.py ui_palettes.h" \
         "gen_splash_logo.py splash_logo.h" "gen_tables.py felucca_tables.h" "gen_fm6_patches.py felucca_fm6.h" "gen_duell_sounds.py duell_sounds.h" \
         "gen_version.py duell_version.h"; do
    set -- $c; s=$1; h=$2; shift 2
    [ -f build/gen/$h ] || python3 tools/$s build/gen/$h "$@" >/dev/null
done
python3 tools/gen_duell_multis.py build/gen/duell_multis.h >/dev/null
python3 tools/gen_duell_multis.py build/gen_test/duell_multis.h --test-mode >/dev/null
python3 tools/gen_midi_map.py build/gen/midi_map_defaults.h >/dev/null
python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv build/gen_test/midi_map_defaults.h >/dev/null
python3 tools/gen_midi_map.py --csv tests/midimap_legacy.csv --sym MIDI_MAP_LEGACY build/gen_test/midi_map_legacy.h >/dev/null
python3 tools/gen_midi_map.py --sym MIDI_MAP_SHIPPED build/gen_test/midi_map_shipped.h >/dev/null
sed 's/^int main(void)/int patchraw_main(void)/' tests/patchraw_test.c > build/dub/patchraw_nomain.c
cc -O1 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/dub -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests '-DFELUCCA_VERSION="x"' \
   -o build/host/dub_render tools/dub_pack/render.c -lm
python3 tools/dub_pack/pack.py spec build/dub/spec.txt
python3 tools/dub_pack/pack.py patterns build/dub/patterns.json
build/host/dub_render build/dub/spec.txt "$OUT"
node tools/dub_pack/assemble.mjs "$OUT/records.txt" build/dub/patterns.json "$OUT"
