// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor, Backup page: the "duell-backup" file, its checks and decoders, and the capture / restore / verify steps.
// No DOM here (editor/index.html applies it; test_duellbackup.mjs runs it against a simulated unit). The transport is the
// editor protocol's BACKUP_LIST / BACKUP_GET / BACKUP_PUT (EDITOR_PROTOCOL.md "v7: full backup"); requests name objects, never addresses.
//
// The objects (firmware editor_backup.c; ids are the unit's, kept as they are):
//   0         the music of both parts (FUN11, 3840 B; restoring it replaces what plays, RAM only)
//   1         the settings record (palette, LEDs, HOLD, button map, favourites, MENU, MIDI learn)
//   40 + slot the saved patches, ONE PATCH AT A TIME as logical records (job A of the cloud plan, INFO 42 01 caps bit 5, value 32): the record PATCH_RAW moves, 20..534 bytes,
//             its slot and revision bytes 0 (a patch is the same bytes in any slot). The wire id is 40 and the slot a byte after it; in memory a patch is the Map key 1000 + slot
//             (patchKey / patchSlot), so the rest of the code keeps numeric ids. A slot that holds its factory sound has no record. Listed by LIST page 1..4, not by the plain LIST
//   6 7 9..12 27..31 35..39  the patch store's PHYSICAL chunks of before (DUELL 0.2.14 .. 0.2.25): gone from the unit's ids; a file of version 1..4 may carry them, it is read
//             and they are kept out of the patches (they are of the old record and chunk format and cannot go to a unit with the new store)
//   13..20    the multis, two to an object, M01-02 .. M15-16 (magic MUL1; DUELL 0.2.14, INFO 42 01 caps bit 2)
//   42..46    the multis M17-18 .. M25-26 (b10, INFO 42 01 caps bit 6; "version 6"; 41 is the FM6 voice bank); a version 5 file has none of them and its restore leaves M17 and up as they are
//   21..23    the pattern slots, 11 + 11 + 10 to an object, P01-11 P12-22 P23-32 (magic PTS3, compact objects: patslot.js; DUELL 0.2.14, format of job F3, caps bit 2)
//   24..26    the user wavetables USR1..USR3 (1228 bytes: version 1, 19, 64, 0, the 8 character name, 19 x 64 harmonic bytes; length 0 = EMPTY;
//             wavetable.js; DUELL 0.2.14, caps bit 3). A file with them is "version 3"; an editor that reads up to version 2 says so and stops
// "version 5" carries the patches as logical records (objects {id: 40, slot, size, crc, data}, only the saved patches, sorted by slot, any number of them, even none); versions 1 to 4
// carried the physical chunks (version 4: all 16 ids; 3: the first six) and are still read.
// Ids 2..5, 8 and 32..34 are retired (always empty: Felucca's project slots and FM6 bank, the user sample slots) and are not in the file.
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, readBackup } from "../fm1backup.js";
import { checkObject as wtCheck, parseObject as wtParse } from "./wavetable.js";
import { recordProblem, stripEndBFmt, WORST, FIX as REC_FIX } from "./patchrecord.js";
import { PS, parsePatObject } from "./patslot.js";

export const FORMAT = "duell-backup";
export const VERSION = 6;                      // 6 adds the multis M17 to M26 (ids 42..46, caps bit 6); 5 carries the patches as logical records (id 40 + slot); 4 adds the ten more patch chunks (ids 27..31, 35..39); 3 adds the user wavetables (ids 24..26); 2 is read as it was; 1 is the installer's "felucca-backup" file of a DUELL unit (read here, never written)
export const CAPS_MULTI_PATTERN = 4;           // INFO 42 01 caps, bit 2
export const CAPS_MULTI_MORE = 64;             // bit 6 (b10): the multis M17 to M26, ids 42..46
export const MULTI2_IDS = [42, 43, 44, 45, 46];   // two multis an object, M17-18 .. M25-26 (the unit has slots up to M24 for now; the last object reads empty)
export const CAPS_WAVETABLES = 8;              // bit 3: the user wavetables (ids 24..26, cmds 81..83)
export const CAPS_PATCH16 = 16;                // bit 4: the unit holds 128 patches (INFO tag 58 01)
export const CAPS_PATCH_RECORDS = 32;          // bit 5: the patches are backed up and restored one at a time, as records (id 40, LIST page)
export const PATCH_ID = 40;                    // the wire id of a patch record (the slot is a byte after it)
export const PATCH_KEY0 = 1000;                // the Map key of the patch in slot k is PATCH_KEY0 + k
export const PATCH_SLOTS = 128, PATCH_PAGE = 32, PATCH_PAGES = PATCH_SLOTS / PATCH_PAGE, PATCH_MAX = WORST;
export const patchKey = (slot) => PATCH_KEY0 + slot;
export const isPatchKey = (key) => Number.isInteger(key) && key >= PATCH_KEY0 && key < PATCH_KEY0 + PATCH_SLOTS;
export const patchSlot = (key) => key - PATCH_KEY0;
export const OLD_PATCH_IDS = [6, 7, 9, 10, 11, 12, 27, 28, 29, 30, 31, 35, 36, 37, 38, 39];   // the physical chunks of before (not a wire id of a unit with caps bit 5; a v1..v4 file may carry them)

