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


# b10 (owner, 11 November 2026: more factory multis: "hypnotic techno, cinematic hypnotic techno, ..."; recipes work/b9/research/attack/, MAPPING.md
# "Systematic round"): ROM-only factory multis past the drones, M19.., built here like M17 / M18 (scratch slots 3..), each a genre the sixteen do not have.
# Unlike the drones TIDE does not force a STYLE on them (tide_ui.c tide_drone_style: M17, M18 only).
MORE = [
    dict(rom=19, slot=3, name="TECH HYPNO", mode="LAYER",
         what="hypnotic techno, F sharp minor (131 BPM, straight): a low dark bass stab, F sharp 1 with an octave pop, every third 16th so it turns against the bar (a 12-step loop), its filter snapping shut, a little soft clip, into a ping-pong echo; above it a dark FM pad holding F sharp minor 7 open, its filter drifting at 0.1 Hz",
         g=dict(G_BPM=131, G_DTIME="8D", G_DFDBK=50, G_DMIX=70, G_DMODE="PING", G_DCOLOR=30, G_RTYPE="HALL", G_RSIZE=96, G_RDAMP=70),
         parts=[dict(sound="HARM BASS", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=70, P_AHOLD="OFF", P_CHRD="OFF", P_TRANS=0, P_FREEA=34, P_FREEB=22, P_E2FLT=50, P_DIST=30,
                                                  P_DLY=40, P_REV=24, P_TRIM=127, P_LEVEL=104),
                     steps="-30A . . -30 . . -30 . . -18 . .", len=12, poly=True, bseed=4325, amt=30, b={"P_FREEA": 40}),
                dict(sound="FM HAZE", set=dict(P_AMODE="SEQ", P_ARATE="4BAR", P_AGATE=127, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="OPEN", P_FREEA=44, P_LRATE=14, P_LD_FLT=16,
                                                P_CHOR=60, P_SPRD=60, P_DLY=30, P_REV=60, P_LEVEL=92),
                     steps="-6 -", len=2, bseed=27687, amt=30, b={"P_FREEA": 50})]),
    dict(rom=20, slot=4, name="DUB HOUSE", mode="LAYER",
         what="dub house, E minor (122 BPM): low-passed minor-ninth and major-seventh chords (E minor 9, G major 7, D minor 9, F major 7, a bar each, E3-F sharp 4), a stab and a long chord on the syncopated spots, through vinyl crackle into a long dark dotted echo; a held sub bass under each chord's root, never on every beat",
         g=dict(G_BPM=122, G_DTIME="8D", G_DFDBK=96, G_DMIX=78, G_DMODE="DIGI", G_DCOLOR=28, G_DMDEP=20, G_RTYPE="PLATE", G_RSIZE=70, G_RDAMP=66),
         parts=[dict(sound="DARK MIN7", set={"P_AMODE": "OFF", "P_SGATE": 110, "P_FREEA": 32, "P_FREEB": 40, "P_E2FLT": 40, "P_DIST": 18, "P_ITYPE": "VINYL", "P_IA": 20, "P_IB": 30, "P_IC": 10, "P_IMIX": 70,
                                               "P_CHOR": 60, "P_SPRD": 50, "P_DLY": 72, "P_REV": 30, "P_LEVEL": 100},
                     steps="n52+55+62+66v104 . . n52+55+62+66v92 - - - . . . n52+55+62+66v80 . . n52+55+62+66v96 . . n55+59+62+66v104 . . n55+59+62+66v92 - - - . . . n55+59+62+66v80 . . n55+59+62+66v96 . . n50+53+60+64v104 . . n50+53+60+64v92 - - - . . . n50+53+60+64v80 . . n50+53+60+64v96 . . n53+57+60+64v104 . . n53+57+60+64v92 - - - . . . n53+57+60+64v80 . . n53+57+60+64v96 . .", len=64, bseed=56484, amt=30, b={"P_FREEA": 38}),
                dict(sound="PEDAL BASS", set={"P_AMODE": "OFF", "P_SGATE": 100, "P_TRANS": 0, "P_DLY": 0, "P_REV": 6, "P_TRIM": 115, "P_LEVEL": 100},
                     steps="n40v110 - . . . . n40v92 - . . . . . . n40v100 . n43v110 - . . . . n43v92 - . . . . . . n43v100 . n38v110 - . . . . n38v92 - . . . . . . n38v100 . n41v110 - . . . . n41v92 - . . . . . . n41v100 .", len=64, bseed=4325, amt=30, b={})]),
    dict(rom=21, slot=5, name="HSE LOFI", mode="LAYER",
         what="lo-fi house, D sharp minor (118 BPM, swing 70 %): a soft electric piano through worn tape, two seventh chords inside one octave a bar (D sharp minor 7 and G sharp minor 7, the eleventh in place of the fifth, the seventh at the bottom), a long dark hall and a dotted echo; a round bass on the off-beats, offset from the chords",
         g=dict(G_BPM=118, G_DTIME="8D", G_DFDBK=50, G_DMIX=64, G_DMODE="DIGI", G_DCOLOR=24, G_RTYPE="HALL", G_RSIZE=96, G_RDAMP=80),
         parts=[dict(sound="SOFT EP", set={"P_AMODE": "OFF", "P_SGATE": 110, "P_SSWING": 70, "P_ITYPE": "TAPE", "P_IA": 50, "P_IB": 60, "P_IC": 70, "P_IMIX": 80, "P_FREEA": 50,
                                             "P_CHOR": 60, "P_SPRD": 50, "P_DLY": 34, "P_REV": 60, "P_LEVEL": 104},
                     steps="n61+63+66+68v100 - - n61+63+66+68v80 - . n54+56+59+61v100 - - - n54+56+59+61v84 - . n61+63+66+68v72 . n54+56+59+61v92", len=16, bseed=25255, amt=30, b={"P_FREEA": 56}),
                dict(sound="FM BASS 2", set={"P_AMODE": "OFF", "P_SGATE": 90, "P_SSWING": 70, "P_TRANS": 0, "P_DKDEP": 0, "P_DLY": 0, "P_REV": 8, "P_TRIM": 115, "P_LEVEL": 107},
                     steps=". . . n39v104 . . . . . n44v96 . . . . n39v90 .", len=16, bseed=4325, amt=30, b={})]),
    dict(rom=22, slot=6, name="TECH CINEMA", mode="LAYER",
         what="cinematic hypnotic techno, A minor (131 BPM): a slow-swelling string pad, rootless A minor 9 two bars, then F major 7 and G add 9, each with its fifth dropped an octave and the ninth or the third on top (E3 to B4), through a deep slow phaser into a big hall; under it a breathy pad holding A1 through every chord",
         g=dict(G_BPM=131, G_DTIME="8D", G_DFDBK=60, G_DMIX=64, G_DMODE="PING", G_DCOLOR=28, G_RTYPE="HALL", G_RSIZE=118, G_RDAMP=70),
         parts=[dict(sound="VELVET BOW", set={"P_AMODE": "OFF", "P_SDIV": "1/1", "P_SGATE": 127, "P_TRANS": 0, "P_ATK": 84, "P_REL": 96, "P_FREEA": 40, "P_LRATE": 12, "P_LD_FLT": 10,
                                                "P_EQHG": -4, "P_EQHF": -10,
                                                "P_ITYPE": "PHASER", "P_IA": 18, "P_IB": 100, "P_IC": 40, "P_IMIX": 60, "P_CHOR": 70, "P_SPRD": 60, "P_DLY": 30, "P_REV": 80, "P_TRIM": 94, "P_LEVEL": 100},
                     steps="n52+55+60+71v100 - n48+53+64+69v96 n50+55+59+69v96", len=4, bseed=34517, amt=30, b={"P_FREEA": 48}),
                dict(sound="BREATH PAD", set={"P_AMODE": "SEQ", "P_ARATE": "4BAR", "P_AGATE": 127, "P_AHOLD": "OFF", "P_CHRD": "OFF", "P_TRANS": 0, "P_FREEA": 44, "P_CHOR": 60, "P_SPRD": 64, "P_DLY": 10, "P_REV": 60, "P_TRIM": 121, "P_LEVEL": 100},
                     steps="-27 -", len=2, bseed=60243, amt=30, b={"P_FREEA": 50})]),
    dict(rom=23, slot=7, name="AMB TECHNO", mode="LAYER",
         what="ambient techno, C major (120 BPM): a chorused saw pad in four parallel major-seventh chords a bar each (F, G, C, D: F3 to F sharp 4), and a soft harp arpeggio on each chord's root, tenth and fifth in eighths, through a dotted-quarter (750 ms) ping-pong echo with a drifting time and a big damped hall",
         g=dict(G_BPM=120, G_DTIME="4D", G_DFDBK=52, G_DMIX=70, G_DMODE="PING", G_DCOLOR=30, G_DMDEP=30, G_RTYPE="HALL", G_RSIZE=120, G_RDAMP=64),
         parts=[dict(sound="SILK CHORUS", set={"P_AMODE": "SEQ", "P_ARATE": "1/1", "P_AGATE": 127, "P_AHOLD": "OFF", "P_CHRD": "MAJ7", "P_VOIC": "CLOSE", "P_REL": 60, "P_FREEA": 44, "P_LRATE": 10, "P_LD_FLT": 8,
                                                 "P_CHOR": 100, "P_SPRD": 60, "P_DLY": 30, "P_REV": 70, "P_TRIM": 92, "P_LEVEL": 92},
                     steps="-7 -5 -12 -10", len=4, bseed=33289, amt=30, b={"P_FREEA": 52}),
                dict(sound="GHOST HARP", set={"P_AMODE": "SEQ", "P_ARATE": "1/8", "P_AGATE": 90, "P_AHOLD": "OFF", "P_CHRD": "OFF", "P_TRANS": 0, "P_REL": 50, "P_FREEA": 54,
                                                "P_CHOR": 40, "P_SPRD": 60, "P_DLY": 70, "P_REV": 60, "P_TRIM": 100, "P_LEVEL": 100},
                     steps="-7A 9 0 . 9 0 9 . -5A 11 2 . 11 2 . . -12A 4 -5 . 4 -5 4 . -10A 6 -3 . 6 -3 . .", len=32, bseed=10920, amt=30, b={"P_FREEA": 60})]),
    dict(rom=24, slot=8, name="TECH MINIMAL", mode="LAYER",
         what="minimal techno, A minor (125 BPM): a long-ringing koto pluck going back and forth between A3 and E4 six times a bar, through a slow phaser, chorus and a dotted ping-pong echo; under it a sub pluck on A2 (its sub on A1) on the off-beat 16ths, never on the beat",
         g=dict(G_BPM=125, G_DTIME="8D", G_DFDBK=56, G_DMIX=82, G_DMODE="PING", G_DCOLOR=30, G_RTYPE="PLATE", G_RSIZE=96, G_RDAMP=66),
         parts=[dict(sound="FAKE KOTO", set={"P_AMODE": "OFF", "P_SDIV": "1/16", "P_SGATE": 127, "P_TRANS": 0, "P_DEC": 116, "P_SUS": 40, "P_REL": 118, "P_FREEA": 56, "P_LRATE": 12, "P_LD_FLT": 10,
                                               "P_ITYPE": "PHASER", "P_IA": 24, "P_IB": 32, "P_IC": 30, "P_IMIX": 50, "P_CHOR": 60, "P_SPRD": 60, "P_DLY": 60, "P_REV": 44, "P_TRIM": 77, "P_LEVEL": 120},
                     steps="n57v110 . n64v84 . . n57v96 . n64v80 . . n57v100 . n64v88 . . .", len=16, bseed=36321, amt=30, b={"P_FREEA": 64}),
                dict(sound="SUB PLUCK", set={"P_AMODE": "OFF", "P_SDIV": "1/16", "P_SGATE": 80, "P_TRANS": 0, "P_SPRD": 30, "P_DLY": 16, "P_REV": 6, "P_TRIM": 98, "P_LEVEL": 120},
                     steps=". . n45v110 n45v70 . . n45v100 . . . n45v110 . . . n45v100 .", len=16, bseed=35683, amt=30, b={})]),
]
ROM_MULTIS = DRONES + MORE                    # (tools/gen_duell_multis.py reads this list: every ROM-only multi, M17 ..)


def main():
    os.chdir(fm.ROOT)
    fm.MULTIS = ROM_MULTIS
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
    for m in ROM_MULTIS:
        shutil.copyfile(scratch / ("M%02d.bin" % m["slot"]), OUT / ("M%02d.bin" % m["rom"]))
        print("M%02d %s -> %s" % (m["rom"], m["name"], OUT / ("M%02d.bin" % m["rom"])))


if __name__ == "__main__":
    main()
