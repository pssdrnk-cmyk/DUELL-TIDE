#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory multis (0.2.23; 0.2.24: the owner's multis back; 0.2.25: COLD CHAPEL in M13): the ONE source of the sixteen factory multis M01..M16 and the tool that builds them.

    tools/factory_multis.py build      the records tools/factory_multis/M01.bin..M16.bin (needs build/gen from DUELL/gen_only.py)
    tools/factory_multis.py check      the records against this list (version, name, sounds, MODE, split) and the similarity check
    tools/factory_multis.py list       the list as text (slot, name, mode, parts '#NN NAME', owner / mine)
    tools/factory_multis.py selftest   the similarity check catches each kind of pair (and lets an owner pair through, reported); the step grammar's rules (T1)

How a record is made: MULTIS below says, per slot, the two parts' factory sounds (by name), what the multi sets on top of each sound
(raw parameter values, core.h P_* ids; an engine EDIT value as "E:LABEL", e.g. "E:CUT" on an ANALOG part), the SEQUENCER arp patterns,
End B, the globals (core.h G_* ids), the output MODE, XFADE and SPLIT (item 157: BALANCE is no multi value; its record byte is a spare, 0). `build` writes that as a C table (build/gen_mm/factory_multis_def.h)
and runs tests/gen_factory_multis.c, which loads each sound with the firmware's own code (set_engine_of + apply_preset_to, as PATCHES loads it),
applies the table and saves with multi.c's packer (project_save_as): the records are byte for byte what a unit saves (version MU_VER 7; a kept record of an older version is converted by tools/multi_upgrade.py). A slot
marked `kept` is a record made earlier that is kept byte for byte (owner, 0.2.20: the factory multis do not follow reworked sounds): it is not
rebuilt, only checked and, when its slot moved, moved (git mv). A slot marked `owner` is a multi the owner made or edited himself (0.2.24, owner:
never drop or change them): kept, and exempt from the similarity check (a pair with one is reported, never a failure). The reverb LOW CUT of
every built multi is the unit's default (G_RLOCUT 75 = 299 Hz) unless the multi sets it; DUCK SRC is PUMP / PART / MIDI (0 / 1 / 2, MU_VER 6); MU_VER 7 adds, per part, the NX extension slots and the hand-set B edit list after the values (tools/multi_upgrade.py).

The owner's own edits of a multi come back into this list through tools/import_owner_edits.py (docs/OWNER_REFINE.md): it writes
tools/owner_multis.py, read below (apply_owner), whose entries replace a slot's record (kept, owner) and its line here.

Old one-off converters (factory_multis_levelsplit.py, _locut.py, _ducksrc.py, _delaymode.py, _lfo_free.py) are gone: every built
record is made in today's format, and the kept ones were converted by them before (0.2.14 .. 0.2.22).

