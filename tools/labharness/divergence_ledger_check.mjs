/*
 * divergence_ledger_check.mjs — the ADR-187 divergence ledger for SCALPEL, docs/port/divergences.json,
 * says what the composed engine does. HYPERSAW, 2026-09-29, ROADMAP B355 (records PR #849, branch
 * lead-records-141), ADR-189: "The ADR-187 divergence ledger starts here: docs/port/divergences.json
 * plus a check."
 * WIRED: ./verify fast.
 *
 *   node tools/labharness/divergence_ledger_check.mjs      (exit 1 on any red row; ~3 s)
 *
 * WHAT A LEDGER ENTRY PROMISES, AND WHAT IS CHECKED (ADR-187 §5: "every divergence is ledgered and
 * checked"; the parity target is the composed engine, docs/design/scalpel-horde-engine.js, §3):
 *   L1  SHAPE. Every entry carries id, adr, status, flag, description, oracle {file, blob}, target,
 *       scope, evidence and js_limit (null or a statement); ids are unique. status is `built` or
 *       `planned` (a planned entry names its ROADMAP owner and has no flag yet: it is listed so the
 *       ledger is the one place a reader finds every divergence, and nothing is checked of it but its
 *       shape).
 *   L2  THE ORACLE IS THE ONE THE LEDGER WAS WRITTEN AGAINST. Each entry pins the git blob id of the
 *       oracle it diverges from; the file's blob id today (computed from its bytes, the git way, so no
 *       git is needed) must equal it. A changed oracle is a spec change (reference/** is protected):
 *       every divergence must then be re-read against it, and this row is what says so.
 *   L3  EVERY BUILT FLAG EXISTS in the engine's `d` and DEFAULTS TO 0, and every evidence path exists.
 *   L4  ALL OFF IS BIT-IDENTICAL to the engine before the first divergence: the ledger's `allOff`
 *       scenarios (presets from reference/scalpel/data/presets.json, a fixed two-note script, seeded
 *       Math.random) render to the SHA-256 fingerprints pinned there, which were taken on the engine at
 *       `allOff.engine.commit` (blob `allOff.engine.blob`). Rendered twice: with the flags absent (the
 *       defaults) and with every built flag written 0.
 *   L5  EVERY BUILT FLAG IS WIRED: switched on alone, it changes at least one scenario's fingerprint.
 *       A flag that changes nothing would pass L4 for the wrong reason.
 * MUST-FAIL CONTROLS (LIBRARY L0032: a detector that shares the assumption it measures confirms
 * whatever you expect), each run through the same functions as the rows above:
 *   C1  an entry whose flag the engine does not have (L3 must reject it);
 *   C2  a pinned fingerprint off by one hex digit (L4 must reject it);
 *   C3  an engine whose constructor defaults one flag ON (L4 must reject it: all-off no longer is);
 *   C4  an oracle blob pin off by one digit (L2 must reject it).
 * Deterministic, no model calls, no clock. Both references are required, never edited.
 */
