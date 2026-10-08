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
 * B290 (RULED 2026-09-26, human: "your recommendation is ratified for all
 * tolerances") re-cut four checks; B287 phase B carried it out. As implemented in
 * the lab's FID / FID_CHECKS:
 *   C3  the self-oscillation level within ±6 dB of the unfiltered source's RMS (the
 *       lead's choice, the same bound as C6's swing); was ≥ −30 dBFS absolute.
 *       Pitch ≤ 5 ¢ and the ≤ 3 dB spread are unchanged.
 *   C4  the GATE delivers round 2's stimuli on the plugin's mod tick
 *       (kGravGridSeconds = 256/44100 s, src/swarm_core.h:136, ≈ 172 Hz), with the
 *       unchanged +12 / +40 dB bounds. C4b REPORTS each type's maximum viable
 *       modulation rate (per-sample delivery, half-octave steps) and never gates.
 *   C5  −60 dBc at the NOMINAL drive 0.3 (the knob default, and C1-C3's operating
 *       point); was drive 1. A per-type COST is reported beside it (never gated).
 *   C6  ≤ 6 dB swing at BOTH geometries: fixed (1 kHz over a 110 Hz saw) and
 *       keytracked (cutoff = f0).
 *
 * IT SLICES, IT DOES NOT REIMPLEMENT. The lab's DSP section is cut out by the
 * `design` banners of tools/golden/extract_core.mjs and evaluated with
 * new Function in THIS context (not a vm sandbox — L0052: vm globals cost ~56×,
 * and the cost column is timed here), so the measured code is the code that makes
 * the sound. The swarm bank's class is reference/swarmfilter.html's FilterLab,
 * sliced by extractCore — the protected reference is read, never modified.
 *
 * WHAT IS ASSERTED — red on any of:
 *   1. COVERAGE: the lab's row list is exactly the pinned one (20 rows), there are
 *      exactly seven checks, and every row carries the C4b report.
 *   2. EVERY CONTROL FAILS (L0016/L0032), C4b's included. A control that passes
 *      means that check cannot see the fault it exists for; it is an error.
 *   3. VERDICTS MATCH THE PINS. A FAIL is a FINDING about a type — reported as
 *      found, never met by loosening a tolerance. The pins record today's findings
 *      so the gate is green while they stand, and so a type that STARTS failing
 *      (regression) or STOPS failing (a fixed finding, or a detector gone blind)
 *      turns the gate red until the pin is moved deliberately, with the reason, in
 *      the same PR.
 *   4. PLANTED FAULTS ARE CAUGHT. Each run plants known faults into scratch copies
 *      of the DSP text by anchored substitution (each anchor must occur exactly
 *      once — the planting mechanism is itself checked with a bogus anchor that
 *      must throw) and asserts the planted cell's verdict DIFFERS FROM ITS PIN (a
 *      fault in a passing type must fail it; a fault in a DETECTOR must flip a
 *      finding it could see). A 'C4b' plant is caught when the reported maximum
 *      rate falls. This is the file's own must-fail proof.
 *   5. THE ADAA CONDITIONING PROOF (B287 phase B): every ADAA option of every
 *      nonlinear type, fed silence (|Δ| = 0 exactly), a held DC (Δ → 0) and the C5
 *      sine, stays finite, and silence stays exactly silent. A plant removes the
 *      small-Δ fallback and must break it (0/0).
 *   6. THE RIG ROW (B291): in every placement, choosing any rack type rebuilds the
 *      rack instance to that type (a SWARM type in PER SOURCE threw in drawPath
 *      when it did not). A plant restores the old order and must break it.
 * REPORTED, NEVER GATED: each row's C4b rate and cost (the manifest table the page
 * prints) and, with --report, the ADAA-first evaluation table (every option × C5 at
 * the nominal and full drive, C1, C2, C3, C7, cost), which also asserts that each
 * type's recorded option passes there.
 *
 * Usage: node tools/labharness/filter_fidelity_check.mjs [--report] [lab.html]
 *        (exit 1 on any error; the argument lets a scratch copy be checked)
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { performance } from 'node:perf_hooks';
import { BANNERS, extractCore } from '../golden/extract_core.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '../..');
const REPORT = process.argv.includes('--report');
const file = process.argv.slice(2).filter(a => a !== '--report')[0] || join(root, 'docs/design/filter-lab.html');
const EXPORTS = 'return { runFidelity, fidRows, fidEnv, FID_CHECKS, FID_REPORTS, TYPES, TYPE_ORDER, aaEval, aaType, AA_OPTS, fidCost, COST_REF, fidMake, fidP, DRIVE_NOM, FilterRig, defaultPatch, TAU };';

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

