// Swarmalator golden generator (new engine): render the JS Swarmalator core
// (sliced live from reference/swarmalator.html) to raw f32 STEREO for
// tools/swarmalator_check.cpp L0-1-style parity. Fully deterministic (seeded
// mulberry32, no Math.random in the audio path) — so parity is bit-exact.
//
// Call sequence replicated verbatim by the C++ oracle: construct → setParam in
// scenario order → noteOn(57, 220 Hz) → render. Order matters (setParam
// rebuilds on nu/detune/drift/seed), so the manifest preserves param order.
//
// Usage: node gen_swarmalator_goldens.mjs [--selfcheck]

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const html = readFileSync(join(root, 'reference/swarmalator.html'), 'utf8').split('\n');
const start = html.findIndex(l => l.includes('const NU_MAX'));
const end = html.findIndex((l, i) => i > start && (/^\s*const \$ =/.test(l) || l.includes('function powerOn()')));
if (start < 0 || end <= start) throw new Error('reference/swarmalator.html: core slice markers not found');
const Swarmalator = new Function(html.slice(start, end).join('\n') + '\nreturn Swarmalator;')();

const SR = 44100, BLOCK = 512, SECONDS = 1;
const NOTE_M = 57, NOTE_F = 440 * Math.pow(2, (NOTE_M - 69) / 12);  // A3 = 220 Hz (spec anchor)

export const SCENARIOS = [
  { name: 'swm-sync', p: { K: 0.9, J: 0 } },
  { name: 'swm-splay', p: { K: -0.9, J: 0 } },
  { name: 'swm-rainbow', p: { K: 0, J: 0.9 } },
  { name: 'swm-sync-rainbow', p: { K: 0.6, J: 0.9 } },
  { name: 'swm-active-wave', p: { K: -0.5, J: 0.9 } },
  { name: 'swm-stability', p: { K: 1, J: 1, drift: 1 } },
  { name: 'swm-sine', p: { kernel: 1, K: 0.6, J: 0.9 } },
  { name: 'swm-detune-seed', p: { detune: 0.8, seed: 777, K: 0.9, J: 0.4 } },
  { name: 'swm-units18', p: { nu: 18, K: 0.5, J: 0.7 } },
];

function renderScenario(sc) {
  const s = new Swarmalator(SR);
  for (const [k, v] of Object.entries(sc.p)) s.setParam(k, v);
  s.noteOn(NOTE_M, NOTE_F);
  const total = SR * SECONDS;
  const out = new Float32Array(total * 2);  // interleaved L/R
  const L = new Float32Array(BLOCK), R = new Float32Array(BLOCK);
  for (let off = 0; off < total; off += BLOCK) {
    s.render(L, R);
    for (let i = 0; i < BLOCK && off + i < total; i++) {
      out[(off + i) * 2] = L[i];
      out[(off + i) * 2 + 1] = R[i];
    }
  }
  return out;
}

const outDir = join(root, 'build-golden', 'swarmalator');
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
    manifest.push([sc.name, Object.entries(sc.p).map(([k, v]) => `${k}=${v}`).join(' ')].join('\t'));
    console.log(`wrote ${sc.name}.f32`);
  }
}
if (selfcheck) {
  console.log(fail ? `selfcheck: ${fail} NON-DETERMINISTIC` : 'selfcheck: renders are deterministic');
  process.exit(fail ? 1 : 0);
}
writeFileSync(join(outDir, 'swarmalator-manifest.tsv'), manifest.join('\n') + '\n');
