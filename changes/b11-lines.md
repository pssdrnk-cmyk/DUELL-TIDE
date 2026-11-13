## changelog
### 1.0.0-b10
- **Fixed (owner on rc8: "3 vertical lines for a couple of milliseconds" when a screen is redrawn):** a page change within one layout (HOME, FX, ENV, LFO and the other card pages) no longer cuts three thin dark lines through the old page before the new one lands. The gaps beside the columns are filled at the end of the redraw, and only where nothing else drew; the finished picture is the same, and a page change sends 2,000 to 3,000 fewer pixels (about 2 ms).
