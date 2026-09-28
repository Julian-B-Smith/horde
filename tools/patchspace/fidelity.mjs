/*
 * fidelity.mjs — B325: the shared machinery of the DSP fidelity audit. HYPERSAW, 2026-09-28,
 * ROADMAP B325 (records PR #821, branch lead-records-128). The human: "I'm also starting to
 * notice more noise and clicks that I'm not certain are supposed to be part of the
 * waveforms. Let's make sure the DSP fidelity is holding up."
 *
 * Used by fidelity_audit.mjs (the audit run and its WAVs) and fidelity_scan_check.mjs (the
 * standing check). Everything here is DETERMINISTIC: Math.random is a seeded mulberry32
 * around every engine instance (the lab's convention), no clock is read.
 *
 * WHAT IS HERE
 *   PHRASES   four played scripts at 48 kHz, events on 128-sample boundaries (the
 *             AudioWorklet quantum, where the lab's messages land): a held chord, a
 *             repeated note (B310's report), a fast arpeggio (pool pressure: steals), and
 *             a legato line (overlaps; glides on the mono presets).
 *   ENGINES   RazorCore (the SCALPEL oracle, PROTECTED, required and never edited), the
 *             composed engine (docs/design/scalpel-horde-engine.js), and ABLATION
 *             VARIANTS of either. A variant of the oracle is built by a TEXT patch of
 *             RazorCore.toString() evaluated in memory: the file on disk is never
 *             touched, and each patch asserts its anchor occurs exactly once, so an
 *             oracle edit that moves the anchor fails loudly instead of silently
 *             ablating nothing (extract_core.mjs's rule).
 *   clickEvents()  a TRANSIENT detector for phrases. B316's clicks() compares every frame
 *             with the median of the WHOLE buffer, which is right for one held note but
 *             not for a phrase whose level moves (a quiet tail would read a loud
 *             neighbour as a click, and a loud passage would hide a real one). Here the
 *             baseline is LOCAL: the median of the frames within ±16 hops (±85 ms),
 *             excluding the frame's own ±2 (a click spans 2-3 overlapping frames and
 *             must not raise its own baseline). Same residual as clicks(): the second
 *             difference, 512-sample frames, hop 256, per channel. A click frame is
 *             > 20 dB over its local baseline AND its residual RMS is over -90 dBFS
 *             (about a -66 dBFS step: the audibility floor, stated, not tuned per
 *             preset). Consecutive click frames merge into one EVENT.
 *   log hooks  the engine instance is wrapped (never its class) so every voice start is
 *             logged with the slot's state before it (active, gated, env): a STEAL is a
 *             start on a slot that is still active. Voice ends are read between blocks.
 */
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, SR, mulberry32, FILES, readRepo } from './space.mjs';

const require = createRequire(import.meta.url);
export { SR, mulberry32 };
export const mtof = n => 440 * Math.pow(2, (n - 69) / 12);

/* ---------------------------------------------------------------- engines */
const RAZOR = require(join(ROOT, FILES.oracle));
const { makeComposedEngine, swarmSourceFromHtml } = require(join(ROOT, FILES.engine));
export const SWARM_SRC = swarmSourceFromHtml(readRepo(FILES.swarm));
export const PRESETS = JSON.parse(readFileSync(join(ROOT, 'reference/scalpel/data/presets.json'), 'utf8')).presets;

/* The oracle's source with text patches applied, evaluated to a class. PANS is a static
   the class body does not carry (razor-core.js assigns it after the class). */
export function patchedRazor(patches) {
  let src = RAZOR.toString();
  for (const [from, to, what] of patches) {
    const n = src.split(from).length - 1;
    if (n !== 1) throw new Error(`fidelity.mjs: ablation "${what}": anchor found ${n} times in razor-core.js (need exactly 1): ${from}`);
    src = src.replace(from, to);
  }
  const C = new Function(src + '\nreturn RazorCore;')();
  C.PANS = RAZOR.PANS;
  return C;
}

/* ABLATIONS. Each removes ONE candidate source and nothing else it can avoid. `oracle`
   patches RazorCore's text (both engines inherit it, since the composed engine extends the
   class it is handed); `composed` post-processes the composed class; `patch` edits the
   patch (a PATH ablation: the path is switched off, so the sound changes too — used only
   to implicate a path, never to measure its size). */
