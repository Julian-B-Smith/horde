/*
 * station_check.mjs — the fidelity suite for reference/station.html (STATION).
 * WIRED: ./verify full.
 *
 * WHERE IT RUNS. `./verify full` runs it (human ruling 2026-09-19, ADR-179 §4
 * inverted the wiring default), at the top of full() — not beside
 * lab_load_check in fast(), because fast is the seconds-scale CI leg and this
 * is ~9 s of DSP measurement. It was hand-run until then: STATION is
 * JavaScript, so there is no CMake target to hang it on, and `./verify`
 * contained zero references to the engine (audit §7: forty-four gate names,
 * none of them STATION's). Its only automated contact before this file was
 * lab_load_check.mjs, which asserts that the file's JS evaluates without
 * throwing and nothing whatever about what it computes. Same idiom as
 * lab_load_check, so it still runs standalone — prints numbers, exits non-zero
 * on failure:
 *
 *     node tools/labharness/station_check.mjs            (~60 s)
 *     node tools/labharness/station_check.mjs S4 S12     (named rows only)
 *
 * IT EXTRACTS, IT DOES NOT REIMPLEMENT. StationCore is sliced live out of the
 * HTML by tools/golden/extract_core.mjs, so the thing measured is the thing
 * that makes the sound. A re-implementation agrees with itself and certifies
 * nothing (reverb_check.mjs and feedback_scan.mjs carry the same note).
 *
 * WHAT THE THRESHOLDS ARE. Every gate is a number MEASURED on this lab plus a
 * stated margin — never an invented target. Rows S16/S18/S19 deliberately PIN a
 * known defect rather than demanding it be absent: parameter smoothing, the
 * op-OFF envelope freeze and release-fade voice stealing are all build-side work
 * (audit §6.4, SPEC-STATION §4/§8), and a gate that fails from day one is a gate
 * nobody reads. When the port changes one of those numbers deliberately,
 * re-measure and move the pin, with the reason. S17 was the fourth such pin; the
 * human ruled for the blocker on 2026-09-19 and it went into the LAB rather than
 * only the port, so S17 and S21 are real gates now and parity covers the stage.
 *
 * EVERY ROW CARRIES A MUST-FAIL CONTROL (L0016/L0032). A probe that only ever
 * reports the expected answer has not been shown to be able to report any other
 * one. Where a pre-fix defect exists, the control IS that defect, planted back
 * into a scratch copy of the DSP by anchored source substitution, and the
 * audit's pre-fix number is the expected failure — so the control is falsifiable
 * too. The planting mechanism is itself verified at start-up by a bogus anchor
 * that must throw.
 *
 * THE MASTER `tanh` IS NOT IN THE MEASUREMENT PATH. Every render below sets
 * `master = 0.02`, where the lab's output tanh is linear to -102 dB THD (audit
 * §4.3). At the default 0.75 it is -39.6 dB and would dominate every aliasing
 * and sideband number here — the monitor chain measuring itself.
 *
 * Provenance: docs/audits/2026-09-18-station-lab-audit.md (§5 is the row list
 * and every threshold's origin); ADR-177 §3 (the three sanctioned lab edits
 * this suite gates: S1 the core class, S4 the per-voice LFSR seed, S12 the
 * Nyquist mute).
 */
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { extractCore, BANNERS } from '../golden/extract_core.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '../..');
const LAB = join(ROOT, 'reference/station.html');
const StationCore = extractCore(LAB, 'StationCore', 'reference');

// ---------------------------------------------------------------- planting --
// A mutated copy of the DSP section, in memory only — the tracked file is never
// written (reference/ is a protected path). Each substitution asserts its
// anchor: if the source text is not found the mutation THROWS rather than
// silently measuring the unmutated lab, which would turn a must-fail control
// into a false pass.
function planted(subs) {
  const html = readFileSync(LAB, 'utf8');
  let src = html.slice(html.search(BANNERS.reference.start), html.search(BANNERS.reference.end));
  for (const [from, to] of subs) {
    if (!src.includes(from)) throw new Error(`ANCHOR MISSING: ${from}`);
    src = src.split(from).join(to);
  }
  return new Function(`"use strict";\n${src}\nreturn StationCore;`)();
}

// The pre-fix defects, as source reversions. Keeping them here (rather than in
// a copy of the old file) means they rot loudly: change the fixed line and the
// anchor throws.
const PLANT_ONE_SEED = [['lfsr:this.lfsrSeed(slot)', 'lfsr:0x7FFF']];               // pre-ADR-177 §3 (S4)
const PLANT_DT_CLAMP = [['const f=opFreq(o,base);\n          if(f>=SR*0.5){ outs[i]=0; this.envStep(v.env[i],o.env,v.gate); continue; }\n          const dt=f/SR;',
                        'const dt=Math.min(opFreq(o,base)/SR,0.45);']];             // pre-ADR-177 §3 (S12)
const PLANT_NO_DC = [['let dl=ml-this.dcX[0]+dcR*this.dcY[0]; if(Math.abs(dl)<DC_FLOOR) dl=0;\n      let dr=mr-this.dcX[1]+dcR*this.dcY[1]; if(Math.abs(dr)<DC_FLOOR) dr=0;',
                      'let dl=ml, dr=mr;']];                                       // pre-blocker (S17/S21)
// Never-shipped mutations, present only as controls:
const PLANT_NO_DELAY = [['s.matrix[0][i]*v.prev[0]', 's.matrix[0][i]*(i===0?v.prev[0]:outs[0])']];
const PLANT_PER_BLOCK = [['render(L,R,N){\n    const s=this.state',
                          'render(L,R,N){\n    for(const _v of this.voices) _v.ph[0]+=1e-3;\n    const s=this.state']];
const PLANT_PER_TICK = [['const t=Math.max(0.0005,p.a/1000)*SR;', 'const t=Math.max(0.0005,p.a/1000)*48000;']];

// ------------------------------------------------------- the lab's numbers --
// Read the blocker's cutoff OUT of the lab instead of restating it: a probe that
// hard-coded 5 Hz would keep passing after someone moved the lab's DC_FC, which
// is precisely the change it exists to catch.
const LAB_K = (() => {
  const html = readFileSync(LAB, 'utf8');
  const src = html.slice(html.search(BANNERS.reference.start), html.search(BANNERS.reference.end));
  return new Function(`"use strict";\n${src}\nreturn { DC_FC, DC_FLOOR, TAU };`)();
})();
const dcPole = (sr = 48000) => Math.exp(-LAB_K.TAU * LAB_K.DC_FC / sr);

// Recover the level/pan sum (`ml`/`mr` inside render) from a rendered channel by
// undoing the two stages that sit between them and the buffer: the engine-output
// DC blocker and the monitoring tanh. S8 and S9 both state their gate in terms
// of an OPERATOR's signal, so both have to see through the whole monitor path;
// before 2026-09-19 that path was `tanh(ml*master*1.4)` alone.
//
// Both inversions are exact in double precision, but the blocker's INTEGRATES
// (`x[n] = y[n] + x[n-1] - R*y[n-1]`, pole R = 0.99934 at 5 Hz / 48 k), so it
// amplifies whatever noise is on its input by up to 1/(1-R) ~ 1500x. That is why
// callers hand the core a Float64Array: at float32's 6e-8 quantisation the
// recovered sum would carry ~1e-4 of integrated noise and S8's 1e-6 gate would
// read the buffer format rather than the PM delay. The float32 render path is
// S13/S15's subject, not this analysis's.
function unmonitor(buf, master, sr = 48000) {
  const R = dcPole(sr), x = new Float64Array(buf.length);
  let xp = 0, yp = 0;
  for (let n = 0; n < buf.length; n++) {
    const y = Math.atanh(buf[n]) / (master * 1.4);
    x[n] = y + xp - R * yp;
    xp = x[n]; yp = y;
  }
  return x;
}

// --------------------------------------------------------------- rendering --
const noteFreq = n => 440 * Math.pow(2, (n - 69) / 12);
const SUSTAIN = { a: 1, d: 10, s: 1, r: 60, loop: 0 };   // steady state in ~11 ms

// A core with a blank matrix, silent operators and a linear monitor path. Every
// probe starts here and switches on exactly what it measures.
function blank(K = StationCore, sr = 48000) {
  const c = new K(sr), s = c.state;
  s.master = 0.02;
  s.matrix = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]];
  s.ops.forEach(o => { o.on = 1; o.lvl = 0; o.pan = 0; o.qnt = 0; o.pure = 1; o.sync = 0; o.env = { ...SUSTAIN }; });
  s.noise.on = 0;
  s.pitchEnv = { amt: 0, dec: 80 };
  return c;
}
// Render `n` samples in one call (block-size effects are S13's business).
function pull(c, n, bs = 0) {
  const L = new Float64Array(n), R = new Float64Array(n);
  if (!bs) { const l = new Float32Array(n), r = new Float32Array(n); c.render(l, r, n); L.set(l); R.set(r); return { L, R }; }
  let done = 0;
  while (done < n) {
    const k = Math.min(bs, n - done);
    const l = new Float32Array(k), r = new Float32Array(k);
    c.render(l, r, k);
    L.set(l, done); R.set(r, done); done += k;
  }
  return { L, R };
}
const rms = a => { let s = 0; for (const v of a) s += v * v; return Math.sqrt(s / a.length); };
const peak = a => { let m = 0; for (const v of a) m = Math.max(m, Math.abs(v)); return m; };
const mean = a => { let s = 0; for (const v of a) s += v; return s / a.length; };

