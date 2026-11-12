// SPDX-License-Identifier: GPL-3.0-only
// A tiny Chrome DevTools Protocol client for the editor's browser test (no npm packages: the emsdk node has fetch and WebSocket). Headless Chrome on a throw-away profile.
//   const c = await launch(); await c.open(url, 1920, 1080); await c.eval("document.title"); await c.shot(path); await c.close();
import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { existsSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

export const CHROME = process.env.CHROME || ["/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", "/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser",
  ...[process.env.PROGRAMFILES, process.env["PROGRAMFILES(X86)"], process.env.LOCALAPPDATA].filter(Boolean).flatMap((p) => [join(p, "Google/Chrome/Application/chrome.exe"), join(p, "Microsoft/Edge/Application/msedge.exe")])].find(existsSync);

// REQUIRE_CHROME=1 (the CI workflow sets it): a missing browser is a failure, not a quiet "skip" that reports PASS
if (!CHROME && process.env.REQUIRE_CHROME) throw new Error("REQUIRE_CHROME is set but no Chrome was found (set CHROME=/path/to/chrome)");

// Stops a Chrome and everything it started (its own process group, where there is one), waits for the exit, removes the profile.
async function stop(proc, dir, exited) {
  // The parent may already have exited while a renderer still belongs to its group.
  if (process.platform !== "win32" || !exited.done) {
    try { process.kill(process.platform === "win32" ? proc.pid : -proc.pid, "SIGKILL"); } catch { try { proc.kill("SIGKILL"); } catch { /* gone */ } }
    await Promise.race([exited.p, new Promise((r) => setTimeout(r, 5000))]);
  }
  await rm(dir, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }).catch(() => {});
}

// One start. It stops waiting the moment Chrome has exited, else after 30 s; the error says why (exit code, the end of Chrome's stderr).
async function start() {
  const dir = await mkdtemp(join(tmpdir(), "duell-chrome-"));
  const args = ["--headless=new", "--remote-debugging-port=0", `--user-data-dir=${dir}`, "--no-first-run", "--no-default-browser-check", "--disable-gpu", "--disable-dev-shm-usage", "--hide-scrollbars=false"];
  if (process.env.CI) args.push("--no-sandbox");   // (a CI runner: the sandbox needs user namespaces, which the runner image may restrict)
  const proc = spawn(CHROME, [...args, "about:blank"], { stdio: ["ignore", "ignore", "pipe"], detached: process.platform !== "win32" });
  let err = "";
  proc.stderr.on("data", (d) => { err = (err + d).slice(-2000); });
  const exited = { done: false, how: "" };
  exited.p = new Promise((res) => { proc.once("exit", (code, sig) => { exited.done = true; exited.how = sig ? `signal ${sig}` : `exit code ${code}`; res(); }); proc.once("error", (e) => { exited.done = true; exited.how = `spawn error ${e.message}`; res(); }); });
  let port = 0;
  for (let i = 0; i < 300 && !port && !exited.done; i++) {
    await new Promise((r) => setTimeout(r, 100));
    try { port = +(await readFile(join(dir, "DevToolsActivePort"), "utf8")).split("\n")[0]; } catch { /* not yet */ }
  }
  if (!port) {
    const why = exited.done ? `Chrome ended (${exited.how})` : "no DevToolsActivePort after 30 s";
    await stop(proc, dir, exited);
    throw new Error(`Chrome did not start: ${why} [${CHROME}]${err.trim() ? "\n" + err.trim() : ""}`);
  }
  return { proc, dir, exited, port };
}

