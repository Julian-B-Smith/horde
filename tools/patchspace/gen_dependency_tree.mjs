/*
 * gen_dependency_tree.mjs — B316 P1: WHICH PARAMETERS ARE LIVE GIVEN WHICH SWITCHES,
 * for the composed SCALPEL × horde engine, derived and SAVED AS A PROCESS.
 * HYPERSAW, 2026-09-27, ROADMAP B316 (records PR #810, branch lead-records-122).
 * The human: "building that dependency tree deterministically and saving the
 * process will be useful down the line when we integrate everything with morph and
 * the mod matrix".
 *
 * Writes tools/patchspace/dependency_tree.json (DATA; regenerate, never hand-edit):
 *   node tools/patchspace/gen_dependency_tree.mjs            (writes the file)
 *   node tools/patchspace/gen_dependency_tree.mjs --stdout   (prints it instead)
 * tools/patchspace/dependency_tree_check.mjs re-runs this and byte-compares.
 *
 * THREE LAYERS, and they must agree:
 *   1. STATIC — one `active_when` condition per parameter, over switch (and gate)
 *      parameters, read off the engine's own guards. Every condition carries the
 *      guard text it rests on (SOURCE anchors below), and the generator LOCATES each
 *      anchor in the file it names and records file:line; an anchor that is gone
 *      THROWS, so a changed engine cannot keep a stale tree quietly. HONEST SCOPE:
 *      the conditions are written by reading the code, not by a JS static analyser;
 *      the anchors pin them to the code, and layer 3 is what checks them.
 *   2. STRUCTURED — the SCALPEL lab's greying rules, EXECUTED, not paraphrased: its
 *      HORDE_INERT / RULE_OWN / inertWhy (docs/design/scalpel-interface-lab.html
 *      "B297"/"B309"), its FOLLOW table and linked() (blade 2 showing blade 1's
 *      value), sliced out of the lab and run per context; plus its three greying
 *      expressions that live inside view code (the FM group's `!isFM(b)`, Shape's
 *      `hot !== 6`, and a blade row's `off` class), each pinned to its text.
 *      (The brief pointed at "ACCOUNTING §6" for greying rules; §6 of
 *      docs/scalpel/ACCOUNTING.md is the structured-discussion QUESTIONS — the
 *      greying rules the human sees are the lab's, so those are what is parsed.)
 *      The lab's claims are one-sided (it greys some things; it never claims
 *      everything else is live), so only "lab greys it" is compared.
 *   3. EMPIRICAL — every parameter is perturbed in seeded contexts and the render is
 *      compared: LIVE iff some alternative value changes some output sample by more
 *      than TOL (max |ΔL|,|ΔR|). Contexts: SHARED ones (every parameter probed in
 *      each) plus TARGETED ones per parameter, drawn by rejection until the STATIC
 *      condition reads true (twice) and false (twice), so both directions of every
 *      condition are exercised, which is also what makes a planted wrong condition
 *      fail (the check's must-fail control).
 * Every disagreement is REPORTED in the JSON, never dropped: static-vs-probe
 * (either direction) and lab-greys-but-probe-live.
 *
 * THE PROBE, stated: 48 kHz; a 3-note script (A3 on at 256; E4 on at 1500 while A3
 * is held; both off at 2400; C4 on at 3000, while the voice still releases — so
 * mono glide, legato and "glide always" are visible; off at 3800; end 4608),
 * velocity 0.8 (so the blade envelope's velocity row is visible); TOL 1e-6 on the
 * float32 output. Context overrides (PROBE_OVERRIDES): the amp and blade-envelope
 * ATTACKS are drawn under 20 ms, because the script holds a note ~26-64 ms and a
 * longer attack would hide Decay/Sustain from the probe (a window limit, not a
 * law). Alternatives: every option of an enum; for a continuous row its default
 * and taper positions 0.15 and 0.85. Seeds: mulberry32 throughout (contexts,
 * Math.random under the engine). No clock is read.
 *
 * DETERMINISM. Output depends on nothing but the repo's files: worker count only
 * changes wall time (results are merged by task index); numbers in the JSON are
 * counts and decade exponents, never raw float deltas, so a last-ulp difference in
 * a libm cannot flip a byte.
 *
 * THE B275 MAPPING (the "active when" field in embryo): a B275 engine manifest
 * declares per parameter its morph behaviour, modulation limits and presets; this
 * file supplies the one field neither exists without — WHEN a parameter means
 * anything. `consumers` in the JSON states how morph (exemption and greying) and
 * the mod matrix (destination eligibility) read it. The schema is provisional,
 * local, and marked for swap-in when FOUNDATIONS' manifest schema lands (B275 P3).
 */
import '../labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { writeFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import { cpus } from 'node:os';
import { ROOT, FILES, readRepo, mulberry32, loadSpace, loadLab, valueAt, render, evalCond, condKeys } from './space.mjs';

export const OUT = 'tools/patchspace/dependency_tree.json';
export const TOL = 1e-6;
export const SCRIPT = { n: 4608, ev: [[256, 'on', 57, 0.8], [1500, 'on', 64, 0.8], [2400, 'off', 57], [2400, 'off', 64], [3000, 'on', 60, 0.8], [3800, 'off', 60]] };
export const SHARED_CONTEXTS = 24;
export const TARGETED_PER_SIDE = 2;
const TARGET_TRIES = 600;
const TARGETED_SIDE = 2;            // contexts per declared side channel: role false, channel true
const SIDE_TRIES = 6000;            // rarer conjunctions (e.g. SC_BLOCK_B2ON ~1e-3 per draw); drawing is cheap, rendering is not
const PROBE_OVERRIDES = { A: 20, benvA: 20, benvA2: 20 };   // ms: attack drawn under this (see header)
const ALT_KCUSTOM = '1, 7/4, 9/4';

/* ---------------------------------------------------------------- condition builders */
const eq = (k, v) => ({ eq: [k, v] }), ne = (k, v) => ({ ne: [k, v] }), inn = (k, a) => ({ in: [k, a] });
const ge = (k, x) => ({ ge: [k, x] }), gt = (k, x) => ({ gt: [k, x] }), le = (k, x) => ({ le: [k, x] });
const absgt = (k, x) => ({ absgt: [k, x] });
const all = (...c) => (c.length === 1 ? c[0] : { all: c }), any = (...c) => (c.length === 1 ? c[0] : { any: c });
const not = c => ({ not: c }), P = n => ({ pred: n });
const CAR = [0, 1, 2, 5];           // modes whose blade reads the blade wave `hot` and the carrier phase (xin, cacc)
const BITE = [1, 2, 4];             // modes that read the collision overlap `ov` (FM index, fold drive)
const XM = 0.0005, W_OFF = 0.004;   // razor-core.js thresholds: xOn / blocker, and the width bypass

/* anchor: [file key, exact text]. Resolved to file:line; missing text throws. */
const O = t => ['oracle', t], E = t => ['engine', t], S = t => ['swarm', t];

