// SPDX-License-Identifier: GPL-3.0-only
// DUELL dub pack (helper): the stored records render.c wrote (records.txt: "slot hex") -> the editor's "duell-patches" files, checked with the editor's own reader.
//   node tools/dub_pack/assemble.mjs RECORDS_TXT PATTERNS_JSON OUTDIR
// Writes OUTDIR/dub-voices.duell-patches.json (the 8 voices), OUTDIR/patches/NN-NAME.duell-patches.json (one file each) and
// OUTDIR/dub-patterns.duell-patterns.json (the 4 pattern slots, the editor's pattern file; patterns.json from pack.py). The slot number in a voice file
// is left out: you choose where a patch goes when you import it. The pattern file's slots are P01..P04 and are as well only a suggestion.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { itemFromRecord, buildFile, stringifyFile, parseFile } from "../../web/editor/patchfile.js";
import { parseRecord, recordProblem, describeRecord } from "../../web/editor/patchrecord.js";
import { encodePattern, decodePattern, patternFile, parsePatternFile } from "../../web/editor/stored.js";
import { recordProblem as patternProblem } from "../../web/editor/patslot.js";

const [, , recs, pats, out] = process.argv;
if (!recs || !pats || !out) { console.error("usage: assemble.mjs RECORDS_TXT PATTERNS_JSON OUTDIR"); process.exit(2); }
const items = readFileSync(recs, "utf8").split("\n").filter(Boolean).map((l) => {
  const [, hex] = l.split(" ");
  const rec = Uint8Array.from(hex.match(/../g).map((h) => parseInt(h, 16)));
  const bad = recordProblem(rec);
  if (bad) throw new Error(`record problem: ${bad}`);
  const it = itemFromRecord(rec, { source: "dub pack" });
  if (it.error) throw new Error(it.error);
  return it;
});
const FW = "DUELL-TIDE host store (dub pack)";
const SRC = "dub pack 2026-11-03";
mkdirSync(`${out}/patches`, { recursive: true });
const put = (name, its) => {
  const text = stringifyFile(buildFile(its, { firmware: FW, source: SRC }));
  const back = parseFile(text);
  if (!back.ok || back.problems.length || back.items.length !== its.length) throw new Error(`${name}: the editor's reader refuses it: ${back.error ?? back.problems.join("; ")}`);
  back.items.forEach((b, i) => { if (b.name !== its[i].name || b.crc !== its[i].crc) throw new Error(`${name}: ${b.name} differs after reading`); });
  writeFileSync(`${out}/${name}`, text, { encoding: "utf8" });
};
put("dub-voices.duell-patches.json", items);
items.forEach((it, i) => put(`patches/${String(i + 1).padStart(2, "0")}-${it.name.replace(/ /g, "-")}.duell-patches.json`, [it]));
for (const it of items) {
  const r = parseRecord(it.record), d = describeRecord(r);
  console.log(`${it.name.padEnd(12)} ${it.engineName.padEnd(7)} ${it.catName.padEnd(6)} ${String(it.record.length).padStart(4)} B  ${d.carries ? [].concat(d.carries).join(",") : ""}`);
}
console.log(`${items.length} patches -> ${out}`);
// the pattern slots
const pj = JSON.parse(readFileSync(pats, "utf8"));
const list = pj.map((p, i) => ({ slot: i, bytes: encodePattern(p) }));
for (const { bytes } of list) { const bad = patternProblem(bytes); if (bad) throw new Error(`pattern: ${bad}`); }
const ptext = JSON.stringify(patternFile(list, FW), null, 1) + "\n";
const pback = parsePatternFile(ptext);
if (!pback.ok || pback.patterns.length !== list.length || pback.patterns.some((p) => !p.bytes)) throw new Error("the editor's pattern reader refuses the file");
pback.patterns.forEach((p, i) => { const d = decodePattern(p.bytes); console.log(`${p.name.padEnd(12)} pattern LEN ${d.len} DIV ${d.div} SWG ${d.swing} GATE ${d.gate}  ${d.steps.filter((s) => s.time === 0 && s.n).length} hits`); });
writeFileSync(`${out}/dub-patterns.duell-patterns.json`, ptext, { encoding: "utf8" });
