/*
 * gui3_seam_check.mjs — GUI 3's engine seam holds its contract (ROADMAP B584, ADR-316).
 * WIRED: ./verify fast (beside lab_load_check).
 *
 * WHY THIS EXISTS. GUI 3's shell runs on a no-audio stub engine (docs/design/gui3/engine-stub.js) behind one
 * adapter (docs/design/gui3/device.js), and the WebAssembly engine (SPEC-ENGINE IF-10, clean-room stage 8)
 * replaces the stub behind the same seam later. A stub that quietly drifts from the interface it stands in
 * for makes the shell look wired while it is not, and the day the real engine plugs in is the day that
 * shows. So the stub and the adapter are held to the spec's own words, here, every run.
 *
 * WHAT IS ASSERTED (each a named test; each plant below must turn its own test red):
 *   seam         the stub has every method of GUI3Engine.METHODS; the adapter refuses an engine missing one, by name
 *   laws         5.1's laws: for every continuous host row, x(v=0) and x(v=1) are the range's ends (dB: −∞ at 0),
 *                x(default v) is the spec's default (the generator inverted the laws in Python, so the two
 *                implementations check each other); `lin, wraps` wraps; every stepped default v names the
 *                spec's default option
 *   params       a set clamps to [0, 1] and snaps a stepped row to its grid; an unknown key, a reserved row's
 *                key (5.1) and a non-number are refused and counted (PA-7)
 *   state        IF-4: save, load, save is byte-identical; floats round-trip exactly; a state's unknown keys
 *                and members are kept and written back (RC-9.3); a document without `version` is refused
 *   snapshot     RC-11.2: snapshot, change, restore gives the state document back byte for byte
 *   observe      IF-10: reading the view changes nothing, twice gives the same answer, and editing the
 *                answer does not reach the engine
 *   gestures     RC-11.3: begin and end come back out unchanged, in order, once
 *   morph        corner A is the patch with morph off; Morphable rows blend by the pad's weights,
 *                Structural rows read their owner's corner, Device rows ignore the morph; a nudge
 *                inside the hysteresis band does not flip an owner
 *   routes       RC-4: a route to a continuous host row from a published source is taken; to a stepped
 *                row, a reserved row, from an unknown source, or with a depth outside 0..1 is refused
 *   history      RC-11: one write outside a transaction is one step, a write that changes nothing is
 *                none, a begin/end bracket of several writes is one; undo restores the engine's state
 *                document exactly, redo re-applies it, and the gesture events reached the engine
 *   determinism  the same seed and calls give byte-identical device and engine state; another seed differs
 *
 * MUST-FAIL CONTROLS (plants), every run, on in-memory copies of the two scripts: an observe() that
 * counts, an undo that applies the wrong side, a scatter that draws from a counter, a route check
 * that admits stepped rows, a log law off by 0.1 %, an adapter that skips the seam check, a load that
 * drops unknown keys, and a gesture that is never reported. Each must turn its own test red (and its
 * anchor must exist, or the plant itself is reported as vacuous).
 *
 * Usage: node tools/labharness/gui3_seam_check.mjs [--verbose]   (exit 1 on any failure; --verbose prints
 *        what each plant turned red, to read that a control failed for the right reason)
 */
// B446 W3b: this file evaluates lab scripts, so it runs under Node's permission model (see sandbox_guard.mjs).
import './sandbox_guard.mjs';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const root = join(dirname(fileURLToPath(import.meta.url)), '../..');
const G = join(root, 'docs/design/gui3');
const SRC = {
  data: readFileSync(join(G, 'shell-data.js'), 'utf8'),
  stub: readFileSync(join(G, 'engine-stub.js'), 'utf8'),
  device: readFileSync(join(G, 'device.js'), 'utf8'),
};

// The three scripts are classic scripts that assign to `window`; give them a fresh one each load.
function load(src) {
  const win = {};
  for (const k of ['data', 'stub', 'device']) new Function('window', src[k])(win);
  return win;
}

class Fail extends Error {}
const ok = (cond, msg) => { if (!cond) throw new Fail(msg); };
const near = (a, b) => a === b || Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b));

