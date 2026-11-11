#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Self test of soundlab measure / rules / compare on synthetic signals (stdlib only; no renderer needed).
usage: selftest.py PATH_TO_soundlab_measure [WORKDIR]   exit 0 when every check passes"""
import json, math, os, struct, subprocess, sys, tempfile, wave

EXE = sys.argv[1]
WORK = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="sl_selftest_")
HERE = os.path.dirname(os.path.abspath(__file__))
SR = 44100
fails = []


def check(name, ok, info=""):
    print(("ok   " if ok else "FAIL ") + name + ("  " + info if info else ""))
    if not ok:
        fails.append(name)


def write(prefix, l, r, voices, events=(), bpm=120, bars=2, extra=None):
    w = wave.open(prefix + ".wav", "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b"".join(struct.pack("<hh", max(-32768, min(32767, int(round(a * 32768)))), max(-32768, min(32767, int(round(b * 32768))))) for a, b in zip(l, r)))
    w.close()
    j = {"tool": "soundlab", "commit": "selftest", "sample_rate": SR, "bars": bars, "bpm": bpm, "source": {"multi": 0, "name": "SELFTEST", "sound": None},
         "settings": {"keys": "none", "tide": "off", "style": "", "morph": "a", "knobs": []}, "voices": voices, "events": list(events), "blocks": {}}
    if extra:
        j.update(extra)
    json.dump(j, open(prefix + ".json", "w"))


def run(*a):
    r = subprocess.run([EXE] + list(a), capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr


def metrics(prefix):
    return json.load(open(prefix + ".metrics.json"))


os.makedirs(WORK, exist_ok=True)
n = SR * 4
# 1. a mono 100 Hz sine at -12 dBFS, one held note
a = 10 ** (-12 / 20)
s = [a * math.sin(2 * math.pi * 100 * i / SR) for i in range(n)]
p1 = os.path.join(WORK, "sine100")
write(p1, s, s, [{"t0": 0.0, "t1": 3.5, "t_end": 3.9, "part": 1, "note": 43, "vel": 100}], bars=2)
rc, out, err = run("measure", p1)
check("measure runs", rc == 0, err.strip())
m = metrics(p1)
check("true peak of a -12 dBFS sine", abs(m["level"]["true_peak_dbfs"] + 12) < 0.1, str(m["level"]["true_peak_dbfs"]))
check("100 Hz sine sits in 60-150 Hz", m["spectrum"]["bands_pct"]["60_150"] > 99.0)
check("mono signal has side/mid far below -60 dB", m["space"]["side_mid_db"] < -60)
check("one note sounding the whole time", abs(m["time"]["bed"]["both_mean"] - 1.0) < 0.1, str(m["time"]["bed"]["both_mean"]))
check("no clipping", m["level"]["clipped_samples"] == 0)
rc, out, err = run("measure", p1, "--from", "1", "--to", "2")
check("measure with a bar range", rc == 0 and abs(metrics(p1)["range"]["seconds"] - 2.0) < 0.01, err.strip())
# 2. wide noise-free stereo: opposite phase sines have side/mid > 0 dB and correlation -1
p2 = os.path.join(WORK, "anti")
write(p2, s, [-v for v in s], [{"t0": 0.0, "t1": 3.5, "part": 1, "note": 43, "vel": 100}])
run("measure", p2)
m2 = metrics(p2)
check("antiphase: side/mid high, correlation -1", m2["space"]["side_mid_db"] > 40 and m2["space"]["lr_correlation"] < -0.99, str(m2["space"]))
# 3. click test: a step in the changed render against a control
c = [0.05 * math.sin(2 * math.pi * 440 * i / SR) for i in range(n)]
ch = list(c)
step_at = SR * 2
for i in range(step_at, n):
    ch[i] += 0.25
p3, p3c = os.path.join(WORK, "clk"), os.path.join(WORK, "clk_ctl")
ev = [{"t": 2.0, "kind": "fx_time", "sample": step_at}]
write(p3c, c, c, [{"t0": 0.0, "t1": 3.9, "part": 1, "note": 69, "vel": 100}])
write(p3, ch, ch, [{"t0": 0.0, "t1": 3.9, "part": 1, "note": 69, "vel": 100}], events=ev)
rc, out, err = run("measure", p3)
mc = metrics(p3)
st = mc.get("click", {}).get("steps", [{}])[0]
check("click found with the default _ctl control", mc.get("click", {}).get("any_click") is True and st.get("pre_identical") is True, json.dumps(st))
check("click excess is about 20 dB", 15 < st.get("excess_db", 0) < 40, str(st.get("excess_db")))
# 4. rules runs and reports 10 rules
rc, out, err = run("rules", p1)
check("rules prints ten rules", sum(1 for ln in out.splitlines() if ln.startswith("rule ")) == 10, out[-300:])
check("rules.json written", os.path.exists(p1 + ".rules.json"))
# 5. sweep table
rc, out, err = run("sweep-table", os.path.join(WORK, "sw"), "-1:" + p2, "0:" + p1, "1:" + p3)
if rc == 2:
    print("skip sweep table (this exe has no sweep-table subcommand)")
else:
    check("sweep table has a header and the LUFS row", rc == 0 and "| metric |" in out and "| LUFS |" in out, out[:200])
# 6. compare.py
rc = subprocess.run([sys.executable, os.path.join(HERE, "..", "compare.py"), p1 + ".metrics.json", p2 + ".metrics.json"], capture_output=True, text=True)
check("compare.py makes a table", rc.returncode == 0 and "| level.lufs |" in rc.stdout, rc.stderr.strip())
print("%d check(s) failed" % len(fails) if fails else "all checks passed")
sys.exit(1 if fails else 0)
