// usage: node dx7_js_parse.mjs OUTFILE FILE.syx ...   (runs the editor's own FM6.parseSysex, web/editor/fm6voice.js)
import { readFileSync, writeFileSync } from "node:fs";
import { FM6 } from "../web/editor/fm6voice.js";
const [outp, ...files] = process.argv.slice(2);
const lines = [];
for (const f of files) {
  const r = FM6.parseSysex(new Uint8Array(readFileSync(f)));
  lines.push(`#${f}\tvoices=${r.voices.length}\tbadSum=${r.badSum}\tshort=${r.short}\tskipped=${r.skipped}`);
  r.voices.forEach((x, k) => {
    const pk = FM6.pack(x.v), back = FM6.unpack(pk);                 // what the editor sends to the unit, and what the device reads back
    lines.push(`${k + 1}\t${Buffer.from(x.v).toString("hex")}\t${Buffer.from(back).toString("hex")}`);
  });
}
writeFileSync(outp, lines.join("\n") + "\n");
