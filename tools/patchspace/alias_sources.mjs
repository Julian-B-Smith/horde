/*
 * alias_sources.mjs — B346: where the composed engine's aliasing comes from, and what would cure
 * each source. HYPERSAW, 2026-09-29, ROADMAP B346 (records PR #845, branch lead-records-140).
 * The human: "could we apply something like ADAA to this, or does it only work with overdrive-
 * type effects?" and, widening it: "I think we should try to build the cleanest system we can
 * muster."
 * UNWIRED: an investigation run (minutes of DSP across workers at up to 32x oversampling, writing
 * git-ignored local data); its standing distillate is metrics_check.mjs's X rows, which ARE wired.
 *
 * NOTHING HERE EDITS AN ENGINE FILE. docs/design/scalpel-horde-engine.js and the protected oracle
 * reference/scalpel/prototype/razor-core.js are required and called. A mechanism is toggled in one
 * of three ways, each named in its row of TOGGLES / FIXES:
 *   param   a patch value (aa 0, fb 0, …): the engine's own switch;
 *   sub     a subclass of the composed engine (the capture below, the ADAA state);
 *   razor   a TEXT patch of RazorCore's source evaluated in memory (fidelity.mjs patchedRazor, B325's
 *           method): needed where the code to change is inline in a static or in render() and no
 *           subclass can reach it (the PolyBLEP scanner, the carrier's wave() call, the output tanh).
 *           The file on disk is never touched, and every anchor must occur exactly once.
 *
 * THE MEASUREMENTS (metrics.mjs states the estimator):
 *   convergence  the patch at os N, 2N, 4N, 8N, 16N (N = its own os), one note held 0.3 s, window
 *                0.05..0.3 s (gauntlet.mjs's ALIAS leg), with the 8N render's PRE-DECIMATION stream
 *                captured (a subclass hooks RazorCore.bqf, which render() calls on every internal
 *                sample before the decimation filter: the hook reads, never changes, the samples) ->
 *                metrics.aliasConvergence: excess, the source-explained fold, the unexplained rest,
 *                convergence, class.
 *   outputStage  what every os shares, so no os comparison can see (metrics.mjs's stated blind spot):
 *                from ONE render at 8N with its POST-filter stream captured,
 *                  E  = the engine's output (its decimator: pick one sample in os; then its 8 Hz
 *                       blocker when on; then tanh at the OUTPUT rate);
 *                  T1 = the same stream through an ideal decimator (a 1537-tap Blackman-Harris
 *                       windowed sinc at 22 kHz at 768 kHz, -92 dB from 24 kHz), then the blocker and
 *                       the tanh at the output rate;
 *                  T2 = the same stream, blocker and tanh at the INTERNAL rate, then the ideal
 *                       decimator.
 *                decimLeak = excess of E over T1; tanhFold = excess of T1 over T2 (single references:
 *                all three are deterministic transforms of the same samples, so there is no
 *                realisation difference to guard against).
 *
 *   node tools/patchspace/alias_sources.mjs matrix [--workers W]   rated patches × notes × toggles (Part B)
 *   node tools/patchspace/alias_sources.mjs fixes  [--workers W]   rated patches × notes × fixes (Part C)
 *   node tools/patchspace/alias_sources.mjs cpu                    fixes' cost, Layer-E (machine load printed)
 *   node tools/patchspace/alias_sources.mjs b828                   broad#828's rate-locked component
 *   node tools/patchspace/alias_sources.mjs ulp [key] [seg]        1-ULP sensitivity through the listening page
 *   node tools/patchspace/alias_sources.mjs summary                the tables, as JSON on stdout
 * Output: local/patchspace/alias_sources/ (git-ignored), one JSON line per job, resumable.
 *
 * DETERMINISM: Math.random is a seeded mulberry32 around every instance (space.mjs's convention); the
 * only clock read is process.hrtime around render() in the cpu pass, outside the DSP.
 */
import '../labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { createRequire } from 'node:module';
import { readFileSync, mkdirSync, existsSync, readdirSync, appendFileSync } from 'node:fs';
import { join } from 'node:path';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import { cpus, loadavg } from 'node:os';
import { ROOT, SR, mulberry32, FILES, engineParams, mtof } from './space.mjs';
import * as M from './metrics.mjs';
import { patchedRazor, SWARM_SRC } from './fidelity.mjs';
import { samplePatch } from './gauntlet.mjs';
import { hash32 } from './gen_dependency_tree.mjs';

const require = createRequire(import.meta.url);
const RAZOR = require(join(ROOT, FILES.oracle));
const { makeComposedEngine } = require(join(ROOT, FILES.engine));
export const OUT = join(ROOT, 'local', 'patchspace', 'alias_sources');
export const LP = JSON.parse(readFileSync(join(ROOT, 'docs/design/listening-pass.json'), 'utf8'));
export const NOTES = [['A1', 33], ['E5', 76], ['A5', 81]];
export const SCRIPT = n => ({ n: 14400, ev: [[0, 'on', n, 0.8]] });   // gauntlet.mjs B_SCRIPT, any note
export const WIN = [2400, 14400];

/* ---------------------------------------------------------------- engines */
/* the capture: RazorCore.render calls this.bqf(bqL[0], accL), bqf(bqL[1], ·), then the same for R,
   once per INTERNAL sample when os > 1. Reading x (pre) and the second stage's result (post) records
   the internal stream without changing a sample. */
const capture = C => class extends C {
  bqf(f, x) {
    const y = super.bqf(f, x), c = this._cap;
    if (c && c.i < c.n) {
      if (f === this.bqL[0]) c.preL[c.i] = x; else if (f === this.bqL[1]) c.postL[c.i] = y;
      else if (f === this.bqR[0]) c.preR[c.i] = x; else if (f === this.bqR[1]) { c.postR[c.i] = y; c.i++; }
    }
    return y;
  }
};
export const withCapture = capture;                 // fidelity_audit.mjs wraps its own engine classes with it
const CACHE = new Map();
/* the composed engine over RazorCore with `razor` text patches applied, then `sub` mixins, then the
   capture (left off with `bare`, for the cpu pass: the hook costs a call per internal sample). Key =
   the variant's name. */