function suite(W) {
  const DATA = W.GUI3_DATA, ENG = W.GUI3Engine, DEV = W.GUI3Device;
  const rows = DATA.params, host = rows.filter(r => !r.reserved);
  const by = Object.fromEntries(rows.map(r => [r.key, r]));
  const fresh = (seed = 1) => { const e = new ENG.Stub(rows); return { e, d: DEV.create(DATA, e, { seed }) }; };
  const firstOf = pred => host.find(pred);
  const cont = firstOf(r => r.law === 'lin' && r.class === 'M');
  const stepS = firstOf(r => r.law === 'step' && r.class === 'S' && r.n >= 3 && r.options);
  const devD = firstOf(r => r.class === 'D');
  const reserved = rows.find(r => r.reserved);
  const T = {};

  T.seam = () => {
    const { e } = fresh();
    for (const m of ENG.METHODS) ok(typeof e[m] === 'function', 'stub lacks ' + m);
    const partial = Object.fromEntries(ENG.METHODS.filter(m => m !== 'observe').map(m => [m, e[m]]));
    let threw = '';
    try { DEV.create(DATA, partial, {}); } catch (err) { threw = String(err.message); }
    ok(/observe/.test(threw), 'the adapter took an engine without observe()');
  };

  T.laws = () => {
    for (const r of host) {
      if (r.law === 'step') {
        const i = Math.round(r.default_v * (r.n - 1));
        if (r.options) ok(r.options[i].replace(/\s*\(.*\)$/, '').toLowerCase() === r.default_text.toLowerCase()
          || Number(r.options[i].replace('−', '-').replace(/ /g, '')) === Number(r.default_text.replace('−', '-').replace(/ /g, '')),
          r.key + ': default v names ' + r.options[i] + ', the spec says ' + r.default_text);
        else ok(r.lo + i === Number(r.default_text.replace(/ /g, '')), r.key + ': default index');
        continue;
      }
      const x0 = ENG.lawX(r, 0), x1 = ENG.lawX(r, 1), xd = ENG.lawX(r, r.default_v);
      if (r.law === 'dB') ok(x0 === -Infinity, r.key + ': a dB row is −∞ at v = 0');
      else if (r.law === 'log, off zone') ok(x0 === 0, r.key + ': off below the zone');
      else ok(near(x0, r.lo), r.key + ': x(0) = ' + x0 + ', not ' + r.lo);
      if (r.law !== 'lin, wraps') ok(near(x1, r.hi), r.key + ': x(1) = ' + x1 + ', not ' + r.hi);
      ok(near(xd, r.default_x), r.key + ': x(default v) = ' + xd + ', the spec says ' + r.default_x);
      if (r.law === 'lin, wraps') ok(near(ENG.lawX(r, 1.25), ENG.lawX(r, 0.25)), r.key + ': does not wrap');
    }
  };

  T.params = () => {
    const { e } = fresh();
    ok(e.setParam(cont.key, 1.7) === 1 && e.setParam(cont.key, -3) === 0, 'set does not clamp');
    const got = e.setParam(stepS.key, 0.49 / (stepS.n - 1) + 1 / (stepS.n - 1));
    ok(near(got, 1 / (stepS.n - 1)), 'a stepped set does not snap to its grid (' + got + ')');
    const r0 = e.observe().refused;
    ok(e.setParam('no.such.key', .5) === null && e.setParam(reserved.key, .5) === null && e.setParam(cont.key, NaN) === null, 'a bad set was taken');
    ok(e.observe().refused === r0 + 3, 'refusals are not counted');
    ok(e.getParam(reserved.key) === 0, 'a reserved row reads other than 0');
  };

  T.state = () => {
    const { e } = fresh();
    e.setParam(cont.key, 0.1 + 0.2);
    const s1 = e.saveState(), d = JSON.parse(s1);
    ok(d.version === 1 && Object.keys(d.params).length === rows.length, 'the document lacks version 1 or a row');
    const e2 = new ENG.Stub(rows);
    ok(e2.loadState(s1).ok && e2.saveState() === s1 && e2.getParam(cont.key) === 0.1 + 0.2, 'save, load, save is not byte-identical');
    d.params['future.key'] = 0.25; d.future = { kept: true };
    const withUnknown = JSON.stringify(d), e3 = new ENG.Stub(rows);
    ok(e3.loadState(withUnknown).ok, 'a state with an unknown key was refused');
    const back = JSON.parse(e3.saveState());
    ok(back.params['future.key'] === 0.25 && back.future && back.future.kept === true, 'unknown keys were dropped (RC-9.3)');
    ok(!new ENG.Stub(rows).loadState(JSON.stringify({ params: {} })).ok, 'a version-0 document was taken');
  };

  T.snapshot = () => {
    const { e } = fresh();
    const s0 = e.saveState(), snap = e.snapshot();
    e.setParam(cont.key, 0.9); e.setParam(stepS.key, 1);
    ok(e.saveState() !== s0, 'the change changed nothing (control)');
    ok(e.restore(snap) && e.saveState() === s0, 'restore did not give the state back');
  };

  T.observe = () => {
    const { e, d } = fresh();
    d.setMorph({ on: true, x: .2, y: .8 });
    const s0 = e.saveState(), o1 = JSON.stringify(e.observe()), o2 = JSON.stringify(e.observe());
    ok(o1 === o2 && e.saveState() === s0, 'reading the view changed something');
    const o = e.observe(); o.heard[stepS.key] = 'tampered'; o.counters.voicesSounding = 9;
    ok(JSON.stringify(e.observe()) === o1, 'editing the view reached the engine');
    ok(o.audio === false && Array.isArray(o.voices) && o.voices.length === 0, 'the stub claims audio');
    ok(Object.keys(JSON.parse(o1).heard).length === host.filter(r => r.law === 'step').length, 'the heard stepped values are not all there (IF-4)');
  };

  T.gestures = () => {
    const { e } = fresh();
    e.events();
    e.gesture(cont.key, 'begin'); e.setParam(cont.key, .3); e.gesture(cont.key, 'end');
    const ev = e.events();
    ok(JSON.stringify(ev) === JSON.stringify([{ type: 'gesture', key: cont.key, phase: 'begin' }, { type: 'gesture', key: cont.key, phase: 'end' }]), 'gestures not reported unchanged: ' + JSON.stringify(ev));
    ok(e.events().length === 0, 'events are reported twice');
  };

  T.morph = () => {
    const { e, d } = fresh();
    d.set(cont.key, .1, { corner: 3 });              // with morph off a corner request is ignored: corner A is the patch
    ok(near(d.heard(cont.key), .1) && near(e.getParam(cont.key), .1) && d.corner(cont.key, 3) === cont.default_v, 'morph off: corner A is not the patch');
    d.setMorph({ on: true, x: 0, y: 0 });
    for (let i = 1; i < 4; i++) d.set(cont.key, [.1, .3, .6, .9][i], { corner: i });
    ok(near(e.getParam(cont.key), .1), 'at (0, 0) the engine does not hear corner A');
    d.setMorph({ x: 1, y: 1 });
    ok(near(e.getParam(cont.key), .9), 'at (1, 1) the engine does not hear corner D');
    d.setMorph({ x: .25, y: .5 });
    const w = d.weights(), blend = w[0] * .1 + w[1] * .3 + w[2] * .6 + w[3] * .9;
    ok(near(w.reduce((a, b) => a + b, 0), 1) && near(e.getParam(cont.key), blend), 'a Morphable row does not blend by the weights');
    for (let i = 0; i < 4; i++) d.set(stepS.key, i % 2 ? 1 : 0, { corner: i });
    const own = d.owner(stepS.key);
    ok(e.getParam(stepS.key) === d.corner(stepS.key, own), 'a Structural row does not read its owner');
    d.setMorph({ x: .25 + 1e-4 });
    ok(d.owner(stepS.key) === own, 'an owner flipped inside the hysteresis band');
    const dv = d.heard(devD.key); d.setMorph({ x: .9, y: .1 });
    ok(d.heard(devD.key) === dv && d.owner(devD.key) === null, 'a Device row moved with the morph');
  };

  T.routes = () => {
    const { d } = fresh();
    const h0 = d.history().past.length;
    const good = d.addRoute({ src: 'env2', param: cont.key, pol: 'uni', depth: .4 });
    ok(good.ok && d.routes().length === 1 && d.history().past.length === h0 + 1, 'a valid route was not taken as one step');
    ok(!d.addRoute({ src: 'env2', param: stepS.key }).ok, 'a route to a stepped row was taken');
    ok(!d.addRoute({ src: 'env2', param: reserved.key }).ok, 'a route to a reserved row was taken');
    ok(!d.addRoute({ src: 'lfo9', param: cont.key }).ok, 'a route from an unpublished source was taken');
    ok(!d.addRoute({ src: 'env2', param: cont.key, depth: 1.5 }).ok, 'a depth outside 0..1 was taken');
    ok(d.removeRoute(good.id) && d.routes().length === 0, 'remove failed');
  };

  T.history = () => {
    const { e, d } = fresh();
    e.events();
    const s0 = e.saveState();
    d.set(cont.key, .7);
    ok(d.history().past.length === 1, 'one write is not one step');
    d.set(cont.key, .7);
    ok(d.history().past.length === 1, 'a write that changed nothing made a step');
    d.begin('drag', cont.key); d.set(cont.key, .2); d.set(cont.key, .25); d.set(cont.key, .3); d.end();
    ok(d.history().past.length === 2, 'a bracket of three writes is not one step');
    const ev = e.events().map(x => x.phase).join(',');
    ok(ev === 'begin,end', 'the gesture did not reach the engine: ' + ev);
    const s2 = e.saveState();
    ok(d.undo() && d.undo() && e.saveState() === s0, 'undo did not restore the engine’s state document');
    ok(d.redo() && d.redo() && e.saveState() === s2, 'redo did not re-apply');
  };

  T.determinism = () => {
    const run = seed => {
      const { e, d } = fresh(seed);
      d.scatter(seed); d.setMorph({ on: true, x: .31, y: .64 }); d.set(stepS.key, 1, { corner: 2 });
      d.addRoute({ src: 'velocity', param: cont.key, depth: .2 }); d.setMorph({ x: .7 });
      return JSON.stringify(d.patch()) + e.saveState() + JSON.stringify(e.observe());
    };
    ok(run(7) === run(7), 'the same seed and calls gave different state');
    ok(run(7) !== run(8), 'another seed gave the same state (the scatter is not seeded)');
  };

  const failed = {};
  for (const [name, fn] of Object.entries(T)) {
    try { fn(); } catch (err) { failed[name] = err instanceof Fail ? err.message : 'threw: ' + (err && err.stack ? err.stack.split('\n')[0] : err); }
  }
  return { names: Object.keys(T), failed };
}

