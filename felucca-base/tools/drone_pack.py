#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL-TIDE drone pack: builds the eight drone multis into packs/drones/drones.duell-multis.json (the web editor's MULTIS tab, "Import file...").

    python3 tools/drone_pack.py [OUTDIR]       (needs build/gen: python3 ../gen_only.py; a C compiler)

One multi per drone theme of the W15 drones (#164 to #171, tools/duell_sounds.py): part 1 the drone, part 2 a partner drone/pad, both latched (HOLD ALL),
LAYER, a slow echo and a big hall. The records are made by the same code as the factory multis (tools/factory_multis.py's table machinery,
tests/gen_factory_multis.c), but into a scratch folder: no factory slot changes. A multi does not carry TIDE's STYLE; the README says to pick STYLE DRONE."""
import base64
import json
import os
import subprocess
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import factory_multis as fm  # noqa: E402

G = dict(G_BPM=60, G_DTIME="4D", G_DFDBK=60, G_DMIX=64, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=110, G_RDAMP=57)


def multi(slot, name, what, a, b, la=98, lb=82):
    hold = dict(P_AMODE="OFF", P_AHOLD="ALL")
    return dict(slot=slot, name=name, mode="LAYER", what=what, g=dict(G),
                parts=[dict(sound=a, set=dict(hold, P_LEVEL=la)), dict(sound=b, set=dict(hold, P_LEVEL=lb))])


DRONES = [
    multi(9, "DUNE THROAT", "a man's vowels in the sand, a slow solar wind over them", "SAND THROAT", "SOLAR WIND"),
    multi(10, "DUNE HORN", "a swelling horn with a shimmer of void above it", "HORN SWELL", "VOID SHIMMER"),
    multi(11, "NEON ROOF", "blade-runner brass over rain on strings", "NEON BRASS", "RAIN STRINGS"),
    multi(12, "RAIN CITY", "rain strings over the hum of the grid", "RAIN STRINGS", "GRID HUM"),
    multi(13, "VOID DEEP", "a wide shimmering void with a thin solar wind", "VOID SHIMMER", "SOLAR WIND", 96, 76),
    multi(14, "SOLAR DRIFT", "a solar wind carrying a slow horn", "SOLAR WIND", "HORN SWELL", 94, 80),
    multi(15, "GRID BUZZ", "a lo-fi mains hum under neon brass", "GRID HUM", "NEON BRASS", 96, 80),
    multi(16, "IRON YARD", "iron works over a sand throat: the heaviest of the eight", "IRON WORKS", "SAND THROAT", 94, 84),
]


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else fm.ROOT.parent / "packs" / "drones")
    os.chdir(fm.ROOT)
    fm.MULTIS = DRONES
    snd = fm.sounds()
    fm.resolve(snd)
    fm.emit(fm.GEN / "factory_multis_def.h")
    (fm.GEN / "duell_multis.h").write_text(fm.BOOT_H)
    exe = fm.ROOT / "build" / "host" / "gen_factory_multis"
    exe.parent.mkdir(parents=True, exist_ok=True)
    cc = ["cc", "-O1", "-w", "-DDUELL_TEST_MODE=1", "-I" + str(fm.GEN), "-Ibuild/gen", "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="x"',
          "-o", str(exe), "tests/gen_factory_multis.c", "-lm"]
    if subprocess.run(cc).returncode:
        sys.exit("drone_pack: tests/gen_factory_multis.c does not compile (python3 ../gen_only.py first)")
    scratch = fm.ROOT / "build" / "drone_pack"
    scratch.mkdir(parents=True, exist_ok=True)
    if subprocess.run([str(exe), "build", str(scratch)]).returncode:
        sys.exit("drone_pack: the build failed")
    items = []
    for m in DRONES:
        b = (scratch / ("M%02d.bin" % m["slot"])).read_bytes()
        items.append({"slot": m["slot"], "label": "M%02d" % m["slot"], "name": m["name"], "version": b[0], "size": len(b),
                      "crc": zlib.crc32(b) & 0xFFFFFFFF, "data": base64.b64encode(b).decode("ascii")})
    f = {"format": "duell-multis", "version": 1, "firmware": "DUELL-TIDE drone pack", "created": "2026-11-08T00:00:00.000Z", "multis": items}
    out.mkdir(parents=True, exist_ok=True)
    (out / "drones.duell-multis.json").write_text(json.dumps(f, indent=1) + "\n", encoding="utf-8", newline="\n")
    print("%d multis -> %s" % (len(items), out / "drones.duell-multis.json"))


if __name__ == "__main__":
    main()