export function engineFor(name, razor, subs, bare) {
  const key = name + (bare ? ':bare' : '');
  if (CACHE.has(key)) return CACHE.get(key);
  const R = razor && razor.length ? patchedRazor(razor) : RAZOR;
  let C = makeComposedEngine(R, SWARM_SRC);
  for (const s of subs || []) C = s(C);
  if (!bare) C = capture(C);
  CACHE.set(key, C);
  return C;
}
/* space.mjs render() with the engine class as a parameter (same operations in the same order: set,
   snap the smoothers, events at their samples, blocks between; Math.random seeded around the
   instance). `cap` asks for the internal stream (os > 1 only). */
export function renderWith(C, patch, script, opt) {
  opt = opt || {};
  const saved = Math.random;
  Math.random = mulberry32((opt.seed === undefined ? 0xB316 : opt.seed) >>> 0);
  try {
    const c = new C(SR);
    c.set(opt.raw ? patch : engineParams(patch));     // raw: engine params as given (a preset), not a lab patch
    Object.assign(c.s, c.t);
    const n = script.n, L = new Float32Array(n), R = new Float32Array(n), os = c.os;
    if (opt.cap && os > 1) c._cap = { n: n * os, i: 0, preL: new Float64Array(n * os), preR: new Float64Array(n * os), postL: new Float64Array(n * os), postR: new Float64Array(n * os) };
    let pos = 0;
    const timing = opt.timing ? [] : null;
    const upto = end => { while (pos < end) { const e = Math.min(end, pos + (opt.block || 128));
      if (timing) { const t0 = process.hrtime.bigint(); c.render(L.subarray(pos, e), R.subarray(pos, e)); timing.push([e - pos, Number(process.hrtime.bigint() - t0)]); }
      else c.render(L.subarray(pos, e), R.subarray(pos, e));
      pos = e; } };
    for (const [t, kind, note, vel] of script.ev) { upto(t); if (kind === 'on') c.noteOn(note, mtof(note), vel); else c.noteOff(note); }
    upto(n);
    return { L, R, os, cap: c._cap || null, core: c, timing };
  } finally { Math.random = saved; }
}

/* the engine's decimation filter (RazorCore.setOS: two RBJ low-passes at 0.45 sr, Q 0.5412 and
   1.3066, designed at sr*os) as a power gain at f Hz, from RazorCore's own mk() */
const G2 = new Map();
export function decimGain2(os) {
  if (os <= 1) return () => 1;                      // os 1 has no decimation filter
  if (G2.has(os)) return G2.get(os);
  const fs = SR * os, fc = 0.45 * SR, bq = [RAZOR.prototype.mk(fs, fc, 0.5412), RAZOR.prototype.mk(fs, fc, 1.3066)];
  const g = f => { let p = 1; const w = 2 * Math.PI * f / fs, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
    for (const b of bq) { const nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -(b.b1 * s1 + b.b2 * s2), dr = 1 + b.a1 * c1 + b.a2 * c2, di = -(b.a1 * s1 + b.a2 * s2); p *= (nr * nr + ni * ni) / (dr * dr + di * di); } return p; };
  G2.set(os, g); return g;
}

const spec = (L, R, a, b) => M.spectrum(M.mono(L.subarray(a, b), R.subarray(a, b)), SR, 8192);
/* THE ESTIMATOR ON ONE SOUND, whoever renders it: renderAt(m, cap) renders the sound at os N*m (m = 1, 2,
   4, 8, 16), capturing the internal stream when `cap`, and returns renderWith()'s { L, R, cap, core }.
   The window [a, b) is in output samples. Returns
     conv   metrics.aliasConvergence, plus the B345 metric on the same renders (b345Db);
     out    (unless opt.output is false) the output stage split at the sound's own os N (at 2N when N is
            1, which has no decimator): decimLeakDb, tanhFoldDb. NOT at a finer os: the engine's blade caps
            scale with os (kCap = 0.45*sr*os/fi), so a finer render is a brighter sound and would overstate
            what its decimator leaks;
     total  (opt.truth) totalDb: everything the sound at os N has that its TRUTH lacks. The truth is the
            same sound at 8N and 16N with the output stage done right (the post-filter stream, then the
            blocker and the tanh at the internal rate, then the ideal decimator: T2 in the header), two
            of them so the region rule can tell a realisation difference from folding. Gains divide out
            each render's decimation filter, as in conv. opt.gainN / opt.latN override the test's own
            filter gain and latency (a fix that replaces the decimator); opt.truthOf supplies another
            sound's truth (a fix is judged against the engine-as-is truth, never its own).
   Shared by this harness, gauntlet.mjs measure() and fidelity_audit.mjs. */
