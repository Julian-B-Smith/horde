/*
 * gen_station_goldens.mjs — STATION parity goldens for tools/station_check.cpp.
 *
 * Slices `StationCore` live out of reference/station.html with extract_core.mjs
 * (ADR-003: the prototype IS the spec; a copied-out core silently diverges the
 * first time the reference moves — LIBRARY L0001), renders a scenario set at
 * BOTH 48 000 and 44 100 Hz, and writes raw interleaved f32 stereo plus a
 * manifest into build-golden/station/.
 *
 * ── THE MONITOR STAGE IS INVERTED, NOT REPLICATED ───────────────────────────
 * SPEC-STATION §11.5: the lab's master `tanh(ml * master * 1.4)` is monitoring
 * convenience and the engine output is clean, so the C++ core does not have it.
 * Every render here therefore sets `master = 0.02` — where the audit measured
 * the tanh at -102.3 dB THD (§4.3) — and then UNDOES it exactly:
 *
 *     engine = atanh(y) / (0.02 * 1.4)
 *
 * atanh is the exact inverse of tanh, so this is not a linearisation: the ×1.4
 * is accounted for to the last bit and the golden is at FULL ENGINE AMPLITUDE.
 * That matters for the gate, not just for tidiness — comparing at master 0.02
 * instead would shrink every error by 0.028 and hand the eps=1e-6 budget a 36×
 * discount it did not earn. The render buffers are Float64Array so the lab's
 * `L[n]=l` never rounds to f32 before the inversion; the single f32 rounding is
 * the golden file itself (6e-8 relative, ~6 % of the budget).
 * `master = 0.02` is also small enough that atanh stays well-conditioned; the
 * generator asserts max|y| stays inside the safe band rather than assuming it.
 *
 * ── THE DC-BLOCKER FLAG IS MEASURED, NOT ASSUMED ────────────────────────────
 * A one-pole DC blocker on the engine output is landing in the lab separately
 * (the human's 2026-09-19 ruling). Rather than grep for a variable name that
 * may not exist yet — an anchor that rots silently — this generator DETECTS the
 * blocker behaviourally: it renders a 10 %-duty raw pulse, which is 80 % DC by
 * construction, and reads whether the DC survives. The answer goes into the
 * manifest as `@dcblock=0|1` and station_check configures its core to match.
 * So when the lab's blocker merges, one `node gen_station_goldens.mjs` run
 * flips the flag and nothing else needs touching. (Goldens are generated
 * artifacts under build-golden/, which .gitignore excludes, so there is no
 * committed set to go stale.)
 *
 * ── WHAT IS DELIBERATELY NOT HERE ───────────────────────────────────────────
 * No DRW scenario with `pure > 0`: SPEC-STATION §11.1 makes the port's DRW pure
 * branch band-limited where the lab linearly interpolates, so parity there
 * would certify the thing the spec says to replace. The DRW RAW branch is
 * parity-gated (scenario `drw-raw`) and DRW pure is covered behaviourally in
 * station_check (its alias floor must beat the lab's -43.7 dB at MIDI 96).
 * Every OTHER waveform's pure branch IS parity-gated (`pure-sawpls`).
 * Likewise no scenario holds more than 8 voices: the lab caps there
 * (`voices.shift()`), and 16-voice release-fade stealing is §11.4's divergence,
 * gated behaviourally.
 *
 * ── THE MANIFEST IS THE SINGLE SOURCE OF TRUTH ──────────────────────────────
 * Each row dumps the WHOLE resulting lab state as flat key=value tokens, not a
 * scenario name the C++ mirrors. A mirrored scenario table in two languages
 * drifts; a dumped state cannot. station_check applies the tokens to its own
 * Patch and an unknown key is a hard failure there.
 *
 * Usage: node gen_station_goldens.mjs [--selfcheck]
 */

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { extractCore } from './extract_core.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const LAB = join(root, 'reference/station.html');
const StationCore = extractCore(LAB, 'StationCore', 'reference');

