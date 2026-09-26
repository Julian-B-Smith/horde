/*
 * filter_fidelity_check.mjs — the filter lab's fidelity programme, run headless.
 * WIRED: ./verify fast (beside fxlab_check).
 *
 * WHY THIS EXISTS (B274, human 2026-09-26: "We'll need to do extensive fidelity
 * testing"). docs/design/filter-lab.html offers twelve filter types behind one
 * interface and measures every one at load with seven checks (C1-C7: the analytic
 * response, cutoff/resonance accuracy, self-oscillation, stability under fast
 * modulation, aliasing, level at resonance, DC), each with a stated tolerance and
 * a must-fail control. A table nobody opens the page to read is not a gate, so
 * this file runs the SAME code headlessly.
 *
 * IT SLICES, IT DOES NOT REIMPLEMENT. The lab's DSP section is cut out by the
 * `design` banners of tools/golden/extract_core.mjs and evaluated with
 * new Function in THIS context (not a vm sandbox — L0052: vm globals cost ~56×),
 * so the measured code is the code that makes the sound. The swarm bank's class
 * is reference/swarmfilter.html's FilterLab, sliced by extractCore — the
 * protected reference is read, never modified.
 *
 * WHAT IS ASSERTED — red on any of:
 *   1. COVERAGE: the lab's row list is exactly the pinned one (20 rows) and there
 *      are exactly seven checks — a type or a check cannot vanish silently.
 *   2. EVERY CONTROL FAILS (L0016/L0032). A control that passes means that check
 *      cannot see the fault it exists for; it is an error, never a pass.
 *   3. VERDICTS MATCH THE PINS. A FAIL is a FINDING about a type — reported as
 *      found, never met by loosening a tolerance (B274: "a failing type is a
 *      finding"). The pins record today's findings so the gate is green while they
 *      stand, and so a type that STARTS failing (regression) or STOPS failing
 *      (a fixed finding, or a detector gone blind) turns the gate red until the pin
 *      is moved deliberately, with the reason, in the same PR.
 *   4. PLANTED FAULTS ARE CAUGHT. Each run plants known faults into scratch copies
 *      of the DSP text by anchored substitution (each anchor must occur exactly
 *      once — the planting mechanism is itself checked with a bogus anchor that
 *      must throw) and asserts the expected cell goes FAIL. This is the file's own
 *      must-fail proof: a green run shows the gate can go red.
 *
 * Usage: node tools/labharness/filter_fidelity_check.mjs [lab.html]
 *        (exit 1 on any error; the argument lets a scratch copy be checked)
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { BANNERS, extractCore } from '../golden/extract_core.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '../..');
const file = process.argv[2] || join(root, 'docs/design/filter-lab.html');
const EXPORTS = 'return { runFidelity, fidRows, fidEnv, FID_CHECKS, TYPES };';

function dspOf(html) {
  const b = BANNERS.design, a = html.search(b.start), e = html.search(b.end);
  if (a < 0 || e < 0 || e <= a) throw new Error(`${b.name} banners not found in the lab`);
  return html.slice(a, e);
}
const load = src => new Function('"use strict";\n' + src + '\n' + EXPORTS)();
/* Replace `anchor` with `repl`, asserting the anchor occurs EXACTLY once — a
   plant that silently matched nothing would "pass" by measuring the unplanted
   code (L0032: every calibration mutation asserts its anchor). */
function plant(src, anchor, repl) {
  const n = src.split(anchor).length - 1;
  if (n !== 1) throw new Error(`plant anchor found ${n}× (need exactly 1): ${JSON.stringify(anchor.slice(0, 60))}`);
  return src.replace(anchor, repl);
}

/* Today's verdicts, C1..C7, P = PASS, F = FAIL (a finding). Measured 2026-09-26
   on the lab as committed in the B274 PR; see the trace for every number.
   B287 phase A (2026-09-26) moved twelve pins F → P, each a fixed finding (a
   tightening): svf12 BP/HP C6, svf24 BP/HP/PEAK C6, ladder C3 + C6, combP C2,
   formant C6, bankP C2. Still F: ladder/ms20 C5 and phaser C4 (phase B, held for
   the B290 tolerance ruling); combP/combN C6 (compensating them trades the
   keytracked comb's level — refused, see traces/2026-09-26-b287-filter-fixes-a.md);
   bank C2 + C6 (properties of the protected reference; PROPOSED is the fix). */