export function estimate(renderAt, N, win, opt) {
  opt = opt || {};
  const [a, b] = win, S = {}, names = ['N', 'N2', 'N4', 'N8', 'N16'], R = {};
  let Sint = null;
  [1, 2, 4, 8, 16].forEach((m, q) => {
    const splitAt = N > 1 ? 1 : 2;                  // the output-stage split needs a decimator: os N, or 2N at os 1
    const r = renderAt(m, m === 8 || (m === 16 && opt.truth && !opt.truthOf) || (m === splitAt && opt.output !== false));
    S[names[q]] = spec(r.L, r.R, a, b);
    R[m] = r;
    if (m === 8) { const k = N * 8, c = r.cap, pre = new Float64Array((b - a) * k);
      for (let i = 0; i < pre.length; i++) pre[i] = 0.5 * (c.preL[a * k + i] + c.preR[a * k + i]);
      Sint = M.spectrum(pre, SR * k, 8192 * k); }
  });
  const conv = M.aliasConvergence(S, Sint, N, SR, decimGain2);
  conv.b345Db = M.aliasing(S.N, S.N4).aliasDb;                 // the B345 metric on the same renders, beside it
  const res = { conv };
  if (opt.keep) res._S = S;
  if (opt.output !== false) res.out = outputFrom(R[N > 1 ? 1 : 2], N > 1 ? N : 2, win, opt.keep);
  if (opt.truth) {
    const TT = opt.truthOf || { T: [truthFrom(R[8], N * 8, b), truthFrom(R[16], N * 16, b)], os: [8 * N, 16 * N] };
    const lat = opt.latN || 0, gN = opt.gainN || decimGain2(N);
    const ST = TT.T.map(t => spec(t[0], t[1], a - lat, b - lat));
    const e = M.excessJoint(S.N, ST, null, [gN, decimGain2(TT.os[0]), decimGain2(TT.os[1])]);
    res.totalDb = e.db; res.totalSame = e.sameShare; res._T = TT;
    if (opt.keep) res._ST = ST;
  }
  return res;
}
/* the patch-and-note form the passes below use */
export function convergence(C, patch, note, seed, keep) {
  const N = patch.os || 2;
  const r = estimate((m, cap) => renderWith(C, Object.assign({}, patch, { os: N * m }), SCRIPT(note), { seed, cap }), N, WIN, { keep, output: false });
  if (keep) r.conv._S = r._S;
  return r.conv;
}
export function estimatePatch(C, patch, note, seed, opt) {
  const N = patch.os || 2;
  return estimate((m, cap) => renderWith(C, Object.assign({}, patch, { os: N * m }), SCRIPT(note), { seed, cap }), N, WIN, opt);
}

/* ---------------------------------------------------------------- the output stage */
const FIR = new Map();
function firFor(k) {                                            // an ideal-ish decimator from sr*k to sr
  if (FIR.has(k)) return FIR.get(k);
  const R = SR * k, L = 2 * Math.round(R / 1000) + 1, c = (L - 1) / 2, fc = 22000 / R, h = new Float64Array(L); let s = 0;
  for (let i = 0; i < L; i++) { const t = i - c, x = 2 * Math.PI * i / (L - 1), w = 0.35875 - 0.48829 * Math.cos(x) + 0.14128 * Math.cos(2 * x) - 0.01168 * Math.cos(3 * x);
    h[i] = (t === 0 ? 2 * fc : Math.sin(2 * Math.PI * fc * t) / (Math.PI * t)) * w; s += h[i]; }
  for (let i = 0; i < L; i++) h[i] /= s;
  FIR.set(k, h); return h;
}
/* x at sr*k -> sr, each output sample centred on the engine's own sampling instant (the last internal
   sample of each group of k: razor-core.js render's `yl = accL` after the j loop); only samples [a, b) */
function decimate(x, k, n, a, b) {
  const h = firFor(k), c = (h.length - 1) / 2, y = new Float64Array(n);
  for (let i = a || 0; i < (b || n); i++) { const m = i * k + k - 1; let s = 0; for (let j = 0; j < h.length; j++) { const q = m + c - j; if (q >= 0 && q < x.length) s += h[j] * x[q]; } y[i] = s; }
  return y;
}
/* the engine's 8 Hz blocker (razor-core.js render, after the decimation) at rate `rate`, and whether it runs */
function blocker(x, rate) { const hp = 1 - 2 * Math.PI * 8 / rate, y = new Float64Array(x.length); let hx = 0, hy = 0; for (let i = 0; i < x.length; i++) { const o = x[i] - hx + hp * hy; hx = x[i]; hy = o; y[i] = o; } return y; }
const blockerOn = c => c.d.dcMode === 1 || (c.d.dcMode === 2 && (c.s.xm > 0.0005 || c.s.fb > 0.0005 || (c.s.b2on && (c.s.colK || c.s.colB))));
/* T2 alone (the truth: blocker and tanh at the internal rate, then the ideal decimator), samples [0, b) */
function truthFrom(r, k, b) {
  const c = r.core, n = r.L.length, g = c.s.gain * 1.6, bl = blockerOn(c);
  return [r.cap.postL, r.cap.postR].map(post => { let u = bl ? blocker(post, SR * k) : post; u = u.map(v => Math.tanh(v * g)); return decimate(u, k, n, 0, b); });
}
/* E, T1, T2 (the header) from one captured render at os k. The blocker's state runs from the render's
   start (its history matters); the FIR is evaluated on the window only (the rest is not measured). */
function outputFrom(r, k, win, keep) {
  const c = r.core, n = r.L.length, [a, b] = win, g = c.s.gain * 1.6, bl = blockerOn(c), T1 = [], T2 = [];
  for (const post of [r.cap.postL, r.cap.postR]) {
    let d = decimate(post, k, n, 0, b); if (bl) d = blocker(d, SR); T1.push(d.map(v => Math.tanh(v * g)));
    let u = bl ? blocker(post, SR * k) : post; u = u.map(v => Math.tanh(v * g)); T2.push(decimate(u, k, n, Math.max(0, a - 64), b));
  }
  /* a latency the engine's decimator adds (decimFir: (taps-1)/2 internal samples) shifts E against T1/T2 */
  const lat = c.fd ? Math.round((c.fd.n - 1) / 2 / k) : 0;
  const SE = spec(r.L, r.R, a, b), S1 = spec(T1[0], T1[1], a - lat, b - lat), S2 = spec(T2[0], T2[1], a - lat, b - lat);
  const out = { os: k, drive: g, blocker: bl, latency: lat, decimLeakDb: M.excessJoint(SE, [S1, S1]).db, tanhFoldDb: M.excessJoint(S1, [S2, S2]).db, totalDb: M.excessJoint(SE, [S2, S2]).db };
  if (keep) out._S = { E: SE, T1: S1, T2: S2 };
  return out;
}
export function outputStage(C, patch, note, seed, keep) {
  const N = patch.os || 2, k = N * 8;
  return outputFrom(renderWith(C, Object.assign({}, patch, { os: k }), SCRIPT(note), { seed, cap: true }), k, WIN, keep);
}

