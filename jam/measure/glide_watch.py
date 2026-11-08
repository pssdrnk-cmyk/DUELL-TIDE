#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""glide_watch.py --com PORT [--secs S] [--load N]: on the DUELL test-mode build (USB SERIAL ON), log once a second the audio peak of the last second
(console `cpu`: pk = the render time, el_pk = the elapsed time the DMA deadline saw; its own window), the missed DMA periods, the shed voices and the preset, as JSON lines, while the owner drives the unit by hand (AUTO TIDE glides)."""
import sys, time, re, json, argparse
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "felucca-base" / "tools"))      # bench_run.py (WinSerialLink / the console link)
import bench_run as b
ap = argparse.ArgumentParser(); ap.add_argument("--secs", type=float, default=600); ap.add_argument("--com", required=True, help="the unit's console port, e.g. COM5 (Windows)"); ap.add_argument("--load", type=int, default=0)
a = ap.parse_args()
NEED = ("cpu_pk_us", "cpu_el_pk_us", "cpu_budget_us", "cpu_miss")      # K2: a counter the unit does not print is an error, never a 0
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
def cpu():
    c = kv("cpu"); miss = [k for k in NEED if k not in c]
    if miss: sys.exit("glide_watch: the console's cpu did not print %s (an old build, or a lost line): not a measurement" % ", ".join(miss))
    return c
cpu(); s0 = kv("status"); c0 = cpu(); miss0 = c0["cpu_miss"]; shed0 = s0.get("voices_shed", 0)
t0 = time.time(); lastp = None
while time.time() - t0 < a.secs:
    time.sleep(0.6)
    c = cpu(); s = kv("status"); BUD = c["cpu_budget_us"]
    row = dict(t=round(time.time() - t0, 1), pk=round(100 * c["cpu_pk_us"] / BUD), el_pk=round(100 * c["cpu_el_pk_us"] / BUD), miss=c["cpu_miss"] - miss0,
               shed=s.get("voices_shed", 0) - shed0, cpu=s.get("cpu_pct"), playing=s.get("playing"), preset=s.get("preset"))
    print(json.dumps(row), flush=True)
l.close()
