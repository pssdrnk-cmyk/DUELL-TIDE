// SPDX-License-Identifier: GPL-3.0-only
// DUELL installer: the pure state -> UI mapping (no DOM). The page (install/index.html) applies the result;
// test_installer_state.mjs checks it with node.
//
// state = { busy: null | "backup" | "install" | "return", ready, hasImage, hasStock, channel, betaAck }
//   ready     WebMIDI is available
//   hasImage  a DUELL package was accepted (downloaded, its SHA-256 checked against the release data, its identity checked)
//   hasStock  the official V15 file was chosen and verified
//   channel   "stable" (default) or "beta": the build the user chose to install
//   betaAck   the user ticked the extra confirmation for a beta (needed only when channel is "beta")
export const ACTIONS = ["backup", "install", "return"];

export const BANNERS = {
  backup:  { kind: "backup",  text: "BACKING UP: do not unplug or close this page until it says DONE. Nothing is written to the FM-1." },
  install: { kind: "install", text: "INSTALLING DUELL: do not unplug or close this page until it says DONE." },
  return:  { kind: "return",  text: "RETURNING TO THE OFFICIAL FIRMWARE: do not unplug or close this page until it says DONE." },
};
const NAMES = { backup: "the backup", install: "the DUELL install", return: "the return to the official firmware" };

// The optional "Erase my saved data after installing" box in the INSTALL card (off by default). Pure texts and outcomes; the page
// does the talking (install/index.html eraseAfterInstall -> fm1backup.js eraseAllData).
export const ERASE = {
  label: "Erase my saved data after installing (start from scratch)",
  help: "Erases your settings (theme, scale, MIDI channels, favourites), the autosave, your patches, your multis and your patterns. " +
        "The factory sounds, multis and patterns come back. This cannot be undone: back up first. " +
        "Left off, your saved data stays and the unit comes back as you left it.",
  status: "Erasing your data ...",
  done: "Done. Your saved data was erased. The unit starts as new.",
  lockNote: "Locked: the install is running.",
};
// rc: the unit's answer to ERASE_ALL (0 erased); err: why there was none (the unit did not appear, no answer, the connection broke).
// Only rc 0 is "erased"; everything else says plainly that the install worked and the data is still there.
export function eraseOutcome(rc, err = null) {
  if (rc === 0) return { erased: true, bad: false, text: ERASE.done };
  const why = rc === 3 ? "The unit was playing." : rc === 1 ? "The unit refused the request." : rc === 4 ? "The unit could not write its memory." :
    err ? "The unit could not be reached." : "The unit did not confirm.";
  return { erased: false, bad: true,
    text: `The install itself succeeded, but your saved data was NOT erased (${why}) and is still on the unit. ` +
          "To start from scratch, use MENU > SYSTEM > ERASE DATA on the unit." };
}

// What the unit runs decides the erase box's default. The unit's answer to the editor protocol's INFO (fm1backup.js, cmd 1) starts with
// "FELUCCA <version string>"; DUELL's version string is "DUELL 0.2.4 beta", Felucca's is a bare number, the official firmware does not answer.
export const SMART = {
  duell:   "Updating DUELL: your patches and settings are kept.",
  clean:   "Clean install recommended: the unit does not run DUELL, so old data from another firmware is cleared.",
  hint:    "Connect the FM-1 to check.",
  keepOwn: "You chose to keep the data: anything an older firmware left on the unit stays.",
  eraseOwn: "You chose to erase: your patches and settings will be erased after the install.",
  detected: { duell: (v, tide = false) => `Detected: DUELL ${v}${tide ? " with TIDE" : ""} running`, other: "Detected: not DUELL (or no answer)", none: "Not checked yet. Connect the FM-1 and press \"Check the unit\"." },
  checking: "Checking the unit ...",
};
// info: the version string from INFO (a string), or null/undefined/anything else when the unit gave no usable answer.
// Returns { kind: "duell", version } or { kind: "other" }. (Not having asked at all is `null` for the caller, never this.)
// A TIDE unit's INFO has the tag 44 01 1 (its version string stays "DUELL 1.1"); the page passes "DUELL 1.1 TIDE" then, and { tide: true } comes back.
export function detectUnit(info) {      // (item 91: a test build "DUELL 1.1.0-test3" is DUELL too; before, it read as "not DUELL" and the erase box came up ticked)
  const m = typeof info === "string" ? /^(?:FELUCCA )?DUELL (\d+\.\d+(?:\.\d+)?(?:-test\d+)?)( TIDE)?(?!\S)/.exec(info.trim()) : null;
  return m ? { kind: "duell", version: m[1], tide: !!m[2] } : { kind: "other" };
}
// The erase box: detected = detectUnit(...) or null (not asked / could not talk yet); touched = the user clicked the box;
// current = the box as it stands. Once touched the box is never changed. Returns what the page shows.
export function eraseDecision({ detected = null, touched = false, current = false } = {}) {
  const isDuell = !!detected && detected.kind === "duell";
  const checked = touched ? !!current : !isDuell;
  const line = !detected ? SMART.detected.none : isDuell ? SMART.detected.duell(detected.version, detected.tide) : SMART.detected.other;
  let help = isDuell ? SMART.duell : SMART.clean;
  if (touched && checked && isDuell) help = SMART.eraseOwn;
  else if (touched && !checked && !isDuell) help = SMART.keepOwn;
  else if (!detected && !touched) help += " " + SMART.hint;
  return { checked, line, help };
}

