// Intent-bus golden generator (B89 phase 2a, ADR-176): drive the protected
// prototype's resolver headlessly and write per-tick fixtures for
// tools/intent_check.cpp. The model is sliced live from
// reference/intent-bus.html (extract_intent.mjs) — never forked, never copied.
//
// WHAT A FIXTURE IS. A flat JSON object with three key groups:
//
//   "b.*"  the BASE inputs — the complete resolver input set, read out of the
//          LIVE model after the case's setup, so whatever the prototype
//          actually holds is what the C++ is handed.
//   "c.*"  the CONTROL inputs — the same set after one named mutation.
//   "e.*"  the expectations, recorded per tick from the base run only.
//
// intent_check must see the base MATCH and the control MISMATCH. That is
// L0032's two-halved cure in one file: a control that must read zero (the
// base) and a corruption that must read non-zero (the control). A control
// that fails to move the PROTOTYPE is a vacuous control, so generation aborts
// on one — never silently retried until one fires (L0033).
//
// DETERMINISM. Every phase is set explicitly (global LFO phase, corner LFO
// phase, puck state, seeds), dt is fixed at 0.016 s, the tick count is fixed,
// and each case re-extracts a pristine model before mutating it. No
// wall-clock, no Math.random: the prototype's `reshuffle()` is seeded
// (SPEC §5.7) and the seeds are either drawn from a device seed or written in
// by hand. `--selfcheck` re-runs every case and compares bit-for-bit.
//
// Usage: node gen_intent_goldens.mjs [--selfcheck]
import { loadModel } from './extract_intent.mjs';
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const OUT = join(root, 'build-golden', 'intent');
const DT = 0.016;

// Global-LFO target encoding: params are their own index, the non-parameter
// targets are negative so one integer carries the whole select.
const T_NONE = -1, T_MORPHX = -2, T_MORPHY = -3, T_INTENTX = -4;
const SCOPE_CORNER = 0, SCOPE_GLOBAL = 1;

/* ---------------------------------------------------------------- helpers */

function indices(api) {
  const P = {}, I = {};
  api.P.forEach((p, i) => (P[p] = i));
  api.INT.forEach((n, i) => (I[n] = i));
  return { P, I, N: api.P.length, NI: api.INT.length, CN: api.CN };
}

function defaultCfg(N) {
  return {
    ticks: 60,
    // atom layout: one atom per parameter, then `home`, then `unison` — the
    // prototype's reshuffle() draw order exactly (§3.5).
    atomOf: Array.from({ length: N }, (_, i) => i),
    homeAtom: N, unisonAtom: N + 1, nAtoms: N + 2,
    useDrawnSeeds: 0, deviceSeed: 1024,
    coup: 0, sharedSeed: 0.5,
    morph2: null, switchTick: -1,     // second morph position, applied from this tick
    drag: null, dragTicks: 0,         // pointer held at `drag` for the first N ticks
    sweepIntent: null,                // non-pad intent driven by SWEEP(t)
    commitAt: -1,                     // run §7 commit after this tick's resolve
  };
}

// Reconcile the atom map with the prototype's per-parameter seeds: parameters
// sharing an atom must share a seed, which is how a LEAD GROUP (ADR-176
// decision 2) is expressed in a prototype that only knows per-parameter atoms.
function applySeeds(api, cfg, ix) {
  if (cfg.useDrawnSeeds) {
    const identity = cfg.atomOf.every((a, i) => a === i)
      && cfg.homeAtom === ix.N && cfg.unisonAtom === ix.N + 1 && cfg.nAtoms === ix.N + 2;
    if (!identity) throw new Error('useDrawnSeeds requires the prototype atom layout');
    api.S.seed = cfg.deviceSeed;
    api.reshuffle();
    // The shared seed is the draw AFTER the atoms — appended, so the per-atom
    // draws stay bit-identical to a stream without coupling (IntentCore::
    // drawSeeds carries the same argument).
    const r = api.mulberry32(cfg.deviceSeed);
    for (let i = 0; i < cfg.nAtoms; i++) r();
    cfg.sharedSeed = r();
  }
  const seeds = new Array(cfg.nAtoms).fill(null);
  for (let p = 0; p < ix.N; p++) {
    const a = cfg.atomOf[p];
    if (seeds[a] === null) seeds[a] = api.S.seeds[api.P[p]];
  }
  if (seeds[cfg.homeAtom] === null) seeds[cfg.homeAtom] = api.S.seeds.home;
  if (seeds[cfg.unisonAtom] === null) seeds[cfg.unisonAtom] = api.S.seeds.unison;
  for (let a = 0; a < cfg.nAtoms; a++)
    if (seeds[a] === null) throw new Error(`atom ${a} has no seed`);
  // write back so the prototype and the fixture agree atom for atom
  for (let p = 0; p < ix.N; p++) api.S.seeds[api.P[p]] = seeds[cfg.atomOf[p]];
  api.S.seeds.home = seeds[cfg.homeAtom];
  api.S.seeds.unison = seeds[cfg.unisonAtom];
  cfg.seeds = seeds;
}

