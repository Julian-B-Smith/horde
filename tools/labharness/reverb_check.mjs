/*
 * reverb_check.mjs — the fidelity suite for docs/design/reverb-lab.html.
 *
 * WHY IT IS STANDALONE AND HAND-RUN. The lab is JavaScript, so there is no
 * CMake target to hang it on, and `./verify` contains zero references to the
 * reverb (B152 audit §6): the lab's only coverage today is lab_load_check.mjs,
 * which proves the file parses and nothing about what it computes. This is that
 * missing oracle, in the lab_load_check idiom — run by hand, prints numbers,
 * exits non-zero on failure:
 *
 *     node tools/labharness/reverb_check.mjs          (~45 s, V6 is 38 of them)
 *     node tools/labharness/reverb_check.mjs V5 V6    (named checks only)
 *
 * UNWIRED: COST, not doubt — ~45 s of measurement (V6 alone is 38) on a
 *   `./verify full` that runs in 75.5 s warm, so wiring it is a 60 % increase
 *   for a lab nothing consumes yet: SwarmVerb has no port, no shell route and
 *   no caller, so a regression here cannot reach a user. The reason expires
 *   the day the port lands — wire it in that PR. Re-ruled 2026-09-21 (B159),
 *   the one of the ten kept unwired; the other nine cost 2.0 s together.
 *
 * IT EXTRACTS, IT DOES NOT REIMPLEMENT. SwarmVerb is sliced live out of the
 * HTML by tools/golden/extract_core.mjs, so the thing measured is the thing
 * that makes the sound. A re-implementation agrees with itself and certifies
 * nothing (feedback_scan.mjs carries the same note).
 *
 * WHAT THE THRESHOLDS ARE. Every gate is a number MEASURED on this lab plus a
 * stated margin — never an invented target. In particular V2 pins today's RT60
 * calibration error (+31 % at 0.5 s) rather than demanding the knob be right:
 * fixing that calibration is a design question for the port (audit R5), and a
 * gate that fails from day one is a gate nobody reads. When the port changes a
 * number deliberately, re-measure and move the pin, with the reason.
 *
 * DETECTORS, AND THE TWO THAT ARE BANNED HERE. Comb content is measured by
 * `combPeak` (cepstral: the autocorrelation height of the de-tilted log
 * spectrum). The two obvious alternatives were calibrated and REJECTED by the
 * audit and must not come back:
 *   · raw spectral flatness reads 0.301 on a comb-FREE but merely tilted tail
 *     against 0.260 for a real comb — it measures damping, not combing;
 *   · the lab's own 1/3-octave `roughnessDb` reads a known L=2227 comb (the
 *     lab's own shortest line) at 1.234 dB, BELOW its own 1.744 dB comb-free
 *     floor — structurally blind at exactly the lengths it would be used on.
 * combPeak separates the same cases by 50x (0.012 comb-free vs 0.83 combed),
 * and both of those controls run below, every time, so the separation is
 * re-proved rather than remembered. Only its peak height is used; its lag
 * estimate aliases for long L.
 *
 * EVERY CHECK CARRIES A MUST-FAIL CONTROL (L0016/L0032). A probe that only ever
 * reports the expected answer has not been shown to be able to report any
 * other. For the two fixes that have a known pre-fix defect, the control is the
 * defect itself, planted back into a scratch copy of the DSP by anchored source
 * substitution — and the audit's pre-fix numbers are the expected failures, so
 * the control is falsifiable too. The substitution mechanism is itself verified
 * by a bogus anchor that must throw.
 *
 * Provenance: docs/audits/2026-09-18-reverb-lab-audit.md (§0 detector
 * calibration, §3 the suite table V1-V14, §5.4 the fix order).
 */
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { extractCore, BANNERS } from '../golden/extract_core.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '../..');
const LAB = join(ROOT, 'docs/design/reverb-lab.html');
const SwarmVerb = extractCore(LAB, 'SwarmVerb', 'design');

