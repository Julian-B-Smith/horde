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
 *   CULL voice cap (B323). Over a host-set cap the quietest releasing tails
 *       fade out (tier 1, then tier 2), a held note is never culled, the
 *       survivors are bit-identical to an uncapped twin, the culled tail's gain
 *       ramps linearly to 0 over 8 ms and B316's click metric reads no new
 *       click (a pure sine and Crushed bells), a cap that does not bind changes
 *       nothing, and at the cap a note-on replaces a sounding tail. Controls: a
 *       tier-blind culler; the same cull as an instant cut, which the metric
 *       must catch.
 *   CAPPO the policy at a FULL cap (B375). Every sounding voice held, the cap
 *       full: REFUSE (the default) drops the note and leaves the pool and the
 *       samples exactly a twin's that never got it; STEAL (the lab's test toggle)
 *       frees the oldest held voice over the 8 ms ramp and plays the note in a
 *       free slot, the other held voices untouched; with no free slot it is the
 *       full-pool law; below the cap both are bit-identical to no cap. Controls:
 *       REPLACE (B323's law) through the refuse detector; the steal as an
 *       instant cut; the cap binding through the below-cap comparison.
 *   KQ  ADR-184 A2 (2). Snapped Cut spread mirrors exactly (half away from
 *       zero) and is the oracle's bit for bit except at negative halves.
 *       Control: the oracle's own Math.round at ±2.5.
 *   B325 first-sample frequencies. RazorCore reads the swarm's member
 *       frequencies (m.inc) from note-on, not the undetuned pitch, and the
 *       neutral case is exact on the Hz-unit presets that exposed the gap and on
 *       a chord struck on a mono voice; the must-fail controls rebuild the old
 *       placeholder and the fix's own first attempt.
 *   GRAV consonance gravity (B335, ADR-008, ADR-086 + A1). Against DynSynth
 *       (reference/swarmdynamics.html) on the same swarm, seed and notes: every
 *       note's member phases, member frequencies and f0cur, EXACTLY (five
 *       scenarios: a sharp fifth, K .35 from random phases, a late triad, an
 *       octave-folded twelfth, and a pair outside the basin that must not move);
 *       a 3 s settle onto 3/2. Controls: DynSynth without gravity; gravity
 *       stepped per render call (the pre-ADR-086 law); a 256-SAMPLE grid at
 *       48 kHz (the pre-Amendment-1 law).
 *   ZERO the new parameters at their defaults are the pre-B335 engine, bit for
 *       bit: three renders fingerprinted against main at c79be56. Must-differ
 *       twins switch each feature on.
 *   ONS  onset scatter and timing correction (B335, ADR-077, B149). NO JS
 *       REFERENCE EXISTS: the C++ (src/swarm_core.h) is the reference, rendered by
 *       tools/onset_ref_check.cpp into tools/labharness/onset_ref_cpp.json (which
 *       `verify full` re-derives and fails when stale). The draws (offsets, waits,
 *       coefficients) over 40-note phrases at four correction gains and two seeds;
 *       ADR-077's structure law (lag-1 of the asynchrony falls with the gain);
 *       rendered entries (exact), entry ramps and phases, coupled and uncoupled.
 *       Controls: the wrong alpha sign, the stream not seeded (the pre-B149
 *       literal), i.i.d. jitter with no memory, a waiting member whose phase runs.
 *       A FINDING row records the C++'s wait counted per sub-sample at its 2x.
 *   VENV per-partial envelopes and attack/release scatter (B335, ADR-078): entries
 *       exact; each member's attack and release half-times scale by the C++'s
 *       drawn factor (the ratio across the two envelope laws is one constant per
 *       stage); unscattered ≡ the voice envelope bit for bit; liveness follows the
 *       loudest member. Control: members without their drawn factors.
 *   AA  ADR-189's anti-aliasing divergences (B355), each flag default 0.
 *       AA0: all three off renders main's engine (d443eb6) bit for bit, flags
 *       absent and written 0 (four presets fingerprinted); each flag on alone
 *       must change one. AA1 (D1, carrier ADAA): the inharmonic residual of a
 *       strictly periodic case falls on a phase-modulated carrier and on a
 *       Band-limit-off carrier; control: the BLEPs removed without the ADAA.
 *       AA1s: ADR-189 A1's narrowed scope leaves a plain sync saw, an S&H-
 *       modulated FM saw and a sine carrier bit-identical; control: the same
 *       comparison on an in-scope FM saw must differ. AA2 (D2, the scanner tracks xin): a
 *       constant xin fixture moves the carrier's wraps; with D2 the residual
 *       is the xin-0 case's; control: the scanner reading −xin. AA3 (D3, the
 *       loop filter over the oracle's tap): broad#828's rate-locked line is
 *       gone; controls: the oracle's loop, and D3's filter as a pass-through,
 *       both bring the R/5 cycle back.
 *   B382 SwarmSynth's divergences (M1-M3, default ON, docs/port/divergences.json) are WRITTEN OFF
 *       (SWARM_OFF) in every row that compares the swarm with SwarmSynth or DynSynth (O1, O3, GRAV) or
 *       pins a render taken before them (ZERO, AA0, AA3's broad#828 evidence): those rows keep proving
 *       what they were written for, against the engine they were measured on. Each divergence's own
 *       rows (M1, M2, M3) run it ON and check it against the C++ law, flag off against SwarmSynth.
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
 * WHY IN full, NOT fast: ~30 s of DSP on this Mac since B335 (O1 and GRAV step
 * two engines one sample at a time for their first 4096 samples; O2 renders 24
 * two-note passes at 2x oversampling; ONS renders 44.1 kHz one sample per call),
 * and fast is the seconds-scale leg — station_check's reason.
 * Deterministic, no model calls. Each section prints the previous one's cost.
 * By hand:  node tools/labharness/composed_engine_check.mjs   (exit 1 on any red row)
 * Both references are PROTECTED and are loaded, never edited: SwarmSynth through
 * tools/golden/extract_core.mjs (the golden generator's own loader), RazorCore by
 * require. Math.random is replaced by a seeded mulberry32 around every oracle
 * instance (the lab's convention) and restored on exit.
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { extractCore } from '../golden/extract_core.mjs';
import { clicks, spectrum, mono } from '../patchspace/metrics.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const require = createRequire(import.meta.url);
const RazorCore = require(join(root, 'reference/scalpel/prototype/razor-core.js'));
const ENGINE = join(root, 'docs/design/scalpel-horde-engine.js');
const { makeComposedEngine, swarmSourceFromHtml } = require(ENGINE);
const SWARM_HTML = join(root, 'reference/swarmsaw.html');
const SwarmSynth = extractCore(SWARM_HTML, 'SwarmSynth');
const SWARM_SRC = swarmSourceFromHtml(readFileSync(SWARM_HTML, 'utf8'));
const Composed = makeComposedEngine(RazorCore, SWARM_SRC);
/* B382: every SwarmSynth divergence the engine registers (its SWARM_PATCHES), written OFF (header) */
const SWARM_OFF = Object.fromEntries(Composed.swarmPatches.map(x => [x.flag, 0]));
/* B382: SwarmSynth with some of the engine's divergences ON (their flags, and any key their text reads,
   written into p): the engine's own patched class, Composed.SwarmSynth, never a copy */
const swarmWith = over => class extends Composed.SwarmSynth { constructor(sr) { super(sr); Object.assign(this.p, over); } };

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
function refOf(h, over, cls) {
  const r = new (cls || SwarmSynth)(SR), p = Object.assign({}, h, over || {});
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
    pivotMode: h.pivotMode }, SWARM_OFF, blade || {}));
  Object.assign(c.s, c.t);
  return c;
}

