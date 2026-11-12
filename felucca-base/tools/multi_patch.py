#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""b7: byte patches of the factory multi records (tools/factory_multis/Mnn.bin), for the owner's records that are kept byte for byte (`kept`, `owner`).

    python3 tools/multi_patch.py show             the patched values now in the records
    python3 tools/multi_patch.py apply            writes PATCHES into the records (idempotent: absolute values)
                                                   (b10: the old `keep-amount DIR` command is gone: since b8 factory_multis.py build and drone_pack.py reproduce every
                                                   shipped record byte for byte, and `factory_multis.py verify` / `drone_pack.py verify` keep it so)

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
    else:
        sys.exit(__doc__)


main()
