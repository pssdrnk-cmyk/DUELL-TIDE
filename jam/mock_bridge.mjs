// SPDX-License-Identifier: GPL-3.0-only
// The offline DUELL unit for jam/jam_dev.py --mock and jam/test_jam_dev.py: the web editor's own MockUnit (felucca-base/web/editor/mockunit.js, the same
// simulated unit test_duellbackup.mjs tests the Backup page against) behind a JSON-lines pipe. Nothing here talks to MIDI.
//
// One JSON object per line on stdin, one answer line on stdout:
//   {"op": "frame", "data": [F0 7D 46 4C cmd ... F7]}  -> {"frames": [[...], ...]}   the unit's answer as whole SysEx frames. Before each reply comes "noise" (a
//                                                         WATCH push, cmd 23, and a reply-shaped frame of another cmd) so the host's reply filter is exercised
//                                                         on every request; a reply the test asked to drop is left out (the host then times out)
//   {"op": "control", "action": ..., "args": {...}}    -> {"ok": true, ...} or {"ok": false, "error": "..."}   test hooks: see CONTROL below
import { createInterface } from "node:readline";
import { MockUnit, buildPatchRecord, buildMultiObject } from "../felucca-base/web/editor/mockunit.js";
import { readArchive, captureBackup, parseInfo, checkObject, DUELL_IDS } from "../felucca-base/web/editor/duellbackup.js";
import { bkCrc } from "../felucca-base/web/fm1backup.js";
import { storeDefaults, engineIndex, XB_DEFS, buildEndB } from "../felucca-base/web/editor/patchrecord.js";

const HDR = [0xf0, 0x7d, 0x46, 0x4c];
let unit = new MockUnit();                     // seeded as test_duellbackup.mjs's default unit: patches in slots 0 1 5 64, multis M03 M04 M12, pattern P03, wavetable USR2
let drops = [];                                // [{cmd, count, skip}]: replies to leave out (after `skip` replies of that cmd went out)
let noise = true;
const counts = {};                             // requests seen per cmd (and per GET piece, "66:id:slot:off")
const sent = [];                               // the request frames' args of cmd 67, for the tests (never retried, abort after a failure)

const isFrame = (d) => Array.isArray(d) && d.length >= 6 && HDR.every((b, i) => d[i] === b) && d.at(-1) === 0xf7 && d.slice(1, -1).every((b) => Number.isInteger(b) && b >= 0 && b <= 127);
const frame = (cmd, args) => [...HDR, cmd, ...args, 0xf7];

async function onFrame(d) {
  if (!isFrame(d)) return { frames: [], error: "not a DUELL editor frame (or a byte above 127)" };
  const cmd = d[4], args = d.slice(5, -1);
  counts[cmd] = (counts[cmd] ?? 0) + 1;
  if (cmd === 66) { const k = `66:${args[0]}:${args[0] === 40 ? args[8] : "-"}:${args.slice(1, 6).join(".")}`; counts[k] = (counts[k] ?? 0) + 1; }
  if (cmd === 67) sent.push(args);
  let reply;
  try { reply = await unit.request([cmd, args], {}); } catch { return { frames: [] }; }   // (the mock throws where the unit sends no reply)
  if (reply.some((b) => !(Number.isInteger(b) && b >= 0 && b <= 127))) return { frames: [], error: `the mock unit answered cmd ${cmd} with a byte above 127` };
  const drop = drops.find((x) => x.cmd === cmd && x.count > 0);
  if (drop && drop.skip > 0) drop.skip--;
  else if (drop) { drop.count--; return { frames: noise ? [frame(23, [0, 1])] : [] }; }
  return { frames: [...(noise ? [frame(23, [0, 1]), frame(cmd === 1 ? 25 : 1, [0])] : []), frame(cmd, reply)] };
}

const crcOf = (b) => (b && b.length ? bkCrc(b) : 0);
// the unit's content as a digest: every DUELL object (size and CRC) and every saved patch record (CRC), what a restore must bring back exactly
function state() {
  const objs = Object.fromEntries(DUELL_IDS.map((id) => { const b = unit.objs.get(id); return [id, `${b ? b.length : 0}:${crcOf(b)}`]; }));
  const patches = Object.fromEntries([...unit.patches.keys()].sort((a, b) => a - b).map((k) => [k, `${unit.patchBytes(k).length}:${crcOf(unit.patchBytes(k))}`]));
  return { objs, patches, staged: unit.staged !== null, playing: unit.playing, log: unit.log.filter((x) => !x.startsWith("cmd ")), counts: { ...counts }, sent };
}

