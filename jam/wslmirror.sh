#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The checks in WSL, on WSL's own disk (a build over /mnt/c is much slower than on the Linux file system): the worktree is mirrored to
# ~/duell-build/<worktree folder> (rsync), built and tested there, and only the logs come back as files. From Git Bash, PowerShell or WSL, in any worktree:
#   sh jam/wslmirror.sh [check.sh arguments]        e.g.  sh jam/wslmirror.sh -j 4 --host-only      sh jam/wslmirror.sh --changed fork/b9 -j 4 --host-only
# No arguments = `-j 4 --host-only` (every host group, 4 at a time). Never more than 4 jobs across all lanes: ask for more only when nothing else is building.
# The mirror keeps its own felucca-base/build (the generated headers, the compile cache, the logs): rsync never touches it, so a second run starts warm.
# Commits happen only in the Windows worktree. The shared repository is never written: the mirror has a repository of its own (a `git clone --shared`, which reads the
# shared objects through its alternates and writes only under ~/duell-build), no GIT_DIR / GIT_WORK_TREE is ever set (a test that makes its own scratch repository would
# then write into the real one), the mirror's HEAD is a detached sha (a commit there can move no shared branch), and the script refuses to run if the shared config has a core.worktree and fails loudly if that config changed after the run (other than its [branch] / [remote] sections, which other lanes write when they push).
# Needs `rsync` and ~/venv (Pillow, fontTools: requirements-dev.txt) in the WSL distribution, as jam/wsl.sh does. Logs: felucca-base/build/check/<sha>/ in the worktree.
set -u
case "$(uname -s 2>/dev/null)" in
    Linux) ;;
    *)  # Git Bash / PowerShell: run the same script inside WSL
        here="$(cd "$(dirname "$0")" && pwd)"
        wp=$(wsl.exe -d Ubuntu -- wslpath -u "$(cygpath -m "$here" 2>/dev/null || echo "$here")" | tr -d '\r')
        MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" exec wsl.exe -d Ubuntu -- sh "$wp/wslmirror.sh" "$@" ;;     # (Git Bash must not turn /mnt/c/... into a Windows path)
