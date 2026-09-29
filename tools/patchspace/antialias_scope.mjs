/*
 * antialias_scope.mjs — what ADR-189 D3 (the feedback-loop filter, `aaLoop`) does to the bench presets it
 * reaches: how much the sound changes, and how much of that change is aliasing removed rather than timbre.
 * HYPERSAW, 2026-09-29, ROADMAP B355 rework (ADR-189 A1, records PR #857, branch lead-records-143). The
 * lead, after the critic: "Re-measure the 14 feedback presets' audible change ... Split the change into
 * aliasing removed versus in-band timbre change ... COMMIT the script, or its output table."
 * UNWIRED: a measurement run (~2 min of DSP), not a gate; its committed output is
 * docs/patchspace/2026-09-29-b355-d3-scope.md, and it reproduces that file byte for byte.
 *
 *   node tools/patchspace/antialias_scope.mjs            (prints the markdown; --write writes the file)
 *
 * THE PRESETS: every bench preset (reference/scalpel/data/presets.json) with feedback or cross-mod on
 * (fb or xm > 0.0005), the loop D3 filters; every other preset is bit-identical under D3 (the ledger's L6
 * checks two of them). Each is rendered as the lab renders it: its own params, os 2 (the engine default),
 * one note at velocity 0.8, Math.random seeded (mulberry32(7)), D3 off and on.
 *
 * PART 1, THE AUDIBLE CHANGE (the critic's measure): a 1 s held note at A3 and E5; 1/3-octave band levels
 * 50 Hz..16 kHz over 0.1..1.0 s; over the bands within 40 dB of the loudest band (in either render), the
 * largest band change D3 on minus off, and the change of RMS level.
 * PART 2, ALIASING REMOVED AGAINST TIMBRE CHANGED, with B346's estimator (alias_sources.mjs estimate(),
 * the 0.3 s note, window 0.05..0.3 s):
 *   - aliasing: the estimator's total (everything the render has that its oversampled TRUTH lacks: the
 *     same sound at 8x and 16x its os, the output stage done right), D3 off and D3 on, each against its
 *     OWN truth. The fall is aliasing removed.
 *   - timbre: the same 1/3-octave band rule applied to the two TRUTHS (D3 off's truth against D3 on's):
 *     what D3 changes in the sound itself, with the aliasing taken out on both sides.
 * Deterministic, no clock, no model calls. Both references are required, never edited.
 */
import { readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, SR, mtof } from './space.mjs';
import * as M from './metrics.mjs';
import * as A from './alias_sources.mjs';

export const OUTFILE = 'docs/patchspace/2026-09-29-b355-d3-scope.md';
const PRESETS = JSON.parse(readFileSync(join(ROOT, 'reference/scalpel/data/presets.json'), 'utf8')).presets;
export const LOOP = PRESETS.filter(p => (p.params.fb || 0) > 0.0005 || (p.params.xm || 0) > 0.0005);
const C = A.baseEngine(), SEED = 7, NOTES = [['A3', 57], ['E5', 76]];

/* 1/3-octave band powers (dB) of a spectrum, centres 50 Hz .. 16 kHz (the last band reaches 18 kHz) */
function bands(S) {
  const out = [];
  for (let c = 50; c < 16500; c *= Math.pow(2, 1 / 3)) {        // centres 50 .. 16.1 kHz (26 bands)
    const lo = c / Math.pow(2, 1 / 6), hi = c * Math.pow(2, 1 / 6); let p = 0;
    for (let k = 1; k < S.P.length; k++) { const f = k * S.binHz; if (f >= lo && f < hi) p += S.P[k]; }
    out.push([c, 10 * Math.log10(Math.max(p, 1e-30))]);
  }
  return out;
}
/* the largest band change over the bands within 40 dB of the loudest (in either), and where */
function worstBand(b0, b1) {
  const top = Math.max(...b0.map(x => x[1]), ...b1.map(x => x[1]));
  let w = 0, at = 0;
  for (let i = 0; i < b0.length; i++) if (b0[i][1] > top - 40 || b1[i][1] > top - 40) { const d = b1[i][1] - b0[i][1]; if (Math.abs(d) > Math.abs(w)) { w = d; at = b0[i][0]; } }
  return { db: w, at };
}
const rmsDb = (L, R, a, b) => { let e = 0; for (let i = a; i < b; i++) e += 0.5 * (L[i] * L[i] + R[i] * R[i]); return 10 * Math.log10(e / (b - a) + 1e-30); };