/* Verdicts, C1..C7, P = PASS, F = FAIL (a finding). History:
   B274 (2026-09-26) first measured them; B287 phase A moved twelve F → P (fixed
   findings). B287 phase B (2026-09-26), under the B290 ruling:
   · F → P, fixed: ladder C5 (2× oversampled), ms20 C5 (centred first-order ADAA),
     phaser C4 (the all-pass chain as a normalised lattice; round 2's C4 read +20.5).
   · P → F, the RE-CUT GATE (B290 C6: keytracked geometry added — no regression; every
     number is in the trace): svf12 LP/BP/HP/PEAK, svf24 LP/BP/HP/PEAK, ladder, ms20,
     phaser, bankP C6. Each is refused with its law room: > 12 dB proves no resonance
     law can pass both geometries; ≤ 12 dB admits only a law fitted between them.
   · Still F: combP/combN C6 (now at both geometries) and bank C2 + C6 (properties of
     the protected reference; PROPOSED is the fix). */
const PINS = {
  'svf12.LP': 'PPPPPFP', 'svf12.BP': 'PPPPPFP', 'svf12.HP': 'PPPPPFP', 'svf12.NOTCH': 'PPPPPPP', 'svf12.PEAK': 'PPPPPFP',
  'svf24.LP': 'PPPPPFP', 'svf24.BP': 'PPPPPFP', 'svf24.HP': 'PPPPPFP', 'svf24.NOTCH': 'PPPPPPP', 'svf24.PEAK': 'PPPPPFP',
  ladder: 'PPPPPFP', ms20: 'PPPPPFP', combP: 'PPPPPFP', combN: 'PPPPPFP', formant: 'PPPPPPP',
  ap4: 'PPPPPPP', phaser: 'PPPPPFP', dj: 'PPPPPPP', bank: 'PFPPPFP', bankP: 'PPPPPFP',
};
/* Planted faults: [what, anchor, replacement, row, check whose verdict must leave its pin]. */
const PLANTS = [
  ['SVF prewarp removed (bilinear cutoff mapping)', 'const g = Math.tan(Math.PI * fc / sr);\n    const k = Math.max(0.05',
    'const g = Math.PI * fc / sr;\n    const k = Math.max(0.05', 'svf12.LP', 'C1'],
  ['ladder loop tanh removed', 'let u = this.nl(a, this) / dg;', 'let u = a / dg;', 'ladder', 'C4'],
  ['comb fractional delay dropped', 'this.N = Math.floor(D); lagr3(D - this.N, this.h);', 'this.N = Math.floor(D); lagr3(0, this.h);', 'combP', 'C1'],
  ['MS-20 output offset by 1e-3', 'const v3 = (y - this.s3) * G; this.s3 = v3 + this.s3 + v3;\n    return y;',
    'const v3 = (y - this.s3) * G; this.s3 = v3 + this.s3 + v3;\n    return y + 1e-3;', 'ms20', 'C7'],
  ['copied bank loop gain 1.6 → 1.7', 'wetS *= 1.6 / Math.sqrt(n) * this.qcomp;', 'wetS *= 1.7 / Math.sqrt(n) * this.qcomp;', 'bank', 'C1'],
  /* B287 phase A: one plant per resonance-compensation law, one per other fix.
     COVERAGE BOUNDARY (L0033), since phase B: the SVF, SVF 24 and ladder laws hold the
     FIXED geometry, but their rows now fail C6 at the keytracked one, so removing a law
     cannot flip C6 any more (it moves svf12 BP's fixed swing 0.3 → 10.3 dB, invisibly).
     Those three plants target C1 instead, which sees the DSP lose a law its H keeps. */
  ['SVF 12 resonance compensation removed', 'this.g = svfComp(this.f.k, Math.SQRT2, this.m); }', 'this.g = 1; }', 'svf12.BP', 'C1'],
  ['SVF 24 resonance compensation removed', 'this.g = svfComp(kb, K24B, this.m); }', 'this.g = 1; }', 'svf24.BP', 'C1'],
  ['ladder half-compensation removed', 'this.out = ladderComp(this.k);', 'this.out = 1;', 'ladder', 'C1'],
  ['formant √qs compensation removed', 'amp[j] = Math.pow(10, dB / 20) * qg;', 'amp[j] = Math.pow(10, dB / 20);', 'formant', 'C6'],
  ['ladder self-oscillation segment removed (k = 4.2·res to the top)',
    'return r <= LADDER_RON ? LADDER_KON * r : 4 + (LADDER_KTOP - 4) * (r - LADDER_RON) / (1 - LADDER_RON); };', 'return LADDER_KON * r; };', 'ladder', 'C3'],
  ['comb delay read back to linear interpolation', 'this.N = Math.floor(D); lagr3(D - this.N, this.h);',
    'this.N = Math.floor(D); { const fr = D - this.N; this.h[0] = 0; this.h[1] = 1 - fr; this.h[2] = fr; this.h[3] = 0; }', 'combP', 'C2'],
  ['P4 spread fit removed (bands pushed past the range)', 'sE = Math.max(0, Math.min(p.spread, Math.min(e - erbN(lo), erbN(hi) - e) / 12));', 'sE = p.spread;', 'bankP', 'C2'],
  ['P4 coefficient cap back to the reference\'s 0.24·fs', 'this.top = this.rangeOn ? 0.49 : 0.24;', 'this.top = 0.24;', 'bankP', 'C2'],
  /* B287 phase B: one plant per fix and per re-cut check. */
  ['ladder oversampling removed (os2 → none)', "aa: 'os2',", "aa: 'none',", 'ladder', 'C5'],
  ['MS-20 ADAA removed (adaa1c → none)', "aa: 'adaa1c',", "aa: 'none',", 'ms20', 'C5'],
  ['MS-20 ADAA delay compensation removed (adaa1c → adaa1: the half-sample delay back)', "aa: 'adaa1c',", "aa: 'adaa1',", 'ms20', 'C3'],
  ['oversampling latency off by one sample in the analytic H', 'const ph = -2 * Math.PI * f * osLat(st) / sr;', 'const ph = -2 * Math.PI * f * (osLat(st) + 1) / sr;', 'ladder', 'C1'],
  ['all-pass chain back to the TPT form (phaser C4a)',
    'tick(x) { const s = this.s, a = this.a, c = this.c; for (let i = 0; i < 4; i++) { const y = c * s[i] - a * x; s[i] = c * x + a * s[i]; x = y; } return x; }',
    'tick(x) { const s = this.s, G = (1 - this.a) / 2; for (let i = 0; i < 4; i++) { const v = (x - s[i]) * G, lp = v + s[i]; s[i] = lp + v; x = 2 * lp - x; } return x; }', 'phaser', 'C4'],
  ['all-pass chain back to the TPT form (phaser C4b: its maximum rate must fall)',
    'tick(x) { const s = this.s, a = this.a, c = this.c; for (let i = 0; i < 4; i++) { const y = c * s[i] - a * x; s[i] = c * x + a * s[i]; x = y; } return x; }',
    'tick(x) { const s = this.s, G = (1 - this.a) / 2; for (let i = 0; i < 4; i++) { const v = (x - s[i]) * G, lp = v + s[i]; s[i] = lp + v; x = 2 * lp - x; } return x; }', 'phaser', 'C4b'],
  ['ladder output ×3 (+9.5 dB): C3\'s source-relative level must see it', 'return u * this.out;', 'return 3 * u * this.out;', 'ladder', 'C3'],
  ['C6\'s keytracked geometry measured at the fixed cutoff (a blind detector must flip a finding)',
    'const g1 = geo(C.fc), g2 = geo(f0);', 'const g1 = geo(C.fc), g2 = geo(C.fc);', 'svf12.BP', 'C6'],
];
/* Plants for the proofs outside the programme (5 and 6). */
const PLANT_FALLBACK = ['ADAA small-Δ fallback removed (0/0 at a repeated sample)',
  'function aaD1(F, f, a, b) { const d = a - b; return Math.abs(d) > AA_EPS ? (F(a) - F(b)) / d : f(0.5 * (a + b)); }',
  'function aaD1(F, f, a, b) { const d = a - b; return (F(a) - F(b)) / d; }'];
