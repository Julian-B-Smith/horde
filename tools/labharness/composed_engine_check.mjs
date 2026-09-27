/*
 * composed_engine_check.mjs — the oracles for docs/design/scalpel-horde-engine.js (B298).
 * WIRED: ./verify full.
 *
 * WHAT IS CHECKED. The composed engine is horde's swarm (reference/swarmsaw.html
 * SwarmSynth) driving the member trajectories and SCALPEL's blades
 * (reference/scalpel/prototype/razor-core.js RazorCore) evaluated on them, per
 * docs/scalpel/ACCOUNTING.md §1.6. Correctness is PARITY WITH THE TWO
 * REFERENCES, never "sounds plausible":
 *   O1  swarm half. Member phases and frequencies equal SwarmSynth's for the
 *       same swarm parameters and seed, EXACTLY, every sample of the first 4096
 *       and every 32nd after; and the phase the blades read equals
 *       frac(φ_horde + ½) (§1.6.6) within a measured rounding bound.
 *   O2  blade half. Fed RazorCore's own trajectories (the 'razor' swarm
 *       source), the composed engine's stereo output equals RazorCore's
 *       sample for sample, over bench presets that exercise every blade path.
 *   O3  neutral case. Where the laws coincide (K 0, dist 0, law 0: ACCOUNTING
 *       row 4 "old patch hears the same") the member frequencies and the output
 *       coincide once the §1.6.6 start is aligned; where they must differ (the
 *       coupling law, row 6) the difference is MEASURED and printed (R(t), lock
 *       time, peak pull) and it is asserted only to be non-zero.
 *   VL  voice law (B310). Poly note-on follows horde's allocator (ADR-083):
 *       a repeated note leaves the first voice releasing, untouched; tier 1
 *       (oldest faded, env < 1e-3), tier 2 (quietest tail, never a held note)
 *       and tier 3 (oldest held) each have a row, and a stolen slot starts
 *       fresh. The must-fail control is the oracle's own law (RazorCore's
 *       same-note reuse and steal-oldest) through the same detector.
 *   DET determinism: same seed and note order give identical output; a
 *       different horde seed does not; the module reads no clock and draws no
 *       unseeded random of its own; the toString() bundle (the AudioWorklet
 *       route) renders the same samples as the direct class.
 * Every property has a MUST-FAIL control (LIBRARY L0032: a detector that shares
 * the assumption it measures confirms whatever you expect): SCALPEL's coupling
 * law swapped in must fail O1; a missing inertia taper, an off-by-one drift
 * seed and a dropped onset must fail O1; a broken phase-origin mapping must fail
 * O2; an unaligned start must break O3's coincidence; a changed seed must break
 * determinism.
 *
 * WHY IN full, NOT fast: ~10 s of DSP on this Mac (O1 steps both engines one
 * sample at a time for its first 4096 samples; O2 renders 24 two-note passes at
 * 2x oversampling), and fast is the seconds-scale leg — station_check's reason.
 * Deterministic, no model calls. Each section prints the previous one's cost.
 * By hand:  node tools/labharness/composed_engine_check.mjs   (exit 1 on any red row)
 * Both references are PROTECTED and are loaded, never edited: SwarmSynth through
 * tools/golden/extract_core.mjs (the golden generator's own loader), RazorCore by
 * require. Math.random is replaced by a seeded mulberry32 around every oracle
 * instance (the lab's convention) and restored on exit.
 */
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { extractCore } from '../golden/extract_core.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const require = createRequire(import.meta.url);
const RazorCore = require(join(root, 'reference/scalpel/prototype/razor-core.js'));
const ENGINE = join(root, 'docs/design/scalpel-horde-engine.js');
const { makeComposedEngine, swarmSourceFromHtml } = require(ENGINE);
const SWARM_HTML = join(root, 'reference/swarmsaw.html');
const SwarmSynth = extractCore(SWARM_HTML, 'SwarmSynth');
const SWARM_SRC = swarmSourceFromHtml(readFileSync(SWARM_HTML, 'utf8'));
const Composed = makeComposedEngine(RazorCore, SWARM_SRC);

const SR = 48000;                       // the SCALPEL lab's rate (scalpel-interface-lab.html SR)
const NOTE = 57, F57 = 440 * Math.pow(2, (NOTE - 69) / 12);
const MATH_RANDOM = Math.random;
function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const frac = x => x - Math.floor(x);
const circ = (a, b) => { const d = frac(a - b); return Math.min(d, 1 - d); };

let red = 0;
const rows = [];
function row(ok, id, text) { rows.push(`${ok ? 'PASS' : 'FAIL'}  ${id.padEnd(5)} ${text}`); if (!ok) red++; console.log(rows[rows.length - 1]); }
function note(text) { console.log('      ' + text); }
/* harness timing only (the cost line the verify wiring relies on); nothing measured reads it */
let lastT = process.uptime();
function section(title) { const t = process.uptime(); console.log(`${title}   [previous section ${(t - lastT).toFixed(1)} s]`); lastT = t; }

