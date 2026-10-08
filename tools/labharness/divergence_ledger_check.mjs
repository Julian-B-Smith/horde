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
 *   L3  EVERY BUILT FLAG EXISTS in the engine's `d` and DEFAULTS TO ITS ENTRY'S `default` (0 when absent),
 *       and every evidence path exists.
 *   L4  ALL OFF IS BIT-IDENTICAL to the engine before the first divergence: the ledger's `allOff`
 *       scenarios (presets from reference/scalpel/data/presets.json, a fixed two-note script, seeded
 *       Math.random) render to the SHA-256 fingerprints pinned there, which were taken on the engine at
 *       `allOff.engine.commit` (blob `allOff.engine.blob`), with every built flag written 0. AND THE
 *       DEFAULTS ARE WHAT THE LEDGER SAYS: with the flags absent, each scenario renders to its allOff pin,
 *       except where a DEFAULT-ON entry (B382) claims it in `moves` {preset: fingerprint at the
 *       defaults}; then it renders to that claim. A claimed scenario that did not move is red (ADR-187
 *       §5: a claimed-but-unchanged fixture), and so is one claimed by two entries (their joint render
 *       needs a pin of its own).
 *   L5  EVERY BUILT FLAG IS WIRED: switched on alone (every other built flag written 0), it changes at
 *       least one allOff scenario's fingerprint or its own `inScope` scenario. A flag that changes nothing
 *       would pass L4 for the wrong reason.
 *   L6  EVERY BUILT FLAG STAYS IN ITS DECLARED SCOPE: switched on alone, it leaves each scenario in its
 *       entry's `outOfScope` list bit-identical to all flags off (the same fingerprint() script). D1 names
 *       a sine-carrier FM preset and a Band-limit-on sync saw (ADR-189 A1's narrowed scope); D2 and D3
 *       name presets with no feedback and no cross-mod. The entry's `inScope` scenario must change (the
 *       row's own must-fail control, C5: the same comparison on a scenario the flag does reach). A
 *       scenario is a preset name, or {preset, over, sr} (B382: parameters written over the preset, and a
 *       sample rate other than 48 kHz, so a divergence that needs a parameter moved, or a rate, to show
 *       can state it).
 *   L7  EVERY SwarmSynth PATCH IS LEDGERED, AND ONLY THOSE (B382). The engine's registered SwarmSynth
 *       text patches (Composed.swarmPatches, its SWARM_PATCHES) and the built entries whose oracle is
 *       reference/swarmsaw.html are the same set, id for id and flag for flag: a patch cannot enter the
 *       engine without its entry, and an entry cannot claim a patch the engine does not make.
 * TWO PIN SETS, ON PURPOSE: L4's fingerprints here (this script: notes 57 + 64 at 0, 12032 samples,
 * seed 45909) and composed_engine_check's AA0 (its own renderWith: two notes and a release, 24064
 * samples, seed 0xB355) were both taken on main d443eb6. They pin the same fact through two different
 * scripts, so a change that happened to leave one script's samples alone is still caught by the other.
 * MUST-FAIL CONTROLS (LIBRARY L0032: a detector that shares the assumption it measures confirms
 * whatever you expect), each run through the same functions as the rows above:
 *   C1  an entry whose flag the engine does not have (L3 must reject it);
 *   C2  a pinned fingerprint off by one hex digit (L4 must reject it);
 *   C3  an engine whose constructor defaults one default-0 flag ON (L4 must reject it: the defaults no
 *       longer are the pins);
 *   C4  an oracle blob pin off by one digit (L2 must reject it);
 *   C5  per built flag, its `inScope` scenario planted into its `outOfScope` list (L6 must reject it);
 *   C6  per default-ON entry that claims `moves`, an engine defaulting its flag OFF (L4 must reject it:
 *       the claimed scenarios no longer move);
 *   C7  a planted SwarmSynth entry the engine does not register (L7 must reject it);
 *   C8  per PER-PLATFORM claim: its pin for this platform off by one digit (L4 must reject it), and the
 *       same claim read on a platform it has no pin for (L4 must SKIP it, never pass it).
 * PER-PLATFORM PINS (B382, ADR-187 item 7: digests are pinned per canonical platform). A `moves` value is
 *   a fingerprint, or {platform: fingerprint} keyed by Node's process.platform (darwin, linux, win32) for
 *   a render whose float32 bits differ between platforms. L4 compares it with the pin for the platform
 *   it runs on; with no pin for that platform it prints a SKIP row naming the scenario and why, and that
 *   scenario is not counted as proven. Found on M1 (2026-09-30): Crunch horde at the defaults hashes
 *   57a4d13ff66d7a9f on macOS and f07ffc9432bcb49d on CI's Linux; on this Mac the same two hashes are
 *   the two outcomes of a 1e-12 relative perturbation of the swarm (M1's coefficient moved 1e5 ULP), so
 *   the scenario sits next to a discrete event and any last-bit difference upstream picks one side.
 * DEFAULT-ON ENTRIES (B382; ADR-187 A1: turning a divergence on by default is one divergence per PR).
 *   An entry with `default: 1` carries `ruled` (the human ruling that turned it on) and, when it moves an
 *   allOff scenario at the defaults, `moves`. With every built flag written 0 the engine is still the
 *   engine before the first divergence (L4), so each divergence stays attributable whatever its default.
 * Deterministic, no model calls, no clock. Both references are required, never edited.
 */
import './sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
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
   at 48 kHz (or `sr`, B382), Math.random seeded (the lab's convention) and restored; SHA-256 of the
   float32 L then R, first 16 hex digits (composed_engine_check ZERO's form) */
export function fingerprint(Composed, preset, over, seed, sr) {
  const p = PRESETS.find(x => x.name === preset);
  if (!p) throw new Error('divergence_ledger_check: preset missing: ' + preset);
  const saved = Math.random;
  Math.random = mulberry32(seed >>> 0);
  try {
    const c = new Composed(sr || 48000); c.set(Object.assign({}, p.params, over || {})); Object.assign(c.s, c.t);
    const n = 12032, L = new Float32Array(n), R = new Float32Array(n);
    c.noteOn(57, 440 * Math.pow(2, -12 / 12), 0.9); c.noteOn(64, 440 * Math.pow(2, -5 / 12), 0.9);
    for (let i = 0; i < n; i += 128) c.render(L.subarray(i, i + 128), R.subarray(i, i + 128));
    return createHash('sha256').update(Buffer.from(L.buffer)).update(Buffer.from(R.buffer)).digest('hex').slice(0, 16);
  } finally { Math.random = saved; }
}

/* ---------------------------------------------------------------- the rules, as functions the controls reuse */
const FIELDS = ['id', 'adr', 'status', 'description', 'oracle', 'target', 'scope', 'evidence', 'js_limit'];
const BUILT_FIELDS = ['flag', 'outOfScope', 'inScope'];
/* a scope scenario: a preset name, or {preset, over, sr} (B382) */
const scen = x => (typeof x === 'string' ? { preset: x, over: null, sr: 48000 } : { preset: x.preset, over: x.over || null, sr: x.sr || 48000 });
const scenName = x => { const s = scen(x); return s.preset + (s.over ? ' ' + JSON.stringify(s.over) : '') + (s.sr !== 48000 ? ` @${s.sr}` : ''); };
const defaultOf = d => (d.default === undefined ? 0 : d.default);
function shapeErrors(led) {
  const e = [], ids = new Set();
  for (const d of led.divergences) {
    for (const f of FIELDS) if (!(f in d)) e.push(`${d.id || '?'}: no ${f}`);
    if (ids.has(d.id)) e.push(`${d.id}: duplicate id`); ids.add(d.id);
    if (d.status === 'built') { for (const f of BUILT_FIELDS) if (!d[f] || (f === 'outOfScope' && !d[f].length)) e.push(`${d.id}: built but no ${f}`); }
    else if (d.status === 'planned') { if (!d.owner) e.push(`${d.id}: planned but no owner`); }
    else e.push(`${d.id}: status ${d.status} is neither built nor planned`);
    if (d.js_limit !== null && typeof d.js_limit !== 'string') e.push(`${d.id}: js_limit is neither null nor a statement`);
    if (defaultOf(d) !== 0 && defaultOf(d) !== 1) e.push(`${d.id}: default ${d.default} is neither 0 nor 1`);
    if (defaultOf(d) === 1 && (d.status !== 'built' || typeof d.ruled !== 'string')) e.push(`${d.id}: default 1 needs status built and a 'ruled' statement (ADR-187 A1)`);
    if (d.moves !== undefined && (defaultOf(d) !== 1 || !Object.values(d.moves).every(pinShape))) e.push(`${d.id}: moves needs default 1 and 16-hex fingerprints, bare or per platform (${PLATFORMS.join(', ')})`);
    for (const m of Object.keys(d.moves || {})) if (!(m in led.allOff.fingerprints)) e.push(`${d.id}: moves ${m}, which is not an allOff scenario`);
  }
  return e;
}
const built = led => led.divergences.filter(d => d.status === 'built');
const allZero = led => Object.fromEntries(built(led).map(d => [d.flag, 0]));
function oracleErrors(led) {
  const e = [];
  for (const d of led.divergences) { const now = blobId(d.oracle.file); if (now !== d.oracle.blob) e.push(`${d.id}: ${d.oracle.file} is blob ${now.slice(0, 12)}, pinned ${d.oracle.blob.slice(0, 12)}`); }
  return e;
}
function flagErrors(led, Composed) {
  const e = [], d0 = new Composed(48000).d;
  for (const d of built(led)) {
    if (!(d.flag in d0)) e.push(`${d.id}: flag ${d.flag} is not an engine key`);
    else if (d0[d.flag] !== defaultOf(d)) e.push(`${d.id}: flag ${d.flag} defaults to ${d0[d.flag]}, not ${defaultOf(d)}`);
    for (const p of [].concat(d.evidence)) if (!existsSync(join(root, p))) e.push(`${d.id}: evidence ${p} does not exist`);
  }
  return e;
}
/* a `moves` pin: a fingerprint, or {platform: fingerprint} (header, PER-PLATFORM PINS) */
const PLATFORMS = ['darwin', 'linux', 'win32'];
const HEX16 = f => typeof f === 'string' && /^[0-9a-f]{16}$/.test(f);
const pinShape = v => HEX16(v) || (v !== null && typeof v === 'object' && Object.keys(v).length > 0 &&
  Object.entries(v).every(([k, f]) => PLATFORMS.includes(k) && HEX16(f)));
const pinFor = (v, platform) => (HEX16(v) ? { f: v } : v[platform] ? { f: v[platform], per: platform }
  : { skip: `no pin for platform ${platform} (pinned for ${Object.keys(v).join(', ')})` });
/* the fingerprint each allOff scenario must have at the DEFAULTS: its pin, or the one default-on claim on it */
function defaultsWant(led, platform) {
  const want = {}, e = [];
  for (const [name, f] of Object.entries(led.allOff.fingerprints)) {
    const by = built(led).filter(d => defaultOf(d) === 1 && d.moves && name in d.moves);
    if (by.length > 1) e.push(`${name}: moved by ${by.map(d => d.id).join(' and ')} at once (a joint move needs its own pin)`);
    want[name] = by.length ? Object.assign(pinFor(by[0].moves[name], platform), { by: by[0].id }) : { f, by: null };
  }
  return { want, e };
}
/* -> errors; errors.skips lists the claims with no pin for `platform` (reported, never passed) */
function allOffErrors(led, Composed, platform) {
  platform = platform || process.platform;
  const zero = allZero(led), { want, e } = defaultsWant(led, platform), skips = [];
  for (const [name, pin] of Object.entries(led.allOff.fingerprints)) {
    const a = fingerprint(Composed, name, null, led.allOff.seed), b = fingerprint(Composed, name, zero, led.allOff.seed), w = want[name];
    if (b !== pin) e.push(`${name}: ${b} with every flag written 0, pinned ${pin}`);
    if (w.skip) skips.push(`${name}: ${a} at the defaults, claimed by ${w.by}'s moves, ${w.skip}`);
    else if (a !== w.f) e.push(`${name}: ${a} at the defaults, ${w.by ? `claimed by ${w.by}'s moves${w.per ? ` (${w.per} pin)` : ''}` : 'pinned'} ${w.f}`);
  }
  e.skips = skips;
  return e;
}

/* L6: each built flag, on alone, leaves its out-of-scope scenarios bit-identical to all off */
function scopeErrors(led, Composed) {
  const e = [], zero = allZero(led);
  for (const d of built(led)) for (const x of d.outOfScope) {
    const s = scen(x), base = Object.assign({}, zero, s.over || {});
    const off = fingerprint(Composed, s.preset, base, led.allOff.seed, s.sr), on = fingerprint(Composed, s.preset, Object.assign({}, base, { [d.flag]: 1 }), led.allOff.seed, s.sr);
    if (on !== off) e.push(`${d.id}: ${d.flag} on changes ${scenName(x)} (${off} -> ${on}), outside its declared scope`);
  }
  return e;
}
/* L7: the engine's SwarmSynth patches and the ledger's swarmsaw.html entries are one set */
const SWARM_ORACLE = 'reference/swarmsaw.html';
function patchErrors(led, Composed) {
  const e = [], reg = Composed.swarmPatches, ent = built(led).filter(d => d.oracle.file === SWARM_ORACLE);
  for (const x of reg) { const d = ent.find(y => y.id === x.id); if (!d) e.push(`${x.id}: the engine patches SwarmSynth, and no built entry ledgers it`); else if (d.flag !== x.flag) e.push(`${x.id}: the engine's flag is ${x.flag}, the ledger's ${d.flag}`); }
  for (const d of ent) if (!reg.some(x => x.id === d.id)) e.push(`${d.id}: ledgered against ${SWARM_ORACLE}, but the engine registers no such patch`);
  return e;
}
/* L5: what a flag on alone moves, among the allOff scenarios and its own inScope scenario */
function wiredBy(led, Composed, d) {
  const zero = allZero(led), on = Object.assign({}, zero, { [d.flag]: 1 });
  const moved = Object.entries(led.allOff.fingerprints).filter(([name, want]) => fingerprint(Composed, name, on, led.allOff.seed) !== want).map(([n]) => n);
  const s = scen(d.inScope), base = Object.assign({}, zero, s.over || {});
  const own = fingerprint(Composed, s.preset, Object.assign({}, base, { [d.flag]: 1 }), led.allOff.seed, s.sr) !== fingerprint(Composed, s.preset, base, led.allOff.seed, s.sr);
  return { moved, own };
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
  row(!e.length, 'L3', `built flags are engine keys at their ledgered defaults (${B.map(d => `${d.flag} ${defaultOf(d)}`).join(', ')}); evidence present${e.length ? ': ' + e.join('; ') : ''}`);
  e = allOffErrors(led, Composed);
  const claimed = B.filter(d => d.moves).map(d => `${d.id} moves ${Object.keys(d.moves).length}`);
  row(!e.length, 'L4', `all off = the engine at ${led.allOff.engine.commit}: ${Object.keys(led.allOff.fingerprints).length} scenarios with every flag written 0; the defaults are the pins${claimed.length ? ' except as claimed (' + claimed.join(', ') + ')' : ''}, on ${process.platform}${e.length ? ': ' + e.join('; ') : ''}`);
  for (const s of e.skips) console.log(`SKIP  L4   ${s} — this scenario's default is NOT proven here`);
  for (const d of B) {
    const { moved, own } = wiredBy(led, Composed, d);
    row(moved.length > 0 || own, 'L5', `${d.id} ${d.flag} on changes ${moved.length} of ${Object.keys(led.allOff.fingerprints).length} scenarios (${moved.join(', ') || 'none'}); its inScope ${scenName(d.inScope)} ${own ? 'changes' : 'DOES NOT change'}`);
  }

  e = scopeErrors(led, Composed);
  row(!e.length, 'L6', `each flag leaves its out-of-scope scenarios bit-identical: ${B.map(d => `${d.id} {${d.outOfScope.map(scenName).join(', ')}}`).join('; ')}${e.length ? ': ' + e.join('; ') : ''}`);

  e = patchErrors(led, Composed);
  row(!e.length, 'L7', `the engine's SwarmSynth patches are the ledger's ${SWARM_ORACLE} entries (${Composed.swarmPatches.map(x => x.id).join(', ') || 'none'})${e.length ? ': ' + e.join('; ') : ''}`);

  /* must-fail controls: the same functions on planted faults */
  const plant = f => { const x = JSON.parse(JSON.stringify(led)); f(x); return x; };
  e = flagErrors(plant(x => { built(x)[0].flag = 'aaNoSuchFlag'; }), Composed);
  row(e.length > 0, 'C1', `CONTROL an entry naming a flag the engine lacks is caught: ${e[0] || 'NOT CAUGHT'}`);
  const k0 = Object.keys(led.allOff.fingerprints)[0];
  e = allOffErrors(plant(x => { const f = x.allOff.fingerprints[k0]; x.allOff.fingerprints[k0] = (f[0] === '0' ? '1' : '0') + f.slice(1); }), Composed);
  row(e.length > 0, 'C2', `CONTROL a pin off by one digit is caught: ${e[0] || 'NOT CAUGHT'}`);
  const off0 = B.filter(d => defaultOf(d) === 0), c3 = off0[off0.length - 1];
  const OnByDefault = class extends Composed { constructor(sr) { super(sr); this.d[c3.flag] = 1; } };
  e = allOffErrors(led, OnByDefault);
  row(e.length > 0, 'C3', `CONTROL an engine defaulting ${c3.flag} ON is caught: ${e[0] || 'NOT CAUGHT'}`);
  e = oracleErrors(plant(x => { const b = x.divergences[0].oracle.blob; x.divergences[0].oracle.blob = (b[0] === '0' ? '1' : '0') + b.slice(1); }));
  row(e.length > 0, 'C4', `CONTROL an oracle pin off by one digit is caught: ${e[0] || 'NOT CAUGHT'}`);
  for (const d of B) {
    e = scopeErrors(plant(x => { const y = x.divergences.find(z => z.id === d.id); y.outOfScope = [y.inScope]; }), Composed);
    row(e.length > 0, 'C5', `CONTROL ${d.id}'s in-scope scenario (${scenName(d.inScope)}) planted out of scope is caught: ${e[0] || 'NOT CAUGHT'}`);
  }
  for (const d of B.filter(x => defaultOf(x) === 1 && x.moves)) {
    const OffByDefault = class extends Composed { constructor(sr) { super(sr); this.d[d.flag] = 0; } };
    e = allOffErrors(led, OffByDefault);
    row(e.length > 0, 'C6', `CONTROL an engine defaulting ${d.id}'s ${d.flag} OFF is caught: ${e[0] || 'NOT CAUGHT'}`);
  }

  e = patchErrors(plant(x => { x.divergences.push({ id: 'MX', status: 'built', flag: 'noSuchPatch', oracle: { file: SWARM_ORACLE } }); }), Composed);
  row(e.length > 0, 'C7', `CONTROL a ledgered SwarmSynth divergence the engine does not make is caught: ${e[0] || 'NOT CAUGHT'}`);
  for (const d of B.filter(x => x.moves)) for (const [name, v] of Object.entries(d.moves)) {
    if (HEX16(v)) continue;
    const here = pinFor(v, process.platform);
    if (here.f) {
      e = allOffErrors(plant(x => { const m = x.divergences.find(z => z.id === d.id).moves[name]; m[process.platform] = (here.f[0] === '0' ? '1' : '0') + here.f.slice(1); }), Composed);
      row(e.length > 0, 'C8', `CONTROL ${d.id}'s ${process.platform} pin for ${name} off by one digit is caught: ${e[0] || 'NOT CAUGHT'}`);
    }
    e = allOffErrors(led, Composed, 'aix');
    row(e.skips.some(s => s.startsWith(name + ':')) && !e.some(s => s.startsWith(name + ':')), 'C8',
      `CONTROL ${d.id}'s claim on ${name} read on a platform it has no pin for (aix) is SKIPPED, not passed: ${e.skips.find(s => s.startsWith(name + ':')) || 'NOT SKIPPED'}`);
  }

  console.log(`\n${red ? 'RED' : 'GREEN'} — divergence_ledger_check: ${B.length} built divergence(s), ${red} row(s) failed`);
  process.exit(red ? 1 : 0);
}
