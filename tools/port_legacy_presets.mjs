/*
 * port_legacy_presets.mjs — B312: port the legacy plugin's USER presets and
 * corner presets onto the composed engine's keys. HYPERSAW, 2026-09-27,
 * dispatched by the horde lead on ROADMAP B312 (records PR #801, branch
 * lead-records-117); ADR-186 revision 2, item (ii). The human: "Sure, let's try
 * to port over my user presets; it will help ground the whole process."
 *
 * WHAT IT IS. An OFFLINE, ONE-WAY tool. It is never part of a plugin loader and
 * nothing in src/ includes it (ADR-186 (ii): "a one-way OFFLINE preset importer
 * as a tool, never in the loader"). It reads the legacy store (read-only), maps
 * every value through docs/scalpel/ACCOUNTING.md's per-parameter fate, and
 * writes a presets file shaped like reference/scalpel/data/presets.json's
 * entries plus a per-preset report.
 *
 *   node tools/port_legacy_presets.mjs [--store DIR] [--out DIR]
 *
 * --store defaults to the legacy root (src/gui/preset_store.h presetRootFor,
 * macOS: ~/Library/Application Support/LiftedTruck/HYPERSAW), resolved from
 * the home directory at run time: no absolute path is written in this file.
 * --out defaults to <repo>/local/legacy-presets/, which .gitignore ignores.
 *
 * PRIVACY (hard rule, B312). The ported presets are the human's creative data
 * and this repo is public. So: the output directory must be git-IGNORED if it
 * lies inside the repo (checked with `git check-ignore`, refused otherwise:
 * safety by construction, not by remembering), and stdout carries COUNTS
 * ONLY. No preset name or value is ever printed. Names and values go to the
 * two output files and nowhere else.
 *
 * THE MAPPING DATA IS ACCOUNTING ITSELF. §1.1 (82 per-oscillator rows) and
 * §1.2 (12 global rows) are PARSED from docs/scalpel/ACCOUNTING.md at run time:
 * each row's key, range, default and fate come from there, never a copy. The
 * RULES table below adds only what ACCOUNTING states in prose: the target key
 * and the conversion. The check (tools/labharness/port_legacy_presets_check.mjs)
 * fails if a row has no rule, a rule has no row, or a rule's fate disagrees
 * with the row's. A merge whose conversion ACCOUNTING does not state is NOT
 * guessed: the value is carried in `unported` and the report says why.
 *
 * THE TARGET IS PROVISIONAL. The keys are the composed engine's as they stand
 * (docs/design/scalpel-horde-engine.js on RazorCore, reference/scalpel/
 * prototype/razor-core.js, both READ, never edited) plus the SCALPEL lab's
 * table rows for horde rows the engine does not voice
 * (docs/design/scalpel-interface-lab.html `R('<key>'`). horde 2's schema is
 * not designed yet (ADR-186 §5); when it is, this tool re-targets.
 *
 * LEGACY FORMATS (read from the code that writes them):
 *   preset  src/hypersaw_clap.cpp stateJson(): {"plugin":"HYPERSAW","schema":N,
 *           ["engine_revision":R,"build":B,] "params":{key:v, "o1.key":v (osc 2),
 *           "sub.key":v, ...}, ["morphLayout":L,] ["cornerNames":[4],]
 *           "morphCorners":[[...]x4], ["morphExempt":[...],] ["modRoutes":"..",]
 *           ["intent":"..",] ["presetName":".."]}. Schema 1..3; the header keys
 *           are absent before B100 (revision 1 by definition).
 *   corner  liveCornerJson(): {["morphLayout":L,] "cornerPreset":[...]} — a
 *           POSITIONAL array read slot by slot against tests/morph_order.txt,
 *           exactly as morphSlotMap() reads it (see decodeCornerArray).
 *   The loader's migrations that touch a mapped row are mirrored: schema < 2
 *   glideMode >= 0.5 means 2 (ADR-103), and a patch with no "enable" key
 *   renders both oscillators (pre-ADR-100). An absent key is its DEFAULT
 *   (B181 note 6, "a load is a load"), the default read from ACCOUNTING.
 *
 * Deterministic: no clock, no random draw. Same store in, same bytes out.
 */
import './labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync, readdirSync, statSync, mkdirSync, writeFileSync, existsSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join, resolve, relative, isAbsolute, basename } from 'node:path';
import { homedir } from 'node:os';
import { registryText, gitIgnored } from './labharness/sandbox_facts.mjs';   // the two child processes, answered by the launcher

