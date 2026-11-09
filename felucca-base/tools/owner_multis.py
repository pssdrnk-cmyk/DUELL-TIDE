# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.24: multis the owner saved on his unit, written by tools/import_owner_edits.py (1.0: slots 1-5, 8-15 from duell-multis-13, copied byte for byte and CRC-checked; D31, 2026-11-05: the list is the nine of the curated order that are the owner's, in their new slots) (docs/OWNER_REFINE.md). The record of each is tools/factory_multis/Mnn.bin,
kept byte for byte; tools/factory_multis.py apply_owner puts these in its list as `kept`, `owner` (exempt from the similarity check)."""

OWNER_MULTIS = [
    dict(slot=2, name='DUB CHORD', mode='LAYER', parts=[dict(sound='BIG STAB'), dict(sound='TIDAL SUB')]),   # D31: was M04; TIDAL SUB's line an octave up (C2 to C3), its LEVEL down 4
    dict(slot=10, name='PAD SPLIT', mode='LAYER', parts=[dict(sound='EVOLVING PAD'), dict(sound='EMBER GLOW')]),   # was M03
    dict(slot=11, name='WHISPER TIDE', mode='LAYER', parts=[dict(sound='DIGITAL PAD'), dict(sound='WHISPER PAD')]),   # was M05
    dict(slot=12, name='TINE STRINGS', mode='LAYER', parts=[dict(sound='STRING MACH'), dict(sound='TINE ROOM')]),   # was M06
    dict(slot=16, name='FINAL REEL', mode='LAYER', parts=[dict(sound='BRASS PAD'), dict(sound='STRING MACH')]),   # was M07
]
