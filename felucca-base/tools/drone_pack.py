#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL-TIDE built-in drone multis (TIDE 1.0.0-b6): makes the records of M17 DUNE HORN and M18 AMB GROWL (VOID DEEP / AMB VOID until b9) into tools/drone_multis/M17.bin, M18.bin.

    python3 tools/drone_pack.py        (needs build/gen: python3 ../gen_only.py; a C compiler)

M17 and M18 are ROM-only multis past the sixteen stored slots (firmware/src/multi.c MU_SHOWN): tools/gen_duell_multis.py puts these two records into
build/gen/duell_multis.h with DF_SLOT 16 and 17, next to the factory multis M01..M16, and checks them against the rows below. Each is a LAYER of two
latched drones (HOLD ALL) of the W15 drone sounds (tools/duell_sounds.py), one of them on a slow ARP SEQ pattern (b6, DRONES below), a 60 BPM ping-pong echo and a big hall. TIDE plays them as STYLE
DRONE (tide.c). The records are made by the same code as the factory multis (tools/factory_multis.py's table machinery, tests/gen_factory_multis.c),
built in scratch slots M01, M02 of a host build (a record does not hold its slot) and copied here. (b4 shipped eight of these as a file pack,
packs/drones/; b6 builds two in and drops the pack, owner 9 November 2026.)"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import factory_multis as fm  # noqa: E402

G = dict(G_BPM=60, G_DTIME="4D", G_DFDBK=60, G_DMIX=64, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=110, G_RDAMP=57)
OUT = fm.ROOT / "tools" / "drone_multis"


def multi(rom, slot, name, what, a, b, la=98, lb=82, seq=None, sets=None):
    """seq: (part 0 / 1, ARP RATE, steps), or a list of them: that part plays the SEQUENCER arp (ARP SEQ) on the latched keys, GATE 127; a part without one stays a latched drone (HOLD ALL, arp OFF)"""
    hold = dict(P_AMODE="OFF", P_AHOLD="ALL")
    parts = [dict(sound=a, set=dict(hold, P_LEVEL=la)), dict(sound=b, set=dict(hold, P_LEVEL=lb))]
    for p, st in enumerate(sets or ()):                  # (b9 rework2: values a part sets on top of its sound, as a factory multi's `set`)
        parts[p]["set"].update(st)
    for p, rate, steps in ([seq] if isinstance(seq, tuple) else seq or []):
        parts[p]["set"].update(P_AMODE="SEQ", P_ARATE=rate, P_AGATE=127)
        parts[p]["steps"] = steps
    return dict(rom=rom, slot=slot, name=name, mode="LAYER", what=what, g=dict(G), parts=parts)


# rom: the multi's place (M17, M18); slot: the scratch slot of the host build.
# The sequences (owner, 9 November 2026: "I'm missing some sequence from the drone multis"): one part of each moves slowly on the held chord (every
# held note by the step's offset, at the held keys' velocity: an ARP SEQ step's vNN is not used), the other stays the latched drone under it, no
# note under the held chord. At 60 BPM a 1/4 step is a second. The intervals are the ones that keep the moving part at its latched level (host
# render, the part alone, against the root): the horn's fifth is +5 dB and the wind's fifth +6, octave +4.5, so neither is used.
# DUNE HORN: the horn (it speaks at once) swells once a bar, root, root, fourth, root: three beats on and a beat to breathe, a 4-bar loop.
# b9 rework2 (owner, 11 November 2026: "I would like at least one deep growling drone"; research work/b9/research/drone.md): M18 AMB GROWL replaces VOID DEEP.
# The bed is REESE BASS (two detuned saws beating slowly, its own sub an octave under) on C2 with the sub on C1 with no key held (an ARP SEQ step two octaves under TIDE's
# stand-in C4 = MIDI 60: the stand-in ignores a sound's TRN, a held key does not, so TRN 0), PRIO LOW (b9 rework3: a held chord plays its lowest key; the chord the docs
# ask for, "around C3" on the unit's keys at octave 0, is MIDI 60 64 67, the stand-in's octave: it sounds where PLAY alone does), overdriven a little, its low-pass on a slow LFO,
# no PUMP duck; above it NIGHT DRONE turns C3, E flat 3, G3 (a dark minor colour, never a semitone against the bed). Was, b6-b9 rc2:
# VOID DEEP: the solar wind (it takes about 2 s to come up) on 2-second steps over an 8-bar loop: the root for 14 s, a whole tone up for 8 s
# (the held C E G under D F# A: an open, lydian colour), the fourth for 6 s, each followed by a breath: the wind turns three times in 32 s.
# b7 (musical review MR2): the VOID SHIMMER bed is an ARP SEQ part with one step tied over the 16, 1/4 each (4 bars; 16 s at 60 BPM), so TIDE's own chord plays it
# with no key held, and a held chord still sets the key. The moving parts' rates are doubled in length (1/4 -> 1/2, 1/2 -> 1/1): under a 120 BPM clock they move as made at 60.
BED_STEPS = "0 - - - - - - - - - - - - - - -"
DRONES = [
    multi(17, 1, "AMB DUNEHORN", "a horn swelling once a bar, root and fourth, with a shimmer of void above it", "HORN SWELL", "VOID SHIMMER", 107, 91,
          seq=[(0, "1/2", "0 - - . 0 - - . 5 - - . 0 - - ."), (1, "1/4", BED_STEPS)]),
    multi(18, 2, "AMB GROWL", "a deep growling drone: detuned low saws on C2 with their sub on C1, beating slowly, a little overdrive, the low-pass moving slowly; above it a dark haze turning C3, E flat 3, G3", "REESE BASS", "NIGHT DRONE", 106, 92,
          seq=[(1, "1/1", "-12 - - - - - - . -9 - - - -5 - - ."), (0, "1/4", "-24" + BED_STEPS[1:])],
          sets=[dict(P_TRANS=0, P_PRIO=1, P_DKDEP=0, P_DIST=45, P_FREEA=66, P_LD_FLT=20, P_EQHG=-6, P_CHOR=40, P_DLY=30, P_REV=70), dict(P_TRANS=0, P_EQHG=-6, P_CHOR=70, P_SPRD=64, P_DLY=40, P_REV=90)]),
]


def main():
    os.chdir(fm.ROOT)
    fm.MULTIS = DRONES
    snd = fm.sounds()
    fm.resolve(snd)
    fm.emit(fm.GEN / "factory_multis_def.h")
    (fm.GEN / "duell_multis.h").write_text(fm.BOOT_H)
    exe = fm.ROOT / "build" / "host" / "gen_factory_multis"
    exe.parent.mkdir(parents=True, exist_ok=True)
    cc = ["cc", "-O1", "-w", "-DDUELL_TEST_MODE=1", "-I" + str(fm.GEN), "-Ibuild/gen", "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="x"',
          "-o", str(exe), "tests/gen_factory_multis.c", "-lm"]
    if subprocess.run(cc).returncode:
        sys.exit("drone_pack: tests/gen_factory_multis.c does not compile (python3 ../gen_only.py first)")
    scratch = fm.ROOT / "build" / "drone_pack"
    scratch.mkdir(parents=True, exist_ok=True)
    if subprocess.run([str(exe), "build", str(scratch)]).returncode:
        sys.exit("drone_pack: the build failed")
    OUT.mkdir(parents=True, exist_ok=True)
    for m in DRONES:
        shutil.copyfile(scratch / ("M%02d.bin" % m["slot"]), OUT / ("M%02d.bin" % m["rom"]))
        print("M%02d %s -> %s" % (m["rom"], m["name"], OUT / ("M%02d.bin" % m["rom"])))


if __name__ == "__main__":
    main()
