#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUELL JAM: guards for every `rm -rf` and every `rsync --delete` of the jam scripts (source it: `. "$REPO/jam/safe.sh"`).
# On 2026-11-12 an empty variable made `rm -rf "$D"/*` the command `rm -rf /*` and emptied a home directory (jam/README.md, "Running things in WSL"):
# a path that comes out of a variable is checked here before anything is deleted.
#   safe_under ROOT PATH   succeeds only if PATH is a strict descendant of ROOT (at least one more component), has no `..` component, and neither is empty, `/` or $HOME;
#                          a ROOT of `/` or $HOME is refused too (a root must be a folder made for this: build/, duell-build/, the work folder)
#   safe_rm ROOT PATH      `rm -rf PATH` after safe_under; otherwise it says so on stderr, removes nothing and fails
safe_under() {
    _r="${1:-}"; _p="${2:-}"
    [ -n "$_r" ] && [ -n "$_p" ] || return 1
    _h="${HOME:-/nonexistent-home}"
    case "$_r" in /|//|"$_h"|"$_h"/) return 1 ;; esac
    case "$_p" in /|//|"$_h"|"$_h"/) return 1 ;; esac
    case "$_p" in ..|../*|*/..|*/../*) return 1 ;; esac
    case "$_r" in ..|../*|*/..|*/../*) return 1 ;; esac
    _r="${_r%/}"
    case "$_p" in "$_r"/?*) return 0 ;; esac
    return 1
}
safe_rm() {
    if safe_under "${1:-}" "${2:-}"; then
        rm -rf "$2"
    else
        echo "safe_rm: refusing to remove '${2:-}' (not inside '${1:-}')" >&2
        return 1
    fi
}
