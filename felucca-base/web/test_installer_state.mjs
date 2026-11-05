// node test_installer_state.mjs : the installer's state -> UI mapping (no DOM, no device)
import { uiState, ACTIONS, BANNERS, TEXT, ERASE, eraseOutcome, SMART, detectUnit, eraseDecision,
  CHANNELS, CHANNEL, BETA, HASH_TEXT, parseChannel, parseManifest, chooseChannel, verifyDownload, infoRows, betaConfirmText } from "./installer_state.js";
import { readFileSync } from "node:fs";
let failed = 0;
const ok = (c, m) => { if (!c) { failed++; console.log("FAIL", m); } };
const base = { busy: null, ready: true, hasImage: true, hasStock: true };

function installerState() {
  // idle, everything prepared: all three enabled, no banner, no bar, no unload prompt
  let u = uiState(base);
  ok(ACTIONS.every((a) => u.buttons[a].enabled), "idle: all enabled");
  ok(!u.banner && !u.confirmUnload && !u.inputsLocked, "idle: no banner/lock");
  ok(ACTIONS.every((a) => !u.bars[a]), "idle: no bar");

  // idle gating (same rules as before)
  ok(uiState({ ...base }).buttons.install.enabled && uiState({ ready: true, hasImage: true }).buttons.install.enabled, "install is enabled by ready && hasImage alone (no checklist gate)");
  ok(!uiState({ ...base, hasImage: false }).buttons.install.enabled, "install needs a package");
  ok(!uiState({ ...base, hasStock: false }).buttons.return.enabled, "return needs the official file");
  ok(uiState({ ...base, hasImage: false, hasStock: false }).buttons.backup.enabled, "backup needs nothing else");
  ok(ACTIONS.every((a) => !uiState({ ...base, ready: false }).buttons[a].enabled), "no WebMIDI: all off");

  // running: exactly one bar (its own), all buttons off, banner for that action, notes on the others
  const words = { backup: /^BACKING UP:/, install: /^INSTALLING DUELL:/, return: /^RETURNING TO THE OFFICIAL FIRMWARE:/ };
  for (const a of ACTIONS) {
    u = uiState({ ...base, busy: a });
    ok(ACTIONS.every((x) => !u.buttons[x].enabled), `${a}: all buttons off`);
    ok(ACTIONS.filter((x) => u.bars[x]).join() === a, `${a}: only its own bar`);
    ok(u.banner && words[a].test(u.banner.text) && u.banner.kind === a, `${a}: banner`);
    ok(/do not unplug or close this page until it says DONE/.test(u.banner.text), `${a}: banner warns`);
    ok(u.confirmUnload && u.inputsLocked, `${a}: unload prompt and inputs locked`);
    for (const x of ACTIONS) if (x !== a) ok(/^Locked:/.test(u.buttons[x].note), `${a}: ${x} has a lock note`);
    // even with nothing missing the other buttons stay off; and with things missing nothing becomes enabled
    ok(ACTIONS.every((x) => !uiState({ ...base, busy: a, hasImage: false }).buttons[x].enabled), `${a}: still all off`);
  }
  ok(/Locked: the return to the official firmware/.test(uiState({ ...base, busy: "return" }).buttons.install.note), "install note names the return");

  // wording
  ok(TEXT.writing.return === "Writing official firmware. Do not unplug" && TEXT.writing.install === "Writing DUELL. Do not unplug", "writing texts");
  ok(TEXT.done.return === "Done. The official firmware is back. Switch the unit off and on.", "return done text");
  ok(BANNERS.return.text.startsWith("RETURNING TO THE OFFICIAL FIRMWARE: do not unplug or close this page until it says DONE"), "return banner text");
}

