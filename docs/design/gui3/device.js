/* GUI 3's device layer: the ONE adapter the shell talks to (ROADMAP B584, ADR-316, ADR-319). horde, 2026-10-29.

   The shell never reaches the engine. It calls this, which holds what the spec says is NOT the engine's,
   and sends the result to the engine as parameter values through a swappable transport:
     - the morph: four corners per morphing parameter, the position, morph on or off, the corner audible
       while it is off, and each corner's name and saved preset (RC-11.1: the morph system's own state).
       Morphable rows blend; Structural rows flip to their owner corner (IF-4); Device rows stay out of
       the morph and hold one value.
       MORPH OFF (ADR-319 item 1): only the audible corner (A by default, chosen on the Morph page) sounds
       and takes writes; B to D (the other three) are silent and frozen: a write addressed to one of them
       is refused, and their values come back unchanged when morph turns on again.
     - the mod routes: the device's matrix (RC-4: the matrix and every route are deferred from the
       engine). A route is refused unless its destination is a continuous host parameter of the
       manifest (RC-4's must-not) and its source is one IF-3 says the engine publishes.
     - the history: undo and redo over device snapshots (RC-11: the stack lives in the shell, not the
       engine). One gesture is one step (RC-11.3's begin and end go to the engine as they come); a
       write outside a gesture is a step of its own; a write that changes nothing is no step. Turning
       morph on or off is a step (ADR-319 item 2); the morph position is performance and is not.
   MIRROR AND CONFIRM (ADR-319 item 3). Every value is shown the moment it is sent (`shown` reads the
   adapter's own state). A snapshot from the engine changes the display only where the engine changed a
   value itself: it is skipped for a key whose latest send the snapshot has not yet seen (its `ack` is
   older: stale) and for the key being dragged, and otherwise a value that differs from what was sent is
   engine-originated and shown until the user writes that key again.
   The transport is any object with GUI3Engine.TRANSPORT (`send`, `onSnapshot`): GUI3Engine.SyncTransport
   over the stub today; the AudioWorklet and shared-memory transports later (gui3/README.md).

   STAND-INS, labelled. Ownership: the highest corner weight plus a seeded per-key offset (so parameters
   flip at different points), with IF-4's hysteresis band of 0.1; the real point is the morph system's.

   Deterministic: mulberry32 streams seeded per key (no clock, no Math.random). */