export const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
export const CATEGORY_PRESET = 'Your presets (legacy)';
export const CATEGORY_CORNER = 'Your corners (legacy)';
// ACCOUNTING §3 option A: the SCALPEL revision plays min(n, cap) members, cap 9; the
// composed engine cannot exceed it (the oracle hardcodes 9 members, §1.8.6).
export const MEMBER_CAP = 9;

/* ------------------------------------------------------------ the RULES ---
   One entry per ACCOUNTING row (§1.1 by number, §1.2 by G-number). `fate` is
   restated so the check can hold it against the parsed table. Kinds:
     map      SURVIVES, the composed engine or the lab has the key: same value.
     merge    MERGES with a conversion ACCOUNTING states (cited in `why`).
              `fwd(v, ctx)` returns the target value, or undefined when the
              stated conversion does not cover v (then: unported, reported).
     retired  RETIRED: dropped, and reported (never carried).
     noTarget SURVIVES (or a MERGES with no stated conversion) but nothing in
              the composed engine or the lab receives it yet: carried in
              `unported`, reported.
   `lab: true` marks a target that is a SCALPEL-lab table key rather than an
   engine key (the check finds its `R('<key>'` row in the lab). */
const NO = (why) => ({ kind: 'noTarget', why });
export const RULES = {
  1: { fate: 'MERGES', kind: 'merge', target: 'N', why: 'row 1: horde id, member count; §3 option A clamps to the cap (9) and must SAY so',
       fwd: (v) => Math.min(v, MEMBER_CAP), note: (v) => (v > MEMBER_CAP ? `clamped ${v} -> ${MEMBER_CAP} (§3)` : '') },
  2: { fate: 'SURVIVES', kind: 'map', target: 'dist' },
  3: { fate: 'SURVIVES', kind: 'map', target: 'seed' },
  4: { fate: 'MERGES', kind: 'merge', target: 'detune', why: 'row 4: knob x 100 c (SCALPEL d c = knob d/100)', fwd: (v) => v * 100 },
  5: { fate: 'SURVIVES', kind: 'map', target: 'h.law', note: (v) => (v === 3 ? 'law 3 (tempo grid) is not in SwarmSynth: the composed engine plays it as ERB (B298 gap 2)' : '') },
  6: { fate: 'MERGES', kind: 'merge', target: 'K', why: 'row 6: horde law wins, same K (the composed engine runs horde\'s law)', fwd: (v) => v },
  7: { fate: 'SURVIVES', kind: 'map', target: 'onset', note: (v) => (v < 0 ? 'onset < 0: the JS reference is symmetric, horde\'s C++ is bipolar (B298 gap 1)' : '') },
  8: { fate: 'SURVIVES', kind: 'map', target: 'dissolve' },
  9: { fate: 'SURVIVES', kind: 'map', target: 'driftDepth' },
  10: { fate: 'SURVIVES', kind: 'map', target: 'h.driftRate' },
  11: { fate: 'SURVIVES', kind: 'map', target: 'rtone', lab: true },
  12: { fate: 'SURVIVES', ...NO('output stage not composed (scalpel-horde-engine.js "NOT COMPOSED THIS ROUND")') },
  13: { fate: 'MERGES', kind: 'merge', target: 'width', why: 'row 13: horde law (0..1.5) wins, same value',
        fwd: (v) => v, note: (v) => (v > 1 ? 'above the oracle\'s 0..1 (horde super-width; output stage not composed)' : '') },
  14: { fate: 'MERGES', kind: 'merge', target: 'aa', why: 'row 14 / §1.5: clean = 1, raw = 0',
        fwd: (v) => (v === 1 ? 1 : v === 0 ? 0 : undefined), unstated: 'a fractional Digital has no stated aa value' },
  15: { fate: 'MERGES', kind: 'merge', target: 'gain', why: 'row 15: horde law wins, same value (output stage not composed: loudness differs)', fwd: (v) => v },
  16: { fate: 'MERGES', kind: 'merge', target: 'phaseMode', why: 'row 16: random = retrig off (0), aligned = retrig on (1)', fwd: (v) => (v >= 0.5 ? 1 : 0) },
  17: { fate: 'MERGES', kind: 'merge', target: 'A', why: 'row 17 / §1.5: seconds -> ms', fwd: (v) => v * 1000 },
  18: { fate: 'MERGES', kind: 'merge', target: 'D', why: 'row 18 / §1.5: seconds -> ms', fwd: (v) => v * 1000 },
  19: { fate: 'MERGES', kind: 'merge', target: 'S', why: 'row 19: same meaning', fwd: (v) => v },
  20: { fate: 'MERGES', kind: 'merge', target: 'R', why: 'row 20 / §1.5: seconds -> ms', fwd: (v) => v * 1000 },
  21: { fate: 'SURVIVES', ...NO('tempo-grid law not in SwarmSynth') },
  22: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  23: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  24: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  25: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  26: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  27: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  28: { fate: 'SURVIVES', ...NO('dynamics/topology not in the composed engine') },
  29: { fate: 'MERGES', kind: 'merge', target: 'cScale', why: '§1.6.2: absK on = seconds (0), horde σ-scaling = cycles (1); not voiced by the composed engine',
        fwd: (v) => (v >= 0.5 ? 0 : 1) },
  30: { fate: 'SURVIVES', kind: 'map', target: 'octave', lab: true },
  31: { fate: 'SURVIVES', kind: 'map', target: 'semi', lab: true },
  32: { fate: 'SURVIVES', kind: 'map', target: 'fine', lab: true },
  33: { fate: 'SURVIVES', ...NO('phase scatter not passed to the composed swarm') },
  34: { fate: 'SURVIVES', ...NO('pan image not composed') },
  35: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  36: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  37: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  38: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  39: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  40: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  41: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  42: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  43: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  44: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  45: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  46: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  47: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  48: { fate: 'SURVIVES', ...NO('dynamics (A/B balance) not in the composed engine') },
  49: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  50: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  51: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  52: { fate: 'SURVIVES', ...NO('SPECTRA-only (parked)') },
  53: { fate: 'RETIRED', kind: 'retired' },
  54: { fate: 'SURVIVES', ...NO('per-member amplitude terms not composed') },
  55: { fate: 'SURVIVES', ...NO('per-member amplitude terms not composed') },
  56: { fate: 'SURVIVES', kind: 'map', target: 'driftMode' },
  57: { fate: 'SURVIVES', kind: 'map', target: 'keepPhase' },
  58: { fate: 'SURVIVES', ...NO('pan image not composed') },
  59: { fate: 'SURVIVES', ...NO('pan image not composed') },
  60: { fate: 'SURVIVES', kind: 'map', target: 'motionCenter' },
  61: { fate: 'SURVIVES', kind: 'map', target: 'harmReach' },
  62: { fate: 'SURVIVES', kind: 'map', target: 'stretchB' },
  63: { fate: 'SURVIVES', kind: 'map', target: 'spread' },
  64: { fate: 'SURVIVES', kind: 'map', target: 'anchor' },
  65: { fate: 'SURVIVES', kind: 'map', target: 'pivotMode' },
  /* Row 66 names panOrder but states no value map: §1.5 gives only "SCALPEL's fan
     ≈ horde's legacy", and horde's default (pitch fan) is "neither". The pan image
     is not composed either. So the value is carried, not converted; panOrder is
     set by §1.7's legacy default instead (see LEGACY_DEFAULTS). */
  66: { fate: 'MERGES', ...NO('row 66 states no value conversion (§1.5: horde\'s pitch fan is neither SCALPEL order); pan image not composed') },
  67: { fate: 'SURVIVES', ...NO('pan image not composed') },
  68: { fate: 'SURVIVES', ...NO('pan image not composed') },
  69: { fate: 'SURVIVES', ...NO('pan image not composed') },
  70: { fate: 'SURVIVES', ...NO('onset & scatter not passed to the composed swarm') },
  71: { fate: 'SURVIVES', ...NO('onset & scatter not passed to the composed swarm') },
  72: { fate: 'SURVIVES', ...NO('onset & scatter not passed to the composed swarm') },
  73: { fate: 'SURVIVES', ...NO('onset & scatter not passed to the composed swarm') },
  74: { fate: 'SURVIVES', ...NO('onset & scatter not passed to the composed swarm') },
  75: { fate: 'SURVIVES', ...NO('mute/solo are mixer state, no lab key') },
  76: { fate: 'SURVIVES', ...NO('mute/solo are mixer state, no lab key') },
  77: { fate: 'RETIRED', kind: 'retired' },
  78: { fate: 'RETIRED', kind: 'retired' },
  79: { fate: 'RETIRED', kind: 'retired' },
  80: { fate: 'RETIRED', kind: 'retired' },
  81: { fate: 'SURVIVES', kind: 'map', target: 'enable', lab: true },
  82: { fate: 'SURVIVES', ...NO('continuous osc pitch has no lab or engine key') },
  /* G1 + G2 are ONE target: polyMode 0 poly = Mono off; 1 mono = Mono on +
     Legato off; 2 legato = Mono on + Legato on (row G1). Converted jointly in
     portParams; G2's rule points at G1's. */
  G1: { fate: 'MERGES', kind: 'merge', target: 'polyMode', why: 'row G1: poly = Mono off, mono = Mono on + Legato off, legato = Mono on + Legato on', joint: 'G2' },
  G2: { fate: 'MERGES', kind: 'merge', target: 'polyMode', why: 'row G1 (with 32)', jointOf: 'G1' },
  G3: { fate: 'MERGES', kind: 'merge', target: 'glide', why: 'row G3: SCALPEL T ms = horde τ s x 3000', fwd: (v) => v * 3000 },
  G4: { fate: 'MERGES', kind: 'merge', target: 'glideAlways', why: 'row G4: overlapping = 0 (held note), always = 2',
        fwd: (v) => (v === 0 ? 0 : v === 2 ? 1 : undefined), unstated: 'glideMode 1 (last note ringing) has no SCALPEL twin' },
  G5: { fate: 'MERGES', kind: 'merge', target: 'os', why: 'row G5: Oversample 2x off = 1x, on = 2x', fwd: (v) => (v >= 0.5 ? 2 : 1) },
  G6: { fate: 'SURVIVES', kind: 'map', target: 'inertia' },
  G7: { fate: 'SURVIVES', kind: 'map', target: 'inertiaCurve' },
  G8: { fate: 'SURVIVES', ...NO('mono fold is output stage, not composed') },
  G9: { fate: 'SURVIVES', kind: 'map', target: 'freqGlide' },
  G10: { fate: 'SURVIVES', ...NO('(dev) vestigial Device row') },
  G11: { fate: 'SURVIVES', kind: 'map', target: 'master', lab: true },
  G12: { fate: 'SURVIVES', ...NO('voice cull has no lab or engine key') },
};

