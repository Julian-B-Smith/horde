/*
 * port_legacy_presets_check.mjs — the oracles for tools/port_legacy_presets.mjs (B312).
 * WIRED: ./verify fast.
 *
 * HYPERSAW, 2026-09-27, dispatched by the horde lead on ROADMAP B312 (records
 * PR #801). SYNTHETIC FIXTURES ONLY: every preset and corner below is built
 * here from the parameter tables. The human's store is never read by this
 * check (their presets are private and this repo is public).
 *
 * WHAT IS CHECKED (the brief's list, plus the gates that keep it honest):
 *   T1 COVERAGE. docs/scalpel/ACCOUNTING.md §1.1/§1.2 parse to 82 + 12 rows with
 *      §0's fate counts (per-osc 64/13/5, globals 7/5); every row has exactly
 *      one rule and every rule a row; each rule's fate equals the row's; every
 *      target is a composed-engine key (read from a live instance) or, for a
 *      `lab` target, a row of the SCALPEL lab's table (`R('<key>'`).
 *   T2 SURVIVES ROUND-TRIP. Every SURVIVES row, set off its default, comes back
 *      EXACTLY: as params[target] when it has a target, else in
 *      unported.noTarget. Every per-osc row's osc-2 twin comes back in
 *      unported.osc2.
 *   T3 MERGES. Each conversion against the formula ACCOUNTING states, the
 *      expected numbers written out by hand here (never by calling the rule),
 *      and the set of rows tested must equal the set of MERGES rows.
 *   T4 RETIRED. Each retired row is reported (outcome retired, non-default
 *      flagged) and carried NOWHERE: not in params, not in any unported bucket.
 *   T5 CORNER DECODE. Positional arrays at every layout the store holds
 *      (2, 5, 7, 8, 9 and the unmarked 178/202/222/224) decode slot j to line
 *      j of tests/morph_order.txt, read here independently; the one layout-1
 *      224-slot shape (ADR-150, oscPitch inside the prefix) is spot-checked
 *      slot by slot; a decoded corner ports through the same fates.
 *   T6 NOTHING SILENTLY LOST. A synthetic preset holding every registry key
 *      (both oscillators) plus engine-block and chunk fields: every param key
 *      appears in the report exactly once, and every non-retired value is
 *      recoverable from params or unported.
 *   T7 PRIVACY. The default output directory is git-ignored, and an in-repo
 *      directory that is not ignored is refused.
 *   T8 BLADES OFF + DETERMINISM. Every port has b1on 0, b2on 0, base Saw; two
 *      runs are byte-identical.
 *   MUST-FAIL CONTROLS, each through the SAME detector: a wrong conversion
 *   (detune x 10), a retired row carried, a corner decoded one slot off, and a
 *   rule whose fate drifted from ACCOUNTING. Each must come back red.
 */
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import * as P from '../port_legacy_presets.mjs';

const ROOT = P.ROOT;
const ctx = P.loadContext(ROOT);
const LAB = readFileSync(join(ROOT, 'docs/design/scalpel-interface-lab.html'), 'utf8');
const labKeys = new Set([...LAB.matchAll(/\bR\('([^']+)'/g)].map((m) => m[1]));
const near = (a, b) => typeof a === 'number' && typeof b === 'number' && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b));