/* ---------------------------------------------------------------- engines */
/* a horde-level scenario → the two engines' parameters. Detune is in CENTS on
   the SCALPEL-facing API, so the reference receives cents/100: the very
   arithmetic the engine's row-4 mapping performs (engine syncSwarm). */
const H0 = { n: 7, dist: 0, seed: 1234, cents: 28, law: 0, K: 0, onset: 0, dissolve: 0.63, driftDepth: 0,
  driftRate: 0.4, driftMode: 0, motionCenter: 0, inertia: 0, inertiaCurve: 2.5, retrig: 1, harmReach: 1,
  stretchB: 0, spread: 1, anchor: 0, freqGlide: 0, pivotMode: 0 };
const taper = (k, c) => c === 0.5 ? Math.sqrt(k) : Math.pow(k, c);          // hypersaw_clap.cpp:7312
function refOf(h, over) {
  const r = new SwarmSynth(SR), p = Object.assign({}, h, over || {});
  const set = { n: p.n, dist: p.dist, seed: p.seed, detune: p.cents / 100, law: p.law, K: p.K, onset: p.onset,
    dissolve: p.dissolve, driftDepth: p.driftDepth, driftRate: p.driftRate, driftMode: p.driftMode,
    motionCenter: p.motionCenter, inertia: 'inertiaRaw' in p ? p.inertiaRaw : taper(p.inertia, p.inertiaCurve),
    retrig: p.retrig, harmReach: p.harmReach, stretchB: p.stretchB, spread: p.spread, anchor: p.anchor,
    freqGlide: p.freqGlide, pivotMode: p.pivotMode };
  for (const k in set) r.setParam(k, set[k]);
  return r;
}
function composedOf(h, blade, src, seed) {
  Math.random = mulberry32(seed >>> 0);
  const c = new Composed(SR);
  c.src = src || 'horde';
  c.set(Object.assign({ N: h.n, detune: h.cents, K: h.K, phaseMode: h.retrig ? 1 : 0, dist: h.dist, seed: h.seed,
    'h.law': h.law, onset: h.onset, dissolve: h.dissolve, driftDepth: h.driftDepth, 'h.driftRate': h.driftRate,
    driftMode: h.driftMode, motionCenter: h.motionCenter, inertia: h.inertia, inertiaCurve: h.inertiaCurve,
    harmReach: h.harmReach, stretchB: h.stretchB, spread: h.spread, anchor: h.anchor, freqGlide: h.freqGlide,
    pivotMode: h.pivotMode }, blade || {}));
  Object.assign(c.s, c.t);
  return c;
}

