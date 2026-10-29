/* GUI 3's no-audio engine stub (ROADMAP B584, ADR-316). horde, 2026-10-29.

   WHAT THIS IS. The engine's interface as SPEC-ENGINE section 7 states it, with no audio behind it: it
   keeps every parameter's normalised value v (IF-11), applies the laws of 5.1 for display, takes
   snapshots and restores them (RC-11.2), writes and reads the state document's members (IF-4, RC-9.3),
   passes parameter gestures back out unchanged (RC-11.3), and answers the observation view (IF-10)
   with what a silent engine has: the heard stepped values, zero counters, no voices.

   THE SEAM. GUI3Engine.METHODS is the engine's interface; the adapter (device.js) reaches it only
   through a transport (GUI3Engine.TRANSPORT, below), so it is mirror and confirm (ADR-319 item 3): the
   adapter shows what it sent at once, and a snapshot brings back only what the engine changed itself.
   SyncTransport refuses an engine that lacks a method, by name. The WebAssembly engine (SPEC-ENGINE
   IF-10, clean-room stage 8, ADR-280) plugs in behind an asynchronous transport with the same two
   methods; nothing above the transport changes. docs/design/gui3/README.md says what each owes.

   WHAT IS NOT THE ENGINE'S, and so is not here: the morph (its corners and position are the morph
   system's own state and reach the engine only as values, RC-11.1), the mod matrix and its routes
   (the device's, RC-4 deferred), and the undo history (the shell's, RC-11). device.js holds those.

   SHAPE ONLY. saveState's text is JSON carrying IF-4's members (version 1, every parameter's v, the
   two oscillators' ratios text); the exact text format is the suite's harness header's, not this
   file's. Version-0 documents (B.2) are refused: their translation is the engine's job.

   Deterministic: no clock, no random source; same calls, same answers. */