const MASTER = 0.02;          // audit §4.3: tanh is -102.3 dB THD here
const MONITOR = MASTER * 1.4; // the lab's `Math.tanh(ml * g * 1.4)`
const BLOCK = 128;
const RATES = [48000, 44100];
const noteFreq = n => 440 * Math.pow(2, (n - 69) / 12);

// ------------------------------------------------------------------ patches --
// Each `p` receives (state, core) and mutates the lab's own boot patch. Keeping
// the boot patch as the base is deliberate: it is SPEC §10's stated default, so
// every scenario is a named departure from it rather than a fresh invention.
const rndTable = c => { c.reseedTable(); c.state.table = c.state.table.map((_, i) => c.TP.RND(i)); };
const CHORD8 = [48, 52, 55, 59, 60, 64, 67, 71];

// The six §4 algorithm presets, copied from the lab's ALGS table (:516-523).
// They live OUTSIDE the extractable DSP span (they are UI patch data), so they
// are transcribed here and the transcription is what the parity gate proves.
const ALGS = {
  'stack':    { m: [],                                  lv: [0.8, 0.8, 0.8] },
  '2to1':     { m: [[1, 0, 2.6]],                       lv: [0.9, 0, 0] },
  '3to2to1':  { m: [[2, 1, 2.4], [1, 0, 2.8]],          lv: [0.9, 0, 0] },
  '2p3to1':   { m: [[1, 0, 2.4], [2, 0, 2.4]],          lv: [0.9, 0, 0] },
  '3to1p2':   { m: [[2, 0, 2.4], [2, 1, 2.4]],          lv: [0.75, 0.75, 0] },
  'fbch':     { m: [[2, 2, 1.4], [2, 1, 2.4], [1, 0, 2.8]], lv: [0.9, 0, 0] },
};
const algPatch = name => s => {
  const a = ALGS[name];
  s.matrix = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]];
  a.m.forEach(([r, c, v]) => s.matrix[r][c] = v);
  a.lv.forEach((v, i) => s.ops[i].lvl = v);
};