/* ------------------------------------------------------------------ T1 --- */
function t1(c) {
  const f = [];
  const per = c.accounting.filter((a) => !a.global), glo = c.accounting.filter((a) => a.global);
  const count = (rows, fate) => rows.filter((a) => a.fate === fate).length;
  if (per.length !== 82 || glo.length !== 12) f.push(`row counts ${per.length}/${glo.length}, want 82/12`);
  if (count(per, 'SURVIVES') !== 64 || count(per, 'MERGES') !== 13 || count(per, 'RETIRED') !== 5) f.push('per-osc fate counts differ from §0 (64/13/5)');
  if (count(glo, 'SURVIVES') !== 7 || count(glo, 'MERGES') !== 5) f.push('global fate counts differ from §0 (7/5)');
  const rows = new Set(c.accounting.map((a) => a.row));
  for (const k of Object.keys(c.rules)) if (!rows.has(k)) f.push(`rule ${k} has no ACCOUNTING row`);
  for (const a of c.accounting) {
    const r = c.rules[a.row];
    if (!r) { f.push(`row ${a.row} (${a.key}) has no rule`); continue; }
    if (r.fate !== a.fate) f.push(`row ${a.row} (${a.key}): rule says ${r.fate}, ACCOUNTING says ${a.fate}`);
    if (r.kind === 'retired' && a.fate !== 'RETIRED') f.push(`row ${a.row}: retired rule on a ${a.fate} row`);
    if (r.target) {
      if (r.lab ? !labKeys.has(r.target) : !(r.target in c.defaults)) f.push(`row ${a.row}: target ${r.target} is not a ${r.lab ? 'lab table' : 'composed-engine'} key`);
    }
  }
  for (const k of Object.keys(P.LEGACY_DEFAULTS)) if (!(k in c.defaults) && !labKeys.has(k)) f.push(`legacy default ${k} is not a target key`);
  return f;
}

/* ------------------------------------------------------------------ T2 --- */
// A value off the default and inside the range: the other end for a two-state row, else 37 % of the span.
const offDefault = (a) => {
  if (a.max - a.min <= 1) return a.def === a.min ? a.max : a.min;
  let v = a.min + 0.37 * (a.max - a.min);
  if (Number.isInteger(a.min) && Number.isInteger(a.max) && Number.isInteger(a.def)) v = Math.round(v);
  return v === a.def ? a.min : v;
};
function t2(c) {
  const f = [];
  for (const a of c.accounting.filter((x) => x.fate === 'SURVIVES')) {
    const v = offDefault(a);
    const legacy = { [a.key]: v };
    if (!a.global) legacy['o1.' + a.key] = v;
    const p = P.portPreset({ schema: 3, params: legacy }, c, 'syn');
    const r = c.rules[a.row];
    const back = r.target ? p.params[r.target] : p.unported.noTarget[a.key];
    if (back !== v) f.push(`SURVIVES row ${a.row} ${a.key}: ${v} came back ${back}`);
    if (!a.global && p.unported.osc2['o1.' + a.key] !== v) f.push(`SURVIVES row ${a.row} osc 2 twin lost`);
  }
  return f;
}

/* ------------------------------------------------------------------ T3 --- */
/* Hand-written from ACCOUNTING's text. [row, legacy params, schema, expect]:
   expect maps a target key to its value, or `unported: key` for a value the
   stated conversion does not cover. */
