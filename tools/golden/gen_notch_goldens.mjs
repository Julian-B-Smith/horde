// Notch swarm golden generator (Track E1.2): render the JS PhaserLab (sliced
// live from reference/swarmphaser.html) to raw f32 for tools/notch_check.cpp L0-1-style
// audio parity. noise=0 every scenario (Math.random dither is not bit-checkable
// — see notch_core.h). One held A3, 1 s, 512-sample blocks; 'seed' applied LAST
// so the pre-render state is a pure function of the final params (this also
// wipes PhaserLab's construction-time controlTick, matching notch_core.h which
// has none).
//
// Usage: node gen_notch_goldens.mjs [--selfcheck]

import { PhaserLab } from './extract_force.mjs';
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const SR = 44100, BLOCK = 512, SECONDS = 1;
const NOTE_M = 57, NOTE_F = 440 * Math.pow(2, (NOTE_M - 69) / 12);

export const SCENARIOS = [
  { name: 'notch-dry', p: { noise: 0, mix: 0 } },
  { name: 'notch-free', p: { noise: 0 } },
  { name: 'notch-sync', p: { noise: 0, K: 1 } },
  { name: 'notch-splay', p: { noise: 0, K: -1 } },
  { name: 'notch-grav', p: { noise: 0, grav: 0.9, basin: 60 } },
  { name: 'notch-fb', p: { noise: 0, feedback: 0.85, K: 0.8 } },
  { name: 'notch-logplace', p: { noise: 0, place: 1, K: 0.6 } },
  { name: 'notch-harmonic', p: { noise: 0, place: 2, grav: 0.9 } },
  { name: 'notch-gauss', p: { noise: 0, dist: 1, K: 1 } },
  { name: 'notch-cauchy-777', p: { noise: 0, dist: 2, seed: 777, K: -1 } },
  { name: 'notch-drift-inertia', p: { noise: 0, K: 1, inertia: 0.6, driftDepth: 100 } },
];

function renderScenario(sc) {
  const lab = new PhaserLab(SR);
  for (const [k, v] of Object.entries(sc.p)) if (k !== 'seed') lab.setParam(k, v);
  lab.setParam('seed', sc.p.seed ?? lab.p.seed);
  lab.noteOn(NOTE_M, NOTE_F);
  const total = SR * SECONDS;
  const out = new Float32Array(total);
  const L = new Float32Array(BLOCK), R = new Float32Array(BLOCK);
  for (let off = 0; off < total; off += BLOCK) {
    lab.render(L, R);
    for (let i = 0; i < BLOCK && off + i < total; i++) out[off + i] = L[i];
  }
  return out;
}

const outDir = join(root, 'build-golden', 'notch');
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
writeFileSync(join(outDir, 'notch-manifest.tsv'), manifest.join('\n') + '\n');