/* ---------------------------------------------------------------- O1 */
function o1(h, opt) {
  opt = opt || {};
  const ref = refOf(h, opt.refOver);
  ref.noteOn(NOTE, F57);
  const c = composedOf(h, opt.blade, opt.src, 0xB298);
  c.noteOn(NOTE, F57, 1);
  const S = c.sw.swarms[c.voices[0].si], rs = ref.swarms[0], v = c.voices[0], n = h.n;
  /* the law control starts the oracle's members where SwarmSynth's start (mapped), so
     what it detects is the coupling law and not the two engines' start conventions */
  if (opt.alignStart) for (let i = 0; i < n; i++) v.m[i].phi = frac(rs.phase[i] + 0.5);
  const total = Math.round((opt.seconds || 0.5) * SR);
  const bufL = new Float32Array(32), bufR = new Float32Array(32), cL = new Float32Array(32), cR = new Float32Array(32);
  let ph = 0, eff = 0, frame = 0, done = 0;
  while (done < total) {
    const b = done < 4096 ? 1 : 32;
    ref.render(bufL.subarray(0, b), bufR.subarray(0, b));
    c.render(cL.subarray(0, b), cR.subarray(0, b));
    done += b;
    for (let i = 0; i < n; i++) {
      /* the swarm half: under the razor source the engine's own swarm is idle, so
         its trajectory is read back from the blade phase through the mapping */
      const phH = c.src === 'horde' ? S.phase[i] : frac(v.m[i].phi - 0.5);
      ph = Math.max(ph, circ(phH, rs.phase[i]));
      if (c.src === 'horde') eff = Math.max(eff, Math.abs(S.eff[i] - rs.eff[i]));
      frame = Math.max(frame, circ(v.m[i].phi, rs.phase[i] + 0.5));
    }
  }
  return { ph, eff, frame, R: rs.R };
}
const O1_TOL_FRAME = 1e-9;   // two integrators of the same increments; measured bound printed below
const O1 = [
  ['law 0 cents', { law: 0, cents: 28, K: 0.35 }],
  ['law 1 Hz', { law: 1, cents: 40, K: 0.35 }],
  ['law 2 ERB', { law: 2, cents: 40, K: 0.35 }],
  ['law 4 harm', { law: 4, cents: 30, K: 0.2, harmReach: 1.5 }],
  ['law 5 strch', { law: 5, cents: 60, K: 0.2, stretchB: 3 }],
  ['sprd+anch', { law: 0, cents: 20, K: 0.3, spread: 2, anchor: 0.5, dist: 1 }],
  ['onset -1', { K: 0.2, onset: -1, dissolve: 0.3 }],
  ['onset 0', { K: 0.2, onset: 0, dissolve: 0.3 }],
  ['onset +1', { K: 0.2, onset: 1, dissolve: 0.3 }],
  ['inert 0', { K: 0.8, inertia: 0 }],
  ['inert .7', { K: 0.8, inertia: 0.7 }],
  ['in.7 c.5', { K: 0.8, inertia: 0.7, inertiaCurve: 0.5 }],
  ['drift m0', { K: 0.3, retrig: 0, driftDepth: 25, driftRate: 0.6, driftMode: 0 }],
  ['drift m1', { K: 0.3, retrig: 0, driftDepth: 25, driftRate: 0.6, driftMode: 1 }],
  ['drift m2', { K: 0.3, retrig: 0, driftDepth: 25, driftRate: 0.6, driftMode: 2, motionCenter: 0.5 }],
  ['K -1', { K: -1, cents: 10, retrig: 0 }],
  ['K -0.5 JP', { K: -0.5, dist: 1, cents: 20, retrig: 0 }],
  ['K +1', { K: 1, cents: 40, retrig: 0 }],
  ['fGlide', { K: 0.4, freqGlide: 0.02, driftDepth: 30, retrig: 0 }],
];
section('O1 — swarm half vs SwarmSynth (reference/swarmsaw.html), 0.5 s at 48 kHz, note 57, N 7, default blade ON');
let worstFrame = 0;
for (const [name, over] of O1) {
  const h = Object.assign({}, H0, over);
  const r = o1(h);
  worstFrame = Math.max(worstFrame, r.frame);
  row(r.ph === 0 && r.eff === 0 && r.frame < O1_TOL_FRAME, 'O1',
    `${name.padEnd(11)} max|Δφ_horde| ${r.ph.toExponential(1)}  max|Δf| ${r.eff.toExponential(1)} Hz  ` +
    `blade frame |φ_S − (φ_H+½)| ${r.frame.toExponential(1)}  (R end ${r.R.toFixed(3)})`);
}
{
  /* blades cannot move the swarm: the same scenario with the blade off and with
     cross-mod + feedback on gives the same trajectories */
  const h = Object.assign({}, H0, { K: 0.5, retrig: 0, driftDepth: 10 });
  const a = o1(h, { blade: { w: 0 } }), b = o1(h, { blade: { xm: 0.6, fb: 0.4, b2on: 1 } });
  row(a.ph === 0 && b.ph === 0, 'O1', `blade-independent: blade off ${a.ph.toExponential(1)} · xm .6 fb .4 blade 2 on ${b.ph.toExponential(1)}`);
}
note(`blade-frame rounding bound over all O1 runs: ${worstFrame.toExponential(2)} cycles (gate ${O1_TOL_FRAME})`);
{
  const h = Object.assign({}, H0, { K: 1, cents: 40, retrig: 1 });
  const z = o1(Object.assign({}, H0, { K: 0, cents: 40, retrig: 0 }), { src: 'razor', alignStart: true });
  row(z.ph < 1e-9, 'O1c', `control's own zero: SCALPEL's swarm half at K 0, same start: max|Δφ| ${z.ph.toExponential(1)} — must read ~0 (the laws coincide there)`);
  const a = o1(h, { src: 'razor', alignStart: true });
  row(a.ph > 1e-3, 'O1c', `CONTROL SCALPEL coupling law swapped in (K +1, same start): max|Δφ| ${a.ph.toFixed(4)} cycles — must be large`);
  const k = o1(Object.assign({}, H0, { K: -1, cents: 10, retrig: 0 }), { src: 'razor', alignStart: true });
  row(k.ph > 1e-3, 'O1c', `CONTROL SCALPEL law, K −1 splay, same start: max|Δφ| ${k.ph.toFixed(4)} — must be large`);
  const t = o1(Object.assign({}, H0, { K: 0.8, inertia: 0.7 }), { refOver: { inertiaRaw: 0.7 } });
  row(t.ph > 1e-3, 'O1c', `CONTROL reference without the shell's inertia taper: max|Δφ| ${t.ph.toFixed(4)} — must be large`);
  /* retrig on and dist 0: the seed then reaches the drift stream alone */
  const s = o1(Object.assign({}, H0, { K: 0.3, retrig: 1, driftDepth: 25, driftRate: 0.6 }), { refOver: { seed: 1235 } });
  row(s.ph > 1e-3, 'O1c', `CONTROL drift seed off by one (aligned start, so only drift sees it): max|Δφ| ${s.ph.toFixed(4)} — must be large`);
  const o = o1(Object.assign({}, H0, { K: 0.2, onset: 1, dissolve: 0.3 }), { refOver: { onset: 0 } });
  row(o.ph > 1e-3, 'O1c', `CONTROL onset dropped on the reference: max|Δφ| ${o.ph.toFixed(4)} — must be large`);
}
{
  /* the JS reference's onset is symmetric (8·onset², swarmsaw.html:355); horde's
     C++ is bipolar (ADR-056, swarm_core.h:632-635): recorded, not hidden */
  const m = o1(Object.assign({}, H0, { K: 0.2, onset: -1, dissolve: 0.3 })), p = o1(Object.assign({}, H0, { K: 0.2, onset: 1, dissolve: 0.3 }));
  note(`FINDING onset −1 vs +1 in SwarmSynth: R at 0.5 s ${m.R.toFixed(6)} vs ${p.R.toFixed(6)} — the JS reference plays the SAME sync burst; horde C++ (ADR-056) plays a splay burst at −1`);
}