// ---- the real run ----
const real = suite(load(SRC));
const bad = [];
for (const [n, m] of Object.entries(real.failed)) bad.push(`${n}: ${m}`);

// ---- the plants: each mutates one script and must turn exactly its own test red ----
const PLANTS = [
  ['observe', 'stub', 'E.observe = function () {', 'E.observe = function () { refused++;'],
  ['history', 'device', 'apply(e.before); return true;', 'apply(e.after); return true;'],
  ['determinism', 'device', "mulberry32(hash32(r.key) ^ (s * 2654435761 >>> 0))", "mulberry32(hash32(r.key) ^ (s * 2654435761 >>> 0) ^ (root.__n = (root.__n | 0) + 1))"],
  ['routes', 'device', "if (!r || r.law === 'step' || !r.pv)", 'if (!r)'],
  ['laws', 'stub', "case 'log': return lo * Math.pow(hi / lo, v);", "case 'log': return lo * Math.pow(hi / lo, v * 1.001);"],
  ['seam', 'device', 'if (missing.length) throw', 'if (false) throw'],
  ['state', 'stub', 'extra = keep;', 'extra = [];'],
  ['gestures', 'stub', "out.push({ type: 'gesture', key: key, phase: phase });", ''],
];
for (const [test, file, from, to] of PLANTS) {
  if (!SRC[file].includes(from)) { bad.push(`plant for ${test}: its anchor is gone from ${file}, so the control is vacuous`); continue; }
  const r = suite(load({ ...SRC, [file]: SRC[file].replace(from, to) }));
  if (!r.failed[test]) bad.push(`plant for ${test}: stayed GREEN (blind test)`);
  else if (process.argv.includes('--verbose')) console.log(`plant ${test}: red as it must be: ${r.failed[test]}`);
}

if (bad.length) {
  console.log('gui3_seam_check: RED\n  ' + bad.join('\n  '));
  process.exit(1);
}
console.log(`gui3_seam_check: GREEN — ${real.names.length} seam tests (${real.names.join(', ')}) on the stub and the adapter; ${PLANTS.length} planted faults each turned their test red`);
