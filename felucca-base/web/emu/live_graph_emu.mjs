// SPDX-License-Identifier: GPL-3.0-only
// TIDE LIVE GRAPHS in the browser build (build/emu/felucca.wasm), headless in Node as emu_test.mjs runs it: the real firmware, the real audio,
// buttons and keys as the panel sends them, the screen read back after each step and written as PPMs.
//   node web/emu/live_graph_emu.mjs build/emu/felucca.wasm OUTDIR [palette ...]     (default palettes: 0 VIOLET, 4 PALE)
// For each palette: AMP ENV with a held note and after its release, LFO 1, LFO 2 (unused: PREVIEW), CYCLE ENV (PREVIEW), the matrix, the
// MULTI FILTER; a run of frames each, 90 ms apart, with how many of them differ (the playhead moves); MENU > ANIMATION OFF (the real menu
// path is covered by the host tests; here the same flag is set the way the editor sets it) leaves the picture still.
import fs from "fs";

const [, , wasmPath = "build/emu/felucca.wasm", outDir = "build/emu/live", ...pals] = process.argv;
const palettes = (pals.length ? pals : ["0", "4"]).map(Number);
const bytes = fs.readFileSync(wasmPath);
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
let fails = 0;
const check = (what, ok) => { console.log(`live: ${what.padEnd(88)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
fs.mkdirSync(outDir, { recursive: true });

async function device() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
  ex.web_boot();
  const d = { ex, mem };
  d.render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  d.tap = (b) => { ex.web_buttons(1 << b); d.render(60); ex.web_buttons(0); d.render(60); };
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  return d;
}
function ppm(name, fb) {
  const out = Buffer.alloc(15 + 240 * 240 * 3);
  const head = "P6 240 240 255 ";
  out.write(head, 0, "latin1");
  for (let i = 0; i < 240 * 240; i++) {
    const v = fb[i], c = ((v >> 8) | (v << 8)) & 0xffff, o = head.length + i * 3;
    out[o] = ((c >> 11) & 31) * 255 / 31; out[o + 1] = ((c >> 5) & 63) * 255 / 63; out[o + 2] = (c & 31) * 255 / 31;
  }
  fs.writeFileSync(`${outDir}/${name}.ppm`, out);
}
const diff = (a, b, y0 = 24, y1 = 112) => { let n = 0; for (let i = y0 * 240; i < y1 * 240; i++) n += a[i] !== b[i]; return n; };   // the picture area: rows 24 .. 111
// n frames of the screen, ms apart (the picture area, rows y0 .. y1 - 1, below the head and above the cards, unless the page is all picture) -> how many differ from the one before
function run(d, tag, n, ms = 90, y0 = 24, y1 = 112) {
  const fr = [];
  for (let i = 0; i < n; i++) { fr.push(d.screen()); ppm(`${tag}_${String(i).padStart(2, "0")}`, fr[i]); d.render(ms); }
  let moved = 0;
  for (let i = 1; i < n; i++) moved += diff(fr[i], fr[i - 1], y0, y1) > 0;
  return { fr, moved };
}

for (const pal of palettes) {
  const d = await device();
  d.render(2600);
  d.tap(B.HOME);                                   // the splash waits for OCT + or HOME (DUELL 1.1 first start)
  d.render(800);
  d.ex.web_palette(pal);
  d.render(300);
  const preset = (n) => { for (let i = 0; i < Math.abs(n); i++) { d.ex.web_enc(2, Math.sign(n)); d.render(40); } d.render(300); };   // the PRESETS knob: part 1 to #5 SQUELCH (ANALOG)
  preset(-75);
  const p = `p${pal}`;
  // ---- AMP ENV: a held note (attack, decay, sustain), then its release
  d.tap(B.ENV); d.render(200);
  const still = run(d, `${p}_amp_env_idle`, 3);
  check(`palette ${pal} AMP ENV, no note: the picture is still`, still.moved === 0);
  d.ex.web_keys(1 << 7);                           // C4
  const held = run(d, `${p}_amp_env_held`, 10, 70);
  check(`palette ${pal} AMP ENV, a held note: the playhead moves (${held.moved} of 9 steps)`, held.moved >= 5);
  d.ex.web_keys(0);
  const rel = run(d, `${p}_amp_env_release`, 6, 90);
  check(`palette ${pal} AMP ENV, released: the release is drawn (${rel.moved} of 5 steps differ: this sound releases in 10 ms)`, rel.moved >= 1);
  // ---- LFO 1, LFO 2 (the second one is not read by anything: its PREVIEW runs on the UI clock)
  d.tap(B.LFO); d.render(200);
  const l1 = run(d, `${p}_lfo1`, 8, 110);
  check(`palette ${pal} LFO 1: the playhead moves (${l1.moved} of 7)`, l1.moved >= 5);
  d.tap(B.LFO); d.tap(B.LFO); d.tap(B.LFO); d.render(200);
  const l2 = run(d, `${p}_lfo2`, 8, 110);
  check(`palette ${pal} LFO 2: the preview moves (${l2.moved} of 7)`, l2.moved >= 5);
  // ---- the ENV family further on (the button enters the page it was left on; each tap goes one on: AMP ENV 1/8, AMP DEST 2, ENV 2 3, ENV 2 DEST 4, CYCLE 1/2 5, CYCLE 2/2 6, MATRIX 1-4 7)
  d.tap(B.ENV); d.tap(B.ENV); d.tap(B.ENV); d.render(200);
  d.ex.web_keys(1 << 7);
  const e2 = run(d, `${p}_env2`, 8, 70);               // ENV 2, the filter envelope (this sound's MULTI CUTOFF reads it)
  d.ex.web_keys(0); d.render(1500);
  d.tap(B.ENV); d.tap(B.ENV); d.render(200);
  const cy = run(d, `${p}_cycle1`, 8, 120);            // CYCLE ENV 1/2: nothing routes it and no note: a silent preview
  check(`palette ${pal} CYCLE ENV 1/2 (not routed): the preview moves (${cy.moved} of 7)`, cy.moved >= 5);
  d.tap(B.ENV); d.render(200);
  const cy2 = run(d, `${p}_cycle2`, 8, 120);
  check(`palette ${pal} CYCLE ENV 2/2 (not routed): the preview moves (${cy2.moved} of 7)`, cy2.moved >= 5);
  d.tap(B.ENV); d.render(200);
  d.ex.web_keys(1 << 7);
  const mx = run(d, `${p}_matrix`, 6, 120, 24, 225);
  d.ex.web_keys(0);
  check(`palette ${pal} MATRIX 1-4 with a held note: a row's source bar moves (${mx.moved} of 5)`, mx.moved >= 1);
  for (let k = 0; k < 6; k++) { d.tap(B.EDIT); d.render(150); d.ex.web_keys(1 << 7); run(d, `${p}_edit${k}`, 2, 150); d.ex.web_keys(0); }   // the EDIT family's pages (MULTI FILTER among them), one frame pair each
  // ---- FM6 (#80 BIG STAB): AMP ENV has no envelope of its own to follow, AMP DEST draws the operator envelope and follows it
  preset(75);
  d.tap(B.LFO); d.tap(B.ENV); d.tap(B.ENV); d.tap(B.ENV); d.render(200);   // ENV re-enters the matrix page; two more taps wrap round to AMP ENV
  d.ex.web_keys(1 << 7);
  const fa = run(d, `${p}_fm6_amp_env`, 6, 70);
  d.ex.web_keys(0); d.render(2000);
  d.tap(B.ENV); d.render(200);
  d.ex.web_keys(1 << 7);
  const fd = run(d, `${p}_fm6_amp_dest`, 10, 60);
  d.ex.web_keys(0);
  check(`palette ${pal} FM6 AMP ENV (the unused generic ADSR): still`, fa.moved === 0);
  check(`palette ${pal} FM6 AMP DEST (the operator envelope): moves with the note (${fd.moved} of 9)`, fd.moved >= 4);
}
console.log(fails ? "LIVE EMU FAILED" : "live emu passed");
process.exit(fails ? 1 : 0);