/* ---------------------------------------------------------------- O2 */
const PRESETS = JSON.parse(readFileSync(join(root, 'reference/scalpel/data/presets.json'), 'utf8')).presets;
const byName = n => { const p = PRESETS.find(x => x.name === n); if (!p) throw new Error('preset missing: ' + n); return p.params; };
const O2_SET = ['Quarter sync', 'Two blades', 'Crush vs FM', 'Golden bells', 'Wandering blades', 'Cross-mod horde',
  'Feedback screech', 'Splayed blades', 'Breath and bite', 'Folded slice', 'Swarm-linked spread', 'Glass horde pad'];
function events(c, total, L, R) {
  /* two overlapping notes and a release, at block boundaries (128, the worklet quantum) */
  const B = 128, ev = [[0, 'on', 57], [9600, 'on', 64], [14336, 'off', 57]];
  let e = 0;
  for (let i = 0; i < total; i += B) {
    while (e < ev.length && ev[e][0] <= i) { const [, k, nn] = ev[e++]; if (k === 'on') c.noteOn(nn, 440 * Math.pow(2, (nn - 69) / 12), 0.9); else c.noteOff(nn); }
    c.render(L.subarray(i, i + B), R.subarray(i, i + B));
  }
}
function renderWith(make, params, seed, total) {
  Math.random = mulberry32(seed);
  const c = make();
  c.set(params); Object.assign(c.s, c.t);
  const L = new Float32Array(total), R = new Float32Array(total);
  events(c, total, L, R);
  return { L, R };
}
const maxDiff = (a, b) => { let m = 0; for (let i = 0; i < a.L.length; i++) m = Math.max(m, Math.abs(a.L[i] - b.L[i]), Math.abs(a.R[i] - b.R[i])); return m; };
const rms = a => { let e = 0; for (let i = 0; i < a.L.length; i++) e += a.L[i] * a.L[i]; return Math.sqrt(e / a.L.length); };
section('O2 — blade half: composed engine fed RazorCore\'s own trajectories vs RazorCore, 0.5 s, two notes + release');
{
  const total = 24064;
  let n = 0;
  for (const name of O2_SET) for (const pm of [0, 1]) {
    const params = Object.assign({}, byName(name), { phaseMode: pm });
    const seed = 0xB298 + n++;
    const o = renderWith(() => new RazorCore(SR), params, seed, total);
    const c = renderWith(() => { const e = new Composed(SR); e.src = 'razor'; return e; }, params, seed, total);
    const d = maxDiff(o, c);
    row(d === 0 && rms(o) > 1e-4, 'O2', `${name.padEnd(20)} phaseMode ${pm}: max|Δ| ${d.toExponential(1)} (oracle RMS ${rms(o).toFixed(4)})`);
  }
  const params = Object.assign({}, byName('Two blades'), { phaseMode: 0 });
  const o = renderWith(() => new RazorCore(SR), params, 0xB2A0, total);
  const c = renderWith(() => { const e = new Composed(SR); e.src = 'razor'; e.origin = 0; return e; }, params, 0xB2A0, total);
  row(maxDiff(o, c) > 1e-3, 'O2c', `CONTROL phase-origin mapping broken (origin 0, not ½): max|Δ| ${maxDiff(o, c).toFixed(4)} — must be large`);
  note('phaseMode 2 (settled) is outside O2: its phases are arbitrary doubles, and the frame round trip frac(frac(φ−½)+½) is not bit-exact below ¼; settle is not voiced by the composition (ACCOUNTING Q B4)');
}

