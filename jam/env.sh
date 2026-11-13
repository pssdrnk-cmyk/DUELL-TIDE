# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM build environment for Git Bash on Windows (no WSL, no Docker). Source it:
#   . jam/env.sh
# Everything lives in portable folders under ../work: the JieLi Windows toolchain and SDK files (jieli-target), the
# Python venv (duell-win-venv), the FriBiDi DLL Pillow's text layout needs (tools/fribidi), llvm-mingw for the host
# tests (tools/llvm-mingw) and Emscripten (emsdk).
# Linux / WSL: the system tools are used (even in a checkout under /mnt/c, where the Windows folders below would be found and the Windows toolchain put in PATH)
case "$(uname -s 2>/dev/null)" in Linux) return 0 2>/dev/null || exit 0 ;; esac
# The shared work folder: ../work beside the main checkout, or the folder two levels up from a worktree in work/wt/<name>
if [ -z "${JAM_WORK:-}" ]; then
    _jam_here="$(cd "$(dirname "${BASH_SOURCE:-$0}")" && pwd)"
    for _c in "$_jam_here/../../work" "$_jam_here/../../.."; do
        if [ -d "$_c/duell-win-venv" ]; then JAM_WORK="$(cd "$_c" && pwd)"; break; fi
    done
    unset _jam_here _c
fi
# Linux / macOS / WSL (and CI): no Windows work folder, nothing to set up (the tools come from the system)
if [ -z "${JAM_WORK:-}" ]; then return 0 2>/dev/null || exit 0; fi
# A Windows-style JAM_WORK (C:/...) would put "C:" into PATH, which Git Bash splits at the colon: the venv, FriBiDi
# (Pillow's Raqm text layout) and the shims would silently drop out and the fonts would be drawn differently
JAM_WORK="$(cd "$JAM_WORK" && pwd)" || return 1 2>/dev/null || exit 1
export JAM_WORK
export JIELI_TOOLCHAIN="$JAM_WORK/jieli-target/windows/C_/JL/pi32"
export AC79_SDK="$JAM_WORK/jieli-target/sdk"
export PATH="$JAM_WORK/shims:$JAM_WORK/duell-win-venv/Scripts:$JAM_WORK/tools/fribidi:$JAM_WORK/tools/llvm-mingw/bin:$PATH"
export EMSDK_QUIET=1
. "$JAM_WORK/emsdk/emsdk_env.sh" >/dev/null 2>&1