export const ABLATIONS = {
  crushExit: { what: 'reflected Crush lands on the base at the blade EXIT (st + w), not at mid-blade (st + w/2)',
    oracle: [
      /* a linear ramp over the reflected half closes the gap between where the palindrome
         returns (the entry's level: the base at st under slew, the first step when hard)
         and the base at the exit, so the blade leaves continuously */
      ['ns.inside = true; ns.g = 1;\n        return xin0 + p.depth*(hot - xin0);',
        'ns.inside = true; ns.g = 1;\n        if (p.mirror === 1 && e > w*0.5){ const ex = st + w, lvE = sl > 0.001 ? RazorCore.wave(p.base, st) : RazorCore.crushAvg(p.base, st, kk, 0);\n' +
        '          hot += (RazorCore.wave(p.base, ex - Math.floor(ex)) - lvE)*(e - w*0.5)/(w*0.5); }\n        return xin0 + p.depth*(hot - xin0);', 'crushExit'],
    ] },
  scanCol: { what: 'the PolyBLEP scanner stops adding the collision accumulator for a Crush blade (which never reads it)',
    oracle: [['if (ns.cd){ o0 += ns.cacc - ns.cd; o1 += ns.cacc; }', 'if (ns.cd && g.mode !== 6){ o0 += ns.cacc - ns.cd; o1 += ns.cacc; }', 'scanCol']] },
  aa: { what: 'antialiasing (every PolyBLEP correction) off: implicates the BLEP placement itself', patch: { aa: 0 } },
  dcJitter: { what: 'the per-cycle DC estimate is taken ONCE per note (no refresh, so no jitter)',
    oracle: [['if (dcTick && j === 0 && (--mm.dcWait <= 0 || mm.dcInit)){', 'if (dcTick && j === 0 && mm.dcInit){', 'dcJitter']] },
  dcOff: { what: 'DC correction off entirely (dcMode 0): implicates the estimator or the 8 Hz blocker', patch: { dcMode: 0 } },
  endRamp: { what: 'a voice is not cut at env 1e-4 but decays to 1e-9 first (the stop ramp)',
    oracle: [['if (v.env < 1e-4){ v.env = 0; v.active = false; }', 'if (v.env < 1e-9){ v.env = 0; v.active = false; }', 'endRamp']] },
  noSteal: { what: 'a pool of 8 (the two slots RazorCore allocates beyond poly 6) so no phrase here needs a steal', patch: { poly: 8 } },
  oracleLaw: { what: 'the composed engine allocates by the ORACLE\'s voice law (same-note reuse, then free, then oldest) instead of B310\'s tiers',
    composed: C => class extends C { noteOn(n, f, v) { return RAZOR.prototype.noteOn.call(this, n, f, v); } } },
  interplay: { what: 'v1.1 interplay off (serial b2mix 0, collision colK/colB 0)', patch: { b2mix: 0, colK: 0, colB: 0 } },
  rotation: { what: 'rotation off (rotRate, rotSpread and blade 2\'s twins 0)', patch: { rotRate: 0, rotSpread: 0, rotRate2: 0, rotSpread2: 0 } },
  xmfb: { what: 'cross-member modulation and feedback off', patch: { xm: 0, fb: 0 } },
  noise: { what: 'random-law and drift spreads off (law 0), which holds every member offset still', patch: { law: 0 } },
  splitPhase: { what: 'the composed engine\'s blade phase is re-read from the swarm phase every sample (one integrator)',
    composed: C => class extends C {
      stepM(m, dphi, c, k, s) {
        if (this.src === 'horde' && m.j === 0) m.phi = (x => x - Math.floor(x))(this.sw.swarms[m.v.si].phase[m.i] + this.origin);
        return super.stepM(m, dphi, c, k, s);
      } } },
  tick1: { what: 'the composed engine ticks the swarm every sample (dt 1/sr) instead of every 16 (horde\'s control tick), so member frequencies stop stepping',
    swarm: [['TICK = 16;', 'TICK = 1;', 'tick1']],
    composed: C => class extends C { stepM(m, dphi, c, k, s) { if (this.src === 'horde' && m.j === 0 && m.i === 0) m.v.sn = 0; return super.stepM(m, dphi, c, k, s); } } },
  coupling: { what: 'coupling off (K 0): where the two laws coincide (ACCOUNTING row 4), so what remains is not the coupling law', patch: { K: 0 } },
  settledStart: { what: 'the composed engine starts a "settled" (phaseMode 2) or random voice with SwarmSynth\'s random draw instead of the aligned retrig start', patch: { phaseMode: 0 } },
};