/* ---------------------------------------------------------------- O3 */
section('O3 — neutral case and the coupling-law difference');
{
  /* (a) coincide: K 0, onset 0, drift 0, inertia 0, dist 0, law 0, aligned start */
  const blade = { N: 5, detune: 14, K: 0, phaseMode: 1 };
  const total = 24000;
  const h = Object.assign({}, H0, { n: 5, cents: 14, K: 0, retrig: 1 });
  const run = (makeC, shift) => {
    Math.random = mulberry32(0xB2A3);
    const c = makeC();
    c.set(blade); Object.assign(c.s, c.t);
    c.noteOn(NOTE, F57, 1);
    if (shift) for (const m of c.voices[0].m) m.phi = 0.5;   // fixture: the oracle's aligned start moved by §1.6.6's ½
    const L = new Float32Array(total), R = new Float32Array(total);
    for (let i = 0; i < total; i += 128) c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    return { c, L, R };
  };
  const comp = run(() => composedOf(h, null, 'horde', 0xB2A3));
  const orc = run(() => new RazorCore(SR), true);
  const raw = run(() => new RazorCore(SR), false);
  let fr = 0;
  for (let i = 0; i < 5; i++) fr = Math.max(fr, Math.abs(comp.c.voices[0].m[i].inc / orc.c.voices[0].m[i].inc - 1));
  const dA = maxDiff(comp, orc), dRaw = maxDiff(comp, raw);
  row(fr < 1e-12, 'O3', `member frequencies, horde law 0 at 0.14 vs SCALPEL 14 c (row 4): max relative Δ ${fr.toExponential(2)}`);
  row(dA < 1e-4, 'O3', `output, oracle's aligned start moved to φ_S ½ (horde's retrig start, §1.6.6): max|Δ| ${dA.toExponential(2)} over 0.5 s`);
  row(dRaw > 1e-2, 'O3c', `CONTROL oracle's own aligned start (φ_S 0, unmoved): max|Δ| ${dRaw.toFixed(4)} — must differ (the ½-cycle origin)`);
  /* (c) the distribution: row 4's identity holds at dist 0 only */
  const cents = d => { const r = refOf(Object.assign({}, h, { dist: d })); r.noteOn(NOTE, F57); r.controlTick(r.swarms[0]);
    return Array.from(r.swarms[0].vf.subarray(0, 5), f => (1200 * Math.log2(f / F57)).toFixed(2)).join(' '); };
  note(`member cents at 14 c: horde dist 0 [${cents(0)}] · dist 1 JP (horde's default) [${cents(1)}] · SCALPEL [-14.00 -7.00 0.00 7.00 14.00]`);
}
{
  /* (b) the coupling law: the same engine, the same start phases, horde's law vs SCALPEL's */
  const law = (src, n, cents, f, K, secs) => {
    const h = Object.assign({}, H0, { n, cents, K, retrig: 1 });
    const c = composedOf(h, { w: 0 }, src, 0xB2A4);
    c.noteOn(69, f, 1);
    const v = c.voices[0], rng = mulberry32(0xC0FFEE), start = [];
    for (let i = 0; i < n; i++) start.push(rng());
    const S = c.sw.swarms[v.si];
    for (let i = 0; i < n; i++) { S.phase[i] = start[i]; v.m[i].phi = frac(start[i] + 0.5); }
    const total = Math.round(secs * SR), B = 16, L = new Float32Array(B), R = new Float32Array(B), Rt = [];
    for (let i = 0; i < total; i += B) {
      c.render(L, R);
      let sx = 0, sy = 0;
      for (let q = 0; q < n; q++) { sx += Math.cos(2 * Math.PI * v.m[q].phi); sy += Math.sin(2 * Math.PI * v.m[q].phi); }
      Rt.push(Math.hypot(sx, sy) / n);
    }
    return { Rt, sigma: S.sigma, c };
  };
  const at = (Rt, t) => Rt[Math.min(Rt.length - 1, Math.round(t * SR / 16) - 1)];
  const t90 = Rt => { const i = Rt.findIndex(r => r >= 0.9); return i < 0 ? '—' : (((i + 1) * 16 / SR) * 1000).toFixed(0) + ' ms'; };
  const held = Rt => { let k = Rt.length; while (k > 0 && Rt[k - 1] >= 0.9) k--; return k === Rt.length ? '—' : ((k * 16 / SR) * 1000).toFixed(0) + ' ms'; };
  const tail = Rt => { const a = Rt.slice(Math.floor(Rt.length * 2 / 3)); return a.reduce((x, y) => x + y, 0) / a.length; };
  const CASES = [[5, 14, 110, 0.35], [5, 14, 440, 0.35], [5, 14, 440, 1.0], [7, 40, 110, 1.0], [7, 40, 880, 0.35], [5, 0, 440, 0.35], [4, 4, 220, -1], [7, 3, 220, -1]];
  note('N · detune · f · K        | law     | R 50 ms  R 250 ms  R 1 s  R 1.5 s | first R≥.9 | held ≥.9 from | mean R last .5 s | peak pull at R 1 (Hz)');
  let maxGap = 0;
  for (const [n, cents, f, K] of CASES) {
    const H = law('horde', n, cents, f, K, 1.5), Z = law('razor', n, cents, f, K, 1.5);
    let gap = 0; for (let i = 0; i < H.Rt.length; i++) gap = Math.max(gap, Math.abs(H.Rt[i] - Z.Rt[i]));
    if (n === 5 && cents === 14 && f === 440 && K === 0.35) maxGap = gap;
    /* steady KsmS (sync) or KsmP (splay, ×3 and toward the seats), swarmsaw.html:486-488 */
    const pullH = (K >= 0 ? 4 : 12) * K * K * H.sigma;
    const pullZ = Math.abs(K * (1.5 * f * (Math.pow(2, cents / 1200) - 1) + 3));                   // razor-core.js keff /2π, cScale 0
    for (const [tag, X, pull] of [['horde  ', H, pullH], ['SCALPEL', Z, pullZ]])
      note(`${String(n).padEnd(2)}· ${String(cents).padStart(2)} c · ${String(f).padStart(3)} · ${String(K).padEnd(5)}| ${tag} | ` +
        `${at(X.Rt, 0.05).toFixed(3)}    ${at(X.Rt, 0.25).toFixed(3)}     ${at(X.Rt, 1).toFixed(3)}  ${at(X.Rt, 1.5).toFixed(3)}   | ` +
        `${t90(X.Rt).padStart(10)} | ${held(X.Rt).padStart(13)} | ${tail(X.Rt).toFixed(3).padStart(16)} | ${pull.toFixed(2)}`);
  }
  row(maxGap > 1e-3, 'O3', `the coupling law DIFFERS (row 6, horde's wins): max|ΔR(t)| ${maxGap.toFixed(3)} at N 5 · 14 c · 440 Hz · K 0.35 — measured above, not hidden`);
}