/* ---------------------------------------------------------------- the mechanisms (Part B) */
const isFM = m => m === 1 || m === 2;
const b2 = p => !!p.b2on;
/* each: `what`, `how` (param | sub | razor), `applies(p)`, and the change. A toggle REMOVES one
   mechanism; the change in the estimate is its share. Path toggles (param) change the sound too, and
   say so: they implicate a path, as B325's patch ablations did. */
export const TOGGLES = {
  aa: { what: 'PolyBLEP off (aa 0): what the band-limiting of edges currently removes (negative = BLEP helps)', how: 'param', applies: p => p.aa !== 0, over: { aa: 0 } },
  blepXin: { what: 'the PolyBLEP scanner tracks the xin phase input (feedback / cross-mod): the edges it currently MISSES', how: 'razor',
    applies: p => p.aa !== 0 && (p.fb > 0.0005 || p.xm > 0.0005),
    razor: [['if (ns.cd){ o0 += ns.cacc - ns.cd; o1 += ns.cacc; }', 'o0 += ns.xin; o1 += ns.xin;\n    if (ns.cd){ o0 += ns.cacc - ns.cd; o1 += ns.cacc; }', 'blepXin']] },
  carrier: { what: 'every carrier wave (hot, hot2) -> sine: the edges and corners inside the blade', how: 'param',
    applies: p => ([0, 1, 2, 5].includes(p.mode) && p.hot !== 0) || (b2(p) && [0, 1, 2, 5].includes(p.mode2) && p.hot2 !== 0), over: { hot: 0, hot2: 0 } },
  baseWave: { what: 'the base wave -> sine: the waveshape the blades cut into', how: 'param', applies: p => p.base !== 0, over: { base: 0 } },
  fold: { what: 'the fold blade (mode 4, a sine waveshaper on the base) at depth 0', how: 'param',
    applies: p => (p.b1on && p.mode === 4) || (b2(p) && p.mode2 === 4), overFn: p => Object.assign(p.mode === 4 ? { depth: 0 } : {}, b2(p) && p.mode2 === 4 ? { depth2: 0 } : {}) },
  fm: { what: 'audio-rate FM / PM off (I 0, I2 0)', how: 'param', applies: p => (p.b1on && isFM(p.mode)) || (b2(p) && isFM(p.mode2)), over: { I: 0, I2: 0 } },
  fb: { what: 'feedback and cross-member modulation off (fb 0, xm 0)', how: 'param', applies: p => p.fb > 0.0005 || p.xm > 0.0005, over: { fb: 0, xm: 0 } },
  col: { what: 'collision off (colK 0, colB 0)', how: 'param', applies: p => b2(p) && (p.colK || p.colB), over: { colK: 0, colB: 0 } },
  crush: { what: 'crush blades (mode 6) at depth 0 (crush is intentional)', how: 'param',
    applies: p => (p.b1on && p.mode === 6) || (b2(p) && p.mode2 === 6), overFn: p => Object.assign(p.mode === 6 ? { depth: 0 } : {}, b2(p) && p.mode2 === 6 ? { depth2: 0 } : {}) },
  noise: { what: 'noise blades (mode 3, Math.random steps, never BLEPed) at depth 0', how: 'param',
    applies: p => (p.b1on && p.mode === 3) || (b2(p) && p.mode2 === 3), overFn: p => Object.assign(p.mode === 3 ? { depth: 0 } : {}, b2(p) && p.mode2 === 3 ? { depth2: 0 } : {}) },
  hash: { what: 'the noise-FM hash (mshape 5 / 7, RazorCore.hash) -> a sine modulator at the same rate', how: 'param',
    applies: p => ((p.b1on && isFM(p.mode)) && (p.mshape === 5 || p.mshape === 7)) || (b2(p) && isFM(p.mode2) && ((p.b2fm ? p.mshape2 : p.mshape) === 5 || (p.b2fm ? p.mshape2 : p.mshape) === 7)),
    over: { mshape: 0, mshape2: 0 } },
  dc: { what: 'the per-cycle DC estimator and blocker off (dcMode 0)', how: 'param', applies: p => p.dcMode !== 0, over: { dcMode: 0 } },
};
export const MATRIX_KEYS = Object.keys(TOGGLES);

/* ---------------------------------------------------------------- the candidate cures (Part C) */
const ADAA_STATIC = `
  // B346 harness: first-order ADAA of a carrier's phase->wave function, on REAL voice states only (the
  // BLEP height probe, hAt, and the DC estimator run voice() on scratch states: those stay plain)
  static adaa1(sh, cp, ns){
    if (!ns.adaa || sh > 4) return RazorCore.wave(sh, cp - Math.floor(cp));
    const p0 = ns.acp, fresh = !ns.inside; ns.acp = cp;
    if (fresh) return RazorCore.wave(sh, cp - Math.floor(cp));
    const d = cp - p0;
    if (Math.abs(d) < 1e-7){ const x = cp - 0.5*d; return RazorCore.wave(sh, x - Math.floor(x)); }
    return (RazorCore.F(sh, cp) - RazorCore.F(sh, p0))/d;
  }`;
/* the base wave through the same ADAA, in voice() (the base every blade path returns outside the blade), on the
   blade-1 primary state only (ns.adaaB): its twin and blade 2 add (voice - wave) differences, which ADAA would
   not cancel exactly; stated as a limit of the prototype */
const ADAA_BASE = [['    const base = RazorCore.wave(p.base, phi), xin0 = inp === undefined ? base : inp;',
  '    const base = ns.adaaB && inp === undefined ? RazorCore.adaaB(p.base, phi, ns) : RazorCore.wave(p.base, phi), xin0 = inp === undefined ? base : inp;', 'adaa base'],
  ['static frac(x){ return x - Math.floor(x); }', `static frac(x){ return x - Math.floor(x); }
  static adaaB(sh, phi, ns){
    if (sh > 4) return RazorCore.wave(sh, phi);
    let p0 = ns.bph; ns.bph = phi;
    let d = phi - p0; if (d < -0.5) d += 1;                    // phi wraps at 1: unwrap the step
    if (!(d > 1e-7 && d < 0.5)) return RazorCore.wave(sh, phi);
    return (RazorCore.F(sh, p0 + d) - RazorCore.F(sh, p0))/d;
  }`, 'adaa base static']];