/* NAMED PREDICATES — the building blocks every condition reuses (in the JSON too) */
const PRED = {
  SW: [ge('N', 2), 'a swarm: two or more members (N 1 has no spread, coupling, detune or pan image)',
    [S('if (n === 1) this.x[0] = 0;'), O('const pn = j => N < 2 ? 0')]],
  B1CUT: [all(eq('b1on', 1), ge('w', W_OFF)), 'blade 1 cuts: switched on and its width outside the bypass zone (lab: off => w 0)',
    [O('if (w < 0.004){ ns.g = 0; ns.inside = false; return xin0; }')]],
  B1: [all(P('B1CUT'), any(gt('depth', 0), all(P('SW'), ne('dspread', 0)))), 'blade 1 SOUNDS: it cuts and some member has depth > 0',
    [O('return xin0 + g*p.depth*(hot - xin0);'), O('s.depth = Math.min(1, Math.max(0, d0 + mm.dAdd));')]],
  B2CUT: [all(eq('b2on', 1), ge('w2', W_OFF)), 'blade 2 cuts', [O('const g = bx.g, w2 = g.w;')]],
  B2: [all(P('B2CUT'), any(gt('depth2', 0), all(P('SW'), any(all(eq('b2sp', 0), ne('dspread', 0)), all(eq('b2sp', 1), ne('dspread2', 0)))))),
    'blade 2 SOUNDS (its depth spread is blade 1\'s unless it owns its spreads)', [O('Math.min(1, Math.max(0, s.depth2 + mm.dAdd2))')]],
  BOTH: [all(P('B1'), P('B2')), 'both blades sound', []],
  CAR1: [all(P('B1'), inn('mode', CAR)), 'blade 1 sounds in a carrier mode (Sync, FM reset, FM free, Ring)',
    [O('case 0: { const cp = hp + ns.xin + ns.cacc; hot = RazorCore.wave(p.hot'), O('case 5: { const cp = hp + ns.xin + ns.cacc; hot = xin0*RazorCore.wave(p.hot')]],
  CAR2: [all(P('B2'), inn('mode2', CAR)), 'blade 2 sounds in a carrier mode', []],
  FM1: [all(P('B1'), inn('mode', [1, 2])), 'blade 1 sounds in an FM mode', [O('static isFM(p){ return p.mode === 1 || p.mode === 2; }')]],
  FM2: [all(P('B2'), inn('mode2', [1, 2])), 'blade 2 sounds in an FM mode', []],
  FMSH: [any(P('FM1'), all(P('FM2'), eq('b2fm', 0))), 'blade 1\'s modulator is read: by blade 1, or by blade 2 sharing it (b2fm 0)',
    [O('const mEff2 = d.b2fm ? (d.mUnit2 ? s.mHz2/fi : s.m2) : (d.mUnit ? s.mHz/fi : s.m);'), O('g.fmType = s.b2fm ? s.fmType2 : s.fmType; g.mshape = s.b2fm ? s.mshape2 : s.mshape;')]],
  FMOWN2: [all(P('FM2'), eq('b2fm', 1)), 'blade 2 runs its own modulator', [O('(d.b2fm ? s.I2 : I0)*mm.iMul2, mEff2);')]],
  SPR1: [any(P('B1'), all(P('B2'), eq('b2sp', 0))), 'blade 1\'s spread set is read: by blade 1, or by blade 2 following it (b2sp 0)',
    [O('if (!s.b2sp){ m.cOff2 = m.cOff; m.kAdd2 = m.kAdd;')]],
  SPR2: [all(P('B2'), eq('b2sp', 1)), 'blade 2 owns its spreads', [O('m.cOff2 = s.bspread2*pn(7);')]],
  ENV1: [any(P('B1'), all(P('B2'), eq('b2env', 0))), 'blade 1\'s envelope is read (blade 2 follows it unless b2env)',
    [O('} else { v.kE2 = v.kE; v.wE2 = v.wE; }')]],
  ENV2: [all(P('B2'), eq('b2env', 1)), 'blade 2 runs its own envelope', [O('if (d.b2env){')]],
  ROT1: [any(P('B1'), all(P('B2'), eq('rot2Follow', 1))), 'blade 1\'s rotation clock is read (blade 2 follows it unless rot2Follow 0)',
    [O('const own2 = s.b2on && !d.rot2Follow'), O('const rot2All = own2 ?')]],
  ROT2: [all(P('B2'), eq('rot2Follow', 0)), 'blade 2 rotates on its own clock', []],
  L2HZ: [all(P('B2'), any(eq('lock2', 2), all(eq('lock2', -1), eq('lock', 2)))), 'blade 2\'s cut rate is in Hz (its own lock, or blade 1\'s when "=1")',
    [O('const lock2 = d.lock2 < 0 ? d.lock : d.lock2;')]],
  SHAPE1: [all(P('CAR1'), eq('hot', 6)), 'blade 1 plays Sine→Saw, the only wave that reads Shape', [O('const r = RazorCore.mr || 0;')]],
  SHAPE2: [all(P('CAR2'), eq('hot2', 6)), 'blade 2 plays Sine→Saw', []],
  DETUNED: [all(P('SW'), ne('detune', 0)), 'a swarm with detune: every detune law multiplies dep = detune·spread',
    [S('const dep = p.detune * p.spread;')]],
  UPCAR: [any(all(eq('b2order', 0), inn('mode2', CAR)), all(eq('b2order', 1), inn('mode', CAR))), 'the UPPER blade (b2order) reads collision pitch (cacc)',
    [O('if (s.b2order){ RazorCore.collide(s.colK, ns')]],
  UPBITE: [any(all(eq('b2order', 0), inn('mode2', BITE)), all(eq('b2order', 1), inn('mode', BITE))), 'the UPPER blade reads collision bite (ov): FM index or fold drive',
    [O('const Ib = ns.ov > 0 ? p.I*(1 + 4*p.colB*ns.ov) : p.I;')]],
  /* where the per-cycle DC estimate is NUMERIC (its resolution J reads the modulator rate:
     `feats += s.mEff*w`, unconditionally) rather than closed-form (Sync on a closed-form wave)
     or skipped (twin − cancels it) */
  NUM1: [all(P('B1'), ne('mirror', 2), not(all(eq('mode', 0), ne('hot', 6)))), 'blade 1\'s per-cycle DC is estimated numerically',
    [O('if (s.mode === 0 && s.hot !== 6){'), O('feats += s.mEff*w;'), O('const fac = s.mirror === 2 ? 0 : s.mirror === 3 ? 2 : 1;')]],
  NUM2: [all(P('B2'), not(any(eq('mirror2', 2), all(eq('mirror2', -1), eq('mirror', 2)))), not(all(eq('mode2', 0), ne('hot2', 6)))),
    'blade 2\'s per-cycle DC is estimated numerically', []],
  PAIR: [all(eq('b2on', 1), gt('b2mix', 1e-6), any(P('B1'), P('B2'))), 'serial interplay on: per-cycle DC is the numeric PAIR estimate, whose resolution reads both blades\' modulator rates',
    [O('if (s.b2on && s.b2mix > 1e-6) mm.dc = this.dcPair(mm, c, kq, s);'), O('2*(s.mEff + g.mEff) + 4;')]],
  /* ---- SIDE CHANNELS: declared "may ALSO be heard via" conditions, each an engine path
     other than the parameter's role. A probe hit here is counted, not a disagreement; a
     hit OUTSIDE role and side channels is. They are findings for the lead as much as data. */
  SC_BLOCK_XM: [all(eq('dcMode', 2), le('fb', XM), not(all(eq('b2on', 1), any(ne('colK', 0), ne('colB', 0))))),
    'THE BLOCKER FAMILY (SC_BLOCK_*): under per-cycle DC the fallback DC blocker (an 8 Hz HP on the whole output) is OFF until xm, fb, or (with b2on) colK/colB leave 0, so each of those, while the others are still 0, is heard as the blocker switching in, not as its role. This one: Cross-mod.',
    [O('if (d.dcMode === 1 || (d.dcMode === 2 && (s.xm > 0.0005 || s.fb > 0.0005 || (s.b2on && (s.colK || s.colB))))){')]],
  SC_BLOCK_FB: [all(eq('dcMode', 2), le('xm', XM), not(all(eq('b2on', 1), any(ne('colK', 0), ne('colB', 0))))), 'Feedback toggles the blocker (SC_BLOCK_XM\'s family)', []],
  SC_BLOCK_COLK: [all(eq('b2on', 1), eq('dcMode', 2), le('xm', XM), le('fb', XM), eq('colB', 0)), 'Collision → pitch toggles the blocker', []],
  SC_BLOCK_COLB: [all(eq('b2on', 1), eq('dcMode', 2), le('xm', XM), le('fb', XM), eq('colK', 0)), 'Collision → bite toggles the blocker', []],
  SC_BLOCK_B2ON: [all(eq('dcMode', 2), le('xm', XM), le('fb', XM), any(ne('colK', 0), ne('colB', 0))), 'Blade 2 on/off toggles the blocker when a collision amount is set', []],
  SC_DCRES1: [all(eq('dcMode', 2), any(P('NUM1'), all(P('NUM2'), eq('b2fm', 0)), P('PAIR'))),
    'blade 1\'s modulator rate (m / mHz / mUnit) sets the resolution J of a NUMERIC per-cycle DC estimate even where no blade is in FM, so the estimate, and the output, move with it',
    [O('feats += s.mEff*w;'), O('const J = Math.min(1024, Math.max(48, Math.ceil(feats*32)));')]],
  SC_DCRES2: [all(eq('dcMode', 2), any(all(P('NUM2'), eq('b2fm', 1)), P('PAIR'))), 'blade 2\'s own modulator rate sets the DC estimate\'s resolution (as SC_DCRES1)', []],
  SC_DCRES_B2FM: [all(eq('dcMode', 2), any(P('NUM2'), P('PAIR'))), 'b2fm swaps which modulator rate blade 2\'s DC estimate resolves by', []],
  SC_DCPAIR: [all(eq('dcMode', 2), eq('b2on', 1), any(P('B1'), P('B2'))), 'Upper hears above 0 swaps per-cycle DC to the numeric PAIR estimate even when only one blade sounds',
    [O('if (s.b2on && s.b2mix > 1e-6) mm.dc = this.dcPair(mm, c, kq, s);')]],
  SC_DCPAIR_B2ON: [all(eq('dcMode', 2), gt('b2mix', 1e-6), P('B1')), 'Blade 2 on/off swaps blade 1\'s per-cycle DC to the pair estimate when Upper hears is above 0', []],
  SC_DRAWS: [any(all(P('B1'), inn('mode', [2, 3])), all(P('B2'), inn('mode2', [2, 3]))),
    'a sounding blade reads state drawn from the ONE shared Math.random stream after note-on (Noise values; FM free\'s modulator phase modX, drawn at each later note-on): the drift law\'s per-tick draws shift that stream',
    [O('static rnd(){ return Math.random()*2 - 1; }'), O('m.phi = d.phaseMode === 1 ? 0 : Math.random(); m.modX = Math.random()*1000;'),
      O('const u = Math.max(1e-12, Math.random())')]],
  SC_SCANCOL: [all(eq('aa', 1), P('BOTH'), any(all(eq('b2order', 0), eq('mode2', 6), le('hard2', 0.001)), all(eq('b2order', 1), eq('mode', 6), le('hard', 0.001)))),
    'an UPPER Crush blade with hard steps: the PolyBLEP scanner places its step corrections using the collision accumulator (cacc), which the Crush voice itself never reads — collision pitch is heard as moved band-limiting corrections',
    [O('if (ns.cd){ o0 += ns.cacc - ns.cd; o1 += ns.cacc; }'), O("else if (g.mode === 6 && g.hard <= 0.001){ off = 0; step = 1; }")]],
};