// FM6's voice bank B1..B32 (fm6_vbank.c; INFO tag 44 01 bit 1, DUELL with TIDE): object 41 is the bank as the unit stores it, FVB_LEN bytes (a 32 byte header: magic "FM6V", version 1, 32, the used mask u32,
// the 12 character name, then the 32 voices of 112 bytes, each the 128 7-bit bytes of a record packed 8 to 7) and its CRC-32 (u32, little endian). Read with GET 41 (not in the plain LIST); a unit
// without a bank answers rc 1. It is written back voice by voice with FM6B_BEGIN / WRITE / END (cmds 74..76), the way the unit's own bank upload goes: a bank cannot be erased, so a file
// without the object leaves the unit's bank alone.
export const FVB_ID = 41, FVB_LEN = 3616, FVB_OBJ = FVB_LEN + 4, FVB_N = 32, FVB_REC = 112, FVB_PK = 128, FVB_HEAD = 32;
export const CAPS_FVBANK = 2;                  // INFO tag 44 01 bit 1
export const FM6B_CMD = { BEGIN: 74, WRITE: 75, END: 76 };
export function unpackVoice(b, at) {           // 112 stored bytes at b[at] -> the record's 128 7-bit bytes (upf_pk_get)
  const pk = new Uint8Array(FVB_PK); let acc = 0, n = 0, o = at;
  for (let i = 0; i < FVB_PK; i++) { while (n < 7) { acc |= b[o++] << n; n += 8; } pk[i] = acc & 127; acc >>>= 7; n -= 7; }
  return pk;
}
export function packVoice(pk) {                // the reverse (upf_pk_put)
  const d = new Uint8Array(FVB_REC); let acc = 0, n = 0, o = 0;
  for (let i = 0; i < FVB_PK; i++) { acc |= (pk[i] & 127) << n; n += 7; while (n >= 8) { d[o++] = acc & 255; acc >>>= 8; n -= 8; } }
  return d;
}
// the bank object -> { name, used (mask), voices: [{index, pk}] } (checkObject has accepted it)
export function bankVoices(b) {
  const used = u32(b, 8), voices = [];
  for (let k = 0; k < FVB_N; k++) if (used >>> k & 1) voices.push({ index: k, pk: unpackVoice(b, FVB_HEAD + k * FVB_REC) });
  return { name: text(b, 12, 12).trim(), used, voices };
}

export const GROUPS = [
  { key: "patches", label: "Patches", ids: [], dynamic: true, caps: CAPS_PATCH_RECORDS },   // the patch keys the file holds: any subset, even none (no group-completeness rule)
  { key: "multis", label: "Multis", ids: [13, 14, 15, 16, 17, 18, 19, 20], caps: CAPS_MULTI_PATTERN },
  { key: "patterns", label: "Pattern slots", ids: [21, 22, 23], caps: CAPS_MULTI_PATTERN },
  { key: "wavetables", label: "User wavetables", ids: [24, 25, 26], caps: CAPS_WAVETABLES },
  { key: "fm6bank", label: "FM6 voice bank (B1 to B32)", ids: [FVB_ID], caps: 0, needs: "fvBank", optional: true },   // optional: a file or a unit without a bank is no exclusion to report   // needs: the INFO flag of the unit (parseInfo) it can be restored to
  { key: "settings", label: "Settings", ids: [1], caps: 0 },
  { key: "multis2", label: "Multis M17 and up", ids: MULTI2_IDS, caps: CAPS_MULTI_MORE, optional: true },   // optional: a file from before version 6, or a unit without the bit, is no exclusion to report (GROUPS[0..3] stay where they are)
  { key: "music", label: "Current music (both parts)", ids: [0], caps: 0 },
];
export const DUELL_IDS = [0, 1, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, ...MULTI2_IDS];   // the objects other than the patches (the voice bank, FVB_ID, is not in the plain LIST)
export const RESTORE_ORDER = [13, 14, 15, 16, 17, 18, 19, 20, ...MULTI2_IDS, 21, 22, 23, 24, 25, 26, 1, 0];   // (the voice bank goes just before the settings: planRestore)   // after the patches (by slot); live music last
const KNOWN_IDS = [0, 1, 2, 3, 4, 5, 8, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, FVB_ID, ...MULTI2_IDS, 32, 33, 34, ...OLD_PATCH_IDS];   // what a unit lists (the plain LIST): the chunk ids only an older firmware lists, and they are ignored
const MAX_OBJ = 3840;
const MUSIC_PARTS = 2, MUSIC_REV = 8;               // the music object: NPART 2 (byte 63) and the revision 8 (byte 67: memory trim 1 two parts, job A extension slots, ROLL CHAINED storage: End B as values, project.c PROJ_REV 8); anything else is refused
const MUL_HDR = 8, MUL_REC_MAX = (MAX_OBJ - MUL_HDR) / 2, MUL_VER = 8, MUL_NAME = 8, MUL_G_AT = 20, P_COUNT = 127;
const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"];

const u16 = (b, o) => b[o] | b[o + 1] << 8;
const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const printable = (b, from, n) => { for (let i = 0; i < n; i++) if (b[from + i] && (b[from + i] < 32 || b[from + i] > 126)) return false; return true; };
const text = (b, from, n) => { let s = ""; for (let i = 0; i < n && b[from + i]; i++) s += String.fromCharCode(b[from + i]); return s; };
export const groupOf = (id) => isPatchKey(id) ? GROUPS[0] : GROUPS.find((g) => g.ids.includes(id)) || null;
const idsOf = (g, objects) => (g.dynamic ? [...objects.keys()].filter(isPatchKey).sort((a, b) => a - b) : g.ids);

