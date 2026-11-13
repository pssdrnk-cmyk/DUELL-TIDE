// SPDX-License-Identifier: GPL-3.0-only
// The Backup page's logic (editor/duellbackup.js) against a simulated unit (editor/mockunit.js): INFO, the "duell-backup" file round trip,
// every refusal before a write, the restore order and what is reported, the safety backup, the verify, older firmware and older files.
import { OLD_PATCH_IDS, PATCH_ID, VERSION, GROUPS, DUELL_IDS, RESTORE_ORDER, patchKey, patchSlot, isPatchKey, objectLabel, parseInfo, parseList, parsePage, readArchive, objectsToFile, captureBackup, planRestore, strippedNote, runRestore,
         verifyRestore, describe, FVB_ID, FVB_OBJ, bankVoices, unpackVoice, packVoice, checkObject, listUnit, fileName, rawLines, MULTI_VERSION } from "./editor/duellbackup.js";
import { MockUnit as MockB12, buildBankObject, buildPatchRecord, buildLegacyPatchChunk, buildMultiObject, buildPatternObject, MOCK_ENGINES } from "./editor/mockunit.js";
import { buildRecord, buildEndB, stripEndBFmt, parseRecord, storeDefaults, engineIndex, PTF, XB_DEFS } from "./editor/patchrecord.js";
import { CMD } from "./editor/proto.js";
import { buildObject as wtObject, WT } from "./editor/wavetable.js";
import { readFileSync } from "node:fs";
import { BACKUP_CMD as BK } from "./fm1backup.js";
// The default mock is the b12 unit (175 slots, SLOT_HI, a store of 150, backup id 47). The tests of the unit of before run on the 128 slot one; the b12 tests (end of file) use MockB12.
const MockUnit = class extends MockB12 { constructor(o = {}) { super({ slots175: false, ...o }); } };
const IDS63 = DUELL_IDS.filter((id) => id < 42);   // what a unit of caps 63 lists (b10 U4: caps 127 adds the multis 42..46)
const R63 = RESTORE_ORDER.filter((id) => id < 42);
import { bkCrc, bkPack, BACKUP_IDS_FULL, BACKUP_IDS_WT, BACKUP_IDS_P16, bkManifest, bkU32, readBackup, captureBackup as installerCapture, restoreBackup as restoreInst, parseInfo as parseInfoInst } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(100)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const athrows = async (f, re) => { try { await f(); return false; } catch (e) { return re ? re.test(e.message) : true; } };
const throwsMsg = (f, re) => { try { f(); return false; } catch (e) { return re.test(e.message); } };
const all = new Set(GROUPS.map((g) => g.key));
const clone = (x) => JSON.parse(JSON.stringify(x));
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const PATCH_IDS_6 = OLD_PATCH_IDS.slice(0, 6), PATCH_IDS_X = OLD_PATCH_IDS.slice(6);   // the physical chunks of an older firmware (the installer's backup still carries them)
const nothing = new Uint8Array(0);
const patchRows = (u) => [...u.patches.keys()].sort((a, b) => a - b).map((k) => `${k}:${bkCrc(u.patchBytes(k))}`).join();   // the unit's saved patches: slot and CRC of the record
const bytesOf = (a) => new Map([...a.objects].map(([id, o]) => [id, o.bytes]));

// ---- INFO and the list
{
  const u = new MockUnit();
  const info = parseInfo(await u.request([1, []]));
  ok(info.version === "FELUCCA DUELL 0.2.25 bet" && info.engines.length === MOCK_ENGINES.length && info.caps === 63 && info.patchCap === 128, "INFO: version, engine names, backup caps 63 (bit 2: multis and patterns, bit 3: user wavetables, bit 4: 128 patches, bit 5: patch records), patch capacity 128");
  ok(parseInfo(await new MockUnit({ caps: 3 }).request([1, []])).caps === 3, "INFO: a unit without bit 2 reports caps 3");
  ok(parseInfo(await new MockUnit({ tide: true }).request([1, []])).tide === true && parseInfo(await u.request([1, []])).tide === false && parseInfoInst(await new MockUnit({ tide: true }).request([1, []])).tide === true,
     "INFO: tag 44 01 marks a TIDE unit (the editor's and the installer's readers); a plain DUELL unit sends none");
  const wire = await u.request([65, []]);
  ok(wire[0] === 1 && wire[1] === 0 && wire[2] === 24 && Array.from({ length: 24 }, (_, i) => wire[3 + 11 * i]).join() === "0,1,2,3,4,5,8,13,14,15,16,17,18,19,20,21,22,23,24,25,26,32,33,34", "LIST (caps 63): 24 ids, 0 1 2 3 4 5 8 13..26 32 33 34; the patch chunk ids 6 7 9..12 27..31 35..39 are not listed");
  const list = await listUnit((r, o) => u.request(r, o));
  ok(same([...list.keys()].sort((a, b) => a - b), IDS63) && list.get(14).size > 8 && list.get(2) === undefined, "LIST: the 16 DUELL objects other than patches (the retired ids 2..5 8 32..34 are left out)");
  ok(list.get(25).size === 1228 && !list.has(24) === false && list.get(24).size === 0, "LIST: the user wavetables 24..26: USR2 holds the 1228 byte object, USR1 and USR3 are empty (length 0)");
  const full = await listUnit((r, o) => u.request(r, o), 63);
  ok(same([...full.keys()].filter(isPatchKey), [1000, 1001, 1005, 1064]) && full.size === 20 && full.get(patchKey(0)).size === u.patchBytes(0).length && full.get(patchKey(0)).crc === bkCrc(u.patchBytes(0)), "LIST + pages: the four saved patches (slots 0 1 5 64) are merged into the same Map under their keys, with the record's size and CRC");
  const pg = await u.request([65, [1]]);
  ok(pg[0] === 1 && pg[1] === 0 && pg[2] === 3 && pg[3] === 0 && pg.length === 3 + 3 * 11 && (await u.request([65, [3]]))[2] === 1 && (await u.request([65, [2]]))[2] === 0, "LIST page 1: 3 patches (slot, size u32, crc u32), page 3 holds slot 64, page 2 none");
  ok((await u.request([65, [0]]))[1] === 1 && (await u.request([65, [5]]))[1] === 1, "LIST page 0 and 5: rc 1");
  const fresh = new MockUnit();
  ok((await fresh.request([65, [1]]))[1] === 5, "LIST page before the plain LIST: rc 5 (stale)");
  await fresh.request([65, []]); fresh.playing = true;
  ok((await fresh.request([65, [1]]))[1] === 3, "LIST page while playing: rc 3");
  const c7 = await listUnit((r, o) => new MockUnit({ caps: 7 }).request(r, o));
  ok(c7.size === 13 && !c7.has(24) && !c7.has(27), "LIST of a unit with caps 7 (0.2.14 before the wavetables): 13 objects, no wavetables");
  const old = await listUnit((r, o) => new MockUnit({ caps: 3 }).request(r, o));
  ok(old.size === 2 && !old.has(13) && !old.has(21), "LIST of a 0.2.13 unit (16 ids, six of them patch chunks an older firmware lists): music and settings only here, no multis, no patterns");
  const o31 = new MockUnit({ caps: 31 }), l31 = await listUnit((r, o) => o31.request(r, o), 31);
  ok(l31.size === 16 && [...l31.keys()].every((k) => !isPatchKey(k)) && (await o31.request([65, [1]]))[1] === 1, "LIST of a unit with caps 31 (no bit 5): the chunk ids it lists are ignored, no patches, a page answers rc 1");
  ok(throwsMsg(() => parseList([1, 3, 0]), /playing/), "LIST while playing (rc 3): says to stop playback");
  ok(throwsMsg(() => parseList([1, 0, 1, 99, ...bkU32(0), ...bkU32(0)]), /newer editor/), "LIST: an unknown object id is refused");
  const pgOk = [1, 0, 1, 40, ...bkU32(50), ...bkU32(7)];
  ok(parsePage(pgOk, 2).get(patchKey(40)).size === 50 && throwsMsg(() => parsePage(pgOk, 1), /page 1/) && throwsMsg(() => parsePage([1, 0, 1, 40, ...bkU32(19), ...bkU32(7)], 2), /size/) && throwsMsg(() => parsePage([1, 0, 1, 40, ...bkU32(581), ...bkU32(7)], 2), /size/)
     && throwsMsg(() => parsePage([1, 0, 2, 40, ...bkU32(50), ...bkU32(7), 40, ...bkU32(50), ...bkU32(7)], 2), /page 2/) && throwsMsg(() => parsePage(pgOk.slice(0, 10), 2), /incomplete/) && throwsMsg(() => parsePage([1, 5, 0], 1), /expired|Start again/),
     "LIST page: a slot off the page, out of order, a size outside 20..580, a short reply and rc 5 are refused");
}

// ---- checks of single objects
{
  const mu = buildMultiObject(["A", "B"]), pa = buildPatternObject(0, { 0: { name: "X" } }), pc = buildPatchRecord({ name: "P", engine: 0, cat: 2 });
  ok(checkObject(14, mu) === null && checkObject(21, pa) === null && checkObject(patchKey(3), pc) === null && checkObject(14, new Uint8Array(0)) === null, "objects: good multi, pattern, patch record and empty multi pass");
  const t = (id, b, f) => { const c = b.slice(); f(c); return checkObject(id, c) !== null; };
  ok(t(14, mu, (c) => { c[0] ^= 1; }) && t(14, mu, (c) => { c[4]++; }) && t(14, mu, (c) => { c[8] = 0; }) && t(14, mu, (c) => { c[8] = 16; }) && t(14, mu, (c) => { c[8 + 8] = 7; }) && t(14, mu, (c) => { c[9] = 3; }),
     "objects: a multi with a bad magic, length, version, name character or part count is refused");
  const music = new MockUnit({ seed: false }).objs.get(0);
  ok(checkObject(0, music) === null && t(0, music, (c) => { c[63] = 4; }) && t(0, music, (c) => { c[67] = 4; }) && t(0, music, (c) => { c[67] = 5; }) && t(0, music, (c) => { c[67] = 6; }) && t(0, music, (c) => { c[67] = 7; }) && t(0, music, (c) => { c[66] = 122; }) && t(0, music, (c) => { c[0] ^= 1; }) && checkObject(0, music.slice(0, 3000)) !== null
     && /older version/.test(checkObject(0, music.map((x, i) => i === 63 ? 4 : x))),
     "objects: the music of two parts and revision 8 passes; one of four parts (byte 63), revision 4, 5, 6 or 7 (byte 67), another P_COUNT, a bad magic or size is refused, the old one with a clear reason");
  const at = 8 + 11;   // (the second slot's record would be later; the first slot's record starts at 8: rlen 2, flags, LEN, DIV, SWING, GATE, nlen, name ...)
  ok(t(21, pa, (c) => { c[0] ^= 1; }) && t(21, pa, (c) => { c[8 + 3] = 0; }) && t(21, pa, (c) => { c[8 + 8] = 3; }) && t(21, pa, (c) => { c[8 + 2] = 4; }) &&
     t(21, pa, (c) => { c[6] = 10; }) && t(21, pa, (c) => { c[4]--; }) && t(21, pa, (c) => { c[8 + 6] = 0; }) && checkObject(21, pa.slice(0, 300)) !== null && checkObject(23, pa) !== null && checkObject(21, new Uint8Array(0)) === null,
     "objects: a pattern object with a bad magic, LEN, name, flag bits, slot count, length, GATE or size, or another object's slot count, is refused; length 0 (the defaults again) passes");
  ok(checkObject(21, (() => { const c = pa.slice(); c[3] = 0x32; return c; })()) === "a pattern slot object of an older firmware (16 slots)", "objects: a PTS2 object of the 16-slot firmware is refused with that reason");
  const v = storeDefaults(0); v[45] = 60; v[46] = 20;
  const pv = buildRecord({ engine: 0, name: "VALUES", values: v }), ex = buildRecord({ engine: 0, name: "XBLOCK", values: v, ext: XB_DEFS.map((d, k) => (k === 6 ? 10 : d - 64)), mb: buildEndB({ engine: 0, amount: 40, values: { 9: 5, 45: 70 }, marks: [9] }) });
  const k = patchKey(7);
  ok(checkObject(k, ex) === null && checkObject(k, pv) === null && (ex[4] & PTF.MB), "objects: a patch record with values, and one with an extension slot and an End B block, pass");
  ok(t(k, pc, (c) => { c[2] = 3; }) && t(k, pc, (c) => { c[3] = 1; }) && t(k, pc, (c) => { c[0]++; }) && t(k, pc, (c) => { c[4] &= ~PTF.CB; }) && t(k, pc, (c) => { c[5] = 99; }) && t(k, pc, (c) => { c[7] = 0; }) && t(k, pc, (c) => { c[8] = 7; }) && t(k, pc, (c) => { c[18] = 65; c[10] = 0; })
     && checkObject(k, pc.slice(0, 19)) !== null && checkObject(k, nothing) !== null && checkObject(k, new Uint8Array(581)) !== null,
     "objects: a patch record with a slot or revision byte not 0, a wrong length field, no CB flag, an unknown engine, no name, a control character or a gap in its name, a size under 20, 0 or over 580 is refused");
  ok(t(k, pv, (c) => { c[20 + 15] |= 128; }) && t(k, pv, (c) => { c[19]++; }) && t(k, pv, (c) => { c[4] |= 16; }) && t(k, pv, (c) => { c[4] |= 32; }) && t(k, ex, (c) => { c[4] &= ~PTF.MB; }) && t(k, ex, (c) => { c[c.length - 1] ^= 255; c[0]--; }),
     "objects: a patch record that names wide id 127 in its bitmap, has a wrong value count, carries a flag of the older form (16) or an AK flag without a block or has its End B flag cleared or its End B cut short is refused");
  ok(/older version/.test(checkObject(k, (() => { const c = pv.slice(); c[4] |= 16; return c; })())), "objects: a patch record of the older form (flag 16 or 64) is refused with a plain message");
  ok(OLD_PATCH_IDS.every((id) => checkObject(id, buildLegacyPatchChunk([{ slot: 3, name: "P" }])) !== null) && checkObject(40, pc) !== null, "objects: a physical patch chunk (ids 6 7 9..12 27..31 35..39) and the bare wire id 40 are no objects of a backup");
  ok(checkObject(0, new Uint8Array(3840)) !== null && checkObject(1, new Uint8Array(584)) !== null && checkObject(2, new Uint8Array(0)) !== null, "objects: music / settings without their magic, and a retired id, are refused");
  ok(objectLabel(patchKey(6), pc) === "patch #07 P" && objectLabel(patchKey(123)) === "patch #124" && objectLabel(patchKey(127)) === "patch U4" && objectLabel(patchKey(128)) === "patch #125" && objectLabel(patchKey(174)) === "patch #171" && objectLabel(6) === "a chunk of the old patch store" && patchSlot(patchKey(9)) === 9 && !isPatchKey(40) && isPatchKey(1127) && isPatchKey(1174) && !isPatchKey(1175), "labels: patch #NN NAME from the slot (1-based, as the Patches page) and the record");
}