// --------------------------------------------------------------- detectors --
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b;
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
}
// Blackman-Harris. -92 dB sidelobes: anything shallower and the window's own
// leakage would be read as the operator's aliasing at the -94 dB floor below.
const NFFT = 1 << 16;
function spectrum(buf, n = NFFT) {
  const re = new Float64Array(n), im = new Float64Array(n);
  const a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
  for (let i = 0; i < n; i++) {
    const w = a0 - a1 * Math.cos(2 * Math.PI * i / (n - 1)) + a2 * Math.cos(4 * Math.PI * i / (n - 1)) - a3 * Math.cos(6 * Math.PI * i / (n - 1));
    re[i] = (buf[i] || 0) * w;
  }
  fft(re, im);
  const m = new Float64Array(n / 2);
  for (let i = 0; i < n / 2; i++) m[i] = Math.hypot(re[i], im[i]);
  return m;
}
const dB = x => 20 * Math.log10(Math.max(x, 1e-300));
// Worst non-harmonic bin, dB below the fundamental. Harmonics (and DC) are
// excluded with a guard wide enough for the window's main lobe; a narrower
// guard reads the skirt of a harmonic and calls it alias.
function aliasFloor(buf, sr, f0, n = NFFT) {
  const m = spectrum(buf, n), binHz = sr / n, GUARD = 12;
  const isHarm = i => {
    const f = i * binHz;
    if (i < GUARD) return true;
    const k = Math.round(f / f0);
    return k >= 1 && Math.abs(f - k * f0) < GUARD * binHz;
  };
  const f0bin = Math.round(f0 / binHz);
  let fund = 0;
  for (let i = Math.max(1, f0bin - GUARD); i <= f0bin + GUARD; i++) fund = Math.max(fund, m[i]);
  let worst = 0, wi = 0;
  for (let i = 1; i < m.length; i++) if (!isHarm(i) && m[i] > worst) { worst = m[i]; wi = i; }
  return { db: dB(worst) - dB(fund), bin: wi, hz: wi * binHz };
}
// Level at an exact frequency (peak of the main lobe), linear magnitude.
function levelAt(m, sr, f, n = NFFT) {
  const b = Math.round(f / (sr / n)); let v = 0;
  for (let i = Math.max(1, b - 6); i <= b + 6 && i < m.length; i++) v = Math.max(v, m[i]);
  return v;
}
// Geometric/arithmetic mean ratio over 200 Hz .. 10 kHz — noise reads ~1, a
// pitched spectrum reads ~0.
function flatness(buf, sr, hiHz = 10000, n = NFFT) {
  const m = spectrum(buf, n), binHz = sr / n;
  const lo = Math.max(1, Math.round(200 / binHz)), hi = Math.min(m.length - 1, Math.round(hiHz / binHz));
  let ls = 0, as = 0, c = 0;
  for (let i = lo; i <= hi; i++) { const p = m[i] * m[i] + 1e-300; ls += Math.log(p); as += p; c++; }
  return Math.exp(ls / c) / (as / c);
}
const besselJ = (n, x) => {                 // series; plenty accurate for |x| <= 8, |n| <= 3
  const a = Math.abs(n); let s = 0;
  for (let k = 0; k < 60; k++) {
    let t = Math.pow(-1, k) * Math.pow(x / 2, 2 * k + a);
    let f = 1; for (let i = 2; i <= k; i++) f *= i;
    let g = 1; for (let i = 2; i <= k + a; i++) g *= i;
    s += t / (f * g);
  }
  return (n < 0 && a % 2) ? -s : s;
};
const isSubnormal = x => x !== 0 && Math.abs(x) < 1.1754943508222875e-38;

// ------------------------------------------------------------------ checks --
const CHECKS = [];
const check = (id, title, fn) => CHECKS.push({ id, title, fn });

// The audit §1.4 max patch, used by S1/S13/S15 — every matrix cell at 8, DRW
// operators at PURE 0.5 / QNT 16, looping envelopes, SHORT noise with KEYTRK,
// +24 st pitch env. It is the worst case for every code path at once.
function maxPatch(K = StationCore, sr = 48000, seedTable = 1024) {
  const c = new K(sr), s = c.state;
  for (let r = 0; r < 4; r++) for (let k = 0; k < 3; k++) s.matrix[r][k] = 8;
  s.ops.forEach(o => { o.on = 1; o.wave = 5; o.lvl = 0.8; o.pure = 0.5; o.qnt = 16; o.pw = 0.37;
    o.env = { a: 3, d: 420, s: 0.55, r: 260, loop: 1 }; });
  s.noise = { on: 1, mode: 1, rate: 0.8, ktrk: 1, lvl: 0.7, pan: 0, env: { a: 1, d: 120, s: 0.6, r: 80, loop: 0 } };
  s.pitchEnv = { amt: 24, dec: 800 };
  s.seed = seedTable; c.reseedTable(); s.table = s.table.map((_, i) => c.TP.RND(i));
  return c;
}
function fnv(...bufs) {
  let h = 0xcbf29ce484222325n;
  for (const b of bufs) {
    const v = new Uint8Array(Float64Array.from(b).buffer);
    for (let i = 0; i < v.length; i++) { h ^= BigInt(v[i]); h = (h * 0x100000001b3n) & 0xffffffffffffffffn; }
  }
  return h.toString(16);
}
function maxRender(K, secs = 3, seedTable = 1024, bs = 0) {
  const c = maxPatch(K, 48000, seedTable);
  c.noteOn(60, noteFreq(60)); c.noteOn(67, noteFreq(67));
  return pull(c, Math.round(48000 * secs), bs);
}

// S1 — determinism. The hash is the whole engine's signature in one number.
check('S1', 'determinism: two fresh instances of the max patch, bit-identical', () => {
  // The hash has moved three times, all deliberately: bc9eea68afc8bfd9 (audit
  // §1.4, pre-fix) -> d867466ebf6ed6d5 (per-voice LFSR seed changed the noise
  // stream) -> 75e363cf732c9081 (the Nyquist mute silences OP3 at the +24 st
  // pitch-env peak, where the old clamp used to detune it) -> this (the 5 Hz
  // engine-output DC blocker, human ruling 2026-09-19). The class wrap itself
  // moved nothing: it reproduced bc9eea68afc8bfd9 exactly.
  const PIN = '7aa8506c5eee18e1';
  const a = maxRender(StationCore), b = maxRender(StationCore);
  let d = 0; for (let i = 0; i < a.L.length; i++) d = Math.max(d, Math.abs(a.L[i] - b.L[i]), Math.abs(a.R[i] - b.R[i]));
  const hA = fnv(a.L, a.R), hB = fnv(b.L, b.R);
  const lines = [`  max|diff| A vs B ${d}  (gate: exactly 0)`,
                 `  FNV-1a A ${hA}   B ${hB}   (pin: ${PIN})`];
  // CONTROL: a different Wave RAM seed must produce a different render, or the
  // hash is insensitive to the thing it claims to certify.
  const hC = (() => { const r = maxRender(StationCore, 3, 99); return fnv(r.L, r.R); })();
  lines.push(`  control  seed 99 hash ${hC}  (gate: != A)`);
  return { ok: d === 0 && hA === PIN && hB === PIN && hC !== hA, lines };
});

// S2 — Wave RAM seed. ADR-122's sanctioned seeding, still spent.
check('S2', 'Wave RAM: seed 1024 reproduces the recorded table', () => {
  const PIN = '5,4,13,6,12,4,6,12,12,1,8,0,2,15,8,6,5,3,1,2,10,14,11,4,7,4,4,3,2,1,1,5';  // audit §1.1
  const tbl = seed => { const c = new StationCore(48000); c.state.seed = seed; c.reseedTable();
    return c.state.table.map((_, i) => c.TP.RND(i)).join(','); };
  const t1 = tbl(1024), t2 = tbl(99);
  const lines = [`  seed 1024 ${t1}`, `  pin       ${PIN}`,
                 `  control  seed 99 differs: ${t1 !== t2}  (gate: true)`];
  // A live Math.random would break replay determinism outright (SPEC §5.7).
  const live = (readFileSync(LAB, 'utf8').match(/Math\.random/g) || []).length;
  const commented = (readFileSync(LAB, 'utf8').match(/\/\/[^\n]*Math\.random/g) || []).length;
  lines.push(`  Math.random occurrences ${live}, of them in comments ${commented}  (gate: none live)`);
  return { ok: t1 === PIN && t1 !== t2 && live === commented, lines };
});