const PLANT_RIG = ['B291: rack rebuild back after the PER SOURCE early return',
  "    if (this.rType !== R.type) this.rebuildRack();\n    if (S.place === 'pre') return;",
  "    if (S.place === 'pre') return;\n    if (this.rType !== R.type) this.rebuildRack();"];

const errors = [];
const html = readFileSync(file, 'utf8');
const src = dspOf(html);
const M = load(src);
const FL = extractCore(join(root, 'reference/swarmfilter.html'), 'FilterLab');
const env = M.fidEnv(FL);
const now = () => performance.now();

// 1. coverage
const rows = M.fidRows(true), ids = rows.map(r => r.id), pinIds = Object.keys(PINS);
if (M.FID_CHECKS.length !== 7) errors.push(`expected 7 checks, the lab has ${M.FID_CHECKS.length}`);
if (M.FID_REPORTS.map(r => r.id).join() !== 'C4b,COST') errors.push(`expected the reports C4b, COST; the lab has ${M.FID_REPORTS.map(r => r.id).join()}`);
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
  if (!cells.C4b) errors.push(`${row.id}: no C4b report`);
  else if (!cells.C4b.ctlOk) errors.push(`${row.id} C4b: control PASSED (${cells.C4b.ctl}) — the rate scan is blind`);
}
console.log('\nFAILs — findings, each reported as measured:');
for (const { row, cells } of res) for (const c of checks) if (!cells[c].ok) console.log(`  ${row.id.padEnd(12)} ${c}  ${cells[c].val}${cells[c].note ? '  — ' + cells[c].note : ''}`);

