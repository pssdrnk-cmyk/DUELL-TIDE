# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.24: multis the owner saved on his unit, written by tools/import_owner_edits.py (1.0: slots 1-5, 8-15 from duell-multis-13, copied byte for byte and CRC-checked; D31, 2026-11-05: the list is the nine of the curated order that are the owner's, in their new slots) (docs/OWNER_REFINE.md). The record of each is tools/factory_multis/Mnn.bin,
kept byte for byte; tools/factory_multis.py apply_owner puts these in its list as `kept`, `owner` (exempt from the similarity check)."""

OWNER_MULTIS = [
    dict(slot=2, name='DUB CHORD', mode='LAYER', parts=[dict(sound='BIG STAB'), dict(sound='TIDAL SUB')]),   # D31: was M04; TIDAL SUB's line an octave up (C2 to C3), its LEVEL down 4
]
