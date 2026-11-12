#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# One command per release: the README pictures and the web emulator's poster, shot from the current build.
#   sh felucca-base/web/emu/scripts/shoot_release.sh
# Needs build/gen (run felucca-base/tools/soundlab/build.sh once). Pictures go to felucca-base/build/shots/, then are copied to
# docs/tide/ (README) and felucca-base/web/emu/img/ (the emulator's poster). Check them by eye before you commit.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
fb="$here/../../.."                       # felucca-base
repo="$fb/.."                             # the DUELL repo root (docs/, README.md)
cd "$fb"
sh web/emu/emu_run.sh --run web/emu/scripts/release_shots.txt --out build/shots
mkdir -p "$repo/docs/tide" web/emu/img
cp build/shots/auto_tide_both_trip.png "$repo/docs/tide/auto-tide-both-parts.png"
cp build/shots/tide_settings.png "$repo/docs/tide/morph-settings.png"
cp build/shots/morph_gliding.png "$repo/docs/tide/morph-gliding.png"
cp build/shots/auto_tide_both_trip.png web/emu/img/auto-tide-both-parts.png
echo "shoot_release: pictures copied (docs/tide/*.png, web/emu/img/auto-tide-both-parts.png)"