/* ---------------------------------------------------------------- O1 */
function o1(h, opt) {
  opt = opt || {};
  const ref = refOf(h, opt.refOver, opt.refCls);
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

/* ---------------------------------------------------------------- B382 M2 */
/* M2 `onsetBipolar` (docs/port/divergences.json M2, default ON): ADR-056's bipolar onset, the C++'s
   law (swarm_core.h :635 Kenv = 8·onset·|onset|; :1894-1895 the sign routed, max(0, Kenv) to sync,
   max(0, -Kenv)·3 to splay), in place of SwarmSynth's 8·onset² (the FINDING above). THE C++ IS THE LAW:
     M2   Kenv at note-on is the C++'s 8·o·|o| for o from -1 to 1, flag on; flag off it is SwarmSynth's 8·o².
     M2s  the swarm half with M2 on is SwarmSynth with M2 on (the engine's own patched class), exactly;
          at onset >= 0 it is SwarmSynth itself, exactly (ADR-056: a superset, bit-inert there); the
          CONTROL is the same comparison at negative onset, which must differ.
     M2b  ADR-056's behavioural anchor (ACCEPTANCE L0-24, trajectory_check's): a negative onset is a SPLAY
          burst, so the early order parameter R under onset -1 sits well under onset +1's; the CONTROL,
          flag off, is SwarmSynth's symmetric burst (R equal to the bit). */
section('M2 — B382: bipolar onset (ADR-056) mirrored into the composed engine');
{
  const Bi = swarmWith({ onsetBipolar: 1 }), kenv = (cls, o) => { const r = new cls(SR); r.setParam('onset', o); r.noteOn(NOTE, F57); return r.swarms[0].Kenv; };
  const OS = [-1, -0.6, -0.2, 0, 0.3, 1];
  const on = OS.map(o => kenv(Bi, o)), off = OS.map(o => kenv(SwarmSynth, o));
  row(OS.every((o, k) => on[k] === 8 * o * Math.abs(o) && off[k] === 8 * o * o), 'M2',
    `Kenv at note-on, onset ${OS.join(' / ')}: ${on.map(x => x.toFixed(3)).join(' / ')} = the C++'s 8·o·|o|; flag off ${off.map(x => x.toFixed(3)).join(' / ')} = SwarmSynth's 8·o²`);

  const cases = [['onset -1 K .2', { K: 0.2, onset: -1, dissolve: 0.3 }], ['onset -.5 K .35', { K: 0.35, onset: -0.5, dissolve: 0.5 }],
    ['onset -1 K 0', { K: 0, onset: -1, dissolve: 0.6, cents: 40 }], ['onset -.6 K -.5', { K: -0.5, onset: -0.6, dissolve: 0.4, retrig: 0 }],
    ['onset +.5 K .35', { K: 0.35, onset: 0.5, dissolve: 0.5 }], ['onset +1 K .2', { K: 0.2, onset: 1, dissolve: 0.3 }]];
  let worst = 0;
  const moved = [], inert = [];
  for (const [name, over] of cases) {
    const h = Object.assign({}, H0, over), r = o1(h, { blade: { onsetBipolar: 1 }, refCls: Bi }), d = o1(h, { blade: { onsetBipolar: 1 } });
    worst = Math.max(worst, r.ph, r.eff);
    (over.onset < 0 ? moved : inert).push(`${name} ${d.ph.toExponential(1)}`);
    if (over.onset >= 0 && (d.ph !== 0 || d.eff !== 0)) row(false, 'M2s', `${name}: M2 on moved a non-negative onset (max|Δφ| ${d.ph}) — ADR-056 is bit-inert there`);
    if (over.onset < 0 && d.ph < 1e-3) row(false, 'M2sc', `CONTROL ${name}: M2 on did not move a negative onset against SwarmSynth (max|Δφ| ${d.ph})`);
  }
  row(worst === 0, 'M2s', `${cases.length} scenarios at 48 kHz, M2 on: member phases and frequencies = SwarmSynth with M2 on, exactly (max ${worst}); against SwarmSynth itself: ${inert.join(' · ')} (onset >= 0, must be 0)`);
  row(moved.length === cases.filter(c => c[1].onset < 0).length, 'M2sc', `CONTROL the negative onsets against SwarmSynth itself: ${moved.join(' · ')} — each must move`);

  /* ADR-056's anchor: early R (at 60 ms), onset -1 against +1, K .9 as trajectory_check's L0-24 */
  const early = (cls, o, flag) => o1(Object.assign({}, H0, { K: 0.9, onset: o, dissolve: 0.3 }), { blade: { onsetBipolar: flag }, refCls: cls, seconds: 0.06 }).R;
  const splay = early(Bi, -1, 1), sync = early(Bi, 1, 1), s0 = early(SwarmSynth, -1, 0), p0 = early(SwarmSynth, 1, 0);
  row(sync - splay > 0.3, 'M2b', `R at 60 ms, K .9: onset -1 ${splay.toFixed(3)} (a splay burst) vs +1 ${sync.toFixed(3)} (sync) — must differ by more than 0.3`);
  row(s0 === p0, 'M2bc', `CONTROL flag off (SwarmSynth's 8·o²): onset -1 ${s0.toFixed(6)} vs +1 ${p0.toFixed(6)} — the same burst, to the bit`);
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
     blade frame ½, §1.6.6) and its glide snaps. The reading before the
     steal is the must-read-non-zero twin: the slot was running, off that start.
     EVIDENCE OF THE RE-STRIKE (B325). This row read `vfInit === 0` (the snap still
     pending) until B325 moved the note's first swarm tick into startVoice, which takes
     the snap at the strike itself and leaves vfInit 1. The re-strike is now read
     directly: the swarm holds the NEW note (midi 70) as SwarmSynth's newest strike
     (age = noteCounter − 1), and its first tick was taken at the strike (tick0), or
     is still pending (vfInit 0). This is stricter than the old reading, not looser. */
  const off = (c, slot) => { let m = 0; for (let i = 0; i < c.d.N; i++) m = Math.max(m, Math.abs(c.voices[slot].m[i].phi - 0.5)); return m; };
  const steal = (oracle, ev, upTo) => {
    const c = vlMake(oracle);
    play(c, ev, upTo);                                  // every event lands before the last block
    const pre = snap(c), was = pre.map((_, i) => off(c, i));
    c.noteOn(70, hz(70), 0.9);                          // the steal, probed before any render
    const post = snap(c), slot = post.findIndex(v => v.note === 70);
    const S = oracle ? null : c.sw.swarms[c.voices[slot].si];
    const fresh = !oracle && off(c, slot) === 0 && S.midi === 70 && S.age === c.sw.noteCounter - 1 && (S.vfInit === 0 || c.voices[slot].tick0 === true);
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

/* ---------------------------------------------------------------- B323 cull */
/* THE CULL (B323, engine cull()): over a host-set voice cap, the quietest RELEASING tails fade
   out fast by ADR-083's tiers 1 and 2; a held voice is never culled. Each row reads the pool
   after a cap is sent; the must-fail control is a culler that ignores the tiers (the oldest
   sounding voice, held or not) run through the same detector. The survivors are compared with
   an uncapped twin of the same run: the cull may touch the culled voice and nothing else. */
section('CULL — the voice cap (B323): quietest tails first by ADR-083\'s tiers, never a held note');
{
  const hz = n => 440 * Math.pow(2, (n - 69) / 12);
  const make = cls => {
    Math.random = mulberry32(0xB323);
    const c = new (cls || Composed)(SR);
    c.set({ N: 5, detune: 14, K: 0.35, phaseMode: 1 }); Object.assign(c.s, c.t);   // poly 6, R 280 ms (defaults)
    return c;
  };
  const play = (c, ev, total, probe) => {
    const L = new Float32Array(128), R = new Float32Array(128);
    for (let i = 0, e = 0; i < total; i += 128) {
      while (e < ev.length && ev[e][0] <= i) { const [, k, n] = ev[e++]; if (k === 'on') c.noteOn(n, hz(n), 0.9); else c.noteOff(n); }
      c.render(L, R);
      if (probe) probe();
    }
  };
  const pool = c => c.voices.slice(0, c.d.poly);
  const state = c => pool(c).map(v => ({ note: v.note, gate: v.gate, active: v.active, env: v.env, cull: !!v.cull }));
  /* 60-62 HELD (the oldest); 63-65 released newest-first, so the quietest tail (65) is the youngest */
  const EV = [[0, 'on', 60], [128, 'on', 61], [256, 'on', 62], [384, 'on', 63], [512, 'on', 64], [640, 'on', 65],
    [9600, 'off', 65], [11520, 'off', 64], [13440, 'off', 63]];
  const T0 = 16896;
  /* the run: EV, then `cap` sent, then `after` samples; the twin is the same run uncapped */
  const run = (cap, after, cls) => {
    const c = make(cls), twin = make();
    play(c, EV, T0); play(twin, EV, T0);
    const pre = state(c);
    c.msg({ t: 'cap', n: cap });
    const envs = [];
    const L = new Float32Array(8), R = new Float32Array(8), L2 = new Float32Array(8), R2 = new Float32Array(8);
    let dSurv = 0;
    for (let i = 0; i < after; i += 8) {
      c.render(L, R); twin.render(L2, R2);
      envs.push(pool(c).map(v => v.env));
      pool(c).forEach((v, k) => {
        if (pre[k].active && !v.cull && v.active) {
          const w = twin.voices[k];
          dSurv = Math.max(dSurv, Math.abs(v.env - w.env));
          for (let q = 0; q < c.d.N; q++) dSurv = Math.max(dSurv, Math.abs(v.m[q].phi - w.m[q].phi));
        }
      });
    }
    return { c, pre, post: state(c), envs, dSurv, culled: c.culled };
  };
  const gone = (r, note) => { const k = r.pre.findIndex(v => v.note === note); return !r.post[k].active; };
  const kept = (r, notes) => notes.every(n => { const k = r.pre.findIndex(v => v.note === n); return r.post[k].active; });
  const heldOk = r => [60, 61, 62].every(n => { const k = r.pre.findIndex(v => v.note === n); return r.post[k].active && r.post[k].gate; });
  {
    const r = run(5, 512);
    const tails = r.pre.filter(v => !v.gate).map(v => `${v.note} ${v.env.toFixed(3)}`).join(' · ');
    row(r.culled === 1 && gone(r, 65) && kept(r, [63, 64]) && heldOk(r) && r.dSurv === 0, 'CULL',
      `cap 5 over 6 sounding (tails ${tails}): culled ${r.culled}, 65 (the quietest tail, the youngest voice) gone in 512 samples; ` +
      `63/64 and held 60-62 kept; survivors vs the uncapped twin max|Δ env, φ| ${r.dSurv.toExponential(1)} — must be 0`);
  }
  {
    const r = run(2, 512);
    const live = r.post.filter(v => v.active).length;
    row(r.culled === 3 && [63, 64, 65].every(n => gone(r, n)) && heldOk(r) && live === 3, 'CULL',
      `cap 2 under 3 held notes: every tail culled (${r.culled}), all 3 held kept gated; ${live} sounding > cap — a held note is never culled`);
  }
  {
    /* the fade (the lead's addendum: never a cut): the culled tail's gain, env over the uncapped
       twin's env, ramps LINEARLY 1 → 0 over CULL_FADE (8 ms = 384 samples), one step a sample */
    const c = make(), twin = make();
    play(c, EV, T0); play(twin, EV, T0);
    const k = pool(c).findIndex(v => v.note === 65), e0 = c.voices[k].env;
    c.msg({ t: 'cap', n: 5 });
    const L = new Float32Array(1), R = new Float32Array(1), L2 = new Float32Array(1), R2 = new Float32Array(1);
    let end = -1, worst = 0, maxStep = 0, prevG = 1;
    for (let i = 1; i <= 512 && end < 0; i++) {
      c.render(L, R); twin.render(L2, R2);
      const g = c.voices[k].active ? c.voices[k].env / twin.voices[k].env : 0;
      if (!c.voices[k].active) end = i;
      else worst = Math.max(worst, Math.abs(g - (1 - i / (0.008 * SR))));
      maxStep = Math.max(maxStep, prevG - g); prevG = g;
    }
    row(end > 0.005 * SR && end <= 0.010 * SR + 1 && worst < 1e-9 && maxStep < 1.01 / (0.008 * SR), 'CULL',
      `the culled tail FADES, never cuts: env ${e0.toFixed(3)}, gain vs the uncapped twin follows 1 − t/8 ms within ${worst.toExponential(1)}, ` +
      `largest step ${maxStep.toExponential(2)} a sample (1/384), freed after ${end} samples (${(end / SR * 1000).toFixed(2)} ms, in the 5-10 ms asked)`);
  }
  {
    /* tier 1 before tier 2 (VL1's scenario): 60/61 both faded (env < 1e-3), 60 OLDER and LOUDER. One cull takes 60 */
    const c = make();
    play(c, [[0, 'on', 60], [128, 'on', 61], [256, 'on', 62], [384, 'on', 63], [512, 'on', 64], [640, 'on', 65],
      [9600, 'off', 61], [12032, 'off', 60]], 36096);
    const pre = state(c);
    c.msg({ t: 'cap', n: 5 });
    c.cull();                                            // the pick itself, read before any fade
    const cut = pool(c).findIndex(v => v.cull);
    row(c.culled === 1 && pre[cut] && pre[cut].note === 60 && pre[0].env > pre[1].env, 'CULL',
      `tier 1 first: tails 60/61 env ${pre[0].env.toExponential(2)}/${pre[1].env.toExponential(2)} (both faded); the cull took ${pre[cut] ? pre[cut].note : '—'} (the OLDEST faded)`);
  }
  {
    /* at the cap a note-on REPLACES a sounding voice: 3 held, 2 tails and one never-used slot, cap 5 */
    const ev = EV.filter(e => e[2] !== 65);
    const at = cap => { const c = make(); play(c, ev, T0); if (cap) c.msg({ t: 'cap', n: cap }); const pre = state(c); c.noteOn(70, hz(70), 0.9); return { pre, slot: state(c).findIndex(v => v.note === 70) }; };
    const capd = at(5), free = at(0);
    row(capd.pre[capd.slot].active && capd.pre[capd.slot].note === 64 && !free.pre[free.slot].active && capd.slot !== free.slot, 'CULL',
      `note-on at the cap (5 sounding, cap 5) takes slot ${capd.slot} (was ${capd.pre[capd.slot].note}, the quietest tail) instead of the free slot ${free.slot} it takes uncapped`);
  }
  {
    /* no-op: cap 0 (off) and a cap above the live count render the uncapped samples exactly */
    const out = cap => { const c = make(), L = new Float32Array(4096), R = new Float32Array(4096); play(c, EV, T0); if (cap) c.msg({ t: 'cap', n: cap }); c.render(L, R); return { L, R }; };
    const a = out(0), b = out(6), cOff = out(0);
    row(maxDiff(a, b) === 0 && maxDiff(a, cOff) === 0 && rms(a) > 1e-4, 'CULL',
      `cap 6 over 6 sounding, and cap 0 (off): max|Δ| vs uncapped ${maxDiff(a, b).toExponential(1)} — the cap costs nothing until it binds`);
  }
  {
    /* NO NEW CLICKS (the lead's addendum, 2026-09-28: the human hears "more noise and clicks that I'm
       not certain are supposed to be part of the waveforms"). B316's click metric
       (tools/patchspace/metrics.mjs clicks: 512-sample frames of the second difference, a click is a
       frame > 20 dB over the median) on the mono sum, a render with the cull forced against the same
       render uncapped. Two patches: a PURE SINE (N 1, blades off, no detune), where any step stands
       far above the median, so the metric is at its most sensitive; and Crushed bells, the heavy class
       (B313), from the packet's presets (read, never edited). MUST-FAIL CONTROL: the same cull as an
       INSTANT cut (a culler that zeroes the voice at once) through the same metric. */
    const PRESETS = JSON.parse(readFileSync(join(root, 'reference/scalpel/data/presets.json'), 'utf8')).presets;
    const bells = PRESETS.find(p => p.name === 'Crushed bells');
    class Cut extends Composed { cull() { super.cull(); for (const v of this.voices) if (v.cull) { v.env = 0; v.active = false; v.cull = false; } } }
    const sine = { N: 1, detune: 0, K: 0, phaseMode: 1, w: 0, b2on: 0, base: 0, xm: 0, fb: 0, dcMode: 0 };
    /* A2, C3 and E3 (low, so the sines' own second difference is small); C3 released at 0.30 s, the
       cap dropped to 2 at 0.312 s, so C3's tail is culled 12 ms into its release, near full level */
    const clickRun = (params, cap, cls) => {
      Math.random = mulberry32(0xC11C);
      const c = new (cls || Composed)(SR); c.set(params); Object.assign(c.s, c.t);
      const total = 48000, L = new Float32Array(total), R = new Float32Array(total), B = 128;
      const ev = [[0, 'on', 45], [0, 'on', 48], [0, 'on', 52], [14400, 'off', 48], [28800, 'off', 52]];
      let e = 0, culledAt = -1;
      for (let i = 0; i < total; i += B) {
        while (e < ev.length && ev[e][0] <= i) { const [, k, n] = ev[e++]; if (k === 'on') c.noteOn(n, hz(n), 0.9); else c.noteOff(n); }
        if (cap && i === 14976) c.msg({ t: 'cap', n: cap });
        const before = c.culled;
        c.render(L.subarray(i, i + B), R.subarray(i, i + B));
        if (culledAt < 0 && c.culled > before) culledAt = i;
      }
      const m = new Float64Array(total); for (let i = 0; i < total; i++) m[i] = 0.5 * (L[i] + R[i]);
      return { m, culled: c.culled, culledAt };
    };
    for (const [name, params] of [['pure sine', sine], ['Crushed bells', Object.assign({}, bells.params)]]) {
      const base = clicks(clickRun(params, 0).m), faded = clickRun(params, 2), cut = clickRun(params, 2, Cut);
      const f = clicks(faded.m), k = clicks(cut.m);
      row(faded.culled === 1 && f.clicks <= base.clicks, 'CULL',
        `${name}: B316's click metric with the cull forced (1 tail culled at ${(faded.culledAt / SR).toFixed(3)} s) reads ${f.clicks} clicks, worst frame ${f.worstDb.toFixed(1)} dB over the median; uncapped ${base.clicks}, ${base.worstDb.toFixed(1)} dB — no new click`);
      if (name === 'pure sine')
        row(cut.culled === 1 && k.clicks > base.clicks, 'CULLc',
          `CONTROL the same cull as an INSTANT cut: ${k.clicks} click(s), worst frame ${k.worstDb.toFixed(1)} dB over the median — the metric must catch it`);
      else note(`${name}, instant cut for scale: ${k.clicks} click(s), worst ${k.worstDb.toFixed(1)} dB (a dense, bright patch masks a step; the sine row is the stringent one)`);
    }
  }
  {
    /* CONTROL: a culler that ignores ADR-083 (the oldest sounding voice, held or not) through the same detector */
    class Naive extends Composed {
      tierPick(pool, skip) { let v = null; for (const x of pool) if (!(skip && skip(x)) && (!v || x.age < v.age)) v = x; return v; }
    }
    const r = run(5, 512, Naive);
    row(!(gone(r, 65) && heldOk(r)), 'CULLc',
      `CONTROL a tier-blind culler (oldest sounding): 65 gone ${gone(r, 65)}, held 60-62 kept ${heldOk(r)} — must fail the CULL detector (it takes held 60)`);
  }
}

/* ---------------------------------------------------------------- B375 cap policy */
/* THE POLICY AT A FULL CAP (B375, engine noteOn, capPolicy). The human, 2026-09-29: "Blocking new
   voices seems like a reasonable policy, though maybe we could include a toggle to test". The case:
   the cap is full and every sounding voice is HELD (no tail for tiers 1-2 to take). REFUSE (0, the
   default) drops the note and touches nothing; STEAL (1, the lab's test toggle) releases the oldest
   held voice into the cull's 8 ms fade and plays the note in a free slot; REPLACE (2) is B323's law as
   built. Each row has a must-fail control run through the same detector. */
section('CAPPO — the full-cap policy (B375): refuse by default, steal (faded) as a test toggle');
{
  const hz = n => 440 * Math.pow(2, (n - 69) / 12);
  const make = (cls, policy) => {
    Math.random = mulberry32(0xB375);
    const c = new (cls || Composed)(SR);
    c.set({ N: 5, detune: 14, K: 0.35, phaseMode: 1 }); Object.assign(c.s, c.t);   // poly 6 (default)
    if (policy !== undefined) c.msg({ t: 'capPolicy', n: policy });
    return c;
  };
  /* 60, 61, 62 struck and HELD (60 the oldest), 4096 samples in: past the attack, nothing released */
  const HELD = [[0, 60], [128, 61], [256, 62]];
  const T0 = 4096;
  const prime = c => {
    const L = new Float32Array(128), R = new Float32Array(128);
    for (let i = 0, e = 0; i < T0; i += 128) { while (e < HELD.length && HELD[e][0] <= i) { const n = HELD[e++][1]; c.noteOn(n, hz(n), 0.9); } c.render(L, R); }
  };
  const pool = c => c.voices.slice(0, c.d.poly);
  const snap = c => pool(c).map(v => ({ note: v.note, gate: v.gate, active: v.active, env: v.env, cull: !!v.cull, age: v.age }));
  const renderN = (c, n) => { const L = new Float32Array(n), R = new Float32Array(n); c.render(L, R); return { L, R }; };
  const slotOf = (s, note) => s.findIndex(v => v.active && v.note === note);
  /* REFUSE: the note-on at a full cap vs a twin that never received it; the held voices, the pool and
     the samples must be exactly the twin's, and the note must be nowhere */
  const refuseRun = (cls, policy) => {
    const c = make(cls, policy), twin = make(cls, policy);
    prime(c); prime(twin);
    c.msg({ t: 'cap', n: 3 }); twin.msg({ t: 'cap', n: 3 });
    const pre = snap(c);
    c.noteOn(70, hz(70), 0.9);
    const post = snap(c), a = renderN(c, 4096), b = renderN(twin, 4096);
    const same = JSON.stringify(post) === JSON.stringify(pre) && JSON.stringify(snap(c)) === JSON.stringify(snap(twin));
    return { c, pre, post, same, d: maxDiff(a, b), rms: rms(a), has70: slotOf(snap(c), 70) >= 0 };
  };
  {
    const dflt = make().capPolicy;
    const r = refuseRun();
    row(dflt === 0 && r.c.refused === 1 && r.c.stolen === 0 && r.same && r.d === 0 && r.rms > 1e-4 && !r.has70, 'CAPPO',
      `REFUSE is the default (capPolicy ${dflt}): cap 3 full of held 60-62, note-on 70 refused (${r.c.refused}); held voices and pool state identical ${r.same}, ` +
      `4096 samples vs a twin that never got the note max|Δ| ${r.d.toExponential(1)} (must be 0), 70 sounding ${r.has70}`);
    /* CONTROL: B323's law (policy 2, REPLACE) through the same detector: it re-strikes held 60's slot */
    const z = refuseRun(undefined, 2);
    const k60 = z.pre.findIndex(v => v.note === 60);
    row(!(z.same && z.d === 0 && !z.has70) && z.post[k60].note === 70, 'CAPPc',
      `CONTROL REPLACE (policy 2, B323's law as built): 70 takes held 60's slot ${k60} at once (now ${z.post[k60].note}); samples differ ${z.d.toExponential(1)} — must fail the REFUSE detector`);
  }
  /* STEAL: the oldest held (60) released into the 8 ms fade; the twin gets 60's note-off at the same
     sample instead, so the stolen voice's gain against the twin's is the ramp alone. 1-sample calls */
  const stealRun = cls => {
    const c = make(cls, 1), twin = make(cls, 1);
    prime(c); prime(twin);
    c.msg({ t: 'cap', n: 3 }); twin.msg({ t: 'cap', n: 3 });
    const pre = snap(c), k = pre.findIndex(v => v.note === 60);
    c.noteOn(70, hz(70), 0.9); twin.noteOff(60);
    const k70 = slotOf(snap(c), 70);
    const L = new Float32Array(1), R = new Float32Array(1), L2 = new Float32Array(1), R2 = new Float32Array(1);
    let end = -1, worst = 0, maxStep = 0, prevG = 1, dSurv = 0;
    for (let i = 1; i <= 1024; i++) {
      c.render(L, R); twin.render(L2, R2);
      if (end < 0) {
        const g = c.voices[k].active ? c.voices[k].env / twin.voices[k].env : 0;
        if (!c.voices[k].active) end = i;
        else worst = Math.max(worst, Math.abs(g - (1 - i / (0.008 * SR))));
        maxStep = Math.max(maxStep, prevG - g); prevG = g;
      }
      for (const n of [61, 62]) {
        const q = pre.findIndex(v => v.note === n), v = c.voices[q], w = twin.voices[q];
        dSurv = Math.max(dSurv, Math.abs(v.env - w.env), v.gate === w.gate && v.active === w.active ? 0 : 1);
        for (let m = 0; m < c.d.N; m++) dSurv = Math.max(dSurv, Math.abs(v.m[m].phi - w.m[m].phi));
      }
    }
    const post = snap(c);
    return { c, k, k70, end, worst, maxStep, dSurv, newOk: k70 >= 0 && k70 !== k && !pre[k70].active && post[k70].gate && post[k70].active };
  };
  {
    const r = stealRun();
    row(r.c.stolen === 1 && r.c.refused === 0 && r.k >= 0 && r.newOk && r.end > 0.005 * SR && r.end <= 0.010 * SR + 1 && r.worst < 1e-9 && r.maxStep < 1.01 / (0.008 * SR) && r.dSurv === 0, 'CAPPO',
      `STEAL (policy 1): note-on 70 at a full cap 3 steals held 60 (the oldest, slot ${r.k}) and sounds in free slot ${r.k70} (${r.newOk}); 60's gain vs a twin released at the same sample follows 1 − t/8 ms within ${r.worst.toExponential(1)}, ` +
      `largest step ${r.maxStep.toExponential(2)} a sample, freed after ${r.end} samples (${(r.end / SR * 1000).toFixed(2)} ms); held 61/62 vs the twin max|Δ env, φ| ${r.dSurv.toExponential(1)} (must be 0)`);
    /* CONTROL: the same steal as an INSTANT cut (the stolen voice zeroed at once) through the same detector */
    class CutSteal extends Composed { noteOn(n, f, v) { super.noteOn(n, f, v); for (const x of this.voices) if (x.cull) { x.env = 0; x.active = false; x.cull = false; } } }
    const z = stealRun(CutSteal);
    row(!(z.end > 0.005 * SR && z.worst < 1e-9), 'CAPPc',
      `CONTROL the steal as an instant cut: freed after ${z.end} sample(s), ramp error ${z.worst.toExponential(1)} — must fail the STEAL detector's fade`);
  }
  {
    /* STEAL with the pool FULL of held voices (6 held, cap 6): no free slot, so it is horde's own
       full-pool law (ADR-083 tier 3, at once), as documented in noteOn; nothing is faded */
    const c = make(undefined, 1);
    const L = new Float32Array(128), R = new Float32Array(128);
    for (let n = 60; n < 66; n++) { c.noteOn(n, hz(n), 0.9); c.render(L, R); }
    c.msg({ t: 'cap', n: 6 });
    const pre = snap(c), k60 = pre.findIndex(v => v.note === 60);
    c.noteOn(70, hz(70), 0.9);
    const post = snap(c);
    row(post[k60].note === 70 && post[k60].gate && c.stolen === 0 && c.refused === 0 && post.every(v => !v.cull), 'CAPPO',
      `STEAL with no free slot (6 held, pool 6, cap 6): 70 re-strikes the oldest held slot ${k60} (now ${post[k60].note}), stolen ${c.stolen}, nothing fading — horde's full-pool law, as uncapped`);
  }
  {
    /* BELOW THE CAP NOTHING CHANGES: 3 held, cap 5 (does not bind), note-on 70, under refuse and under
       steal, against no cap at all: the same samples, bit for bit */
    const out = (cap, policy) => { const c = make(undefined, policy); prime(c); if (cap) c.msg({ t: 'cap', n: cap }); c.noteOn(70, hz(70), 0.9); const o = renderN(c, 4096); o.c = c; return o; };
    const off = out(0), ref = out(5, 0), stl = out(5, 1);
    row(maxDiff(off, ref) === 0 && maxDiff(off, stl) === 0 && rms(off) > 1e-4 && ref.c.refused === 0 && stl.c.stolen === 0, 'CAPPO',
      `below the cap (4 sounding after the note, cap 5): refuse and steal vs no cap max|Δ| ${maxDiff(off, ref).toExponential(1)} / ${maxDiff(off, stl).toExponential(1)} — bit-identical`);
    /* CONTROL: the same comparison with the cap binding (cap 3) must differ, under either policy */
    const bRef = out(3, 0), bStl = out(3, 1);
    row(maxDiff(off, bRef) > 1e-4 && maxDiff(off, bStl) > 1e-4, 'CAPPc',
      `CONTROL the cap binding (cap 3): refuse differs from no cap by ${maxDiff(off, bRef).toFixed(4)}, steal by ${maxDiff(off, bStl).toFixed(4)} — the comparison sees a policy that acts`);
  }
}

/* ---------------------------------------------------------------- ADR-184 A2 (2) */
/* THE EXACT CUT-SPREAD MIRROR UNDER QUANTIZE: every member's snapped offset at −x is exactly
   minus its offset at +x, half-integers included; every value but a negative half is
   bit-identical to the oracle. The must-fail control is the oracle's own Math.round. */
section('KQ — ADR-184 A2 (2): snapped Cut spread rounds half away from zero, the oracle elsewhere');
{
  const offs = (cls, x, two) => {
    Math.random = mulberry32(0xA2);
    const c = new cls(SR);
    c.set({ N: 5, law: 0, kq: 1, kRule: 0, kRule2: 0, b2sp: two ? 1 : 0, kspread: x, kspread2: two ? x * 0.6 : 0 });   // blade 2 at 0.6x: ±1.5 and ±4.5 are halves too
    Object.assign(c.s, c.t);
    c.noteOn(57, F57, 1);
    return c.voices[0].m.slice(0, 5).map(m => [m.kAdd, m.kAdd2]);
  };
  const mirr = (cls, x, two) => { const a = offs(cls, x, two), b = offs(cls, -x, two); let e = 0; for (let i = 0; i < 5; i++) for (let j = 0; j < 2; j++) e = Math.max(e, Math.abs(a[i][j] + b[i][j])); return e; };
  const same = (x, two) => { const a = offs(Composed, x, two), b = offs(RazorCore, x, two); let e = 0; for (let i = 0; i < 5; i++) for (let j = 0; j < 2; j++) if (a[i][j] !== b[i][j]) e++; return e; };
  const HALF = [0.5, 1.5, 2.5, 7.5, 11.5], PLAIN = [2.3, 6.7, 3, 12, 0.49];
  let worst = 0; for (const x of HALF.concat(PLAIN)) { worst = Math.max(worst, mirr(Composed, x, false), mirr(Composed, x, true)); }
  row(worst === 0, 'KQ', `mirror at ±{${HALF.concat(PLAIN).join(', ')}}, both blades (blade 2 owning its spread): max|kAdd(−x) + kAdd(+x)| ${worst} — must be 0`);
  let diff = 0; const tested = [];
  for (const x of PLAIN.concat(PLAIN.map(v => -v), HALF)) { diff += same(x, false) + same(x, true); tested.push(x); }
  row(diff === 0, 'KQ', `bit-identical to the oracle except the negative halves: ${tested.length} values incl. every positive half, ${diff} members differ`);
  const at = offs(Composed, -2.5, false).map(v => v[0]), ot = offs(RazorCore, -2.5, false).map(v => v[0]);
  row(at[0] === 2 && ot[0] === 1, 'KQ', `−2.5 at the gradient's first member (pn −½): composed ${at[0]} (−3 · −½ = 1.5, snapped to 2: the mirror of +2.5's −2), oracle ${ot[0]} (Math.round(−2.5) = −2)`);
  const oc = mirr(RazorCore, 2.5, false);
  row(oc >= 0.5, 'KQc', `CONTROL the oracle's Math.round at ±2.5: max|kAdd(−x) + kAdd(+x)| ${oc} — must be ≥ 0.5 (a step off)`);
}

/* ---------------------------------------------------------------- first-sample frequencies (B325) */
/* The B325 fidelity audit found the one composed-engine-introduced defect: until the swarm's
   first tick reached couple() (its 32-sample pass), RazorCore read the UNDETUNED pitch as every
   member's frequency (m.inc), and m.inc feeds the Hz-unit cut rate (lock 2), the Hz-unit
   modulator (mUnit) and the per-cycle DC estimate taken on the note's first sample. In the
   neutral case (K 0, the aligned start, where the composed engine must equal the oracle) it
   read a slow offset of up to 2.5e-2 per note on Formant pluck (lock 2) and a modulator phase
   offset that never recovered on Crunch horde (mUnit 1). The fix takes the first tick in
   startVoice. The rows: m.inc is the swarm's own frequency before any render, and those two
   presets are exact in the neutral case. The MUST-FAIL control rebuilds the old placeholder
   (no early tick) and must read the defect through the same detectors. */
section('B325 — the blade reads the swarm\'s member frequencies from the first sample; the old placeholder as the must-fail control');
{
  /* the pre-B325 behaviour, rebuilt: no tick in startVoice, so couple() hands RazorCore the pitch */
  class Placeholder extends Composed {
    startVoice(v, n, f, vel, fresh, re) { this.noEarly = true; super.startVoice(v, n, f, vel, fresh, re); this.noEarly = false; v.tick0 = false; }
    tickSwarm(v, S) { if (!this.noEarly) super.tickSwarm(v, S); }
  }
  class Shifted extends RazorCore { startVoice(v, n, f, vel, fresh, re) { super.startVoice(v, n, f, vel, fresh, re); if (fresh) for (const m of v.m) m.phi = 0.5; } }
  const incGap = Cls => {
    Math.random = mulberry32(0xB325);
    const c = new Cls(SR); c.set(Object.assign({}, byName('Formant pluck'), { K: 0.5 })); Object.assign(c.s, c.t);
    c.noteOn(NOTE, F57, 0.9);
    const v = c.voices[0], S = c.sw.swarms[v.si]; let g = 0;
    for (let i = 0; i < c.d.N; i++) g = Math.max(g, Math.abs(v.m[i].inc - S.eff[i]));
    return { g, vfInit: S.vfInit };
  };
  /* the neutral case: four notes on a fresh pool (every law allocates slots 0..3 alike), 0.5 s */
  const neutral = (Cls, name, ev, over) => {
    ev = ev || [[0, 57], [3072, 64], [6144, 60], [9216, 67]];
    const go = (make) => {
      Math.random = mulberry32(0xB325);
      const c = make(); c.set(Object.assign({}, byName(name), { K: 0, phaseMode: 1 }, over || {})); Object.assign(c.s, c.t);
      const L = new Float32Array(24064), R = new Float32Array(24064);
      let e = 0;
      for (let i = 0; i < L.length; i += 128) {
        while (e < ev.length && ev[e][0] <= i) { const n = ev[e++][1]; c.noteOn(n, 440 * Math.pow(2, (n - 69) / 12), 0.9); }
        c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
      }
      return { L, R };
    };
    return maxDiff(go(() => new Shifted(SR)), go(() => new Cls(SR)));
  };
  const a = incGap(Composed), b = incGap(Placeholder);
  row(a.g === 0 && a.vfInit === 1, 'B325', `after noteOn, before any render (Formant pluck, K 0.5): max|m.inc − S.eff| ${a.g.toExponential(1)} Hz, swarm ticked ${a.vfInit === 1}`);
  const fp = neutral(Composed, 'Formant pluck'), ch = neutral(Composed, 'Crunch horde');
  row(fp === 0 && ch === 0, 'B325', `neutral case (K 0, aligned) ≡ oracle: Formant pluck (lock 2, Hz cut rate) max|Δ| ${fp.toExponential(1)} · Crunch horde (mUnit 1, Hz modulator) max|Δ| ${ch.toExponential(1)}`);
  const fpc = neutral(Placeholder, 'Formant pluck'), chc = neutral(Placeholder, 'Crunch horde');
  row(b.g > 0.1 && fpc > 1e-3 && chc > 1e-3, 'B325c', `CONTROL the old placeholder (no early tick): max|m.inc − S.eff| ${b.g.toFixed(3)} Hz; neutral max|Δ| Formant pluck ${fpc.toExponential(1)}, Crunch horde ${chc.toExponential(1)} — must be large`);
  /* the case that caught the fix's first attempt, which KEPT the look-ahead tick and skipped the
     scheduled one: a chord struck on a mono voice (four note-ons in one block) froze the swarm
     at the FIRST note's pitch for 16 samples, a phase offset that never recovered. The look-
     ahead is now undone before the real first tick, and re-taken when the pitch moves. Glide off
     (1 ms): a glide is stepped at the swarm's tick by design (G3), so it could not be exact. */
  class FirstTry extends Composed {
    lookAhead(v, S, first) { if (first) { this.tickSwarm(v, S); v.tick0 = true; } }
    unLookAhead(v) { v.tick0 = false; v.skip0 = true; }
    tickSwarm(v, S) { if (v.skip0) { v.skip0 = false; return; } super.tickSwarm(v, S); }
  }
  const monoChord = [[0, 48], [0, 55], [0, 60], [0, 64]];
  const mz = neutral(Composed, 'Zap bass', monoChord, { glide: 1 }), ms = neutral(Composed, 'Screamer', monoChord, { glide: 1 });
  row(mz === 0 && ms === 0, 'B325', `mono chord, four note-ons in one block (legato presets, glide off), neutral case ≡ oracle: Zap bass max|Δ| ${mz.toExponential(1)} · Screamer ${ms.toExponential(1)}`);
  const fz = neutral(FirstTry, 'Zap bass', monoChord, { glide: 1 });
  row(fz > 1e-3, 'B325c', `CONTROL the fix's first attempt (look-ahead kept, scheduled tick skipped): Zap bass mono chord max|Δ| ${fz.toExponential(1)} — must be large`);
}

/* ---------------------------------------------------------------- B335 gravity */
/* CONSONANCE GRAVITY (ADR-008; ADR-086 and Amendment 1), composed in B335. The reference is
   DynSynth (reference/swarmdynamics.html), loaded here by the golden generator's own loader and
   run beside the composed engine with the same swarm (N members, even distribution, law 0, K ≥ 0:
   the laws coincide there, O3) and the same seed and notes. Blade off (w 0); the swarm half does
   not read it (O1's blade-independence row). What is compared, every sample of the first 4096 and
   every 32nd after: each note's member phases, member frequencies (DynSynth's vf + couple, the
   engine's S.eff) and the note's f0cur (the pitch gravity moves). The must-fail controls plant
   the two defects ADR-086 and its amendment record: gravity stepped once per render CALL (dt =
   the block), and the grid as 256 SAMPLES at 48 kHz instead of a fixed time. */
section('GRAV — consonance gravity vs DynSynth (reference/swarmdynamics.html), 48 kHz, blade off');
const DynSynth = extractCore(join(root, 'reference/swarmdynamics.html'), 'DynSynth');
const cents = r => 1200 * Math.log2(r);
function gravRun(h, opt) {
  opt = opt || {};
  const ref = new DynSynth(SR);
  const rp = { n: h.n, detune: h.cents / 100, seed: h.seed, retrig: h.retrig, K: h.K, grav: 'refGrav' in opt ? opt.refGrav : h.grav, basin: h.basin };
  for (const k in rp) ref.setParam(k, rp[k]);
  Math.random = mulberry32(0xB335);
  const c = new (opt.cls || Composed)(SR);
  c.set(Object.assign({ N: h.n, detune: h.cents, K: h.K, phaseMode: h.retrig ? 1 : 0, seed: h.seed, grav: h.grav, basin: h.basin, w: 0 }, SWARM_OFF));
  Object.assign(c.s, c.t);
  const total = Math.round((h.seconds || 1) * SR), bufL = new Float32Array(32), bufR = new Float32Array(32), cL = new Float32Array(32), cR = new Float32Array(32);
  const vs = [], rs = [];
  let ph = 0, eff = 0, f0 = 0, done = 0, e = 0;
  const f0c = v => (v.gOn ? v.gf0 : v.freq);
  while (done < total) {
    while (e < h.notes.length && h.notes[e][0] <= done) {
      const f = h.notes[e][1];
      ref.noteOn(60 + e, f); rs.push(ref.swarms[e]);
      c.noteOn(60 + e, f, 1); vs.push(c.voices.reduce((a, v) => (v.age > a.age ? v : a)));
      e++;
    }
    const b = done < 4096 ? 1 : 32;
    ref.render(bufL.subarray(0, b), bufR.subarray(0, b));
    c.render(cL.subarray(0, b), cR.subarray(0, b));
    done += b;
    for (let j = 0; j < vs.length; j++) {
      const S = c.sw.swarms[vs[j].si], rj = rs[j];
      for (let i = 0; i < h.n; i++) {
        ph = Math.max(ph, circ(S.phase[i], rj.phase[i]));
        eff = Math.max(eff, Math.abs(S.eff[i] - (rj.vf[i] + rj.couple[i])));
      }
      f0 = Math.max(f0, Math.abs(f0c(vs[j]) / rj.f0cur - 1));
    }
  }
  const last = vs.length - 1;
  return { ph, eff, f0, c, iv: cents(f0c(vs[last]) / f0c(vs[0])), ivRef: cents(rs[last].f0cur / rs[0].f0cur), moved: vs.some(v => v.gOn), gravN: c.gravN };
}
const G0 = { n: 7, cents: 14, K: 0, retrig: 1, seed: 1234, grav: 1, basin: 35, seconds: 1 };
const A3 = 220, above = c0 => A3 * Math.pow(2, c0 / 1200);
const GRAV = [
  ['fifth +13 c', { notes: [[0, A3], [0, above(713)]] }],
  ['K .35 rand', { K: 0.35, retrig: 0, grav: 0.5, notes: [[0, A3], [0, above(713)]] }],
  ['triad late', { K: 0.2, grav: 0.8, notes: [[0, A3], [2048, above(390)], [4096, above(708)]] }],
  ['12th folded', { K: 0.1, notes: [[0, A3], [0, above(1200 + 715)]] }],
  ['out of basin', { basin: 10, notes: [[0, A3], [0, above(713)]] }],
];
for (const [name, over] of GRAV) {
  const h = Object.assign({}, G0, over);
  const r = gravRun(h);
  const exact = r.ph === 0 && r.eff === 0 && r.f0 === 0;
  const pulls = over.basin === 10 ? !r.moved : r.moved;
  row(exact && pulls, 'GRAV', `${name.padEnd(12)} max|Δφ| ${r.ph.toExponential(1)}  max|Δf| ${r.eff.toExponential(1)} Hz  max|Δf0cur|/f0cur ${r.f0.toExponential(1)}; ` +
    `interval at ${h.seconds} s ${r.iv.toFixed(3)} c (DynSynth ${r.ivRef.toFixed(3)}; ${over.basin === 10 ? 'outside the 10 c basin: unmoved' : 'moved toward the just ratio'})`);
}
{
  const long = gravRun(Object.assign({}, G0, { seconds: 3, notes: [[0, A3], [0, above(713)]] }));
  row(Math.abs(long.iv - cents(1.5)) < 0.05 && long.ph === 0 && long.gravN === 1, 'GRAV',
    `settles: a fifth 13 c sharp reaches ${long.iv.toFixed(4)} c after 3 s (3/2 is ${cents(1.5).toFixed(4)} c), with DynSynth to the bit; readout holds ${long.gravN} pair`);
  const h = Object.assign({}, G0, { notes: [[0, A3], [0, above(713)]] });
  const off = gravRun(h, { refGrav: 0 });
  row(off.ph > 1e-3 && off.f0 > 1e-4, 'GRAVc', `CONTROL DynSynth without gravity: max|Δφ| ${off.ph.toFixed(4)}, max|Δf0cur|/f0cur ${off.f0.toExponential(2)} — must be large (the detector sees gravity)`);
  class PerCall extends Composed {
    render(L, R) { const g = this.d.grav; this.d.grav = 0; super.render(L, R); this.d.grav = g; this.gravityStep(L.length / this.sr); }
  }
  const pc = gravRun(h, { cls: PerCall });
  row(pc.ph > 1e-9 || pc.f0 > 1e-12, 'GRAVc', `CONTROL gravity stepped once per render call (dt = the block, the pre-ADR-086 law): max|Δφ| ${pc.ph.toExponential(2)}, max|Δf0cur|/f0cur ${pc.f0.toExponential(2)} — must be non-zero`);
  class Samples256 extends Composed { constructor(sr) { super(sr); this.gravGrid = 256; } }
  const s256 = gravRun(h, { cls: Samples256 });
  row(s256.ph > 1e-9 || s256.f0 > 1e-12, 'GRAVc', `CONTROL a 256-SAMPLE grid at 48 kHz (the pre-Amendment-1 law; the time grid is ${Math.round(SR * 256 / 44100)}): max|Δφ| ${s256.ph.toExponential(2)}, max|Δf0cur|/f0cur ${s256.f0.toExponential(2)} — must be non-zero`);
}

/* ---------------------------------------------------------------- B335 zero */
/* NEW PARAMETERS AT THEIR DEFAULTS CHANGE NOTHING: three renders (a four-note script with a
   release, 0.5 s) fingerprinted against the engine BEFORE B335 (main at c79be56, the same script
   run on docs/design/scalpel-horde-engine.js as it stood). SHA-256 of the float32 output, first 16
   hex digits. The must-DIFFER twins switch each feature on in the same script. */
section('ZERO — gravity 0 and every scatter 0 are bit-identical to the pre-B335 engine');
{
  const ZERO = [['Glass horde pad', 'acfdf284a132da01'], ['Two blades', '7dbb7b19c2412b11'],
    ['horde rows', '8aecd8e01b3da445']];
  const pOf = name => (name === 'horde rows' ? { N: 7, detune: 28, K: 0.4, phaseMode: 0, driftDepth: 20, onset: 0.8, inertia: 0.6 } : byName(name));
  const fp = (params, cls) => {
    Math.random = mulberry32(0xB335);
    const c = new (cls || Composed)(SR); c.set(Object.assign({}, params, SWARM_OFF)); Object.assign(c.s, c.t);
    const total = 24064, L = new Float32Array(total), R = new Float32Array(total), B = 128;
    const ev = [[0, 'on', 57], [0, 'on', 64], [0, 'on', 69], [9600, 'on', 60], [14336, 'off', 57], [14336, 'off', 64]];
    for (let i = 0, e = 0; i < total; i += B) {
      while (e < ev.length && ev[e][0] <= i) { const [, k, n] = ev[e++]; if (k === 'on') c.noteOn(n, 440 * Math.pow(2, (n - 69) / 12), 0.9); else c.noteOff(n); }
      c.render(L.subarray(i, i + B), R.subarray(i, i + B));
    }
    return createHash('sha256').update(Buffer.from(L.buffer)).update(Buffer.from(R.buffer)).digest('hex').slice(0, 16);
  };
  for (const [name, want] of ZERO) {
    const got = fp(pOf(name)), explicit = fp(Object.assign({}, pOf(name), { grav: 0.004, basin: 35, onsetScatter: 0, onsetAlpha: 0.25, attackScatter: 0, voiceEnv: 0, relScatter: 0 }));
    row(got === want && explicit === want, 'ZERO', `${name.padEnd(16)} ${got} (pre-B335 ${want}); with grav 0.004 (under DynSynth's 0.005) and every scatter 0 written explicitly ${explicit}`);
  }
  const base = pOf('horde rows');
  const twins = [['grav 0.5', { grav: 0.5 }], ['onsetScatter 10', { onsetScatter: 10 }], ['voiceEnv + relScatter .8', { voiceEnv: 1, relScatter: 0.8 }]];
  for (const [tag, over] of twins) {
    const d = fp(Object.assign({}, base, over));
    row(d !== ZERO[2][1], 'ZEROc', `CONTROL ${tag} on the same script: ${d} — must differ from ${ZERO[2][1]}`);
  }
}

/* ---------------------------------------------------------------- B382 M1 */
/* M1 `ksmPerRate` (docs/port/divergences.json M1, default ON): the coupling smoother's per-tick
   coefficient from a time constant in SECONDS, B150's law (swarm_core.h SwarmCore(), :389-391), in
   place of SwarmSynth's literal 0.08 per tick. THE C++ IS THE LAW HERE:
     M1   the coefficient the engine's swarm uses, at five rates, is the C++ constructor's expression over
          the constants READ FROM the lifted core's header (h2/cores/swarm/swarm_core.h kTick,
          kKsmTauSeconds), and 0.08 exactly at 44.1 kHz; flag off it is 0.08 at every rate. (swarm48_check
          compares the same numbers bit for bit with the C++'s own libm.)
     M1t  ADR-009's invariant, measured on the swarm's own state: a one-member swarm under a K step
          (sigma is its 0.08 floor, so the target is exact) reaches 1 - 1/e of its target after the same
          TIME at 44.1, 48 and 96 kHz, within one tick; the CONTROL (flag off) does not.
     M1s  the swarm half with M1 on is SwarmSynth with M1 on (the engine's own patched class), exactly,
          over O1's scenarios; SwarmSynth itself (the flag's 0) is O1. What M1 moves is printed.
     M1z  44.1 kHz: nothing moves, bit for bit, on every ledger preset and O1 scenario (B150's
          special case); the CONTROL is the same comparison at 48 kHz.
     M1p  THE MOVED PINS (at the defaults, 48 kHz): AA0's and ZERO's renders, which SWARM_OFF keeps on
          their pre-B382 pins, are pinned here at the defaults, each with its old pin beside it. */
section('M1 — B382: the per-rate coupling smoother (B150) mirrored into the composed engine');
{
  const hdr = readFileSync(join(root, 'h2/cores/swarm/swarm_core.h'), 'utf8');
  const tau = +(/constexpr double kKsmTauSeconds = ([0-9.eE+-]+);/.exec(hdr) || [])[1], tick = +(/constexpr int kTick = (\d+);/.exec(hdr) || [])[1];
  const cpp = sr => (sr === 44100 ? 0.08 : 1 - Math.exp(-(tick / sr) / tau));
  const rates = [22050, 44100, 48000, 88200, 96000];
  const coef = rates.map(sr => { const c = new Composed(sr); return { sr, on: c.sw.ksmC, want: cpp(sr) }; });
  row(Number.isFinite(tau) && tick === 16 && coef.every(x => x.on === x.want) && coef[1].on === 0.08, 'M1',
    `coefficient = the C++'s (kTick ${tick}, kKsmTauSeconds ${tau} from h2/cores/swarm/swarm_core.h): ${coef.map(x => `${x.sr} ${x.on.toPrecision(6)}`).join(', ')}; 44.1 kHz is 0.08 exactly`);

  /* one member, K 0.5: km = 4·0.5·0.5 = 1, sigma = 0.08 (one voice has no spread), so the target is
     0.08 exactly and KsmS after n ticks is 0.08·(1 - (1-c)^n). The time to 1 - 1/e, in seconds. */
  const settle = (sr, on) => {
    Math.random = mulberry32(0xB382);
    const c = new Composed(sr); c.set({ N: 1, K: 0.5, detune: 0, phaseMode: 1, w: 0, ksmPerRate: on }); Object.assign(c.s, c.t);
    c.noteOn(NOTE, F57, 1);
    const S = c.sw.swarms[c.voices[0].si], L = new Float32Array(16), R = new Float32Array(16);
    for (let n = 1; n < 4000; n++) { c.render(L, R); if (S.KsmS >= 0.08 * (1 - Math.exp(-1))) return { t: n * 16 / sr, tick: 16 / sr }; }
    return { t: NaN, tick: 16 / sr };
  };
  const on = [44100, 48000, 96000].map(sr => settle(sr, 1)), off = [44100, 48000, 96000].map(sr => settle(sr, 0));
  const spread = xs => Math.max(...xs.map(x => x.t)) - Math.min(...xs.map(x => x.t));
  const ms = xs => xs.map(x => (x.t * 1000).toFixed(3)).join(' / ');
  row(spread(on) <= on[0].tick, 'M1t', `K step to 1-1/e at 44.1 / 48 / 96 kHz: ${ms(on)} ms — the same time within one 44.1 kHz tick (${(on[0].tick * 1000).toFixed(3)} ms)`);
  row(spread(off) > 1e-3, 'M1tc', `CONTROL flag off (SwarmSynth's 0.08 per tick): ${ms(off)} ms — must differ by more than 1 ms across the rates`);

  /* the swarm half with M1 on is SwarmSynth with M1 on, exactly; what M1 moves, printed */
  const Mir = swarmWith({ ksmPerRate: 1 });
  let worst = 0, moved = [];
  for (const [name, over] of O1) {
    const h = Object.assign({}, H0, over);
    const r = o1(h, { blade: { ksmPerRate: 1 }, refCls: Mir }), d = o1(h, { blade: { ksmPerRate: 1 } });
    worst = Math.max(worst, r.ph, r.eff);
    if (d.ph > 0) moved.push(`${name.trim()} ${d.ph.toExponential(1)}`);
    if (r.ph !== 0 || r.eff !== 0) row(false, 'M1s', `${name}: max|Δφ| ${r.ph.toExponential(1)} max|Δf| ${r.eff.toExponential(1)} against SwarmSynth with M1 on`);
  }
  row(worst === 0, 'M1s', `${O1.length} O1 scenarios at 48 kHz with M1 on: member phases and frequencies = SwarmSynth with M1 on, exactly (max ${worst})`);
  note(`what M1 moves at 48 kHz (max|Δφ_horde| against SwarmSynth, 0.5 s): ${moved.join(' · ') || 'nothing'}`);
  row(moved.length > 0, 'M1sc', `CONTROL the same scenarios against SwarmSynth itself: ${moved.length} of ${O1.length} move — must be at least one`);

  /* 44.1 kHz: B150's special case, so nothing moves at all */
  const fp44 = (name, on, sr) => {
    Math.random = mulberry32(0xB382);
    const c = new Composed(sr); c.set(Object.assign({}, byName(name), { ksmPerRate: on })); Object.assign(c.s, c.t);
    const n = 12032, L = new Float32Array(n), R = new Float32Array(n);
    c.noteOn(57, F57, 0.9); c.noteOn(64, 440 * Math.pow(2, -5 / 12), 0.9);
    for (let i = 0; i < n; i += 128) c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    return createHash('sha256').update(Buffer.from(L.buffer)).update(Buffer.from(R.buffer)).digest('hex').slice(0, 16);
  };
  const SET = ['Feedback snarl', 'Cross-mod roar', 'Crunch horde', 'Quarter sync', 'Zap bass', 'Glass horde pad', 'Two blades', 'Ring saw'];
  const same44 = SET.filter(n => fp44(n, 1, 44100) === fp44(n, 0, 44100)), diff48 = SET.filter(n => fp44(n, 1, 48000) !== fp44(n, 0, 48000));
  let o144 = 0;
  for (const [, over] of O1) {
    const h = Object.assign({}, H0, over), mk = on => { Math.random = mulberry32(1); const c = new Composed(44100); c.set({ N: h.n, detune: h.cents, K: h.K, phaseMode: h.retrig ? 1 : 0, onset: h.onset, dissolve: h.dissolve, inertia: h.inertia, 'h.law': h.law, driftDepth: h.driftDepth, ksmPerRate: on }); Object.assign(c.s, c.t); c.noteOn(NOTE, F57, 1); return c; };
    const a = mk(1), b = mk(0), L = new Float32Array(441), R = new Float32Array(441), L2 = new Float32Array(441), R2 = new Float32Array(441);
    for (let k = 0; k < 50; k++) { a.render(L, R); b.render(L2, R2); for (let i = 0; i < 441; i++) o144 = Math.max(o144, Math.abs(L[i] - L2[i]), Math.abs(R[i] - R2[i])); }
  }
  row(same44.length === SET.length && o144 === 0, 'M1z', `44.1 kHz, M1 on vs off: ${same44.length} of ${SET.length} presets bit-identical, ${O1.length} O1 scenarios max|Δ| ${o144} (must be 0: B150 keeps 44.1 kHz bit-frozen)`);
  row(diff48.length > 0, 'M1zc', `CONTROL the same presets at 48 kHz: ${diff48.length} of ${SET.length} move (${diff48.join(', ')}) — must be at least one`);

  /* the moved pins: AA0's renders (seed 0xB355, two notes and a release) and ZERO's (seed 0xB335, the
     four-note script) at the DEFAULTS, each beside its pre-B382 pin (which AA0/ZERO keep with M1 off) */
  const fpDefaults = (params, seed, ev) => {
    Math.random = mulberry32(seed);
    const c = new Composed(SR); c.set(params); Object.assign(c.s, c.t);
    const total = 24064, L = new Float32Array(total), R = new Float32Array(total);
    for (let i = 0, e = 0; i < total; i += 128) {
      while (e < ev.length && ev[e][0] <= i) { const [, k, n] = ev[e++]; if (k === 'on') c.noteOn(n, 440 * Math.pow(2, (n - 69) / 12), 0.9); else c.noteOff(n); }
      c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    }
    return createHash('sha256').update(Buffer.from(L.buffer)).update(Buffer.from(R.buffer)).digest('hex').slice(0, 16);
  };
  const EV_AA = [[0, 'on', 57], [9600, 'on', 64], [14336, 'off', 57]];
  const EV_ZERO = [[0, 'on', 57], [0, 'on', 64], [0, 'on', 69], [9600, 'on', 60], [14336, 'off', 57], [14336, 'off', 64]];
  const HORDE_ROWS = { N: 7, detune: 28, K: 0.4, phaseMode: 0, driftDepth: 20, onset: 0.8, inertia: 0.6 };
  /* [what, params, seed, script, the pre-B382 pin (AA0 / ZERO), the pin at the defaults since M1] */
  const M1P = [
    ['AA0 Feedback snarl', byName('Feedback snarl'), 0xB355, EV_AA, '86e1c74e8cb7e644', '86e1c74e8cb7e644'],
    ['AA0 Cross-mod roar', byName('Cross-mod roar'), 0xB355, EV_AA, '03ee212c48e57c19', 'ac31dd847fceb3c3'],
    ['AA0 Crunch horde', byName('Crunch horde'), 0xB355, EV_AA, 'af83ca85d8e03488', 'ad05f1d99219a523'],
    ['AA0 Ring saw', byName('Ring saw'), 0xB355, EV_AA, '629a11c078f055c5', '17e79b2be92ab266'],
    ['AA0 Zap bass', byName('Zap bass'), 0xB355, EV_AA, 'ccf47610c2316f3d', '98395e932d413e1c'],
    ['ZERO Glass horde pad', byName('Glass horde pad'), 0xB335, EV_ZERO, 'acfdf284a132da01', '36691fb1b61c889c'],
    ['ZERO Two blades', byName('Two blades'), 0xB335, EV_ZERO, '7dbb7b19c2412b11', 'cd56de86c91f7241'],
    ['ZERO horde rows', HORDE_ROWS, 0xB335, EV_ZERO, '8aecd8e01b3da445', '7b6aabddf0468f37'],
  ];
  for (const [what, params, seed, ev, was, now] of M1P) {
    const got = fpDefaults(params, seed, ev), off = fpDefaults(Object.assign({}, params, { ksmPerRate: 0 }), seed, ev);
    row(got === now && off === was, 'M1p', `${what.padEnd(21)} at the defaults ${got} (pinned ${now}${now === was ? ', unmoved' : `; pre-B382 ${was}`}); M1 written 0 ${off}`);
  }
}

/* ---------------------------------------------------------------- B335 onset / timing (C++ reference) */
/* ONSET SCATTER, TIMING CORRECTION, ATTACK SCATTER, PER-PARTIAL ENV, RELEASE SCATTER (ADR-077/078,
   B149). There is no JS reference: the C++ (src/swarm_core.h) is the reference, rendered by
   tools/onset_ref_check.cpp into tools/labharness/onset_ref_cpp.json, which `verify full` re-derives
   from the C++ of the day and fails when stale. This compares the engine's transcription with it,
   same seeds, same notes. TOLERANCES, with reasons: the draws are the same expressions over the same
   mulberry32 stream, so they differ only by libm's last ulp (V8's log/cos/exp are not macOS libm's):
   offsets 1e-15 s, onset delays 1e-9 samples, coefficients 1e-11 relative (relC = 1 − exp(−x) at
   x ~ 2e-4 cancels ~12 bits). The wait is an integer count, compared EXACTLY. Phases 1e-9 cycles. */
section('ONS — the ensemble timing, JS transcription vs swarm_core.h (tools/labharness/onset_ref_cpp.json)');
const REF = JSON.parse(readFileSync(join(root, 'tools/labharness/onset_ref_cpp.json'), 'utf8'));
const onsMake = (sr, over, cls) => {
  Math.random = mulberry32(0xB335);
  const c = new (cls || Composed)(sr);
  c.set(Object.assign({ N: 7, detune: 20, K: 0, phaseMode: 1, dist: 0, seed: 1234, 'h.law': 0, w: 0, S: 1 }, over));
  Object.assign(c.s, c.t);
  return c;
};
const newest = c => c.voices.reduce((a, v) => (v.age > a.age ? v : a));
function serialRun(run, cls, alphaOver, notes) {
  const S = REF.serial;
  const c = onsMake(S.sr, { onsetScatter: S.scatter, onsetAlpha: alphaOver === undefined ? run.alpha : alphaOver, attackScatter: S.attackScatter, relScatter: S.relScatter, A: S.A, R: S.R, seed: run.seed }, cls);
  const tOff = [], onsD0 = [], onsC = [], relC = [];
  for (let k = 0; k < (notes || S.notes); k++) {
    const midi = 57 + (k % 5);
    c.noteOn(midi, S.f, 1);
    const v = newest(c);
    for (let i = 0; i < S.n; i++) { tOff.push(c.tOff[i]); onsD0.push(v.m[i].onsD0); onsC.push(v.m[i].onsC); relC.push(v.m[i].relC); }
    c.noteOff(midi);
  }
  return { tOff, onsD0, onsC, relC };
}
const maxAbs = (a, b) => { let m = 0; for (let i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] - b[i])); return m; };
const maxRel = (a, b) => { let m = 0; for (let i = 0; i < a.length; i++) m = Math.max(m, Math.abs(a[i] / b[i] - 1)); return m; };
/* ADR-077's STRUCTURE statistic (L0021: a variance test cannot tell correction from jitter),
   as tools/waveshape_check.cpp measures it: a member's ASYNCHRONY (its onset delay minus the
   note's mean delay) across successive notes, lag-1 autocorrelation; here pooled over members */
