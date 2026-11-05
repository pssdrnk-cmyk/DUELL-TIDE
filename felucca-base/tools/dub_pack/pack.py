#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL dub techno / house pack (content only, no firmware change): 8 voice patches and 4 pattern patches.

    tools/dub_pack/pack.py patterns OUT.json  the patterns as step objects (assemble.mjs encodes them)
    tools/dub_pack/pack.py spec OUT_SPEC      resolve the sounds below (tools/gen_duell_sounds.py's own label checks, ranges and End B encoder)
                                              into the line file tools/dub_pack/render.c reads
    tools/dub_pack/pack.py list               the pack, one line each

The sounds use the form of tools/duell_sounds.py (p = values off INIT by label, bend = End B values that differ from the Original). The
records themselves are written by the firmware's own store (render.c: up_store_trk), never by hand; tools/dub_pack/assemble.mjs turns them
into the editor's "duell-patches" files and checks them with the editor's own reader. tools/dub_pack/build.sh runs the three.

Since the 8 voices joined the factory bank (places #129 on), tools/duell_sounds.py is their one source: all_sounds() takes each voice's category, values
and End B from there by name. The p / bend written in SOUNDS below are the pack as first auditioned (2026-11-03), kept as history; the bank's
TRIMs, DUST BED's filter and End B and SUB PLUCK's SAT have moved on since.

The 4 PATTERNS are pattern slots (the editor's "duell-patterns" file): 16 steps of absolute chord notes, with ties, accents and chances.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import gen_duell_sounds as g  # noqa: E402

CATS = [""] + g.CATS                      # category.c: 0 none, BASS .. OTHER

# the keys held while a sound is auditioned (render.c): a minor 7th chord, an open minor 9th, one low note
CHORD_M7 = [48, 51, 55, 58]
CHORD_OPEN = [48, 55, 58, 63]
LOW_C = [36]

# kind -> (keys, hold ms, tail ms, bpm) of the audition
KIND = {
    "stab": (CHORD_M7, 700, 3600, 122),
    "pad": (CHORD_OPEN, 6500, 4500, 120),
    "pluck": (LOW_C, 450, 3200, 122),
    "bed": (CHORD_OPEN[:2], 8000, 4500, 120),
}

SOUNDS = [
    # ---------------------------------------------------------------- chord stabs
    dict(engine="ANALOG", name="DUB CHORD", cat="KEYS", kind="stab", bamt=20,
         # a saw chord through the 24 dB multimode filter, the filter envelope snapping it open and closed, chorus + a long delay + a room behind it.
         # End B: the same chord thrown into the echo: darker, more resonant, the envelope's snap gone, the delay and the reverb up.
         p={"ATK": 3, "DEC": 72, "SUS": 34, "REL": 70, "LRATE": 30, "LWAVE": 1, "LD_FLT": 3, "KTRK": 20, "CHOR": 68, "DLY": 70, "REV": 48,
            "FREEA": 38, "FREEB": 44, "FREED": 2, "E2ATK": 0, "E2DEC": 68, "E2SUS": 0, "E2REL": 66, "E2FLT": 26, "TRIM": 100,
            "E2": 24, "E7": 18, "MOD1": ("VEL", "CUT", 14), "LTRIG": 1, "L2TRIG": 1},
         bend={"FREEA": 26, "FREEB": 66, "E2FLT": 6, "DLY": 108, "REV": 84, "CHOR": 94, "REL": 88, "DEC": 80, "SUS": 40}),
    dict(engine="ANALOG", name="DARK MIN7", cat="KEYS", kind="stab", bamt=20,
         # a square chord, closed filter, a little drive and tape wobble: the dark minor seventh that sits under a hat.
         # End B: the filter opens, the drive comes up, the delay and the reverb deepen (a lift into the break).
         p={"ATK": 10, "DEC": 78, "SUS": 26, "REL": 80, "LRATE": 24, "LWAVE": 1, "LD_FLT": 4, "KTRK": 16, "CHOR": 36, "DLY": 60, "REV": 62,
            "DIST": 22, "FREEA": 30, "FREEB": 56, "FREED": 2, "E2DEC": 74, "E2SUS": 0, "E2REL": 72, "E2FLT": 18,
            "ITYPE": 9, "IA": 50, "IB": 71, "IC": 110, "TRIM": 98, "E1": 1, "E2": 16, "E7": 30,
            "MOD1": ("VEL", "CUT", 12), "LTRIG": 1, "L2TRIG": 1},
         bend={"FREEA": 46, "FREEB": 34, "DIST": 54, "DLY": 86, "REV": 96, "E2FLT": 30, "DEC": 84}),
    dict(engine="WHEEL", name="ORGAN SKANK", cat="KEYS", kind="stab", bamt=20,
         # a drawbar organ with the second-harmonic percussion and key click, snapped by the filter envelope, slow rotor, a dub delay.
         # End B: a mellower registration, the filter closed, the rotor FAST and the delay and the reverb up.
         p={"ATK": 0, "DEC": 62, "SUS": 52, "REL": 50, "KTRK": 20, "CHOR": 14, "DLY": 64, "REV": 36, "FREEA": 56, "FREEB": 26, "FREED": 2,
            "E2ATK": 0, "E2DEC": 60, "E2SUS": 0, "E2REL": 56, "E2FLT": 22, "TRIM": 104, "E1": 4, "E5": 1, "E6": 55, "E7": 18, "E8": 1,
            "MOD1": ("VEL", "CUT", 12), "LTRIG": 1, "L2TRIG": 1},
         bend={"FREEA": 36, "E2FLT": 6, "E1": 1, "E8": 2, "DLY": 96, "REV": 70, "E7": 40}),
    # ---------------------------------------------------------------- pads
    dict(engine="ANALOG", name="WARM WASH", cat="PAD", kind="pad", bamt=20,
         # detuned saws, a slow swell, a low-pass that breathes with a slow LFO, a thick chorus and a long room.
         # End B: the filter opens, the detune doubles, the chorus and the room fill the space.
         p={"ATK": 84, "DEC": 100, "SUS": 127, "REL": 100, "LRATE": 20, "LWAVE": 1, "LD_FLT": 5, "KTRK": 24, "CHOR": 92, "DLY": 22, "REV": 76,
            "EQLG": -2, "FREEA": 44, "FREEB": 12, "FREED": 2, "E2ATK": 90, "E2DEC": 100, "E2SUS": 96, "E2REL": 96, "L2RATE": 14, "L2WAVE": 1,
            "E2FLT": 12, "TRIM": 92, "E2": 34, "E4": 3, "E7": 12, "MOD1": ("VEL", "CUT", 8), "MOD2": ("LFO2", "CUT", 4),
            "LTRIG": 1, "L2TRIG": 1},
         bend={"FREEA": 58, "E2": 60, "CHOR": 112, "REV": 100, "DLY": 58, "ATK": 96, "E2FLT": 22}),
    dict(engine="PHASE", name="AIR SHIMMER", cat="PAD", kind="pad", bamt=20,
         # two phase-distortion voices (double sine and resonant triangle), the DCW swept slowly by an LFO and the envelope, high-shelf lift,
         # wide stereo spread, a very long room and a late echo.
         # End B: the DCW opens to a brighter, glassier tone with more detune, the spread and the room at the top.
         p={"ATK": 78, "DEC": 100, "SUS": 127, "REL": 104, "LRATE": 18, "LWAVE": 1, "LD_SHP": 5, "KTRK": 30, "CHOR": 84, "DLY": 46, "REV": 100,
            "FREEA": 68, "FREEB": 8, "FREED": 1, "E2ATK": 84, "E2DEC": 100, "E2SUS": 80, "E2REL": 98, "L2RATE": 21, "L2WAVE": 1,
            "EQHG": 4, "SPRD": 70, "TRIM": 86, "E1": 3, "E2": 7, "E3": 50, "E4": 44, "E5": 24,
            "MOD1": ("LFO2", "E3", 10), "MOD2": ("ENV2", "E3", 8), "MOD3": ("VEL", "CUT", 6), "LTRIG": 1, "L2TRIG": 1},
         bend={"E3": 88, "E5": 52, "REV": 127, "CHOR": 110, "FREEA": 74, "DLY": 70, "ATK": 92, "SPRD": 100}),
    dict(engine="ANALOG", name="SLOW TIDE", cat="PAD", kind="pad", bamt=25,
         # a pulse-width pad that never sits still: a very slow LFO, a slow second LFO and the cycle envelope (about 20 s) move the filters
         # and the oscillator mix against each other; a band-pass twin filter on top. Hold a chord and leave it.
         # End B: the same motion, faster and the other way round, the filter lower, the detune up, the room deeper.
         p={"ATK": 100, "DEC": 100, "SUS": 127, "REL": 108, "LRATE": 5, "LWAVE": 1, "LD_FLT": 10, "KTRK": 22, "CHOR": 66, "DLY": 50, "REV": 96,
            "RES2": 20, "TWTYPE": 1, "FREEA": 52, "FREEB": 26, "FREED": 2, "E2ATK": 104, "E2DEC": 110, "E2SUS": 70, "E2REL": 100,
            "L2RATE": 9, "L2WAVE": 1, "E2FLT": 20, "CUT2": 6, "TRIM": 110, "E1": 4, "E2": 40, "E3": 70, "E4": 6,
            "MOD1": ("CYCLE", "CUT", 16), "MOD2": ("LFO2", "E3", 24), "MOD3": ("LFO1", "CUT2", 10), "MOD4": ("VEL", "CUT", 6),
            "CYCLE RISE": 127, "CYCLE FALL": 127, "LTRIG": 1, "L2TRIG": 1},
         bend={"FREEA": 40, "LRATE": 16, "L2RATE": 22, "M1AMT": -16, "E2": 70, "REV": 110, "CHOR": 90, "E2FLT": 6}),
    # ---------------------------------------------------------------- pluck, texture
    dict(engine="ANALOG", name="SUB PLUCK", cat="PLUCK", kind="pluck", bamt=20,
         # a sine pluck with the sub oscillator an octave below, mono legato with a short glide, a filter that pops and closes; play it low.
         # End B: the pluck rings longer, the filter and the sub open, the echo comes up (a held bass note in the break).
         p={"ATK": 0, "DEC": 68, "SUS": 0, "REL": 58, "KTRK": 22, "DIST": 16, "DLY": 36, "REV": 16, "FREEA": 34, "FREEB": 28, "FREED": 2,
            "E2ATK": 0, "E2DEC": 62, "E2SUS": 0, "E2REL": 56, "E2FLT": 30, "VOICE": 2, "GLIDE": 14, "GLMODE": 1, "TRIM": 124,
            "E1": 3, "E2": 6, "E7": 20, "SUB LEVEL": 96, "SUB OCTAVE": 1, "SUB WAVE": 0,
            "MOD1": ("VEL", "CUT", 14), "LTRIG": 1, "L2TRIG": 1},
         bend={"DEC": 86, "FREEA": 48, "DLY": 66, "E2FLT": 12, "SUB LEVEL": 112, "REL": 74}),
    dict(engine="NOISE", name="DUST BED", cat="FX", kind="bed", bamt=25,
         # crackle (DUST) through a band-pass that drifts, vinyl insert, a long room and an echo: a bed to lay under the chords. Any key starts it.
         # End B: denser and darker, the crush up, the room fuller.
         p={"ATK": 100, "DEC": 100, "SUS": 127, "REL": 104, "LRATE": 12, "LWAVE": 1, "LD_FLT": 8, "KTRK": 0, "CHOR": 48, "DLY": 56, "REV": 94,
            "RES2": 20, "TWTYPE": 2, "FREEA": 56, "FREEB": 30, "FREED": 1, "ITYPE": 10, "IA": 60, "IB": 80, "IC": 70, "CUT2": 24, "TRIM": 127,
            "E1": 1, "E2": 58, "E6": 100, "E7": 36, "LTRIG": 1, "L2TRIG": 1},
         bend={"E2": 30, "E6": 127, "E8": 30, "FREEA": 44, "REV": 110, "LRATE": 30}),
]

# Pattern slots (the editor's "duell-patterns" files; the unit's pattern slots Pnn): 16 steps, absolute notes (up to 4 a step), ties, accents, chances.
# Each is auditioned on the voice it is made for ("voice"): the pattern plays by itself on PLAY, in plain DUELL too, whatever key is held.
# A step is (index, [notes], length in steps, accent, chance %). The notes are C minor: Cm7 48 51 55 58, Abmaj7 56 60 63 67, Fm7 53 56 60 63, Gm7 55 58 62 65.
CM7, CM9, ABM7, FM7, GM7 = [48, 51, 55, 58], [51, 55, 58, 62], [56, 60, 63, 67], [53, 56, 60, 63], [55, 58, 62, 65]
PATTERNS = [
    dict(name="DUB OFFBEAT", voice="DUB CHORD", bpm=122, div=2, swing=54, gate=42, len=16,
         steps=[(2, CM7, 1, 0, 100), (6, CM7, 1, 1, 100), (10, CM7, 1, 0, 100), (14, CM7, 1, 1, 100)]),
    dict(name="DOT DELAY", voice="DARK MIN7", bpm=120, div=2, swing=52, gate=36, len=16,
         steps=[(0, CM7, 1, 1, 100), (3, CM7, 1, 0, 100), (7, ABM7, 1, 1, 100), (10, FM7, 1, 0, 100), (14, GM7, 1, 0, 70)]),
    dict(name="HOUSE STAB", voice="ORGAN SKANK", bpm=124, div=2, swing=56, gate=30, len=16,
         steps=[(2, CM9, 1, 0, 100), (3, CM9, 1, 0, 80), (6, CM9, 1, 1, 100), (10, CM9, 1, 0, 100), (11, CM9, 1, 0, 80), (14, CM9, 1, 1, 100)]),
    dict(name="PAD BREATH", voice="WARM WASH", bpm=118, div=2, swing=50, gate=100, len=16,
         steps=[(0, [48, 55, 58, 62], 7, 1, 100), (8, [44, 51, 55, 60], 7, 1, 100)]),
]


def pattern_json():
    """the patterns as the editor's step objects (index, n, notes, time 0 NOTE / 1 TIE / 2 REST, flags 1 ACCENT, vel, chance, ratchet, nudge)"""
    out = []
    for p in PATTERNS:
        steps = [dict(index=i, n=0, notes=[0, 0, 0, 0], time=2, flags=0, vel=0, chance=100, ratchet=1, nudge=8) for i in range(64)]
        for i, notes, ln, acc, chance in p["steps"]:
            steps[i] = dict(index=i, n=len(notes), notes=notes + [0] * (4 - len(notes)), time=0, flags=1 if acc else 0, vel=100, chance=chance, ratchet=1, nudge=8)
            for k in range(1, ln):
                steps[i + k] = dict(index=i + k, n=0, notes=[0, 0, 0, 0], time=1, flags=0, vel=0, chance=100, ratchet=1, nudge=8)
        out.append(dict(name=p["name"], len=p["len"], div=p["div"], swing=p["swing"], gate=p["gate"], qnt=1, steps=steps))
    return out


def factory_sounds():
    """name -> the factory record of tools/duell_sounds.py (the one source of the 8 voices since they joined the bank as #129-#160)"""
    spec = importlib.util.spec_from_file_location("duell_sounds", HERE.parent / "duell_sounds.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return {s["name"]: s for s in mod.SOUNDS}


def all_sounds():
    """-> the 8 voice dicts in pack order: the audition (kind) from here, the sound itself (cat, p, bend, bamt) from tools/duell_sounds.py"""
    fac = factory_sounds()
    out = []
    for s in SOUNDS:
        f = fac[s["name"]]
        out.append(dict(s, cat=f["cat"], p=dict(f["p"]), bend=dict(f.get("bend", {})), bamt=f.get("bamt", s.get("bamt"))))
    seen = {}
    for s in out:                                          # factory idx within an engine: any distinct numbers (the sound list is not written)
        s["idx"] = seen.get(s["engine"], 0)
        seen[s["engine"]] = s["idx"] + 1
    return out


def resolve():
    snd = all_sounds()
    data = types.SimpleNamespace(NO_OWNER=True, SOUNDS=[{k: v for k, v in s.items() if k in ("engine", "idx", "name", "cat", "p", "bend", "bamt")} for s in snd])
    built = g.build(data)[0]
    by = {b["name"]: b for b in built}
    return snd, by


def write_spec(path):
    snd, by = resolve()
    lines = []
    for s in snd:
        b = by[s["name"]]
        keys, hold, tail, bpm = KIND[s["kind"]]
        lines.append(f"S {b['e']} {CATS.index(s['cat'])} {bpm} {hold} {tail} {s['name']}")
        for pid, v in b["pairs"]:
            lines.append(f"P {pid} {v}")
        if b["endb"]:
            blk = bytes(b["endb"])
            lines.append(f"E {len(blk)} " + " ".join(f"{x:02x}" for x in blk))
        lines.append("K %d %s" % (len(keys), " ".join(map(str, keys))))
    names = [s["name"] for s in snd]
    for p, pj in zip(PATTERNS, pattern_json()):             # R: a pattern slot played (PLAY) on the stored voice base
        lines.append(f"R {names.index(p['voice'])} {p['bpm']} 8000 3600 {p['name']}")
        lines.append(f"Y {p['len']} {p['div']} {p['swing']} {p['gate']}")
        for st in pj["steps"][:p["len"]]:
            if st["time"] != 2:
                lines.append(f"T {st['index']} {st['time']} {st['flags']} {st['vel']} {st['chance']} {st['n']} " + " ".join(map(str, st["notes"])))
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return len(snd) + len(PATTERNS)


def main(argv):
    if len(argv) >= 2 and argv[0] == "patterns":
        Path(argv[1]).write_text(json.dumps(pattern_json()), encoding="utf-8", newline="\n")
        return 0
    if len(argv) >= 2 and argv[0] == "spec":
        print(write_spec(argv[1]), "sounds ->", argv[1])
        return 0
    if argv[:1] == ["list"]:
        for i, s in enumerate(all_sounds(), 1):
            print(i, s["name"], s["engine"], s["cat"], s["kind"])
        for p in PATTERNS:
            print("P", p["name"], p["voice"], p["bpm"])
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