/* ---------------------------------------------------------------- voice law (B310) */
/* horde's allocator (ADR-083, src/swarm_core.h alloc()): free slot oldest first,
   else the quietest releasing tail, else the oldest held. The engine overrides
   RazorCore's poly noteOn, whose same-note reuse cut a repeated note's first
   release off (razor-core.js:372). The MUST-FAIL control for every row is the
   ORACLE ITSELF run through the same detector: RazorCore's own law must read
   the old behaviour, so the detector is proven able to see the difference.
   O1-O3 above use one note or distinct notes on an empty pool, where every law
   picks slots 0, 1, … in order: their numbers are unchanged by this override. */
section('VOICE — horde\'s voice law (ADR-083) in the poly note-on, oracle as the must-fail control');
{
  const vlMake = oracle => {
    Math.random = mulberry32(0xB310);
    const c = oracle ? new RazorCore(SR) : new Composed(SR);
    c.set({ N: 5, detune: 14, K: 0.35, phaseMode: 1 }); Object.assign(c.s, c.t);   // poly 6, R 280 ms (defaults)
    return c;
  };
  const hz = n => 440 * Math.pow(2, (n - 69) / 12);
  /* ev: [sample, 'on'|'off', note]; probe(sampleIndex) is called after each 128 block */
  const play = (c, ev, total, probe) => {
    const L = new Float32Array(128), R = new Float32Array(128);
    let e = 0;
    for (let i = 0; i < total; i += 128) {
      while (e < ev.length && ev[e][0] <= i) { const [, k, n] = ev[e++]; if (k === 'on') c.noteOn(n, hz(n), 0.9); else c.noteOff(n); }
      c.render(L, R);
      if (probe) probe(i + 128);
    }
  };
  const pool = c => c.voices.slice(0, c.d.poly);
  const snap = c => pool(c).map(v => ({ note: v.note, gate: v.gate, active: v.active, env: v.env, age: v.age }));

  /* (1) the human's report: A on, A off, A on again inside the release */
  const A = 57, T_OFF = 9600, T_RE = 14400, T_END = 24064;          // off at 0.2 s, re-strike 0.1 s into the release
  const repeat = oracle => {
    const c = vlMake(oracle), env1 = [];
    play(c, [[0, 'on', A], [T_OFF, 'off', A], [T_RE, 'on', A]], T_END, () => env1.push(c.voices[0].env));
    return { c, env1, s: snap(c) };
  };
  const hc = repeat(false), oc = repeat(true);
  const alone = (() => { const c = vlMake(false), env1 = []; play(c, [[0, 'on', A], [T_OFF, 'off', A]], T_END, () => env1.push(c.voices[0].env)); return env1; })();
  const act = s => s.filter(v => v.active);
  const first = hc.s[0], second = hc.s[1];
  let tailD = 0; for (let i = 0; i < alone.length; i++) tailD = Math.max(tailD, Math.abs(hc.env1[i] - alone[i]));
  row(act(hc.s).length === 2 && first.note === A && !first.gate && first.env > 1e-3 && second.note === A && second.gate,
    'VL', `repeat A: ${act(hc.s).length} active voices at 0.5 s; first (slot 0) releasing, gate ${first.gate}, env ${first.env.toExponential(2)}; ` +
    `second (slot 1) gated ${second.gate}`);
  row(tailD === 0, 'VL', `the first release is untouched by the repeat: max|Δenv| vs the same A released alone ${tailD.toExponential(1)} over ${alone.length} blocks`);
  row(act(oc.s).length === 1 && oc.s[0].gate, 'VLc',
    `CONTROL the oracle's reuse law (RazorCore): ${act(oc.s).length} active voice, slot 0 re-gated ${oc.s[0].gate} — must be 1 (the report's "the note gets stolen")`);

  /* (2) note-off targets the voice still gated: a second A-off releases the NEW voice, the tail rings on */
  {
    const c = vlMake(false);
    play(c, [[0, 'on', A], [T_OFF, 'off', A], [T_RE, 'on', A], [19200, 'off', A]], T_END);
    const s = snap(c);
    row(!s[0].gate && s[0].active && !s[1].gate && s[1].active && c.voices[1].stage === 4 && s[1].env > s[0].env,
      'VL', `second A-off releases the gated voice: slot 1 gate ${s[1].gate} stage ${c.voices[1].stage} env ${s[1].env.toExponential(2)}; ` +
      `slot 0 tail still active, env ${s[0].env.toExponential(2)}`);
    /* doubly-held A (two ons, no off): one off releases every gated A, horde's by-key rule (swarm_core.h noteOff) */
    const d = vlMake(false);
    play(d, [[0, 'on', A], [4800, 'on', A], [9600, 'off', A]], 12032);
    const g = pool(d).filter(v => v.gate).length, both = pool(d).filter(v => v.active && v.note === A).length;
    row(g === 0 && both === 2, 'VL', `doubly-held A, one off: ${both} voices hold A, ${g} still gated — must be 0 (no stuck note)`);
  }

  /* A stolen slot is a NEW voice: startVoice runs fresh, so the swarm restarts at
     horde's aligned retrig start (SwarmSynth noteOn: phase 0, swarmsaw.html:365;
     blade frame ½, §1.6.6) and its glide snaps (vfInit 0). The reading before the
     steal is the must-read-non-zero twin: the slot was running, off that start. */
  const off = (c, slot) => { let m = 0; for (let i = 0; i < c.d.N; i++) m = Math.max(m, Math.abs(c.voices[slot].m[i].phi - 0.5)); return m; };
  const steal = (oracle, ev, upTo) => {
    const c = vlMake(oracle);
    play(c, ev, upTo);                                  // every event lands before the last block
    const pre = snap(c), was = pre.map((_, i) => off(c, i));
    c.noteOn(70, hz(70), 0.9);                          // the steal, probed before any render
    const post = snap(c), slot = post.findIndex(v => v.note === 70);
    const fresh = !oracle && off(c, slot) === 0 && c.sw.swarms[c.voices[slot].si].vfInit === 0;
    return { pre, post, slot, fresh, was: was[slot] };
  };

  /* (2b) tier 1's "faded" is horde's env < 1e-3, not RazorCore's !active (env
     < 1e-4). 62-65 held; 61 released first, 60 (OLDER) 50 ms later, so both
     tails sit in the 1e-4..1e-3 window with the older one LOUDER. Tier 1 takes
     the oldest faded slot (60's); a free test of !active would find nothing
     and fall to tier 2, which takes the quietest (61's). */
  {
    const ev = [[0, 'on', 60], [128, 'on', 61], [256, 'on', 62], [384, 'on', 63], [512, 'on', 64], [640, 'on', 65],
      [9600, 'off', 61], [12032, 'off', 60]];
    const h = steal(false, ev, 36096);
    const win = [0, 1].every(i => h.pre[i].active && h.pre[i].env >= 1e-4 && h.pre[i].env < 1e-3);
    row(win && h.pre[0].env > h.pre[1].env && h.slot === 0, 'VL1',
      `tier 1: tails 60/61 env ${h.pre[0].env.toExponential(2)}/${h.pre[1].env.toExponential(2)} (both faded, both still active); ` +
      `note 70 took slot ${h.slot} (was ${h.pre[h.slot].note}, the OLDEST faded, not the quietest)`);
  }

  /* (3) tier 2: 60-62 HELD (the oldest), 63-65 released newest-first so the
     quietest tail (65) is the YOUNGEST voice. Tier 2 must take 65's slot:
     not a held note (the ADR-083 bug), and not merely the oldest tail (63). */
  {
    const ev = [[0, 'on', 60], [128, 'on', 61], [256, 'on', 62], [384, 'on', 63], [512, 'on', 64], [640, 'on', 65],
      [9600, 'off', 65], [11520, 'off', 64], [13440, 'off', 63]];
    const h = steal(false, ev, 16896), o = steal(true, ev, 16896);
    const tails = h.pre.map((v, i) => ({ i, ...v })).filter(v => !v.gate);
    const quiet = tails.reduce((a, b) => (b.env < a.env ? b : a));
    const heldKept = [60, 61, 62].every(n => h.post.some(v => v.note === n && v.gate));
    row(tails.length === 3 && tails.every(v => v.env >= 1e-3) && h.slot === quiet.i && h.pre[h.slot].note === 65 && heldKept,
      'VL2', `tier 2: tails 63/64/65 env ${tails.map(v => v.env.toFixed(3)).join('/')}, none free; note 70 took slot ${h.slot} ` +
      `(was ${h.pre[h.slot].note}; quietest tail ${quiet.note}); held 60-62 kept ${heldKept}`);
    row(h.fresh && h.was > 1e-3, 'VL2', `the stolen slot starts FRESH: max|φ_S − ½| ${h.was.toFixed(4)} before the steal → 0 after, swarm glide re-snapped`);
    row(o.slot !== h.slot && o.pre[o.slot].gate, 'VL2c',
      `CONTROL the oracle's law steals slot ${o.slot} (was ${o.pre[o.slot].note}, gated ${o.pre[o.slot].gate}) — must differ: a held note is lost`);
  }

  /* (4) tier 3: every slot gated. Slot 0 is re-struck after its first note fades,
     so the OLDEST held voice is slot 1, not the first index: an index-order
     allocator would fail this row. */
  {
    const ev = [[0, 'on', 60], [128, 'on', 61], [256, 'on', 62], [384, 'on', 63], [512, 'on', 64], [640, 'on', 65],
      [1280, 'off', 60], [38400, 'on', 66]];
    const { pre, post, slot, fresh, was } = steal(false, ev, 38528);
    const oldest = pre.reduce((a, v, i) => (v.age < pre[a].age ? i : a), 0);
    row(pre.every(v => v.gate) && pre[0].note === 66 && slot === oldest && slot === 1 && post[0].note === 66 && post.filter(v => v.gate).length === 6,
      'VL3', `tier 3: all ${pre.filter(v => v.gate).length} gated (slot 0 re-struck as 66); note 70 took slot ${slot} ` +
      `(was ${pre[slot].note}, age ${pre[slot].age}, the oldest held); 66 kept in slot 0`);
    row(fresh && was > 1e-3, 'VL3', `the stolen held slot starts FRESH: max|φ_S − ½| ${was.toFixed(4)} before → 0 after`);
  }
}