The test scenes of the host tests (the CPU TEST multis of before 0.2.23 and the factory multis of 0.2.22) are in tests/scenes/,
not in a factory slot (tests/scenes.h). SPARE below lists multis made for 0.2.23 that left the sixteen in 0.2.24 (their records are in
tests/scenes/spare/, so they can come back: move the entry into MULTIS, give it a slot and run `build`).
"""
import importlib.util
import math
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIR = ROOT / "tools" / "factory_multis"
GEN = ROOT / "build" / "gen_mm"
REC_MAX, MU_G_AT, MB_N, PS_STEP, NAME_MAX = 1916, 20, 7, 9, 12
MU_VER, MU_STEP, NX, UPF_PK = 8, 7, 28, 112                # multi.c MU_VER 8 (ROLL CHAINED storage, 2026-10-29): End B as values after a length byte, the NX slots dense, 7-byte steps, the FM6 patch packed 8 to 7, only the globals a multi loads
ENGINES = ["ANALOG", "DIGITAL", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "DRUM", "NOISE", "FM6", "SLICE", "WAVE"]
MODES = {"DUEL": 0, "STEREO": 1, "LAYER": 2}           # core.h DM_SPLIT (shown DUEL), DM_STEREO, DM_LAYER: the record's byte 4
NOTE = {n: i for i, n in enumerate(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}


def note(s):
    """'F3' -> 53 (C4 = 60)"""
    m = re.fullmatch(r"([A-G]#?)(-?\d)", s)
    return 12 * (int(m.group(2)) + 1) + NOTE[m.group(1)]


# ---- the multis ---------------------------------------------------------------------------------------------------------------
# A part: sound (factory sound name), set {P_id: value or 'NAME' of the parameter's list; "E:LABEL": the engine's EDIT value}, steps (the
# SEQUENCER arp's pattern: offsets in semitones from the held key, '.' a rest, '-' a tie (holds the note before it; after a rest or at step 0 it is
# an error), "vNN" after the note or offset a velocity 1..127 (default 100; order: notes, v, A), suffix A an accent; LEN = its length: 8 or 16
# steps, or 12 on a part with poly=True (a polymeter on purpose), or 32 on a part with bars=2 (two bars); any other count fails `check`), bseed / amt (End B rolled
# again with this seed / at this AMOUNT %; 0 the sound's own). A multi: g {G_id: value}, mode, xf (0..100), split (a note:
# below it part 1, at or above part 2; compared with the key as played, before any TRN: F3 is the lowest key at octave 0, so F4 is the middle
# of the keyboard), what (one line, the character).
# Units are the stored ones (params.c): sends / SAT / RES 0..127, LEVEL / TRIM 0..127 (112 = 0 dB), PAN -64..63, EQ dB -12..12,
# DUCK DEPTH 0..100, arp GATE 1..127, the lists by name. Owner rules (0.2.23): at most 2 or 3 tributes (none in the sixteen since 0.2.25: BASILDON is spare),
# no Moroder multi (it is a pattern), classic honest names, no two multis alike (check_similar). Owner (0.2.24): his own multis
# (COLD CHAPEL, ACID ETHER, GARGLE ACID of 0.2.22; DEAD BUFFER holds his edits) are never dropped: `owner` (D31, 2026-11-05: the owner took ACID ETHER, GARGLE ACID, DEAD BUFFER, WIRED SNAIL, REESE SWAMP and WOOD & SKIN out of the sixteen himself).
# In LAYER each part maps a played key with its own TRN (seq.c key_on, 0.2.23: until 0.2.22 part 2 got part 1's note and the split compared it),
# so an octave between the parts can be set with either part's TRN.
# Order (D31, audit G14, 2026-11-05): the curated dub / house order, a background layer for a Volca Bass and a drum machine. 1-9 are the grooves
# of the set (BASIC CHORD, DUB CHORD, CELLAR TAPE, NIGHT ORGAN: chords and a sparse bass part where there is one; ORGAN SKANK, WARM ECHO, MINIMAL 12,
# TINE DUB, DEEP 332: the six new ones, built here), 10-16 the colour (the owner's pads and textures, kept byte for byte). The rules of the new
# ones: no part below C3 (its first note, the arps' stand-in C4 with the steps as written), the reverb LOW CUT at the unit's default (299 Hz), the
# rhythmic part six hits a bar or fewer and the other sustained or sparse, no part above C6, the chord and pad sounds from the dub pack (centroid
# under about 1 kHz, so the hats keep 4-10 kHz). The patterns are the factory patterns' steps (engines.c PATTERNS: P03 DUB OFFBEAT, P04 HOUSE STAB,
# P05 DOT DELAY, P10 MINIMAL 12, P11 DUB HALFTIME, P13 DEEP 332, P15 PAD BREATH, P17 HALF PAD, P01 DUB CHORD, P29 TIED ROOTS), written here as the arps' offsets
# from the held key ("-" is a tie, which the SEQ arp honours). Left the sixteen: the three acid multis (REESE SWAMP, ACID ETHER, GARGLE ACID), WIRED SNAIL and
# DEAD BUFFER (FX arps up to C8), WOOD & SKIN (a percussion demo); their records are in the git history (integ bb730b0, tools/factory_multis/M01 M09 M10 M12 M14 M15.bin)
# and the older scenes in tests/scenes. The records kept move to their new slot byte for byte: DUB CHORD (was M04; its TIDAL SUB line moved up an octave, C2 to C3,
# and its LEVEL down 4 so that the sub does not sit on the Volca Bass), CELLAR TAPE (was M16), NIGHT ORGAN (M02), PAD SPLIT (M03), WHISPER TIDE (M05),
# TINE STRINGS (M06), COLD CHAPEL (M13), EERIE SIGNAL (M11), HAZY TENSOR (M08), FINAL REEL (M07); the first start loads M02 DUB CHORD (D5, tide.h TIDE_FIRST_MULTI).
SEND = ("P_CHOR", "P_DLY", "P_REV")
MULTIS = [
    # ---- 1-9: the grooves of the set ----
    dict(slot=1, name="BASIC CHORD", mode="LAYER",
         what="dub techno chords with nothing in the bass: off-beat minor-seventh stabs through a dotted echo and a plate, ducked on the quarters, a crackle bed under them (the first multi to try)",
         g=dict(G_BPM=122, G_DTIME="8D", G_DFDBK=70, G_DMIX=84, G_DMODE="DIGI", G_RTYPE="PLATE", G_RSIZE=80, G_RDAMP=61),
         parts=[dict(sound="CHORD ECHO", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=45, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="OPEN",
                                                   P_DKSRC="PUMP", P_DKRATE="1/4", P_DKDEP=30, P_LEVEL=102),
                     steps=". . 0 . . . 0A . . . 0 . . . 0A ."),
                dict(sound="DUST BED", set=dict(P_AHOLD="ALL", P_EQLG=-12, P_LEVEL=94))]),
    dict(slot=2, name="DUB CHORD", kept=True, owner=True, mode="LAYER", parts=[dict(sound="BIG STAB"), dict(sound="TIDAL SUB")],
         what="off-beat minor-seventh stabs through a long dotted echo over a sparse sub line (the owner's; the sub an octave up since D31; the first start's multi)"),
    # ---- b7 (lane TECHNO, owner 9 November 2026: "adjustments of the multis for techno, like berlin and such"; then: "a curated selection of electronic music and
    # genres, berlin school, idm, electro"): M03 acid, M04 electro, M07 techno, M13 IDM, M14 Berlin school, M15 Detroit, 120-130 BPM, two ARP SEQ parts each (both play on
    # PLAY with no key held), the motif's anchors accented and a few softer hits TIDE may thin.
    # Replaced: CELLAR TAPE (M03), NIGHT ORGAN (M04, the owner's), MINIMAL 12's blip and pad (M07), COLD CHAPEL, EERIE SIGNAL, HAZY TENSOR (M13-M15, the owner's);
    # their records are in the git history (b7/integ dc8f1d3, tools/factory_multis/M03 M04 M07 M13 M14 M15.bin).
    dict(slot=3, name="ACID FIELD", mode="LAYER",
         what="acid: a squelching mono line with slides and accents (the sound an octave down, C3), octave jumps and a flat seventh, a deep phase pad an octave above holding C minor and A flat minor (130 BPM)",
         g=dict(G_BPM=130, G_DTIME="8D", G_DFDBK=46, G_DMIX=71, G_DMODE="PING", G_RTYPE="ROOM", G_RSIZE=80, G_RDAMP=64),
         parts=[dict(sound="SQUELCH", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=70, P_AHOLD="OFF", P_DLY=24, P_LEVEL=108),
                     steps="0A . 12v80 . . . 10 - . 0A . 12 . 3v76 - ."),
                dict(sound="DEEP BLUE", set=dict(P_AMODE="SEQ", P_ARATE="1/4", P_AGATE=127, P_AHOLD="OFF", P_CHRD="MIN", P_VOIC="CLOSE", P_DLY=30, P_REV=60, P_LEVEL=98),
                     steps="12A - - - - - - . 8A - - - - - - .")]),
    dict(slot=4, name="ROBOT FUNK", mode="LAYER",
         what="electro: a tight robotic pluck on a syncopated single-note figure that jumps the octave and the fifth, a cold vowel pad underneath like a vocoder choir, C minor to B flat (128 BPM)",
         g=dict(G_BPM=128, G_DTIME="1/8", G_DFDBK=30, G_DMIX=64, G_DMODE="PING", G_RTYPE="ROOM", G_RSIZE=70, G_RDAMP=70),
         parts=[dict(sound="DIGI PLUCK", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=26, P_AHOLD="OFF", P_DLY=20, P_REV=16, P_LEVEL=104),
                     steps="0A . . 0v80 . . 12A . . 0v72 . 7 . 0v66 10 ."),
                dict(sound="VOWEL PAD", set=dict(P_AMODE="SEQ", P_ARATE="1/4", P_AGATE=127, P_AHOLD="OFF", P_CHRD="MIN", P_VOIC="CLOSE", P_EQLG=-6, P_REV=50, P_LEVEL=92),
                     steps="-12A - - - - - - . -14A - - - - - - .")]),
    dict(slot=5, name="ORGAN SKANK", mode="LAYER",
         what="reggae-house organ stabs on the off-beats, a spring and a dotted echo, a breathing shimmer pad an octave up holding two chords a bar",
         g=dict(G_BPM=123, G_DTIME="8D", G_DFDBK=55, G_DMIX=77, G_DMODE="DIGI", G_RTYPE="SPRING", G_RSIZE=71, G_RDAMP=71),
         parts=[dict(sound="ORGAN SKANK", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=30, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="CLOSE", P_LEVEL=104),
                     steps=". . 0 0 . . 0A . . . 0 0 . . 0A ."),
                dict(sound="AIR SHIMMER", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=100, P_AHOLD="ARP", P_CHRD="MIN7", P_VOIC="CLOSE",
                                                    P_DLY=30, P_REV=64, P_LEVEL=86),
                     steps="12A - - - - - - . 8A - - - - - - .")]),
    dict(slot=6, name="WARM ECHO", mode="LAYER",
         what="a warm detuned wash in open fifths under a slow dotted-delay pluck figure ping-ponging in the echo (118 BPM)",
         g=dict(G_BPM=118, G_DTIME="8D", G_DFDBK=58, G_DMIX=84, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=84, G_RDAMP=61),
         parts=[dict(sound="WARM WASH", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=100, P_AHOLD="OFF", P_CHRD="POW", P_VOIC="CLOSE", P_REV=60, P_LEVEL=89),
                     steps="0A - - - - - - - - - - - - - - . -2A - - - - - - - - - - - - - - .", bars=2),
                dict(sound="TAPE PLUCK", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=36, P_AHOLD="OFF", P_DLY=77, P_REV=40, P_LEVEL=111),
                     steps="12A . . 12 . . . 8A . . 5 . . . 7 .")]),
    dict(slot=7, name="MINIMAL 12", mode="LAYER",
         what="hypnotic minimal techno: a hollow two-note pluck on a twelve-step loop against the sixteen, the dotted echo on its grid, an iron ring-mod texture swelling once a bar on beat two, every other bar a fifth up (130 BPM; b7: was a blip and a pad)",
         g=dict(G_BPM=130, G_DTIME="8D", G_DFDBK=62, G_DMIX=77, G_DMODE="DIGI", G_RTYPE="ROOM", G_RSIZE=80, G_RDAMP=64),
         parts=[dict(sound="HOLLOW POP", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=40, P_AHOLD="OFF", P_DLY=66, P_REV=30, P_LEVEL=110),
                     steps="12A . . 12v80 . . 15A . . . 12v66 .", poly=True),
                dict(sound="IRON WORKS", set=dict(P_AMODE="SEQ", P_ARATE="1/8", P_AGATE=127, P_AHOLD="OFF", P_DLY=30, P_REV=50, P_LEVEL=94),
                     steps=". . 0A - - - - . . . 7v80 - - - - .")]),
    dict(slot=8, name="TINE DUB", mode="LAYER",
         what="FM electric-piano tines stabbing a dub chord through a tape echo, a slow-moving analog pad holding open fifths under them (the FM voice of the set, 120 BPM)",
         g=dict(G_BPM=120, G_DTIME="8D", G_DFDBK=60, G_DMIX=84, G_DMODE="DIGI", G_RTYPE="PLATE", G_RSIZE=77, G_RDAMP=61),
         parts=[dict(sound="TINE ROOM", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=50, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="CLOSE", P_DLY=64, P_REV=30, P_LEVEL=98),
                     steps=". . 0A - - . 0 . . . 0A - - . 3 ."),
                dict(sound="SLOW TIDE", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=100, P_AHOLD="OFF", P_CHRD="POW", P_VOIC="CLOSE", P_REV=50, P_LEVEL=85),
                     steps="0A - - - - - - - 7A - - - - - - -")]),
    dict(slot=9, name="DEEP 332", mode="LAYER",
         what="a deep-house soul stab comping a 3-3-5-3-2 rhythm with a touch of swing, a dark fog holding the root and the fifth underneath (124 BPM)",
         g=dict(G_BPM=124, G_DTIME="8D", G_DFDBK=45, G_DMIX=71, G_DMODE="DIGI", G_RTYPE="ROOM", G_RSIZE=71, G_RDAMP=61),
         parts=[dict(sound="SOUL STAB", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=40, P_ASWING=54, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="OPEN", P_LEVEL=101),
                     steps="0A . . 0 . . 0A . . . . 3 . . 0 ."),
                dict(sound="FOG BANK", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=100, P_AHOLD="OFF", P_REV=55, P_LEVEL=98),
                     steps="0A - - - - - - - 7A - - - - - - -")]),
    # ---- 10-16: colour: the owner's pads and textures, kept byte for byte (tools/owner_multis.py; their old slots in brackets) ----
    # 10 PAD SPLIT (M03), 11 WHISPER TIDE (M05), 12 TINE STRINGS (M06), 13 COLD CHAPEL (M13), 14 EERIE SIGNAL (M11), 15 HAZY TENSOR (M08), 16 FINAL REEL (M07)
    # b7 lane TECHNO: 13-15 are IDM, Berlin school and Detroit scenes now (the owner's COLD CHAPEL, EERIE SIGNAL and HAZY TENSOR left; see M03 above)
    dict(slot=13, name="BROKEN BELLS", mode="LAYER",
         what="IDM: glassy FM bells on a broken twelve-step figure (fifth, ninth, third, seventh at four loudnesses) drifting against the bar, a detuned tape pad wobbling between C minor and F minor an octave below (120 BPM)",
         g=dict(G_BPM=120, G_DTIME="16D", G_DFDBK=52, G_DMIX=71, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=90, G_RDAMP=64),
         parts=[dict(sound="FROST BELLS", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=50, P_AHOLD="OFF", P_DLY=40, P_REV=50, P_LEVEL=97),
                     steps="7A . . 14v72 . 3v90 . . 10A . . .", poly=True),
                dict(sound="TAPE CLOUD", set=dict(P_AMODE="SEQ", P_ARATE="1/4", P_AGATE=127, P_AHOLD="OFF", P_CHRD="MIN", P_VOIC="CLOSE", P_REV=60, P_LEVEL=90),
                     steps="-12A - - - - - - . -7A - - - - - - .")]),
    dict(slot=14, name="BERLIN RUN", mode="LAYER",
         what="Berlin school on a techno clock: an analog pluck running eighths through C minor, root against the fifth, seventh and octave, over string fifths an octave below: C for two bars, A flat and B flat a bar each (126 BPM)",
         g=dict(G_BPM=126, G_DTIME="8D", G_DFDBK=48, G_DMIX=71, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=96, G_RDAMP=61),
         parts=[dict(sound="ARP PLUCK", set=dict(P_AMODE="SEQ", P_ARATE="1/8", P_AOCT=1, P_AGATE=60, P_AHOLD="OFF", P_DLY=40, P_REV=34, P_LEVEL=104),
                     steps="0A 7v90 0v84 10 . 12A 0v84 10v90 0A 7v90 0v84 3 . 5A 0v84 3v90"),
                dict(sound="RAIN STRINGS", set=dict(P_AMODE="SEQ", P_ARATE="1/4", P_AGATE=127, P_AHOLD="OFF", P_CHRD="POW", P_VOIC="CLOSE", P_REV=64, P_LEVEL=92),
                     steps="-12A - - - - - - . -16A - - . -14 - - .")]),
    dict(slot=15, name="DETROIT STAB", mode="LAYER",
         what="Detroit techno: bright minor-seventh stabs on the Detroit figure (C, F, G minor seven, two hits softer) with a long echo, an 80s brass-string pad swelling under them an octave down, C minor to F minor (127 BPM)",
         g=dict(G_BPM=127, G_DTIME="8D", G_DFDBK=50, G_DMIX=71, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=90, G_RDAMP=61),
         parts=[dict(sound="ICE STAB", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=34, P_AHOLD="OFF", P_CHRD="MIN7", P_VOIC="CLOSE", P_DLY=54, P_REV=30, P_LEVEL=96),
                     steps="0A . . 0v80 . . 5 . . . 0A . . 7v80 . ."),
                dict(sound="NEON BRASS", set=dict(P_AMODE="SEQ", P_ARATE="1/4", P_AGATE=127, P_AHOLD="OFF", P_CHRD="MIN", P_VOIC="CLOSE", P_DLY=20, P_REV=70, P_LEVEL=86),
                     steps="-12A - - - - - - . -7A - - - - - - .")]),
]

# ---- spare multis (0.2.24): made for 0.2.23, out of the sixteen so the owner's COLD CHAPEL, ACID ETHER and GARGLE ACID could come back.
# Chosen by the similarity check's own measure against the restored and kept ones: VELVET TINES and SEVENOAKS are both a PAD + KEYS layer
# like the owner's EERIE SIGNAL (VESPERS + JAZZ CLUB), and the 0.2.23 drone shared SINGING BOWL with his COLD CHAPEL and took its name
# (renamed here). The 0.2.23 TINE STRINGS is VELVET TINES here since 0.2.25: the owner's M06 has that name (its record still says TINE STRINGS). Their records of 0.2.23 are tests/scenes/spare/ (rendered and hashed by tests/multi_render_test.c). Not built.
SPARE = [
    dict(name="VELVET TINES", scene="spare/M06_TINE_STRINGS", mode="LAYER", xf=37,
         what="electric piano over a string pad: harder notes bring the tines forward (XFADE), MORPH opens the strings (M06 in 0.2.23)",
         g=dict(G_BPM=77, G_DTIME="4D", G_DFDBK=27, G_DMIX=71, G_DMODE="DIGI", G_RTYPE="PLATE", G_RSIZE=77, G_RDAMP=61),
         parts=[dict(sound="VELVET BOW", set=dict(P_REV=47, P_M8SRC="MORPH", P_M8DST="CUT", P_M8AMT=21, P_LEVEL=101), bseed=25230, amt=27),
                dict(sound="TINE ROOM", set=dict(P_DLY=11, P_REV=31, P_LEVEL=112), amt=27)]),
    dict(name="BOWL & WIND", scene="spare/M07_BOWL_AND_WIND", mode="LAYER",
         what="cinematic drone: a bowed singing bowl an octave down over cold moor wind, both latched; MORPH opens them (M07 in 0.2.23, named COLD CHAPEL then: the record still says so)",
         g=dict(G_BPM=71, G_DTIME="4D", G_DFDBK=71, G_DMIX=97, G_RTYPE="HALL", G_RSIZE=117, G_RDAMP=57, G_DMODE="PING", G_DMDEP=17),
         parts=[dict(sound="SINGING BOWL", set=dict(P_AHOLD="ALL", P_TRANS=-12, P_REV=87, P_DLY=27, P_M8SRC="MORPH", P_M8DST="E3", P_M8AMT=17, P_LEVEL=95),
                     bseed=7869, amt=27),
                dict(sound="MOOR WIND", set=dict(P_AHOLD="ALL", P_REV=71, P_DLY=17, P_M8SRC="MORPH", P_M8DST="CUT", P_M8AMT=11, P_LEVEL=97), amt=27)]),
    dict(name="SEVENOAKS", scene="spare/M14_SEVENOAKS", mode="LAYER", tribute=True,
         what="tribute to Kent's rave-era duo: a 1/16 FM bell arpeggio over two octaves above a phase-distortion string pad (M14 in 0.2.23)",
         g=dict(G_BPM=127, G_DTIME="8D", G_DFDBK=41, G_DMIX=90, G_DMODE="PING", G_RTYPE="HALL", G_RSIZE=97, G_RDAMP=61),
         parts=[dict(sound="LAMPLIGHT", set=dict(P_CHRD="OFF", P_REV=57, P_M8SRC="MORPH", P_M8DST="SHP", P_M8AMT=17, P_LEVEL=90)),
                dict(sound="FROST BELLS", set=dict(P_AMODE="UP", P_ARATE="1/16", P_AOCT=2, P_AGATE=71, P_AHOLD="OFF", P_DLY=37, P_REV=47, P_LEVEL=93))]),
    dict(name="BASILDON", scene="spare/M13_BASILDON", mode="LAYER", tribute=True,
         what="tribute to Essex synth-pop: an 8th-note FM bass sequence under a ring-modulated metal riff, 80s plate (M13 in 0.2.23 .. 0.2.25)",
         g=dict(G_BPM=117, G_DTIME="8D", G_DFDBK=31, G_DMIX=77, G_DMODE="PING", G_RTYPE="PLATE", G_RSIZE=71, G_RDAMP=57),
         parts=[dict(sound="FM BASS 2", set=dict(P_AMODE="SEQ", P_ARATE="1/8", P_AGATE=71, P_AHOLD="OFF", P_TRANS=-12, P_LEVEL=109),
                     steps="0A 0 0 0 0 0 0 +7"),
                dict(sound="TIN TOLL", set=dict(P_AMODE="SEQ", P_ARATE="1/16", P_AGATE=57, P_AHOLD="OFF", P_DLY=31, P_REV=41, P_LEVEL=101),
                     steps="+12A . +19 +12 . +22 +19 . +12A . +15 +17 . +19 . +10")]),
    dict(name="MONO DUEL", scene="spare/M08_MONO_DUEL", mode="DUEL",
         what="two dry mono voices for outboard effects: a chip-Moog bass hard left, a square lead with glide hard right (M08 in 0.2.23 .. 0.2.25; the owner: Mono-dual is out; the only factory multi in DUEL)",
         g=dict(G_BPM=117, G_DTIME="1/8", G_DFDBK=31, G_DMIX=77, G_DMODE="DIGI", G_RTYPE="ROOM", G_RSIZE=51),
         parts=[dict(sound="BREADBIN", set=dict(P_VOICE="LEG", P_TRANS=-12, P_CHOR=0, P_DLY=0, P_REV=0, P_DKDEP=0, P_LEVEL=117)),
                dict(sound="SQUARE LEAD", set=dict(P_VOICE="LEG", P_CHOR=0, P_DLY=0, P_REV=0, P_LEVEL=117))]),
]


def apply_owner(multis):
    """tools/owner_multis.py (written by tools/import_owner_edits.py): OWNER_MULTIS = [dict(slot, name, mode, parts=[dict(sound), dict(sound)], what, ...)],
    each a record the owner saved on his unit, kept byte for byte in tools/factory_multis/Mnn.bin: it replaces the slot's entry here (kept, owner)"""
    f = ROOT / "tools" / "owner_multis.py"
    if not f.exists():
        return multis
    spec = importlib.util.spec_from_file_location("owner_multis", f)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    out = list(multis)
    for o in mod.OWNER_MULTIS:
        if o.get("slot") not in range(1, 17):
            sys.exit("factory_multis: owner_multis.py: slot %r" % o.get("slot"))
        e = dict(o, kept=True, owner=True)
        out = [m for m in out if m["slot"] != e["slot"]] + [e]
    return sorted(out, key=lambda m: m["slot"])


