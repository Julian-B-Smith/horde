// SPECTRA golden generator (Phase 4): render SpectraSynth (sliced live from
// reference/swarmspectra.html — the HTML stays the source of truth) and write raw f32
// stereo streams for tools/spectra_check.cpp L0-1-style parity.
//
// Protocol: params applied via setParam in manifest order; one note
// (midi 57, 440*2^((57-69)/12) Hz) at t=0; 1 s rendered in 512-sample
// blocks (block size matters only in that BOTH sides must use the same —
// the 16-sample control cadence is carried across blocks by the tick
// counter). Output: build-golden/spectra/<name>.f32 (interleaved LR) +
// spectra-manifest.tsv. --selfcheck renders twice and compares.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const html = readFileSync(join(root, 'reference/swarmspectra.html'), 'utf8').split('\n');
const start = html.findIndex(l => l.includes('const P_MAX'));
const end = html.findIndex(l => l.includes('= Audio graph'));
if (start < 0 || end <= start) throw new Error('reference/swarmspectra.html: slice markers not found');
const SpectraSynth = new Function(html.slice(start, end).join('\n') + '\nreturn SpectraSynth;')();

const SR = 44100, BLOCK = 512, SECONDS = 1;
const NOTE_M = 57, NOTE_F = 440 * Math.pow(2, (NOTE_M - 69) / 12);

export const SCENARIOS = [
  { name: 'spectra-default', p: {} },
  { name: 'spectra-K09', p: { K: 0.9 } },
  { name: 'spectra-interfere', p: { K: -1 } },
  { name: 'spectra-cascade1', p: { K: 0.9, cascade: 1 } },
  { name: 'spectra-stretch', p: { stretch: 0.7, K: 0.5 } },
  { name: 'spectra-wtilt-neg', p: { wtilt: -0.8, cwidth: 0.6 } },
  { name: 'spectra-hzlaw', p: { wlaw: 1, cwidth: 0.8 } },
  { name: 'spectra-freephase-777', p: { retrig: 0, seed: 777, K: 0.4 } },
  { name: 'spectra-P1-cloud7', p: { partials: 1, cloud: 7, cwidth: 0.5 } },
];

function renderScenario(sc) {
  const s = new SpectraSynth(SR);
  for (const [k, v] of Object.entries(sc.p)) s.setParam(k, v);
  s.noteOn(NOTE_M, NOTE_F);
  const total = SR * SECONDS;
  const out = new Float32Array(total * 2);
  const L = new Float32Array(BLOCK), R = new Float32Array(BLOCK);
  for (let off = 0; off < total; off += BLOCK) {
    s.render(L, R);
    for (let i = 0; i < BLOCK; i++) { out[(off + i) * 2] = L[i]; out[(off + i) * 2 + 1] = R[i]; }
  }
  return out;
}

const outDir = join(root, 'build-golden', 'spectra');
const selfcheck = process.argv.includes('--selfcheck');
if (!selfcheck) mkdirSync(outDir, { recursive: true });
let fail = 0;
const manifest = [];
for (const sc of SCENARIOS) {
  const audio = renderScenario(sc);
  if (selfcheck) {
    const again = renderScenario(sc);
    let same = again.length === audio.length;
    if (same) for (let i = 0; i < audio.length; i++) if (audio[i] !== again[i]) { same = false; break; }
    console.log(`${same ? 'OK  ' : 'FAIL'} ${sc.name}`);
    if (!same) fail++;
  } else {
    writeFileSync(join(outDir, `${sc.name}.f32`), Buffer.from(audio.buffer));
    manifest.push([sc.name, Object.entries(sc.p).map(([k, v]) => `${k}=${v}`).join(' ') || '-'].join('\t'));
    console.log(`wrote ${sc.name}.f32`);
  }
}
if (selfcheck) {
  console.log(fail ? `selfcheck: ${fail} NON-DETERMINISTIC` : 'selfcheck: renders are deterministic');
  process.exit(fail ? 1 : 0);
}
writeFileSync(join(outDir, 'spectra-manifest.tsv'), manifest.join('\n') + '\n');