function lag1(onsD0, n, notes) {
  let num = 0, den = 0;
  for (let i = 0; i < n; i++) {
    const a = [];
    for (let k = 0; k < notes; k++) { let m = 0; for (let j = 0; j < n; j++) m += onsD0[k * n + j]; a.push(onsD0[k * n + i] - m / n); }
    let mu = 0; for (const x of a) mu += x; mu /= notes;
    for (let k = 0; k < notes; k++) { const d = a[k] - mu; den += d * d; if (k) num += d * (a[k - 1] - mu); }
  }
  return num / den;
}
const LAG_NOTES = 300;   // waveshape_check's phrase length: a 40-note window biases a random walk's lag-1 low
{
  const S = REF.serial, lagJ = {}, lagC = {};
  for (const run of S.runs) {
    const j = serialRun(run);
    const dT = maxAbs(j.tOff, run.tOff), dD = maxAbs(j.onsD0, run.onsD0);
    let waits = 0; for (let x = 0; x < j.onsD0.length; x++) if (Math.ceil(j.onsD0[x]) !== Math.ceil(run.onsD0[x])) waits++;
    let coef = '', ok = dT <= 1e-15 && dD <= 1e-9 && waits === 0;
    if (run.onsC) { const a = maxRel(j.onsC, run.onsC), b = maxRel(j.relC, run.relC); ok = ok && a <= 1e-11 && b <= 1e-11; coef = `; onsC ${a.toExponential(1)}, relC ${b.toExponential(1)} rel`; }
    row(ok, 'ONS', `draws, alpha ${String(run.alpha).padEnd(4)} seed ${String(run.seed).padEnd(4)}: ${S.notes} notes × ${S.n} members, max|ΔtOff| ${dT.toExponential(1)} s, max|Δwait| ${dD.toExponential(1)} samples, ` +
      `whole-sample waits differing ${waits}${coef}`);
    if (run.seed === 1234) { lagJ[run.alpha] = lag1(j.onsD0, S.n, S.notes); lagC[run.alpha] = lag1(run.onsD0, S.n, S.notes); }
  }
  const a = [0, 0.25, 1, 1.5];
  const L = a.map(x => lag1(serialRun(S.runs.find(r => r.alpha === x && r.seed === 1234), null, undefined, LAG_NOTES).onsD0, S.n, LAG_NOTES));
  const same = a.every(x => Math.abs(lagJ[x] - lagC[x]) < 1e-9);
  row(L[0] > 0.9 && L[1] > 0.4 && L[1] < L[0] && L[2] < 0.2 && L[3] < 0 && same, 'ONS',
    `timing-correction law (ADR-077's gate, waveshape_check's statistic, ${LAG_NOTES} notes): lag-1 of the asynchrony at alpha 0 / .25 / 1 / 1.5 = ${L.map(x => x.toFixed(3)).join(' / ')} ` +
    `(ADR-077 measured +0.985 / +0.679 / −0.072 / −0.550); over the fixture's ${S.notes} notes JS ${a.map(x => lagJ[x].toFixed(3)).join(' / ')} = C++ ${a.map(x => lagC[x].toFixed(3)).join(' / ')}`);
  const s99 = S.runs.find(r => r.seed === 99), s1234 = S.runs.find(r => r.seed === 1234 && r.alpha === 0.25);
  row(maxAbs(s99.tOff, s1234.tOff) > 1e-3, 'ONS', `the seed reaches the stream (B149): seed 99 vs 1234 at alpha .25, max|ΔtOff| ${maxAbs(s99.tOff, s1234.tOff).toExponential(2)} s`);
  /* controls */
  const r25 = s1234;
  const neg = serialRun(r25, null, -0.25);
  row(maxAbs(neg.tOff, r25.tOff) > 1e-3, 'ONSc', `CONTROL timing correction with the WRONG SIGN (−0.25 against the C++'s +0.25): max|ΔtOff| ${maxAbs(neg.tOff, r25.tOff).toExponential(2)} s — must be large`);
  class Unseeded extends Composed { ensSync() { if (!this.ensSeeded) { this.ensSeeded = true; this.tRng = 12345; this.tOff.fill(0); } } }
  const un = serialRun(r25, Unseeded);
  row(maxAbs(un.tOff, r25.tOff) > 1e-3, 'ONSc', `CONTROL the stream NOT SEEDED from the seed (the pre-B149 literal 12345): max|ΔtOff| ${maxAbs(un.tOff, r25.tOff).toExponential(2)} s — must be large`);
  class Jitter extends Composed { armMembers(v) { this.tOff.fill(0); super.armMembers(v); } }
  const ji = serialRun(r25, Jitter, undefined, LAG_NOTES), lj = lag1(ji.onsD0, S.n, LAG_NOTES);
  row(lj < 0.4, 'ONSc', `CONTROL no memory across notes (i.i.d. jitter, "conventional humanize"): lag-1 at alpha .25 reads ${lj.toFixed(3)} — must fail the law row's > 0.4`);
}
/* the rendered scenarios: entries (exact), and at checkpoints the entry ramp and the phases */
function onsRender(q, cls) {
  const Rn = REF.render;
  const c = onsMake(Rn.sr, { K: q.K, onsetScatter: q.scatter, onsetAlpha: q.alpha, attackScatter: q.attackScatter, relScatter: q.relScatter, voiceEnv: q.voiceEnv, A: q.A, R: q.R }, cls);
  const L = new Float32Array(1), R = new Float32Array(1), n = Rn.n;
  let v1 = null, v2 = null, dPh = 0, dE = 0, k = 0;
  const e1 = new Array(n).fill(-1), e2 = new Array(n).fill(-1), t50a = new Array(n).fill(-1), t50r = new Array(n).fill(-1), eOff = new Array(n).fill(0);
  const lvl = m => (q.voiceEnv > 0.5 ? m.eE : m.onsE);
  for (let t = 0; t < q.total; t++) {
    if (t === 0) { c.noteOn(57, Rn.f, 1); v1 = newest(c); }
    if (q.on2 && t === q.on2) { c.noteOn(57, Rn.f, 1); v2 = newest(c); }
    if (t === q.off1) { for (let i = 0; i < n; i++) eOff[i] = lvl(v1.m[i]); c.noteOff(57); }
    if (q.off2 && t === q.off2) c.noteOff(57);
    c.render(L, R);
    const S1 = c.sw.swarms[v1.si];
    for (let i = 0; i < n; i++) {
      if (e1[i] < 0 && S1.phase[i] !== 0) e1[i] = t;
      if (q.t50a) {
        if (t50a[i] < 0 && e1[i] >= 0 && lvl(v1.m[i]) >= 0.5) t50a[i] = t - e1[i];
        if (t50r[i] < 0 && t >= q.off1 && lvl(v1.m[i]) <= 0.5 * eOff[i]) t50r[i] = t - q.off1;
      }
    }
    if (v2 && t >= q.on2) { const S2 = c.sw.swarms[v2.si]; for (let i = 0; i < n; i++) if (e2[i] < 0 && S2.phase[i] !== 0) e2[i] = t - q.on2; }
    if (q.cp && k < q.cp.length && q.cp[k] === t) {
      for (let i = 0; i < n; i++) { dPh = Math.max(dPh, circ(S1.phase[i], q.phase[k * n + i])); dE = Math.max(dE, Math.abs(lvl(v1.m[i]) - q.onsE[k * n + i])); }
      k++;
    }
  }
  return { entries: e1.concat(q.on2 ? e2 : []), dPh, dE, t50a, t50r };
}
{
  const scen = name => REF.render.scen.find(s => s.name === name);
  const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
  const ens = scen('ens'), rE = onsRender(ens);
  row(same(rE.entries, ens.entries) && rE.dE <= 1e-12 && rE.dPh <= 1e-9, 'ONS',
    `render 'ens' (44.1 kHz, scatter 15 ms, attack scatter .6, two notes): member entry samples [${rE.entries.join(' ')}] = C++ exactly; ` +
    `entry ramp onsE max|Δ| ${rE.dE.toExponential(1)}, phase max|Δ| ${rE.dPh.toExponential(1)} cycles at ${ens.cp.length} checkpoints`);
  const cpl = scen('coupled'), rC = onsRender(cpl);
  row(same(rC.entries, cpl.entries) && rC.dPh <= 1e-9, 'ONS',
    `render 'coupled' (K .35, scatter 20 ms): entries [${rC.entries.join(' ')}] = C++; phase max|Δ| ${rC.dPh.toExponential(1)} cycles — a waiting member's frozen phase sits in the mean field in both`);
  class Drifts extends Composed { memberStep(v) { super.memberStep(v); for (const m of v.m) m.hold = false; } }
  const rD = onsRender(ens, Drifts);
  row(!same(rD.entries, ens.entries), 'ONSc', `CONTROL a waiting member that still advances its phase: entries [${rD.entries.slice(0, 7).join(' ')} …] — must differ from the C++'s`);
  const os2 = scen('os2');
  const half = ens.entries.slice(0, 7).every((x, i) => Math.abs(os2.entries[i] - x / 2) <= 1);
  row(half, 'ONS', `FINDING (C++, not this engine): at its 2x oversampling swarm_core.h counts the wait per SUB-sample, entries [${os2.entries.join(' ')}] ≈ half of 1x ` +
    `[${ens.entries.slice(0, 7).join(' ')}]: a 15 ms scatter plays as 7.5 ms. This engine counts output samples at every os (its default is 2x), which is the C++'s 1x law`);

  /* PER-PARTIAL ENV: the draws and the waits are the C++'s exactly; the envelope SHAPE is this
     engine's (RazorCore's ADSR, per member; engine header (2)). What must hold across the two
     laws is that each member's attack and release TIMES are scaled by the same drawn factor, so
     the ratio JS/C++ of each member's half-time is one constant per stage: (A/2)/(A·ln 2) = 0.7213
     for the linear attack against the one-pole, (R·ln 2/4)/(R·ln 2) = 0.25 for the release.
     Tolerance: each half-time is a whole number of samples, so ratio·(1/t_JS + 1/t_C++). */
  const ve = scen('venv'), rV = onsRender(ve);
  const lawRow = (tj, tc, expect) => { let worst = 0, ok = true; for (let i = 0; i < tj.length; i++) { const r = tj[i] / tc[i], tol = expect * (1 / tj[i] + 1 / tc[i]); worst = Math.max(worst, Math.abs(r - expect)); if (!(tj[i] > 0 && tc[i] > 0 && Math.abs(r - expect) <= tol)) ok = false; } return { ok, worst }; };
  const la = lawRow(rV.t50a, ve.t50a, 1 / (2 * Math.LN2)), lr = lawRow(rV.t50r, ve.t50r, 0.25);
  row(same(rV.entries, ve.entries) && la.ok && lr.ok, 'VENV',
    `per-partial env + attack/release scatter .8: entries = C++ [${rV.entries.join(' ')}]; attack half-times JS [${rV.t50a.join(' ')}] vs C++ [${ve.t50a.join(' ')}]: ratio 0.7213 within ${la.worst.toExponential(1)}; ` +
    `release [${rV.t50r.join(' ')}] vs [${ve.t50r.join(' ')}]: ratio 0.25 within ${lr.worst.toExponential(1)}`);
  class Unscaled extends Composed { armMembers(v) { super.armMembers(v); for (const m of v.m) { m.aMul = 1; m.rMul = 1; } } }
  const rU = onsRender(ve, Unscaled), lu = lawRow(rU.t50a, ve.t50a, 1 / (2 * Math.LN2)), lur = lawRow(rU.t50r, ve.t50r, 0.25);
  row(!(lu.ok && lur.ok), 'VENVc', `CONTROL members without their drawn time factors: attack ratio off by up to ${lu.worst.toFixed(3)}, release by ${lur.worst.toFixed(3)} — the law row must fail`);
  /* ADR-078: "voiceEnv on with scatter 0 — per-voice spread exactly 0": with nothing scattered
     every member IS the voice envelope, so the render is the voiceEnv-off render, bit for bit
     (the attack must be at least the C++'s 2 ms floor, which both engines keep) */
  const uni = over => renderWith(() => new Composed(SR), Object.assign({}, byName('Glass horde pad'), { A: 40 }, over), 0xB335, 24064);
  const u0 = uni({ voiceEnv: 0 }), u1 = uni({ voiceEnv: 1 });
  row(maxDiff(u0, u1) === 0 && rms(u0) > 1e-4, 'VENV', `per-partial env with nothing scattered ≡ the voice envelope (Glass horde pad, A 40 ms, two notes and a release): max|Δ| ${maxDiff(u0, u1).toExponential(1)}`);
  const u2 = uni({ voiceEnv: 1, relScatter: 0.8, attackScatter: 0.8 });
  row(maxDiff(u0, u2) > 1e-3, 'VENVc', `CONTROL the same with attack and release scatter .8: max|Δ| ${maxDiff(u0, u2).toFixed(4)} — must differ`);
  /* the voice law and the cull still read the bookkeeping level: a per-partial voice released
     with a long-scattered member stays alive until its LAST member fades (ADR-078's design) */
  const tailOf = relSc => {
    Math.random = mulberry32(0xB335);
    const c = new Composed(SR); c.set({ N: 5, detune: 14, K: 0.35, phaseMode: 1, voiceEnv: 1, relScatter: relSc, A: 5, R: 100 }); Object.assign(c.s, c.t);
    c.noteOn(57, F57, 1);
    const B = new Float32Array(128), B2 = new Float32Array(128);
    for (let i = 0; i < 40; i++) c.render(B, B2);
    c.noteOff(57);
    const v = c.voices[0];
    let alive = 0;
    for (let i = 0; i < 2000 && v.active; i++) { c.render(B, B2); alive = (i + 1) * 128; }
    return { alive, longest: Math.max(...v.m.slice(0, 5).map(m => m.rMul)) };
  };
  const t0 = tailOf(0), t1 = tailOf(1);
  row(t1.longest > 1.2 && t1.alive > t0.alive * (t1.longest - 0.2), 'VENV', `liveness follows the loudest member: released with release scatter 1 (longest member ×${t1.longest.toFixed(2)} of R 100 ms) the voice rings ` +
    `${(t1.alive / SR * 1000).toFixed(0)} ms, against ${(t0.alive / SR * 1000).toFixed(0)} ms unscattered`);
}
{
  /* the lab's next round reads these: the new rows in t ∪ d, the viz fields */
  Math.random = mulberry32(8);
  const e = new Composed(SR); e.set({ N: 5, onsetScatter: 10, grav: 0.5 }); Object.assign(e.s, e.t);
  let viz = null; e.post = m => { if (m && m.t === 'viz' && m.mem) viz = m; };
  e.noteOn(57, F57, 1); e.noteOn(64, F57 * 1.5 * 1.004, 1);
  const L = new Float32Array(4096), R = new Float32Array(4096); e.render(L, R);
  const keys = new Set(Object.keys(e.t).concat(Object.keys(e.d)));
  const want = ['grav', 'basin', 'onsetScatter', 'onsetAlpha', 'attackScatter', 'voiceEnv', 'relScatter'];
  const miss = want.filter(k => !keys.has(k));
  const h = viz && viz.horde;
  row(!miss.length && h && typeof h.f0 === 'number' && Array.isArray(h.grav) && h.grav.length === 1 && h.onsetMs && h.onsetMs.length === 5 && h.gain.length === 5, 'API',
    `B335 keys in t∪d${miss.length ? ' MISSING ' + miss.join(',') : ' (' + want.join(', ') + ')'}; viz.horde carries f0 ${h ? h.f0.toFixed(2) : '—'}, grav pairs ${h ? h.grav.length : '—'}` +
    `${h && h.grav[0] ? ' (' + h.grav[0].ratio.toFixed(4) + ', ' + h.grav[0].err.toFixed(2) + ' c)' : ''}, onsetMs ×${h && h.onsetMs ? h.onsetMs.length : 0}, gain ×${h && h.gain ? h.gain.length : 0}`);
}