/* the spread rows and what else each needs to be heard (per blade reading it) */
const SPREADS1 = {   // blade 1's set: [row, extra condition for it to be heard]
  bspread: true, kspread: eq('kRule', 0), kRuleAmt: ne('kRule', 0), wspread: true, dspread: true,
  mspread: null /* shape */, ispread: null /* FM */, rotSpread: null /* rotation */,
};
function spreadHeard(k) {   // the static condition of a blade-1 spread row, and of its blade-2 twin
  switch (k) {
    case 'mspread': return all(P('SW'), any(P('SHAPE1'), all(P('SHAPE2'), eq('b2sp', 0))));
    case 'ispread': return all(P('SW'), any(P('FM1'), all(P('FM2'), eq('b2sp', 0))));
    case 'rotSpread': return all(P('SW'), any(P('B1'), all(P('B2'), any(eq('rot2Follow', 1), eq('b2sp', 0)))));
    case 'mspread2': return all(P('SW'), P('SHAPE2'), eq('b2sp', 1));
    case 'ispread2': return all(P('SW'), P('FM2'), eq('b2sp', 1));
    case 'rotSpread2': return all(P('SW'), P('B2'), eq('rot2Follow', 0), eq('b2sp', 1));
    default: {
      const two = /2$/.test(k), base = two ? k.slice(0, -1) : k, extra = SPREADS1[base];
      const ex = extra === true ? null : two ? JSON.parse(JSON.stringify(extra).replace(/"kRule"/g, '"kRule2"')) : extra;
      return all(P('SW'), P(two ? 'SPR2' : 'SPR1'), ...(ex ? [ex] : []));
    }
  }
}
/* "this spread row is engaged": heard and non-zero (kRuleAmt: a rule is on and it is > 0) */
const SPREAD_ROWS = ['bspread', 'kspread', 'kRuleAmt', 'wspread', 'dspread', 'mspread', 'ispread', 'rotSpread',
  'bspread2', 'kspread2', 'kRuleAmt2', 'wspread2', 'dspread2', 'mspread2', 'ispread2', 'rotSpread2'];
const engaged = k => all(spreadHeard(k), ne(k, 0));
/* what blade 2 hears of a spread row when it FOLLOWS (b2sp 0) or OWNS (b2sp 1) — for b2sp itself */
function b2Reads(k) {
  const two = /2$/.test(k), base = two ? k.slice(0, -1) : k, rule = two ? 'kRule2' : 'kRule';
  const q = { bspread: true, kspread: eq(rule, 0), kRuleAmt: ne(rule, 0), wspread: true, dspread: true,
    mspread: P('SHAPE2'), ispread: P('FM2'), rotSpread: eq('rot2Follow', 0) }[base];
  return all(ne(k, 0), ...(q === true ? [] : [q]));
}

/* ---------------------------------------------------------------- THE STATIC TREE */
/* [row, active_when, anchors, note, side channels]. active_when is the parameter's ROLE:
   where it is heard doing what it is for. Side channels (SC_* predicates) are where it
   may ALSO be heard through another engine path. Order is the lab table's. */