const PINS = {
  'svf12.LP': 'PPPPPPP', 'svf12.BP': 'PPPPPPP', 'svf12.HP': 'PPPPPPP', 'svf12.NOTCH': 'PPPPPPP', 'svf12.PEAK': 'PPPPPPP',
  'svf24.LP': 'PPPPPPP', 'svf24.BP': 'PPPPPPP', 'svf24.HP': 'PPPPPPP', 'svf24.NOTCH': 'PPPPPPP', 'svf24.PEAK': 'PPPPPPP',
  ladder: 'PPPPFPP', ms20: 'PPPPFPP', combP: 'PPPPPFP', combN: 'PPPPPFP', formant: 'PPPPPPP',
  ap4: 'PPPPPPP', phaser: 'PPPFPPP', dj: 'PPPPPPP', bank: 'PFPPPFP', bankP: 'PPPPPPP',
};
/* Planted faults: [what, anchor, replacement, row, check that must go FAIL]. */
const PLANTS = [
  ['SVF prewarp removed (bilinear cutoff mapping)', 'const g = Math.tan(Math.PI * fc / sr);\n    const k = Math.max(0.05',
    'const g = Math.PI * fc / sr;\n    const k = Math.max(0.05', 'svf12.LP', 'C1'],
  ['ladder loop tanh removed', 'let u = Math.tanh(dg * (x - k * y4)) / dg;', 'let u = x - k * y4;', 'ladder', 'C4'],
  ['comb fractional delay dropped', 'this.N = Math.floor(D); lagr3(D - this.N, this.h);', 'this.N = Math.floor(D); lagr3(0, this.h);', 'combP', 'C1'],
  ['MS-20 output offset by 1e-3', 'const v3 = (y - this.s3) * G; this.s3 = v3 + this.s3 + v3;\n    return y;',
    'const v3 = (y - this.s3) * G; this.s3 = v3 + this.s3 + v3;\n    return y + 1e-3;', 'ms20', 'C7'],
  ['copied bank loop gain 1.6 → 1.7', 'wetS *= 1.6 / Math.sqrt(n) * this.qcomp;', 'wetS *= 1.7 / Math.sqrt(n) * this.qcomp;', 'bank', 'C1'],
  /* B287 phase A: one plant per new resonance-compensation law (removing it must
     turn C6 red), and one per other fix (reverting it must turn its check red). */
  ['SVF 12 resonance compensation removed', 'this.g = svfComp(this.f.k, Math.SQRT2, this.m); }', 'this.g = 1; }', 'svf12.BP', 'C6'],
  ['SVF 24 resonance compensation removed', 'this.g = svfComp(kb, K24B, this.m); }', 'this.g = 1; }', 'svf24.BP', 'C6'],
  ['ladder half-compensation removed', 'this.out = ladderComp(this.k);', 'this.out = 1;', 'ladder', 'C6'],
  ['formant √qs compensation removed', 'amp[j] = Math.pow(10, dB / 20) * qg;', 'amp[j] = Math.pow(10, dB / 20);', 'formant', 'C6'],
  ['ladder self-oscillation segment removed (k = 4.2·res to the top)',
    'return r <= LADDER_RON ? LADDER_KON * r : 4 + (LADDER_KTOP - 4) * (r - LADDER_RON) / (1 - LADDER_RON); };', 'return LADDER_KON * r; };', 'ladder', 'C3'],
  ['comb delay read back to linear interpolation', 'this.N = Math.floor(D); lagr3(D - this.N, this.h);',
    'this.N = Math.floor(D); { const fr = D - this.N; this.h[0] = 0; this.h[1] = 1 - fr; this.h[2] = fr; this.h[3] = 0; }', 'combP', 'C2'],
  ['P4 spread fit removed (bands pushed past the range)', 'sE = Math.max(0, Math.min(p.spread, Math.min(e - erbN(lo), erbN(hi) - e) / 12));', 'sE = p.spread;', 'bankP', 'C2'],
  ['P4 coefficient cap back to the reference\'s 0.24·fs', 'this.top = this.rangeOn ? 0.49 : 0.24;', 'this.top = 0.24;', 'bankP', 'C2'],
];

