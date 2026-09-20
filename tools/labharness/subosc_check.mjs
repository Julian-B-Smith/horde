/*
 * subosc_check.mjs — fidelity harness for reference/subosc.html (B155).
 *
 * WHY THIS EXISTS. The SAW engine's lab-inherited expedients were only found by
 * an audit, years after they shipped: a hand-tuned per-tick smoother that drifts
 * 54.9 % across sample rates, an uncapped voice count that segfaults, an
 * unseeded RNG that makes a bounce depend on session history
 * (docs/audits/2026-09-18-saw-engine-audit.md §1). Every one of those is a
 * property a cheap headless check would have caught on day one, and none of
 * them is visible to a parity oracle, because the reference SHARES them
 * (LIBRARY L0031). So this module gets its invariant suite BEFORE it gets a
 * port, not after.
 *
 * SEVEN PROPERTY GROUPS, EACH WITH A MUST-FAIL CONTROL. A detector that shares an
 * assumption with what it measures confirms whatever you expect (LIBRARY
 * L0032), so no row here is trusted unless a deliberate corruption of the core
 * makes it read differently. The corruptions are string plants into the lab's
 * own DSP source, and every plant ASSERTS ITS ANCHOR — a plant that silently
 * fails to apply is the exact failure mode the lesson names.
 *
 * WIRED into `./verify full` (ADR-180 §1: a check is wired in the PR that
 * creates it unless its header states why not — this one's stated reason had
 * been "UNWIRED: reason not stated", which is not a reason). It runs at the top
 * of full() beside station_check.mjs, and it is also the HOME of §10.1's
 * aliasing floors: those need the 65 536-point Kaiser-windowed FFT sweep that
 * exists here and not on the C++ side, which is why tools/subosc_check.cpp's
 * header points at this file for them rather than approximating them. 0.6 s.
 * By hand:
 *     node tools/labharness/subosc_check.mjs
 * Exit 1 if any property fails. Thresholds below are MEASURED on this build at
 * the date in the header of each section, with the margin stated inline; they
 * are re-measured, never relaxed, if the core changes (ADR-009 house rule for
 * ACCEPTANCE numbers).
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { extractCore } from '../golden/extract_core.mjs';

const here = dirname(fileURLToPath(import.meta.url));
const LAB = resolve(here, '../../reference/subosc.html');

// The pristine core comes through the SHIPPED extractor, so a banner drift that
// would break golden generation breaks this harness first and loudly.
const SubOscCore = extractCore(LAB, 'SubOscCore');

// The mutants need the source TEXT, which extract_core does not hand back. The
// slice is repeated here rather than exported, deliberately: extract_core is a
// golden-path tool and widening its surface for a hand-run harness is the wrong
// trade. If the banners ever move, the line above throws before this runs.
function dspSource() {
  const html = readFileSync(LAB, 'utf8');
  const start = html.search(/\/\* =+ DSP:/);
  const end = html.search(/\/\* =+ Audio graph/);
  if (start < 0 || end < 0 || end <= start) throw new Error('subosc_check: DSP banners not found');
  return html.slice(start, end);
}

// The DSP source evaluated, optionally with a corruption whose anchor is
// ASSERTED: `from` must occur exactly once — a plant that does not apply
// produces a "clean" reading that looks like a pass (LIBRARY L0032/L0033).
// Hands back the free functions as well as the class, because §7's peak
// property tests the SHAPE (subBump) against the normaliser the core computed
// for it, and extract_core only returns the class.
function dspExports(from, to) {
  let src = dspSource();
  if (from !== undefined) {
    const parts = src.split(from);
    if (parts.length !== 2) {
      throw new Error(`subosc_check: plant anchor not unique — ${JSON.stringify(from)} occurs ${parts.length - 1}x`);
    }
    src = parts.join(to);
  }
  return new Function('"use strict";\n' + src + '\nreturn { SubOscCore, subBump };')();
}
const mutantCore = (from, to) => dspExports(from, to).SubOscCore;

const mtof = m => 440 * Math.pow(2, (m - 69) / 12);
const WAVE = { sine: 0, triangle: 1, square: 2, saw: 3, pulse: 4, noise: 5, bump: 6 };
const F32_MIN_NORMAL = 1.1754943508222875e-38;

let failures = 0;
const results = [];
function judge(name, ok, detail) {
  if (!ok) failures++;
  results.push((ok ? 'PASS  ' : 'FAIL  ') + name.padEnd(56) + detail);
  console.log((ok ? 'PASS  ' : 'FAIL  ') + name.padEnd(56) + detail);
}
const head = t => console.log('\n=== ' + t + ' ===');

// One trial = one FRESH core. No state can cross between trials.
function make(Core, sr, patch) {
  const c = new Core(sr);
  for (const k in patch) c.setParam(k, patch[k]);
  return c;
}
function renderInto(core, n, master) {
  const L = new Float32Array(n), R = new Float32Array(n);
  core.render(L, R, n, master);
  return L;
}

/* ---------------------------------------------------------------------------
 * FFT + window. Plain iterative radix-2; N is a power of two by construction.
 * ------------------------------------------------------------------------ */
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len;
    const wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const ur = re[i + k], ui = im[i + k];
        const vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        const vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        const nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
      }
    }
  }
}
function besselI0(x) {
  let s = 1, t = 1;
  for (let k = 1; k < 60; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-18 * s) break; }
  return s;
}
// Kaiser beta=19: sidelobes far below any alias this module can produce, so the
// residue measured between the harmonics is the signal's own, not the window's
// leakage — the "measure the defect where the signal structurally ISN'T"
// discipline of LIBRARY L0016.
function kaiser(n, beta) {
  const w = new Float64Array(n), d = besselI0(beta);
  for (let i = 0; i < n; i++) {
    const r = 2 * i / (n - 1) - 1;
    w[i] = besselI0(beta * Math.sqrt(Math.max(0, 1 - r * r))) / d;
  }
  return w;
}