// S3 — LFSR periods. Spec §6 names both numbers; they are the identity of the
// NES noise channel, not an implementation detail.
check('S3', 'LFSR periods: LONG 32767, SHORT 93, pre-period 0', () => {
  const walk = (mode, seed, tap6) => {
    const seen = new Map(); let l = seed, i = 0;
    for (; i < 200000; i++) {
      if (seen.has(l)) return { period: i - seen.get(l), pre: seen.get(l) };
      seen.set(l, i);
      const b0 = l & 1, tap = mode ? ((l >> 6) & 1) : ((l >> (tap6 || 1)) & 1);
      l = (l >> 1) | (((b0 ^ tap) & 1) << 14);
    }
    return { period: NaN, pre: NaN };
  };
  const c = blank(); c.state.seed = 1024;
  const seeds = [0, 1, 2, 3].map(slot => c.lfsrSeed(slot));
  const lines = [], nonZero = seeds.every(s => s !== 0 && s <= 0x7FFF);
  let ok = nonZero;
  for (const s of seeds) {
    const L = walk(0, s), S = walk(1, s);
    ok = ok && L.period === 32767 && L.pre === 0 && S.period === 93 && S.pre === 0;
    lines.push(`  seed 0x${s.toString(16).padStart(4, '0')}  LONG ${L.period}/pre ${L.pre}   SHORT ${S.period}/pre ${S.pre}`);
  }
  lines.push(`  all seeds nonzero and 15-bit: ${nonZero}  (spec §6)`);
  // CONTROL: a wrong tap must NOT give 32767 — the period is a real fingerprint.
  const w = walk(0, 0x7FFF, 2);
  lines.push(`  control  tap bit2 instead of bit1 -> period ${w.period}  (gate: != 32767)`);
  return { ok: ok && w.period !== 32767, lines };
});

// S4 — LFSR decorrelation. THE row ADR-177 §3's first sanctioned edit exists for.
check('S4', 'noise decorrelates across voices (N-voice / 1-voice RMS)', () => {
  const GATE = 2.2;                                     // sqrt(4) = 2.0 + 10 % margin
  const run = (K, nv, ktrk) => {
    const c = blank(K); const s = c.state;
    s.ops.forEach(o => { o.on = 0; o.lvl = 0; });
    s.noise = { on: 1, mode: 0, rate: 0.35, ktrk, lvl: 1, pan: 0, env: { a: 1, d: 120, s: 1, r: 80, loop: 0 } };
    for (let i = 0; i < nv; i++) c.noteOn(60, noteFreq(60));
    return rms(pull(c, 48000).L);
  };
  const r1 = run(StationCore, 1, 0), r4 = run(StationCore, 4, 0), ratio = r4 / r1;
  const on1 = run(StationCore, 1, 1), on4 = run(StationCore, 4, 1);
  const lines = [`  KEYTRK off  1v ${r1.toExponential(3)}  4v ${r4.toExponential(3)}  ratio ${ratio.toFixed(3)}  (gate: <= ${GATE}; independent = 2.000)`,
                 `  KEYTRK on   1v ${on1.toExponential(3)}  4v ${on4.toExponential(3)}  ratio ${(on4 / on1).toFixed(3)}`];
  // MUST-FAIL CONTROL: the pre-ADR-177 constant seed, planted back. The audit
  // measured 3.999 on it; if this reads low the probe cannot see coherence at
  // all and the green above means nothing.
  const p1 = run(planted(PLANT_ONE_SEED), 1, 0), p4 = run(planted(PLANT_ONE_SEED), 4, 0);
  const pr = p4 / p1;
  lines.push(`  control  planted lfsr:0x7FFF for every voice -> ratio ${pr.toFixed(3)}  (gate: >= 3.5; audit measured 3.999)`);
  return { ok: ratio <= GATE && pr >= 3.5, lines };
});

// S5 — aliasing floor per waveform.
//
// TWO TABLES, AND THE TWO PLACES THEY DISAGREE. ALIAS_PIN is this suite's own
// measurement (the gate). ALIAS_AUDIT is the audit's §2.1 table, carried as an
// INDEPENDENT cross-check — a second harness, written separately, on the same
// lab. Over the 24 cells at MIDI >= 60 above -60 dB the two agree to 0.1 dB,
// and that agreement is what justifies gating on the pin at all.
//
// They part in exactly two places, and NEITHER is a difference in the lab:
//   · every cell below -60 dB — both probes are reading their own analysis
//     floor, not the operator. This detector's 12-bin harmonic guard and
//     65536-point Blackman-Harris put that floor near -105 dB; the audit's
//     harness put it near -94. "Nothing measurable here", said twice.
//   · the whole MIDI 36 column, by up to 3.55 dB — at 65.4 Hz the harmonics sit
//     ~89 bins apart, so "worst NON-harmonic bin" depends on how wide the
//     harmonic guard is, and the two harnesses chose differently. At MIDI 60
//     the spacing is 4x wider and the disagreement vanishes.
// So the cross-check is asserted where it is meaningful and the remainder is
// PRINTED — never folded into a looser tolerance that would hide both.
const ALIAS_PIN = {    // measured 2026-09-19 on this detector; gate +-1.0 dB
  1: { SIN: [-105.6, -107.6, -108.9, -104.4], TRI: [-103.4, -78.7, -54.4, -44.8], SAW: [-60.8, -47.0, -35.0, -30.0],
       PLS: [-60.8, -47.4, -35.0, -32.9], QTR: [-103.4, -78.7, -54.4, -44.8], DRW: [-93.6, -65.2, -44.9, -43.7] },
  0: { SIN: [-105.6, -107.6, -108.9, -104.4], TRI: [-103.4, -78.7, -54.4, -44.8], SAW: [-52.9, -39.1, -27.1, -21.3],
       PLS: [-52.9, -39.3, -27.1, -22.5], QTR: [-53.2, -41.5, -29.2, -29.0], DRW: [-50.7, -35.2, -25.1, -24.9] },
};
const ALIAS_AUDIT = {  // audit §2.1, PURE = 1 / PURE = 0
  1: { SIN: [-94.3, -96.9, -98.2, -93.2], TRI: [-94.2, -78.7, -54.4, -44.8], SAW: [-58.9, -47.0, -35.0, -30.0],
       PLS: [-59.2, -47.4, -35.0, -32.9], QTR: [-94.2, -78.7, -54.4, -44.8], DRW: [-89.5, -65.2, -44.9, -43.7] },
  0: { SIN: [-94.3, -96.9, -98.2, -93.2], TRI: [-94.2, -78.7, -54.4, -44.8], SAW: [-51.0, -39.1, -27.1, -21.3],
       PLS: [-51.1, -39.3, -27.1, -22.5], QTR: [-51.6, -41.5, -29.2, -28.9], DRW: [-47.2, -35.2, -25.1, -24.9] },
};
const WAVE_NAMES = ['SIN', 'TRI', 'SAW', 'PLS', 'QTR', 'DRW'];
function oneOp(wave, midi, pure, qnt = 0, sr = 48000) {
  const c = blank(StationCore, sr), s = c.state;
  s.ops[0].wave = wave; s.ops[0].lvl = 1; s.ops[0].pure = pure; s.ops[0].qnt = qnt; s.ops[0].coarse = 1;
  s.ops[1].on = 0; s.ops[2].on = 0;
  c.noteOn(midi, noteFreq(midi));
  const { L } = pull(c, NFFT + 4096);
  return { buf: L.subarray(4096), f0: noteFreq(midi), sr };
}
check('S5', 'aliasing floor, 6 waveforms x MIDI {36,60,84,96} x PURE {0,1}', () => {
  const TOL = 1.0;          // gate against this suite's own pin
  const XTOL = 1.5;         // cross-check against the audit, real-aliasing cells only
  const FLOOR = -60;        // above this the two probes measure the operator, below it themselves
  const MIDIS = [36, 60, 84, 96];
  const cmp = (aud, i) => aud > FLOOR && MIDIS[i] >= 60;   // where the cross-check is meaningful
  const lines = []; let ok = true, worstPin = 0, worstX = 0, xCells = 0, worst36 = 0;
  for (const pure of [1, 0]) {
    lines.push(`  PURE = ${pure}     (measured [pin] {audit §2.1})`);
    for (let w = 0; w < 6; w++) {
      const got = MIDIS.map(mi => aliasFloor(oneOp(w, mi, pure).buf, 48000, noteFreq(mi)).db);
      const pin = ALIAS_PIN[pure][WAVE_NAMES[w]], aud = ALIAS_AUDIT[pure][WAVE_NAMES[w]];
      got.forEach((g, i) => {
        worstPin = Math.max(worstPin, Math.abs(g - pin[i]));
        if (cmp(aud[i], i)) { worstX = Math.max(worstX, Math.abs(g - aud[i])); xCells++; }
        else if (aud[i] > FLOOR) worst36 = Math.max(worst36, Math.abs(g - aud[i]));
      });
      ok = ok && got.every((g, i) => Math.abs(g - pin[i]) <= TOL);
      lines.push(`    ${WAVE_NAMES[w]}  ` + got.map((g, i) =>
        `${g.toFixed(1)}`.padStart(7) + `[${pin[i].toFixed(1)}]` + (aud[i] > FLOOR ? `{${aud[i].toFixed(1)}}` : '{floor}')).join(' '));
    }
  }
  ok = ok && worstX <= XTOL;
  lines.push(`  worst drift from this suite's pin: ${worstPin.toFixed(2)} dB  (gate: <= ${TOL})`);
  lines.push(`  cross-check vs the audit, ${xCells} cells at MIDI >= 60 above ${FLOOR} dB: worst ${worstX.toFixed(2)} dB  (gate: <= ${XTOL})`);
  lines.push(`  REPORT  MIDI 36 column, same cells: worst ${worst36.toFixed(2)} dB — harmonic-guard width, not the lab (see header)`);
  // CONTROL: SIN on the pure branch has no aliasing to find; if this row does
  // not read the floor the detector is measuring window leakage, not aliasing.
  const sinPure = aliasFloor(oneOp(0, 96, 1).buf, 48000, noteFreq(96)).db;
  lines.push(`  control  SIN pure @MIDI 96 -> ${sinPure.toFixed(1)} dB  (gate: <= -90)`);
  return { ok: ok && sinPure <= -90, lines };
});

