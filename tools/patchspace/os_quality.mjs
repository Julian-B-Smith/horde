/*
 * os_quality.mjs — B445 / ADR-191: what the heavy class loses at os 1 against os 2, measured, and
 * the listening pairs the human hears before the device default flips. HYPERSAW, 2026-10-05,
 * ROADMAP B445 (the human: "Oversampling rec ratified. I had always intended it as optional", then
 * "B445 proposal ratified"); the lever is the B441 audit's D1 (os 2 -> 1 saves 43-45 %).
 * UNWIRED: an investigation run (seconds to minutes of JS DSP at up to 32x oversampling, writing
 * git-ignored WAVs); it prints, never judges a gate. B346's estimator it calls is held by
 * metrics_check.mjs's X rows, which ARE wired.
 *
 *   node tools/patchspace/os_quality.mjs conv  [--sr 44100] [--only <preset>]   one JSON line per preset x note x os
 *   node tools/patchspace/os_quality.mjs wav   [--sr 44100] [--out DIR]          the listening pairs (WAV, 24-bit)
 *
 * NOTHING HERE EDITS AN ENGINE FILE. The composed engine (docs/design/scalpel-horde-engine.js over the
 * protected reference/scalpel/prototype/razor-core.js and reference/swarmsaw.html) is required and
 * called. Presets are the bench's (reference/scalpel/data/presets.json), played as the parity
 * scenarios play them (tools/h2_scenarios.mjs presetCmds: the params, then gain 0.35, then the
 * smoothers snapped). No bench preset sets `os`, so `os` is written over each preset here: 1 and 2.
 *
 * WHY NOT alias_sources.mjs DIRECTLY. Its renders and decimation-gain model read space.mjs's SR, a
 * module constant fixed at 48 kHz (the SCALPEL lab's rate); B445 measures at 44.1 kHz, the CPU ledger's
 * reference rate (B441-2, the lead's E-6 ruling), where the band above 20 kHz is narrowest and folding
 * is worst. So the three rate-bound pieces (renderWith's capture, decimGain2, spec) are restated here
 * with the rate as a parameter, operation for operation, and the ESTIMATOR itself is not restated:
 * metrics.mjs aliasConvergence and spectrum are imported and given the rate.
 *
 * THE MEASUREMENT (B346's, metrics.mjs's header states it): the sound at os N, 2N, 4N, 8N, 16N, one
 * note held, the 8N render's pre-decimation stream captured; excess at N against {4N, 8N}, convergence
 * against {8N, 16N}, the source test. Two departures, both stated in every row:
 *   - THE WINDOW starts after the attack: B346 measured 0.05..0.3 s, which on Glass horde pad (A 900
 *     ms) and Breathing pad (A 1500 ms) is the attack ramp, not the sound. Here it is 12 000 samples
 *     (~0.27 s, the same length) from max(0.05 s, A + 0.05 s).
 *   - THE RATE is --sr (default 44.1 kHz); 48 kHz reproduces B346's rate.
 * Notes: B346's three (alias_sources.mjs NOTES: A1, E5, A5). A preset's one number is its WORST note.
 * The threshold the report names is gauntlet.mjs THRESH.aliasConvDb (-33.4 dB, B351's fit to the
 * human's ratings); it is imported, not copied.
 *
 * DETERMINISM: Math.random is a seeded mulberry32 around every instance (space.mjs's convention); no
 * clock is read. Same tree in, same numbers and same WAV bytes out.
 */
import '../labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { createRequire } from 'node:module';
import { readFileSync, mkdirSync, writeFileSync, existsSync } from 'node:fs';
import { join, resolve, relative, isAbsolute } from 'node:path';
import { gitIgnored } from '../labharness/sandbox_facts.mjs';   // git check-ignore, answered by the launcher
import { ROOT, mulberry32, FILES, readRepo } from './space.mjs';
import * as M from './metrics.mjs';
import { THRESH } from './gauntlet.mjs';

