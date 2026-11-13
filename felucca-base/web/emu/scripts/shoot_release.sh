#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# One command per release: the web emulator poster, shot from the current build (the README pictures are rc atlas stills and docs/tide/auto-tide-glide.gif).
#   sh felucca-base/web/emu/scripts/shoot_release.sh
# Needs build/gen (run felucca-base/tools/soundlab/build.sh once). Pictures go to felucca-base/build/shots/, then the poster is copied to
# felucca-base/web/emu/img/. Check them by eye before you commit.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
fb="$here/../../.."                       # felucca-base
repo="$fb/.."                             # the DUELL repo root (docs/, README.md)
cd "$fb"
sh web/emu/emu_run.sh --run web/emu/scripts/release_shots.txt --out build/shots
mkdir -p "$repo/docs/tide" web/emu/img
cp build/shots/auto_tide_both_trip.png web/emu/img/auto-tide-both-parts.png
echo "shoot_release: poster copied (web/emu/img/auto-tide-both-parts.png)"
