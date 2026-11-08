#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""glide_watch.py --com PORT [--secs S] [--load N]: on the DUELL test-mode build (USB SERIAL ON), log once a second the audio peak of the last second
(console `cpu`, clears it), the missed blocks, the shed voices and the preset, as JSON lines, while the owner drives the unit by hand (AUTO TIDE glides)."""
import sys, time, re, json, argparse
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "felucca-base" / "tools"))      # bench_run.py (WinSerialLink / the console link)
import bench_run as b
ap = argparse.ArgumentParser(); ap.add_argument("--secs", type=float, default=600); ap.add_argument("--com", required=True, help="the unit's console port, e.g. COM5 (Windows)"); ap.add_argument("--load", type=int, default=0)
a = ap.parse_args()
BUD = 2902
l = b.WinSerialLink(a.com); l.open(); time.sleep(0.4); l.read(0.3)
def kv(cmd, wait=0.25):
    l.send(cmd); time.sleep(wait); d = {}
    for ln in l.read(0.3):
        s = ln.strip()
        m = re.match(r"^([a-z_0-9]+) (-?\d+)$", s)
        if m: d[m.group(1)] = int(m.group(2))
        m = re.match(r"^(preset|engine) (.+)$", s)
        if m: d[m.group(1)] = m.group(2)
    return d
if a.load:
    l.send("mload %d" % a.load); time.sleep(1.5); l.read(0.5)
kv("cpu"); s0 = kv("status"); miss0 = kv("cpu").get("cpu_miss", 0); shed0 = s0.get("voices_shed", 0)
t0 = time.time(); lastp = None
while time.time() - t0 < a.secs:
    time.sleep(0.6)
    c = kv("cpu"); s = kv("status")
    row = dict(t=round(time.time() - t0, 1), pk=round(100 * c.get("cpu_pk_us", 0) / BUD), miss=c.get("cpu_miss", 0) - miss0,
               shed=s.get("voices_shed", 0) - shed0, cpu=s.get("cpu_pct"), playing=s.get("playing"), preset=s.get("preset"))
    print(json.dumps(row), flush=True)
l.close()