// ---- the user wavetables (ids 24..26)
{
  const data = Uint8Array.from({ length: WT.BYTES }, (_, i) => 1 + (i * 7) % 255), w = wtObject("MY TABLE", data);
  const t = (f) => { const c = w.slice(); f(c); return checkObject(25, c) !== null; };
  ok(checkObject(24, w) === null && checkObject(26, new Uint8Array(0)) === null && w.length === 1228, "objects: a good wavetable object and an empty one pass");
  ok(t((c) => { c[0] = 2; }) && t((c) => { c[1] = 18; }) && t((c) => { c[2] = 63; }) && t((c) => { c[3] = 1; }) && t((c) => { c[6] = 7; }) && t((c) => { c.fill(0, 12); }) && checkObject(25, w.slice(0, 1227)) !== null,
     "objects: a wavetable with another version, frame count, harmonic count, flags, a control character in the name, all zero bytes, or a wrong size is refused");
  const d = describe(new Map([[24, new Uint8Array(0)], [25, w], [26, null]].filter(([, b]) => b)));
  ok(d.wavetables.length === 1 && d.wavetables[0].label === "USR2" && d.wavetables[0].name === "MY TABLE" && d.has.wavetables, "describe: USR2 MY TABLE listed, the empty slots not");
  ok(GROUPS.find((g) => g.key === "wavetables").caps === 8, "the wavetables group needs caps bit 3");
}

// ---- capture, file, round trip
const A = new MockUnit();
const info = parseInfo(await A.request([1, []]));
const seen = []; const rqA = (r, o) => A.request(r, o);
const file = await captureBackup(rqA, info, (d, t) => seen.push([d, t]));
const archive = readArchive(JSON.stringify(file));
ok(file.format === "duell-backup" && file.version === 7 && VERSION === 7 && file.slots175 === false && file.firmware === info.version && file.objects.length === 20 && file.patchRecords === true && !Number.isNaN(Date.parse(file.created)), "file: format duell-backup, version 7, firmware, date (slots175 false: a 128-slot unit), 20 objects (16 others and the 4 saved patches)");
ok(file.objects.slice(16).every((o, i) => o.id === 40 && o.slot === [0, 1, 5, 64][i] && o.size >= 20 && o.size <= 580) && file.objects.slice(0, 16).every((o) => o.id !== 40) && archive.hasPatches && archive.oldPatchChunks === 0 && !archive.notes.length, "file: the patches are {id: 40, slot, size, crc, data} objects, sorted by slot, after the other objects");
ok(seen.at(-1)[0] === seen.at(-1)[1] && seen.length > 10, "capture: progress ends at the total");
ok(IDS63.every((id) => same(archive.objects.get(id).bytes, A.objs.get(id) || new Uint8Array(0))) && [...A.patches.keys()].every((k) => same(archive.objects.get(patchKey(k)).bytes, A.patchBytes(k))) && archive.objects.size === 20,
   "round trip: capture -> JSON -> bytes equal the unit's objects, empty ones as 0, and each patch equals the record the unit gives out");
const d = describe(bytesOf(archive), archive.engines);
ok(d.patches.map((p) => `${p.slot}:${p.name}:${p.engineName}:${p.category}`).join() === "0:WARM PAD:ANALOG:PAD,1:SUB BASS:ANALOG:BASS,5:BELL:FM6:KEYS,64:WAVE LEAD:WAVE:LEAD", "preview: patch names, engines and categories by slot");
ok(d.multis.map((m) => `${m.label}:${m.name}`).join() === "M03:TESTONE,M04:TESTTWO,M12:LIVE SET", "preview: multi names (M03 M04 in object 14, M12 in object 18)");
ok(d.patterns.length === 32 && d.patterns[2].label === "P03" && d.patterns[2].name === "PAT THREE" && d.patterns[2].length === 12 && d.patterns[31].label === "P32" && d.patterns[31].name === "SEVEN EIGHT" && d.patterns[31].length === 14 && d.patterns[0].name === "DUB CHORD", "preview: 32 pattern slots by name and length (P03 PAT THREE 12, P32 SEVEN EIGHT 14)");
ok(await athrows(() => captureBackup((r, o) => { const x = new MockUnit(); x.playing = true; return x.request(r, o); }, info), /playing/), "capture while the unit plays: refused with a plain message");
{
  const u = new MockUnit(); let n = 0;
  const f2 = await captureBackup(async (r, o) => { const a = await u.request(r, o); if (r[0] === 66 && ++n === 3) u.objs.get(0)[3000] ^= 1; return a; }, info);
  const a2 = readArchive(JSON.stringify(f2));
  ok(f2.problems?.length === 1 && f2.problems[0].id === 0 && /changed/.test(f2.problems[0].problem) && a2.objects.get(0).raw && a2.objects.size === 20 && rawLines(a2.problems)[0].startsWith("Object 0 (the current music) could not be checked"),
     "capture (0.2.25): an object that changes during the read is kept as raw data with a warning line; the backup goes on");
}
{   // more than 32 patches: the four pages, every patch read in pieces of 256 (a record of 580 bytes needs three)
  const U = new MockUnit({ seed: false }), want = [];
  for (let slot = 0; slot < 128; slot += 3) {            // 43 patches over all four pages; a few with the extension block and a long FM6 record
    const eng = slot % 5 === 0 ? 14 : 0, more = slot % 9 === 0 ? { ext: XB_DEFS.map((d, k) => (k === slot % 15 ? d - 64 + 3 : d - 64)), mb: buildEndB({ engine: eng, amount: 50, values: Object.fromEntries(Array.from({ length: 30 }, (_, i) => i + 1).filter((w) => ![5, 21, 22, 23, 24, 29, 30, 31, 32].includes(w)).map((w) => [w, 3])) }) } : {};
    U.setRecord(slot, buildPatchRecord({ name: `PATCH ${slot}`, engine: eng, cat: slot % 9, values: storeDefaults(eng).map((v, i) => (i === 50 + slot % 20 ? v + 1 : v)), ...more })); want.push(slot);
  }
  U.rebuild();
  const rq = (r, o) => U.request(r, o), pagesSeen = [], gets = [];
  const f = await captureBackup(async (r, o) => { if (r[0] === 65 && r[1].length) pagesSeen.push(r[1][0]); if (r[0] === 66 && r[1][0] === 40) gets.push(r[1].length); return rq(r, o); }, info);
  const a = readArchive(JSON.stringify(f)), keys = [...a.objects.keys()].filter(isPatchKey);
  ok(want.length === 43 && same(pagesSeen, [1, 2, 3, 4]) && same(keys, want.map(patchKey)) && want.every((k) => same(a.objects.get(patchKey(k)).bytes, U.patchBytes(k))) && gets.every((n) => n === 9) && gets.length >= 43,
     "capture: 43 patches over the four LIST pages (1..4), each read with GET 40 (9 byte requests with the slot), every record equals the unit's");
  {   // a record of over 256 bytes (an FM6 sound with its patch, End B as values and 100 values off their default) is read in two pieces
    const fm = engineIndex("FM6"), vv = storeDefaults(fm).map((v, i) => (i >= 20 && i < 120 ? (v >= 100 ? v - 1 : v + 1) : v));
    U.setRecord(126, buildPatchRecord({ name: "BIGGEST", engine: fm, values: vv, patch128: Array.from({ length: 128 }, (_, i) => i), mb: buildEndB({ engine: fm, amount: 7, values: Object.fromEntries(Array.from({ length: 60 }, (_, i) => i + 6).filter((w) => ![21, 22, 23, 24, 29, 30, 31, 32, 40, 81, 82, 86, 94].includes(w)).slice(0, 40).map((w) => [w, 5])), marks: [6, 7], fm6: Array.from({ length: 65 }, (_, i) => i) }), ext: XB_DEFS.map((d) => d - 64 + 1) }));
    const g = []; const f2 = await captureBackup(async (r, o) => { if (r[0] === 66 && r[1][8] === 126) g.push(r[1][6] | r[1][7] << 7); return rq(r, o); }, info), a2 = readArchive(JSON.stringify(f2));
    ok(a2.objects.get(patchKey(126)).size > 256 && g.length === 2 && g[0] === 256 && same(a2.objects.get(patchKey(126)).bytes, U.patchBytes(126)) && parseRecord(a2.objects.get(patchKey(126)).bytes).ok, `capture: a ${a2.objects.get(patchKey(126)).size} byte record is read in two pieces (256 and the rest) and equals the unit's`);
    U.patches.delete(126);
  }
  const seen = []; await captureBackup(rq, info, (d, t) => seen.push([d, t]));
  const total = [...(await listUnit(rq, 63)).values()].reduce((n, o) => n + o.size, 0);
  ok(seen.at(-1)[0] === total && seen.at(-1)[1] === total, "capture: progress counts the bytes of the patches too and ends at the total");
}
{   // a patch that changes while it is read: kept raw with the problem noted, the rest saved
  const U = new MockUnit(); let n = 0;
  const f = await captureBackup(async (r, o) => {
    if (r[0] === 66 && r[1][0] === 40 && r[1][8] === 5 && ++n === 1) { const b = U.patchBytes(5); b[6] = (b[6] + 1) % 9; U.setRecord(5, b); }   // (the category changed on the unit's panel after the LIST)
    return U.request(r, o);
  }, info);
  const a = readArchive(JSON.stringify(f)), p5 = a.objects.get(patchKey(5));
  ok(f.problems?.length === 1 && f.problems[0].id === 40 && f.problems[0].slot === 5 && /changed/.test(f.problems[0].problem) && p5.raw && a.problems[0].id === patchKey(5) && !a.objects.get(patchKey(0)).raw
     && rawLines(a.problems, a.objects)[0].startsWith("Object 1005 (patch #06 ") && a.objects.size === 20,
     "capture: a patch that changes on the unit while it is read is kept as raw data with a warning line naming it; the other patches and objects are saved");
  const V = new MockUnit({ seed: false });
  const pl = planRestore(a, await listUnit((r, o) => V.request(r, o), 63), 63, new Set(["patches"])), pi = planRestore(a, await listUnit((r, o) => V.request(r, o), 63), 63, new Set(["patches"]), { includeRaw: true });
  ok(!pl.steps.some((s) => s.slot === 5) && pl.excluded.some((x) => x.id === patchKey(5) && /could not be checked/.test(x.reason)) && pi.steps.some((s) => s.slot === 5) && pl.steps.length === 3, "restore plan: the raw patch is skipped (and said so) unless the user asks for it");
}
{   // a unit without caps bit 5 gives no patches, and the result says so
  const U = new MockUnit({ caps: 31 }), inf = parseInfo(await U.request([1, []]));
  const f = await captureBackup((r, o) => U.request(r, o), inf), a = readArchive(JSON.stringify(f));
  ok(f.version === 7 && f.patchRecords === false && !f.objects.some((o) => o.id === 40) && a.hasPatches === false && /cannot back up patches one at a time/.test(f.notes?.[0] ?? "") && a.notes.length === 1 && a.objects.size === 16,
     "capture: a unit without caps bit 5 (an older firmware) gives no patches; the file says so (a note, patchRecords false) and holds the rest");
}
{   // 0.2.25, the owner's report: "The backup failed. Nothing was saved. Object 13: damaged multi." (firmware 0.2.23 with an editor that refused version 6)
  const { readFileSync } = await import("node:fs");
  const recs = ["M01", "M02"].map((n) => readFileSync(new URL(`../tools/factory_multis/${n}.bin`, import.meta.url)));
  const ob = new Uint8Array(8 + recs[0].length + recs[1].length); ob.set([0x4D, 0x55, 0x4C, 0x31, recs[0].length & 255, recs[0].length >> 8, recs[1].length & 255, recs[1].length >> 8]); ob.set(recs[0], 8); ob.set(recs[1], 8 + recs[0].length);
  ok(recs[0][0] === MULTI_VERSION && MULTI_VERSION === 8 && checkObject(13, ob) === null, "objects: a multi object as the firmware writes it (version 8 now, End B as values, 127 values: the factory records M01, M02) passes");
  const oldCheck = (b) => { const at = 8; return b[at] > 2 ? "damaged multi" : null; };   // the 0.2.23 editor's rule (MUL_VER 2, b[at] > MUL_VER refused)
  ok(oldCheck(ob) === "damaged multi", "the 0.2.23 editor's multi check refused every version 6 record: that was the owner's 'Object 13: damaged multi'");
  const u = new MockUnit(); u.objs.set(13, ob);
  const g = await captureBackup((r, o) => u.request(r, o), info), ga = readArchive(JSON.stringify(g));
  ok(!g.problems && same(ga.objects.get(13).bytes, ob) && describe(bytesOf(ga)).multis.slice(0, 2).map((m) => m.name).join() === "DUB BASIC,DUB CHORD", "capture: a unit holding 0.2.23 / 0.2.24 multis in object 13 backs up without a warning, names listed");
  const bad = ob.slice(); bad[8 + 8] = 7;                               // a control character in M01's name: the editor cannot check it
  u.objs.set(13, bad);
  const gb = await captureBackup((r, o) => u.request(r, o), info), gba = readArchive(JSON.stringify(gb));
  ok(gb.problems?.length === 1 && gb.problems[0].id === 13 && gba.objects.size === 20 && same(gba.objects.get(13).bytes, bad) && same(gba.objects.get(patchKey(0)).bytes, u.patchBytes(0)) && /multis M01 and M02/.test(rawLines(gba.problems)[0]),
     "capture: a damaged multi object does not stop the backup: everything else is saved, object 13 as raw data with a warning naming M01 and M02");
  const T = new MockUnit({ seed: false }), lt = await listUnit((r, o) => T.request(r, o));
  const pr = planRestore(gba, lt, 63, all), pi = planRestore(gba, lt, 63, all, { includeRaw: true });
  ok(!pr.steps.some((s) => s.id === 13) && pr.steps.some((s) => s.id === 14) && pr.excluded.some((x) => x.id === 13 && /could not be checked/.test(x.reason)) && pi.steps.some((s) => s.id === 13),
     "restore plan: the raw object is skipped (and said so) unless the user asks for it; the other multis restore");
  const v5 = ob.slice(); v5[8] = 5; const f5 = objectsToFile([...ga.objects.values()].map((o) => ({ id: o.id, bytes: o.id === 13 ? v5 : o.bytes })), info);
  const p5 = planRestore(readArchive(JSON.stringify(f5)), lt, 63, new Set(["multis"]));
  ok(!p5.steps.some((s) => s.id === 13) && p5.excluded.some((x) => x.id === 13 && /version 5/.test(x.reason)), "restore plan: a multi record of another version (5) is left out with the reason (the unit would refuse it)");
}

