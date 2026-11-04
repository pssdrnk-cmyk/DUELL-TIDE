// SPDX-License-Identifier: GPL-3.0-only
// node test_install_flow.mjs SITE_DIR   (SITE_DIR: a folder made by web/make_site.py with a stable and a beta package)
// The installer page in headless Chrome against a SIMULATED FM-1 (a fake WebMIDI injected into the page; no real unit is touched, no MIDI is used).
// Checks: Stable is the default; Beta needs its box and a second confirmation; the install runs end to end for both builds and the unit
// reports the package's identity afterwards; resuming an interrupted install; a package whose bytes do not match the SHA-256 in
// channels.json is refused and nothing is sent to the unit; a manifest with a wrong SHA-256 likewise.
// Skips itself when there is no SITE_DIR or no Chrome (set CHROME=/path/to/chrome).
import http from "node:http";
import { existsSync, readFileSync, statSync } from "node:fs";
import { extname, join, normalize, resolve } from "node:path";
import { launch, CHROME } from "./cdp.mjs";

const dir = process.argv[2] ? resolve(process.argv[2]) : "";
if (!dir || !existsSync(join(dir, "install", "channels.json")) || !CHROME) {
  console.log("install flow: skipped (needs a built site folder as the argument and Chrome)");
  process.exit(0);
}
let failed = 0;
const ok = (c, m) => { console.log(`${m.padEnd(90)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const MIME = { ".html": "text/html", ".js": "text/javascript", ".json": "application/json", ".css": "text/css", ".woff": "font/woff", ".png": "image/png", ".jpg": "image/jpeg", ".fwsc": "application/octet-stream" };

// a static server for the site, with switches that damage what the page downloads
const mode = { tamperManifest: false, tamperPackage: false };
const server = http.createServer((req, res) => {
  const url = new URL(req.url, "http://x");
  const p = normalize(decodeURIComponent(url.pathname)).replace(/^([/\\])+/, "");
  let f = join(dir, p);
  if (existsSync(f) && statSync(f).isDirectory()) f = join(f, "index.html");
  if (!f.startsWith(dir) || !existsSync(f)) { res.writeHead(404); res.end("no"); return; }
  let body = readFileSync(f);
  if (mode.tamperManifest && p.endsWith("channels.json")) {
    const m = JSON.parse(body.toString());
    m.channels.beta.sha256 = "0".repeat(64);
    body = Buffer.from(JSON.stringify(m));
  }
  if (mode.tamperPackage && p.endsWith(".fwsc")) { body = Buffer.from(body); body[1000] ^= 1; }
  res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream", "cache-control": "no-store" });
  res.end(body);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const base = `http://127.0.0.1:${server.address().port}`;

// injected before the page's own scripts: a WebMIDI with one simulated FM-1 (the protocol of fm1ota.js: handshake, update mode, reads, loader)
const FAKE = `(() => {
  const HS = [0xF0,0,0x32,0x45,0,0,0,0x40,0x7F,0xF7], UPGRADE = [0xF0,0x22,0x24,0x35,0x7F,0xF7];
  const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
  const pack7 = (data) => { const out = []; let acc = 0, nb = 0; for (const b of data) { acc |= b << nb; nb += 8; while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } } if (nb) out.push(acc & 0x7F); return out; };
  const unpack7 = (s) => { const out = []; let acc = 0, nb = 0; for (const b of s) { acc |= (b & 0x7F) << nb; nb += 7; while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; } } return out; };
  const w = window.__fake = { sent: 0, served: 0, bad: 0, upgrades: 0, identity: null, log: [], startAs: window.__startAs || "FM-1_015" };
  const access = { inputs: new Map(), outputs: new Map(), onstatechange: null };
  let waiting = null, queue = [], target = "FM-1_901";
  function boot(identity, name) {
    w.identity = identity; waiting = null; queue = [];
    for (const m of [access.inputs, access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    const input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    const output = { id: "o" + id, name, state: "connected", open: async () => {}, send: (d) => { if (output.state !== "connected") throw new Error("InvalidStateError"); w.sent++; setTimeout(() => rx(Array.from(d)), 1); } };
    access.inputs.set(input.id, input); access.outputs.set(output.id, output); w.input = input;
  }
  const tx = (bytes) => setTimeout(() => { const i = w.input; if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1);
  function next() {
    const r = queue.shift(); if (!r) return; waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0; for (let i = 6; i < 14; i++) s += u[i]; u.push(~s & 0xFF);
    tx([0xF0, ...pack7(u), 0xF7]);
  }
  function rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(w.identity)];
      tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      w.upgrades++;
      queue = w.identity.startsWith("ota-") ? [...Array.from({ length: 6 }, (_, k) => [k * 512, 512]), [0xF0000000, 8]] : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      next();
    } else if (waiting) {
      const [addr] = waiting; waiting = null; w.served++;
      if (addr === 0xE0000000) setTimeout(() => boot("ota-FM-1_901", "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => boot(target, "Felucca"), 300);
      else next();
    }
  }
  boot(w.startAs, w.startAs.startsWith("ota-") ? "Felucca Update" : "FM-1");
  navigator.requestMIDIAccess = async () => access;
  Object.defineProperty(navigator, "permissions", { value: { query: async () => ({ state: "prompt" }) }, configurable: true });
  window.__confirms = []; window.__answer = true;
  window.confirm = (t) => { window.__confirms.push(String(t)); return window.__answer; };
})();`;

const c = await launch();
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let scriptId = null;
async function open(hash, { startAs = "FM-1_015", answer = true } = {}) {
  if (scriptId) await c.send("Page.removeScriptToEvaluateOnNewDocument", { identifier: scriptId });
  scriptId = (await c.send("Page.addScriptToEvaluateOnNewDocument", { source: `window.__startAs = ${JSON.stringify(startAs)};` + FAKE + `window.__answer = ${answer};` })).identifier;
  await c.send("Emulation.setDeviceMetricsOverride", { width: 1000, height: 900, deviceScaleFactor: 1, mobile: false });
  await c.send("Page.navigate", { url: `${base}/install/?r=${Math.random()}${hash}` });
  await sleep(300);
  await c.until("document.readyState === 'complete'", 10000);
  await c.until("!!document.getElementById('hashstate') && document.getElementById('hashstate').textContent !== '' && !document.getElementById('hashstate').textContent.startsWith('Checking')", 15000).catch(() => {});
}
const t = (id) => c.eval(`document.getElementById(${JSON.stringify(id)}).textContent`);
const dis = (id) => c.eval(`document.getElementById(${JSON.stringify(id)}).disabled`);
const click = (id) => c.eval(`document.getElementById(${JSON.stringify(id)}).click()`);
const manifest = JSON.parse(readFileSync(join(dir, "install", "channels.json"), "utf8")).channels;
const hasBeta = !!manifest.beta;
const untick = () => c.eval("(() => { const e = document.getElementById('erase-after'); if (e.checked) e.click(); })()");

// 1. Stable is the default
await open("");
ok(await c.eval("document.getElementById('ch-stable').checked && document.getElementById('beta-panel').hidden"), "Stable is the default build, the beta panel is hidden");
ok((await t("hashstate")).startsWith("SHA-256 checked") && !(await dis("go")), "Stable: the SHA-256 is checked in the browser and Install is on without a box");
ok((await t("pkgfacts")).includes(manifest.stable.sha256) && (await t("pkgfacts")).includes(manifest.stable.tag), "Stable: the page shows the tag and the SHA-256 that will be written");
await untick();
await click("go");
await c.until("document.getElementById('result-install').textContent.length > 0", 90000);
ok((await t("result-install")) === "Done. Switch the unit off and on once.", "Stable: installed end to end against the simulated unit (Done line)");
const f1 = await c.eval("({ served: __fake.served, identity: __fake.identity, upgrades: __fake.upgrades })");
ok(f1.served > 8 && f1.identity === "FM-1_901" && f1.upgrades === 2, `Stable: the unit went through update mode and reports the package identity (${JSON.stringify(f1)})`);
ok((await t("log")).includes(`install: stable ${manifest.stable.tag} ${manifest.stable.sha256}`), "Stable: the log names the build and its SHA-256");
ok((await c.eval("__confirms.length")) === 0, "Stable: no extra confirmation");

if (hasBeta) {
  // 2. Beta: the box, then the confirmation
  await open("#beta");
  ok(await c.eval("document.getElementById('ch-beta').checked && !document.getElementById('beta-panel').hidden"), "#beta preselects the beta and shows its warning");
  ok((await t("beta-issues")).length > 0 || !(await c.eval("document.getElementById('beta-noissues').hidden")), "Beta: known issues (or the no-issues line) are shown");
  ok((await t("hashstate")).startsWith("SHA-256 checked") && (await dis("go")) && /tick the box/i.test(await t("lock-install")), "Beta: verified, but Install is off until the box is ticked");
  await click("go");
  ok((await c.eval("__fake.sent")) === 0 && (await c.eval("__confirms.length")) === 0, "Beta: a click on the disabled Install sends nothing");
  await click("beta-ack");
  ok(!(await dis("go")), "Beta: the box turns Install on");
  await c.eval("window.__answer = false");
  await untick();
  await click("go"); await sleep(500);
  ok((await c.eval("__confirms.length")) === 1 && /BETA build/.test(await c.eval("__confirms[0]")) && (await c.eval("__fake.sent")) === 0 && (await t("result-install")) === "", "Beta: the second confirmation is asked; 'Cancel' writes nothing");
  await c.eval("window.__answer = true");
  await click("go");
  await c.until("document.getElementById('result-install').textContent.length > 0", 90000);
  ok((await t("result-install")) === "Done. Switch the unit off and on once." && (await c.eval("__fake.identity")) === "FM-1_901", "Beta: installed after the box and the confirmation");
  // switching away and back clears the box
  await open("#beta");
  await click("beta-ack");
  await click("ch-stable"); await sleep(600); await click("ch-beta"); await sleep(1200);
  ok(!(await c.eval("document.getElementById('beta-ack').checked")) && (await dis("go")), "Beta: choosing it again asks for the box again");

  // 3. a manifest whose SHA-256 is not the package's
  mode.tamperManifest = true;
  await open("#beta");
  ok((await t("hashstate")).includes("does not match") && (await dis("go")) && (await t("pkgstate")).includes("cannot be installed"), "Hash: the beta whose manifest SHA-256 differs from the package is refused");
  await click("beta-ack"); await click("go");
  ok((await c.eval("__fake.sent")) === 0, "Hash: nothing is sent to the unit, even with the box ticked");
  mode.tamperManifest = false;
}
// 4. a package damaged in transit (one byte)
mode.tamperPackage = true;
await open("#stable");
ok((await t("hashstate")).includes("does not match") && (await dis("go")), "Hash: a damaged stable package is refused, Install stays off");
await click("go");
ok((await c.eval("__fake.sent")) === 0 && !(await c.eval("document.getElementById('banner').hidden === false")), "Hash: nothing is sent to the unit, no install banner");
mode.tamperPackage = false;

// 5. resume: the unit is already in update mode
await open("#stable", { startAs: "ota-FM-1_901" });
await untick();
await click("go");
await c.until("document.getElementById('result-install').textContent.length > 0", 90000);
ok((await t("result-install")) === "Done. Switch the unit off and on once." && (await c.eval("__fake.identity")) === "FM-1_901" && (await c.eval("__fake.upgrades")) === 1, "Resume: a unit left in update mode is finished with the chosen build");

const errs = c.errors().filter((e) => !/404|favicon/.test(e));
ok(errs.length === 0, "no script errors in the page" + (errs.length ? ": " + errs[0] : ""));
await c.close();
server.close();
console.log(failed ? `INSTALL FLOW TESTS FAILED (${failed})` : "install flow tests passed");
process.exit(failed ? 1 : 0);