function snapshot(api, cfg, ix) {
  const { N, NI, CN } = ix;
  const base = [], lo = [], hi = [], bind = [], homeX = [], homeY = [], unison = [];
  const cLfoTarget = [], cLfoDepth = [], cLfoScope = [];
  CN.forEach((c) => {
    const cr = api.corners[c];
    api.P.forEach((p) => { base.push(cr.base[p]); lo.push(cr.range[p][0]); hi.push(cr.range[p][1]); });
    api.INT.forEach((i) => api.P.forEach((p) => bind.push(cr.bind[i][p])));
    homeX.push(cr.home.x); homeY.push(cr.home.y); unison.push(cr.unison);
    cLfoTarget.push(cr.lfo.target in ix.P ? ix.P[cr.lfo.target] : T_NONE);
    cLfoDepth.push(cr.lfo.depth);
    cLfoScope.push(cr.lfo.scope === 'global' ? SCOPE_GLOBAL : SCOPE_CORNER);
  });
  const gt = api.S.gLfo.target;
  const gTarget = gt === 'morphX' ? T_MORPHX : gt === 'morphY' ? T_MORPHY
    : gt === 'intentX' ? T_INTENTX : (gt in ix.P ? ix.P[gt] : T_NONE);
  return {
    nParams: N, nIntents: NI, nAtoms: cfg.nAtoms, ticks: cfg.ticks, dt: DT,
    lfoRateParam: ix.P.lfoRate,
    steepness: api.S.steep, coup: cfg.coup,
    useDrawnSeeds: cfg.useDrawnSeeds, deviceSeed: cfg.deviceSeed,
    seeds: cfg.seeds.slice(), sharedSeed: cfg.sharedSeed,
    atomOf: cfg.atomOf.slice(), homeAtom: cfg.homeAtom, unisonAtom: cfg.unisonAtom,
    morphX: api.S.morph.x, morphY: api.S.morph.y,
    morph2X: cfg.morph2 ? cfg.morph2.x : api.S.morph.x,
    morph2Y: cfg.morph2 ? cfg.morph2.y : api.S.morph.y,
    switchTick: cfg.switchTick,
    dragTicks: cfg.dragTicks, dragX: cfg.drag ? cfg.drag.x : 0, dragY: cfg.drag ? cfg.drag.y : 0,
    latch: api.S.latch ? 1 : 0,
    puckX: api.S.puck.x, puckY: api.S.puck.y, puckVX: api.S.puck.vx, puckVY: api.S.puck.vy,
    knob: api.INT.map((i) => api.S.intents[i]),
    padDriven: api.INT.map((i) => (i === 'X' || i === 'Y' ? 1 : 0)),
    base, rangeLo: lo, rangeHi: hi, bind, homeXs: homeX, homeYs: homeY, unisonReq: unison,
    gTarget, gRate: api.S.gLfo.rate, gDepth: api.S.gLfo.depth,
    gPhase0: api.S.gLfo.phase, gArmor: api.S.gLfo.armor ? 1 : 0,
    cLfoTarget, cLfoDepth, cLfoScope, cPhase0: api.S.cLfoPhase,
    sweepIntent: cfg.sweepIntent ? ix.I[cfg.sweepIntent] : -1,
    commitAt: cfg.commitAt,
  };
}

// Drive the prototype. Ordering: apply the tick's schedule, resolve, RECORD,
// then commit if this is the commit tick — so tick `commitAt` is the last
// pre-commit observation and the tick after it shows the bake.
function drive(api, cfg, ix) {
  const { N, NI, CN } = ix;
  const e = {
    weights: [], owner: [], homeOwner: [], unisonOwner: [], unisonValue: [],
    final: [], clamped: [], intents: [], puck: [],
  };
  const m0 = { x: api.S.morph.x, y: api.S.morph.y };
  for (let t = 0; t < cfg.ticks; t++) {
    const m = (cfg.switchTick >= 0 && t >= cfg.switchTick && cfg.morph2) ? cfg.morph2 : m0;
    api.S.morph.x = m.x; api.S.morph.y = m.y;
    api.S.drag = (cfg.drag && t < cfg.dragTicks) ? { x: cfg.drag.x, y: cfg.drag.y } : null;
    if (cfg.sweepIntent) api.S.intents[cfg.sweepIntent] = SWEEP(t);
    api.resolve(DT);
    CN.forEach((c) => e.weights.push(api.S.weights[c]));
    api.P.forEach((p) => e.owner.push(CN.indexOf(api.S.owner[p])));
    api.P.forEach((p) => e.final.push(api.S.final[p]));
    api.P.forEach((p) => e.clamped.push(api.S.clamped[p] ? 1 : 0));
    api.INT.forEach((i) => e.intents.push(api.S.intents[i]));
    e.homeOwner.push(CN.indexOf(api.S.homeOwner));
    e.unisonOwner.push(CN.indexOf(api.S.unisonOwner));
    e.unisonValue.push(api.corners[api.S.unisonOwner].unison);
    e.puck.push(api.S.puck.x, api.S.puck.y);
    if (t === cfg.commitAt) api.commit();
  }
  // Post-run corner bases: the only observable §7's bake leaves behind.
  e.baseAfter = [];
  CN.forEach((c) => api.P.forEach((p) => e.baseAfter.push(api.corners[c].base[p])));
  // Only the KNOB intents: S.intents also carries the pad-derived X/Y, which
  // are an output of §4.4 and already compared per tick as `intents`. Mixing
  // them in here would compare a knob array against a resolved one.
  e.macroAfter = api.INT.filter((i) => i !== 'X' && i !== 'Y').map((i) => api.S.intents[i]);
  void N; void NI;
  return e;
}