// REPORTED: the manifest (C5 option, dBc at the nominal drive, cost, max viable modulation rate)
const refRow = rows.find(r => r.id === M.COST_REF.id);
for (let w = 0; w < 2; w++) M.fidCost(M.TYPES[refRow.type], refRow, env, now);
const costOf = row => M.fidCost(M.TYPES[row.type], row, env, now).perSample;
const base = costOf(refRow);
console.log(`\nMANIFEST (reported, never gated) — C5 at drive ${M.DRIVE_NOM}; cost × SVF 12 LP (${(base * 1e6).toFixed(1)} ns/sample, median of 7); C4b max viable modulation rate`);
for (const { row, cells } of res) {
  const T = M.TYPES[row.type];
  console.log(`  ${row.id.padEnd(12)} ${(T.aa || '—').padEnd(8)} ${cells.C5.val.padStart(11)}  ${(costOf(row) / base).toFixed(2).padStart(6)}×  ${cells.C4b.val}`);
}

// REPORTED: the ADAA-first evaluation (~8 s, so only with --report; the page runs it
// live. The GATE on the chosen options is their rows' pins above, every run.)
for (const id of REPORT ? M.TYPE_ORDER.filter(t => M.TYPES[t].aa) : []) {
  const ev = M.aaEval(id, env, now), b = costOf(refRow);
  console.log(`\nADAA FIRST — ${id} (chosen: ${M.TYPES[id].aa}; C5 at drive ${M.DRIVE_NOM} gates, drive 1 is reported)`);
  console.log('  ' + 'option'.padEnd(11) + 'C5@nom'.padStart(8) + 'C5@1'.padStart(8) + '  ' + 'C1'.padEnd(22) + 'C2'.padEnd(21) + 'C3'.padEnd(20) + 'C7'.padEnd(4) + 'cost×'.padStart(7) + '  all');
  const cheapest = ev.filter(r => r.passes).sort((p, q) => p.cost - q.cost)[0];
  for (const r of ev) {
    const v = c => (c.ok ? 'P ' : 'F ') + String(c.val).split(' · ').slice(0, 2).join(' · ');
    console.log('  ' + r.opt.padEnd(11) + r.C5.val.replace(' dBc', '').padStart(8) + r.full.val.replace(' dBc', '').padStart(8) + '  ' + v(r.C1).padEnd(22) + v(r.C2).padEnd(21)
      + ((r.C3.ok ? 'P ' : 'F ') + r.C3.val.split(' · ')[0]).padEnd(20) + (r.C7.ok ? 'P' : 'F').padEnd(4) + (r.cost / b).toFixed(2).padStart(7) + '  ' + (r.passes ? 'PASS' : '—')
      + (r.opt === M.TYPES[id].aa ? '  ← chosen' : ''));
  }
  const chosen = ev.find(r => r.opt === M.TYPES[id].aa);
  if (!chosen || !chosen.passes) errors.push(`${id}: its recorded anti-aliasing option ${M.TYPES[id].aa} does not pass C1/C2/C3/C5/C7 in the evaluation`);
  if (cheapest && chosen && cheapest.opt !== chosen.opt) console.log(`  NOTE: in this run ${cheapest.opt} timed cheaper than the recorded ${chosen.opt} (timing is reported, not gated)`);
}