// ---- object checks: the shapes the unit insists on at commit (the unit checks again; a file that fails here is never sent)
// null when the object is acceptable for id, else a short reason
export function checkObject(id, b) {
  const n = b.length;
  if (n > MAX_OBJ) return "too large";
  if (id === 0) {                                      // the music: FUNB, 3840 B; byte 63 the parts (2), 66 P_COUNT, 67 the revision (firmware project.c PROJ_REV)
    if (n !== MAX_OBJ || u32(b, 0) !== 0x46554E42) return "not the current music of a DUELL unit";
    if (b[63] !== MUSIC_PARTS || b[66] !== P_COUNT || b[67] !== MUSIC_REV) return "the music of an older DUELL version; make a new backup with this version";
    return null;
  }
  if (id === 1) return n >= 64 && u32(b, 0) === 0x50455234 ? null : "not a settings record";
  if (isPatchKey(id)) {                                // a patch record: as the unit reads it (up_put_raw, pt_rec_ok), and in the form PATCH_RAW moves (slot and revision 0)
    if (n < REC_FIX || n > PATCH_MAX) return "not a patch record (its size)";
    if (b[2] || b[3]) return "not a patch record as it is backed up (the slot and revision bytes are not 0)";
    const why = recordProblem(b);
    if (why && (b[4] & (16 | 64))) return "a patch record of an older DUELL version, which this firmware does not load; make a new backup with this version";   // (PT_VER 3: the extension block and bitmap flags, 16 and 64)
    return why ? `damaged patch record (${why})` : null;
  }
  if (groupOf(id)?.key === "multis" || groupOf(id)?.key === "multis2") {
    if (!n) return null;
    const l = [u16(b, 4), u16(b, 6)];
    if (n < MUL_HDR || u32(b, 0) !== 0x314C554D || l[0] > MUL_REC_MAX || l[1] > MUL_REC_MAX || MUL_HDR + l[0] + l[1] !== n) return "not a multi object";
    // 0.2.25: the shape only (the unit lists a multi object only when its own loader takes it: firmware 0.2.22 .. 0.2.24 write version 6). An editor of
    // before 0.2.24 refused version 6 here ("damaged multi"); a record of another version is kept in the file and planRestore says whether it can go back
    for (let h = 0, at = MUL_HDR; h < 2; at += l[h++]) {
      if (!l[h]) continue;
      if (l[h] < MUL_G_AT || !b[at] || b[at] > 15 || b[at + 1] < 8 || b[at + 1] > 127 || !printable(b, at + MUL_NAME, 12)) return "damaged multi";
    }
    return null;
  }
  if (groupOf(id)?.key === "patterns") {
    if (!n) return null;
    return parsePatObject(id - 21, b).error ?? null;       // PTS3: the compact object of patslot.js (a PTS2 object of the 16-slot firmware is refused)
  }
  if (id === FVB_ID) {                                 // the voice bank: size, magic "FM6V", version 1, 32 voices, a printable name, the CRC at the end
    if (n !== FVB_OBJ || u32(b, 0) !== 0x56364D46 || u16(b, 4) !== 1 || u16(b, 6) !== FVB_N || !printable(b, 12, 12)) return "not an FM6 voice bank";
    return bkCrc(b.subarray(0, FVB_LEN)) === u32(b, FVB_LEN) ? null : "the voice bank failed its checksum";
  }
  if (groupOf(id)?.key === "wavetables") {
    if (!n) return null;
    const why = wtCheck(b);
    return why ? `not a user wavetable (${why})` : null;
  }
  return "not an object of a DUELL backup";
}

// ---- decoders for the preview (what is in an object), pure
// patch records (Map key -> bytes) -> [{slot, name, engine, category}] by slot
export function decodePatches(objects) {
  const out = [];
  for (const [key, b] of objects) if (isPatchKey(key) && b && b.length >= REC_FIX) out.push({ slot: patchSlot(key), name: text(b, 7, 12), engine: b[5], category: CATEGORIES[b[6]] || "-" });
  return out.sort((a, c) => a.slot - c.slot);
}
// multi objects (index = id - 13) -> [{label: "M01", name}] for the records that are stored (an empty record shows the factory multi, or nothing)
export function decodeMultis(objects) {
  const out = [];
  objects.forEach((b, o) => {
    if (!b || b.length < MUL_HDR) return;
    const l = [u16(b, 4), u16(b, 6)];
    for (let h = 0, at = MUL_HDR; h < 2; at += l[h++])
      if (l[h]) out.push({ label: "M" + String(2 * o + h + 1).padStart(2, "0"), name: text(b, at + MUL_NAME, 12) });
  });
  return out;
}
// the record versions in a multi object ([] for none): the current firmware writes and loads MUL_VER only
export function multiVersions(b) {
  if (!b || b.length < MUL_HDR) return [];
  const l = [u16(b, 4), u16(b, 6)], out = [];
  for (let h = 0, at = MUL_HDR; h < 2; at += l[h++]) if (l[h] && at + 1 < b.length) out.push({ version: b[at], np: b[at + 1] });
  return out;
}
export const MULTI_VERSION = MUL_VER;
// a short name of what object id holds, for the messages ("multis M01 and M02"); bytes (a patch record) adds the patch's name
export function objectLabel(id, bytes = null) {
  if (isPatchKey(id)) return `patch #${String(patchSlot(id) + 1).padStart(2, "0")}${bytes && bytes.length >= REC_FIX && bytes[7] ? " " + text(bytes, 7, 12) : ""}`;
  if (id === 0) return "the current music";
  if (id === 1) return "the settings";
  if (id >= 13 && id <= 20) { const a = 2 * (id - 13) + 1; return `multis M${String(a).padStart(2, "0")} and M${String(a + 1).padStart(2, "0")}`; }
  if (MULTI2_IDS.includes(id)) { const a = 2 * (id - 42) + 17; return `multis M${String(a).padStart(2, "0")} and M${String(a + 1).padStart(2, "0")}`; }
  if (id >= 21 && id <= 23) { const a = 11 * (id - 21) + 1, b = Math.min(32, a + 10); return `pattern slots P${String(a).padStart(2, "0")} to P${String(b).padStart(2, "0")}`; }
  if (id >= 24 && id <= 26) return `user wavetable ${id - 23}`;
  if (id === FVB_ID) return "the FM6 voice bank";
  return OLD_PATCH_IDS.includes(id) ? "a chunk of the old patch store" : `object ${id}`;
}
// pattern objects (index = id - 21) -> [{label: "P01", name, length}]
export function decodePatterns(objects) {
  const out = [];
  objects.forEach((b, o) => {
    const r = b && parsePatObject(o, b);
    if (r?.recs) r.recs.forEach((rec, i) => out.push({ label: "P" + String(o * PS.PER + i + 1).padStart(2, "0"), name: text(rec, 8, PS.NAME), length: rec[2] }));
  });
  return out;
}
// the user wavetables (index = id - 24) -> [{slot: 1.., label: "USR1", name}] for the slots that hold a table
export function decodeWavetables(objects) {
  const out = [];
  objects.forEach((b, o) => { if (b && b.length) out.push({ slot: o, label: "USR" + (o + 1), name: wtParse(b).name }); });
  return out;
}
// the whole content of an archive (objects: Map id -> bytes) for the page
export function describe(objects, engines = [], oldPatchChunks = 0) {
  const get = (ids) => ids.map((id) => objects.get(id) || null);
  return {
    patches: decodePatches(objects).map((p) => ({ ...p, engineName: engines[p.engine] || `ENGINE ${p.engine}` })),
    multis: decodeMultis([...get(GROUPS[1].ids), ...get(MULTI2_IDS)]),   // (index = object: ids 13..20 are 0..7, ids 42..46 are 8..12)
    patterns: decodePatterns(get(GROUPS[2].ids)),
    wavetables: decodeWavetables(get(GROUPS[3].ids)),
    fm6bank: objects.get(FVB_ID)?.length === FVB_OBJ ? (({ name, voices }) => ({ name, voices: voices.length }))(bankVoices(objects.get(FVB_ID))) : null,
    has: Object.fromEntries(GROUPS.map((g) => [g.key, idsOf(g, objects).some((id) => objects.has(id) && objects.get(id).length > 0)])),
    notes: oldPatchChunks ? [OLD_CHUNKS_NOTE] : [],
  };
}
export const OLD_CHUNKS_NOTE = "Patches saved by an older firmware (physical chunks) are in this file but cannot be restored.";