function runVariant(def, control) {
  const api = loadModel();
  const ix = indices(api);
  const cfg = defaultCfg(ix.N);
  def.setup(api, cfg, ix);
  if (control) def.mutate(api, cfg, ix);
  applySeeds(api, cfg, ix);
  const inputs = snapshot(api, cfg, ix);
  const expect = drive(api, cfg, ix);
  return { api, ix, cfg, inputs, expect };
}

const flat = (o) => JSON.stringify(o);
function sameArrays(a, b) {
  for (const k of Object.keys(a)) if (flat(a[k]) !== flat(b[k])) return k;
  return null;
}

/* ------------------------------------------------------------------ cases */
// Each case: a setup that configures the prototype, a mutate that is the
// MUST-FAIL control, and an assert that pins what the case actually proves on
// the base run (so "this fixture covers T4" is measured, not asserted in prose).

const at = (x, y) => (api, cfg) => { api.S.morph.x = x; api.S.morph.y = y; };
const distinct = (owners, n) => new Set(owners).size >= n;

function ownersAt(e, ix, t) { return e.owner.slice(t * ix.N, (t + 1) * ix.N); }
function finalsAt(e, ix, t) { return e.final.slice(t * ix.N, (t + 1) * ix.N); }
function spread(e, ix, p) {
  let lo = Infinity, hi = -Infinity;
  for (let t = 0; t < e.final.length / ix.N; t++) {
    const v = e.final[t * ix.N + p];
    if (v < lo) lo = v; if (v > hi) hi = v;
  }
  return hi - lo;
}
// A slow macro sweep: something has to MOVE for a lock or an inert binding to
// be a claim rather than a coincidence of a static patch. It is an INPUT
// SCHEDULE, not a wrapper round resolve() — intent_check replays the same
// closed form, so the sweep is part of the fixture rather than a JS-only
// convenience. Only non-pad intents may be swept: resolve() overwrites X/Y.
const SWEEP = (t) => Math.sin(t * 0.13);
function sweepMacro(api, cfg, name) {
  if (name === 'X' || name === 'Y') throw new Error('cannot sweep a pad-driven intent');
  cfg.sweepIntent = name;
}

const CASES = [];

/* ---- T1: a locked (lo == hi) or narrow corner range is load-bearing ---- */
CASES.push({
  name: 't1-lock-held',
  proves: 'T1: with B owning everything, B\'s locked cutoff [.18,.18] does not move under a full INT sweep, and clamped[] lights',
  setup(api, cfg, ix) {
    at(1, 0)(api, cfg);                       // morph parked on B -> B owns every atom
    api.corners.B.range.cutoff = [0.18, 0.18];
    api.corners.B.bind.INT.cutoff = 0.6;      // a binding that WANTS to move it
    sweepMacro(api, cfg, 'INT');
  },
  mutate(api) { api.corners.B.range.cutoff = [0, 1]; },
  control: 'widen B.range.cutoff to [0,1] — the same sweep must now move it',
  assert(r, ix) {
    const p = ix.P.cutoff;
    if (spread(r.expect, ix, p) > 1e-12) throw new Error('cutoff moved under a lock');
    if (!r.expect.clamped.some((c, i) => c === 1 && i % ix.N === p)) throw new Error('clamp indicator never lit');
  },
});

CASES.push({
  name: 't1-lock-partial',
  proves: 'T1 under quantum: D locks drive to [0,.05]; on the ticks D owns drive the value is pinned there while the same sweep moves it freely elsewhere',
  setup(api, cfg, ix) {
    at(0.9, 0.9)(api, cfg); api.S.steep = 1;
    api.corners.D.range.drive = [0, 0.05];
    api.corners.D.bind.INT.drive = 0.8;
    sweepMacro(api, cfg, 'INT');
  },
  mutate(api) { api.corners.D.range.drive = [0, 1]; },
  control: 'widen D.range.drive to [0,1]',
  assert(r, ix) {
    const p = ix.P.drive;
    let sawD = false;
    for (let t = 0; t < r.cfg.ticks; t++)
      if (ownersAt(r.expect, ix, t)[p] === 3) {
        sawD = true;
        if (r.expect.final[t * ix.N + p] > 0.05 + 1e-12) throw new Error('D-owned drive escaped its range');
      }
    if (!sawD) throw new Error('D never owned drive — the case proves nothing');
  },
});

CASES.push({
  name: 't1-lock-globalmod',
  proves: 'T1 boundary: a DEVICE routing moves a locked parameter (armor off) — the lock binds intents and corner mods, not the device tier',
  setup(api, cfg, ix) {
    at(1, 0)(api, cfg);
    api.corners.B.range.cutoff = [0.18, 0.18];
    api.S.gLfo = { target: 'cutoff', rate: 0.9, depth: 0.4, armor: false, phase: 0 };
  },
  mutate(api) { api.S.gLfo.armor = true; },
  control: 'arm the device routing (respectRange) — the lock must then hold',
  assert(r, ix) {
    if (spread(r.expect, ix, ix.P.cutoff) < 0.05) throw new Error('device routing did not move the locked parameter');
  },
});

CASES.push({
  name: 't1-clamped-flag',
  proves: 'T1 indicator: the clamped[] flag is per-parameter and follows the ranges, not the values',
  setup(api, cfg, ix) {
    at(0, 1)(api, cfg);                       // corner C
    api.corners.C.range.reso = [0.6, 1]; api.corners.C.range.drive = [0.5, 1];
    api.corners.C.bind.MOT.reso = -0.9; api.corners.C.bind.MOT.drive = -0.9;
    sweepMacro(api, cfg, 'MOT');
  },
  mutate(api) { api.corners.C.range.reso = [0, 1]; api.corners.C.range.drive = [0, 1]; },
  control: 'widen both C ranges — the two clamped parameters must swing instead',
  assert(r, ix) {
    if (!r.expect.clamped.some((c) => c === 1)) throw new Error('no clamp ever lit');
  },
});

