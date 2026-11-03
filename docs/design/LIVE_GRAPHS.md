# LIVE GRAPHS (TIDE): design note, rework of JAM 1.0's ui_live.c

The old file is a behaviour reference only. Rule kept from the owner: every picture shows what it showed before; the animation only adds the playhead or the live value. Everything is behind `DUELL_TIDE` (`jam/check_off.sh` stays byte-identical); MENU > ANIMATION OFF (`PREF_ANIM_OFF`) draws none of it.

## What moves
| Page | Moves |
|---|---|
| AMP ENV / DEST, ENV 2 / DEST | line + dot of the latest note along the drawn curve, the played part lit; ENV 2 only while the audio ticks it; AMP ENV on FM6: nothing (inert) |
| OP ENV, AMP DEST on FM6 (draws the OP ENV picture) | line + dot on the selected operator's real envelope |
| LFO 1 / 2 (3 pages each) | line + dot on the audio phase; an LFO 2 nothing reads: the same marker on a UI clock + PREVIEW |
| CYCLE ENV 1/2, 2/2 | real rise/fall position of the latest note; unrouted / no note: UI clock + PREVIEW |
| MULTI FILTER, TWIN | the response at the live cutoff + a tick; a tick per filter |
| INSERT (phaser, flanger, chorus) | line + dot on the sweep |
| MATRIX | a 2 px bar under each acting row (strips, the text is not redrawn) |
| CHORD | a bar in each sounding key |
List pages (MULTIS, PATTERNS, SAVE) and the TIDE pages: no motion.

## One shared layer, not per-page cases
1. **Taps** (read-only, pure): `env(t,e2)` stage + level of voice `m_vi`; `fm(op)` the operator's `fm6_env_t`; `phase(src)` LFO 1/2, INSERT, CYCLE as one Q32 phase; `cut(t)` `lvc/lvc2` (the audio writes two pad bytes, never reads them); `src(slot)` `mod_tsrc`; `lvl(voice)`. No tap ticks a generator or adds a route.
2. **One marker**: every kind ends in `lv.x, lv.y` (canvas px of the picture) + `lv.pv` (from a UI clock). One `lv_mark()` draws line + dot for ENV, OP ENV, LFO, INSERT, CYCLE. Only FILTER/TWIN/MATRIX/CHORD draw their own few rects.
3. **One sweep**: LFO, INSERT, CYCLE are "phase over N drawn cycles": one function maps phase to x (and counts which cycle).
4. **One UI clock** for PREVIEW: a Q32 phase advanced by `rate x (fm1_ms - last)`; LFO 2 and CYCLE only differ in their rate. One PREVIEW label (caption row, MID, no ground), drawn by the layer whenever `lv.pv`.
5. **One curve walk** for the envelopes: a pixel walk along graph_adsr's curve (attack, exp decay, sustain, exp release) finds the point for a level and draws the lit part to it (replaces the old inverse lookup + three-phase lit drawing).

## One frame clock
`live_sig()` (called from `graph_signature`) looks at 30 a second (`ui.frame` even) or 15 for pictures costing more than ~80 k basic blocks (table, one number per kind); a still marker is not drawn again; ANIM OFF = kind NONE. Time-based creep: on a held sustain the marker moves 8 px a second whatever the look rate.

## Fit with 1.1
Geometry is read from the pictures (76-row card, `GOY`, x 12..226); the pictures are not touched. Colours are tokens only: playhead `T_ACCENT`, dot `T_TEXT`, lit curve `T_ACCENT` (the text tone where a light palette makes the accent the part colour), tracks `T_RAISE`, label `T_MID`: dark and light palettes read. Cost (measured, basic blocks): a picture redraw 87..150 k, so animation = redraw rate; MATRIX bars as strips cost ~500 per frame instead of 161 k.

## Better than the old version
- about 40 % less code (one marker, one sweep, one clock, one curve walk; goal under 400 lines against 535 old / 629 first port);
- PREVIEW looks and behaves the same everywhere; frame-rate independent creep; no hand-kept per-voice dots; matrix without page redraws; FM6 pages follow the real operator envelope.