const CACHE = new Map();
/* kind 'oracle' | 'composed'; ablation a key of ABLATIONS or null */
export function engineClass(kind, ablation) {
  const key = kind + ':' + (ablation || '');
  if (CACHE.has(key)) return CACHE.get(key);
  const A = ablation ? ABLATIONS[ablation] : null;
  if (ablation && !A) throw new Error('fidelity.mjs: unknown ablation ' + ablation);
  const R = A && A.oracle ? patchedRazor(A.oracle) : RAZOR;
  let sw = SWARM_SRC;
  if (A && A.swarm) for (const [from, to, what] of A.swarm) {
    if (sw.split(from).length !== 2) throw new Error(`fidelity.mjs: ablation "${what}": anchor not found exactly once in swarmsaw.html: ${from}`);
    sw = sw.replace(from, to);
  }
  let C = kind === 'oracle' ? R : makeComposedEngine(R, sw);
  if (A && A.composed) { if (kind !== 'composed') C = null; else C = A.composed(C); }
  CACHE.set(key, C);
  return C;
}
/* the ablation reaches this engine at all (a composed-only ablation has no oracle twin) */
export const ablationApplies = (kind, ab) => !ab || (!ABLATIONS[ab].composed && !ABLATIONS[ab].swarm) || kind === 'composed';

/* ---------------------------------------------------------------- phrases */
/* [sample, 'on'|'off', note, vel]; every time a multiple of 128. `n` total samples. */
const Q = x => Math.round(x * SR / 128) * 128;
function arp() {
  const seq = [48, 52, 55, 60, 64, 67, 72, 67, 64, 60, 55, 52], ev = [];
  for (let i = 0; i < 24; i++) { const t = Q(i * 0.0625); ev.push([t, 'on', seq[i % seq.length], 0.8], [Q(i * 0.0625 + 0.05), 'off', seq[i % seq.length]]); }
  return ev;
}
function repeat() {
  const ev = [];
  for (let i = 0; i < 8; i++) ev.push([Q(i * 0.2), 'on', 57, 0.8], [Q(i * 0.2 + 0.12), 'off', 57]);
  return ev;
}
function legato() {
  const seq = [57, 60, 64, 62, 59, 57], ev = [];
  for (let i = 0; i < seq.length; i++) ev.push([Q(i * 0.3), 'on', seq[i], 0.8], [Q(i * 0.3 + 0.35), 'off', seq[i]]);
  return ev;
}
const sortEv = ev => ev.sort((a, b) => a[0] - b[0] || (a[1] === 'off' ? -1 : 1));
export const PHRASES = {
  chord: { what: 'held chord C3 G3 C4 E4, 1.2 s, then 0.8 s of tail', n: Q(2.0),
    ev: sortEv([48, 55, 60, 64].flatMap(n => [[0, 'on', n, 0.8], [Q(1.2), 'off', n]])) },
  repeat: { what: 'A3 struck 8 times, 120 ms on / 80 ms off (B310: each strike inside the last release)', n: Q(2.0), ev: sortEv(repeat()) },
  arp: { what: '16 notes/s over C3..C5, 50 ms gates, 24 notes (pool pressure: steals once tails outlast 6 slots)', n: Q(2.0), ev: sortEv(arp()) },
  legato: { what: 'six overlapping notes, 350 ms each every 300 ms (50 ms overlaps; glides on mono presets)', n: Q(2.4), ev: sortEv(legato()) },
};

/* ---------------------------------------------------------------- rendering */
/* One render of a preset's params (plus a patch-level ablation) through an engine class,
   with the voice log. Math.random is seeded around the instance and restored. */
