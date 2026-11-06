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
     The LFOs are the device's and aren't in the engine yet (RC-4 deferred): they're listed MOCK, so the Mod page's editor has something to edit (GUI-37). The macros and the XY's axes
     aren't in the engine either (B89): they're here, MOCK, so routes can carry them until the intent bus does (GUI-9). The
     axes are bipolar because they rest at the pad's centre, so a new mapping offsets nothing until the puck moves. */
  var SOURCES = [
    { id: 'env1', name: 'ENV 1', kind: 'env' }, { id: 'env2', name: 'ENV 2', kind: 'env' },
    { id: 'env3', name: 'ENV 3', kind: 'env' }, { id: 'env4', name: 'ENV 4', kind: 'env' },
    { id: 'lfo1', name: 'LFO 1', kind: 'lfo', pol: 'bi' }, { id: 'lfo2', name: 'LFO 2', kind: 'lfo', pol: 'bi' },
    { id: 'lfo3', name: 'LFO 3', kind: 'lfo', pol: 'bi' }, { id: 'lfo4', name: 'LFO 4', kind: 'lfo', pol: 'bi' },
    { id: 'velocity', name: 'Velocity', kind: 'engine' }, { id: 'aftertouch', name: 'Aftertouch', kind: 'engine' },
    { id: 'osc1R', name: 'Osc 1 R', kind: 'engine' }, { id: 'osc1Psi', name: 'Osc 1 ψ', kind: 'engine' },
    { id: 'osc2R', name: 'Osc 2 R', kind: 'engine' }, { id: 'osc2Psi', name: 'Osc 2 ψ', kind: 'engine' },
    { id: 'onset', name: 'Onset burst', kind: 'engine' }, { id: 'gravity', name: 'Gravity offset', kind: 'engine' },
    { id: 'vibrato', name: 'Vibrato', kind: 'engine' }, { id: 'expression', name: 'Expression', kind: 'engine' },
    { id: 'brightness', name: 'Brightness', kind: 'engine' }, { id: 'pressure', name: 'Pressure', kind: 'engine' },
    { id: 'macro1', name: 'Tone', kind: 'macro' }, { id: 'macro2', name: 'Space', kind: 'macro' },
    { id: 'macro3', name: 'Time', kind: 'macro' }, { id: 'macro4', name: 'Motion', kind: 'macro' },
    { id: 'macro5', name: 'Macro 5', kind: 'macro' }, { id: 'macro6', name: 'Macro 6', kind: 'macro' },
    { id: 'macro7', name: 'Macro 7', kind: 'macro' }, { id: 'macro8', name: 'Macro 8', kind: 'macro' },
    { id: 'xyx', name: 'XY X', kind: 'macro', pol: 'bi' }, { id: 'xyy', name: 'XY Y', kind: 'macro', pol: 'bi' }
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
    var jit = Object.create(null), jitSeed = null;   // the seeded per-key offset of the ownership stand-in, redrawn when morph.seed moves
    function draw(key, s) { var g = mulberry32(hash32(key) ^ s); return [0, 1, 2, 3].map(function () { return (g() - 0.5) * 0.32; }); }

    // st is everything undo restores. The morph position (pos) is deliberately outside it (ADR-319 item 2).
    var st = { corners: [{}, {}, {}, {}], dev: {}, routes: [], nextRoute: 1, morphOn: false, audible: 0,
      names: [null, null, null, null], saved: [null, null, null, null], regions: {},   // regions: a row's own locked region offsets (B269), so undo restores them
      pins: {}, blend: { law: 'tps', clamp: true },
      modes: {}, counts: {} };   // MO-11: a key's mode and count, where they differ from the defaults. Per patch, never morphed
      // a row's waypoints (B235) and the patch's waypoint law (B272). Held, drawn and undone; the stub's blend doesn't hear them
    morphing.forEach(function (r) { for (var i = 0; i < 4; i++) st.corners[i][r.key] = r.default_v; });
    rows.forEach(function (r) { if (r.class === 'D') st.dev[r.key] = r.default_v; });
    var pos = { x: 0.5, y: 0.5 }, owners = Object.create(null);
    /* The undo tree (ADR-160, B119; was a flat stack): a node per gesture, holding the state before and after it. Going back and editing branches, so nothing you
       pass through is lost. Capped at 200, and eviction takes the oldest node OFF the path to where you stand, so a base you keep branching from outlives an abandoned run. */
    var HIST_CAP = 200, nodes = [], cur = null, nextNode = 0, tx = null, rev = 0;
    function newRoot() { nodes = [{ i: nextNode++, parent: -1, tick: 0, label: 'Start', before: null, after: null, last: null }]; cur = nodes[0]; }
    newRoot();
    function nodeOf(i) { for (var k = 0; k < nodes.length; k++) if (nodes[k].i === i) return nodes[k]; return null; }
    function childrenOf(n) { return nodes.filter(function (c) { return c.parent === n.i; }); }
    function pathOf(n) { var out = []; for (var q = n, g = 0; q && g <= HIST_CAP + 1; q = q.parent < 0 ? null : nodeOf(q.parent), g++) out.unshift(q); return out; }
    /* Redo goes back the way you came, else to the newest branch. */
    function redoChild(n) { var l = n.last !== null ? nodeOf(n.last) : null, c = childrenOf(n); return l && l.parent === n.i ? l : c.length ? c[c.length - 1] : null; }
    function evict() {
      while (nodes.length > HIST_CAP) {
        var on = {}; pathOf(cur).forEach(function (q) { on[q.i] = true; });
        var gone = nodes.filter(function (q) { return !on[q.i]; })[0] || nodes[0];
        nodes.splice(nodes.indexOf(gone), 1);
        nodes.forEach(function (q) { if (q.parent === gone.i) q.parent = -1; });   // its children become roots of their own
      }
    }
    var seq = 0, sentSeq = Object.create(null), sentV = Object.create(null), fromEngine = Object.create(null);
    var lastSnap = null, events = [];
    var pending = [];                                // sources activated with the bank's + and not yet mapped (B556): never saved, never undone
    var D = {};

    function snap(r, v) { return root.GUI3Engine.snap(r, v); }
    function weights() {
      if (!st.morphOn) { var w = [0, 0, 0, 0]; w[st.audible] = 1; return w; }
      var x = pos.x, y = pos.y; return [(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y];
    }
    /* A row's four region offsets, the same stand-in the pad paints: its own lock, else its seeded draw mixed toward one shared
       draw by Cohesion. One function, so the pad, the grid and the knobs can't disagree about who owns a row (ADR-110). */
    function offsets(key) {
      if (st.regions[key]) return st.regions[key];
      var sv = 'morph.seed' in st.dev ? st.dev['morph.seed'] : 0, c = 'morph.cohesion' in st.dev ? st.dev['morph.cohesion'] : 0;
      if (jitSeed !== sv) { jitSeed = sv; jit = Object.create(null); }
      var o = jit[key] || (jit[key] = draw(key, seed ^ Math.round(sv * 9999))), f = jit['#field'] || (jit['#field'] = draw('#field', seed ^ Math.round(sv * 9999)));
      return o.map(function (x, i) { return (1 - c) * x + c * f[i]; });
    }
    function reown() {
      var w = weights();
      morphing.forEach(function (r) {
        if (!st.morphOn) { owners[r.key] = st.audible; return; }
        var j = offsets(r.key), best = 0, bv = -1e9;
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
      if (st.modes[key] === 'exempt') return st.corners[0][key];   // MO-12: corner A's value, at every position
      if (r.class === 'S' || st.modes[key] === 'quantum') return st.corners[owners[key]][key];   // MO-11: flips at its boundaries, whatever the law is
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
    function sendModes() { transport.send({ type: 'modes', seq: ++seq, modes: st.modes, counts: st.counts }); }   // IF-12: to the resolver, never the engine
    function apply(s) { st = JSON.parse(s); reown(); send(); sendModes(); rev++; }
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
      if (cur.after === null) cur.after = t.before;   // the root learns its state from the first edit, so a state prepared before then is the start
      var n = { i: nextNode++, parent: cur.i, tick: cur.tick + 1, label: t.label, before: t.before === cur.after ? cur.after : t.before, after: after, last: null };   // the same string, not a copy, when nothing moved between gestures
      nodes.forEach(function (q) { if (q.tick >= n.tick) n.tick = q.tick + 1; });   // ticks only go up, so newest first holds across branches
      nodes.push(n); cur = n; evict(); return true;
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
    D.offsets = offsets;
    /* D.2's settings as the morph layer's views read them, so the shell needs no key of its own (its controls come from shell-data.js). */
    var lawX = function (k) { return root.GUI3Engine.lawX(by[k], D.shown(k)); };
    D.morphSettings = function () { return { law: Math.round(D.shown('morph.law') * 2), cohesion: D.shown('morph.cohesion'), seed: D.shown('morph.seed'), rate: lawX('morph.stepRate'), glide: lawX('morph.glide') }; };
    D.pins = function (key) { return (st.pins[key] || []).map(function (p) { return p.slice(); }); };
    D.setPins = function (key, pins, label) { if (JSON.stringify(st.pins[key] || []) === JSON.stringify(pins)) return false; step(label || 'Waypoints', function () { if (pins.length) st.pins[key] = pins.map(function (p) { return p.slice(); }); else delete st.pins[key]; }); changed(); return true; };
    D.blendSettings = function () { return { law: st.blend.law, clamp: st.blend.clamp }; };
    D.setBlendSettings = function (law, clamp) { if (law === st.blend.law && clamp === st.blend.clamp) return false; step('Waypoint law', function () { st.blend = { law: law, clamp: clamp }; }); changed(); return true; };
    D.setMorphLaw = function (i, label) { return D.set('morph.law', i / 2, { label: label }); };
    D.regions = function () { return st.regions; };
    D.reown = function () { reown(); send(morphing.map(function (r) { return r.key; })); changed(); };
    D.owner = function (key) { var r = by[key]; return !r || r.class === 'D' ? null : owners[key]; };
    D.corner = function (key, i) { var r = by[key]; if (!r) return null; return r.class === 'D' ? st.dev[key] : st.corners[i][key]; };
    /* Write v to one corner: the requested one, else the owner while morph is on; while it is off, only the
       audible corner takes writes, and a write addressed to a frozen corner is refused (null). */
    D.set = function (key, v, o) {
      var r = by[key]; if (!r || typeof v !== 'number' || v !== v) return null;
      v = snap(r, v);
      if (key === 'morph.on') { D.setMorph({ on: v >= 0.5 }); return v; }
      if (key === 'morph.audible') { D.setAudible(Math.round(v * 3)); return v; }
      // An Exempt key has one value, whichever corner a face addresses, so the four never part (MO-12). Morph off's freeze has no corner to protect here.
      if (st.modes[key] === 'exempt') {
        if (st.corners[0][key] === v) return v;
        step((o && o.label) || r.label, function () { for (var c = 0; c < 4; c++) st.corners[c][key] = v; });
        send([key]); changed(); return v;
      }
      var want = o && o.corner !== undefined && o.corner !== null ? o.corner : null, i;
      if (r.class === 'D') i = null;
      else if (!st.morphOn) { if (want !== null && want !== st.audible) return null; i = st.audible; }
      else i = want !== null ? want : owners[key];
      var cur = i === null ? st.dev[key] : st.corners[i][key];
      if (cur === v) return v;
      step((o && o.label) || r.label, function () { if (i === null) st.dev[key] = v; else st.corners[i][key] = v; });
      if (key === 'morph.law' || key === 'morph.cohesion' || key === 'morph.seed') { reown(); send(morphing.map(function (x) { return x.key; })); }   // what's heard and who owns it depend on these
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
    /* MO-11 to MO-13: a key's mode and count. One pick is one undo step, and re-picking makes none. Making a key Exempt writes what's heard now into all four
       corners, so nothing jumps; going back leaves them equal, so nothing jumps then either. Only the mode changes what the stub plays: the count is held and sent. */
    D.mode = function (key) { return st.modes[key] || 'follow'; };
    D.count = function (key) { return st.counts[key] || 0; };
    D.modes = function () { return copy(st.modes); };
    D.setMode = function (key, mode) {
      var r = by[key]; if (!r || r.class === 'D' || ['follow', 'quantum', 'exempt'].indexOf(mode) < 0 || D.mode(key) === mode) return false;
      step('Morph mode: ' + r.label + ', ' + mode, function () {
        if (mode === 'exempt') { var held = heard(key); for (var c = 0; c < 4; c++) st.corners[c][key] = held; }
        if (mode === 'follow') delete st.modes[key]; else st.modes[key] = mode;
      });
      reown(); send([key]); sendModes(); changed(); return true;
    };
    D.setCount = function (key, n) {
      var r = by[key]; if (!r || r.class === 'D' || [0, 2, 4].indexOf(n) < 0 || D.count(key) === n) return false;
      step('Morph count: ' + r.label + ', ' + (n || 'as the morph'), function () { if (n) st.counts[key] = n; else delete st.counts[key]; });
      sendModes(); changed(); return true;
    };
    /* "Save current sound to snapshot" (ADR-319 item 1): what is heard now becomes corner i, named, and saved. */
    D.saveSoundTo = function (i, name) {
      if (!(i >= 0 && i < 4) || !okName(name)) return { ok: false, why: 'a corner A to D and a name of 1 to ' + NAME_MAX + ' characters' };
      step('Save the current sound to ' + CORNER_IDS[i] + ' as ' + name.trim(), function () {
        morphing.forEach(function (r) { st.corners[i][r.key] = heard(r.key); });
        st.names[i] = name.trim(); st.saved[i] = copy(st.corners[i]);
      });
      reown(); send(morphing.map(function (r) { return r.key; })); changed(); return { ok: true };
    };
    /* ADR-320 r4: all four as one set, named together. Each corner's own values, like Save preset. */
    D.saveAll = function (name) {
      if (!okName(name) || name.trim().length > NAME_MAX - 4) return { ok: false, why: 'a name of 1 to ' + (NAME_MAX - 4) + ' characters' };
      step('Save all four corners as ' + name.trim(), function () { for (var i = 0; i < 4; i++) { st.names[i] = name.trim() + ' · ' + CORNER_IDS[i]; st.saved[i] = copy(st.corners[i]); } });
      changed(); return { ok: true };
    };
    /* ADR-320 r4: delete a saved snapshot. The corner keeps its values and goes back to Empty, with nothing to revert to. */
    D.forget = function (i) {
      if (!(i >= 0 && i < 4) || !st.saved[i]) return false;
      step('Delete ' + st.names[i] + ' from corner ' + CORNER_IDS[i], function () { st.names[i] = null; st.saved[i] = null; });
      changed(); return true;
    };
    /* "Save preset to [corner]": corner i's own values become its saved preset, under a name. */
    D.savePreset = function (i, name) {
      if (!(i >= 0 && i < 4) || !okName(name)) return { ok: false, why: 'a corner A to D and a name of 1 to ' + NAME_MAX + ' characters' };
      step('Save corner ' + CORNER_IDS[i] + ' as ' + name.trim(), function () { st.names[i] = name.trim(); st.saved[i] = copy(st.corners[i]); });
      changed(); return { ok: true };
    };
    D.edited = function (i) {
      var s = st.saved[i]; if (!s) return false;
      return morphing.some(function (r) { return st.modes[r.key] !== 'exempt' && st.corners[i][r.key] !== s[r.key]; });
    };
    D.revert = function (i) {
      if (!st.saved[i] || !D.edited(i)) return false;
      step('Revert corner ' + CORNER_IDS[i] + ' to ' + st.names[i], function () { var was = st.corners[i]; st.corners[i] = copy(st.saved[i]); for (var k in st.modes) if (st.modes[k] === 'exempt') st.corners[i][k] = was[k]; });   // an Exempt key keeps its one value
      send(morphing.map(function (r) { return r.key; })); changed(); return true;
    };
    D.sources = function () { return SOURCES; };
    D.routes = function () { return st.routes.map(function (r) { return routeOf(r.id, r); }); };
    /* Why a route cannot exist, or null. A destination is a continuous parameter of a manifest (RC-4): an engine row that
       takes per-voice modulation, or a hosted module's or Arp's continuous row. The morph's settings are the resolver's
       inputs (IF-12), not modulation destinations in any ruling, so they are refused. */
    function routeFault(rt) {
      var r = by[rt && rt.param];
      if (!r || r.law === 'step' || (r.target === 'resolver') || ((r.target || 'engine') === 'engine' && !r.pv)) return (rt && rt.param) + ' is not a continuous parameter a route can reach (RC-4)';
      if (!SOURCES.some(function (s) { return s.id === rt.src; })) return rt.src + ' is not a source the engine publishes (IF-3)';
      var pol = rt.pol || 'uni', depth = rt.depth === undefined ? 0.25 : rt.depth;
      if (POLS.indexOf(pol) < 0 || typeof depth !== 'number' || !(depth >= 0 && depth <= 1)) return 'depth is in the destination’s normalised range, 0 to 1 (RC-4)';
      return null;
    }
    /* A route as stored. A Control (the bundle's `ctl` polarity) also carries the band it sweeps; no other polarity does. */
    function routeOf(id, rt) {
      var o = { id: id, src: rt.src, param: rt.param, pol: rt.pol || 'uni', depth: rt.depth === undefined ? 0.25 : rt.depth };
      if (o.pol === 'ctl' && typeof rt.lo === 'number' && typeof rt.hi === 'number') { o.lo = rt.lo; o.hi = rt.hi; }
      return o;
    }
    D.addRoute = function (rt) {
      var why = routeFault(rt); if (why) return { ok: false, why: why };
      var id = 'r' + st.nextRoute;
      step('Route ' + rt.src + ' → ' + by[rt.param].label, function () { st.routes.push(routeOf(id, rt)); st.nextRoute++; });
      changed(); return { ok: true, id: id };
    };
    /* MAP MODE (ADR-261, the Map Mode spec). The design system's session edits a working list of mappings in place
       (create on click or drag, depth by drag, polarity by P, remove by Backspace); this writes that list into the
       device's matrix. An item with an `id` is an existing route (edited when it differs), one without is a new route,
       and a route absent from the list is removed. The whole difference is ONE history step (the gesture), and a new
       item the matrix refuses (RC-4) is reported by its index and not made. Returns { ids, refused }: ids[i] is item
       i's route id, or null for a refused new item. */
    D.syncRoutes = function (list) {
      var ids = [], refused = [], next = [], dirty = false, label = null, fresh = 0;
      list.forEach(function (rt, i) {
        var old = rt.id ? st.routes.filter(function (x) { return x.id === rt.id; })[0] : null, why = routeFault(rt);
        if (why) { refused.push({ index: i, why: why }); ids.push(old ? old.id : null); if (old) next.push(old); return; }
        if (!old) {
          var id = 'r' + (st.nextRoute + fresh++);
          next.push(routeOf(id, rt)); ids.push(id); dirty = true; label = label || 'Map ' + rt.src + ' → ' + by[rt.param].label; return;
        }
        var n = routeOf(old.id, rt);
        if (JSON.stringify(n) !== JSON.stringify(old)) { dirty = true; label = label || 'Edit route ' + rt.src + ' → ' + by[rt.param].label; }
        next.push(n); ids.push(old.id);
      });
      var kept = next.map(function (x) { return x.id; });
      if (st.routes.some(function (x) { return kept.indexOf(x.id) < 0; })) { dirty = true; label = label || 'Remove route'; }
      if (dirty) { step(label, function () { st.routes = next; st.nextRoute += fresh; }); changed(); }
      return { ids: ids, refused: refused };
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
    /* REMOVAL, M-R2 (ADR-338 r9). A modulator and EVERY route it drives leave as ONE undo step (the host owns the stack, the
       design master's UndoLine offers the inverse). A pending modulator is outside the history, so dropping it is no step;
       the pending entry is spliced whether or not the source also has routes, else removing the routes would let a stale
       `pending` entry show it again as pending. The engine's sources stay in the engine: only the routes and the
       activation go, and + brings it back. Returns the routes that went (for the removal line's text). */
    D.removeModulator = function (src) {
      var s = SOURCES.filter(function (x) { return x.id === src; })[0];
      if (!s) return { ok: false, why: src + ' is not a source the engine publishes (IF-3)' };
      var gone = st.routes.filter(function (r) { return r.src === src; }).map(function (r) { return routeOf(r.id, r); });
      var k = pending.indexOf(src);
      if (!gone.length && k < 0) return { ok: false, why: s.name + ' is not active: no routes and not pending' };
      if (k >= 0) pending.splice(k, 1);
      if (gone.length) step('Remove ' + s.name + ' and its ' + gone.length + (gone.length === 1 ? ' route' : ' routes'), function () {
        st.routes = st.routes.filter(function (r) { return r.src !== src; });
      });
      changed(); return { ok: true, routes: gone, step: gone.length > 0 };
    };
    /* A map handle dropped on a parameter: a route through addRoute, which refuses what RC-4 refuses. A refused
       drop leaves a pending source pending; an accepted one maps it. */
    D.canRoute = function (key) { return !routeFault({ src: SOURCES[0].id, param: key }); };
    D.drop = function (src, key) {
      var so = SOURCES.filter(function (x) { return x.id === src; })[0];
      var res = D.addRoute({ src: src, param: key, pol: (so && so.pol) || 'uni', depth: 0.25 });
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
          if (r.class !== 'M' || st.modes[r.key] === 'exempt') return;
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
        st.routes = []; st.names = [null, null, null, null]; st.saved = [null, null, null, null]; st.modes = {}; st.counts = {};
      });
      reown(); send(); sendModes(); changed();
    };
    /* past: the labels from the start to where you stand. future: the redo chain, the next one last. */
    D.history = function () {
      var fut = []; for (var q = redoChild(cur), g = 0; q && g <= HIST_CAP; q = redoChild(q), g++) fut.unshift(q.label);
      return { past: pathOf(cur).filter(function (q) { return q.before !== null; }).map(function (q) { return q.label; }), future: fut };
    };
    D.tree = function () { return { cur: cur.i, cap: HIST_CAP, nodes: nodes.map(function (q) { return { i: q.i, parent: q.parent, tick: q.tick, label: q.label }; }) }; };
    D.clearRedo = function () {   // the shell's self-checks undo their own probe and leave no redo behind
      var drop = {}, grow = function (n) { childrenOf(n).forEach(function (c) { drop[c.i] = true; grow(c); }); }; grow(cur);
      nodes = nodes.filter(function (q) { return !drop[q.i]; }); cur.last = null;
    };
    D.clearHistory = function () { newRoot(); };   // a review link opens on a prepared state with nothing to undo
    D.undo = function () { var p = cur.parent < 0 ? null : nodeOf(cur.parent); if (tx || !p || cur.before === null) return false; p.last = cur.i; var b = cur.before; cur = p; apply(b); return true; };
    D.redo = function () { var c = redoChild(cur); if (tx || !c) return false; cur = c; apply(c.after); return true; };
    /* Go to any state in the tree (ADR-160: the next edit branches from it). Redo still leads back along the way you came. */
    D.restore = function (i) {
      var n = nodeOf(i); if (tx || !n || n.after === null) return false;
      pathOf(cur).reduce(function (a, b) { if (a) a.last = b.i; return b; }, null);
      cur = n; apply(n.after); return true;
    };
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
    /* GUI-34: what the voices are doing, once per frame: { t: 'vz', peak, act, env, gates, r }, or null with no voices to read. The Specimen strikes from it (AUD.feed).
       The stub plays no notes, so it's null unless a test build hands one in. The engine's snapshot answers this at stage 8 (RC-8, GN-47). */
    D.feed = function () { return window.GUI3_FEED || feedOf(lastSnap && lastSnap.observe); };
    /* B628: the engine's frame (RC-8, as the transport decodes it) as the Specimen's feed. A gated note is a gate, the loudest
       voice's level is the envelope, and R is the first oscillator's of the loudest voice. Null from the stub, and where nothing is reported. */
    function feedOf(o) {
      if (!o || o.source !== 'engine') return null;   // the stub's meters are MOCK: a feed from them would wake every picture with nothing playing
      var gates = [], env = 0, act = 0, r = 0;
      o.voices.forEach(function (v) {
        if (v.notes && v.notes.gate) gates.push(v.notes.key);
        if (v.level > 0 || (v.notes && v.notes.gate)) act++;
        if (v.level > env) { env = v.level; r = v.osc && v.osc[0] ? v.osc[0].R : r; }
      });
      var out = o.meters && o.meters.out, peak = out && out.peak > -Infinity ? Math.pow(10, out.peak / 20) : 0;
      return { t: 'vz', peak: peak, act: act, env: env, gates: gates, r: r };
    }
    /* B628, the plug-in's own messages: which parts of RC-8 to send (`want`), its preset bank (`preset`) and its Voices
       (`plugin`). None is a step in the history: a preset load replaces the engine, and the bridge's state answers for it. */
    D.sides = function () { return transport.sides || { engine: true, resolver: true, device: true }; };
    D.want = function (w) { transport.send({ type: 'want', seq: ++seq, osc: w.osc || [], members: !!w.members, cycles: !!w.cycles }); };
    D.presets = function () { return lastSnap && lastSnap.presets || null; };
    D.loadPreset = function (i) { transport.send({ type: 'preset', seq: ++seq, index: i }); rev++; };
    D.plugin = function () { return lastSnap && lastSnap.plugin || null; };
    D.setVoices = function (n) { transport.send({ type: 'plugin', seq: ++seq, voices: n }); rev++; };
    /* GUI-33: is any voice sounding? The pictures rest while none is. A voice alive, or a peak at or over B365's -90 dBFS, so a release tail counts (GN-45). */
    D.sounding = function () { var f = D.feed(); return !!f && (f.act > 0 || f.peak >= 3.2e-5); };
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
