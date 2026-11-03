#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL dub pack (helper): put the pack's 8 voice records into a backup file that jam/jam_dev.py restore (and the web Backup page) can take.

    tools/dub_pack/to_backup.py RECORDS_TXT BACKUP.json OUT.json --first-slot N

BACKUP.json is a backup you made yourself (jam_dev.py backup); the 8 voices replace patch slots #N .. #N+7 in a copy, OUT.json. A restore only takes
a complete backup (settings and the current music included), so a file of the patches alone is refused by the reader; this starts from yours. N has
no default: the pack's slots are not decided until the USER slots exist. jam_dev's own reader checks the result before it is written.
Nothing here talks to a unit."""
import argparse
import base64
import json
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "jam"))
import jam_dev as jd  # noqa: E402


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("records")
    ap.add_argument("backup")
    ap.add_argument("out")
    ap.add_argument("--first-slot", type=int, required=True)
    a = ap.parse_args(argv)
    recs = [bytes.fromhex(l.split(" ")[1]) for l in Path(a.records).read_text().splitlines() if l.strip()]
    recs = recs[:8]                                   # the voices; the pattern slots are not patches
    if not 1 <= a.first_slot <= jd.PATCH_SLOTS - len(recs) + 1:
        sys.exit(f"--first-slot must be 1 .. {jd.PATCH_SLOTS - len(recs) + 1}")
    f = json.loads(Path(a.backup).read_text(encoding="utf-8"))
    jd.read_archive(json.dumps(f))                    # the starting file must be sound
    mine = {a.first_slot - 1 + i: {"id": jd.PATCH_ID, "slot": a.first_slot - 1 + i, "size": len(r), "crc": zlib.crc32(r) & 0xFFFFFFFF,
                                   "data": base64.b64encode(r).decode("ascii")} for i, r in enumerate(recs)}
    keep = [o for o in f["objects"] if not (o.get("id") == jd.PATCH_ID and o.get("slot") in mine)]
    f["objects"] = [o for o in keep if o.get("id") != jd.PATCH_ID] + sorted([o for o in keep if o.get("id") == jd.PATCH_ID] + list(mine.values()), key=lambda o: o["slot"])
    f["patchRecords"] = True
    f["notes"] = [*f.get("notes", []), "DUELL dub pack: 8 voice patches added (tools/dub_pack)."]
    text = jd.file_text(f)
    arch = jd.read_archive(text)                      # the restore tool's own checks
    Path(a.out).write_text(text, encoding="utf-8", newline="\n")
    print(f"{len(arch.objects)} objects ({sum(1 for k in arch.objects if jd.is_patch_key(k))} patches) -> {a.out} (slots #{a.first_slot:02d}..#{a.first_slot + len(recs) - 1:02d})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