const ADAA_CARRIER = [
  ['static frac(x){ return x - Math.floor(x); }', 'static frac(x){ return x - Math.floor(x); }' + ADAA_STATIC, 'adaa static'],
  ['case 0: { const cp = hp + ns.xin + ns.cacc; hot = RazorCore.wave(p.hot, cp - Math.floor(cp)); break; }',
    'case 0: { const cp = hp + ns.xin + ns.cacc; hot = RazorCore.adaa1(p.hot, cp, ns); break; }', 'adaa mode 0'],
  ['        hot = RazorCore.wave(p.hot, cp - Math.floor(cp)); break;\n      }', '        hot = RazorCore.adaa1(p.hot, cp, ns); break;\n      }', 'adaa modes 1/2'],
  ['case 5: { const cp = hp + ns.xin + ns.cacc; hot = xin0*RazorCore.wave(p.hot, cp - Math.floor(cp)); break; }',
    'case 5: { const cp = hp + ns.xin + ns.cacc; hot = xin0*RazorCore.adaa1(p.hot, cp, ns); break; }', 'adaa mode 5'],
];
/* the flag lives on the member states the render uses (m.ns, ns2, bx.ns3, bx.ns4); every state object
   gets both fields up front so V8 keeps one shape (the CPU numbers would otherwise carry the cost of
   a shape change, not of the ADAA) */
const adaaFlags = C => class extends C {
  constructor(sr) {
    super(sr);
    for (const s of [this.sc, this.sc2, this.bxs.ns3, this.bxs.ns4]) { s.adaa = false; s.acp = 0; s.adaaB = false; s.bph = 0; }
    for (const v of this.voices) for (const m of v.m) for (const s of [m.ns, m.ns2, m.bx.ns3, m.bx.ns4]) { s.adaa = true; s.acp = 0; s.adaaB = s === m.ns; s.bph = 0; }
  }
};
/* ADAA replacing (not stacked on) the scanner's carrier BLEPs: scan() returns before placing any
   carrier-wrap correction; blade entry/exit and base-wave BLEPs (tryE) stay */
const SCAN_OFF = [['    if (off < 0) return;\n    let e0 = p0 - st;', '    if (off < 0 || carrier) return;\n    let e0 = p0 - st;', 'scan carriers off']];
const TANH_ADAA = [
  ['      L[i] = Math.tanh(yl*s.gain*1.6);\n      R[i] = Math.tanh(yr*s.gain*1.6);', '      L[i] = this.adT(0, yl*s.gain*1.6);\n      R[i] = this.adT(1, yr*s.gain*1.6);', 'tanh -> ADAA1'],
];
const tanhAdaaSub = C => class extends C {
  constructor(sr) { super(sr); this.ax = new Float64Array(2); }
  /* first-order ADAA of tanh: (logcosh(x) - logcosh(x0)) / (x - x0), tanh of the midpoint when the step
     is tiny (the 0/0 case); logcosh in its overflow-free form. Half a sample of delay, and a gentle
     low-pass (sinc of the step), are its known costs. */
  adT(ch, x) {
    const x0 = this.ax[ch]; this.ax[ch] = x;
    const d = x - x0;
    if (Math.abs(d) < 1e-5) return Math.tanh(0.5 * (x + x0));
    const lc = z => { const a = Math.abs(z); return a + Math.log1p(Math.exp(-2 * a)) - Math.LN2; };
    return (lc(x) - lc(x0)) / d;
  }
};
const TANH_OS = [
  ['        s.w = w0; s.depth = d0; s.I = I0;\n        if (os > 1){', '        s.w = w0; s.depth = d0; s.I = I0;\n        accL = Math.tanh(accL*s.gain*1.6); accR = Math.tanh(accR*s.gain*1.6);\n        if (os > 1){', 'tanh inside the os loop'],
  ['      L[i] = Math.tanh(yl*s.gain*1.6);\n      R[i] = Math.tanh(yr*s.gain*1.6);', '      L[i] = yl;\n      R[i] = yr;', 'no output tanh'],
];
/* a steeper decimator (B346's own finding: the engine's 4th-order Butterworth at 0.45 sr, then picking one
   sample in os, leaks 30 to 55 dB under the signal on bright patches, and every os shares it). The
   biquads are replaced by a windowed-sinc low-pass at 22 kHz (Blackman-Harris, 2*R/1000+1 taps at the
   internal rate R: -92 dB from 24 kHz, flat to 20 kHz), evaluated only on the internal sample the
   decimation keeps (j = os - 1), from a ring of the last taps. Causal, so it adds (taps-1)/2 internal
   samples of latency (1 ms at every os), where the biquads' group delay is about 0.03 ms. */
const DECIM_FIR = [['          accL = this.bqf(bqL[1], this.bqf(bqL[0], accL));\n          accR = this.bqf(bqR[1], this.bqf(bqR[0], accR));',
  '          accL = this.firD(0, accL, j); accR = this.firD(1, accR, j);', 'FIR decimator']];