const NFFT = 65536, WARM = 4096, KWIN = kaiser(NFFT, 19);

/* Alias floor: the largest spectral magnitude OUTSIDE the harmonic series,
   relative to the fundamental, in dB. Bins within EX of any k*f0 are excluded
   (that is where the signal legitimately is); everything left is either an
   alias or the numerical floor. */
function aliasFloorDb(Core, sr, wave, midi, extra) {
  const c = make(Core, sr, Object.assign({
    wave, octave: 0, keytrack: 1, level: 1, tone: 20000,
    attack: 0.0005, release: 0.01, phase: 0, width: 0.3,
  }, extra || {}));
  c.noteOn(midi, 1);
  renderInto(c, WARM);
  const sig = renderInto(c, NFFT);
  const re = new Float64Array(NFFT), im = new Float64Array(NFFT);
  for (let i = 0; i < NFFT; i++) re[i] = sig[i] * KWIN[i];
  fft(re, im);
  const f0 = c.freqHz(), binHz = sr / NFFT, EX = 12;
  const half = NFFT / 2;
  const mag = new Float64Array(half);
  for (let i = 0; i < half; i++) mag[i] = Math.hypot(re[i], im[i]);
  // fundamental magnitude = the peak in the bins around f0
  const b0 = Math.round(f0 / binHz);
  let ref = 0;
  for (let i = Math.max(1, b0 - EX); i <= b0 + EX; i++) ref = Math.max(ref, mag[i]);
  let worst = 0, resE = 0, totE = 0, worstHz = 0;
  for (let i = 1; i < half; i++) {
    const f = i * binHz;
    totE += mag[i] * mag[i];
    if (f < 20) continue;                       // DC skirt: not a harmonic, not an alias
    const k = f / f0;
    if (Math.abs(k - Math.round(k)) * f0 < EX * binHz) continue;   // a harmonic lives here
    resE += mag[i] * mag[i];
    if (mag[i] > worst) { worst = mag[i]; worstHz = f; }
  }
  const db = v => (v > 0 ? 20 * Math.log10(v / ref) : -999);
  return { worst: db(worst), worstHz, integral: 10 * Math.log10(resE / totE), f0 };
}

/* ===========================================================================
 * 1. ALIASING FLOOR across the sub range (MIDI 24/36/48/60)
 * ======================================================================== */
head('1. aliasing floor, 44.1 kHz, octave 0, tone wide open — worst inharmonic bin, dB below the fundamental');
/* MEASURED 2026-09-18 on this build, PER ROW. One threshold per waveform would
   be set by the top note and give the bottom of the range no coverage at all —
   the floor moves 18 dB from MIDI 24 to MIDI 60 — which is why audit §3.1 asks
   for per-row floors. Each number below is the measurement rounded UP by ~5 dB;
   the BLEP-disabled control at the same row sits 5-7 dB ABOVE the threshold, so
   the band is narrow on both sides: this is a REGRESSION gate, not a headroom
   claim. The tone filter is in the path (it is part of the module's output), so
   these are the module's floors, not a bare oscillator's. */
const ALIAS_MAX_DB = {
  //          MIDI 24  36    48    60      measured worst (dB)
  sine:     [-155, -155, -155, -155],  // -167.2 -164.1 -166.7 -164.4
  triangle: [-110,  -99,  -87,  -76],  // -116.9 -105.0  -92.8  -81.0
  square:   [ -64,  -58,  -52,  -46],  //  -69.6  -63.9  -57.6  -51.9
  saw:      [ -64,  -58,  -52,  -46],  //  -69.3  -63.9  -57.6  -51.9
  pulse:    [ -62,  -57,  -51,  -45],  //  -67.9  -62.4  -56.3  -50.3
  // BUMP (B155, measured 2026-09-19). The gate is NOT the ~5 dB regression band
  // the BLEP rows use, because there is no aliasing mechanism to regress from:
  // the shape is two partials, so the only thing in the inharmonic bins is the
  // detector's own skirt. It is held to the SINE limit — the additive floor —
  // and its must-fail control is the naive fold below, not the no-BLEP plant
  // (which cannot reach it, the same coverage boundary sine and triangle have).
  bump:     [-155, -155, -155, -155],  // -166.8 -163.6 -166.5 -164.4 (fold control: -102.8 .. -67.2)
};
const NOTES = [24, 36, 48, 60];
// The control: a core whose polyBLEP has been neutered. Anchor asserted inside
// mutantCore — a plant that silently fails to apply reads as a clean pass.
const NoBlep = mutantCore('function subBlep(ph, dph) {', 'function subBlep(ph, dph) {\n  return 0;');
const aliasWorst = {};
for (const w of ['sine', 'triangle', 'square', 'saw', 'pulse', 'bump']) {
  const blepped = w === 'square' || w === 'saw' || w === 'pulse';
  let over = '', ctlFail = '', worst = -999;
  for (let i = 0; i < NOTES.length; i++) {
    const m = NOTES[i], lim = ALIAS_MAX_DB[w][i];
    const r = aliasFloorDb(SubOscCore, 44100, WAVE[w], m);
    const ctl = aliasFloorDb(NoBlep, 44100, WAVE[w], m);
    console.log(`      ${w.padEnd(9)} MIDI ${String(m).padEnd(3)} f0 ${r.f0.toFixed(2).padStart(7)} Hz   ` +
                `worst ${r.worst.toFixed(1).padStart(7)} dB @ ${r.worstHz.toFixed(0).padStart(6)} Hz   ` +
                `integral ${r.integral.toFixed(1).padStart(6)} dB   ` +
                `limit ${String(lim).padStart(5)}   no-BLEP control ${ctl.worst.toFixed(1).padStart(7)} dB`);
    if (r.worst > lim) over += ` MIDI ${m}: ${r.worst.toFixed(1)} > ${lim};`;
    if (blepped && ctl.worst <= lim) ctlFail += ` MIDI ${m}: ${ctl.worst.toFixed(1)} <= ${lim};`;
    if (r.worst > worst) worst = r.worst;
  }
  aliasWorst[w] = worst;
  judge(`alias floor ${w}, all four notes under their limits`, over === '', over || 'all rows pass');
  if (blepped) {
    judge(`CONTROL no-BLEP ${w} must BREACH every limit`, ctlFail === '', ctlFail || 'control breaches all four rows');
  }
}
// CONTROL, must read ~zero: the sine row is a band-limited signal with no
// aliasing mechanism at all, so it reports the DETECTOR's own floor — that
// -164 dB is the Kaiser skirt around the fundamental, not the signal's alias.
// If this row ever read large, every row above would be measuring the window.
judge('CONTROL sine must read below -150 dB (detector reads ~zero on clean)',
      aliasWorst.sine < -150, `${aliasWorst.sine.toFixed(1)} dB`);
