/*
 * space.mjs — the patch space of the composed SCALPEL × horde engine, shared by the
 * B316 tools (gen_dependency_tree, gauntlet, their checks). HYPERSAW, 2026-09-27,
 * ROADMAP B316 (records PR #810, branch lead-records-122), phases P1-P3.
 *
 * WHAT LIVES HERE, AND WHY NOTHING IS RE-LISTED BY HAND:
 *   - the ENGINE: docs/design/scalpel-horde-engine.js's makeComposedEngine over the
 *     PROTECTED oracle reference/scalpel/prototype/razor-core.js (required, never
 *     edited) and reference/swarmsaw.html's DSP section (sliced by its banners, the
 *     engine's own swarmSourceFromHtml). Same construction as
 *     tools/labharness/composed_engine_check.mjs.
 *   - the PARAMETER TABLE: labels, kinds, ranges, TAPERS, tiers and ids are the
 *     SCALPEL lab's own table (docs/design/scalpel-interface-lab.html, the `R(...)`
 *     rows between `const E_MODE` and `const TIER_RANK`, which the lab itself calls
 *     "a copy of the accounting"), and position<->value is the lab's own
 *     toPos/fromPos. Both are SLICED OUT OF THE LAB AND EVALUATED, so a lab edit to a
 *     range or taper reaches the sampler without anyone remembering; a missing anchor
 *     throws rather than silently slicing the wrong span (extract_core.mjs's rule).
 *     Defaults are the ENGINE's constructor values (t, d, os), read from an instance,
 *     exactly as the lab reads them (its oracleDefaults()), plus the lab's b1on: 1.
 *   - the lab's THREE STATED MAPPINGS onto the engine are honoured the lab's way
 *     (oracleParams): blade 1 off => w 0; the pitch rows (octave/semi/fine) and the
 *     tab power (enable) are NOT part of the patch space here — the played note is
 *     the gauntlet's variable, not the patch's.
 *   - the CONDITION GRAMMAR of dependency_tree.json and its evaluator (evalCond).
 *
 * DETERMINISM: every render seeds Math.random with mulberry32 around the engine's
 * lifetime (the lab's convention — RazorCore draws Math.random; the swarm draws its
 * own seeded streams) and restores it after. No clock is read here; the gauntlet's
 * CPU metric reads process.hrtime OUTSIDE the render calls, in gauntlet.mjs.
 */
import '../labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

export const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
export const SR = 48000;                                  // the SCALPEL lab's rate
const require = createRequire(import.meta.url);

export const FILES = {
  oracle: 'reference/scalpel/prototype/razor-core.js',
  engine: 'docs/design/scalpel-horde-engine.js',
  swarm: 'reference/swarmsaw.html',
  lab: 'docs/design/scalpel-interface-lab.html',
  accounting: 'docs/scalpel/ACCOUNTING.md',
};
export const readRepo = rel => readFileSync(join(ROOT, rel), 'utf8');

export function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/* ---------------------------------------------------------------- engine */
let ENGINE = null;
export function loadEngine() {
  if (ENGINE) return ENGINE;
  const RazorCore = require(join(ROOT, FILES.oracle));
  const { makeComposedEngine, swarmSourceFromHtml } = require(join(ROOT, FILES.engine));
  const Composed = makeComposedEngine(RazorCore, swarmSourceFromHtml(readRepo(FILES.swarm)));
  ENGINE = { RazorCore, Composed };
  return ENGINE;
}

