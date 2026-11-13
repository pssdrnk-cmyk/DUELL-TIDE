#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""soundlab compare A B: before / after table (Markdown) of two PREFIX.metrics.json files, or of two folders (files matched by name).

  compare.py A.metrics.json B.metrics.json            headline metrics, A | B | delta
  compare.py BEFORE_DIR AFTER_DIR                     one section per common *.metrics.json, a summary of the ones in one folder only
  options: --all (every number)   --only REGEX (metrics whose dotted name matches)   --changed (only rows that moved)
Stdlib only.  Dotted names are the JSON paths, e.g. level.lufs, spectrum.bands_pct.lt60, space.echo.hf_drop_db_per_repeat."""
import json
import os
import re
import sys

HEADLINE = [
    "level.lufs", "level.true_peak_dbfs", "level.rms_dbfs", "level.clipped_samples",
    "spectrum.bands_pct.lt60", "spectrum.bands_pct.60_150", "spectrum.bands_pct.150_400", "spectrum.bands_pct.lt150",
    "spectrum.bands_pct.gt2k", "spectrum.bands_pct.gt4k", "spectrum.centroid_hz", "spectrum.centroid_bar_range_oct",
    "spectrum.whistle.spike_db", "spectrum.whistle.narrow_longest_s",
    "time.bed.both_mean", "time.bed.both_max", "time.silence_notes_pct", "time.silence_audio_pct", "time.onsets_per_bar.all",
    "time.low_onsets_per_beat.0", "time.low_onsets_per_beat.1", "time.low_onsets_per_beat.2", "time.low_onsets_per_beat.3",
    "time.sub.level_dbfs", "time.sub.mod_depth_std_over_mean",
    "parts.p1.attack_ms", "parts.p1.blip_pct", "parts.p1.odd_even_db", "parts.p1.env_move_pct",
    "parts.p2.attack_ms", "parts.p2.blip_pct", "parts.p2.odd_even_db", "parts.p2.env_move_pct",
    "space.side_mid_db", "space.lr_correlation", "space.after_noteoff.energy_pct", "space.echo.hf_drop_db_per_repeat",
    "space.echo.repeats_above_minus40db", "space.echo.time_to_minus40db_s", "space.hits.gap_fill_db_re_hit_median",
    "space.hits.decay_to_minus20db_ms_median",
    "pitch.p1.median", "pitch.p1.p90", "pitch.p1.highest", "pitch.p2.median", "pitch.p2.p90", "pitch.p2.highest",
    "pitch.rubs_REL.minor2_or_9_pct", "pitch.rubs_ALL.minor2_or_9_pct", "pitch.rubs_ALL.both_parts_sounding_pct",
    "quality.second_difference_click_db", "quality.end_cut_dbfs",
]


def flatten(obj, prefix=""):
    """number leaves by dotted path; short arrays of numbers by index, long ones (per-bar series) skipped"""
    out = {}
    if isinstance(obj, dict):
        for k, v in obj.items():
            out.update(flatten(v, prefix + "." + k if prefix else k))
    elif isinstance(obj, list):
        if len(obj) <= 8 and all(isinstance(v, (int, float, type(None))) for v in obj):
            for i, v in enumerate(obj):
                if v is not None:
                    out["%s.%d" % (prefix, i)] = float(v)
        elif all(isinstance(v, dict) for v in obj) and len(obj) <= 8:
            for i, v in enumerate(obj):
                out.update(flatten(v, "%s.%d" % (prefix, i)))
    elif isinstance(obj, bool):
        out[prefix] = 1.0 if obj else 0.0
    elif isinstance(obj, (int, float)):
        out[prefix] = float(obj)
    return out


def fmt(v):
    if v is None:
        return "-"
    if abs(v) >= 1000:
        return "%.0f" % v
    return "%.4g" % v


def table(a, b, args, la="A", lb="B"):
    fa, fb = flatten(a), flatten(b)
    names = [k for k in sorted(set(fa) | set(fb))] if args["all"] else [k for k in HEADLINE if k in fa or k in fb]
    if args["only"]:
        names = [k for k in sorted(set(fa) | set(fb)) if re.search(args["only"], k)]
    rows = []
    for k in names:
        x, y = fa.get(k), fb.get(k)
        d = None if x is None or y is None else y - x
        if args["changed"] and (d is None or abs(d) < 1e-9) and x == y:
            continue
        rows.append("| %s | %s | %s | %s |" % (k, fmt(x), fmt(y), "-" if d is None else ("%+.3g" % d)))
    head = ["| metric | %s | %s | delta |" % (la, lb), "|---|---|---|---|"]
    return "\n".join(head + rows) if rows else "(no rows)"


def load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def main(argv):
    args = {"all": False, "only": None, "changed": False}
    pos = []
    i = 0
    while i < len(argv):
        if argv[i] == "--all":
            args["all"] = True
        elif argv[i] == "--changed":
            args["changed"] = True
        elif argv[i] == "--only" and i + 1 < len(argv):
            i += 1
            args["only"] = argv[i]
        elif argv[i].startswith("--"):
            sys.exit(__doc__)
        else:
            pos.append(argv[i])
        i += 1
    if len(pos) != 2:
        sys.exit(__doc__)
    a, b = pos
    if os.path.isdir(a) and os.path.isdir(b):
        fa = {f for f in os.listdir(a) if f.endswith(".metrics.json")}
        fb = {f for f in os.listdir(b) if f.endswith(".metrics.json")}
        for f in sorted(fa & fb):
            print("### %s\n" % f[:-len(".metrics.json")])
            print(table(load(os.path.join(a, f)), load(os.path.join(b, f)), args, "before", "after"))
            print()
        if fa - fb:
            print("only in %s: %s" % (a, ", ".join(sorted(fa - fb))))
        if fb - fa:
            print("only in %s: %s" % (b, ", ".join(sorted(fb - fa))))
        if not (fa & fb):
            sys.exit("no *.metrics.json with the same name in both folders")
    else:
        print(table(load(a), load(b), args, os.path.basename(a), os.path.basename(b)))


if __name__ == "__main__":
    main(sys.argv[1:])
