# Contributing to DUELL with TIDE

DUELL with TIDE is pssdrnk's fork of DUELL 1.1 by OSCILL/R. It adds TIDE. This page is for the fork; OSCILL/R's own rules for plain DUELL are in [felucca-base/CONTRIBUTING.md](felucca-base/CONTRIBUTING.md).

## Where things go

| What | Where |
|---|---|
| A bug you can reproduce | This fork's [issues](https://github.com/pssdrnk-cmyk/DUELL-TIDE/issues/new/choose), bug report form: one bug per issue |
| An idea or request | The same place, idea form: one idea per issue (no promise it will be added) |
| A bug that plain DUELL has too | OSCILL/R's [issues](https://github.com/oscillr/DUELL/issues/new/choose). If you are not sure, report it here and say so |
| A question about using TIDE | Read [docs/TIDE_GUIDE.md](docs/TIDE_GUIDE.md) and [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) first |

Please read [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) before you report. Back up your unit before you test ([docs/RECOVERY.md](docs/RECOVERY.md)).

## Pull requests

Small and focused, one thing at a time. Say what you changed and what you tested, and on what: a real FM-1, the emulator, or the computer only. A change to the sound, the sequencer or storage is best tried on a real FM-1, and comes with a host-test group that fails without it and passes with it (`./host_tests.sh group`). By contributing you agree that your work is licensed under GPL-3.0-only; new source files carry the SPDX header (`SPDX-License-Identifier: GPL-3.0-only`).

**Fixes that belong in plain DUELL.** A fix that is also right for DUELL 1.1 without TIDE is offered to OSCILL/R's DUELL as a small, device-tested pull request, made from a branch based on OSCILL/R's `main` and not on a TIDE branch. OSCILL/R welcomes small, device-tested pull requests, and the fork owner opens these ones: say in an issue here which change you mean. TIDE itself stays in this fork (since b10 there is no plain build here any more: TIDE is the only firmware).

## Build

The firmware builds with the JieLi toolchain, which runs on Linux x86-64, or in Docker on macOS. Steps and prerequisites: [felucca-base/BUILDING.md](felucca-base/BUILDING.md). In short, in `felucca-base`:

```
python3 tools/build.py
```

TIDE is the firmware (the plain `DUELL_TIDE=0` build was retired in b10). The package is `felucca-base/build/duell-1.1.fwsc`. On Windows, use Git Bash and `. jam/env.sh` (it expects a work folder with the toolchain next to the repository).

## Test

| Where | Command |
|---|---|
| Linux, macOS | `./host_tests.sh` (all groups) or `./host_tests.sh group ...` (the groups are named at the top of the script) |
| Windows (Git Bash) | `sh jam/check.sh` (the device build, then every group) or `sh jam/check.sh group ...` |

Run the groups that cover what you changed; a firmware change that touches storage, MIDI or the sound needs its own group to pass. CI runs all groups on Linux (`.github/workflows/host-tests.yml`). Some groups are skipped on Windows because they need `fork()`; `jam/check.sh` reports them as SKIP.

## Style

- **Line endings are LF.** Set `git config core.autocrlf false` before you commit on Windows.
- **Commit messages:** `area: what changed`, in plain words, lower case after the colon, for example `README: STYLE presets table`. One change per commit; say why in the body when it is not obvious.
- **Code comments say what the code does now.** A function's comment tells what it does and why, so that someone who reads only that function understands it. History does not go in code: what a change fixed and how it used to work belongs in the commit message and `CHANGELOG.md`. In new comments, leave out item numbers, "item N", job or branch names ("cloud job H") and version tags ("0.2.19", "DUELL 1.1"); a decision that still binds is stated as the rule ("K4 is empty on this page"), not as who decided it or when. When you change a function, bring its comment up to date and move any history paragraph out of it. Do not rewrite comments in code you do not touch.
- **Versions:** TIDE's version is the one line of `TIDE_VERSION`; a pull request does not change it. The maintainer sets it, tags and releases ([how](docs/ROADMAP.md#versions-and-releases)).
- **Documents:** plain, short sentences, no hype, facts checked against the code. Terms: TIDE, AUTO TIDE, TIDE SETTINGS, End A and End B, part 1 and part 2, the stable and the beta build. New documents are listed in [docs/DOCS_INDEX.md](docs/DOCS_INDEX.md).