const require = createRequire(import.meta.url);
const RAZOR = require(join(ROOT, FILES.oracle));
const { makeComposedEngine, swarmSourceFromHtml } = require(join(ROOT, FILES.engine));
const PRESETS = JSON.parse(readFileSync(join(ROOT, 'reference/scalpel/data/presets.json'), 'utf8')).presets;
const mtof = n => 440 * Math.pow(2, (n - 69) / 12);

// The heavy class as B445 names it (the CPU ledger's heavy rows plus C3/C3b's --also picks).
export const HEAVY = ['Glass horde pad', 'Crushed bells', 'Breathing pad', 'Fold over sync', 'Crush vs FM', 'Sync over fold'];
export const NOTES = [['A1', 33], ['E5', 76], ['A5', 81]];   // alias_sources.mjs NOTES (B346)
const SEED = 0xB316;                                          // alias_sources.mjs renderWith's default seed
const WIN_LEN = 12000;                                        // B346's window length, in output samples

const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 && i + 1 < process.argv.length ? process.argv[i + 1] : d; };
const SR = Number(arg('--sr', 44100));

/* the capture (alias_sources.mjs `capture`, unchanged): RazorCore.render calls bqf(bqL[0]) then
   bqf(bqL[1]) on every INTERNAL sample when os > 1; x into the first stage is the pre-decimation stream */
const Engine = (C => class extends C {
  bqf(f, x) {
    const y = super.bqf(f, x), c = this._cap;
    if (c && c.i < c.n) {
      if (f === this.bqL[0]) c.preL[c.i] = x; else if (f === this.bqR[0]) c.preR[c.i] = x;
      else if (f === this.bqR[1]) c.i++;
    }
    return y;
  }
})(makeComposedEngine(RAZOR, swarmSourceFromHtml(readRepo(FILES.swarm))));

const presetOf = name => {
  const p = PRESETS.find(q => q.name === name);
  if (!p) throw new Error(`os_quality: no bench preset '${name}'`);
  return p;
};
/* one engine, the preset as the parity scenarios play it (presetCmds), `os` written over it.
   events: [[sample, 'on'|'off', note, vel]] in time order; n output samples. */
function render(preset, os, events, n, cap) {
  const saved = Math.random;
  Math.random = mulberry32(SEED);
  try {
    const c = new Engine(SR);
    for (const [k, v] of Object.entries(preset.params)) c.set({ [k]: v });
    c.set({ gain: 0.35 });
    c.set({ os });
    Object.assign(c.s, c.t);
    const L = new Float64Array(n), R = new Float64Array(n);
    if (cap && os > 1) c._cap = { n: n * os, i: 0, preL: new Float64Array(n * os), preR: new Float64Array(n * os) };
    let pos = 0;
    const upto = end => { while (pos < end) { const e = Math.min(end, pos + 128); c.render(L.subarray(pos, e), R.subarray(pos, e)); pos = e; } };
    for (const [t, kind, note, vel] of events) { upto(t); if (kind === 'on') c.noteOn(note, mtof(note), vel); else c.noteOff(note); }
    upto(n);
    return { L, R, cap: c._cap || null };
  } finally { Math.random = saved; }
}

/* alias_sources.mjs decimGain2 with the rate as a parameter: the engine's two RBJ low-passes at 0.45 sr,
   designed at sr*os, from RazorCore's own mk(); os 1 has none */
const G2 = new Map();
function decimGain2(os) {
  if (os <= 1) return () => 1;
  if (G2.has(os)) return G2.get(os);
  const fs = SR * os, fc = 0.45 * SR, bq = [RAZOR.prototype.mk(fs, fc, 0.5412), RAZOR.prototype.mk(fs, fc, 1.3066)];
  const g = f => { let p = 1; const w = 2 * Math.PI * f / fs, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
    for (const b of bq) { const nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -(b.b1 * s1 + b.b2 * s2), dr = 1 + b.a1 * c1 + b.a2 * c2, di = -(b.a1 * s1 + b.a2 * s2); p *= (nr * nr + ni * ni) / (dr * dr + di * di); } return p; };
  G2.set(os, g); return g;
}