/* ACCOUNTING §1.7, the defaults a LEGACY patch needs on the new engine ("with
   blades off … must reproduce a rev-1 patch"): blade 1 off (the lab's `b1on`,
   §1.7's proposed row 300), blade 2 off, base wave Saw (2), balanced pan off
   (panOrder 1, the only non-balanced order). These are not values the legacy
   patch stored, so the report lists them as defaults, not as mappings. */
export const LEGACY_DEFAULTS = { b1on: 0, b2on: 0, base: 2, panOrder: 1 };
const LAB_ONLY_DEFAULT_KEYS = new Set(['b1on']);
// `bend` is the wheel (a performance input the lab drives from octave/semi/fine), never patch state.
const NOT_PATCH_STATE = new Set(['bend']);

/* ---------------------------------------------------------- the context --- */
function splitRow(line) {
  // Cells may hold an escaped pipe (row 6: 4K\|K\|·σ); split on unescaped ones only.
  return line.split(/(?<!\\)\|/).slice(1, -1).map((c) => c.trim());
}
function section(md, from, to) {
  const a = md.indexOf(from), b = md.indexOf(to, a + 1);
  if (a < 0 || b < 0) throw new Error(`ACCOUNTING: section "${from}" not found`);
  return md.slice(a, b);
}
/* Parses §1.1 and §1.2. Returns [{row, id, key, min, max, def, fate, global}]. */
export function parseAccounting(md) {
  const out = [];
  const take = (text, global) => {
    for (const line of text.split('\n')) {
      const m = /^\|\s*(G?\d+)\s*\|/.exec(line);
      if (!m) continue;
      const c = splitRow(line);
      const id = Number(/^(\d+)/.exec(c[1])[1]);
      const key = /`([^`]+)`/.exec(c[2])[1];
      const r = /^(-?[\d.]+)\.\.(-?[\d.]+)\s*\((-?[\d.]+)\)/.exec(c[3]);
      if (!r) throw new Error(`ACCOUNTING row ${m[1]}: range "${c[3]}" unparsed`);
      const fate = /\*\*([A-Z]+)\*\*/.exec(c[5])[1];
      out.push({ row: m[1], id, key, min: Number(r[1]), max: Number(r[2]), def: Number(r[3]), fate, global });
    }
  };
  take(section(md, '### 1.1', '### 1.2'), false);
  take(section(md, '### 1.2', '### 1.3'), true);
  return out;
}
/* tests/morph_order.txt: the frozen slot order. Returns { layout, ids }. */
export function parseMorphOrder(text) {
  let layout = 0;
  const ids = [];
  for (const raw of text.split('\n')) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    if (line.startsWith('layout')) { layout = Number(line.split(/\s+/)[1]); continue; }
    ids.push(Number(line));
  }
  return { layout, ids };
}
/* The legacy registry (id, key, global) from tools/registry_decl.py, REUSED
   rather than re-parsed here: it already strips comments before reading the
   kGlobalIds block, a trap it documents. */
export function loadRegistry(root = ROOT) {
  const txt = registryText(root);
  const byId = new Map(), byKey = new Map();
  for (const line of txt.split('\n')) {
    const [id, key, g] = line.split('\t');
    if (!key) continue;
    const e = { id: Number(id), key, global: g === '1' };
    byId.set(e.id, e); byKey.set(key, e);
  }
  return { byId, byKey };
}
/* The composed engine's own defaults, read from a live instance (never copied),
   the way the SCALPEL lab reads DEF from its oracle. */
export function engineDefaults(root = ROOT) {
  const require = createRequire(import.meta.url);
  const RazorCore = require(join(root, 'reference/scalpel/prototype/razor-core.js'));
  const { makeComposedEngine, swarmSourceFromHtml } = require(join(root, 'docs/design/scalpel-horde-engine.js'));
  const Composed = makeComposedEngine(RazorCore, swarmSourceFromHtml(readFileSync(join(root, 'reference/swarmsaw.html'), 'utf8')));
  const e = new Composed(44100);
  const out = {};
  for (const src of [e.t, e.d]) for (const k of Object.keys(src)) if (!NOT_PATCH_STATE.has(k)) out[k] = src[k];
  out.os = e.os;
  return out;
}
export function loadContext(root = ROOT) {
  const accounting = parseAccounting(readFileSync(join(root, 'docs/scalpel/ACCOUNTING.md'), 'utf8'));
  const byKey = new Map(accounting.map((a) => [a.key, a]));
  return {
    accounting, byKey,
    registry: loadRegistry(root),
    morph: parseMorphOrder(readFileSync(join(root, 'tests/morph_order.txt'), 'utf8')),
    defaults: engineDefaults(root),
    rules: RULES,
  };
}

/* ------------------------------------------------------- corner decode --- */
/* Where stored slot j lands: morphSlotMap() (src/hypersaw_clap.cpp), mirrored.
   Layout >= 2, or any array that is not exactly 224 long, is the frozen order
   read 1:1 as a PREFIX. A layout-1 (absent marker) array of exactly 224
   (kMorphAdr150Size) was written between ADR-150 and ADR-159, with oscPitch
   (181/1181) INSIDE the per-osc prefix: kParams walks per-osc rows in id order
   and 181 is the last, so it sat right after the prefix, and everything after
   shifts by two. Returns the id per stored slot (null = no live slot). */
export function slotIds(layout, len, ctx) {
  const order = ctx.morph.ids;
  if (layout >= 2 || len !== 224) return Array.from({ length: len }, (_, j) => (j < order.length ? order[j] : null));
  const isPerOsc = (id) => {
    const base = id >= 1000 && id < 2000 ? id - 1000 : id;
    const e = ctx.registry.byId.get(base);
    return base < 182 && e && !e.global;
  };
  let p = 0;
  while (p < order.length && isPerOsc(order[p])) p++;
  const late = new Set([181, 1181]);
  const legacy = order.slice(0, p).concat([181, 1181], order.slice(p).filter((id) => !late.has(id)));
  return Array.from({ length: len }, (_, j) => (j < legacy.length ? legacy[j] : null));
}
/* The legacy STATE KEY of an id: osc 1 rows and globals by their key, osc 2
   twins as "o1.<key>" (stateJson's convention), anything else (engine blocks,
   routing cells) as "id:<n>" so it is still carried, keyed, never dropped. */
export function legacyKeyOfId(id, ctx) {
  const e = ctx.registry.byId.get(id);
  if (e) return e.key;
  if (id >= 1000 && id < 2000) {
    const b = ctx.registry.byId.get(id - 1000);
    if (b && !b.global) return 'o1.' + b.key;
  }
  return 'id:' + id;
}
export function decodeCornerArray(arr, layout, ctx) {
  const ids = slotIds(layout, arr.length, ctx);
  const values = {}, undecoded = [];
  arr.forEach((v, j) => {
    if (ids[j] === null) undecoded.push(v);
    else values[legacyKeyOfId(ids[j], ctx)] = v;
  });
  return { values, undecoded };
}

/* ------------------------------------------------------------- the port --- */
/* `params` is a flat legacy {stateKey: value}. `opts.schema` applies the
   loader's schema migrations (presets only; a corner file has no schema and the
   corner loader applies none). Returns { params, unported, report }. */
export function portParams(legacy, ctx, opts = {}) {
  const rules = opts.rules || ctx.rules;
  const params = Object.assign({}, ctx.defaults, LEGACY_DEFAULTS);
  const unported = { noTarget: {}, osc2: {}, outsideAccounting: {} };
  const report = [];
  const seen = new Set();
  const has = (k) => Object.prototype.hasOwnProperty.call(legacy, k);
  const val = (a) => {
    let v = has(a.key) ? Number(legacy[a.key]) : a.def;
    // ADR-103: schema < 2 stored glideMode 1 meaning ALWAYS; the loader rewrites it to 2.
    if (a.key === 'glideMode' && opts.schema !== undefined && opts.schema < 2 && v >= 0.5) v = 2;
    // pre-ADR-100: no "enable" key means every oscillator rendered.
    if (a.key === 'enable' && !has('enable') && opts.enableMigration) v = 1;
    return v;
  };
  const line = (a, outcome, target, from, to, note) =>
    report.push({ key: a.key, row: a.row, fate: a.fate, outcome, target: target || '', from, to, stored: has(a.key), note: note || '' });

  for (const a of ctx.accounting) {
    seen.add(a.key);
    const r = rules[a.row];
    if (!r) throw new Error(`no rule for ACCOUNTING row ${a.row}`);
    const v = val(a);
    if (r.kind === 'retired') { line(a, 'retired', '', v, undefined, v === a.def ? 'at default' : 'NON-DEFAULT value lost'); continue; }
    if (r.kind === 'noTarget') { unported.noTarget[a.key] = v; line(a, 'unported', '', v, undefined, r.why); continue; }
    if (r.kind === 'map') {
      params[r.target] = v;
      line(a, 'mapped', r.target, v, v, r.note ? r.note(v) : '');
      continue;
    }
    // merges
    if (r.jointOf) { line(a, 'merged', r.target, v, params[r.target], 'converted with ' + r.jointOf); continue; }
    if (r.joint) {
      const legato = val(ctx.accounting.find((x) => x.row === r.joint));
      const to = v >= 0.5 ? (legato >= 0.5 ? 2 : 1) : 0;
      params[r.target] = to;
      line(a, 'merged', r.target, v, to, r.why);
      continue;
    }
    const to = r.fwd(v);
    if (to === undefined) { unported.noTarget[a.key] = v; line(a, 'unported', r.target, v, undefined, r.unstated + ' (MERGES, no stated conversion)'); continue; }
    params[r.target] = to;
    line(a, 'merged', r.target, v, to, [r.why, r.note ? r.note(v) : ''].filter(Boolean).join('; '));
  }
  // osc 2: the composed engine is ONE swarm, so every twin is carried, keyed.
  for (const k of Object.keys(legacy)) {
    if (seen.has(k)) continue;
    if (k.startsWith('o1.')) {
      const a = ctx.byKey.get(k.slice(3));
      unported.osc2[k] = legacy[k];
      report.push({ key: k, row: a ? a.row : '', fate: a ? a.fate : '', outcome: 'unported', target: '', from: legacy[k], to: undefined, stored: true, note: 'osc 2: the composed engine is one swarm' });
    } else {
      unported.outsideAccounting[k] = legacy[k];
      report.push({ key: k, row: '', fate: '', outcome: 'unported', target: '', from: legacy[k], to: undefined, stored: true, note: 'outside ACCOUNTING (FX, filters, sub, mod, note lane, …)' });
    }
  }
  return { params, unported, report };
}

const HEADER_KEYS = ['plugin', 'schema', 'engine_revision', 'build', 'morphLayout'];
export function portPreset(json, ctx, fallbackName, opts = {}) {
  if (!json || typeof json !== 'object' || !json.params || typeof json.params !== 'object') throw new Error('not a legacy preset (no "params")');
  const schema = json.schema === undefined ? 1 : Number(json.schema);
  const { params, unported, report } = portParams(json.params, ctx, { ...opts, schema, enableMigration: true });
  /* The loader's migrations (applyStateJson), named so the carried values read
     as the plugin would have played them. */
  const has = (k) => Object.prototype.hasOwnProperty.call(json.params, k);
  const migrations = [];
  if (!has('enable')) {
    migrations.push('pre-ADR-100: no "enable" key, so both oscillators render (o1.enable = 1)');
    if (!has('o1.enable')) unported.osc2['o1.enable'] = 1;
  }
  if (schema < 2 && has('glideMode') && Number(json.params.glideMode) >= 0.5) migrations.push('ADR-103: schema < 2 glideMode >= 0.5 read as 2');
  if (!has('noteLawLink') && has('glide')) migrations.push('pre-note-lane: glide without noteLawLink, so note lane = own settings + lag (ids 137/138, unported)');
  if (migrations.length) unported.migrations = migrations;
  const layout = json.morphLayout === undefined ? 1 : Number(json.morphLayout);
  if (Array.isArray(json.morphCorners)) {
    const corners = json.morphCorners.map((c) => decodeCornerArray(c, layout, ctx));
    unported.morph = {
      layout,
      cornerNames: json.cornerNames || null,
      corners: corners.map((c) => c.values),
      undecoded: corners.map((c) => c.undecoded),
    };
    if (Array.isArray(json.morphExempt)) {
      const ex = decodeCornerArray(json.morphExempt, layout, ctx);
      unported.morph.exempt = Object.keys(ex.values).filter((k) => Number(ex.values[k]) !== 0);
    }
  }
  for (const k of ['modRoutes', 'intent']) if (json[k] !== undefined) unported[k] = json[k];
  const other = Object.keys(json).filter((k) => !HEADER_KEYS.includes(k) && !['params', 'morphCorners', 'morphExempt', 'cornerNames', 'modRoutes', 'intent', 'presetName'].includes(k));
  if (other.length) unported.otherFields = Object.fromEntries(other.map((k) => [k, json[k]]));
  const legacy = { source: 'preset', schema, engine_revision: json.engine_revision === undefined ? 1 : json.engine_revision, morphLayout: layout };
  return { name: json.presetName || fallbackName, category: CATEGORY_PRESET, params, unported, legacy, report };
}
/* A corner preset is the morph field's members only: decode it, then port it
   like a preset whose every other key is absent (so at its default: B124's
   resetCorner rule, "what the file does not carry is the default"). */
export function portCorner(json, ctx, name, opts = {}) {
  if (!json || !Array.isArray(json.cornerPreset)) throw new Error('not a legacy corner preset (no "cornerPreset")');
  const layout = json.morphLayout === undefined ? 1 : Number(json.morphLayout);
  const { values, undecoded } = decodeCornerArray(json.cornerPreset, layout, ctx);
  const { params, unported, report } = portParams(values, ctx, opts);
  if (undecoded.length) unported.undecodedSlots = undecoded;
  return { name, category: CATEGORY_CORNER, params, unported, legacy: { source: 'corner', morphLayout: layout }, report };
}

/* ------------------------------------------------------------ privacy --- */
/* The output may not land anywhere git would track. Inside this repo it must be
   ignored; outside it, it is not this repo's business. */
export function outDirIsSafe(dir, root = ROOT) {
  const rel = relative(root, resolve(dir));
  if (rel.startsWith('..') || isAbsolute(rel)) return true;
  return gitIgnored(root, join(rel, 'probe.json'), true);
}

/* --------------------------------------------------------------- report --- */
const fmt = (v) => (v === undefined ? '' : typeof v === 'number' ? String(Number(v.toPrecision(10))) : JSON.stringify(v));
export function tally(report) {
  const t = { mapped: 0, merged: 0, retired: 0, dropped: 0, unported: 0 };
  for (const r of report) t[r.outcome]++;
  return t;
}
function reportMarkdown(entries, failures, totals) {
  const L = [];
  L.push('# Legacy preset port — report (B312)', '');
  L.push('LOCAL, GITIGNORED, PRIVATE: this file holds the human\'s preset names and values. Never commit it.', '');
  L.push('Target: the composed engine\'s keys as they stand (docs/design/scalpel-horde-engine.js on RazorCore), PROVISIONAL until horde 2\'s schema exists.');
  L.push('Blades are OFF in every ported preset (b1on 0, b2on 0): "your old swarm, on the new engine". Fates: docs/scalpel/ACCOUNTING.md §1.1/§1.2.', '');
  L.push(`Totals: ${entries.length} ported, ${failures.length} parse failures. Fields: mapped ${totals.mapped}, merged ${totals.merged}, retired ${totals.retired}, dropped ${totals.dropped}, unported ${totals.unported}.`, '');
  L.push('DROPPED is 0 by construction: ACCOUNTING\'s DROPPED rows are all SCALPEL bench-UI rows (§1.4 38, 50–53, 84), none of which a legacy patch holds.', '');
  for (const f of failures) L.push(`- PARSE FAILURE: ${f.kind}/${f.file}: ${f.error}`);
  for (const e of entries) {
    const t = tally(e.report);
    L.push('', `## ${e.name} — ${e.legacy.source}`, '');
    L.push(`legacy: ${JSON.stringify(e.legacy)} · mapped ${t.mapped} · merged ${t.merged} · retired ${t.retired} · dropped ${t.dropped} · unported ${t.unported}`, '');
    L.push('| legacy key | ACCOUNTING row | fate | outcome | target | legacy value | ported value | note |', '|---|---|---|---|---|---|---|---|');
    for (const r of e.report) {
      if (r.outcome === 'unported' && !r.row && r.note.startsWith('outside')) continue;   // summarised below
      L.push(`| \`${r.key}\` | ${r.row} | ${r.fate} | ${r.outcome} | ${r.target ? '`' + r.target + '`' : ''} | ${fmt(r.from)}${r.stored ? '' : ' (default)'} | ${fmt(r.to)} | ${r.note.replace(/\|/g, '/')} |`);
    }
    const out = e.report.filter((r) => r.outcome === 'unported' && !r.row && r.note.startsWith('outside')).map((r) => r.key);
    L.push('', `Outside ACCOUNTING, carried in \`unported.outsideAccounting\` (${out.length}): ${out.map((k) => '`' + k + '`').join(', ') || 'none'}.`);
    L.push(`§1.7 legacy defaults set: ${Object.entries(LEGACY_DEFAULTS).map(([k, v]) => `${k}=${v}`).join(', ')}.`);
  }
  return L.join('\n') + '\n';
}

