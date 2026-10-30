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
 *   seam         the stub has every method of GUI3Engine.METHODS and the transport GUI3Engine.TRANSPORT's; the
 *                transport refuses an engine missing a method and the adapter a transport missing one, by name
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
 *   morphoff     ADR-319 item 1: with morph off a write lands on the audible corner only, a write to a frozen
 *                corner is refused, the audible corner is what the engine hears, and B to D come back unchanged
 *   morphhistory ADR-319 item 2: turning morph on is one history node and undo/redo toggle it; moving the pad is none
 *   snapshots    ADR-319 item 1: "save current sound" makes the heard values a named, saved corner; "save preset"
 *                keeps a corner's own values as its preset; revert returns to it; a blank name is refused
 *   sync         ADR-319 item 3, mirror and confirm: an echo is not an engine change; the engine's own change is
 *                shown until the user writes the key; a snapshot never overwrites the key being dragged; over an
 *                asynchronous transport a stale snapshot never overwrites a newer send
 *   pending      ADR-322 item 3: + makes a modulator pending (outside the patch and the history); closing the
 *                interface drops it unless it was mapped
 *   mapdrop      ADR-322 item 3: a map handle dropped on a stepped or reserved row is refused and the modulator
 *                stays pending; on a continuous row it maps
 *   paths        ADR-330: a hosted module's value reaches the rack (RC-5, IF-7), an Arp value the device's MIDI module,
 *                a morph setting and the position the resolver (IF-11, IF-12); none reaches the engine's setParam
 *   determinism  the same seed and calls give byte-identical device and engine state; another seed differs
 *
 * MUST-FAIL CONTROLS (plants), every run, on in-memory copies of the two scripts: an observe() that
 * counts, an undo that applies the wrong side, a scatter that draws from a counter, a route check
 * that admits stepped rows, a log law off by 0.1 %, a transport that skips the seam check, a load that
 * drops unknown keys, a gesture that is never reported, a morph-off write that reaches a frozen corner,
 * a morph toggle kept off the history, a snapshot save that keeps no preset, a snapshot that overwrites
 * a drag, a snapshot that overwrites a newer send, a close that keeps unmapped modulators, a drop that maps a
 * stepped row, a refused drop that clears the pending state, and the module, Arp and morph-setting values each
 * sent to the engine instead of their own side. Each must turn its own test red (and its
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
  // The engine holds SPEC-ENGINE 5's rows only; the morph's settings are the resolver's and the modules' and Arp's
  // the device side's (ADR-330). `all` is every row the shell shows.
  const all = DATA.params, rows = all.filter(r => r.target === 'engine'), host = rows.filter(r => !r.reserved);
  const by = Object.fromEntries(all.map(r => [r.key, r]));
  const sides = () => ({ R: new ENG.ResolverStub(all.filter(r => r.target === 'resolver')), X: new ENG.DeviceSideStub(all.filter(r => r.target === 'rack' || r.target === 'midi')) });
  const fresh = (seed = 1) => { const e = new ENG.Stub(rows), { R, X } = sides(), t = ENG.SyncTransport(e, R, X); return { e, t, R, X, d: DEV.create(DATA, t, { seed }) }; };
  /* An ASYNCHRONOUS transport, as the AudioWorklet's will be: the engine applies a message at once, but its
     snapshot waits in a queue until deliver() hands it over, so a snapshot can be older than the latest send. */
  const delayed = (seed = 1) => {
    const e = new ENG.Stub(rows), { R, X } = sides(), inner = ENG.SyncTransport(e, R, X), q = [], subs = [];
    let live = false;
    inner.onSnapshot(sn => { if (live) q.push(sn); });
    live = true;
    const t = { send: m => inner.send(m), onSnapshot: fn => subs.push(fn), deliver: n => { for (const sn of q.splice(0, n === undefined ? q.length : n)) subs.forEach(fn => fn(sn)); } };
    return { e, t, d: DEV.create(DATA, t, { seed }) };
  };
  const firstOf = pred => host.find(pred);
  const cont = firstOf(r => r.law === 'lin' && r.class === 'M');
  const stepS = firstOf(r => r.law === 'step' && r.class === 'S' && r.n >= 3 && r.options);
  const devD = firstOf(r => r.class === 'D');
  const reserved = rows.find(r => r.reserved);
  const T = {};

  T.seam = () => {
    const { e, t } = fresh();
    for (const m of ENG.METHODS) ok(typeof e[m] === 'function', 'stub lacks ' + m);
    for (const m of ENG.TRANSPORT) ok(typeof t[m] === 'function', 'the transport lacks ' + m);
    const partial = Object.fromEntries(ENG.METHODS.filter(m => m !== 'observe').map(m => [m, e[m]]));
    let threw = '';
    try { ENG.SyncTransport(partial); } catch (err) { threw = String(err.message); }
    ok(/observe/.test(threw), 'the transport took an engine without observe()');
    threw = '';
    try { DEV.create(DATA, { send: t.send }, {}); } catch (err) { threw = String(err.message); }
    ok(/onSnapshot/.test(threw), 'the adapter took a transport without onSnapshot()');
  };

  T.laws = () => {
    // Every continuous row the shell shows (the engine's, D.2's, the modules' and Arp's); the stepped-default check
    // reads the spec's own default text, so it runs on the engine's rows (a module's default is its stored value).
    for (const r of all.filter(q => !q.reserved && (q.target === 'engine' || q.law !== 'step'))) {
      if (r.law === 'step') {
        const i = Math.round(r.default_v * (r.n - 1));
        if (r.options) ok(r.options[i].replace(/\s*\(.*\)$/, '').toLowerCase() === r.default_text.toLowerCase()
          || Number(r.options[i].replace('−', '-').replace(/ /g, '')) === Number(r.default_text.replace('−', '-').replace(/ /g, '')),
          r.key + ': default v names ' + r.options[i] + ', the spec says ' + r.default_text);
        else ok(r.lo + i === Number(r.default_text.replace(/ /g, '')), r.key + ': default index');
        continue;
      }
      const x0 = ENG.lawX(r, 0), x1 = ENG.lawX(r, 1), xd = ENG.lawX(r, r.default_v);
      if (r.law === 'dB' || r.law === 'io-cut') ok(x0 === -Infinity, r.key + ': a dB row (or a cut-only output’s detent) is −∞ at v = 0');
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
    d.set(cont.key, .1);
    ok(near(d.heard(cont.key), .1) && near(e.getParam(cont.key), .1), 'morph off: corner A is not the patch');
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
    d.engineEvents();
    const s0 = e.saveState();
    d.set(cont.key, .7);
    ok(d.history().past.length === 1, 'one write is not one step');
    d.set(cont.key, .7);
    ok(d.history().past.length === 1, 'a write that changed nothing made a step');
    d.begin('drag', cont.key); d.set(cont.key, .2); d.set(cont.key, .25); d.set(cont.key, .3); d.end();
    ok(d.history().past.length === 2, 'a bracket of three writes is not one step');
    const ev = d.engineEvents().map(x => x.phase).join(',');
    ok(ev === 'begin,end', 'the gesture did not reach the engine: ' + ev);
    const s2 = e.saveState();
    ok(d.undo() && d.undo() && e.saveState() === s0, 'undo did not restore the engine’s state document');
    ok(d.redo() && d.redo() && e.saveState() === s2, 'redo did not re-apply');
  };

  /* ADR-319 item 1: with morph off only the audible corner sounds and takes writes; B to D are frozen. */
  T.morphoff = () => {
    const { e, d } = fresh();
    d.setMorph({ on: true });
    [.2, .5, .8].forEach((v, k) => d.set(cont.key, v, { corner: k + 1 }));
    const bd = [1, 2, 3].map(i => d.corner(cont.key, i));
    d.setMorph({ on: false });
    ok(d.set(cont.key, .42) === .42 && d.corner(cont.key, 0) === .42 && e.getParam(cont.key) === .42, 'morph off: a write did not land on the audible corner A');
    ok(d.set(cont.key, .9, { corner: 2 }) === null, 'morph off: a write to frozen corner C was taken');
    ok([1, 2, 3].every((i, k) => d.corner(cont.key, i) === bd[k]), 'morph off: B to D moved');
    ok(d.setAudible(2) && e.getParam(cont.key) === bd[1], 'the audible corner C is not what the engine hears');
    ok(d.set(cont.key, .33) === .33 && d.corner(cont.key, 2) === .33 && d.corner(cont.key, 0) === .42, 'morph off with C audible: the write did not land on C alone');
    d.setAudible(0); d.setMorph({ on: true });
    ok(d.corner(cont.key, 1) === bd[0] && d.corner(cont.key, 3) === bd[2], 'B and D did not come back unchanged when morph turned on');
  };
  /* ADR-319 item 2: toggling morph is one history node; the position is not. */
  T.morphhistory = () => {
    const { d } = fresh();
    const h0 = d.history().past.length;
    d.setMorph({ on: true });
    ok(d.history().past.length === h0 + 1, 'turning morph on made no history node');
    d.setMorph({ x: .1, y: .9 }); d.setMorph({ x: .7 });
    ok(d.history().past.length === h0 + 1, 'moving the pad made a history node');
    ok(d.undo() && !d.morph().on && d.redo() && d.morph().on, 'undo and redo do not toggle morph');
  };
  /* ADR-319 item 1: snapshots and presets on corners. */
  T.snapshots = () => {
    const { e, d } = fresh();
    d.setMorph({ on: true, x: .5, y: .5 }); d.set(cont.key, .8, { corner: 3 });
    const heardNow = d.heard(cont.key);
    ok(!d.saveSoundTo(1, '  ').ok && !d.saveSoundTo(5, 'x').ok, 'a blank name or a corner past D was taken');
    ok(d.saveSoundTo(1, ' Glass horde ').ok && d.corner(cont.key, 1) === heardNow && d.names()[1] === 'Glass horde' && !d.edited(1), 'the current sound did not become corner B, named and saved');
    d.set(cont.key, .1, { corner: 1 });
    ok(d.edited(1) && d.revert(1) && d.corner(cont.key, 1) === heardNow, 'revert did not return corner B to its saved preset');
    d.set(cont.key, .6, { corner: 2 });
    ok(d.savePreset(2, 'Under') .ok && !d.edited(2) && d.names()[2] === 'Under' && d.corner(cont.key, 2) === .6, 'save preset to C did not keep C\u2019s own values as its preset');
    const n = d.history().past.length; d.undo();
    ok(d.names()[2] === null && d.history().past.length === n - 1, 'saving a preset is not one undo step');
  };
  /* ADR-319 item 3: mirror and confirm. Echoes change nothing; the engine's own change is shown; a stale
     snapshot (older than the latest send) and a snapshot under a drag never overwrite the user's value. */
  T.sync = () => {
    const { e, t, d } = fresh();
    d.setMorph({ on: true, x: .3, y: .6 }); d.set(cont.key, .4); d.set(stepS.key, 1);
    ok(d.fromEngine().length === 0, 'an echo of our own sends was read as an engine change');
    e.setParam(cont.key, .9); t.publish();
    ok(d.shown(cont.key) === .9 && d.heard(cont.key) !== .9, 'an engine-originated change was not shown');
    d.set(cont.key, .45);
    ok(d.shown(cont.key) === d.heard(cont.key) && e.getParam(cont.key) === d.heard(cont.key) && d.fromEngine().length === 0, 'the user\u2019s write did not take the key back');
    const other = firstOf(r => r.law === 'lin' && r.class === 'M' && r.key !== cont.key);
    d.begin('drag', other.key); d.set(other.key, .3); e.setParam(other.key, .8); t.publish();
    ok(d.shown(other.key) === d.heard(other.key) && d.shown(other.key) !== .8, 'a snapshot overwrote the value being dragged (' + d.shown(other.key) + ')');
    d.end();
    const A = delayed();
    A.t.deliver();
    A.d.set(cont.key, .2); A.d.set(cont.key, .4);
    A.t.deliver(1);
    ok(A.d.shown(cont.key) === .4 && A.d.fromEngine().length === 0, 'a stale snapshot overwrote a newer send (' + A.d.shown(cont.key) + ')');
    A.t.deliver();
    ok(A.d.shown(cont.key) === .4 && A.e.getParam(cont.key) === .4, 'the confirmed snapshot disagrees');
  };

  /* ADR-322 item 3: a modulator activated with + is pending until mapped; closing the interface drops it. */
  T.pending = () => {
    const { d } = fresh();
    const h0 = d.history().past.length, shell0 = JSON.stringify(d.patch().shell);
    ok(d.activate('env3').ok && d.pending().join() === 'env3', '+ did not make ENV 3 pending');
    ok(!d.activate('env3').ok && !d.activate('lfo9').ok, 'a second activation, or an unpublished source, was taken');
    ok(d.history().past.length === h0 && JSON.stringify(d.patch().shell) === shell0, 'a pending modulator reached the patch or the history');
    ok(d.close().join() === 'env3' && d.pending().length === 0, 'closing the interface did not drop the unmapped ENV 3');
    d.activate('env4'); d.drop('env4', cont.key);
    ok(d.close().length === 0 && d.routes().some(r => r.src === 'env4'), 'closing dropped a mapped modulator');
  };
  /* ADR-322 item 3: a map handle dropped on a parameter goes through addRoute; a drop on a stepped or a
     reserved row is refused and leaves the modulator pending; a drop on a continuous row maps it. */
  T.mapdrop = () => {
    const { d } = fresh();
    d.activate('env2');
    ok(!d.drop('env2', stepS.key).ok && d.pending().includes('env2'), 'a drop on a stepped row was taken, or unpended the modulator');
    ok(!d.drop('env2', reserved.key).ok && d.pending().includes('env2'), 'a drop on a reserved row was taken');
    ok(d.routes().length === 0, 'a refused drop left a route');
    const r = d.drop('env2', cont.key);
    ok(r.ok && d.pending().length === 0 && d.routes().length === 1 && d.routes()[0].param === cont.key, 'a drop on a continuous row did not map it');
  };

  /* ADR-330: every value goes to the side that owns it. A hosted module's value reaches the rack and an Arp value
     the device's MIDI module, a morph setting and the position reach the resolver (IF-11, IF-12), and none of them
     reaches the engine's setParam (its refusal counter stays where it was, PA-7). */
  T.paths = () => {
    const { e, R, X, d } = fresh();
    const fx = all.find(r => r.target === 'rack' && r.law !== 'step' && r.class === 'M');
    const arp = all.find(r => r.target === 'midi' && r.law === 'step' && r.n >= 3 && r.options);
    const ms = all.find(r => r.target === 'resolver' && r.law !== 'step');
    ok(fx && arp && ms, 'a source is missing from the data: rack ' + !!fx + ', midi ' + !!arp + ', resolver ' + !!ms);
    const r0 = e.observe().refused;
    ok(d.set(fx.key, .7) === .7 && X.get(fx.key) === .7, 'a hosted module\u2019s value did not reach the rack (' + X.get(fx.key) + ')');
    const av = 1 / (arp.n - 1);
    ok(d.set(arp.key, av) === av && X.get(arp.key) === av, 'an Arp value did not reach the MIDI module (' + X.get(arp.key) + ')');
    ok(d.set(ms.key, .3) === .3 && R.get(ms.key) === .3, 'a morph setting did not reach the resolver (' + R.get(ms.key) + ')');
    d.setMorph({ on: true, x: .2, y: .7 });
    const po = R.observe().position;
    ok(po.x === .2 && po.y === .7 && R.get('morph.on') === 1, 'the position or morph.on did not reach the resolver');
    ok(e.observe().refused === r0 && e.getParam(fx.key) === null && e.getParam(ms.key) === null, 'a non-engine value reached the engine (refused ' + (e.observe().refused - r0) + ')');
    ok(X.observe().refused === 0 && R.observe().refused === 0, 'a side was sent a key it does not own');
    e.setParam(fx.key, .1);
    ok(e.observe().refused === r0 + 1, 'the engine took a hosted module\u2019s key (control: it must refuse it, PA-7)');
  };

  T.determinism = () => {
    const run = seed => {
      const { e, d } = fresh(seed);
      d.setMorph({ on: true, x: .31, y: .64 }); d.scatter(seed); d.set(stepS.key, 1, { corner: 2 });
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
  // The whole destination test goes: the engine's pv test alone already refuses a stepped engine row, so removing only
  // the step test would leave this plant blind.
  ['routes', 'device', "if (!r || r.law === 'step' || (r.target === 'resolver') || ((r.target || 'engine') === 'engine' && !r.pv))", 'if (!r)'],
  ['paths', 'device', "rack: 'rack'", "rack: 'set'"],
  ['paths', 'device', "midi: 'midi'", "midi: 'set'"],
  ['paths', 'device', "resolver: 'morph'", "resolver: 'set'"],
  ['laws', 'stub', "case 'log': return lo * Math.pow(hi / lo, v);", "case 'log': return lo * Math.pow(hi / lo, v * 1.001);"],
  ['seam', 'stub', "if (missing.length) throw new Error('the engine lacks", "if (false) throw new Error('the engine lacks"],
  ['state', 'stub', 'extra = keep;', 'extra = [];'],
  ['gestures', 'stub', "out.push({ type: 'gesture', key: key, phase: phase });", ''],
  ['morphoff', 'device', 'if (want !== null && want !== st.audible) return null; i = st.audible;', 'i = want !== null ? want : st.audible;'],
  ['morphhistory', 'device', "if (on !== st.morphOn) step(on ? 'Morph on' : 'Morph off', function () { st.morphOn = on; });", 'if (on !== st.morphOn) st.morphOn = on;'],
  ['snapshots', 'device', 'st.names[i] = name.trim(); st.saved[i] = copy(st.corners[i]);\n', 'st.names[i] = name.trim();\n'],
  ['sync', 'device', 'if (k === g) continue;', ''],
  ['pending', 'device', '      pending = [];\n      if (dropped.length) changed();', '      if (dropped.length) changed();'],
  ['mapdrop', 'device', "if (!r || r.law === 'step' || (r.target === 'resolver') || ((r.target || 'engine') === 'engine' && !r.pv))", 'if (!r)'],
  ['mapdrop', 'device', '      if (res.ok) { var k = pending.indexOf(src);', '      { var k = pending.indexOf(src);'],
  ['sync', 'device', 'if (sentSeq[k] !== undefined && s.ack < sentSeq[k]) continue;', ''],
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
