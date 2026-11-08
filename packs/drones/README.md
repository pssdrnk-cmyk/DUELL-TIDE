# Drone scenes (file pack)

Eight multis for the eight factory drone sounds (#164 to #171), built with the firmware's own multi code (`felucca-base/tools/drone_pack.py`). Each is a LAYER of two latched drones (HOLD ALL, arp OFF) with a slow ping-pong echo and a big hall, 60 BPM.

| Multi | Part 1 | Part 2 |
|---|---|---|
| DUNE THROAT | SAND THROAT | SOLAR WIND |
| DUNE HORN | HORN SWELL | VOID SHIMMER |
| NEON ROOF | NEON BRASS | RAIN STRINGS |
| RAIN CITY | RAIN STRINGS | GRID HUM |
| VOID DEEP | VOID SHIMMER | SOLAR WIND |
| SOLAR DRIFT | SOLAR WIND | HORN SWELL |
| GRID BUZZ | GRID HUM | NEON BRASS |
| IRON YARD | IRON WORKS | SAND THROAT |

## Load them

1. Back up first (the web editor's Backup page): an import replaces what the slot holds.
2. Open the web editor, connect the unit, go to the MULTIS tab and press **Import file...** (or drop `drones.duell-multis.json` on the tab).
3. Pick a multi from the file and the unit slot to write it to (the file suggests M09 to M16; any slot works). Press **Write into the slot**. Repeat for each multi you want. A slot you write replaces the factory multi shown there.
4. On the unit: pick the multi, then TIDE SETTINGS > STYLE > DRONE. A multi does not carry TIDE's STYLE, so this is a one-time pick (it is saved).
5. Play: hold a key or a two or three note chord around C3 (HOLD latches it), press PLAY with AUTO TIDE on, and let it move.

To rebuild the file: `python3 gen_only.py`, then in `felucca-base` `python3 tools/drone_pack.py` (needs a C compiler; the same host tools as the tests).
