/*
 * modlab_sweep.mjs — exhaustive deterministic sweep of EVERY mod routing in
 * docs/design/mod-lab.html.
 *
 * WHY: the 2026-08-05 chorus crash was a single (source, destination) pair
 * reaching a floating-point edge no one would think to test by hand. The whole
 * matrix is only 12 x 9; there is no excuse for sampling it. Every routing is
 * driven at full depth, both polarities, from a FRESH engine, and checked for:
 *
 *   NON-FINITE  a NaN/Inf anywhere in the output      -> hard failure
 *   HOT         peak far above the unmodulated level  -> level blow-up
 *   DEAD        output bit-identical to depth 0       -> the routing does nothing
 *
 * DEAD is included deliberately: a control that silently does nothing is the
 * same failure class as the FX dropdown that shipped Comb unreachable — no
 * oracle sees it, because nothing is wrong with the audio.
 *
 * Deterministic: fixed seeds, fixed note, fresh ModLab per trial, no wall clock.
 * Usage: node tools/labharness/modlab_sweep.mjs [--blocks N]
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const html = readFileSync(resolve(here, '../../docs/design/mod-lab.html'), 'utf8');
const dsp = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)][0][1];
if (!dsp.includes('UI (below this banner')) throw new Error('DSP block shape changed');
const api = new Function(dsp + '\n;return {ModLab, SOURCES, DESTS, MAXK, SRC_ENV};')();
const { ModLab, SOURCES, DESTS, MAXK } = api;

const argBlocks = (() => { const i = process.argv.indexOf('--blocks');
  return i > 0 ? +process.argv[i + 1] : 260; })();
const SR = 44100, NOTE = 45, FREQ = 440 * Math.pow(2, (NOTE - 69) / 12);

/*
 * CALIBRATING THE DETECTOR. A routing reads "dead" for two very different
 * reasons, and conflating them makes the whole sweep useless:
 *   (a) the routing is broken                    -> a finding
 *   (b) its target was switched off in the bench -> my fault, not the lab's
 * The first run of this sweep reported 53 dead routings and every one was (b).
 * So the bench is now configured so each destination has somewhere to GO:
 *   - rotor n = MAXK, or K5..K8 are hard-zeroed sources (the lab's own UI
 *     hides those rows, so they are correct-by-design, not dead controls)
 *   - chorus/phaser mix > 0, or choDep/phDep act on nothing
 *   - cutoff parked mid-range, or a positive cutoff mod clamps instantly
 *   - one corner-scoped routing present, or morphX/morphY are inert BY
 *     DESIGN: in this lab morph position only gates scope, it does not blend
 *     corner parameters, so with every scope system-wide there is nothing
 *     for a morph move to change.
 */
function makeLab() {
  const l = new ModLab(SR);
  l.rotor.setParam('n', MAXK);
  l.chorus.p.mix = 0.6;
  l.phaser.p.mix = 0.6;
  l.choSwarm.setParam('link', 0.5);
  l.phSwarm.setParam('link', 0.5);
  l.synth.p.cutoff = 0.5;
  // corner-0-scoped detune route: gives morph position something to gate, so
  // a morphX/morphY routing that still reads dead is genuinely dead.
  l.depth[api.SRC_ENV][2] = 0.8;
  l.scope[api.SRC_ENV][2] = 0;
  return l;
}
function run(setup) {
  const lab = makeLab();
  setup(lab);
  lab.noteOn(NOTE, FREQ);
  const L = new Float32Array(512), R = new Float32Array(512);
  let peak = 0, acc = 0, n = 0, bad = null;
  for (let b = 0; b < argBlocks; b++) {
    lab.render(L, R);
    for (let i = 0; i < 512; i++) {
      const v = L[i];
      if (!Number.isFinite(v)) { bad = bad || { block: b, sample: i }; break; }
      const a = Math.abs(v);
      if (a > peak) peak = a;
      acc += v * v; n++;
    }
    if (bad) break;
  }
  return { bad, peak, rms: n ? Math.sqrt(acc / n) : 0, resets: lab.nanResets | 0 };
}

const base = run(() => {});
console.log(`baseline (no routing):  peak ${base.peak.toFixed(3)}  rms ${base.rms.toFixed(4)}`);
console.log(`sweeping ${SOURCES.length} sources x ${DESTS.length} destinations x 2 polarities, ` +
            `${argBlocks} blocks each\n`);

const HOT = base.peak * 4;              // 12 dB above the unmodulated peak
const fails = [], hot = [], dead = [];
for (let si = 0; si < SOURCES.length; si++) {
  for (let di = 0; di < DESTS.length; di++) {
    for (const pol of [1, -1]) {
      const r = run(l => { l.depth[si][di] = pol; });
      const tag = `${SOURCES[si]} -> ${DESTS[di]} @ ${pol > 0 ? '+1' : '-1'}`;
      if (r.bad) fails.push(`${tag}: NON-FINITE at block ${r.bad.block} sample ${r.bad.sample}`);
      else if (r.resets) fails.push(`${tag}: watchdog fired ${r.resets}x (non-finite, healed)`);
      else if (r.peak > HOT) hot.push(`${tag}: peak ${r.peak.toFixed(2)} (baseline ${base.peak.toFixed(2)})`);
      // DEAD only meaningful for polarity +1; identical rms to baseline means
      // the routing changed nothing at all
      if (!r.bad && pol > 0 && Math.abs(r.rms - base.rms) < 1e-12)
        dead.push(`${tag}: output identical to no routing`);
    }
  }
}
const report = (name, arr) => {
  console.log(`${arr.length ? 'FAIL' : 'OK  '}  ${name}: ${arr.length}`);
  for (const s of arr) console.log(`        ${s}`);
};
report('non-finite routings', fails);
report('level blow-ups (>12 dB over baseline peak)', hot);
report('dead routings (no audible effect)', dead);
console.log(`\n${fails.length ? 'RED' : 'GREEN'} — ${SOURCES.length * DESTS.length * 2} routings swept`);
process.exit(fails.length ? 1 : 0);
