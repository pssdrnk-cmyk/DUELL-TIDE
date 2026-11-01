// SPDX-License-Identifier: GPL-3.0-only
// Optional check of the actual packaged firmware with Joakim Eriksson's MIT fm1-emu.
// Usage and pinned emulator download: docs/design/FM1_ECOSYSTEM_REVIEW.md
import fs from 'node:fs';
import crypto from 'node:crypto';
const [emulator, packageFile = 'felucca-base/build/duell-1.0.fwsc'] = process.argv.slice(2);
if (!emulator) {
  console.error('Usage: node jam/package_smoke.mjs path/to/fm1_web.wasm [firmware.fwsc]');
  process.exit(2);
}
const wasm = fs.readFileSync(emulator), pkg = fs.readFileSync(packageFile);
const emulatorSha = crypto.createHash('sha256').update(wasm).digest('hex');
if (emulatorSha !== '0f9e446c4be424221f7159f17e6460b6fcebb92f38221bad0a781999ed051778')
  throw Error('Unexpected emulator build; review it before updating the pinned hash');
let x, compiled = 0;
const module = await WebAssembly.compile(wasm);
console.log('CPU emulator SHA256', crypto.createHash('sha256').update(wasm).digest('hex'));
console.log('Package SHA256', crypto.createHash('sha256').update(pkg).digest('hex'));
x = (await WebAssembly.instantiate(module, {env:{fm1_jit_compile(ptr,len){
  const bytes = new Uint8Array(x.memory.buffer,ptr,len).slice();
  const table=x.__indirect_function_table;
  const run=new WebAssembly.Instance(new WebAssembly.Module(bytes),{env:{memory:x.memory,table}}).exports.run;
  const slot=table.grow(1);table.set(slot,run);compiled++;return slot;
}}})).exports;
x.fm1_jit(1,16); x.fm1_ahead?.(1024);
const ptr=x.fm1_alloc(pkg.length);new Uint8Array(x.memory.buffer,ptr,pkg.length).set(pkg);
if(x.fm1_boot_fwsc(ptr,pkg.length,0,0,100)!==0)throw Error('Package rejected');
x.fm1_pot(800);
let peak=0, frames=0;
function advance(until){
  let calls=0;
  while(x.fm1_ms()<until){
    const count=x.fm1_render(128,2_000_000);frames+=count;
    for(const v of new Float32Array(x.memory.buffer,x.fm1_out_l(),count)){
      if(!Number.isFinite(v))throw Error('Nonfinite audio');peak=Math.max(peak,Math.abs(v));
    }
    if(x.fm1_halted())throw Error('Packaged firmware halted at '+x.fm1_ms()+'ms');
    if(++calls>40000)throw Error('No boot progress');
  }
  console.log('Emulated ms',x.fm1_ms(),'audio frames',frames,'peak',peak,'JIT blocks',compiled);
}
advance(2400);
const lcd = new Uint16Array(x.memory.buffer,x.fm1_lcd(),240*240);
if(new Set(lcd).size<3)throw Error('No screen rendered');
peak=0;
for(const b of [0x90,60,96])x.fm1_midi(b);
advance(2900);
if(!(peak>0&&peak<=1))throw Error('No bounded note output');
for(const b of [0x80,60,0])x.fm1_midi(b);
advance(3300);
console.log('PACKAGED FIRMWARE: boot, LCD, MIDI note on/off and finite audio PASS');
