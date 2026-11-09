#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL-TIDE built-in drone multis (TIDE 1.0.0-b6): makes the records of M17 DUNE HORN and M18 VOID DEEP into tools/drone_multis/M17.bin, M18.bin.

    python3 tools/drone_pack.py        (needs build/gen: python3 ../gen_only.py; a C compiler)

M17 and M18 are ROM-only multis past the sixteen stored slots (firmware/src/multi.c MU_SHOWN): tools/gen_duell_multis.py puts these two records into
build/gen/duell_multis.h with DF_SLOT 16 and 17, next to the factory multis M01..M16, and checks them against the rows below. Each is a LAYER of two
latched drones (HOLD ALL, arp OFF) of the W15 drone sounds (tools/duell_sounds.py), a 60 BPM ping-pong echo and a big hall. TIDE plays them as STYLE
DRONE (tide.c). The records are made by the same code as the factory multis (tools/factory_multis.py's table machinery, tests/gen_factory_multis.c),
built in scratch slots M01, M02 of a host build (a record does not hold its slot) and copied here. (b4 shipped eight of these as a file pack,
packs/drones/; b6 builds two in and drops the pack, owner 9 November 2026.)"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import factory_multis as fm  # noqa: E402

G = dict(G_BPM=60, G_DTIME="4D", G_DFDBK=60, G_DMIX=64, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=110, G_RDAMP=57)
OUT = fm.ROOT / "tools" / "drone_multis"


def multi(rom, slot, name, what, a, b, la=98, lb=82):
    hold = dict(P_AMODE="OFF", P_AHOLD="ALL")
    return dict(rom=rom, slot=slot, name=name, mode="LAYER", what=what, g=dict(G),
                parts=[dict(sound=a, set=dict(hold, P_LEVEL=la)), dict(sound=b, set=dict(hold, P_LEVEL=lb))])


# rom: the multi's place (M17, M18); slot: the scratch slot of the host build
DRONES = [
    multi(17, 1, "DUNE HORN", "a swelling horn with a shimmer of void above it", "HORN SWELL", "VOID SHIMMER"),
    multi(18, 2, "VOID DEEP", "a wide shimmering void with a thin solar wind", "VOID SHIMMER", "SOLAR WIND", 96, 80),   # (part 2 at 80: the factory rule, -16 .. +4 dB, tests/level_test.c; the b4 pack had 76)
]


def main():
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
    OUT.mkdir(parents=True, exist_ok=True)
    for m in DRONES:
        shutil.copyfile(scratch / ("M%02d.bin" % m["slot"]), OUT / ("M%02d.bin" % m["rom"]))
        print("M%02d %s -> %s" % (m["rom"], m["name"], OUT / ("M%02d.bin" % m["rom"])))


if __name__ == "__main__":
    main()