const MERGE_CASES = [
  ['1', { n: 5 }, 3, { N: 5 }], ['1', { n: 16 }, 3, { N: 9, noteHas: 'clamped' }],          // §3 cap 9, said
  ['4', { detune: 0.28 }, 3, { detune: 28 }], ['4', { detune: 1 }, 3, { detune: 100 }],      // knob x 100 c
  ['6', { K: -0.4 }, 3, { K: -0.4 }],                                                         // horde law, same K
  ['13', { width: 1.2 }, 3, { width: 1.2 }],                                                  // horde law, same value
  ['14', { digital: 1 }, 3, { aa: 1 }], ['14', { digital: 0 }, 3, { aa: 0 }], ['14', { digital: 0.5 }, 3, { unported: 'digital' }],
  ['15', { vol: 0.4 }, 3, { gain: 0.4 }],
  ['16', { retrig: 0 }, 3, { phaseMode: 0 }], ['16', { retrig: 1 }, 3, { phaseMode: 1 }],    // random / aligned
  ['17', { attack: 0.003 }, 3, { A: 3 }], ['18', { decay: 0.16 }, 3, { D: 160 }],            // s -> ms
  ['19', { sustain: 0.7 }, 3, { S: 0.7 }], ['20', { release: 1.5 }, 3, { R: 1500 }],
  ['29', { absK: 1 }, 3, { cScale: 0 }], ['29', { absK: 0 }, 3, { cScale: 1 }],              // seconds / cycles
  ['66', { panLayout: 1 }, 3, { unported: 'panLayout', panOrder: 1 }],                       // no stated map; §1.7 balanced off
  ['G1', { voiceMono: 0, voiceLegato: 1 }, 3, { polyMode: 0 }], ['G1', { voiceMono: 1, voiceLegato: 0 }, 3, { polyMode: 1 }],
  ['G2', { voiceMono: 1, voiceLegato: 1 }, 3, { polyMode: 2 }],
  ['G3', { glide: 0.5 }, 3, { glide: 1500 }],                                                 // T ms = τ s x 3000
  ['G4', { glideMode: 0 }, 3, { glideAlways: 0 }], ['G4', { glideMode: 2 }, 3, { glideAlways: 1 }],
  ['G4', { glideMode: 1 }, 3, { unported: 'glideMode' }], ['G4', { glideMode: 1 }, 1, { glideAlways: 1 }],   // ADR-103 migration
  ['G5', { oversample: 0 }, 3, { os: 1 }], ['G5', { oversample: 1 }, 3, { os: 2 }],
];
function t3(c) {
  const f = [];
  const tested = new Set(MERGE_CASES.map((m) => m[0]));
  for (const a of c.accounting.filter((x) => x.fate === 'MERGES')) if (!tested.has(a.row)) f.push(`MERGES row ${a.row} (${a.key}) has no case`);
  for (const [row, params, schema, exp] of MERGE_CASES) {
    const p = P.portPreset({ schema, params }, c, 'syn');
    for (const [k, want] of Object.entries(exp)) {
      if (k === 'unported') { if (!(want in p.unported.noTarget)) f.push(`row ${row}: ${want} should be carried unported`); continue; }
      if (k === 'noteHas') { if (!p.report.some((r) => r.row === row && r.note.includes(want))) f.push(`row ${row}: report does not say "${want}"`); continue; }
      if (!near(p.params[k], want)) f.push(`row ${row} ${JSON.stringify(params)}: ${k} = ${p.params[k]}, want ${want}`);
    }
  }
  return f;
}

/* ------------------------------------------------------------------ T4 --- */
function t4(c) {
  const f = [];
  for (const a of c.accounting.filter((x) => x.fate === 'RETIRED')) {
    const v = offDefault(a);
    const p = P.portPreset({ schema: 3, params: { [a.key]: v } }, c, 'syn');
    const line = p.report.find((r) => r.key === a.key);
    if (!line || line.outcome !== 'retired' || !line.note.startsWith('NON-DEFAULT')) f.push(`RETIRED row ${a.row} ${a.key} not reported as a non-default loss`);
    if (a.key in p.params) f.push(`RETIRED row ${a.row} ${a.key} carried in params`);
    const r = c.rules[a.row];
    if (r.target && p.params[r.target] === v) f.push(`RETIRED row ${a.row} ${a.key} carried as ${r.target}`);
    for (const [b, bucket] of Object.entries(p.unported)) if (bucket && typeof bucket === 'object' && a.key in bucket) f.push(`RETIRED row ${a.row} ${a.key} carried in unported.${b}`);
  }
  return f;
}

