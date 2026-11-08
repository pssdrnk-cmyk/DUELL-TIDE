#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""drone_probe.py PORT NUMS (e.g. COM5 164,165,171): test-mode build; per drone (#164..#171) load it on part 1 by bank 2 program change, hold C3-Eb3-G3 for 12 s, read raw peak, misses, bnotes0, voices0."""
import sys, time, re, json, argparse
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "felucca-base" / "tools"))      # bench_run.py (WinSerialLink / the console link)
import bench_run as b, mido
out = mido.open_output([n for n in mido.get_output_names() if "DUELL" in n][0])
l = b.WinSerialLink(sys.argv[1]); l.open(); time.sleep(0.4); l.read(0.3)
def kv(cmd):
    l.send(cmd); time.sleep(0.25); d = {}
    for ln in l.read(0.3):
        s = ln.strip(); m = re.match(r"^([a-z_0-9]+) (-?\d+)$", s)
        if m: d[m.group(1)] = int(m.group(2))
        m = re.match(r"^(preset) (.+)$", s)
        if m: d[m.group(1)] = m.group(2)
    return d
ch = 2
for num in [int(x) for x in sys.argv[2].split(",")]:
    l.send("pload %d" % num); time.sleep(1.5); l.read(0.4)
    kv("cpu"); m0 = kv("cpu").get("cpu_miss", 0)
    for n in (48, 51, 55): out.send(mido.Message("note_on", channel=ch, note=n, velocity=100))
    pk = []; bn = []; v = []
    t0 = time.time()
    while time.time() - t0 < 12:
        time.sleep(0.6); c = kv("cpu"); pk.append(round(100 * c.get("cpu_raw_pk_us", 0) / 2902)); bn.append(c.get("bnotes0")); v.append(c.get("voices0"))
    for n in (48, 51, 55): out.send(mido.Message("note_off", channel=ch, note=n, velocity=0))
    s = kv("status"); m1 = kv("cpu").get("cpu_miss", 0); time.sleep(2.5)
    print(json.dumps(dict(num=num, preset=s.get("preset"), raw_max=max(pk), bnotes=min(x for x in bn if x is not None), voices=max(x for x in v if x is not None), miss=m1 - m0)), flush=True)
l.close()
