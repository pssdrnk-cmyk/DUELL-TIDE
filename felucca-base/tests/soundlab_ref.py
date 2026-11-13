#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare (or write) the stored soundlab reference numbers.
usage: soundlab_ref.py METRICS.json REF.txt [--write]
REF.txt lines: <dotted.json.path> <value> ; a value must match within 0.1 (the unit of each figure is in tools/soundlab/METRICS.md)."""
import json, sys
KEYS = ["level.lufs", "level.true_peak_dbfs", "level.rms_dbfs", "spectrum.bands_pct.lt150", "spectrum.bands_pct.gt2k", "spectrum.centroid_hz", "space.side_mid_db"]
m = json.load(open(sys.argv[1]))
def get(path):
    v = m
    for k in path.split("."):
        v = v[k]
    return float(v)
if "--write" in sys.argv:
    with open(sys.argv[2], "w", newline="\n") as f:
        for k in KEYS:
            f.write("%s %.2f\n" % (k, get(k)))
    print("wrote", sys.argv[2])
    sys.exit(0)
bad = 0
for ln in open(sys.argv[2]):
    if ln.startswith("#") or not ln.strip():
        continue
    k, v = ln.split()
    try:
        g = get(k)
    except (KeyError, TypeError):
        print("FAIL soundlab: %s missing" % k); bad += 1; continue
    if abs(g - float(v)) > 0.1:
        print("FAIL soundlab: %s = %.2f, reference %s" % (k, g, v)); bad += 1
sys.exit(1 if bad else 0)
