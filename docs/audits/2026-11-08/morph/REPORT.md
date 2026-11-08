# MORPH design review, 8 November 2026 (b4/integ)

Read-only review by a reviewer agent, saved by the coordinator. Nothing was built, tested or heard for this report.

## Verdict
- **Keep the morph core.** TIDE is built on it. Every sound change TIDE makes is an end B plus the fader moving: GLIDE's sound lane, DRIFT, WNDR, JRNY, ROLL methods, roles, WASH, the valley and the FM6 bridge, and KEEP.
- **These don't use it:** SEQ MORPH (its own ends and fader), VARY, TURNS, POLY, SYNC, ARC, dynamics and STYLE.
- **The MORPH page is the only optional part** (about 1.3 KB of flash). Keep it as the hand page and the wave card's home.
- **No big rework.** The blend is linear over values that are already perceptual (log tables), the cutoff is slewed, and there are no duplicate paths: everything goes through morph_do.

## How it works
- **Ends:** each part has Original, A and B, each a full parameter set plus the FM6 bytes. Engine, level, tuning, voice mode, arp and LFO levels never move. Each end keeps a 3-deep history for UNDO.
- **Blend:** runs in the audio interrupt per part and block, only over the values that differ, and is cached while nothing changes.
- **Values that can't blend:** choices cross-fade, switches flip at the middle, matrix routes fade out and back in.
- **Size:** about 25 KB of code; RAM `mph` 6,944 B plus `mwork` 1,240 B.

## Weaknesses a player would notice
| # | What | Fix | Cost |
|---|---|---|---|
| W1 | A fader parked by hand near the middle (40-60 %) between ends that differ in a switch stays dulled (filter valley), and FM6 stays on pure sines (5-95 %). Normal after JRNY. | c2: after about 1 s at rest, settle the dip and the bridge onto the nearer side. | about 100 B, audio interrupt, listen on the unit |
| W2 | AMOUNT (K4) does nothing in JRNY, and every factory load resets it to 40. | c3: loads keep AMOUNT; K4 dim in JRNY. | about 50 B |
| W3 | UNDO is 2 steps deep, and TIDE's own rolls fill it, so after a tide session it undoes TIDE, not you. | c1: drop the history (no UNDO): frees about 1.9 KB of RAM, enough for the wave card. Owner's choice. | probe build, storage and JRNY tests |
| W4 | A part with an end B pays the morph budget even at rest (fewer notes). | Already planned: "Transition costs only while moving". | |
| W5 | K2 ROLL (an action) and the TIDE SETTINGS ROLL row (a method) share a name. | Minor. | |
