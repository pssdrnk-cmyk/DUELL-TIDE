# VOICE controls: when each one is heard (DUELL 0.2.25)

The owner asked which VOICE controls do something, and when. This table is read from the firmware (`firmware/src/voice.c`, `fx.c`), not assumed.
The editor hides a control that cannot be heard in the current setting and shows the reason in its place (`web/editor/layout.js` `voiceWhy`).
The unit itself shows every VOICE card all the time; nothing on the unit changed.

| Editor label (unit label) | What it does | When it is heard | When the editor hides it |
| --- | --- | --- | --- |
| VOICES (VCE) | POLY: chords, one voice per key. MONO: one note, every new key restarts the envelopes. LEGATO: one note, a key pressed while another is held glides there without a restart. UNISON: one note on several voices at once | always | never |
| NOTE PRIORITY (PRIO) | Which held key sounds when only one note can: LAST pressed, LOWEST, HIGHEST (`mono_pick`) | MONO, LEGATO, UNISON | VOICES is POLY |
| UNISON DETUNE (DTUNE) | How far the unison voices are spread in pitch: at 127 the outer voices are about +-23.5 cents from the middle (`mono_play`; fewer voices are spread as wide) | UNISON | VOICES is not UNISON |
| GLIDE (GLD) | The slide from the last pitch to the new one. In POLY each voice glides from the pitch it played last; in MONO every note glides; in LEGATO and UNISON only between held keys. It also sets the speed of the arpeggiator's TARGET moves and of a step's SLIDE | always (0 is off) | never |
| GLIDE MODE (GLMOD) | RATE: the same speed for every interval, 64 semitones in the time GLD shows (an octave takes 12/64 of it; a wide jump takes longer). TIME: every interval in the time GLD shows, 1 ms to 10 s (`glide_set`) | GLIDE above 0 | GLIDE is 0 |
| STEREO SPREAD (SPRD) | The voices alternate left and right around PAN (`fx.c` `spread_on`) | POLY and UNISON (not in the output MODE DUEL, where the unit hides it too) | VOICES is MONO or LEGATO |
| PATCH LEVEL (TRIM) | The sound's own loudness, saved with the patch | always | never |

The number of unison voices is not a control: UNISON uses every voice the engine has (its voice cap; FM6 and other heavy engines fewer).

## Findings for the owner (no change made; the owner decides)

- **No VOICE control is dead in the firmware**: each one is read in the cases above.
- **VOICE ALLOCATION is no control any more**: the stored byte is ignored. A new POLY note rotates through the free voices; with GLIDE above 0 it takes the free voice nearest in pitch (`voice.c` `voice_alloc`).
- **GLIDE MODE TIME with GLIDE at 0 and a step SLIDE**: `glide_set` with TIME and GLIDE 0 takes GLD 0's 1 ms for the whole interval, so a pattern's SLIDE all but jumps instead of the slide it has with RATE (GLD 40's rate: an octave in about 3.5 ms). The editor hides GLIDE MODE at GLIDE 0 (the usual case is right); this corner is a firmware question.
- **STEREO SPREAD in DUEL**: the unit hides it in DUEL (each part on its own output); the editor cannot read the output MODE, so it can show it there.