/* COVERAGE BOUNDARY, recorded rather than retried (LIBRARY L0033). The no-BLEP
   plant does NOT move the sine or triangle rows AT ALL (separation 0.0 dB at
   every note): neither shape goes through subBlep, because the triangle is
   rendered NAIVE — no BLEP, no BLAMP. Its -81.0 dB floor at MIDI 60 is the
   honest price of that, and it is 30 dB worse than the band-limited shapes.
   The spec draft names it as a v0 limit with the fix (BLAMP) priced. */
console.log('      NOTE: the no-BLEP plant cannot reach sine or triangle (separation 0.0 dB) —');
console.log('            the triangle is a NAIVE shape; that is a coverage boundary, not a pass.');

/* BUMP'S OWN MUST-FAIL CONTROL (B155). The bump row above claims the ADDITIVE
   floor, and the danger is that the claim is vacuous: the sine row proves the
   DETECTOR reads ~zero on a clean signal, but not that it would read LARGE on a
   shape that merely LOOKS like this one. So the control is the plausible wrong
   way to build a double bump — a naive wavefolder, a sine overdriven past unity
   and reflected at +-1, which produces the same two-humped silhouette from a
   NONLINEARITY instead of from two partials. If the floor did not move, the
   bump row would be measuring the window and not the construction. */
const NaiveFold = mutantCore(
  'function subBump(ph, a, phi, norm) {',
  'function subBump(ph, a, phi, norm) {\n' +
  '  let s = Math.sin(SUB_TWO_PI * ph) * (1 + 2 * a);\n' +
  '  if (s > 1) s = 2 - s; else if (s < -1) s = -2 - s;\n' +
  '  return s * norm;');
{
  let ctlFail = '', worstCtl = -999;
  for (let i = 0; i < NOTES.length; i++) {
    const m = NOTES[i], lim = ALIAS_MAX_DB.bump[i];
    const ctl = aliasFloorDb(NaiveFold, 44100, WAVE.bump, m);
    console.log(`      bump/FOLD MIDI ${String(m).padEnd(3)} worst ${ctl.worst.toFixed(1).padStart(7)} dB @ ${ctl.worstHz.toFixed(0).padStart(6)} Hz   limit ${String(lim).padStart(5)}`);
    if (ctl.worst <= lim) ctlFail += ` MIDI ${m}: ${ctl.worst.toFixed(1)} <= ${lim};`;
    if (ctl.worst > worstCtl) worstCtl = ctl.worst;
  }
  judge('CONTROL naive-fold bump must BREACH every limit', ctlFail === '',
        ctlFail || `worst ${worstCtl.toFixed(1)} dB vs limit -155`);
}

/* NAMED LIMIT L6, PINNED RATHER THAN WRITTEN DOWN (LIBRARY L0036). "Band-limited
   by construction" is true only while 3.f0 < Nyquist; above f0 = sr/6 (7350 Hz
   at 44.1 k) the third partial folds and the guarantee lapses. A deliberate
   absence needs a test or it becomes an accidental presence, so the boundary is
   asserted in BOTH directions: clean just below it, aliasing just above. */
{
  const below = aliasFloorDb(SubOscCore, 44100, WAVE.bump, 100);   // f0 2637 Hz, 3f0 7911 Hz
  const above = aliasFloorDb(SubOscCore, 44100, WAVE.bump, 120);   // f0 8372 Hz, 3f0 25116 Hz -> folds
  console.log(`      bump boundary: MIDI 100 f0 ${below.f0.toFixed(0)} Hz -> ${below.worst.toFixed(1)} dB   ` +
              `MIDI 120 f0 ${above.f0.toFixed(0)} Hz (3f0 ${(3 * above.f0).toFixed(0)} Hz > Nyquist) -> ${above.worst.toFixed(1)} dB @ ${above.worstHz.toFixed(0)} Hz`);
  judge('bump still at the additive floor at MIDI 100 (3.f0 under Nyquist)',
        below.worst < -150, `${below.worst.toFixed(1)} dB`);
  judge('CONTROL L6: above f0 = sr/6 the third partial MUST fold', above.worst > -60,
        `${above.worst.toFixed(1)} dB @ ${above.worstHz.toFixed(0)} Hz`);
}

/* ===========================================================================
 * 2. DETERMINISM — same seed, bit-identical; different seed, different
 * ======================================================================== */