// ---------------------------------------------------------------- planting --
// A mutated copy of the DSP section, in memory only — the tracked file is never
// written. Each substitution asserts its anchor: if the source text is not
// found the mutation THROWS rather than silently measuring the unmutated lab,
// which would turn a must-fail control into a false pass.
function planted(subs) {
  const html = readFileSync(LAB, 'utf8');
  let src = html.slice(html.search(BANNERS.design.start), html.search(BANNERS.design.end));
  for (const [from, to] of subs) {
    if (!src.includes(from)) throw new Error(`ANCHOR MISSING: ${from}`);
    src = src.split(from).join(to);
  }
  return new Function(`"use strict";\n${src}\nreturn SwarmVerb;`)();
}

// The pre-fix defects, as source reversions. Keeping them here (rather than in
// a copy of the old file) means they rot loudly: change the fixed line and the
// anchor throws.
const PLANT_R1 = [
  ['Math.max(1, Math.min(this.erBufL.length - 4, this.tapT[t] * p.erSize * 2 * sr + spin))',
   'Math.max(1, this.tapT[t] * p.erSize * 2 * sr + spin)'],
  ['if (r2 >= this.erBufL.length) r2 -= this.erBufL.length;', ''],
];
const PLANT_R2 = [
  ['if (this.tickCtr === 0) this.stepMod(dt * TICK);', 'if ((smp & (TICK - 1)) === 0) this.stepMod(dt * TICK);'],
  ['this.tickCtr = (this.tickCtr + 1) & (TICK - 1);', ''],
];

// --------------------------------------------------------------- rendering --
function mk(Klass, sr, over = {}) {
  const v = new Klass(sr);
  for (const [k, val] of Object.entries(over)) v.setParam(k, val);
  return v;
}
// Impulse response of `secs` seconds at host block size `bs`.
function ir(v, sr, secs, bs = 256, amp = 1) {
  const n = Math.round(secs * sr);
  const oL = new Float64Array(n), oR = new Float64Array(n);
  const iL = new Float64Array(bs), iR = new Float64Array(bs);
  const bL = new Float64Array(bs), bR = new Float64Array(bs);
  let done = 0, first = true;
  while (done < n) {
    const k = Math.min(bs, n - done);
    iL.fill(0); iR.fill(0);
    if (first) { iL[0] = amp; iR[0] = amp; first = false; }
    v.process(iL.subarray(0, k), iR.subarray(0, k), bL.subarray(0, k), bR.subarray(0, k));
    oL.set(bL.subarray(0, k), done); oR.set(bR.subarray(0, k), done);
    done += k;
  }
  return { L: oL, R: oR };
}
const rms = a => { let s = 0; for (const v of a) s += v * v; return Math.sqrt(s / a.length); };
const peak = a => { let m = 0; for (const v of a) m = Math.max(m, Math.abs(v)); return m; };
const firstNonZero = a => { let i = 0; while (i < a.length && a[i] === 0) i++; return i; };

