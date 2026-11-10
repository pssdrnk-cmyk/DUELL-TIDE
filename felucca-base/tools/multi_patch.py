#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""b7: byte patches of the factory multi records (tools/factory_multis/Mnn.bin), for the owner's records that are kept byte for byte (`kept`, `owner`).

    python3 tools/multi_patch.py show             the patched values now in the records
    python3 tools/multi_patch.py apply            writes PATCHES into the records (idempotent: absolute values)
    python3 tools/multi_patch.py keep-amount DIR  copies both parts' AMOUNT byte and End B block from DIR/Mnn.bin (the records as shipped before) into the
                                                   rebuilt records of REBUILT: a rebuild picks the sounds' present End B amounts, the shipped records keep theirs
                                                   (b8: not needed now: factory_multis.py build and drone_pack.py reproduce every shipped record, M01..M18, byte for byte)

A patch is (slot, part 0 / 1, parameter, value), the value as the unit shows it (LEVEL / TRIM 0..127, EQ dB): the record stores it +64.
Only what the music asks for is touched (b7 MULTIS lane, owner 9 November 2026: the owner's M10, M11, M13 and M16 get a level or a low cut, nothing else)."""
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("fm", ROOT / "tools" / "factory_multis.py")
fm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fm)

# the owner's records, patched (slot, part, parameter, value)
PATCHES = [
    (16, 0, "P_EQLG", -6),          # FINAL REEL: a gentle low cut on the brass pad (it keeps its octave)
    (16, 0, "P_TRIM", 95), (16, 1, "P_TRIM", 80),       # FINAL REEL: +1 dB, back to the set median after the low cut (-1.0 dB under it with a chord held)
    (10, 0, "P_TRIM", 101), (10, 1, "P_TRIM", 81),       # PAD SPLIT: -5 dB with a chord held (was +6.0 dB over the set)
    (11, 0, "P_TRIM", 113), (11, 1, "P_TRIM", 108),      # WHISPER TIDE: -6 dB (was +6.8)
]
REBUILT = (1, 5, 8, 9, 17, 18)


def at_of(b, part):
    parts, _ = fm.parts_of(b)
    pos = parts[part][2]
    return pos, pos + 5 + b[pos + 4]


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    P = fm.p_ids()
    if cmd in ("show", "apply"):
        for slot in sorted({p[0] for p in PATCHES}):
            f = fm.DIR / ("M%02d.bin" % slot)
            b = bytearray(f.read_bytes())
            for s, part, key, val in PATCHES:
                if s != slot:
                    continue
                v = at_of(bytes(b), part)[1] + P[key]
                if cmd == "apply":
                    b[v] = val + 64
                print("M%02d part %d %s = %d" % (slot, part + 1, key, b[v] - 64))
            if cmd == "apply":
                f.write_bytes(bytes(b))
    elif cmd == "keep-amount" and len(sys.argv) > 2:
        for slot in REBUILT:
            old = Path(sys.argv[2], "M%02d.bin" % slot).read_bytes()
            f = (fm.ROOT / "tools" / "drone_multis" if slot > 16 else fm.DIR) / ("M%02d.bin" % slot)
            b = bytearray(f.read_bytes())
            for part in range(2):
                n, o = at_of(bytes(b), part)[0], at_of(old, part)[0]
                assert b[n + 4] == old[o + 4]
                b[n + 2] = old[o + 2]
                b[n + 5:n + 5 + b[n + 4]] = old[o + 5:o + 5 + old[o + 4]]
            f.write_bytes(bytes(b))
            print("M%02d AMOUNT bytes kept from %s" % (slot, sys.argv[2]))
    else:
        sys.exit(__doc__)


main()
