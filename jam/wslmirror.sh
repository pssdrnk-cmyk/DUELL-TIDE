#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The checks in WSL, on WSL's own disk (a build over /mnt/c is much slower than on the Linux file system): the worktree is mirrored to
# ~/duell-build/<worktree folder> (rsync), built and tested there, and only the logs come back. From Git Bash, PowerShell or WSL, in any worktree:
#   sh jam/wslmirror.sh [check.sh arguments]        e.g.  sh jam/wslmirror.sh -j 4 --host-only      sh jam/wslmirror.sh --changed fork/b9 -j 4 --host-only
# No arguments = `-j 4 --host-only` (every host group, 4 at a time). Never more than 4 jobs across all lanes: ask for more only when nothing else is building.
# The mirror keeps its own felucca-base/build (the generated headers, the compile cache, the logs): rsync never touches it, so a second run starts warm.
# Excluded from the copy: .git (git is pointed at the real repository, see below), felucca-base/build, node_modules. Needs `rsync` and ~/venv (Pillow, fontTools:
# requirements-dev.txt) in the WSL distribution, as jam/wsl.sh does. Logs: felucca-base/build/check/<sha>/ in the worktree.
set -u
case "$(uname -s 2>/dev/null)" in
    Linux) ;;
    *)  # Git Bash / PowerShell: run the same script inside WSL
        here="$(cd "$(dirname "$0")" && pwd)"
        wp=$(wsl.exe -d Ubuntu -- wslpath -u "$(cygpath -m "$here" 2>/dev/null || echo "$here")" | tr -d '\r')
        exec wsl.exe -d Ubuntu -- sh "$wp/wslmirror.sh" "$@" ;;
esac
SRC="$(cd "$(dirname "$0")/.." && pwd)"
NAME="$(basename "$SRC")"
DST="$HOME/duell-build/$NAME"
[ -x "$HOME/venv/bin/python" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
command -v rsync >/dev/null 2>&1 || { echo "wslmirror: rsync is not installed in this distribution (sudo apt-get install rsync)"; exit 1; }
mkdir -p "$DST" || exit 1
t0=$(date +%s)
rsync -a --delete --exclude=/.git --exclude=/felucca-base/build --exclude=node_modules "$SRC/" "$DST/" || { echo "wslmirror: rsync failed"; exit 1; }
echo "== mirror $SRC -> $DST ($(( $(date +%s) - t0 )) s)"
# git (--changed, multiv8, otareuse read it): the mirror gets a .git file of its own that names a git dir on the Linux disk (HEAD, a copy of the index, commondir = the repository
# on /mnt/c): no GIT_DIR in the environment, so the tests' own scratch repositories are not disturbed, and the real worktree's index is never written
if [ -f "$SRC/.git" ]; then
    gd=$(sed -n 's/^gitdir: //p' "$SRC/.git")
    GD=$(wslpath -u "$gd" 2>/dev/null || echo "$gd")
    MAIN=$(cd "$GD/../.." 2>/dev/null && pwd)
elif [ -d "$SRC/.git" ]; then
    GD="$SRC/.git"; MAIN="$SRC/.git"
fi
if [ -n "${GD:-}" ] && [ -d "$GD" ]; then
    LG="$HOME/duell-build/.gitdirs/$NAME"
    mkdir -p "$LG"
    cp -f "$GD/HEAD" "$LG/HEAD"
    [ -f "$GD/index" ] && cp -f "$GD/index" "$LG/index"
    printf '%s
' "$MAIN" > "$LG/commondir"
    printf 'gitdir: %s
' "$LG" > "$DST/.git"
fi
cd "$DST/felucca-base" || exit 1
mkdir -p build/host
python3 tools/build.py --gen-only > build/host/wsl_gen.log 2>&1 || { tail build/host/wsl_gen.log; echo "wslmirror: generating the headers failed"; exit 1; }
cd "$DST" || exit 1
sha=$(git rev-parse --short HEAD 2>/dev/null || echo nogit)
[ $# -eq 0 ] && set -- -j 4 --host-only
JAM_CHECK_LOG="$DST/felucca-base/build/check/$sha" sh jam/check.sh "$@"
rc=$?
# only the logs come back
mkdir -p "$SRC/felucca-base/build/check"
rsync -a "$DST/felucca-base/build/check/$sha/" "$SRC/felucca-base/build/check/$sha/" 2>/dev/null && echo "== logs: $SRC/felucca-base/build/check/$sha"
exit $rc