function staticRules() {
  const R = {};
  const r = (k, cond, anchors, note, side) => { R[k] = { cond, anchors: anchors || [], note: note || '', side: side || [] }; };
  const blade1Sound = any(gt('depth', 0), all(P('SW'), ne('dspread', 0)));
  /* ---- blade 1 */
  r('b1on', all(ge('w', W_OFF), blade1Sound), [O('if (w < 0.004){ ns.g = 0; ns.inside = false; return xin0; }')],
    'lab mapping: off => w 0. Heard only when blade 1 would cut and sound.');
  r('mode', P('B1'), [O('switch (p.mode){')]);
  r('hot', P('CAR1'), [O('case 0: { const cp = hp + ns.xin + ns.cacc; hot = RazorCore.wave(p.hot')],
    'Noise, Fold and Crush never read the blade wave.');
  r('w', eq('b1on', 1), [O('if (w < 0.004){ ns.g = 0; ns.inside = false; return xin0; }')], 'Width IS blade 1\'s on/off below 0.004.');
  r('k', all(P('B1'), ne('lock', 2)), [O('const kq = Math.min((d.lock === 2 ? Math.max(0.05, s.kHz/fi + mm.kAdd) : Math.max(0.25, s.k + mm.kAdd))')]);
  r('kHz', all(P('B1'), eq('lock', 2)), [O('const kq = Math.min((d.lock === 2 ? Math.max(0.05, s.kHz/fi + mm.kAdd) : Math.max(0.25, s.k + mm.kAdd))')]);
  r('lock', any(P('B1'), all(P('B2'), eq('lock2', -1))), [O('const kk = p.lock === 1 ? k / w : k;'), O('const lock2 = d.lock2 < 0 ? d.lock : d.lock2;')]);
  r('c', P('B1'), [O('let c = s.c + rotAll + mm.cOff + (d.frame ? mm.lead : 0);')]);
  r('hard', P('B1'), [O('const t = p.hard * w * 0.5;'), O('const wEff = p.mirror === 1 ? w*0.5 : w, hpEnd = kk*wEff, sl = p.hard;')],
    'Edges in every mode; Crush reads it as slew.');
  r('depth', P('B1CUT'), [O('return xin0 + g*p.depth*(hot - xin0);')]);
  r('mirror', any(P('B1'), all(P('B2'), eq('mirror2', -1))), [O('g.lock = s.lock2 < 0 ? s.lock : s.lock2; g.mirror = s.mirror2 < 0 ? s.mirror : s.mirror2;')]);
  r('morph', P('SHAPE1'), [O('m.mor = Math.min(1, Math.max(0, s.morph + s.mspread*2*pn(6)));'), O('const r = RazorCore.mr || 0;')]);
  r('rotRate', P('ROT1'), [O('const rOn = Math.abs(t.rotRate) > 0.004, sOn = t.rotSpread > 0.004')]);
  const spRot = any(all(P('ROT1'), absgt('rotSpread', 0.004)),
    all(P('ROT2'), any(all(eq('b2sp', 0), absgt('rotSpread', 0.004)), all(eq('b2sp', 1), absgt('rotSpread2', 0.004)))));
  r('rotSync', any(all(P('ROT1'), absgt('rotRate', 0.004)), all(P('ROT2'), absgt('rotRate2', 0.004)), all(ne('polyMode', 0), P('SW'), spRot)),
    [O('const rotBase = d.rotSync ? null : this.gRot;'), O('if (d.rotSync || fresh){ v.rot = 0; v.rot2 = 0; for (const m of v.m){ m.rot = 0; m.rot2 = 0; } }')],
    'Restart-per-note vs the free-running clock: heard while a blade rotates, and in mono/legato (a non-fresh retrigger) while members rotate by spread.');
  r('fb', any(P('CAR1'), P('CAR2')), [O('const xin = xOn ? 0.5*(s.xm*xb[(q + 1) % N] + s.fb*0.5*(mm.y1 + mm.y2)) : 0;')],
    'Read as a phase push by the carrier modes.', ['SC_BLOCK_FB']);
  r('fmType', P('FMSH'), [O('if (p.fmType !== 1 || !RazorCore.isFM(p)) return 0;')]);
  r('mshape', P('FMSH'), [O('RazorCore.mod(p.mshape, p.mode === 1 ? p.mEff*er : modX, ns.seed)')]);
  r('I', P('FMSH'), [O('const Ib = ns.ov > 0 ? p.I*(1 + 4*p.colB*ns.ov) : p.I;')]);
  r('mUnit', P('FMSH'), [O('s.mEff = d.mUnit ? s.mHz/fi : s.m;')], '', ['SC_DCRES1']);
  r('m', all(P('FMSH'), eq('mUnit', 0)), [O('s.mEff = d.mUnit ? s.mHz/fi : s.m;')], '', ['SC_DCRES1']);
  r('mHz', all(P('FMSH'), eq('mUnit', 1)), [O('s.mEff = d.mUnit ? s.mHz/fi : s.m;')], '', ['SC_DCRES1']);
  const env1On = all(P('ENV1'), any(ne('benvK', 0), ne('benvW', 0)));
  r('benvK', P('ENV1'), [O('v.kE = s.benvK ? Math.pow(2, s.benvK*4*bE) : 1;')]);
  r('benvW', P('ENV1'), [O('v.wE = s.benvW ? Math.pow(2, s.benvW*3*bE) : 1;')]);
  r('benvA', env1On, [O('const beA = 1/Math.max(1, s.benvA*0.001*sr)')], 'The blade envelope is heard only through Env → cut / Env → width.');
  r('benvD', env1On, [O('beD = 1 - Math.exp(-4/Math.max(1, s.benvD*0.001*sr));')]);
  r('benvVel', env1On, [O('v.bv = 1 - this.s.benvVel + this.s.benvVel*vel;')], 'At velocity 1 it is inert by arithmetic; the probe plays 0.8.');
  r('base', true, [O('const base = RazorCore.wave(p.base, phi)')], 'The base wave is the sound when no blade cuts.');
  r('dcMode', true, [O('if (d.dcMode === 1 || (d.dcMode === 2 &&')], '1 (blocker) filters every output.');
  r('xm', any(P('CAR1'), P('CAR2')), [O('const xin = xOn ? 0.5*(s.xm*xb[(q + 1) % N] + s.fb*0.5*(mm.y1 + mm.y2)) : 0;')],
    'As Feedback: the carrier modes read it (N 1 reads its own previous output).', ['SC_BLOCK_XM']);
  r('frame', all(P('SW'), any(P('B1'), all(P('B2'), eq('frame2', -1)))), [O('fr2 = d.frame2 < 0 ? d.frame : d.frame2')]);
  r('phaseMode', true, [E('p.retrig = d.phaseMode === 0 ? 0 : 1;'), O('m.phi = d.phaseMode === 1 ? 0 : Math.random();')],
    'random vs aligned starts; settled (2) plays as aligned in the composed engine but draws one Math.random per member that aligned (1) does not.');
  r('panOrder', all(ge('N', 3), ne('width', 0)), [O('const pos = d.panOrder ? pn*2 : RazorCore.panSlot(N, i);')],
    'N 2: the balanced slots ARE the fan (±1).');
  const lawHeard = all(P('SW'), any(...SPREAD_ROWS.map(engaged)));
  r('law', lawHeard, [O('const pn = j => N < 2 ? 0 : law === 0 ? g'), O('} else if (d.law === 4){')],
    'Heard through any engaged spread.', ['SC_DRAWS']);
  r('driftRate', all(eq('law', 4), lawHeard), [O('const th = 6.283185307179586*s.driftRate')]);
  for (const k of SPREAD_ROWS) r(k, spreadHeard(k), [O({ bspread: 'm.cOff = s.bspread*pn(0);', kspread: 'let ko = (q ? Math.round(spread) : spread)*pv;',
    kRuleAmt: 'out[1] = Math.pow(r, amt);', wspread: 'm.wMul = Math.pow(2, s.wspread*4*pn(3));', dspread: 'm.dAdd = s.dspread*2*pn(4);',
    mspread: 'm.mor = Math.min(1, Math.max(0, s.morph + s.mspread*2*pn(6)));', ispread: 'm.iMul = Math.pow(2, s.ispread*4*pn(5));',
    rotSpread: 'm.rotOff = s.rotSpread*2*pn(2);', bspread2: 'm.cOff2 = s.bspread2*pn(7);', kspread2: 'const k2 = RazorCore.kSpread(s.kRule2, s.kRuleAmt2, s.kspread2',
    kRuleAmt2: 'const k2 = RazorCore.kSpread(s.kRule2, s.kRuleAmt2, s.kspread2', wspread2: 'm.wMul2 = Math.pow(2, s.wspread2*4*pn(9));',
    dspread2: 'm.dAdd2 = s.dspread2*2*pn(10);', mspread2: 's.mspread2*2*pn(12)', ispread2: 'm.iMul2 = Math.pow(2, s.ispread2*4*pn(13));',
    rotSpread2: 'm.rotOff2 = s.b2sp ? s.rotSpread2*2*pn(11) : m.rotOff;' }[k])]);
  r('kRule', all(P('SW'), P('SPR1'), any(ne('kspread', 0), ne('kRuleAmt', 0))), [O('if (!rule){')],
    'Even vs a rule: identical only when Cut spread and Rule depth are both 0.');
  r('kRule2', all(P('SW'), P('SPR2'), any(ne('kspread2', 0), ne('kRuleAmt2', 0))), [O('const k2 = RazorCore.kSpread(s.kRule2, s.kRuleAmt2, s.kspread2')]);
  const qRule = (rule, amt, sp) => any(all(eq(rule, 0), ne(sp, 0)), all(ne(rule, 0), ne(amt, 0), inn('law', [1, 3, 4])));
  r('kq', all(P('SW'), any(all(P('SPR1'), qRule('kRule', 'kRuleAmt', 'kspread')), all(P('SPR2'), qRule('kRule2', 'kRuleAmt2', 'kspread2')))),
    [O('let ko = (q ? Math.round(spread) : spread)*pv;'), O('if (q) r = L[Math.round(x)];')],
    'Under a rule, gradient and alternate laws already land on exact slots, so snapping changes nothing there.');
  r('kCustom', any(all(P('SW'), P('SPR1'), eq('kRule', 9), ne('kRuleAmt', 0)), all(P('SW'), P('SPR2'), eq('kRule2', 9), ne('kRuleAmt2', 0))),
    [O("default: L.push(CU ? CU[j % CU.length]*Math.pow(2, Math.floor(j/CU.length)) : 1);")]);
  /* ---- blade 2 */
  r('b2on', all(ge('w2', W_OFF), any(gt('depth2', 0), all(P('SW'), any(all(eq('b2sp', 0), ne('dspread', 0)), all(eq('b2sp', 1), ne('dspread2', 0)))))),
    [O('const bx = s.b2on ? m.bx : null;')], 'Heard when blade 2 would cut and sound.', ['SC_BLOCK_B2ON', 'SC_DCPAIR_B2ON']);
  r('mode2', P('B2'), [O('g.mode = s.mode2; g.hot = s.hot2;')]);
  r('hot2', P('CAR2'), [O('g.mode = s.mode2; g.hot = s.hot2;')]);
  r('w2', eq('b2on', 1), [O('RazorCore.fillG2(bx.g, s, s.w2 < 0.004 ? s.w2')]);
  r('k2', all(P('B2'), not(P('L2HZ'))), [O('bx.k = Math.min((lock2 === 2 ?')]);
  r('kHz2', P('L2HZ'), [O('bx.k = Math.min((lock2 === 2 ?')]);
  r('lock2', P('B2'), [O('g.lock = s.lock2 < 0 ? s.lock : s.lock2;')]);
  r('c2', P('B2'), [O('bx.c = s.c2 + rot2All + mm.cOff2 + (fr2 ? mm.lead : 0);')]);
  r('hard2', P('B2'), [O('g.hard = s.hard2;')]);
  r('depth2', P('B2CUT'), [O('Math.min(1, Math.max(0, s.depth2 + mm.dAdd2))')]);
  r('mirror2', P('B2'), [O('g.mirror = s.mirror2 < 0 ? s.mirror : s.mirror2;')]);
  r('morph2', P('SHAPE2'), [O('const mor2 = Math.min(1, Math.max(0, s.morph2 +')]);
  r('rot2Follow', all(P('B2'), any(absgt('rotRate', 0.004), absgt('rotRate2', 0.004),
    all(P('SW'), any(absgt('rotSpread', 0.004), all(eq('b2sp', 1), absgt('rotSpread2', 0.004)))))), [O('const own2 = s.b2on && !d.rot2Follow')]);
  r('rotRate2', P('ROT2'), [O('const own2 = s.b2on && !d.rot2Follow, rs2 = s.rotRate2/sr')]);
  r('frame2', all(P('SW'), P('B2')), [O('fr2 = d.frame2 < 0 ? d.frame : d.frame2')]);
  r('b2sp', all(P('SW'), P('B2'), any(...['bspread', 'kspread', 'kRuleAmt', 'wspread', 'dspread', 'mspread', 'ispread', 'rotSpread'].flatMap(k => [b2Reads(k), b2Reads(k + '2')]))),
    [O('if (!s.b2sp){ m.cOff2 = m.cOff;')], 'Following vs owning: heard when either set would move blade 2.');
  r('b2fm', P('FM2'), [O('if (s.b2fm){ modX20 = bx.modX;')], '', ['SC_DCRES_B2FM']);
  for (const k of ['fmType2', 'mshape2', 'I2']) r(k, P('FMOWN2'), [O('g.fmType = s.b2fm ? s.fmType2 : s.fmType; g.mshape = s.b2fm ? s.mshape2 : s.mshape;')]);
  r('mUnit2', P('FMOWN2'), [O('const mEff2 = d.b2fm ? (d.mUnit2 ? s.mHz2/fi : s.m2)')], '', ['SC_DCRES2']);
  r('m2', all(P('FMOWN2'), eq('mUnit2', 0)), [O('const mEff2 = d.b2fm ? (d.mUnit2 ? s.mHz2/fi : s.m2)')], '', ['SC_DCRES2']);
  r('mHz2', all(P('FMOWN2'), eq('mUnit2', 1)), [O('const mEff2 = d.b2fm ? (d.mUnit2 ? s.mHz2/fi : s.m2)')], '', ['SC_DCRES2']);
  r('b2env', all(P('B2'), any(ne('benvK', 0), ne('benvW', 0), ne('benvK2', 0), ne('benvW2', 0))), [O('if (d.b2env){')]);
  const env2On = all(P('ENV2'), any(ne('benvK2', 0), ne('benvW2', 0)));
  r('benvK2', P('ENV2'), [O('v.kE2 = s.benvK2 ? Math.pow(2, s.benvK2*4*bE2) : 1;')]);
  r('benvW2', P('ENV2'), [O('v.wE2 = s.benvW2 ? Math.pow(2, s.benvW2*3*bE2) : 1;')]);
  r('benvA2', env2On, [O('const beA2 = 1/Math.max(1, s.benvA2*0.001*sr)')]);
  r('benvD2', env2On, [O('beD2 = 1 - Math.exp(-4/Math.max(1, s.benvD2*0.001*sr));')]);
  r('benvVel2', env2On, [O('v.bv2 = 1 - this.s.benvVel2 + this.s.benvVel2*vel;')]);
  /* ---- v1.1 interplay */
  r('b2order', all(P('BOTH'), any(gt('b2mix', 1e-6), all(ne('colK', 0), any(inn('mode', CAR), inn('mode2', CAR))),
    all(ne('colB', 0), any(inn('mode', BITE), inn('mode2', BITE))))),
    [O('const g = bx.g, lam = p.b2mix, up2 = !p.b2order;'), O('if (s.b2order){ RazorCore.collide(s.colK, ns')],
    'Which blade is upper: heard through serial interplay, or through a collision the swapped blade reads.');
  r('b2mix', P('BOTH'), [O('if (bx && p.b2mix > 1e-6) return RazorCore.outSerial(')],
    'Serial interplay: the upper blade hears the lower one; needs both blades sounding.', ['SC_DCPAIR']);
  r('colK', all(P('BOTH'), P('UPCAR')), [O('const d = amt ? (Math.pow(4, amt*ov) - 1)*kkU*dphi : 0;'), O('if (bx && on && on2 && (s.colK || s.colB)){')],
    'v1.1 collision pitch acts only on an UPPER blade in Sync / FM / Ring, and only while the two blades overlap (geometry the grammar does not model: see regime).',
    ['SC_BLOCK_COLK', 'SC_SCANCOL']);
  r('colB', all(P('BOTH'), P('UPBITE')), [O('const Ib = ns.ov > 0 ? p.I*(1 + 4*p.colB*ns.ov) : p.I;'), O('(ns.ov > 0 ? 1 + 2*p.colB*ns.ov : 1)')],
    'v1.1 collision bite acts only on an UPPER blade in FM or Fold, while the blades overlap.', ['SC_BLOCK_COLB']);
  /* ---- swarm and voice (horde rows merge into RazorCore's keys) */
  r('N', true, [E('const want = { n: d.N, dist: d.dist, seed: d.seed, law: d[\'h.law\'] };')]);
  r('detune', P('SW'), [E('p.detune = t.detune / 100;'), S('const dep = p.detune * p.spread;')], 'Row 4 MERGES: horde law.');
  r('K', P('SW'), [S('const km = 4 * p.K * Math.abs(p.K);')], 'Row 6: horde law wins; one member has nothing to couple to.');
  r('width', P('SW'), [O('const ang = (pos*s.width + 1)*Math.PI/4;')], 'N 1 sits at the centre slot.');
  r('A', true, [O('const attInc = 1/Math.max(1, s.A*0.001*sr);')]);
  r('D', ne('S', 1), [O('else if (v.stage === 2) v.env += (s.S - v.env)*dC;')], 'At Sustain 1 the decay has nowhere to go.');
  r('S', true, [O('else if (v.stage === 2) v.env += (s.S - v.env)*dC;')]);
  r('R', true, [O('else if (v.stage === 4){ v.env -= v.env*rC;')]);
  r('gain', true, [O('L[i] = Math.tanh(yl*s.gain*1.6);')]);
  r('polyMode', true, [E('if (d.polyMode) return super.noteOn(note, freq, vel);')], 'The probe overlaps notes, so mono/legato differ from poly.');
  r('glide', ne('polyMode', 0), [O('const glide = !fresh && (held || d.glideAlways) && s.glide > 1;')], 'Poly note-on sets the pitch outright.');
  r('glideAlways', all(ne('polyMode', 0), gt('glide', 1)), [O('const glide = !fresh && (held || d.glideAlways) && s.glide > 1;')]);
  r('os', true, [O('setOS(n){')]);
  const disc = (b) => { const x = b === 1 ? '' : '2', mode = 'mode' + x, hot = 'hot' + x, hard = 'hard' + x;
    const reflect = b === 1 ? eq('mirror', 1) : any(eq('mirror2', 1), all(eq('mirror2', -1), eq('mirror', 1)));
    return all(P('B' + b), any(eq(hard, 0), all(inn(mode, CAR), inn(hot, [2, 3, 4])), eq(mode, 3), all(eq(mode, 6), any(le(hard, 0.001), reflect)))); };
  r('aa', any(inn('base', [2, 3, 4]), disc(1), disc(2)), [O('if (s.aa && dphi > 0 && dphi < 0.5){'), O('if (b === 2 || b === 4) this.tryE(m, 0.5, p0, dphi, c, k, s);'),
    O("if (carrier && (g.hot === 2 || g.hot === 4)){ off = 0.5; step = 1; }")],
    'Band-limiting corrects JUMPS: the base wave\'s (saw, rev saw, square), a hard-edged blade\'s edges, a discontinuous blade wave in a carrier mode, Noise and hard Crush steps, and a REFLECTED Crush at any slew. ' +
    'FINDING (probe-found, then read): reflect replays the hold levels backwards, so at the blade EXIT the level returns to the blade START\'s base value (crushLevel\'s j 0 slews from wave(base, st)), while the slew-to-base lands at the blade\'s middle (st + wEff, wEff = w/2); the voice then jumps by depth·(base(st) − base(st + w)) at the exit. A smooth patch has nothing to correct.');
  r('cScale', false, [O('const floor = 6.283185307179586*3*(this.d.cScale ? f/110 : 1);'), E("settle(v) { if (this.src === 'razor') super.settle(v); }"),
    E('for (let i = 0; i < N; i++) v.m[i].inc = Math.max(0, glideOn ? S.fRun[i] : S.eff[i]);')],
    'NEVER heard in the composed engine: keff() feeds only RazorCore\'s coupling increments (overwritten by the swarm) and settle() (skipped). ACCOUNTING row 29: absK/cScale not voiced.');
  r('onset', P('SW'), [S('s.Kenv = 8 * this.p.onset * this.p.onset;')], 'JS reference: ±onset play the same burst (B298 finding 1).');
  r('dissolve', all(P('SW'), ne('onset', 0)), [S('s.Kenv *= Math.exp(-dt / Math.max(0.01, p.dissolve));')]);
  r('driftDepth', true, [S('if (p.driftDepth > 0) {')], 'Drift walks even a single member\'s pitch.');
  r('h.driftRate', gt('driftDepth', 0), [S('const dm = p.driftMode | 0, rate = (0.2 + p.driftRate * 8);')]);
  r('h.law', P('DETUNED'), [S('if (p.law === 0) { f = s.f0 * Math.pow(2, (x * dep * 100) / 1200); }')]);
  r('harmReach', all(P('DETUNED'), eq('h.law', 4)), [S('else if (p.law === 4) { f = s.f0 * (1 + dep * p.harmReach * i); }')]);
  r('stretchB', all(P('DETUNED'), eq('h.law', 5)), [S('f = s.f0 * (1 + rat * (1 + p.stretchB * x * x));')]);
  r('spread', P('DETUNED'), [S('const dep = p.detune * p.spread;')]);
  r('anchor', all(P('DETUNED'), ne('h.law', 4)), [S('let f; const x = this.x[i] - p.anchor * this.xmin;')], 'Harmonic ignores x, so it ignores the anchor.');
  const moving = any(all(P('SW'), any(ne('K', 0), ne('onset', 0))), gt('driftDepth', 0), ne('polyMode', 0));
  r('inertia', moving, [S('if (w <= 0.001) {'), E('p.inertia = c === 0.5 ? Math.sqrt(k) : Math.pow(k, c);')],
    'Momentum toward a target that never moves (no coupling, no onset burst, no drift, no mono/legato pitch change) changes nothing.');
  r('inertiaCurve', all(gt('inertia', 0), moving), [E('p.inertia = c === 0.5 ? Math.sqrt(k) : Math.pow(k, c);')]);
  return R;
}

