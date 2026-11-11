#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Turns an old reelrender voice log (ON / REL / END / NV / BAR lines) into a soundlab PREFIX.json, so the metrics can be tested on
WAVs the older lanes already rendered.  usage: oldlog2json.py OLD.log OUT.json [--name NAME]  (stdlib only)"""
import json, sys


def convert(log_path, name=""):
    fs, bpm, total = 44100, 120, 0
    open_v, voices, bars = {}, [], []
    for ln in open(log_path, errors="replace"):
        t = ln.split()
        if not t:
            continue
        if t[0].startswith("M") and "bpm" in t:
            bpm = int(t[t.index("bpm") + 1])
            if "fs" in t:
                fs = int(t[t.index("fs") + 1])
        elif t[0] == "ON":
            s, part, vo, note, vel = int(t[1]), int(t[2]), int(t[3]), int(t[4]), int(t[5])
            k = (part, vo)
            if k in open_v:                      # stolen: it ends here
                o = open_v.pop(k)
                if o["t1"] is None:
                    o["t1"] = s
                o["t_end"] = s
            o = {"t0": s, "t1": None, "t_end": None, "part": part, "note": note, "vel": vel}
            open_v[k] = o
            voices.append(o)
        elif t[0] == "REL":
            k = (int(t[2]), int(t[3]))
            if k in open_v and open_v[k]["t1"] is None:
                open_v[k]["t1"] = int(t[1])
        elif t[0] == "END":
            k = (int(t[2]), int(t[3]))
            if k in open_v:
                o = open_v.pop(k)
                if o["t1"] is None:
                    o["t1"] = int(t[1])
                o["t_end"] = int(t[1])
        elif t[0] == "BAR":
            bars.append(int(t[2]))
        elif t[0] == "TOTAL":
            total = int(t[1])
    for o in open_v.values():
        o["t1"] = o["t1"] if o["t1"] is not None else total
        o["t_end"] = total
    sec = lambda v: v / float(fs)
    out = {"tool": "soundlab", "commit": "oldlog", "sample_rate": fs, "bars": len(bars) + 1, "bpm": bpm,
           "source": {"multi": None, "name": name, "sound": None}, "settings": {},
           "bar_starts": [sec(b) for b in [0] + bars],
           "voices": [{"t0": sec(o["t0"]), "t1": sec(o["t1"]), "t_end": sec(o["t_end"]), "part": o["part"], "note": o["note"], "vel": o["vel"]} for o in voices],
           "events": [], "blocks": {}}
    return out


if __name__ == "__main__":
    nm = sys.argv[sys.argv.index("--name") + 1] if "--name" in sys.argv else ""
    json.dump(convert(sys.argv[1], nm), open(sys.argv[2], "w"), indent=0)