/* alias_sources.mjs estimate()'s conv leg, at SR: one preset, one note, at os N */
function convergence(preset, note, N) {
  const a = Math.max(Math.round(0.05 * SR), Math.round(((preset.params.A || 0) / 1000 + 0.05) * SR)), b = a + WIN_LEN;
  const ev = [[0, 'on', note, 0.8]], S = {}, names = ['N', 'N2', 'N4', 'N8', 'N16'];
  let Sint = null;
  [1, 2, 4, 8, 16].forEach((m, q) => {
    const r = render(preset, N * m, ev, b, m === 8);
    S[names[q]] = M.spectrum(M.mono(r.L.subarray(a, b), r.R.subarray(a, b)), SR, 8192);
    if (m === 8) {
      const k = N * 8, c = r.cap, pre = new Float64Array((b - a) * k);
      for (let i = 0; i < pre.length; i++) pre[i] = 0.5 * (c.preL[a * k + i] + c.preR[a * k + i]);
      Sint = M.spectrum(pre, SR * k, 8192 * k);
    }
  });
  return Object.assign(M.aliasConvergence(S, Sint, N, SR, decimGain2), { win: [a, b] });
}

/* MUST-READ CONTROLS, at this rate, every run (the detector must be able to say both answers here, not
   only at metrics_check's 48 kHz): blades off on a sine base must read CLEAN at os 1 (nothing above the
   output Nyquist to fold), and blades off on a raw saw base (Band-limit off) must read OVER the threshold
   at os 1 at E5 (a naive saw folds). Either failing voids the table: the run exits 1. */
const CONTROLS = [
  { name: 'control: sine base, blades off', must: 'clean', params: { w: 0, b2on: 0, base: 0, N: 1 } },
  { name: 'control: raw saw base, blades off', must: 'over', params: { w: 0, b2on: 0, base: 2, aa: 0, N: 1 } },
];
const r6 = x => (x === null ? null : Math.round(x * 1e6) / 1e6);
function conv() {
  const only = arg('--only', null);
  let bad = 0;
  for (const ctl of CONTROLS) {
    const c = convergence({ params: ctl.params }, 76, 1);
    const ok = ctl.must === 'clean' ? c.cls === 'clean' : c.excessDb > THRESH.aliasConvDb;
    if (!ok) bad++;
    console.log(JSON.stringify({ B445: 'control', preset: ctl.name, note: 'E5', os: 1, sr: SR, excessDb: r6(c.excessDb), cls: c.cls, must: ctl.must, ok }));
  }
  for (const name of HEAVY) {
    if (only && only !== name) continue;
    const p = presetOf(name);
    for (const [nn, note] of NOTES) for (const os of [1, 2]) {
      const c = convergence(p, note, os);
      console.log(JSON.stringify({ B445: 'conv', preset: name, note: nn, os, sr: SR, win_s: c.win.map(x => r6(x / SR)),
        excessDb: r6(c.excessDb), foldDb: r6(c.foldDb), dynDb: r6(c.dynDb), convDb: r6(c.convDb), cls: c.cls,
        over: c.excessDb > THRESH.aliasConvDb, thresh: THRESH.aliasConvDb }));
    }
  }
  if (bad) { console.error(`os_quality: ${bad} must-read control(s) failed; the table above is void`); process.exit(1); }
}

/* ---------------------------------------------------------------- the listening pairs */
/* one phrase per preset, the same notes and seed at both os: the preset's chord in its own register
   (h2_scenarios.mjs notesFor's table), held, released, then a high line (E5, A5, E6) where folding is
   loudest. Every hold outlasts the attack, every gap the release, so each note is heard settled. */