export const SCENARIOS = [
  // The default patch on a mid note — the one scenario that certifies §10's
  // "default patch = prototype boot patch" with nothing else moving.
  { name: 'default', notes: [60], secs: 1.5, p: () => {} },

  ...Object.keys(ALGS).map(k => ({ name: `alg-${k}`, notes: CHORD8, secs: 1.0, p: algPatch(k) })),

  // Noise as a PM SOURCE (§6: "a first-class texture, not an afterthought").
  // NS level 0 so the gate reads the modulation, not the noise bed.
  { name: 'noise-pm', notes: [45, 57], secs: 1.0, p: s => {
      s.matrix = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [6, 0, 0]];
      s.ops[0].lvl = 0.9; s.ops[1].on = 0; s.ops[2].on = 0;
      s.noise = { on: 1, mode: 0, rate: 0.35, ktrk: 1, lvl: 0, pan: 0,
                  env: { a: 1, d: 400, s: 1, r: 80, loop: 0 } };
    } },

  // Self-feedback (the matrix diagonal). THE INDEX IS 2, NOT 8, AND THAT IS A
  // MEASUREMENT, NOT TIMIDITY — the audit's §4.1 claim that bit-parity is
  // available "for the whole DSP" is FALSE on this diagonal above index ~2.
  // `out = sin(2pi*ph + cell*prev)` has d(out)/d(prev) = cos(.)*2pi*cell*0.1591549
  // ~= cell*cos(.), so the map is contracting below index 1 and CHAOTIC above it,
  // and V8's Math.sin and libm's differ by ~1 ulp somewhere in range. Measured
  // here, this machine, 2026-09-19, RMS vs the lab at 48 k / 44.1 k:
  //     index 0.9  0.000e+00 / 0.000e+00      index 2    1.257e-07 / 2.408e-08
  //     index 1.0  0.000e+00 / 0.000e+00      index 4    2.091e-01 / 2.084e-01
  //     index 1.2  1.585e-07 / 3.232e-08      index 8    3.085e-01 / 3.031e-01
  // Index 2 is the loudest self-feedback that still clears eps = 1e-6 with ~8x
  // of margin. Index 8 is not a parity question at all and must never be made
  // one: it is gated BEHAVIOURALLY in station_check (bounded, finite, |op| <= 1
  // over 10 s on all three diagonals), which is the only claim about a chaotic
  // map that two implementations can both honour.
  { name: 'selffb', notes: [45], secs: 1.0, p: s => {
      s.matrix = [[2, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]];
      s.ops[0].lvl = 0.9; s.ops[1].on = 0; s.ops[2].on = 0;
      s.ops[0].env = { a: 2, d: 800, s: 1, r: 200, loop: 0 };
    } },

  // DRW on the RAW branch with a non-default Wave RAM, plus phase quantisation.
  { name: 'drw-raw', notes: [48, 60], secs: 1.0, p: (s, c) => {
      s.seed = 1024; rndTable(c);
      s.ops.forEach((o, i) => { o.on = 1; o.wave = 5; o.pure = 0; o.qnt = 16;
                                o.lvl = i === 0 ? 0.8 : 0; });
      s.matrix[1][0] = 3.2; s.matrix[2][0] = 1.6;
    } },

  // Pitch envelope: +24 st decaying over 800 ms, the curve §7 names.
  { name: 'pitchenv', notes: [36, 43], secs: 1.0, p: s => {
      s.pitchEnv = { amt: 24, dec: 800 };
      s.ops[0].lvl = 0.9;
      s.ops[0].env = { a: 2, d: 600, s: 0.4, r: 200, loop: 0 };
    } },

  // SHORT noise (93-step, pitched) with KEYTRK — the metallic/melodic case.
  { name: 'noise-short', notes: [52, 64, 67], secs: 1.0, p: s => {
      s.ops.forEach(o => { o.on = 0; o.lvl = 0; });
      s.noise = { on: 1, mode: 1, rate: 0.8, ktrk: 1, lvl: 0.7, pan: 0.3,
                  env: { a: 1, d: 300, s: 0.6, r: 80, loop: 0 } };
    } },

  // The PURE branch where §11.1's divergence does NOT apply: polyBLEP SAW and
  // PLS are the lab's own band-limiting and the port keeps them bit-for-bit.
  // High note on purpose — MIDI 84 is where the BLEP correction is largest.
  { name: 'pure-sawpls', notes: [84], secs: 1.0, p: s => {
      s.ops[0].wave = 2; s.ops[0].pure = 1; s.ops[0].lvl = 0.8;
      s.ops[1].wave = 3; s.ops[1].pure = 1; s.ops[1].pw = 0.17; s.ops[1].coarse = 1.5;
      s.ops[1].lvl = 0.6; s.ops[1].env = { a: 4, d: 500, s: 0.7, r: 150, loop: 0 };
      s.ops[2].on = 0;
      s.matrix = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]];
    } },

  // Release + voice death: four notes released at 0.5 s, rendered well past the
  // tail. This is the only scenario whose voices leave the array, which is why
  // every note-on in every scenario happens BEFORE the first sample (a note-on
  // after a death would take a different slot in the lab's compacting array
  // than in the port's fixed one, and the LFSR seeds would part company).
  { name: 'release', notes: [50, 57, 62, 69], secs: 1.5, offAt: 0.5, p: s => {
      s.ops[0].lvl = 0.85;
      s.ops[0].env = { a: 5, d: 300, s: 0.5, r: 300, loop: 0 };
    } },

  // Hard sync + QTR + a coarse phase-quantised pulse: the three chip-authentic
  // shapes in one patch, including the QNT ordering signature (audit §2.1).
  { name: 'sync-qtr', notes: [55], secs: 1.0, p: s => {
      s.ops[0].wave = 2; s.ops[0].lvl = 0.6;
      s.ops[1].wave = 4; s.ops[1].coarse = 3.5; s.ops[1].sync = 1; s.ops[1].lvl = 0.7;
      s.ops[1].pure = 0; s.ops[1].env = { a: 2, d: 500, s: 0.8, r: 120, loop: 1 };
      s.ops[2].wave = 3; s.ops[2].coarse = 5; s.ops[2].sync = 1; s.ops[2].qnt = 32;
      s.ops[2].pure = 0; s.ops[2].pw = 0.3; s.ops[2].lvl = 0.5;
      s.matrix = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]];
    } },
];

