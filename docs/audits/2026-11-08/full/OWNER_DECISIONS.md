# Owner decisions asked by the round-2 audit (2026-11-08)

Only questions the owner can answer (musical or product choices). Each names the finding in `AUDIT_REPORT.md`. The
coordinator's recommendation is first. Numbers continue the round-1 series (D1..D41 in
`../full-audit-2026-11-04/OWNER_DECISIONS.md`); the CPU audit of the same day asks its own in
`../cpu-audit-2026-11-08/CPU_AUDIT.md` section 7 and is not repeated here.

## CPU and sound

- **D42 (finding 1, B-01)** The GLIDE sweep after the b2 fix still reaches raw peaks of 91 to 101 % on five of the 16
  multis, with one missed block on M11. The ROADMAP records that you accept about 3 misses per FM6 transition "for now".
  Question: does that acceptance cover the first stable `1.0.0`, or must the stable build show 0 misses over the 16-multi
  GLIDE sweep (which means the ease ladder K3 or a lower cap on those five multis comes before 1.0.0)?
  Recommendation: 0 misses is the bar for `1.0.0`; betas may ship with the accepted misses.
- **D43 (finding 2, B-02)** The NOTES cap acts only when a new note starts. A chord or drone already held when a GLIDE
  or crossing begins keeps every note at the higher cost. Question: when a crossing starts under a held chord, may the
  firmware fade one note of that chord (a one-block fade, no click), or must a held chord always stay whole, so the
  charge must size the chord from the start (fewer notes on drones whenever AUTO TIDE GLIDE is on)?
  Recommendation: a held chord stays whole; measure the drones with a running GLIDE first (finding 2), decide after.
- **D44 (finding 12, A-15)** On the unit (b2) every moving fader was blended every other block; the merged test fix
  limits that saving to a running GLIDE, so a hand- or MIDI-moved fader is back on the every-block blend the unit
  measured at about 70 us a block. The blend trails the fader by 0.73 ms either way (inaudible by reading, not listened).
  Question: should a hand or MIDI move also blend every other block?
  Recommendation: yes, the same path for both; it is one condition.

## Process

- **D45 (finding 3, E-01, E-07)** `36f03cd` and `fb0848c` were flashed while 3 to 4 host groups were red and the
  plain-DUELL hash was unpinned (all fixed and merged on 2026-11-08). Question: may a test image be flashed while the
  full suite is red? Recommendation: a test-mode image yes, when the red groups are named in the LOG; a tagged beta
  never; a stable never.
- **D46 (finding 25, E-15)** The `tideroles` guard (roles spec T2) is red on today's multis: 43 of 338 checks, mostly
  the 30 to 120 Hz band and loudness within 3 dB of the median (M04 +3.8 dB, M10 +5.3, M12 -3.1, M15 -7.2, M16 -9.2).
  Question: fix the multis' levels (a mix decision you hear on the unit) or relax the spec's bounds?
  Recommendation: trim M15 and M16 up and M10 down by the numbers, re-measure, keep the bounds.

## Interface

- **D47 (finding 29, D-16)** OCT- held with K3 or K4 resets TIME and WAIT to the STYLE's values and says nothing on
  the screen (K1 says BACK TO ORIGINAL, K2 shows the drop message). Question: should K3 / K4 show the value they set
  ("TIME 16", "WAIT 2"), about 40 B of flash? Recommendation: yes.

## Storage (coordinator's call unless you object)

- **D48 (finding 33, C-06)** After a migration from an older TIDE build the old settings object 48 stays in the
  flash, so a return to `stable-2026-11-03` and back keeps the pre-migration TIDE values. The coordinator recommends
  keeping it (a return then keeps your settings) and fixing the comment that says otherwise. Say so if you want it wiped.
