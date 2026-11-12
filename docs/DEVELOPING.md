# Developing DUELL with TIDE

How to build the generated files, run the tests and what CI runs. The hardware checklist is [TESTING.md](TESTING.md); how to send a change is [../CONTRIBUTING.md](../CONTRIBUTING.md).

## What you need

| | Host tests (most of the work) | Device build (the `.fwsc` package) |
|---|---|---|
| Needs | a C compiler (`cc`: gcc or clang), `sh`, Python 3.12, Node 22 | the same, plus the JieLi pi32v2 toolchain and three files of the AC79 SDK |
| Linux | `sudo apt-get install build-essential libfribidi0 libraqm0 nodejs` | native; [../felucca-base/BUILDING.md](../felucca-base/BUILDING.md) |
| macOS | Xcode command line tools, `brew install fribidi libraqm node` | Docker (BUILDING.md) |
| Windows | Git Bash, llvm-mingw (the `cc`), the layout below | the JieLi Windows toolchain, same layout |

Python packages: `sh jam/bootstrap.sh` makes a `.venv` from [../requirements-dev.txt](../requirements-dev.txt) (Pillow 12.3.0 and fontTools 4.65.0 are pinned: with another version, or without Raqm, the generated fonts differ and some tests fail). The device toolchain is JieLi's and is not redistributed here; CI never builds the device image.

**Windows layout.** `. jam/env.sh` expects one shared work folder next to the main checkout (`../work`, or two levels above a worktree kept in its `wt/<name>` subfolder) holding: `duell-win-venv` (the Python environment of requirements-dev.txt), `tools/llvm-mingw` (the host `cc`), `tools/fribidi` (the DLL Pillow's text layout needs), `jieli-target` (toolchain and SDK) and `shims` (a `cc` and `python3` wrapper). Nothing in the repository creates it; `jam/env.sh` says what each folder is for. On Windows 3 of the 173 default groups skip (`dx7compat`: no DX7 banks; `benchrun`: termios and `select()` on pipes; `benchaudio`: no numpy; the `fork()` groups run through the shim in `jam/wincompat`); `wsl.exe -d Ubuntu -- sh jam/wsl.sh <group>` runs them in WSL ([../jam/wsl.sh](../jam/wsl.sh)).

## Run tests

```
. jam/env.sh                                   # Windows only: PATH for cc, python3, node
cd felucca-base && python3 tools/build.py --gen-only && cd ..    # the generated headers (build/gen), no device toolchain needed
sh host_tests.sh chord mode                    # named groups; no names: every group (about 140; the list is `want=` in host_tests.sh)
sh jam/check.sh                                # the device build, every group with a time limit, jam_dev's tests; logs outside the repository
sh jam/check_off.sh                            # DUELL_TIDE=0 must still build plain DUELL, byte for byte (needs the device toolchain)
```

Each group prints `== name` and then `PASS`, `FAIL` or `SKIP <why>` (a group that could not run its checks: no Chrome, no update package, no DX7 banks). The sources are in `felucca-base/tests/`; the comment block at the top of `host_tests.sh` says what each group covers. The `tideoff` group builds the firmware with `DUELL_TIDE=0` and runs seven tests on it.

**Windows: the groups that SKIP there.** Three of the 173 default groups: `dx7compat` (no DX7 banks), `benchrun` (needs `termios` and `select()` on pipes) and `benchaudio` (needs numpy); in a snapshot without git history `otareuse` and `multiv8` SKIP as well. They print SKIP, so a red CI line in them is invisible. Run them in WSL (Ubuntu): `sudo apt install build-essential python3-venv python3-numpy python3-pil`, `python3 -m venv --system-site-packages ~/dv && ~/dv/bin/pip install fonttools`, then from the worktree under `/mnt/c/...` (with `~/dv/bin` first in `PATH`): `(cd felucca-base && python3 tools/build.py --gen-only)` and `sh host_tests.sh benchrun benchaudio`. Put these commands in a script file and run `wsl.exe -d Ubuntu -- bash script.sh`: an inline command from Git Bash gets its `$PATH` expanded on the Windows side and breaks.

## What CI runs

[`.github/workflows/host-tests.yml`](../.github/workflows/host-tests.yml) runs on every push to `jam11` (and by hand): the generators with the pinned packages, then every group of the default list, dealt to 8 machines, with `REQUIRE_CHROME=1` (a missing browser fails the browser tests). Each machine writes a PASS / FAIL / SKIP table to its job summary and keeps its log. [`pages.yml`](../.github/workflows/pages.yml) publishes the site only after that workflow is green on the same commit. Every job runs on Linux; the one macOS job, the CPU instruction counts, is [`cost-macos.yml`](../.github/workflows/cost-macos.yml): by hand (Actions > CPU cost (macOS) > Run workflow) and every Sunday at 22:00 UTC, never part of a push or pull request run. [`ci-retry.yml`](../.github/workflows/ci-retry.yml) re-runs, once or twice, only the jobs GitHub's runner failed ("job not acquired by Runner of type hosted", a shutdown signal; `jam/ci_retry.py`): never a job whose test failed, a run a newer one replaced, or a commit that is no longer the branch head. The device build, `jam/check_off.sh` and the flashing are the owner's steps.

## Rules that save time

- One build or test run at a time in a checkout (they share `felucca-base/build/`).
- A change that alters plain DUELL (the `DUELL_TIDE=0` image) changes the hash in `jam/off.sha256`: update it in the same commit and say why (`jam/off.history.md` keeps what it covers).
- Test files are LF, new files carry `SPDX-License-Identifier: GPL-3.0-only`.
- Never spell a factory multi's name in a test or a bench suite. The generated table (`build/gen/multi_names.h`: `MULTI_NAME[slot - 1]`, and `multi_names.txt` for the bench runner: `load multi M06`) is written by `tools/gen_duell_multis.py` from `tools/factory_multis.py`, so a rename changes it in one place; `tests/bench_run_test.py` fails on a suite that spells a name.
- Hot code stays out of the flash. The build (`tools/hot_reach.py`, from `tools/build.py`, RAM-placement builds) walks the direct calls from the audio interrupt, core 1's entry and every function already in RAM, and fails when a function it reaches sits in the slow XIP flash and is not in `felucca-base/tools/ram_allow.py`. The message names the function and the call chain. Fix it by putting the function in range Z (a group in `tools/ram_fns_z.py`; the 8,192 B reserve stays) or, if it only runs on an event (a note-on, a load, a mode change), add it to the allow-list with a one-line reason. When you move a function into RAM, delete its allow-list line. Function pointers are followed too: a function whose address a reached function takes, and the functions in a data table it points to (the engine table: ENGINES -> engine_t -> note_on, desc, ...); the message then reads `fn <-[ENG_X]- caller`. Only a computed jump and inline asm are not followed.