// the page wiring: three cards, each progress bar only in its own card, ids used by the script exist
function pageWiring() {
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  for (const a of ACTIONS) {
    const card = h.match(new RegExp(`<div class="card ${a}" id="card-${a}">([\\s\\S]*?)\\n    </div>\\n(?:    </details>\\n)?  </section>`));
    ok(card, `card ${a} exists`);
    if (!card) continue;
    for (const part of ["bar", "status", "result", "lock"])
      ok(card[1].includes(`id="${part}-${a}"`), `card ${a} has ${part}`);
    ok(!/id="(bar|status|result)-(?!${a}\b)\w+"/.test(card[1].replace(new RegExp(`id="\\w+-${a}"`, "g"), "")), `card ${a} holds no other card's ids`);
    ok(new RegExp(`<progress class="bar" id="bar-${a}"[^>]*\\shidden`).test(card[1]), `bar ${a} starts hidden`);
  }
  ok(/This writes the official M-VAVE firmware back\. DUELL will be removed\. Keep the cable connected until DONE\./.test(h), "return warning text");
  ok(/Go back to the official firmware/.test(h), "return section title");
  ok(!/id="(bar|status)"/.test(h), "no shared bar/status left");
  // the return card is not inside the package-gated blocks
  const gated = [...h.matchAll(/<div class="needpkg" hidden>([\s\S]*?)\n  <\/div>/g)].map((m) => m[1]).join("");
  ok(!gated.includes('id="card-return"') && !gated.includes('id="card-backup"') && gated.includes('id="card-install"'), "only install is package-gated");
}
// the optional "Erase my saved data after installing" box: locked while anything runs, and an outcome that never claims an erase the unit did not report
function eraseBox() {
  ok(!uiState(base).eraseBox.disabled && uiState(base).eraseBox.note === "", "erase box: usable when idle");
  for (const a of ACTIONS) ok(uiState({ ...base, busy: a }).eraseBox.disabled && /^Locked:/.test(uiState({ ...base, busy: a }).eraseBox.note), `erase box: locked while ${a} runs`);
  ok(ERASE.status === "Erasing your data ...", "erase status text");
  ok(ERASE.done === "Done. Your saved data was erased. The unit starts as new.", "erase done text");
  const yes = eraseOutcome(0);
  ok(yes.erased && !yes.bad && yes.text === ERASE.done, "rc 0: erased, the done line");
  const clean = (o) => !o.erased && o.bad && /install itself succeeded/.test(o.text) && /NOT erased/.test(o.text) && /MENU > SYSTEM > ERASE DATA/.test(o.text) && !/Your saved data was erased/.test(o.text);
  for (const [rc, err] of [[1, null], [3, null], [4, null], [99, null], [undefined, null], [null, new Error("timed out")], [null, "notfound"]])
    ok(clean(eraseOutcome(rc, err)), `rc ${rc} / ${err}: install succeeded, data NOT erased, menu item named`);
  ok(/playing/.test(eraseOutcome(3).text) && /could not be reached/.test(eraseOutcome(null, "x").text), "the reason is named");
  for (const w of ["settings", "autosave", "patches", "multis", "patterns", "factory sounds, multis and patterns come back", "cannot be undone", "back up first"])
    ok(ERASE.help.includes(w), `help says: ${w}`);
}
function erasePage() {
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  const card = h.match(/<div class="card install" id="card-install">([\s\S]*?)\n    <\/div>\n  <\/section>/);
  ok(card, "install card");
  if (!card) return;
  const box = card[1].match(/<input type="checkbox" id="erase-after"([^>]*)>/);
  ok(box && !/class="chk"/.test(box[1]), "erase box is in the INSTALL card, not a checklist box (its state comes from eraseDecision)");
  ok(card[1].includes(ERASE.label) && card[1].includes(ERASE.help), "the page shows the label and the description");
  ok(!document_has_chk_erase(h), "the checklist gating does not count the erase box");
  ok(/const wantErase = \$\("erase-after"\)\.checked;/.test(h) && /if \(installed && !wantErase\) result\("install", TEXT\.done\.install\)/.test(h), "the erase runs only when ticked, after the install finished");
  ok(TEXT.done.install === "Done. Switch the unit off and on once." && /result\("install", o\.text \+ " " \+ SWITCH_OFF_ON, o\.bad\)/.test(h) && /import \{[^}]*SWITCH_OFF_ON[^}]*\} from "..\/installer_state\.js"/.test(h), "install done text: the one boot-guard line (switch the unit off and on once), also after the optional erase");
  ok(/\$\("erase-after"\)\.disabled = u\.eraseBox\.disabled/.test(h), "the box is locked with the state");
}
const document_has_chk_erase = (h) => /class="chk"[^>]*id="erase-after"|id="erase-after"[^>]*class="chk"/.test(h);
// the smart default: what the unit runs decides the box, the user's own click always wins
function smartDefault() {
  const duell = detectUnit("DUELL 0.2.4 beta");
  ok(duell.kind === "duell" && duell.version === "0.2.4", "DUELL 0.2.4 beta is DUELL");
  ok(detectUnit("FELUCCA DUELL 0.2.4 beta").kind === "duell" && detectUnit("DUELL 0.2.4").version === "0.2.4", "with the FELUCCA prefix, without 'beta'");
  for (const v of [null, undefined, "", "FELUCCA 1.5.2", "1.2", "FELUCCA 1.0", "M-VAVE", "DUELLX 0.2", 42, "DUELL"])
    ok(detectUnit(v).kind === "other", `not DUELL: ${JSON.stringify(v)}`);
  let d = eraseDecision({ detected: duell });
  ok(d.checked === false && d.help === "Updating DUELL: your patches and settings are kept." && d.line === "Detected: DUELL 0.2.4 running", "DUELL answered: unticked, update text, detected line");
  d = eraseDecision({ detected: detectUnit(null) });
  ok(d.checked === true && d.help === "Clean install recommended: the unit does not run DUELL, so old data from another firmware is cleared." && d.line === "Detected: not DUELL (or no answer)", "no answer / other: ticked, clean text, not-DUELL line");
  d = eraseDecision({});
  ok(d.checked === true && d.help.startsWith(SMART.clean) && /Connect the FM-1 to check/.test(d.help), "not checked yet: ticked, clean text and the connect hint");
  ok(!/Detected: (DUELL|not DUELL)/.test(d.line) && /Not checked yet/.test(d.line), "never claims a detection without an answer");
  for (const det of [null, duell, detectUnit(null)]) {
    ok(eraseDecision({ detected: det, touched: true, current: true }).checked === true, "touched ticked stays ticked");
    ok(eraseDecision({ detected: det, touched: true, current: false }).checked === false, "touched unticked stays unticked");
  }
  ok(eraseDecision({ detected: duell, touched: true, current: true }).help === SMART.eraseOwn, "chose to erase on DUELL: says so");
  ok(eraseDecision({ detected: detectUnit(null), touched: true, current: false }).help === SMART.keepOwn, "chose to keep on non-DUELL: says so");
  ok(eraseDecision({ detected: duell, touched: true, current: false }).help === SMART.duell, "touched in line with the detection: the plain text");
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  ok(/id="erase-detect"/.test(h) && /id="erase-smart"/.test(h) && /id="detect"/.test(h), "page has the detected line, the recommendation and the check button");
  ok(/addEventListener\("change", \(\) => \{ touched = true;/.test(h), "a click on the box marks it touched");
  ok(/requestMIDIAccess/.test(h.split("a silent check at load")[1].split("// ---- install")[0]) && /state !== "granted"\) return/.test(h), "the load check asks nothing unless MIDI was already allowed");
}
// editor2 item 8 (owner A6): one button, no gating tick boxes, no numbered steps; the checks that protect the unit are enforced by code and stay (a test fails if one disappears)
function onePage() {
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8"), ota = readFileSync(new URL("./fm1ota.js", import.meta.url), "utf8");
  ok(!/class="chk"/.test(h) && !/Flash checklist/.test(h) && !/checksOk/.test(h), "install page: no flash checklist, no tick box gates the Install button");
  ok(!/<span class="n">\d<\/span>/.test(h), "install page: no numbered step headings");
  ok(/Do not switch off or unplug the unit while it installs\./.test(h) && /first saves a backup file to your downloads and stops if that fails \(<a href="\.\.\/editor\/backup\.html">the Backup page<\/a> restores it\)/.test(h), "install page: the do-not-switch-off line and the Back up first link (a link, not a gate)");
  const go = h.split('$("go").addEventListener("click"')[1].split("\n});")[0];
  ok(/else if \(await backupBeforeInstall\(up\)\) \{ await up\.install\(/.test(go) && !/up\.install\(/.test(go.replace(/else if \(await backupBeforeInstall\(up\)\) \{ await up\.install\(/, "")), "install page: every fresh install goes through backupBeforeInstall (only the update-mode resume skips it)");
  ok(h.indexOf("Do not switch off or unplug") < h.indexOf('<button id="go"') && h.indexOf('<button id="go"') < h.indexOf("RETURN TO OFFICIAL FIRMWARE"), "install page: the line stands above the one Install button");
  ok(/<details id="backup-details">/.test(h) && h.includes('id="card-backup"') && /async function doBackup\(/.test(h) && /await doBackup\(up, "return"\)/.test(h), "install page: the Back up card is kept as a closed details; doBackup stays for the return");
  // kept, enforced by code (silent)
  ok(/FM-1_9\\d\\d/.test(h) && h.includes("FELUCCA-LOADER-1") && /if \(!\/\^FM-1_9/.test(h), "kept: package identity FM-1_9xx and the loader marker FELUCCA-LOADER-1 are checked before a package is accepted");
  ok(/addEventListener\("beforeunload"/.test(h) && /if \(busy\)/.test(h.split('addEventListener("beforeunload"')[1].split("\n")[0]), "kept: the beforeunload prompt while writing");
  ok(/fail\("model"/.test(ota) && /fail\("mismatch"/.test(ota), "kept: fm1ota.js refuses another device model and a device that reports another firmware afterwards");
  ok(uiState({ ...base, busy: "install" }).confirmUnload && !uiState({ ...base, busy: "install" }).buttons.backup.enabled, "kept: one action at a time and the unload prompt (uiState)");
}
// ---- Stable / Beta: the channel choice, the beta confirmation, the manifest and the SHA-256 check
const SHA_S = "7795e77798d053177a4d0b099f0754701e16e308c92fbe22fd54913abac41b30", SHA_B = "d94ac77c368417a889728e57db64f4b0853eaa0a2152e37ad4c97df9014e5d7c";
const mk = (channel, tag, sha, extra = {}) => ({ channel, tag, date: tag.slice(-10), commit: "187e9d0", file: `${channel}/duell-tide-${tag}.fwsc`, size: 614666, sha256: sha, product: "FM-1_901",
  notes_url: "https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/" + tag, summary: "s", known_issues: ["one", "two"], ...extra });
const manifest = { schema: 1, channels: { stable: mk("stable", "stable-2026-11-03", SHA_S), beta: mk("beta", "beta-2026-11-04", SHA_B, { commit: "77a6df2" }) } };
function channels() {
  const m = parseManifest(manifest);
  ok(m.stable && m.beta && m.stable.sha256 === SHA_S && m.beta.commit === "77a6df2" && m.beta.issues.length === 2 && m.stable.notesUrl.startsWith("https://"), "manifest: both builds read with tag, commit, SHA-256, notes link, issues");
  // the choice: stable is the default and the recommended one; a beta only when asked for and published
  ok(chooseChannel(m, undefined) === "stable" && chooseChannel(m, "") === "stable" && chooseChannel(m, "junk") === "stable", "choice: stable is the default");
  ok(chooseChannel(m, "beta") === "beta" && chooseChannel(m, "stable") === "stable", "choice: beta when asked for (#beta), stable otherwise");
  ok(chooseChannel({ stable: m.stable, beta: null }, "beta") === "stable", "choice: an unpublished beta is never selected");
  ok(chooseChannel({ stable: null, beta: m.beta }, "stable") === "beta" && chooseChannel({ stable: null, beta: null }, "stable") === null && chooseChannel(null, "beta") === null, "choice: beta alone is selected; nothing published gives null");
  ok(CHANNELS.join() === "stable,beta" && CHANNEL.stable.badge === "Recommended" && CHANNEL.beta.badge === "Under test", "labels: Stable recommended, Beta under test");
  // the beta needs the extra confirmation; stable does not; nothing else changes
  const b = { ...base, channel: "beta" };
  ok(!uiState(b).buttons.install.enabled && uiState(b).buttons.install.note === BETA.lockNote, "beta without the box: Install is off, with the reason");
  ok(uiState({ ...b, betaAck: true }).buttons.install.enabled, "beta with the box: Install is on");
  ok(uiState({ ...base, channel: "stable" }).buttons.install.enabled && uiState({ ...base, channel: "stable", betaAck: false }).buttons.install.enabled, "stable: no box needed");
  ok(!uiState({ ...b, betaAck: true, hasImage: false }).buttons.install.enabled, "beta with the box but no verified package: Install stays off");
  ok(uiState({ ...b, betaAck: false }).buttons.backup.enabled && uiState({ ...b, betaAck: false }).buttons.return.enabled, "the beta box gates only Install (backup and return stay available)");
  ok(ACTIONS.every((a) => !uiState({ ...b, betaAck: true, busy: "install" }).buttons[a].enabled) && uiState({ ...base, busy: "install" }).channelLocked && !uiState(base).channelLocked, "the build choice is locked while an action runs");
  // a manifest is never trusted blindly
  for (const bad of [{ sha256: "abc" }, { sha256: "z".repeat(64) }, { file: "../evil.fwsc" }, { file: "/abs/x.fwsc" }, { file: "https://x/y.fwsc" }, { file: "stable/x.bin" }, { tag: "" }, { channel: "beta" }])
    ok(parseChannel("stable", { ...manifest.channels.stable, ...bad }) === null, `manifest: refuses ${JSON.stringify(bad).slice(0, 40)}`);
  ok(parseManifest(null).stable === null && parseManifest({}).beta === null && parseManifest({ channels: { stable: "x" } }).stable === null, "manifest: junk gives no build");
  ok(parseChannel("stable", { ...manifest.channels.stable, sha256: SHA_S.toUpperCase() }).sha256 === SHA_S, "manifest: SHA-256 is compared in lower case");
  ok(parseChannel("stable", { ...manifest.channels.stable, notes_url: "javascript:alert(1)" }).notesUrl === "", "manifest: a notes link must be https");
  const rows = infoRows(m.beta, "FM-1_901", 614666).map((r) => r.join("=")).join("|");
  ok(/Build=Beta, beta-2026-11-04/.test(rows) && /Date=2026-11-04/.test(rows) && /Commit=77a6df2/.test(rows) && rows.includes("SHA-256=" + SHA_B) && /614666 bytes, FM-1_901/.test(rows), "what will be written: build, date, commit, package, SHA-256");
  ok(/BETA build beta-2026-11-04 \(77a6df2\)/.test(betaConfirmText(m.beta)) && /under test/.test(betaConfirmText(m.beta)), "beta: the confirmation names the build");
}
function hashCheck() {
  const m = parseManifest(manifest);
  ok(verifyDownload(m.stable, { sha256: SHA_S, size: 614666 }).ok, "hash: the right bytes pass");
  ok(verifyDownload(m.stable, { sha256: SHA_S.toUpperCase(), size: 614666 }).ok, "hash: case does not matter");
  const bad = verifyDownload(m.stable, { sha256: SHA_B, size: 614666 });
  ok(!bad.ok && bad.text.includes(SHA_S) && bad.text.includes(SHA_B) && /nothing will be written/.test(bad.text), "hash: another file is refused, both digests shown");
  ok(!verifyDownload(m.stable, { sha256: "", size: 614666 }).ok && !verifyDownload(m.stable, { sha256: undefined, size: 614666 }).ok, "hash: no digest is a refusal");
  ok(!verifyDownload(m.stable, { sha256: SHA_S, size: 614665 }).ok && verifyDownload(m.stable, { sha256: SHA_S, size: 614665 }).text === HASH_TEXT.size, "hash: a wrong size is refused");
  ok(!verifyDownload(null, { sha256: SHA_S, size: 1 }).ok && !verifyDownload({ ...m.stable, sha256: "" }, { sha256: "", size: 614666 }).ok, "hash: a build with no SHA-256 in the release data cannot be installed");
  ok(verifyDownload({ ...m.stable, size: 0 }, { sha256: SHA_S, size: 5 }).ok, "hash: size is checked only when the release says it");
}
function channelPage() {
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  ok(/id="ch-stable"/.test(h) && /id="ch-beta"/.test(h) && /id="beta-ack"/.test(h) && h.includes(BETA.ackLabel), "page: Stable and Beta choices, the beta box with its text");
  ok(h.indexOf('id="ch-stable"') < h.indexOf('id="ch-beta"') && h.indexOf('id="choose"') < h.indexOf('<button id="go"'), "page: the choice stands above the Install button, Stable first");
  ok(/Beta: under test/.test(h) && /id="beta-issues"/.test(h) && /id="notes-beta"/.test(h) && /id="pkgfacts"/.test(h) && /id="hashstate"/.test(h), "page: beta warning, known issues, notes link, what will be written, hash state");
  // the hash is verified before a package becomes installable, and a failure leaves no image
  const acc = h.split("async function acceptPackage")[1].split("async function loadPackage")[0];
  ok(acc.indexOf("sha256hex(bytes)") >= 0 && acc.indexOf("verifyDownload(") > acc.indexOf("sha256hex(bytes)") && acc.indexOf("image = logicalImage(bytes)") > acc.indexOf("if (!check.ok)"), "page: SHA-256 is computed and checked before the image is made");
  ok(/if \(!check\.ok\) \{ image = null; product = null;[^}]*return; \}/.test(acc), "page: a mismatch clears the image and stops");
  ok(!/duell-latest|duell-version\.json/.test(h) && /fetch\("\.\/channels\.json"/.test(h), "page: reads channels.json (no duell-latest / duell-version.json)");
  ok(/if \(channel === "beta" && \(!betaAck \|\| !confirm\(betaConfirmText\(entry\)\)\)\) return;/.test(h) && h.indexOf("betaConfirmText(entry)") < h.indexOf("const wantErase"), "page: a beta asks again at the click, before anything is written");
  ok(/betaAck = false;/.test(h.split("function selectChannel")[1].split("\n}")[0]), "page: choosing a build clears the beta box");
  ok(/uiState\(\{[^}]*channel: channel \|\| "stable", betaAck/.test(h), "page: the channel and the box feed uiState");
  // legal and safety lines that must stay
  for (const t of ["GPL-3.0", "no warranty", "Felucca by Leo Kuroshita", "JIANT", "JieLi AC79 SDK", "Apache-2.0", "not affiliated with, endorsed by or supported by", "not by OSCILL/R", "This writes the official M-VAVE firmware back."])
    ok(h.includes(t), `page keeps: ${t}`);
  const l = readFileSync(new URL("./landing.html", import.meta.url), "utf8");
  for (const t of ["GPL-3.0", "no warranty", "Felucca", "JIANT", "JieLi AC79 SDK", "Apache-2.0", "not made, affiliated with or endorsed by OSCILL/R or M-VAVE", "TIDE by pssdrnk, built on DUELL 1.1 by OSCILL/R", "Install Stable", "Install Beta", "Try the emulator", "DUELL-TIDE/issues", "Use at your own risk"])
    ok(l.includes(t), `landing keeps: ${t}`);
  const strip = (x) => x.replace(/data:image[^"]*/g, "");
  ok(!/oscillr\/DUELL\/issues/.test(h + l), "pages: no bug-report link to OSCILL/R (they are credit and original-DUELL links only)");
  ok(!/(?:src|href)="https?:\/\/(?!pssdrnk-cmyk\.github\.io|github\.com|www\.m-vave\.com|oscillr\.github\.io)/.test(strip(h + l)) && !/<script[^>]+src="https?:/.test(h + l), "pages: no scripts, styles or fonts from other hosts");
}
channels(); hashCheck(); channelPage();
installerState(); pageWiring(); eraseBox(); erasePage(); smartDefault(); onePage();
console.log(failed ? `INSTALLER STATE TESTS FAILED (${failed})` : "installer state tests passed");
process.exit(failed ? 1 : 0);
