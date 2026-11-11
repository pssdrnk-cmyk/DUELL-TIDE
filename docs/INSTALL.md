# Installing DUELL with TIDE

**DUELL** with TIDE: pssdrnk's fork of DUELL 1.1, MultiMorphing Bitimbral Synthesizer by OSCILL/R.

> **Install at your own risk.** TIDE has been flashed to and run on **one** real FM-1, the fork owner's. The way back to the official firmware (V15) has worked on that unit and has not been tried on others. The FM-1 has a single-bank layout and there is no known rollback. No warranty. DUELL with TIDE is not affiliated with OSCILL/R or M-VAVE, and M-VAVE's firmware is not distributed here. Read [RECOVERY.md](RECOVERY.md), [KNOWN_LIMITS.md](KNOWN_LIMITS.md) and [KNOWN_ISSUES.md](KNOWN_ISSUES.md) first.

> **DUELL with TIDE (this fork) has its own installer page:** <https://pssdrnk-cmyk.github.io/DUELL-TIDE/install/>.
> - It offers two builds.
> - **Stable** is the default: the build the fork owner validated, a normal GitHub release.
>   - TIDE's version, for example `1.0.0`.
>   - tag `tide-1.0.0`.
>   - package `duell-tide-stable-1.0.0.fwsc`.
> - **Beta** is a build under test, a GitHub pre-release.
>   - For example `1.0.0-b2`.
>   - tag `tide-1.0.0-b2`.
>   - package `duell-tide-beta-1.0.0-b2.fwsc`.
>   - shown only while it is newer than the stable one.
>   - needs a tick box and a second confirmation.
> - Builds from before the versions have dated names, `stable-2026-11-03` and `beta-2026-11-04`.
>   - they count as older than any version.
> - The page shows the tag, version or date, commit and SHA-256 of what will be written.
> - It checks the SHA-256 of the downloaded package in the browser against the release data (`install/channels.json`, written by the Pages workflow from the release) before it writes.
>   - on a mismatch it refuses.
> - The backup, the way back to the official firmware and the resume after an interruption are as described below.
> - After publishing or editing a release, run Host tests by hand on `jam11` (Actions > Host tests > Run workflow).
>   - Pages refreshes the site and release channels only after that run passes, using exactly the tested commit.
> - The text below was written for OSCILL/R's page, whose files and names differ (`duell-latest.fwsc`, `duell-version.json`).

## What you need
- An M-VAVE FM-1.
- A computer with **Chrome or Edge** (the installer uses WebMIDI with SysEx; Safari and Firefox will not work).
- A **data** USB cable. Some cables only charge.
- Nothing to download for DUELL: the installer page loads the package itself (see "The file" below). Only for the way back you need M-VAVE's official V15 file.

## Flash checklist
1. Data USB cable, plugged **straight into the computer**. No hub, no dock.
2. **Back up first** (step 2 below).
3. Close other programs that use MIDI (DAWs, other browser tabs).
4. **Do not interrupt** the write: do not unplug, close the tab or let the computer sleep.
5. When it says Done, **switch the FM-1 off and on** before you use it.

## The file
The installer page is `felucca-base/web/install/index.html`. It keeps Felucca's install method (the running firmware stages an update loader, the loader writes the package) and works with a file that **`felucca-base/tools/build.py`** builds:

| Command | File written to `felucca-base/build/` |
|---|---|
| `tools/build.py --release` (recommended) | `duell-<VERSION>.fwsc` (`duell-1.1.fwsc`; the version is the `VERSION` file; a 0.x.y test version is named `duell-<VERSION>-beta.fwsc`), plus a folder `release-<VERSION>/` with the package, the app, `SHA256SUMS`, licences |
| `tools/build.py` (no flag) | the same package, without the release folder |

Every build also writes `duell-latest.fwsc` (the same file) and `duell-version.json` next to it.