export function uiState(s) {
  const busy = s.busy || null;
  const channel = s.channel === "beta" ? "beta" : "stable";
  const needAck = channel === "beta" && !s.betaAck;        // a beta needs the extra confirmation before the Install button works
  const wants = {
    backup: s.ready,
    install: s.ready && s.hasImage && !needAck,
    return: s.ready && s.hasStock,
  };
  const buttons = {};
  for (const a of ACTIONS) {
    let note = "";
    if (busy && busy !== a) note = `Locked: ${NAMES[busy]} is running. Wait until it says DONE.`;
    else if (busy === a) note = "Running. Do not unplug.";
    else if (a === "install" && s.ready && s.hasImage && needAck) note = BETA.lockNote;
    buttons[a] = { enabled: !busy && !!wants[a], note };
  }
  return {
    banner: busy ? BANNERS[busy] : null,
    buttons,
    bars: { backup: busy === "backup", install: busy === "install", return: busy === "return" },   // a bar is shown only in its own card, only while it runs
    inputsLocked: !!busy,                  // file pickers and checkboxes
    channelLocked: !!busy,                 // the Stable / Beta choice and the beta box
    eraseBox: { disabled: !!busy, note: busy ? ERASE.lockNote : "" },   // the INSTALL card's erase box: locked while anything runs
    confirmUnload: !!busy,                 // beforeunload protection
  };
}

// The install saves a backup of a DUELL unit first (the page: backupBeforeInstall). A unit that runs no DUELL (the official firmware, or no answer) has nothing to save.
export const BACKUP_FIRST = {
  looking: "Backing up the unit first (nothing is written yet)",
  saved: "Backup saved to your downloads. Writing the firmware ...",
  failed: (why) => `The backup failed (${why}). Nothing was written to the unit: the install did not start. Keep the cable connected, stop the unit if it plays, and press Install again.`,
  cannot: "This DUELL build cannot make a complete backup, so your sounds and settings cannot be saved first. Install anyway?",
  declined: "Nothing was written: the install did not start.",
};

// texts per card
export const SWITCH_OFF_ON = "Switch the unit off and on once.";
export const TEXT = {
  writing: { install: "Writing DUELL. Do not unplug", return: "Writing official firmware. Do not unplug" },
  done: {
    install: "Done. Switch the unit off and on once.",   // (owner 2026-10-21, item 31: the boot guard step; the one line after a successful install)
    return: "Done. The official firmware is back. Switch the unit off and on.",
  },
};

// ---- the two builds: Stable and Beta (the Pages workflow publishes install/channels.json, see web/make_site.py)
// Stable is the default and the recommended one: the build the fork owner validated on a unit. Beta is a build under test.
export const REPO_URL = "https://github.com/pssdrnk-cmyk/DUELL-TIDE";
export const CHANNELS = ["stable", "beta"];
export const CHANNEL = {
  stable: { label: "Stable", badge: "Recommended",
            blurb: "Played and validated on the fork owner's FM-1. Choose this one unless you want to help test." },
  beta:   { label: "Beta", badge: "Under test",
            blurb: "A newer build that is still being tested. It can have bugs the stable build does not." },
};
export const BETA = {
  warning: "This is a beta. It is under test and has known issues. The stable build is the one the fork owner validated.",
  ackLabel: "I understand this is a beta under test. I have a backup, or I accept the risk without one.",
  lockNote: "Beta: tick the box above to enable the install.",
  none: "No known issues are listed in the release notes. Read the notes before you install.",
};
export const NOT_RELEASED = {
  title: "DUELL with TIDE is not released yet.",
  text: "No stable or beta package is published with this page, so there is nothing to install here. The emulator works without a unit.",
};
export const HASH_TEXT = {
  checking: "Checking the SHA-256 in your browser ...",
  ok: "SHA-256 checked in this browser: it matches the release data of this site. The release notes on GitHub print the same value: compare them.",
  nohash: "The release data has no SHA-256 for this build, so it cannot be checked. Nothing will be written.",
  size: "The downloaded file has another size than the release says. It was not accepted. Nothing will be written.",
  bad: (want, got) => `The downloaded file does not match the release's SHA-256 (release ${want}, file ${got}). ` +
                      "It was not accepted and nothing will be written. Reload the page; if it happens again, do not install and report it in the issues.",
};

