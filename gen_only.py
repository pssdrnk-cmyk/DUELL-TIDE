#!/usr/bin/env python3
"""Emulator-only: run Felucca's generators (no JieLi toolchain / Docker). Same as `python felucca-base/tools/build.py --gen-only`
(one list of generator jobs, in tools/build.py generate()). Run with the Python of requirements-dev.txt (sh jam/bootstrap.sh)."""
import os, subprocess, sys
from pathlib import Path
base = Path(__file__).resolve().parent / "felucca-base"
env = dict(os.environ, DYLD_FALLBACK_LIBRARY_PATH="/opt/homebrew/lib")  # libraqm for Pillow
sys.exit(subprocess.run([sys.executable, "tools/build.py", "--gen-only"], cwd=base, env=env).returncode)