/* ---------------------------------------------------------------- the lab's table */
function slice(src, a, b, what) {
  const i = src.indexOf(a), j = i < 0 ? -1 : src.indexOf(b, i + a.length);
  if (i < 0 || j < 0) throw new Error(`space.mjs: ${what}: anchors "${a}" .. "${b}" not found in ${FILES.lab}; ` +
    'the lab changed shape — update the anchors deliberately');
  return src.slice(i, j);
}
let LAB = null;
export function loadLab() {
  if (LAB) return LAB;
  const html = readRepo(FILES.lab);
  const table = slice(html, 'const E_MODE', 'const TIER_RANK', 'parameter table');
  const curves = slice(html, 'function toPos(', 'function fmtHz(', 'toPos/fromPos');
  const frac = x => x - Math.floor(x);
  const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
  const out = new Function('frac', 'clamp', '"use strict";\n' + table + '\n' + curves +
    '\nreturn { SP, toPos, fromPos, E_MODE, E_WAVE, E_BASE, E_MIRROR, E_LOCK, E_MSHAPE, E_RULE, E_LAW };')(frac, clamp);
  LAB = Object.assign(out, { html });
  return LAB;
}

/* ---------------------------------------------------------------- the space */
/* The patch space = the lab's rows that the ENGINE voices: kinds c (continuous),
   e (enum), t (toggle), s (string), restricted to keys the engine knows (its t ∪ d
   ∪ os, the lab's ORK) plus the lab-mapped b1on. Rows of kind x (drawn, not voiced)
   and the lab-only rows are out, with the reason recorded by the tree. */
const LAB_ONLY = { enable: 'tab power, a lab mapping onto gain (not a patch value)',
  octave: 'pitch, a lab mapping onto bend: the played note is the gauntlet\'s variable',
  semi: 'pitch, a lab mapping onto bend: the played note is the gauntlet\'s variable',
  fine: 'pitch, a lab mapping onto bend: the played note is the gauntlet\'s variable',
  master: 'the lab\'s output gain node, after the engine' };
let SPACE = null;
export function loadSpace() {
  if (SPACE) return SPACE;
  const { Composed } = loadEngine(), lab = loadLab();
  const inst = new Composed(SR);
  const engineKeys = new Set(Object.keys(inst.t).concat(Object.keys(inst.d), ['os']));
  const defaults = Object.assign({}, inst.t, inst.d, { os: inst.os, b1on: 1 });
  const params = [], excluded = [];
  for (const key of Object.keys(lab.SP)) {
    const sp = lab.SP[key];
    const voiced = key === 'b1on' || engineKeys.has(key);
    if (!voiced || !'cets'.includes(sp.kind)) {
      excluded.push({ key, kind: sp.kind, why: LAB_ONLY[key] || (sp.kind === 'x' ? 'kind x: a horde row the engine does not voice (drawn, not voiced)' : 'not an engine key') });
      continue;
    }
    params.push({ key, label: sp.label, id: sp.id, tier: sp.tier, kind: sp.kind,
      min: sp.min, max: sp.max, curve: sp.curve, unit: sp.unit,
      opts: sp.opts ? sp.opts.filter(o => !o[2]).map(o => o[0]) : null,
      disabledOpts: sp.opts ? sp.opts.filter(o => o[2]).map(o => ({ v: o[0], why: o[2] })) : [],
      default: defaults[key] });
  }
  /* engine keys the lab does not draw: fixed at the engine default (named, never hidden) */
  const unsampled = [...engineKeys].filter(k => !lab.SP[k]).sort().map(k => ({ key: k, fixed: defaults[k] }));
  SPACE = { params, byKey: Object.fromEntries(params.map(p => [p.key, p])), excluded, unsampled, defaults };
  return SPACE;
}

/* value <-> taper position, the lab's own curves; enums/toggles by index */
export function valueAt(p, pos) {
  const lab = loadLab();
  if (p.kind === 'e') return p.opts[Math.min(p.opts.length - 1, Math.floor(pos * p.opts.length))];
  if (p.kind === 't') return pos < 0.5 ? 0 : 1;
  if (p.kind === 's') return p.default;
  const v = lab.fromPos(lab.SP[p.key], pos);
  return p.curve === 'int' ? Math.round(v) : v;
}
export function posOf(p, v) { return loadLab().toPos(loadLab().SP[p.key], v); }