head('2. determinism (the seed is the control, and it reaches the stream — audit A3 is the counter-example)');
function noiseRun(seed, sr = 44100) {
  const c = make(SubOscCore, sr, { wave: WAVE.noise, level: 1, tone: 20000, seed, attack: 0.001 });
  c.noteOn(48, 1);
  const a = renderInto(c, 8192);
  c.noteOff();
  const b = renderInto(c, 8192);
  const out = new Float32Array(16384);
  out.set(a, 0); out.set(b, 8192);
  return out;
}
const bitEq = (a, b) => { for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return i; return -1; };
{
  const a = noiseRun(12345), b = noiseRun(12345);
  const i = bitEq(a, b);
  judge('two instances, same seed -> bit-identical', i === -1, i === -1 ? '16384 samples equal' : `first diff at ${i}`);
  /* ALLOFF IS A TOTAL RESET — the A3 property stated exactly. The SAW engine's
     failure is not that history matters; it is that a stream (tOff/tRng)
     survives every reset there is, so no allOff, no state restore and no new
     note can put the core back in a known condition. Here: five notes of
     history, then allOff(), must reproduce a fresh core bit-for-bit.
     What is NOT claimed: that history is irrelevant WITHOUT allOff. The
     envelope and the filter state deliberately carry across a retrigger (that
     is what makes a legato retrigger click-free), and the control below shows
     the comparator sees exactly that. */
  const hrun = (reset) => {
    const c = make(SubOscCore, 44100, { wave: WAVE.noise, level: 1, seed: 12345, attack: 0.001 });
    for (const m of [40, 41, 42, 43, 44]) { c.noteOn(m, 1); renderInto(c, 2000); c.noteOff(); renderInto(c, 2000); }
    if (reset) c.allOff();
    c.noteOn(48, 1);
    const h1 = renderInto(c, 8192);
    c.noteOff();
    const h2 = renderInto(c, 8192);
    const out = new Float32Array(16384); out.set(h1, 0); out.set(h2, 8192);
    return out;
  };
  const j = bitEq(a, hrun(true));
  judge('FIVE notes of history + allOff() -> bit-identical to fresh', j === -1,
        j === -1 ? 'no stream survives allOff()' : `first diff at ${j}`);
  // CONTROL (must differ): without the reset, the envelope and filter state DO
  // carry over. If this read "identical" the row above would be vacuous.
  const k = bitEq(a, hrun(false));
  judge('CONTROL history WITHOUT allOff must differ (env/filter carry, by design)',
        k !== -1, `first diff at ${k}`);
  // CONTROL (must differ): if this read "identical" the comparator is blind and
  // both rows above are worthless.
  const d = noiseRun(999999);
  judge('CONTROL different seed must differ', bitEq(a, d) !== -1, `first diff at ${bitEq(a, d)}`);
}

/* BUMP determinism (B155). The shape draws on no stream at all, so the claim
   here is narrower and worth stating precisely: the output is a pure function
   of (parameters, note), and allOff() is still a TOTAL reset for it — the
   filter and envelope are shared state and a shape without an RNG can still
   inherit them. The must-differ control is the phase parameter, not a seed. */
{
  const bumpRun = (patch, history) => {
    const c = make(SubOscCore, 44100, Object.assign(
      { wave: WAVE.bump, level: 1, tone: 20000, attack: 0.001, release: 0.01 }, patch));
    if (history) {
      for (const m of [40, 41, 42, 43, 44]) { c.noteOn(m, 1); renderInto(c, 2000); c.noteOff(); renderInto(c, 2000); }
      c.allOff();
    }
    c.noteOn(36, 1);
    const h1 = renderInto(c, 8192);
    c.noteOff();
    const h2 = renderInto(c, 8192);
    const out = new Float32Array(16384); out.set(h1, 0); out.set(h2, 8192);
    return out;
  };
  const a = bumpRun({}, false), b = bumpRun({}, false);
  judge('bump: two instances, same params -> bit-identical', bitEq(a, b) === -1,
        bitEq(a, b) === -1 ? '16384 samples equal' : `first diff at ${bitEq(a, b)}`);
  const h = bitEq(a, bumpRun({}, true));
  judge('bump: five notes of history + allOff() -> bit-identical to fresh', h === -1,
        h === -1 ? 'no state survives allOff()' : `first diff at ${h}`);
  // CONTROL (must differ): if a phase shift of 0.01 rad did not change a single
  // sample, the two rows above would be comparing something other than the shape.
  const p = bitEq(a, bumpRun({ bumpPhase: -0.24 }, false));
  judge('CONTROL bumpPhase -0.25 vs -0.24 must differ', p !== -1, `first diff at ${p}`);
}

/* ===========================================================================
 * 3. SAMPLE-RATE INDEPENDENCE — behaviour expressed in SECONDS (ADR-009)
 * ======================================================================== */
head('3. sample-rate independence, 44.1 / 48 / 96 kHz');
const RATES = [44100, 48000, 96000];
const spread = xs => (Math.max(...xs) - Math.min(...xs)) / (xs.reduce((a, b) => a + b, 0) / xs.length) * 100;