// ------------------------------------------------------------------- render --
function build(sc, sr) {
  const c = new StationCore(sr);
  sc.p(c.state, c);
  for (const n of sc.notes) c.noteOn(n, noteFreq(n));
  c.state.master = MASTER;
  return c;
}

// Renders in BLOCK chunks with a note-off split, into Float64Array so the lab's
// store never rounds to f32 before the atanh inversion.
function renderScenario(sc, sr) {
  const c = build(sc, sr);
  const total = Math.round(sr * sc.secs);
  const offSample = sc.offAt === undefined ? -1 : Math.round(sr * sc.offAt);
  const out = new Float32Array(total * 2);
  const L = new Float64Array(BLOCK), R = new Float64Array(BLOCK);
  let peakY = 0;
  for (let off = 0; off < total; ) {
    let k = Math.min(BLOCK, total - off);
    if (offSample > off && offSample < off + k) k = offSample - off;   // split exactly on the note-off
    if (off === offSample) for (const n of sc.notes) c.noteOff(n);
    c.render(L, R, k);
    for (let i = 0; i < k; i++) {
      peakY = Math.max(peakY, Math.abs(L[i]), Math.abs(R[i]));
      out[(off + i) * 2] = Math.atanh(L[i]) / MONITOR;
      out[(off + i) * 2 + 1] = Math.atanh(R[i]) / MONITOR;
    }
    off += k;
  }
  // atanh loses conditioning as |y| -> 1. 0.25 is ~5x the loudest thing these
  // scenarios produce and still keeps d(atanh)/dy under 1.07, so a breach means
  // a scenario got much louder than intended, not that the bound is tight.
  if (!(peakY < 0.25)) throw new Error(`${sc.name} @${sr}: monitor peak ${peakY} — atanh inversion is out of its safe band`);
  return out;
}

// ----------------------------------------------------------- state dumping --
// The whole resulting lab state, flat. station_check applies these to its own
// Patch struct and rejects an unknown key, so a lab field this forgets is a
// loud failure on the next run rather than a silent parity hole.
const num = v => Number(v).toPrecision(17);
function dumpState(s) {
  const t = [];
  s.ops.forEach((o, i) => {
    for (const k of ['on', 'wave', 'mode', 'coarse', 'fine', 'semis', 'fixed', 'lvl', 'pan', 'pw', 'pure', 'qnt', 'sync'])
      t.push(`ops.${i}.${k}=${num(o[k])}`);
    for (const k of ['a', 'd', 's', 'r', 'loop']) t.push(`ops.${i}.env.${k}=${num(o.env[k])}`);
  });
  for (const k of ['on', 'mode', 'rate', 'ktrk', 'lvl', 'pan']) t.push(`noise.${k}=${num(s.noise[k])}`);
  for (const k of ['a', 'd', 's', 'r', 'loop']) t.push(`noise.env.${k}=${num(s.noise.env[k])}`);
  for (let r = 0; r < 4; r++) for (let c = 0; c < 3; c++) t.push(`matrix.${r}.${c}=${num(s.matrix[r][c])}`);
  t.push(`pitchEnv.amt=${num(s.pitchEnv.amt)}`, `pitchEnv.dec=${num(s.pitchEnv.dec)}`);
  for (let i = 0; i < 32; i++) t.push(`table.${i}=${num(s.table[i])}`);
  t.push(`seed=${num(s.seed)}`);
  return t;
}