const errors = [];
const html = readFileSync(file, 'utf8');
const src = dspOf(html);
const M = load(src);
const FL = extractCore(join(root, 'reference/swarmfilter.html'), 'FilterLab');
const env = M.fidEnv(FL);

// 1. coverage
const rows = M.fidRows(true), ids = rows.map(r => r.id), pinIds = Object.keys(PINS);
if (M.FID_CHECKS.length !== 7) errors.push(`expected 7 checks, the lab has ${M.FID_CHECKS.length}`);
for (const id of pinIds) if (!ids.includes(id)) errors.push(`pinned row ${id} is missing from the lab`);
for (const id of ids) if (!(id in PINS)) errors.push(`lab row ${id} has no pin — measure it and add one`);

// 2 + 3. the programme, every control, every verdict
const res = M.runFidelity(env);
const checks = M.FID_CHECKS.map(c => c.id);
console.log('row'.padEnd(13) + checks.map(c => c.padEnd(5)).join(' ') + '  (P pass · F finding · ! control blind)');
let pass = 0, fail = 0, pinned = 0, ctl = 0;
for (const { row, cells } of res) {
  const got = checks.map(c => (cells[c].ok ? 'P' : 'F')).join('');
  const marks = checks.map(c => ((cells[c].ok ? 'P' : 'F') + (cells[c].ctlOk ? ' ' : '!')).padEnd(5)).join(' ');
  console.log(row.id.padEnd(13) + marks + (PINS[row.id] && got !== PINS[row.id] ? `  ≠ pin ${PINS[row.id]}` : ''));
  for (let i = 0; i < checks.length; i++) {
    const c = cells[checks[i]];
    if (c.ok) pass++; else { fail++; if (PINS[row.id] && PINS[row.id][i] === 'F') pinned++; }
    if (c.ctlOk) ctl++; else errors.push(`${row.id} ${checks[i]}: control PASSED (${c.ctl}) — the check is blind`);
    const want = PINS[row.id] && PINS[row.id][i];
    if (want && want !== got[i]) errors.push(`${row.id} ${checks[i]}: ${got[i] === 'F' ? 'now FAILS' : 'now PASSES'} (pinned ${want}) — ${c.val}${c.note ? ' — ' + c.note : ''}`);
  }
}
console.log('\nFAILs — findings, each reported as measured:');
for (const { row, cells } of res) for (const c of checks) if (!cells[c].ok) console.log(`  ${row.id.padEnd(12)} ${c}  ${cells[c].val}${cells[c].note ? '  — ' + cells[c].note : ''}`);

// 4. planted faults
let caught = 0;
try { plant(src, 'this anchor is not in the lab', ''); errors.push('the planter accepted a bogus anchor — plants are unverified'); }
catch (_) { /* as it must */ }
for (const [what, anchor, repl, rowId, check] of PLANTS) {
  let verdict;
  try {
    const Mp = load(plant(src, anchor, repl));
    const [r] = Mp.runFidelity(env, [rowId]);
    verdict = r && r.cells[check] && r.cells[check].ok ? 'PASS' : 'FAIL';
  } catch (e) { verdict = /plant anchor/.test(String(e && e.message)) ? 'ANCHOR' : 'FAIL'; if (verdict === 'ANCHOR') errors.push(String(e.message)); }
  if (verdict === 'FAIL') caught++;
  else if (verdict === 'PASS') errors.push(`planted fault NOT caught: ${what} — ${rowId} ${check} still passes`);
  console.log(`plant ${verdict === 'FAIL' ? 'caught' : 'MISSED'}  ${what} → ${rowId} ${check} ${verdict}`);
}

for (const e of errors) console.log(`ERROR ${e}`);
console.log(`\n${errors.length ? 'RED' : 'GREEN'} — filter_fidelity_check: ${res.length} types × ${checks.length} checks, ${pass} pass, ${fail} FAIL (${pinned} of them pinned findings), `
  + `${ctl}/${res.length * checks.length} controls fail as they must, ${caught}/${PLANTS.length} planted faults caught, ${errors.length} error(s)`);
process.exit(errors.length ? 1 : 0);