MULTIS = apply_owner(MULTIS)


# ---- the firmware's names --------------------------------------------------------------------------------------------------------
def _lists():
    src = (ROOT / "firmware" / "src" / "params.c").read_text()
    out = {}
    for m in re.finditer(r"static const char \*const (N_\w+)\[\]\s*=\s*\{(.*?)\};", src, re.S):
        body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
        out[m.group(1)] = [s.replace("\\n", " ") for s in re.findall(r'"((?:[^"\\]|\\.)*)"|\b0\b', body)]   # (DUELL 1.1: a shown name may break at a line (\n): the key is the words with a space, SOFT CLIP)
    return out


LISTS = _lists()
P_LIST = {"P_AMODE": "N_AMODE", "P_ARATE": "N_DIV", "P_AHOLD": "N_HOLD", "P_CHRD": "N_CHRD", "P_VOIC": "N_VOIC", "P_VOICE": "N_VOICE",
          "P_LSYNC": "N_LSYNC", "P_ITYPE": "N_ITYPE", "P_DKSRC": "N_DKSRC", "P_DKRATE": "N_DIV", "P_M8SRC": "N_MSRC", "P_M8DST": "N_MDST",
          "P_SDIV": "N_DIV"}
G_LIST = {"G_DTIME": "N_DIV", "G_RTYPE": "N_RTYPE", "G_DMODE": "N_DMODE", "G_SHPITCH": "N_SHP"}
RETIRED = {("G_SHPITCH", "BOTH"): 2}               # 0.2.25 (owner): SHIM's PITCH BOTH removed; a stored 2 plays UP (project.c, fx_bus.c rev_shimx). M16 STRESS TEST holds it