(function (root) {
  'use strict';
  var METHODS = ['manifest', 'setParam', 'getParam', 'value', 'display', 'snapshot', 'restore',
    'saveState', 'loadState', 'gesture', 'events', 'observe'];
  var MINUS = '−';

  function clamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
  function frac(v) { return v - Math.floor(v); }

  /* x(v) for a continuous row: 5.1's law table. A stepped row's value is its option index. */
  function lawX(row, v) {
    var lo = row.lo, hi = row.hi;
    switch (row.law) {
      case 'lin': return lo + (hi - lo) * v;
      case 'lin, wraps': return lo + (hi - lo) * frac(v);
      case 'log': return lo * Math.pow(hi / lo, v);
      case 'pow': return lo + (hi - lo) * v * v;
      case 'log, off zone': return v <= 0.01 ? 0 : 0.004 * Math.pow(hi / 0.004, (v - 0.01) / 0.99);
      case 'dB': return v === 0 ? -Infinity : lo + (hi - lo) * v;
      case 'step': return Math.round(v * (row.n - 1));
    }
    throw new Error('unknown law ' + row.law);
  }
  function snap(row, v) {
    v = clamp01(v);
    return row.law === 'step' ? (row.n === 1 ? 0 : Math.round(v * (row.n - 1)) / (row.n - 1)) : v;
  }
  function signed(x, digits) { var s = Math.abs(x).toFixed(digits); return (x > 0 ? '+' : x < 0 && Number(s) !== 0 ? MINUS : '') + s; }
  function fixed(x, digits) { return (x < 0 && Number(Math.abs(x).toFixed(digits)) !== 0 ? MINUS : '') + Math.abs(x).toFixed(digits); }

  /* The display shows the unit (IF-11). Presentation only: what the host, morph and mod move is v. */
  function display(row, v) {
    var x = lawX(row, v), u = row.unit;
    if (row.law === 'step') return row.options ? row.options[x] : String(row.lo + x);
    if (row.law === 'dB') return x === -Infinity ? MINUS + '∞ dB' : signed(x, 1) + ' dB';
    if (row.law === 'log, off zone' && x === 0) return 'off';
    var bip = row.lo < 0;
    switch (u) {
      case 's': return x < 1 ? fixed(x * 1000, x < 0.01 ? 2 : 1) + ' ms' : fixed(x, 2) + ' s';
      case 'ms': return fixed(x, 1) + ' ms';
      case 'Hz': return x >= 1000 ? fixed(x / 1000, 2) + ' kHz' : fixed(x, x < 10 ? 2 : 0) + ' Hz';
      case '%': return (row.hi > 1 ? (bip ? signed(x, 0) : fixed(x, 0)) : (bip ? signed(x * 100, 0) : fixed(x * 100, 0))) + ' %';
      case 'st': return signed(x, 2) + ' st';
      case 'cents': return signed(x, 1) + ' ct';
      case '°': return (bip ? signed(x, 0) : fixed(x, 0)) + '°';
      case '×': return '×' + fixed(x, 2);
      case '/s': return fixed(x, x < 10 ? 1 : 0) + ' /s';
      case 'rad': return signed(x, 2) + ' rad';
      case 'cycle': return fixed(x, 3) + ' cyc';
      case '±cycle': return signed(x, 2) + ' cyc';
      case '±Hz': return signed(x, 2) + ' Hz';
      case 'harmonics': return signed(x, 1) + ' harm';
      default: return (bip ? signed(x, 2) : fixed(x, 2)) + (u && u !== '±' ? ' ' + u : '');
    }
  }

  function Stub(params) {
    if (!(this instanceof Stub)) return new Stub(params);
    var E = this, rows = params.slice(), by = Object.create(null), v = Object.create(null);
    rows.forEach(function (r) { by[r.key] = r; v[r.key] = r.default_v; });
    var ratios = { osc1: '1, 5/4, 3/2', osc2: '1, 5/4, 3/2' };   // 5.3: oscillator state, not a host parameter
    var refused = 0, out = [], extra = [];                        // extra: a loaded state's unknown members, in the order read (RC-9.3)
    function host(key) { var r = by[key]; return r && !r.reserved ? r : null; }

    E.manifest = function () { return rows; };
    /* A Param for an unknown key, or for a reserved row's, is refused and counted (5.1, PA-7). */
    E.setParam = function (key, val) {
      var r = host(key);
      if (!r || typeof val !== 'number' || val !== val) { refused++; return null; }
      v[key] = snap(r, val); return v[key];
    };
    E.getParam = function (key) { return by[key] ? (by[key].reserved ? 0 : v[key]) : null; };
    E.value = function (key) { var r = by[key]; return r ? lawX(r, E.getParam(key)) : null; };
    E.display = function (key, val) { var r = by[key]; return r ? display(r, val === undefined ? E.getParam(key) : snap(r, val)) : ''; };
    /* RC-11.2: every parameter's v and the ratios text; restore applies them as one set of changes (RC-11.4). */
    E.snapshot = function () { return { v: rows.map(function (r) { return v[r.key]; }), ratios: { osc1: ratios.osc1, osc2: ratios.osc2 } }; };
    E.restore = function (s) {
      if (!s || !s.v || s.v.length !== rows.length) return false;
      rows.forEach(function (r, i) { v[r.key] = s.v[i]; }); ratios = { osc1: s.ratios.osc1, osc2: s.ratios.osc2 }; return true;
    };
    /* IF-4's members as JSON (SHAPE ONLY, see the header). Known keys in manifest order, then the unknown ones kept. */
    E.saveState = function () {
      var p = {}, top = {};
      rows.forEach(function (r) { p[r.key] = v[r.key]; });
      extra.forEach(function (e) { if (e.where === 'params') p[e.key] = e.value; });
      var doc = { version: 1, params: p, ratios: { osc1: ratios.osc1, osc2: ratios.osc2 } };
      extra.forEach(function (e) { if (e.where === 'top') top[e.key] = e.value; });
      for (var k in top) doc[k] = top[k];
      return JSON.stringify(doc);
    };
    E.loadState = function (text) {
      var d;
      try { d = JSON.parse(text); } catch (err) { return { ok: false, why: 'not JSON' }; }
      if (!d || d.version === undefined) return { ok: false, why: 'a version-0 document (B.2): its translation is the engine’s, not the stub’s' };
      if (d.version !== 1) return { ok: false, why: 'version ' + d.version + ' is newer than this build' };
      var keep = [];
      for (var k in d.params) {
        if (by[k]) v[k] = by[k].reserved ? d.params[k] : snap(by[k], d.params[k]);   // a reserved row's v is kept as read (5.1)
        else keep.push({ where: 'params', key: k, value: d.params[k] });
      }
      if (d.ratios) ratios = { osc1: String(d.ratios.osc1), osc2: String(d.ratios.osc2) };
      for (var t in d) if (t !== 'version' && t !== 'params' && t !== 'ratios') keep.push({ where: 'top', key: t, value: d[t] });
      extra = keep;
      return { ok: true };
    };
    /* RC-11.3: accepted and reported back unchanged; they change nothing in the sound. */
    E.gesture = function (key, phase) { if (phase !== 'begin' && phase !== 'end') return false; out.push({ type: 'gesture', key: key, phase: phase }); return true; };
    E.events = function () { var e = out; out = []; return e; };
    /* IF-10: read without changing anything. A silent engine has no voices; its counters (IF-9) stay 0.
       The heard value of every stepped key (IF-4) is the one thing it can report, and it does. */
    E.observe = function () {
      var heard = {};
      rows.forEach(function (r) { if (r.law === 'step' && !r.reserved) heard[r.key] = display(r, v[r.key]); });
      return { source: 'stub', audio: false, voices: [], heard: heard, refused: refused,
        counters: { memberSamples: 0, edgesCorrected: 0, edgesLeftOver: 0, envelopeSteps: 0, voicesSounding: 0 } };
    };
  }

  /* THE TRANSPORT (ADR-319 item 3, B588). The adapter never calls an engine: it sends messages and hears
     snapshots, through any object with TRANSPORT (`send(msg)` and `onSnapshot(fn)`), so the way the two
     sides talk can be swapped without touching the shell or the adapter. Messages:
       { type: 'set', seq, values: { key: v, ... } }     parameter values (one message for many keys)
       { type: 'gesture', seq, key, phase }              RC-11.3's begin and end
     A snapshot is { ack, params: { key: v }, events, observe, state }: `ack` is the seq of the last message
     the engine applied, so the adapter can tell a snapshot that has not yet seen its latest send (stale)
     from a change the engine made itself (engine-originated: host automation, a load, a clamp).
     This one is SYNCHRONOUS: send applies the message and publishes a snapshot before it returns. The two
     to come are written up in gui3/README.md: the AudioWorklet's postMessage (asynchronous, the engine
     publishes RC-8's snapshot on its own clock) and shared memory (B588: a SharedArrayBuffer double
     buffer, which needs cross-origin isolation). `publish()` is the stub's stand-in for the engine
     publishing on its own; the seam check uses it to play host automation. */
  var TRANSPORT = ['send', 'onSnapshot'];
  function SyncTransport(engine) {
    var missing = METHODS.filter(function (m) { return !engine || typeof engine[m] !== 'function'; });
    if (missing.length) throw new Error('the engine lacks the seam’s methods: ' + missing.join(', '));
    var subs = [], ack = 0, rows = engine.manifest();
    function snapshot() {
      var p = {};
      rows.forEach(function (r) { if (!r.reserved) p[r.key] = engine.getParam(r.key); });
      return { ack: ack, params: p, events: engine.events(), observe: engine.observe(), state: engine.saveState() };
    }
    function publish() { var s = snapshot(); subs.forEach(function (fn) { fn(s); }); }
    return {
      send: function (m) {
        if (m.type === 'set') { for (var k in m.values) engine.setParam(k, m.values[k]); }
        else if (m.type === 'gesture') engine.gesture(m.key, m.phase);
        else throw new Error('unknown message ' + m.type);
        ack = m.seq; publish();
      },
      onSnapshot: function (fn) { subs.push(fn); fn(snapshot()); },
      publish: publish
    };
  }

  root.GUI3Engine = { METHODS: METHODS, TRANSPORT: TRANSPORT, Stub: Stub, SyncTransport: SyncTransport, lawX: lawX, snap: snap, display: display };
})(typeof window !== 'undefined' ? window : globalThis);