const decimFirSub = C => class extends C {
  firD(ch, x, j) {
    const os = this.os;
    if (!this.fd || this.fd.os !== os) { const h = firFor(os); this.fd = { os, h, n: h.length, buf: [new Float64Array(h.length), new Float64Array(h.length)], w: [0, 0] }; }
    const d = this.fd, b = d.buf[ch]; let w = d.w[ch];
    b[w] = x; w = w + 1 === d.n ? 0 : w + 1; d.w[ch] = w;
    const c = this._cap;                            // the capture hook, since bqf is no longer called: pre = post = x
    if (c && c.i < c.n) { if (ch === 0) { c.preL[c.i] = x; c.postL[c.i] = x; } else { c.preR[c.i] = x; c.postR[c.i] = x; c.i++; } }
    if (j !== os - 1) return 0;                     // only the kept sample is computed
    let s = 0, q = w; const h = d.h;
    for (let t = d.n - 1; t >= 0; t--) { s += h[t] * b[q]; q = q + 1 === d.n ? 0 : q + 1; }
    return s;
  }
};
export const FIXES = {
  base: { what: 'the engine as it is', razor: [], subs: [] },
  blepXin: { what: 'missed-edge BLEP coverage: the scanner tracks the xin phase input', razor: TOGGLES.blepXin.razor, subs: [] },
  adaaStack: { what: 'first-order ADAA on every carrier\'s phase->wave, STACKED on the existing PolyBLEP', razor: ADAA_CARRIER, subs: [adaaFlags] },
  adaaSwap: { what: 'first-order ADAA on every carrier, REPLACING the scanner\'s carrier-wrap BLEPs (entry/exit/base BLEPs kept)', razor: ADAA_CARRIER.concat(SCAN_OFF), subs: [adaaFlags] },
  tanhAdaa: { what: 'first-order ADAA on the output tanh', razor: TANH_ADAA, subs: [tanhAdaaSub] },
  /* the same, judged with its own linear response divided out: for small signals first-order ADAA is the two-sample
     mean, |H| = cos(pi f / sr) at the output rate (-11.7 dB at 20 kHz), and a region power rule would otherwise
     credit that low-pass as removed folding */
  tanhAdaaEq: { what: 'first-order ADAA on the output tanh, its two-sample-mean droop cos(pi f/sr) divided out before judging', razor: TANH_ADAA, subs: [tanhAdaaSub],
    gain: N => { const g = decimGain2(N); return f => g(f) * Math.pow(Math.cos(Math.PI * f / SR), 2); }, pass: 'fixes3' },
  tanhOs: { what: 'the output tanh moved inside the oversampled loop (before the decimator; the 8 Hz blocker now follows it)', razor: TANH_OS, subs: [] },
  os2x: { what: 'twice the oversampling (the engine\'s own os, doubled: FM and feedback run finer)', razor: [], subs: [], osMul: 2 },
  /* pass 'fixes2' (added after the first fixes run; same judging): */
  aaOn: { what: 'Band-limit switched on (aa 1): what the engine\'s own PolyBLEP would remove where the patch has it off', razor: [], subs: [], over: { aa: 1 }, pass: 'fixes2' },
  adaaBase: { what: 'first-order ADAA on the base wave as well as every carrier, with the scanner\'s carrier BLEPs replaced (Band-limit as the patch has it)', razor: ADAA_CARRIER.concat(SCAN_OFF, ADAA_BASE), subs: [adaaFlags], pass: 'fixes2' },
  os4x: { what: 'four times the oversampling (a rate-locked loop limit cycle at 0.2 x the internal rate leaves the audio band from 4x)', razor: [], subs: [], osMul: 4, pass: 'fixes2' },
  decimFir: { what: 'a steep FIR decimator in place of the two biquads (os > 1 only; os 1 has no decimator)', razor: DECIM_FIR, subs: [decimFirSub],
    gain: () => () => 1, latency: os => (os > 1 ? Math.round((firFor(os).length - 1) / 2 / os) : 0) },
};
export const fixEngine = (key, bare) => engineFor('fix:' + key, FIXES[key].razor, FIXES[key].subs, bare);
export const baseEngine = () => engineFor('fix:base', [], []);

/* ---------------------------------------------------------------- subjects */
export function subjects() {
  return LP.items.filter(it => !it.key.endsWith('~r')).map(it => ({ key: it.key, patch: samplePatch(LP.run.seed, it.i, it.mode).patch, seed: hash32(LP.run.seed, it.i, 77) }));
}
const r2 = x => (typeof x === 'number' ? Math.round(x * 100) / 100 : x);
const pick = o => Object.fromEntries(Object.entries(o).filter(([k]) => !k.startsWith('_')).map(([k, v]) => [k, r2(v)]));
function toggled(p, t) { return Object.assign({}, p, t.over || {}, t.overFn ? t.overFn(p) : {}); }

/* ---------------------------------------------------------------- jobs */
function jobsFor(pass) {
  const J = [];
  if (pass === 'matrix') for (const s of subjects()) for (const [nn, note] of NOTES) {
    J.push({ id: `${s.key}|${nn}|none`, key: s.key, note: nn, toggle: null });
    for (const t of MATRIX_KEYS) if (TOGGLES[t].applies(s.patch)) J.push({ id: `${s.key}|${nn}|${t}`, key: s.key, note: nn, toggle: t });
  }
  if (pass === 'fixes' || pass === 'fixes2' || pass === 'fixes3') for (const s of subjects()) for (const [nn] of NOTES) J.push({ id: `${s.key}|${nn}`, key: s.key, note: nn });
  return J;
}
function runJob(pass, j) {
  const s = subjects().find(x => x.key === j.key), note = NOTES.find(x => x[0] === j.note)[1];
  if (pass === 'matrix') {
    /* the baseline with its output-stage split; a toggled patch against ITS OWN truth (the toggle changes the sound) */
    if (!j.toggle) { const E = estimatePatch(baseEngine(), s.patch, note, s.seed, { truth: true }); return { conv: pick(E.conv), out: pick(E.out), totalDb: r2(E.totalDb) }; }
    const t = TOGGLES[j.toggle], C = t.razor ? engineFor('tog:' + j.toggle, t.razor, []) : baseEngine();
    const E = estimatePatch(C, toggled(s.patch, t), note, s.seed, { truth: true, output: false });
    return { conv: pick(E.conv), totalDb: r2(E.totalDb) };
  }
  if (pass === 'fixes' || pass === 'fixes2' || pass === 'fixes3') {
    /* every fix of one patch and note, each judged against the ENGINE-AS-IS truth (a fix never redefines it) */
    const base = estimatePatch(baseEngine(), s.patch, note, s.seed, { truth: true, output: false }), out = { base: { conv: pick(base.conv), totalDb: r2(base.totalDb) } };
    for (const f of Object.keys(FIXES)) {
      if (f === 'base' || (FIXES[f].pass || 'fixes') !== pass) continue;
      const F = FIXES[f], N0 = s.patch.os || 2, N = N0 * (F.osMul || 1), p = Object.assign({}, s.patch, F.over || {}, { os: N });
      const E = estimatePatch(fixEngine(f), p, note, s.seed, { truth: true, output: false, truthOf: base._T,
        gainN: F.gain ? F.gain(N) : decimGain2(N), latN: F.latency ? F.latency(N) : 0 });
      out[f] = { conv: pick(E.conv), totalDb: r2(E.totalDb) };
    }
    return out;
  }
  throw new Error('unknown pass ' + pass);
}

