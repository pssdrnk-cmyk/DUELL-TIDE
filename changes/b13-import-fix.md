## roadmap
### Planned, not yet scheduled
- **MORPH TO and DUCK SRC (found by b13, tests/tide_keep_test.c):** a MORPH TO end switches the part's DUCK SRC to the target sound's at the middle (`morph_xfer.c` morph_xnote), but the End B block never stores DUCK SRC (`morph.c` mbv_never), so a saved patch whose target ducks from another source plays the part's own SRC at B after loading. Either the morph keeps the part's SRC (DUCK is a mix tool and never rolls) or the block stores it. No factory pair shows it since b13 (HOUSE PIANO's SRC matches SLAP DOCTOR's, DEPTH 0).