import { readFileSync, existsSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const require = createRequire(import.meta.url);
export const LEDGER = 'docs/port/divergences.json';

/* the engine exactly as the lab and composed_engine_check build it */
export function loadComposed(engineFile) {
  const RazorCore = require(join(root, 'reference/scalpel/prototype/razor-core.js'));
  const { makeComposedEngine, swarmSourceFromHtml } = require(engineFile || join(root, 'docs/design/scalpel-horde-engine.js'));
  return makeComposedEngine(RazorCore, swarmSourceFromHtml(readFileSync(join(root, 'reference/swarmsaw.html'), 'utf8')));
}
function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
/* git's blob id: sha1 of "blob <bytes>\0<content>", so the pin reads without git (CI clones shallow) */
export const blobId = rel => { const b = readFileSync(join(root, rel)); return createHash('sha1').update(`blob ${b.length}\0`).update(b).digest('hex'); };

const PRESETS = JSON.parse(readFileSync(join(root, 'reference/scalpel/data/presets.json'), 'utf8')).presets;
/* one scenario: the preset, then `over`, notes 57 and 64 struck at 0, 12032 samples (94 blocks of 128)
   at 48 kHz, Math.random seeded (the lab's convention) and restored; SHA-256 of the float32 L then R,
   first 16 hex digits (composed_engine_check ZERO's form) */
export function fingerprint(Composed, preset, over, seed) {
  const p = PRESETS.find(x => x.name === preset);
  if (!p) throw new Error('divergence_ledger_check: preset missing: ' + preset);
  const saved = Math.random;
  Math.random = mulberry32(seed >>> 0);
  try {
    const c = new Composed(48000); c.set(Object.assign({}, p.params, over || {})); Object.assign(c.s, c.t);
    const n = 12032, L = new Float32Array(n), R = new Float32Array(n);
    c.noteOn(57, 440 * Math.pow(2, -12 / 12), 0.9); c.noteOn(64, 440 * Math.pow(2, -5 / 12), 0.9);
    for (let i = 0; i < n; i += 128) c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    return createHash('sha256').update(Buffer.from(L.buffer)).update(Buffer.from(R.buffer)).digest('hex').slice(0, 16);
  } finally { Math.random = saved; }
}

/* ---------------------------------------------------------------- the rules, as functions the controls reuse */
const FIELDS = ['id', 'adr', 'status', 'description', 'oracle', 'target', 'scope', 'evidence', 'js_limit'];
function shapeErrors(led) {
  const e = [], ids = new Set();
  for (const d of led.divergences) {
    for (const f of FIELDS) if (!(f in d)) e.push(`${d.id || '?'}: no ${f}`);
    if (ids.has(d.id)) e.push(`${d.id}: duplicate id`); ids.add(d.id);
    if (d.status === 'built') { if (!d.flag) e.push(`${d.id}: built but no flag`); }
    else if (d.status === 'planned') { if (!d.owner) e.push(`${d.id}: planned but no owner`); }
    else e.push(`${d.id}: status ${d.status} is neither built nor planned`);
    if (d.js_limit !== null && typeof d.js_limit !== 'string') e.push(`${d.id}: js_limit is neither null nor a statement`);
  }
  return e;
}
const built = led => led.divergences.filter(d => d.status === 'built');
function oracleErrors(led) {
  const e = [];
  for (const d of led.divergences) { const now = blobId(d.oracle.file); if (now !== d.oracle.blob) e.push(`${d.id}: ${d.oracle.file} is blob ${now.slice(0, 12)}, pinned ${d.oracle.blob.slice(0, 12)}`); }
  return e;
}
function flagErrors(led, Composed) {
  const e = [], d0 = new Composed(48000).d;
  for (const d of built(led)) {
    if (!(d.flag in d0)) e.push(`${d.id}: flag ${d.flag} is not an engine key`);
    else if (d0[d.flag] !== 0) e.push(`${d.id}: flag ${d.flag} defaults to ${d0[d.flag]}, not 0`);
    for (const p of [].concat(d.evidence)) if (!existsSync(join(root, p))) e.push(`${d.id}: evidence ${p} does not exist`);
  }
  return e;
}
function allOffErrors(led, Composed) {
  const e = [], zero = Object.fromEntries(built(led).map(d => [d.flag, 0]));
  for (const [name, want] of Object.entries(led.allOff.fingerprints)) {
    const a = fingerprint(Composed, name, null, led.allOff.seed), b = fingerprint(Composed, name, zero, led.allOff.seed);
    if (a !== want || b !== want) e.push(`${name}: ${a} (defaults) / ${b} (flags written 0), pinned ${want}`);
  }
  return e;
}

/* ---------------------------------------------------------------- main */
const main = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (main) {
  let red = 0;
  const row = (ok, id, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${id.padEnd(4)} ${text}`); if (!ok) red++; };
  const led = JSON.parse(readFileSync(join(root, LEDGER), 'utf8'));
  const Composed = loadComposed();
  const B = built(led);

  let e = shapeErrors(led);
  row(!e.length, 'L1', `${led.divergences.length} entries (${B.map(d => d.id).join(', ')} built; ${led.divergences.filter(d => d.status === 'planned').map(d => d.id).join(', ') || 'none'} planned)${e.length ? ': ' + e.join('; ') : ''}`);
  e = oracleErrors(led);
  row(!e.length, 'L2', `every entry's oracle is the pinned blob (${led.divergences[0].oracle.file} ${led.divergences[0].oracle.blob.slice(0, 12)})${e.length ? ': ' + e.join('; ') : ''}`);
  e = flagErrors(led, Composed);
  row(!e.length, 'L3', `built flags ${B.map(d => d.flag).join(', ')} are engine keys defaulting to 0; evidence present${e.length ? ': ' + e.join('; ') : ''}`);
  e = allOffErrors(led, Composed);
  row(!e.length, 'L4', `all off = the engine at ${led.allOff.engine.commit}: ${Object.keys(led.allOff.fingerprints).length} scenarios, defaults and flags written 0${e.length ? ': ' + e.join('; ') : ''}`);
  for (const d of B) {
    const moved = Object.entries(led.allOff.fingerprints).filter(([name, want]) => fingerprint(Composed, name, { [d.flag]: 1 }, led.allOff.seed) !== want).map(([n]) => n);
    row(moved.length > 0, 'L5', `${d.id} ${d.flag} on changes ${moved.length} of ${Object.keys(led.allOff.fingerprints).length} scenarios (${moved.join(', ') || 'none'})`);
  }

  /* must-fail controls: the same functions on planted faults */
  const plant = f => { const x = JSON.parse(JSON.stringify(led)); f(x); return x; };
  e = flagErrors(plant(x => { built(x)[0].flag = 'aaNoSuchFlag'; }), Composed);
  row(e.length > 0, 'C1', `CONTROL an entry naming a flag the engine lacks is caught: ${e[0] || 'NOT CAUGHT'}`);
  const k0 = Object.keys(led.allOff.fingerprints)[0];
  e = allOffErrors(plant(x => { const f = x.allOff.fingerprints[k0]; x.allOff.fingerprints[k0] = (f[0] === '0' ? '1' : '0') + f.slice(1); }), Composed);
  row(e.length > 0, 'C2', `CONTROL a pin off by one digit is caught: ${e[0] || 'NOT CAUGHT'}`);
  const OnByDefault = class extends Composed { constructor(sr) { super(sr); this.d[B[B.length - 1].flag] = 1; } };
  e = allOffErrors(led, OnByDefault);
  row(e.length > 0, 'C3', `CONTROL an engine defaulting ${B[B.length - 1].flag} ON is caught: ${e[0] || 'NOT CAUGHT'}`);
  e = oracleErrors(plant(x => { const b = x.divergences[0].oracle.blob; x.divergences[0].oracle.blob = (b[0] === '0' ? '1' : '0') + b.slice(1); }));
  row(e.length > 0, 'C4', `CONTROL an oracle pin off by one digit is caught: ${e[0] || 'NOT CAUGHT'}`);

  console.log(`\n${red ? 'RED' : 'GREEN'} — divergence_ledger_check: ${B.length} built divergence(s), ${red} row(s) failed`);
  process.exit(red ? 1 : 0);
}