// (a) envelope attack: time to env = 0.9, rendered one sample at a time so the
// observable is read at sample resolution, crossing interpolated (the
// samplerate_check discipline).
function attackSeconds(Core, sr) {
  const c = make(Core, sr, { attack: 0.05, level: 1, wave: WAVE.sine });
  c.noteOn(60, 1);
  const L = new Float32Array(1), R = new Float32Array(1);
  let prev = 0;
  for (let i = 0; i < Math.ceil(0.2 * sr); i++) {
    c.render(L, R, 1);
    if (c.env >= 0.9) {
      const frac = (0.9 - prev) / (c.env - prev);
      return (i + frac) / sr;
    }
    prev = c.env;
  }
  return NaN;
}
{
  const t = RATES.map(sr => attackSeconds(SubOscCore, sr));
  RATES.forEach((sr, i) => console.log(`      attack to 0.9 @ ${sr} = ${(t[i] * 1000).toFixed(4)} ms`));
  const d = spread(t);
  judge('attack time drift <= 0.5 %', d <= 0.5, `${d.toFixed(4)} % (44.1k = ${(t[0] * 1000).toFixed(3)} ms)`);
  // CONTROL: the ADR-009 trap itself — a hand-tuned per-sample increment in
  // place of the seconds-derived one. If this does NOT drift, the measurement
  // is not looking at the envelope.
  const PerSample = mutantCore('this._atkInc = 1 / Math.max(1, p.attack * this.sr);', 'this._atkInc = 0.0005;');
  const tc = RATES.map(sr => attackSeconds(PerSample, sr));
  const dc = spread(tc);
  judge('CONTROL per-sample-constant attack must drift > 10 %', dc > 10,
        `${dc.toFixed(2)} % (${tc.map(x => (x * 1000).toFixed(1)).join(' / ')} ms)`);
}

// (b) tone time constant, in SECONDS, measured from the decay RATIO at two
// points well past the edge. A ratio of two late samples cancels the one-sample
// BLEP smear at the edge, which is itself rate-dependent and would otherwise be
// the thing measured.
function toneTauSeconds(Core, sr) {
  const c = make(Core, sr, {
    wave: WAVE.square, keytrack: 0, octave: -2, tone: 200, level: 1,
    attack: 0.0005, phase: 0,
  });
  c.noteOn(60, 1);
  const n = Math.ceil(0.06 * sr);
  const y = renderInto(c, n);
  const f0 = c.freqHz();
  const edge = Math.round(0.5 / f0 * sr);          // the square's falling edge
  const settled = y[edge + Math.round(0.020 * sr)]; // asymptote, 20 ms later
  const tau0 = 1 / (2 * Math.PI * 200);
  const i1 = edge + Math.round(2 * tau0 * sr), i2 = edge + Math.round(6 * tau0 * sr);
  const d1 = Math.abs(y[i1] - settled), d2 = Math.abs(y[i2] - settled);
  return ((i2 - i1) / sr) / Math.log(d1 / d2);
}
{
  const t = RATES.map(sr => toneTauSeconds(SubOscCore, sr));
  RATES.forEach((sr, i) => console.log(`      tone tau (fc 200 Hz) @ ${sr} = ${(t[i] * 1e6).toFixed(2)} us   (ideal 1/2pi.fc = ${(1e6 / (2 * Math.PI * 200)).toFixed(2)} us)`));
  const d = spread(t);
  judge('tone time constant drift <= 0.5 %', d <= 0.5, `${d.toFixed(4)} % (44.1k = ${(t[0] * 1e6).toFixed(2)} us)`);
}

// (c) the audit's actual A7 finding: does the tone's MAGNITUDE response move
// with the rate? A time constant can be right while the response warps.
function toneMagRatio(Core, sr) {
  const rms = fc => {
    // octave 0 EXPLICITLY: the default is -1, and the closed-form anchor below
    // is written in terms of mtof(83). The first run of this check disagreed
    // with the anchor by 57.7 % for exactly that reason — the anchor caught the
    // harness's own assumption, which is the entire point of having one.
    const c = make(Core, sr, { wave: WAVE.sine, tone: fc, level: 1, attack: 0.0005, keytrack: 1, octave: 0 });
    c.noteOn(83, 1);                       // 987.77 Hz, the same note at every rate
    renderInto(c, Math.round(0.05 * sr));
    const y = renderInto(c, Math.round(0.2 * sr));
    let s = 0;
    for (let i = 0; i < y.length; i++) s += y[i] * y[i];
    return Math.sqrt(s / y.length);
  };
  return rms(500) / rms(20000);
}
{
  const r = RATES.map(sr => toneMagRatio(SubOscCore, sr));
  // Closed-form anchor: the TPT one-pole's magnitude is g / sqrt(g^2 + tan^2),
  // an oracle that needs no reference implementation (LIBRARY L0031).
  const closed = RATES.map(sr => {
    const g = Math.tan(Math.PI * 500 / sr), t = Math.tan(Math.PI * mtof(83) / sr);
    return g / Math.sqrt(g * g + t * t);
  });
  RATES.forEach((sr, i) => console.log(`      |H(987.8 Hz)| fc 500 @ ${sr} = ${r[i].toFixed(6)}   closed form ${closed[i].toFixed(6)}`));
  const d = spread(r);
  judge('tone magnitude drift <= 0.5 %', d <= 0.5, `${d.toFixed(4)} %`);
  const err = Math.max(...r.map((v, i) => Math.abs(v - closed[i]) / closed[i] * 100));
  judge('tone magnitude matches closed form within 1 %', err < 1, `worst ${err.toFixed(4)} %`);
}

/* ===========================================================================
 * 4. BLOCK-SIZE INDEPENDENCE — nothing in render is a per-call integrator
 * ======================================================================== */