/* ---------------------------------------------------------------- storage and run */
export function readPass(pass) {
  const rows = new Map(), d = join(OUT, pass);
  if (!existsSync(d)) return rows;
  for (const f of readdirSync(d).filter(f => f.endsWith('.jsonl'))) for (const l of readFileSync(join(d, f), 'utf8').split('\n')) {
    if (!l) continue; let r; try { r = JSON.parse(l); } catch { continue; } rows.set(r.id, r); }
  return rows;
}
function arg(name, def) { const i = process.argv.indexOf('--' + name); return i > 0 ? process.argv[i + 1] : def; }
if (!isMainThread && workerData && workerData.kind === 'alias_sources') {
  for (const j of workerData.jobs) parentPort.postMessage({ line: JSON.stringify(Object.assign({}, j, runJob(workerData.pass, j))) });
  parentPort.postMessage({ done: true });
}
async function runPass(pass) {
  const workers = +arg('workers', Math.max(1, Math.min(5, cpus().length - 3)));
  const d = join(OUT, pass); mkdirSync(d, { recursive: true });
  const have = readPass(pass), jobs = jobsFor(pass).filter(j => !have.has(j.id));
  console.error(`alias_sources ${pass}: ${have.size} on disk, ${jobs.length} to run on ${workers} workers`);
  if (!jobs.length) return;
  const shares = Array.from({ length: workers }, (_, w) => jobs.filter((_, k) => k % workers === w));
  const t0 = Date.now(); let done = 0;
  await Promise.all(shares.map((share, w) => new Promise((res, rej) => {
    const file = join(d, `w${w}-${Date.now().toString(36)}.jsonl`);
    const wk = new Worker(new URL(import.meta.url), { workerData: { kind: 'alias_sources', pass, jobs: share } });
    wk.on('message', m => { if (m.done) return res(); appendFileSync(file, m.line + '\n'); done++;
      if (done % 20 === 0 || done === jobs.length) { const el = (Date.now() - t0) / 1000; console.error(`  ${done}/${jobs.length}  ${el.toFixed(0)} s, ETA ${(el / done * (jobs.length - done)).toFixed(0)} s`); } });
    wk.on('error', rej); wk.on('exit', c => { if (c) rej(new Error('worker exit ' + c)); });
  })));
  console.error(`alias_sources ${pass}: done in ${((Date.now() - t0) / 1000).toFixed(0)} s`);
}

/* ---------------------------------------------------------------- CPU (Layer-E: measured, never gated) */
/* each fix's render cost against the engine as it is, same patch, same note, min of `reps` timings of a
   0.3 s render (A3), per voice; the machine's load average is printed beside it because the numbers
   carry it */
function cpu() {
  const reps = +arg('reps', 5), keys = (arg('keys', 'broad#828,broad#511,broad#169,broad#857,broad#383,broad#758,broad#540,broad#730')).split(','), out = [];
  const S = subjects().filter(s => keys.includes(s.key));
  const t = (C, p, seed) => { let best = Infinity; for (let r = 0; r < reps; r++) { const x = renderWith(C, p, { n: 14400, ev: [[0, 'on', 57, 0.8]] }, { seed, timing: true, block: 128 }); best = Math.min(best, x.timing.reduce((a, [, ns]) => a + ns, 0) / 1e9 / (14400 / SR)); } return best; };
  const la0 = loadavg();
  for (const s of S) {
    const row = { key: s.key, os: s.patch.os || 2 };
    for (const f of Object.keys(FIXES)) { const F = FIXES[f], p = F.osMul ? Object.assign({}, s.patch, { os: (s.patch.os || 2) * F.osMul }) : s.patch; row[f] = t(fixEngine(f, true), p, s.seed); }
    out.push(row);
  }
  console.log(JSON.stringify({ load: { before: la0, after: loadavg(), cpus: cpus().length, model: cpus()[0].model }, reps, rows: out }, null, 1));
}

/* ---------------------------------------------------------------- broad#828 (Part B's special case) */
/* the component near 0.2 x the internal rate: where it sits at each os, when, and what removes it.
   `loopRaw` feeds the member's RAW sample back instead of its PolyBLEP output, which lags it by one
   internal sample (razor-core.js stepM returns m.prev + _cp, the previous sample corrected): if the
   loop's taps at 2 and 3 internal samples are what lock it to 0.2 x the rate, taps at 1 and 2 would
   move it to 1/3 of the rate or remove it. */
