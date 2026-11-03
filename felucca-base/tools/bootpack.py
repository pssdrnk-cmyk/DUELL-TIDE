# SPDX-License-Identifier: GPL-3.0-only
"""The boot pack (DUELL_RAMPLACE builds; firmware/src/ramplace.c unpacks it, firmware/app_ram.ld places it at _boot_pack).

The four RAM load images (.ram_text, .data, .ramz, .ramx) as ONE raw LZMA1 stream (lc 1, lp 1, pb 1) behind a header of little-endian
words: magic "BLZ1", the unpacked length, the stream's length, then per section its RAM address, length and bp_hash.
tools/build.py writes it, tools/elfcheck.py check_boot_pack decodes it again, tests/bootpack_test.c runs the firmware's decoder on it."""
import lzma
import struct

SECTIONS = ((".ram_text", "_rt"), (".data", "_data"), (".ramz", "_rz"), (".ramx", "_rx"))   # ramplace.c BP_RT, BP_DATA, BP_RZ, BP_RX
MAGIC = 0x315A4C42                      # "BLZ1"
HDR_WORDS = 3 + 3 * len(SECTIONS)
FILTERS = [{"id": lzma.FILTER_LZMA1, "lc": 1, "lp": 1, "pb": 1, "dict_size": 1 << 20}]
PACK_FILTERS = [dict(FILTERS[0], preset=9 | lzma.PRESET_EXTREME)]


def bp_hash(b):
    """ramplace.c bp_hash: FNV-1a over little-endian words"""
    h = 0x811C9DC5
    for (w,) in struct.iter_unpack("<I", b):
        h = ((h ^ w) * 0x01000193) & 0xFFFFFFFF
    return h


def pack(blobs):
    """blobs: [(RAM address, bytes)] in SECTIONS' order, whole words each -> the header and the stream"""
    raw = b"".join(b for _, b in blobs)
    stream = lzma.compress(raw, format=lzma.FORMAT_RAW, filters=PACK_FILTERS)
    hdr = [MAGIC, len(raw), len(stream)]
    for addr, b in blobs:
        if addr % 4 or len(b) % 4:
            raise ValueError(f"boot pack: the section at {addr:#x} ({len(b)} B) is not whole words")
        hdr += [addr, len(b), bp_hash(b)]
    out = struct.pack(f"<{HDR_WORDS}I", *hdr) + stream
    if unpack(out)[1] != [b for _, b in blobs]:
        raise ValueError("boot pack: the stream does not decode to the sections")
    return out


def unpack(data):
    """the header (a tuple of words) and the sections' bytes as the stream decodes them; ValueError for a broken pack"""
    if len(data) < 4 * HDR_WORDS:
        raise ValueError("boot pack: shorter than its header")
    hdr = struct.unpack_from(f"<{HDR_WORDS}I", data)
    if hdr[0] != MAGIC:
        raise ValueError("boot pack: no magic")
    stream = bytes(data[4 * HDR_WORDS:4 * HDR_WORDS + hdr[2]])
    if len(stream) != hdr[2]:
        raise ValueError("boot pack: the stream is cut short")
    try:
        raw = lzma.decompress(stream, format=lzma.FORMAT_RAW, filters=FILTERS)[:hdr[1]]
    except lzma.LZMAError as e:
        raise ValueError(f"boot pack: the stream does not decode: {e}") from e
    if len(raw) != hdr[1]:
        raise ValueError(f"boot pack: the stream decodes to {len(raw)} B, not {hdr[1]}")
    secs, pos = [], 0
    for k in range(len(SECTIONS)):
        n = hdr[4 + 3 * k]
        secs.append(raw[pos:pos + n])
        pos += n
    return hdr, secs