- The package name starts with `duell-` (for example `duell-1.1.fwsc`). The installer page also still finds the older name `felucca-0.1-beta.fwsc`.
- The device shows the version as **DUELL 1.1** (from the `VERSION` file); TIDE does not change that text.
- The AUTO TIDE page in HOME's round shows that TIDE is on the unit.
- **MENU > SYSTEM > ABOUT** shows TIDE's own version beside the date (for example `1.0.0-b1`, from the file `TIDE_VERSION`).
- A `+` after the version means the build is not exactly the tagged release (for example the owner's unit shows `1.0.0-b2+`, the build made just before `tide-1.0.0-b2` was tagged).
- The packages on the releases are named after that version: [Versions](ROADMAP.md#versions-and-releases) says how a release is made.
- Build steps: [felucca-base/BUILDING.md](../felucca-base/BUILDING.md).

When you build the site yourself, the page does these things:

- Reads the version from `duell-version.json`.
- Looks for **`duell-latest.fwsc` next to itself** (in `felucca-base/web/install/`).
- If that file is missing, looks for `duell-<VERSION>.fwsc`, then the older names `duell-0.1-beta.fwsc`, `felucca-0.1-beta.fwsc`.
- If none of those files are there, shows the notice **"DUELL firmware is not yet released"** and keeps the install flow hidden; the flow appears when the file exists.
- Accepts only a Felucca-loader package (identity `FM-1_9xx`, which `tools/build.py` makes `FM-1_901`).
- Shows the SHA-256 so you can compare it with `SHA256SUMS`.

To view the page, serve the folder `felucca-base/web/` with any static web server, for example `python3 -m http.server -d felucca-base/web 8000`, and open `http://localhost:8000/install/`. (Opening the file straight from disk does not work, because the page loads modules.) The same page is published at <https://pssdrnk-cmyk.github.io/DUELL-TIDE/install/> (GitHub Pages), which loads the package of the channel you pick from this fork's releases.

## The three cards
The installer page has three separate action cards, each with its own button, progress bar, status line and result line:
- **BACK UP** (step 2): reads the unit and saves a file. Nothing is written to the FM-1.
- **INSTALL DUELL** (step 5): writes DUELL. Shown only when a DUELL package is found or chosen. The bar says "Writing DUELL".
- **RETURN TO OFFICIAL FIRMWARE** (the section "Go back to the official firmware"): writes the official M-VAVE firmware back and removes DUELL. It has its own amber border and warning ("This writes the official M-VAVE firmware back. DUELL will be removed. Keep the cable connected until DONE."), and its bar says "Writing official firmware".

While one of them runs, a coloured banner at the top says which ("BACKING UP", "INSTALLING DUELL" or "RETURNING TO THE OFFICIAL FIRMWARE: do not unplug or close this page until it says DONE"), the other two buttons, the file pickers and the checklist are locked (grey, with a note), and the browser asks before the tab is closed. A progress bar appears only in the card that is running. A failure message stays in the card that failed: press the same button again and do not unplug.

## Steps
1. **Checklist.** Tick the four boxes on the page.
2. **Back up.** Press "Save a backup file". It downloads a `DUELL-backup-DATE-TIME.json`. Limits: it works only if the FM-1 already runs Felucca or DUELL (not the official firmware). The file holds the patches, multis, patterns, user wavetables, settings and the current music; the restore is on the editor's Backup page.
3. **The channel.** Pick **Stable** (the build the fork owner has validated; the one to take unless you want to help test) or **Beta** (the newest build, under test). The page loads that channel's package from the release; you do not choose a file. (Only the official M-VAVE file for the way back is chosen by hand.)
4. **Connect** the FM-1 and allow MIDI/SysEx in the browser.
5. **Install.** On a unit that runs DUELL the page first saves a backup file to your downloads, and stops with nothing written if the backup fails (stop the unit if it plays, then press Install again). A unit on the official firmware has nothing to save. Wait for "Done".
6. **Switch the unit off and on**, then hold HOME and open ABOUT to see the version (DUELL 1.1, and TIDE's own version beside the date). While the firmware is written the unit shows the update screen (UPDATE, WRITING, the percent, a bar and DO NOT SWITCH OFF); a dark screen does not mean the write has stopped.

If anything stops: replug the cable and press Install again. The device stays in update mode until the write is finished. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) and [RECOVERY.md](RECOVERY.md).

## Keep or erase your data
- **A reinstall keeps your data.** Installing DUELL again, even after going back to the official firmware, does not touch the area where the unit keeps its saved data, so it comes back as you left it: your settings, patches, multis and patterns.
- **To start from scratch while installing:** do these steps on the installer page:

  1. Open the INSTALL DUELL card. You see the box **"Erase my saved data after installing (start from scratch)"**.
  2. The page checks what the FM-1 runs. Press "Check the unit" if the page has not talked to the unit yet.
  3. The line "Detected: ..." tells you which case it found.
     - If the unit runs DUELL now, the box is unticked by default (an update keeps your patches and settings).
     - Otherwise it is ticked by default (clean install).
  4. You can tick or untick the box yourself at any time. Your choice always wins over what the page detected.
  5. If the box is ticked, after the install finishes and the unit restarts into DUELL:
     - The page connects to the unit.
     - It shows "Erasing your data ..." (the card stays locked: do not unplug).
     - Then it shows "Done. Your saved data was erased. The unit starts as new."
  6. If the unit cannot be reached or refuses (for example while it is playing), the card says that the install itself succeeded, that your data was **not** erased, and to use the menu item below.
- **To start from scratch any time:** on the unit, hold HOME, go to SYSTEM > ERASE DATA, press OCT+ and confirm with OCT+ (OCT- or any page button cancels).
- **What is erased:** the settings (theme, scale, MIDI channels, favourites, preferences), the autosave, all your saved patches (every slot holds its factory sound again, the USER slots U1 to U4 are empty), your multis, your pattern slots, the three user wave tables (USR1 to USR3) and the FM6 voice bank the editor loaded. The factory sounds, the factory multis M01 to M16 and the factory patterns come back, and the unit starts as new (VIOLET, LAYER, M01; with TIDE, M04 DUB CHORD). **Not erased:** the firmware itself.
- **It cannot be undone: back up first** (step 2). The backup holds your multis and patterns too.

## Return to the official firmware
1. Download the official FM-1 firmware (`FM-1.fwsc`, **V15**) yourself from M-VAVE: <https://www.m-vave.com/download>. This project does not host M-VAVE's files.
2. On the installer page, choose that file under "Return to the official firmware". It is checked in your browser (size and SHA-256 must match the unmodified V15; nothing is uploaded).
3. Press "Back up and return to official V15" in the amber RETURN card. The page saves a backup first (if the unit runs DUELL), then writes V15. All progress shows in that card only, under an amber banner. Keep the cable connected until the card says "Done. The official firmware is back. Switch the unit off and on." Then switch the unit off and on.

Your DUELL data cannot be opened by the official firmware. This path is the same method as for installing. It has worked on the author's unit about three times; it is untested on other units.


## Source and licences
The package you install is DUELL with TIDE, licensed **GPL-3.0-only**. Its source is this repository at the tag of the release you chose (`tide-1.0.0-b1`, `tide-1.0.0` or a later one, or the older `stable-2026-11-03` and `beta-2026-11-04`); the release page names the commit. The licence texts of the parts that keep their own licence are in [felucca-base/LICENSES/](../felucca-base/LICENSES/), and [felucca-base/LICENSING.md](../felucca-base/LICENSING.md) says which file each one covers. Every package also holds three unmodified files of the JieLi AC79 SDK (Apache-2.0, `LICENSES/Apache-2.0.txt`). No warranty, as the GPL says.
