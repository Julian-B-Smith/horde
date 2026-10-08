/*
 * modlab_reach.mjs — REACHABILITY probe for the mod matrix.
 *
 * The sweep answers "does this routing blow up?". This answers a different and
 * more design-relevant question: "can this routing express anything?"
 *
 * Motivation: R -> Kboost is bit-identical to no routing at positive depth. That
 * is not a bug in the routing, it is a POLARITY MISMATCH -- the R source is
 * effectively unipolar-negative below the coupling knee, and Kboost is half-wave
 * rectified, so the product is identically zero. Rather than hand-maintain a
 * list of rejected pairs (a list rots; a rule does not), this probe measures
 * each source's actual excursion and each routing's response on both sides of
 * zero depth, and reports the pairs whose range is wholly or half unreachable.
 *
 * Usage: node tools/labharness/modlab_reach.mjs
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const html = readFileSync(resolve(here, '../../docs/design/mod-lab.html'), 'utf8');
const dsp = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)][0][1];
const api = new Function(dsp + '\n;return {ModLab, SOURCES, DESTS, MAXK, SRC_ENV};')();
const { ModLab, SOURCES, DESTS, MAXK, SRC_ENV } = api;

const SR = 44100, BLOCKS = 150, NOTE = 45, FREQ = 440 * Math.pow(2, (NOTE - 69) / 12);
const mk = () => { const l = new ModLab(SR);
  l.rotor.setParam('n', MAXK); l.chorus.p.mix = 0.6; l.phaser.p.mix = 0.6;
  l.choSwarm.setParam('link', 0.5); l.phSwarm.setParam('link', 0.5);
  l.synth.p.cutoff = 0.5; l.depth[SRC_ENV][2] = 0.8; l.scope[SRC_ENV][2] = 0; return l; };
const run = setup => { const lab = mk(); setup(lab); lab.noteOn(NOTE, FREQ);
  const L = new Float32Array(512), R = new Float32Array(512);
  let acc = 0, n = 0;
  for (let b = 0; b < BLOCKS; b++) { lab.render(L, R);
    for (let i = 0; i < 512; i++) { acc += L[i] * L[i]; n++; } }
  return Math.sqrt(acc / n); };

// --- source excursions: what does each source ACTUALLY swing through? -------
const probe = mk(); probe.noteOn(NOTE, FREQ);
const lo = new Array(SOURCES.length).fill(Infinity), hi = new Array(SOURCES.length).fill(-Infinity);
{ const L = new Float32Array(512), R = new Float32Array(512);
  const orig = probe.controlTick.bind(probe);
  probe.controlTick = () => { orig();
    for (let i = 0; i < SOURCES.length; i++) { const v = probe.srcVal[i];
      if (v < lo[i]) lo[i] = v; if (v > hi[i]) hi[i] = v; } };
  for (let b = 0; b < BLOCKS; b++) probe.render(L, R); }

console.log('SOURCE EXCURSIONS (as actually observed, default rotor settings)');
console.log('source   min      max      polarity');
const polarity = [];
for (let i = 0; i < SOURCES.length; i++) {
  const p = hi[i] <= 1e-9 ? 'NEGATIVE-only' : lo[i] >= -1e-9 ? 'POSITIVE-only' : 'bipolar';
  polarity.push(p);
  console.log(`${SOURCES[i].padEnd(8)} ${lo[i].toFixed(4).padStart(8)} ${hi[i].toFixed(4).padStart(8)}  ${p}`);
}

// --- reachability: is either side of zero depth inert? ---------------------
const base = run(() => {});
const eq = (a, b) => Math.abs(a - b) < 1e-12;
const dead = [], half = [];
console.log(`\nREACHABILITY (baseline rms ${base.toFixed(6)}; a side is INERT if both its`);
console.log('depths reproduce the baseline bit-for-bit)\n');
for (let si = 0; si < SOURCES.length; si++) {
  for (let di = 0; di < DESTS.length; di++) {
    const pos = [0.5, 1].map(d => run(l => { l.depth[si][di] = d; }));
    const neg = [-0.5, -1].map(d => run(l => { l.depth[si][di] = d; }));
    const pDead = pos.every(v => eq(v, base)), nDead = neg.every(v => eq(v, base));
    const tag = `${SOURCES[si]} -> ${DESTS[di]}`;
    if (pDead && nDead) dead.push(tag);
    else if (pDead || nDead) half.push(`${tag}  (${pDead ? '+' : '-'} half inert)`);
  }
}
console.log(`FULLY UNREACHABLE (reject candidates): ${dead.length}`);
for (const s of dead) console.log('   ' + s);
console.log(`\nHALF UNREACHABLE (polarity mismatch — usable, but half the knob does nothing): ${half.length}`);
for (const s of half) console.log('   ' + s);
