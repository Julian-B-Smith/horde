// Filter (resonator bank) golden generator (Track E1): render the JS FilterLab
// (sliced live from reference/swarmfilter.html) to raw f32 for tools/filter_check.cpp
// L0-1-style audio parity.
//
// Protocol: noise = 0 for EVERY scenario — FilterLab's exciter mixes
// Math.random() dither when noise>0, which is non-deterministic and cannot be
// bit-checked (see filter_core.h header). One held A3 (midi 57) at t=0; 1 s
// rendered in 512-sample blocks (the 16-sample control tick is carried across
// blocks by the internal counter). Params applied in manifest order, then
// 'seed' LAST so pre-run state is a pure function of the final values.
//
// Usage: node gen_filter_goldens.mjs [--selfcheck]

import { FilterLab } from './extract_force.mjs';
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const SR = 44100, BLOCK = 512, SECONDS = 1;
const NOTE_M = 57, NOTE_F = 440 * Math.pow(2, (NOTE_M - 69) / 12);

// noise:0 pinned in every scenario; the rest exercise placement, distributions,
// sync/splay/gravity, inertia, drift, and the collapse→Q routing.
export const SCENARIOS = [
  { name: 'filter-dry', p: { noise: 0, wet: 0 } },
  { name: 'filter-free', p: { noise: 0 } },
  { name: 'filter-sync', p: { noise: 0, K: 1 } },
  { name: 'filter-splay', p: { noise: 0, K: -1 } },
  { name: 'filter-grav', p: { noise: 0, grav: 0.8, basin: 45 } },
  { name: 'filter-logplace', p: { noise: 0, place: 1, K: 0.6 } },
  { name: 'filter-harmonic', p: { noise: 0, place: 2, K: 0.8 } },
  { name: 'filter-gauss', p: { noise: 0, dist: 1, K: 1 } },
  { name: 'filter-cauchy-777', p: { noise: 0, dist: 2, seed: 777, K: -1 } },
  { name: 'filter-inertia-drift', p: { noise: 0, K: 1, inertia: 0.6, driftDepth: 40 } },
  { name: 'filter-qcollapse', p: { noise: 0, K: 1, qbase: 0.7, q2col: 1 } },
];

function renderScenario(sc) {
  const lab = new FilterLab(SR);
  for (const [k, v] of Object.entries(sc.p)) if (k !== 'seed') lab.setParam(k, v);
  lab.setParam('seed', sc.p.seed ?? lab.p.seed);
  lab.noteOn(NOTE_M, NOTE_F);
  const total = SR * SECONDS;
  const out = new Float32Array(total);
  const L = new Float32Array(BLOCK), R = new Float32Array(BLOCK);
  for (let off = 0; off < total; off += BLOCK) {
    lab.render(L, R);
    for (let i = 0; i < BLOCK && off + i < total; i++) out[off + i] = L[i];  // mono (L==R)
  }
  return out;
}

const outDir = join(root, 'build-golden', 'filter');
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
writeFileSync(join(outDir, 'filter-manifest.tsv'), manifest.join('\n') + '\n');