// ------------------------------------------------- DC-blocker detection (§) --
// Behavioural, not textual: a 10 %-duty raw pulse is 80 % DC by construction
// (audit §2.7 measured it at -1.9 dB below peak). If that DC survives a second
// of render the lab has no blocker; if it is gone, it has one. There is no
// middle ground to misread — the two answers are ~0.8 and ~0.
function labHasDcBlocker() {
  const c = new StationCore(48000), s = c.state;
  s.master = MASTER;
  s.matrix = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]];
  s.ops.forEach((o, i) => { o.on = i === 0; o.lvl = i === 0 ? 1 : 0; });
  s.ops[0].wave = 3; s.ops[0].pure = 0; s.ops[0].pw = 0.1;
  s.ops[0].env = { a: 1, d: 10, s: 1, r: 60, loop: 0 };
  s.noise.on = 0;
  c.noteOn(60, noteFreq(60));
  const L = new Float64Array(48000), R = new Float64Array(48000);
  c.render(L, R, 48000);
  let sum = 0, pk = 0;
  for (let i = 24000; i < 48000; i++) { sum += L[i]; pk = Math.max(pk, Math.abs(L[i])); }
  const ratio = Math.abs(sum / 24000) / Math.max(pk, 1e-300);
  if (ratio > 0.4) return { has: 0, ratio };
  if (ratio < 0.01) return { has: 1, ratio };
  throw new Error(`DC-blocker detection is ambiguous (|DC|/peak = ${ratio}); the probe's two answers are ~0.8 and ~0 — read the lab before trusting this manifest`);
}

// --------------------------------------------------------------------- main --
const outDir = join(root, 'build-golden', 'station');
const selfcheck = process.argv.includes('--selfcheck');
if (!selfcheck) mkdirSync(outDir, { recursive: true });

const dc = labHasDcBlocker();
let fail = 0;
const manifest = [];
for (const sr of RATES)
  for (const sc of SCENARIOS) {
    const name = `${sc.name}-${sr / 100}`;
    const audio = renderScenario(sc, sr);
    if (selfcheck) {
      const again = renderScenario(sc, sr);
      let same = again.length === audio.length;
      if (same) for (let i = 0; i < audio.length; i++) if (audio[i] !== again[i]) { same = false; break; }
      console.log(`${same ? 'OK  ' : 'FAIL'} ${name}`);
      if (!same) fail++;
      continue;
    }
    writeFileSync(join(outDir, `${name}.f32`), Buffer.from(audio.buffer));
    const meta = [`@sr=${sr}`, `@secs=${num(sc.secs)}`, `@block=${BLOCK}`, `@dcblock=${dc.has}`,
                  ...sc.notes.map(n => `@note=${n}:${num(noteFreq(n))}`)];
    if (sc.offAt !== undefined) meta.push(`@off=${Math.round(sr * sc.offAt)}`);
    manifest.push([name, [...meta, ...dumpState(build(sc, sr).state)].join(' ')].join('\t'));
    console.log(`wrote ${name}.f32`);
  }

if (selfcheck) {
  console.log(`selfcheck: lab DC blocker ${dc.has ? 'PRESENT' : 'ABSENT'} (|DC|/peak on a 10 % pulse = ${dc.ratio.toExponential(3)})`);
  console.log(fail ? `selfcheck: ${fail} NON-DETERMINISTIC` : 'selfcheck: renders are deterministic');
  process.exit(fail ? 1 : 0);
}
const labHash = createHash('sha256').update(readFileSync(LAB)).digest('hex').slice(0, 16);
writeFileSync(join(outDir, 'station-manifest.tsv'),
  `#lab\treference/station.html\tsha256:${labHash}\tdcblock=${dc.has}\n` + manifest.join('\n') + '\n');
console.log(`lab sha256:${labHash}  DC blocker ${dc.has ? 'PRESENT' : 'ABSENT'} (|DC|/peak ${dc.ratio.toExponential(3)})`);