// --------------------------------------------------------------- detectors --
// Octave-band bandpass (two cascaded biquads), so the decay is measured per
// band the way the standard defines it — and the way the lab's loop-gain
// compensation claims to work ("the knob means what it says at fRef = 1000 Hz").
function band(x, sr, fc, Q = 1.414) {
  const w = 2 * Math.PI * fc / sr, a = Math.sin(w) / (2 * Q), c = Math.cos(w);
  const b0 = a, b2 = -a, a0 = 1 + a, a1 = -2 * c, a2 = 1 - a;
  let y = Float64Array.from(x);
  for (let pass = 0; pass < 2; pass++) {
    let x1 = 0, x2 = 0, y1 = 0, y2 = 0; const o = new Float64Array(y.length);
    for (let i = 0; i < y.length; i++) {
      const v = (b0 * y[i] + b2 * x2 - a1 * y1 - a2 * y2) / a0;
      x2 = x1; x1 = y[i]; y2 = y1; y1 = v; o[i] = v;
    }
    y = o;
  }
  return y;
}
// Schroeder backward integration, fitted over -5..-25 dB and extrapolated to
// RT60. THE WINDOW IS DELIBERATE: the audit's tables label this "T30" but its
// harness fitted -5..-25, and the pins below are the audit's own numbers, so
// the estimator has to be the audit's too. A true -5..-35 fit reads this lab
// 9 % shorter at decay 0.5 s (0.595 vs 0.655) — the tail is not a single
// exponential, so the window is part of the measurement, not a detail.
function rt60Fit(x, sr, loDb = -5, hiDb = -25) {
  const n = x.length, e = new Float64Array(n + 1);
  for (let i = n - 1; i >= 0; i--) e[i] = e[i + 1] + x[i] * x[i];
  if (e[0] <= 0) return NaN;
  const db = new Float64Array(n);
  for (let i = 0; i < n; i++) db[i] = 10 * Math.log10(Math.max(1e-300, e[i] / e[0]));
  const at = t => { for (let i = 1; i < n; i++) if (db[i] <= t) {
    const f = (db[i - 1] - t) / (db[i - 1] - db[i]); return (i - 1 + f) / sr; } return NaN; };
  const t1 = at(loDb), t2 = at(hiDb);
  return (!isFinite(t1) || !isFinite(t2) || t2 <= t1) ? NaN : (t2 - t1) * (-60 / (hiDb - loDb));
}
function fftMag(x) {
  let n = 1; while (n < x.length) n <<= 1;
  const re = new Float64Array(n), im = new Float64Array(n);
  re.set(x.subarray(0, Math.min(x.length, n)));
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
    if (i < j) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const ur = re[i + k], ui = im[i + k];
        const vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        const vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        const ncr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = ncr;
      }
    }
  }
  const m = new Float64Array(n / 2);
  for (let i = 0; i < n / 2; i++) m[i] = Math.hypot(re[i], im[i]);
  return m;
}
// Welch-averaged magnitude: one periodogram's chi-square scatter drags the
// geometric mean down, so the raw number is not interpretable on its own.
function welchMag(x, nfft, hop) {
  const win = new Float64Array(nfft);
  for (let i = 0; i < nfft; i++) win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / nfft);
  const acc = new Float64Array(nfft / 2); let frames = 0;
  for (let o = 0; o + nfft <= x.length; o += hop) {
    const f = new Float64Array(nfft);
    for (let i = 0; i < nfft; i++) f[i] = x[o + i] * win[i];
    const m = fftMag(f);
    for (let i = 0; i < nfft / 2; i++) acc[i] += m[i] * m[i];
    frames++;
  }
  for (let i = 0; i < acc.length; i++) acc[i] = Math.sqrt(acc[i] / Math.max(1, frames));
  return acc;
}
// COMB DETECTOR (cepstral). A comb of period L samples puts a peak in the
// autocorrelation of the log-magnitude spectrum at quefrency L. The broad tilt
// is removed first so the autocorrelation sees ripple, not slope — which is
// precisely what raw flatness cannot separate.
function combPeak(x, sr, lagLo = 300, lagHi = 9000, nfft = 1 << 15) {
  const mag = welchMag(x, nfft, nfft >> 1);
  const lo = Math.max(2, Math.round(200 / sr * nfft * 2));
  const hi = Math.min(mag.length - 2, Math.round(10000 / sr * nfft * 2));
  const db = []; for (let i = lo; i <= hi; i++) db.push(20 * Math.log10(Math.max(1e-300, mag[i])));
  const W = 129, det = new Float64Array(db.length);
  for (let i = 0; i < db.length; i++) {
    let s = 0, c = 0;
    for (let j = Math.max(0, i - W); j <= Math.min(db.length - 1, i + W); j++) { s += db[j]; c++; }
    det[i] = db[i] - s / c;
  }
  let e0 = 0; for (const v of det) e0 += v * v;
  let best = 0;
  for (let Lq = lagLo; Lq <= lagHi; Lq++) {
    const q = Math.round(nfft / Lq); if (q < 2 || q >= det.length / 2) continue;
    let s = 0; for (let i = 0; i + q < det.length; i++) s += det[i] * det[i + q];
    best = Math.max(best, s / e0);
  }
  return best;
}
// mulberry32, the lab's own generator — used for the comb-free control noise.
// NOT an LCG: a 31-bit LCG's lattice structure reads combPeak 0.33 on noise
// that is comb-free, i.e. it would poison the control it is there to provide.
function mulberry(seed) {
  let s = seed | 0;
  return () => { s |= 0; s = (s + 0x6D2B79F5) | 0;
    let t = Math.imul(s ^ (s >>> 15), 1 | s);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}

// ------------------------------------------------------------------ checks --
const CHECKS = [];
const check = (id, title, fn) => CHECKS.push({ id, title, fn });
const pct = (a, b) => (a - b) / b * 100;

// V1 — silence in, silence out. Exactly zero, not "small".
check('V1', 'silence in -> silence out (exactly 0)', () => {
  const sr = 44100, lines = []; let ok = true;
  const cfgs = [['default', {}], ['max feedback', { decay: 12, damp: 0, lowCut: 20, size: 1 }],
                ['wet only', { mix: 1, erSend: 1, envelop: 1 }]];
  for (const [name, cfg] of cfgs) {
    const v = mk(SwarmVerb, sr, cfg);
    const z = new Float64Array(1024), oL = new Float64Array(1024), oR = new Float64Array(1024);
    let nz = 0;
    for (let b = 0; b < Math.round(2 * sr / 1024); b++) {
      v.process(z, z, oL, oR);
      for (let i = 0; i < 1024; i++) if (oL[i] !== 0 || oR[i] !== 0) nz++;
    }
    ok = ok && nz === 0;
    lines.push(`  ${name.padEnd(13)} non-zero samples in 2 s: ${nz}  (gate: 0)`);
  }
  // CONTROL: the same path must be able to produce a non-zero reading at all.
  const p = peak(ir(mk(SwarmVerb, sr, {}), sr, 1, 256).L);
  lines.push(`  control       impulse peak at defaults ${p.toFixed(4)}  (gate: > 0.1 — audit measured 0.650)`);
  return { ok: ok && p > 0.1, lines };
});

// V2 — RT60 vs the knob. PINS today's calibration error; does not demand it be
// right. Fixing the calibration is the port's design question (audit R5).
check('V2', 'RT60 (-5..-25 dB fit, 1 kHz band) vs the decay knob, 44.1 / 96 kHz', () => {
  // audit's pre-fix 44.1 kHz numbers, and this branch's post-fix 96 kHz ones
  const PIN = { 44100: { 0.5: 0.655, 2.2: 1.901, 6.0: 5.596 },
                96000: { 0.5: 0.653, 2.2: 1.832, 6.0: 5.300 } };
  const MARGIN = 0.05;                       // 5 % of the pinned value
  const lines = []; let ok = true;
  for (const sr of [44100, 96000]) for (const set of [0.5, 2.2, 6.0]) {
    const r = ir(mk(SwarmVerb, sr, { mix: 1, decay: set }), sr, Math.max(6, set * 3.5), 256);
    const t = rt60Fit(band(r.L, sr, 1000), sr), pin = PIN[sr][set];
    const off = Math.abs(pct(t, pin)); ok = ok && off <= MARGIN * 100;
    lines.push(`  ${sr} decay ${set.toFixed(1)}s  RT60 ${t.toFixed(3)}s  knob err ${pct(t, set).toFixed(1).padStart(6)}%  ` +
               `vs pin ${pin.toFixed(3)} -> ${off.toFixed(2)}%  (gate: <= 5 %)`);
  }
  // CONTROL: the detector on synthetic decays of known T60 (audit: <= 1.0 %).
  const sr = 44100;
  for (const want of [0.5, 2.2, 6.0]) {
    const rng = mulberry(4242), n = Math.round(want * 3.5 * sr), x = new Float64Array(n);
    for (let i = 0; i < n; i++) x[i] = (rng() * 2 - 1) * Math.pow(10, -3 * i / (want * sr));
    const got = rt60Fit(x, sr), e = Math.abs(pct(got, want)); ok = ok && e <= 2;
    lines.push(`  control synthetic ${want.toFixed(1)}s decay -> ${got.toFixed(4)}s  err ${e.toFixed(2)}%  (gate: <= 2 %)`);
  }
  return { ok, lines };
});

// V3 — comb-free tail. combPeak only; see the header for the two banned metrics.
check('V3', 'comb content of the late tail (combPeak, cepstral)', () => {
  const sr = 44100, PIN = { 'default': 0.1470, 'modulation off': 0.1751, 'full modulation': 0.0117 };
  const CFG = { 'default': {}, 'modulation off': { modDepth: 0 },
                'full modulation': { modDepth: 1, modRate: 3 } };
  const lines = []; let ok = true;
  for (const name of Object.keys(PIN)) {
    const r = ir(mk(SwarmVerb, sr, { mix: 1, decay: 4, damp: 0.45, ...CFG[name] }), sr, 6, 256);
    const cp = combPeak(r.L.subarray(Math.round(0.35 * sr), Math.round(3.5 * sr)), sr);
    const gate = PIN[name] * 1.25; ok = ok && cp <= gate;
    lines.push(`  ${name.padEnd(16)} combPeak ${cp.toFixed(4)}  (pin ${PIN[name].toFixed(4)}, gate <= ${gate.toFixed(4)} = pin + 25 %)`);
  }
  // CONTROLS: comb-free noise must read low, a known comb must read high. Both
  // every run — a 50x separation remembered is a separation unverified.
  const rng = mulberry(12345), n = sr * 3, noise = new Float64Array(n);
  for (let i = 0; i < n; i++) noise[i] = (rng() * 2 - 1) * Math.exp(-i / sr * 2);
  const cFree = combPeak(noise, sr);
  const comb = Float64Array.from(noise), Lc = 2227;   // the lab's own shortest line
  for (let i = Lc; i < comb.length; i++) comb[i] += 0.9 * comb[i - Lc];
  const cComb = combPeak(comb, sr);
  ok = ok && cFree <= 0.05 && cComb >= 0.5;
  lines.push(`  control comb-free decaying noise  ${cFree.toFixed(4)}  (gate: <= 0.05)`);
  lines.push(`  control comb g=0.9 L=2227         ${cComb.toFixed(4)}  (gate: >= 0.50)`);
  return { ok, lines };
});

// V4 — pre-delay accuracy, measured against the module's OWN zero-setting
// arrival rather than a hardcoded offset, so it does not silently re-encode the
// FDN build-up (which is what R3 was).
check('V4', 'pre-delay accuracy at 44.1 / 48 / 96 kHz', () => {
  const lines = []; let ok = true;
  const base = { mix: 1, erSend: 0, modDepth: 0, spinDepth: 0 };
  for (const sr of [44100, 48000, 96000]) {
    const t0 = firstNonZero(ir(mk(SwarmVerb, sr, { ...base, preDelay: 0 }), sr, 0.5, 256).L) / sr * 1000;
    const got = [];
    for (const set of [0.005, 0.02, 0.1]) {
      const t = firstNonZero(ir(mk(SwarmVerb, sr, { ...base, preDelay: set }), sr, 0.5, 256).L) / sr * 1000;
      const err = (t - t0) - set * 1000; got.push(err);
      ok = ok && Math.abs(err) <= 0.15;
    }
    lines.push(`  ${sr}  FDN offset ${t0.toFixed(3)} ms  errors ` +
      got.map(e => `${e >= 0 ? '+' : ''}${e.toFixed(3)}`).join(' / ') +
      ` ms  (gate: |err| <= 0.15 ms; audit measured +-0.08)`);
    // CONTROL: the three settings must not all read the same — a detector that
    // cannot move cannot certify that the delay tracks.
    if (got.length !== new Set(got.map(e => e.toFixed(6))).size && got[0] === got[2]) ok = false;
  }
  return { ok, lines };
});

// V5 — block-size independence (R2 / ADR-175). Bit-identical, not "close".
check('V5', 'block-size independence, bit-identical (R2)', () => {
  const sr = 44100, cfg = { mix: 1, decay: 3, modDepth: 0.35, modRate: 0.35, spinDepth: 1.5 };
  const lines = []; let ok = true;
  const run = (K, bs) => ir(mk(K, sr, cfg), sr, 3, bs);
  const ref = run(SwarmVerb, 256);
  for (const bs of [1, 7, 64, 256, 333]) {
    const r = run(SwarmVerb, bs);
    let md = 0;
    for (let i = 0; i < ref.L.length; i++)
      md = Math.max(md, Math.abs(ref.L[i] - r.L[i]), Math.abs(ref.R[i] - r.R[i]));
    ok = ok && md === 0;
    lines.push(`  chunk ${String(bs).padStart(4)}  max|diff| ${md.toExponential(3)}  rmsRatio ${(rms(r.L) / rms(ref.L)).toFixed(4)}  (gate: 0)`);
  }
  // stepMod rate: ceil(n/16) calls, because the tick fires on the first sample.
  for (const bs of [1, 7, 256]) {
    const v = mk(SwarmVerb, sr, cfg); let calls = 0;
    const o = v.stepMod.bind(v); v.stepMod = dt => { calls++; return o(dt); };
    ir(v, sr, 1, bs);
    const want = Math.ceil(sr / 16); ok = ok && calls === want;
    lines.push(`  chunk ${String(bs).padStart(4)}  stepMod calls/s ${calls}  (gate: ${want} = ceil(sr/16))`);
  }
  // MUST-FAIL CONTROL: plant the block-keyed tick back. Audit's pre-fix
  // numbers: 16.000x the rate at block 1, rmsRatio 0.912.
  const P = planted(PLANT_R2);
  const pref = run(P, 256), p1 = run(P, 1);
  let pmd = 0; for (let i = 0; i < pref.L.length; i++) pmd = Math.max(pmd, Math.abs(pref.L[i] - p1.L[i]));
  const pv = mk(P, sr, cfg); let pc = 0;
  const po = pv.stepMod.bind(pv); pv.stepMod = dt => { pc++; return po(dt); };
  ir(pv, sr, 1, 1);
  const controlFires = pmd > 0.1 && pc > 10 * Math.ceil(sr / 16);
  ok = ok && controlFires;
  lines.push(`  control (pre-fix tick planted, chunk 1): max|diff| ${pmd.toExponential(3)}, ` +
             `rmsRatio ${(rms(p1.L) / rms(pref.L)).toFixed(4)}, stepMod/s ${pc} = x${(pc / Math.ceil(sr / 16)).toFixed(3)}` +
             `  (must FAIL: diff > 0.1 and rate > 10x)`);
  return { ok, lines };
});

// V6 — index safety (R1). Sweep the erSize slider at its own step and count
// watchdog fires; a fire means the whole reverb was wiped to silence.
check('V6', 'index safety over the erSize slider (R1)', () => {
  const lines = []; let ok = true;
  const sweep = (K, sr) => {
    let bad = 0, worst = 0, minRms = Infinity;
    for (let e = 15; e <= 150; e++) {
      const v = mk(K, sr, { mix: 1, erSize: e / 100 });
      let fires = 0; const o = v.reset.bind(v); v.reset = () => { fires++; return o(); };
      minRms = Math.min(minRms, rms(ir(v, sr, 1, 256).L));
      if (fires) { bad++; worst = Math.max(worst, fires); }
    }
    return { bad, worst, minRms };
  };
  for (const sr of [44100, 48000, 96000]) {
    const s = sweep(SwarmVerb, sr);
    ok = ok && s.bad === 0 && s.minRms > 0;
    lines.push(`  ${sr}  ${s.bad}/136 settings wipe the reverb (gate: 0)  ` +
               `min wet RMS over the sweep ${s.minRms.toExponential(3)} (gate: > 0)`);
  }
  // MUST-FAIL CONTROL: plant the unguarded read back. Audit's pre-fix numbers:
  // 2/136 at 48 kHz, 46/136 at 96 kHz, wet RMS exactly 0.
  const P = planted(PLANT_R1);
  const p96 = sweep(P, 96000);
  ok = ok && p96.bad > 0 && p96.minRms === 0;
  lines.push(`  control (pre-fix read planted, 96 kHz): ${p96.bad}/136 wipe, worst ${p96.worst}/s, ` +
             `min wet RMS ${p96.minRms.toExponential(3)}  (must FAIL: audit measured 46/136, RMS exactly 0)`);
  return { ok, lines };
});

// V7 — how far into the denormal band the state falls. REPORTS the reach and
// gates only on finiteness: whether to flush to zero is a C++-side decision for
// the port (audit R8), not something to legislate here.
check('V7', 'denormal reach of the tail state at 60 s (report + finiteness)', () => {
  const DEN = 2.2250738585072014e-308, sr = 44100, lines = []; let ok = true;
  // CONTROL FIRST: the band test must separate a denormal from a normal.
  const isDen = v => v !== 0 && Math.abs(v) < DEN;
  if (!isDen(5e-320) || isDen(1.0)) ok = false;
  lines.push(`  control: 5e-320 denormal? ${isDen(5e-320)}   1.0 denormal? ${isDen(1.0)}  (gate: true / false)`);
  for (const decay of [0.5, 2.2]) {
    const v = mk(SwarmVerb, sr, { mix: 1, decay });
    ir(v, sr, 60, 256);
    let mx = 0, allFinite = true, den = 0, total = 0;
    const states = [...v.line, v.lp, v.hp];
    for (const arr of states) for (const s of arr) {
      total++; if (!Number.isFinite(s)) allFinite = false;
      if (isDen(s)) den++;
      mx = Math.max(mx, Math.abs(s));
    }
    ok = ok && allFinite;
    lines.push(`  decay ${decay}s: max|state| at 60 s ${mx.toExponential(3)}  denormal words ${den}/${total}  ` +
               `all finite ${allFinite}  (gate: finite; audit measured 1.98e-323 at decay 0.5)`);
  }
  return { ok, lines };
});

// V8 — modulation depth in CENTS (the audible quantity), across rates.
// Measured from the audio: freeze the modulation, walk line 0's phase, and fit
// the first-arrival sine. Never re-derives the lab's own depth constant — a
// detector that shares the assumption cannot test it.
check('V8', 'modulation wobble in cents, 44.1 / 48 / 96 kHz (R4)', () => {
  const lines = []; let ok = true;
  const depthSamples = (sr, depth) => {
    const cfg = { mix: 1, erSend: 0, preDelay: 0, spinDepth: 0, modRate: 0, modK: 0, modDepth: depth };
    const nph = 32; let num = 0, den = 0;
    for (let k = 0; k < nph; k++) {
      const ph = k / nph, v = mk(SwarmVerb, sr, cfg); v.modPh[0] = ph;
      const s = Math.sin(2 * Math.PI * ph);
      num += firstNonZero(ir(v, sr, 0.12, 256).L) * s; den += s * s;
    }
    return num / den;
  };
  // line 0's effective rate is modRate * (0.7 + 0.6 * 0/NLINE) = 0.7 * modRate
  const cents = (A, modRate, sr) => 1200 * Math.log2(1 / (1 - A * 2 * Math.PI * (0.7 * modRate) / sr));
  for (const [depth, rate] of [[0.35, 0.35], [0.35, 3], [1.0, 3]]) {
    const row = [44100, 48000, 96000].map(sr => {
      const A = depthSamples(sr, depth); return { sr, A, c: cents(A, rate, sr) };
    });
    const cs = row.map(r => r.c);
    const spread = (Math.max(...cs) - Math.min(...cs)) / cs[0] * 100;
    ok = ok && spread <= 1.5;
    lines.push(`  depth ${depth.toFixed(2)} rate ${String(rate).padEnd(4)} ` +
      row.map(r => `${r.A.toFixed(2)}smp/${r.c.toFixed(3)}c`).join('  ') +
      `  spread ${spread.toFixed(2)}%  (gate: <= 1.5 %; audit pre-fix 54 %)`);
  }
  // CONTROL (must read zero): no modulation, no wobble.
  const z = depthSamples(44100, 0);
  ok = ok && Math.abs(z) < 0.05;
  lines.push(`  control depth 0 -> ${z.toFixed(4)} samples of wobble  (gate: |A| < 0.05)`);
  return { ok, lines };
});

// -------------------------------------------------------------------- main --
// Self-check of the planting mechanism itself (L0032): a bogus anchor MUST
// throw, or every must-fail control silently measures the unmutated lab.
try {
  planted([['this_string_is_not_in_the_lab', 'x']]);
  console.log('RED — the anchor mechanism did not throw on a bogus anchor; no control below is trustworthy.');
  process.exit(1);
} catch (e) {
  if (!/ANCHOR MISSING/.test(String(e.message))) throw e;
  console.log(`anchor mechanism verified (bogus anchor throws)\nlab: docs/design/reverb-lab.html\n`);
}

const want = process.argv.slice(2).map(s => s.toUpperCase());
const run = CHECKS.filter(c => !want.length || want.includes(c.id));
let failed = 0;
for (const c of run) {
  const t = Date.now();
  let r;
  try { r = c.fn(); }
  catch (e) { r = { ok: false, lines: [`  THREW: ${e && e.message}`] }; }
  if (!r.ok) failed++;
  console.log(`${r.ok ? 'PASS' : 'FAIL'}  ${c.id}  ${c.title}   [${((Date.now() - t) / 1000).toFixed(1)}s]`);
  for (const l of r.lines) console.log(l);
  console.log('');
}
console.log(`${failed ? 'RED' : 'GREEN'} — ${run.length} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