// S6 — QNT. It is SUPPOSED to alias; what is gated is the counter-intuitive
// ORDERING (coarse quantization aliases LESS at the top), because a "cleaner"
// implementation that worsens monotonically with QNT has changed the instrument.
check('S6', 'phase quantization: level and the QNT-4 > QNT-64 ordering at MIDI 96', () => {
  const lines = []; let ok = true;
  const rows = [];
  for (const q of [0, 4, 8, 16, 32, 64]) {
    const r = [36, 60, 84, 96].map(mi => aliasFloor(oneOp(0, mi, 0, q).buf, 48000, noteFreq(mi)).db);
    rows.push({ q, r });
    lines.push(`  QNT ${String(q === 0 ? 'OFF' : q).padStart(3)}  ` + r.map(x => x.toFixed(1).padStart(7)).join('  '));
  }
  const q4 = rows.find(x => x.q === 4).r[3], q64 = rows.find(x => x.q === 64).r[3];
  ok = q4 > q64 + 5;                                      // audit: -22.5 vs -35.6, 13 dB apart
  lines.push(`  ordering at MIDI 96: QNT 4 ${q4.toFixed(1)} dB louder-aliasing than QNT 64 ${q64.toFixed(1)} dB, gap ${(q4 - q64).toFixed(1)} dB  (gate: >= 5; audit 13.1)`);
  // CONTROL: QNT OFF must read the clean floor, or every row above is noise.
  const off = rows.find(x => x.q === 0).r[3];
  lines.push(`  control  QNT OFF @MIDI 96 -> ${off.toFixed(1)} dB  (gate: <= -90)`);
  return { ok: ok && off <= -90, lines };
});

// S7/S8 — the two halves of the PM law. Carrier MIDI 36, modulator ratio 7:1
// (no sideband overlap), sine into sine, pure branch, normalised so sum Jn^2 = 1.
// This validates spec §4's index law WITHOUT reference to the lab at all, and
// then S8 gates the SIGN of the one-sample delay's asymmetry.
function sidebands(K, index, sr = 48000) {
  const c = blank(K, sr), s = c.state;
  s.ops[0].wave = 0; s.ops[0].coarse = 7; s.ops[0].lvl = 0;     // OP1 = modulator
  s.ops[1].wave = 0; s.ops[1].coarse = 1; s.ops[1].lvl = 1;     // OP2 = carrier
  s.ops[2].on = 0;
  s.matrix[0][1] = index;                                       // OP1 -> OP2
  c.noteOn(36, noteFreq(36));
  const { L } = pull(c, NFFT + 4096);
  const m = spectrum(L.subarray(4096));
  const fc = noteFreq(36), fm = fc * 7;
  const lv = {}; let sum = 0;
  // fc + n*fm is NEGATIVE for the lower sidebands here (fm = 7*fc), and a
  // negative-frequency component appears in a real spectrum at |f|. Taking the
  // magnitude is the measurement; forgetting it reads every lower sideband as 0.
  for (let n = -3; n <= 3; n++) { const v = levelAt(m, sr, Math.abs(fc + n * fm)); lv[n] = v; sum += v * v; }
  const norm = Math.sqrt(sum);
  for (const k of Object.keys(lv)) lv[k] /= norm;
  return lv;
}
check('S7', 'PM index vs sideband energy (Bessel), I in {0.5,1,2,4,8}', () => {
  const TOL = 0.6;                                        // audit §4.4 worst 0.50 dB + margin
  const lines = []; let ok = true, worst = 0;
  for (const I of [0.5, 1, 2, 4, 8]) {
    const lv = sidebands(StationCore, I);
    const th = {}; let sum = 0;
    for (let n = -3; n <= 3; n++) { th[n] = Math.abs(besselJ(n, I)); sum += th[n] * th[n]; }
    for (let n = -3; n <= 3; n++) th[n] /= Math.sqrt(sum);
    const errs = [];
    for (let n = -2; n <= 3; n++) {
      const e = Math.abs(dB(lv[n]) - dB(th[n]));
      if (th[n] > 0.02) { errs.push(e); worst = Math.max(worst, e); }
    }
    lines.push(`  I=${String(I).padEnd(4)} ` + [-2, -1, 0, 1, 2, 3].map(n => `n${n >= 0 ? '+' : ''}${n} ${lv[n].toFixed(4)}/${th[n].toFixed(4)}`).join(' '));
  }
  ok = worst <= TOL;
  lines.push(`  worst |measured - theory| over n in [-2,+3] with |Jn| > 0.02: ${worst.toFixed(2)} dB  (gate: <= ${TOL}; audit 0.50)`);
  // CONTROL: index 0 must put everything in the carrier. A probe that cannot
  // read an unmodulated spectrum cannot be trusted on a modulated one.
  const z = sidebands(StationCore, 0);
  lines.push(`  control  I=0 -> n0 ${z[0].toFixed(5)}, n+1 ${z[1].toFixed(5)}  (gate: n0 > 0.999)`);
  return { ok: ok && z[0] > 0.999, lines };
});