const HEX = /^[0-9a-f]+$/;
const SAFE_PATH = /^(?!\/)(?!.*\.\.)[A-Za-z0-9._\-\/]+$/;       // relative, no "..", no scheme or backslash: a file next to the page
const clean = (v, n) => (typeof v === "string" ? v.replace(/[\u0000-\u001f]/g, " ").trim().slice(0, n) : "");
const sha = (v) => (typeof v === "string" && /^[0-9a-fA-F]{64}$/.test(v.trim()) ? v.trim().toLowerCase() : "");

// One manifest entry -> a checked entry, or null. The page never trusts a field it cannot check (a bad sha256 or file makes the build unavailable).
export function parseChannel(name, e) {
  if (!e || typeof e !== "object" || (e.channel && e.channel !== name)) return null;
  const file = typeof e.file === "string" && SAFE_PATH.test(e.file) && e.file.endsWith(".fwsc") ? e.file : "";
  const sha256 = sha(e.sha256);
  const tag = clean(e.tag, 80);
  if (!file || !sha256 || !tag) return null;
  const commit = typeof e.commit === "string" && HEX.test(e.commit.toLowerCase()) && e.commit.length >= 7 && e.commit.length <= 40 ? e.commit.toLowerCase() : "";
  const notesUrl = typeof e.notes_url === "string" && /^https:\/\//.test(e.notes_url) ? e.notes_url : "";
  const issues = Array.isArray(e.known_issues) ? e.known_issues.map((x) => clean(x, 300)).filter(Boolean).slice(0, 12) : [];
  return {
    channel: name, tag, file, sha256, commit, notesUrl, issues,
    version: clean(e.version, 24),
    date: /^\d{4}-\d{2}-\d{2}$/.test(e.date || "") ? e.date : "",
    size: Number.isInteger(e.size) && e.size > 0 ? e.size : 0,
    product: /^FM-1_9\d\d$/.test(e.product || "") ? e.product : "",
    summary: clean(e.summary, 400),
  };
}
// the whole install/channels.json -> { stable, beta } (each an entry or null)
export function parseManifest(m) {
  const c = m && typeof m === "object" && m.channels && typeof m.channels === "object" ? m.channels : {};
  return { stable: parseChannel("stable", c.stable), beta: parseChannel("beta", c.beta) };
}
// Which build is selected. wanted: what the user (or the address, #beta) asked for. Stable wins by default; a channel that is not
// published is never selected; null when there is no build at all.
export function chooseChannel(manifest, wanted) {
  const have = (c) => !!(manifest && manifest[c]);
  if (CHANNELS.includes(wanted) && have(wanted)) return wanted;
  if (have("stable")) return "stable";
  if (have("beta")) return "beta";
  return null;
}
// The check before anything is written: the downloaded bytes must be the file the release data describes.
// entry: a parseChannel result; got: { sha256, size } of the bytes the browser downloaded. Returns { ok, text }.
export function verifyDownload(entry, got) {
  if (!entry || !entry.sha256) return { ok: false, text: HASH_TEXT.nohash };
  if (entry.size && got.size !== entry.size) return { ok: false, text: HASH_TEXT.size };
  const have = String(got.sha256 || "").toLowerCase();
  if (have !== entry.sha256) return { ok: false, text: HASH_TEXT.bad(entry.sha256, have || "(none)") };
  return { ok: true, text: HASH_TEXT.ok };
}
// the rows the page shows for "what will be written"
export function infoRows(entry, product, size) {
  const rows = [["Build", `${CHANNEL[entry.channel].label}, ${entry.tag}`]];
  if (entry.date) rows.push(["Date", entry.date]);
  if (entry.commit) rows.push(["Commit", entry.commit.slice(0, 7)]);
  rows.push(["Package", `${entry.file.split("/").pop()} (${size || entry.size || "?"} bytes${product ? ", " + product : ""})`]);
  rows.push(["SHA-256", entry.sha256]);
  return rows;
}
// the question asked once more at the click on Install, for a beta only
export function betaConfirmText(entry) {
  return `Install the BETA build ${entry.tag}${entry.commit ? " (" + entry.commit.slice(0, 7) + ")" : ""}?\n\n` +
    "It is under test and has known issues. The stable build is the one the fork owner validated. Back up first. Do not unplug while it writes.";
}
