<!-- Thanks. Keep it small and focused: one change per pull request. A fix that plain DUELL needs too is offered to OSCILL/R's DUELL separately, as a small device-tested pull request from a branch based on their main; the fork owner opens that one. TIDE stays in this fork. -->

## What and why

One or two plain sentences: what you get, and why.

## Checked

- [ ] It builds and fits (`python3 tools/build.py` in `felucca-base`; say the flash and RAM numbers if they moved)
- [ ] Looked at on the unit, in the [emulator](https://pssdrnk-cmyk.github.io/DUELL-TIDE/emu/) or in a picture ("not tried" is fine, say it)
- [ ] Documents: links work, facts match the code, [docs/DOCS_INDEX.md](https://github.com/pssdrnk-cmyk/DUELL-TIDE/blob/jam11/docs/DOCS_INDEX.md) updated if a document was added or moved
- [ ] A line in `changes/` for what a player will notice

The full test suite runs once per build, on the package that is flashed, not on every pull request. A change to the audio interrupt, the loader or the save format also runs the test groups it touches; name them.

## Notes

LF line endings, GPL-3.0-only header on new source files. See [CONTRIBUTING.md](https://github.com/pssdrnk-cmyk/DUELL-TIDE/blob/jam11/CONTRIBUTING.md). The credit line stays as it is: TIDE for FM-1 by pssdrnk. Built on DUELL (OSCILL/R) and Felucca (Hügelton Instruments). GPL-3.0.