// ---- INFO (cmd 1): version, engine names, and the capability tags. caps: the `42 01 caps` tag (null: a unit without it)
const TAG_LEN = { 0x55: 3, 0x4d: 4, 0x42: 3, 0x46: 4, 0x53: 3, 0x50: 3, 0x4e: 3, 0x52: 3, 0x4c: 3, 0x41: 4, 0x54: 3, 0x56: 3, 0x43: 3, 0x58: 4, 0x59: 3, 0x5a: 3, 0x5b: 3, 0x4b: 3, 0x4f: 3, 0x44: 3 };
export function parseInfo(a) {
  let pos = a.indexOf(0);
  if (pos < 1) throw new Error("The unit sent an unreadable INFO");
  const version = String.fromCharCode(...a.slice(0, pos)); pos++;
  const nEng = a[pos]; pos += 5;                       // engines, P_COUNT, G_COUNT, NSTEP, P_E0
  const engines = [];
  for (let i = 0; i < nEng; i++) {
    const end = a.indexOf(0, pos);
    if (end < 0) throw new Error("The unit sent an unreadable INFO");
    engines.push(String.fromCharCode(...a.slice(pos, end))); pos = end + 1;
  }
  pos += 2;                                            // NTRK, song rows
  let caps = null, patchCap = null, tide = false, tideAnswer, fvBank = false;
  while (pos < a.length && TAG_LEN[a[pos]]) {
    if (a[pos] === 0x42 && a[pos + 1] === 1) caps = a[pos + 2];
    if (a[pos] === 0x44 && a[pos + 1] === 1) { tide = (a[pos + 2] & 1) !== 0; tideAnswer = tide; fvBank = (a[pos + 2] & CAPS_FVBANK) !== 0; }   // DUELL with TIDE; bit 1: GET 41, the FM6 voice bank
    if (a[pos] === 0x58 && a[pos + 1] === 1) patchCap = a[pos + 2] | a[pos + 3] << 7;   // the patch capacity (DUELL 0.2.15)
    pos += TAG_LEN[a[pos]];
  }
  // tide: for the display (no tag = false). tideAnswer: what the unit SAID: true / false = tag 44 01 present with bit 0 set / clear; undefined = no tag 44 (plain DUELL 1.1 and the TIDE builds before the tag, stable-2026-11-03 and
  // beta-2026-11-04, look the same: the restore planner must not take it for plain DUELL)
  return { version, engines, caps, patchCap, tide, tideAnswer, fvBank };
}