export function measure(preset) {
  const p0 = preset.params, p1 = Object.assign({}, p0, { aaLoop: 1 }), N = p0.os || 2, row = { name: preset.name, fb: p0.fb || 0, xm: p0.xm || 0 };
  for (const [nn, note] of NOTES) {
    /* part 1: 1 s held */
    const a = 4800, b = 48000, script = { n: b, ev: [[0, 'on', note, 0.8]] };
    const r0 = A.renderWith(C, p0, script, { seed: SEED, raw: true }), r1 = A.renderWith(C, p1, script, { seed: SEED, raw: true });
    const s0 = M.spectrum(M.mono(r0.L.subarray(a, b), r0.R.subarray(a, b)), SR, 8192), s1 = M.spectrum(M.mono(r1.L.subarray(a, b), r1.R.subarray(a, b)), SR, 8192);
    const heard = worstBand(bands(s0), bands(s1));
    /* part 2: the estimator, each against its own truth; the two truths' bands */
    const est = p => A.estimate((m, cap) => A.renderWith(C, Object.assign({}, p, { os: N * m }), A.SCRIPT(note), { seed: SEED, cap, raw: true }), N, A.WIN, { truth: true, output: false, keep: true });
    const e0 = est(p0), e1 = est(p1);
    const timbre = worstBand(bands(e0._ST[0]), bands(e1._ST[0]));
    row[nn] = { band: +heard.db.toFixed(1), at: Math.round(heard.at), level: +(rmsDb(r1.L, r1.R, a, b) - rmsDb(r0.L, r0.R, a, b)).toFixed(2),
      aliasOff: +e0.totalDb.toFixed(1), aliasOn: +e1.totalDb.toFixed(1), timbre: +timbre.db.toFixed(1), timbreAt: Math.round(timbre.at) };
  }
  return row;
}

const fHz = x => (x >= 1000 ? (x / 1000).toFixed(x >= 10000 ? 1 : 2) + ' k' : x + ' ');
export function table(rows) {
  const L = [];
  L.push('# ADR-189 D3 (`aaLoop`): what the loop filter does to the bench presets it reaches');
  L.push('');
  L.push('Generated by `node tools/patchspace/antialias_scope.mjs --write` (B355 rework, ADR-189 A1); its header states the method. D3 = the oracle\'s own loop tap through a one-pole loop filter (`LOOP_FC` in docs/design/scalpel-horde-engine.js). Every bench preset with feedback or cross-mod on; every other preset is bit-identical under D3. os 2, velocity 0.8, seed 7.');
  L.push('');
  L.push('- **heard band / level**: a 1 s held note, 0.1..1.0 s; the largest 1/3-octave band change (D3 on minus off) over the bands within 40 dB of the loudest, and the RMS level change, dB.');
  L.push('- **aliasing off -> on**: B346\'s estimator total (dB of the window), each render against its own oversampled truth; a fall is aliasing removed.');
  L.push('- **timbre band**: the same band rule on the two TRUTHS (off\'s against on\'s): what D3 changes in the sound once the aliasing is out of both.');
  L.push('');
  L.push('| preset | fb | xm | A3 heard band | A3 level | A3 aliasing off -> on | A3 timbre band | E5 heard band | E5 level | E5 aliasing off -> on | E5 timbre band |');
  L.push('|---|---|---|---|---|---|---|---|---|---|---|');
  for (const r of rows) {
    const c = n => { const x = r[n]; return `${x.band > 0 ? '+' : ''}${x.band.toFixed(1)} @ ${fHz(x.at)}Hz | ${x.level > 0 ? '+' : ''}${x.level.toFixed(2)} | ${x.aliasOff.toFixed(1)} -> ${x.aliasOn.toFixed(1)} | ${x.timbre > 0 ? '+' : ''}${x.timbre.toFixed(1)} @ ${fHz(x.timbreAt)}Hz`; };
    L.push(`| ${r.name} | ${r.fb.toFixed(2)} | ${r.xm.toFixed(2)} | ${c('A3')} | ${c('E5')} |`);
  }
  L.push('');
  return L.join('\n');
}

if (process.argv[1] && process.argv[1].endsWith('antialias_scope.mjs')) {
  const rows = LOOP.map(p => { const r = measure(p); console.error(r.name); return r; });
  const md = table(rows);
  if (process.argv.includes('--write')) writeFileSync(join(ROOT, OUTFILE), md);
  process.stdout.write(md);
}