export function renderPhrase(Cls, params, phrase, opt) {
  opt = opt || {};
  const seed = (opt.seed === undefined ? 0xB325 : opt.seed) >>> 0, block = opt.block || 128;
  const saved = Math.random;
  Math.random = mulberry32(seed);
  try {
    const c = new Cls(SR);
    c.set(Object.assign({}, params, opt.over || {}));
    Object.assign(c.s, c.t);
    const n = phrase.n, L = new Float32Array(n), R = new Float32Array(n), log = [];
    let pos = 0;
    const sv = c.startVoice;
    c.startVoice = function (v, note, freq, vel, fresh, retrig) {
      /* `cut`: what a FRESH start on a sounding slot throws away at once, in output units
         (small-signal: × gain × 1.6, before the tanh): env·vel·norm·Σ member·pan of the members'
         last outputs (m.prev, before the DC correction). A reuse (fresh false) keeps the phases,
         so it cuts nothing. */
      let cut = 0;
      if (fresh && v.active) {
        const N = this.d.N; let cl = 0, cr = 0;
        for (let q = 0; q < N; q++) { cl += v.m[q].prev * this.gl[q]; cr += v.m[q].prev * this.gr[q]; }
        cut = v.env * v.vel / Math.sqrt(N) * Math.max(Math.abs(cl), Math.abs(cr)) * this.s.gain * 1.6;
      }
      log.push({ t: pos, kind: 'start', slot: this.voices.indexOf(v), note, fresh, wasActive: v.active, wasGate: v.gate, env: v.env, cut });
      return sv.call(this, v, note, freq, vel, fresh, retrig);
    };
    const act = c.voices.map(v => v.active);
    for (const [t, kind, note, vel] of phrase.ev) {
      while (pos < t) { const e = Math.min(t, pos + block); c.render(L.subarray(pos, e), R.subarray(pos, e)); pos = e; ends(); }
      if (kind === 'on') c.noteOn(note, mtof(note), vel); else { log.push({ t: pos, kind: 'off', note }); c.noteOff(note); }
      c.voices.forEach((v, i) => { act[i] = v.active; });
    }
    while (pos < n) { const e = Math.min(n, pos + block); c.render(L.subarray(pos, e), R.subarray(pos, e)); pos = e; ends(); }
    function ends() { c.voices.forEach((v, i) => { if (act[i] && !v.active) log.push({ t: pos, kind: 'end', slot: i }); act[i] = v.active; }); }
    return { L, R, log, core: c };
  } finally { Math.random = saved; }
}

/* ---------------------------------------------------------------- the neutral pair */
/* THE SENSITIVE LAYER. Where the two swarm laws coincide (ACCOUNTING row 4: K 0, dist 0,
   law 0, no drift, the aligned retrig start) the composed engine must equal the oracle
   SAMPLE FOR SAMPLE once the oracle's aligned start is moved by §1.6.6's ½ (the fixture
   composed_engine_check's O3 uses) and the composed engine allocates by the oracle's voice
   law (B310's law differs by design, and composed_engine_check's VL rows own it). Measured:
   max|Δ| is EXACTLY 0 over whole phrases, blades included, so any plumbing defect in the
   composition (a phase slip, a start without its ramp, a jitter) shows as a non-zero
   difference, however masked it would be in the mix. */
export const NEUTRAL = { K: 0, phaseMode: 1 };
export function neutralPair() {
  const O = engineClass('oracle');
  const Oshift = CACHE.get('oracle:shift') || class extends O {
    startVoice(v, n, f, vel, fresh, re) { super.startVoice(v, n, f, vel, fresh, re); if (fresh) for (const m of v.m) m.phi = 0.5; }
  };
  CACHE.set('oracle:shift', Oshift);
  return { oracle: Oshift, composed: engineClass('composed', 'oracleLaw') };
}
/* sample-level difference of two renders: max |Δ|, the largest one-sample STEP of Δ, and where */
export function diffStats(a, b) {
  let max = 0, step = 0, at = -1, n40 = 0;
  for (const [x, y] of [[a.L, b.L], [a.R, b.R]]) {
    let d0 = 0;
    for (let i = 0; i < x.length; i++) {
      const d = y[i] - x[i], s = Math.abs(d - d0); d0 = d;
      if (Math.abs(d) > max) { max = Math.abs(d); at = i; }
      if (s > step) step = s;
      if (s > 0.01) n40++;
    }
  }
  return { max, step, at, n40 };
}
export const dBFS = x => (x > 0 ? 20 * Math.log10(x) : -Infinity);
/* a phrase cut short (the standing check's budget): events past the end dropped */
export function shortPhrase(ph, secs) { const n = Math.round(secs * SR / 128) * 128; return { what: ph.what + ` (first ${secs} s)`, n, ev: ph.ev.filter(e => e[0] < n) }; }