/* what the engine receives: the lab's oracleParams() mapping (b1on off => w 0) */
export function engineParams(patch) {
  const o = {};
  for (const k in patch) if (k !== 'b1on') o[k] = patch[k];
  if (!patch.b1on) o.w = 0;
  return o;
}

/* ---------------------------------------------------------------- rendering */
/* script: { n: total samples, ev: [[sample, 'on'|'off', note, vel]] sorted }.
   Returns interleaved-free Float32 L/R. Voices are the engine's own (poly 6). */
export const mtof = n => 440 * Math.pow(2, (n - 69) / 12);
export function render(patch, script, opt) {
  opt = opt || {};
  const { Composed } = loadEngine();
  const sr = opt.sr || SR, scale = sr / SR, seed = (opt.seed === undefined ? 0xB316 : opt.seed) >>> 0;
  const saved = Math.random;
  Math.random = mulberry32(seed);
  try {
    const c = new Composed(sr);
    c.set(engineParams(patch));
    Object.assign(c.s, c.t);
    const n = Math.round(script.n * scale), L = new Float32Array(n), R = new Float32Array(n);
    let pos = 0;
    const timing = opt.timing ? [] : null;
    const upto = end => {
      while (pos < end) {
        const e = Math.min(end, pos + (opt.block || 128));
        if (timing) { const t0 = process.hrtime.bigint(); c.render(L.subarray(pos, e), R.subarray(pos, e)); timing.push([e - pos, Number(process.hrtime.bigint() - t0)]); }
        else c.render(L.subarray(pos, e), R.subarray(pos, e));
        pos = e;
      }
    };
    for (const [t, kind, note, vel] of script.ev) {
      upto(Math.round(t * scale));
      if (kind === 'on') c.noteOn(note, mtof(note), vel);
      else c.noteOff(note);
    }
    upto(n);
    return { L, R, sr, timing };
  } finally { Math.random = saved; }
}

/* ---------------------------------------------------------------- conditions */
/* THE CONDITION GRAMMAR (dependency_tree.json `grammar`), JSON only so any consumer
   (morph's greying, the mod matrix's destination list, a C++ readiness gate) can
   evaluate it without this file:
     true | false
     {"eq":[key,v]} {"ne":[key,v]} {"in":[key,[v…]]} {"ge":[key,x]} {"gt":[key,x]}
     {"le":[key,x]} {"absgt":[key,x]}
     {"all":[c…]} {"any":[c…]} {"not":c} {"pred":name}   (name ∈ the tree's predicates) */
export function evalCond(c, P, preds) {
  if (c === true || c === false) return c;
  const op = Object.keys(c)[0], a = c[op];
  switch (op) {
    case 'all': return a.every(x => evalCond(x, P, preds));
    case 'any': return a.some(x => evalCond(x, P, preds));
    case 'not': return !evalCond(a, P, preds);
    case 'pred': if (!preds[a]) throw new Error('evalCond: unknown predicate ' + a); return evalCond(preds[a], P, preds);
    case 'eq': return P[a[0]] === a[1];
    case 'ne': return P[a[0]] !== a[1];
    case 'in': return a[1].includes(P[a[0]]);
    case 'ge': return P[a[0]] >= a[1];
    case 'gt': return P[a[0]] > a[1];
    case 'le': return P[a[0]] <= a[1];
    case 'absgt': return Math.abs(P[a[0]]) > a[1];
    default: throw new Error('evalCond: unknown operator ' + op);
  }
}
/* the keys a condition reads, predicates expanded (the tree's depends_on) */
export function condKeys(c, preds, out) {
  out = out || new Set();
  if (c === true || c === false) return out;
  const op = Object.keys(c)[0], a = c[op];
  if (op === 'all' || op === 'any') a.forEach(x => condKeys(x, preds, out));
  else if (op === 'not') condKeys(a, preds, out);
  else if (op === 'pred') condKeys(preds[a], preds, out);
  else out.add(a[0]);
  return out;
}
