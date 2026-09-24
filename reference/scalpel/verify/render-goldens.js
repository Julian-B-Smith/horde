#!/usr/bin/env node
// render-goldens.js — render every preset in data/presets.json through the oracle to 32-bit float stereo WAVs,
// with a manifest (sha256, rms, peak, DC) for parity work. Output: ./goldens/ (not shipped: ~45 MB).
// Usage: node render-goldens.js [--seconds 1.5] [--only "Showcase / Pads"]
const fs = require('fs'), path = require('path'), crypto = require('crypto');
const RazorCore = require('../prototype/razor-core.js');
const {seed} = require('./rng.js');
const SR = 48000, mtof = n => 440*Math.pow(2, (n - 69)/12);
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const secs = parseFloat(arg('--seconds', '1.5')), only = arg('--only', null);
const presets = JSON.parse(fs.readFileSync(path.join(__dirname, '../data/presets.json'), 'utf8')).presets;
// register per category (MIDI notes, Ableton naming: 60 = C3)
const NOTES = {'Growls':[33], 'FM sines':[60,64,67], 'Movement':[48,55], 'Leads':[62], 'Pads':[48,55,60,64], 'Oddities':[52,59]};
const notesFor = cat => { for (const k in NOTES) if (cat.endsWith(k)) return NOTES[k]; return [45]; };
function wav(l, r){
  const n = l.length, buf = Buffer.alloc(44 + n*8);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n*8, 4); buf.write('WAVE', 8); buf.write('fmt ', 12);
  buf.writeUInt32LE(16, 16); buf.writeUInt16LE(3, 20); buf.writeUInt16LE(2, 22); buf.writeUInt32LE(SR, 24);
  buf.writeUInt32LE(SR*8, 28); buf.writeUInt16LE(8, 32); buf.writeUInt16LE(32, 34); buf.write('data', 36); buf.writeUInt32LE(n*8, 40);
  for (let i = 0; i < n; i++){ buf.writeFloatLE(l[i], 44 + i*8); buf.writeFloatLE(r[i], 48 + i*8); }
  return buf;
}
const outDir = path.join(__dirname, 'goldens'); fs.mkdirSync(outDir, {recursive:true});
const manifest = [];
for (const pr of presets){
  if (only && pr.category !== only) continue;
  seed(0xC0FFEE);
  const c = new RazorCore(SR); c.set(Object.assign({}, pr.params, {gain:.35})); Object.assign(c.s, c.t);
  const notes = notesFor(pr.category); for (const n of notes) c.noteOn(n, mtof(n), .85);
  const L = new Float32Array(128), R = new Float32Array(128), l = [], r = [];
  for (let b = 0; b < secs*SR/128; b++){ c.render(L, R); for (let i = 0; i < 128; i++){ l.push(L[i]); r.push(R[i]); } }
  const file = (pr.category + ' - ' + pr.name).replace(/[^\w\- ]+/g, '').replace(/\s+/g, '_') + '.wav';
  const data = wav(l, r); fs.writeFileSync(path.join(outDir, file), data);
  let e = 0, pk = 0, dc = 0; for (const v of l){ e += v*v; pk = Math.max(pk, Math.abs(v)); dc += v; }
  manifest.push({category:pr.category, name:pr.name, file, notes, seconds:secs, seed:'0xC0FFEE',
    sha256:crypto.createHash('sha256').update(data).digest('hex'), rms:+Math.sqrt(e/l.length).toFixed(5), peak:+pk.toFixed(5), dc:+(dc/l.length).toFixed(5)});
  process.stdout.write('.');
}
fs.writeFileSync(path.join(outDir, 'manifest.json'), JSON.stringify(manifest, null, 1));
console.log(`\n${manifest.length} goldens → ${outDir}`);
