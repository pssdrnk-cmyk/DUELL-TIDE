#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM: host test groups in the owner's WSL Ubuntu, where fork(), termios and the rest exist (on Windows 25 groups SKIP).
# From Git Bash or PowerShell, in any checkout:
#   wsl.exe -d Ubuntu -- sh jam/wsl.sh [group ...]        (default: every group; the log is felucca-base/build/host/wsl.out)
# Ubuntu needs once: sudo apt-get install build-essential python3-venv nodejs libfribidi0 libraqm0, and a venv at ~/venv with
# pillow==12.3.0 fonttools==4.65.0 numpy (the generators' versions of work/duell-win-venv: the same headers, byte for byte).
# The checkout is used where it is (/mnt/c/...); the generated headers are made without the device toolchain (build.py --gen-only).
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
[ -x "$HOME/venv/bin/python" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
# A git worktree made on Windows has a .git file naming a C:/ path, which Linux git cannot read (multiv8 needs git show): point git at the main repository.
if [ -f "$REPO/.git" ] && ! git -C "$REPO" rev-parse >/dev/null 2>&1; then
  gd=$(sed -n 's/^gitdir: //p' "$REPO/.git" | sed 's|/worktrees/.*||')
  [ -n "$gd" ] && GIT_DIR=$(wslpath -u "$gd" 2>/dev/null) && export GIT_DIR
fi
cd "$REPO/felucca-base" || exit 1
mkdir -p build/host
python3 tools/build.py --gen-only > build/host/wsl_gen.log 2>&1 || { tail build/host/wsl_gen.log; exit 1; }
cd "$REPO" || exit 1
sh host_tests.sh "$@" 2>&1 | tee felucca-base/build/host/wsl.out