/* ---- T2: a corner that binds nothing makes an intent inert (the value half) */
CASES.push({
  name: 't2-inert-corner',
  proves: 'T2: parked on B, whose MOT bindings are all zero, a full MOT sweep changes nothing',
  setup(api, cfg, ix) {
    at(1, 0)(api, cfg);
    api.corners.B.lfo.depth = 0;              // silence the corner LFO: isolate the intent
    // Park the puck ON B's home: with X = Y = 0 and held there, the ONLY thing
    // moving is MOT, so "nothing changes" is a claim about MOT and not an
    // accident of a static patch.
    api.S.puck = { x: api.corners.B.home.x, y: api.corners.B.home.y, vx: 0, vy: 0 };
    sweepMacro(api, cfg, 'MOT');
  },
  mutate(api) { api.corners.B.bind.MOT.width = 0.5; },
  control: 'give B one non-zero MOT depth — exactly one parameter must move',
  assert(r, ix) {
    for (let p = 0; p < ix.N; p++)
      if (spread(r.expect, ix, p) > 1e-12) throw new Error(`param ${p} moved under an inert intent`);
  },
});

CASES.push({
  name: 't2-one-depth',
  proves: 'T2: with exactly one non-zero depth in the owning corner, exactly one parameter moves',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg);                       // corner A
    api.corners.A.lfo.depth = 0;
    api.INT.forEach((i) => api.P.forEach((p) => { api.corners.A.bind[i][p] = 0; }));
    api.corners.A.bind.MOT.sub = 0.45;
    sweepMacro(api, cfg, 'MOT');
  },
  mutate(api) { api.corners.A.bind.MOT.sub = 0; api.corners.A.bind.MOT.width = 0.45; },
  control: 'move the single depth from sub to width',
  assert(r, ix) {
    const moved = [];
    for (let p = 0; p < ix.N; p++) if (spread(r.expect, ix, p) > 1e-9) moved.push(p);
    if (moved.length !== 1 || moved[0] !== ix.P.sub) throw new Error(`moved: ${moved}`);
  },
});

/* ---- T4: at steepness 1 an intent acts through two corners at once ---- */
CASES.push({
  name: 't4-steep1-mid',
  proves: 'T4: morph (.5,.5) steepness 1 — the owner map spans >= 2 corners, so one macro acts through two corners\' bindings simultaneously',
  setup(api, cfg, ix) { at(0.5, 0.5)(api, cfg); api.S.steep = 1; sweepMacro(api, cfg, 'INT'); },
  mutate(api, cfg) { at(0, 0)(api, cfg); },
  control: 'park on corner A — the macro then acts through ONE corner\'s bindings',
  assert(r, ix) { if (!distinct(ownersAt(r.expect, ix, 0), 2)) throw new Error('owner map did not span two corners'); },
});

CASES.push({
  name: 't4-steep-sweep',
  proves: 'T4: a morph move at steepness 1 re-owns parameters mid-run (the owner map is not constant)',
  setup(api, cfg, ix) {
    at(0.2, 0.2)(api, cfg); api.S.steep = 1;
    cfg.morph2 = { x: 0.85, y: 0.8 }; cfg.switchTick = 30;
  },
  mutate(api, cfg) { cfg.morph2 = { x: 0.2, y: 0.2 }; },
  control: 'never move the morph',
  assert(r, ix) {
    if (flat(ownersAt(r.expect, ix, 0)) === flat(ownersAt(r.expect, ix, r.cfg.ticks - 1)))
      throw new Error('owner map never changed');
  },
});

CASES.push({
  name: 't4-collapse',
  proves: 'T4 inverse: at steepness 24 near a corner every atom collapses to that corner, so the other corners\' bindings go silent',
  setup(api, cfg, ix) { at(0.8, 0.15)(api, cfg); api.S.steep = 24; sweepMacro(api, cfg, 'INT'); },
  mutate(api, cfg) { at(0.5, 0.5)(api, cfg); },
  control: 'step to the field centre — the collapse must break',
  assert(r, ix) { if (distinct(ownersAt(r.expect, ix, 0), 2)) throw new Error('field did not collapse'); },
});

/* ---- T5: structural atoms and lead groups resolve whole, never partial - */
CASES.push({
  name: 't5-unison-atom',
  proves: 'T5: sweeping the morph, the unison request is always exactly one corner\'s value — never a blend, never partial',
  setup(api, cfg, ix) {
    at(0.1, 0.1)(api, cfg); api.S.steep = 1;
    cfg.morph2 = { x: 0.9, y: 0.9 }; cfg.switchTick = 25;
  },
  mutate(api) { api.S.seeds.unison = 0.97; },
  control: 'move the unison seed — its owner (and value) must change',
  assert(r) {
    const allowed = new Set([7, 1, 3, 1]);
    for (const v of r.expect.unisonValue) if (!allowed.has(v)) throw new Error(`unison ${v} not a corner request`);
  },
});