head('4. block-size independence, chunks 1 / 7 / 64 / 256 / 333 against one whole-buffer render');
const CHUNKS = [1, 7, 64, 256, 333];
const TOTAL = 20000;
function chunked(Core, patch, chunk, masterAll) {
  const c = make(Core, 44100, patch);
  c.noteOn(45, 1);
  const out = new Float32Array(TOTAL);
  const L = new Float32Array(chunk), R = new Float32Array(chunk);
  const m = masterAll ? new Float32Array(chunk) : null;
  let i = 0;
  while (i < TOTAL) {
    const n = Math.min(chunk, TOTAL - i);
    if (m) for (let k = 0; k < n; k++) m[k] = masterAll[i + k];
    c.render(L, R, n, m);
    for (let k = 0; k < n; k++) out[i + k] = L[k];
    i += n;
  }
  return out;
}
// A master phase the lab would supply: a plain saw, precomputed so every
// chunking sees identical input.
const masterAll = new Float32Array(TOTAL);
{ let ph = 0; for (let i = 0; i < TOTAL; i++) { ph += 220 / 44100; ph -= Math.floor(ph); masterAll[i] = ph; } }
const LEGS = {
  'pulse + tone + hard sync': [{ wave: WAVE.pulse, width: 0.27, tone: 900, level: 0.9, sync: 1, attack: 0.004, release: 0.05 }, masterAll],
  'seeded noise + tone': [{ wave: WAVE.noise, tone: 1500, level: 0.9, seed: 7, attack: 0.002 }, null],
};
for (const name in LEGS) {
  const [patch, master] = LEGS[name];
  const ref = chunked(SubOscCore, patch, TOTAL, master);
  let bad = '';
  for (const ch of CHUNKS) {
    const i = bitEq(ref, chunked(SubOscCore, patch, ch, master));
    if (i !== -1) bad += ` chunk ${ch} differs at ${i};`;
  }
  judge(`block-size independent: ${name}`, bad === '', bad || 'all chunkings bit-identical');
}
{
  // CONTROL: a per-render-call state reset — the defect class the SAW engine's
  // pan motion still has (audit A10). Must be visible to this comparator.
  const PerCall = mutantCore('render(outL, outR, n, master) {', 'render(outL, outR, n, master) {\n    this._z = 0;');
  const [patch, master] = LEGS['pulse + tone + hard sync'];
  const ref = chunked(PerCall, patch, TOTAL, master);
  const i = bitEq(ref, chunked(PerCall, patch, 64, master));
  judge('CONTROL per-call filter reset must break block independence', i !== -1, `first diff at ${i}`);
}

/* ===========================================================================
 * 5. SILENCE AT LEVEL 0 IS EXACT
 * ======================================================================== */
head('5. level 0 is silence, exactly — not -140 dB of something');
{
  let bad = '';
  for (const w in WAVE) {
    const c = make(SubOscCore, 44100, { wave: WAVE[w], level: 0, tone: 900, attack: 0.001 });
    c.noteOn(36, 1);
    const y = renderInto(c, 8192);
    for (let i = 0; i < y.length; i++) if (y[i] !== 0) { bad += ` ${w}@${i}=${y[i]};`; break; }
  }
  // The count comes from the WAVE map, not from a literal: a seventh shape was
  // added (bump, B155) and a hardcoded "six" would have made this row lie about
  // its own coverage while still passing.
  const nWaves = Object.keys(WAVE).length;
  judge(`all ${nWaves} waveforms, level 0 -> every sample exactly 0`, bad === '', bad || `${nWaves} waveforms x 8192 samples`);
  // CONTROL: the same test at a level a thousand times smaller than unity must
  // FAIL to be silent, or the assertion is vacuous.
  const c = make(SubOscCore, 44100, { wave: WAVE.saw, level: 0.001, tone: 900, attack: 0.001 });
  c.noteOn(36, 1);
  const y = renderInto(c, 8192);
  let nz = 0;
  for (let i = 0; i < y.length; i++) if (y[i] !== 0) nz++;
  judge('CONTROL level 0.001 must NOT be silent', nz > 0, `${nz} non-zero samples`);
}

/* ===========================================================================
 * 6. DENORMAL-FREE TAIL
 * ======================================================================== */
head('6. the release tail never reaches float32 subnormals');
function subnormalCount(Core) {
  const c = make(Core, 44100, { wave: WAVE.saw, level: 1, tone: 200, attack: 0.001, release: 0.01 });
  c.noteOn(36, 1);
  renderInto(c, Math.round(0.2 * 44100));
  c.noteOff();
  const y = renderInto(c, Math.round(2.0 * 44100));
  let n = 0, last = 0;
  for (let i = 0; i < y.length; i++) {
    const a = Math.abs(y[i]);
    if (a !== 0 && a < F32_MIN_NORMAL) n++;
    if (a !== 0) last = i;
  }
  return { n, lastNonZero: last, len: y.length };
}
{
  const r = subnormalCount(SubOscCore);
  judge('2.0 s of tail contains 0 subnormal samples', r.n === 0,
        `${r.n} subnormals; last non-zero sample at ${r.lastNonZero} of ${r.len}`);
  // CONTROL: remove the flush-to-zero and the tail decays straight through the
  // subnormal range. Honest limit: this counts subnormals in the OUTPUT, which
  // is the contract the host sees — it is not evidence about CPU stalls, which
  // this machine cannot show either (audit §3.7 records the same limit).
  const NoFlush = mutantCore('const SUB_FLUSH = 1e-20;', 'const SUB_FLUSH = 0;');
  const rc = subnormalCount(NoFlush);
  judge('CONTROL flush-to-zero removed must produce subnormals', rc.n > 0, `${rc.n} subnormals`);
}

/* ===========================================================================
 * 7. THE BUMP SHAPE — two lobes, big one first, and a peak of exactly 1
 * ======================================================================== */
head('7. bump (B155): the two-lobe silhouette at the defaults, and a peak of exactly 1');

/* The lobes are read from a RISING ZERO CROSSING, not from wherever the render
   window happens to start. Without that anchor the "first" maximum is whichever
   one the buffer opened on, and the ordering claim — big bump FIRST — silently
   inverts at some notes. (It did, at MIDI 48, in the first draft of this
   measurement.) */
