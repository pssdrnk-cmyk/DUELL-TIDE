## changelog
### 1.0.0-b14 (in progress)
- **Fixed: the screen no longer blinks when a quick menu opens or closes, or when a button changes the page.** A full redraw filled the whole screen with the background first and the new page came in over it; now each pixel goes straight from the old picture to the new one. The MIXER's LOW / MID / HIGH bands, the cards under QUICK SEQ's and PARAMETER LOCKS' step strip, the dialogs and the count-in (which blinked on every beat) are fixed the same way.
- **Fixed: a value ending in 4 no longer touches its unit** ("54ms" on VOICE 2/2's GLIDE, and on every card and cell).
- **Fixed: on WAVE ACID / ACSQ the filter pictures draw the resonance peak you hear** (they used the ordinary RES law).