CASES.push({
  name: 't5-lead-group',
  proves: 'T5/ADR-176 d2: detune and width share one atom — they report the SAME owner on every tick of a morph sweep',
  setup(api, cfg, ix) {
    at(0.15, 0.2)(api, cfg); api.S.steep = 1;
    cfg.morph2 = { x: 0.9, y: 0.85 }; cfg.switchTick = 30;
    // one atom for {detune, width}; the rest keep their own
    cfg.atomOf = [0, 0, 1, 2, 3, 4, 5, 6]; cfg.homeAtom = 7; cfg.unisonAtom = 8; cfg.nAtoms = 9;
  },
  mutate(api, cfg, ix) {
    cfg.atomOf = Array.from({ length: ix.N }, (_, i) => i);
    cfg.homeAtom = ix.N; cfg.unisonAtom = ix.N + 1; cfg.nAtoms = ix.N + 2;
    api.S.seeds.width = 0.93;                 // break the lead map in the prototype too
  },
  control: 'break the lead map (ungroup detune/width) — the members must then report different owners',
  assert(r, ix) {
    for (let t = 0; t < r.cfg.ticks; t++) {
      const o = ownersAt(r.expect, ix, t);
      if (o[ix.P.detune] !== o[ix.P.width]) throw new Error(`group split at tick ${t}`);
    }
  },
});

CASES.push({
  name: 't5-group-of-three',
  proves: 'T5: a three-parameter atom (the FX-slot shape) flips whole across a morph move',
  setup(api, cfg, ix) {
    at(0.1, 0.85)(api, cfg); api.S.steep = 1;
    cfg.morph2 = { x: 0.9, y: 0.1 }; cfg.switchTick = 30;
    cfg.atomOf = [0, 1, 2, 2, 2, 3, 4, 5]; cfg.homeAtom = 6; cfg.unisonAtom = 7; cfg.nAtoms = 8;
  },
  mutate(api, cfg, ix) {
    cfg.atomOf = [0, 1, 2, 2, 3, 4, 5, 6]; cfg.homeAtom = 7; cfg.unisonAtom = 8; cfg.nAtoms = 9;
    api.S.seeds.reso = 0.02;
  },
  control: 'pull reso out of the atom — the slot half-flips',
  assert(r, ix) {
    for (let t = 0; t < r.cfg.ticks; t++) {
      const o = ownersAt(r.expect, ix, t);
      if (o[ix.P.sub] !== o[ix.P.cutoff] || o[ix.P.sub] !== o[ix.P.reso])
        throw new Error(`atom split at tick ${t}`);
    }
  },
});

/* ---- T6: commit bakes offsets into the DOMINANT corner ---------------- */
CASES.push({
  name: 't6-commit-dominant',
  proves: 'T6: displaced macros + pad, commit at tick 30 — the dominant corner\'s bases absorb the offsets',
  setup(api, cfg, ix) {
    at(0.05, 0.05)(api, cfg); api.S.steep = 8;
    api.S.intents.INT = 0.8; api.S.intents.MOT = -0.6;
    cfg.drag = { x: 0.8, y: 0.2 }; cfg.dragTicks = 20;
    cfg.commitAt = 30;
  },
  mutate(api, cfg) { at(0.95, 0.95)(api, cfg); },
  control: 'bake into the WRONG corner (morph moved to D) — before/after must differ',
  assert(r, ix) {
    const before = r.inputs.base, after = r.expect.baseAfter;
    if (flat(before) === flat(after)) throw new Error('commit baked nothing');
    if (r.expect.macroAfter.some((v) => v !== 0))
      throw new Error('commit did not zero the macro intents');
  },
});

CASES.push({
  name: 't6-commit-noop',
  proves: 'T6: committing with every offset at rest changes nothing — the bake is the offsets, not a re-save',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 8;
    api.S.intents.INT = 0; api.S.intents.MOT = 0;
    api.corners.A.home = { x: 0.5, y: 0.5 };  // puck starts at home: X = Y = 0
    cfg.commitAt = 30;
  },
  mutate(api) { api.S.intents.INT = 0.9; },
  control: 'displace one macro before the commit — the bake must then move bases',
  assert(r, ix) {
    const baseA = r.inputs.base.slice(0, ix.N), afterA = r.expect.baseAfter.slice(0, ix.N);
    for (let p = 0; p < ix.N; p++)
      if (Math.abs(baseA[p] - afterA[p]) > 1e-12) throw new Error(`rest commit moved param ${p}`);
  },
});

CASES.push({
  name: 't6-commit-clamped',
  proves: 'T6: the bake is clamped to the corner range — a locked parameter survives a commit under a displaced macro',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 8;
    api.corners.A.range.cutoff = [0.7, 0.7];
    api.corners.A.bind.INT.cutoff = 0.9;
    api.S.intents.INT = 1;
    cfg.commitAt = 20;
  },
  mutate(api) { api.corners.A.range.cutoff = [0, 1]; },
  control: 'widen the range — the bake must then escape 0.7',
  assert(r, ix) {
    if (Math.abs(r.expect.baseAfter[ix.P.cutoff] - 0.7) > 1e-12) throw new Error('locked base moved on commit');
  },
});