// S8 — the one-sample delay. Spec §2 reads every PM source from the PREVIOUS
// sample, and a port that reads the current one is a different instrument.
//
// THE AUDIT'S PROPOSED DETECTOR DOES NOT WORK, AND THE CONTROL IS WHAT PROVED
// IT. Audit §4.4 offers the sideband SIGN pattern (lower sidebands above theory
// by +0.14..+0.43 dB, upper below by -0.11..-0.50) as "the phase lag of the
// one-sample delay", and asserts "a C++ port that reads the current sample
// instead of `prev` produces a symmetric spectrum and fails the asymmetry
// check". Measured here on 2026-09-19, that is false twice over:
//   · PLANT_NO_DELAY — the lab with the delay removed — reproduces the sign
//     pattern to three decimals (+0.218 / -0.223 at I=2, |n|=1: identical);
//   · a synthetic exact PM signal built outside the lab reads the same
//     +0.218 / -0.223 whether its modulator is delayed by one sample or not.
// This is sound: one sample of delay on a sinusoidal modulator is a pure phase
// rotation, and |Jn| is invariant under it. The sign pattern IS real and
// reproducible — it is a property of the ANALYSIS (the lower sidebands fold
// through DC at fm = 7*fc), not of the delay. Gating it would have been a
// detector confirming the expected answer for the wrong reason.
//
// WHAT IS GATED INSTEAD is the delay itself, in the time domain and exactly:
// the carrier's own phase `v.ph[1]` and the modulator's output `v.prev[0]` are
// sampled after every single-sample render, the carrier output is inverted
// through the monitor path, and the sample it was actually built from is
// identified by residual. The shipped lab matches lag 1 to float32 resolution
// and mismatches lag 0 by six orders of magnitude; the planted no-delay build
// reverses that exactly. The audit's sideband numbers are still printed, as a
// measurement, with their interpretation corrected.
check('S8', 'PM sources are read from the PREVIOUS sample (time-domain, exact)', () => {
  const I = 2, lines = [];
  const lagResidual = (K) => {
    const c = blank(K); const s = c.state;
    s.ops[0].wave = 0; s.ops[0].coarse = 7; s.ops[0].lvl = 0;     // OP1 = modulator
    s.ops[1].wave = 0; s.ops[1].coarse = 1; s.ops[1].lvl = 1;     // OP2 = carrier
    s.ops[2].on = 0;
    s.matrix[0][1] = I;
    const v = c.noteOn(36, noteFreq(36));
    // Float64 buffers, and the blocker's inverse is run from sample 0: see
    // unmonitor() — the inverse integrates, so it needs the whole history and
    // cannot afford the float32 quantisation the audio graph lives with.
    const L = new Float64Array(1), R = new Float64Array(1);
    const SETTLE = 2000, N = 4000, raw = [], ph = [], m = [];
    for (let n = 0; n < SETTLE + N; n++) {
      c.render(L, R, 1); raw.push(L[0]);
      if (n >= SETTLE) { ph.push(v.ph[1]); m.push(v.prev[0]); }
    }
    // Invert the monitor path: l = tanh(DC(out * lvl * 0.35) * master * 1.4).
    const sum = unmonitor(Float64Array.from(raw), 0.02).subarray(SETTLE);
    const res = lag => {
      let e = 0;
      for (let n = 1; n < N; n++) {
        const out = sum[n] / 0.35;
        const p = ph[n] + I * m[n - lag] * 0.1591549;
        e = Math.max(e, Math.abs(out - Math.sin((p - Math.floor(p)) * 2 * Math.PI)));
      }
      return e;
    };
    return { l0: res(0), l1: res(1) };
  };
  const r = lagResidual(StationCore);
  lines.push(`  shipped lab   max|out - model(lag 1)| ${r.l1.toExponential(2)}   max|out - model(lag 0)| ${r.l0.toExponential(2)}`);
  lines.push(`                (gate: lag 1 < 1e-6 — the recovered sum is double-precision — AND lag 0 > 0.01)`);
  // MUST-FAIL CONTROL: the same probe on a build that reads the modulator's
  // CURRENT sample must come out the other way round.
  const p = lagResidual(planted(PLANT_NO_DELAY));
  lines.push(`  control  planted no-delay build: lag 1 ${p.l1.toExponential(2)}   lag 0 ${p.l0.toExponential(2)}  (gate: reversed)`);
  // REPORT — the audit's §4.4 sideband asymmetry, re-measured, with its
  // interpretation corrected by the control above.
  const asym = (K) => {
    const out = [];
    for (const Ix of [0.5, 1, 2, 4, 8]) {
      const lv = sidebands(K, Ix);
      const th = {}; let sum = 0;
      for (let n = -3; n <= 3; n++) { th[n] = Math.abs(besselJ(n, Ix)); sum += th[n] * th[n]; }
      for (let n = -3; n <= 3; n++) th[n] /= Math.sqrt(sum);
      for (const n of [1, 2]) {
        if (th[n] < 0.05) continue;
        out.push({ I: Ix, n, lo: dB(lv[-n]) - dB(th[-n]), up: dB(lv[n]) - dB(th[n]) });
      }
    }
    return out;
  };
  const a = asym(StationCore), b = asym(planted(PLANT_NO_DELAY));
  const same = Math.max(...a.map((r0, i) => Math.max(Math.abs(r0.lo - b[i].lo), Math.abs(r0.up - b[i].up))));
  lines.push(`  REPORT  sideband asymmetry (audit §4.4): lower +${Math.min(...a.map(x => x.lo)).toFixed(2)}..+${Math.max(...a.map(x => x.lo)).toFixed(2)} dB, ` +
             `upper ${Math.min(...a.map(x => x.up)).toFixed(2)}..${Math.max(...a.map(x => x.up)).toFixed(2)} dB  (audit: +0.14..+0.43 / -0.11..-0.50)`);
  lines.push(`  REPORT  the no-delay build reproduces it to ${same.toFixed(3)} dB — so it is the ANALYSIS, not the delay; do not gate it`);
  return { ok: r.l1 < 1e-6 && r.l0 > 0.01 && p.l0 < 1e-6 && p.l1 > 0.01, lines };
});
check('S9', 'self-feedback at index 8: bounded and finite over 10 s, all 3 diagonals', () => {
  const lines = []; let ok = true;
  for (let d = 0; d < 3; d++) {
    const c = blank(); const s = c.state;
    s.ops.forEach((o, i) => { o.on = i === d ? 1 : 0; o.lvl = i === d ? 1 : 0; o.wave = 0; });
    s.matrix[d][d] = 8;
    c.noteOn(60, noteFreq(60));
    // The gate is on the OPERATOR's excursion, so the engine-output DC blocker
    // has to be undone with the tanh (unmonitor()) — its transient response
    // overshoots by ~13 % at note-on, which is the monitor path's business and
    // not the feedback loop's. Float64 for the same reason S8 uses it.
    const L = new Float64Array(48000 * 10), R = new Float64Array(L.length);
    c.render(L, R, L.length);
    let bad = 0;
    for (const v of L) if (!isFinite(v)) bad++;
    const opPeak = peak(unmonitor(L, 0.02)) / 0.35;
    ok = ok && bad === 0 && opPeak <= 1 + 1e-6;
    lines.push(`  OP${d + 1} -> OP${d + 1}  |op| peak ${opPeak.toFixed(6)}  non-finite ${bad}  (gate: <= 1.000001, 0)`);
  }
  // CONTROL: index 0 is a plain sine — perfectly periodic, so flatness must
  // read ~0. A stability probe that cannot tell chaos from a sine is useless.
  const c0 = blank(); c0.state.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; });
  c0.noteOn(60, noteFreq(60));
  const f0 = flatness(pull(c0, NFFT + 4096).L.subarray(4096), 48000);
  lines.push(`  control  index 0 spectral flatness ${f0.toExponential(2)}  (gate: < 1e-3 — a pure sine)`);
  return { ok: ok && f0 < 1e-3, lines };
});

// S10 — the noise channel's own character: LONG is noise, SHORT is pitched.
//
// THE BAND IS PART OF THE MEASUREMENT. The LFSR output is a zero-order hold at
// `rate * SR/2`, so its spectrum carries that clock's sinc envelope with a null
// AT the clock. Measuring flatness over a fixed 200 Hz..10 kHz reads the sinc
// notch, not the sequence: at rate 0.35 (clock 8400 Hz) it gives 0.2272 against
// the audit's 0.6050, while at rate 1.0 (clock 24 kHz, null outside the band) it
// gives 0.6136 against the audit's 0.6779. So the band is capped at half the
// clock, where the hold is flat and the number is about the SEQUENCE.
check('S10', 'noise spectrum: LONG flat, SHORT pitched', () => {
  const band = rate => Math.min(10000, rate * 48000 * 0.5 * 0.5);
  const run = (mode, rate) => {
    const c = blank(); const s = c.state;
    s.ops.forEach(o => { o.on = 0; o.lvl = 0; });
    s.noise = { on: 1, mode, rate, ktrk: 0, lvl: 1, pan: 0, env: { a: 1, d: 120, s: 1, r: 80, loop: 0 } };
    c.noteOn(60, noteFreq(60));
    return flatness(pull(c, NFFT + 4096).L.subarray(4096), 48000, band(rate));
  };
  const lo = run(0, 0.35), sh = run(1, 0.35);
  const lines = [`  LONG  (bit0^bit1, 32767) flatness ${lo.toFixed(4)} over 200..${band(0.35).toFixed(0)} Hz  (gate: >= 0.50; audit 0.6050)`,
                 `  SHORT (bit0^bit6, 93)    flatness ${sh.toExponential(2)} over the same band  (gate: <= 0.05; audit 0.0017)`];
  // CONTROL: the detector must move with the clock rate, not only with the tap.
  const hi = run(0, 1.0);
  lines.push(`  control  LONG at rate 1.0 -> ${hi.toFixed(4)} over 200..${band(1.0).toFixed(0)} Hz  (gate: > 0.5; audit 0.6779)`);
  return { ok: lo >= 0.50 && sh <= 0.05 && hi > 0.5, lines };
});

// S11 — sample-rate portability of the envelopes. ADR-009's trap is per-tick
// constants; STATION has none, and this is the row that keeps it that way.
function envLandmarks(K, sr) {
  const c = blank(K, sr); const s = c.state;
  s.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; o.wave = 0; o.pure = 1;
    o.env = { a: 50, d: 400, s: 0.3, r: 200, loop: 0 }; });
  const v = c.noteOn(60, noteFreq(60));
  const L = new Float32Array(1), R = new Float32Array(1);
  let atk = NaN, dec = NaN;
  for (let n = 0; n < sr * 2; n++) {
    c.render(L, R, 1);
    if (isNaN(atk) && v.env[0].stage >= 1) atk = n / sr;
    if (isNaN(dec) && v.env[0].stage >= 2) { dec = n / sr; break; }
  }
  return { atk, dec };
}
check('S11', 'envelope landmarks are sample-rate portable (44.1 / 48 / 96 k)', () => {
  const GATE = 1.6;     // audit measured attack 1.587 % — the ONE-SAMPLE quantisation
                        // of `lvl += 1/t`, not a per-tick constant. Pinned, not wished away.
  const r = [44100, 48000, 96000].map(sr => ({ sr, ...envLandmarks(StationCore, sr) }));
  const spread = key => (Math.max(...r.map(x => x[key])) - Math.min(...r.map(x => x[key]))) / r[0][key] * 100;
  const sa = spread('atk'), sd = spread('dec');
  const lines = [`  attack complete  ` + r.map(x => `${x.sr / 1000}k ${x.atk.toFixed(5)}s`).join('  ') + `   drift ${sa.toFixed(3)} %  (gate: <= ${GATE})`,
                 `  decay complete   ` + r.map(x => `${x.sr / 1000}k ${x.dec.toFixed(5)}s`).join('  ') + `   drift ${sd.toFixed(3)} %  (gate: <= 0.5)`];
  // MUST-FAIL CONTROL: the ADR-009 trap itself, planted — an attack time
  // computed against a hard-coded 48000 instead of SR.
  const p = [44100, 48000, 96000].map(sr => envLandmarks(planted(PLANT_PER_TICK), sr).atk);
  const ps = (Math.max(...p) - Math.min(...p)) / p[0] * 100;
  lines.push(`  control  planted *48000 instead of *SR -> attack drift ${ps.toFixed(1)} %  (gate: > 10)`);
  return { ok: sa <= GATE && sd <= 0.5 && ps > 10, lines };
});

