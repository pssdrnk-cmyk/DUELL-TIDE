#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""cpu_multis.py SLOTS --com PORT [--hold S] [--ch N] [--start] [--glide [--gtime T]]: on a DUELL test-mode build (USB SERIAL ON), load each factory multi (console `mload N`),
play it (a held Cm7 chord on part 1's channel, optionally MIDI Start for PLAY), and log the console's status once a second.
--glide (wave 16): after the load, start the sequence and launch the TIDE page's K1 glide through the console (`glide 1 [T]`: MODE BACK AND FORTH, PLAY starts with it; nothing is touched on the unit),
sample `cpu` once a second for --hold seconds (the longest half of each second in us, as a percent of the half budget the unit reports (cpu_budget_us), in "peak_pct" = the render time and "el_peak_pct" = the elapsed time the DMA deadline saw), then `glide 0`. --gtime T = the trip's TIME index (0 = 1 bar .. 6 = 64 bars,
default 0: a bar, 4 seconds at 120 BPM a leg, so a hold of 20 s sees five legs). The row gets glide = 1 and the misses, sheds and late blocks of the hold, as without --glide."""
import sys, time, re, json, argparse
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "felucca-base" / "tools"))      # bench_run.py (WinSerialLink / the console link)
import bench_run as b, mido
ap = argparse.ArgumentParser(); ap.add_argument("slots"); ap.add_argument("--hold", type=float, default=20); ap.add_argument("--ch", type=int, default=3)
ap.add_argument("--start", action="store_true"); ap.add_argument("--glide", action="store_true"); ap.add_argument("--gtime", type=int, default=0); ap.add_argument("--com", required=True, help="the unit's console port, e.g. COM5 (Windows)"); ap.add_argument("--chord", default="48,51,55,58")
a = ap.parse_args()
slots = [int(x) for x in a.slots.split(",")] if "," in a.slots or a.slots.isdigit() else list(range(1, 17))
ch = a.ch - 1; chord = [] if a.chord == "none" else [int(x) for x in a.chord.split(",")]
out = mido.open_output([n for n in mido.get_output_names() if "DUELL" in n][0])
l = b.WinSerialLink(a.com); l.open(); time.sleep(0.4); l.read(0.3)
def st():
    l.send("status"); time.sleep(0.25); txt = l.read(0.35)
    d = {}
    for ln in txt:
        m = re.match(r"^([a-z_0-9]+) (-?\d+)$", ln.strip())
        if m: d[m.group(1)] = int(m.group(2))
        m = re.match(r"^(preset|engine) (.+)$", ln.strip())
        if m: d[m.group(1)] = m.group(2)
    return d
NEED = ("cpu_pk_us", "cpu_el_pk_us", "cpu_budget_us", "cpu_miss")      # K2: a counter the unit does not print is an error, never a 0
def cpu_kv():
    l.send("cpu"); time.sleep(0.2); d = {}
    for ln in l.read(0.3):
        m = re.match(r"^([a-z_0-9]+) (-?\d+)$", ln.strip())
        if m: d[m.group(1)] = int(m.group(2))
    miss = [k for k in NEED if k not in d]
    if miss: sys.exit("cpu_multis: the console's cpu did not print %s (an old build, or a lost line): not a measurement" % ", ".join(miss))
    return d
res = []
for s in slots:
    l.send("mload %d" % s); time.sleep(0.3); got = l.read(0.5); time.sleep(1.5)
    d0 = st(); rows = []; pks = []; cs = []; cpu_kv(); m0 = cpu_kv()["cpu_miss"]
    if a.start: out.send(mido.Message("start"))
    for n in chord: out.send(mido.Message("note_on", channel=ch, note=n, velocity=100))
    if a.glide:
        l.send("glide 1 %d" % a.gtime); time.sleep(0.3); gl = l.read(0.5)
        if not any(x.strip() == "glide 1" for x in gl): print(json.dumps(dict(slot=s, error="glide did not start", reply=gl)), flush=True)
    t0 = time.time(); nxt = t0
    while time.time() - t0 < a.hold:
        d = st(); rows.append(d); c = cpu_kv(); cs.append(c); pks.append(c["cpu_pk_us"])
        nxt += 1.0 if a.glide else 0.2
        time.sleep(max(0.0, nxt - time.time()))
    if a.glide: l.send("glide 0"); time.sleep(0.3); l.read(0.3)
    for n in chord: out.send(mido.Message("note_off", channel=ch, note=n, velocity=0))
    if a.start or a.glide: out.send(mido.Message("stop"))
    m1 = cpu_kv()["cpu_miss"]; time.sleep(3.0); d1 = st()
    cpu = [r.get("cpu_pct", 0) for r in rows]
    bud = cs[0]["cpu_budget_us"]                       # the half's time as the unit reports it (K2), not a fixed 2902
    r = dict(slot=s, glide=int(a.glide), gtime=a.gtime if a.glide else None, loaded=" ".join(got)[-40:], peak_pct=[round(100*x/bud) for x in pks], el_peak_pct=[round(100*c["cpu_el_pk_us"]/bud) for c in cs],
             seen=dict(voices=[max(c.get("voices0", 0) for c in cs), max(c.get("voices1", 0) for c in cs)], fm6_bridge_seconds=[sum(1 for c in cs if c.get("fm6_bridge0", 0)), sum(1 for c in cs if c.get("fm6_bridge1", 0))], valley_seconds=[sum(1 for c in cs if c.get("valley0", 0)), sum(1 for c in cs if c.get("valley1", 0))], motion_seconds=[sum(1 for c in cs if c.get("motion0", 0)), sum(1 for c in cs if c.get("motion1", 0))]), missed=m1 - m0, preset=d0.get("preset"), bpm=d0.get("bpm"), playing=max(x.get("playing", 0) for x in rows), cpu_mean=round(sum(cpu)/len(cpu),1), cpu_max=max(cpu),
             late=d1.get("audio_late",0)-d0.get("audio_late",0), max_us=max(x.get("audio_max_us",0) for x in rows), shed=d1.get("voices_shed",0)-d0.get("voices_shed",0),
             fx_light=d1.get("fx_light_count",0)-d0.get("fx_light_count",0), given_up=d1.get("voices_given_up",0)-d0.get("voices_given_up",0))
    print(json.dumps(r), flush=True); res.append(r)
l.close()