/* ---- T8: pad spring, release, latch, retarget -------------------------- */
CASES.push({
  name: 't8-release-return',
  proves: 'T8: drag for 20 ticks then release — the puck springs back to home and X/Y settle to zero',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 24; api.S.latch = false;
    cfg.drag = { x: 0.95, y: 0.05 }; cfg.dragTicks = 20; cfg.ticks = 140;
  },
  mutate(api) { api.S.latch = true; },
  control: 'latch on — the puck must NOT return',
  assert(r, ix) {
    const n = r.expect.intents.length / ix.NI;
    const lastX = Math.abs(r.expect.intents[(n - 1) * ix.NI + ix.I.X]);
    const lastY = Math.abs(r.expect.intents[(n - 1) * ix.NI + ix.I.Y]);
    if (lastX > 1e-3 || lastY > 1e-3) throw new Error(`pad did not settle (${lastX}, ${lastY})`);
  },
});

CASES.push({
  name: 't8-latch-hold',
  proves: 'T8: latch on — after release the puck holds position and its velocity is killed outright',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 24; api.S.latch = true;
    cfg.drag = { x: 0.2, y: 0.9 }; cfg.dragTicks = 25; cfg.ticks = 120;
  },
  mutate(api) { api.S.latch = false; },
  control: 'latch off — the puck must return',
  assert(r) {
    const n = r.expect.puck.length / 2;
    const dx = Math.abs(r.expect.puck[(n - 1) * 2] - r.expect.puck[(n - 2) * 2]);
    if (dx > 1e-9) throw new Error('latched puck kept moving');
  },
});

CASES.push({
  name: 't8-retarget',
  proves: 'T8: the home owner flips mid-return (morph jumps at tick 40); the puck retargets without a discontinuity in its own position',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 24; api.S.latch = false;
    cfg.drag = { x: 0.9, y: 0.9 }; cfg.dragTicks = 20;
    cfg.morph2 = { x: 1, y: 1 }; cfg.switchTick = 40; cfg.ticks = 140;
  },
  mutate(api, cfg) { cfg.morph2 = { x: 0, y: 1 }; },
  control: 'retarget to a different corner (C, not D)',
  assert(r) {
    const n = r.expect.puck.length / 2;
    for (let t = 1; t < n; t++) {
      const dx = Math.abs(r.expect.puck[t * 2] - r.expect.puck[(t - 1) * 2]);
      const dy = Math.abs(r.expect.puck[t * 2 + 1] - r.expect.puck[(t - 1) * 2 + 1]);
      if (dx > 0.25 || dy > 0.25) throw new Error(`puck jumped at tick ${t}`);
    }
  },
});

CASES.push({
  name: 't8-drag-stiff',
  proves: 'T8: the dragging spring (k=600, d=40) is a different law from the resting one — held for the whole run, the puck reaches the pointer',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 24;
    cfg.drag = { x: 0.9, y: 0.1 }; cfg.dragTicks = 60; cfg.ticks = 60;
  },
  mutate(api, cfg) { cfg.dragTicks = 0; },
  control: 'never drag — the puck must sit at home',
  assert(r) {
    const n = r.expect.puck.length / 2;
    if (Math.abs(r.expect.puck[(n - 1) * 2] - 0.9) > 0.02) throw new Error('drag did not reach the pointer');
  },
});

/* ---- T9: seeds (the 2026-09-10 amendment's corrected reading) ---------- */
CASES.push({
  name: 't9-seed-1024',
  proves: 'T9: seeds drawn from device seed 1024 — same seed, identical owner map (the C++ redraws the stream and must land on the same ten seeds)',
  setup(api, cfg, ix) { at(0.45, 0.55)(api, cfg); api.S.steep = 4; cfg.useDrawnSeeds = 1; cfg.deviceSeed = 1024; },
  mutate(api, cfg) { cfg.deviceSeed = 20260918; },
  control: 'a different device seed — the owner map must change',
  assert(r, ix) { if (!distinct(ownersAt(r.expect, ix, 0), 2)) throw new Error('seed case needs a split field to be sensitive'); },
});

CASES.push({
  name: 't9-seed-77',
  proves: 'T9: a second device seed, same law — reshuffle() moves the flip points, nothing else',
  setup(api, cfg, ix) { at(0.55, 0.45)(api, cfg); api.S.steep = 4; cfg.useDrawnSeeds = 1; cfg.deviceSeed = 77; },
  mutate(api, cfg) { cfg.deviceSeed = 1024; },
  control: 'swap back to seed 1024',
  assert(r, ix) { if (!distinct(ownersAt(r.expect, ix, 0), 2)) throw new Error('seed case needs a split field'); },
});

// The four exact corners: ownership there is seed-INDEPENDENT. One fixture per
// corner, each on a scrambled seed, each expecting a one-hot owner map — this
// is the "corner sounds do not change" half of T9, and it is the half a
// reshuffle regression would break silently.
[[0, 0, 0, 'a'], [1, 0, 1, 'b'], [0, 1, 2, 'c'], [1, 1, 3, 'd']].forEach(([x, y, k, tag]) => {
  CASES.push({
    name: `t9-corner-${tag}`,
    proves: `T9: parked exactly on corner ${tag.toUpperCase()} under a scrambled seed, every atom is owned by ${tag.toUpperCase()} — reshuffle cannot move a corner`,
    setup(api, cfg, ix) {
      at(x, y)(api, cfg); api.S.steep = 8;
      cfg.useDrawnSeeds = 1; cfg.deviceSeed = 987654321 + k;
    },
    mutate(api, cfg) { cfg.morph2 = { x: 0.5, y: 0.5 }; cfg.switchTick = 10; },
    control: 'step off the corner at tick 10 — ownership must then depend on the seed',
    assert(r, ix) {
      for (let t = 0; t < r.cfg.ticks; t++)
        for (const o of ownersAt(r.expect, ix, t)) if (o !== k) throw new Error(`corner ${tag} not one-hot`);
    },
  });
});