/* ------------------------------------------------------------------ T5 --- */
// The frozen order read HERE, line by line, not through the tool's parser.
const ORDER = readFileSync(join(ROOT, 'tests/morph_order.txt'), 'utf8').split('\n').map((l) => l.trim()).filter((l) => /^\d+$/.test(l)).map(Number);
function t5(c) {
  const f = [];
  const keyOf = (id) => P.legacyKeyOfId(id, ctx);   // naming uses the real registry; the ORDER is what is under test
  for (const [layout, len] of [[2, 224], [5, 248], [7, 264], [8, 272], [9, 273], [undefined, 178], [undefined, 202], [undefined, 222]]) {
    const arr = Array.from({ length: len }, (_, j) => j + 0.25);
    const { values, undecoded } = P.decodeCornerArray(arr, layout === undefined ? 1 : layout, c);
    if (undecoded.length) f.push(`layout ${layout} len ${len}: ${undecoded.length} undecoded`);
    for (let j = 0; j < len; j++) if (values[keyOf(ORDER[j])] !== j + 0.25) { f.push(`layout ${layout} len ${len}: slot ${j} is not id ${ORDER[j]}`); break; }
  }
  // ADR-150: layout 1, exactly 224 — oscPitch (181/1181) sat right after the per-osc prefix (slot 161 = 1150).
  const arr = Array.from({ length: 224 }, (_, j) => j + 0.5);
  const { values } = P.decodeCornerArray(arr, 1, c);
  const spot = [[0, 1], [161, 1150], [162, 181], [163, 1181], [164, 57], [223, ORDER[221]]];
  for (const [j, id] of spot) if (values[keyOf(id)] !== j + 0.5) f.push(`ADR-150 layout: slot ${j} should be id ${id}`);
  // A decoded corner ports through the fates: detune (id 4) is slot 6, osc 2's (1004) slot 7.
  const corner = Array.from({ length: 273 }, () => 0);
  corner[ORDER.indexOf(4)] = 0.5; corner[ORDER.indexOf(1004)] = 0.3;
  const p = P.portCorner({ morphLayout: 9, cornerPreset: corner }, c, 'syn');
  if (!near(p.params.detune, 50)) f.push(`corner port: detune slot gave ${p.params.detune}, want 50`);
  if (p.unported.osc2['o1.detune'] !== 0.3) f.push('corner port: osc 2 detune not carried');
  if (p.category !== P.CATEGORY_CORNER) f.push('corner port: wrong category');
  return f;
}

/* ------------------------------------------------------------------ T6 --- */
function t6(c) {
  const f = [];
  const params = {};
  let i = 0;
  for (const e of c.registry.byId.values()) {
    params[e.key] = 0.001 * ++i;
    if (!e.global) params['o1.' + e.key] = 0.001 * ++i;
  }
  params['sub.level'] = 0.5;   // an engine-block key (SUB OSC prefix), outside the registry
  const json = { plugin: 'HYPERSAW', schema: 3, engine_revision: 1, params, morphLayout: 9,
    morphCorners: [0, 1, 2, 3].map((k) => ORDER.map((_, j) => k + j / 1000)), morphExempt: ORDER.map((_, j) => (j === 6 ? 1 : 0)),
    cornerNames: ['a', 'b', 'c', 'd'], modRoutes: '0:4:0.5;', intent: 'x', presetName: 'syn' };
  const p = P.portPreset(json, c, 'fallback');
  const seen = new Map();
  for (const r of p.report) seen.set(r.key, (seen.get(r.key) || 0) + 1);
  for (const k of Object.keys(params)) if (seen.get(k) !== 1) f.push(`param ${k} reported ${seen.get(k) || 0} times`);
  const retired = new Set(c.accounting.filter((a) => a.fate === 'RETIRED').map((a) => a.key));
  for (const r of p.report) {
    if (r.outcome === 'retired') { if (!retired.has(r.key)) f.push(`${r.key} dropped but not RETIRED`); continue; }
    const where = r.outcome === 'unported'
      ? [p.unported.noTarget, p.unported.osc2, p.unported.outsideAccounting].find((b) => r.key in b)
      : null;
    if (r.outcome === 'unported' && (!where || where[r.key] !== params[r.key])) f.push(`${r.key}: unported but not recoverable`);
    if ((r.outcome === 'mapped' || r.outcome === 'merged') && !(r.target in p.params)) f.push(`${r.key}: ${r.outcome} to missing ${r.target}`);
  }
  if (!p.unported.morph || p.unported.morph.corners.length !== 4 || p.unported.morph.corners[2][P.legacyKeyOfId(ORDER[10], c)] !== 2 + 10 / 1000) f.push('morph corners not carried keyed');
  if (!p.unported.morph || p.unported.morph.exempt.join() !== P.legacyKeyOfId(ORDER[6], c)) f.push('morph exempt set not carried keyed');
  if (p.unported.modRoutes !== '0:4:0.5;' || p.unported.intent !== 'x') f.push('modRoutes / intent not carried');
  if (p.name !== 'syn') f.push('presetName not used as the name');
  return f;
}

