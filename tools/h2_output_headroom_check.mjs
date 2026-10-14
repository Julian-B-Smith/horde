/*
 * h2_output_headroom_check.mjs — ADR-221 (ROADMAP B489) on the C++ engine: the composed
 * engine's default output level is 0.0875, a quarter of the blade oracle's 0.35, and at it a
 * four-note chord no longer drives the output stage over its knee. The JS twin's half (the
 * default exactly 0.25 of the oracle's, every drive exactly 0.25 of the old one's, read through
 * Math.tanh) is composed_engine_check's GAIN rows; the twins' parity check ties the two.
 *
 * WIRED: ./verify full
 *
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so this is the C++
 * engine as it ships. `ledger/defaults` (the engine with no preset; the cell's poly 8), the chord
 * C4 E4 G4 B4 at velocity 0.85, os 1, 48 kHz, 1 s held, host seed 1, with the pre-limiter bus:
 * the drive into tanh(y * gain * 1.6), read as atanh(out) (h2/harness/README.md, "The
 * pre-limiter bus"). Four renders: at the default; with gain 0.0875 written; with the old default
 * 0.35 written; and with gain 0.0875 x 1.01 written.
 *
 * WHAT IS JUDGED.
 *   DEFAULT  the default render's digest equals the render with gain 0.0875 written (the default is
 *            that double, to the bit), and differs from the one at 0.35.
 *   SCALE    the default render's drive is 0.25 times the drive at 0.35, sample by sample, within
 *            kScaleTol. The drives themselves are the same double by construction (0.0875 is
 *            0.35 / 4 exactly, and a power-of-two scale commutes with each rounding, so
 *            y * 0.0875 * 1.6 == (y * 0.35 * 1.6) / 4, for the same y, which the level never
 *            feeds back into); what the tolerance covers is the bus's own reading, atanh of the
 *            output, whose error grows toward the rail (1.3e-15 at 3, README). Measured worst
 *            2026-10-14 (darwin-arm64): 3.3e-16, printed on every run; the 1 % control reads 5.6e-3.
 *   HEADROOM the default render's peak drive is under 1 (and over 0.1, so the render is live).
 *   CONTROLS (LIBRARY L0032: a detector that cannot see the fault proves nothing): the old default,
 *            0.35, must read a peak drive over 1 (2.227 measured, ROADMAP B486/B489); a level 1 %
 *            off (0.0875 x 1.01) must fail SCALE's comparison.
 *
 * By hand (repo root): node tools/h2_output_headroom_check.mjs [--bin <h2_harness_render>]
 * Deterministic: no clock, no randomness but the harness's seeded streams. Its last line is its verdict.
 */
import { realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { jobFor, run } from '../h2/harness/harness.mjs';

const PATCH = 'ledger/defaults', CHORD = [60, 64, 67, 71], VEL = 0.85, SR = 48000, SECONDS = 1;
const kDefault = 0.0875, kOld = 0.35;
const kScaleTol = 1e-12;   // atanh's reading of the bus, not the drive (header, SCALE)

const job = over => jobFor({ patch: PATCH, sr: SR, seed: 1, seconds: SECONDS, notes: CHORD, vel: VEL, pre: true,
  overrides: Object.assign({ os: 1 }, over) });
const peak = r => { let m = 0; for (const b of [r.preL, r.preR]) for (const x of b) m = Math.max(m, Math.abs(x)); return m; };
const scaleGap = (a, b) => { let m = 0; for (const [x, y] of [[a.preL, b.preL], [a.preR, b.preR]]) for (let i = 0; i < x.length; i++) m = Math.max(m, Math.abs(x[i] - 0.25 * y[i])); return m; };

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const [def, written, old, off] = run([job({}), job({ gain: kDefault }), job({ gain: kOld }), job({ gain: kDefault * 1.01 })], opt);
  const unknown = [def, written, old, off].filter(r => r.summary.unknownKeys).length;
  row(!unknown, 'KEYS', `the engine knew every key on ${4 - unknown} of 4 jobs`);
  console.log(`${PATCH}, chord ${CHORD.join(' ')} at velocity ${VEL}, os 1, ${SR} Hz, ${SECONDS} s held, the pre-limiter bus (atanh of the output)`);
  row(def.summary.digest === written.summary.digest && def.summary.digest !== old.summary.digest, 'DEFAULT',
    `at the default ${def.summary.digest}; gain ${kDefault} written ${written.summary.digest} (must be equal); gain ${kOld} written ${old.summary.digest} (must differ)`);
  const g = scaleGap(def, old), gc = scaleGap(off, old);
  row(g <= kScaleTol, 'SCALE', `drive at the default against 0.25 x the drive at ${kOld}: max |difference| ${g.toExponential(2)} (tolerance ${kScaleTol})`);
  row(gc > kScaleTol, 'CONTROL', `a level 1 % off (gain ${kDefault * 1.01}) against 0.25 x the drive at ${kOld}: max |difference| ${gc.toExponential(2)} — must exceed ${kScaleTol}`);
  const pd = peak(def), po = peak(old);
  row(pd < 1 && pd > 0.1, 'HEADROOM', `peak drive into the output tanh at the default: ${pd.toFixed(3)} — must be under 1 (and over 0.1: the render is live)`);
  row(po > 1, 'CONTROL', `the old default, gain ${kOld}: peak drive ${po.toFixed(3)} — must read over 1 (the clipping ADR-221 removes)`);
  console.log(`h2_output_headroom_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the default level, its scale against the old one, the chord's headroom, and their controls)`);
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (e) {
    console.error(String(e && e.stack || e));
    console.log(`h2_output_headroom_check: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    process.exitCode = 2;
  }
}
