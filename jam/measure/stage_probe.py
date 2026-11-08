#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""stage_probe.py SLOT --com PORT [--glide] [--secs S]: a build with DUELL_PROBE_STAGES=1 as well (the st_* words of `cpu`); per second the longest block of morph_begin / part 1 / part 2 wait (us), raw half peak %, misses."""
import sys, time, re, json, argparse
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "felucca-base" / "tools"))      # bench_run.py (WinSerialLink / the console link)
import bench_run as b, mido
ap = argparse.ArgumentParser(); ap.add_argument("slot", type=int); ap.add_argument("--glide", action="store_true"); ap.add_argument("--secs", type=float, default=16); ap.add_argument("--com", required=True, help="the unit's console port, e.g. COM5 (Windows)")
a = ap.parse_args()
out = mido.open_output([n for n in mido.get_output_names() if "DUELL" in n][0])
l = b.WinSerialLink(a.com); l.open(); time.sleep(0.4); l.read(0.3)
def kv(cmd):
    l.send(cmd); time.sleep(0.25); d = {}
    for ln in l.read(0.3):
        m = re.match(r"^([a-z_0-9]+) (-?\d+)$", ln.strip())
        if m: d[m.group(1)] = int(m.group(2))
    return d
l.send("mload %d" % a.slot); time.sleep(1.5); l.read(0.5)
out.send(mido.Message("start")); time.sleep(1)
if a.glide: l.send("glide 1"); time.sleep(0.3); l.read(0.3)
kv("cpu"); m0 = kv("cpu").get("cpu_miss", 0); t0 = time.time()
while time.time() - t0 < a.secs:
    time.sleep(0.6); c = kv("cpu")
    print(json.dumps(dict(raw=round(100*c.get("cpu_raw_pk_us",0)/2902), morph=c.get("st_morph_us"), p1=c.get("st_part1_us"), p2wait=c.get("st_part2wait_us"), xb=c.get("st_xblock_us"), vox=c.get("st_voices1_us"), br=c.get("fm6_bridge0"), bn=c.get("bnotes0"), mv=c.get("bdmv0"), need=c.get("need0"), fd=c.get("fader0"), v0=c.get("voices0"), v1=c.get("voices1"), miss=c.get("cpu_miss",0)-m0)), flush=True)
if a.glide: l.send("glide 0"); time.sleep(0.3)
out.send(mido.Message("stop")); l.close()