/* ---------------------------------------------------------------- ADR-189 anti-aliasing (B355) */
/* THE THREE ADR-189 DIVERGENCES FROM THE ORACLE, each behind its own flag, every flag default 0 (the
   engine header's ADR-189 block; docs/port/divergences.json is the ledger). The detectors:
   - AA0: fingerprints, ZERO's form, pinned on the engine at main d443eb6 (before B355).
   - AA1/AA2: THE INHARMONIC RESIDUAL of a strictly periodic case (one member, K 0, the aligned start,
     nothing modulated): every legitimate component is a harmonic of f0, so the power more than four
     bins off the harmonic grid, over 20 Hz..20 kHz, is aliasing (plus window leakage, the floor). It
     is not B346's estimator (which judges against an oversampled truth and so also counts any timbre
     change); here nothing but aliasing can move it.
   - AA3: a LINE at a fixed fraction of the internal rate R, over its local floor (the same-width
     bands 0.02 R and 0.04 R either side): broad#828's limit cycle sits at R/5 at every os (B346), and
     the tap fix alone moves it to R/3 (B355). */
section('AA — ADR-189 anti-aliasing divergences (B355): all off is main; D1, D2, D3 each do what they claim');
{
  const fpAA = (params, cls) => {
    const A = renderWith(() => new (cls || Composed)(SR), Object.assign({}, params, SWARM_OFF), 0xB355, 24064);
    return createHash('sha256').update(Buffer.from(A.L.buffer)).update(Buffer.from(A.R.buffer)).digest('hex').slice(0, 16);
  };
  /* AA0: pinned on main at d443eb6 (docs/design/scalpel-horde-engine.js blob 5f96285) with this very
     renderWith, seed 0xB355: presets that exercise every flag (carriers, feedback, cross-mod, FM) */
  const AA0 = [['Feedback snarl', '86e1c74e8cb7e644'], ['Cross-mod roar', '03ee212c48e57c19'], ['Crunch horde', 'af83ca85d8e03488'], ['Ring saw', '629a11c078f055c5'],
    ['Zap bass', 'ccf47610c2316f3d']];                                    // Zap bass: an FM saw carrier, inside D1's narrowed scope (ADR-189 A1)
  const OFF = { aaCarrier: 0, aaXin: 0, aaLoop: 0 };
  for (const [name, want] of AA0) {
    const got = fpAA(byName(name)), zero = fpAA(Object.assign({}, byName(name), OFF));
    row(got === want && zero === want, 'AA0', `${name.padEnd(15)} ${got}, flags written 0 ${zero} (main d443eb6: ${want})`);
  }
  for (const f of Object.keys(OFF)) {
    const moved = AA0.filter(([name, want]) => fpAA(Object.assign({}, byName(name), { [f]: 1 })) !== want).map(([n]) => n);
    row(moved.length > 0, 'AA0c', `CONTROL ${f} 1 changes ${moved.length} of ${AA0.length} (${moved.join(', ')}) — must change at least one`);
  }

  /* the periodic case and its residual (1 s at E5, measured over 0.2..1 s) */
  const P1 = { N: 1, K: 0, detune: 0, phaseMode: 1, base: 0, mode: 0, hot: 2, w: 0.6, k: 7.7, depth: 1, hard: 0, fb: 0, xm: 0, os: 1, aa: 1, dcMode: 0, b2on: 0, gain: 0.35, A: 1, R: 50 };
  const E5 = 76, F76 = 440 * Math.pow(2, (E5 - 69) / 12);
  const inharm = (cls, params) => {
    Math.random = mulberry32(1);
    const c = new cls(SR); c.set(params); Object.assign(c.s, c.t);
    const n = 48000, L = new Float32Array(n), R = new Float32Array(n);
    c.noteOn(E5, F76, 0.8);
    for (let i = 0; i < n; i += 128) c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    const S = spectrum(mono(L.subarray(9600), R.subarray(9600)), SR, 8192);
    let tot = 0, off = 0;
    for (let k = 1; k < S.P.length; k++) {
      const f = k * S.binHz; if (f < 20 || f > 20000) continue;
      tot += S.P[k]; const h = f / F76; if (Math.abs(h - Math.round(h)) * F76 > 4 * S.binHz) off += S.P[k];
    }
    return 10 * Math.log10(Math.max(off, 1e-30) / tot);
  };
  /* AA1 (D1): carriers the PolyBLEP cannot band-limit. (a) FM free, a phase-modulated saw carrier: the
     scanner BLEPs its wraps from a linear phase interpolation that the modulation breaks; (b) Band-limit
     off: no carrier BLEPs at all, ADAA is the only band-limiting (the base is a sine, so Band-limit's
     own base BLEP has nothing to do). CONTROL: the carrier BLEPs removed WITHOUT the ADAA (D1's scan,
     voice() left plain): the detector must see the difference, or the row proves nothing. */
  class NoAdaa extends Composed { renderBlock(L, R) { this.renderPlain(L, R); } }
  for (const [tag, over] of [['FM free, PM saw carrier', { mode: 2, I: 3, m: 2 }], ['Band-limit off, sync saw', { aa: 0 }]]) {
    const off = inharm(Composed, Object.assign({}, P1, over)), on = inharm(Composed, Object.assign({}, P1, over, { aaCarrier: 1 }));
    row(on <= off - 3, 'AA1', `D1 ${tag.padEnd(24)} inharmonic ${off.toFixed(1)} → ${on.toFixed(1)} dB — must fall by 3 dB or more`);
    const bare = inharm(NoAdaa, Object.assign({}, P1, over, { aaCarrier: 1 }));
    row(bare >= on + 3, 'AA1c', `CONTROL ${tag.padEnd(24)} carrier BLEPs off and no ADAA: ${bare.toFixed(1)} dB — must read 3 dB or more above D1's ${on.toFixed(1)}`);
  }
  /* ADR-189 A1's NARROWED SCOPE: on a PLAIN sync carrier with Band-limit on, first-order ADAA band-limits
     LESS well than the 2-point PolyBLEP (a box kernel against the BLEP's triangle: -30.3 against -22.8 dB,
     B355), and an S&H modulator's phase jumps are smeared by the mean. D1 must leave both to the oracle,
     bit for bit. CONTROL: the same comparison on a carrier inside the scope (FM free saw, sine modulator)
     must differ, so a scope row cannot pass because the detector is blind. */
  const same = (a, b) => maxDiff(a, b) === 0;
  const r1 = (cls, p) => renderWith(() => new cls(SR), p, 0xB355, 12032);
  for (const [tag, p] of [['plain sync saw, Band-limit on', P1], ['FM free saw, S&H modulator', Object.assign({}, P1, { mode: 2, I: 3, m: 2, mshape: 7 })],
    ['FM free, SINE carrier', Object.assign({}, P1, { mode: 2, I: 3, m: 2, hot: 0 })]]) {
    const off = r1(Composed, p), on = r1(Composed, Object.assign({}, p, { aaCarrier: 1 }));
    row(same(off, on), 'AA1s', `D1 leaves a ${tag.padEnd(30)} to the oracle: max|Δ| ${maxDiff(off, on).toExponential(1)} (must be 0)`);
  }
  { const pin = Object.assign({}, P1, { mode: 2, I: 3, m: 2, mshape: 0 }), off = r1(Composed, pin), on = r1(Composed, Object.assign({}, pin, { aaCarrier: 1 }));
    row(!same(off, on), 'AA1c', `CONTROL the same comparison inside D1's scope (FM free saw, sine modulator): max|Δ| ${maxDiff(off, on).toFixed(4)} — must be non-zero`); }

  /* AA2 (D2): AN XIN-DRIVEN EDGE. A fixture drives a CONSTANT phase input xin = 0.37 into the four blade
     states (what feedback or cross-mod would write, with the loop taken out so nothing else moves):
     the carrier's wraps then sit 0.37 cycles away from where the oracle's scanner looks, so its BLEPs
     miss them. With D2 the residual must be the xin-0 case's (a phase offset does not alias a saw
     that is band-limited) and far under D2-off's. CONTROL: a scanner reading -xin (a sign error) must
     miss the edges again. */
  const XIN = 0.37;
  class Xin extends Composed { bladeStep(m, dphi, c, k, s) { m.ns.xin = XIN; m.ns2.xin = XIN; m.bx.ns3.xin = XIN; m.bx.ns4.xin = XIN; return super.bladeStep(m, dphi, c, k, s); } }
  class XinWrong extends Xin { scan(...a) { const ns = a[11], x = ns.xin; ns.xin = -x; try { return super.scan(...a); } finally { ns.xin = x; } } }
  {
    const ref = inharm(Composed, P1), off = inharm(Xin, P1), on = inharm(Xin, Object.assign({}, P1, { aaXin: 1 })), wrong = inharm(XinWrong, Object.assign({}, P1, { aaXin: 1 }));
    row(Math.abs(on - ref) <= 3 && on <= off - 10, 'AA2', `D2 sync saw, xin ${XIN}: inharmonic ${off.toFixed(1)} (scanner blind to xin) → ${on.toFixed(1)} dB; xin 0 reads ${ref.toFixed(1)} — must be within 3 dB of it and 10 dB under D2 off`);
    row(wrong >= on + 10, 'AA2c', `CONTROL the scanner reading −xin: ${wrong.toFixed(1)} dB — must read 10 dB or more above D2's ${on.toFixed(1)}`);
  }

  /* AA3 (D3): BROAD#828's LIMIT CYCLE (B346): the listening pass's patch (its non-default engine values,
     from gauntlet.mjs samplePatch(45846, 828, 'broad')), E5 held, os 1, its seed; the line at R/5 and R/3
     over its local floor, over 0.05..0.3 s. D3 (the oracle's tap through the loop filter, ADR-189 A1)
     must leave no line more than 6 dB over its floor. CONTROLS: the oracle's loop (aaLoop 0) must show its
     R/5 line; D3 with its filter a PASS-THROUGH (the oracle's taps, unfiltered) must show it too, so the
     filter is what cures it. */
  const B828 = { mode: 1, hot: 4, w: 0.02497344250487307, k: 1.166184157966395, c: 0.821492628660053, hard: 0.22659096238203347, depth: 0.16760664246976376, rotRate: 3.6889861542731524, rotSync: 0, fb: 0.3823352499896128, mshape: 7, I: 0.0754069117297927, m: 6.862318260510085, benvK: -0.04634629702195525, benvW: 0.09298173757269979, benvA: 3.576603711459627, benvD: 2341.79573983158, benvVel: 0.5122116324491799, base: 4, dcMode: 1, xm: 0.15870987800850156, frame: 1, phaseMode: 0, law: 3, bspread: -0.6364673553034663, kRule: 8, kRuleAmt: 0.6274632841814309, wspread: -0.2908894410356879, dspread: -0.2656447202898562, ispread: 0.9232641374692321, rotSpread: -0.28748671136165305, b2on: 1, mode2: 2, w2: 0.05261411756061071, k2: 31.958604020527126, lock2: 0, c2: 0.34182922495529056, hard2: 0.39319653320126235, depth2: 0.9189625040162355, mirror2: 0, rot2Follow: 0, rotRate2: 1.0956337340176105, frame2: 1, b2order: 1, b2mix: 0.6863440533634275, colK: 0.3284184467047453, colB: 0.03496146504767239, N: 3, detune: 71.6670430265367, K: -0.8084876798093319, width: 0.44364295271225274, A: 2.49053468199747, D: 417.97740792484007, S: 0.5356353237293661, R: 2399.0311701255923, gain: 0.5667388490401208, polyMode: 1, glide: 3.392950330909303, os: 1, onset: 0.508713430725038, dissolve: 0.37915171489879057, driftDepth: 74.03178447857499, 'h.driftRate': 0.5697026196867228, 'h.law': 5, stretchB: 4.374309228267521, spread: 11.062973401974887, anchor: 0.675719597376883, inertia: 0.10279140272177756, inertiaCurve: 4.4292594762519 };
  const lines = (cls, params) => {
    Math.random = mulberry32(2860116571);
    const c = new cls(SR); c.set(Object.assign({}, params, SWARM_OFF)); Object.assign(c.s, c.t);
    const n = 14400, L = new Float32Array(n), R = new Float32Array(n);
    c.noteOn(E5, F76, 0.8);
    for (let i = 0; i < n; i += 128) c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    const S = spectrum(mono(L.subarray(2400), R.subarray(2400)), SR, 8192);
    const bp = f => { let p = 0; for (let k = 1; k < S.P.length; k++) if (Math.abs(k * S.binHz - f) <= 0.004 * SR) p += S.P[k]; return p; };
    const over = f => 10 * Math.log10(bp(f) / ((bp(f - 0.02 * SR) + bp(f + 0.02 * SR) + bp(f - 0.04 * SR) + bp(f + 0.04 * SR)) / 4));
    return { r5: over(SR / 5), r3: over(SR / 3) };
  };
  class Unfiltered extends Composed { setOS(n) { super.setOS(n); this.loopA = 1; } }  // the filter a pass-through: the oracle's taps
  const fmt = l => `R/5 ${l.r5.toFixed(1)}, R/3 ${l.r3.toFixed(1)} dB over floor`;
  const d3 = lines(Composed, Object.assign({}, B828, { aaLoop: 1 })), old = lines(Composed, B828), unf = lines(Unfiltered, Object.assign({}, B828, { aaLoop: 1 }));
  row(d3.r5 <= 6 && d3.r3 <= 6, 'AA3', `D3 broad#828 E5 os 1: ${fmt(d3)} — no line more than 6 dB over its floor`);
  row(old.r5 > 10, 'AA3c', `CONTROL the oracle's loop (aaLoop 0): ${fmt(old)} — its R/5 cycle must read more than 10 dB`);
  row(unf.r5 > 10, 'AA3c', `CONTROL D3 with its filter a pass-through (the oracle's taps): ${fmt(unf)} — the R/5 cycle must read more than 10 dB`);
}