export async function launch() {
  if (!CHROME) throw new Error("no Chrome found (set CHROME=/path/to/chrome)");
  let st;
  try { st = await start(); } catch (e) {   // one retry: a start that was slow or lost to the previous Chrome's leftovers; the first error is kept in the log
    console.log(`cdp: first Chrome start failed (${String(e.message).split("\n")[0]}), trying once more`);
    st = await start();
  }
  const { proc, dir, exited, port } = st;
  let ws;
  try {
    // Chrome has written its port, but on a loaded machine the DevTools endpoint and the WebSocket answer late: poll every 250 ms for up to 30 s (a flat 5 s failed in CI)
    const t0 = Date.now();
    let last = "no answer";
    for (;;) {
      try {
        const tabs = await (await fetch(`http://127.0.0.1:${port}/json/list`, { signal: AbortSignal.timeout(2000) })).json();
        const page = tabs.find((t) => t.type === "page");
        if (!page) throw new Error("no page tab yet");
        const w = new WebSocket(page.webSocketDebuggerUrl);
        try {
          await new Promise((res, rej) => {
            const timer = setTimeout(() => rej(new Error("WebSocket not open after 2 s")), 2000);
            w.onopen = () => { clearTimeout(timer); res(); };
            w.onerror = (e) => { clearTimeout(timer); rej(e instanceof Error ? e : new Error("WebSocket error")); };
          });
        } catch (e2) { try { w.close(); } catch { /* closed */ } throw e2; }
        ws = w;
        break;
      } catch (e3) { last = String(e3.message || e3); }
      if (exited.done) throw new Error(`Chrome ended while waiting for its DevTools endpoint (${exited.how})`);
      if (Date.now() - t0 > 30000) throw new Error(`Chrome DevTools endpoint did not answer within 30 s (${last})`);
      await new Promise((r) => setTimeout(r, 250));
    }
  } catch (e) {
    try { ws?.close(); } catch { /* closed */ }
    await stop(proc, dir, exited);
    throw e;
  }
  let id = 0; const wait = new Map(), events = [];
  ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && wait.has(d.id)) { const [res, rej] = wait.get(d.id); wait.delete(d.id); d.error ? rej(new Error(d.error.message)) : res(d.result); } else if (d.method) events.push(d); };
  ws.onclose = () => { for (const [, rej] of wait.values()) rej(new Error("Chrome DevTools connection closed")); wait.clear(); };
  const send = (method, params = {}) => new Promise((res, rej) => {
    const i = ++id, timer = setTimeout(() => { wait.delete(i); rej(new Error(`Chrome DevTools timed out: ${method}`)); }, 30000);
    wait.set(i, [(v) => { clearTimeout(timer); res(v); }, (e) => { clearTimeout(timer); rej(e); }]);
    ws.send(JSON.stringify({ id: i, method, params }));
  });
  try {
    await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");
  } catch (e) {
    try { ws.close(); } catch { /* closed */ }
    await stop(proc, dir, exited);
    throw e;
  }
  const c = {
    send, events, errors: () => events.filter((e) => e.method === "Runtime.exceptionThrown" || (e.method === "Runtime.consoleAPICalled" && e.params.type === "error") || (e.method === "Log.entryAdded" && e.params.entry.level === "error")).map((e) => JSON.stringify(e.params).slice(0, 400)),
    async open(url, w, h) {
      await send("Emulation.setDeviceMetricsOverride", { width: w, height: h, deviceScaleFactor: 1, mobile: false });
      for (let tries = 0; ; tries++) {          // (a first load now and then stalls: navigate again)
        await send("Page.navigate", { url });
        try { await c.until(`location.href === ${JSON.stringify(url)} && document.readyState === 'complete' && !!window.__app`, 8000); break; } catch (e) { if (tries >= 2) throw e; }
      }
    },
    async size(w, h) { await send("Emulation.setDeviceMetricsOverride", { width: w, height: h, deviceScaleFactor: 1, mobile: false }); await c.sleep(150); },
    async eval(expr) {
      const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
      if (r.exceptionDetails) throw new Error(`page error: ${r.exceptionDetails.exception?.description || r.exceptionDetails.text}`);
      return r.result.value;
    },
    sleep: (ms) => new Promise((r) => setTimeout(r, ms)),
    async until(expr, ms = 15000) { const t = Date.now(); for (;;) { if (await c.eval(expr).catch(() => false)) return true; if (Date.now() - t > ms) throw new Error(`timeout waiting for: ${expr}`); await c.sleep(50); } },
    async shot(path) { const r = await send("Page.captureScreenshot", { format: "png" }); writeFileSync(path, Buffer.from(r.data, "base64")); },
    // mouse in page coordinates
    async mouse(type, x, y, o = {}) { await send("Input.dispatchMouseEvent", { type, x, y, button: o.button ?? "left", buttons: o.buttons ?? (type === "mouseReleased" || type === "mouseMoved" ? 0 : 1), clickCount: o.clickCount ?? 1, modifiers: o.modifiers ?? 0 }); },
    async click(x, y, o = {}) { await c.mouse("mousePressed", x, y, o); await c.mouse("mouseReleased", x, y, o); },
    async drag(x0, y0, x1, y1, o = {}) {
      await c.mouse("mouseMoved", x0, y0); await c.mouse("mousePressed", x0, y0, o);
      const n = 6; for (let i = 1; i <= n; i++) await c.mouse("mouseMoved", x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, { buttons: 1 });
      await c.mouse("mouseReleased", x1, y1, o);
    },
    // a key: name as KeyboardEvent.key; mods: 1 alt, 2 ctrl, 4 meta, 8 shift
    async key(k, mods = 0) {
      const codes = { ArrowLeft: 37, ArrowRight: 39, ArrowUp: 38, ArrowDown: 40, Delete: 46, Backspace: 8, Enter: 13, Escape: 27, Home: 36, End: 35 };
      const vk = codes[k] ?? k.toUpperCase().charCodeAt(0), text = (mods & 6) ? undefined : k.length === 1 ? k : k === "Enter" ? "\r" : undefined;
      await send("Input.dispatchKeyEvent", { type: text ? "keyDown" : "rawKeyDown", key: k, code: k.length === 1 ? "Key" + k.toUpperCase() : k, windowsVirtualKeyCode: vk, modifiers: mods, text });
      await send("Input.dispatchKeyEvent", { type: "keyUp", key: k, code: k.length === 1 ? "Key" + k.toUpperCase() : k, windowsVirtualKeyCode: vk, modifiers: mods });
    },
    async close() { try { ws.close(); } catch { /* closed */ } await stop(proc, dir, exited); },
  };
  return c;
}