/* ---------------------------------------------------------------- anchors → file:line */
const SRC = {};
export function locate(anchor) {
  const [fk, text] = anchor, file = FILES[fk];
  const src = SRC[fk] || (SRC[fk] = readRepo(file));
  const i = src.indexOf(text);
  if (i < 0) throw new Error(`gen_dependency_tree: anchor not found in ${file}: ${JSON.stringify(text)} — the engine changed; re-derive the condition that rests on it`);
  return { file, line: src.slice(0, i).split('\n').length, text };
}

/* ---------------------------------------------------------------- the lab's greying (executed) */
function labClaims() {
  const lab = loadLab(), html = lab.html;
  const cut = (a, b) => { const i = html.indexOf(a), j = i < 0 ? -1 : html.indexOf(b, i); if (i < 0 || j < 0) throw new Error('lab anchor missing: ' + a); return html.slice(i, j); };
  const code = cut('const FOLLOW = [', '/* ---- B293') + cut('function linked(key2) {', '/* the value a control SHOWS') +
    cut('const RULE_OWN', 'function afterWrite(') + cut('function bladeKey(b, k)', 'function bladeSummary(');
  const Pst = {};
  const f = new Function('P', 'SP', 'E_RULE', '"use strict";\n' + code + '\nreturn { inertWhy, linked, isFM, FOLLOW_OF };');
  const api = f(Pst, lab.SP, lab.E_RULE);
  const pins = {
    fmGhost: locate(['lab', "const g3 = grp('FM', () => !isFM(b), 'fm');"]),
    shapeGhost: locate(['lab', "cmpRow(c1, 'Shape', 'morph', 'morph2', T, { ghost: bl => P[bladeKey(bl, 'hot')] !== 6 });"]),
    bladeOff: locate(['lab', "const row = el('div', 'blade' + (b === 2 ? ' b2' : '') + (on ? '' : ' off'));"]),
    inertWhy: locate(['lab', 'function inertWhy(key) {']),
    follow: locate(['lab', 'function linked(key2) {']),
  };
  const FACE = ['hot', 'rotRate', 'w', 'k', 'kHz', 'c', 'depth', 'hard', 'mirror', 'I', 'm', 'mHz', 'bspread', 'kspread', 'kRuleAmt'];
  const blade2key = k => (k === 'rotRate' ? 'rotRate2' : k + '2');
  /* returns [{key, why, pin}] the lab draws inert / ghosted / following in context C */
  function claims(C) {
    for (const k of Object.keys(Pst)) delete Pst[k];
    Object.assign(Pst, C);
    const out = [];
    for (const k of Object.keys(C)) {
      const why = api.inertWhy(k);
      if (why) out.push({ key: k, rule: 'inertWhy', why: why.split(':')[0] });
      /* a SWITCH-linked group (b2fm, b2sp, b2env, rot2Follow) makes the blade-2 row inert while it
         follows; a SENTINEL group (lock2/mirror2/frame2 = -1, "=1") is the selector itself, so it
         is never an inert claim */
      if (api.FOLLOW_OF[k] && !api.FOLLOW_OF[k].g.sentinel && api.linked(k)) out.push({ key: k, rule: 'follow', why: 'blade 2 follows blade 1\'s ' + api.FOLLOW_OF[k].src });
    }
    for (const b of [1, 2]) {
      const bk = k => (b === 1 ? k : k === 'rotRate' ? 'rotRate2' : k + '2');
      if (!api.isFM(b)) for (const k of ['I', 'm', 'mHz']) out.push({ key: bk(k), rule: 'fmGhost', why: 'FM group ghosted: blade ' + b + ' not in an FM mode' });
      if (C[b === 1 ? 'hot' : 'hot2'] !== 6) out.push({ key: b === 1 ? 'morph' : 'morph2', rule: 'shapeGhost', why: 'Shape ghosted: blade wave not Sine→Saw' });
      if (!(b === 1 ? C.b1on : C.b2on)) for (const k of FACE) out.push({ key: b === 1 ? k : blade2key(k), rule: 'bladeOff', why: 'blade ' + b + ' row drawn off' });
    }
    return out;
  }
  return { claims, pins };
}