// ---- the file
const base64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
const B64 = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;
// objects: [{id (a patch: its key), bytes, raw?, problem?}] (raw: kept as read, not checked: 0.2.25). opt.patchRecords: the unit gave its patches as records (the file says so even with
// none: a file without the flag and without patches leaves a unit's patches alone); opt.notes: lines for the user
export function objectsToFile(objects, info, opt = {}) {
  const wire = (o) => (isPatchKey(o.id) ? { id: PATCH_ID, slot: patchSlot(o.id) } : { id: o.id });
  const ordered = [...objects.filter((o) => !isPatchKey(o.id)), ...objects.filter((o) => isPatchKey(o.id)).sort((a, b) => a.id - b.id)];
  const problems = ordered.filter((o) => o.raw).map((o) => ({ ...wire(o), problem: o.problem || "not checked" }));
  return {
    format: FORMAT, version: VERSION, firmware: info.version, created: new Date().toISOString(), engines: info.engines,
    patchRecords: opt.patchRecords ?? objects.some((o) => isPatchKey(o.id)),
    ...(opt.notes?.length ? { notes: opt.notes } : {}),
    ...(problems.length ? { problems } : {}),
    objects: ordered.map((o) => ({ ...wire(o), size: o.bytes.length, crc: o.bytes.length ? bkCrc(o.bytes) : 0, data: base64(o.bytes), ...(o.raw ? { raw: true, problem: o.problem || "not checked" } : {}) })),
  };
}
// Parse and check a whole file before anything is sent. Returns { version, source, firmware, created, engines, objects: Map key -> {id, size, crc, bytes}, problems, notes,
// hasPatches (the file carries the patches as records: version 5 made by a unit with them), oldPatchChunks (the physical chunks a version 1..4 file holds: kept out, never restored) }.
export function readArchive(file) {
  if (typeof file === "string") { try { file = JSON.parse(file); } catch { throw new Error("This is not a backup file (it is not valid JSON)."); } }
  if (!file || typeof file !== "object" || !Array.isArray(file.objects)) throw new Error("This is not a DUELL backup file.");
  let source, items, version;
  if (file.format === FORMAT) {
    if (!Number.isInteger(file.version) || file.version < 2) throw new Error("This backup has an unknown version.");
    if (file.version > VERSION) throw new Error(`This backup is version ${file.version}; this editor reads up to version ${VERSION}. Use a newer editor.`);
    version = file.version; source = FORMAT; items = file.objects;
    if (version < 4 && items.some((o) => o && [27, 28, 29, 30, 31, 35, 36, 37, 38, 39].includes(o.id))) throw new Error("This backup holds patch chunks a version 3 file does not have: it is damaged.");
    if (version >= 5 && items.some((o) => o && OLD_PATCH_IDS.includes(o.id))) throw new Error(`This backup is version ${version}, which carries patches as records, but it holds patch chunks (ids 6, 7, 9 to 12, 27 to 31, 35 to 39): it is damaged.`);
    if (version < 5 && items.some((o) => o && o.id === PATCH_ID)) throw new Error(`This backup is version ${version}, which has no patch records (id ${PATCH_ID}): it is damaged.`);
    if (version < 6 && items.some((o) => o && MULTI2_IDS.includes(o.id))) throw new Error(`This backup is version ${version}, which has no multis M17 and up (ids 42 to 46): it is damaged.`);
  } else if (file.format === "felucca-backup" && file.version === 1) {
    if (!/DUELL/i.test(String(file.firmware || ""))) throw new Error("This is a Felucca backup, not a DUELL one: its objects mean other things. It was not read.");
    items = readBackup(file).objects;                  // the installer's file: its own checks (ids, sizes, CRC); the retired objects are dropped below
    version = 1; source = "felucca-backup v1";
  } else throw new Error("This is not a DUELL backup file.");
  const objects = new Map(); let oldPatchChunks = 0;
  for (const o of items) {
    if (!o || !Number.isInteger(o.id)) throw new Error("A backup object has no id.");
    const patch = o.id === PATCH_ID;
    if (patch && !(Number.isInteger(o.slot) && o.slot >= 0 && o.slot < PATCH_SLOTS)) throw new Error("A patch object has no valid slot.");
    const key = patch ? patchKey(o.slot) : o.id, name = patch ? `Patch ${objectLabel(key).slice(6)}` : `Object ${o.id}`;   // (the message names)
    if (!patch && !KNOWN_IDS.includes(o.id)) throw new Error(`Unknown object id ${o.id} in this backup.`);
    const old = OLD_PATCH_IDS.includes(o.id);
    if (!patch && !old && !DUELL_IDS.includes(o.id) && o.id !== FVB_ID) {   // a retired object: must be empty, then it is left out
      if (o.size) throw new Error(`Object ${o.id} is retired in DUELL and holds data: this is not a DUELL backup.`);
      continue;
    }
    if (objects.has(key)) throw new Error(patch ? `${name} is in the file twice.` : `Object ${o.id} is in the file twice.`);
    let bytes = o.bytes;
    if (!bytes) {
      if (!Number.isInteger(o.size) || o.size < 0 || o.size > MAX_OBJ || !Number.isInteger(o.crc) || o.crc < 0 || o.crc > 0xffffffff ||
          typeof o.data !== "string" || o.data.length !== 4 * Math.ceil(o.size / 3) || !B64.test(o.data)) throw new Error(`${name} is damaged.`);
      bytes = Uint8Array.from(atob(o.data), (c) => c.charCodeAt(0));
      if (bytes.length !== o.size || (o.size ? bkCrc(bytes) : 0) !== o.crc) throw new Error(`${name} failed its checksum: the file is damaged.`);
    }
    if (old) { if (bytes.length) oldPatchChunks++; continue; }   // a physical chunk of the old store: read (its checksum held), never restored
    if (o.raw === true) {                              // 0.2.25: an object the backup could not check, carried as it was read (a restore skips it unless asked)
      if (patch && bytes.length > PATCH_MAX) throw new Error(`${name} is damaged.`);
      objects.set(key, { id: key, size: bytes.length, crc: bytes.length ? bkCrc(bytes) : 0, bytes, raw: true, problem: String(o.problem || "not checked") });
      continue;
    }
    const why = checkObject(key, bytes);
    if (why) throw new Error(`${name}: ${why}.`);
    objects.set(key, { id: key, size: bytes.length, crc: bytes.length ? bkCrc(bytes) : 0, bytes });
  }
  for (const g of GROUPS) {                            // a group is whole or absent (v1 files lack the multis and patterns); the patches are any subset
    if (g.dynamic) continue;
    const n = g.ids.filter((id) => objects.has(id)).length;
    if (n && n !== g.ids.length) throw new Error(`The ${g.label.toLowerCase()} in this file are incomplete.`);
  }
  if (!objects.has(0) || !objects.has(1) || (!objects.get(0).size && !objects.get(0).raw) || (!objects.get(1).size && !objects.get(1).raw)) throw new Error("This backup has no current music or no settings: it is not complete.");
  const problems = [...objects.values()].filter((o) => o.raw).map((o) => ({ id: o.id, problem: o.problem }));
  const hasPatches = version >= 5 && (file.patchRecords === true || [...objects.keys()].some(isPatchKey));
  const notes = [...(Array.isArray(file.notes) ? file.notes.map(String) : []), ...(oldPatchChunks ? [OLD_CHUNKS_NOTE] : [])];
  return { version, source, firmware: String(file.firmware || ""), created: String(file.created || ""),
           engines: Array.isArray(file.engines) ? file.engines.map(String) : [], objects, problems, notes, hasPatches, oldPatchChunks };
}

// ---- the unit
const NOTES = { 1: "The unit refused the request", 2: "The unit found the data invalid", 3: "The unit is playing. Stop playback on the unit and try again",
                4: "The unit could not write its flash memory", 5: "The backup session on the unit expired. Start again",
                6: "The unit's patch store is full (STORE FULL): delete a saved patch on the unit, then try again" };