// S12 — the Nyquist limit. ADR-177 §3's third sanctioned edit.
check('S12', 'no operator plays a WRONG pitch: correct below Nyquist, silent above', () => {
  const pitchOf = (K, sr, midi, coarse) => {
    const c = blank(K, sr); const s = c.state;
    s.ops.forEach((o, i) => { o.on = i === 2; o.lvl = i === 2 ? 0.85 : 0; o.wave = 0; });
    s.ops[2].coarse = coarse;
    c.noteOn(midi, noteFreq(midi));
    const { L } = pull(c, NFFT + 4096);
    const buf = L.subarray(4096);
    if (peak(buf) === 0) return { f: 0, silent: true };
    const m = spectrum(buf);
    let bi = 1, bv = 0;
    for (let i = 1; i < m.length; i++) if (m[i] > bv) { bv = m[i]; bi = i; }
    return { f: bi * sr / NFFT, silent: false };
  };
  // THE TOLERANCE IS IN HERTZ, NOT PERCENT. A peak-bin read resolves to
  // sr/NFFT (0.67 Hz at 44.1 k, 1.46 Hz at 96 k), so at MIDI 24 with ratio 1
  // (32.7 Hz) one bin IS 4.5 % — a percentage gate would fail the detector's
  // own resolution and call it a pitch error. Two bins separates a correct
  // operator from the planted clamp's error by four orders of magnitude.
  const lines = []; let ok = true, worstErr = 0, worstHz = 0;
  for (const sr of [44100, 48000, 96000]) {
    const tolHz = 2 * sr / NFFT;
    for (const coarse of [1, 7, 14]) {
      let bad = 0, sil = 0, n = 0;
      for (let midi = 24; midi <= 108; midi += 6) {
        const want = noteFreq(midi) * coarse;
        const g = pitchOf(StationCore, sr, midi, coarse);
        n++;
        if (g.silent) { sil++; if (want < sr * 0.5) { bad++; } continue; }   // silent below Nyquist = wrong too
        const dHz = Math.abs(g.f - want);
        worstErr = Math.max(worstErr, dHz / want * 100); worstHz = Math.max(worstHz, dHz);
        if (dHz > tolHz) bad++;
      }
      ok = ok && bad === 0;
      lines.push(`  ${sr / 1000}k  ratio ${String(coarse).padStart(2)}  ${n} notes MIDI 24..108: wrong-pitch ${bad}, silent-above-Nyquist ${sil}  (gate: wrong-pitch 0, tol ${tolHz.toFixed(2)} Hz = 2 bins)`);
    }
  }
  lines.push(`  worst deviation where the operator sounds: ${worstHz.toFixed(2)} Hz (${worstErr.toFixed(3)} %)  (gate: <= 2 bins)`);
  // MUST-FAIL CONTROL: the pre-ADR-177 phase-increment clamp, planted back. The
  // audit measured -32.27 % at 44.1 k, MIDI 96, ratio 14.
  const pc = pitchOf(planted(PLANT_DT_CLAMP), 44100, 96, 14);
  const perr = (pc.f - noteFreq(96) * 14) / (noteFreq(96) * 14) * 100;
  lines.push(`  control  planted min(f/SR, 0.45) -> MIDI 96 ratio 14 @44.1k reads ${pc.f.toFixed(0)} Hz, err ${perr.toFixed(2)} %  (gate: |err| > 5; audit -32.27)`);
  return { ok: ok && Math.abs(perr) > 5, lines };
});

// S13 — block-size independence. A host that hands the plugin 333 frames must
// get exactly what a host handing it 512 gets.
check('S13', 'block-size independence: {1,7,64,256,333} bit-identical to one call', () => {
  const ref = maxRender(StationCore, 0.5);
  const lines = []; let ok = true;
  for (const bs of [1, 7, 64, 256, 333]) {
    const got = maxRender(StationCore, 0.5, 1024, bs);
    let d = 0; for (let i = 0; i < ref.L.length; i++) d = Math.max(d, Math.abs(ref.L[i] - got.L[i]), Math.abs(ref.R[i] - got.R[i]));
    ok = ok && d === 0;
    lines.push(`  chunk ${String(bs).padStart(4)}  max|diff| ${d}  (gate: exactly 0)`);
  }
  // MUST-FAIL CONTROL: a deliberate per-CALL side effect must break the row.
  const K = planted(PLANT_PER_BLOCK);
  const a = (() => { const c = maxPatch(K); c.noteOn(60, noteFreq(60)); return pull(c, 24000).L; })();
  const b = (() => { const c = maxPatch(K); c.noteOn(60, noteFreq(60)); return pull(c, 24000, 64).L; })();
  let pd = 0; for (let i = 0; i < a.length; i++) pd = Math.max(pd, Math.abs(a[i] - b[i]));
  lines.push(`  control  planted per-call phase nudge -> max|diff| ${pd.toExponential(2)}  (gate: > 0)`);
  return { ok: ok && pd > 0, lines };
});

// S14 — silence. Exactly 0, not "small": anything else is a DC offset or an
// idling filter, and the shell mixes 16 of these.
check('S14', 'no voices -> exactly 0 for 2 s', () => {
  const c = maxPatch();
  const { L, R } = pull(c, 96000, 256);
  let nz = 0; for (let i = 0; i < L.length; i++) if (L[i] !== 0 || R[i] !== 0) nz++;
  const lines = [`  non-zero samples in 2 s with no voices: ${nz}  (gate: 0)`];
  // CONTROL: one gated voice must be non-zero for the whole window, or the row
  // is passing because the engine is broken rather than because it is quiet.
  const c2 = maxPatch(); c2.noteOn(60, noteFreq(60));
  const r2 = pull(c2, 96000, 256);
  let nz2 = 0; for (let i = 0; i < r2.L.length; i++) if (r2.L[i] !== 0) nz2++;
  lines.push(`  control  one gated voice -> ${nz2} non-zero samples  (gate: > 90000)`);
  return { ok: nz === 0 && nz2 > 90000, lines };
});

// S15 — denormals. Structural here (absolute envelope exits snap to 0 long
// before the subnormal range), so this row is a watch on that structure.
check('S15', 'no subnormal output samples, 6 s past note-off on the max patch', () => {
  const c = maxPatch();
  c.noteOn(60, noteFreq(60)); c.noteOn(67, noteFreq(67));
  pull(c, 48000, 256);
  c.noteOff(60); c.noteOff(67);
  const { L, R } = pull(c, 48000 * 6, 256);
  let sub = 0; for (let i = 0; i < L.length; i++) { if (isSubnormal(L[i])) sub++; if (isSubnormal(R[i])) sub++; }
  const lines = [`  subnormal samples in 6 s post-release: ${sub}  (gate: 0)`,
                 `  tail RMS ${rms(L).toExponential(3)}  peak ${peak(L).toExponential(3)}`];
  // CONTROL: the detector must be able to SEE a subnormal at all.
  const probe = [1e-40, 5e-39, 0, 1e-20].filter(isSubnormal).length;
  lines.push(`  control  detector on [1e-40, 5e-39, 0, 1e-20] finds ${probe}  (gate: exactly 2)`);
  return { ok: sub === 0 && probe === 2, lines };
});

