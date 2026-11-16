#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""travel_watch.py [--port "DUELL"] [--multi 7] [--mode 4] [--time 5] [--secs 240] [--out log.jsonl]: AUTO TIDE on a factory multi, driven and read over USB, no hands on the unit.

Needs a MEASURING build (tools/build.py with env DUELL_MEASURE=1: the editor's MEASURE command, firmware/src/editor_measure.c). It loads the multi (the editor's PROJECT load),
sets AUTO TIDE's MODE (4 TRAVEL, 3 EVOLVE) and TIME (the index of 1 2 4 8 16 32 64 bars: 5 = 32), turns K1 right (PLAY starts with it) and reads once a second: the longest elapsed
audio half of that second against the half's budget (the number the AUTO TIDE header shows, as %), the DMA periods missed and the voices shed since the start. At the end it stops
the glide and PLAY (MIDI Stop). One JSON line a second to --out; the summary on stdout."""
import argparse, json, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import jam_dev as jd                                    # (jam/jam_dev.py: the editor SysEx transport)

ED_PROJECT, ED_MEASURE = 9, 91
BUDGET_US = 2902                                        # (core.h CPU_BUDGET_US: a half of 128 frames at 44.1 kHz)


def read(t):
    a = t.request(ED_MEASURE, [0], 1.0)
    if len(a) < 21 or a[0] != 0:
        raise SystemExit(f"travel_watch: MEASURE READ gave {a!r}: not a measuring build (DUELL_MEASURE=1)?")
    w = [sum(a[1 + 5 * k + i] << (7 * i) for i in range(5)) for k in range(4)]
    st = [a[21 + 2 * k] | a[22 + 2 * k] << 7 for k in range(6)] if len(a) >= 33 else []
    nv = [a[33], a[34], a[35], a[36]] if len(a) >= 37 else []
    return {"st": st, "nv": nv, "el_us": w[0], "miss": w[1], "shed": w[2], "playing": w[3] & 1, "glide": (w[3] >> 1) & 1, "to": (w[3] >> 2) & 1, "ease": w[3] >> 3}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="DUELL"); ap.add_argument("--multi", type=int, default=7); ap.add_argument("--mode", type=int, default=4)
    ap.add_argument("--time", type=int, default=5); ap.add_argument("--secs", type=float, default=240); ap.add_argument("--out", default="")
    ap.add_argument("--preset", default="", help="ENGINE,INDEX: a factory sound loaded on part 1 after the multi (the editor's PRESET), e.g. 8,10 = FM6 SALOON KEYS")
    a = ap.parse_args()
    c = jd.open_midi(a.port)
    t = c.t
    out = open(a.out, "w") if a.out else None
    try:
        r = t.request(ED_PROJECT, [0, a.multi - 1], 3.0)
        if r[:2] != [0, a.multi - 1]:
            raise SystemExit(f"travel_watch: the load of M{a.multi:02d} gave {r!r}")
        time.sleep(2.0)
        if a.preset:
            e, k = (int(x) for x in a.preset.split(","))
            print("preset:", t.request(8, [e, k], 3.0)); time.sleep(1.0)
        s0 = read(t)
        r = t.request(ED_MEASURE, [1, a.time, a.mode], 1.0)
        if r != [1, 1]:
            raise SystemExit(f"travel_watch: the launch gave {r!r}")
        t0 = time.time(); rows = []
        while time.time() - t0 < a.secs:
            time.sleep(1.0)
            s = read(t)
            s["t"] = round(time.time() - t0, 1); s["el_pct"] = round(100.0 * s["el_us"] / BUDGET_US, 1)
            s["miss_d"] = s["miss"] - s0["miss"]; s["shed_d"] = s["shed"] - s0["shed"]
            rows.append(s)
            if out:
                out.write(json.dumps(s) + "\n"); out.flush()
            print(f"t {s['t']:6.1f}  el {s['el_pct']:5.1f} %  missed {s['miss_d']:4d}  shed {s['shed_d']:4d}  ease {s['ease']}  to {s['to']}  glide {s['glide']}" + (f"  stages morph {s['st'][0]} p1 {s['st'][1]} p2wait {s['st'][2]} xblk {s['st'][3]} v1 {s['st'][4]} bud {s['st'][5]} us" if s['st'] else ""), flush=True)
        t.request(ED_MEASURE, [2], 1.0)
        t.out.send(t._mido.Message("stop"))
        el = sorted(x["el_pct"] for x in rows)
        print(f"travel_watch: M{a.multi:02d} mode {a.mode} time {a.time}, {len(rows)} s: busiest second {el[-1]:.1f} %, median {el[len(el) // 2]:.1f} %, "
              f"seconds over 100 % {sum(x > 100 for x in el)}, missed {rows[-1]['miss_d']}, shed {rows[-1]['shed_d']}")
    finally:
        t.close()
        if out:
            out.close()


if __name__ == "__main__":
    main()