const rcCheck = (rc) => { if (rc) throw Object.assign(new Error(NOTES[rc] || `The unit answered error ${rc}`), { rc }); };
export const CHUNK = 256;
// LIST -> Map id -> {size, crc}; only the ids a DUELL backup holds are kept (the patches are not in it: LIST page)
export function parseList(a) {
  if (a[0] !== 1) throw new Error("Unexpected answer from the unit");
  rcCheck(a[1]);
  const n = a[2];
  if (a.length !== 3 + n * 11) throw new Error("The unit sent an incomplete list");
  const m = new Map();
  for (let i = 0; i < n; i++) {
    const p = 3 + i * 11, id = a[p];
    if (!KNOWN_IDS.includes(id)) throw new Error(`The unit lists an object (${id}) this editor does not know. Use a newer editor.`);
    const size = bkR32(a, p + 1), crc = bkR32(a, p + 6);
    if (size > MAX_OBJ || (!size && crc)) throw new Error("The unit lists an object that is too large");
    if (DUELL_IDS.includes(id)) m.set(id, { size, crc });
  }
  return m;
}
// LIST page (1..4) -> Map patch key -> {size, crc}: per saved patch of the page's 32 slots, slot (the id byte), size u32, crc u32, in slot order
export function parsePage(a, page) {
  if (a[0] !== 1) throw new Error("Unexpected answer from the unit");
  rcCheck(a[1]);
  const n = a[2];
  if (n > PATCH_PAGE || a.length !== 3 + n * 11) throw new Error("The unit sent an incomplete list of patches");
  const m = new Map(); let last = -1;
  for (let i = 0; i < n; i++) {
    const p = 3 + i * 11, slot = a[p], size = bkR32(a, p + 1), crc = bkR32(a, p + 6);
    if (slot < PATCH_PAGE * (page - 1) || slot >= PATCH_PAGE * page || slot <= last) throw new Error(`The unit lists a patch (slot ${slot}) that does not belong on page ${page}.`);
    if (size < REC_FIX || size > PATCH_MAX) throw new Error("The unit lists a patch record of a size this editor does not know. Use a newer editor.");
    m.set(patchKey(slot), { size, crc }); last = slot;
  }
  return m;
}
// the plain LIST, and with caps bit 5 the four pages of patches merged into the same Map under their keys (the pages need the plain LIST before)
export async function listUnit(request, caps = null) {
  const list = parseList(await request([BACKUP_CMD.LIST, []], { timeout: 4000, retries: 0 }));
  if ((caps ?? 0) & CAPS_PATCH_RECORDS)
    for (let page = 1; page <= PATCH_PAGES; page++)
      for (const [k, v] of parsePage(await request([BACKUP_CMD.LIST, [page]], { timeout: 4000, retries: 0 }), page)) list.set(k, v);
  return list;
}

// GET 41 (not in the plain LIST, so the LIST before is needed): does the unit hold an FM6 voice bank? The probe reads one byte; rc 1 = no bank. A playing unit stops the backup, as for any object.
async function hasBank(request) {
  const a = await request([BACKUP_CMD.GET, [FVB_ID, ...bkU32(0), 1, 0]], { timeout: 1500, retries: 1 });
  if (a[1] === 3) throw Object.assign(new Error(NOTES[3]), { rc: 3, fatal: true });
  return a[1] === 0;
}
// the bank on the unit now, whole (null: none); used by the verify
async function readBank(request) {
  if (!await hasBank(request)) return null;
  const bytes = new Uint8Array(FVB_OBJ);
  for (let off = 0; off < FVB_OBJ; off += CHUNK) {
    const n = Math.min(CHUNK, FVB_OBJ - off), a = await request([BACKUP_CMD.GET, [FVB_ID, ...bkU32(off), n & 127, n >>> 7]], { timeout: 1500, retries: 1 });
    rcCheck(a[1]);
    if (a[0] !== FVB_ID || bkR32(a, 2) !== off || (a[7] | a[8] << 7) !== n) throw new Error("Unexpected answer from the unit");
    bytes.set(bkUnpack(a.slice(9), n), off);
  }
  return bytes;
}
// Write a bank the way the unit's own bank upload does (FM6B_BEGIN, one WRITE a voice, END with the name and the CRC-32 of the records in index order). Allow a second for BEGIN and END (flash).
async function restoreBank(request, bytes) {
  const { name, voices } = bankVoices(bytes), recs = [];
  rcCheck((await request([FM6B_CMD.BEGIN, []], { timeout: 6000, retries: 0 }))[0]);
  for (const v of voices) {
    const a = await request([FM6B_CMD.WRITE, [v.index, ...v.pk]], { timeout: 3000, retries: 0 });
    if (a[0] !== v.index) throw new Error("Unexpected answer from the unit");
    rcCheck(a[1]); recs.push(v.pk);
  }
  const all = new Uint8Array(recs.length * FVB_PK); recs.forEach((r, i) => all.set(r, i * FVB_PK));
  const nm = [...name.padEnd(10, " ").slice(0, 10)].map((c) => c.charCodeAt(0) & 127);
  rcCheck((await request([FM6B_CMD.END, [...nm, ...bkU32(all.length ? bkCrc(all) : 0)]], { timeout: 6000, retries: 0 }))[0]);
}