// S16 — parameter steps. PINNED, NOT DEMANDED: the lab has no smoothing at all
// and SPEC-STATION §4 makes the 5 ms smoother the BUILD's job (audit S6). This
// row is a regression pin on today's numbers plus the four controls that must
// read the natural inter-sample step.
check('S16', 'parameter-write discontinuity (PIN — smoothing is the port\'s, SPEC §4)', () => {
  // MEASURED AS A RATIO, OVER 16 PHASES, BY MEDIAN. A single-shot |step| is a
  // function of where in the carrier's cycle the write lands — the audit says so
  // itself ("the single 0.0039 reading for the ALG recall is luck, not safety").
  // So each write is repeated at 16 settle lengths spanning one 261.6 Hz cycle
  // (183 samples) and reported as the MEDIAN of |step| / (the largest natural
  // inter-sample step in the cycle just before the write). That ratio is the
  // audit's own unit — its 13.9x / 11.1x / 7.4x / 6.3x and its 1.0x controls —
  // and unlike the raw |step| it is reproducible.
  const PIN = { 'matrix[O2->O1] 2.6->8.0': 13.9, 'OP1 wave SIN->SAW': 11.1, 'OP1 LVL 0.9->0.1': 7.4, 'MASTER 0.75->0.2': 6.3 };
  const CTRL = { 'OP1 PURE 1->0': 1.0, 'OP1 QNT OFF->4': 1.0, 'OP1 PAN 0->-1': 1.0, 'Wave RAM edit while SIN': 1.0 };
  const CYCLE = Math.round(48000 / noteFreq(60));          // 183 samples
  const median = a => { const b = [...a].sort((x, y) => x - y); return b[b.length >> 1]; };
  const step = (mutate, setup) => {
    const rs = [];
    for (let k = 0; k < 16; k++) {
      const c = new StationCore(48000); const s = c.state;
      if (setup) setup(s);
      c.noteOn(60, noteFreq(60));
      const a = pull(c, 4800 + Math.round(k * CYCLE / 16)).L;
      let nat = 0;
      for (let i = a.length - CYCLE; i < a.length; i++) nat = Math.max(nat, Math.abs(a[i] - a[i - 1]));
      mutate(s);
      const b = pull(c, 8).L;
      rs.push(Math.abs(b[0] - a[a.length - 1]) / nat);
    }
    return median(rs);
  };
  // The matrix row needs OP2 still SOUNDING as a modulator. In the shipped patch
  // OP2's envelope is d 180 ms / s 0, so 100 ms after note-on it has decayed to
  // ~0.08 and the cell write moves almost nothing (1.8x) — the write is real, the
  // modulator is not. A cell morph is specified for a sustaining modulator
  // (SPEC-STATION §9.2 calls the cells the primary morph surface), so that is the
  // condition measured, and it is stated rather than tuned into.
  const SUSTAIN_OP2 = s => { s.ops[1].env = { a: 2, d: 180, s: 1, r: 120, loop: 0 }; };
  const muts = {
    'matrix[O2->O1] 2.6->8.0': s => { s.matrix[1][0] = 8.0; },
    'OP1 wave SIN->SAW': s => { s.ops[0].wave = 2; },
    'OP1 LVL 0.9->0.1': s => { s.ops[0].lvl = 0.1; },
    'MASTER 0.75->0.2': s => { s.master = 0.2; },
    'OP1 PURE 1->0': s => { s.ops[0].pure = 0; },
    'OP1 QNT OFF->4': s => { s.ops[0].qnt = 4; },
    'OP1 PAN 0->-1': s => { s.ops[0].pan = -1; },
    'Wave RAM edit while SIN': s => { s.table[7] = (s.table[7] + 7) & 15; },
  };
  const lines = []; let ok = true;
  for (const [name, want] of Object.entries(PIN)) {
    const got = step(muts[name], name.startsWith('matrix') ? SUSTAIN_OP2 : null);
    ok = ok && got >= 3;                                   // a real discontinuity, several x the signal's own slope
    lines.push(`  ${name.padEnd(26)} ${got.toFixed(1)}x the natural step  (gate: >= 3x; audit ${want}x)` +
               (name.startsWith('matrix') ? '  [OP2 sustaining — see note]' : ''));
  }
  for (const [name, want] of Object.entries(CTRL)) {
    const got = step(muts[name]);
    ok = ok && got <= 2;                                   // must read the floor — the calibrated null
    lines.push(`  control ${name.padEnd(18)} ${got.toFixed(1)}x  (gate: <= 2x; audit ${want.toFixed(1)}x — these MUST read the floor)`);
  }
  return { ok, lines };
});

// S17 — DC at the engine output. A GATE since 2026-09-19, not a pin: the human
// ruled for a blocker and ruled it into the LAB rather than only the port, so
// reference/station.html now runs a one-pole highpass per channel on the
// level/pan sum, before the master gain and the monitoring tanh, and C++ parity
// covers the stage (SPEC-STATION §2 / §11.8; audit S8 / §2.7 is the defect).
//
// THE THRESHOLD IS THE DETECTOR'S OWN FLOOR PLUS MARGIN. A 1 s mean of a
// 261.63 Hz note is not a whole number of cycles, and that truncation residue
// reads as ~-61 dB of "DC" on configurations that have none — which is exactly
// what the two control rows measured before the blocker existed and still
// measure now. So the gate is not "zero DC", it is "every configuration sits at
// the floor the DC-free ones sit at".
//
// THE CONTROL IS THE DEFECT, PLANTED BACK (L0016/L0032). The identical probe on
// the pre-blocker lab must FAIL the three DC-prone rows by 20-50 dB, or the row
// is measuring something other than the blocker; and the two DC-free controls
// must sit at the floor in BOTH builds, or it is reading "blocker on" rather
// than "DC absent".
check('S17', 'DC at the engine output: the 5 Hz blocker holds every configuration at the detector floor', () => {
  const GATE_PEAK = -55, GATE_RMS = -50;   // dB; measured floor -61.4 / -56.6 (QTR) plus ~6 dB
  const cases = {
    'SIN, no feedback (control)': [0, s => { s.ops[0].wave = 0; s.ops[0].lvl = 1; }],
    'QTR raw (control)':          [0, s => { s.ops[0].wave = 4; s.ops[0].lvl = 1; s.ops[0].pure = 0; }],
    'self-feedback index 8':      [1, s => { s.ops[0].wave = 0; s.ops[0].lvl = 1; s.matrix[0][0] = 8; }],
    'NS SHORT':                   [1, s => { s.ops.forEach(o => { o.on = 0; o.lvl = 0; });
                                             s.noise = { on: 1, mode: 1, rate: 0.35, ktrk: 0, lvl: 1, pan: 0, env: { a: 1, d: 120, s: 1, r: 80, loop: 0 } }; }],
    'PLS raw, pw 0.1':            [1, s => { s.ops[0].wave = 3; s.ops[0].lvl = 1; s.ops[0].pure = 0; s.ops[0].pw = 0.1; }],
  };
  // Two seconds held, measured over the LAST one: the blocker's time constant is
  // 1/(2*pi*5) = 32 ms and its note-on transient is not what "DC offset" means.
  const dcOf = (K, mut) => {
    const c = blank(K); c.state.ops.forEach((o, i) => { o.on = i === 0; }); mut(c.state);
    c.noteOn(60, noteFreq(60));
    const W = pull(c, 48000 * 2).L.subarray(48000);
    const m = Math.abs(mean(W));
    return { p: dB(m) - dB(peak(W)), r: dB(m) - dB(rms(W)) };
  };
  const Pre = planted(PLANT_NO_DC);
  const lines = []; let ok = true;
  for (const [name, [prone, mut]] of Object.entries(cases)) {
    const a = dcOf(StationCore, mut), b = dcOf(Pre, mut);
    const pass = a.p <= GATE_PEAK && a.r <= GATE_RMS;
    // The control rows must read the floor in BOTH builds; the DC-prone ones must
    // read it in this build and BREACH it in the pre-blocker one.
    const ctl = prone ? (b.p > GATE_PEAK || b.r > GATE_RMS)
                      : (b.p <= GATE_PEAK && b.r <= GATE_RMS);
    ok = ok && pass && ctl;
    lines.push(`  ${name.padEnd(28)} DC/peak ${a.p.toFixed(1)}  DC/rms ${a.r.toFixed(1)} dB` +
               `   pre-blocker ${b.p.toFixed(1)} / ${b.r.toFixed(1)}` +
               `   (gate: <= ${GATE_PEAK} / ${GATE_RMS}${prone ? ', pre-blocker must breach' : ', control — floor in both'})`);
  }
  lines.push(`  (the two worst pre-blocker rows are STRUCTURAL, not bugs: a 10 % pulse is 80 % DC and a 93-step LFSR has`);
  lines.push(`   48 ones to 45 zeros — but both are envelope-multiplied at the source, so each note-on was a thump and 16 summed)`);
  return { ok, lines };
});

// S21 — the blocker's shape, MEASURED rather than asserted from its algebra: the
// lab rendered against the planted pre-blocker lab at one frequency at a time,
// which is the only reading that can tell "a 5 Hz highpass" from "5 Hz written in
// a comment". The row doubles as the must-read-zero control the DC rows need: at
// 1 kHz the same difference has to come out at zero, or the probe is reporting a
// constant offset and its -3 dB means nothing.
check('S21', 'DC blocker response: -3 dB at DC_FC, audio band untouched (vs. the planted pre-blocker lab)', () => {
  const Pre = planted(PLANT_NO_DC);
  // FIXED mode puts the operator at an exact frequency in Hz, so every test tone
  // is a whole number of cycles in the 1 s window and the RMS needs no window.
  const level = (K, f) => {
    const c = blank(K); c.state.ops.forEach((o, i) => { o.on = i === 0; });
    const o = c.state.ops[0]; o.wave = 0; o.lvl = 1; o.mode = 2; o.fixed = f;
    c.noteOn(60, noteFreq(60));
    return rms(pull(c, 48000 * 3).L.subarray(48000, 96000));   // second 1 s: past the 32 ms settle
  };
  const fc = LAB_K.DC_FC;
  const tones = [fc, 2 * fc, 5 * fc, 10 * fc, 20 * fc, 80 * fc, 200 * fc];
  const resp = tones.map(f => ({ f, d: dB(level(StationCore, f)) - dB(level(Pre, f)) }));
  const at = f => resp.find(r => r.f === f).d;
  const lines = resp.map(r => `  ${String(r.f).padStart(5)} Hz   ${r.d >= 0 ? '+' : ''}${r.d.toFixed(4)} dB`);
  const cut = Math.abs(at(fc) - (-3.0103)) <= 0.5;               // one-pole -3 dB point, +-0.5 dB
  const band = Math.abs(at(20 * fc)) <= 0.1 && Math.abs(at(80 * fc)) <= 0.1;
  const zero = Math.abs(at(200 * fc)) <= 0.01;
  lines.push(`  gate: ${fc} Hz within 0.5 dB of -3.01 (${cut ? 'ok' : 'FAIL'})   ${20 * fc} and ${80 * fc} Hz within 0.1 dB (${band ? 'ok' : 'FAIL'})`);
  lines.push(`  control  ${200 * fc} Hz must read ZERO to 0.01 dB (${zero ? 'ok' : 'FAIL'}) — a probe that cannot read zero cannot read -3`);
  lines.push(`  (DC_FC is read out of the lab, not restated here, so moving the lab's cutoff moves this row)`);
  return { ok: cut && band && zero, lines };
});

