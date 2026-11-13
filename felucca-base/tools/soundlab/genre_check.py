#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""soundlab rules --genre NAME PREFIX: the measured numbers of PREFIX (run `soundlab measure PREFIX` first) against genres.json (stdlib only).
Per target: PASS (inside min..max), FAIL (outside), UNSURE (not a number soundlab measures, or the basis is a single example / disputed, shown with the number).
'measured' ranges (the factory multis of the genre in the approved set) are a regression band: outside = DRIFT, not a failure of the genre."""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
# genres.json target / measured name -> path in PREFIX.metrics.json (or "bpm" in PREFIX.json)
PATH = {
    "side_mid_db": "space.side_mid_db", "bed_notes_mean": "time.bed.both_mean", "hf_drop_per_repeat_db": "space.echo.hf_drop_db_per_repeat",
    "share_under_150hz_pct": "spectrum.bands_pct.lt150", "share_under_60hz_pct": "spectrum.bands_pct.lt60", "share_above_4khz_pct": "spectrum.bands_pct.gt4k",
    "centroid_hz": "spectrum.centroid_hz", "lufs": "level.lufs", "true_peak_dbfs": "level.true_peak_dbfs", "onsets_per_bar": "time.onsets_per_bar.all",
    "tail_pct": "space.after_noteoff.energy_pct", "reverb_decay_s": "space.echo.time_to_minus40db_s",
}


def get(m, path):
    for k in path.split("."):
        if not isinstance(m, dict) or k not in m:
            return None
        m = m[k]
    return m if isinstance(m, (int, float)) else None


def inside(v, t):
    return (t["min"] is None or v >= t["min"]) and (t["max"] is None or v <= t["max"])


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == "--exe":                    # (soundlab passes its own path first)
        a = a[2:]
    if len(a) < 2:
        sys.exit("usage: soundlab rules --genre NAME PREFIX")
    name, prefix = a[0], a[1]
    G = json.load(open(os.path.join(HERE, "genres.json")))["genres"]
    if name not in G:
        sys.exit("genres: %s" % ", ".join(sorted(G)))
    m = json.load(open(prefix + ".metrics.json"))
    j = json.load(open(prefix + ".json")) if os.path.exists(prefix + ".json") else {}
    g = G[name]
    worst = 0
    print("genre %s (multis %s); PASS / FAIL / UNSURE per sourced target, DRIFT for the measured band" % (name, ",".join("M%02d" % n for n in g["multis"])))
    for tn, t in g["targets"].items():
        v = j.get("bpm") if tn == "tempo_bpm" else get(m, PATH[tn]) if tn in PATH else None
        rng = "%s..%s %s" % ("" if t["min"] is None else t["min"], "" if t["max"] is None else t["max"], t["unit"])
        if v is None:
            print("  UNSURE %-24s not measured by soundlab (target %s; %s)" % (tn, rng, t["basis"]))
        elif t["basis"] in ("example", "sourced-disputed", "sourced-other-genre"):
            print("  UNSURE %-24s %.2f vs %s: %s, not a hard limit (%s)" % (tn, v, rng, t["basis"], t["source"].split(":")[0]))
        elif inside(v, t):
            print("  PASS   %-24s %.2f in %s" % (tn, v, rng))
        else:
            print("  FAIL   %-24s %.2f outside %s (%s)" % (tn, v, rng, t["basis"])); worst = 1
    for tn, t in g.get("measured", {}).items():
        v = get(m, PATH[tn]) if tn in PATH else None
        if v is None:
            continue
        if not inside(v, t):
            print("  DRIFT  %-24s %.2f outside the approved multis' %s..%s %s (n=%d)" % (tn, v, t["min"], t["max"], t["unit"], t["n"]))
    return worst


if __name__ == "__main__":
    sys.exit(main())