/* ---------------------------------------------------------------- determinism */
section('DET — determinism and the worklet route');
{
  const h = Object.assign({}, H0, { K: 0.4, retrig: 0, driftDepth: 20, onset: 0.8, inertia: 0.6 });
  const params = { N: 7, detune: 28, K: 0.4, phaseMode: 0, driftDepth: 20, onset: 0.8, inertia: 0.6, xm: 0.3, b2on: 1, law: 4 };
  const total = 24064;
  const A = renderWith(() => new Composed(SR), params, 0xD00D, total);
  const B = renderWith(() => new Composed(SR), params, 0xD00D, total);
  row(maxDiff(A, B) === 0 && rms(A) > 1e-4, 'DET', `same seed, same note order: max|Δ| ${maxDiff(A, B).toExponential(1)}`);
  const C = renderWith(() => new Composed(SR), Object.assign({}, params, { seed: 1235 }), 0xD00D, total);
  row(maxDiff(A, C) > 1e-3, 'DETc', `CONTROL horde seed 1234 → 1235: max|Δ| ${maxDiff(A, C).toFixed(4)} — must differ`);
  const Bundled = new Function(Composed.toString() + '\nreturn RazorCore;')();
  const W = renderWith(() => new Bundled(SR), params, 0xD00D, total);
  row(maxDiff(A, W) === 0 && Bundled !== RazorCore, 'DET', `toString() bundle (the AudioWorklet route, RazorCore rebound to the composed class): max|Δ| ${maxDiff(A, W).toExponential(1)}`);
  void h;
  /* the lab's contract (scalpel-interface-lab.html memberCtx/bladeAt/monStrike): the viz
     post still carries mem.phi/c/k, now with the swarm's state; CORE.mr/mn reach the
     statics RazorCore.voice() reads by name; the lab's key filter (t ∪ d) sees horde's rows */
  Math.random = mulberry32(7);
  const e = new Composed(SR); e.set(params); Object.assign(e.s, e.t);
  let viz = null; e.post = m => { if (m && m.t === 'viz' && m.mem) viz = m; };
  e.noteOn(57, F57, 1);
  const L = new Float32Array(2048), R = new Float32Array(2048); e.render(L, R);
  const mr0 = RazorCore.mr; Composed.mr = 0.3125; const fwd = RazorCore.mr === 0.3125; RazorCore.mr = mr0;
  const keys = new Set(Object.keys(e.t).concat(Object.keys(e.d)));
  const want = ['onset', 'dissolve', 'driftDepth', 'inertia', 'inertiaCurve', 'h.law', 'h.driftRate', 'harmReach', 'stretchB', 'dist', 'seed'];
  const miss = want.filter(k => !keys.has(k));
  const ok = viz && viz.mem.phi.length === 7 && viz.mem.c.length === 7 && viz.mem.k.length === 7 && viz.horde && viz.horde.eff.length === 7 && fwd && !miss.length;
  row(!!ok, 'API', `viz post (mem.phi/c/k ×${viz ? viz.mem.phi.length : 0}, horde R ${viz && viz.horde ? viz.horde.R.toFixed(3) : '—'}), static mr/mn forwarding ${fwd}, horde keys in t∪d${miss.length ? ' MISSING ' + miss.join(',') : ''}`);
  /* the module itself reads no clock and no unseeded random (SPEC §5.7); comments stripped first */
  const code = readFileSync(ENGINE, 'utf8').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
  const bad = ['Date', 'performance', 'Math.random', 'setTimeout', 'currentTime'].filter(w => code.includes(w));
  row(bad.length === 0, 'DET', `engine source reads no clock and draws no Math.random of its own${bad.length ? ': found ' + bad.join(', ') : ''}`);
}

Math.random = MATH_RANDOM;
console.log(`\n${red ? 'RED' : 'GREEN'} — composed_engine_check: ${rows.length} rows, ${red} failed`);
process.exit(red ? 1 : 0);
