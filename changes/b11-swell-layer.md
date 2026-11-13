## changelog
### 1.0.0-b10
- **Fixed (SWELL / RIPPLE on the quick sequence layer):** with SEQ held, the layer did not redraw when a waiting SWELL or RIPPLE landed at the loop start (or a stopped transport applied it), so the new notes were not seen. The layer now redraws on landing, the SWELL or RIPPLE box that waits for the loop start is lit, and the header line "SWELL  NEXT LOOP" stays until it lands, however long the loop.