def value(lists, key, v):
    if isinstance(v, int):
        return v
    if v.startswith("u_uf("):                      # the multimode filter's CUT for a frequency in Hz (tools/duell_sounds.py, core.h UF_STEPS 82)
        hz = float(v[5:-1])
        return max(0, min(82, round(82 * math.log(hz / 30) / math.log(16000 / 30))))
    names = LISTS[lists[key]]
    if (key, v) in RETIRED:                        # (0.2.25: a value the unit no longer offers; the record keeps its byte, the unit reads it as the stand-in)
        return RETIRED[(key, v)]
    if v not in names:
        sys.exit("factory_multis: %s has no value %r (%s)" % (key, v, ", ".join(names)))
    return names.index(v)


def _gen_sounds():
    spec = importlib.util.spec_from_file_location("gen_duell_sounds", ROOT / "tools" / "gen_duell_sounds.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def sounds():
    spec = importlib.util.spec_from_file_location("duell_sounds", ROOT / "tools" / "duell_sounds.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return _gen_sounds().with_owner(mod.SOUNDS)        # (0.2.24: the owner's edited sounds, tools/owner_sounds.py, as the build sees them)


def resolve(snd):
    by = {s["name"]: s for s in snd}
    for m in MULTIS + SPARE:
        for p in m["parts"]:
            if p["sound"] not in by:
                sys.exit("factory_multis: M%02d %s: no factory sound %r" % (m.get("slot", 0), m["name"], p["sound"]))
            s = by[p["sound"]]
            p["eng"], p["idx"] = ENGINES.index(s["engine"]), s["idx"]


_EDIT = {}


def edit_key(eng, label):
    """'E:CUT' on a part of engine eng -> 'P_E0 + k' (the engine's EDIT table, eng_*.c, read by tools/gen_duell_sounds.py)"""
    if not _EDIT:
        engines, _ = _gen_sounds().parse_engines()
        for i, e in engines.items():
            _EDIT[e["name"]] = {r[0]: k for k, r in enumerate(e["edit"])}
    labels = _EDIT.get(ENGINES[eng], {})
    if label not in labels:
        sys.exit("factory_multis: %s has no EDIT value %r (%s)" % (ENGINES[eng], label, ", ".join(labels)))
    return "P_E0 + %d" % labels[label]


# ---- the C table ------------------------------------------------------------------------------------------------------------------
def emit(path):
    lines = ["/* generated by tools/factory_multis.py: the factory multis built by tests/gen_factory_multis.c (do not edit) */",
             "static const multi_t MM[] = {"]
    for m in MULTIS:
        if m.get("kept"):
            continue
        g = []
        for k, v in m.get("g", {}).items():
            g += [k, str(value(G_LIST, k, v))]
        split = note(m["split"]) + 1 if m.get("split") else 0
        lines.append('    {"%s", %d, %d, %d, %d, {%s}, {' % (m["name"], m["slot"] - 1, MODES[m["mode"]], m.get("xf", 0), split,
                                                         ", ".join(g + ["END"])))
        for p in m["parts"]:
            st = []
            for k, v in p.get("set", {}).items():
                if k.startswith("E:"):
                    st += [edit_key(p["eng"], k[2:]), str(v)]
                else:
                    st += [k, str(value(P_LIST, k, v))]
            steps = p.get("steps")
            if steps and check_steps(steps, bool(p.get("poly")), p.get("bars", 1)):
                sys.exit("factory_multis: %s: %s" % (m["name"], "; ".join(check_steps(steps, bool(p.get("poly")), p.get("bars", 1)))))
            n = len(steps.split()) if steps else 16
            lines.append('        {%d, %d, "%s", {%s}, %s, %d, %d, %d},' % (p["eng"], p["idx"], p["sound"], ", ".join(st + ["END"]),
                                                                       '"%s"' % steps if steps else "0", n, p.get("bseed", 0), p.get("amt", 0)))
        lines.append("    }},")
    lines.append("};")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


BOOT_H = """/* tools/factory_multis.py: no factory multis (the build of the generator itself) */
#define DF_TEST_MULTIS DUELL_TEST_MODE
#define DF_N 0u
#define DF_TIDE DUELL_TIDE
static const uint8_t DF_SLOT[1] = {0};
static const uint16_t DM_LEN[1] = {0};
static const uint16_t DM_AT[1] = {0};
static const uint8_t DM_REC[1] = {0};
static const uint8_t *dm_rec(uint32_t i, uint8_t *out) { (void)out; return DM_REC + i; }
"""


def build():
    snd = sounds()
    resolve(snd)
    emit(GEN / "factory_multis_def.h")
    (GEN / "duell_multis.h").write_text(BOOT_H)
    exe = ROOT / "build" / "host" / "gen_factory_multis"
    exe.parent.mkdir(parents=True, exist_ok=True)
    cc = ["cc", "-O1", "-w", "-DDUELL_TEST_MODE=1", "-I" + str(GEN), "-Ibuild/gen", "-Ifirmware/src", "-Itests", '-DFELUCCA_VERSION="x"',
          "-o", str(exe), "tests/gen_factory_multis.c", "-lm"]
    if subprocess.run(cc, cwd=ROOT).returncode:
        sys.exit("factory_multis: tests/gen_factory_multis.c does not compile (run DUELL/gen_only.py first: build/gen)")
    if subprocess.run([str(exe), "build", str(DIR)], cwd=ROOT).returncode:
        sys.exit("factory_multis: the build failed")
    want = {"M%02d.bin" % m["slot"] for m in MULTIS}
    for f in DIR.glob("M*.bin"):
        if f.name not in want:
            f.unlink()
            print("removed", f.name)
    check(snd)


# ---- checks -------------------------------------------------------------------------------------------------------------------------
def parts_of(b):
    """the (engine, factory index, part start) of both parts of record b (MU_VER 8), the end position (after the automation)"""
    np_, pos, out = b[1], MU_G_AT + 2 * b[2], []
    for _ in range(2):
        out.append((b[pos], b[pos + 1], pos))
        flags, nb = b[pos + 3], b[pos + 4]
        pos += 5 + nb + np_ + NX                        # engine, preset, AMOUNT, flags, End B's length; End B (morph.c MBV); the values; the slots
        pos += 8 + MU_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (UPF_PK if flags & 1 else 0)
    return out, pos


def g_ids():
    core = re.sub(r"//[^\n]*", " ", re.sub(r"/\*.*?\*/", " ", (ROOT / "firmware" / "src" / "core.h").read_text(), flags=re.S))
    m = re.search(r"enum\s*\{\s*G_BPM\b(.*?)G_COUNT", core, re.S)
    names = ["G_BPM"] + re.findall(r"\b(G_\w+)\b", m.group(1))
    ids = {}
    for i, n in enumerate(names):
        ids.setdefault(n, i)
    for a, b in re.findall(r"#define\s+(G_\w+)\s+(G_\w+)\b", core):   # (the ids taken over under another name: G_DMDEP is G_SYNC, ..)
        if b in ids and a not in ids:
            ids[a] = ids[b]
    return ids


def stored_globals():
    """the global ids a multi stores, in order (multi.c MU_GLOBAL)"""
    g = g_ids()
    one = {g[n] for n in ("G_BPM", "G_SWING", "G_RTYPE", "G_RLOCUT", "G_PINGSPREAD", "G_DMDEP", "G_DMRATE", "G_DMODE", "G_SHPITCH", "G_SHMOD")}
    one |= set(range(g["G_DTIME"], g["G_CDEPTH"] + 1))
    return sorted(one)


def check_record(m, b, by_idx, P):
    slot, err = m["slot"], []
    if len(b) > REC_MAX or len(b) < MU_G_AT or b[0] != MU_VER or b[2] != len(stored_globals()):
        return ["M%02d: not a version %d record of at most %d bytes" % (slot, MU_VER, REC_MAX)]
    raw = b[8:20].split(b"\0")[0]
    if raw.decode("ascii", "replace") != m["name"] or any(not 32 <= c <= 126 for c in raw) or len(m["name"]) > NAME_MAX:
        err.append("M%02d: name is %r, expected %r" % (slot, raw, m["name"]))
    if b[4] != MODES[m["mode"]]:
        err.append("M%02d: MODE byte %d, expected %d (%s)" % (slot, b[4], MODES[m["mode"]], m["mode"]))
    parts, pos = parts_of(b)
    for t, (eng, idx, at) in enumerate(parts):
        have = by_idx.get((ENGINES[eng] if eng < len(ENGINES) else "?", idx))
        if have != m["parts"][t]["sound"]:
            err.append("M%02d part %d: engine %d sound %d is %r, expected %r" % (slot, t + 1, eng, idx, have, m["parts"][t]["sound"]))
        v = at + 5 + b[at + 4]                          # (the P_COUNT values follow the End B block and its length byte)
        if not b[at + 4]:
            err.append("M%02d part %d: no End B block" % (slot, t + 1))
        if b[v + P["P_FREED"]] - 64 not in (1, 2):
            err.append("M%02d part %d: the multimode filter's SLOPE is not 1 or 2" % (slot, t + 1))
        if b[v + P["P_DKSRC"]] - 64 not in (0, 1, 2, 3):          # (3 = OFF, 0.2.24: the default of a new sound)
            err.append("M%02d part %d: DUCK SRC is not PUMP / PART / MIDI / OFF" % (slot, t + 1))
    n = b[pos]
    if pos + 1 + 3 * n + 2 != len(b):
        err.append("M%02d: record length does not match its parts" % slot)
    else:
        split = b[-1]
        want = note(m["split"]) + 1 if m.get("split") else 0
        if not m.get("kept") and split != want:
            err.append("M%02d: SPLIT byte %d, expected %d" % (slot, split, want))
    gi = stored_globals().index(g_ids()["G_RLOCUT"])     # (MU_VER 8: only the stored globals, in id order)
    locut = b[MU_G_AT + 2 * gi] | b[MU_G_AT + 2 * gi + 1] << 8
    if locut != 75 and not m.get("owner"):          # (an owner's record keeps whatever he set)
        err.append("M%02d: reverb LOW CUT %d, not the default 75" % (slot, locut))
    return err


def p_ids():
    core = re.sub(r"//[^\n]*", " ", re.sub(r"/\*.*?\*/", " ", (ROOT / "firmware" / "src" / "core.h").read_text(), flags=re.S))
    m = re.search(r"enum\s*\{\s*P_LEVEL\b(.*?)P_COUNT", core, re.S)
    names = ["P_LEVEL"] + re.findall(r"\b(P_\w+)\b", m.group(1))
    return {n: i for i, n in enumerate(names)}


STEP_TOKEN = re.compile(r"(?P<rest>\.)|(?P<tie>-)|(?:n\d+(?:\+\d+){0,3}|[+-]?\d+)(?:v(?P<vel>\d+))?A*")


def check_steps(steps, poly=False, bars=1):
    """T1: the problems of a SEQUENCER arp pattern as text (an empty list: it is fine): every token is a rest '.', a tie '-', a note
    ('+7', '-5', '0', or 'n60+64' absolute) with an optional 'vNN' velocity 1..127 then any 'A' accents; a tie needs a note before it
    (not a rest, not step 0); the step count is 8 or 16, or 12 with poly, or 32 with bars=2"""
    err, prev = [], None
    toks = steps.split()
    for i, tok in enumerate(toks):
        m = STEP_TOKEN.fullmatch(tok)
        if not m:
            err.append("step %d '%s': not a rest, a tie, or a note with an optional vNN then A" % (i + 1, tok))
            prev = "bad"
            continue
        if m.group("vel") is not None and not 1 <= int(m.group("vel")) <= 127:
            err.append("step %d '%s': velocity %s is not 1..127" % (i + 1, tok, m.group("vel")))
        if m.group("tie") and prev in (None, "rest"):
            err.append("step %d: tie without a note" % (i + 1))
        prev = "rest" if m.group("rest") else "note"
    allowed = [8, 16] + ([12] if poly else []) + ([32] if bars == 2 else [])
    if len(toks) not in allowed:
        err.append("the pattern has %d steps, not %s (12 needs poly=True, 32 needs bars=2)" % (len(toks), " or ".join(str(n) for n in allowed)))
    return err


def check_similar(entries):
    """No two multis alike (owner, 0.2.23: "no two multis near-duplicates"). entries: (slot, name, mode, (sound 1, sound 2), split,
    ((engine, category) of part 1, of part 2), owner). Alike: the same two sounds (either order); one sound in the same place with the same
    MODE and split; or the same kind: both parts of the same engines and categories (either order) in the same MODE. Returns (bad, allowed):
    the clashes as text; a pair with an owner multi (0.2.24: his are never dropped or changed) is `allowed`, reported and not a failure."""
    bad, allowed = [], []
    for i, a in enumerate(entries):
        for b in entries[i + 1:]:
            why = None
            if set(a[3]) == set(b[3]):
                why = "the same two sounds"
            elif any(a[3][t] == b[3][t] for t in (0, 1)) and a[2] == b[2] and a[4] == b[4]:
                why = "the same part %d sound (%s) in the same MODE and split" % (1 if a[3][0] == b[3][0] else 2, a[3][0] if a[3][0] == b[3][0] else a[3][1])
            elif sorted(a[5]) == sorted(b[5]) and a[2] == b[2]:
                why = "the same kind: %s + %s in the same MODE" % ("/".join(a[5][0]), "/".join(a[5][1]))
            if why:
                line = "M%02d %s and M%02d %s: %s" % (a[0], a[1], b[0], b[1], why)
                owner = (len(a) > 6 and a[6]) or (len(b) > 6 and b[6])
                (allowed if owner else bad).append(line + (" (allowed: the owner's)" if owner else ""))
    return bad, allowed


def selftest():
    """check_similar must catch each kind of pair, pass distinct ones, and let an owner pair through (reported)"""
    k = lambda e1, c1, e2, c2: ((e1, c1), (e2, c2))
    base = (1, "A", 2, ("REESE BASS", "SQUELCH"), 0, k("WAVE", "BASS", "ANALOG", "BASS"), False)
    cases = [((2, "B", 2, ("SQUELCH", "REESE BASS"), 0, k("ANALOG", "BASS", "WAVE", "BASS"), False), 1, 0),      # the same two sounds, swapped
             ((2, "B", 2, ("REESE BASS", "DRAWBARS"), 0, k("WAVE", "BASS", "WAVE", "KEYS"), False), 1, 0),        # part 1's sound, same MODE and split
             ((2, "B", 2, ("GROWL BASS", "LADDER BASS"), 0, k("WAVE", "BASS", "ANALOG", "BASS"), False), 1, 0),   # the same kind
             ((2, "B", 0, ("REESE BASS", "DRAWBARS"), 0, k("WAVE", "BASS", "WAVE", "KEYS"), False), 0, 0),        # the sound shared, another MODE
             ((2, "B", 2, ("TINE ROOM", "VELVET BOW"), 0, k("FM6", "KEYS", "ANALOG", "PAD"), False), 0, 0),       # nothing shared
             ((2, "B", 2, ("GROWL BASS", "SQUELCH"), 0, k("WAVE", "BASS", "ANALOG", "BASS"), True), 0, 1)]        # the owner's: allowed, reported
    ok = True
    for e, want_bad, want_allowed in cases:
        bad, allowed = check_similar([base, e])
        ok &= len(bad) == want_bad and len(allowed) == want_allowed
    print("factory_multis: selftest of the similarity check %s" % ("ok" if ok else "FAILED"))
    ok_steps = True
    n16 = lambda head: head + " ." * (16 - len(head.split()))
    step_cases = [(n16("0A - - 3v90A"), {}, 0),                       # a tie after a note, a velocity before the accent
                  (n16("0 . - 3"), {}, 1),                            # a tie after a rest
                  (n16("- 0"), {}, 1),                                # a tie at step 0
                  (n16("0 - -"), {}, 0),                              # a chain of ties
                  (n16("0v0"), {}, 1),                                # velocity under 1
                  (n16("0v128"), {}, 1),                              # velocity over 127
                  (n16("0v127 +7v1A n60+64v64A"), {}, 0),             # the bounds, offsets, absolute notes
                  (n16("0Av90"), {}, 1),                              # the accent before the velocity
                  (n16("0v"), {}, 1),                                 # a velocity without a number
                  (" ".join(["0"] * 8), {}, 0),                       # LEN 8
                  (" ".join(["0"] * 12), {}, 1),                      # LEN 12 without poly
                  (" ".join(["0"] * 12), dict(poly=True), 0),         # LEN 12 with poly
                  (" ".join(["0"] * 32), {}, 1),                      # LEN 32 without bars
                  (" ".join(["0"] * 32), dict(bars=2), 0),            # LEN 32 with bars=2
                  (" ".join(["0"] * 15), dict(poly=True, bars=2), 1)] # any other count
    for pat, kw, want in step_cases:
        got = len(check_steps(pat, **kw))
        if bool(got) != bool(want):
            print("factory_multis: selftest step case %r %r: %d problems, wanted %s" % (pat, kw, got, "some" if want else "none"))
            ok_steps = False
    print("factory_multis: selftest of the step rules %s" % ("ok" if ok_steps else "FAILED"))
    return ok and ok_steps


def check(snd=None):
    snd = snd or sounds()
    resolve(snd)
    by_idx = {(s["engine"], s["idx"]): s["name"] for s in snd}
    cat = {s["name"]: (s["engine"], s["cat"]) for s in snd}
    P = p_ids()
    err, entries = [], []
    if len({m["slot"] for m in MULTIS}) != len(MULTIS) or len({m["name"] for m in MULTIS + SPARE}) != len(MULTIS) + len(SPARE):
        err.append("two multis share a slot or a name (the spare ones counted)")
    if sum(1 for m in MULTIS if m.get("tribute")) > 3:
        err.append("more than three tribute multis (owner: at most 2 or 3)")
    for m in MULTIS:                                  # (T1: a pattern is 8 or 16 steps, or 12 on a part with poly=True (a polymeter on purpose), or 32 on a part with bars=2 (two bars): one token short drifts a 16th a bar against the other part, M16 had 15; and the step grammar: ties, velocities)
        for t, p in enumerate(m["parts"]):
            if p.get("steps"):
                err += ["M%02d part %d: %s" % (m["slot"], t + 1, e) for e in check_steps(p["steps"], bool(p.get("poly")), p.get("bars", 1))]
    for m in MULTIS:
        f = DIR / ("M%02d.bin" % m["slot"])
        if not f.exists():
            err.append("M%02d: %s missing" % (m["slot"], f.name))
            continue
        b = f.read_bytes()
        err += check_record(m, b, by_idx, P)
        if not m.get("stress"):
            snds = (m["parts"][0]["sound"], m["parts"][1]["sound"])
            entries.append((m["slot"], m["name"], b[4], snds, b[-1], (cat[snds[0]], cat[snds[1]]), bool(m.get("owner"))))
    for m in SPARE:                                   # (the spare records exist and name their sounds)
        for t, p in enumerate(m["parts"]):
            if p.get("steps"):
                err += ["spare %s part %d: %s" % (m["name"], t + 1, e) for e in check_steps(p["steps"], bool(p.get("poly")), p.get("bars", 1))]
        f = ROOT / "tests" / "scenes" / (m["scene"] + ".bin")
        if not f.exists():
            err.append("spare %s: %s missing" % (m["name"], f.name))
        else:
            parts, _ = parts_of(f.read_bytes())
            for t, (eng, idx, _at) in enumerate(parts):
                if by_idx.get((ENGINES[eng], idx)) != m["parts"][t]["sound"]:
                    err.append("spare %s part %d: the record holds %r, the list says %r" % (m["name"], t + 1, by_idx.get((ENGINES[eng], idx)), m["parts"][t]["sound"]))
    sim, allowed = check_similar(entries)
    for e in err + sim + allowed:
        print("factory_multis:", e)
    print("factory_multis: %d multis checked, %d record problems, multi similarity: %d pairs of %d alike, %d allowed (the owner's); %d spare" % (
        len(MULTIS), len(err), len(sim), len(entries) * (len(entries) - 1) // 2, len(allowed), len(SPARE)))
    return not err and not sim


def listing():
    snd = sounds()
    resolve(snd)
    num = patch_numbers(snd)
    for m in MULTIS:
        print("M%02d %-12s %-6s %-5s %s + %s%s | %s" % (m["slot"], m["name"], m["mode"], "owner" if m.get("owner") else "kept" if m.get("kept") else "mine",
                                                       *("#%d %s" % (num[p["sound"]], p["sound"]) for p in m["parts"]),
                                                       "  split " + m["split"] if m.get("split") else "", m.get("what", "")))
    for m in SPARE:
        print("--  %-12s %-6s spare %s + %s | %s" % (m["name"], m["mode"], *("#%d %s" % (num[p["sound"]], p["sound"]) for p in m["parts"]), m.get("what", "")))


def patch_numbers(snd):
    """sound name -> #NN, from docs/PATCH_NUMBERS.md (the PATCHES list)"""
    out = {}
    for line in (ROOT.parent / "docs" / "PATCH_NUMBERS.md").read_text().splitlines():
        m = re.match(r"\|\s*(\d+)\s*\|\s*(.+?)\s*\|", line)
        if m:
            out[m.group(2).replace(" (NEW)", "")] = int(m.group(1))
    return out


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    os.chdir(ROOT)
    if cmd == "build":
        build()
        sys.exit(0 if check() else 1)
    if cmd == "check":
        sys.exit(0 if check() else 1)
    if cmd == "selftest":
        sys.exit(0 if selftest() else 1)
    if cmd == "list":
        listing()
        sys.exit(0)
    sys.exit(__doc__)