/* ------------------------------------------------------------------ CLI --- */
function listJson(dir) {
  if (!existsSync(dir)) return [];
  // Non-recursive, like the plugin's own listing: presets/factory/ is the shipped bank, not the user's.
  return readdirSync(dir).filter((f) => f.endsWith('.json') && statSync(join(dir, f)).isFile()).sort();
}
export function runPort(store, ctx) {
  const entries = [], failures = [];
  const counts = { presetFiles: 0, cornerFiles: 0 };
  for (const [kind, fn] of [['presets', portPreset], ['corners', portCorner]]) {
    for (const f of listJson(join(store, kind))) {
      counts[kind === 'presets' ? 'presetFiles' : 'cornerFiles']++;
      try {
        entries.push(fn(JSON.parse(readFileSync(join(store, kind, f), 'utf8')), ctx, basename(f, '.json')));
      } catch (err) { failures.push({ kind, file: f, error: String(err.message || err) }); }
    }
  }
  return { entries, failures, counts };
}
function main(argv) {
  const arg = (flag, def) => { const i = argv.indexOf(flag); return i >= 0 ? argv[i + 1] : def; };
  const store = arg('--store', join(homedir(), 'Library', 'Application Support', 'LiftedTruck', 'HYPERSAW'));
  const out = arg('--out', join(ROOT, 'local', 'legacy-presets'));
  if (!outDirIsSafe(out)) {
    console.error('port_legacy_presets: REFUSED — the output directory is inside the repo and not git-ignored (the presets are private).');
    process.exit(2);
  }
  const ctx = loadContext();
  const { entries, failures, counts } = runPort(store, ctx);
  const totals = { mapped: 0, merged: 0, retired: 0, dropped: 0, unported: 0 };
  // The same tally over only the fields a file actually STORED (absent keys port as defaults).
  const stored = { mapped: 0, merged: 0, retired: 0, dropped: 0, unported: 0 };
  let retiredNonDefault = 0, clamped = 0, osc2Enabled = 0;
  for (const e of entries) {
    const t = tally(e.report);
    for (const k in totals) totals[k] += t[k];
    for (const r of e.report) if (r.stored) stored[r.outcome]++;
    retiredNonDefault += e.report.filter((r) => r.outcome === 'retired' && r.note.startsWith('NON')).length;
    clamped += e.report.filter((r) => r.row === '1' && r.note.startsWith('clamped')).length;
    if (e.legacy.source === 'preset' && Number(e.unported.osc2['o1.enable']) === 1) osc2Enabled++;
  }
  mkdirSync(out, { recursive: true });
  const presets = entries.map(({ name, category, params, unported, legacy }) => ({ name, category, params, unported, legacy }));
  writeFileSync(join(out, 'legacy-presets.json'), JSON.stringify({
    note: 'B312 legacy port. PRIVATE (the human\'s presets): local and git-ignored, never commit. Params are the composed engine\'s keys as they stand (PROVISIONAL); blades off. `unported` carries everything with no target yet.',
    presets,
  }, null, 1) + '\n');
  writeFileSync(join(out, 'report.md'), reportMarkdown(entries, failures, totals));
  // COUNTS ONLY on stdout: no name, no value (B312 privacy rule).
  console.log(`port_legacy_presets: ${counts.presetFiles} preset files, ${counts.cornerFiles} corner files; ported ${entries.length}, parse failures ${failures.length}`);
  console.log(`fields: mapped ${totals.mapped}, merged ${totals.merged}, retired ${totals.retired} (${retiredNonDefault} non-default), dropped ${totals.dropped}, unported ${totals.unported}; member clamps ${clamped}; osc 2 enabled in ${osc2Enabled} presets`);
  console.log(`of the fields the files stored: mapped ${stored.mapped}, merged ${stored.merged}, retired ${stored.retired}, dropped ${stored.dropped}, unported ${stored.unported}`);
  console.log(`wrote ${relative(ROOT, out) || out}/legacy-presets.json and report.md`);
}
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main(process.argv.slice(2));