// 4. planted faults
let caught = 0;
const nPlants = PLANTS.length + 2;
try { plant(src, 'this anchor is not in the lab', ''); errors.push('the planter accepted a bogus anchor — plants are unverified'); }
catch (_) { /* as it must */ }
const c4bBase = Object.fromEntries(res.map(({ row, cells }) => [row.id, cells.C4b.max]));
for (const [what, anchor, repl, rowId, check] of PLANTS) {
  let verdict;
  try {
    const Mp = load(plant(src, anchor, repl));
    const [r] = Mp.runFidelity(env, [rowId]);
    if (check === 'C4b') verdict = r && r.cells.C4b.max < c4bBase[rowId] ? 'MOVED' : 'SAME';
    else {
      const i = checks.indexOf(check), got = r && r.cells[check] && r.cells[check].ok ? 'P' : 'F';
      verdict = got !== PINS[rowId][i] ? 'MOVED' : 'SAME';
    }
  } catch (e) { verdict = /plant anchor/.test(String(e && e.message)) ? 'ANCHOR' : 'MOVED'; if (verdict === 'ANCHOR') errors.push(String(e.message)); }
  if (verdict === 'MOVED') caught++;
  else if (verdict === 'SAME') errors.push(`planted fault NOT caught: ${what} — ${rowId} ${check} unchanged`);
  console.log(`plant ${verdict === 'MOVED' ? 'caught' : 'MISSED'}  ${what} → ${rowId} ${check}`);
}