esac
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE
SRC="$(cd "$(dirname "$0")/.." && pwd)"
NAME="$(basename "$SRC")"
DST="$HOME/duell-build/$NAME"
case "$DST" in /mnt/*|"$SRC"*) echo "wslmirror: the mirror must be on WSL's own disk, not under /mnt or inside the worktree ($DST)"; exit 1 ;; esac
. "$SRC/jam/safe.sh"
# rsync --delete below empties DST: it must be a folder inside ~/duell-build, never ~, / or an empty / odd name
safe_under "$HOME/duell-build" "$DST" || { echo "wslmirror: refusing: the mirror folder '$DST' is not a folder inside $HOME/duell-build"; exit 1; }
[ -x "$HOME/venv/bin/python" ] && PATH="$HOME/venv/bin:$PATH"
export PATH
command -v rsync >/dev/null 2>&1 || { echo "wslmirror: rsync is not installed in this distribution (sudo apt-get install rsync)"; exit 1; }

# the shared repository (read only from here on): a worktree's .git file names its git dir; the common dir is two levels up
if [ -f "$SRC/.git" ]; then
    gd=$(sed -n 's/^gitdir: //p' "$SRC/.git")
    GD=$(wslpath -u "$gd" 2>/dev/null || echo "$gd")
    MAIN=$(cd "$GD/../.." 2>/dev/null && pwd)
elif [ -d "$SRC/.git" ]; then
    GD="$SRC/.git"; MAIN="$SRC/.git"
else
    GD=""; MAIN=""
fi
shared_config_ok() {      # no core.worktree in the shared config (a broken one stops every git command of every lane)
    [ -z "$MAIN" ] || [ -z "$(git config --file "$MAIN/config" --get core.worktree 2>/dev/null)" ]
}
cfg_sum() {      # the config without its [branch ...] / [remote ...] sections (another lane that pushes or makes a branch writes those at any time); anything else changing is a warning sign
    awk '/^\[/ { skip = ($0 ~ /^\[(branch|remote) /) } !skip' "$MAIN/config" | md5sum | cut -d' ' -f1
}
CFG_BEFORE=""; [ -n "$MAIN" ] && [ -f "$MAIN/config" ] && CFG_BEFORE=$(cfg_sum)
shared_config_ok || { echo "wslmirror: refusing to run: $MAIN/config has a core.worktree (remove that line first: it breaks every git command on Windows)"; exit 3; }

mkdir -p "$DST" || exit 1
t0=$(date +%s)
rsync -a --delete --exclude=/.git --exclude=/felucca-base/build --exclude=node_modules "${SRC:?}/" "${DST:?}/" || { echo "wslmirror: rsync failed"; exit 1; }
echo "== mirror $SRC -> $DST ($(( $(date +%s) - t0 )) s)"

# git for --changed, multiv8, otareuse: the mirror's own repository, made once as a shared clone, refs fetched from the shared one each run, HEAD and index set to the worktree's HEAD
if [ -n "$MAIN" ] && [ -d "$MAIN" ]; then
    [ -f "${DST:?}/.git" ] && rm -f "${DST:?}/.git"          # (an older mirror had a .git file naming a git dir: not any more)
    if [ ! -d "$DST/.git" ]; then
        git clone -q --no-checkout --shared "$MAIN" "${DST:?}/.clone.tmp" 2>/dev/null && mv "${DST:?}/.clone.tmp/.git" "${DST:?}/.git" && rmdir "${DST:?}/.clone.tmp" \
            || echo "wslmirror: note: could not make a repository for the mirror: the groups that read git will fail"
    fi
    if [ -d "$DST/.git" ]; then
        git -C "$DST" config core.fileMode false          # (rsync -a from /mnt/c gives every file mode 755; the index has 644: without this `git diff BASE` lists every file and --changed selects every group; set on every run, also on a mirror made before this line)
        git -C "$DST" fetch -q --no-tags origin '+refs/remotes/fork/*:refs/remotes/fork/*' '+refs/heads/*:refs/remotes/origin/*' '+refs/tags/*:refs/tags/*' 2>/dev/null
        head=$(git --git-dir="$GD" rev-parse HEAD 2>/dev/null)
        if [ -n "$head" ]; then
            git -C "$DST" update-ref --no-deref HEAD "$head" 2>/dev/null
            git -C "$DST" reset -q --mixed "$head" 2>/dev/null          # (index = HEAD; the files stay: `git diff BASE` sees the worktree's edits)
        fi
    fi
fi
cd "$DST/felucca-base" || exit 1
mkdir -p build/host
python3 tools/build.py --gen-only > build/host/wsl_gen.log 2>&1 || { tail build/host/wsl_gen.log; echo "wslmirror: generating the headers failed"; exit 1; }
cd "$DST" || exit 1
sha=$(git rev-parse --short HEAD 2>/dev/null || echo nogit)
[ $# -eq 0 ] && set -- -j 4 --host-only
JAM_CHECK_LOG="$DST/felucca-base/build/check/$sha" sh jam/check.sh "$@"
rc=$?
# only the logs come back, as files
mkdir -p "$SRC/felucca-base/build/check"
rsync -a "$DST/felucca-base/build/check/$sha/" "$SRC/felucca-base/build/check/$sha/" 2>/dev/null && echo "== logs: $SRC/felucca-base/build/check/$sha"
if [ -n "$CFG_BEFORE" ] && [ "$(cfg_sum)" != "$CFG_BEFORE" ]; then
    echo "wslmirror: ERROR: the shared git config $MAIN/config changed outside its [branch]/[remote] sections during this run: something wrote it; tell the owner of the session, do not leave it"
    exit 3
fi
shared_config_ok || { echo "wslmirror: ERROR: $MAIN/config now has a core.worktree: something in this run wrote the shared config; tell the owner of the session, do not leave it"; exit 3; }
exit $rc