/* ---------------------------------------------------------------- probe contexts */
export function hash32(...xs) { let h = 0x811C9DC5; for (const x of xs) { h ^= x >>> 0; h = Math.imul(h, 0x01000193) >>> 0; h ^= h >>> 13; } return h >>> 0; }
export function probeContext(seed) {
  const { params } = loadSpace(), rnd = mulberry32(seed), C = {};
  for (const p of params) {
    const u = rnd(), u2 = rnd();
    if (p.kind === 't') C[p.key] = u < (p.key === 'b1on' ? 0.85 : 0.7) ? 1 : 0;
    else if (p.kind === 'e') C[p.key] = p.opts[Math.floor(u * p.opts.length)];
    else if (p.kind === 's') C[p.key] = u < 0.8 ? p.default : ALT_KCUSTOM;
    else if (PROBE_OVERRIDES[p.key]) C[p.key] = p.min * Math.pow(PROBE_OVERRIDES[p.key] / p.min, u2);
    else C[p.key] = u < 0.3 ? p.default : valueAt(p, u2);
  }
  return C;
}
function alternatives(p, C) {
  if (p.kind === 'e') return p.opts;
  if (p.kind === 't') return [0, 1];
  if (p.kind === 's') return [p.default, ALT_KCUSTOM];
  return [p.default, valueAt(p, 0.15), valueAt(p, 0.85)];
}
function maxDelta(a, b) {
  let m = 0;
  for (const [x, y] of [[a.L, b.L], [a.R, b.R]]) for (let i = 0; i < x.length; i++) {
    const u = x[i], v = y[i];
    if (u === v || (u !== u && v !== v)) continue;
    const d = (u !== u || v !== v) ? Infinity : Math.abs(u - v);
    if (d > m) m = d;
  }
  return m;
}
/* probe param `key` in context C (base render shared by the caller): live iff some alternative moves the output */
function probeOne(key, C, base, seed) {
  const p = loadSpace().byKey[key];
  let best = 0;
  for (const v of alternatives(p, C)) {
    if (v === C[key]) continue;
    const r = render(Object.assign({}, C, { [key]: v }), SCRIPT, { seed });
    const d = maxDelta(base, r);
    if (d > best) best = d;
    if (best > TOL) break;
  }
  return best;
}
/* a task: {id, seed, keys} — render the base once, probe each key */
function runTask(t) {
  const C = probeContext(t.seed), rs = hash32(t.seed, 7);
  const base = render(C, SCRIPT, { seed: rs });
  return t.keys.map(k => probeOne(k, C, base, rs));
}