// 5. the ADAA conditioning proof: finite everywhere, silence stays exactly silent
function conditioning(Mx) {
  const bad = [];
  for (const id of Mx.TYPE_ORDER.filter(t => Mx.TYPES[t].aa)) for (const opt of Mx.AA_OPTS.filter(o => /adaa/.test(o))) for (const drive of [Mx.DRIVE_NOM, 1]) {
    const T = Mx.aaType(Mx.TYPES[id], opt), P = Mx.fidP({ mode: 1, type: id }, { cutoff: 1000, res: 0.5, drive });
    const mk = () => { const i = T.make(); i.set(48000, P); return i; };
    let i = mk(), y;
    for (let n = 0; n < 4096; n++) { y = i.tick(0); if (y !== 0) { bad.push(`${id} ${opt} drive ${drive}: silence → ${y}`); break; } }
    i = mk(); let last = 0;
    for (let n = 0; n < 48000; n++) { y = i.tick(0.5); if (!Number.isFinite(y)) { bad.push(`${id} ${opt} drive ${drive}: DC → ${y} at ${n}`); break; } last = y; }
    if (Number.isFinite(last) && Math.abs(i.tick(0.5) - last) > 1e-9) bad.push(`${id} ${opt} drive ${drive}: DC never settles`);
    i = mk();
    for (let n = 0; n < 24000; n++) { y = i.tick(0.9 * Math.sin(Mx.TAU * 7000 * n / 48000)); if (!Number.isFinite(y)) { bad.push(`${id} ${opt} drive ${drive}: sine → ${y} at ${n}`); break; } }
  }
  return bad;
}
const cond = conditioning(M);
for (const b of cond) errors.push(`ADAA conditioning: ${b}`);
{
  let bad = [];
  try { bad = conditioning(load(plant(src, PLANT_FALLBACK[1], PLANT_FALLBACK[2]))); } catch (e) { if (/plant anchor/.test(String(e.message))) errors.push(String(e.message)); else bad = [String(e.message)]; }
  if (bad.length) caught++; else errors.push(`planted fault NOT caught: ${PLANT_FALLBACK[0]}`);
  console.log(`plant ${bad.length ? 'caught' : 'MISSED'}  ${PLANT_FALLBACK[0]} → conditioning proof (${bad.length} failure(s), first: ${bad[0] || '—'})`);
}

// 6. the rig row (B291): every placement × every rack type
function rigCheck(Mx) {
  const bad = [];
  for (const place of ['pre', 'post', 'both']) for (const rt of Mx.TYPE_ORDER) {
    const S = Mx.defaultPatch(); S.place = place;
    const rig = new Mx.FilterRig(48000, S); rig.attachBank(FL);
    const L = new Float32Array(256);
    rig.noteOn(57); rig.render(L, null, 256);
    S.flt.R.type = rt; rig.render(L, null, 256);             /* the user picks a rack type */
    const i = rig.rInst;
    if (!i || i.tid !== rt) bad.push(`${place} → ${rt}: the rack instance is ${i ? i.tid : 'null'}`);
    else if (Mx.TYPES[rt].needsRef && !(i.b && i.b.p)) bad.push(`${place} → ${rt}: the bank instance has no .b.p (drawPath reads it)`);
    if (!L.every(Number.isFinite)) bad.push(`${place} → ${rt}: non-finite output`);
  }
  return bad;
}
for (const b of rigCheck(M)) errors.push(`rig: ${b}`);
{
  let bad = [];
  try { bad = rigCheck(load(plant(src, PLANT_RIG[1], PLANT_RIG[2]))); } catch (e) { if (/plant anchor/.test(String(e.message))) errors.push(String(e.message)); else bad = [String(e.message)]; }
  if (bad.length) caught++; else errors.push(`planted fault NOT caught: ${PLANT_RIG[0]}`);
  console.log(`plant ${bad.length ? 'caught' : 'MISSED'}  ${PLANT_RIG[0]} → rig row (${bad.length} failure(s), first: ${bad[0] || '—'})`);
}

for (const e of errors) console.log(`ERROR ${e}`);
console.log(`\n${errors.length ? 'RED' : 'GREEN'} — filter_fidelity_check: ${res.length} types × ${checks.length} checks, ${pass} pass, ${fail} FAIL (${pinned} of them pinned findings), `
  + `${ctl}/${res.length * checks.length} controls fail as they must, ${caught}/${nPlants} planted faults caught, ${errors.length} error(s)`);
process.exit(errors.length ? 1 : 0);
