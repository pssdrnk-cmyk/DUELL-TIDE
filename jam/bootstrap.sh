#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Makes the Python environment the host tests and the generators need (Linux, macOS, Git Bash on Windows). It downloads nothing but the
# packages of requirements-dev.txt from PyPI. The device toolchain (JieLi) is not part of it: see docs/DEVELOPING.md.
#   sh jam/bootstrap.sh [venv folder]       default: .venv in the repository (Windows: the work folder's duell-win-venv is what jam/env.sh uses)
set -eu
REPO="$(cd "$(dirname "$0")/.." && pwd)"
VENV="${1:-$REPO/.venv}"
python3 -m venv "$VENV"
if [ -x "$VENV/bin/python" ]; then PY="$VENV/bin/python"; else PY="$VENV/Scripts/python.exe"; fi
"$PY" -m pip install --quiet --upgrade pip
"$PY" -m pip install --quiet -r "$REPO/requirements-dev.txt"
echo "ready: $PY"
echo "use it:  PATH=\"$(dirname "$PY"):\$PATH\"; (cd felucca-base && python tools/build.py --gen-only) && sh host_tests.sh chord"
