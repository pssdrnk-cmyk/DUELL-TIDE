# Builds, branches and building it yourself

Moved here from the README. The README says which build to install; this page lists them all.

## Builds: stable and beta

TIDE has its own version, `MAJOR.MINOR.PATCH`, kept in the file [`TIDE_VERSION`](../TIDE_VERSION) and shown in MENU > SYSTEM > ABOUT beside the DUELL version (1.1). Every build flashed for testing is a **beta**: `1.0.0-b1`, `1.0.0-b2`, and so on. The first build the fork owner has validated on the unit is `1.0.0`; a fix after that is `1.0.1`, a new feature `1.1.0`. The git tag, the GitHub release and the section of the [CHANGELOG-TIDE](../CHANGELOG-TIDE.md) carry the same name (the tag has `tide-` in front, as in `tide-1.0.0-b1`, so it cannot be mistaken for OSCILL/R's own `v1.0` and `v1.1`). A `+` after the version in ABOUT (`1.0.0-b1+`) means the build is not exactly the tagged release: built from a working copy with changes, or before the tag existed (a build made before its tag existed shows the `+`). The commit is named in the release notes.

| Channel | Build | Date | Status |
|---|---|---|---|
| **Stable** | [`stable-2026-11-03`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/stable-2026-11-03) (`187e9d0`), branch [`stable`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/stable) | 2026-11-03 | **Tested and validated** on the fork owner's FM-1. It is older than the versions: its ABOUT shows a commit, not a TIDE version. The first versioned stable will be `1.0.0`. Build this one unless you want to help test. |
| **Beta** | [`1.0.0-b9`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/tide-1.0.0-b9) (tag `tide-1.0.0-b9`), on branch `jam11` | [GitHub pre-release](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/tide-1.0.0-b9), 2026-11-11 | **Under test.** The newest published beta; it carries `1.0.0-b7` and `1.0.0-b8`, which were never released on their own. All 18 factory multis re-voiced, genre knobs on HOME page 1 (KNOBS row), AUTO TIDE with EVOLVE and TRAVEL, USB audio off by default, a smaller USB resampler. What it holds: [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b9). |
| **Beta** (previous) | [`1.0.0-b6`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/tide-1.0.0-b6) (tag `tide-1.0.0-b6`) | [GitHub pre-release](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/tide-1.0.0-b6), 2026-11-09 | The beta before b9; it carries 1.0.0-b5, which was not released on its own. On top of `1.0.0-b4`: the MORPH page shows the real sound as waves, like an oscilloscope; one part selection, PART 1 / PART 2 / BOTH, on MORPH, TIDE SETTINGS and AUTO TIDE; the update screen in TIDE's look; MORPH UNDO is gone (its RAM went to the wave picture). New in b6: two drone scenes built in (M25 AMB DUNEHORN, M18 AMB VOID); loading a factory sound or multi keeps your AMOUNT; pattern notes stay in the register you wrote; three FM6 sounds no longer jump in level at End B; JOURNEY travels to sounds close in level and brightness; 4.8 KB of flash freed. What it holds: [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b6). |
| **Beta** (not released) | `1.0.0-b5` | Never published | Not released separately: everything in it is in `1.0.0-b6`. [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b5). |
| **Beta** (previous) | [`1.0.0-b4`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/tide-1.0.0-b4) (tag `tide-1.0.0-b4`) | [GitHub pre-release](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/tide-1.0.0-b4), 2026-11-09 | The ease ladder (no missed audio blocks or cut voices on the unit's GLIDE sweep), STYLE DRONE and the drone pack, cleaner screens, the arp follows TEMPO SWING. [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b4). |
| **Beta** (older) | [`1.0.0-b3`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/tide-1.0.0-b3) (tag `tide-1.0.0-b3`) | [GitHub pre-release](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/tide-1.0.0-b3), 2026-11-08 | HOME without MODE, keys record into ARP SEQ with a REC row, the CPU pill and badge, the PATCHES order. [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b3). |
| **Beta** (older) | [`1.0.0-b1`](https://github.com/pssdrnk-cmyk/DUELL-TIDE/tree/tide-1.0.0-b1) (tag `tide-1.0.0-b1`) | flashed 2026-11-05; [GitHub pre-release](https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/tide-1.0.0-b1) | The first versioned build. It replaces `beta-2026-11-04` (adds STYLE, ARC, SYNC, TURNS LEAD, POLY, the USER slots, 32 more sounds, new patterns and multi sequences, list previews, JRNY role families and live graphs) and fixes its three known issues: the SAVE > MULTIS page layout, the TIDE page step bars not always matching what plays, and a latched arp playing on after STOP. What it holds: [CHANGELOG-TIDE](../CHANGELOG-TIDE.md#100-b1). |
| **Beta** (oldest) | `beta-2026-11-04` | 2026-11-04 | A GitHub pre-release; `1.0.0-b1` replaces it. |

Links from the project's pages (installer, emulator, issue forms) to the guides point at the `stable` branch, so they describe the firmware you can install; only the development documents stay on `jam11`.

`jam11` is the development line: it holds the **beta** (and what comes after it). A build becomes stable only after the fork owner has played and validated it on the unit. To build the stable one, check out the `stable` branch first (see [Build it yourself](#build-it-yourself) below).

## Build it yourself

1. Pick the build: `git checkout stable` for the validated one, or stay on `jam11` for the beta, or `git checkout tide-1.0.0-b1` for exactly that version ([Builds](#builds-stable-and-beta)).
2. Build the package as in [felucca-base/BUILDING.md](../felucca-base/BUILDING.md): `python3 tools/build.py` in `felucca-base`. TIDE is on by default. The result is `felucca-base/build/duell-1.1.fwsc`.
3. Install it with `python3 ../jam/jam_dev.py flash build/duell-1.1.fwsc` (needs `pip install mido python-rtmidi`). It saves a backup of the unit first, then writes the package with `tools/fm1_install.py`. Keep the computer awake.

While it writes, the unit shows the update screen (WRITING, a percent number, DO NOT SWITCH OFF). Don't unplug until the installer says it is done, then switch the unit off and on once.

TIDE keeps the version string DUELL 1.1 so every stock tool takes the unit for DUELL; the AUTO TIDE page in HOME's round shows that TIDE is on the unit, and the installer, the editor and `jam_dev.py info` read a TIDE flag in its INFO and say "with TIDE".

**The way back:** install the original DUELL 1.1 package from [OSCILL/R's releases](https://github.com/oscillr/DUELL/releases) the same way (or with their installer page), or return to M-VAVE's official firmware (V15) as in [INSTALL.md](INSTALL.md). Your backup restores with `jam_dev.py restore` or the editor's Backup page.

## Branches

`jam11` is the development line (the name is the project's old code name). `stable` is the build the fork owner has validated; the tags `tide-1.0.0-b1`, `tide-1.0.0-b2`, `tide-1.0.0`, ... mark the published builds (the older ones are `stable-2026-11-03` and `beta-2026-11-04`). `main` is meant to mirror OSCILL/R's DUELL, the base of a pull request that belongs to plain DUELL, and carries no TIDE.