// ---- reading files: every refusal comes before any write
{
  const bad = (f) => { const c = clone(file); f(c); return c; };
  ok(throwsMsg(() => readArchive("{ nope"), /JSON/), "file: not JSON");
  ok(throwsMsg(() => readArchive({ format: "something", objects: [] }), /not a TIDE/), "file: another format");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 8; })), /newer editor/), "file: a newer version is refused with a plain message");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 1; })), /unknown version/), "file: version 1 of the duell-backup format does not exist");
  ok(throwsMsg(() => readArchive(bad((c) => { const o = c.objects.find((x) => x.id === 14); o.crc = (o.crc ^ 1) >>> 0; })), /checksum/), "file: a damaged object (CRC) is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { const o = c.objects.find((x) => x.id === 14); o.data = o.data.slice(0, -4); })), /damaged/), "file: a truncated object is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects.push(c.objects[2]); })), /twice/), "file: an object listed twice is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects[2].id = 99; })), /Unknown object/), "file: an unknown id is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects = c.objects.filter((o) => o.id !== 17); })), /incomplete/), "file: a group with a missing object (multis without id 17) is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects = c.objects.filter((o) => o.id !== 0); })), /no current music/), "file: a file without the current music is refused");
  const evil = clone(file); const m = evil.objects.find((o) => o.id === 14), raw = Uint8Array.from(atob(m.data), (c) => c.charCodeAt(0)); raw[0] ^= 1;
  m.data = btoa(String.fromCharCode(...raw)); m.crc = bkCrc(raw);
  ok(throwsMsg(() => readArchive(evil), /Object 14: not a multi object/), "file: a good CRC over a bad multi object is still refused (content check)");
  const retired = clone(file); retired.objects.push({ id: 2, size: 4, crc: 1, data: "AAAAAA==" });
  ok(throwsMsg(() => readArchive(retired), /retired|checksum/), "file: data in a retired object is refused");
  const fel = { format: "felucca-backup", version: 1, firmware: "FELUCCA 1.6.1", created: "x", objects: [] };
  ok(throwsMsg(() => readArchive(fel), /Felucca backup/), "file: a Felucca backup is refused (other id meanings)");
}

