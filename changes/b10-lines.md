## changelog
### 1.0.0-b10
- **Fixed:** the two thin vertical lines that blinked on the screen during a page change (rc6, seen on the unit only). Since the screen redraw of rc6 a page change painted the new layout's gaps and gutters first, over the OLD page still on the glass, so thin background stripes cut through its cards for the ~40 ms the cards took to draw. A change to another grid layout now fills the whole body first again (one blank, as before), the same layout only its gaps. Pictures unchanged (`pages_fp`, `emu_smoke`); test `uigaps`.