(function (root) {
  'use strict';
  var HYST = 0.1, NAME_MAX = 40;
  /* IF-3's published per-voice sources, plus IF-1's velocity, aftertouch and the four MPE expressions.
     The LFOs and macros are the device's and do not exist yet (RC-4 deferred), so they are not listed. */
  var SOURCES = [
    { id: 'env1', name: 'ENV 1', kind: 'env' }, { id: 'env2', name: 'ENV 2', kind: 'env' },
    { id: 'env3', name: 'ENV 3', kind: 'env' }, { id: 'env4', name: 'ENV 4', kind: 'env' },
    { id: 'velocity', name: 'Velocity', kind: 'engine' }, { id: 'aftertouch', name: 'Aftertouch', kind: 'engine' },
    { id: 'osc1R', name: 'Osc 1 R', kind: 'engine' }, { id: 'osc1Psi', name: 'Osc 1 ψ', kind: 'engine' },
    { id: 'osc2R', name: 'Osc 2 R', kind: 'engine' }, { id: 'osc2Psi', name: 'Osc 2 ψ', kind: 'engine' },
    { id: 'onset', name: 'Onset burst', kind: 'engine' }, { id: 'gravity', name: 'Gravity offset', kind: 'engine' },
    { id: 'vibrato', name: 'Vibrato', kind: 'engine' }, { id: 'expression', name: 'Expression', kind: 'engine' },
    { id: 'brightness', name: 'Brightness', kind: 'engine' }, { id: 'pressure', name: 'Pressure', kind: 'engine' }
  ];
  var POLS = ['uni', 'bi', 'inv', 'ctl'], CORNER_IDS = ['A', 'B', 'C', 'D'];

  function mulberry32(a) { return function () { a |= 0; a = a + 0x6D2B79F5 | 0; var t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
  function hash32(s) { var x = 2166136261; for (var i = 0; i < s.length; i++) { x ^= s.charCodeAt(i); x = Math.imul(x, 16777619); } return x >>> 0; }
  function clamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
  function copy(o) { var c = {}; for (var k in o) c[k] = o[k]; return c; }

  function create(data, transport, opts) {
    opts = opts || {};
    if (!root.GUI3Engine) throw new Error('GUI3Engine (engine-stub.js) is not loaded: the seam has no interface to check against');
    var missing = root.GUI3Engine.TRANSPORT.filter(function (m) { return !transport || typeof transport[m] !== 'function'; });
    if (missing.length) throw new Error('the transport lacks the seam’s methods: ' + missing.join(', '));
    var seed = opts.seed === undefined ? 1 : opts.seed >>> 0;
    var rows = data.params.filter(function (r) { return !r.reserved; }), by = Object.create(null);
    rows.forEach(function (r) { by[r.key] = r; });
    var morphing = rows.filter(function (r) { return r.class !== 'D'; });
    var jit = Object.create(null);                   // the seeded per-key offset of the ownership stand-in
    morphing.forEach(function (r) { var g = mulberry32(hash32(r.key) ^ seed); jit[r.key] = [0, 1, 2, 3].map(function () { return (g() - 0.5) * 0.32; }); });

    // st is everything undo restores. The morph position (pos) is deliberately outside it (ADR-319 item 2).
    var st = { corners: [{}, {}, {}, {}], dev: {}, routes: [], nextRoute: 1, morphOn: false, audible: 0,
      names: [null, null, null, null], saved: [null, null, null, null] };
    morphing.forEach(function (r) { for (var i = 0; i < 4; i++) st.corners[i][r.key] = r.default_v; });
    rows.forEach(function (r) { if (r.class === 'D') st.dev[r.key] = r.default_v; });
    var pos = { x: 0.5, y: 0.5 }, owners = Object.create(null);
    var past = [], future = [], tx = null, rev = 0;
    var seq = 0, sentSeq = Object.create(null), sentV = Object.create(null), fromEngine = Object.create(null);
    var lastSnap = null, events = [];
    var pending = [];                                // sources activated with the bank's + and not yet mapped (B556): never saved, never undone
    var D = {};

    function snap(r, v) { return root.GUI3Engine.snap(r, v); }
    function weights() {
      if (!st.morphOn) { var w = [0, 0, 0, 0]; w[st.audible] = 1; return w; }
      var x = pos.x, y = pos.y; return [(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y];
    }
    function reown() {
      var w = weights();
      morphing.forEach(function (r) {
        if (!st.morphOn) { owners[r.key] = st.audible; return; }
        var j = jit[r.key], best = 0, bv = -1e9;
        for (var i = 0; i < 4; i++) { var s = w[i] + j[i]; if (s > bv) { bv = s; best = i; } }
        var cur = owners[r.key];
        owners[r.key] = cur === undefined || bv - (w[cur] + j[cur]) > HYST ? best : cur;
      });
    }
    /* morph.on and morph.audible are D.2 rows (ADR-330) and also the adapter's own morph state (ADR-319): one value,
       read from the state, so the chrome's switch, the Morph page's control and the resolver never disagree. */
    function heard(key) {
      var r = by[key]; if (!r) return null;
      if (key === 'morph.on') return st.morphOn ? 1 : 0;
      if (key === 'morph.audible') return st.audible / 3;
      if (r.class === 'D') return st.dev[key];
      if (r.class === 'S') return st.corners[owners[key]][key];
      var w = weights(), b = 0; for (var i = 0; i < 4; i++) b += w[i] * st.corners[i][key]; return b;
    }
    /* Send the heard values of `keys` (all by default) as one message. A value the adapter sends replaces an
       engine-originated one for that key, so a key the engine had changed is always re-sent, even when our
       own value did not move (else the engine would keep its value while the display showed ours). */
    /* Each value goes to the side that owns it (ADR-330): the engine's to the engine (IF-11), the morph's settings to
       the resolver (IF-12: "the morph's parameters reach the resolver, never the engine"), a hosted module's to the
       rack (RC-5, IF-7) and Arp's to the device's MIDI module. One message per side. */
    var MSG = { engine: 'set', resolver: 'morph', rack: 'rack', midi: 'midi' };
    function send(keys) {
      var by_t = {}, n = 0;
      (keys || rows.map(function (r) { return r.key; })).forEach(function (k) {
        var v = heard(k), had = k in fromEngine; delete fromEngine[k];
        if (!had && sentV[k] === v) return;
        var t = MSG[by[k].target || 'engine'];
        (by_t[t] = by_t[t] || {})[k] = v; n++;
      });
      if (!n) return;
      ['set', 'morph', 'rack', 'midi'].forEach(function (t) {
        var vals = by_t[t]; if (!vals) return;
        var s = ++seq;
        for (var k in vals) { sentV[k] = vals[k]; sentSeq[k] = s; }
        transport.send(t === 'morph' ? { type: t, seq: s, settings: vals } : { type: t, seq: s, values: vals });
      });
    }
    function onSnapshot(s) {
      lastSnap = s;
      if (s.events && s.events.length) events = events.concat(s.events);
      var g = tx && tx.key, moved = false;
      for (var k in s.params) {
        if (!by[k]) continue;
        if (sentSeq[k] !== undefined && s.ack < sentSeq[k]) continue;   // stale: it has not seen our latest send
        if (k === g) continue;                                             // never under a drag
        var v = s.params[k];
        if (v === sentV[k]) { if (k in fromEngine) { delete fromEngine[k]; moved = true; } continue; }
        if (fromEngine[k] !== v) { fromEngine[k] = v; moved = true; }    // engine-originated
      }
      if (moved) rev++;
    }
    function snapshot() { return JSON.stringify(st); }
    function apply(s) { st = JSON.parse(s); reown(); send(); rev++; }
    function changed() { rev++; }

    /* A transaction: begin/end bracket one undo step. Nested begins join the outer one. */
    D.begin = function (label, gestureKey) {
      if (tx) { tx.depth++; return; }
      tx = { label: label || 'Edit', before: snapshot(), depth: 1, key: gestureKey || null };
      if (tx.key) transport.send({ type: 'gesture', seq: ++seq, key: tx.key, phase: 'begin' });
    };
    D.end = function () {
      if (!tx) return false;
      if (--tx.depth > 0) return false;
      var t = tx; tx = null;
      if (t.key) transport.send({ type: 'gesture', seq: ++seq, key: t.key, phase: 'end' });
      var after = snapshot();
      if (after === t.before) return false;
      past.push({ label: t.label, before: t.before, after: after }); future.length = 0; return true;
    };
    D.inTx = function () { return !!tx; };
    D.gestureKey = function () { return tx && tx.key; };
    function step(label, fn) { var own = !tx; if (own) D.begin(label); try { return fn(); } finally { if (own) D.end(); } }

    D.params = function () { return rows; };
    D.row = function (key) { return by[key] || null; };
    D.rev = function () { return rev; };
    D.weights = weights;
    D.heard = heard;
    /* What the display shows: the engine's own change where it made one, else what we hold and sent. */
    D.shown = function (key) { return key in fromEngine ? fromEngine[key] : heard(key); };
    D.fromEngine = function () { return Object.keys(fromEngine); };
    D.owner = function (key) { var r = by[key]; return !r || r.class === 'D' ? null : owners[key]; };
    D.corner = function (key, i) { var r = by[key]; if (!r) return null; return r.class === 'D' ? st.dev[key] : st.corners[i][key]; };
    /* Write v to one corner: the requested one, else the owner while morph is on; while it is off, only the
       audible corner takes writes, and a write addressed to a frozen corner is refused (null). */
    D.set = function (key, v, o) {
      var r = by[key]; if (!r || typeof v !== 'number' || v !== v) return null;
      v = snap(r, v);
      if (key === 'morph.on') { D.setMorph({ on: v >= 0.5 }); return v; }
      if (key === 'morph.audible') { D.setAudible(Math.round(v * 3)); return v; }
      var want = o && o.corner !== undefined && o.corner !== null ? o.corner : null, i;
      if (r.class === 'D') i = null;
      else if (!st.morphOn) { if (want !== null && want !== st.audible) return null; i = st.audible; }
      else i = want !== null ? want : owners[key];
      var cur = i === null ? st.dev[key] : st.corners[i][key];
      if (cur === v) return v;
      step((o && o.label) || r.label, function () { if (i === null) st.dev[key] = v; else st.corners[i][key] = v; });
      send([key]); changed(); return v;
    };
    D.morph = function () { return { on: st.morphOn, x: pos.x, y: pos.y, audible: st.audible, weights: weights() }; };
    /* Position: performance, never a step. On/off: one undo step (ADR-319 item 2). */
    D.setMorph = function (m) {
      var x = m.x === undefined ? pos.x : clamp01(m.x), y = m.y === undefined ? pos.y : clamp01(m.y);
      var on = m.on === undefined ? st.morphOn : !!m.on;
      if (x === pos.x && y === pos.y && on === st.morphOn) return;
      var moved = x !== pos.x || y !== pos.y;
      pos = { x: x, y: y };
      if (moved) transport.send({ type: 'position', seq: ++seq, x: x, y: y });   // IF-12: the position, to the resolver
      if (on !== st.morphOn) step(on ? 'Morph on' : 'Morph off', function () { st.morphOn = on; });
      reown(); send(morphing.map(function (r) { return r.key; }).concat(by['morph.on'] ? ['morph.on'] : [])); changed();
    };
    D.setAudible = function (i) {
      if (!(i >= 0 && i < 4) || i === st.audible) return false;
      step('Audible with morph off: corner ' + CORNER_IDS[i], function () { st.audible = i; });
      reown(); send(morphing.map(function (r) { return r.key; }).concat(by['morph.audible'] ? ['morph.audible'] : [])); changed(); return true;
    };
    /* Corners: names (a preset's name, never product copy; null reads "Empty"), saved presets, revert. */
    function okName(n) { return typeof n === 'string' && n.trim().length > 0 && n.trim().length <= NAME_MAX; }
    D.names = function () { return st.names.slice(); };
    /* "Save current sound to snapshot" (ADR-319 item 1): what is heard now becomes corner i, named, and saved. */
    D.saveSoundTo = function (i, name) {
      if (!(i >= 0 && i < 4) || !okName(name)) return { ok: false, why: 'a corner A to D and a name of 1 to ' + NAME_MAX + ' characters' };
      step('Save the current sound to ' + CORNER_IDS[i] + ' as ' + name.trim(), function () {
        morphing.forEach(function (r) { st.corners[i][r.key] = heard(r.key); });
        st.names[i] = name.trim(); st.saved[i] = copy(st.corners[i]);
      });
      reown(); send(morphing.map(function (r) { return r.key; })); changed(); return { ok: true };
    };
    /* "Save preset to [corner]": corner i's own values become its saved preset, under a name. */
    D.savePreset = function (i, name) {
      if (!(i >= 0 && i < 4) || !okName(name)) return { ok: false, why: 'a corner A to D and a name of 1 to ' + NAME_MAX + ' characters' };
      step('Save corner ' + CORNER_IDS[i] + ' as ' + name.trim(), function () { st.names[i] = name.trim(); st.saved[i] = copy(st.corners[i]); });
      changed(); return { ok: true };
    };
    D.edited = function (i) {
      var s = st.saved[i]; if (!s) return false;
      return morphing.some(function (r) { return st.corners[i][r.key] !== s[r.key]; });
    };
    D.revert = function (i) {
      if (!st.saved[i] || !D.edited(i)) return false;
      step('Revert corner ' + CORNER_IDS[i] + ' to ' + st.names[i], function () { st.corners[i] = copy(st.saved[i]); });
      send(morphing.map(function (r) { return r.key; })); changed(); return true;
    };
    D.sources = function () { return SOURCES; };
    D.routes = function () { return st.routes.map(function (r) { return { id: r.id, src: r.src, param: r.param, pol: r.pol, depth: r.depth }; }); };
    D.addRoute = function (rt) {
      var r = by[rt && rt.param];
      // A destination is a continuous parameter of a manifest (RC-4): an engine row that takes per-voice modulation,
      // or a hosted module's or Arp's continuous row. The morph's settings are the resolver's inputs (IF-12), not
      // modulation destinations in any ruling, so they are refused.
      if (!r || r.law === 'step' || (r.target === 'resolver') || ((r.target || 'engine') === 'engine' && !r.pv)) return { ok: false, why: (rt && rt.param) + ' is not a continuous parameter a route can reach (RC-4)' };
      if (!SOURCES.some(function (s) { return s.id === rt.src; })) return { ok: false, why: rt.src + ' is not a source the engine publishes (IF-3)' };
      var pol = rt.pol || 'uni', depth = rt.depth === undefined ? 0.25 : rt.depth;
      if (POLS.indexOf(pol) < 0 || typeof depth !== 'number' || !(depth >= 0 && depth <= 1)) return { ok: false, why: 'depth is in the destination’s normalised range, 0 to 1 (RC-4)' };
      var id = 'r' + st.nextRoute;
      step('Route ' + rt.src + ' → ' + r.label, function () { st.routes.push({ id: id, src: rt.src, param: rt.param, pol: pol, depth: depth }); st.nextRoute++; });
      changed(); return { ok: true, id: id };
    };
    D.removeRoute = function (id) {
      var k = st.routes.findIndex(function (x) { return x.id === id; }); if (k < 0) return false;
      step('Remove route', function () { st.routes.splice(k, 1); }); changed(); return true;
    };
    D.clearRoutes = function () { if (!st.routes.length) return false; step('Clear routes', function () { st.routes = []; }); changed(); return true; };
    /* THE BANK'S + (ADR-322 item 3, B556). A modulator activated with + is PENDING: shown in the bank, routed
       nowhere, and outside the patch (not in st, so no undo step and nothing saved). Mapping it (a route from it)
       makes it an ordinary routed modulator; closing the interface or ending the session with it still unmapped
       discards it (close()). A source that already has a route is active already and is not activated again. */
    function routed(src) { return st.routes.some(function (r) { return r.src === src; }); }
    D.pending = function () { return pending.filter(function (id) { return !routed(id); }); };
    D.activate = function (src) {
      if (!SOURCES.some(function (s) { return s.id === src; })) return { ok: false, why: src + ' is not a source the engine publishes (IF-3)' };
      if (routed(src) || pending.indexOf(src) >= 0) return { ok: false, why: src + ' is active already' };
      pending.push(src); changed(); return { ok: true };
    };
    /* A map handle dropped on a parameter: a route through addRoute, which refuses what RC-4 refuses. A refused
       drop leaves a pending source pending; an accepted one maps it. */
    D.drop = function (src, key) {
      var res = D.addRoute({ src: src, param: key, pol: 'uni', depth: 0.25 });
      if (res.ok) { var k = pending.indexOf(src); if (k >= 0) pending.splice(k, 1); changed(); }
      return res;
    };
    /* The interface closes (or the session ends): unmapped activations are dropped. Returns what was dropped. */
    D.close = function () {
      var dropped = D.pending();
      pending = [];
      if (dropped.length) changed();
      return dropped;
    };
    /* Seeded spread of B to D around A for the morphable rows, so the pad has something to move. Refused
       while morph is off: B to D are frozen then. */
    D.scatter = function (s) {
      if (!st.morphOn) return false;
      s = s >>> 0;
      step('Scatter corners (seed ' + s + ')', function () {
        morphing.forEach(function (r) {
          if (r.class !== 'M') return;
          var g = mulberry32(hash32(r.key) ^ (s * 2654435761 >>> 0));
          for (var i = 1; i < 4; i++) st.corners[i][r.key] = clamp01(st.corners[0][r.key] + (g() - 0.5) * 0.6);
        });
      });
      send(); changed(); return true;
    };
    D.reset = function () {
      step('Reset to the init patch', function () {
        morphing.forEach(function (r) { for (var i = 0; i < 4; i++) st.corners[i][r.key] = r.default_v; });
        rows.forEach(function (r) { if (r.class === 'D') st.dev[r.key] = r.default_v; });
        st.routes = []; st.names = [null, null, null, null]; st.saved = [null, null, null, null];
      });
      reown(); send(); changed();
    };
    D.history = function () { return { past: past.map(function (e) { return e.label; }), future: future.map(function (e) { return e.label; }) }; };
    D.clearRedo = function () { future.length = 0; };   // the shell's self-checks undo their own probe and leave no redo behind
    D.clearHistory = function () { past.length = 0; future.length = 0; };   // a review link opens on a prepared state with nothing to undo
    D.undo = function () { if (tx || !past.length) return false; var e = past.pop(); future.push(e); apply(e.before); return true; };
    D.redo = function () { if (tx || !future.length) return false; var e = future.pop(); past.push(e); apply(e.after); return true; };
    /* What the engine said last: its state document (IF-4) and its observation view (IF-10), from the latest snapshot. */
    D.patch = function () { return { shell: JSON.parse(snapshot()), morph: D.morph(), engine: lastSnap ? lastSnap.state : null }; };
    D.observe = function () { return { engine: lastSnap ? lastSnap.observe : null, resolver: lastSnap ? lastSnap.resolver : null,
      device: lastSnap ? lastSnap.device : null, morph: D.morph(), routes: st.routes.length }; };
    D.engineEvents = function () { var e = events; events = []; return e; };
    /* The meters and clip latches the engine publishes (IF-6), read from the latest snapshot (MOCK in the stub), and the
       latch's clear, which goes to the engine like any other message (the latch lives on the audio side). */
    D.meters = function () { return lastSnap && lastSnap.observe && lastSnap.observe.meters || {}; };
    D.clearClip = function (id) { transport.send({ type: 'clear', seq: ++seq, id: id }); rev++; };
    /* The summed oscilloscope and spectrum the Mix page draws (ADR-333): read from the engine's observation view
       (IF-10; MOCK in the stub), never computed here. */
    D.summed = function () { return lastSnap && lastSnap.observe && lastSnap.observe.sum || null; };
    /* The envelope a source follows (RC-6.5, ADR-333): the generated `follow` names the source's selector row and one
       block per option; the block the selector holds now is the one shown and edited, and its rows are the shared keys
       (the Mod page's and Main's own), never a copy. */
    D.followed = function (f) {
      var r = by[f && f.selector]; if (!r || !r.options) return null;
      return f.options[r.options[Math.round(heard(f.selector) * (r.n - 1))]] || null;
    };

    reown();
    transport.onSnapshot(onSnapshot);
    send();
    return D;
  }

  root.GUI3Device = { create: create, SOURCES: SOURCES };
})(typeof window !== 'undefined' ? window : globalThis);