/* ---------------------------------------------------------------- orchestration */
export function buildTasks(tree, only) {
  const { params } = loadSpace();
  const keys = params.map(p => p.key).filter(k => !only || only.includes(k));
  const tasks = [];
  if (!only) for (let i = 0; i < SHARED_CONTEXTS; i++) tasks.push({ id: 'shared#' + i, seed: hash32(0xB316, 1, i), keys });
  params.forEach((p, pi) => {
    if (only && !only.includes(p.key)) return;
    for (const want of [true, false]) {
      let found = 0;
      for (let a = 0; a < TARGET_TRIES && found < TARGETED_PER_SIDE; a++) {
        const seed = hash32(0xB316, 2, pi, want ? 1 : 0, a);
        if (evalCond(tree.params[p.key].active_when, probeContext(seed), tree.predicates) === want) {
          tasks.push({ id: `target:${p.key}:${want ? 'live' : 'inert'}#${found}`, seed, keys: [p.key] }); found++;
        }
      }
    }
    /* each declared side channel: contexts where the role is false and the channel true, so a
       side channel that never fires is visible as UNCONFIRMED rather than silently assumed */
    (tree.params[p.key].side || []).forEach((sc, si) => {
      let found = 0;
      for (let a = 0; a < SIDE_TRIES && found < TARGETED_SIDE; a++) {
        const seed = hash32(0xB316, 3, pi, si, a), C = probeContext(seed);
        if (!evalCond(tree.params[p.key].active_when, C, tree.predicates) && evalCond(tree.predicates[sc], C, tree.predicates)) {
          tasks.push({ id: `target:${p.key}:${sc}#${found}`, seed, keys: [p.key] }); found++;
        }
      }
    });
  });
  return tasks;
}
export async function runTasks(tasks, workers) {
  workers = Math.max(1, Math.min(workers || Math.max(1, cpus().length - 2), tasks.length));
  const out = new Array(tasks.length);
  if (workers === 1) { tasks.forEach((t, i) => { out[i] = runTask(t); }); return out; }
  const idx = tasks.map((_, i) => i);
  const shares = Array.from({ length: workers }, (_, w) => idx.filter(i => i % workers === w));
  let done = 0;
  await Promise.all(shares.map(share => new Promise((res, rej) => {
    const wk = new Worker(new URL(import.meta.url), { workerData: { kind: 'probe', tasks: share.map(i => tasks[i]) } });
    wk.on('message', m => { if (m.progress) { done++; if (done % 50 === 0) process.stderr.write(`  probe ${done}/${tasks.length}\n`); return; }
      m.results.forEach((r, j) => { out[share[j]] = r; }); res(); });
    wk.on('error', rej);
    wk.on('exit', c => { if (c) rej(new Error('probe worker exit ' + c)); });
  })));
  return out;
}
/* compare the probe with the static tree (and the lab's claims). THREE OUTCOMES per
   (context, parameter) where the probe and the role disagree:
     probe live, role inert, a declared side channel true  -> a SIDE-CHANNEL HIT (counted);
     probe live, role inert, no side channel true          -> STRUCTURAL disagreement
                                                              ("undeclared-live": the tree is wrong);
     probe inert, role live                                -> REGIME (the role holds but this
                                                              context's values leave nothing to hear:
                                                              blades that never overlap, a blade
                                                              narrowed under the bypass by its spread
                                                              or envelope, a jump-free patch). Reported
                                                              per parameter, never dropped.
   And per parameter: role declared live in >= 2 contexts but NEVER probe-live there is
   STRUCTURAL too ("declared-live-never-live": the condition names the wrong switch value). */
export function compare(tree, tasks, results, lab) {
  const per = {}, dis = [], regime = [], labDis = [], sideHits = {};
  for (const k of Object.keys(tree.params)) per[k] = { contexts: 0, live: 0, inert: 0, declaredLive: 0, declaredInert: 0, liveInDeclared: 0,
    regime: 0, sideHits: 0, minLive: Infinity, maxInert: 0 };
  tasks.forEach((t, ti) => {
    const C = probeContext(t.seed), lc = lab ? lab.claims(C) : [];
    t.keys.forEach((k, ki) => {
      const d = results[ti][ki], live = d > TOL, P = tree.params[k], s = per[k];
      const decl = evalCond(P.active_when, C, tree.predicates);
      s.contexts++; live ? s.live++ : s.inert++; decl ? s.declaredLive++ : s.declaredInert++;
      if (live) s.minLive = Math.min(s.minLive, d); else s.maxInert = Math.max(s.maxInert, d);
      if (live && decl) s.liveInDeclared++;
      else if (!live && decl) { s.regime++; regime.push({ key: k, context: t.id }); }
      else if (live && !decl) {
        const sc = (P.side || []).filter(n => evalCond(tree.predicates[n], C, tree.predicates));
        if (sc.length) { s.sideHits++; for (const n of sc) sideHits[n] = (sideHits[n] || 0) + 1; }
        else dis.push({ kind: 'undeclared-live', key: k, context: t.id, deltaDecade: decade(d) });
      }
      if (live) for (const c of lc) if (c.key === k) labDis.push({ key: k, context: t.id, rule: c.rule, lab: c.why, deltaDecade: decade(d),
        via: decl ? 'role' : 'side channel' });
    });
  });
  for (const k of Object.keys(per)) if (per[k].declaredLive >= 2 && per[k].liveInDeclared === 0)
    dis.push({ kind: 'declared-live-never-live', key: k, contexts: per[k].declaredLive });
  return { per, dis, regime, labDis, sideHits };
}
const decade = d => (d === Infinity ? 'nonfinite' : d > 0 ? Math.floor(Math.log10(d)) : null);

/* ---------------------------------------------------------------- the tree */
export function staticTree() {
  const R = staticRules(), { params, excluded, unsampled } = loadSpace();
  const predicates = {}, predDoc = {};
  for (const [n, [c, why, anchors]] of Object.entries(PRED)) { predicates[n] = c; predDoc[n] = { why, source: anchors.map(locate) }; }
  const missing = params.filter(p => !R[p.key]).map(p => p.key), extra = Object.keys(R).filter(k => !loadSpace().byKey[k]);
  if (missing.length || extra.length) throw new Error(`gen_dependency_tree: static rules out of step with the lab table — missing ${missing.join(',') || '-'}; unknown ${extra.join(',') || '-'}`);
  const out = {};
  for (const p of params) {
    const r = R[p.key];
    out[p.key] = { label: p.label, kind: p.kind, tier: p.tier, id: p.id,
      range: p.kind === 'c' ? { min: p.min, max: p.max, taper: p.curve, default: p.default }
        : { options: p.opts, default: p.default, ...(p.disabledOpts.length ? { disabled: p.disabledOpts } : {}) },
      active_when: r.cond, depends_on: [...condKeys(r.cond, predicates)].sort(),
      side_channels: r.side, source: r.anchors.map(locate), note: r.note };
  }
  return { predicates, predDoc, params: out, excluded, unsampled };
}
/* roles, and a deterministic sampling order: Tarjan SCCs of depends_on, condensed */
function order(params) {
  const keys = Object.keys(params), readBy = {};
  for (const k of keys) for (const d of params[k].depends_on) (readBy[d] = readBy[d] || []).push(k);
  let index = 0; const idx = {}, low = {}, st = [], on = {}, sccs = [];
  const strong = v => {
    idx[v] = low[v] = index++; st.push(v); on[v] = true;
    for (const w of params[v].depends_on) { if (!(w in params)) continue; if (!(w in idx)) { strong(w); low[v] = Math.min(low[v], low[w]); } else if (on[w]) low[v] = Math.min(low[v], idx[w]); }
    if (low[v] === idx[v]) { const c = []; let w; do { w = st.pop(); on[w] = false; c.push(w); } while (w !== v); sccs.push(c.sort((a, b) => keys.indexOf(a) - keys.indexOf(b))); }
  };
  for (const k of keys) if (!(k in idx)) strong(k);
  return { order: sccs.flat(), cycles: sccs.filter(c => c.length > 1 || params[c[0]].depends_on.includes(c[0])), readBy };
}

