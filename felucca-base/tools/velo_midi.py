#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""Key velocity on the unit, read from its USB MIDI output (audit T17): a guided session in which the owner presses keys hard, normally and softly, the velocity each press sent, and the report.

The unit plays every key it sends as a note-on over USB MIDI (like any played key), so no developer build is needed: the NORMAL firmware, MENU > CONTROL > KEY VELOCITY ON. Notes 0 .. 19 (F3 .. C5)
have a converter under the key (sense.c): their velocity comes from the press. Notes 20 .. 26 (C#5 .. G5) have none and send a fixed 100 (sense_cal.h SC_TOP_VEL) even when KEY VELOCITY is ON:
the session confirms that too. `jam_dev.py measure velocity` is the owner's command; this file is the plan and the analysis (no MIDI in it: a source object gives the notes, so the tests need no unit).

What the report says: for each key played, the velocities of its hard / normal / soft presses (the median and the range), whether they come in the right order, how far apart the hard and the soft
median are (the usable range), whether the keys differ from each other (the per-key range learns, sense_cal.h), the top keys (fixed 100?), and whether KEY VELOCITY looks OFF (everything 100).
It changes nothing; it is the measurement the audit asked for ("pressing a key hard made it very quiet; to be measured")."""
import statistics

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
NOTE0 = 53                                   # F3 = key 0 at OCT 0 (seq.c: 53 + k + 12 octave)
N_ANALOG, N_KEYS = 20, 27
SESSION_KEYS = (0, 4, 7, 11, 14, 19)         # F3, A3, C4, E4, G4, C5 as tools/velo_capture.py: spread over the keyboard (the same six keys)
TOP_KEYS = (20, 23, 26)                      # C#5, E5, G5: no converter
STYLES = (("hard", "HARD", "hit it firmly, like a loud note"),
          ("normal", "NORMAL", "play it the way you normally would"),
          ("soft", "SOFT", "press it gently, as softly as it still sounds"))
PRESSES = 3
TOP_PRESSES = 2
FIXED = 100                                  # SC_TOP_VEL, and what an unmeasured press sends (SC_VEL_UNKNOWN)


def key_name(k):
    m = NOTE0 + k
    return f"{NAMES[m % 12]}{m // 12 - 1}"


def build_plan(keys=SESSION_KEYS, presses=PRESSES, top=TOP_KEYS, top_presses=TOP_PRESSES):
    """[{'key', 'style', 'n', 'text'}]: the analog keys hard / normal / soft, then each top key hard and soft (any strength: it should send 100 both times)"""
    plan = []
    for k in keys:
        for style, word, how in STYLES:
            plan.append({"key": k, "style": style, "n": presses, "text": f"key {key_name(k)} (note {NOTE0 + k}): {word}, {presses} presses: {how}"})
    for k in top:
        for style, word, how in (STYLES[0], STYLES[2]):
            plan.append({"key": k, "style": style, "n": top_presses, "text": f"key {key_name(k)} (note {NOTE0 + k}, no converter): {word}, {top_presses} presses: {how}"})
    return plan


def run_session(source, plan, say, step_timeout=90.0, offset=0):
    """Play the plan through `source` (get(timeout) -> (t, note, velocity) or None). Each step waits for its n presses of the expected note (other notes are kept as strays). Returns the presses
    [{'key', 'style', 'note', 'vel'}] and the stray notes; `offset` shifts the expected notes (OCT not at 0: the session finds it from the first press when it is off by whole octaves)."""
    got, stray = [], []
    for step in plan:
        say(step["text"])
        want = NOTE0 + step["key"] + offset
        n, t_end = 0, None
        while n < step["n"]:
            ev = source.get(step_timeout)
            if ev is None:
                say(f"   no press of note {want} within {step_timeout:.0f} s: this step is left out")
                break
            _, note, vel = ev
            if note == want:
                got.append({"key": step["key"], "style": step["style"], "note": note, "vel": vel})
                n += 1
            elif not got and n == 0 and (note - want) % 12 == 0 and note != want:
                offset += note - want                      # the octave button is not at 0: take the shift from the first press
                want = note
                say(f"   (the keyboard is {offset // 12:+d} octave(s) from the middle: following it)")
                got.append({"key": step["key"], "style": step["style"], "note": note, "vel": vel})
                n += 1
            else:
                stray.append(note)
                say(f"   (note {note} is not the one asked for, {want}; left out)")
    return got, stray


def med(v):
    return statistics.median(v) if v else None


def analyse(presses):
    """the facts of a session: per key and style the velocities; the order of the medians; the spread; the top keys; whether KEY VELOCITY looks off"""
    keys = {}
    for p in presses:
        keys.setdefault(p["key"], {}).setdefault(p["style"], []).append(p["vel"])
    analog = {k: s for k, s in keys.items() if k < N_ANALOG}
    top = {k: s for k, s in keys.items() if k >= N_ANALOG}
    out = {"keys": {}, "top": {}, "n_analog": 0, "n_top": 0}
    all_analog = [v for s in analog.values() for vs in s.values() for v in vs]
    out["n_analog"] = len(all_analog)
    out["looks_off"] = bool(all_analog) and all(v == FIXED for v in all_analog)
    ordered = 0
    for k, s in sorted(analog.items()):
        m = {st: med(s.get(st, [])) for st in ("hard", "normal", "soft")}
        rng = {st: (min(s[st]), max(s[st])) if s.get(st) else None for st in ("hard", "normal", "soft")}
        ok = all(m[x] is not None for x in m) and m["soft"] < m["normal"] < m["hard"]
        ordered += ok
        out["keys"][k] = {"median": m, "range": rng, "ordered": ok, "span": (m["hard"] - m["soft"]) if m["hard"] is not None and m["soft"] is not None else None}
    out["ordered"], out["n_keys"] = ordered, len(analog)
    spans = [v["span"] for v in out["keys"].values() if v["span"] is not None]
    out["span_min"], out["span_max"] = (min(spans), max(spans)) if spans else (None, None)
    allv = [v for s in analog.values() for vs in s.values() for v in vs]
    out["vel_min"], out["vel_max"] = (min(allv), max(allv)) if allv else (None, None)
    for k, s in sorted(top.items()):
        v = [x for vs in s.values() for x in vs]
        out["top"][k] = {"vels": sorted(set(v)), "fixed": all(x == FIXED for x in v)}
        out["n_top"] += len(v)
    out["top_fixed"] = bool(out["top"]) and all(t["fixed"] for t in out["top"].values())
    return out


def report(an, stray=()):
    L = []
    if an["n_analog"] == 0:
        return "No press of an analog key was recorded: nothing to report (KEY VELOCITY ON? the unit's USB MIDI port free?)."
    if an["looks_off"]:
        L.append("EVERY analog key sent velocity 100: KEY VELOCITY looks OFF (MENU > CONTROL > KEY VELOCITY), or the unit measured nothing. Switch it ON and run the session again.")
    L += [f"{an['n_analog']} presses of {an['n_keys']} analog keys; velocities {an['vel_min']} .. {an['vel_max']}; in the right order (soft < normal < hard by the median) on {an['ordered']} of {an['n_keys']} keys; "
          f"the usable range, hard minus soft median: {an['span_min']} .. {an['span_max']}.", "",
          "| key | hard (median, range) | normal | soft | order | span |", "|---|---|---|---|---|---|"]
    for k, v in an["keys"].items():
        def c(st):
            r = v["range"][st]
            return "-" if r is None else f"{v['median'][st]:.0f} ({r[0]}..{r[1]})"
        span = "-" if v["span"] is None else f"{v['span']:.0f}"
        L.append(f"| {key_name(k)} | {c('hard')} | {c('normal')} | {c('soft')} | {'ok' if v['ordered'] else 'NO'} | {span} |")
    if an["top"]:
        L += ["", "The top keys (no converter, notes 20 .. 26): " + "; ".join(f"{key_name(k)} sent {', '.join(map(str, t['vels']))}" for k, t in an["top"].items())
              + (". All 100: confirmed, they stay at a fixed 100 with KEY VELOCITY ON (document it in QUICKSTART)." if an["top_fixed"] else
                 ". NOT all 100: the top keys do something else than the code says; look at sense.c sense_vel."), ]
    if an["span_min"] is not None and an["span_min"] < 20 and not an["looks_off"]:
        L += ["", f"The range from soft to hard is narrow on some key ({an['span_min']}): the curve or the per-key range (sense_cal.h SC_FLOOR_PRIOR, SC_CEIL_PRIOR, the learning) needs a look."]
    if an["ordered"] < an["n_keys"] and not an["looks_off"]:
        L += ["", "Some keys do not come in the order soft < normal < hard: the presses were not told apart on them (an unsteady hand, or the key's range)."]
    if stray:
        L += ["", f"{len(stray)} presses of other notes were left out."]
    return "\n".join(L) + "\n"
