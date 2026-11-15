#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL-TIDE built-in drone multis (TIDE 1.0.0-b6): makes the records of M17 DUNE HORN and M18 AMB GROWL (VOID DEEP / AMB VOID until b9) into tools/drone_multis/M17.bin, M18.bin.

    python3 tools/drone_pack.py        (needs build/gen: python3 ../gen_only.py; a C compiler)
    python3 tools/drone_pack.py verify  the committed records equal a rebuild, byte for byte (writes nothing in tools/)

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
        if len(steps.split()) not in (8, 16):           # (b13-grid: a drone part on its partner's grid is 32 or 64 steps)
            parts[p]["len"] = len(steps.split())
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
    multi(17, 1, "AMB DUNEHORN", "a horn swelling once a bar, root and fourth, with a shimmer of void above it", "HORN SWELL", "VOID SHIMMER", 107, 99,
          seq=[(0, "1/4", "0 - - - - - . . 0 - - - - - . . " "5 - - - - - . . 0 - - - - - . ."), (1, "1/4", BED_STEPS)]),
    multi(18, 2, "AMB GROWL", "a deep growling drone: detuned low saws on C2 with their sub on C1, beating slowly, a little overdrive, the low-pass moving slowly; above it a dark haze turning C3, E flat 3, G3", "REESE BASS", "NIGHT DRONE", 106, 100,
          seq=[(1, "1/4", "-12 - - - - - - - - - - - - - - - " "- - - - - - - - - - - - . . . . " "-9 - - - - - - - - - - - - - - - " "-5 - - - - - - - - - - - . . . ."), (0, "1/4", "-24" + BED_STEPS[1:])],
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
                dict(sound="FM STRINGS", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=127, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="OPEN", P_FREEA=44, P_LRATE=14, P_LD_FLT=16, **{"E:DTUN": 10, "E:MLVL": -12},
                                                P_CHOR=60, P_SPRD=60, P_DLY=30, P_REV=60, P_LEVEL=92),
                     steps="-6 - - - - - - - - - - - - - - - "
                           "- - - - - - - - - - - - - - - - "
                           "- - - - - - - - - - - - - - - - "
                           "- - - - - - - - - - - - - - - -", len=64, bseed=27687, amt=30, b={"P_FREEA": 50})]),
    # b13-multis (2026-11-15): M20 re-voiced (was DUB HOUSE, DARK MIN7 + PEDAL BASS, deep house / dub techno "near" only): the dub / roots gap. Sources: GENRES.md 2 [T6 T8 T11 T31].
    dict(rom=20, slot=4, name="DUB ROOTS", mode="LAYER",
         what="dub / roots, G minor (75 BPM, one drop): a deep round sine bass with its sub an octave under, a little saturation, mono, playing the line laid back: on the one, then the 'and's, the third beat left to the kick (G1 to G2, G minor then C minor, 2 bars); a short organ skank chord on beats 2 and 4, high-passed, thrown into a dark triplet echo of 4-5 repeats and a spring",
         # GENRES.md 2: core pair "deep round bass that plays the line (forward, mono)" + "an offbeat chord skank ... that gets dubbed" (derived); the bass laid back, accents falling
         # away from the main beats [T6]; bass recipe: a sine / triangle sub under a mid layer [T31], TIDE: ANALOG sine with a little SAT, mono; echo ~0.7 s, 4-5 repeats, timed in
         # triplets, high-cut on the return [T6]; spring "clang" [T8]; "No phasers. High-pass filter." [T11]. No source gives a BPM (gap): 75, the one-drop's slow feel.
         g=dict(G_BPM=75, G_DTIME="4T", G_DFDBK=84, G_DMIX=80, G_DMODE="DIGI", G_DCOLOR=22, G_DMDEP=16, G_RTYPE="SPRING", G_RSIZE=60, G_RDAMP=66),
         parts=[dict(sound="SUB PLUCK", set={"P_AMODE": "OFF", "P_SGATE": 118, "P_TRANS": 0, "P_DEC": 92, "P_SUS": 96, "P_REL": 60, "P_DIST": 34, "P_DKDEP": 0,
                                              "P_CHOR": 0, "P_SPRD": 0, "P_DLY": 0, "P_REV": 4, "P_TRIM": 113, "P_LEVEL": 104},
                     steps="n31v112 - - n31v90 . . n34v100 n36v96 . . n38v108 - - . n34v92 . "
                           "n36v112 - - n36v90 . . n38v100 n41v96 . . n43v108 - - . n38v92 .", bars=2, bseed=35683, amt=30, b={"P_FREEA": 44}),
                dict(sound="PHASE ORGAN", set={"P_AMODE": "OFF", "P_SGATE": 36, "P_TRANS": 0, "P_REL": 20, "P_EQLG": -6, "P_EQLF": 12, "P_EQHG": -4,
                                                "P_CHOR": 30, "P_SPRD": 40, "P_DLY": 96, "P_REV": 40, "P_TRIM": 104, "P_LEVEL": 102},
                     steps=". . . . n55+58+62v104 . . . . . . . n55+58+62v96 . . . "
                           ". . . . n55+60+63v104 . . . . . . . n55+60+63v96 . . .", bars=2, bseed=14853, amt=30, b={"P_FREEA": 60})]),
    dict(rom=21, slot=5, name="HSE LOFI", mode="LAYER",
         what="lo-fi house, D sharp minor (118 BPM, swing 70 %): a soft electric piano through worn tape, two seventh chords inside one octave a bar (D sharp minor 7 and G sharp minor 7, the eleventh in place of the fifth, the seventh at the bottom), a long dark hall and a dotted echo; a round bass on the off-beats, offset from the chords",
         g=dict(G_BPM=118, G_DTIME="8D", G_DFDBK=50, G_DMIX=64, G_DMODE="DIGI", G_DCOLOR=24, G_RTYPE="HALL", G_RSIZE=96, G_RDAMP=80),
         parts=[dict(sound="SOFT EP", set={"P_AMODE": "OFF", "P_SGATE": 110, "P_SSWING": 70, "P_ITYPE": "TAPE", "P_IA": 50, "P_IB": 60, "P_IC": 70, "P_IMIX": 80, "P_FREEA": 50,
                                             "P_CHOR": 60, "P_SPRD": 50, "P_DLY": 34, "P_REV": 60, "P_TRIM": 91, "P_LEVEL": 108},
                     steps="n61+63+66+68v100 - - n61+63+66+68v80 - . n54+56+59+61v100 - - - n54+56+59+61v84 - . n61+63+66+68v72 . n54+56+59+61v92", len=16, bseed=25255, amt=30, b={"P_FREEA": 56}),
                dict(sound="FM BASS 2", set={"P_AMODE": "OFF", "P_SGATE": 90, "P_SSWING": 70, "P_TRANS": 0, "P_DKDEP": 0, "P_DLY": 0, "P_REV": 8, "P_TRIM": 115, "P_LEVEL": 104},
                     steps=". . . n39v104 . . . . . n44v96 . . . . n39v90 .", len=16, bseed=4325, amt=30, b={})]),
    # b13-multis (2026-11-15): M22 re-voiced (was TECH CINEMA, VELVET BOW + BREATH PAD: two pads, the second far darker than its role): the synthwave gap. Sources: GENRES.md 16 [W22 W23 W24].
    dict(rom=22, slot=6, name="ELEC OUTRUN", mode="LAYER",
         what="synthwave (outrun), A minor (100 BPM, straight): a driving 16th mono saw bass of three saws, two an octave under, its filter snapping on each note, A1 to C2, the note on every beat left out so it pulls against the kick (A minor, F, C, G, a bar each); above it chorused pulse-width string-machine chords held legato, their tops on C5 and B4, widened, into a dotted echo and a big hall",
         # GENRES.md 16: "A driving 16th saw bass on F1-C2 with beat-one notes removed, syncopated against the kick" + "long legato strings on C5 / F4" [W24]; NI "unhurried" 100 [W24];
         # Nightcall bass "two saws an octave apart" [W23]; core pair "the sequenced saw bass" + "a chorused arp or string pad" (derived); strings widened [W22 W23]
         g=dict(G_BPM=100, G_DTIME="8D", G_DFDBK=44, G_DMIX=60, G_DMODE="PING", G_DCOLOR=36, G_RTYPE="HALL", G_RSIZE=100, G_RDAMP=60),
         parts=[dict(sound="FAT MONO", set={"P_AMODE": "OFF", "P_SGATE": 80, "P_TRANS": 0, "P_GLIDE": 0, "P_DKDEP": 0, "P_CHOR": 0, "P_SPRD": 0, "P_DLY": 10, "P_REV": 6, "P_TRIM": 112, "P_LEVEL": 120},
                     steps=". n33v100 n33v84 n33v92 . n33v100 n33v84 n45v96 . n33v100 n33v84 n33v92 . n33v100 n45v88 n33v92 "
                           ". n29v100 n29v84 n29v92 . n29v100 n29v84 n41v96 . n29v100 n29v84 n29v92 . n29v100 n41v88 n29v92 "
                           ". n36v100 n36v84 n36v92 . n36v100 n36v84 n48v96 . n36v100 n36v84 n36v92 . n36v100 n48v88 n36v92 "
                           ". n31v100 n31v84 n31v92 . n31v100 n31v84 n43v96 . n31v100 n31v84 n31v92 . n31v100 n43v88 n31v92", len=64, bseed=8464, amt=30, b={"P_FREEA": 52}),
                dict(sound="PWM STRINGS", set={"P_AMODE": "OFF", "P_SDIV": "1/16", "P_SGATE": 127, "P_TRANS": 0, "P_CHOR": 90, "P_SPRD": 64, "P_DLY": 26, "P_REV": 64, "P_TRIM": 90, "P_LEVEL": 90},
                     steps="n64+69+72v96 - - - - - - - - - - - - - - - "
                           "n65+69+72v92 - - - - - - - - - - - - - - - "
                           "n64+67+72v96 - - - - - - - - - - - - - - - "
                           "n62+67+71v92 - - - - - - - - - - - - - - -", len=64, bseed=7996, amt=30, b={"P_FREEA": 68})]),
    dict(rom=23, slot=7, name="AMB TECHNO", mode="LAYER",
         what="ambient techno, C major (120 BPM): a chorused saw pad in four parallel major-seventh chords a bar each (F, G, C, D: F3 to F sharp 4), and a soft harp arpeggio on each chord's root, tenth and fifth in eighths, through a dotted-quarter (750 ms) ping-pong echo with a drifting time and a big damped hall",
         g=dict(G_BPM=120, G_DTIME="4D", G_DFDBK=52, G_DMIX=70, G_DMODE="PING", G_DCOLOR=30, G_DMDEP=30, G_RTYPE="HALL", G_RSIZE=120, G_RDAMP=64),
         parts=[dict(sound="SILK CHORUS", set={"P_AMODE": "SEQ", "P_ARATE": "1/8", "P_AGATE": 127, "P_AHOLD": "OFF", "P_CHRD": "MAJ7", "P_VOIC": "CLOSE", "P_REL": 60, "P_FREEA": 44, "P_LRATE": 10, "P_LD_FLT": 8,
                                                 "P_CHOR": 100, "P_SPRD": 60, "P_DLY": 30, "P_REV": 70, "P_TRIM": 92, "P_LEVEL": 84},
                     steps="-7 - - - - - - - -5 - - - - - - - "
                           "-12 - - - - - - - -10 - - - - - - -", len=32, bseed=33289, amt=30, b={"P_FREEA": 52}),
                dict(sound="GHOST HARP", set={"P_AMODE": "SEQ", "P_ARATE": "1/8", "P_AGATE": 90, "P_AHOLD": "OFF", "P_CHRD": "OFF", "P_TRANS": 0, "P_REL": 50, "P_FREEA": 54,
                                                "P_CHOR": 40, "P_SPRD": 60, "P_DLY": 70, "P_REV": 60, "P_TRIM": 100, "P_LEVEL": 104},
                     steps="-7A 9 0 . 9 0 9 . -5A 11 2 . 11 2 . . -12A 4 -5 . 4 -5 4 . -10A 6 -3 . 6 -3 . .", len=32, bseed=10920, amt=30, b={"P_FREEA": 60})]),
    dict(rom=24, slot=8, name="TECH MINIMAL", mode="LAYER",
         what="minimal techno, A minor (125 BPM): a long-ringing koto pluck going back and forth between A3 and E4 six times a bar, through a slow phaser, chorus and a dotted ping-pong echo; under it a sub pluck on A2 (its sub on A1) on the off-beat 16ths, never on the beat",
         g=dict(G_BPM=125, G_DTIME="8D", G_DFDBK=56, G_DMIX=82, G_DMODE="PING", G_DCOLOR=30, G_RTYPE="PLATE", G_RSIZE=96, G_RDAMP=66),
         parts=[dict(sound="FAKE KOTO", set={"P_AMODE": "OFF", "P_SDIV": "1/16", "P_SGATE": 127, "P_TRANS": 0, "P_DEC": 116, "P_SUS": 40, "P_REL": 118, "P_FREEA": 56, "P_LRATE": 12, "P_LD_FLT": 10,
                                               "P_ITYPE": "PHASER", "P_IA": 24, "P_IB": 32, "P_IC": 30, "P_IMIX": 50, "P_CHOR": 60, "P_SPRD": 60, "P_DLY": 60, "P_REV": 44, "P_TRIM": 77, "P_LEVEL": 120},
                     steps="n57v110 . n64v84 . . n57v96 . n64v80 . . n57v100 . n64v88 . . .", len=16, bseed=36321, amt=30, b={"P_FREEA": 64}),
                dict(sound="SUB PLUCK", set={"P_AMODE": "OFF", "P_SDIV": "1/16", "P_SGATE": 80, "P_TRANS": 0, "P_SPRD": 30, "P_DLY": 16, "P_REV": 6, "P_TRIM": 98, "P_LEVEL": 120},
                     steps=". . n45v110 n45v70 . . n45v100 . . . n45v110 . . . n45v100 .", len=16, bseed=35683, amt=30, b={})]),
    # b10 MR9 (musical review M5: the roles the set lacked; research work/b9/research/minimal-dub.md, deep-house.md, attack/dub-house.md, attack/minimal.md)
    dict(rom=25, slot=9, name="DUB MINIMAL", mode="LAYER",
         what="minimal dub, G minor (115 BPM): a soft low-passed saw blip two or three times a bar, off the beat (G3, D4; G3, B flat 3, F3 over two bars), thrown into a long dark drifting ping-pong echo that does the rest; under it a quiet floor of tape hiss, held, steady and about 15 dB under the blip, its lows cut (b10 round 5: was held three bars of four, 5 dB under, and swelled every four bars)",
         g=dict(G_BPM=115, G_DTIME="8D", G_DFDBK=72, G_DMIX=80, G_DMODE="PING", G_DCOLOR=26, G_DMDEP=20, G_RTYPE="PLATE", G_RSIZE=80, G_RDAMP=70),
         parts=[dict(sound="DUB BLIP", set={"P_AMODE": "SEQ", "E:WAVE": 0, "P_ARATE": "1/16", "P_AGATE": 100, "P_AHOLD": "OFF", "P_CHRD": "OFF", "P_TRANS": 0, "P_FREEA": 38,
                                             "P_CHOR": 50, "P_SPRD": 50, "P_DLY": 96, "P_REV": 30, "P_TRIM": 127, "P_LEVEL": 106},
                     steps=". . . -5A . . . . . . 2 . . . . . . . . -5 . . . -2 . . . . . . -7 .", bars=2, bseed=41213, amt=30, b={"P_FREEA": 36}),
                dict(sound="TAPE HISS", set={"P_AMODE": "SEQ", "P_ARATE": "1/16", "P_AGATE": 127, "P_AHOLD": "OFF", "P_CHRD": "OFF", "P_TRANS": 0, "P_EQLG": -12, "P_EQLF": 12, "P_EQHG": -6,
                                              "P_CHOR": 40, "P_SPRD": 64, "P_DLY": 20, "P_REV": 70, "P_TRIM": 106, "P_LEVEL": 82},
                     steps="0 - - - - - - - - - - - - - - - "
                           "- - - - - - - - - - - - - - - - "
                           "- - - - - - - - - - - - - - - - "
                           "- - - - - - - - - - - - - - - -", len=64, bseed=17761, amt=30, b={})]),
    # b13-multis (2026-11-15): M26 re-voiced (was HSE KEYS, SUNDAY KEYS + ROTOR HAZE: deep house had five multis, SUNDAY KEYS also plays M08): the atmospheric drum & bass gap.
    # Sources: GENRES.md 15 [W2 W3 W4 W7 W11].
    dict(rom=26, slot=10, name="AMB JUNGLE", mode="LAYER",
         what="atmospheric drum & bass, D minor (174 BPM, half-time against the break): the Reese, two saws beating slowly over a sine sub, mono with glide, D2 down to A1, a note a half-bar or longer, moving where the break's kicks fall (the 1 and the 'and' of 3), its low-pass drifting and a light duck; above it soft phase-distortion strings, D minor 9, B flat major 7, G minor 9, A suspended, two bars each, high-passed, a light reverb",
         # GENRES.md 15: core pair "sub / Reese bass (mono)" + "a slow pad or string bed", "At 170 the parts play half-time against the break" (derived); 174 [W11], mid-170s [W2];
         # the Reese on a Casio CZ phase-distortion synth [W3]; modern Reese: slow LFO, kick-ducked [W4]; NI kicks on the 1st and 6th 8th [W11]; pad high-passed at 200 Hz,
         # ~20 % reverb [W11]; "Timeless": string parts [W7]; light side: ambient pads, strings [W2]
         g=dict(G_BPM=174, G_DTIME="4D", G_DFDBK=40, G_DMIX=56, G_DMODE="PING", G_DCOLOR=30, G_RTYPE="HALL", G_RSIZE=96, G_RDAMP=64),
         parts=[dict(sound="NIGHT REESE", set={"P_AMODE": "OFF", "P_SDIV": "1/8", "P_SGATE": 127, "P_TRANS": 0, "P_DKSRC": "PUMP", "P_DKRATE": "1/2", "P_DKDEP": 18, "P_CHOR": 0, "P_SPRD": 0, "P_DLY": 0, "P_REV": 8, "P_TRIM": 94, "P_LEVEL": 112},
                     steps="n38v110 - - - - n38v96 - - n36v110 - - - - n33v100 - - "
                           "n34v110 - - - - n34v96 - - n36v110 - - - - n33v100 - n31v92", len=32, bseed=55474, amt=30, b={"P_FREEA": 64}),
                dict(sound="PHASE STRING", set={"P_AMODE": "OFF", "P_SDIV": "1/8", "P_SGATE": 127, "P_TRANS": 0, "P_EQLG": -9, "P_EQLF": 10, "P_CHOR": 60, "P_SPRD": 60, "P_DLY": 16, "P_REV": 44, "P_TRIM": 90, "P_LEVEL": 94},
                     steps="n53+57+60+64v96 - - - - - - - - - - - - - - - "
                           "n50+53+57+62v92 - - - - - - - - - - - - - - - "
                           "n50+55+58+62v96 - - - - - - - - - - - - - - - "
                           "n52+57+62+64v92 - - - - - - - - - - - - - - -", len=64, bseed=64729, amt=30, b={"P_FREEA": 60})]),
    # b12 EMPTY (owner 2026-11-13: "an empty multi for easy clearance and performance"): a blank slate to play live from. Two plain sounds, no steps, no arp, the sends at 0, short tails.
    dict(rom=27, slot=11, name="EMPTY", mode="LAYER", empty=True,
         what="a blank slate for live playing: one plain FM6 electric piano on part 1, one plain ANALOG square lead on part 2, nothing in either pattern, no arp, no sends, short tails; AUTO TIDE idle (a load starts both parts at A)",
         g=dict(G_BPM=120, G_DTIME="8D", G_DFDBK=0, G_DMIX=0, G_DMODE="DIGI", G_RTYPE="PLATE", G_RSIZE=60, G_RDAMP=60),
         parts=[dict(sound="TINE ROOM", set=dict(P_AMODE="OFF", P_AHOLD="OFF", P_CHOR=0, P_DLY=0, P_REV=0, P_LEVEL=100), bseed=14977, amt=0, b={}),
                dict(sound="SQUARE LEAD", set=dict(P_AMODE="OFF", P_AHOLD="OFF", P_REL=40, P_CHOR=0, P_DLY=0, P_REV=0, P_LEVEL=96), bseed=64088, amt=0, b={})]),
]
ROM_MULTIS = DRONES + MORE                    # (tools/gen_duell_multis.py reads this list: every ROM-only multi, M17 ..)


def main():
    verify = sys.argv[1:] == ["verify"]
    if verify and os.name == "nt":
        print("drone_pack: verify not run on Windows (tests/gen_factory_multis.c needs fork())")
        return
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
    if verify:                                          # `drone_pack.py verify`: the committed records must equal a rebuild; nothing is written to tools/
        bad = [("M%02d" % m["rom"]) for m in ROM_MULTIS
               if not (OUT / ("M%02d.bin" % m["rom"])).exists() or (scratch / ("M%02d.bin" % m["slot"])).read_bytes() != (OUT / ("M%02d.bin" % m["rom"])).read_bytes()]
        print("drone_pack: %d records rebuilt, %d differ from the committed ones%s" % (len(ROM_MULTIS), len(bad), (": " + " ".join(bad)) if bad else ""))
        sys.exit(1 if bad else 0)
    OUT.mkdir(parents=True, exist_ok=True)
    for m in ROM_MULTIS:
        shutil.copyfile(scratch / ("M%02d.bin" % m["slot"]), OUT / ("M%02d.bin" % m["rom"]))
        print("M%02d %s -> %s" % (m["rom"], m["name"], OUT / ("M%02d.bin" % m["rom"])))


if __name__ == "__main__":
    main()