export async function generate(opt) {
  opt = opt || {};
  const st = staticTree(), lab = labClaims();
  const tree = { predicates: st.predicates, params: Object.fromEntries(Object.entries(st.params).map(([k, p]) => [k, { active_when: p.active_when, side: p.side_channels }])) };
  const tasks = buildTasks(tree);
  const results = await runTasks(tasks, opt.workers);
  const { per, dis, regime, labDis, sideHits } = compare(tree, tasks, results, lab);
  const { order: ord, cycles, readBy } = order(st.params);
  for (const k of Object.keys(st.params)) {
    const p = st.params[k], s = per[k];
    p.role = readBy[k] ? (p.kind === 'c' ? 'gate' : 'switch') : 'leaf';
    p.gates = (readBy[k] || []).slice().sort();
    p.probe = { contexts: s.contexts, live: s.live, inert: s.inert, declaredLive: s.declaredLive, declaredInert: s.declaredInert,
      liveInDeclared: s.liveInDeclared, regime: s.regime, sideChannelHits: s.sideHits,
      minLiveDeltaDecade: s.live ? decade(s.minLive) : null, inertExactlyZero: s.maxInert === 0 };
  }
  const labByKey = {};
  for (const d of labDis) { const id = d.key + '|' + d.rule + '|' + d.via; const g = labByKey[id] || (labByKey[id] = { key: d.key, rule: d.rule, lab: d.lab, via: d.via, contexts: [] }); g.contexts.push(d.context); }
  const labSummary = Object.values(labByKey).map(g => ({ key: g.key, rule: g.rule, lab: g.lab, liveVia: g.via, liveIn: g.contexts.length, example: g.contexts[0] }));
  return {
    schema: 'hypersaw.patchspace.dependency-tree/0 (PROVISIONAL, local; swap-in point for FOUNDATIONS\' manifest schema, B275 P3)',
    generator: 'tools/patchspace/gen_dependency_tree.mjs', queue: 'B316 P1', engine: {
      composed: FILES.engine, oracle: FILES.oracle, swarm: FILES.swarm, table: FILES.lab + ' (SP rows, toPos/fromPos)' },
    consumers: {
      b275: 'This is the manifest\'s "active_when" field in embryo: one condition per parameter over switch and gate parameters, with its evidence. A B275 manifest row = {id, units/taper (range), morph class, modulation limits, preset participation, ACTIVE_WHEN (this)}.',
      morph: 'Exemption and greying: a corner-to-corner morph need not interpolate a parameter whose active_when is false at BOTH ends (it is inaudible there); greying in a morph editor draws a lane inert where active_when reads false at the current point. A parameter whose active_when itself reads a morphing gate (role gate) is where a morph can switch audibility mid-travel: those are listed per switch/gate under `gates`.',
      modMatrix: 'Destination eligibility: a destination is offered iff its active_when is satisfiable (cScale is not: it is never heard in this engine); a lane is drawn inert while active_when reads false under the current switches. A modulated GATE (e.g. w near 0.004, depth near 0) can toggle other parameters\' audibility at modulation rate — B199\'s territory, flagged by role.',
      sampler: 'P3 draws only parameters whose active_when holds (fixpoint over `order`); the rest stay at their defaults.',
    },
    grammar: 'true | false | {"eq":[k,v]} {"ne":[k,v]} {"in":[k,[v…]]} {"ge":[k,x]} {"gt":[k,x]} {"le":[k,x]} {"absgt":[k,x]} | {"all":[…]} {"any":[…]} {"not":c} {"pred":name}. Evaluator: tools/patchspace/space.mjs evalCond. Keys are engine keys; b1on is the lab mapping (off => w 0).',
    probe: { tolerance: TOL, measure: 'max |ΔL|,|ΔR| over the float32 render', sampleRate: 48000, script: SCRIPT,
      alternatives: 'enum: every option; toggle: 0/1; continuous: default, taper 0.15, taper 0.85; string: default, ' + JSON.stringify(ALT_KCUSTOM),
      contexts: { shared: SHARED_CONTEXTS, targetedPerSide: TARGETED_PER_SIDE, targetedPerSideChannel: TARGETED_SIDE, targetTries: TARGET_TRIES, sideTries: SIDE_TRIES,
        sampler: 'mulberry32 per context; toggles on w.p. 0.85 (b1on) / 0.7; enums uniform; continuous: default w.p. 0.3, else uniform in taper', overrides: PROBE_OVERRIDES },
      tasks: tasks.length, comparisons: results.reduce((a, r) => a + r.length, 0) },
    summary: { params: Object.keys(st.params).length, switches: Object.values(st.params).filter(p => p.role === 'switch').length,
      gates: Object.values(st.params).filter(p => p.role === 'gate').length, leaves: Object.values(st.params).filter(p => p.role === 'leaf').length,
      neverLive: Object.keys(st.params).filter(k => st.params[k].active_when === false),
      alwaysLive: Object.keys(st.params).filter(k => st.params[k].active_when === true),
      structuralDisagreements: dis.length, regimeInert: regime.length, sideChannelHits: Object.values(sideHits).reduce((a, b) => a + b, 0),
      labGreysButLive: labSummary.length },
    disagreements: dis,
    regime: { note: 'role declared live, probe inert in THIS context (values leave nothing to hear). Per parameter: count and the first context.',
      byParam: Object.fromEntries(Object.keys(per).filter(k => per[k].regime).map(k => [k, { regime: per[k].regime, of: per[k].declaredLive, first: regime.find(r => r.key === k).context }])) },
    sideChannels: Object.fromEntries(Object.keys(st.predicates).filter(n => n.startsWith('SC_')).map(n => [n, {
      params: Object.keys(st.params).filter(k => st.params[k].side_channels.includes(n)), probeHits: sideHits[n] || 0,
      status: sideHits[n] ? 'confirmed by the probe' : 'UNCONFIRMED: declared from the code, never observed by the probe' }])),
    lab: { pins: lab.pins, greysButLive: labSummary },
    predicates: Object.fromEntries(Object.entries(st.predicates).map(([n, c]) => [n, { when: c, why: st.predDoc[n].why, source: st.predDoc[n].source }])),
    order: ord, cycles,
    params: st.params,
    excluded: st.excluded, unsampled: st.unsampled,
  };
}
/* the tree as the evaluator wants it: predicates flattened to conditions */
export function asEvalTree(json) {
  return { predicates: Object.fromEntries(Object.entries(json.predicates).map(([n, p]) => [n, p.when])),
    params: Object.fromEntries(Object.entries(json.params).map(([k, p]) => [k, { active_when: p.active_when, side: p.side_channels }])) };
}
export const serialize = obj => JSON.stringify(obj, null, 1) + '\n';

/* worker branch keyed by kind: other tools import this module inside THEIR workers */
if (!isMainThread && workerData && workerData.kind === 'probe') {
  const res = workerData.tasks.map(t => { const r = runTask(t); parentPort.postMessage({ progress: 1 }); return r; });
  parentPort.postMessage({ results: res });
} else if (process.argv[1] && process.argv[1].endsWith('gen_dependency_tree.mjs')) {
  const w = process.argv.indexOf('--workers');
  const json = await generate({ workers: w > 0 ? +process.argv[w + 1] : undefined });
  const text = serialize(json);
  if (process.argv.includes('--stdout')) process.stdout.write(text);
  else { writeFileSync(join(ROOT, OUT), text); }
  const s = json.summary;
  console.error(`gen_dependency_tree: ${s.params} params (${s.switches} switches, ${s.gates} gates, ${s.leaves} leaves); ` +
    `${json.probe.tasks} probe contexts, ${json.probe.comparisons} comparisons; STRUCTURAL disagreements ${s.structuralDisagreements}, ` +
    `regime-inert ${s.regimeInert}, side-channel hits ${s.sideChannelHits}; ` +
    `lab greys-but-live ${s.labGreysButLive}${process.argv.includes('--stdout') ? '' : ' → ' + OUT}`);
}