const REGISTER = { 'Growls': [33], 'FM sines': [60, 64, 67], 'Movement': [48, 55], 'Leads': [62], 'Pads': [48, 55, 60, 64], 'Interplay': [45], 'Oddities': [52, 59] };
const registerOf = cat => { for (const k in REGISTER) if (cat.endsWith(k)) return REGISTER[k]; return [45]; };
export function phrase(preset) {
  const A = (preset.params.A || 0) / 1000, Rl = (preset.params.R || 300) / 1000, s = x => Math.round(x * SR);
  const root = registerOf(preset.category), chord = root.length >= 2 ? root : [root[0], root[0] + 7, root[0] + 12];
  const ev = [], hold = Math.max(1.5, A + 1.2), step = Math.max(0.6, A + 0.6);
  let t = 0;
  for (const n of chord) ev.push([s(t), 'on', n, 0.85]);
  t += hold; for (const n of chord) ev.push([s(t), 'off', n, 0]);
  t += Math.min(Rl, 2) + 0.3;
  for (const n of [76, 81, 88]) { ev.push([s(t), 'on', n, 0.85]); t += step; ev.push([s(t), 'off', n, 0]); t += 0.15; }
  t += Math.min(Rl, 3) + 0.2;
  return { ev, n: s(t), chord };
}
function wav24(path, L, R, scale) {
  const n = L.length, data = Buffer.alloc(n * 6), hdr = Buffer.alloc(44);
  for (let i = 0; i < n; i++) for (const [j, x] of [[0, L[i]], [1, R[i]]]) {
    const v = Math.max(-8388608, Math.min(8388607, Math.round(x * scale * 8388607)));
    data.writeIntLE(v, i * 6 + j * 3, 3);
  }
  hdr.write('RIFF', 0); hdr.writeUInt32LE(36 + data.length, 4); hdr.write('WAVE', 8); hdr.write('fmt ', 12);
  hdr.writeUInt32LE(16, 16); hdr.writeUInt16LE(1, 20); hdr.writeUInt16LE(2, 22); hdr.writeUInt32LE(SR, 24);
  hdr.writeUInt32LE(SR * 6, 28); hdr.writeUInt16LE(6, 32); hdr.writeUInt16LE(24, 34); hdr.write('data', 36); hdr.writeUInt32LE(data.length, 40);
  writeFileSync(path, Buffer.concat([hdr, data]));
}
const peakOf = (L, R) => { let p = 0; for (let i = 0; i < L.length; i++) p = Math.max(p, Math.abs(L[i]), Math.abs(R[i])); return p; };
const rmsDb = (L, R) => { let s = 0; for (let i = 0; i < L.length; i++) s += L[i] * L[i] + R[i] * R[i]; return 10 * Math.log10(s / (2 * L.length) + 1e-30); };
function wav() {
  const out = resolve(arg('--out', join(ROOT, 'local', 'listening', 'b445')));
  /* the renders are the human's listening material, never repo content: refuse a directory inside this
     repo that git would track (port_legacy_presets.mjs's rule, safety by construction) */
  const rel = relative(ROOT, out);
  if (!rel.startsWith('..') && !isAbsolute(rel)) {
    const ignored = gitIgnored(ROOT, rel);
    if (!ignored) throw new Error(`os_quality: --out ${rel} is inside the repo and not git-ignored`);
  }
  if (!existsSync(out)) mkdirSync(out, { recursive: true });
  const TARGET = Math.pow(10, -1 / 20);                       // every file peaks at -1 dBFS (peak-matched)
  HEAVY.forEach((name, i) => {
    const p = presetOf(name), ph = phrase(p), slug = String(i + 1).padStart(2, '0') + '-' + name.toLowerCase().replace(/[^a-z0-9]+/g, '-');
    const r = {}; for (const os of [1, 2]) r[os] = render(p, os, ph.ev, ph.n, false);
    const row = { B445: 'wav', preset: name, sr: SR, seconds: Math.round(ph.n / SR * 100) / 100, chord: ph.chord, line: [76, 81, 88] };
    for (const os of [1, 2]) {
      const pk = peakOf(r[os].L, r[os].R), file = `${slug}-os${os}.wav`;
      wav24(join(out, file), r[os].L, r[os].R, TARGET / pk);
      row['os' + os] = { file, peak: Math.round(pk * 1e6) / 1e6, rmsDbAfterMatch: Math.round((rmsDb(r[os].L, r[os].R) + 20 * Math.log10(TARGET / pk)) * 100) / 100 };
    }
    console.log(JSON.stringify(row));
  });
}

const mode = process.argv[2];
if (mode === 'conv') conv();
else if (mode === 'wav') wav();
else { console.error('usage: node tools/patchspace/os_quality.mjs conv|wav [--sr 44100] [--only <preset>] [--out DIR]'); process.exit(2); }