/* ------------------------------------------------------------------ T7 --- */
function t7() {
  const f = [];
  if (!P.outDirIsSafe(join(ROOT, 'local', 'legacy-presets'))) f.push('local/legacy-presets/ is not git-ignored');
  if (P.outDirIsSafe(join(ROOT, 'tools'))) f.push('a tracked in-repo directory was accepted as output');
  return f;
}

/* ------------------------------------------------------------------ T8 --- */
function t8(c) {
  const f = [];
  const json = { schema: 2, params: { n: 12, detune: 0.3, K: 0.2, shape: 0.4 }, morphCorners: [[1, 2], [3], [], [4]] };
  const a = JSON.stringify(P.portPreset(json, c, 'syn')), b = JSON.stringify(P.portPreset(json, c, 'syn'));
  if (a !== b) f.push('two ports of one preset differ');
  for (const p of [P.portPreset(json, c, 'syn'), P.portCorner({ cornerPreset: [0.1] }, c, 'syn')]) {
    if (p.params.b1on !== 0 || p.params.b2on !== 0 || p.params.base !== 2) f.push(`${p.legacy.source}: blades not off / base not Saw`);
    if ('bend' in p.params) f.push('bend (the wheel) written as patch state');
  }
  return f;
}

/* ------------------------------------------------------------- run it --- */
let bad = 0;
const run = (name, fails) => {
  if (fails.length) { bad++; console.log(`FAIL ${name}:`); for (const x of fails.slice(0, 12)) console.log('   ' + x); }
  else console.log(`ok   ${name}`);
};
run('T1 coverage: every ACCOUNTING row has one rule, fates agree, targets exist', t1(ctx));
run('T2 every SURVIVES row round-trips (and its osc-2 twin)', t2(ctx));
run(`T3 every MERGES conversion matches its stated formula (${MERGE_CASES.length} cases)`, t3(ctx));
run('T4 every RETIRED row is reported and carried nowhere', t4(ctx));
run('T5 corner arrays decode by position against tests/morph_order.txt', t5(ctx));
run('T6 nothing silently lost (every registry key, both oscillators)', t6(ctx));
run('T7 privacy: output is git-ignored, a tracked dir is refused', t7());
run('T8 blades off, base Saw, deterministic', t8(ctx));

/* MUST-FAIL CONTROLS: the same detectors on planted faults. */
const withRules = (patch) => ({ ...ctx, rules: { ...ctx.rules, ...patch } });
const controls = [
  ['wrong conversion (detune x 10)', t3(withRules({ 4: { ...ctx.rules[4], fwd: (v) => v * 10 } }))],
  ['a RETIRED row carried (shape mapped)', t4(withRules({ 53: { fate: 'RETIRED', kind: 'map', target: 'morph' } }))],
  ['corner decoded one slot off', t5({ ...ctx, morph: { ...ctx.morph, ids: ctx.morph.ids.slice(1) } })],
  ['a rule whose fate drifted (dist as MERGES)', t1(withRules({ 2: { ...ctx.rules[2], fate: 'MERGES' } }))],
];
for (const [name, fails] of controls) {
  if (fails.length) console.log(`ok   control caught: ${name} (${fails.length})`);
  else { bad++; console.log(`FAIL control NOT caught: ${name}`); }
}
console.log(`port_legacy_presets_check: ${bad ? 'FAIL' : 'PASS'} — 8 oracles, ${controls.length} must-fail controls`);
process.exit(bad ? 1 : 0);