// Read everything the unit lists (multis and patterns only when its firmware has them; the patches one by one, from the pages, when it has caps bit 5). 0.2.25 (owner: one multi
// object the editor did not accept stopped the whole backup): an object that cannot be read whole, changes while it is read or fails the editor's check is NOT a reason to
// stop. Its bytes (as far as they were read) go into the file flagged raw, with the reason in the file's `problems`; everything else is saved
// as usual. A restore skips a raw object unless the user asks for it (planRestore opt.includeRaw). Only a refused LIST stops the backup.
export async function captureBackup(request, info, onProgress = () => {}) {
  const list = await listUnit(request, info.caps), ids = [...DUELL_IDS.filter((id) => list.has(id)), ...[...list.keys()].filter(isPatchKey).sort((a, b) => a - b)];
  if (!list.has(0) || !list.has(1)) throw new Error("The unit does not list its music and settings: is it running DUELL?");
  if (info.fvBank && await hasBank(request)) { list.set(FVB_ID, { size: FVB_OBJ, crc: 0 }); ids.splice(ids.indexOf(0), 0, FVB_ID); }   // the voice bank is not in the plain LIST; its own CRC is its last four bytes
  const total = ids.reduce((n, id) => n + list.get(id).size, 0); let done = 0; const got = [];
  const readOne = async (id, size) => {
    const bytes = new Uint8Array(size), patch = isPatchKey(id), wire = patch ? PATCH_ID : id;
    for (let off = 0; off < size; off += CHUNK) {
      const n = Math.min(CHUNK, size - off);
      const a = await request([BACKUP_CMD.GET, [wire, ...bkU32(off), n & 127, n >>> 7, ...(patch ? [patchSlot(id)] : [])]], { timeout: 1500, retries: 1 });
      if (a[1] === 3) throw Object.assign(new Error(NOTES[3]), { rc: 3, fatal: true });   // (playing: nothing more can be read)
      rcCheck(a[1]);
      if (a[0] !== wire || bkR32(a, 2) !== off || (a[7] | a[8] << 7) !== n) throw new Error("Unexpected answer from the unit");
      bytes.set(bkUnpack(a.slice(9), n), off);
      done += n; onProgress(Math.min(done, total), total);
    }
    return bytes;
  };
  for (const id of ids) {
    const { size, crc } = list.get(id);
    let bytes = null, problem = null;
    try {
      bytes = await readOne(id, size);
      if (id !== FVB_ID && (size ? bkCrc(bytes) : 0) !== crc) problem = "it changed on the unit while it was read (checksum)";
      else { const why = checkObject(id, bytes); if (why) problem = `this editor does not recognise it (${why})`; }
    } catch (e) {
      if (e.fatal) throw e;
      problem = `it could not be read (${e.message})`; bytes = bytes || new Uint8Array(0);
    }
    got.push(problem ? { id, bytes: bytes || new Uint8Array(0), raw: true, problem } : { id, bytes });
  }
  if (got.every((o) => o.raw)) throw new Error(`Nothing could be read from the unit: ${got[0]?.problem ?? "no answer"}.`);
  const records = ((info.caps ?? 0) & CAPS_PATCH_RECORDS) !== 0;
  const file = objectsToFile(got, info, { patchRecords: records, notes: records ? [] : ["This unit's firmware cannot back up patches one at a time (it needs DUELL with INFO caps bit 5): this file has no patches."] });
  readArchive(file);                                   // what was read must pass the same checks a restore applies (raw objects are carried as they are)
  return file;
}
// the lines that tell the user which objects are in a file only as raw data
export const rawLines = (problems, objects = null) => (problems || []).map((p) => `Object ${p.id} (${objectLabel(p.id, objects?.get(p.id)?.bytes)}) could not be checked: ${p.problem}. It is saved as raw data; a restore skips it unless you ask for it.`);