/* ---- T10: the human's named test (ADR-176) ---------------------------- */
// One intent bound in TWO corners with DIFFERENT ranges on an envelope-like
// parameter. The shallow corner stays shallow while it owns; the deep one
// swings. Elegant exactly because ranges are corner-owned.
CASES.push({
  name: 't10-shallow-owns',
  proves: 'T10: "Time" (INT) bound to delay at the same depth in A and C, but A clamps it to [.35,.45] and C to [0,1] — while A owns delay the swing stays shallow',
  setup(api, cfg, ix) {
    at(0.15, 0.15)(api, cfg); api.S.steep = 1;
    cfg.morph2 = { x: 0.15, y: 0.9 }; cfg.switchTick = 45; cfg.ticks = 90;
    api.corners.A.base.delay = 0.4; api.corners.C.base.delay = 0.4;
    api.corners.A.range.delay = [0.35, 0.45];
    api.corners.C.range.delay = [0, 1];
    api.corners.A.bind.INT.delay = 0.8; api.corners.C.bind.INT.delay = 0.8;
    sweepMacro(api, cfg, 'INT');
  },
  mutate(api) {
    api.corners.A.range.delay = [0, 1];
    api.corners.C.range.delay = [0.35, 0.45];
  },
  control: 'SWAP the ranges — the shallow corner must then go deep',
  assert(r, ix) {
    const p = ix.P.delay;
    let sawA = false, sawC = false;
    for (let t = 0; t < r.cfg.ticks; t++) {
      const o = ownersAt(r.expect, ix, t)[p], v = r.expect.final[t * ix.N + p];
      if (o === 0) { sawA = true; if (v < 0.35 - 1e-12 || v > 0.45 + 1e-12) throw new Error(`A-owned delay ${v} left its range`); }
      if (o === 2) sawC = true;
    }
    if (!sawA || !sawC) throw new Error(`T10 needs both owners in one run (A:${sawA} C:${sawC})`);
  },
});

CASES.push({
  name: 't10-deep-owns',
  proves: 'T10 (the other half): with A shallow and D wide on drive, the same INT sweep produces a visibly larger excursion on D\'s ticks',
  setup(api, cfg, ix) {
    at(0.2, 0.2)(api, cfg); api.S.steep = 1;
    cfg.morph2 = { x: 0.9, y: 0.9 }; cfg.switchTick = 45; cfg.ticks = 90;
    api.corners.A.base.drive = 0.5; api.corners.D.base.drive = 0.5;
    api.corners.A.range.drive = [0.45, 0.55];
    api.corners.D.range.drive = [0, 1];
    api.corners.A.bind.INT.drive = 0.9; api.corners.D.bind.INT.drive = 0.9;
    api.S.gLfo.target = 'none';
    sweepMacro(api, cfg, 'INT');
  },
  mutate(api) {
    api.corners.A.range.drive = [0, 1];
    api.corners.D.range.drive = [0.45, 0.55];
  },
  control: 'SWAP the ranges',
  assert(r, ix) {
    const p = ix.P.drive;
    let aMax = 0, dMax = 0;
    for (let t = 0; t < r.cfg.ticks; t++) {
      const o = ownersAt(r.expect, ix, t)[p], d = Math.abs(r.expect.final[t * ix.N + p] - 0.5);
      if (o === 0) aMax = Math.max(aMax, d);
      if (o === 3) dMax = Math.max(dMax, d);
    }
    if (!(dMax > aMax + 0.1)) throw new Error(`deep corner not deeper (A ${aMax}, D ${dMax})`);
  },
});

/* ---- modulation tiers: where the clamp sits relative to each tier ------ */
CASES.push({
  name: 'tier-promoted-outside',
  proves: '§4.5 tier order: a corner LFO promoted to GLOBAL scope applies regardless of owner and OUTSIDE the clamp, so it moves a locked parameter',
  setup(api, cfg, ix) {
    at(1, 0)(api, cfg);
    api.corners.B.range.drive = [0.3, 0.3];
    api.corners.C.lfo = { target: 'drive', depth: 0.8, scope: 'global' };
  },
  mutate(api) { api.corners.C.lfo.scope = 'corner'; },
  control: 'demote the routing to corner scope — it must go silent (C does not own drive)',
  assert(r, ix) { if (spread(r.expect, ix, ix.P.drive) < 0.05) throw new Error('promoted routing was silent'); },
});

CASES.push({
  name: 'tier-corner-inside',
  proves: '§4.5 tier order: a corner-scope LFO applies only for the owning corner and INSIDE the clamp',
  setup(api, cfg, ix) {
    at(0, 1)(api, cfg);                       // C owns everything
    api.corners.C.lfo = { target: 'cutoff', depth: 0.9, scope: 'corner' };
    api.corners.C.range.cutoff = [0.8, 0.9];
  },
  mutate(api) { api.corners.C.range.cutoff = [0, 1]; },
  control: 'widen the range — the corner LFO must then swing wide',
  assert(r, ix) {
    for (let t = 0; t < r.cfg.ticks; t++) {
      const v = r.expect.final[t * ix.N + ix.P.cutoff];
      if (v < 0.8 - 1e-12 || v > 0.9 + 1e-12) throw new Error(`corner LFO escaped the clamp: ${v}`);
    }
  },
});