function bumpLobes(Core, sr, midi, patch) {
  const c = make(Core, sr, Object.assign({
    wave: WAVE.bump, octave: 0, keytrack: 1, level: 1, tone: 20000,
    attack: 0.0005, release: 0.01, phase: 0,
  }, patch || {}));
  c.noteOn(midi, 1);
  renderInto(c, 4096);                                  // past the attack ramp
  const f0 = c.freqHz(), per = sr / f0;
  const y = renderInto(c, Math.ceil(3 * per) + 4);
  let z = -1;
  for (let i = 1; i < y.length; i++) if (y[i - 1] <= 0 && y[i] > 0) { z = i; break; }
  const pk = [], tr = [];
  const end = Math.min(y.length - 1, z + Math.round(per));
  for (let i = z + 1; i < end; i++) {
    if (y[i] > 0 && y[i] > y[i - 1] && y[i] >= y[i + 1]) pk.push(y[i]);
    if (y[i] > 0 && y[i] < y[i - 1] && y[i] <= y[i + 1]) tr.push(y[i]);
  }
  let amax = 0;
  for (let i = 0; i < y.length; i++) amax = Math.max(amax, Math.abs(y[i]));
  return { f0, per, pk, tr, amax };
}

/* DERIVED 2026-09-19 by rendering one period of y = sin(th) + a.sin(3th+phi)
   over a grid of (a, phi) and measuring the two positive lobes
   (traces/2026-09-19-b155-subosc-bump.md). The defaults a = 0.35, phi = -0.25
   rad were chosen for "a big bump followed by a SLIGHTLY smaller bump": the
   trailing lobe is 0.8823 of the leading one (1.09 dB down — audible as a
   shape, not as two different events), with the trough between them at 0.637
   of the leading peak, which is what makes the pair read as two bumps rather
   than as one crest with a nick in it. phi's SIGN decides the order. */
const BUMP_RATIO = 0.8823, BUMP_TROUGH_FRAC = 0.6369, BUMP_TOL = 0.05;
{
  let bad = '';
  for (const m of [24, 36, 48, 60]) {
    const r = bumpLobes(SubOscCore, 44100, m);
    if (r.pk.length !== 2) { bad += ` MIDI ${m}: ${r.pk.length} positive lobes;`; continue; }
    const ratio = r.pk[1] / r.pk[0], tf = r.tr.length ? r.tr[0] / r.pk[0] : NaN;
    console.log(`      MIDI ${String(m).padEnd(3)} f0 ${r.f0.toFixed(2).padStart(7)} Hz  period ${r.per.toFixed(1).padStart(7)} smp  ` +
                `lobes ${r.pk[0].toFixed(5)} / ${r.pk[1].toFixed(5)}  ratio ${ratio.toFixed(5)}  ` +
                `trough/peak ${tf.toFixed(5)}  peak ${r.amax.toFixed(5)}`);
    if (Math.abs(ratio - BUMP_RATIO) / BUMP_RATIO > BUMP_TOL) bad += ` MIDI ${m}: ratio ${ratio.toFixed(4)};`;
    if (Math.abs(tf - BUMP_TROUGH_FRAC) / BUMP_TROUGH_FRAC > BUMP_TOL) bad += ` MIDI ${m}: trough ${tf.toFixed(4)};`;
  }
  judge(`bump lobe ratio = ${BUMP_RATIO} +-5 % at every note (big bump FIRST)`, bad === '',
        bad || 'four notes, ratio and trough both within 5 %');

  // CONTROL (must read ~1): at phi = 0 the two lobes are exactly equal by
  // symmetry. If this still read 0.88 the measurement would be returning a
  // constant and the row above would be vacuous.
  const sym = bumpLobes(SubOscCore, 44100, 36, { bumpPhase: 0 });
  const symRatio = sym.pk[1] / sym.pk[0];
  judge('CONTROL phi = 0 must give two EQUAL lobes (ratio 1.000 +-0.2 %)',
        Math.abs(symRatio - 1) < 0.002, `${symRatio.toFixed(5)}`);

  // CONTROL (must invert): phi = +0.25 is the mirror, so the SMALL bump leads.
  // This is what pins the human's ordering — "big bump followed by a slightly
  // smaller one" — to the sign of the default rather than to the indexing.
  const mir = bumpLobes(SubOscCore, 44100, 36, { bumpPhase: 0.25 });
  const mirRatio = mir.pk[1] / mir.pk[0];
  judge('CONTROL phi = +0.25 must INVERT the order (small bump leads)',
        Math.abs(mirRatio - 1 / BUMP_RATIO) / (1 / BUMP_RATIO) < BUMP_TOL, `${mirRatio.toFixed(5)}`);
}

// The normaliser line, named once because two plants below target it. A string
// plant that silently stops matching is caught by dspExports, not by a wrong
// reading (LIBRARY L0032).
const NORM_LINE = 'this._bumpNorm = 1 / subBumpPeak(p.bumpAmt, p.bumpPhase);';

/* One grid, two properties: the audio-path sweep below (does the RENDERED
   signal stay within 1?) and the shape-level peak check after it (does the
   shape reach exactly 1?). The extremes a = 0 / a = 0.6 and phi = +-pi are in
   it deliberately. */
const AS = [0, 0.1, 0.2, 0.3, 0.35, 0.4, 0.5, 0.6];
const PHIS = [-Math.PI, -2, -1, -0.25, 0, 0.25, 1, 2, Math.PI];

