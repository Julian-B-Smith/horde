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
 *   meters       B589's console: the MOCK meters follow IF-6's gate (off, mute, solo) and the output's sum; over 0 dBFS
 *                latches the clip lamp, which holds until a clear
 *   summed       ADR-333: the Mix page's summed scope and spectrum read the observation view (MOCK): silence reads
 *                flat, a source moves them, +6 dB on its fader scales the scope by 10^(6/20)
 *   envelope     ADR-333: an oscillator view shows the envelope its `env` row selects, and edits the shared keys the
 *                Mod page carries (one model, two views)
 *   mapclick     the Map Mode spec (ADR-261), on the design system's REAL session and knob over the adapter: with MAP on a
 *                click on a knob makes a DEVICE route from the current source, 25 % unipolar, as one undo step; with MAP
 *                off none; a click on an already-mapped parameter only selects; undo takes it away and the session follows
 *   mapdrag      a drag released at exactly 0 makes no route and no step (even if another part of the shell emits mid-drag);
 *                a drag up makes one route at the dragged depth; a drag on an existing mapping edits its depth, one step
 *   mapkeys      (the SESSION's own keys since master batch 2, ADR-338 ruling 11; the shell hands it the scope as `mapKeysAlways`)
 *                M tap latches, M hold is momentary, Esc and M turn MAP off, 1 to 9 choose the source, P cycles the device
 *                route's polarity and Backspace removes it, one step each, WHETHER OR NOT keyboard mode is on (ADR-335 ruling 1),
 *                each key handled exactly once and keyboard mode's other keys (digits pin corners with MAP off) unchanged; the
 *                inspector's polarity pill and remove button (S.cyclePol, S.remove) do the same
 *   mapkeysfield a map key typed into a text input, text area, drop-down or editable region does nothing, in either mode, and
 *                the same key elsewhere does (a control); Cmd and Ctrl keys are the host's, never the map's
 *   mapkeysflag  the scope is one named constant (MAP_KEYS_ALWAYS), the page hands it to the session as it stands, and with it
 *                false the keys are keyboard mode's alone again (ADR-335: the human may reverse the ruling)
 *   escape       ADR-338 ruling 11, on the session's REAL layered Escape with the shell's layer in front: one layer a press, innermost
 *                first (the shell's menu, MAP, the pinned corner, keyboard mode), a press that cancelled nothing is left alone, a field's
 *                Escape is the field's; and the page puts its layer in front with S.escapeLayers.unshift, running no handler of its own
 *   removal      M-R2 (ADR-338 ruling 9): the adapter removes a modulator and ALL its routes as ONE undo step and undo puts every route
 *                back; a pending modulator is dropped outside the history, and a stale pending entry cannot bring a routed one back
 *   shellparts   the page builds its bank, removal line, Mod page and MAP source chip from the design master's components
 *                (H.ModBank, H.UndoLine, H.ModPage, MapControl chip), and removes through the adapter
 *   sections     ADR-335 ruling 4: every section of every page is described (a DRAFT with its source) or says it has no
 *                description yet and is counted; the box never shows another section's text or an invented one
 *   infofixed    ADR-335 ruling 4 (B582), structural: the info box is one fixed size, in the foot row left of the bank, and its
 *                text scrolls inside it; the browser's own self-check measures a hover moving nothing
 *   mapbar       ADR-335 ruling 3, structural: the MAP control does not shrink (the browser's self-check measures the names)
 *   mapcontrol   the mouse routes: the MAP button, the source dropdown and the strip's chips, which list every device source
 *   mappage      a page change keeps MAP and the source: one session, and the page switch never touches it (structural; the
 *                shell's own self-check clicks it); the control is mounted in the top bar, outside the page area
 *   maprefused   a knob no route can reach (a morph setting) makes no route, no ring, and the refusal is said (RC-4)
 *   settab       a click on every page-bar group's tab opens a page that exists (the Set tab's group is `set`, its page `settings`)
 *   envsummary   an oscillator section's readout names the envelope its source follows (its own `env` row), not the first block
 *   determinism  the same seed and calls give byte-identical device and engine state; another seed differs
 *
 * MUST-FAIL CONTROLS (plants), every run, on in-memory copies of the two scripts: an observe() that
 * counts, an undo that applies the wrong side, a scatter that draws from a counter, a route check
 * that admits stepped rows, a log law off by 0.1 %, a transport that skips the seam check, a load that
 * drops unknown keys, a gesture that is never reported, a morph-off write that reaches a frozen corner,
 * a morph toggle kept off the history, a snapshot save that keeps no preset, a snapshot that overwrites
 * a drag, a snapshot that overwrites a newer send, a close that keeps unmapped modulators, a drop that maps a
 * stepped row, a refused drop that clears the pending state, and the module, Arp and morph-setting values each
 * sent to the engine instead of their own side, a meter that ignores solo, a clear that does not unlatch, summed
 * views fed a constant, and an oscillator view that edits a copy of its envelope instead of the shared keys, a session list that never
 * reaches the device, a mapping written bipolar, a MAP gesture that is no history step, a push during the drag, a removal the device
 * never learns of, a MAP button that only switches on, a refusal that is not said or a refused ring that stays, a page switch that
 * turns MAP off, a one-page group left to the bundle (the Set tab's fault), an envelope summary read from the first block, the map keys
 * left to keyboard mode, a key taken twice, a key taken in a text field, a Cmd-key taken, a flag that is ignored or overridden by the
 * page, a section given a description it does not have or shown without its DRAFT mark, an info box that can grow or whose text does not
 * scroll, and a MAP control that can shrink, then (master batch 2) a session that cancels every Escape layer at once or behind the shell's,
 * a removal that leaves a route, is no history step or leaves a stale pending entry, and a page that lost its bank, removal line or
 * source chip. Each must
 * turn its own test red (and its anchor must exist, or the plant itself is reported as vacuous).
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
  logic: readFileSync(join(G, 'shell-logic.js'), 'utf8'),
  // the design system's vendored bundle (read only: plants mutate in-memory copies) and the shell page, for the map-mode tests
  bundle: readFileSync(join(root, 'docs/design/arp-face/bundle.js'), 'utf8'),
  shell: readFileSync(join(root, 'docs/design/gui3.html'), 'utf8'),
};

/* A DOM just big enough to run the design system's own Knob, Page nav and Map control: elements that keep their children,
   text, classes and listeners, and an event helper. It parses no HTML (the inspector's rows are drawn with innerHTML, so
   the inspector is exercised through the calls its buttons make). The bundle's keys are listeners on the window, so the
   test presses them by calling those listeners. */
class El {
  constructor(tag) { this.tagName = String(tag).toUpperCase(); this.children = []; this.attrs = {}; this.on = {}; this.style = { setProperty() {}, cssText: '' }; this.dataset = {}; this._cls = new Set(); this._text = ''; this._html = ''; this.hidden = false; this.title = ''; }
  get className() { return [...this._cls].join(' '); }
  set className(v) { this._cls = new Set(String(v).split(/\s+/).filter(Boolean)); }
  get classList() { const c = this._cls; return { add: (...a) => a.forEach(x => c.add(x)), remove: (...a) => a.forEach(x => c.delete(x)), toggle: (x, f) => { const on = f === undefined ? !c.has(x) : !!f; if (on) c.add(x); else c.delete(x); return on; }, contains: x => c.has(x) }; }
  get textContent() { return this._text + this.children.map(c => c.textContent).join(''); }
  set textContent(v) { this.children = []; this._text = String(v); }
  get innerHTML() { return this._html; }
  set innerHTML(v) { this.children = []; this._text = ''; this._html = String(v); }
  appendChild(c) { this.children.push(c); return c; }
  setAttribute(k, v) { this.attrs[k] = String(v); }
  getAttribute(k) { return k in this.attrs ? this.attrs[k] : null; }
  addEventListener(t, f) { (this.on[t] = this.on[t] || []).push(f); }
  getBoundingClientRect() { return { left: 0, top: 0, right: 200, bottom: 200, width: 200, height: 200 }; }
  setPointerCapture() {} releasePointerCapture() {} hasPointerCapture() { return false; } focus() {} removeEventListener() {}
  *walk() { for (const c of this.children) { yield c; yield* c.walk(); } }
  all(pred) { return [...this.walk()].filter(pred); }
  querySelector(sel) { const cls = sel.match(/\.([\w-]+)/g) || []; return this.all(x => cls.length && cls.every(c => x._cls.has(c.slice(1))))[0] || new El('i'); }
  fire(t, props) { const e = { type: t, target: this, button: 0, pointerId: 1, preventDefault() {}, stopPropagation() {}, ...props }; (this.on[t] || []).forEach(f => f(e)); }
}
function fakeDom() {
  const doc = { createElement: t => new El(t), createElementNS: (_, t) => new El(t), createTextNode: t => Object.assign(new El('#text'), { _text: t }), body: new El('body'), documentElement: new El('html'), activeElement: null };
  // `caps` are capture-phase listeners (a browser runs them before the bubble-phase ones in `keys`)
  const keys = { keydown: [], keyup: [], blur: [] }, caps = { keydown: [], keyup: [] };
  return { document: doc, keys, caps, addEventListener: (t, f, cap) => { const m = cap ? caps : keys; (m[t] = m[t] || []).push(f); } };
}

// The scripts are classic scripts that assign to `window`; give them a fresh one each load. The bundle runs on the fake DOM.
function load(src) {
  const win = {}, dom = fakeDom();
  for (const k of ['data', 'stub', 'device', 'logic']) new Function('window', src[k])(win);
  new Function('window', 'document', 'addEventListener', 'requestAnimationFrame', src.bundle)(win, dom.document, dom.addEventListener, () => 0);
  win.__dom = dom; win.__shellText = src.shell; win.__logicText = src.logic;
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

  /* B589's console reads the engine's meters and clip latches (IF-6), MOCK in the stub: a source's meter follows its
     level through IF-6's gate (off, mute, solo), the output sums the sounding sources, a reading over 0 dBFS latches
     the clip lamp, and only a clear (a message, the latch lives on the audio side) unlatches it. */
  T.meters = () => {
    const { e, d } = fresh();
    const lv = k => by[k], v0 = k => (0 - lv(k).lo) / (lv(k).hi - lv(k).lo);   // the v of 0 dB on a dB row
    d.set('osc1.on', 1); d.set('osc2.on', 1);
    d.set('mix.osc1', v0('mix.osc1')); d.set('mix.osc2', v0('mix.osc2'));
    let m = d.meters();
    ok(Math.abs(m.osc1.peak) < 1e-9 && m.sub.peak === -Infinity, 'osc 1 at 0 dB does not read 0 dBFS, or the sub (off) is not silent');
    ok(Math.abs(m.out.peak - 20 * Math.log10(2)) < 1e-9, 'the output is not the sum of the sounding sources (' + m.out.peak + ')');
    d.set('osc2.solo', 1); m = d.meters();
    ok(m.osc1.peak === -Infinity && m.osc2.peak > -Infinity, 'a solo did not silence the other sources (IF-6)');
    d.set('osc2.solo', 0); d.set('osc1.mute', 1); m = d.meters();
    ok(m.osc1.peak === -Infinity, 'a muted source still reads');
    d.set('osc1.mute', 0); d.set('mix.osc1', 1); m = d.meters();
    ok(m.osc1.clip.on && m.osc1.peak > 0, 'a reading over 0 dBFS did not latch the clip lamp');
    d.set('mix.osc1', v0('mix.osc1')); m = d.meters();
    ok(m.osc1.clip.on, 'the clip latch let go by itself (it holds until cleared)');
    d.clearClip('osc1'); m = d.meters();
    ok(!m.osc1.clip.on, 'clear did not unlatch the clip lamp');
    ok(JSON.stringify(e.observe().meters) === JSON.stringify(e.observe().meters), 'reading the meters changed them');
  };

  /* ADR-333: the Mix page's summed oscilloscope and spectrum read the engine's observation view (IF-10; MOCK in the
     stub). Silence reads flat, a sounding source moves them, and a louder fader scales the scope. */
  T.summed = () => {
    const { d } = fresh();
    const v0 = k => (0 - by[k].lo) / (by[k].hi - by[k].lo), peak = a => a.reduce((m, x) => Math.max(m, Math.abs(x)), 0);
    d.set('osc1.on', 0);
    let sm = d.summed();
    ok(sm && sm.scope.length > 0 && peak(sm.scope) === 0 && sm.bands.every(b => b === 0), 'with every source off the summed views do not read silence');
    d.set('osc1.on', 1); d.set('mix.osc1', v0('mix.osc1'));
    const p1 = peak(d.summed().scope), b1 = d.summed().bands.slice();
    d.set('mix.osc1', (6 - by['mix.osc1'].lo) / (by['mix.osc1'].hi - by['mix.osc1'].lo));
    const p2 = peak(d.summed().scope);
    ok(p1 > 0 && Math.abs(p2 / p1 - Math.pow(10, 6 / 20)) < 1e-9, 'the summed scope does not follow the source and its fader (' + p1 + ', ' + p2 + ')');
    ok(JSON.stringify(d.summed().bands) !== JSON.stringify(b1), 'the summed spectrum did not move with the fader');
  };
  /* ADR-333: an oscillator view's envelope is the one its `env` row selects (RC-6.5), and its rows are the shared keys:
     an edit through the oscillator view's key is what the Mod page's key reads, and the other way round. */
  T.envelope = () => {
    const { d } = fresh();
    const osc = DATA.pages.find(p => p.id === 'osc1'), mod = DATA.pages.find(p => p.id === 'mod');
    const f = osc.groups.find(g => g.follow).follow, modKeys = new Set(mod.groups.flatMap(g => g.controls.map(c => c.key)));
    const opt = by[f.selector].options[2];
    d.set(f.selector, 2 / (by[f.selector].n - 1));
    const fb = d.followed(f);
    ok(fb && fb === f.options[opt], 'the followed envelope is not the one the selector holds (' + (fb && fb.block) + ')');
    const ka = fb.viz.a;
    ok(modKeys.has(ka), 'the oscillator view edits ' + ka + ', which the Mod page does not carry');
    d.set(ka, .61);
    ok(d.heard(ka) === .61, 'an edit on the oscillator view did not reach the shared key');
    d.set(ka, .27);
    const again = d.followed(f);
    ok(d.heard(again.viz.a) === .27, 'an edit on the Mod page did not read back on the oscillator view');
  };

  /* MAP MODE (the Map Mode spec, ADR-261; B584 slice 6). The rig is the design system's REAL session and knob (on the fake
     DOM) over the real adapter, joined by shell-logic.js as the shell joins them: every claim below is about what reaches the
     DEVICE's routes and its history, never about the session's own list. Keys are the session's own (keyboard mode, ADR-259
     ruling 6); the inspector's polarity pill and remove button call S.cyclePol and S.remove, so those calls are the mouse
     route of P and Backspace. */
  const SPEC_DEPTH = .25;
  const mapRig = (o = {}) => {
    const f = fresh(), H = W.Horde, dom = W.__dom, d = f.d;
    // as the page builds it: the scope is the shell's constant, handed to the session as `mapKeysAlways` (unless a test sets it); the keys
    // and Escape are the session's own listeners, started below
    const S = new H.Session({ x: .5, y: .5, corners: [null, null, null, null], mapKeysAlways: o.always === undefined ? W.GUI3Shell.MAP_KEYS_ALWAYS : o.always,
      sources: d.sources().map(s => ({ id: s.id, name: s.name, kind: s.kind, f: () => 0 })), mappings: [] });
    const br = W.GUI3Shell.attachMap(S, d);
    const n0 = { keydown: dom.keys.keydown.length, keyup: dom.keys.keyup.length }, c0 = { keydown: dom.caps.keydown.length, keyup: dom.caps.keyup.length };
    S.start();
    const knob = key => {
      const row = by[key], host = new El('div');
      H.Knob(host, { key, name: row.label, path: row.label, kind: 'c', fmt: v => v.toFixed(2), def: row.default_v, vals: [0, 1, 2, 3].map(i => d.corner(key, i)), size: 'compact' }, S);
      return host.all(x => x.on.pointerdown)[0];
    };
    const at = (y, alt) => ({ clientX: 10, clientY: y, altKey: !!alt });
    return { S, d, br, H, knob, steps: () => d.history().past.length,
      click: face => { face.fire('pointerdown', at(100)); face.fire('pointerup', at(100)); },
      // a vertical drag by dy px (up is positive); `mid` runs while the pointer is still down (any other part of the shell may emit)
      drag: (face, dy, mid, alt) => { face.fire('pointerdown', at(100, alt)); face.fire('pointermove', at(100 - dy, alt)); if (mid) mid(); face.fire('pointerup', at(100 - dy, alt)); },
      // an event goes through the capture listeners, then the bubble ones, and stops where a listener says so
      press: (o, t = 0) => {
        const e = { repeat: false, altKey: false, metaKey: false, ctrlKey: false, timeStamp: t, target: { tagName: 'DIV' }, prevented: false, stopped: false,
          preventDefault() { this.prevented = true; }, stopImmediatePropagation() { this.stopped = true; }, ...o };
        for (const fn of [...dom.caps.keydown.slice(c0.keydown), ...dom.keys.keydown.slice(n0.keydown)]) { if (e.stopped) break; fn(e); }
        return e;
      },
      release: (o, t = 0) => {
        const e = { timeStamp: t, stopped: false, stopImmediatePropagation() { this.stopped = true; }, ...o };
        for (const fn of [...dom.caps.keyup.slice(c0.keyup), ...dom.keys.keyup.slice(n0.keyup)]) { if (e.stopped) break; fn(e); }
        return e;
      } };
  };
  const cont2 = firstOf(r => r.law !== 'step' && r.pv && r.key !== cont.key);
  // the morph's settings are the resolver's inputs (IF-12), never a modulation destination: a continuous one no route can reach
  const noPv = all.find(r => r.target === 'resolver' && r.law !== 'step');

  T.mapclick = () => {
    const { S, d, br, knob, click, steps } = mapRig(), face = knob(cont.key);
    S.setSrc('env3');
    click(face);
    ok(d.routes().length === 0 && steps() === 0, 'MAP off: a click on a knob made a route (it is a base-value gesture)');
    S.setMap(true); click(face);
    const r = d.routes();
    ok(r.length === 1 && r[0].src === 'env3' && r[0].param === cont.key && r[0].pol === 'uni' && near(r[0].depth, SPEC_DEPTH), 'MAP on: a click did not make a DEVICE route from the current source at 25 % unipolar: ' + JSON.stringify(r));
    ok(steps() === 1, 'a route made by MAP is not one undo step (' + steps() + ')');
    ok(S.mappings.length === 1 && S.mappings[0].id === r[0].id, 'the session does not hold the device’s route (its ring would not draw)');
    click(face);
    ok(d.routes().length === 1 && near(d.routes()[0].depth, SPEC_DEPTH) && steps() === 1, 'a click on a parameter already mapped from the current source changed it (the spec: it only selects)');
    S.setSrc('env4'); click(knob(cont2.key));
    ok(d.routes().length === 2 && d.routes()[1].src === 'env4' && d.routes()[1].param === cont2.key && steps() === 2, 'a second source on a second knob is not a second route and a second step');
    ok(d.undo() && d.routes().length === 1, 'undo did not take the last MAP route away');
    br.frame();
    ok(S.mappings.length === 1 && S.mappings[0].src === 'env3', 'the session did not follow the device after the undo');
    ok(S.map.on && S.map.src === 'env4', 'MAP or its source changed with a device change (a page change must keep both)');
  };

  /* MD1 (ADR-340 r7, the human's ruling amending the Map Mode spec and ADR-261 r3): with MAP on a PLAIN drag moves the value
     and makes no route; Option-drag sets the current source's depth (the spec's old plain-drag rule, now on Option). */
  T.mapdrag = () => {
    const { S, d, knob, drag, steps } = mapRig(), face = knob(cont.key);
    S.setMap(true); S.setSrc('env3');
    const p = S.params[cont.key], ai = S.editing(p), v0 = p.vals[ai];
    drag(face, 40);
    ok(d.routes().length === 0 && S.mappings.length === 0 && Math.abs(p.vals[ai] - v0) > .1, 'MD1: a plain drag under MAP made a route or did not move the value (' + d.routes().length + ' routes, ' + v0 + ' → ' + p.vals[ai] + ')');
    drag(face, -40, () => S.say('another part of the shell speaks mid-drag'), true);   // down, so the depth stays 0: released at exactly 0
    ok(d.routes().length === 0 && steps() === 0 && S.mappings.length === 0, 'an Option-drag released at 0 left a route or a step (' + d.routes().length + ' routes, ' + steps() + ' steps)');
    drag(face, 80, () => S.say('mid-drag'), true);                                    // one drag length is 160 px: depth .5
    ok(d.routes().length === 1 && near(d.routes()[0].depth, .5) && steps() === 1, 'an Option-drag up did not make one route at the dragged depth: ' + JSON.stringify(d.routes()));
    drag(face, -40, null, true);                                                       // the same mapping, Option-dragged down: .25
    ok(d.routes().length === 1 && near(d.routes()[0].depth, .25) && steps() === 2, 'a drag on an existing mapping did not edit its depth as one step: ' + JSON.stringify(d.routes()) + ', ' + steps() + ' steps');
    ok(d.undo() && near(d.routes()[0].depth, .5), 'undo did not put the depth back');
  };

  /* The whole key set, in one keyboard-mode setting. ADR-335 ruling 1: the same in both. Times are well past 280 ms from zero,
     so a tap the session's own keyup would misread (it reads a press time only its own keydown sets) is caught. */
  const keySet = keysOn => {
    const { S, d, knob, click, press, release, steps } = mapRig(), face = knob(cont.key), who = (keysOn ? 'keyboard mode ON: ' : 'keyboard mode OFF: ');
    S.keys = keysOn;
    press({ code: 'KeyM' }, 1000); release({ code: 'KeyM' }, 1100);
    ok(S.map.on && !S.map.momentary, who + 'M tapped (within 280 ms) did not latch MAP on');
    press({ code: 'KeyM' }, 1500); release({ code: 'KeyM' }, 1600);
    ok(!S.map.on, who + 'M while MAP is latched did not turn it off');
    press({ code: 'KeyM' }, 2000);
    ok(S.map.on && S.map.momentary, who + 'M held did not turn MAP on for the hold');
    release({ code: 'KeyM' }, 2400);
    ok(!S.map.on, who + 'releasing a held M did not end MAP');
    S.setMap(true); press({ key: 'Escape' });
    ok(!S.map.on, who + 'Esc did not turn MAP off');
    S.setMap(true);
    let emits = 0; S.on(() => { emits++; });
    const e3 = press({ code: 'Digit3' });
    ok(S.map.src === 'env3', who + 'the number 3 did not choose the third source (' + S.map.src + ')');
    ok(emits === 1 && e3.prevented, who + 'the number 3 was handled ' + emits + ' times (or its default was not stopped), not once');
    press({ code: 'Digit9' });
    ok(S.map.src === d.sources()[8].id, who + 'the number 9 did not choose the ninth source');
    press({ code: 'Digit3' }); click(face);
    ok(d.routes().length === 1 && steps() === 1, who + 'no route to cycle');
    press({ code: 'KeyP' });
    ok(d.routes()[0].pol === 'bi' && steps() === 2, who + 'P did not make the device route bipolar as one step (' + d.routes()[0].pol + ', ' + steps() + ')');
    S.cyclePol(S.lastMap);                                                              // the inspector's polarity pill
    ok(d.routes()[0].pol === 'inv' && steps() === 3, who + 'the polarity pill did not make the device route inverse as one step');
    press({ key: 'Backspace' });
    ok(d.routes().length === 0 && steps() === 4, who + 'Backspace did not remove the device route as one step');
    click(face); const id = d.routes()[0].id;
    S.remove(S.lastMap);                                                                // the inspector's remove button
    ok(!d.routes().some(r => r.id === id) && steps() === 6, who + 'the remove button did not remove the device route as one step');
    S.setMap(false); click(face); press({ key: 'Backspace' });
    ok(d.routes().length === 0, who + 'MAP off: a click made a route');
    // with MAP off the digits are not the map's: they stay free, and in keyboard mode they pin morph corners as they always did
    const e2 = press({ code: 'Digit2' });
    ok(keysOn ? S.pinned === 1 : (S.pinned === null && !e2.prevented), who + 'a digit with MAP off ' + (keysOn ? 'no longer pins a corner (keyboard mode changed)' : 'was taken by the map (pinned ' + S.pinned + ')'));
    S.setPin(null);
  };
  T.mapkeys = () => { keySet(false); keySet(true); };

  /* A key typed into a field is the field's. In both modes, for each kind of field, and the same key elsewhere works (so the
     test is not blind); Cmd and Ctrl keys belong to the host (Cmd-M minimises, Cmd-P prints). */
  T.mapkeysfield = () => {
    const fields = [{ tagName: 'INPUT' }, { tagName: 'TEXTAREA' }, { tagName: 'SELECT' }, { tagName: 'DIV', isContentEditable: true }];
    for (const keysOn of [false, true]) {
      for (const target of fields) {
        const { S, d, knob, click, press, steps } = mapRig(), face = knob(cont.key), who = (keysOn ? 'keyboard mode ON, ' : 'keyboard mode OFF, ') + (target.isContentEditable ? 'an editable region' : target.tagName) + ': ';
        S.keys = keysOn; S.setMap(true); S.setSrc('env2'); click(face);
        const n = steps();
        ok(d.routes().length === 1, who + 'no route to test the keys with');
        press({ code: 'KeyP', target });
        press({ key: 'Backspace', target });
        ok(d.routes().length === 1 && d.routes()[0].pol === 'uni' && steps() === n, who + 'P or Backspace in a field changed the route (' + JSON.stringify(d.routes()) + ')');
        press({ code: 'Digit4', target });
        ok(S.map.src === 'env2', who + 'a digit in a field chose a source (' + S.map.src + ')');
        press({ code: 'KeyM', target }, 1000);
        press({ key: 'Escape', target });
        ok(S.map.on && !S.map.momentary, who + 'M or Esc in a field changed MAP');
        press({ code: 'KeyP' });
        ok(d.routes()[0].pol === 'bi', who + 'control: the same P outside a field did not work');
      }
    }
    for (const mod of [{ metaKey: true }, { ctrlKey: true }]) {
      const { S, press } = mapRig();
      press({ code: 'KeyM', ...mod }, 1000); press({ code: 'KeyP', ...mod });
      ok(!S.map.on, 'a Cmd or Ctrl key was taken by the map (' + Object.keys(mod)[0] + ')');
    }
  };

  /* The scope is one named constant the human can flip (ADR-335: "I may ultimately reverse the keys always decision"). */
  T.mapkeysflag = () => {
    const L = W.__logicText, P = W.__shellText;
    ok(/ADR-335[\s\S]{0,1400}var MAP_KEYS_ALWAYS = (true|false);/.test(L), 'shell-logic.js has no single named MAP_KEYS_ALWAYS constant with its ADR-335 comment');
    ok((L.match(/var MAP_KEYS_ALWAYS/g) || []).length === 1, 'the scope is not held in exactly one constant');
    ok(/mapKeysAlways: SH\.MAP_KEYS_ALWAYS\b/.test(P), 'the page does not hand the constant to the session as it stands (an override in the page would hide the one-line change)');
    const off = mapRig({ always: false });
    off.press({ code: 'KeyM' }, 1000); off.release({ code: 'KeyM' }, 1100);
    ok(!off.S.map.on, 'with the scope reversed, a key still worked with keyboard mode off');
    off.S.keys = true;
    off.press({ code: 'KeyM' }, 3000); off.release({ code: 'KeyM' }, 3100);
    ok(off.S.map.on && !off.S.map.momentary, 'with the scope reversed, keyboard mode lost the keys');
    ok(W.GUI3Shell.MAP_KEYS_ALWAYS === true, 'the ruling in force is keys always (ADR-335)');
  };

  /* ADR-335 ruling 4: the section descriptions. Every section is described (with its DRAFT mark and source) or says so. */
  T.sections = () => {
    const SEC = DATA.sections, all = DATA.pages.flatMap(p => p.groups.map(g => ({ page: p, group: g }))), find = id => all.find(x => x.group.id === id);
    const SH = W.GUI3Shell;
    ok(SEC && /^DRAFT/.test(SEC.status), 'the sections file is not marked a draft');
    ok(new Set(all.map(x => x.group.id)).size === all.length, 'two sections share an id');
    ok(Object.keys(SEC.text).length + SEC.missing.length === all.length && SEC.missing.every(id => find(id) && !SEC.text[id]), 'a section is neither described nor counted as missing');
    ok(Object.keys(SEC.text).length > 0 && SEC.missing.length > 0, 'the test has nothing to tell a described section from an undescribed one with');
    for (const { group } of all) {
      const i = SH.sectionInfo(SEC, group.id, find), d = SEC.text[group.id];
      ok(i && i.described === !!d, group.id + ': described or not does not match the data');
      if (d) {
        ok(i.text === d.text && /DRAFT/.test(i.tag) && d.src.every(c => i.meta.includes(c.path)), group.id + ': the box does not show its own text, its DRAFT mark and its source (' + i.tag + ')');
        ok(d.src.length > 0, group.id + ': a description with no source');
      } else {
        ok(i.text === 'No section description yet.' && !/DRAFT/.test(i.tag), group.id + ': an undescribed section shows “' + i.text + '”, not that it has none');
      }
    }
    ok(SH.sectionInfo(SEC, 'no/such-section', find) === null, 'an unknown section id gave an info box');
    const a = SH.sectionInfo(SEC, Object.keys(SEC.text)[0], find), b = SH.sectionInfo(SEC, SEC.missing[0], find);
    ok(a.text !== b.text && b.text === 'No section description yet.', 'a section with no description shows another section’s text');
  };

  /* ADR-335 ruling 4 (B582), structural: one size, in the foot row, left of the bank, its text scrolling inside it. The
     browser's self-check (the shell's own) measures that a hover moves and resizes nothing, with a growing box as its control. */
  T.infofixed = () => {
    const t = W.__shellText, css = n => (new RegExp('\\n  ' + n.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ' \\{([^}]*)\\}').exec(t) || [, ''])[1];
    ok(/--foot-h: \d+px; --info-w: \d+px/.test(css('.footrow')) && /height: var\(--foot-h\)/.test(css('.footrow')), 'the foot row has no fixed height');
    ok(/width: var\(--info-w\); flex: 0 0 var\(--info-w\); height: 100%/.test(css('.info')), 'the info box is not one fixed size');
    ok(/overflow-y: auto/.test(css('.info .face')) && /min-height: 0/.test(css('.info .face')), 'the info box’s text does not scroll inside it');
    const row = /<div class="footrow" id="footrow">([\s\S]*?)\n        <\/div>/.exec(t);
    ok(row && row[1].indexOf('id="info"') >= 0 && row[1].indexOf('id="info"') < row[1].indexOf('id="bank"'), 'the info box is not in the foot row, left of the bank');
    ok(t.indexOf('id="footrow"') > t.indexOf('<main id="pages">'), 'the foot row is not below the page area');
  };

  /* ADR-335 ruling 3, structural: the control is as wide as its parts; the strip beside it scrolls instead. */
  T.mapbar = () => {
    const t = W.__shellText;
    ok(/\.mapbar #mapHost \{ flex: 0 0 auto; \}/.test(t), 'the MAP control can shrink: its source drop-down is cut off (ADR-335 ruling 3)');
    ok(/\.mapbar #srcStrip \{[^}]*min-width: 0;[^}]*overflow-x: auto;/.test(t), 'the source strip does not scroll in the room that is left');
  };

  /* The mouse routes of the keys: the MAP button (M, Esc), the dropdown and the source strip's chips (1 to 9). */
  T.mapcontrol = () => {
    const { S, d, H } = mapRig(), mc = new El('div'), strip = new El('div');
    H.MapControl(mc, S, { strip, frame: new El('div') });
    const btn = mc.all(x => x.tagName === 'BUTTON')[0], sel = mc.all(x => x.tagName === 'SELECT')[0];
    ok(btn && sel, 'the control has no button or no source dropdown');
    btn.fire('click'); ok(S.map.on, 'the MAP button did not turn MAP on');
    sel.value = 'env4'; sel.fire('change'); ok(S.map.src === 'env4', 'the dropdown did not choose the source');
    const chips = strip.all(x => x.dataset && x.dataset.src);
    ok(chips.length === d.sources().length, 'the source strip does not list every source the device publishes (' + chips.length + ')');
    chips.find(c => c.dataset.src === 'env2').fire('click'); ok(S.map.src === 'env2', 'a chip did not choose its source');
    btn.fire('click'); ok(!S.map.on && S.map.src === 'env2', 'the MAP button did not turn MAP off, or the source changed with it');
  };

  /* A page change keeps MAP and the source: there is ONE session, and the page switch never touches it. A structural
     test, because the page switch is the shell's own code (gui3.html); the in-page self-check clicks it for real. */
  T.mappage = () => {
    const t = W.__shellText, sp = /function selectPage\(id\) \{[\s\S]*?\n\}/.exec(t);
    ok(sp, 'selectPage is not in the shell');
    ok(!/S\.map\b|setMap|setSrc/.test(sp[0]), 'the page switch touches MAP state: ' + (/S\.map\b|setMap|setSrc/.exec(sp[0]) || [''])[0]);
    ok((t.match(/new H\.Session\(/g) || []).length === 1, 'the shell has more than one session: a page could have its own MAP state');
    ok(/<main id="pages"><\/main>/.test(t) && t.indexOf('id="mapHost"') > 0 && t.indexOf('id="mapHost"') < t.indexOf('<main id="pages">') && /H\.MapControl\(\$\('mapHost'\)/.test(t), 'the MAP control is not mounted in the top bar, outside the page area');
  };

  /* A knob whose destination no route can reach (RC-4: here a morph setting, the resolver's input) makes no mapping:
     the matrix refuses, the session does not keep a ring the device does not hold, and it says why. */
  T.maprefused = () => {
    ok(noPv, 'no continuous morph setting to test a refused destination with');
    const { S, d, knob, click, steps } = mapRig(), face = knob(noPv.key);
    S.setMap(true); S.setSrc('env2'); click(face);
    ok(d.routes().length === 0 && steps() === 0, 'a refused destination made a route or a step');
    ok(S.mappings.length === 0, 'the session kept a mapping the device refused (a ring with no route behind it)');
    ok(/RC-4/.test(S.message), 'the refusal was not said (' + S.message + ')');
  };

  /* The Set tab: its group is `set` and its page `settings`; a click on any group's tab must open a page that exists. */
  T.settab = () => {
    const PG = Object.fromEntries(DATA.pages.map(p => [p.id, p])), opened = [];
    const host = new El('div'), groups = W.GUI3Shell.navGroups(DATA.nav, PG, () => false);
    W.Horde.PageNav(host, { label: 'Page groups', panel: 'pages', keys: () => false, groups, onChange: (g, pid) => opened.push([g, pid]) });
    const tabs = host.all(x => x.className.split(' ').includes('pn-g'));
    ok(tabs.length === DATA.nav.length, 'the page bar does not draw every group (' + tabs.length + ')');
    for (const g of DATA.nav.slice(1).concat(DATA.nav.slice(0, 1))) {
      opened.length = 0;
      tabs.find(t => t.textContent === g.label).fire('click');
      ok(opened.length === 1 && g.pages.includes(opened[0][1]) && PG[opened[0][1]], 'the ' + g.label + ' tab opens ' + JSON.stringify(opened) + ', not one of its pages ' + g.pages.join(', '));
    }
  };

  /* An oscillator section's readout names the envelope its source FOLLOWS (RC-6.5): for each source its own `env` row's pick. */
  T.envsummary = () => {
    const { d } = fresh(), S = W.GUI3Shell, shown = (f, blk) => ['a', 'd'].map(k => W.GUI3Engine.display(by[f.options[blk].viz[k]], d.shown(f.options[blk].viz[k]))).join(' · ');
    const follows = DATA.pages.filter(p => ['osc1', 'osc2', 'sub', 'noise'].includes(p.id)).map(p => ({ page: p.id, f: p.groups.find(g => g.follow).follow }));
    ok(follows.length === 4, 'the four oscillator pages do not each follow an envelope');
    follows.forEach(({ page, f }, i) => {
      const blk = ['ENV 1', 'ENV 2', 'ENV 3', 'ENV 4'][(i + 1) % 4], other = blk === 'ENV 1' ? 'ENV 2' : 'ENV 1';
      d.set(f.selector, (['ENV 1', 'ENV 2', 'ENV 3', 'ENV 4'].indexOf(blk)) / (by[f.selector].n - 1));
      d.set(f.options[blk].viz.a, .3 + i / 10); d.set(f.options[other].viz.a, .9);
      const t0 = S.followedSummary(d, W.GUI3Engine, by, f).join(' · ');
      ok(t0 === shown(f, blk), page + ': the summary reads “' + t0 + '”, not ' + blk + '’s “' + shown(f, blk) + '”');
      d.set(f.options[other].viz.a, .1);
      ok(S.followedSummary(d, W.GUI3Engine, by, f).join(' · ') === t0, page + ': an envelope the source does not follow moved its summary');
      d.set(f.options[blk].viz.a, .75);
      ok(S.followedSummary(d, W.GUI3Engine, by, f).join(' · ') !== t0, page + ': the followed envelope moved and its summary did not');
    });
  };

  /* ADR-338 ruling 11: Escape is the session's layered cancel, the shell's own layers in front of the session's (map, pin, keyboard mode).
     The rig puts a stand-in menu layer in front exactly as the page does, and presses real Escape events through the session's listener. */
  T.escape = () => {
    const P = W.__shellText;
    ok(/S\.escapeLayers\.unshift\(\{ name: 'menu'/.test(P), 'the page does not put its menu layer IN FRONT of the session’s with S.escapeLayers.unshift');
    ok(!/attachEscape|attachMapKeys/.test(P), 'the page still runs a handler of its own for Escape or the map keys');
    for (const keysOn of [false, true]) {
      const { S, press } = mapRig(), who = keysOn ? 'keyboard mode ON: ' : 'keyboard mode OFF: ';
      let menu = true; S.escapeLayers.unshift({ name: 'menu', active: () => menu, cancel: () => { menu = false; } });
      S.keys = keysOn; S.setMap(true); S.setPin(2);
      const e1 = press({ key: 'Escape' });
      ok(!menu && S.map.on && S.pinned === 2 && e1.prevented, who + 'the first Escape did not close the menu ALONE (menu ' + menu + ', MAP ' + S.map.on + ', pin ' + S.pinned + ')');
      const field = press({ key: 'Escape', target: { tagName: 'SELECT' } });
      ok(S.map.on && !field.prevented, who + 'an Escape typed in a drop-down cancelled a layer (it is the field’s)');
      const e2 = press({ key: 'Escape' });
      ok(!S.map.on && S.pinned === 2 && e2.prevented, who + 'the second Escape did not turn MAP off alone');
      const e3 = press({ key: 'Escape' });
      ok(S.pinned === null && S.keys === keysOn && e3.prevented, who + 'the third Escape did not unpin the corner alone');
      if (keysOn) { const e4 = press({ key: 'Escape' }); ok(S.keys === false && e4.prevented, who + 'the fourth Escape did not turn keyboard mode off'); }
      const last = press({ key: 'Escape' });
      ok(!last.prevented && !S.map.on && S.pinned === null && !S.keys, who + 'an Escape with nothing open was taken (it must cancel nothing)');
    }
  };

  /* M-R2 (ADR-338 ruling 9): the adapter's removal. ONE undo step for the modulator and every route it drives; undo puts them all back. */
  T.removal = () => {
    const { d } = fresh(), k1 = cont.key, k2 = cont2.key, k3 = firstOf(r => r.law !== 'step' && r.pv && r.key !== k1 && r.key !== k2).key;
    d.begin('setup'); d.addRoute({ src: 'env3', param: k1 }); d.addRoute({ src: 'env3', param: k2 }); d.addRoute({ src: 'env4', param: k3 }); d.end();
    const before = JSON.stringify(d.routes()), h0 = d.history().past.length, mine = id => d.routes().filter(r => r.src === id).length;
    ok(mine('env3') === 2 && mine('env4') === 1, 'the setup did not make the routes it removes (a control: nothing to remove proves nothing)');
    const r = d.removeModulator('env3');
    ok(r.ok && r.step && r.routes.length === 2, 'the removal did not report the 2 routes it took: ' + JSON.stringify(r));
    ok(d.history().past.length === h0 + 1, 'the removal was not ONE history step (' + (d.history().past.length - h0) + ')');
    ok(mine('env3') === 0 && mine('env4') === 1, 'the removal left a route of the modulator, or took another modulator’s');
    ok(d.undo() && JSON.stringify(d.routes()) === before, 'undo did not put every route back, as it was');
    ok(!d.removeModulator('env2').ok && d.history().past.length === h0, 'a modulator with no routes that is not pending was “removed” (or made a step)');
    d.activate('env2'); const hp = d.history().past.length, p = d.removeModulator('env2');
    ok(p.ok && !p.step && d.history().past.length === hp && !d.pending().includes('env2'), 'a pending modulator was not dropped outside the history');
    // the trap: a pending modulator mapped by MAP (syncRoutes, not the bank's drop) stays in the pending list; once its routes go it must not reappear
    d.activate('env1'); d.syncRoutes(d.routes().concat([{ src: 'env1', param: k1 }]));
    ok(mine('env1') === 1, 'the setup did not map the pending modulator');
    d.removeModulator('env1');
    ok(mine('env1') === 0 && !d.pending().includes('env1'), 'a modulator removed after being mapped came back as pending');
  };

  /* The page builds the bank, the removal line, the Mod page and the MAP source chip from the design master's components and removes through
     the adapter: structural (the browser's self-checks drive them for real). */
  T.shellparts = () => {
    const P = W.__shellText;
    ok(/H\.ModBank\(\$\('bankHost'\), S, \{/.test(P), 'the page does not mount the design master’s ModBank');
    ok(/H\.UndoLine\(\$\('undoLine'\)/.test(P), 'the page has no removal line (UndoLine)');
    ok(/H\.ModPage\(host, S, \{/.test(P), 'the page does not mount the design master’s ModPage');
    ok(/H\.MapControl\(\$\('mapHost'\), S, \{[^}]*chip: true/.test(P), 'the MAP control does not take its source as the chip (MS3)');
    ok(/dev\.removeModulator\(m\.id\)/.test(P) && /H\.ModRemoval\.describe\(/.test(P), 'the removal does not go through the adapter and the master’s removal text');
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
  ['meters', 'stub', "var silent = !isOn(s + '.on') || isOn(s + '.mute') || (anySolo && !isOn(s + '.solo'));", "var silent = !isOn(s + '.on') || isOn(s + '.mute');"],
  ['meters', 'stub', "latch[id] = { on: false, max: -Infinity }; return was;", 'return was;'],
  ['summed', 'device', "D.summed = function () { return lastSnap && lastSnap.observe && lastSnap.observe.sum || null; };",
    "D.summed = function () { return { scope: [0.5, -0.5], bands: [0.5], mock: true }; };"],
  ['envelope', 'device', "return f.options[r.options[Math.round(heard(f.selector) * (r.n - 1))]] || null;",
    "var o = f.options[r.options[0]]; var c = {}; for (var k in o.viz) c[k] = f.selector + '.copy.' + k; return { block: f.selector + '.copy', viz: c };"],
  ['pending', 'device', '      pending = [];\n      if (dropped.length) changed();', '      if (dropped.length) changed();'],
  ['mapdrop', 'device', "if (!r || r.law === 'step' || (r.target === 'resolver') || ((r.target || 'engine') === 'engine' && !r.pv))", 'if (!r)'],
  ['mapdrop', 'device', '      if (res.ok) { var k = pending.indexOf(src);', '      { var k = pending.indexOf(src);'],
  ['sync', 'device', 'if (sentSeq[k] !== undefined && s.ack < sentSeq[k]) continue;', ''],
  // slice 6 (map mode, the Set tab, the envelope summary). The session's list never reaches the device; a mapping written
  // with the wrong polarity; a gesture that is no history step; a push during the drag; a removal the device never learns
  // of; a MAP button that only switches on; a refused mapping the session keeps; a page switch that turns MAP off; a
  // one-page group left to the bundle (the Set tab's fault); a summary read from the first block.
  ['mapclick', 'logic', 'if (sig(dev.routes()) !== last) pull(); else push();', 'pull();'],
  ['mapclick', 'logic', 'var o = { id: m.id, src: m.src, param: m.param, pol: m.pol, depth: m.depth };', "var o = { id: m.id, src: m.src, param: m.param, pol: 'bi', depth: m.depth };"],
  ['mapclick', 'device', 'if (dirty) { step(label, function () { st.routes = next; st.nextRoute += fresh; }); changed(); }', 'if (dirty) { st.routes = next; st.nextRoute += fresh; changed(); }'],
  ['mapdrag', 'logic', '    function push() {\n      if (S.drag) return;', '    function push() {'],
  // MD1 (ADR-340 r7): the bundle back on the Map Mode spec's old rule (a plain drag under MAP sets depth) must turn mapdrag red
  ['mapdrag', 'bundle', 'if (S.map.on && !e.altKey) { st.mapClick = true;', 'if (false) { st.mapClick = true;'],
  ['mapkeys', 'device', 'if (st.routes.some(function (x) { return kept.indexOf(x.id) < 0; })) {', 'if (false) {'],
  ['mapcontrol', 'bundle', "btn.addEventListener('click', function () { S.setMap(!S.map.on); });", "btn.addEventListener('click', function () { S.setMap(true); });"],
  ['maprefused', 'logic', "      pull();\n      if (r.refused.length) S.say(", "      if (r.refused.length) S.say("],
  ['maprefused', 'logic', "if (r.refused.length) S.say('Refused: ' + r.refused[0].why);", 'void 0;'],
  ['mappage', 'shell', 'page = id; closeMenu();', 'page = id; closeMenu(); S.setMap(false);'],
  ['settab', 'logic', ': [{ id: g.pages[0], label: g.label }] };', ': undefined };'],
  ['envsummary', 'logic', 'var fb = dev.followed(follow);', 'var fb = follow.options[Object.keys(follow.options)[0]];'],
  // slice 7 (ADR-335). The keys left to keyboard mode (the scope constant false); a key taken twice (the shell does not stand aside in
  // keyboard mode); a key taken in a field; a Cmd-key taken; a flag the code ignores; a flag the page overrides; a tap that the session's
  // own keyup misreads as a hold; a section shown with a description it has not, or without its DRAFT mark; an info box that can grow or
  // whose text does not scroll; a MAP control that can shrink.
  ['mapkeys', 'logic', 'var MAP_KEYS_ALWAYS = true;', 'var MAP_KEYS_ALWAYS = false;'],
  // Master batch 2 (ADR-338 r11) moved the keys and Escape into the SESSION (the vendored bundle), so the plants that mutated the shell's
  // own key listener now mutate the same fault in the session's: a key taken twice, a tap misread as a hold, a key taken in a field, a
  // Cmd/Ctrl key taken, the field kinds cut to INPUT, a flag the session ignores, and the page overriding the flag.
  ['mapkeys', 'bundle', "if (dg && S.map.on) { var n = +dg[1]; if (n >= 1 && n <= S.sources.length) S.setSrc(S.sources[n - 1].id); e.preventDefault(); return; }",
    "if (dg && S.map.on) { var n = +dg[1]; if (n >= 1 && n <= S.sources.length) { S.setSrc(S.sources[n - 1].id); S.setSrc(S.sources[n - 1].id); } e.preventDefault(); return; }"],
  ['mapkeys', 'bundle', "if (e.code === 'KeyM' && S.map.momentary) { if (e.timeStamp - mDown < 280)", "if (e.code === 'KeyM' && S.map.momentary) { if (e.timeStamp - mDown < 0)"],
  ['mapkeysfield', 'bundle', "if (e.repeat || (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA' || e.target.isContentEditable))) return;", 'if (e.repeat) return;'],
  ['mapkeysfield', 'bundle', 'if (e.metaKey || e.ctrlKey) return;\n      /* THE MAP KEYS', '/* THE MAP KEYS'],
  ['mapkeysfield', 'bundle', "e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA' || e.target.isContentEditable))) return;   // a field owns", 'false))) return;   // a field owns'],
  ['mapkeysflag', 'bundle', 'if (!S.keys && !S.mapKeysAlways) return;', ''],
  ['mapkeysflag', 'shell', 'mapKeysAlways: SH.MAP_KEYS_ALWAYS,', 'mapKeysAlways: true,'],
  // master batch 2, half A: Escape is layered through the session, and the removal is one step through the adapter
  ['escape', 'bundle', 'if (L.active()) { L.cancel(); this.emit(); return L.name; }', 'if (L.active()) { L.cancel(); }'],
  ['escape', 'shell', "S.escapeLayers.unshift({ name: 'menu'", "S.escapeLayers.push({ name: 'menu'"],
  ['removal', 'device', 'return r.src !== src; });\n      });', 'return r.src !== src || r.id === gone[0].id; });\n      });'],
  ['removal', 'device', "if (gone.length) step('Remove '", "if (gone.length) (function (l, f) { f(); })('Remove '"],
  ['removal', 'device', '      if (k >= 0) pending.splice(k, 1);\n      if (gone.length) step(', '      if (gone.length) step('],
  ['shellparts', 'shell', "H.ModBank($('bankHost'), S, {", "H.ModBankX($('bankHost'), S, {"],
  ['shellparts', 'shell', "strip: $('srcStrip'), chip: true });", "strip: $('srcStrip') });"],
  ['shellparts', 'shell', 'dev.removeModulator(m.id)', 'dev.clearRoutes(m.id)'],
  ['sections', 'logic', "text: 'No section description yet.'", "text: 'Shapes the sound of this part of the instrument.'"],
  ['sections', 'logic', "tag: 'SECTION · DRAFT'", "tag: 'SECTION'"],
  ['infofixed', 'shell', 'height: var(--foot-h); min-width: 0; }', 'min-width: 0; }'],
  ['infofixed', 'shell', '.info .face { flex: 1 1 auto; min-height: 0; overflow-y: auto;', '.info .face { flex: 1 1 auto; min-height: 0; overflow-y: visible;'],
  ['infofixed', 'shell', '.info { width: var(--info-w); flex: 0 0 var(--info-w); height: 100%;', '.info { width: var(--info-w); flex: 0 0 var(--info-w); height: auto;'],
  ['mapbar', 'shell', '.mapbar #mapHost { flex: 0 0 auto; }', '.mapbar #mapHost { flex: 0 1 auto; }'],
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