CASES.push({
  name: 'tier-lfo-rate-dep',
  proves: '§4.5 ordering: the corner LFO\'s own RATE is a morphable parameter resolved before the parameters it drives — a bind on lfoRate changes the modulation frequency',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg);
    api.corners.A.bind.MOT.lfoRate = 0.7;
    api.corners.A.lfo = { target: 'width', depth: 0.6, scope: 'corner' };
    api.S.intents.MOT = 1;
    cfg.ticks = 90;
  },
  mutate(api) { api.corners.A.bind.MOT.lfoRate = 0; },
  control: 'zero the lfoRate binding — the modulation frequency must change',
  assert(r, ix) { if (spread(r.expect, ix, ix.P.width) < 0.05) throw new Error('corner LFO never moved width'); },
});

/* ---- §4.1 device routings to the morph axes --------------------------- */
CASES.push({
  name: 'morphmod-axis',
  proves: '§4.1: a device routing on MorphX moves the effective morph position before weights, so every downstream owner follows it',
  setup(api, cfg, ix) {
    at(0.5, 0.5)(api, cfg); api.S.steep = 2;
    api.S.gLfo = { target: 'morphX', rate: 1.6, depth: 0.9, armor: false, phase: 0 };
    cfg.ticks = 90;
  },
  mutate(api) { api.S.gLfo.depth = 0; },
  control: 'zero the routing depth — the owner map must stop moving',
  assert(r, ix) {
    // Compare against EVERY tick, not a hand-picked one: at 1.6 Hz on a 0.016 s
    // tick the LFO period is 39.06 ticks, so tick 0 and tick 40 are nearly in
    // phase and a two-point test reads "nothing moved" on a field that is
    // sweeping the whole pad.
    const first = flat(ownersAt(r.expect, ix, 0));
    let moved = false;
    for (let t = 1; t < r.cfg.ticks; t++) if (flat(ownersAt(r.expect, ix, t)) !== first) { moved = true; break; }
    if (!moved) throw new Error('morph routing did not re-own anything');
  },
});

CASES.push({
  name: 'morphmod-intent',
  proves: '§4.4: a device routing targeting intent X adds AFTER the pad clamp, so the intent can exceed the pad\'s own +-1',
  setup(api, cfg, ix) {
    at(0, 0)(api, cfg); api.S.steep = 24;
    api.S.gLfo = { target: 'intentX', rate: 1.1, depth: 0.9, armor: false, phase: 0.25 };
    cfg.drag = { x: 1, y: 0.5 }; cfg.dragTicks = 60; cfg.ticks = 60;
  },
  mutate(api) { api.S.gLfo.target = 'none'; },
  control: 'remove the routing',
  assert(r, ix) {
    let over = false;
    for (let t = 0; t < r.cfg.ticks; t++) if (Math.abs(r.expect.intents[t * ix.NI + ix.I.X]) > 1.0 + 1e-9) over = true;
    if (!over) throw new Error('intent never exceeded the pad clamp');
  },
});

/* ---------------------------------------------------------------- driver */

function build(def) {
  const b = runVariant(def, false);
  const c = runVariant(def, true);
  def.assert(b, b.ix);
  // A control that does not move the PROTOTYPE proves nothing. Abort loudly;
  // never retry until one fires (L0033).
  const differs = sameArrays(b.expect, c.expect);
  if (differs === null)
    throw new Error(`${def.name}: CONTROL IS VACUOUS — "${def.control}" leaves the prototype output identical`);
  const obj = { case: def.name, proves: def.proves, control: def.control, controlDiffersIn: differs };
  for (const [k, v] of Object.entries(b.inputs)) obj[`b.${k}`] = v;
  for (const [k, v] of Object.entries(c.inputs)) obj[`c.${k}`] = v;
  for (const [k, v] of Object.entries(b.expect)) obj[`e.${k}`] = v;
  return obj;
}

const selfcheck = process.argv.includes('--selfcheck');
if (!selfcheck) mkdirSync(OUT, { recursive: true });
let fail = 0;
const manifest = [];
for (const def of CASES) {
  let obj;
  try { obj = build(def); }
  catch (err) { console.log(`FAIL ${def.name}: ${err.message}`); fail++; continue; }
  if (selfcheck) {
    const again = build(def);
    const same = JSON.stringify(obj) === JSON.stringify(again);
    console.log(`${same ? 'OK  ' : 'FAIL'} ${def.name}`);
    if (!same) fail++;
  } else {
    const lines = Object.entries(obj).map(([k, v]) => `${JSON.stringify(k)}:${JSON.stringify(v)}`);
    const text = `{\n${lines.join(',\n')}\n}\n`;
    JSON.parse(text);                          // the fixture must be real JSON
    writeFileSync(join(OUT, `${def.name}.json`), text);
    manifest.push([def.name, obj['b.ticks'], def.proves].join('\t'));
    console.log(`wrote ${def.name}.json`);
  }
}
if (selfcheck) {
  console.log(fail ? `selfcheck: ${fail} case(s) NON-DETERMINISTIC or broken` : `selfcheck: ${CASES.length} cases are deterministic`);
  process.exit(fail ? 1 : 0);
}
if (fail) { console.log(`${fail} case(s) failed to generate`); process.exit(1); }
writeFileSync(join(OUT, 'intent-manifest.tsv'), manifest.join('\n') + '\n');
console.log(`${CASES.length} fixtures -> build-golden/intent/`);
