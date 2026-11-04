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

Small and focused, one thing at a time. Say what you changed and what you tested. If the change touches the firmware, say whether you tried it on a real FM-1, in the emulator, or on the computer only. By contributing you agree that your work is licensed under GPL-3.0-only; new source files carry the SPDX header (`SPDX-License-Identifier: GPL-3.0-only`).

**No pull requests go to OSCILL/R's DUELL or to Felucca from this fork,** and TIDE is not announced there. A fix that belongs in plain DUELL is described in an issue; the maintainer decides what to do with it.

## Build

The firmware builds with the JieLi toolchain, which runs on Linux x86-64, or in Docker on macOS. Steps and prerequisites: [felucca-base/BUILDING.md](felucca-base/BUILDING.md). In short, in `felucca-base`:

```
python3 tools/build.py
```

TIDE is on by default. The package is `felucca-base/build/duell-1.1.fwsc`. `DUELL_TIDE=0` builds plain DUELL 1.1. On Windows, use Git Bash and `. jam/env.sh` (it expects a work folder with the toolchain next to the repository).

## Test

| Where | Command |
|---|---|
| Linux, macOS | `./host_tests.sh` (all groups) or `./host_tests.sh group ...` (the groups are named at the top of the script) |
| Windows (Git Bash) | `sh jam/check.sh` (the device build, then every group) or `sh jam/check.sh group ...` |
| TIDE off | `sh jam/check_off.sh` |

Run the groups that cover what you changed; a firmware change that touches storage, MIDI or the sound needs its own group to pass. CI runs all groups on Linux (`.github/workflows/host-tests.yml`). Some groups are skipped on Windows because they need `fork()`; `jam/check.sh` reports them as SKIP.

**The check_off rule.** `jam/check_off.sh` builds with `DUELL_TIDE=0` and compares the package hash with `jam/off.sha256`. A TIDE change must never change that hash. A fix to DUELL's own behaviour (one that is also in a build without TIDE) does change it: update `jam/off.sha256` in the same commit and give the reason in the commit message.

## Style

- **Line endings are LF.** Set `git config core.autocrlf false` before you commit on Windows.
- **Commit messages:** `area: what changed`, in plain words, lower case after the colon, for example `README: STYLE presets table`. One change per commit; say why in the body when it is not obvious.
- **Documents:** plain, short sentences, no hype, facts checked against the code. Terms: TIDE, AUTO TIDE, TIDE SETTINGS, End A and End B, part 1 and part 2, the stable and the beta build. New documents are listed in [docs/DOCS_INDEX.md](docs/DOCS_INDEX.md).