/* ---------------------------------------------------------------- the transient detector */
export const CLICK = { F: 512, H: 256, W: 16, X: 2, ratioDb: 20, floorDb: -90 };
function frameEnergy(x) {
  const { F, H } = CLICK, e = [];
  for (let s = 2; s + F <= x.length; s += H) {
    let acc = 0; for (let i = s; i < s + F; i++) { const d = x[i] - 2 * x[i - 1] + x[i - 2]; acc += d * d; }
    e.push(acc / F);
  }
  return e;
}
function localBase(e, f) {
  const { W, X } = CLICK, w = [];
  for (let g = Math.max(0, f - W); g <= Math.min(e.length - 1, f + W); g++) if (Math.abs(g - f) > X) w.push(e[g]);
  if (!w.length) return 0;
  w.sort((a, b) => a - b);
  return w[w.length >> 1];
}
/* events: [{ t (sample of the frame centre), frames, excessDb (worst frame over its baseline),
   levelDb (worst frame's residual RMS, dBFS), energy (Σ excess residual energy, both channels) }] */
export function clickEvents(L, R) {
  const { F, H, ratioDb, floorDb } = CLICK, thr = Math.pow(10, ratioDb / 10), floor = Math.pow(10, floorDb / 10);
  const eL = frameEnergy(L), eR = frameEnergy(R), n = eL.length, hits = [];
  for (let f = 0; f < n; f++) {
    let best = null;
    for (const e of [eL, eR]) {
      const b = Math.max(localBase(e, f), 1e-20), r = e[f] / b;
      if (e[f] > floor && r > thr && (!best || r > best.r)) best = { r, lv: e[f], ex: (e[f] - b) * F };
    }
    hits.push(best);
  }
  const ev = [];
  for (let f = 0; f < n; f++) {
    if (!hits[f]) continue;
    const cur = ev.length && ev[ev.length - 1].last === f - 1 ? ev[ev.length - 1] : null;
    if (cur) { cur.last = f; cur.frames++; cur.energy += hits[f].ex; if (hits[f].r > cur.r) { cur.r = hits[f].r; cur.lv = hits[f].lv; cur.f = f; } }
    else ev.push({ f, last: f, frames: 1, r: hits[f].r, lv: hits[f].lv, energy: hits[f].ex });
  }
  return ev.map(e => ({ t: 2 + e.f * H + F / 2, frames: e.frames, excessDb: 10 * Math.log10(e.r), levelDb: 10 * Math.log10(e.lv), energy: e.energy }));
}
export const clickEnergyDb = evs => { const s = evs.reduce((a, e) => a + e.energy, 0); return s > 0 ? 10 * Math.log10(s) : -200; };

/* which logged engine event a click sits on: within one frame of its centre */
export function nearestCause(click, log) {
  const tol = CLICK.F;
  let best = null;
  for (const e of log) {
    const d = Math.abs(e.t - click.t);
    if (d > tol) continue;
    const kind = e.kind === 'start' ? (e.wasActive ? (e.fresh ? 'steal' : 'retrigger') : 'onset') : e.kind;
    const rank = { steal: 0, retrigger: 1, end: 2, onset: 3, off: 4 }[kind];
    if (!best || rank < best.rank || (rank === best.rank && d < best.d)) best = { kind, d, rank, e };
  }
  return best ? best.kind : 'mid-note';
}

/* do two click lists share an event (within two hops)? used by the ablation attribution */
export const sameEvent = (a, b) => Math.abs(a.t - b.t) <= 2 * CLICK.H;

/* ---------------------------------------------------------------- WAV (local listening only) */
export function wav(L, R, sr) {
  const n = L.length, buf = Buffer.alloc(44 + n * 4);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n * 4, 4); buf.write('WAVE', 8); buf.write('fmt ', 12);
  buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22); buf.writeUInt32LE(sr, 24);
  buf.writeUInt32LE(sr * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34); buf.write('data', 36); buf.writeUInt32LE(n * 4, 40);
  const q = x => Math.max(-32768, Math.min(32767, Math.round((Number.isFinite(x) ? x : 0) * 32767)));
  for (let i = 0; i < n; i++) { buf.writeInt16LE(q(L[i]), 44 + i * 4); buf.writeInt16LE(q(R[i]), 46 + i * 4); }
  return buf;
}