/* THE NORMALISATION IS AN INVARIANT, NOT A DEFAULT-ONLY FACT. The claim is
   peak <= 1 for EVERY (a, phi), which is why the grid is swept rather than the
   defaults spot-checked. This row reads the peak through the whole render path
   — envelope, level, tone filter — so it is the one that would catch a
   normaliser that is right in theory and clipped in practice. It cannot
   resolve 1e-6 (the render samples the shape at ~674 points per period and the
   20 kHz tone filter takes a few parts in 1e5 off the third partial), which is
   why the exactness claim is a separate property below and not a tightening of
   this threshold. */
{
  const sweep = (Core) => {
    let worst = 0, at = '', lowest = 9;
    for (const a of AS) for (const phi of PHIS) {
      const r = bumpLobes(Core, 44100, 36, { bumpAmt: a, bumpPhase: phi });
      if (r.amax > worst) { worst = r.amax; at = `a=${a}, phi=${phi.toFixed(2)}`; }
      if (r.amax < lowest) lowest = r.amax;
    }
    return { worst, at, lowest };
  };
  const s = sweep(SubOscCore);
  console.log(`      ${AS.length} x ${PHIS.length} grid: worst rendered peak ${s.worst.toFixed(6)} (${s.at}), ` +
              `quietest ${s.lowest.toFixed(6)} — a shortfall of ` +
              `${(1 - s.lowest).toExponential(1)}, which is this measurement's own ` +
              `resolution (sampling + tone filter), not the normaliser's error`);
  judge('bump peak <= 1 over the whole (a, phi) grid', s.worst <= 1, `worst ${s.worst.toFixed(6)} at ${s.at}`);
  // CONTROL: remove the normaliser and the same sweep must overshoot. Without
  // it, "peak <= 1" could be true because the shape is quiet, not because it is
  // normalised.
  const NoNorm = mutantCore(NORM_LINE, 'this._bumpNorm = 1;');
  const sc = sweep(NoNorm);
  judge('CONTROL normaliser removed must overshoot 1', sc.worst > 1.05,
        `worst ${sc.worst.toFixed(6)} at ${sc.at}`);
}

/* PEAK-EXACT, NOT MERELY BOUNDED (ADR-178 R7). The shape used to be divided by
   the analytic bound 1 + a, which is provable in one operation but left the
   bump at 0.750 of full scale at the defaults and 0.707 at worst — up to 3 dB
   quieter than the other six waveforms. The normaliser is now the MEASURED peak
   for the current (a, phi), so this row is the claim that pins it: max|y| = 1,
   everywhere in range, to 1e-6.

   MEASURED OFF THE SHAPE, NOT THE RENDER, and that is not a convenience — the
   render cannot resolve 1e-6 (see the row above). So the property evaluates the
   lab's own subBump with the normaliser the lab's own _recalc computed for
   those parameters, on a 65536-point scan of one period. The scan is the
   INDEPENDENT half: it is brute force over the raw formula, where the core's
   normaliser is a 32-bracket bisection search, so agreement to 1e-6 is two
   different methods meeting, not one method agreeing with itself. Scan bias is
   bounded by |f''|/2 . (dth/2)^2 <= 6.4/2 . (4.8e-5)^2 = 7e-9, two orders under
   the tolerance. */
const PEAK_TOL = 1e-6, PEAK_SCAN = 65536;
{
  const shapePeak = (bag, a, phi) => {
    const c = make(bag.SubOscCore, 44100, { bumpAmt: a, bumpPhase: phi });
    const norm = c._bumpNorm;
    let m = 0;
    for (let i = 0; i < PEAK_SCAN; i++) {
      const v = Math.abs(bag.subBump(i / PEAK_SCAN, a, phi, norm));
      if (v > m) m = v;
    }
    return m;
  };
  const pristine = dspExports();
  let worstDev = 0, at = '', lo = 9, hi = 0;
  for (const a of AS) for (const phi of PHIS) {
    const pk = shapePeak(pristine, a, phi);
    if (pk < lo) lo = pk;
    if (pk > hi) hi = pk;
    if (Math.abs(pk - 1) > worstDev) { worstDev = Math.abs(pk - 1); at = `a=${a}, phi=${phi.toFixed(2)}`; }
  }
  console.log(`      ${AS.length} x ${PHIS.length} grid, ${PEAK_SCAN}-point scan of one period: ` +
              `peak in [${lo.toFixed(9)}, ${hi.toFixed(9)}], worst deviation ${worstDev.toExponential(2)} (${at})`);
  judge(`bump peaks at 1.000000 +-${PEAK_TOL} over the whole (a, phi) grid`, worstDev <= PEAK_TOL,
        `worst |peak - 1| = ${worstDev.toExponential(3)} at ${at}`);

  // CONTROL: plant the OLD analytic bound back and the defaults must read 0.750
  // — the number the spec recorded for it. A peak check that still read 1.000
  // with the wrong normaliser in place would be measuring the scan, not the
  // core (LIBRARY L0032). The plant's anchor is asserted by dspExports.
  const bound = dspExports(NORM_LINE, 'this._bumpNorm = 1 / (1 + p.bumpAmt);');
  const boundDef = shapePeak(bound, 0.35, -0.25);
  const boundWorst = Math.min(...AS.flatMap(a => PHIS.map(phi => shapePeak(bound, a, phi))));
  judge('CONTROL old 1/(1+a) bound must read 0.750 at the defaults',
        Math.abs(boundDef - 0.750) < 5e-4,
        `defaults ${boundDef.toFixed(6)}, quietest over the grid ${boundWorst.toFixed(6)}`);
}

console.log('\n' + (failures ? 'RED' : 'GREEN') + ` — ${results.length} properties, ${failures} failed`);
process.exit(failures ? 1 : 0);
