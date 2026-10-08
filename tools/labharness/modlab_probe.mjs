/*
 * modlab_probe.mjs — headless harness for docs/design/mod-lab.html.
 *
 * WHY THIS EXISTS: the mod lab's DSP was only reachable through a browser
 * context that could not be reliably reset, and a probe that cannot prove it
 * started from a known state cannot support any conclusion — four consecutive
 * false findings on 2026-08-05 came from exactly that. The lab's first <script>
 * block is deliberately DOM-free ("no document/window/canvas references in this
 * section"), so it can be evaluated in Node and a FRESH ModLab constructed per
 * test. Same discipline the C++ cores already get.
 *
 * Usage: node tools/labharness/modlab_probe.mjs
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const html = readFileSync(resolve(here, '../../docs/design/mod-lab.html'), 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
const dsp = blocks[0][1];
if (!dsp.includes('UI (below this banner')) throw new Error('DSP block shape changed');

// Evaluate the DSP block and hand back its constructors. Indirect eval keeps it
// in one scope; the block declares classes and consts with no exports.
const api = new Function(dsp + '\n;return {ModLab, SOURCES, DESTS, SRC_R, SRC_LFOA, SRC_LFOB, SRC_ENV, MAXK, TICK};')();
const { ModLab, SOURCES, DESTS, SRC_R } = api;
const D = Object.fromEntries(DESTS.map((d, i) => [d, i]));
const mtof = m => 440 * Math.pow(2, (m - 69) / 12);
const SR = 44100;

// One trial = one FRESH ModLab. No state can cross between trials.
function trial(setup, { blocks: nBlk = 900, notes = [45, 52] } = {}) {
  const lab = new ModLab(SR);
  setup(lab);
  for (const m of notes) lab.noteOn(m, mtof(m));
  const L = new Float32Array(512), R = new Float32Array(512);
  let peak = 0;
  for (let b = 0; b < nBlk; b++) {
    lab.render(L, R);
    for (let i = 0; i < 512; i++) {
      const v = L[i];
      if (!Number.isFinite(v)) return { ok: false, block: b, sample: i, peak };
      if (Math.abs(v) > peak) peak = Math.abs(v);
    }
  }
  return { ok: true, peak, nanResets: lab.nanResets | 0 };
}
const fmt = r => r.ok ? `clean  peak ${r.peak.toFixed(3)}${r.nanResets ? ` (watchdog fired ${r.nanResets}x)` : ''}`
                      : `NON-FINITE at block ${r.block} sample ${r.sample}`;
const row = (name, r) => console.log(`  ${name.padEnd(42)} ${fmt(r)}`);

console.log('=== 0. control: does the lab render at all? ===');
row('defaults, nothing touched', trial(() => {}));

console.log('\n=== 1. the human report, and its two halves separated ===');
row('R -> choDep  +  chorus link 0.5', trial(l => {
  l.chorus.p.mix = 0.6; l.choSwarm.setParam('link', 0.5); l.depth[SRC_R][D.choDep] = 1; }));
row('R -> choDep,  link 0 (no link)', trial(l => {
  l.chorus.p.mix = 0.6; l.depth[SRC_R][D.choDep] = 1; }));
row('chorus link 0.5,  no routing', trial(l => {
  l.chorus.p.mix = 0.6; l.choSwarm.setParam('link', 0.5); }));

console.log('\n=== 2. is it R, or any source, into choDep? ===');
for (const [nm, si] of [['R', SRC_R], ['K1', 0], ['LFOA', api.SRC_LFOA], ['ENV', api.SRC_ENV]])
  row(`${nm} -> choDep (link 0.5)`, trial(l => {
    l.chorus.p.mix = 0.6; l.choSwarm.setParam('link', 0.5); l.depth[si][D.choDep] = 1; }));

console.log('\n=== 3. is it choDep, or R into any destination? ===');
for (const d of ['K', 'Kboost', 'detune', 'cutoff', 'level', 'choDep', 'phDep', 'morphX', 'morphY'])
  row(`R -> ${d} (link 0.5)`, trial(l => {
    l.chorus.p.mix = 0.6; l.phaser.p.mix = 0.6; l.choSwarm.setParam('link', 0.5);
    l.depth[SRC_R][D[d]] = 1; }));

console.log('\n=== 4. link amount sweep, R -> choDep held at 1 ===');
for (const lk of [0, 0.25, 0.5, 0.75, 1])
  row(`link ${lk}`, trial(l => {
    l.chorus.p.mix = 0.6; l.choSwarm.setParam('link', lk); l.depth[SRC_R][D.choDep] = 1; }));

console.log('\n=== 5. depth polarity and magnitude ===');
for (const dep of [-1, -0.5, 0.5, 1])
  row(`R -> choDep depth ${dep}`, trial(l => {
    l.chorus.p.mix = 0.6; l.choSwarm.setParam('link', 0.5); l.depth[SRC_R][D.choDep] = dep; }));