// What a restore would do. sel: Set of group keys. list: listUnit(request, caps) now (the patches included). caps: INFO caps (null unknown). Returns
// { steps: [{id, size, crc, bytes, group}], excluded: [{key, label, reason}], skipped: [keys of the patches the unit holds as the file does], caps }; nothing is sent. A patch step is
// {id: 40, slot, key, size, crc, bytes, group: "patches"}: a PUT of the record, or (size 0) of nothing, which removes the slot's record. The patches are made EXACTLY the file's: one PUT
// for each patch whose size or CRC differs on the unit (an identical one is not written again) and a removal for each slot the unit has saved and the file has not.
// opt.includeRaw (0.2.25): also send the objects the backup could not check (the unit checks them at commit and refuses what it cannot load).
// opt.tide (INFO tag 44 01, info.tideAnswer): true = the unit says it is DUELL with TIDE: a patch's FM6 MORPH TO target (End B flag 4) is kept. false (tag 44 present, bit 0 clear) or undefined (no tag 44: plain DUELL 1.1 and the TIDE builds before the tag look the same) = the target is taken out before the PUT (patchrecord.js stripEndBFmt; plain DUELL 1.1 would drop the whole record, D02); plan.stripped counts those patches, the confirm text says so.
// opt.patchCap (INFO tag 58 01, the patch records the store takes: 126 with TIDE): a file with more patches than that cannot be restored whole, so the patches are left out
// with that reason and the other groups still go (D05). The removals of the slots the file does not have come BEFORE the patches that are written, so a nearly full store has the room.
// A multi object whose records are of another version than this editor's firmware writes is left out unless includeRaw (the unit would refuse it).
export function planRestore(archive, list, caps, sel, opt = {}) {
  const steps = [], excluded = [], skipped = [];
  let stripped = 0;
  for (const g of GROUPS) {
    if (!sel.has(g.key) || (g.optional && !g.ids.every((id) => archive.objects.has(id)))) continue;
    let reason = null;
    if (g.key === "patches") {
      if (!archive.hasPatches) reason = archive.oldPatchChunks ? OLD_CHUNKS_NOTE.replace(/^P/, "p").replace(/\.$/, "") : "not in this file";
      else if (!((caps ?? 0) & g.caps)) reason = "this unit's firmware cannot restore patches one at a time (update it first)";
      else if (opt.patchCap && idsOf(g, archive.objects).length > opt.patchCap) reason = `the file holds ${idsOf(g, archive.objects).length} patches and this unit stores at most ${opt.patchCap} (STORE FULL); remove some from the file or restore them later in parts`;
    } else if (g.caps && ((caps ?? 0) & g.caps) !== g.caps) reason = "this unit's firmware cannot restore them (update it first)";
    else if (!g.ids.every((id) => archive.objects.has(id))) reason = "not in this file";
    else if (g.needs && !opt[g.needs]) reason = "this unit's firmware cannot restore it (it needs DUELL with TIDE, a build with the FM6 voice bank in its backup)";
    else if ((g.key === "settings" || g.key === "music") && list.get(g.ids[0])?.size !== archive.objects.get(g.ids[0]).size) reason = "the file was made by another DUELL version (its size differs from the unit's)";
    if (reason) { excluded.push({ key: g.key, label: g.label, reason }); continue; }
    if (g.dynamic) {
      const inFile = idsOf(g, archive.objects);
      for (const key of inFile) {
        const o = archive.objects.get(key), label = `${g.label}: ${objectLabel(key, o.bytes)}`;
        if (o.raw && (!opt.includeRaw || !o.size)) { excluded.push({ key: g.key, id: key, label, reason: `it could not be checked when the backup was made (${o.problem}); the unit keeps what it holds there` }); continue; }
        let { size, crc, bytes } = o, cut = false;
        if (opt.tide !== true && !o.raw) { const t = stripEndBFmt(bytes); if (t !== bytes) { bytes = t; size = t.length; crc = bkCrc(t); cut = true; } }   // (plain DUELL 1.1 would drop an FM6 MORPH TO target's record: D02)
        const now = list.get(key);
        if (now && now.size === size && now.crc === crc) { skipped.push(key); continue; }
        if (cut) stripped++;                           // counted only when it is written: a patch that is identical on the unit is skipped and loses nothing
        steps.push({ id: PATCH_ID, slot: patchSlot(key), key, size, crc, bytes, group: g.key });
      }
      for (const key of [...list.keys()].filter(isPatchKey))
        if (!archive.objects.has(key)) steps.push({ id: PATCH_ID, slot: patchSlot(key), key, size: 0, crc: 0, bytes: new Uint8Array(0), group: g.key });
      continue;
    }
    for (const id of g.ids) {
      const o = archive.objects.get(id);
      if (o.raw && !opt.includeRaw) { excluded.push({ key: g.key, id, label: `${g.label}: object ${id} (${objectLabel(id)})`, reason: `it could not be checked when the backup was made (${o.problem}); the unit keeps what it holds there` }); continue; }
      const old = (g.key === "multis" || g.key === "multis2") && !o.raw ? multiVersions(o.bytes).find((r) => r.version !== MUL_VER || r.np !== P_COUNT) : null;
      if (old && !opt.includeRaw) { excluded.push({ key: g.key, id, label: `${g.label}: object ${id} (${objectLabel(id)})`, reason: `a multi record of version ${old.version}, which this firmware does not load; the unit keeps what it holds there` }); continue; }
      steps.push({ id, size: o.size, crc: o.crc, bytes: o.bytes, group: g.key });
    }
  }
  const rank = (s) => (s.key !== undefined ? (s.size ? s.slot : s.slot - 200) : 200 + (s.id === FVB_ID ? RESTORE_ORDER.indexOf(1) - 0.5 : RESTORE_ORDER.indexOf(s.id)));   // the removals first (they free room), then the patches by slot, then the rest
  steps.sort((a, b) => rank(a) - rank(b));
  return { steps, excluded, skipped, caps, stripped };
}
// The confirm line for the patches that lose their FM6 MORPH TO target ("" when none). tideAnswer: true TIDE, false plain DUELL, undefined no tag 44 (unknown).
export function strippedNote(stripped, tideAnswer) {
  if (!stripped) return "";
  const n = stripped === 1 ? "1 PATCH LOSES" : `${stripped} PATCHES LOSE`;
  return `${n} the FM6 MORPH TO target ${tideAnswer === false ? "(this unit says it is plain DUELL, which has no such target)" : "(this unit does not say it is TIDE, so it may be plain DUELL or an older TIDE build; the target is taken out of the patches before they are written)"}.

`;
}
// Send the steps, one object at a time (begin, 256-byte pieces, commit: the unit checks the CRC and the content, then writes; the old copy
// stays valid until the new one is complete). A patch is a PUT of id 40 with its slot byte at the begin; every patch is its own atomic sequence. An object that fails (a full
// store, a flash error) is reported and the others still go (D05); only "the unit is playing" (rc 3) or no answer from the unit stops the run (`stopped`), as nothing more can be written then.
// Never aborts silently: the result says what was written (a patch by its key) and every failure: { written, failed (the first or null), failures: [{id, message, rc}], stopped }.
export async function runRestore(request, plan, onProgress = () => {}) {
  let stopped = false; const written = [], failures = [], total = plan.steps.reduce((n, s) => n + s.size, 0); let done = 0;
  const put = async (args) => { const a = await request([BACKUP_CMD.PUT, args], { timeout: 6000, retries: 0 }); rcCheck(a[2]); };
  for (const s of plan.steps) {
    const at = s.key ?? s.id;
    let failure = null;
    if (s.id === FVB_ID) {                             // the voice bank goes in through the bank upload commands, not through PUT
      try { await restoreBank(request, s.bytes); done += s.size; onProgress(done, total); written.push(at); }
      catch (e) { failures.push({ id: at, message: e.message, rc: e.rc }); if (e.rc === undefined || e.rc === 3) { stopped = true; break; } }
      continue;
    }
    try {
      await put([0, s.id, ...bkU32(s.size), ...bkU32(s.crc), ...(s.key !== undefined ? [s.slot] : [])]);
    } catch (e) { failure = { id: at, message: e.message, rc: e.rc }; }
    if (!failure) {
      try {
        for (let off = 0; off < s.size; off += CHUNK) {
          const piece = s.bytes.subarray(off, off + CHUNK);
          await put([1, s.id, ...bkU32(off), ...bkPack(piece)]);
          done += piece.length; onProgress(done, total);
        }
        await put([2, s.id]);
      } catch (e) {
        await put([3, s.id]).catch(() => {});
        failure = { id: at, message: e.message, rc: e.rc };
      }
    }
    if (failure) { failures.push(failure); if (failure.rc === undefined || failure.rc === 3) { stopped = true; break; } continue; }   // (playing, or no answer: nothing more can be written)
    written.push(at);
  }
  return { written, failed: failures[0] ?? null, failures, stopped };
}
// After a restore: a fresh LIST (the patch pages too). The patch, multi and pattern objects must equal the file's (size and CRC; a removed patch must be gone). The music and the settings are
// checked by the unit at commit but may read back in the unit's own normal form (an older record is brought up to date), so they are not compared.
export async function verifyRestore(request, plan) {
  const list = await listUnit(request, plan.caps), same = [], differ = [], notCompared = [];
  for (const s of plan.steps) {
    if (s.id === 0 || s.id === 1) { notCompared.push(s.id); continue; }
    if (s.id === FVB_ID) { const now = await readBank(request).catch(() => null); (now && now.length === s.bytes.length && now.every((v, i) => v === s.bytes[i]) ? same : differ).push(s.id); continue; }
    const at = s.key ?? s.id, now = list.get(at);
    ((s.key !== undefined && !s.size ? !now : now && now.size === s.size && now.crc === s.crc) ? same : differ).push(at);
  }
  return { same, differ, notCompared };
}
export const fileName = (prefix, d = new Date()) => `${prefix}-${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}-${String(d.getHours()).padStart(2, "0")}${String(d.getMinutes()).padStart(2, "0")}.json`;