const LOOP_RAW = [['if (xOn){ mm.y2 = mm.y1; mm.y1 = y; }', 'if (xOn){ mm.y2 = mm.y1; mm.y1 = mm.prev; }', 'feed back the raw sample']];
function bandDb(L, R, a, b, fHz, bw) {
  const S = M.spectrum(M.mono(L.subarray(a, b), R.subarray(a, b)), SR, 2048);
  let tot = 0, bd = 0; for (let k = 1; k < S.P.length; k++) { tot += S.P[k]; if (Math.abs(k * S.binHz - fHz) < bw) bd += S.P[k]; }
  return 10 * Math.log10(Math.max(bd, 1e-30) / tot);
}
const at = (frac, os) => { const f = (frac * SR * os) % SR; return f > SR / 2 ? SR - f : f; };
function b828() {
  const s = subjects().find(x => x.key === 'broad#828'), note = 76, out = { key: s.key, rows: [] };
  const variants = [['as is', {}, null], ['fb 0', { fb: 0 }, null], ['xm 0', { xm: 0 }, null], ['collision 0', { colK: 0, colB: 0 }, null], ['hash -> sine (mshape 0)', { mshape: 0 }, null],
    ['dcMode 0', { dcMode: 0 }, null], ['blade envelope off (benvK/W 0)', { benvK: 0, benvW: 0 }, null], ['blade 2 off', { b2on: 0 }, null], ['feed back the raw sample', {}, 'loopRaw']];
  for (const [name, over, eng] of variants) {
    const C = eng ? engineFor('b828:' + eng, LOOP_RAW, []) : baseEngine(), row = { name };
    for (const os of [1, 2, 4]) {
      const r = renderWith(C, Object.assign({}, s.patch, over, { os }), SCRIPT(note), { seed: s.seed });
      row['os' + os] = { at02: r2(at(0.2, os)), attack: r2(bandDb(r.L, r.R, 0, 2400, at(0.2, os), 1500)), mid: r2(bandDb(r.L, r.R, 2400, 7200, at(0.2, os), 1500)), late: r2(bandDb(r.L, r.R, 7200, 14400, at(0.2, os), 1500)),
        at033: r2(at(1 / 3, os)), third: r2(bandDb(r.L, r.R, 2400, 14400, at(1 / 3, os), 1500)) };
    }
    row.conv = pick(convergence(C, Object.assign({}, s.patch, over), note, s.seed));
    out.rows.push(row);
  }
  /* the loop's small-signal gain: 2*|slope| of the blades the xin reaches, times the taps' sum at 0.2 x the rate */
  console.log(JSON.stringify(out, null, 1));
}

/* ---------------------------------------------------------------- 1-ULP sensitivity (Part A3) */
/* broad#383's Chrome/Node drift: the page's own render and measurement (listening_sample.mjs loadPage),
   the segment re-rendered with ONE value nudged by 1 or 2 ULP (the played frequency via a subclass whose
   noteOn adds the ULPs; fb, detune, w in the patch). A chaotic patch swings like the browser did; a
   regular one reads identically (the must-read-zero twin: any patch whose spread is 0). */
async function ulp() {
  const key = process.argv[3] || 'broad#383', segId = process.argv[4] || 'E5';
  const { loadPage } = await import('./listening_sample.mjs'), { loadEngine } = await import('./space.mjs');
  const P = loadPage(), { Composed } = loadEngine(), it = LP.items.find(x => x.key === key);
  const patch0 = samplePatch(LP.run.seed, it.i, it.mode).patch, pseed = hash32(LP.run.seed, it.i, 77), seg = P.SEGS.find(x => x.id === segId);
  const f64 = new Float64Array(1), u64 = new BigUint64Array(f64.buffer), nudge = (x, n) => { f64[0] = x; u64[0] = u64[0] + BigInt(n); return f64[0]; };
  const cls = n => (n ? class extends Composed { noteOn(a, f, v) { return super.noteOn(a, nudge(f, n), v); } } : Composed);
  const meas = (C, p) => P.runSync(P.measureSegmentSteps(M, C, p, pseed, seg, P.runSync(P.renderSegmentSteps(C, p, pseed, seg))));
  const rows = [['baseline', meas(Composed, patch0)]];
  for (const n of [1, -1, 2]) rows.push([`freq ${n > 0 ? '+' : ''}${n} ULP`, meas(cls(n), patch0)]);
  for (const k of ['fb', 'detune', 'w']) for (const n of [1, -1]) rows.push([`${k} ${n > 0 ? '+' : ''}${n} ULP`, meas(Composed, Object.assign({}, patch0, { [k]: nudge(patch0[k], n) }))]);
  const al = rows.map(r => r[1].aliasDb);
  console.log(JSON.stringify({ key, seg: segId, committed: it.metrics.aliasDb, rows: rows.map(([n, m]) => ({ n, aliasDb: m.aliasDb, roughness: m.roughness, rootPresence: m.rootPresence, noiseDb: m.noiseDb, rmsDb: m.rmsDb })),
    spreadDb: r2(Math.max(...al) - Math.min(...al)) }, null, 1));
}

/* ---------------------------------------------------------------- summary */
function summary() {
  const mx = readPass('matrix'), fx = readPass('fixes');
  const byKey = {};
  for (const r of mx.values()) { const k = `${r.key}|${r.note}`; (byKey[k] = byKey[k] || {})[r.toggle || 'base'] = r; }
  for (const r of fx.values()) { const k = `${r.key}|${r.note}`; ((byKey[k] = byKey[k] || {}).fix = byKey[k].fix || {})[r.fix] = r; }
  console.log(JSON.stringify(byKey));
}

if (isMainThread && process.argv[1] && process.argv[1].endsWith('alias_sources.mjs')) {
  const cmd = process.argv[2];
  if (['matrix', 'fixes', 'fixes2', 'fixes3'].includes(cmd)) await runPass(cmd);
  else if (cmd === 'cpu') cpu();
  else if (cmd === 'b828') b828();
  /* not awaited: ulp() imports listening_sample.mjs, which imports gauntlet.mjs, which imports this module;
     awaiting here would hold this module's evaluation open and the import would wait on it for ever */
  else if (cmd === 'ulp') ulp().catch(e => { console.error(e); process.exit(1); });
  else if (cmd === 'summary') summary();
  else { console.error('usage: alias_sources.mjs matrix|fixes|cpu|b828|ulp|summary [--workers W]'); process.exit(2); }
}