/* ---------------------------------------------------------------- B382 M3 */
/* M3 `tempoGrid` (docs/port/divergences.json M3, default ON): law 3 is the C++'s TEMPO GRID
   (swarm_core.h :1803-1812, ADR-022), not SwarmSynth's fall-through to ERB. THE C++ IS THE LAW:
     M3g  ADR-022's property, on the engine's own swarm: with law 3, K 0 and no drift, every member's
          offset from the played pitch is an exact multiple of the grid u = (bpm/60)·beatMult, at two
          tempi; the CONTROL (flag off: ERB) is off the grid.
     M3f  each member's frequency is the C++ expression, transcribed here from swarm_core.h :1809-1811
          (f0 + round(f0·(2^(x·dep·100/1200) - 1)/u)·u, std::round's half away from zero), exactly.
     M3s  the swarm half with M3 on is SwarmSynth with M3 on (the engine's own patched class), exactly;
          flag off it is SwarmSynth itself (law 3 = ERB there), exactly; the CONTROL: M3 on against
          SwarmSynth itself must differ.
     M3o  no other law moves: laws 0, 1, 2, 4 and 5 render bit-identically with the flag on and off. */
section('M3 — B382: law 3 is the tempo grid (ADR-022) in the composed engine');
{
  const grid = (on, bpm, beatMult) => {
    Math.random = mulberry32(0xB383);
    const c = new Composed(SR); c.set({ N: 7, detune: 40, K: 0, phaseMode: 1, 'h.law': 3, dist: 0, bpm, beatMult, tempoGrid: on, w: 0 }); Object.assign(c.s, c.t);
    c.noteOn(NOTE, F57, 1);
    const L = new Float32Array(64), R = new Float32Array(64); c.render(L, R);
    const S = c.sw.swarms[c.voices[0].si], u = (bpm / 60) * beatMult;
    const off = Array.from(S.vf.subarray(0, 7), f => (f - S.f0) / u);
    const want = Array.from(c.sw.x.subarray(0, 7), x => { const q = S.f0 * (Math.pow(2, (x * 0.4 * 100) / 1200) - 1) / u; return S.f0 + (q < 0 ? -Math.round(-q) : Math.round(q)) * u; });
    return { u, err: Math.max(...off.map(k => Math.abs(k - Math.round(k)))), exact: want.every((f, i) => f === S.vf[i]), vf: Array.from(S.vf.subarray(0, 7)) };
  };
  const g1 = grid(1, 120, 1), g2 = grid(1, 140, 2), e1 = grid(0, 120, 1);
  row(g1.err < 1e-9 && g2.err < 1e-9, 'M3g', `law 3, 7 members, 40 c, K 0: offsets from the pitch are multiples of u = ${g1.u} Hz (worst ${g1.err.toExponential(1)} of a step) and u = ${g2.u.toFixed(4)} Hz (${g2.err.toExponential(1)})`);
  row(e1.err > 0.01, 'M3gc', `CONTROL flag off (SwarmSynth: law 3 falls through to ERB): worst ${e1.err.toFixed(3)} of a ${e1.u} Hz step off the grid — must exceed 0.01`);
  row(g1.exact && g2.exact, 'M3f', `every member = the C++'s f0 + round(f0·(2^(x·dep·100/1200) - 1)/u)·u, exactly, at both tempi (${g1.vf.map(f => f.toFixed(3)).join(', ')} Hz)`);

  const GRID = { tempoGrid: 1, bpm: 120, beatMult: 1 }, G = swarmWith(GRID);
  const cases = [['law 3 K .2', { law: 3, cents: 30, K: 0.2 }], ['law 3 K 0 drift', { law: 3, cents: 40, K: 0, retrig: 0, driftDepth: 20, driftRate: 0.5 }],
    ['law 3 K -.5', { law: 3, cents: 25, K: -0.5, retrig: 0 }], ['law 3 anchor', { law: 3, cents: 50, K: 0.3, anchor: 1, spread: 2, dist: 1 }]];
  let worstOn = 0, worstOff = 0;
  const moved = [];
  for (const [name, over] of cases) {
    const h = Object.assign({}, H0, over);
    const a = o1(h, { blade: GRID, refCls: G }), b = o1(h), d = o1(h, { blade: GRID });
    worstOn = Math.max(worstOn, a.ph, a.eff); worstOff = Math.max(worstOff, b.ph, b.eff);
    moved.push(`${name} ${d.ph.toExponential(1)}`);
    if (d.ph < 1e-3) row(false, 'M3sc', `CONTROL ${name}: M3 on did not move law 3 against SwarmSynth (max|Δφ| ${d.ph})`);
  }
  row(worstOn === 0 && worstOff === 0, 'M3s', `${cases.length} law-3 scenarios at 48 kHz: M3 on = SwarmSynth with M3 on, exactly (max ${worstOn}); M3 off = SwarmSynth itself (its ERB), exactly (max ${worstOff})`);
  row(moved.length === cases.length, 'M3sc', `CONTROL M3 on against SwarmSynth itself: ${moved.join(' · ')} — each must move`);

  const lawRender = (law, on) => renderWith(() => new Composed(SR), { N: 5, detune: 30, K: 0.3, 'h.law': law, harmReach: 1.5, stretchB: 2, tempoGrid: on, phaseMode: 0 }, 0xB383, 12032);
  const same = [0, 1, 2, 4, 5].filter(l => maxDiff(lawRender(l, 1), lawRender(l, 0)) === 0), l3 = maxDiff(lawRender(3, 1), lawRender(3, 0));
  row(same.length === 5, 'M3o', `laws ${same.join(', ')} render bit-identically with M3 on and off (must be all five)`);
  row(l3 > 1e-4, 'M3oc', `CONTROL law 3 itself, M3 on against off: max|Δ| ${l3.toFixed(4)} — must differ`);
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
  /* B355: the SCALPEL lab plays ADR-189's three flags ON through that route, and D1 swaps the
     bundle's own RazorCore.voice: the bundle must render the direct class's samples with them on */
  const aaOn = Object.assign({}, params, { fb: 0.4, aaCarrier: 1, aaXin: 1, aaLoop: 1 });
  const Ad = renderWith(() => new Composed(SR), aaOn, 0xD00D, total), Wd = renderWith(() => new Bundled(SR), aaOn, 0xD00D, total);
  const Ao = renderWith(() => new Composed(SR), Object.assign({}, aaOn, { aaCarrier: 0, aaXin: 0, aaLoop: 0 }), 0xD00D, total);
  row(maxDiff(Ad, Wd) === 0 && maxDiff(Ad, Ao) > 1e-4, 'DET', `toString() bundle with ADR-189's flags on (D1 swaps the bundle's own voice()): max|Δ| ${maxDiff(Ad, Wd).toExponential(1)} against the direct class; the flags move the sound ${maxDiff(Ad, Ao).toFixed(4)} (must be non-zero)`);
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