// S18 — an OFF op still runs its envelope. Was a PIN on the opposite behaviour
// until 2026-09-19: `if(!o.on){outs[i]=0;continue;}` skipped envStep, so an
// automated ON switch left a stale level that clicked on re-enable (audit S9,
// measured 0.32392/stage 1 held for 500 ms and a 1.645e-3 click). The lead
// ruled the LAB fixed rather than the defect preserved (ROADMAP B162), so this
// is now a GATE: OFF silences the op's output and its matrix contribution and
// nothing else. The Nyquist mute added by ADR-177 §3 always behaved this way,
// which is what its control proves.
check('S18', 'op ON->OFF->ON resumes at the LIVE envelope stage, not a stale one', () => {
  const run = (offFor) => {
    const c = blank(); const s = c.state;
    s.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; o.wave = 0;
      o.env = { a: 1, d: 400, s: 0, r: 60, loop: 0 }; });
    const v = c.noteOn(60, noteFreq(60));
    pull(c, Math.round(48000 * 0.05));
    const at50 = v.env[0].lvl;
    if (offFor) s.ops[0].on = 0;
    // 200 ms, not 500: the envelope is still MID-DECAY here, so the equality
    // below is a real bit-comparison of two moving values rather than 0 === 0.
    pull(c, Math.round(48000 * 0.2));
    return { at50, after: v.env[0].lvl, stage: v.env[0].stage };
  };
  const off = run(true), on = run(false);
  // MUST-FAIL CONTROL: `at50` is exactly what the frozen behaviour returned, so
  // a build that still freezes reads off.after === off.at50 and fails both
  // clauses at once — the detector cannot confuse the two answers.
  const live = off.after === on.after && Math.abs(off.after - off.at50) > 1e-3;
  const lines = [`  op switched OFF at 50 ms: env ${off.at50.toFixed(5)} -> ${off.after.toFixed(5)} after 200 ms, stage ${off.stage}  (gate: must have moved on)`,
                 `  control  op left ON:        env ${on.at50.toFixed(5)} -> ${on.after.toFixed(5)}, stage ${on.stage}  (gate: identical to the line above)`,
                 `  control  the FROZEN value the old lab returned: ${off.at50.toFixed(5)}  (gate: must NOT be what the OFF op reads)`];
  // The Nyquist mute (ADR-177 §3) must NOT freeze: an op pushed above Nyquist by
  // the pitch env has to come back with a current envelope, not a stale one.
  const c = blank(); const s = c.state;
  s.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; o.coarse = 200; o.env = { a: 1, d: 400, s: 0, r: 60, loop: 0 }; });
  const v = c.noteOn(60, noteFreq(60));
  pull(c, Math.round(48000 * 0.05)); const m50 = v.env[0].lvl;
  pull(c, Math.round(48000 * 0.5));
  const muteMoved = Math.abs(v.env[0].lvl - m50) > 1e-6;
  lines.push(`  control  op MUTED by the Nyquist limit: env ${m50.toFixed(5)} -> ${v.env[0].lvl.toFixed(5)}  (gate: must still decay — ADR-177 §3)`);
  return { ok: live && on.after < on.at50 - 1e-6 && muteMoved, lines };
});

// S19 — voice stealing. PINNED: the lab caps at 8 and hard-shifts, spec §8 wants
// 16 with a release fade (§11.4). The number is what the port has to beat.
check('S19', 'voice-steal discontinuity (PIN — lab is 8 + hard shift, spec §8 wants 16 + fade)', () => {
  const c = blank(); const s = c.state;
  s.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; o.wave = 0; o.env = { ...SUSTAIN }; });
  s.master = 0.02;
  for (let i = 0; i < 8; i++) c.noteOn(60 + i, noteFreq(60 + i));
  const a = pull(c, 24000).L;
  const before = a[a.length - 1], nat = Math.abs(a[a.length - 1] - a[a.length - 2]);
  c.noteOn(80, noteFreq(80));                                  // the 9th note steals voice 0
  const b = pull(c, 8).L;
  const stepv = Math.abs(b[0] - before);
  const lines = [`  steal step |d| ${stepv.toExponential(4)} on a peak of ${peak(a).toExponential(4)}  (${(stepv / peak(a) * 100).toFixed(1)} % of peak; pin: > 10 %)`,
                 `  natural inter-sample step at the same instant ${nat.toExponential(4)}  (ratio ${(stepv / nat).toFixed(1)}x)`];
  // CONTROL: a 9th note when only 4 voices are held steals nothing, so the same
  // probe must read the natural floor.
  const c2 = blank(); c2.state.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; o.env = { ...SUSTAIN }; });
  for (let i = 0; i < 4; i++) c2.noteOn(60 + i, noteFreq(60 + i));
  const a2 = pull(c2, 24000).L, nat2 = Math.abs(a2[a2.length - 1] - a2[a2.length - 2]);
  c2.noteOn(80, noteFreq(80));
  const s2 = Math.abs(pull(c2, 8).L[0] - a2[a2.length - 1]);
  lines.push(`  control  9th note with only 4 held (no steal) -> |d| ${s2.toExponential(4)} vs natural ${nat2.toExponential(4)}  (gate: <= 2x natural)`);
  return { ok: stepv / peak(a) > 0.10 && s2 <= 2 * nat2 + 1e-12, lines };
});

// S20 — cost. The ABSOLUTE ratio is machine-dependent and is REPORTED, never
// gated; what is gated is that cost scales LINEARLY in voices, because that is
// what makes a `measure_cpu`-style extrapolation on the ported core trustworthy
// (audit §3.4 framed this as "the budget is met iff the C++ core is >= 9.4x
// this"; SPEC §12's <= ~2 % was an ESTIMATE and was retired 2026-09-19 — the
// spec now carries phase 1's MEASURED 5.2 % standalone and re-sets the budget
// from the shell's measure_cpu at phase 2, so the ratio below is context, not a
// gate).
check('S20', 'cost: reported, with the linear-voice-scaling gate', () => {
  const secs = 1;
  // noteOn() enforces the lab's 8-voice cap (`voices.shift()`), so 16 noteOns
  // leave 8 voices sounding and the "16-voice" reading is the 8-voice one. The
  // cap is a browser expedient the port replaces with 16 + a release fade
  // (SPEC-STATION §8 / §11.4), so the cost of SPEC's polyphony is measured by
  // filling the array directly — as the audit did to reach 18.71 %.
  const cost = nv => {
    const N = 48000 * secs;
    let best = Infinity;
    for (let t = 0; t < 3; t++) {
      const cc = maxPatch();
      for (let i = 0; i < nv; i++) cc.voices.push(cc.mkVoice(48 + i, noteFreq(48 + i), i));
      const t0 = process.hrtime.bigint();
      pull(cc, N, 256);
      best = Math.min(best, Number(process.hrtime.bigint() - t0) / 1e9);
    }
    return best / secs * 100;
  };
  const c1 = cost(1), c8 = cost(8), c16 = cost(16);
  const lin8 = c8 / (c1 * 8), lin16 = c16 / (c1 * 16);
  const lines = [`  REPORT (this machine, Node ${process.version}): 1v ${c1.toFixed(2)} %  8v ${c8.toFixed(2)} %  16v ${c16.toFixed(2)} % of realtime`,
                 `  audit's reference reading was 1.35 / 9.42 / 18.71 % — the ported core measured 5.2 % of one core at 16 voices (SPEC §12, 2026-09-19)`,
                 `  linearity  8v/8x1v ${lin8.toFixed(2)}   16v/16x1v ${lin16.toFixed(2)}   agreement ${Math.abs(lin8 - lin16).toFixed(3)}`,
                 `  (gate: both in [0.4, 1.6] AND within 0.15 of each other — the constant < 1 is the fixed`,
                 `   per-sample overhead outside the voice loop, so what "linear" means is that the two agree)`];
  const ok = lin8 > 0.4 && lin8 < 1.6 && lin16 > 0.4 && lin16 < 1.6 && Math.abs(lin8 - lin16) <= 0.15;
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
  console.log(`anchor mechanism verified (bogus anchor throws)\nlab: reference/station.html\n`);
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