const CONTROL = {
  reset: (m) => { unit = new MockUnit(m.options ?? {}); drops = []; sent.length = 0; for (const k of Object.keys(counts)) delete counts[k]; return {}; },
  state: () => ({ state: state() }),
  noise: (m) => { noise = !!m.on; return {}; },
  drop: (m) => { drops.push({ cmd: m.cmd, count: m.count ?? 1, skip: m.skip ?? 0 }); return {}; },
  playing: (m) => { unit.playing = !!m.on; return {}; },
  fail: (m) => { unit.fail = m.id == null ? null : { id: m.id, op: m.op, rc: m.rc }; return {}; },
  version: (m) => { unit.version = String(m.version); return {}; },
  // a saved patch; rich: with the extension block, 32 hand-set edits and 60 values off their default (the bitmap form), as test_duellbackup.mjs's biggest records
  setPatch: (m) => {
    const eng = m.engine ?? 0, rich = m.rich ? { values: storeDefaults(eng).map((v, i) => (i >= 40 && i < 100 ? (v >= 100 ? v - 1 : v + 1) : v)), ext: XB_DEFS.map((d) => d - 64 + 1),
      edits: Array.from({ length: 32 }, (_, i) => ({ wid: i < 26 ? i * 4 : 128 + (i - 26), val: i * 3 - 40 })), mb: buildEndB({ engine: eng, amount: 40, values: { 9: 5, 45: 70 }, marks: [9], ...(eng === engineIndex("FM6") ? { fm6: Array.from({ length: 65 }, (_, i) => i) } : {}) }),
      ...(eng === engineIndex("FM6") ? { patch128: Array.from({ length: 128 }, (_, i) => i) } : {}) } : {};   // (an FM6 one with its patch: over 256 bytes, two GET / PUT pieces)
    unit.setRecord(m.slot, buildPatchRecord({ name: m.name, engine: eng, cat: m.cat ?? 0, ...rich })); unit.rebuild(); return {};
  },
  alterPatch: (m) => { const b = unit.patchBytes(m.slot); b[6] = (b[6] + 1) % 9; unit.setRecord(m.slot, b); unit.rebuild(); return {}; },   // (its category: still a good record)
  deletePatch: (m) => { unit.patches.delete(m.slot); unit.rebuild(); return {}; },
  setMulti: (m) => { unit.objs.set(m.id, buildMultiObject(m.names)); unit.fromObjects(m.id); return {}; },
  // the web editor's own object check (duellbackup.js checkObject) on [{key, data: base64}]: the reference for jam_dev.py's port of it
  checkObjects: (m) => ({ reasons: m.items.map((it) => checkObject(it.key, Uint8Array.from(Buffer.from(it.data, "base64")))) }),
  // the web editor's own reader on a file's text: what the Backup page would say when the file is chosen
  readArchive: (m) => {
    try {
      const a = readArchive(m.text);
      return { accepted: true, version: a.version, firmware: a.firmware, objects: a.objects.size, patches: [...a.objects.keys()].filter((k) => k >= 1000).map((k) => k - 1000), problems: a.problems, notes: a.notes, hasPatches: a.hasPatches };
    } catch (e) { return { accepted: false, reason: e.message }; }
  },
  // the web editor's own capture of this unit, as the page downloads it (JSON.stringify): the byte-for-byte reference for jam_dev.py's file
  captureJs: async () => {
    const rq = (r, o) => unit.request(r, o), info = parseInfo(await rq([1, []]));
    return { text: JSON.stringify(await captureBackup(rq, info)) };
  },
};

const rl = createInterface({ input: process.stdin, crlfDelay: Infinity });
for await (const line of rl) {
  if (!line.trim()) continue;
  let out;
  try {
    const m = JSON.parse(line);
    if (m.op === "frame") out = await onFrame(m.data);
    else if (m.op === "control" && CONTROL[m.action]) out = { ok: true, ...(await CONTROL[m.action](m.args ?? {})) };
    else out = { ok: false, error: `unknown request ${JSON.stringify(m.op)} / ${JSON.stringify(m.action)}` };
  } catch (e) { out = { ok: false, error: String(e?.message ?? e) }; }
  process.stdout.write(JSON.stringify(out) + "\n");
}