// ---- restore
const rows = (u) => IDS63.map((id) => `${id}:${u.objs.has(id) ? bkCrc(u.objs.get(id)) : 0}`).join() + "|" + patchRows(u);
const KEYS = [1000, 1001, 1005, 1064];
{
  const B = new MockUnit({ seed: false });                       // a unit that lost its data
  const rq = (r, o) => B.request(r, o);
  const list = await listUnit(rq, 63);
  const plan = planRestore(archive, list, 63, all);
  ok(same(plan.steps.map((s) => s.key ?? s.id), [...KEYS, ...R63]) && plan.excluded.length === 0 && plan.skipped.length === 0 && plan.steps.slice(0, 4).every((s) => s.id === 40 && s.slot === patchSlot(s.key) && s.size >= 20 && s.group === "patches"),
     "plan: all groups, order patches (by slot, id 40 with the slot) -> multis -> patterns -> wavetables -> settings -> current music last");
  const wire = []; const prog = [];
  const r = await runRestore(async (q, o) => { if (q[0] === 67) wire.push(q[1].slice(0, 2).join() + (q[1][0] === 0 && q[1][1] === 40 ? ":" + q[1][12] + ":" + q[1].length : "")); return rq(q, o); }, plan, (dn, t) => prog.push(dn / t));
  ok(r.failed === null && r.written.length === 20 && prog.at(-1) === 1, "restore: all 20 objects written (4 patches, 16 others), progress reaches 100 %");
  ok(wire.filter((w) => w.startsWith("0,40")).join() === "0,40:0:13,0,40:1:13,0,40:5:13,0,40:64:13" && wire.indexOf("2,40") < wire.indexOf("0,13") && wire.filter((w) => w === "2,40").length === 4,
     "restore: each patch is begin (13 bytes: 0, 40, size, crc, slot) / data / commit [2, 40]; the slot byte is sent at begin; patches before the multis");
  const v = await verifyRestore(rq, plan);
  ok(v.differ.length === 0 && v.same.length === 18 && same(v.same.slice(0, 4), KEYS) && same(v.notCompared, [1, 0]), "verify: patches, multis, patterns equal the file (size, CRC, from a fresh list with the pages); settings and music accepted by the unit's check");
  ok(IDS63.every((id) => B.objs.has(id) === (A.objs.get(id)?.length > 0)) && IDS63.every((id) => (B.objs.get(id) || nothing).length === (A.objs.get(id) || nothing).length) && patchRows(B) === patchRows(A), "restore: the unit now holds exactly the backed-up objects and patch records");
  const w = B.log.filter((x) => x.startsWith("write ")).map((x) => (x.startsWith("write patch") ? 1000 + +x.slice(12) : +x.slice(6)));
  ok(same(w.slice(0, 4), KEYS) && w.at(-1) === 0 && w.at(-2) === 1 && w.indexOf(21) < w.indexOf(1) && w.indexOf(14) < w.indexOf(21) && w.indexOf(1064) < w.indexOf(13), "restore: the writes the unit made are in that order");
  const again = planRestore(archive, await listUnit(rq, 63), 63, new Set(["patches"]));
  ok(again.steps.length === 0 && again.skipped.join() === KEYS.join(), "plan: patches the unit already holds (same size and CRC) are skipped, not written again");
}
{   // a restore over a unit with other data: the objects the file holds empty become empty (an erased multi stays erased); extra patches on the unit are removed
  const C = new MockUnit(); C.objs.set(15, buildMultiObject(["OLD ONE", null])); C.objs.set(22, buildPatternObject(1, { 0: { name: "OLDPAT" } }));
  C.setRecord(9, buildPatchRecord({ name: "MINE ONLY" })); C.setRecord(100, buildPatchRecord({ name: "ALSO MINE" })); C.setRecord(5, buildPatchRecord({ name: "OTHER BELL", engine: 3 })); C.rebuild();
  const rq = (r, o) => C.request(r, o);
  const plan = planRestore(archive, await listUnit(rq, 63), 63, all);
  const puts = plan.steps.filter((s) => s.key !== undefined);
  ok(same(puts.map((s) => s.slot), [9, 100, 5]) && same(puts.map((s) => s.size > 0), [false, false, true]) && plan.skipped.join() === "1000,1001,1064" && puts.filter((s) => !s.size).every((s) => s.id === 40 && s.crc === 0 && s.bytes.length === 0),
     "plan: the removals first (D05: slots 9 and 100, a PUT of length 0 for each slot the unit has saved and the file has not), then the one PUT for the patch that differs (slot 5); the identical ones (0, 1, 64) are skipped");
  const r = await runRestore(rq, plan);
  const v = await verifyRestore(rq, plan);
  ok(r.failed === null && !C.objs.has(15) && bkCrc(C.objs.get(22)) === bkCrc(A.objs.get(22)) && v.differ.length === 0 && patchRows(C) === patchRows(A) && !C.patches.has(9) && !C.patches.has(100) && C.log.includes("remove patch 9") && C.log.includes("remove patch 100"),
     "restore: multi objects empty in the file are emptied on the unit (size 0 PUT), the pattern object (never empty) is the file's, patches the file lacks are removed, the unit's saved patches end up exactly the file's, verify equal");
  const none = readArchive(JSON.stringify(objectsToFile([...archive.objects.values()].filter((o) => !isPatchKey(o.id)), info, { patchRecords: true })));
  const pn = planRestore(none, await listUnit(rq, 63), 63, new Set(["patches"]));
  ok(none.hasPatches && pn.steps.length === 4 && pn.steps.every((s) => !s.size), "plan: a file of a unit with no saved patch (patchRecords true) removes every patch of the unit");
}
{   // group selection and honest exclusions
  const D = new MockUnit({ seed: false });
  const rq = (r, o) => D.request(r, o);
  const list = await listUnit(rq, 63);
  const p1 = planRestore(archive, list, 63, new Set(["patches"]));
  ok(same(p1.steps.map((s) => s.key), KEYS), "plan: only the chosen group (patches) is written");
  const p2 = planRestore(archive, list, 3, all);
  ok(p2.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && p2.steps.every((s) => s.id < 13) && p2.excluded[0].reason === "this unit's firmware cannot restore patches one at a time (update it first)" && /update/.test(p2.excluded[1].reason),
     "plan: a unit without caps bits 2, 3 and 5: patches (\"cannot restore patches one at a time (update it first)\"), multis, patterns and wavetables left out, with the reason");
  const p2c = planRestore(archive, list, 31, all);
  ok(p2c.excluded.map((e) => e.key).join() === "patches" && p2c.steps.every((s) => s.key === undefined) && p2c.steps.length === 16, "plan: a unit with caps 31 (everything but bit 5): only the patches are left out");
  const old6 = JSON.parse(JSON.stringify({ ...file, version: 3, patchRecords: undefined, objects: file.objects.filter((o) => o.id !== 40).concat(PATCH_IDS_6.map((id) => ({ id, size: 0, crc: 0, data: "" }))) }));
  const o6 = readArchive(JSON.stringify(old6));
  ok(o6.version === 3 && !o6.hasPatches && o6.oldPatchChunks === 0 && planRestore(o6, list, 63, new Set(["patches"])).excluded[0].reason === "not in this file", "file: a version 3 file with empty chunks has no patches (not in this file)");
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...old6, objects: [...old6.objects, { id: 27, size: 0, crc: 0, data: "" }] })), /version 3 file does not have|damaged/), "file: a version 3 file that holds the ten more chunks is refused");
  const p2b = planRestore(archive, list, 55, all);
  ok(p2b.excluded.map((e) => e.key).join() === "wavetables" && p2b.steps.every((s) => s.id < 24 || s.id > 26) && /update/.test(p2b.excluded[0].reason), "plan: a unit with caps 55 (no wavetables): only the wavetables left out");
  const p3 = planRestore(archive, list, null, all);
  ok(p3.excluded.length === 4, "plan: a unit that reported no caps is treated as not able");
  const odd = readArchive(file); odd.objects.get(1).size = 600;
  ok(planRestore(odd, list, 63, new Set(["settings"])).excluded[0]?.reason.includes("another version"), "plan: a settings object of another size than the unit's is left out");
  ok(GROUPS[0].key === "patches" && GROUPS[0].dynamic && GROUPS[0].caps === 32 && GROUPS.slice(1).every((g) => !g.dynamic && g.ids.length > 0), "groups: patches is dynamic (caps bit 5), the others keep their ids");
}
{   // errors while writing: reported, the object aborted, nothing after it written, what was written is named
  const E = new MockUnit({ seed: false }); E.fail = { id: 14, op: 1, rc: 4 };
  const rq = (r, o) => E.request(r, o);
  const plan = planRestore(archive, await listUnit(rq, 63), 63, all);
  const r = await runRestore(rq, plan);
  ok(r.failed?.id === 14 && r.failures.length === 1 && /flash/.test(r.failed.message) && r.written.join() === "1000,1001,1005,1064,13,15,16,17,18,19,20,21,22,23,24,25,26,1,0" && !E.objs.has(14) && E.objs.has(0) && E.staged === null, "restore: a flash error in object 14 is reported (D05) and every other object is still written, the settings and the music included");
  const P = new MockUnit({ seed: false }); P.fail = { id: 40, op: 2, rc: 4 };
  const rp = await runRestore((r2, o) => P.request(r2, o), planRestore(archive, await listUnit((r2, o) => P.request(r2, o), 63), 63, all));
  ok(rp.failed?.id === 1000 && rp.failed.rc === 4 && rp.failures.map((f) => f.id).join() === "1000,1001,1005,1064" && rp.written.length === 16 && P.patches.size === 0 && P.staged === null && /patch #01/.test(objectLabel(rp.failed.id)), "restore: a flash error at the commit of every patch (rc 4) reports all four by their keys (D05: not just the first) and writes the other groups");
  const F = new MockUnit({ seed: false });
  const rq2 = (r, o) => F.request(r, o);
  const plan2 = planRestore(archive, await listUnit(rq2, 63), 63, all);
  F.playing = true;
  const r2 = await runRestore(rq2, plan2);
  ok(r2.failed?.rc === 3 && r2.stopped && r2.failures.length === 1 && /Stop playback/.test(r2.failed.message) && r2.written.length === 0 && F.objs.size === 5 && F.patches.size === 0, "restore while the unit plays (rc 3): nothing written, the message says stop playback");
  const Z = new MockUnit({ seed: false }), rz = (r, o) => Z.request(r, o);
  let nz = 0;
  const rt = await runRestore(async (q, o) => { if (q[0] === 67 && ++nz === 5) throw new Error("timed out"); return rz(q, o); }, planRestore(archive, await listUnit(rz, 63), 63, all));
  ok(rt.stopped && rt.failures.length === 1 && rt.failed.rc === undefined && /timed out/.test(rt.failed.message) && rt.written.length < 20, "restore: no answer from the unit (a timeout) stops the run (nothing more can be written); the failure is reported");
  const G = new MockUnit({ seed: false, caps: 3 });                // the page would not offer it; if the unit refuses at begin it is reported, never skipped
  const plan3 = planRestore(archive, await listUnit((r, o) => G.request(r, o)), 7, new Set(["multis"]));
  const r3 = await runRestore((r, o) => G.request(r, o), plan3);
  ok(r3.failed?.id === 13 && r3.failed.rc === 1 && r3.failures.length === 8 && r3.written.length === 0, "restore: an object the unit refuses at begin (rc 1) is reported as failed, not skipped");
  const Q = new MockUnit({ seed: false, caps: 31 }), pq = planRestore(archive, await listUnit((r, o) => Q.request(r, o)), 63, new Set(["patches"]));
  const rq3 = await runRestore((r, o) => Q.request(r, o), pq);
  ok(rq3.failed?.id === 1000 && rq3.failed.rc === 1 && rq3.failures.length === 4 && Q.patches.size === 0, "restore: a patch PUT sent to a unit without caps bit 5 is refused at begin (rc 1) and reported");
  const H = new MockUnit({ seed: false }); H.fail = { id: 21, op: 2, rc: 2 };
  const plan4 = planRestore(archive, await listUnit((r, o) => H.request(r, o), 63), 63, all);
  const r4 = await runRestore((r, o) => H.request(r, o), plan4);
  ok(r4.failed?.id === 21 && r4.failures.length === 1 && r4.written.includes(20) && r4.written.includes(22) && r4.written.includes(1) && r4.written.includes(0), "restore: the unit refusing a commit (rc 2) is reported and the settings and the music are still restored (D05)");
}
{   // D04 / D05: a nearly full store (TIDE holds 126 records; the mock holds 4 here): STORE FULL is told as such, the removals go first, every failure is reported
  const S = new MockUnit({ seed: false, storeMax: 4 }), rq = (r, o) => S.request(r, o);
  for (const k of [7, 8, 9, 10]) S.setRecord(k, buildPatchRecord({ name: `OLD ${k}` }));
  S.rebuild();
  const info4 = parseInfo(await S.request([1, []]));
  ok(info4.patchCap === 4, "mock: INFO 58 01 says the records the store takes (4 here, 126 on TIDE)");
  const listS = await listUnit(rq, 63), planS = planRestore(archive, listS, 63, new Set(["patches"]), { patchCap: 4 });
  ok(planS.steps.slice(0, 4).every((s) => !s.size) && planS.steps.slice(4).every((s) => s.size) && planS.steps.length === 8, "plan: the removals of the unit's four saved patches come BEFORE the four patches that are written (a full store has the room then)");
  const runS = await runRestore(rq, planS);
  ok(runS.failed === null && runS.failures.length === 0 && S.patches.size === 4 && [...S.patches.keys()].sort((x, y) => x - y).join() === "0,1,5,64", "restore into a full store: removals first, then all four patches take (no rc 6)");
  // a file with more patches than the unit takes: left out with the reason, the other groups still restored
  const T = new MockUnit({ seed: false, storeMax: 3 }), rt = (r, o) => T.request(r, o);
  const planT = planRestore(archive, await listUnit(rt, 63), 63, all, { patchCap: 3 });
  ok(planT.excluded.some((x) => x.key === "patches" && /4 patches and this unit stores at most 3/.test(x.reason)) && planT.steps.every((s) => s.key === undefined) && planT.steps.length === 16, "plan: a file with more patches than the unit stores is left out with the reason (STORE FULL), the 16 other objects stay in the plan");
  // the same without the pre-check (an older editor flow): the unit answers rc 6, it is reported by name with the STORE FULL text, and the other groups are still written
  const U2 = new MockUnit({ seed: false, storeMax: 2 }), ru = (r, o) => U2.request(r, o);
  const runU = await runRestore(ru, planRestore(archive, await listUnit(ru, 63), 63, all));
  ok(runU.failures.length === 2 && runU.failures.every((f) => f.rc === 6 && /STORE FULL/.test(f.message)) && runU.failures.map((f) => f.id).join() === "1005,1064" && runU.written.length === 18 && U2.patches.size === 2 && U2.objs.has(0) && U2.objs.has(1),
     "restore: a store that is full answers rc 6 for the patches that do not fit; both are reported by key with 'STORE FULL', the two that fit and every other group are written");
}
{   // the unit's own rules for PUT 40, over the wire
  const U = new MockUnit({ seed: false }), rq = (r, o) => U.request(r, o), rec = buildPatchRecord({ name: "WIRE" }), crc = bkCrc(rec);
  const begin = (size, c, slot, tail = []) => rq([67, [0, 40, ...bkU32(size), ...bkU32(c), slot, ...tail]]);
  ok((await begin(19, 0, 3))[2] === 1 && (await begin(581, 0, 3))[2] === 1 && (await begin(20, 0, 128))[2] === 1 && (await begin(20, 0, 3, [0]))[2] === 1 && (await rq([67, [0, 40, ...bkU32(20), ...bkU32(0)]]))[2] === 1 && (await begin(rec.length, crc, 3))[2] === 0,
     "PUT 40 begin: a size 1..19 or over 580, a slot over 127 and a request of 12 or 14 bytes answer rc 1; size 20..580 with a slot is accepted");
  await rq([67, [3, 40]]);
  const piece = async (off, n) => rq([67, [1, 40, ...bkU32(off), ...bkPack(rec.subarray(off, off + n))]]);
  await begin(rec.length, crc ^ 1, 3); await piece(0, rec.length);
  ok((await rq([67, [2, 40]]))[2] === 2 && U.patches.size === 0, "PUT 40: a wrong CRC at commit is rc 2, nothing written");
  const junk = rec.slice(); junk[4] &= ~PTF.CB; await begin(junk.length, bkCrc(junk), 3); await rq([67, [1, 40, ...bkU32(0), ...bkPack(junk)]]);
  ok((await rq([67, [2, 40]]))[2] === 2 && U.patches.size === 0, "PUT 40: a record the store would not read (no CB flag) is rc 2 at commit, nothing written");
  await begin(rec.length, crc, 77); await piece(0, rec.length);
  ok((await rq([67, [2, 40]]))[2] === 0 && U.patches.has(77) && same(U.patchBytes(77), rec) && (await rq([67, [2, 40]]))[2] === 5, "PUT 40: a good record goes into the slot of the begin (no slot in its bytes), a second commit is stale (rc 5)");
  U.playing = true;
  ok((await begin(0, 0, 77))[2] === 3, "PUT 40 while playing: rc 3");
  U.playing = false;
  await begin(0, 0, 77);
  ok((await rq([67, [2, 40]]))[2] === 0 && !U.patches.has(77), "PUT 40 of size 0 removes the slot's record (its factory sound is back)");
  await rq([65, []]);
  const get = (slot, off, n, extra = []) => rq([66, [40, ...bkU32(off), n & 127, n >> 7, slot, ...extra]]);
  U.setRecord(4, rec);
  await rq([65, []]);
  const g1 = await get(4, 0, rec.length);
  ok(g1[0] === 40 && g1[1] === 0 && g1.length === 9 + Math.ceil(rec.length / 7) * 8 - (7 - rec.length % 7) % 7 && (await get(6, 0, 10))[1] === 1 && (await get(200, 0, 10))[1] === 1 && (await get(4, 0, 0))[1] === 1 && (await get(4, 5, rec.length))[1] === 1
     && (await get(4, 0, 10, [0]))[1] === 1 && (await rq([66, [40, ...bkU32(0), 10, 0]]))[1] === 1 && (await get(4, 0, 257))[1] === 1,
     "GET 40: a saved slot answers (the reply has no slot); a slot with no record, a slot over 127, count 0, a count past the end, 10 bytes and 8 bytes requests answer rc 1");
  ok((await rq([66, [6, ...bkU32(0), 10, 0]]))[1] === 1 && (await rq([66, [27, ...bkU32(0), 10, 0]]))[1] === 1 && (await rq([67, [0, 6, ...bkU32(0), ...bkU32(0)]]))[2] === 1 && (await rq([67, [0, 39, ...bkU32(0), ...bkU32(0)]]))[2] === 1,
     "GET and PUT of the old patch chunk ids (6, 27 ...) answer rc 1 on a unit with caps bit 5");
}
{   // verify notices a difference
  const I = new MockUnit({ seed: false });
  const rq = (r, o) => I.request(r, o);
  const plan = planRestore(archive, await listUnit(rq, 63), 63, all);
  await runRestore(rq, plan);
  I.setRecord(5, buildPatchRecord({ name: "SOMEONE ELSE" })); I.rebuild();
  I.objs.get(14)[10] ^= 1;
  ok((await verifyRestore(rq, plan)).differ.join() === "1005,14", "verify: an object and a patch that differ from the file are named");
  const rm = planRestore(readArchive(JSON.stringify(objectsToFile([...archive.objects.values()].filter((o) => !isPatchKey(o.id)), info, { patchRecords: true }))), await listUnit(rq, 63), 63, new Set(["patches"]));
  await runRestore(rq, rm);
  I.setRecord(9, buildPatchRecord({ name: "BACK AGAIN" })); I.rebuild();
  ok((await verifyRestore(rq, rm)).differ.join() === "" && (await verifyRestore(rq, { ...rm, steps: [{ ...rm.steps[0], slot: 9, key: 1009 }] })).differ.join() === "1009", "verify: a removed patch is checked to be gone");
}
{   // the safety backup: a capture of the unit as it is, readable and restorable
  const J = new MockUnit();
  J.setRecord(9, buildPatchRecord({ name: "MINE ONLY" })); J.rebuild(); J.objs.delete(14);   // (a unit with other data than the file)
  const safety = await captureBackup((r, o) => J.request(r, o), info);
  const before = rows(J);
  await runRestore((r, o) => J.request(r, o), planRestore(archive, await listUnit((r, o) => J.request(r, o), 63), 63, all));
  ok(rows(J) !== before && !J.patches.has(9), "safety: the restore changed the unit");
  await runRestore((r, o) => J.request(r, o), planRestore(readArchive(safety), await listUnit((r, o) => J.request(r, o), 63), 63, all));
  ok(rows(J).split(",").slice(2).join() === before.split(",").slice(2).join() && J.patches.has(9), "safety: restoring the safety backup brings back patches, multis and patterns exactly");
}
{   // the whole path of a record with extension slots and End B as values: build -> PATCH_RAW write -> read back -> backup -> restore onto a fresh unit -> bytes equal
  const { rawWrite, rawRead } = await import("./editor/librarian.js");
  const { Unit } = await import("./editor/unit.js"), { MockLink } = await import("./editor/mockunit.js");
  const { L: LL } = await import("./editor/layout.js"), ext = XB_DEFS.map((d, k) => Math.max(LL.XTP[k].min, Math.min(LL.XTP[k].max, d - 64 + 2))), endB = Object.fromEntries(Array.from({ length: 32 }, (_, i) => [i < 26 ? 6 + i * 4 : 128 + (i - 26), i * 2 - 30]).filter(([w]) => ![5, 21, 22, 23, 24, 29, 30, 31, 32, 40, 81, 82, 86, 94, 105, 111, 112, 115, 116, 117, 118].includes(w)));   // (morph.c mbv_never ids dropped)
  const rec = buildRecord({ engine: 0, name: "XB + END B", cat: 4, values: storeDefaults(0).map((v, i) => (i === 60 ? v + 2 : v)), mb: buildEndB({ engine: 0, amount: 25, values: endB }), ext });
  ok((rec[4] & PTF.MB) && parseRecord(rec).ok && parseRecord(rec).mb.length > 22 && parseRecord(rec).xv.every((x, k) => x === ext[k]), "xblock path: the record has the extension slots and the End B block");
  const src = new MockUnit({ seed: false }), u = new Unit(new MockLink(src)); await u.connect();
  await rawWrite(u, 9, rec);
  const back = await rawRead(u, 9);
  ok(same(back, rec) && same(src.patchBytes(9), rec), "xblock path: PATCH_RAW write on the unit and read back: the bytes are equal");
  const f = await captureBackup((r, o) => src.request(r, o), info), a = readArchive(JSON.stringify(f));
  ok(same(a.objects.get(patchKey(9)).bytes, rec) && a.objects.size === 17 && f.objects.some((o) => o.id === 40 && o.slot === 9 && o.size === rec.length), "xblock path: the backup holds the record bytes under {id: 40, slot: 9}");
  const dst = new MockUnit({ seed: false }), rd = (r, o) => dst.request(r, o);
  const run = await runRestore(rd, planRestore(a, await listUnit(rd, 63), 63, all));
  const v = await verifyRestore(rd, planRestore(a, await listUnit(rd, 63), 63, all));
  ok(run.failed === null && same(dst.patchBytes(9), rec) && v.differ.length === 0 && parseRecord(dst.patchBytes(9)).mb.length === parseRecord(rec).mb.length, "xblock path: restored onto a fresh unit the bytes are equal, End B and all, verify equal");
  const dst2 = new MockUnit({ seed: false }), u2 = new Unit(new MockLink(dst2)); await u2.connect();
  ok(same(await rawRead(u2, 9) ?? new Uint8Array(0), new Uint8Array(0)), "xblock path: (the fresh unit held nothing there before)");
  const mid = new MockUnit({ seed: false }), rm = (r, o) => mid.request(r, o);
  await runRestore(rm, planRestore(a, await listUnit(rm, 63), 63, all));
  const u3 = new Unit(new MockLink(mid)); await u3.connect();
  ok(same(await rawRead(u3, 9), rec), "xblock path: the restored record reads back through PATCH_RAW too");
  const damaged = rec.slice(); damaged[19]++;
  ok(await athrows(() => rawWrite(u, 10, damaged)) && !src.patches.has(10), "xblock path: PATCH_RAW refuses a record whose value count is off by one at commit (nothing written)");
  ok(checkObject(patchKey(9), damaged) !== null && throwsMsg(() => readArchive(JSON.stringify(objectsToFile([...a.objects.values()].map((o) => (o.id === patchKey(9) ? { id: o.id, bytes: damaged } : o)), info))), /Patch #10/), "xblock path: a record whose value count is off by one is refused when a file is read (Patch #10)");
}

// ---- older firmware and older files
{
  const K = new MockUnit({ caps: 3 });                           // 0.2.13: 16 ids (six of them patch chunks), no multis / patterns
  const f13 = await captureBackup((r, o) => K.request(r, o), parseInfo(await K.request([1, []])));
  const a13 = readArchive(f13);
  ok(f13.objects.length === 2 && f13.version === 7 && !a13.objects.has(13) && !a13.hasPatches && !describe(bytesOf(a13)).has.multis && !describe(bytesOf(a13)).has.patches && /cannot back up patches/.test(a13.notes[0]),
     "0.2.13 unit: the backup holds settings and music only (no patches: it cannot give them one at a time, and the file says so), and the preview says no multis");
  const L = new MockUnit();
  L.setRecord(9, buildPatchRecord({ name: "NEWER ONE" })); L.rebuild();
  const m14 = bkCrc(L.objs.get(14)), p21 = bkCrc(L.objs.get(21)), pr = patchRows(L);
  const plan = planRestore(a13, await listUnit((r2, o) => L.request(r2, o), 63), 63, all);
  const r = await runRestore((r2, o) => L.request(r2, o), plan);
  ok(r.failed === null && plan.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && plan.excluded.every((e) => e.reason === "not in this file") && same(r.written, [1, 0]) && bkCrc(L.objs.get(14)) === m14 && bkCrc(L.objs.get(21)) === p21 && patchRows(L) === pr,
     "a 0.2.13 file onto a new unit: settings and music restored; patches, multis and patterns left as they are (a file without patch records never empties the unit's patches)");
}
{   // a version 1 .. 4 file with physical patch chunks: read, the chunks kept out, the rest restorable
  const chunks = [buildLegacyPatchChunk([{ slot: 0, name: "OLD ONE" }, { slot: 1, name: "OLD TWO" }]), buildLegacyPatchChunk([{ slot: 9, name: "OLD NINE" }])];
  const mk = (version, ids) => ({ ...clone(file), version, patchRecords: undefined, objects: file.objects.filter((o) => o.id !== 40).concat(ids.map((id, i) => {
    const b = i < 2 ? chunks[i] : nothing; return { id, size: b.length, crc: b.length ? bkCrc(b) : 0, data: btoa(String.fromCharCode(...b)) }; })) });
  for (const [version, ids] of [[4, OLD_PATCH_IDS], [3, PATCH_IDS_6]]) {
    const ar = readArchive(JSON.stringify(mk(version, ids)));
    ok(ar.version === version && !ar.hasPatches && ar.oldPatchChunks === 2 && ![...ar.objects.keys()].some((k) => isPatchKey(k) || OLD_PATCH_IDS.includes(k)) && ar.objects.size === 16 && /older firmware \(physical chunks\) are in this file but cannot be restored/.test(ar.notes[0]) && describe(bytesOf(ar), [], ar.oldPatchChunks).notes.length === 1 && describe(bytesOf(ar)).patches.length === 0,
       `v${version} file with physical patch chunks: read; its patches are kept out; the description says they cannot be restored`);
    const U = new MockUnit({ seed: false }), rq = (r, o) => U.request(r, o);
    const pl = planRestore(ar, await listUnit(rq, 63), 63, all);
    ok(pl.excluded.length === 1 && pl.excluded[0].key === "patches" && /older firmware \(physical chunks\).*cannot be restored/.test(pl.excluded[0].reason) && same(pl.steps.map((s) => s.id), R63), `v${version} file: the patches are excluded with the reason, everything else restores in order`);
    const run = await runRestore(rq, pl), v = await verifyRestore(rq, pl);
    ok(run.failed === null && v.differ.length === 0 && U.patches.size === 0 && bkCrc(U.objs.get(14)) === bkCrc(A.objs.get(14)), `v${version} file: the rest restores and verifies, the unit's patches are untouched`);
  }
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...clone(file), objects: [...clone(file).objects, { id: 6, size: 0, crc: 0, data: "" }] })), /version 7.*patch chunks.*damaged/), "file: a version 5 file with a physical patch chunk (id 6) is refused");
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...clone(file), objects: [...clone(file).objects, { id: 39, size: 0, crc: 0, data: "" }] })), /patch chunks/) && throwsMsg(() => readArchive(JSON.stringify({ ...mk(4, OLD_PATCH_IDS), objects: [...mk(4, OLD_PATCH_IDS).objects, file.objects.at(-1)] })), /version 4.*no patch records/), "file: a version 5 file with chunk 39 is refused, and so is a version 4 file with a patch record (id 40)");
}
{   // a version 5 file: every refusal about the patches comes before any write
  const bad = (f) => { const c = clone(file); f(c); return c; }, P = (c, slot = 0) => c.objects.find((o) => o.id === 40 && o.slot === slot);
  const rewrite = (c, f, slot = 0) => { const o = P(c, slot), raw = Uint8Array.from(atob(o.data), (x) => x.charCodeAt(0)); f(raw); o.data = btoa(String.fromCharCode(...raw)); o.crc = bkCrc(raw); o.size = raw.length; };
  ok(readArchive(JSON.stringify(file)).objects.size === 20, "file v5: the good file reads (20 objects)");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects.push(clone(P(c, 1))); P(c, 1).slot = 1; c.objects.at(-1).slot = 1; })), /Patch #02 is in the file twice/), "file v5: a slot listed twice is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { P(c).slot = 128; })), /valid slot/) && throwsMsg(() => readArchive(bad((c) => { delete P(c).slot; })), /valid slot/) && throwsMsg(() => readArchive(bad((c) => { P(c).slot = "1"; })), /valid slot/), "file v5: a patch without a slot, with a slot over 127 or not a number is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { P(c).crc = (P(c).crc ^ 1) >>> 0; })), /Patch #01 failed its checksum/) && throwsMsg(() => readArchive(bad((c) => { P(c).data = P(c).data.slice(0, -4); })), /Patch #01 is damaged/), "file v5: a patch with a wrong CRC or a truncated one is refused, naming the patch");
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[2] = 5; }))), /Patch #01: .*slot and revision/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[3] = 1; }))), /Patch #01: .*slot and revision/), "file v5: a good CRC over a record with a slot or revision byte is refused (the records of a backup have both 0)");
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[0]++; }))), /Patch #01: damaged patch record/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[4] &= ~PTF.CB; }))), /damaged patch record/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[7] = 7; }))), /damaged patch record/)
     && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[5] = 99; }))), /damaged patch record/), "file v5: a record with a wrong length field, no CB flag, a control character in the name or an unknown engine is refused");
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { /* too short */ }, 0) || (P(c).size = 0, P(c).data = "", P(c).crc = 0))), /Patch #01/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, () => {}, 0) || (() => { const r = new Uint8Array(581); r[0] = 581 & 255; r[1] = 581 >> 8; P(c).data = btoa(String.fromCharCode(...r)); P(c).size = 581; P(c).crc = bkCrc(r); })())), /Patch #01/),
     "file v5: a patch of size 0 or of 581 bytes is refused (only saved patches, 20 to 580 bytes)");
  const xb = readArchive(JSON.stringify(file));
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[4] |= 16; }))), /Patch #01: .*older version/), "file v5: a patch record of the older form (the extension flag, PT_VER 3) is refused with a plain message");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 5; c.objects.find((o) => o.id === 40).id = 47; })), /version 5.*no patches of slots 129 to 175/), "file v5: an id 47 object (slots 129..175) is damaged in a version 5 file");
  ok(readArchive(JSON.stringify({ ...clone(file), objects: clone(file).objects.filter((o) => o.id !== 40) })).hasPatches === true && readArchive(JSON.stringify({ ...clone(file), patchRecords: undefined, objects: clone(file).objects.filter((o) => o.id !== 40) })).hasPatches === false, "file v5: patchRecords says a file without patches is the file of a unit with none; without the flag it leaves a unit's patches alone");
  ok(xb.objects.get(1000).size >= 20 && !xb.problems.length, "file v5: (no raw objects in the good file)");
}
{   // the installer's file of a DUELL unit (felucca-backup v1, 16 objects) is read, its patch chunks kept out
  const M = new MockUnit({ caps: 3 });
  const v1 = await installerCapture((r, o) => M.request(r, o), "FELUCCA DUELL 0.2.13 bet");
  const a1 = readArchive(v1);
  ok(a1.version === 1 && a1.source === "felucca-backup v1" && a1.objects.size === 2 && a1.oldPatchChunks === 2 && !a1.hasPatches, "v1 file (the installer's, 16 objects of a DUELL unit) is read: music and settings; the two patch chunks that hold patches are kept out, retired ones dropped");
  const N = new MockUnit({ seed: false });
  const plan = planRestore(a1, await listUnit((r, o) => N.request(r, o), 63), 63, all);
  ok(plan.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && plan.excluded[0].reason.includes("older firmware") && plan.excluded.slice(1).every((e) => e.reason === "not in this file"), "v1 file: the patches are 'older firmware', multis, patterns and wavetables are 'not in this file'");
  const r = await runRestore((r2, o) => N.request(r2, o), plan);
  ok(r.failed === null && same(r.written, [1, 0]), "v1 file: restored in order (settings, music)");
  const felucca = clone(v1); felucca.firmware = "FELUCCA 1.6.1";
  ok(throwsMsg(() => readArchive(felucca), /Felucca backup/), "v1 file of a Felucca unit (firmware without DUELL) is refused");
}
{   // the installer's own backup flow against every generation of unit (the mock unit answers as the firmware does)
  const html = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  const archiveInfo = new Function("parseInfo", html.match(/function archiveInfo\(a\) \{[\s\S]*?\n\}\n/)[0] + "return archiveInfo;")(parseInfoInst);   // the page's own function
  ok(BACKUP_IDS_FULL.length === 27 && BACKUP_IDS_WT.length === 30 && BACKUP_IDS_P16.length === 40 && same(BACKUP_IDS_P16.filter((id) => !PATCH_IDS_X.includes(id)).slice(26, 29), [26, 32, 33]), "installer: manifests of 27 (0.2.14), 30 (wavetables) and 40 ids (0.2.15)");
  for (const [caps, n, objs] of [[3, 16, 2], [7, 27, 13], [15, 30, 16], [31, 40, 16]]) {
    const U = new MockUnit({ caps }), link = (r, o) => U.request(r, o);
    if (caps & 16) { U.objs.set(29, buildLegacyPatchChunk([{ slot: 100, name: "CHUNK 29" }])); U.objs.set(38, buildLegacyPatchChunk([{ slot: 121, name: "CHUNK 38" }])); }   // (chunks past the seed)
    const info = archiveInfo(await U.request([1, []]));
    ok(info.supported && info.caps === caps, `installer: archiveInfo accepts a unit with song rows 0 and caps ${caps}`);
    const inst = await installerCapture(link, info.version);
    ok(readBackup(inst).objects.length === n && inst.objects.length === n, `installer caps ${caps}: capture lists ${n} objects and the file reads`);
    const ar = readArchive(inst);
    ok(ar.objects.size === objs && ar.source === "felucca-backup v1", `installer caps ${caps}: the editor's Restore reads the file (${objs} DUELL objects)`);
    ok(!ar.hasPatches && [...ar.objects.keys()].every((k) => !OLD_PATCH_IDS.includes(k) && !isPatchKey(k)) && ar.oldPatchChunks >= 2 && ar.notes.length === 1, `installer caps ${caps}: the patch chunks of the file (${ar.oldPatchChunks} hold patches) are kept out of the patches (never restored by the editor)`);
    // the installer's own restore onto a fresh unit of the same kind, then the editor's verify
    const T = new MockUnit({ caps, seed: false });
    const log = []; const tlink = async (r, o) => { log.push(r[0]); return T.request(r, o); };
    await restoreInst(tlink, inst);
    ok(!log.some((c) => c >= 11 && c <= 13) && log.filter((c) => c === 14).length === 3, `installer caps ${caps}: its restore touches only the three sample slots 32..34 (erase), never ids 35..39 (patch chunks)`);
    const planned = planRestore(ar, await listUnit((r, o) => T.request(r, o)), caps, all);
    const v = await verifyRestore((r, o) => T.request(r, o), { steps: [...ar.objects.values()].filter((o) => o.id > 1).map((o) => ({ id: o.id, size: o.size, crc: o.crc })) });
    ok(v.differ.length === 0 && (caps === 3 || v.same.length > 0) && OLD_PATCH_IDS.filter((id) => U.has(id)).every((id) => same(T.objs.get(id) || nothing, U.objs.get(id) || nothing)), `installer caps ${caps}: after the installer's restore every object equals the file, its patch chunks too (the editor's verify: ${v.same.length} same)`);
    // the editor's Restore of the installer's file
    const E = new MockUnit({ caps, seed: false });
    const run = await runRestore((r, o) => E.request(r, o), planned.steps.length ? planRestore(ar, await listUnit((r, o) => E.request(r, o)), caps, all) : planned);
    const v2 = await verifyRestore((r, o) => E.request(r, o), planRestore(ar, await listUnit((r, o) => E.request(r, o)), caps, all));
    ok(run.failed === null && v2.differ.length === 0, `installer caps ${caps}: the editor's Restore of the installer's file succeeds and verifies`);
  }
  {   // a 40-object file onto older firmware (the installer's restore skips what that unit refuses) and a 16-object file onto a new unit (the ten more chunks are emptied)
    const N = new MockUnit({ caps: 31 }), file = await installerCapture((r, o) => N.request(r, o), "FELUCCA DUELL 0.2.15 bet");
    const O = new MockUnit({ caps: 3, seed: false });
    await restoreInst((r, o) => O.request(r, o), file);
    ok(bkCrc(O.objs.get(6)) === bkCrc(N.objs.get(6)) && !O.objs.has(27), "installer: a 40-object file onto 0.2.13 firmware restores what it has and skips the rest");
    const U3 = new MockUnit({ caps: 3 }), old = await installerCapture((r, o) => U3.request(r, o), "FELUCCA DUELL 0.2.13 bet");
    const X = new MockUnit({ caps: 31 }); X.objs.set(28, buildLegacyPatchChunk([{ slot: 9, name: "STALE" }]));
    await restoreInst((r, o) => X.request(r, o), old);
    ok(!X.objs.get(28)?.length, "installer: a 16-object file onto a new unit empties the ten more chunks (no record outlives the restore)");
  }
}
{   // what the installer's generic backup (fm1backup.js, out of scope here) still assumes: the manifests of 16 .. 40 ids with the patch chunks. A unit with caps bit 5 lists 24 ids.
  const N = new MockUnit();
  ok(await athrows(() => installerCapture((r, o) => N.request(r, o), "FELUCCA DUELL 0.2.26 bet"), /manifest/), "installer (still the old chunk model): a unit with caps 63 lists 24 ids, which no installer manifest has: its capture refuses (the Backup page is the way to back it up)");
}
{   // FM6's voice bank (INFO tag 44 01 bit 1, GET 41, restored with FM6B_BEGIN / WRITE / END)
  const voice = (k) => Uint8Array.from({ length: 128 }, (_, i) => (i * 7 + k * 13 + 5) & 127);
  ok(same([...unpackVoice(packVoice(voice(3)), 0)], [...voice(3)]), "voice bank: a record packs 8 to 7 and unpacks to the same 128 bytes");
  const S = new MockUnit({ fvBank: true }), sq = (r, o) => S.request(r, o);
  ok(parseInfo(await sq([1, []])).fvBank === true && parseInfo(await new MockUnit({ tide: true }).request([1, []])).fvBank === false, "voice bank: INFO tag 44 01 bit 1 says the unit can GET 41");
  const noBank = await captureBackup(sq, parseInfo(await sq([1, []])));
  ok(!noBank.objects.some((o) => o.id === FVB_ID) && !readArchive(noBank).objects.has(FVB_ID), "voice bank: a unit with none gives a file without object 41 (the probe answers rc 1)");
  S.bank = buildBankObject("MY BANK", new Map([[0, voice(0)], [5, voice(5)], [31, voice(31)]]));
  const fileB = await captureBackup(sq, parseInfo(await sq([1, []])));
  const ob = fileB.objects.find((o) => o.id === FVB_ID), arch = readArchive(JSON.stringify(fileB));
  ok(ob && ob.size === FVB_OBJ && arch.objects.has(FVB_ID) && fileB.objects.indexOf(ob) < fileB.objects.findIndex((o) => o.id === 0), "voice bank: the backup holds object 41 (3620 bytes), before the music");
  const dsc = describe(bytesOf(arch)).fm6bank;
  ok(dsc.name === "MY BANK" && dsc.voices === 3 && bankVoices(arch.objects.get(FVB_ID).bytes).voices.map((v) => v.index).join() === "0,5,31", "voice bank: describe names it and counts 3 voices");
  const T = new MockUnit({ fvBank: true, seed: false }), tq = (r, o) => T.request(r, o);
  const plan = planRestore(arch, await listUnit(tq, 63), 63, all, { fvBank: true });
  ok(plan.steps.some((s) => s.id === FVB_ID) && plan.steps.findIndex((s) => s.id === FVB_ID) < plan.steps.findIndex((s) => s.id === 1), "voice bank: the plan restores it, before the settings and the music");
  const res = await runRestore(tq, plan), ver = await verifyRestore(tq, plan);
  ok(res.failed === null && T.bank && same([...T.bank], [...S.bank]) && ver.differ.length === 0 && ver.same.includes(FVB_ID), "voice bank: restored onto an empty unit through FM6B commands: the same bytes, verify reads it back equal");
  const U = new MockUnit({ fvBank: true, seed: false }); U.bank = buildBankObject("KEEP ME", new Map([[2, voice(2)]]));
  const keep = [...U.bank], noObj = readArchive(JSON.stringify(noBank));
  await runRestore((r, o) => U.request(r, o), planRestore(noObj, await listUnit((r, o) => U.request(r, o), 63), 63, all, { fvBank: true }));
  ok(same([...U.bank], keep), "voice bank: a file without object 41 leaves the unit's bank alone");
  const Pl = new MockUnit({ seed: false }), pOld = planRestore(arch, await listUnit((r, o) => Pl.request(r, o), 63), 63, all, { fvBank: false });
  ok(!pOld.steps.some((s) => s.id === FVB_ID) && pOld.excluded.some((x) => x.key === "fm6bank" && /cannot restore/.test(x.reason)), "voice bank: a unit without the flag is not sent object 41 (excluded, with the reason)");
  const flip = clone(fileB); const bad = flip.objects.find((o) => o.id === FVB_ID); const raw = atob(bad.data).split(""); raw[40] = String.fromCharCode(raw[40].charCodeAt(0) ^ 1); bad.data = btoa(raw.join(""));
  ok(throwsMsg(() => readArchive(flip), /checksum|damaged/), "voice bank: a file whose bank was changed fails its checksum and is refused");
  const half = clone(fileB); const h = half.objects.find((o) => o.id === FVB_ID); const hb = Uint8Array.from(atob(h.data), (c) => c.charCodeAt(0)); hb[FVB_OBJ - 1] ^= 1; h.data = btoa(String.fromCharCode(...hb)); h.crc = bkCrc(hb);
  ok(throwsMsg(() => readArchive(half), /voice bank/), "voice bank: a bank whose own CRC (last four bytes) is wrong is refused even when the file's CRC matches");
  const playing = new MockUnit({ fvBank: true, playing: true }); playing.bank = S.bank;
  ok(await athrows(() => runRestore((r, o) => playing.request(r, o), { steps: [{ id: FVB_ID, size: FVB_OBJ, bytes: S.bank, group: "fm6bank" }] }).then((x) => { if (x.failed) throw new Error("x"); }), /x/), "voice bank: a playing unit refuses the bank upload (reported as a failure)");
}
{   // D02: a patch whose End B is an FM6 MORPH TO target (MBV_FMT, flag 4) is taken to a unit without TIDE with the flag and its 3 bytes removed; a TIDE unit gets it as it is
  const fm = engineIndex("FM6"), eb = buildEndB({ engine: fm, amount: 7, values: { 6: 5 }, fm6: Array.from({ length: 65 }, (_, i) => i) }), ebf = Uint8Array.from([...eb, 9, 0x5a, 0x3c]);
  ebf[0] |= 4;                                           // (End B + the target 9 and its check sum: TIDE's MBV_FMT)
  const patch128 = Array.from({ length: 128 }, (_, i) => i), withFmt = buildRecord({ engine: fm, name: "MORPH TO", values: storeDefaults(fm), mb: ebf, patch128 }), plain = buildRecord({ engine: fm, name: "NO TARGET", values: storeDefaults(fm), mb: eb, patch128 });
  const cut = stripEndBFmt(withFmt), pr = parseRecord(cut), po = parseRecord(withFmt);
  ok(po.ok && cut !== withFmt && cut.length === withFmt.length - 3 && pr.ok && pr.name === "MORPH TO" && pr.fm6.every((v, i) => v === patch128[i]) && same(pr.mb, eb) && same(pr.values, po.values) && stripEndBFmt(plain) === plain,
     "strip: End B's MORPH TO target (flag 4 and its 3 bytes) is removed, the rest of the record stays; a record without it comes back as it is");
  const T = new MockUnit({ seed: false, tide: true }); T.setRecord(2, withFmt); T.setRecord(3, plain); T.rebuild();
  const arch = readArchive(JSON.stringify(await captureBackup((r, o) => T.request(r, o), parseInfo(await T.request([1, []])))));
  const P = new MockUnit({ seed: false }), rq = (r, o) => P.request(r, o), list = await listUnit(rq, 63);
  const noTag = parseInfo(await P.request([1, []])), plainSaid = parseInfo(await new MockUnit({ plainTag: true }).request([1, []])), tideSaid = parseInfo(await T.request([1, []]));
  ok(noTag.tideAnswer === undefined && plainSaid.tideAnswer === false && plainSaid.tide === false && tideSaid.tideAnswer === true,
     "INFO: no tag 44 is an unknown answer (tideAnswer undefined: plain DUELL 1.1 and TIDE builds before the tag look the same); tag 44 with bit 0 clear says plain DUELL");
  const pn = planRestore(arch, list, 63, new Set(["patches"]), { tide: plainSaid.tideAnswer }), pk = planRestore(arch, list, 63, new Set(["patches"]), { tide: noTag.tideAnswer }), pt = planRestore(arch, list, 63, new Set(["patches"]), { tide: true }), pu = planRestore(arch, list, 63, new Set(["patches"]));
  const st = pn.steps.find((x) => x.slot === 2), ot = pt.steps.find((x) => x.slot === 2);
  ok(st.size === withFmt.length - 3 && st.crc === bkCrc(st.bytes) && same(st.bytes, cut) && ot.size === withFmt.length && same(ot.bytes, withFmt) && same(pu.steps.find((x) => x.slot === 2).bytes, cut) && same(pk.steps.find((x) => x.slot === 2).bytes, cut) && pk.stripped === 1 && pn.stripped === 1 && pt.stripped === 0 && pu.stripped === 1 && same(pt.steps.find((x) => x.slot === 2).bytes, withFmt) && same(pn.steps.find((x) => x.slot === 3).bytes, plain),
     "plan: a unit that says plain DUELL and one without tag 44 (unknown) get the record without MBV_FMT (size, CRC of what is sent) and plan.stripped counts it; a unit that says TIDE gets it as it is");
  const r = await runRestore(rq, pn);
  ok(r.failed === null && same(P.patchBytes(2), cut) && parseRecord(P.patchBytes(2)).ok, "restore: the stripped record is what the unit holds");
  const again = planRestore(arch, await listUnit(rq, 63), 63, new Set(["patches"]), { tide: false });
  ok(again.stripped === 0 && again.steps.length === 0 && again.skipped.length === 2,
     "plan: a restore twice onto a plain unit counts nothing as stripped: the patches already identical on the unit are skipped, not written, and lose nothing");
  const n1 = strippedNote(1, false), n2 = strippedNote(2, undefined), n0 = strippedNote(0, undefined);
  ok(n0 === "" && /^1 PATCH LOSES the FM6 MORPH TO target \(this unit says it is plain DUELL/.test(n1) && /^2 PATCHES LOSE the FM6 MORPH TO target \(this unit does not say it is TIDE/.test(n2) && !/would keep/.test(n1 + n2) && n2.includes("taken out"),
     "confirm text: LOSES/LOSE by count, the plain-DUELL and the unknown-unit variants, nothing when none, and no claim that an older TIDE build keeps the target");
}
{   // b10 U4: the multis M17 and up (ids 42..46, caps bit 6, archive version 6)
  const A = new MockUnit({ caps: 127 });
  A.objs.set(42, buildMultiObject(["SEVENTEEN", null])); A.objs.set(45, buildMultiObject([null, "TWENTYFOUR"]));
  const inf = parseInfo(await A.request([1, []])), f = await captureBackup((r, o) => A.request(r, o), inf), arch = readArchive(JSON.stringify(f));
  ok(inf.caps === 127 && f.version === 7 && [42, 43, 44, 45, 46].every((id) => arch.objects.has(id)) && arch.objects.get(42).size > 8 && arch.objects.get(43).size === 0 && arch.objects.get(45).size > 8,
     "U4: a unit with caps 127 lists the multi objects 42..46; the version 7 file carries all five (M17 and M24 stored, the rest empty)");
  const dsc = describe(bytesOf(arch));
  ok(dsc.multis.some((m) => m.label === "M17" && m.name === "SEVENTEEN") && dsc.multis.some((m) => m.label === "M24" && m.name === "TWENTYFOUR") && objectLabel(42) === "multis M17 and M18" && objectLabel(46) === "multis M25 and M26",
     "U4: the preview names M17 and M24, and the object labels count on from M16");
  const B = new MockUnit({ caps: 127, seed: false }), rq = (r, o) => B.request(r, o);
  const plan = planRestore(arch, await listUnit(rq, 127), 127, new Set(["multis2"]), { tide: true });
  ok(plan.excluded.length === 0 && plan.steps.some((s) => s.id === 42) && plan.steps.some((s) => s.id === 45),
     "U4: the plan for the multis M17 and up writes the stored objects 42 and 45");
  const run = await runRestore(rq, plan);
  ok(run.failed === null && same(B.objs.get(42), A.objs.get(42)) && same(B.objs.get(45), A.objs.get(45)), "U4: the restore puts the objects 42 and 45 on a blank unit as they were");
  const v5 = { ...f, version: 5, objects: f.objects.filter((o) => ![42, 43, 44, 45, 46].includes(o.id)) }, a5 = readArchive(JSON.stringify(v5));
  const p5 = planRestore(a5, await listUnit(rq, 127), 127, new Set(["multis", "multis2", "settings", "music"]), { tide: true });
  ok(a5.objects.size === arch.objects.size - 5 && !p5.steps.some((s) => s.id >= 42) && !p5.excluded.some((x) => x.key === "multis2"),
     "U4: a version 5 file has no M17 and up; its restore leaves them alone and reports nothing");
  const old = new MockUnit({ caps: 63 }), p63 = planRestore(arch, await listUnit((r, o) => old.request(r, o), 63), 63, new Set(["multis2"]), { tide: true });
  ok(p63.excluded.some((x) => x.key === "multis2" && /update it first/.test(x.reason)) && p63.steps.length === 0, "U4: a unit without caps bit 6 cannot take them: the group is excluded with the reason");
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...v5, objects: [...v5.objects, f.objects.find((o) => o.id === 42)] })), /no multis M17 and up/), "U4: a version 5 file with an object 42 is refused as damaged");
}
// ---- b12: 175 slots (INFO 44 01 bit 2): the patches of slots 129..175 travel as id 47 (slot byte = slot - 128), LIST pages 5 and 6, archive version 7
{
  const rqOf = (u) => (r, o) => u.request(r, o);
  const GET = BK.GET, PUT = BK.PUT;
  const recs = (u, ks) => { for (const k of ks) u.setRecord(k, buildPatchRecord({ name: `HI ${k}`, engine: k % 3 })); u.rebuild(); };
  const A = new MockB12({ seed: false }); recs(A, [3, 160, 174, 128]);                  // b12 source: slots 3, 128 (#125), 160 and 174 (#171)
  const inf = parseInfo(await A.request([1, []])), rA = rqOf(A);
  ok(inf.slots175 === true && inf.patchCap === 150 && parseInfo(await new MockUnit().request([1, []])).slots175 === false, "b12: INFO 44 01 bit 2 is read (slots175), the capacity says 150; a unit without the bit says false");
  // LIST pages 5 and 6: slots 128..159 and 160..174, the byte of each entry is slot - 128
  await A.request([BK.LIST, []]);
  const p5 = await A.request([BK.LIST, [5]]), p6 = await A.request([BK.LIST, [6]]), p1 = await A.request([BK.LIST, [1]]);
  ok(p5[2] === 1 && p5[3] === 0 && p6[2] === 2 && p6[3] === 32 && p6[3 + 11] === 46 && p1[2] === 1 && (await A.request([BK.LIST, [7]]))[1] === 1, "b12: LIST page 5 lists slot 128 (byte 0), page 6 slots 160 and 174 (bytes 32 and 46), page 7 is rc 1");
  const old = new MockUnit({ seed: false });
  await old.request([BK.LIST, []]);
  ok((await old.request([BK.LIST, [5]]))[1] === 1 && (await old.request([BK.LIST, [6]]))[1] === 1, "b12: a unit without the bit answers LIST pages 5 and 6 with rc 1");
  const list = await listUnit(rA, inf.caps, true);
  ok([...list.keys()].filter(isPatchKey).sort((a, b) => a - b).join() === "1003,1128,1160,1174" && list.get(patchKey(160)).size === A.patchBytes(160).length, "b12: listUnit reads the six pages: the patches of slots 3, 128, 160, 174 under their keys");
  // capture -> file version 7 with id 47 objects and slot bytes slot - 128
  const f7 = await captureBackup(rA, inf);
  const ids47 = f7.objects.filter((o) => o.id === 47);
  ok(f7.slots175 === true && readArchive(JSON.stringify(f7)).slots175 === true, "b12: a backup of a b12 unit says slots175 (it carries slots 129..175)");
  ok(f7.version === 7 && ids47.map((o) => o.slot).join() === "0,32,46" && f7.objects.filter((o) => o.id === 40).map((o) => o.slot).join() === "3" && f7.patchRecords === true, "b12: the backup is version 7: slot 3 is id 40, slots 128 / 160 / 174 are id 47 with the bytes 0 / 32 / 46");
  const a7 = readArchive(JSON.stringify(f7));
  ok(a7.version === 7 && [...a7.objects.keys()].filter(isPatchKey).sort((x, y) => x - y).join() === "1003,1128,1160,1174" && a7.hasPatches && !a7.problems.length, "b12: the version 7 file reads back: the patch keys are 1000 + the real slot");
  ok(objectLabel(patchKey(160)) === "patch #157" && objectLabel(patchKey(160), a7.objects.get(1160).bytes) === "patch #157 HI 160" && describe(bytesOf(a7)).patches.map((p) => p.slot).join() === "3,128,160,174", "b12: the preview and the labels use the unit's numbers (slot 160 is #157)");
  // restore the file onto a blank b12 unit: round trip, verify
  const B = new MockB12({ seed: false }), rB = rqOf(B);
  const planB = planRestore(a7, await listUnit(rB, 63, true), 63, all, { tide: true, slots175: true, patchCap: 150 });
  ok(planB.steps.filter((s) => s.key !== undefined).map((s) => `${s.id}:${s.slot}`).join() === "40:3,47:0,47:32,47:46" && planB.wide === true && !planB.excluded.length, "b12: the plan sends slot 3 as id 40 and slots 128 / 160 / 174 as id 47 with the bytes 0 / 32 / 46");
  const runB = await runRestore(rB, planB), vB = await verifyRestore(rB, planB);
  ok(!runB.failed && runB.written.includes(1160) && patchRows(B) === patchRows(A) && vB.differ.length === 0 && vB.same.includes(1174), "b12: the version 7 file restores onto a blank b12 unit, every record the same (slots 3 and 160 included), verify finds no difference");
  const plan2 = planRestore(a7, await listUnit(rB, 63, true), 63, new Set(["patches"]), { tide: true, slots175: true });
  ok(plan2.steps.length === 0 && plan2.skipped.length === 4, "b12: a second restore finds all four patches identical and writes nothing");
  // a version 7 file makes the patches exactly the file's (slots past 127 too)
  const C = new MockB12({ seed: false }); recs(C, [3, 140, 170]);
  const planC = planRestore(a7, await listUnit(rqOf(C), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true });
  ok(planC.steps.filter((s) => !s.size).map((s) => s.key).join() === "1140,1170" && (await runRestore(rqOf(C), planC)).failures.length === 0 && patchRows(C) === patchRows(A), "b12: a version 7 file makes the patches exactly the file's: the unit's slots 140 and 170 are removed first");
  {   // review PR 99, 3: a backup made with this editor on a 128-slot unit is version 6, and restoring it onto a b12 unit keeps the saved #125..#171
    const O = new MockUnit({ seed: false }); recs(O, [3, 9]);
    const fo = await captureBackup(rqOf(O), parseInfo(await O.request([1, []]))), ao = readArchive(JSON.stringify(fo));
    const T = new MockB12({ seed: false }); recs(T, [3, 130, 140]);
    const pl = planRestore(ao, await listUnit(rqOf(T), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true, patchCap: 150 });
    await runRestore(rqOf(T), pl);
    ok(fo.version === 7 && fo.slots175 === false && !ao.slots175 && !pl.steps.some((s) => s.key === 1130 || s.key === 1140) && T.patches.has(130) && T.patches.has(140) && T.patches.has(9),
       "b12 (review PR 99, 3): a backup of a 128-slot unit says slots175 false; restored onto a b12 unit it keeps the unit's saved #126 and #136 (slots 130, 140)");
  }
  {   // review PR 99, 4: the two rooms (126 for slots 1..128, 24 for 129..175) are checked apart, before anything is written
    const big = new MockB12({ seed: false }), ks = [...Array(126).keys(), ...Array.from({ length: 25 }, (_, i) => 128 + i)];
    recs(big, ks.slice(0, 126));
    const fb = await captureBackup(rqOf(big), parseInfo(await big.request([1, []]))), ab = readArchive(JSON.stringify(fb));
    const T2 = new MockB12({ seed: false });
    const okLow = !planRestore(ab, await listUnit(rqOf(T2), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true, patchCap: 150 }).excluded.length;
    ab.objects.set(patchKey(126), ab.objects.get(patchKey(0)));                       // a 127th of slots 1..128
    const exLow = planRestore(ab, await listUnit(rqOf(T2), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true, patchCap: 150 }).excluded;
    ab.objects.delete(patchKey(126));
    for (let i = 0; i < 25; i++) ab.objects.set(patchKey(128 + i), ab.objects.get(patchKey(0)));   // 25 of slots 129..175
    const exHi = planRestore(ab, await listUnit(rqOf(T2), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true, patchCap: 150 }).excluded;
    ok(okLow && exLow.some((x) => x.key === "patches" && /127 patches of slots 1\.\.128.*126 and 24/.test(x.reason)) && exHi.some((x) => x.key === "patches" && /25 of slots 129\.\.175/.test(x.reason)),
       "b12 (review PR 99, 4): 126 patches of slots 1..128 fit, a 127th or a 25th past the 128 is refused before anything is written (STORE FULL)");
    ok(ks.length === 151, "(the test's slots)");
  }
  // a version 6 file: restores as before and leaves slots 129..175 as they are
  const S6 = new MockB12({ seed: false }); recs(S6, [3, 9]);
  const f6 = clone(await captureBackup(rqOf(S6), parseInfo(await S6.request([1, []])))); f6.version = 6;
  const a6 = readArchive(JSON.stringify(f6));
  const D = new MockB12({ seed: false }); recs(D, [3, 160, 5]);                          // the target holds slot 160 (and slot 5, which the file has not: removed)
  const crc160 = bkCrc(D.patchBytes(160));
  const planD = planRestore(a6, await listUnit(rqOf(D), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true });
  const runD = await runRestore(rqOf(D), planD);
  ok(!runD.failed && !planD.steps.some((s) => s.id === 47) && D.patches.has(160) && bkCrc(D.patchBytes(160)) === crc160 && D.patches.has(9) && D.patches.get(3).name === "HI 3" && !D.patches.has(5), "b12: a version 6 file restores on a b12 unit as before (slots under 128 made the file's) and leaves the saved record of slot 160 untouched");
  ok((await verifyRestore(rqOf(D), planD)).differ.length === 0, "b12: ..and its verify finds nothing different");
  // a version 7 file to a unit WITHOUT the bit: the id 47 patches are skipped, the plan says so, the rest is restored
  const O = new MockUnit({ seed: false }), rO = rqOf(O);
  const infoO = parseInfo(await O.request([1, []]));
  const planO = planRestore(a7, await listUnit(rO, infoO.caps, infoO.slots175), infoO.caps, all, { tide: infoO.tideAnswer, slots175: infoO.slots175, patchCap: infoO.patchCap });
  const exO = planO.excluded.find((x) => x.key === "patches");
  ok(infoO.slots175 === false && exO && /^3 patches of slots 129\.\.175 need TIDE b12 or newer; they are not restored$/.test(exO.reason) && planO.steps.filter((s) => s.key !== undefined).map((s) => `${s.id}:${s.slot}`).join() === "40:3" && planO.wide === false,
     "b12: a version 7 file onto a unit without the bit: the 3 patches of slots 129..175 are left out with the plain note, slot 3 and every other group stay in the plan");
  const runO = await runRestore(rO, planO), vO = await verifyRestore(rO, planO);
  ok(!runO.failed && O.patches.size === 1 && O.patches.has(3) && vO.differ.length === 0 && O.log.every((x) => x !== "cmd 90"), "b12: ..the rest is restored and verified on the old unit; nothing past slot 127 is sent");
  const f7b = clone(f7); f7b.objects = f7b.objects.filter((o) => o.id !== 47 || o.slot === 32);
  const planO1 = planRestore(readArchive(JSON.stringify(f7b)), await listUnit(rO, infoO.caps, false), infoO.caps, new Set(["patches"]), { slots175: false });
  ok(/^1 patch of slots 129\.\.175 needs TIDE b12 or newer; it is not restored$/.test(planO1.excluded[0]?.reason), "b12: one patch past 128 reads '1 patch of slots 129..175 needs TIDE b12 or newer; it is not restored'");
  // the file checks of id 47
  const withSlot = (id, slot) => { const c = clone(f7); c.objects.find((o) => o.id === id).slot = slot; return JSON.stringify(c); };
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...clone(f7), version: 6 })), /no patches of slots 129 to 175/) && throwsMsg(() => readArchive(withSlot(47, 47)), /no valid slot/) && throwsMsg(() => readArchive(withSlot(40, 128)), /no valid slot/),
     "b12: id 47 in a file of version 6 is refused, and slot bytes out of range (id 47: over 46, id 40: over 127) are no valid slot");
  // the unit's rules for PUT 47 over the wire: b12 only, begin 13 bytes, byte 0..46
  const W = new MockB12({ seed: false }), rw = rqOf(W), rec = buildPatchRecord({ name: "WIRE 47" }), crc = bkCrc(rec);
  await W.request([BK.LIST, []]);
  const b47 = (u, slot) => u.request([PUT, [0, 47, ...bkU32(rec.length), ...bkU32(crc), slot]]);
  ok((await b47(W, 47))[2] === 1 && (await b47(W, 0))[2] === 0, "b12: PUT 47 begin: a byte over 46 is rc 1, 0..46 is accepted");
  ok((await b47(new MockUnit({ seed: false }), 0))[2] === 1, "b12: a unit without the bit answers PUT 47 with rc 1 (unknown object)");
  // GET 47 reads slot 128 + byte (rc 1 for byte 5 under id 40: no record there; rc 5 on a unit without the bit: no LIST before, as for any id)
  const G = new MockB12({ seed: false }); recs(G, [133]); await G.request([BK.LIST, []]);
  const g47 = await G.request([GET, [47, ...bkU32(0), 20, 0, 5]]), g40 = await G.request([GET, [40, ...bkU32(0), 20, 0, 5]]);
  ok(g47[1] === 0 && g40[1] === 1, "b12: GET 47 with byte 5 reads slot 133; GET 40 with byte 5 has no record (rc 1)");
  // the installer's capture gives the same version 7 file
  const { captureUnitBackup } = await import("./installer_backup.js");
  const cap = await captureUnitBackup(rA, await A.request([1, []]));
  ok(cap.format === "duell-backup" && cap.file.version === 7 && cap.file.objects.filter((o) => o.id === 47).length === 3, "b12: the installer's capture of a b12 unit is the same version 7 file (id 47 objects)");
  // STORE FULL for id 47 patches
  const FS = new MockB12({ seed: false, storeMax: 2 }); recs(FS, [1, 2]);
  const planFS = planRestore(a7, await listUnit(rqOf(FS), 63, true), 63, new Set(["patches"]), { tide: true, slots175: true });
  const runFS = await runRestore(rqOf(FS), planFS);
  ok(planFS.steps.slice(0, 2).every((s) => !s.size) && runFS.failures.length === 2 && runFS.failures.every((x) => x.rc === 6) && FS.patches.size === 2, "b12: STORE FULL (rc 6) answers the patches that do not fit; removals come first, the failures are reported by key");
}

ok(/^TIDE-backup-\d{4}-\d\d-\d\d-\d{4}\.json$/.test(fileName("TIDE-backup")), "file name: TIDE-backup-YYYY-MM-DD-HHMM.json");

console.log(fails ? `DUELL BACKUP WEB TESTS FAILED (${fails})` : "duell backup web tests passed");
process.exit(fails ? 1 : 0);
