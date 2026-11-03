# SPDX-License-Identifier: GPL-3.0-only
"""Boot packs for tests/bootpack_test.c: python3 tests/bootpack_gen.py OUTDIR -> the file names, one per line.
The device build's own (build/felucca.bin, when there is one) and synthetic ones: source text, random bytes (literals, far matches),
long runs (long matches, rep matches), tiny and empty sections."""
import random
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import bootpack  # noqa: E402


def words(b):
    return b + b"\0" * (-len(b) % 4)


def main(out):
    out.mkdir(parents=True, exist_ok=True)
    names = []
    img = ROOT / "build" / "felucca.bin"
    if img.exists():                    # the pack at the end of the device image: the last "BLZ1" whose stream ends the image
        data = img.read_bytes()
        at = data.rfind(struct.pack("<I", bootpack.MAGIC))
        while at >= 0 and (at % 4 or at + 4 * bootpack.HDR_WORDS + struct.unpack_from("<3I", data, at)[2] > len(data)):
            at = data.rfind(struct.pack("<I", bootpack.MAGIC), 0, at)
        if at >= 0:
            (out / "device.pack").write_bytes(data[at:])
            names.append("device.pack")
    rnd = random.Random(1)
    src = b"".join(p.read_bytes() for p in sorted((ROOT / "firmware" / "src").glob("*.c"))[:12])
    cases = {
        "source.pack": [src[:20000], src[20000:20004], src[30000:61000], src[70000:90000]],
        "random.pack": [bytes(rnd.getrandbits(8) for _ in range(9000)), b"", bytes(rnd.getrandbits(8) for _ in range(3000)), src[:4000]],
        "runs.pack": [b"\0" * 40000, b"\x55\xaa" * 3000, (b"abcd" * 50 + bytes(range(256))) * 40, b"\xff" * 4],
        "tiny.pack": [b"\1\2\3\4", b"", b"\0\0\0\0", b"DUEL"],
    }
    for name, secs in cases.items():
        blobs = [(0x01C00000 + 0x10000 * k, words(s)) for k, s in enumerate(secs)]
        (out / name).write_bytes(bootpack.pack(blobs))
        names.append(name)
    sys.stdout.buffer.write("".join(f"{(out / n).as_posix()}\n" for n in names).encode())   # (no CR on Windows: the shell reads the names)


if __name__ == "__main__":
    main(Path(sys.argv[1]))
