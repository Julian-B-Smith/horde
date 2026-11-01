/* vendored from the horde Design System artifact, version 1793572935-0ce9, 2026-11-01; do not edit, re-vendor */
/* @ds-bundle: {"format":4,"namespace":"Horde","components":[{"name":"Knob"},{"name":"Slider"},{"name":"MapControl"},{"name":"XYPad"},{"name":"CornerChip"},{"name":"ScreenScheme"},{"name":"SourceStrip"},{"name":"ModMatrixGrid"},{"name":"RouteRow"},{"name":"ModLedger"},{"name":"TraceLanes"},{"name":"CompactWires"},{"name":"HostedMacroRows"},{"name":"ModCardHeader"},{"name":"LfoShapeEditor"},{"name":"EnvelopeEditor"},{"name":"TempoDivisions"},{"name":"CurvePlot"},{"name":"OrbitalCard"},{"name":"MidiTrackers"},{"name":"GateLanes"},{"name":"EventLanes"},{"name":"SmoothingLanes"},{"name":"ShClockCard"},{"name":"OwnershipTier"},{"name":"QuantumPad"},{"name":"PadBars"},{"name":"CrossSection"},{"name":"MorphGrid"},{"name":"FindPane"},{"name":"MorphInspector"},{"name":"KindLegend"},{"name":"SteppedControl"},{"name":"Stepper"},{"name":"Cohesion"},{"name":"FxRack"},{"name":"FxMatrix"},{"name":"FxPanel"},{"name":"FxCables"},{"name":"GainStaging"},{"name":"ChainLine"},{"name":"BlendMaps"},{"name":"PageNav"},{"name":"SectionHeader"},{"name":"ChannelStrip"},{"name":"Companion"},{"name":"SourceChip"},{"name":"SourceRows"},{"name":"RoutesBySource"},{"name":"Cluster"},{"name":"Lens"},{"name":"Fold"},{"name":"PageView"},{"name":"ModTiers"},{"name":"MacroRow"},{"name":"ModBank"},{"name":"ModPage"},{"name":"UndoLine"}]} */
/* horde house reference behaviours (vanilla JS, no React, no network).
   The morph knob and slider follow the human's HORDE Morph Knob Spec and HORDE Map Mode Spec
   (2026-10-21, 2026-10-16); colours, type and themes follow ADR-259. Everything a plugin would get
   from the engine (ownership, source values) is a deterministic stand-in here, labelled as such. */
(function () {
  'use strict';
  var NS = 'http://www.w3.org/2000/svg';
  var CORNERS = [
    { id: 'A', glyph: 'M5 .5 9.5 5 5 9.5 .5 5Z', shape: 'diamond' },
    { id: 'B', glyph: 'M5 1 9.5 9H.5Z', shape: 'triangle' },
    { id: 'C', glyph: 'M5 .7a4.3 4.3 0 1 0 .001 0Z', shape: 'circle' },
    { id: 'D', glyph: 'M1.2 1.2h7.6v7.6H1.2Z', shape: 'square' }
  ];
  /* FOUR POLARITIES (ADR-266 ruling 2): unipolar, bipolar and inverse ADD to the parameter; CONTROL ('ctl') drives
     its position directly between the mapping's lo and hi, ignoring the parameter's own value. At most one Control
     per parameter; uni/bi/inv still add on top of it, clamped once; removing it restores the own value. */
  var POL = ['uni', 'bi', 'inv', 'ctl'], POLN = { uni: 'UNI', bi: 'BI', inv: 'INV', ctl: 'CTL' };
  var DRAG = 160;                                       // size-drag: px for the full range
  var DEF_DEPTH = .25;                                  // a click-made mapping's depth (the Map Mode spec, ADR-261 r3)
  /* pointer capture, guarded: a pointer the browser no longer tracks (a synthetic event, a pen lifted mid-press) throws
     NotFoundError, and an unguarded throw there cost the whole gesture */
  function capture(el, e) { try { el.setPointerCapture(e.pointerId); } catch (_) { /* keep the gesture */ } }
  /* MP-c (ADR-341 r4 as amended by A1): the polarity chord is Shift+Option-click (Shift+Alt on Windows), the same keys on
     every keyboard. Command is not in it: Windows keyboards have no Command key, and Ctrl+Alt is AltGr on many European
     layouts. Command and Ctrl must be up (macOS Ctrl-click is the secondary click). With movement the chord is a fine
     Option-drag, as Shift is fine everywhere. Not verified in a host (Live, Bitwig), nor whether Windows' Shift+Alt
     layout hotkey fires around the click; the chip and the menu stay the routes if either takes it. */
  function chordHit(e) { return e.altKey && e.shiftKey && !e.metaKey && !e.ctrlKey; }
  function clamp(v, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, v)); }
  function v(name) { return 'var(--' + name + ')'; }
  function fillOf(i) { return v('c' + CORNERS[i].id); }
  function lineOf(i) { return v('c' + CORNERS[i].id + '-tx'); }   // a corner as a line or text: its legible ink
  function srcColor(s) { return v('src-' + s.kind); }
  function mk(t, a, parent) { var e = document.createElementNS(NS, t); for (var k in a) e.setAttribute(k, a[k]); if (parent) parent.appendChild(e); return e; }
  function glyphSvg(i, size, color) {
    return '<svg viewBox="0 0 10 10" width="' + size + '" height="' + size + '" aria-hidden="true" style="flex:0 0 auto;fill:' + color + '"><path d="' + CORNERS[i].glyph + '"/></svg>';
  }
  /* PIPS ARE SHAPE-CODED (ADR-261 ruling 1): each corner's pip takes its glyph's shape, A diamond,
     B triangle, C circle, D square (the Design System's glyphs), so corners stay distinct without
     colour even at the smallest weighted size. r is the circle-equivalent radius (equal area). */
  function pip(g, i, cx, cy, r, attrs) {
    var d, k;
    if (i === 2) { attrs.cx = cx; attrs.cy = cy; attrs.r = r; return mk('circle', attrs, g); }
    if (i === 0) { k = r * 1.2533; d = 'M' + cx + ' ' + (cy - k) + 'L' + (cx + k) + ' ' + cy + 'L' + cx + ' ' + (cy + k) + 'L' + (cx - k) + ' ' + cy + 'Z'; }
    else if (i === 3) { k = r * .8862; d = 'M' + (cx - k) + ' ' + (cy - k) + 'H' + (cx + k) + 'V' + (cy + k) + 'H' + (cx - k) + 'Z'; }
    else { k = r * 1.3467; d = 'M' + cx + ' ' + (cy - k * 1.1547) + 'L' + (cx + k) + ' ' + (cy + k * .5774) + 'L' + (cx - k) + ' ' + (cy + k * .5774) + 'Z'; }
    attrs.d = d; attrs['stroke-linejoin'] = 'round'; return mk('path', attrs, g);
  }
  /* the padlock (corner lock) and the revert arrow, drawn as shapes like the glyphs */
  var LOCK = '<svg viewBox="0 0 10 10" width="8" height="8" aria-label="locked" role="img" style="flex:0 0 auto"><path d="M3 4.6V3.3a2 2 0 0 1 4 0v1.3" style="fill:none;stroke:currentColor;stroke-width:1.3"/><rect x="1.8" y="4.6" width="6.4" height="4.6" rx="1" style="fill:currentColor"/></svg>';
  var LOCK_OPEN = '<svg viewBox="0 0 10 10" width="10" height="10" aria-hidden="true"><path d="M3 4.6V3.3a2 2 0 0 1 4 0" style="fill:none;stroke:currentColor;stroke-width:1.3"/><rect x="1.8" y="4.6" width="6.4" height="4.6" rx="1" style="fill:none;stroke:currentColor;stroke-width:1.2"/></svg>';
  var REVERT = '<svg viewBox="0 0 10 10" width="10" height="10" aria-hidden="true"><path d="M2.4 4.4A3.1 3.1 0 1 1 3.2 7.6" style="fill:none;stroke:currentColor;stroke-width:1.3;stroke-linecap:round"/><path d="M.9 2.6 2.6 5 4.6 3.1Z" style="fill:currentColor"/></svg>';
  function pct(x) { return Math.round(x * 100) + '%'; }
  function hash(s) { var h = 2166136261; for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }

  /* ---------- the session: one morph cursor, one MAP state, one keyboard mode ---------- */
  function Session(opts) {
    opts = opts || {};
    var S = this;
    S.pad = { x: opts.x === undefined ? .3 : opts.x, y: opts.y === undefined ? .32 : opts.y };   // y measured downward
    S.pinned = null;                 // null = Auto (each parameter's owner)
    S.map = { on: false, src: (opts.sources && opts.sources[0] && opts.sources[0].id) || null, momentary: false };   // an empty source list is a page with no modulators (a dashboard), not an error
    S.sources = opts.sources || [];
    S.mappings = opts.mappings || []; // { param, src, pol, depth, cd? [4] }
    S.focus = {}; S.lastMap = null; S.alt = false; S.keys = !!opts.keys; S.mapKeysAlways = !!opts.mapKeysAlways;
    S.params = {}; S.selected = null; S.t = 0; S.views = []; S.listeners = [];
    S.reduce = typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;
    S.owner = {}; S.drag = null; S.message = '';
    /* CORNER PRESETS. Each corner shows the ACTUAL name of the preset it holds (never a placeholder);
       null = an empty corner. Locks are per corner and independent: a locked corner still morphs but
       refuses edits. S.saved holds each corner's values as last saved, for revert and the edited mark. */
    S.corners = opts.corners || [null, null, null, null];
    S.locks = [false, false, false, false]; S.saved = {}; S.refused = null;
    /* THE LAYERED ESCAPE (ADR-338 r11): Escape cancels the innermost open thing and nothing else. A host puts its own
       layers (a menu, a map-then-pick, a number entry it owns) IN FRONT with S.escapeLayers.unshift({name, active, cancel}).
       Panic is on no key (ADR-340 r12: ADR-226 item 6 holds, the engine answers the host's all-notes-off). */
    S.escapeLayers = [
      { name: 'map', active: function () { return S.map.on; }, cancel: function () { S.setMap(false); } },
      { name: 'pin', active: function () { return S.pinned !== null; }, cancel: function () { S.setPin(null); } },
      { name: 'keys', active: function () { return S.keys; }, cancel: function () { S.keys = false; S.say('Keyboard mode off'); } }
    ];
  }
  /* one Escape: the first active layer is cancelled; returns its name, or null when nothing was open */
  Session.prototype.escape = function () {
    for (var i = 0; i < this.escapeLayers.length; i++) { var L = this.escapeLayers[i]; if (L.active()) { L.cancel(); this.emit(); return L.name; } }
    return null;
  };
  Session.prototype.weights = function () { var x = this.pad.x, y = this.pad.y; return [(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y]; };
  Session.prototype.src = function (id) { for (var i = 0; i < this.sources.length; i++) if (this.sources[i].id === id) return this.sources[i]; return null; };
  Session.prototype.sourceValue = function (id) { var s = this.src(id); return s ? clamp(s.f(this.t)) : 0; };
  /* OWNERSHIP STAND-IN. The plugin reads the owner from the engine and never computes it (morph knob
     spec, Ties). Here each parameter gets a fixed threshold offset from its key's hash, so owners
     differ per knob and flip at different moments, deterministically. */
  Session.prototype.ownerOf = function (p) {
    var w = this.weights(), best = 0, bv = -1e9, h = hash(p.key), Mo = window.Horde && window.Horde.Morph;
    /* with the morph family in use (S.morph), the morph layer's stand-in answers (seed, Cohesion, locked rows, the
       STEPPED heard position), so a knob and the morph grid always name the same owner; it equals this hash law at
       seed 0, Cohesion 0 and no locks */
    if (this.morph && Mo && Mo.engine) best = Mo.engine.owner(this, p);
    else for (var i = 0; i < 4; i++) { var j = (((h >>> (i * 8)) & 255) / 255 - .5) * .32, s = w[i] + j; if (s > bv) { bv = s; best = i; } }
    var o = this.owner[p.key];
    if (!o) this.owner[p.key] = { i: best, at: -1e9 }; else if (o.i !== best) this.owner[p.key] = { i: best, at: this.now || 0 };
    return best;
  };
  Session.prototype.editing = function (p) {
    if (p.locked) return null;
    if (this.drag && this.drag.key === p.key) return this.drag.ai;          // latched for the gesture
    return this.pinned !== null ? this.pinned : this.ownerOf(p);
  };
  Session.prototype.base = function (p) { if (p.locked) return p.g; var w = this.weights(), b = 0; for (var i = 0; i < 4; i++) b += w[i] * p.vals[i]; return b; };
  Session.prototype.depthOf = function (m) { if (!m.cd) return m.depth; var w = this.weights(), d = 0; for (var i = 0; i < 4; i++) d += w[i] * m.cd[i]; return d; };
  Session.prototype.contrib = function (m) {
    if (m.pol === 'ctl') return 0;                                         // Control sets the position; it never adds
    var s = this.sourceValue(m.src), d = this.depthOf(m);
    return m.pol === 'uni' ? s * d : m.pol === 'bi' ? (2 * s - 1) * d : -s * d;
  };
  Session.prototype.ctlOf = function (key) { var l = this.mapsOf(key); for (var i = 0; i < l.length; i++) if (l[i].pol === 'ctl') return l[i]; return null; };
  /* where the additive mappings start: the Control position when one drives the parameter, else its own (blended) value */
  Session.prototype.origin = function (p) { var c = this.ctlOf(p.key); return c ? c.lo + this.sourceValue(c.src) * (c.hi - c.lo) : this.base(p); };
  /* a mapping's amount as text: a depth, or a Control's swept range */
  Session.prototype.amount = function (m) { return m.pol === 'ctl' ? Math.round(m.lo * 100) + '–' + Math.round(m.hi * 100) + '%' : pct(this.depthOf(m)); };
  Session.prototype.mapsOf = function (key) { return this.mappings.filter(function (m) { return m.param === key; }); };
  Session.prototype.focused = function (key) { var l = this.mapsOf(key); if (!l.length) return null; return l[Math.min(this.focus[key] || 0, l.length - 1)]; };
  Session.prototype.output = function (p) { var r = this.origin(p), S = this; this.mapsOf(p.key).forEach(function (m) { r += S.contrib(m); }); return clamp(r); };
  Session.prototype.say = function (msg) { this.message = msg; this.emit(); };
  Session.prototype.on = function (fn) { this.listeners.push(fn); };
  Session.prototype.emit = function () { for (var i = 0; i < this.listeners.length; i++) this.listeners[i](this); };
  Session.prototype.setPin = function (i) { this.pinned = i; this.say(i === null ? 'Editing corner: Auto (each knob\'s owner)' : 'Editing corner pinned: ' + CORNERS[i].id); };
  Session.prototype.setMap = function (on, momentary) { this.map.on = on; this.map.momentary = !!(on && momentary); this.emit(); };
  Session.prototype.setSrc = function (id) { this.map.src = id; this.emit(); };
  /* UNI → BI → INV → CTL → UNI (`back`: the other way, MP1's shift-click). CTL is skipped while another mapping already
     controls the parameter (one Control per parameter). Entering CTL the first time, the range starts around the current
     value (PLACEHOLDER: value ± depth, at least ±25%; the Map Mode spec does not say where a new Control's range starts). */
  Session.prototype.cyclePol = function (m, back) {
    if (!m) { this.say('No mapping to change'); return; }
    var nx = POL[(POL.indexOf(m.pol) + (back ? 3 : 1)) % 4], other = this.ctlOf(m.param), note = '';
    if (nx === 'ctl' && other && other !== m) { nx = back ? 'inv' : 'uni'; note = ' (CTL skipped: ' + this.src(other.src).name + ' already controls it)'; }
    if (nx === 'ctl' && m.lo === undefined) { var p = this.params[m.param], b = p ? this.base(p) : .5, d = Math.max(.25, this.depthOf(m)); m.lo = clamp(b - d); m.hi = clamp(b + d); }
    m.pol = nx; this.say(this.src(m.src).name + ' → ' + m.param + ' · ' + POLN[m.pol] + note);
  };
  Session.prototype.remove = function (m) {
    if (!m) { this.say('No mapping to remove'); return; }
    var l = this.mapsOf(m.param), idx = l.indexOf(m);
    this.mappings.splice(this.mappings.indexOf(m), 1);
    if ((this.focus[m.param] || 0) >= idx) this.focus[m.param] = Math.max(0, (this.focus[m.param] || 0) - 1);   // focus moves to the previous mapping
    if (this.lastMap === m) this.lastMap = null;
    this.say('Removed ' + this.src(m.src).name + ' → ' + m.param);
  };
  Session.prototype.add = function (view) { this.views.push(view); };
  Session.prototype.edited = function (i) {
    for (var k in this.saved) { var p = this.params[k]; if (p && !p.locked && Math.abs(p.vals[i] - this.saved[k][i]) > 1e-6) return true; }
    return false;
  };
  Session.prototype.revert = function (i) {
    if (this.locks[i]) { this.say('Corner ' + CORNERS[i].id + ' is locked: unlock it to revert'); return; }
    for (var k in this.saved) { var p = this.params[k]; if (p && !p.locked) p.vals[i] = this.saved[k][i]; }
    this.say('Corner ' + CORNERS[i].id + ' reverted to its saved preset' + (this.corners[i] ? ' (' + this.corners[i] + ')' : ''));
  };
  Session.prototype.toggleLock = function (i) { this.locks[i] = !this.locks[i]; this.refused = null; this.say('Corner ' + CORNERS[i].id + (this.locks[i] ? ' locked: edits refused, still morphs' : ' unlocked')); };
  /* THE CENSUS: each corner's share of the parameters it currently owns (or has flipped to). */
  Session.prototype.census = function () {
    var n = [0, 0, 0, 0], tot = 0;
    for (var k in this.params) { var p = this.params[k]; if (p.locked) continue; n[this.ownerOf(p)]++; tot++; }
    return tot ? n.map(function (x) { return x / tot; }) : null;
  };
  /* the parameter's OWN value under a Control mapping is ignored, so writing it is refused and said */
  Session.prototype.ownWritable = function (p, ai) {
    var c = this.ctlOf(p.key);
    if (c) { this.refused = { key: p.key, at: this.now || 0, ai: ai, ctl: true }; this.say(p.name + ' is controlled by ' + this.src(c.src).name + ': its own value is ignored (remove the Control to edit it)'); return false; }
    return this.writable(p, ai);
  };
  /* a write to a corner: refused (and said why) when that corner is locked */
  Session.prototype.writable = function (p, ai) {
    if (p.locked || ai === null || !this.locks[ai]) return true;
    this.refused = { key: p.key, at: this.now || 0, ai: ai };
    this.say('Corner ' + CORNERS[ai].id + (this.corners[ai] ? ' (' + this.corners[ai] + ')' : '') + ' is locked: unlock it to edit');
    return false;
  };
  Session.prototype.start = function () {
    var S = this, t0 = null;
    function frame(now) {
      if (t0 === null) t0 = now;
      requestAnimationFrame(frame);                                       // scheduled FIRST: a throw below can never stop the loop
      S.now = now; S.t = S.reduce ? 0 : Math.max(0, (now - t0) / 1000);   // never a negative step (map mode spec)
      for (var i = 0; i < S.views.length; i++) {
        try { S.views[i].render(); }
        catch (err) { if (!S.views[i].failed) { S.views[i].failed = true; if (window.console) console.error('horde: a view failed and is skipped this frame', err); } }
      }
      Horde.frames = (Horde.frames || 0) + 1;   // a liveness counter the tests read
    }
    requestAnimationFrame(frame);
    /* KEYS: accelerators only, and only in keyboard mode (ADR-259 ruling 6). Every one has a mouse route. */
    var mDown = 0;
    addEventListener('keydown', function (e) {
      if (e.key === 'Alt') { S.alt = true; return; }
      if (e.repeat || (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA' || e.target.isContentEditable))) return;   // a field owns its own keys, Escape included
      if (e.metaKey || e.ctrlKey) return;
      /* THE MAP KEYS (M, 1-9 under MAP, P, Backspace under MAP) and Escape act in keyboard mode (ADR-259 r6), or always when
         the host sets S.mapKeysAlways (GUI 3 does, from ADR-335 r1's one constant, which the human may flip: "I may
         ultimately reverse the keys always decision"). Every other accelerator stays keyboard-mode only. Each has a mouse route. */
      if (!S.keys && !S.mapKeysAlways) return;
      if (e.key === 'Escape') { if (S.escape()) e.preventDefault(); return; }
      if (e.code === 'KeyM') { if (S.map.on && !S.map.momentary) S.setMap(false); else { mDown = e.timeStamp; S.setMap(true, true); } e.preventDefault(); return; }
      var dg = /^Digit([0-9])$/.exec(e.code);
      if (dg && S.map.on) { var n = +dg[1]; if (n >= 1 && n <= S.sources.length) S.setSrc(S.sources[n - 1].id); e.preventDefault(); return; }
      if (e.code === 'KeyP') { S.cyclePol(e.altKey ? S.focused(S.selected) : S.lastMap); e.preventDefault(); return; }
      if ((e.key === 'Backspace' || e.key === 'Delete') && S.map.on) { S.remove(S.lastMap); e.preventDefault(); return; }
      if (!S.keys) return;
      if (dg) { var n2 = +dg[1]; if (n2 === 0) S.setPin(null); else if (n2 <= 4) S.setPin(n2 - 1); e.preventDefault(); return; }
    });
    addEventListener('keyup', function (e) {
      if (e.key === 'Alt') S.alt = false;
      if (e.code === 'KeyM' && S.map.momentary) { if (e.timeStamp - mDown < 280) S.setMap(true, false); else S.setMap(false); }   // tap latches, hold is momentary
    });
    addEventListener('blur', function () { S.alt = false; if (S.map.momentary) S.setMap(false); });
    /* Option held shows the focus caption under every mapped control, read from the pointer too (a host may keep the key events) */
    ['pointermove', 'pointerdown', 'wheel'].forEach(function (ev) { addEventListener(ev, function (e) { S.alt = !!e.altKey; }, { passive: true }); });
  };

  /* ---------- shared gesture: value, depth (Option / map mode), reset ---------- */
  function gesture(target, p, S, axisPx) {
    target.addEventListener('pointerdown', function (e) {
      if (e.button !== 0) return;
      capture(target, e); S.selected = p.key; S.emit();
      var ai = S.editing(p), chord = chordHit(e), st = { key: p.key, ai: ai, x: e.clientX, y: e.clientY, moved: false, mode: 'value', isNew: false, target: null, start: 0, px: axisPx(), mapClick: false, chord: chord, signed: false };
      /* MD1 (ADR-340 r7, amending the Map Mode spec and ADR-261 r3): with MAP on a plain drag moves the VALUE, as with MAP
         off; a click with no movement maps the current source (or focuses its mapping); Option-drag sets the current
         source's depth. The spec's "under MAP a drag sets depth" confused the human: it changed the scale with no modifier. */
      /* MP-c (ADR-341 r4): the chord is Option plus the platform key. A click with no movement CYCLES the polarity of the focused
         mapping (under MAP, the current source's), exactly as a click on its chip; with movement it is the Option-drag it also is.
         It never makes a mapping, so it takes neither MAP branch below. */
      if (chord) {
        var cm = (S.map.on && S.mapsOf(p.key).filter(function (q) { return q.src === S.map.src; })[0]) || S.focused(p.key);
        st.mode = 'alt'; st.target = cm; if (cm) { S.lastMap = cm; st.start = cm.pol === 'ctl' ? cm.lo : cm.cd && ai !== null ? cm.cd[ai] : cm.depth; }
      }
      else if (S.map.on && !e.altKey) { st.mapClick = true; st.start = p.locked ? p.g : p.vals[ai]; }
      else if (S.map.on) {
        var m = S.mapsOf(p.key).filter(function (q) { return q.src === S.map.src; })[0];
        if (!m) { m = { param: p.key, src: S.map.src, pol: 'uni', depth: 0 }; S.mappings.push(m); st.isNew = true; S.focus[p.key] = S.mapsOf(p.key).length - 1; }
        st.mode = 'depth'; st.target = m; st.start = m.pol === 'ctl' ? m.lo : m.cd && ai !== null ? m.cd[ai] : m.depth; S.lastMap = m;
      } else if (e.altKey) {
        st.mode = 'alt'; st.target = S.focused(p.key);
        if (st.target) { S.lastMap = st.target; st.start = st.target.pol === 'ctl' ? st.target.lo : st.target.cd && ai !== null ? st.target.cd[ai] : st.target.depth; }
      } else st.start = p.locked ? p.g : p.vals[ai];
      /* MP3 (ADR-340 r9): an Option-drag on a UNI or INV mapping runs through zero and comes out the other polarity, stored as
         magnitude + polarity (ADR-266 r2: a depth is a magnitude), so the drag's signed value is -1..1. BI and CTL have no
         sign to turn over, and a per-corner depth is edited one corner at a time, so those keep the 0..1 depth drag. Only a
         mapping that existed before this drag turns over (the lab: "⌥drag a UNI mapping"): a NEW one dragged down stays at 0,
         so a drag released at exactly 0 still makes no route (the Map Mode spec; gui3_seam_check's mapdrag). */
      var tg = st.target;
      if (st.mode !== 'value' && !st.isNew && tg && !tg.cd && (tg.pol === 'uni' || tg.pol === 'inv')) { st.signed = true; st.start = (tg.pol === 'inv' ? -1 : 1) * tg.depth; }
      if (st.mode === 'value' && !S.ownWritable(p, ai)) st.mode = 'refused';
      else if (st.mode === 'alt' && st.target && st.target.cd && !S.writable(p, ai)) st.noDrag = true;   // a refused corner depth blocks the DRAG only: an Option-click must still move focus
      S.drag = st;
    });
    target.addEventListener('pointermove', function (e) {
      var st = S.drag; if (!st || st.key !== p.key) return;
      var d = p.horizontal ? e.clientX - st.x : st.y - e.clientY, k = e.shiftKey ? .25 : 1;
      if (Math.abs(d) > 3) st.moved = true; if (!st.moved || st.mode === 'refused' || st.noDrag) return;
      /* RE-ANCHOR (ADR-340 r2): a relative drag that clamps at a stop, or changes its fine/coarse rate, restarts from where
         it is. Without it an overshoot past a stop was a dead range to unwind before the value moved back, and pressing
         Shift mid-drag jumped the value by the rate change. */
      if (st.k !== undefined && st.k !== k) { st.start = st.cur; st.x = e.clientX; st.y = e.clientY; d = 0; }
      st.k = k;
      var raw = st.start + d / st.px * k, nv = st.signed ? clamp(raw, -1, 1) : clamp(raw);
      if (raw !== nv) { st.start = nv; st.x = e.clientX; st.y = e.clientY; }
      st.cur = nv;
      if (st.mode === 'value') { if (p.locked) p.g = nv; else p.vals[st.ai] = nv; }
      else if (st.signed) { st.target.pol = nv < 0 ? 'inv' : 'uni'; st.target.depth = Math.abs(nv); }
      else if (st.target && st.target.pol === 'ctl') { var wd = st.target.hi - st.target.lo; st.target.lo = Math.min(nv, 1 - Math.max(0, wd)); st.target.hi = st.target.lo + wd; }   // a Control's depth gesture slides its whole range (PLACEHOLDER)
      else if (st.target) { if (st.target.cd && st.ai !== null) st.target.cd[st.ai] = nv; else st.target.depth = nv; }
    });
    function end(e) {
      if (e && e.pointerId !== undefined && target.hasPointerCapture && target.hasPointerCapture(e.pointerId)) target.releasePointerCapture(e.pointerId);
      var st = S.drag; if (!st || st.key !== p.key) return; S.drag = null;
      if (!st.moved && st.mapClick) {
        var mm = S.mapsOf(p.key).filter(function (q) { return q.src === S.map.src; })[0];
        if (!mm) { mm = { param: p.key, src: S.map.src, pol: 'uni', depth: DEF_DEPTH }; S.mappings.push(mm); S.say('Mapped ' + S.src(mm.src).name + ' → ' + p.name + ' · UNI · 25%'); }
        else S.say('Focus: ' + S.src(mm.src).name + ' → ' + p.name);
        S.focus[p.key] = S.mapsOf(p.key).indexOf(mm); S.lastMap = mm;
      } else if (!st.moved) {
        if (st.chord) { if (st.target) { S.cyclePol(st.target); S.lastMap = st.target; } else S.say(p.name + ' has no mapping to change'); }
        else if (st.mode === 'depth' && st.isNew) { st.target.depth = DEF_DEPTH; S.say('Mapped ' + S.src(st.target.src).name + ' → ' + p.name + ' · UNI · 25%'); }
        else if (st.mode === 'alt') { var l = S.mapsOf(p.key); if (l.length) { S.focus[p.key] = ((S.focus[p.key] || 0) + 1) % l.length; S.lastMap = S.focused(p.key); S.say('Focus: ' + S.src(S.lastMap.src).name); } else S.say(p.name + ' has no modulators'); }
      } else if (st.mode === 'depth' && st.isNew && st.target.depth === 0) { S.mappings.splice(S.mappings.indexOf(st.target), 1); S.lastMap = null; S.say('Released at 0: no mapping made'); }
      S.emit();
    }
    target.addEventListener('pointerup', end); target.addEventListener('pointercancel', end); target.addEventListener('lostpointercapture', end);
    target.addEventListener('dblclick', function (e) {                       // B449: double-click resets
      if (S.map.on || e.altKey) return;
      var ai = S.editing(p); if (!S.ownWritable(p, ai)) return; if (p.locked) p.g = p.def; else p.vals[ai] = p.def;
    });
    target.addEventListener('wheel', function (e) {                          // B374: alt+wheel only
      if (!e.altKey) return; e.preventDefault();
      /* Option + wheel is ALWAYS the value wheel, mapped control or not (the human, 2026-10-21: "Keep it for value");
         it never moves the focused mapping. Modulators cycle by Option-click or an inspector row only. */
      var ai = S.editing(p), dv = -Math.sign(e.deltaY) * .01; if (!S.ownWritable(p, ai)) return;
      if (p.locked) p.g = clamp(p.g + dv); else p.vals[ai] = clamp(p.vals[ai] + dv);
    }, { passive: false });
    target.addEventListener('keydown', function (e) {                         // arrows: keyboard mode only
      if (!S.keys) return;
      var s = e.shiftKey ? .0025 : .01, ai = S.editing(p), dv = 0;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') dv = s; else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') dv = -s; else return;
      e.preventDefault(); if (!S.ownWritable(p, ai)) return; if (p.locked) p.g = clamp(p.g + dv); else p.vals[ai] = clamp(p.vals[ai] + dv);
    });
  }
  function readout2(S, p, ai, el2) {
    var f = S.focused(p.key);
    if ((S.alt || S.map.on) && f) {
      var l = S.mapsOf(p.key), src = S.src(f.src);
      el2.style.color = srcColor(src);
      el2.innerHTML = src.name + ' ' + POLN[f.pol] + ' ' + S.amount(f) + (l.length > 1 ? ' (' + (l.indexOf(f) + 1) + '/' + l.length + ')' : '');
      return;
    }
    var c = S.ctlOf(p.key);
    if (c) { el2.style.color = srcColor(S.src(c.src)); el2.innerHTML = '<span class="ctl-cap">controlled by ' + S.src(c.src).name + '</span>'; el2.title = 'controlled by ' + S.src(c.src).name; return; }
    el2.title = '';
    if (p.locked) { el2.style.color = v('hint'); el2.textContent = 'LOCKED'; return; }
    var w = S.weights()[ai], rf = S.refused && !S.refused.ctl && S.refused.key === p.key && (S.now || 0) - S.refused.at < 1600;   // a Control refusal is said, not shown as a corner lock
    el2.style.color = lineOf(ai);
    if (rf) { el2.innerHTML = LOCK + '<span>' + CORNERS[ai].id + ' locked</span>'; return; }
    el2.innerHTML = glyphSvg(ai, 7, lineOf(ai)) + '<span>' + CORNERS[ai].id + ' ' + p.fmt(p.vals[ai]) + (w > .999 ? '' : ' · ' + pct(w)) + '</span>' + (S.locks[ai] ? LOCK : '');
  }
  function emphasis(S, m, f) {
    var emph = S.map.on ? (m.src === S.map.src || m === f) : true;
    return { op: emph ? 1 : .3, heavy: m === f && (S.alt || S.map.on) && S.mapsOf(m.param).length > 1 };
  }

  /* ---------- KNOB (morph knob spec; 92-unit viewBox, track radius 22) ---------- */
  var C = 46, R = 22;
  // UI Spec §4: a 280° sweep from 7 o'clock (ruled 2026-10-21; the map spec's 270° is not used)
  function pt(r, x) { var a = (-140 + 280 * x) * Math.PI / 180; return [C + r * Math.sin(a), C - r * Math.cos(a)]; }
  function arc(r, a, b) {
    a = clamp(a); b = clamp(b); if (b < a) { var t = a; a = b; b = t; } if (b - a < .003) return '';
    var p1 = pt(r, a), p2 = pt(r, b);
    return 'M' + p1[0].toFixed(2) + ' ' + p1[1].toFixed(2) + 'A' + r + ' ' + r + ' 0 ' + ((b - a) * 280 > 180 ? 1 : 0) + ' 1 ' + p2[0].toFixed(2) + ' ' + p2[1].toFixed(2);
  }
  function radial(g, x, r1, r2, a) { var p1 = pt(r1, x), p2 = pt(r2, x); a.x1 = p1[0]; a.y1 = p1[1]; a.x2 = p2[0]; a.y2 = p2[1]; a['stroke-linecap'] = 'round'; return mk('line', a, g); }
  /* KNOB GEOMETRY, one per size (the Density ladder, README). CLASSIC is the morph knob spec's drawing, unchanged: a 92-unit
     box that reserves room for the editing flag, four rings 3.7 apart and their corner-depth hairlines, at 68 px (84 big).
     Most of that box is ring room, so its track is only ~33 px across: the human's "way too spacious".
     TIGHT (primary, standard, compact) hugs the track: a 72-unit box, rings 2.4 apart from r 27.5, strokes thickened in
     units so they stay at least ~1 px on screen, pips 1.25× so their SHAPE still reads (ADR-261 r1). It drops, by design:
     the editing flag (the editing pip is already the large ink-edged one), the corner-depth hairline (the inspector and the
     section readout carry it) and the in-drawing "+n" (an HTML badge instead: 7 units would render under the 8 px floor).
     UTILITY (28 px) keeps one ring, the focused mapping's, dashed when there are more. Every size keeps the blend arc, the
     tether, the shape-coded pips sized by weight and the needle at the output. */
  var KG = {
    classic: { v: 92, C: 46, R: 22, tw: 4, cap: 12.5, sh: 2, capW: 1.6, teth: 16.5, tethW: 1.6, ndl: 10.5, ndlW: 2, pipK: 1, pipE: 1.1, ed: 3.9, edW: 1.4,
      flag: true, bip: [25, 28.5], r0: 31, step: 3.7, stepCd: 6.9, rw: 2.2, rwH: 3.2, dot: 2.5, tick: 2.2, rings: 4, hair: true },
    tight: { v: 72, C: 36, R: 21, tw: 5, cap: 12.5, capW: 2.4, teth: 16.6, tethW: 2, ndl: 10.5, ndlW: 2.6, pipK: 1.25, pipE: 1.3, ed: 4.6, edW: 1.8,
      flag: false, bip: [23.6, 26.4], r0: 27.5, step: 2.4, stepCd: 2.4, rw: 1.8, rwH: 2.6, dot: 1.9, tick: 1.3, rings: 4, hair: false },
    utility: { v: 56, C: 28, R: 20, tw: 4.5, cap: 10, capW: 2.4, teth: 14.6, tethW: 2, ndl: 8.5, ndlW: 2.6, pipK: 1, pipE: 1.2, ed: 3.4, edW: 1.6,
      flag: false, bip: [22.4, 24.6], r0: 25.2, step: 0, stepCd: 0, rw: 2, rwH: 2.8, dot: 1.8, tick: 1.2, rings: 1, hair: false }
  };
  /* the ladder's face boxes in px: size-knob-primary / -standard / -compact / -utility (tokens.json) */
  var KSIZE = { primary: 56, standard: 48, compact: 40, utility: 28 };
  /* a sized control's ONE-LINE head: the name at rest; on hover, drag or focus the value in its place; while Option is held or
     MAP is on, a short focus caption in the source colour ("LFO 2 2/3"); a refused write, the padlock and "C locked". The
     long forms ("LFO 2 BI 30% (2/3)", "▲ B 72% · 41%") go to the section's readout slot (H.ReadoutSlot), the title and the
     accessible value text. Returns the mode it drew, for the CSS class. */
  function headCap(S, p, ai, el2) {
    var l = S.mapsOf(p.key), f = S.focused(p.key);
    if ((S.alt || S.map.on) && f) {
      var src = S.src(f.src); el2.style.color = srcColor(src); el2.textContent = src.name + (l.length > 1 ? ' ' + (l.indexOf(f) + 1) + '/' + l.length : ''); return 'cap';
    }
    if (!p.locked && S.refused && !S.refused.ctl && S.refused.key === p.key && (S.now || 0) - S.refused.at < 1600) {
      el2.style.color = lineOf(ai); el2.innerHTML = LOCK + '<span>' + CORNERS[ai].id + ' locked</span>'; return 'cap';
    }
    el2.textContent = ''; return '';
  }
  /* HOT: the control under the pointer or keyboard focus, for the section readout slot. Plain fields, no emit: the slot is
     a view and reads them on its next frame. */
  function hot(el, p, S) {
    function on() { S.hot = p.key; S.hotEl = el; }
    function off() { if (S.hot === p.key) { S.hot = null; S.hotEl = null; } }
    el.addEventListener('pointerenter', on); el.addEventListener('pointerleave', off);
    el.addEventListener('focusin', on); el.addEventListener('focusout', off);
  }
  /* ---------- THE TINY TIER (ADR-338 r1 and r3, drawn as ADR-340 r1 and r2; source: docs/design/composition-lab-r2.html) ----------
     WHAT TRAVELS END TO END. A drag writes ONE corner (the morph knob spec), so the blend moves only by that corner's
     weight times the drag: round 1's tiny knob drew only the blend, so nothing on its face crossed the sweep under the hand
     and it "dragged arbitrarily" (the human). T2d and TS1a/TS2a put the editing corner's value back as a SEE-THROUGH mark:
     a hole cut through the track and the arc (an SVG mask, so the hole shows whatever is behind the control, never a
     painted copy of a background colour), rimmed in the corner's ink, between two ink end stops. The arc or fill stays the
     blend you hear, the needle (or the lane's dot) the output. The label line never changes (ADR-340 r4, H1): the value
     goes to the section's fixed readout slot (H.ReadoutSlot) and the info box, never into the cell. */
  var TMID = 0;
  function seeThrough(svg, W, Hh, hole) {
    var id = 'hdt' + (TMID++), defs = mk('defs', {}, svg), m = mk('mask', { id: id, maskUnits: 'userSpaceOnUse', x: -4, y: -4, width: W + 8, height: Hh + 8 }, defs);
    mk('rect', { x: -4, y: -4, width: W + 8, height: Hh + 8, fill: '#fff' }, m);
    var hl = hole === 'rect' ? mk('rect', { x: 0, y: 0, width: 0, height: 0, rx: .8, fill: '#000' }, m) : mk('circle', { cx: 0, cy: 0, r: 0, fill: '#000' }, m);
    return { g: mk('g', { mask: 'url(#' + id + ')' }, svg), hl: hl };
  }
  /* the one ring a tiny control draws: the focused mapping, stacked on the rings inside it (the utility knob's rule);
     dashed when more mappings exist than the one drawn */
  function ringOf(S, p) {
    var l = S.mapsOf(p.key); if (!l.length) return null;
    var c = S.ctlOf(p.key); if (c) l = [c].concat(l.filter(function (m) { return m !== c; }));
    var f = S.focused(p.key) || l[0], run = S.origin(p), k;
    for (k = 0; k < l.indexOf(f); k++) run += S.contrib(l[k]);
    var d = S.depthOf(f), orig = f.pol === 'ctl' ? f.lo : run, src = S.src(f.src);
    var ends = f.pol === 'ctl' ? [f.lo, f.hi] : f.pol === 'uni' ? [orig, orig + d] : f.pol === 'bi' ? [orig - d, orig + d] : [orig - d, orig];
    return { f: f, n: l.length, ends: ends, orig: orig, live: run + S.contrib(f), color: srcColor(src), op: emphasis(S, f, f).op };
  }
  function tinyHead(el, p) {
    var nm = document.createElement('div'); nm.className = 'mk-name'; nm.textContent = p.abbr || p.name; el.appendChild(nm);
    el.title = p.path || p.name; return nm;
  }
  /* THE CORNER MARK, one for the tiny knob (T2d) and the level cell (T4c-f), ADR-341 r1: a hollow, see-through circle at the
     editing corner's value, a hole of radius 2.1 with a 0.9 px rim at 1.65, in the corner's legible -tx ink. The ink, not the
     fill: on the light panel corner fills A, C and D reach only 1.3 to 1.8 : 1 and the human found T4c's fill-coloured
     triangle "barely visible", while every corner's -tx ink reaches about 4.9 : 1 (composition lab round 3). The hole is cut
     in the face's mask (seeThrough) and the rim drawn on top. set(x, y, ink, on): on = false hides it (a global parameter). */
  var MARK = { hr: 2.1, rim: 1.65, w: .9 };
  function cornerMark(svg, ms) {
    var rim = mk('circle', { cx: 0, cy: 0, r: 0, fill: 'none', 'stroke-width': MARK.w }, svg);
    return { set: function (x, y, ink, on) {
      ms.hl.setAttribute('cx', x); ms.hl.setAttribute('cy', y); ms.hl.setAttribute('r', on ? MARK.hr : 0);
      rim.setAttribute('cx', x); rim.setAttribute('cy', y); rim.setAttribute('r', on ? MARK.rim : 0); rim.setAttribute('stroke', ink);
    } };
  }
  /* T2d: a 20 px face (size-knob-tiny) in a 28 px hit cell (size-hit), 160 px for the full range (size-drag) */
  var T2D = { face: 20, r: 6.2, tw: 2.2, ndl: 4.3, ndlW: 1.4, cap: 2.3, capW: 1.3, ring: 9, rw: 1.5, tick: 1.1 };
  function TinyKnob(el, p, S) {
    var G = T2D, f = G.face, c = f / 2, from = p.bipolar ? .5 : 0;
    el.classList.add('hd-mk', 'sz', 'sz-tiny'); el.innerHTML = '';
    if (p.label !== false) tinyHead(el, p); else el.title = p.path || p.name;
    var box = document.createElement('div'); box.className = 'mk-face hd-focus'; box.setAttribute('role', 'slider');
    box.setAttribute('aria-label', p.path || p.name); box.setAttribute('aria-valuemin', '0'); box.setAttribute('aria-valuemax', '1'); el.appendChild(box);
    var svg = mk('svg', { viewBox: '0 0 ' + f + ' ' + f, width: f, height: f, 'aria-hidden': 'true' }); box.appendChild(svg);
    function P(r, x) { var a = (-140 + 280 * x) * Math.PI / 180; return [c + r * Math.sin(a), c - r * Math.cos(a)]; }
    function A(r, a, b) { a = clamp(a); b = clamp(b); if (b < a) { var t = a; a = b; b = t; } if (b - a < .004) return '';
      var p1 = P(r, a), p2 = P(r, b); return 'M' + p1[0].toFixed(2) + ' ' + p1[1].toFixed(2) + 'A' + r + ' ' + r + ' 0 ' + ((b - a) * 280 > 180 ? 1 : 0) + ' 1 ' + p2[0].toFixed(2) + ' ' + p2[1].toFixed(2); }
    function R(x, r1, r2, at) { var a = P(r1, x), b = P(r2, x); at.x1 = a[0]; at.y1 = a[1]; at.x2 = b[0]; at.y2 = b[1]; at['stroke-linecap'] = 'round'; return mk('line', at, svg); }
    function setR(e, x, r1, r2) { var a = P(r1, x), b = P(r2, x); e.setAttribute('x1', a[0]); e.setAttribute('y1', a[1]); e.setAttribute('x2', b[0]); e.setAttribute('y2', b[1]); }
    var ms = seeThrough(svg, f, f, 'circle'), tg = ms.g;
    mk('path', { d: A(G.r, 0, 1), fill: 'none', stroke: v('well-alt'), 'stroke-width': G.tw, 'stroke-linecap': 'round' }, tg);
    if (p.bipolar) { var q1 = P(G.r - 1.6, .5), q2 = P(G.r + 1.6, .5); mk('line', { x1: q1[0], y1: q1[1], x2: q2[0], y2: q2[1], stroke: v('ink'), 'stroke-width': .9 }, tg); }
    var arcE = mk('path', { d: '', fill: 'none', stroke: v('ink'), 'stroke-width': G.tw, 'stroke-linecap': 'round' }, tg);
    var mark = cornerMark(svg, ms);
    R(0, G.r - 1.7, G.r + 1.9, { stroke: v('ink'), 'stroke-width': .9 }); R(1, G.r - 1.7, G.r + 1.9, { stroke: v('ink'), 'stroke-width': .9 });   // the end stops
    var ring = mk('path', { d: '', fill: 'none', 'stroke-width': G.rw, 'stroke-linecap': 'round' }, svg);
    var tick = mk('line', { x1: c, y1: c, x2: c, y2: c, stroke: v('ink'), 'stroke-width': .9, 'stroke-linecap': 'round' }, svg);
    var cap = mk('circle', { cx: c, cy: c, r: G.cap, fill: v('panel'), 'stroke-width': G.capW }, svg);
    var ndl = mk('line', { x1: c, y1: c, x2: c, y2: c, stroke: v('ink'), 'stroke-width': G.ndlW, 'stroke-linecap': 'round' }, svg);
    hot(el, p, S); gesture(box, p, S, function () { return DRAG; });
    var last = '';
    var view = { render: function () {
      var ai = S.editing(p), b = S.base(p), out = S.output(p), rg = ringOf(S, p), av = p.locked ? b : p.vals[ai], ctl = S.ctlOf(p.key);
      var sig = [b.toFixed(4), out.toFixed(4), av.toFixed(4), ai, rg ? rg.ends[0].toFixed(3) + rg.ends[1].toFixed(3) + rg.live.toFixed(3) + rg.op + rg.n + rg.f.src : '', S.selected === p.key, S.map.on, S.keys, !!ctl].join('|');
      if (sig === last) return; last = sig;
      box.tabIndex = S.keys ? 0 : -1; el.classList.toggle('sel', S.selected === p.key); el.dataset.selc = ai === null ? '' : CORNERS[ai].id;
      box.style.cursor = S.map.on ? 'copy' : ctl ? 'not-allowed' : 'ns-resize';
      arcE.setAttribute('d', A(G.r, from, b)); arcE.setAttribute('opacity', ctl ? .22 : 1);
      var ink = p.locked ? v('ink') : lineOf(ai), q = P(G.r, av);
      mark.set(q[0], q[1], ink, !p.locked);
      if (rg) { ring.setAttribute('d', A(G.ring, rg.ends[0], rg.ends[1]) || A(G.ring, rg.orig, rg.orig + .006)); ring.setAttribute('stroke', rg.color); ring.setAttribute('opacity', rg.op);
        ring.setAttribute('stroke-dasharray', rg.n > 1 ? '2.2 1.2' : 'none'); setR(tick, rg.live, G.ring - G.tick, G.ring + G.tick); tick.setAttribute('opacity', rg.op); }
      else { ring.setAttribute('d', ''); tick.setAttribute('x1', c); tick.setAttribute('x2', c); tick.setAttribute('y1', c); tick.setAttribute('y2', c); }
      cap.setAttribute('stroke', ink);
      var n = P(G.ndl, out); ndl.setAttribute('x2', n[0]); ndl.setAttribute('y2', n[1]);
      box.setAttribute('aria-valuenow', out.toFixed(3));
      box.setAttribute('aria-valuetext', p.fmt(out) + (ctl ? ', controlled by ' + S.src(ctl.src).name : p.locked ? ', locked' : ', editing corner ' + CORNERS[ai].id + ' ' + p.fmt(av)) + (rg ? ', ' + rg.n + ' mapping' + (rg.n > 1 ? 's' : '') : ''));
    } };
    S.add(view); return view;
  }
  /* T4c-f, THE LEVEL CELL (ADR-340 r3 taking T4c, amended by ADR-341 r1; Knob `size: 'level'`; source: composition-lab-r2 T4c,
     composition-lab-r3 T4c-f). NEW SIZE, not TS2a: TS2a (Slider size 'cell') is the 16 px-tall horizontal VALUE cell of a table
     row, with its number in it; T4c is a 16 × 16 vertical LEVEL in a 28 px hit cell under a 4-character label, the tiny
     knob's sibling for a cluster (ADR-338 r4), dragged up and down like a knob. The fill is the blend, ink caps stop both ends,
     the focused mapping's lane runs down the left edge (the output a tick across it), and the editing corner is the corner
     mark, centred on the level's right edge: the same mark as T2d's pip, so the cell reads as the knob does. */
  function TinyLevel(el, p, S) {
    var from = p.bipolar ? .5 : 0, W = 16;
    el.classList.add('hd-mk', 'sz', 'sz-tiny', 'sz-level'); el.innerHTML = '';
    if (p.label !== false) tinyHead(el, p); else el.title = p.path || p.name;
    var box = document.createElement('div'); box.className = 'mk-face hd-focus'; box.setAttribute('role', 'slider');
    box.setAttribute('aria-label', p.path || p.name); box.setAttribute('aria-valuemin', '0'); box.setAttribute('aria-valuemax', '1'); el.appendChild(box);
    var svg = mk('svg', { viewBox: '0 0 ' + W + ' ' + W, width: W, height: W, 'aria-hidden': 'true' }); box.appendChild(svg);
    var Y = function (x) { return 15.4 - 14.8 * clamp(x); };
    var ms = seeThrough(svg, W, W, 'circle'), tg = ms.g;
    mk('rect', { x: 2, y: 0, width: 12, height: 1.3, rx: .5, fill: v('ink') }, tg); mk('rect', { x: 2, y: 14.7, width: 12, height: 1.3, rx: .5, fill: v('ink') }, tg);   // the end stops
    mk('rect', { x: 3.5, y: .6, width: 9, height: 14.8, rx: 2, fill: v('well-alt'), stroke: v('ink'), 'stroke-width': 1 }, tg);
    var fill = mk('rect', { x: 4, y: 8, width: 8, height: 7, rx: 1.4, fill: v('ink') }, tg);
    var mark = cornerMark(svg, ms);
    var lane = mk('rect', { x: .2, y: 2, width: 1.8, height: 4, rx: .9, fill: 'transparent' }, svg);
    var outl = mk('line', { x1: 0, x2: 3.4, y1: 8, y2: 8, stroke: 'transparent', 'stroke-width': 1.4 }, svg);
    hot(el, p, S); gesture(box, p, S, function () { return DRAG; });
    var last = '';
    var view = { render: function () {
      var ai = S.editing(p), b = S.base(p), out = S.output(p), rg = ringOf(S, p), av = p.locked ? b : p.vals[ai], ctl = S.ctlOf(p.key);
      var sig = [b.toFixed(4), out.toFixed(4), av.toFixed(4), ai, rg ? rg.ends[0].toFixed(3) + rg.ends[1].toFixed(3) + rg.op + rg.n + rg.f.src : '', S.selected === p.key, S.map.on, S.keys, !!ctl].join('|');
      if (sig === last) return; last = sig;
      box.tabIndex = S.keys ? 0 : -1; el.classList.toggle('sel', S.selected === p.key); el.dataset.selc = ai === null ? '' : CORNERS[ai].id;
      box.style.cursor = S.map.on ? 'copy' : ctl ? 'not-allowed' : 'ns-resize';
      var top = Math.min(Y(b), Y(from)), bot = from === 0 ? 14.9 : Math.max(Y(b), Y(from));
      fill.setAttribute('y', top); fill.setAttribute('height', Math.max(.5, bot - top)); fill.setAttribute('opacity', ctl ? .22 : 1);
      if (rg) { var y1 = Y(Math.max(rg.ends[0], rg.ends[1])), y2 = Y(Math.min(rg.ends[0], rg.ends[1]));
        lane.setAttribute('y', y1); lane.setAttribute('height', Math.max(1, y2 - y1)); lane.setAttribute('fill', rg.color); lane.setAttribute('opacity', rg.op);
        outl.setAttribute('y1', Y(out)); outl.setAttribute('y2', Y(out)); outl.setAttribute('stroke', rg.color); }
      else { lane.setAttribute('fill', 'transparent'); outl.setAttribute('stroke', 'transparent'); }
      mark.set(12.5, Y(av), p.locked ? v('ink') : lineOf(ai), !p.locked);
      box.setAttribute('aria-valuenow', out.toFixed(3));
      box.setAttribute('aria-valuetext', p.fmt(out) + (ctl ? ', controlled by ' + S.src(ctl.src).name : p.locked ? ', locked' : ', editing corner ' + CORNERS[ai].id + ' ' + p.fmt(av)) + (rg ? ', ' + rg.n + ' mapping' + (rg.n > 1 ? 's' : '') : ''));
    } };
    S.add(view); return view;
  }
  /* TS1a (size 'tiny': a 20 px row for route depths and amounts) and TS2a (size 'cell': a 16 px table value cell). Both are
     a named exception to size-hit on their cross axis (ADR-338 r3) and drag 1:1 with their own drawn track, measured at
     the pointer-down, re-anchoring at the stops (ADR-340 r2). TS1a: a hollow see-through handle at the editing corner,
     the fill the blend, ink stops at both ends. TS2a: the fill the blend, the editing corner a see-through cut with a tick
     on the top edge in its ink, and the output's value in the cell. */
  function TinySlider(el, p, S) {
    var cell = p.size === 'cell', W = p.width || (cell ? 76 : 64), Hh = cell ? 16 : 20, from = p.bipolar ? .5 : 0;
    p.horizontal = true;
    el.classList.add(cell ? 'hd-ts2' : 'hd-ts1', 'hd-focus'); el.innerHTML = ''; el.style.width = W + 'px';
    el.setAttribute('role', 'slider'); el.setAttribute('aria-label', p.path || p.name); el.setAttribute('aria-valuemin', '0'); el.setAttribute('aria-valuemax', '1'); el.title = p.path || p.name;
    var svg = mk('svg', { viewBox: '0 0 ' + W + ' ' + Hh, width: W, height: Hh, 'aria-hidden': 'true' }); el.appendChild(svg);
    var X = cell ? function (x) { return W * clamp(x); } : function (x) { return 2 + (W - 4) * clamp(x); };
    var ms = seeThrough(svg, W, Hh, 'rect'), tg = ms.g, fill, edge, tri, ho, lane, outM, vEl = null;
    if (cell) {
      fill = mk('rect', { x: 0, y: 0, width: 0, height: Hh, fill: v('well-alt') }, tg);
      edge = mk('line', { x1: 0, x2: 0, y1: 0, y2: Hh, stroke: v('ink'), 'stroke-width': 1 }, tg);
      tri = mk('path', { d: '', stroke: 'none' }, svg);
      lane = mk('line', { x1: 0, x2: 0, y1: Hh - 1, y2: Hh - 1, stroke: 'transparent', 'stroke-width': 2 }, svg);
      outM = mk('line', { x1: 0, x2: 0, y1: 4, y2: Hh, stroke: 'transparent', 'stroke-width': 1.4 }, svg);
      vEl = document.createElement('span'); vEl.className = 'v'; el.appendChild(vEl);
    } else {
      mk('rect', { x: 2, y: 8.5, width: W - 4, height: 3, rx: 1.5, fill: v('well-alt'), stroke: v('ink'), 'stroke-width': .8 }, tg);
      if (p.bipolar) mk('line', { x1: W / 2, x2: W / 2, y1: 6, y2: 14, stroke: v('ink'), 'stroke-width': .8 }, tg);
      fill = mk('rect', { x: 2, y: 8.5, width: 0, height: 3, rx: 1.5, fill: v('ink') }, tg);
      mk('line', { x1: 1, x2: 1, y1: 5, y2: 15, stroke: v('ink'), 'stroke-width': 1 }, svg); mk('line', { x1: W - 1, x2: W - 1, y1: 5, y2: 15, stroke: v('ink'), 'stroke-width': 1 }, svg);
      lane = mk('line', { x1: 0, x2: 0, y1: 16.5, y2: 16.5, stroke: 'transparent', 'stroke-width': 1.6, 'stroke-linecap': 'round' }, svg);
      ho = mk('rect', { x: 0, y: 3.6, width: 3.6, height: 12.8, rx: 1, fill: 'none', 'stroke-width': 1.1 }, svg);
      outM = mk('circle', { cx: 0, cy: 16.5, r: 0, fill: 'transparent' }, svg);
    }
    hot(el, p, S); gesture(el, p, S, function () { return Math.max(8, el.getBoundingClientRect().width - (cell ? 0 : 4)); });
    var last = '';
    var view = { render: function () {
      var ai = S.editing(p), b = S.base(p), out = S.output(p), rg = ringOf(S, p), av = p.locked ? b : p.vals[ai], ctl = S.ctlOf(p.key);
      var sig = [b.toFixed(4), out.toFixed(4), av.toFixed(4), ai, rg ? rg.ends.join() + rg.op : '', S.keys, S.map.on, S.selected === p.key, !!ctl].join('|');
      if (sig === last) return; last = sig;
      el.tabIndex = S.keys ? 0 : -1; el.classList.toggle('sel', S.selected === p.key);
      el.style.cursor = S.map.on ? 'copy' : ctl ? 'not-allowed' : 'ew-resize';
      fill.setAttribute('x', Math.min(X(from), X(b))); fill.setAttribute('width', Math.abs(X(b) - X(from))); fill.setAttribute('opacity', ctl ? .22 : 1);
      var ink = p.locked ? v('ink') : lineOf(ai), x = X(av), on = p.locked ? 0 : 1;
      var lo = rg ? X(Math.min(rg.ends[0], rg.ends[1])) : 0, hi = rg ? X(Math.max(rg.ends[0], rg.ends[1])) : 0;
      if (cell) {
        edge.setAttribute('x1', X(b)); edge.setAttribute('x2', X(b));
        ms.hl.setAttribute('x', x - 1.1); ms.hl.setAttribute('width', 2.2 * on); ms.hl.setAttribute('y', -1); ms.hl.setAttribute('height', Hh + 2);
        tri.setAttribute('d', on ? 'M' + (x - 2.6).toFixed(2) + ' 0L' + (x + 2.6).toFixed(2) + ' 0L' + x.toFixed(2) + ' 3.6Z' : ''); tri.setAttribute('fill', ink);
        vEl.textContent = p.fmt(out);
        if (rg) { lane.setAttribute('x1', lo); lane.setAttribute('x2', hi); lane.setAttribute('stroke', rg.color); lane.setAttribute('opacity', rg.op);
          outM.setAttribute('x1', X(out)); outM.setAttribute('x2', X(out)); outM.setAttribute('stroke', rg.color); }
        else { lane.setAttribute('stroke', 'transparent'); outM.setAttribute('stroke', 'transparent'); }
      } else {
        ms.hl.setAttribute('x', x - 2.2); ms.hl.setAttribute('width', 4.4 * on); ms.hl.setAttribute('y', 3); ms.hl.setAttribute('height', 14);
        ho.setAttribute('x', x - 1.8); ho.setAttribute('stroke', ink); ho.setAttribute('opacity', on);
        if (rg) { lane.setAttribute('x1', lo); lane.setAttribute('x2', hi); lane.setAttribute('stroke', rg.color); lane.setAttribute('opacity', rg.op);
          outM.setAttribute('cx', X(out)); outM.setAttribute('r', 1.6); outM.setAttribute('fill', rg.color); }
        else { lane.setAttribute('stroke', 'transparent'); outM.setAttribute('r', 0); }
      }
      el.setAttribute('aria-valuenow', out.toFixed(3));
      el.setAttribute('aria-valuetext', p.fmt(out) + (ctl ? ', controlled by ' + S.src(ctl.src).name : p.locked ? ', locked' : ', editing corner ' + CORNERS[ai].id + ' ' + p.fmt(av)));
    } };
    S.add(view); return view;
  }
  function Knob(el, p, S) {
    p.fmt = p.fmt || pct; p.def = p.def === undefined ? (p.locked ? p.g : .5) : p.def;
    S.params[p.key] = p; if (!p.locked) S.saved[p.key] = p.vals.slice();
    if (p.size === 'tiny') return TinyKnob(el, p, S);   // T2d (ADR-340 r1)
    if (p.size === 'level') return TinyLevel(el, p, S);   // T4c-f (ADR-340 r3, ADR-341 r1)
    var sz = KSIZE[p.size] ? p.size : null, G = sz ? KG[sz === 'utility' ? 'utility' : 'tight'] : KG.classic, Cc = G.C;
    el.classList.add('hd-mk'); if (sz) { el.classList.add('sz', 'sz-' + sz); }
    el.innerHTML = '';
    var box = document.createElement('div'); box.className = 'mk-face hd-focus'; box.setAttribute('role', 'slider');
    box.setAttribute('aria-label', p.path || p.name); box.setAttribute('aria-valuemin', '0'); box.setAttribute('aria-valuemax', '1');
    var size = sz ? KSIZE[sz] : p.big ? 84 : 68;
    var svg = mk('svg', { viewBox: '0 0 ' + G.v + ' ' + G.v, width: size, height: size, 'aria-hidden': 'true' }); box.appendChild(svg);
    var g = mk('g', {}, svg);
    var nm = document.createElement('div'); nm.className = 'mk-name'; nm.textContent = p.name;
    var r1 = document.createElement('div'); r1.className = 'mk-v1';
    var r2 = document.createElement('div'); r2.className = 'mk-v2';
    var more = null;
    if (!sz) { el.appendChild(box); el.appendChild(nm); el.appendChild(r1); el.appendChild(r2); }
    else {
      /* label ABOVE the face (the SCALPEL bench); the value and the caption overlay the label's own line, so nothing moves.
         A primary control also keeps its value under the face, always (a macro's number matters at a glance). */
      var head = document.createElement('div'); head.className = 'mk-head'; head.appendChild(nm); head.appendChild(r2);
      if (sz !== 'primary') head.appendChild(r1);
      el.appendChild(head); el.appendChild(box); if (sz === 'primary') el.appendChild(r1);
      more = document.createElement('span'); more.className = 'mk-more'; box.appendChild(more);
      el.title = p.path || p.name;
    }
    hot(el, p, S);
    function P(r, x) { var a = (-140 + 280 * x) * Math.PI / 180; return [Cc + r * Math.sin(a), Cc - r * Math.cos(a)]; }
    function A(r, a, b) {
      a = clamp(a); b = clamp(b); if (b < a) { var t = a; a = b; b = t; } if (b - a < .003) return '';
      var p1 = P(r, a), p2 = P(r, b);
      return 'M' + p1[0].toFixed(2) + ' ' + p1[1].toFixed(2) + 'A' + r + ' ' + r + ' 0 ' + ((b - a) * 280 > 180 ? 1 : 0) + ' 1 ' + p2[0].toFixed(2) + ' ' + p2[1].toFixed(2);
    }
    function RAD(gg, x, a1, a2, at) { var p1 = P(a1, x), p2 = P(a2, x); at.x1 = p1[0]; at.y1 = p1[1]; at.x2 = p2[0]; at.y2 = p2[1]; at['stroke-linecap'] = 'round'; return mk('line', at, gg); }
    var shU = G.sh || 1.5 * G.v / size;                                     // the house 1.5 px hard shadow, in this drawing's units
    gesture(box, p, S, function () { return DRAG; });
    var view = { render: function () {
      box.tabIndex = S.keys ? 0 : -1;
      box.style.cursor = S.map.on ? 'copy' : 'ns-resize';
      var ai = S.editing(p), b = S.base(p), w = S.weights(), out = S.output(p), drag = S.drag && S.drag.key === p.key;
      el.classList.toggle('sel', S.selected === p.key); el.dataset.selc = ai === null ? '' : CORNERS[ai].id;
      g.innerHTML = '';
      mk('path', { d: A(G.R, 0, 1), fill: 'none', stroke: v('well-alt'), 'stroke-width': G.tw, 'stroke-linecap': 'round' }, g);
      if (p.bipolar) RAD(g, .5, G.bip[0], G.bip[1], { stroke: v('ink'), 'stroke-width': 1.4 });
      /* UNDER CONTROL the knob's own value is ignored: its arc, pips, tether and flag fade to a ghost (kept, since
         removing the Control restores them), and the Control ring below shows the range the source sweeps */
      var ctl = S.ctlOf(p.key), gOwn = ctl ? mk('g', { opacity: .22 }, g) : g;
      var from = p.bipolar ? .5 : 0, va = A(G.R, from, b);
      if (va) mk('path', { d: va, fill: 'none', stroke: v('ink'), 'stroke-width': G.tw, 'stroke-linecap': 'round' }, gOwn);
      if (!p.locked) { var g0 = g; g = gOwn;
        var av = p.vals[ai], td = A(G.teth, b, av);
        if (td) mk('path', { d: td, fill: 'none', stroke: lineOf(ai), 'stroke-width': G.tethW, 'stroke-dasharray': '2 2.4', 'stroke-linecap': 'round' }, g);
        for (var i = 0; i < 4; i++) {
          if (i === ai || w[i] < .0005) continue;                        // zero weight: the pip disappears
          var q = P(G.R, p.vals[i]);
          pip(g, i, q[0], q[1], (1.3 + 2.3 * w[i]) * G.pipK, { fill: fillOf(i), stroke: v('mark-edge'), 'stroke-width': G.pipE, opacity: (.28 + .72 * w[i]).toFixed(2) });
        }
        var e = P(G.R, av);
        pip(g, ai, e[0], e[1], G.ed, { fill: fillOf(ai), stroke: v('ink'), 'stroke-width': G.edW });
        if (G.flag) RAD(g, av, 25.2, 27.6, { stroke: lineOf(ai), 'stroke-width': 1.8 });
        g = g0;
      }
      var l = S.mapsOf(p.key), f = S.focused(p.key), run = S.origin(p), rr = G.r0;
      if (ctl) l = [ctl].concat(l.filter(function (m) { return m !== ctl; }));   // the Control ring is innermost: the others stack on its output
      var shown = G.rings === 1 ? (f ? [f] : l.slice(0, 1)) : l.slice(0, G.rings);   // utility: the focused mapping's ring only
      if (G.rings === 1 && f && l[0] !== f) { var k0 = l.indexOf(f); for (var kk = 0; kk < k0; kk++) run += S.contrib(l[kk]); }   // its stacked origin is still the output of the rings inside it
      shown.forEach(function (m) {
        var d = S.depthOf(m), orig = run, src = S.src(m.src), em = emphasis(S, m, f); run += S.contrib(m);
        if (m.pol === 'ctl') orig = m.lo;                                    // its origin dot sits at the low end; the tick is the live position
        var ends = m.pol === 'ctl' ? [m.lo, m.hi] : m.pol === 'uni' ? [orig, orig + d] : m.pol === 'bi' ? [orig - d, orig + d] : [orig - d, orig];
        var ad = A(rr, ends[0], ends[1]) || A(rr, orig, orig + .004), ra = { d: ad, fill: 'none', stroke: srcColor(src), 'stroke-width': em.heavy ? G.rwH : G.rw, 'stroke-linecap': 'round', opacity: em.op };
        if (G.rings === 1 && l.length > 1) ra['stroke-dasharray'] = '3 1.6';    // utility: dashed = more mappings than the one drawn
        mk('path', ra, g);
        if (G.hair && m.cd && !p.locked && m.pol !== 'ctl') {
          var dc = m.cd[ai], he = m.pol === 'uni' ? [orig, orig + dc] : m.pol === 'bi' ? [orig - dc, orig + dc] : [orig - dc, orig], hd = A(rr + 3.2, he[0], he[1]);
          if (hd) mk('path', { d: hd, fill: 'none', stroke: lineOf(ai), 'stroke-width': 1.1, 'stroke-linecap': 'round', opacity: em.op }, g);
        }
        var o = P(rr, orig);
        mk('circle', { cx: o[0], cy: o[1], r: G.dot, fill: srcColor(src), stroke: v('mark-edge'), 'stroke-width': 1.1, opacity: em.op }, g);
        RAD(g, run, rr - G.tick, rr + G.tick, { stroke: v('ink'), 'stroke-width': 1.4, opacity: em.op === 1 ? .95 : .3 });
        rr += G.hair && m.cd ? G.stepCd : G.step;
      });
      if (!sz) { if (l.length > 4) { var tx = mk('text', { x: 78, y: 88, style: 'font:500 7px var(--font-mono);fill:var(--t2)' }, g); tx.textContent = '+' + (l.length - 4); } }
      else { var nMore = l.length - shown.length; more.textContent = nMore > 0 ? '+' + nMore : ''; }
      var ow = S.owner[p.key];
      if (!p.locked && S.pinned === null && ow && S.now) { var age = (S.now - ow.at) / 500; if (age >= 0 && age < 1 && !S.reduce) mk('circle', { cx: Cc, cy: Cc, r: (G.cap + .5 + age * 7).toFixed(2), fill: 'none', stroke: lineOf(ai), 'stroke-width': 2.2, opacity: (1 - age).toFixed(2) }, g); }
      if (!drag) mk('circle', { cx: Cc + shU, cy: Cc + shU, r: G.cap, fill: v('ink') }, g);   // the house hard shadow (1.5 px), flat while dragging
      mk('circle', { cx: Cc, cy: Cc, r: G.cap, fill: v('panel'), stroke: p.locked ? v('ink') : lineOf(ai), 'stroke-width': p.locked ? 1 : G.capW }, g);
      var n = P(G.ndl, out); mk('line', { x1: Cc, y1: Cc, x2: n[0], y2: n[1], stroke: v('ink'), 'stroke-width': G.ndlW, 'stroke-linecap': 'round' }, g);
      r1.textContent = p.fmt(out);
      if (sz) { el.classList.toggle('drag', !!drag); el.classList.toggle('cap', headCap(S, p, ai, r2) === 'cap'); }
      else readout2(S, p, ai, r2);
      box.setAttribute('aria-valuenow', out.toFixed(3));
      box.setAttribute('aria-valuetext', p.fmt(out) + (ctl ? ', controlled by ' + S.src(ctl.src).name : p.locked ? ', locked' : ', editing corner ' + CORNERS[ai].id + ' ' + p.fmt(p.vals[ai])));
      box.style.cursor = S.map.on ? 'copy' : ctl ? 'not-allowed' : 'ns-resize';
    } };
    S.add(view); return view;
  }

  /* ---------- SLIDER (both specs' slider sections; 200 × 50 viewBox, track 180 units) ---------- */
  /* SLIDER GEOMETRY. CLASSIC is the specs' 200 × 62 drawing (vertical 76 × 200) with its 19ch two-line readout column.
     TIGHT (any `size`) is 200 × 35 (vertical 40 × 200, a wider 22 × 8 fader cap): the track at 13 (14), the tether 10 (12.5)
     units off it, four lanes 3.2 apart from 10 (12) units off on the other side, no corner-depth hairline; the readout column holds the value only (8ch), and the
     corner line and full focus caption go to the section's readout slot, as on the sized knob. */
  var SG = {
    classic: { W: 200, H: 62, VW: 76, tr: 14, vtr: 16, teth: -9.5, l0: 11, step: 5.4, stepCd: 8.4, lw: 2.4, lwH: 3.2, tick: 2.4, hair: true, th: [7, 16] },
    tight: { W: 200, H: 35, VW: 40, tr: 13, vtr: 14, teth: -10, vteth: -12.5, l0: 10, vl0: 12, step: 3.2, stepCd: 3.2, lw: 1.9, lwH: 2.6, tick: 1.6, hair: false, th: [7, 14], vth: [22, 8] }
  };
  function Slider(el, p, S) {
    p.fmt = p.fmt || pct; p.def = p.def === undefined ? (p.locked ? p.g : .5) : p.def;
    S.params[p.key] = p; if (!p.locked) S.saved[p.key] = p.vals.slice();
    if (p.size === 'tiny' || p.size === 'cell') return TinySlider(el, p, S);   // TS1a / TS2a (ADR-340 r2)
    var vert = !!p.vert, LEN = 180, sz = KSIZE[p.size] ? p.size : null, G = sz ? SG.tight : SG.classic;
    if (!vert) p.horizontal = true;
    el.classList.add(vert ? 'hd-ms-v' : 'hd-ms'); if (sz) el.classList.add('sz', 'sz-' + sz); el.innerHTML = '';
    var nm = document.createElement('div'); nm.className = 'mk-name'; nm.textContent = p.name;
    var face = document.createElement('div'); face.className = 'ms-face hd-focus'; face.setAttribute('role', 'slider'); face.setAttribute('aria-label', p.path || p.name);
    var svg = mk('svg', vert ? { viewBox: '0 0 ' + G.VW + ' 200', width: G.VW, height: 200, 'aria-hidden': 'true' } : { viewBox: '0 0 200 ' + G.H, 'aria-hidden': 'true', style: 'width:100%;height:auto' });   // CONSTANT box: room for 4 lanes + hairlines (to 14+53.6+2.4) and the tether, so nothing live resizes it
    face.appendChild(svg);   // scales by viewBox: never wider than its row. (Keep this on its own line: it once fell into the comment above and every slider vanished.)
    var g = mk('g', {}, svg);
    var rd = document.createElement('div'); rd.className = 'ms-rd';
    var r1 = document.createElement('div'); r1.className = 'mk-v1'; var r2 = document.createElement('div'); r2.className = 'mk-v2';
    rd.appendChild(r1); rd.appendChild(r2);
    var more = null; if (sz) { more = document.createElement('span'); more.className = 'mk-more'; face.appendChild(more); el.title = p.path || p.name; }
    if (vert && !sz) { el.appendChild(face); el.appendChild(nm); el.appendChild(rd); } else if (vert) { el.appendChild(nm); el.appendChild(face); el.appendChild(rd); } else { el.appendChild(nm); el.appendChild(face); el.appendChild(rd); }
    hot(el, p, S);
    var TV = G.vtr || G.tr, P2 = vert ? function (x, o) { return [TV + o, 10 + (1 - clamp(x)) * LEN]; } : function (x, o) { return [10 + clamp(x) * LEN, G.tr + o]; };
    function line(a, b, off, at) { var p1 = P2(a, off), p2 = P2(b, off); at.x1 = p1[0]; at.y1 = p1[1]; at.x2 = p2[0]; at.y2 = p2[1]; at['stroke-linecap'] = 'round'; return mk('line', at, g); }
    function perp(x, off, h, at) { var q = P2(x, off); if (vert) { at.x1 = q[0] - h; at.x2 = q[0] + h; at.y1 = at.y2 = q[1]; } else { at.y1 = q[1] - h; at.y2 = q[1] + h; at.x1 = at.x2 = q[0]; } at['stroke-linecap'] = 'round'; return mk('line', at, g); }
    gesture(face, p, S, function () { var r = svg.getBoundingClientRect(); return (vert ? r.height : r.width) * LEN / 200; });   // one slider length = full range
    var view = { render: function () {
      face.tabIndex = S.keys ? 0 : -1; face.style.cursor = S.map.on ? 'copy' : (vert ? 'ns-resize' : 'ew-resize');
      var ai = S.editing(p), b = S.base(p), w = S.weights();
      el.classList.toggle('sel', S.selected === p.key); el.dataset.selc = ai === null ? '' : CORNERS[ai].id;
      g.innerHTML = '';
      line(0, 1, 0, { stroke: v('well-alt'), 'stroke-width': 4 });
      if (p.bipolar) perp(.5, 0, 4, { stroke: v('t2'), 'stroke-width': 1 });
      var ctl = S.ctlOf(p.key), g0 = g; if (ctl) g = mk('g', { opacity: .22 }, g0);   // under Control the own value is a ghost (as on the knob)
      var from = p.bipolar ? .5 : 0; if (Math.abs(b - from) > .002) line(from, b, 0, { stroke: v('ink'), 'stroke-width': 4 });
      g = g0;
      var l = S.mapsOf(p.key), f = S.focused(p.key), run = S.origin(p), off = vert && G.vl0 ? G.vl0 : G.l0;
      if (ctl) l = [ctl].concat(l.filter(function (m) { return m !== ctl; }));
      l.slice(0, 4).forEach(function (m) {
        var d = S.depthOf(m), orig = run, src = S.src(m.src), em = emphasis(S, m, f); run += S.contrib(m);
        if (m.pol === 'ctl') orig = m.lo;
        var ends = m.pol === 'ctl' ? [m.lo, m.hi] : m.pol === 'uni' ? [orig, orig + d] : m.pol === 'bi' ? [orig - d, orig + d] : [orig - d, orig];
        line(ends[0], ends[1], off, { stroke: srcColor(src), 'stroke-width': em.heavy ? G.lwH : G.lw, opacity: em.op });
        if (G.hair && m.cd && ai !== null && m.pol !== 'ctl') { var dc = m.cd[ai], he = m.pol === 'uni' ? [orig, orig + dc] : m.pol === 'bi' ? [orig - dc, orig + dc] : [orig - dc, orig]; line(he[0], he[1], off + 3.4, { stroke: lineOf(ai), 'stroke-width': 1.1, opacity: em.op }); }
        var o = P2(orig, off); mk('circle', { cx: o[0], cy: o[1], r: sz ? 1.9 : 2.4, fill: srcColor(src), stroke: v('mark-edge'), 'stroke-width': 1.1, opacity: em.op }, g);
        perp(run, off, G.tick, { stroke: v('ink'), 'stroke-width': 1.4, opacity: em.op === 1 ? .95 : .3 });
        off += G.hair && m.cd ? G.stepCd : G.step;
      });
      var tv = b;
      if (ctl) g = mk('g', { opacity: .22 }, g0);
      if (!p.locked) {
        for (var i = 0; i < 4; i++) { if (i === ai || w[i] < .0005) continue; var q = P2(p.vals[i], 0);
          pip(g, i, q[0], q[1], 1.2 + 2.2 * w[i], { fill: fillOf(i), stroke: v('mark-edge'), 'stroke-width': 1.1, opacity: (.28 + .72 * w[i]).toFixed(2) }); }
        tv = p.vals[ai];
        if (Math.abs(tv - b) > .003) line(b, tv, vert && G.vteth ? G.vteth : G.teth, { stroke: lineOf(ai), 'stroke-width': 1.6, 'stroke-dasharray': '2 2.6' });
        perp(b, 0, 5, { stroke: v('ink'), 'stroke-width': 1.6 });                 // the blend notch
      }
      g = g0;
      var TH = vert ? (G.vth || [16, 7]) : G.th, t = P2(tv, 0), ow = S.owner[p.key], tw = TH[0], thh = TH[1];
      if (!p.locked && S.pinned === null && ow && S.now && !S.reduce) { var age = (S.now - ow.at) / 500;
        if (age >= 0 && age < 1) { var s = 1 + age * 1.3; mk('rect', { x: t[0] - tw / 2 * s - 1, y: t[1] - thh / 2 * s - 1, width: tw * s + 2, height: thh * s + 2 }, g)
          .setAttribute('style', 'fill:none;stroke:' + lineOf(ai) + ';stroke-width:1.8;opacity:' + (1 - age).toFixed(2)); } }
      var th = mk('rect', { x: t[0] - tw / 2, y: t[1] - thh / 2, width: tw, height: thh, rx: 2 }, g);
      th.setAttribute('fill', p.locked ? v('ink') : fillOf(ai)); th.setAttribute('stroke', v('mark-edge')); th.setAttribute('stroke-width', 1.2);
      if (ctl) { th.setAttribute('opacity', .22); var cp = P2(S.origin(p), 0); mk('circle', { cx: cp[0], cy: cp[1], r: 3.6, fill: srcColor(S.src(ctl.src)), stroke: v('ink'), 'stroke-width': 1.3 }, g); }   // the controlled position
      if (!sz) { if (l.length > 4) { var q2 = P2(1, off); var tx = mk('text', { x: vert ? q2[0] - 6 : q2[0] - 14, y: q2[1] + 3, style: 'font:500 7px var(--font-mono);fill:var(--t2)' }, g); tx.textContent = '+' + (l.length - 4); } }
      else more.textContent = l.length > 4 ? '+' + (l.length - 4) : '';
      var out = S.output(p);
      r1.textContent = p.fmt(out);
      if (sz) el.classList.toggle('cap', headCap(S, p, ai, r2) === 'cap'); else readout2(S, p, ai, r2);
      face.setAttribute('aria-valuenow', out.toFixed(3));
      face.setAttribute('aria-valuetext', p.fmt(out) + (ctl ? ', controlled by ' + S.src(ctl.src).name : p.locked ? ', locked' : ', editing corner ' + CORNERS[ai].id + ' ' + p.fmt(p.vals[ai])));
      if (ctl && !S.map.on) face.style.cursor = 'not-allowed';
    } };
    S.add(view); return view;
  }

  /* ---------- READOUT SLOT: the section's one live readout (Density and hierarchy, README), DR-c ----------
     A sized control has no readout cell of its own. The slot, at the right end of a section header, shows the HOT control
     (under the pointer, dragged, or keyboard-focused) when it sits inside `scope`, as TWO STACKED 13 px LINES (ADR-341 r2, DR-c;
     composition-lab-r3): line 1 is where the hand is taking the control, line 2 is what you hear. The slot is a fixed box
     (height 26 px, width size-rslot or what the header leaves if that is less, ADR-340 A1) in every state, so changing text never moves a pixel (H1, ADR-340 r4). Otherwise it shows
     the section's summary.
       line 1   name · the editing corner's OWN value (bold) · its glyph, letter, weight and lock ("▲ B set · 41%"); LOCKED
                for a global parameter; the padlock and "C locked" for 1.6 s after a refused write
       line 2   "now" the output after morph and every mapping · the focused mapping (source, polarity, amount, "+n" more),
                in its source colour; "controlled by SRC" under a Control; "no modulation" otherwise
     PLACEHOLDER (ADR-341 "Open"): line 1 is ASSUMED to be the editing corner's own value (the value T2d's hollow pip shows),
     not the blend. The human has not said which; to switch, change `setValue` below to S.base(p). */
  function dualLines(S, p, ai, n, v1, set, l2) {
    var l = S.mapsOf(p.key), f = S.focused(p.key), c = S.ctlOf(p.key), out = p.fmt(S.output(p)), t;
    n.textContent = p.name;
    var setValue = p.locked ? p.g : p.vals[ai];                          // PLACEHOLDER: the editing corner's value, not the blend
    v1.textContent = p.fmt(setValue); set.style.color = ''; set.innerHTML = '';
    if (p.locked) { set.style.color = v('hint'); set.textContent = 'LOCKED'; }
    else if (S.refused && !S.refused.ctl && S.refused.key === p.key && (S.now || 0) - S.refused.at < 1600) { set.style.color = lineOf(ai); set.innerHTML = LOCK + '<span>' + CORNERS[ai].id + ' locked</span>'; }
    else { var w = S.weights()[ai]; set.style.color = lineOf(ai); set.innerHTML = glyphSvg(ai, 7, lineOf(ai)) + '<span>' + CORNERS[ai].id + ' set' + (w > .999 ? '' : ' · ' + pct(w)) + '</span>' + (S.locks[ai] ? LOCK : ''); }
    if (c) { l2.style.color = srcColor(S.src(c.src)); t = 'now ' + out + ' · controlled by ' + S.src(c.src).name + ' (' + S.amount(c) + ')'; }
    else if (f) { var src = S.src(f.src); l2.style.color = srcColor(src); t = 'now ' + out + ' · ' + src.name + ' ' + POLN[f.pol] + ' ' + S.amount(f) + (l.length > 1 ? ' (+' + (l.length - 1) + ')' : ''); }
    else { l2.style.color = v('hint'); t = 'now ' + out + ' · no modulation'; }
    if (l2.textContent !== t) l2.textContent = t;
    l2.title = t;
  }
  function ReadoutSlot(el, S, opts) {
    opts = opts || {};
    el.classList.add('hd-rslot'); el.setAttribute('aria-hidden', 'true');   // the controls' own aria-valuetext carries the same text
    var sm = document.createElement('span'); sm.className = 'rs-sum';
    var a1 = document.createElement('span'); a1.className = 'rs-l1';
    var n = document.createElement('span'); n.className = 'rs-n';
    var v1 = document.createElement('b'); v1.className = 'rs-v';
    var set = document.createElement('span'); set.className = 'mk-v2 rs-2';
    var a2 = document.createElement('span'); a2.className = 'rs-l2';
    a1.appendChild(n); a1.appendChild(v1); a1.appendChild(set);
    el.appendChild(sm); el.appendChild(a1); el.appendChild(a2);
    var view = { render: function () {
      var key = S.drag ? S.drag.key : S.hot, hEl = S.hotEl, p = key ? S.params[key] : null, scope = opts.scope || el.parentNode;
      var inside = p && scope && hEl && scope.contains(hEl);
      el.classList.toggle('live', !!inside);
      if (!inside) { var s = typeof opts.summary === 'function' ? opts.summary() : opts.summary || ''; if (sm.textContent !== s) sm.textContent = s; return; }
      dualLines(S, p, S.editing(p), n, v1, set, a2);
    } };
    S.add(view); return view;
  }

  /* ---------- MAP CONTROL: toggle + source selector, the live source strip, the inspector ---------- */
  function MapControl(el, S, opts) {
    opts = opts || {};
    el.classList.add('hd-map'); el.innerHTML = '';
    var btn = document.createElement('button'); btn.type = 'button'; btn.className = 'map-btn hd-focus';
    btn.innerHTML = '<span class="led" aria-hidden="true"></span>MAP <span class="hold">HOLD</span>';
    /* MS3 (ADR-340 r8): `chip: true` makes the source a chip you drag through or step (H.SourceChip) instead of the select */
    var sel = null;
    el.appendChild(btn);
    if (opts.chip) { var ch = document.createElement('span'); el.appendChild(ch); window.Horde.SourceChip(ch, S, opts.chip === true ? {} : opts.chip); }
    else {
      sel = document.createElement('select'); sel.className = 'map-sel hd-focus'; sel.setAttribute('aria-label', 'Modulation source');
      S.sources.forEach(function (s) { var o = document.createElement('option'); o.value = s.id; o.textContent = s.name; sel.appendChild(o); });
      el.appendChild(sel); sel.addEventListener('change', function () { S.setSrc(sel.value); });
    }
    btn.addEventListener('click', function () { S.setMap(!S.map.on); });
    /* the live source strip is ONE component, shared with the mod matrix: H.SourceStrip (10-matrix part), looked up at
       run time because that part loads after this core */
    if (opts.strip && window.Horde.SourceStrip) window.Horde.SourceStrip(opts.strip, S);
    var frame = opts.frame;
    function sync() {
      var s = S.src(S.map.src); el.style.setProperty('--sc', srcColor(s));
      btn.setAttribute('aria-pressed', String(S.map.on)); btn.setAttribute('aria-label', 'Map mode ' + (S.map.on ? 'on' : 'off') + ', source ' + s.name);
      el.classList.toggle('on', S.map.on); el.classList.toggle('momentary', S.map.momentary);
      if (sel) sel.value = S.map.src;
      if (frame) { frame.classList.toggle('map-on', S.map.on); frame.style.setProperty('--sc', srcColor(s)); }
    }
    S.on(sync); sync();
    return { sync: sync };
  }
  function ModInspector(el, S) {
    el.classList.add('hd-insp');
    function draw() {
      var p = S.params[S.selected]; if (!p) { el.innerHTML = '<span class="hd-hint">Select a control.</span>'; return; }
      var l = S.mapsOf(p.key), f = S.focused(p.key);
      var h = '<div class="insp-name">' + p.name + '</div>';
      if (!l.length) h += '<span class="hd-hint">No modulators. Turn MAP on and click this control to map ' + S.src(S.map.src).name + '.</span>';
      l.forEach(function (m, i) { var s = S.src(m.src);
        h += '<div class="insp-row' + (m === f ? ' focus' : '') + '" style="--sc:' + srcColor(s) + '" data-i="' + i + '"><button class="insp-pick hd-focus" data-i="' + i + '"><i class="dot"></i>' + s.name + '</button>' +
          '<button class="hd-pill insp-pol hd-focus" data-i="' + i + '" aria-label="Polarity ' + POLN[m.pol] + ', click to cycle">' + POLN[m.pol] + '</button><span class="hd-value">' + S.amount(m) + (m.cd && m.pol !== 'ctl' ? ' ◇' : '') + '</span>' +
          '<button class="hd-iconbtn insp-del hd-focus" data-i="' + i + '" aria-label="Remove ' + s.name + '"><svg viewBox="0 0 10 10"><path d="M2 1 5 4 8 1 9 2 6 5 9 8 8 9 5 6 2 9 1 8 4 5 1 2Z"/></svg></button></div>'; });
      el.innerHTML = h;
      /* MP1 (ADR-340 r9): every route's chip is the same chip: click next, shift-click previous, a vertical drag for the depth */
      [].forEach.call(el.querySelectorAll('.insp-pol'), function (b) { var m = l[+b.dataset.i]; window.Horde.PolChip(b, S, m, { cycle: function (back) { S.cyclePol(m, back); } }); });
    }
    el.addEventListener('click', function (e) {
      var b = e.target.closest('button') || e.target.closest('.insp-row'); if (!b) return; var p = S.params[S.selected], m = S.mapsOf(p.key)[+b.dataset.i]; if (!m) return;   // the whole row focuses, not only its name button
      if (b.classList.contains('insp-pol')) return; else if (b.classList.contains('insp-del')) S.remove(m); else { S.focus[p.key] = +b.dataset.i; S.lastMap = m; S.emit(); }
    });
    var last = '';
    S.on(draw);
    S.add({ render: function () { var p = S.params[S.selected], k = p ? S.selected + JSON.stringify(S.mapsOf(p.key)) + (S.focus[p.key] || 0) : ''; if (k !== last) { last = k; draw(); } } });
  }

  /* ---------- MORPH PAD and the editing-corner selector ---------- */
  function XYPad(el, S, opts) {
    opts = opts || {};
    el.classList.add('hd-xy'); if (opts.size) { el.style.width = opts.size + 'px'; el.style.height = opts.size + 'px'; }
    el.innerHTML = '<div class="wash"></div><svg class="census-ring" aria-hidden="true"></svg><div class="puck hd-focus" role="slider" aria-label="Morph pad"></div>';
    var ring = el.querySelector('.census-ring'), N = opts.dots || 12, censusOn = !!opts.census;   // the ring is OPTIONAL and OFF by default
    var puck = el.querySelector('.puck'), pos = [['left:5px;top:5px'], ['right:5px;top:5px'], ['left:5px;bottom:5px'], ['right:5px;bottom:5px']];
    var chips = CORNERS.map(function (c, i) {
      var b = document.createElement('button'); b.type = 'button'; b.className = 'corner hd-chip on hd-focus'; b.style.cssText = pos[i][0] + ';--cc:var(--c' + c.id + ')';
      b.innerHTML = '<svg viewBox="0 0 10 10"><path d="' + c.glyph + '"/></svg>'; b.setAttribute('aria-label', 'Pin corner ' + c.id);
      b.addEventListener('pointerdown', function (e) { e.stopPropagation(); });
      b.addEventListener('click', function () { S.setPin(S.pinned === i ? null : i); });
      el.appendChild(b); return b;
    });
    /* opts.relative (the morph editor's pad, the lab's B269 review): only a grab on the puck (its 28 px hit area) moves it,
       by the pointer's travel from the grab, never a jump; a click on the empty pad moves nothing. Default: the puck
       follows the pointer anywhere on the pad. */
    var grab = null;
    function at(e) { var r = el.getBoundingClientRect(); S.pad.x = clamp((e.clientX - r.left) / r.width + (grab ? grab.dx : 0)); S.pad.y = clamp((e.clientY - r.top) / r.height + (grab ? grab.dy : 0)); }
    var down = false;
    el.addEventListener('pointerdown', function (e) {
      if (opts.relative) { if (!puck.contains(e.target)) return; var r = el.getBoundingClientRect(); grab = { dx: S.pad.x - (e.clientX - r.left) / r.width, dy: S.pad.y - (e.clientY - r.top) / r.height }; }
      down = true; try { el.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ } el.classList.add('drag'); at(e); });
    el.addEventListener('pointermove', function (e) { if (down) at(e); });
    var padEnd = function (e) { down = false; grab = null; el.classList.remove('drag'); if (e && el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId); };
    el.addEventListener('pointerup', padEnd); el.addEventListener('pointercancel', padEnd); el.addEventListener('lostpointercapture', padEnd);
    puck.addEventListener('dblclick', function () { S.pad.x = .5; S.pad.y = .5; });
    puck.addEventListener('keydown', function (e) { if (!S.keys) return; var s = e.shiftKey ? .0025 : .01;
      if (e.key === 'ArrowRight') S.pad.x = clamp(S.pad.x + s); else if (e.key === 'ArrowLeft') S.pad.x = clamp(S.pad.x - s);
      else if (e.key === 'ArrowUp') S.pad.y = clamp(S.pad.y - s); else if (e.key === 'ArrowDown') S.pad.y = clamp(S.pad.y + s); else return; e.preventDefault(); });
    S.add({ render: function () {
      puck.style.setProperty('--x', S.pad.x); puck.style.setProperty('--y', 1 - S.pad.y); puck.tabIndex = S.keys ? 0 : -1;
      var w = S.weights(); puck.setAttribute('aria-valuetext', CORNERS.map(function (c, i) { return c.id + ' ' + pct(w[i]); }).join(', '));
      chips.forEach(function (b, i) { b.classList.toggle('pinned', S.pinned === i); b.setAttribute('aria-pressed', String(S.pinned === i)); b.tabIndex = S.keys ? 0 : -1; });
      /* CENSUS RING (optional, off by default): 12 plain dots round the puck, each corner's count its census
         share by largest remainder, so the total never drifts. Plain discs, a deliberate exception to the glyph
         rule: it is an optional decoration, and the pad's badges carry each corner's glyph and name. */
      var sh = censusOn ? S.census() : null, r = el.getBoundingClientRect();
      ring.setAttribute('viewBox', '0 0 ' + r.width + ' ' + r.height); ring.innerHTML = '';
      if (!sh) { puck.removeAttribute('aria-description'); return; }
      var cnt = apportion(sh, N), cx = S.pad.x * r.width, cy = S.pad.y * r.height, R = 19, k = 0, dot = opts.dot || 5.4;
      for (var i = 0; i < 4; i++) for (var j = 0; j < cnt[i]; j++, k++) {
        var a = -Math.PI / 2 + 2 * Math.PI * (k + .5) / N, x = cx + R * Math.cos(a), y = cy + R * Math.sin(a), at2 = { fill: fillOf(i), stroke: v('mark-edge'), 'stroke-width': .8 };
        at2.cx = x; at2.cy = y; at2.r = dot / 2; mk('circle', at2, ring);
      }
      puck.setAttribute('aria-description', 'Census: ' + CORNERS.map(function (c, i) { return c.id + ' ' + cnt[i] + ' of ' + N; }).join(', '));
    } });
    return { census: function (on) { censusOn = !!on; } };
  }
  /* largest-remainder apportionment: integer counts summing exactly to n */
  function apportion(share, n) {
    var raw = share.map(function (x) { return x * n; }), c = raw.map(Math.floor), left = n - c.reduce(function (a, b) { return a + b; }, 0);
    var order = raw.map(function (x, i) { return [x - Math.floor(x), i]; }).sort(function (a, b) { return b[0] - a[0] || a[1] - b[1]; });
    for (var q = 0; q < left; q++) c[order[q][1]]++;
    return c;
  }

  /* ---------- CORNER CHIPS: glyph + the corner preset's real name; lock, revert, edited ---------- */
  function CornerChips(el, S, opts) {
    opts = opts || {};
    el.classList.add('hd-corners'); el.setAttribute('role', 'group'); el.setAttribute('aria-label', 'Morph corners');
    var rows = CORNERS.map(function (c, i) {
      var r = document.createElement('div'); r.className = 'hd-crow'; r.style.setProperty('--cc', 'var(--c' + c.id + ')');
      r.innerHTML = '<button type="button" class="hd-chip hd-focus pick"><svg viewBox="0 0 10 10"><path d="' + c.glyph + '"/></svg></button>' +
        '<span class="cname"></span><span class="edited" role="img" aria-label="edited since saved" title="Edited since saved"></span>' +
        '<button type="button" class="hd-iconbtn hd-focus lock"></button><button type="button" class="hd-iconbtn hd-focus rev" title="Revert to saved">' + REVERT + '</button>';
      r.querySelector('.pick').addEventListener('click', function () { S.setPin(S.pinned === i ? null : i); });
      r.querySelector('.lock').addEventListener('click', function () { S.toggleLock(i); });
      r.querySelector('.rev').addEventListener('click', function () { S.revert(i); });
      el.appendChild(r); return r;
    });
    function sync() {
      rows.forEach(function (r, i) {
        var name = S.corners[i], empty = !name, id = CORNERS[i].id, ed = !empty && S.edited(i);
        r.classList.toggle('empty', empty); r.classList.toggle('locked', S.locks[i]);
        r.classList.toggle('sel', S.pinned === i); r.dataset.selc = S.pinned === i ? id : '';
        var pk = r.querySelector('.pick'); pk.setAttribute('aria-pressed', String(S.pinned === i)); pk.classList.toggle('on', S.pinned === i);
        pk.setAttribute('aria-label', 'Corner ' + id + ', ' + CORNERS[i].shape + ', ' + (empty ? 'empty' : name) + (S.pinned === i ? ', pinned' : ''));
        var nm = r.querySelector('.cname'); nm.textContent = empty ? 'Empty' : name; nm.title = empty ? 'Corner ' + id + ' holds no preset' : name;
        r.querySelector('.edited').hidden = !ed;
        var lk = r.querySelector('.lock'); lk.innerHTML = S.locks[i] ? LOCK.replace('width="8" height="8"', 'width="10" height="10"') : LOCK_OPEN;
        lk.setAttribute('aria-pressed', String(S.locks[i])); lk.setAttribute('aria-label', (S.locks[i] ? 'Unlock' : 'Lock') + ' corner ' + id); lk.title = S.locks[i] ? 'Locked: edits refused (still morphs)' : 'Lock against edits';
        lk.disabled = empty;
        var rv = r.querySelector('.rev'); rv.disabled = !ed || S.locks[i]; rv.setAttribute('aria-label', 'Revert corner ' + id + ' to its saved preset');
      });
    }
    S.on(sync); sync();
    S.add({ render: function () { var k = rows.map(function (r, i) { return S.edited(i) ? 1 : 0; }).join(''); if (k !== el.dataset.ed) { el.dataset.ed = k; sync(); } } });
  }
  /* the knob inspector's corner rows: weight, stored value, depth, edited, lock, revert */
  function CornerInspector(el, S) {
    el.classList.add('hd-cinsp');
    function draw() {
      var p = S.params[S.selected]; if (!p) { el.innerHTML = ''; return; }
      var w = S.weights(), ai = S.editing(p), cdm = S.mapsOf(p.key).filter(function (m) { return m.cd; })[0];
      var h = '<div class="ci-head"><span class="insp-name">' + p.name + '</span>' + (p.locked ? '<span class="hd-hint">one global value</span>' : '') + '</div>';
      if (!p.locked) CORNERS.forEach(function (c, i) {
        var ed = S.saved[p.key] && Math.abs(p.vals[i] - S.saved[p.key][i]) > 1e-6;
        h += '<div class="ci-row' + (i === ai ? ' act sel' : '') + '" data-selc="' + (i === ai ? c.id : '') + '" style="--cc:var(--c' + c.id + ')">' +
          '<span class="hd-chip on" style="cursor:default" role="img" aria-label="Corner ' + c.id + '"><svg viewBox="0 0 10 10"><path d="' + c.glyph + '"/></svg></span>' +
          '<span class="cname" title="' + (S.corners[i] || 'Empty') + '">' + (S.corners[i] || 'Empty') + '</span>' +
          '<span class="wbar" role="img" aria-label="weight ' + pct(w[i]) + '"><i style="width:' + (w[i] * 100) + '%"></i></span>' +
          '<span class="hd-value">' + p.fmt(p.vals[i]) + '</span><span class="hd-value">' + (cdm ? pct(cdm.cd[i]) : '') + '</span>' +
          '<span class="edited"' + (ed ? '' : ' hidden') + ' role="img" aria-label="edited"></span>' +
          '<button class="hd-iconbtn hd-focus lock" data-i="' + i + '" aria-pressed="' + S.locks[i] + '" aria-label="' + (S.locks[i] ? 'Unlock' : 'Lock') + ' corner ' + c.id + '">' + (S.locks[i] ? LOCK.replace('width="8" height="8"', 'width="10" height="10"') : LOCK_OPEN) + '</button>' +
          '<button class="hd-iconbtn hd-focus rev" data-i="' + i + '" aria-label="Revert corner ' + c.id + '"' + (S.edited(i) && !S.locks[i] ? '' : ' disabled') + '>' + REVERT + '</button></div>';
      });
      h += '<div class="ci-row mix"><span></span><span class="hd-label">MIX</span><span></span><span class="hd-value">' + p.fmt(S.base(p)) + '</span><span class="hd-value">' + (cdm ? pct(S.depthOf(cdm)) : '') + '</span></div>';
      el.innerHTML = h;
    }
    el.addEventListener('click', function (e) { var b = e.target.closest('button'); if (!b || b.disabled) return; var i = +b.dataset.i; if (b.classList.contains('lock')) S.toggleLock(i); else if (b.classList.contains('rev')) S.revert(i); });
    var last = ''; S.on(draw);
    S.add({ render: function () { var p = S.params[S.selected]; if (!p) return; var w = S.weights(); var k = S.selected + (p.locked ? 'g' + p.g : p.vals.join()) + S.locks.join() + w.map(function (x) { return x.toFixed(2); }).join() + S.editing(p); if (k !== last) { last = k; draw(); } } });
  }

  /* ---------- SCREEN SCHEME: gui2's SCR chip; one scheme redefines all six scr- roles ---------- */
  var SCHEMES = ['tube', 'orchid', 'frost', 'ember', 'dusk', 'lumen', 'paper'];
  function ScreenScheme(el, target, opts) {
    opts = opts || {}; var cur = opts.scheme || null;   // null = follow the chassis theme (cream ORCHID, dark TUBE)
    el.classList.add('hd-scr');
    var chip = document.createElement('button'); chip.type = 'button'; chip.className = 'hd-scrchip hd-focus';
    el.appendChild(chip);
    var list = null;
    if (opts.list) { list = document.createElement('div'); list.className = 'hd-pills'; list.setAttribute('role', 'radiogroup'); list.setAttribute('aria-label', 'Screen scheme');
      [null].concat(SCHEMES).forEach(function (id) { var b = document.createElement('button'); b.type = 'button'; b.className = 'hd-pill hd-focus'; b.setAttribute('role', 'radio'); b.textContent = id ? id.toUpperCase() : 'THEME';
        b.dataset.id = id || ''; b.addEventListener('click', function () { set(id); }); list.appendChild(b); });
      el.appendChild(list); }
    function set(id) {
      cur = id;
      ['ground', 'value', 'meter', 'physics', 'marker', 'grid'].forEach(function (r) { if (id) target.style.setProperty('--scr-' + r, 'var(--' + id + '-' + r + ')'); else target.style.removeProperty('--scr-' + r); });
      target.dataset.scr = id || '';
      chip.innerHTML = 'SCR <b>' + (id ? id.toUpperCase() : 'THEME') + '</b>';
      chip.setAttribute('aria-label', 'Screen scheme: ' + (id ? id.toUpperCase() : 'follows the theme') + '. Click for the next');
      if (list) list.querySelectorAll('button').forEach(function (b) { b.setAttribute('aria-checked', String((b.dataset.id || null) === id)); });
      if (opts.onchange) opts.onchange(id);
    }
    chip.addEventListener('click', function () { var i = SCHEMES.indexOf(cur); set(i < 0 ? SCHEMES[0] : i === SCHEMES.length - 1 ? null : SCHEMES[i + 1]); });
    set(cur);
    return { set: set };
  }
  function CornerPin(el, S) {
    el.classList.add('hd-pills'); el.setAttribute('role', 'radiogroup'); el.setAttribute('aria-label', 'Editing corner');
    var opts = [{ i: null, label: 'Auto' }].concat(CORNERS.map(function (c, i) { return { i: i, label: c.id }; }));
    var bs = opts.map(function (o) {
      var b = document.createElement('button'); b.type = 'button'; b.className = 'hd-pill hd-focus'; b.setAttribute('role', 'radio');
      b.innerHTML = o.i === null ? 'Auto' : '<span style="display:inline-flex;gap:4px;align-items:center">' + glyphSvg(o.i, 8, 'currentColor') + o.label + '</span>';
      b.addEventListener('click', function () { S.setPin(o.i); }); el.appendChild(b); return b;
    });
    function sync() { bs.forEach(function (b, k) { b.setAttribute('aria-checked', String(opts[k].i === S.pinned)); }); }
    S.on(sync); sync();
  }

  window.Horde = { CORNERS: CORNERS, SCHEMES: SCHEMES, Session: Session, Knob: Knob, Slider: Slider, MapControl: MapControl, ModInspector: ModInspector, XYPad: XYPad, CornerPin: CornerPin, CornerChips: CornerChips, CornerChip: CornerChips, CornerInspector: CornerInspector, ScreenScheme: ScreenScheme, apportion: apportion, glyph: glyphSvg,
    ReadoutSlot: ReadoutSlot, readout: readout2, KNOB_SIZES: KSIZE };

  /* =====================================================================
     VISUALIZERS. Painters ported from gui2.html's canvases and the SCALPEL lab's (B295, B293),
     drawing ONLY with the screen roles (scr-ground, -value, -meter, -physics, -marker, -grid),
     so every one follows the Screen scheme. The phosphor rules (ADR-119): glow is a wider
     underlay stroke or disc at low alpha, never a blur; scanlines are 1 px dark lines every
     3 px at the scheme's scan strength; a mark's alpha is scaled by the scheme's alpha.
     ===================================================================== */
  var Viz = {};
  function schemeOf(el) {
    var t = el.closest('[data-scr]'), id = t && t.dataset.scr;
    if (!id) id = document.documentElement.getAttribute('data-theme') === 'dark' ? 'tube' : 'orchid';
    var cs = getComputedStyle(document.documentElement);
    return { id: id, scan: parseFloat(cs.getPropertyValue('--scan-' + id)) || .05, alpha: parseFloat(cs.getPropertyValue('--alpha-' + id)) || 1 };
  }
  function rgbOf(el, name) {
    var e = document.createElement('i'); e.style.color = 'var(--' + name + ')'; e.style.display = 'none'; el.appendChild(e);
    var m = getComputedStyle(e).color.match(/[\d.]+/g).map(Number); e.remove(); return [m[0], m[1], m[2]];
  }
  /* one frame's drawing kit: the canvas fitted to its CSS box at the display's scale (gui2 fitViz),
     the palette, and the scheme's scan and alpha */
  Viz.kit = function (cv) {
    var V = Math.min(window.devicePixelRatio || 1, 2), w = Math.max(2, Math.round(cv.clientWidth * V)), h = Math.max(2, Math.round(cv.clientHeight * V));
    if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
    var sc = schemeOf(cv), P = {};
    ['scr-ground', 'scr-value', 'scr-meter', 'scr-physics', 'scr-marker', 'scr-grid', 'alarm'].forEach(function (n) { P[n.replace('scr-', '')] = rgbOf(cv.parentElement, n); });
    var g = cv.getContext('2d');
    /* ROUND JOINS, ALWAYS (ADR-340 Consequences; the human, 2026-10-30: the strokes were "jumpy"). The canvas default is
       a miter join with miterLimit 10: a corner sharper than ~11.5° flips from a long spike to a flat bevel, so a trace
       whose angle crosses that steepness visibly jumps. A round join has no such threshold. Set on every frame's kit, and
       never set back to miter (a component that wants a sharp corner draws it, it does not change the join). */
    g.lineJoin = 'round';
    return { g: g, W: w, H: h, V: V, P: P, sc: sc,
      c: function (k, a) { var q = P[k]; return 'rgba(' + q[0] + ',' + q[1] + ',' + q[2] + ',' + (a === undefined ? 1 : Math.min(1, a * sc.alpha)) + ')'; },
      mix: function (k1, k2, t, a) { var A = P[k1], B = P[k2]; return 'rgba(' + [0, 1, 2].map(function (i) { return Math.round(A[i] + (B[i] - A[i]) * t); }).join(',') + ',' + (a === undefined ? 1 : a) + ')'; },
      tube: function (a) { g.fillStyle = this.c('ground', a === undefined ? 1 : a / sc.alpha); g.fillRect(0, 0, w, h); },
      scan: function (k) { g.fillStyle = 'rgba(0,0,0,' + sc.scan * (k || 1) + ')'; for (var y = V; y < h; y += 3 * V) g.fillRect(0, y, w, V); } };
  };
  /* KURAMOTO RING: drawn by the part at the end of this file (the SCALPEL lab's phase ring, B544); gui2's _drawPhaseOne is retired */
  /* WAVEFORM (gui2 paintScopeInto): one period from the f0 trigger; L is the beam, R a dashed reference */
  Viz.scope = function (cv, s) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, pk = 1e-4, i;
    K.tube(); K.scan();
    g.strokeStyle = K.c('grid', .4); g.lineWidth = V; g.beginPath(); g.moveTo(0, H / 2); g.lineTo(W, H / 2); g.stroke();
    for (i = 0; i < s.l.length; i++) pk = Math.max(pk, Math.abs(s.l[i]), Math.abs(s.r[i]));
    var gain = (H / 2 - 3 * V) / pk, wet = K.mix('value', 'marker', Math.max(0, Math.min(1, s.R || 0)));
    function trace(arr) { g.beginPath(); for (var j = 0; j < arr.length; j++) { var x = j / (arr.length - 1) * W, y = H / 2 - arr[j] * gain; j ? g.lineTo(x, y) : g.moveTo(x, y); } g.stroke(); }
    g.setLineDash([2 * V, 3 * V]); g.strokeStyle = K.c('meter', .65); g.lineWidth = 1.4 * V; trace(s.r); g.setLineDash([]);
    g.strokeStyle = wet.replace(/,1\)$/, ',.22)'); g.lineWidth = 8 * V; trace(s.l);       // beam underlay
    g.strokeStyle = wet; g.lineWidth = 2.2 * V; trace(s.l);
  };
  /* SPECTRUM (gui2 _drawSpecOne): third-octave bars 30 Hz–16 kHz, the level-mapped sunset (physics → value → marker),
     peak caps held 1.5 s then falling, a cap at the ceiling is the clip signal (alarm, the one red) */
  Viz.spectrum = function (cv, bands, st, now) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, nb = bands.length, slot = W / nb, bw = slot * .75, floorY = H - 2 * V, top = H - 6 * V;
    K.tube(); K.scan();
    st.pk = st.pk || new Float32Array(nb); st.at = st.at || new Float64Array(nb);
    for (var b = 0; b < nb; b++) {
      var v = Math.max(0, Math.min(1, bands[b])), x = b * slot + (slot - bw) / 2, t = v * 2;
      g.fillStyle = t < 1 ? K.mix('physics', 'value', t) : K.mix('value', 'marker', t - 1);
      g.fillRect(x, floorY - v * top, bw, v * top);
      if (v >= st.pk[b]) { st.pk[b] = v; st.at[b] = now; } else if (now - st.at[b] > 1500) st.pk[b] = Math.max(v, st.pk[b] - .012);
      g.fillStyle = st.pk[b] >= .999 ? K.c('alarm') : K.c('meter');
      g.fillRect(x, floorY - st.pk[b] * top - 2.5 * V, bw, 2.5 * V);
    }
    g.fillStyle = K.c('grid', .55); g.fillRect(0, floorY, W, 2 * V);
    g.strokeStyle = K.c('grid', .55); g.lineWidth = V; g.setLineDash([3 * V, 3 * V]); g.beginPath(); g.moveTo(0, floorY - top); g.lineTo(W, floorY - top); g.stroke(); g.setLineDash([]);
  };
  /* PHASE CARPET (SCALPEL lab B295): one row per member, phase across the width; the trail is the last
     16 frames at 0.75^age; a member inside a blade window is drawn in value, else physics */
  Viz.carpet = function (cv, hist, inBlade) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V; K.tube(); K.scan();
    if (!hist.length) return;
    var n = hist[hist.length - 1].length, rowH = (H - 4 * V) / n;
    for (var r = 0; r < n; r++) { g.strokeStyle = K.c('grid', .18); g.lineWidth = V; g.beginPath(); g.moveTo(4 * V, 2 * V + (r + .5) * rowH); g.lineTo(W - 4 * V, 2 * V + (r + .5) * rowH); g.stroke(); }
    hist.slice(-16).forEach(function (row, f, all) { var age = all.length - 1 - f, a = Math.pow(.75, age);
      row.forEach(function (ph, i) { var hot = inBlade && inBlade(ph); g.fillStyle = K.c(hot ? 'value' : 'physics', a);
        g.beginPath(); g.arc(4 * V + ph * (W - 8 * V), 2 * V + (i + .5) * rowH, (age ? 2 : 2.6) * V, 0, 2 * Math.PI); g.fill(); }); });
  };
  /* PHASE CARPET, VERTICAL: the rows carpet turned 90°. Each member is a COLUMN and its phase dot moves up and down
     (phase 0 at the bottom); the same 16-frame trail at 0.75^age, the same inks and guides as the rows carpet */
  Viz.carpetV = function (cv, hist, inBlade) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V; K.tube(); K.scan();
    if (!hist.length) return;
    var n = hist[hist.length - 1].length, colW = (W - 4 * V) / n;
    for (var c = 0; c < n; c++) { g.strokeStyle = K.c('grid', .18); g.lineWidth = V; g.beginPath(); g.moveTo(2 * V + (c + .5) * colW, 4 * V); g.lineTo(2 * V + (c + .5) * colW, H - 4 * V); g.stroke(); }
    hist.slice(-16).forEach(function (row, f, all) { var age = all.length - 1 - f, a = Math.pow(.75, age);
      row.forEach(function (ph, i) { var hot = inBlade && inBlade(ph); g.fillStyle = K.c(hot ? 'value' : 'physics', a);
        g.beginPath(); g.arc(2 * V + (i + .5) * colW, H - 4 * V - ph * (H - 8 * V), (age ? 2 : 2.6) * V, 0, 2 * Math.PI); g.fill(); }); });
  };
  /* BLADE VIEW (SCALPEL lab paintCycle, SUM view): one cycle across the width. Each blade: its NOMINAL window
     (the voice's own) as the solid band, and the members' full extent (earliest start to latest end, from the
     SAME member windows the render used) as a lighter band; blade 2 hatched. The base wave dashed; members
     faint; the heard sum on top. The bands are true to the data: the sum departs from the base only inside them. */
  Viz.blade = function (cv, cy) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, top = 3 * V, bot = H - 3 * V, mid = H / 2, amp = (bot - top) / 2 / 1.25, q;
    K.tube(); K.scan();
    g.strokeStyle = K.c('grid', .18); g.lineWidth = V; g.beginPath();
    for (q = 1; q < 4; q++) { g.moveTo(Math.round(q * W / 4) + .5, top); g.lineTo(Math.round(q * W / 4) + .5, bot); } g.moveTo(0, mid); g.lineTo(W, mid); g.stroke();
    var marks = [];
    function band(st, w, blade, a, edge, dash) { var A = st * W, Z = (st + w) * W, segs = Z <= W ? [[A, Z]] : [[A, W], [0, Z - W]];
      segs.forEach(function (sg) {
        if (blade === 2) { g.save(); g.beginPath(); g.rect(sg[0], top, sg[1] - sg[0], bot - top); g.clip(); g.strokeStyle = K.c('value', a * 1.6); g.lineWidth = V;
          for (var x = sg[0] - H; x < sg[1]; x += 5 * V) { g.beginPath(); g.moveTo(x, bot); g.lineTo(x + (bot - top), top); g.stroke(); } g.restore(); }
        else { g.fillStyle = K.c('value', a); g.fillRect(sg[0], top, sg[1] - sg[0], bot - top); }
        if (edge) { g.strokeStyle = K.c('value', edge); g.lineWidth = 1.2 * V; if (dash) g.setLineDash([2 * V, 2 * V]);
          g.beginPath(); g.moveTo(sg[0], top); g.lineTo(sg[0], bot); g.moveTo(sg[1], top); g.lineTo(sg[1], bot); g.stroke(); g.setLineDash([]); } });
      return [A / V, Z / V]; }
    cy.windows.forEach(function (wd) {
      var lo = Infinity, hi = -Infinity; (wd.members || [{ st: wd.st, w: wd.w }]).forEach(function (m) { lo = Math.min(lo, m.st); hi = Math.max(hi, m.st + m.w); });
      var ext = band(lo, hi - lo, wd.blade, wd.a * .45, .45, true);         // the members' full extent, lighter
      var nom = band(wd.st, wd.w, wd.blade, wd.a, .75, false);                // the nominal window, solid
      marks.push({ blade: wd.blade, nominal: nom, extent: ext });
    });
    cv.__marks = marks;                                                      // CSS px, read by the true-to-data check
    function line(arr, k, a, lw, dash) { g.strokeStyle = K.c(k, a); g.lineWidth = lw * V; if (dash) g.setLineDash(dash.map(function (d) { return d * V; }));
      g.beginPath(); for (var n2 = 0; n2 <= arr.length; n2++) { var y = mid - arr[n2 % arr.length] * amp, x = n2 / arr.length * W; n2 ? g.lineTo(x, y) : g.moveTo(x, y); } g.stroke(); g.setLineDash([]); }
    line(cy.base, 'grid', .55, 1, [3, 3]);
    cy.members.forEach(function (m) { line(m, 'physics', .28, 1); });
    if (K.sc.alpha > 1) line(cy.sum, 'ground', .62 / K.sc.alpha, 1.8 + 2);
    line(cy.sum, 'meter', 1, 1.8);
  };
  /* ENVELOPE (UI Spec §6; the phosphor horizon floor): the curve in value with an 18% fill, node handles in
     marker / meter / physics / grid, the playhead dot */
  Viz.envelope = function (cv, e, play) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, x0 = 8 * V, x1 = W - 8 * V, y0 = 8 * V, y1 = H - 8 * V, i;
    K.tube(); K.scan();
    g.strokeStyle = K.c('grid', .22); g.lineWidth = V;
    for (i = 0; i < 6; i++) { var yy = y1 - (y1 - y0) * Math.pow(i / 6, 1.8); g.beginPath(); g.moveTo(x0, yy); g.lineTo(x1, yy); g.stroke(); }   // the receding floor
    for (i = 0; i <= 8; i++) { var xx = x0 + (x1 - x0) * i / 8; g.beginPath(); g.moveTo(W / 2 + (xx - W / 2) * .3, y0); g.lineTo(xx, y1); g.stroke(); }
    var T = e.a + e.d + e.hold + e.r, X = function (t) { return x0 + (x1 - x0) * t / T; }, Y = function (v) { return y1 - (y1 - y0) * v; };
    var pts = Viz.envPoints(e);
    g.beginPath(); pts.forEach(function (p, j) { j ? g.lineTo(X(p[0]), Y(p[1])) : g.moveTo(X(p[0]), Y(p[1])); }); g.lineTo(X(T), Y(0)); g.lineTo(X(0), Y(0)); g.closePath(); g.fillStyle = K.c('value', .18); g.fill();
    g.beginPath(); pts.forEach(function (p, j) { j ? g.lineTo(X(p[0]), Y(p[1])) : g.moveTo(X(p[0]), Y(p[1])); }); g.strokeStyle = K.c('value', .22); g.lineWidth = 8 * V; g.stroke(); g.strokeStyle = K.c('value'); g.lineWidth = 2.5 * V; g.stroke();
    var nodes = [[e.a, 1, 'marker'], [e.a + e.d, e.s, 'meter'], [e.a + e.d + e.hold, e.s, 'physics'], [T, 0, 'grid']];
    nodes.forEach(function (nd) { g.fillStyle = K.c(nd[2], .3); g.beginPath(); g.arc(X(nd[0]), Y(nd[1]), 8 * V, 0, 2 * Math.PI); g.fill();
      g.fillStyle = K.c(nd[2]); g.beginPath(); g.arc(X(nd[0]), Y(nd[1]), 5 * V, 0, 2 * Math.PI); g.fill(); });
    if (play !== null && play !== undefined) { var pv = Viz.envAt(e, play); g.fillStyle = K.c('physics'); g.beginPath(); g.arc(X(play), Y(pv), 3.5 * V, 0, 2 * Math.PI); g.fill(); }
  };
  Viz.envAt = function (e, t) {
    /* the engine's law (SPEC-ENGINE C.12): a stage's curve u in -2.5..2.5 bends it with c = 4u, so u = +2.303 is the
       bank's exponential; the modulator family's editors use the same law and unit */
    function bend(x, u) { var c = 4 * u; return c === 0 ? x : (1 - Math.exp(-c * x)) / (1 - Math.exp(-c)); }
    if (t < e.a) return bend(t / e.a, e.ac); t -= e.a;
    if (t < e.d) return 1 - (1 - e.s) * bend(t / e.d, e.dc); t -= e.d;
    if (t < e.hold) return e.s; t -= e.hold;
    return t < e.r ? e.s * (1 - bend(t / e.r, e.rc)) : 0;
  };
  Viz.envPoints = function (e) { var T = e.a + e.d + e.hold + e.r, out = []; for (var i = 0; i <= 160; i++) { var t = T * i / 160; out.push([t, Viz.envAt(e, t)]); } return out; };
  /* LFO ROTOR (UI Spec §6; phosphor: the lead gets the beam): coupled members on a ring, the output puck */
  Viz.rotor = function (cv, r) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, cx = W / 2, cy = H / 2, rad = Math.min(W, H) * .36;
    K.tube(.3); K.scan(.3);
    g.strokeStyle = K.c('grid', .4); g.lineWidth = V; g.beginPath(); g.arc(cx, cy, rad, 0, 2 * Math.PI); g.stroke();
    /* the arms start at the HUB's edge and end in round caps; the hub disc is drawn LAST, opaque screen ground with a
       grid outline, so no stroke or glow ever reaches or crosses at the axis (the human, 2026-10-21: the wide strokes
       piled up messily there). The hub's radius derives from the widest arm (the lead's glow), 1.5x its width. */
    var ks = ['physics', 'meter', 'marker', 'grid'], ARM = [8, 1.2], hub = 1.5 * Math.max(ARM[0], ARM[1]) * V;
    g.lineCap = 'round';
    r.phase.forEach(function (ph, i) { var a = ph * 2 * Math.PI, ca = Math.cos(a), sa = Math.sin(a), x = cx + rad * ca, y = cy - rad * sa, x0 = cx + hub * ca, y0 = cy - hub * sa;
      g.strokeStyle = K.c(ks[i], i ? .7 : .25); g.lineWidth = (i ? ARM[1] : ARM[0]) * V; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x, y); g.stroke();
      if (!i) { g.strokeStyle = K.c('physics'); g.lineWidth = 2.5 * V; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x, y); g.stroke(); }
      g.fillStyle = K.c(ks[i]); g.beginPath(); g.arc(x, y, (i ? 3 : 4) * V, 0, 2 * Math.PI); g.fill(); });
    g.lineCap = 'butt';
    g.fillStyle = K.mix('ground', 'ground', 0, 1); g.beginPath(); g.arc(cx, cy, hub, 0, 2 * Math.PI); g.fill();   // mix(): fully opaque, never the trail alpha
    g.strokeStyle = K.c('grid', .9); g.lineWidth = 1.2 * V; g.stroke();
    cv.__hub = { cx: cx / V, cy: cy / V, r: hub / V };   // CSS px, for the axis check
    var ox = 10 * V + (W - 20 * V) * .5, oy = cy - (r.out - .5) * (H - 20 * V);
    g.fillStyle = K.c('value', .3); g.beginPath(); g.arc(W - 14 * V, oy, 10 * V, 0, 2 * Math.PI); g.fill();
    g.fillStyle = K.c('value'); g.beginPath(); g.arc(W - 14 * V, oy, 5 * V, 0, 2 * Math.PI); g.fill();
    g.strokeStyle = K.c('grid', .5); g.lineWidth = V; g.beginPath(); g.moveTo(W - 14 * V, 10 * V); g.lineTo(W - 14 * V, H - 10 * V); g.stroke();
  };
  /* VOICE MAP (UI Spec §6; gui2 drawVmap): x pan, y detune in cents (±24 rails dashed); a hollow TARGET ring,
     the filled ACTUAL dot with its bloom, the root star. The ring-to-dot gap IS drift + glide + pull. */
  Viz.voiceMap = function (cv, vm) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, X = function (p) { return W / 2 + p * (W / 2 - 14 * V); }, Y = function (c) { return H / 2 - c / 30 * (H / 2 - 8 * V); };
    K.tube(); K.scan();
    g.strokeStyle = K.c('grid', .4); g.lineWidth = V; g.setLineDash([3 * V, 3 * V]); [-24, 24].forEach(function (c) { g.beginPath(); g.moveTo(0, Y(c)); g.lineTo(W, Y(c)); g.stroke(); }); g.setLineDash([]);
    g.strokeStyle = K.c('grid', .25); g.beginPath(); g.moveTo(W / 2, 0); g.lineTo(W / 2, H); g.moveTo(0, H / 2); g.lineTo(W, H / 2); g.stroke();
    vm.forEach(function (m, i) {
      g.strokeStyle = K.c('grid', .6); g.lineWidth = .8 * V; g.beginPath(); g.moveTo(X(m.tp), Y(m.tc)); g.lineTo(X(m.p), Y(m.c)); g.stroke();
      g.strokeStyle = K.c('physics'); g.lineWidth = 1.5 * V; g.beginPath(); g.arc(X(m.tp), Y(m.tc), 5 * V, 0, 2 * Math.PI); g.stroke();
      g.fillStyle = K.c('value', .3); g.beginPath(); g.arc(X(m.p), Y(m.c), 8 * V, 0, 2 * Math.PI); g.fill();
      g.fillStyle = K.c('value'); g.beginPath(); g.arc(X(m.p), Y(m.c), 4.5 * V, 0, 2 * Math.PI); g.fill();
      if (m.root) { g.fillStyle = K.c('marker'); star(g, X(m.p) + 13 * V, Y(m.c) - 10 * V, 6.5 * V); }
    });
  };
  function star(g, x, y, r) { g.beginPath(); for (var i = 0; i < 10; i++) { var a = -Math.PI / 2 + i * Math.PI / 5, q = i % 2 ? r * .45 : r; g.lineTo(x + q * Math.cos(a), y + q * Math.sin(a)); } g.closePath(); g.fill(); }
  /* GONIOMETER (UI Spec §6): the L/R Lissajous blob in value (25% fill, 1.5 stroke), the mono-safe diamond dashed,
     the correlation bar from the centre in meter; below −0.2 it turns marker (the screen has no red) */
  Viz.gonio = function (cv, l, r, opts) {
    opts = opts || {};
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, S = Math.min(W, H - 18 * V) * .42, cx = W / 2, cy = (H - 18 * V) / 2, i, sxy = 0, sxx = 0, syy = 0;
    K.tube(); if (!opts.traceOnly) K.scan();
    if (!opts.traceOnly) { g.strokeStyle = K.c('grid', .5); g.lineWidth = V; g.setLineDash([3 * V, 3 * V]); g.beginPath(); g.moveTo(cx, cy - S); g.lineTo(cx + S, cy); g.lineTo(cx, cy + S); g.lineTo(cx - S, cy); g.closePath(); g.stroke(); g.setLineDash([]); }
    /* the trace: an OPEN polyline, never filled or closed (the human, 2026-10-21); points closer than 0.5 px are
       dropped so tight turns cannot spike; round joins and caps; the glow is a wider round underlay stroke */
    var pts = [];
    for (i = 0; i < l.length; i++) { var m = (l[i] + r[i]) / 2, sd = (l[i] - r[i]) / 2; sxy += l[i] * r[i]; sxx += l[i] * l[i]; syy += r[i] * r[i];
      var x = cx + sd * S, y = cy - m * S, last = pts[pts.length - 1]; if (!last || Math.hypot(x - last[0], y - last[1]) >= .5 * V) pts.push([x, y]); }
    /* the wrap: a periodic trace ends where it began, so the last point joins the first (continuous, never filled) */
    if (pts.length > 2 && Math.hypot(pts[0][0] - pts[pts.length - 1][0], pts[0][1] - pts[pts.length - 1][1]) < 6 * V) pts.push(pts[0].slice());
    g.lineJoin = 'round'; g.lineCap = 'round'; g.miterLimit = 2;
    function poly() { g.beginPath(); pts.forEach(function (p, j) { j ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1]); }); }
    /*TRACE*/ poly(); g.strokeStyle = K.c('value', .22); g.lineWidth = 6 * V; g.stroke(); poly(); g.strokeStyle = K.c('value'); g.lineWidth = 1.5 * V; g.stroke();
    g.lineJoin = 'round'; g.lineCap = 'butt';
    cv.__trace = { pts: pts.map(function (p) { return [p[0] / V, p[1] / V]; }), halfWidth: 3 };   // CSS px, for the painted-pixel check
    var corr = sxy / Math.sqrt(sxx * syy || 1), by = H - 8 * V;
    if (!opts.traceOnly) {
      g.fillStyle = K.c('grid', .3); g.fillRect(8 * V, by - 2 * V, W - 16 * V, 4 * V);
      g.fillStyle = K.c(corr < -.2 ? 'marker' : 'meter'); var bx = cx + corr * (W / 2 - 8 * V); g.fillRect(Math.min(cx, bx), by - 3 * V, Math.abs(bx - cx), 6 * V);
      g.fillStyle = K.c('grid'); g.fillRect(cx - .5 * V, by - 5 * V, V, 10 * V); }
    return corr;
  };
  /* SYNTHETIC DATA for the previews: seeded and deterministic, generated in the preview, never the engine */
  function mulberry(a) { return function () { a |= 0; a = a + 0x6D2B79F5 | 0; var t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
  var Synth = { rng: mulberry };
  /* a Kuramoto swarm: N members, natural spread, coupling K; phases in cycles */
  Synth.swarm = function (seed, n) {
    var r = mulberry(seed), ph = [], w = [], i;
    for (i = 0; i < n; i++) { ph.push(r()); w.push((r() - .5) * .9); }
    return { phase: ph, R: 0, psi: 0, K: 0, step: function (dt, K) {
      var sx = 0, sy = 0, j; for (j = 0; j < n; j++) { sx += Math.cos(2 * Math.PI * ph[j]); sy += Math.sin(2 * Math.PI * ph[j]); }
      this.R = Math.hypot(sx, sy) / n; this.psi = Math.atan2(sy, sx);
      for (j = 0; j < n; j++) { var d = w[j] + K * this.R * Math.sin(this.psi - 2 * Math.PI * ph[j]) / (2 * Math.PI); ph[j] = ((ph[j] + (.35 + d) * dt) % 1 + 1) % 1; }
      return this; } };
  };
  function saw(x) { x = x - Math.floor(x); return 2 * x - 1; }
  Synth.saw = saw;
  window.Horde.Viz = Viz; window.Horde.Synth = Synth;

  /* =====================================================================
     THE PITCH SYSTEM'S VIEWS (B504, ADR-232, RC-3). The travel laws and the quantiser are the bend
     lab's (docs/design/bend-lab.html, Inertia.step and quantise), run at the tick rate (2756/s), so the
     drawn curve is the law the plugin would run, not a sketch of it.
     ===================================================================== */
  var Glide = {}, PC = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
  Glide.LAWS = ['off (instant)', 'constant time', 'constant rate', 'lag (one-pole)', 'mass-spring'];
  Glide.SCALES = [['chromatic', [0,1,2,3,4,5,6,7,8,9,10,11]], ['major (ionian)', [0,2,4,5,7,9,11]], ['natural minor', [0,2,3,5,7,8,10]],
    ['harmonic minor', [0,2,3,5,7,8,11]], ['melodic minor', [0,2,3,5,7,9,11]], ['dorian', [0,2,3,5,7,9,10]], ['phrygian', [0,1,3,5,7,8,10]],
    ['lydian', [0,2,4,6,7,9,11]], ['mixolydian', [0,2,4,5,7,9,10]], ['locrian', [0,1,3,5,6,8,10]], ['major pentatonic', [0,2,4,7,9]],
    ['minor pentatonic', [0,3,5,7,10]], ['blues', [0,3,5,6,7,10]], ['whole tone', [0,2,4,6,8,10]], ['diminished (h-w)', [0,1,3,4,6,7,9,10]],
    ['phrygian dominant', [0,1,4,5,7,8,10]], ['hirajoshi', [0,2,3,7,8]], ['in-sen', [0,1,5,7,10]]];
  Glide.PC = PC;
  Glide.mask = function (deg) { var m = [0,0,0,0,0,0,0,0,0,0,0,0]; deg.forEach(function (d) { m[d] = 1; }); return m; };
  function osFromZeta(z) { return z >= 1 ? 0 : Math.exp(-Math.PI * z / Math.sqrt(1 - z * z)); }
  function zetaFromOs(os) { var l = Math.log(Math.min(.95, Math.max(1e-4, os))); return -l / Math.sqrt(Math.PI * Math.PI + l * l); }
  /* simulate one move from `from` to `to` (semitones, absolute); p: { law, time, rate, lag, springF, damp, distOver, retMul,
     quant: 0 off | 1 chromatic | 2 scale, root, mask, hyst (cents), qTime (ms) }; returns the law's path x and the emitted y */
  Glide.simulate = function (p, from, to, dur, lane) {
    var sr = 2756, dt = 1 / sr, n = Math.round(dur * sr), pre = Math.round(.06 * sr), x = from, v = 0, D = 0, q, qT = 1e9, out = { t: [], x: [], y: [], target: [] };
    for (var i = 0; i < n; i++) {
      var tg = i < pre ? from : to, err = Math.abs(tg - x);
      if (err > D) D = err; if (err < .004 && Math.abs(v) < .02) D = 0;
      var Dd = Math.max(D, .01), rm = lane === 'bend' && Math.abs(tg) < Math.abs(x) ? Math.max(.05, p.retMul || 1) : 1, st, z, w;
      switch (p.law) {
        case 0: x = tg; v = 0; break;
        case 1: st = Dd / Math.max(1e-3, p.time * 1e-3 * rm) * dt; x += Math.max(-st, Math.min(st, tg - x)); break;
        case 2: st = (p.rate / rm) * dt; x += Math.max(-st, Math.min(st, tg - x)); break;
        case 3: x += (tg - x) * (1 - Math.exp(-dt / Math.max(1e-4, p.lag * 1e-3 * rm))); break;
        case 4: w = 2 * Math.PI * p.springF / rm; z = p.damp;
          if (z < 1 && p.distOver !== 1) z = zetaFromOs(osFromZeta(z) * Math.pow(Dd / 2, p.distOver - 1));
          v += (w * w * (tg - x) - 2 * z * w * v) * dt; x += v * dt; break;
      }
      var y = x;
      if (p.quant) {   // the quantiser: a MODIFIER on the emitted pitch; the law runs on underneath
        var root = p.quant === 2 ? p.root : 0, best = Math.round(x), bd = Infinity, c;
        for (c = Math.floor(x) - 12; c <= Math.ceil(x) + 12; c++) { if (p.quant === 2 && !p.mask[((c - root) % 12 + 12) % 12]) continue; var d = Math.abs(c - x); if (d < bd) { bd = d; best = c; } }
        if (q !== undefined && best !== q && Math.abs(x - q) - Math.abs(x - best) < (p.hyst || 0) / 100) best = q;
        qT += dt; if (q !== undefined && best !== q && p.qTime > 0 && qT < p.qTime / 1000) best = q;
        if (best !== q) { q = best; qT = 0; } y = q;
      }
      out.t.push(i * dt); out.x.push(x); out.y.push(y); out.target.push(tg);
    }
    /* the readouts the Design System's sticker prices: lag50, overshoot, settle, rings */
    var dist = to - from, sgn = dist >= 0 ? 1 : -1, t50 = null, over = 0, settle = 0, rings = 0, prev = 0;
    for (i = pre; i < n; i++) { var yy = out.y[i], e2 = (yy - to) * sgn;
      if (t50 === null && (yy - from) * sgn >= Math.abs(dist) / 2) t50 = (i - pre) * dt;
      if (e2 > over) over = e2; if (Math.abs(yy - to) > .05) settle = (i - pre) * dt;
      var side = e2 > 0 ? 1 : e2 < 0 ? -1 : 0; if (side && prev && side !== prev) rings++; if (side) prev = side; }
    out.metrics = { lag50: t50, over: over * 100, settle: settle, rings: rings };
    out.pre = pre * dt;
    return out;
  };
  /* BEND VIEW: the travel from the old pitch to the new; the hand's target dashed, the law's path faint, what is
     emitted as the beam, the peak overshoot marked, and the scale's snap points when quantise is on */
  Viz.bend = function (cv, sim, opts) {
    var K = Viz.kit(cv), g = K.g, W = K.W, H = K.H, V = K.V, pad = 8 * V, L = 34 * V, i;
    K.tube(); K.scan();
    var lo = Infinity, hi = -Infinity; sim.y.concat(sim.target, sim.x).forEach(function (u) { lo = Math.min(lo, u); hi = Math.max(hi, u); });
    lo = Math.floor(lo - .6); hi = Math.ceil(hi + .6);
    var T = sim.t[sim.t.length - 1], X = function (t) { return L + (W - L - pad) * t / T; }, Y = function (s2) { return H - pad - (H - 2 * pad) * (s2 - lo) / (hi - lo); };
    g.font = (9 * V) + 'px ' + (getComputedStyle(cv).getPropertyValue('--font-mono') || 'monospace'); g.textBaseline = 'middle';
    for (var s3 = lo; s3 <= hi; s3++) {
      var admitted = !opts.quant || opts.quant === 1 || opts.mask[((s3 - opts.root) % 12 + 12) % 12];
      if (opts.quant && admitted) { g.strokeStyle = K.c('grid', .45); g.setLineDash([2 * V, 3 * V]); } else { g.strokeStyle = K.c('grid', .12); g.setLineDash([]); }
      g.lineWidth = V; g.beginPath(); g.moveTo(L, Y(s3)); g.lineTo(W - pad, Y(s3)); g.stroke(); g.setLineDash([]);
      if (admitted && (hi - lo < 16 || s3 % 2 === 0)) { g.fillStyle = K.c(opts.quant ? 'grid' : 'grid', opts.quant ? 1 : .7); g.fillText(PC[((s3 % 12) + 12) % 12] + (Math.floor(s3 / 12) - 1), 4 * V, Y(s3)); }
    }
    function path(arr, k, a, lw, dash) { g.strokeStyle = K.c(k, a); g.lineWidth = lw * V; if (dash) g.setLineDash(dash.map(function (d) { return d * V; }));
      g.beginPath(); for (var j = 0; j < arr.length; j += 2) { var px = X(sim.t[j]), py = Y(arr[j]); j ? g.lineTo(px, py) : g.moveTo(px, py); } g.stroke(); g.setLineDash([]); }
    path(sim.target, 'grid', .9, 1.2, [4, 3]);                         // what the hand asked
    if (opts.quant) path(sim.x, 'physics', .55, 1.2);                   // the law, running underneath
    path(sim.y, 'value', .22, 8); path(sim.y, 'value', 1, 2.2);         // what is heard: the beam
    var to = sim.target[sim.target.length - 1], from = sim.target[0], sg = to >= from ? 1 : -1, bi = -1, be = 0;
    for (i = 0; i < sim.y.length; i++) { var e3 = (sim.y[i] - to) * sg; if (e3 > be) { be = e3; bi = i; } }
    if (bi >= 0 && be > .005) { g.fillStyle = K.c('marker', .3); g.beginPath(); g.arc(X(sim.t[bi]), Y(sim.y[bi]), 8 * V, 0, 2 * Math.PI); g.fill();
      g.fillStyle = K.c('marker'); g.beginPath(); g.arc(X(sim.t[bi]), Y(sim.y[bi]), 4 * V, 0, 2 * Math.PI); g.fill(); }
  };
  Viz.Glide = Glide;

  /* ---------- SCALE SELECTOR: root + a 12-bit mask; the mask is the truth, the name a convenience ---------- */
  function ScalePicker(el, opts) {
    opts = opts || {};
    var st = { root: opts.root || 0, mask: (opts.mask || Glide.mask(Glide.SCALES[1][1])).slice(), skin: opts.skin || 'global', played: opts.played === undefined ? 0 : opts.played, visual: opts.visual || 'piano', fifths: false };
    el.classList.add('hd-scale'); el.innerHTML =
      '<div class="hd-row" style="gap:6px"><label class="hd-label">Root <select class="hd-select hd-focus sc-root" aria-label="Scale root"></select></label>' +
      '<select class="hd-select hd-focus sc-name" aria-label="Scale"></select>' +
      '<div class="hd-pills sc-skin" role="radiogroup" aria-label="Scale view"><button class="hd-pill hd-focus" role="radio" data-v="global">GLOBAL</button><button class="hd-pill hd-focus" role="radio" data-v="offset">OFFSET</button></div></div>' +
      (opts.variants ? '<div class="hd-row" style="gap:6px"><span class="hd-label">Visual</span><div class="hd-pills sc-vis" role="radiogroup" aria-label="Scale visual">' +
        [['circles', 'key circles'], ['piano', 'piano'], ['ring', 'ring'], ['steps', 'steps']].map(function (v) { return '<button class="hd-pill hd-focus" role="radio" data-v="' + v[0] + '">' + v[1] + '</button>'; }).join('') +
        '</div><label class="hd-label sc-fifths" style="display:none;gap:6px;align-items:center">Fifths <button class="hd-toggle hd-focus" role="switch" aria-checked="false" aria-label="Circle of fifths order"></button></label></div>' : '') +
      '<div class="sc-keys" role="group" aria-label="Scale notes"></div><div class="hd-hint sc-pattern"></div><div class="hd-hint sc-note"></div>';
    var rootSel = el.querySelector('.sc-root'), nameSel = el.querySelector('.sc-name'), keys = el.querySelector('.sc-keys'), note = el.querySelector('.sc-note');
    PC.forEach(function (n, i) { rootSel.insertAdjacentHTML('beforeend', '<option value="' + i + '">' + n + '</option>'); });
    Glide.SCALES.forEach(function (sc, i) { nameSel.insertAdjacentHTML('beforeend', '<option value="' + i + '">' + sc[0] + '</option>'); });
    nameSel.insertAdjacentHTML('beforeend', '<option value="-1">custom</option>');
    var INT = ['1', '♭2', '2', '♭3', '3', '4', '♭5', '5', '♭6', '6', '♭7', '7'];
    function emit() { if (opts.onChange) opts.onChange({ root: st.root, mask: st.mask.slice() }); }
    function toggle(rel) {
      if (rel === 0) { note.textContent = 'The root always stays in the scale.'; return; }
      if (st.mask[rel] && st.mask.reduce(function (a, b) { return a + b; }, 0) <= 1) return;
      st.mask[rel] = st.mask[rel] ? 0 : 1; draw(); emit();
    }
    function draw() {
      rootSel.value = st.root; rootSel.disabled = st.skin === 'offset';
      var match = -1; Glide.SCALES.forEach(function (sc, i) { var m = Glide.mask(sc[1]); if (m.every(function (b, j) { return b === st.mask[j]; })) match = i; });
      nameSel.value = match;
      el.querySelectorAll('.sc-skin button').forEach(function (b) { b.setAttribute('aria-checked', String(b.dataset.v === st.skin)); });
      var vis = st.skin === 'global' ? st.visual : 'offset';
      keys.className = 'sc-keys ' + (vis === 'piano' ? 'global' : vis); keys.innerHTML = '';
      el.querySelectorAll('.sc-vis button').forEach(function (b) { b.setAttribute('aria-checked', String(b.dataset.v === st.visual)); });
      var ff = el.querySelector('.sc-fifths'); if (ff) { ff.style.display = st.visual === 'ring' && st.skin === 'global' ? 'inline-flex' : 'none'; ff.querySelector('button').setAttribute('aria-checked', String(st.fifths)); }
      var WH = [0, 2, 4, 5, 7, 9, 11], BL = [[1, 0], [3, 1], [6, 3], [8, 4], [10, 5]];
      if (vis === 'piano') {
        WH.forEach(function (pc, i) { key(pc, 'w', 'left:calc(' + i + ' * (100% / 7))'); });
        BL.forEach(function (b) { key(b[0], 'b', 'left:calc(' + (b[1] + 1) + ' * (100% / 7) - 14px)'); });
      } else if (vis === 'circles') {   // (a) the human's idea: twelve circles laid out like the keys, two staggered rows
        WH.forEach(function (pc, i) { key(pc, 'cw', 'left:calc(' + i + ' * (100% / 7) + (100% / 14) - 14px)'); });
        BL.forEach(function (b) { key(b[0], 'cb', 'left:calc(' + (b[1] + 1) + ' * (100% / 7) - 14px)'); });
      } else if (vis === 'ring') {      // (c) twelve dots on a circle: chromatic, or the circle of fifths
        var order = st.fifths ? [0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5] : [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], pos = {}, pts = [];
        order.forEach(function (pc, i) { var a = -Math.PI / 2 + i * Math.PI / 6; pos[pc] = [50 + 38 * Math.cos(a), 50 + 38 * Math.sin(a)]; });
        order.forEach(function (pc) { if (st.mask[((pc - st.root) % 12 + 12) % 12]) pts.push(pos[pc][0] + ',' + pos[pc][1]); });
        keys.insertAdjacentHTML('beforeend', '<svg viewBox="0 0 100 100" aria-hidden="true" style="position:absolute;inset:0;width:100%;height:100%"><polygon points="' + pts.join(' ') + '" style="fill:none;stroke:var(--ink);stroke-width:.8;opacity:.35"/></svg>');
        order.forEach(function (pc) { key(pc, 'rd', 'left:calc(' + pos[pc][0] + '% - 14px);top:calc(' + pos[pc][1] + '% - 14px)'); });
      } else if (vis === 'steps') {     // (d) a narrow strip: one cell per pitch class from the root, lit cells raised
        for (var r2 = 0; r2 < 12; r2++) { var pc2 = (st.root + r2) % 12; step(pc2, r2); }
      } else for (var r = 0; r < 12; r++) dom(r);
      var pat = [], prev = 0; for (var q2 = 1; q2 <= 12; q2++) if (q2 === 12 || st.mask[q2]) { var gap = q2 - prev; pat.push(gap === 1 ? 'H' : gap === 2 ? 'W' : gap === 3 ? 'W+H' : gap + ' st'); prev = q2; }
      el.querySelector('.sc-pattern').textContent = 'steps: ' + pat.join(' · ');
      note.textContent = st.skin === 'global' ? 'GLOBAL: pitches are places; the root wears the star.' : 'OFFSET: intervals are distances from the note you play; the root has no meaning here.';
    }
    function key(pc, cls, pos) {
      var rel = ((pc - st.root) % 12 + 12) % 12, on = !!st.mask[rel], b = document.createElement('button');
      b.type = 'button'; b.className = 'sk ' + cls + (on ? ' on' : '') + (rel === 0 ? ' root' : ''); b.style.cssText = pos;
      b.setAttribute('aria-pressed', String(on)); b.setAttribute('aria-label', PC[pc] + (rel === 0 ? ', root' : '') + (on ? ', in the scale' : ', not in the scale'));
      b.innerHTML = '<span>' + (rel === 0 ? '★' : '') + PC[pc] + '</span>'; b.addEventListener('click', function () { toggle(rel); }); keys.appendChild(b);
    }
    function step(pc, rel) {
      var on = !!st.mask[rel], b = document.createElement('button'); b.type = 'button'; b.className = 'st' + (on ? ' on' : '') + (rel === 0 ? ' root' : '');
      b.setAttribute('aria-pressed', String(on)); b.setAttribute('aria-label', PC[pc] + (rel === 0 ? ', root' : '') + (on ? ', in the scale' : ', not in the scale'));
      b.innerHTML = '<i></i><span>' + (rel === 0 ? '★' : '') + PC[pc] + '</span>'; b.addEventListener('click', function () { toggle(rel); }); keys.appendChild(b);
    }
    function dom(rel) {
      var on = !!st.mask[rel], b = document.createElement('button'); b.type = 'button'; b.className = 'sd' + (on ? ' on' : '') + (rel === 0 ? ' played' : '');
      b.setAttribute('aria-pressed', String(on)); b.setAttribute('aria-label', rel === 0 ? 'the played note' : INT[rel] + (on ? ', in the scale' : ', not in the scale'));
      b.textContent = rel === 0 ? '▶' : INT[rel]; b.addEventListener('click', function () { toggle(rel); }); keys.appendChild(b);
    }
    rootSel.addEventListener('change', function () { st.root = +rootSel.value; draw(); emit(); });
    nameSel.addEventListener('change', function () { var i = +nameSel.value; if (i >= 0) { st.mask = Glide.mask(Glide.SCALES[i][1]); draw(); emit(); } });
    el.querySelector('.sc-skin').addEventListener('click', function (e) { var b = e.target.closest('button'); if (!b) return; st.skin = b.dataset.v; draw(); });
    var vs = el.querySelector('.sc-vis'); if (vs) vs.addEventListener('click', function (e) { var b = e.target.closest('button'); if (!b) return; st.visual = b.dataset.v; if (st.skin !== 'global') st.skin = 'global'; draw(); });
    var fb = el.querySelector('.sc-fifths button'); if (fb) fb.addEventListener('click', function () { st.fifths = !st.fifths; draw(); });
    draw();
    return { get: function () { return { root: st.root, mask: st.mask.slice() }; }, set: function (r, m) { st.root = r; st.mask = m.slice(); draw(); } };
  }

  /* ---------- a plain slider row (the house slider without the morph marks), for lab-style panels ---------- */
  function PlainSlider(el, o) {
    var lo = o.min, hi = o.max, toN = function (v) { return o.log ? Math.log(v / lo) / Math.log(hi / lo) : (v - lo) / (hi - lo); }, fromN = function (n) { return o.log ? lo * Math.pow(hi / lo, n) : lo + (hi - lo) * n; };
    var v = o.value;
    el.className = 'hd-srow'; el.innerHTML = '<label class="hd-label">' + o.name + '</label><div class="hd-slider hd-focus" role="slider" tabindex="0" aria-label="' + (o.path || o.name) + '"><div class="track"><i class="fill"></i></div><i class="handle"></i></div><output class="hd-value" style="text-align:right"></output>';
    var sl = el.querySelector('.hd-slider'), out = el.querySelector('output');
    function paint(fire) { var n = toN(v), W = sl.getBoundingClientRect().width || 120; sl.style.setProperty('--v', n); sl.style.setProperty('--f0', '0px'); sl.style.setProperty('--fw', Math.max(0, 6.5 + n * (W - 13) - 1.5) + 'px');
      out.textContent = o.fmt(v); sl.setAttribute('aria-valuetext', o.fmt(v)); if (fire && o.onInput) o.onInput(v); }
    var x0 = 0, n0 = 0, k0 = 1, drag = false;
    sl.addEventListener('pointerdown', function (e) { try { sl.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ } drag = true; sl.classList.add('drag'); x0 = e.clientX; n0 = toN(v); k0 = e.shiftKey ? .25 : 1; });
    /* re-anchored at the stops and on a Shift change, as the house gesture (ADR-340 r2) */
    sl.addEventListener('pointermove', function (e) { if (!drag) return; var W = sl.getBoundingClientRect().width - 13, k = e.shiftKey ? .25 : 1;
      if (k !== k0) { n0 = toN(v); x0 = e.clientX; k0 = k; }
      var raw = n0 + (e.clientX - x0) / W * k, n = clamp(raw); if (raw !== n) { n0 = n; x0 = e.clientX; }
      v = fromN(n); paint(true); });
    var slEnd = function (e) { drag = false; sl.classList.remove('drag'); if (e && sl.hasPointerCapture(e.pointerId)) sl.releasePointerCapture(e.pointerId); };
    sl.addEventListener('pointerup', slEnd); sl.addEventListener('pointercancel', slEnd); sl.addEventListener('lostpointercapture', slEnd);
    sl.addEventListener('dblclick', function () { v = o.def === undefined ? o.value : o.def; paint(true); });
    sl.addEventListener('keydown', function (e) { var s = e.shiftKey ? .0025 : .01, n = toN(v); if (e.key === 'ArrowRight' || e.key === 'ArrowUp') n += s; else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') n -= s; else return; e.preventDefault(); v = fromN(clamp(n)); paint(true); });
    paint(false); addEventListener('resize', function () { paint(false); });
    return { get: function () { return v; } };
  }
  /* ---------- STEPPER: the house stepper (ADR-338 r2's TC1 / TC2 steppers, gestures ADR-340 r5, drawing SG-b ADR-341 r3) ----------
     The human, 2026-10-30: "Steppers need to be draggable, possibly even able to accept number input", then 2026-10-31:
     "double clicking returns to home while option clicking allows you to type". So, on any stepper:
       drag the value up / down   one step per 6 px (Shift: per 24 px), re-anchoring at the ends (no dead range)
       click ‹ ›                   one step (the mouse route; the flat form has no arrows)
       double-click anywhere       the default (home), as on a knob
       Option-click                a NUMERIC stepper opens a number field in the same box: Enter or Tab commits, Escape cancels
                                   (the innermost layer of the layered Escape, ADR-338 r11, so it never reaches the page), a
                                   click away commits. Out of range clamps and says so; text that is no number is refused and said.
                                   A stepper of NAMED options opens a DROPDOWN of those options instead (ADR-341 r3: "it doesn't
                                   make sense to be able to type in your own text"): ↑ ↓ Home End move, Enter commits, Escape
                                   cancels, a click on an option commits, a click away cancels. There is no typed-name entry.
       Alt + wheel                 one step (B374: the wheel only with Option)
       ↑ ↓ in keyboard mode        one step; Shift ten; PgUp / PgDn ten; Home / End the ends; Enter or F2 the field or dropdown
     SG-b (the drawing): a small ⇕ glyph beside the value says it drags, and a STEP SCALE under the face shows where the value sits
     in its range, with HOME the ringed tick (one tick per step up to 25, a bar beyond). The scale lies on the face's 2 px foot in
     the flat table form (a 17 px row has no room below). The value box is as wide as the widest value, so nothing reflows as it
     changes (H1, ADR-340 r4).
     o: { name, path, options: [names] | { min, max, step }, value, def, fmt(v), spoken(v) (aria-valuetext where the display
     is too short to say it), onInput(v), say(msg), flat (TC2's table form, K4), sm (the dense 20 px face), tc (TC1's 16 px
     face), keys() (keyboard mode, default off) }. The range is fixed at mount: a stepper whose range changes is remounted. */
  var CUE = '<svg class="st-cue" viewBox="0 0 6 10" width="6" height="10" aria-hidden="true"><path d="M3 .8 5.4 3.6H.6ZM3 9.2 5.4 6.4H.6Z"/></svg>';
  function Stepper(el, o) {
    var named = !!(o.options && o.options.length), min = named ? 0 : (o.min || 0), max = named ? o.options.length - 1 : (o.max === undefined ? 1 : o.max);
    var step = named ? 1 : (o.step || 1), v = o.value === undefined ? min : o.value, def = o.def === undefined ? v : o.def;
    var fmt = o.fmt || function (x) { return named ? o.options[x] : String(+x.toFixed(6)); }, say = o.say || function () {};
    var keys = o.keys || function () { return false; };
    function q(x) { return Math.min(max, Math.max(min, min + Math.round((x - min) / step) * step)); }
    var CHL = '<svg viewBox="0 0 8 8" aria-hidden="true"><path d="M6 0v8L1 4z"/></svg>', CHR = '<svg viewBox="0 0 8 8" aria-hidden="true"><path d="M2 0v8l5-4z"/></svg>';
    el.classList.add('hd-stepper', 'hd-step', 'hd-focus'); if (o.flat) el.classList.add('hd-step-flat'); if (o.sm) el.classList.add('hd-sm'); if (o.tc) el.classList.add('hd-tc');
    el.innerHTML = (o.flat ? '' : '<button type="button" class="st-dn" tabindex="-1" aria-label="previous">' + CHL + '</button>') + '<output></output>' + CUE +
      (o.flat ? '' : '<button type="button" class="st-up" tabindex="-1" aria-label="next">' + CHR + '</button>') + '<span class="st-scale" aria-hidden="true"></span>';
    el.setAttribute('role', 'spinbutton'); el.setAttribute('aria-label', o.path || o.name || 'stepper');
    el.title = (o.path || o.name || '') + ' · drag, double-click: default' + (named ? ', ⌥-click: choose' : ', ⌥-click: type');
    var out = el.querySelector('output'), scale = el.querySelector('.st-scale'), wide = 0, n;
    if (named) o.options.forEach(function (s) { wide = Math.max(wide, String(s).length); });
    else for (n = min; n <= max + 1e-9 && wide < 12; n += Math.max(step, (max - min) / 64)) wide = Math.max(wide, fmt(q(n)).length);
    out.style.minWidth = (wide + 1) + 'ch';
    var count = Math.round((max - min) / step) + 1;
    /* the step scale, redrawn on a paint: the current tick wide and ink, home ringed (when it is not the current), the rest hint */
    function drawScale() {
      var cur = Math.round((v - min) / step), home = Math.round((q(def) - min) / step), h = '', i, x;
      if (count <= 25) for (i = 0; i < count; i++) {
        x = count === 1 ? 50 : 2 + 96 * i / (count - 1);
        h += '<rect x="' + (x - (i === cur ? 2.2 : .5)) + '" y="' + (i === cur ? 0 : 1.5) + '" width="' + (i === cur ? 4.4 : 1) + '" height="' + (i === cur ? 4 : 2.5) + '" fill="var(--' + (i === cur ? 'ink' : 'hint') + ')"/>';
        if (i === home && i !== cur) h += '<rect x="' + (x - 1.6) + '" y=".2" width="3.2" height="3.6" rx=".6" fill="none" stroke="var(--ink)" stroke-width=".8" vector-effect="non-scaling-stroke"/>';
      } else {
        h = '<rect x="0" y="1.5" width="100" height="1" fill="var(--hint)"/><rect x="' + (100 * cur / (count - 1) - 1.5) + '" y="0" width="3" height="4" fill="var(--ink)"/>';
        x = 100 * home / (count - 1); if (home !== cur) h += '<rect x="' + (x - 1.6) + '" y=".2" width="3.2" height="3.6" rx=".6" fill="none" stroke="var(--ink)" stroke-width=".8" vector-effect="non-scaling-stroke"/>';
      }
      scale.innerHTML = '<svg viewBox="0 0 100 4" preserveAspectRatio="none">' + h + '</svg>';
    }
    function set(x, quiet) { var nv = q(x); if (nv === v && quiet) return; v = nv; paint(); if (o.onInput) o.onInput(v); }
    function paint() { out.textContent = fmt(v); el.setAttribute('aria-valuenow', v); el.setAttribute('aria-valuetext', o.spoken ? o.spoken(v) : fmt(v)); el.setAttribute('aria-valuemin', min); el.setAttribute('aria-valuemax', max); el.tabIndex = keys() ? 0 : -1; el.classList.toggle('home', v === q(def)); drawScale(); }
    var b = el.querySelectorAll('button');
    if (b.length) { b[0].addEventListener('click', function (e) { if (!e.altKey) set(v - step, true); }); b[1].addEventListener('click', function (e) { if (!e.altKey) set(v + step, true); }); }
    var st = null, field = null, menu = null;
    el.addEventListener('pointerdown', function (e) {
      if (e.button !== 0 || field || menu || (e.target.closest && e.target.closest('button'))) return;
      if (e.altKey) { e.preventDefault(); open(); return; }
      try { el.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
      st = { y: e.clientY, v0: v, px: e.shiftKey ? 24 : 6 }; el.classList.add('drag');
    });
    el.addEventListener('pointermove', function (e) {
      if (!st) return; var px = e.shiftKey ? 24 : 6;
      if (px !== st.px) { st.y = e.clientY; st.v0 = v; st.px = px; }
      var raw = st.v0 + Math.trunc((st.y - e.clientY) / px) * step;
      if (raw > max || raw < min) { st.v0 = raw > max ? max : min; st.y = e.clientY; }   // re-anchor at the ends
      set(raw, true);
    });
    function end(e) { if (e && e.pointerId !== undefined && el.hasPointerCapture && el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId); st = null; el.classList.remove('drag'); }
    el.addEventListener('pointerup', end); el.addEventListener('pointercancel', end); el.addEventListener('lostpointercapture', end);
    el.addEventListener('dblclick', function (e) { if (field || menu || e.altKey) return; e.preventDefault(); set(def, true); say((o.name || 'Value') + ': default, ' + fmt(v)); });
    el.addEventListener('wheel', function (e) { if (!e.altKey) return; e.preventDefault(); set(v + (e.deltaY < 0 ? step : -step), true); }, { passive: false });   // B374
    function open() { if (field || menu) return; if (named) openMenu(); else openField(); }
    /* the number field (numeric steppers only) */
    function openField() {
      field = document.createElement('input'); field.className = 'st-field'; field.type = 'text'; field.value = fmt(v);
      field.setAttribute('aria-label', (o.path || o.name || 'value') + ', type a value'); field.style.width = out.style.minWidth;
      out.replaceWith(field); field.focus(); field.select();
      var done = false;
      function close(commit) {
        if (done) return; done = true;
        if (commit) { var t = field.value.trim().replace(/−/g, '-'), nv = null;
          if (t !== '' && isFinite(+t)) nv = +t;
          if (nv === null) say('"' + t + '" is not a number: ' + (o.name || 'this control') + ' unchanged');
          else { if (nv > max || nv < min) say((o.name || 'Value') + ': ' + t + ' is out of range, set to ' + fmt(q(nv))); set(nv, false); } }
        field.replaceWith(out); field = null; paint();
      }
      field.addEventListener('keydown', function (e) {
        if (e.key === 'Enter' || e.key === 'Tab') { close(true); if (e.key === 'Enter') e.preventDefault(); }
        else if (e.key === 'Escape') { close(false); e.preventDefault(); e.stopPropagation(); }
        e.stopPropagation();   // a typed digit is text here, never a map key
      });
      field.addEventListener('blur', function () { close(true); });
    }
    /* the dropdown (named steppers): a listbox of the options under the face, fixed so it never moves layout; the highlight
       starts on the current value and moves with the arrows; only Enter or a click on an option commits */
    function openMenu() {
      var hi = v, done = false, r = el.getBoundingClientRect();
      menu = document.createElement('div'); menu.className = 'st-menu'; menu.setAttribute('role', 'listbox'); menu.tabIndex = -1;
      menu.setAttribute('aria-label', (o.path || o.name || 'value') + ', choose an option');
      var items = o.options.map(function (s, i) {
        var it = document.createElement('div'); it.className = 'st-opt'; it.setAttribute('role', 'option'); it.id = 'st-o' + Math.random().toString(36).slice(2, 8) + i; it.textContent = String(s);
        it.addEventListener('pointermove', function () { light(i); });
        it.addEventListener('pointerdown', function (e) { e.preventDefault(); close(true, i); });   // preventDefault keeps focus on the menu, so the blur below does not cancel first
        menu.appendChild(it); return it;
      });
      function light(i) { hi = Math.min(max, Math.max(min, i)); items.forEach(function (it, k) { it.classList.toggle('on', k === hi); it.setAttribute('aria-selected', String(k === v)); }); menu.setAttribute('aria-activedescendant', items[hi].id); items[hi].scrollIntoView({ block: 'nearest' }); }
      function close(commit, pick) {
        if (done) return; done = true; removeEventListener('pointerdown', away, true);
        var m = menu; menu = null; m.remove();
        if (commit) { set(pick === undefined ? hi : pick, false); say((o.name || 'Value') + ': ' + fmt(v)); }
        el.focus({ preventScroll: true });
      }
      function away(e) { if (!menu.contains(e.target)) close(false); }   // a click away cancels: nothing was typed to commit
      menu.addEventListener('keydown', function (e) {
        var k = e.key;
        if (k === 'ArrowDown') light(hi + 1); else if (k === 'ArrowUp') light(hi - 1); else if (k === 'Home') light(min); else if (k === 'End') light(max);
        else if (k === 'PageDown') light(hi + 5); else if (k === 'PageUp') light(hi - 5);
        else if (k === 'Enter') close(true); else if (k === 'Tab') close(true);
        else if (k === 'Escape') close(false);
        else { e.stopPropagation(); return; }   // any other key is swallowed: a digit is never a map key, and there is no typing here
        e.preventDefault(); e.stopPropagation();   // Escape included: the dropdown is the innermost layer of the layered Escape
      });
      menu.addEventListener('blur', function () { close(false); });
      document.body.appendChild(menu);
      var mh = menu.getBoundingClientRect().height, top = r.bottom + 4;
      if (top + mh > innerHeight - 8 && r.top - mh - 4 > 8) top = r.top - mh - 4;   // flip above when there is no room below
      menu.style.left = Math.round(Math.min(r.left, innerWidth - 8 - Math.max(menu.getBoundingClientRect().width, 0))) + 'px'; menu.style.top = Math.round(top) + 'px';
      menu.style.minWidth = Math.round(r.width) + 'px';
      light(v); menu.focus({ preventScroll: true });
      addEventListener('pointerdown', away, true);
    }
    el.addEventListener('keydown', function (e) {
      if (field || menu || !keys()) return; var k = e.key, big = 10 * step;
      if (k === 'ArrowUp' || k === 'ArrowRight') set(v + (e.shiftKey ? big : step), true); else if (k === 'ArrowDown' || k === 'ArrowLeft') set(v - (e.shiftKey ? big : step), true);
      else if (k === 'PageUp') set(v + big, true); else if (k === 'PageDown') set(v - big, true); else if (k === 'Home') set(min, true); else if (k === 'End') set(max, true);
      else if (k === 'Enter' || k === 'F2') open(); else return;
      e.preventDefault();
    });
    paint();
    return { get: function () { return v; }, set: function (x) { v = q(x); paint(); }, open: open, paint: paint };
  }
  window.Horde.Glide = Glide; window.Horde.ScalePicker = ScalePicker; window.Horde.PlainSlider = PlainSlider; window.Horde.Stepper = Stepper;
})();
/* ---------- THE MOD MATRIX FAMILY (B531, ADR-266 rulings 1-4) ----------
   The mod matrix lab's components on the house Session: the grid, the trace lanes, the compact wires,
   the route rows, the ONE source strip (shared with the MAP control), the "what is moving it" ledger
   and a hosted module's read-only macro rows. Source: docs/design/mod-matrix-lab.html.
   Everything reads the Session (S.mappings, S.sources, S.output's parts); nothing here keeps a second
   copy of a mapping. Two PLACEHOLDERS live here, both documented in the READMEs:
   - depth routes (the lab's B70/B207 proposal, one level): S.depthRoutes = [{ src, pol, depth, of: m }],
     which the core's depthOf does not read yet (see depthNow);
   - the greyed row of a silent corner (ADR-266 r4): a mapping's optional `corner` (0-3), display only. */
(function (H) {
  'use strict';
  var POLS = ['uni', 'bi', 'inv', 'ctl'], POLN = { uni: 'UNI', bi: 'BI', inv: 'INV', ctl: 'CTL' };
  var POLHINT = { uni: 'unipolar: adds 0 to +depth', bi: 'bipolar: adds −depth to +depth', inv: 'inverse: adds −depth to 0',
    ctl: 'Control: sweeps the parameter between its low and high ends; its own value is ignored' };
  var DRAG = 160, DEF_DEPTH = .25;                    // size-drag; the Map Mode spec's default depth (ADR-261 r3)
  var XSVG = '<svg viewBox="0 0 10 10" aria-hidden="true"><path d="M2 1 5 4 8 1 9 2 6 5 9 8 8 9 5 6 2 9 1 8 4 5 1 2Z"/></svg>';
  var LOCK = '<svg viewBox="0 0 10 10" width="9" height="9" aria-hidden="true" class="mx-lock"><path d="M3 4.6V3.3a2 2 0 0 1 4 0v1.3" style="fill:none;stroke:currentColor;stroke-width:1.3"/><rect x="1.8" y="4.6" width="6.4" height="4.6" rx="1" style="fill:currentColor"/></svg>';
  function clamp(x, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, x)); }
  function v(n) { return 'var(--' + n + ')'; }
  function pct(x) { return Math.round(x * 100) + '%'; }
  /* signed percent; a true minus sign, and no sign on zero, so the width of the readout never jumps */
  function spct(x) { var r = Math.round(x * 100); return (r > 0 ? '+' : r < 0 ? '−' : '') + Math.abs(r) + '%'; }
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function srcColor(s) { return s ? v('src-' + s.kind) : v('hint'); }
  function el(tag, cls, html) { var e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; }
  function num(S, id) { for (var i = 0; i < S.sources.length; i++) if (S.sources[i].id === id) return i + 1; return 0; }
  function pname(S, key) { var p = S.params[key]; return p ? p.name : key; }

  /* ---------- the arithmetic the views show (the core's, plus the depth-route placeholder) ----------
     With no depth routes, push(m) === S.contrib(m) and now(p) === S.output(p): the matrix and the knob agree.
     DEPTH ROUTES (PLACEHOLDER, the lab's B70 answer): a route whose destination is another mapping's DEPTH;
     contributions sum onto that depth, clamped to 0..1 (the depth is a magnitude, ADR-266 r2); one level only;
     uni/bi/inv only (a Control has a range, not a depth). The core's depthOf does not read them yet, so until the
     integrator adds that, a knob's rings show the authored depth while these views show the moved one. */
  function dRoutes(S) { if (!S.depthRoutes) S.depthRoutes = []; return S.depthRoutes; }
  function routesOn(S, m) { return dRoutes(S).filter(function (d) { return d.of === m; }); }
  function polar(pol, s, d) { return pol === 'uni' ? s * d : pol === 'bi' ? (2 * s - 1) * d : pol === 'inv' ? -s * d : 0; }
  function dPush(S, d) { return polar(d.pol, S.sourceValue(d.src), d.depth); }
  function depthNow(S, m) { var r = S.depthOf(m), l = routesOn(S, m); for (var i = 0; i < l.length; i++) r += dPush(S, l[i]); return l.length ? clamp(r) : r; }
  function depthReach(S, m) {
    var b = S.depthOf(m), lo = b, hi = b;
    routesOn(S, m).forEach(function (d) { if (d.pol === 'uni') hi += d.depth; else if (d.pol === 'bi') { lo -= d.depth; hi += d.depth; } else lo -= d.depth; });
    return [clamp(lo), clamp(hi)];
  }
  function push(S, m) { return m.pol === 'ctl' ? 0 : polar(m.pol, S.sourceValue(m.src), depthNow(S, m)); }
  function ctlPos(S, m) { return m.lo + S.sourceValue(m.src) * (m.hi - m.lo); }
  function raw(S, p) { var r = S.origin(p); S.mapsOf(p.key).forEach(function (m) { r += push(S, m); }); return r; }
  function now(S, p) { return clamp(raw(S, p)); }
  /* the reach: where the additive mappings can take the parameter from where they start, at today's depths */
  function reach(S, p) {
    var o = S.origin(p), lo = o, hi = o;
    S.mapsOf(p.key).forEach(function (m) { if (m.pol === 'ctl') return; var d = depthNow(S, m);
      if (m.pol === 'uni') hi += d; else if (m.pol === 'bi') { lo -= d; hi += d; } else lo -= d; });
    return [clamp(lo), clamp(hi)];
  }
  /* SILENT CORNER (PLACEHOLDER, ADR-266 r4): mappings are global for 1.0; a corner-level mapping would carry `corner`.
     Its row greys when that corner's morph weight is zero. Display only: its arithmetic is not ruled. */
  function silent(S, m) { return m.corner !== undefined && m.corner !== null && S.weights()[m.corner] < .001; }
  function amount(S, m) { return S.amount ? S.amount(m) : m.pol === 'ctl' ? pct(m.lo) + '–' + pct(m.hi) : pct(S.depthOf(m)); }
  /* a cheap signature of the mapping structure; views rebuild only when it changes */
  function sig(S) {
    var a = S.mappings.map(function (m) { return m.param + '|' + m.src + '|' + m.pol + '|' + (m.corner === undefined ? '' : m.corner); }).join(',');
    var b = dRoutes(S).map(function (d) { return S.mappings.indexOf(d.of) + '|' + d.src + '|' + d.pol; }).join(',');
    return a + '#' + b;
  }
  function removeMapping(S, m) {   // the lab's port trap, closed: a mapping's depth routes go with it
    var l = dRoutes(S); for (var i = l.length - 1; i >= 0; i--) if (l[i].of === m) l.splice(i, 1);
    S.remove(m);
  }
  function cyclePolOf(S, m, isDepthRoute, back) {
    if (!isDepthRoute) { S.cyclePol(m, back); return; }
    m.pol = POLS[(POLS.indexOf(m.pol) + (back ? 2 : 1)) % 3];   // a depth route: UNI, BI, INV (CTL has no meaning on a depth)
    S.say(S.src(m.src).name + ' → depth of ' + S.src(m.of.src).name + ' → ' + pname(S, m.of.param) + ' · ' + POLN[m.pol]);
  }
  function addDepthRoute(S, m) {
    if (m.pol === 'ctl') { S.say('A Control has a range, not a depth: depth routes modulate UNI, BI and INV mappings'); return null; }
    var src = S.map.src || (S.sources[0] && S.sources[0].id);
    var d = { src: src, pol: 'uni', depth: DEF_DEPTH, of: m }; dRoutes(S).push(d);
    S.say('Depth route: ' + S.src(src).name + ' now moves the depth of ' + S.src(m.src).name + ' → ' + pname(S, m.param) + ' · UNI · 25%');
    return d;
  }

  /* one relative drag: start(e) returns a state or null; move(st, delta in px along the axis, e); end(st, moved).
     Pointer capture is released on every exit path (the tests fail on a leaked capture).
     RE-ANCHORED (ADR-340 r2), like the Session gesture and PlainSlider: the delta handed to move() is the ACCUMULATED effective
     delta, so a Shift change mid-drag changes the rate from here on without jumping the value, and move() may RETURN the
     delta it actually reached when it clamped at a stop: the drag restarts from that stop, so reversing moves at once and
     an overshoot is no dead range to unwind. */
  function drag(target, o) {
    var st = null;
    target.addEventListener('pointerdown', function (e) {
      if (e.button !== 0) return;
      st = o.start(e); if (!st) return;
      st.x = e.clientX; st.y = e.clientY; st.moved = false; st.pid = e.pointerId; st.acc = 0; st.d0 = 0; st.k = e.shiftKey ? .25 : 1;
      try { target.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
    });
    target.addEventListener('pointermove', function (e) {
      if (!st) return;
      var d = o.axis === 'x' ? e.clientX - st.x : st.y - e.clientY, k = e.shiftKey ? .25 : 1;
      if (Math.abs(d) > 3) st.moved = true; if (!st.moved) return;
      if (k !== st.k) { st.acc += (d - st.d0) * st.k; st.d0 = d; st.k = k; }
      var eff = st.acc + (d - st.d0) * k, got = o.move(st, eff, e);
      if (typeof got === 'number' && got !== eff) { st.acc = got; st.d0 = d; }
    });
    function end(e) {
      if (e && e.pointerId !== undefined && target.hasPointerCapture && target.hasPointerCapture(e.pointerId)) target.releasePointerCapture(e.pointerId);
      if (!st) return; var s = st; st = null; o.end(s, s.moved);
    }
    target.addEventListener('pointerup', end); target.addEventListener('pointercancel', end); target.addEventListener('lostpointercapture', end);
  }
  /* a view that redraws only when its key changes: reduced motion freezes S.t, so the key stops changing (one still frame) */
  function keyed(fn) { var last = null; return function (k) { if (k === last) return; last = k; fn(); }; }

  /* =====================================================================
     SOURCE STRIP — the ONE strip, for the matrix and for the MAP control. A chip per source: its kind's colour,
     its name and number, a live meter; a click selects it (S.setSrc). opts.counts adds how many routes read it.
     ===================================================================== */
  function SourceStrip(el0, S, opts) {
    opts = opts || {};
    el0.classList.add('hd-srcstrip'); el0.__S = S; el0.classList.add('mx-strip'); el0.innerHTML = '';
    el0.setAttribute('role', 'group'); el0.setAttribute('aria-label', opts.label || 'Modulation sources');
    var chips = {};
    S.sources.forEach(function (s, i) {
      var b = el('button', 'src-chip hd-focus'); b.type = 'button'; b.style.setProperty('--sc', srcColor(s)); b.dataset.src = s.id;
      b.innerHTML = '<span class="meter" aria-hidden="true"><i></i></span><span class="nm">' + esc(s.name) + '</span>' +
        (opts.counts ? '<span class="ct" aria-hidden="true"></span>' : '') + '<kbd>' + (i + 1) + '</kbd>';
      b.title = s.name + ' (' + s.kind + ', source ' + (i + 1) + ')';
      b.addEventListener('click', function () { S.setSrc(s.id); if (opts.onPick) opts.onPick(s.id); });
      el0.appendChild(b); chips[s.id] = { b: b, bar: b.querySelector('.meter i'), ct: b.querySelector('.ct'), s: s, n: i + 1 };
    });
    function used(id) { var n = 0; S.mappings.forEach(function (m) { if (m.src === id) n++; }); dRoutes(S).forEach(function (d) { if (d.src === id) n++; }); return n; }
    function sync() {
      for (var k in chips) { var c = chips[k], on = k === S.map.src, u = used(k);
        c.b.setAttribute('aria-pressed', String(on));
        if (c.ct) c.ct.textContent = u ? '◦' + u : '';
        c.b.setAttribute('aria-label', 'Source ' + c.n + ', ' + c.s.name + ', ' + c.s.kind + (opts.counts ? ', read by ' + u + ' route' + (u === 1 ? '' : 's') : '') + (on ? ', selected' : '')); }
    }
    S.on(sync); sync();
    var lastSig = sig(S);
    var view = { el: el0, sync: sync, render: function () {
      for (var k in chips) chips[k].bar.style.width = (S.sourceValue(k) * 100).toFixed(1) + '%';
      var g = sig(S); if (g !== lastSig) { lastSig = g; sync(); }
    } };
    S.add(view); return view;
  }

  /* =====================================================================
     MOD MATRIX GRID — sources × destinations. A cell: its authored depth as a fill rising in the source's kind
     colour, the live push as a signed bar on its floor, a dashed ring while its depth is itself modulated;
     a Control shows its swept band and the position. Panel UI (the lab's DOM well), not a screen.
     ===================================================================== */
  function ModMatrixGrid(el0, S, opts) {
    opts = opts || {};
    var dests = opts.dests || Object.keys(S.params).map(function (k) { return { key: k, name: S.params[k].name }; });
    var hosted = opts.hosted || [];
    el0.classList.add('mx-grid'); el0.__S = S; el0.innerHTML = '';
    var scroller = el('div', 'mx-gscroll hd-focus'); scroller.tabIndex = 0;
    scroller.setAttribute('role', 'region'); scroller.setAttribute('aria-label', 'Modulation matrix, sources by destinations' + (opts.label ? ': ' + opts.label : ''));
    var tbl = el('div', 'mx-gtable'); tbl.setAttribute('role', 'table'); tbl.setAttribute('aria-label', 'Modulation matrix');
    tbl.style.gridTemplateColumns = 'minmax(58px,104px) repeat(' + dests.length + ', minmax(30px,64px))';
    scroller.appendChild(tbl); el0.appendChild(scroller);
    var cap = el('div', 'mx-cap'); cap.setAttribute('aria-live', 'off'); el0.appendChild(cap);
    var head = el('div', 'mx-grow'); head.setAttribute('role', 'row'); tbl.appendChild(head);
    var corner = el('div', 'mx-gh mx-corner', 'src&thinsp;\\&thinsp;dst'); corner.setAttribute('role', 'columnheader'); corner.setAttribute('aria-label', 'sources by destinations'); head.appendChild(corner);
    var heads = {}, cells = [], rowHeads = {};
    dests.forEach(function (d) {
      var hc = el('div', 'mx-ghc'); hc.setAttribute('role', 'columnheader');
      var b = el('button', 'mx-gh hd-focus', '<span>' + esc(d.short || d.name) + '</span>'); b.type = 'button'; b.title = d.name;
      b.addEventListener('click', function () { S.selected = d.key; S.emit(); });
      hc.appendChild(b); head.appendChild(hc); heads[d.key] = b;
    });
    function cellEl(s, d, ro) {
      var c = el('button', 'mx-cell hd-focus' + (ro ? ' ro' : ''), '<span class="box"><i class="fill"></i><i class="band"></i><i class="pos"></i></span><span class="lane"><i class="mid"></i><i class="nowb"></i></span>');
      c.type = 'button'; c.style.setProperty('--sc', ro ? v('hint') : srcColor(s));
      var wrapC = el('div', 'mx-gc'); wrapC.setAttribute('role', 'cell'); wrapC.appendChild(c);
      return { wrap: wrapC, c: c, s: s, d: d, ro: ro };
    }
    S.sources.forEach(function (s, i) {
      var row = el('div', 'mx-grow'); row.setAttribute('role', 'row');
      var rh = el('div', 'mx-rh', '<i class="dot" style="--sc:' + srcColor(s) + '"></i><span class="nm">' + esc(s.name) + '</span><kbd>' + (i + 1) + '</kbd>');
      rh.setAttribute('role', 'rowheader'); rh.title = s.name + ' (' + s.kind + ')'; row.appendChild(rh); rowHeads[s.id] = rh;
      dests.forEach(function (d) { var x = cellEl(s, d, false); row.appendChild(x.wrap); cells.push(x); wire(x); });
      tbl.appendChild(row);
    });
    /* HOSTED MODULE MACROS (ADR-266 r3): visible, read-only, never cross-mapped: every cell refuses and says why */
    hosted.forEach(function (mod) {
      mod.macros.forEach(function (mc, j) {
        var row = el('div', 'mx-grow ro'); row.setAttribute('role', 'row');
        var rh = el('div', 'mx-rh ro', LOCK + '<span class="nm">' + esc(mod.name + ' ' + mc.name) + '</span>'); rh.setAttribute('role', 'rowheader');
        rh.title = mod.name + ' macro ' + (j + 1) + ', ' + mc.name + ': read-only, mapped inside ' + mod.name; row.appendChild(rh);
        dests.forEach(function (d) { var x = cellEl(null, d, true); x.mod = mod; x.mc = mc; row.appendChild(x.wrap); cells.push(x);
          x.c.setAttribute('aria-label', mod.name + ' ' + mc.name + ' → ' + d.name + ': read-only, ' + mod.name + "'s macros are mapped inside " + mod.name);
          x.c.addEventListener('click', function () { S.say(mod.name + "'s macros are mapped inside " + mod.name + ': they cannot be cross-mapped into horde (read-only here)'); }); });
        tbl.appendChild(row);
      });
    });
    function mapOf(x) { if (x.ro) return null; var l = S.mapsOf(x.d.key); for (var i = 0; i < l.length; i++) if (l[i].src === x.s.id) return l[i]; return null; }
    function describe(x) {
      var m = mapOf(x), p = S.params[x.d.key];
      if (x.ro) return x.mod.name + ' ' + x.mc.name + ' → ' + x.d.name + ': read-only (mapped inside ' + x.mod.name + ')';
      if (!m) { var c0 = S.ctlOf(x.d.key); return x.s.name + ' → ' + x.d.name + ': empty, click to map at 25%' + (c0 ? ' (controlled by ' + S.src(c0.src).name + ')' : ''); }
      var t = x.s.name + ' → ' + x.d.name + ' · ' + POLN[m.pol] + ' ' + amount(S, m);
      if (m.pol === 'ctl') t += ' · controlled, now ' + pct(ctlPos(S, m)); else t += ' · now ' + spct(push(S, m));
      if (routesOn(S, m).length) t += ' · depth moved to ' + pct(depthNow(S, m));
      if (silent(S, m)) t += ' · corner ' + H.CORNERS[m.corner].id + ' silent';
      return t + (p ? '' : '');
    }
    var hover = null;
    function wire(x) {
      var c = x.c;
      c.addEventListener('pointerenter', function () { hover = x; });
      c.addEventListener('pointerleave', function () { if (hover === x) hover = null; });
      c.addEventListener('focus', function () { hover = x; });
      c.addEventListener('blur', function () { if (hover === x) hover = null; });
      drag(c, { axis: 'y',
        start: function () {
          var m = mapOf(x), isNew = false, p = S.params[x.d.key];
          if (!m) { m = { param: x.d.key, src: x.s.id, pol: 'uni', depth: 0 }; S.mappings.push(m); isNew = true; S.focus[x.d.key] = S.mapsOf(x.d.key).length - 1; }
          var ai = p && m.cd ? S.editing(p) : null;
          S.lastMap = m; S.selected = x.d.key; S.emit();
          return { m: m, isNew: isNew, ai: ai, start: m.pol === 'ctl' ? [m.lo, m.hi] : (m.cd && ai !== null ? m.cd[ai] : m.depth) };
        },
        move: function (st, d) {
          var m = st.m, dv = d / DRAG, nv, from;
          if (m.pol === 'ctl') { var w = st.start[1] - st.start[0]; nv = clamp(st.start[0] + dv, 0, 1 - w); m.lo = nv; m.hi = nv + w; from = st.start[0]; }   // the band slides, its width kept
          else { from = st.start; nv = clamp(from + dv); if (m.cd && st.ai !== null) m.cd[st.ai] = nv; else m.depth = nv; }
          return (nv - from) * DRAG;   // the delta reached, for the re-anchor
        },
        end: function (st, moved) {
          var m = st.m;
          if (!moved && st.isNew) { m.depth = DEF_DEPTH; S.say('Mapped ' + x.s.name + ' → ' + x.d.name + ' · UNI · 25%'); }
          else if (moved && st.isNew && m.depth === 0) { S.mappings.splice(S.mappings.indexOf(m), 1); S.lastMap = null; S.say('Released at 0: no mapping made'); }
          else if (moved) S.say(x.s.name + ' → ' + x.d.name + ' · ' + POLN[m.pol] + ' ' + amount(S, m));
          S.emit();
        } });
      c.addEventListener('dblclick', function () {                     // double-click resets: the default depth, or the full sweep
        var m = mapOf(x); if (!m) return;
        if (m.pol === 'ctl') { m.lo = 0; m.hi = 1; } else if (m.cd) { var p = S.params[x.d.key], ai = p ? S.editing(p) : null; if (ai !== null) m.cd[ai] = DEF_DEPTH; } else m.depth = DEF_DEPTH;
        S.say(x.s.name + ' → ' + x.d.name + ' reset to ' + amount(S, m)); S.emit();
      });
      c.addEventListener('keydown', function (e) {                       // accelerators, keyboard mode only; each has a mouse route
        if (!S.keys) return; var m = mapOf(x);
        if (!m) return;
        if (e.key === 'ArrowUp' || e.key === 'ArrowDown') { var dv = (e.key === 'ArrowUp' ? 1 : -1) * (e.shiftKey ? .0025 : .01);
          if (m.pol === 'ctl') { var w = m.hi - m.lo; m.lo = clamp(m.lo + dv, 0, 1 - w); m.hi = m.lo + w; } else m.depth = clamp(m.depth + dv);
          e.preventDefault(); e.stopPropagation(); S.emit(); }
        else if (e.key === 'Backspace' || e.key === 'Delete') { removeMapping(S, m); e.preventDefault(); e.stopPropagation(); }
        else if (e.code === 'KeyP') { S.cyclePol(m); e.preventDefault(); e.stopPropagation(); }
      });
    }
    function paint() {
      el0.__paints = (el0.__paints || 0) + 1;
      var sel = S.selected;
      for (var k in heads) {
        var c0 = S.ctlOf(k), hb = heads[k];
        hb.setAttribute('aria-pressed', String(k === sel)); hb.classList.toggle('ctl', !!c0);
        hb.style.setProperty('--sc', c0 ? srcColor(S.src(c0.src)) : 'transparent');
        hb.setAttribute('aria-label', pname(S, k) + (c0 ? ', controlled by ' + S.src(c0.src).name : '') + (k === sel ? ', selected' : ', select this destination'));
      }
      for (var id in rowHeads) rowHeads[id].classList.toggle('on', id === S.map.src);
      cells.forEach(function (x) {
        var c = x.c; x.wrap.classList.toggle('colsel', x.d.key === sel);
        if (x.ro) { c.classList.add('ro'); return; }
        var m = mapOf(x);
        c.classList.toggle('on', !!m);
        ['uni', 'bi', 'inv', 'ctl'].forEach(function (pp) { c.classList.toggle(pp, !!m && m.pol === pp); });
        var dm = m && routesOn(S, m).length; c.classList.toggle('dmod', !!dm);
        c.classList.toggle('silent', !!m && silent(S, m));
        c.setAttribute('aria-label', describe(x));
        if (!m) return;
        if (dm) c.style.setProperty('--rc', srcColor(S.src(routesOn(S, m)[0].src)));
        if (m.pol === 'ctl') {
          var ps = ctlPos(S, m);
          c.style.setProperty('--lo', m.lo.toFixed(3)); c.style.setProperty('--hi', m.hi.toFixed(3)); c.style.setProperty('--pos', ps.toFixed(3));
          c.style.setProperty('--nl', '0'); c.style.setProperty('--nw', ps.toFixed(3));
        } else {
          var pu = push(S, m);
          c.style.setProperty('--d', clamp(S.depthOf(m)).toFixed(3));
          c.style.setProperty('--nl', (.5 + Math.min(0, pu) / 2).toFixed(3)); c.style.setProperty('--nw', (Math.abs(pu) / 2).toFixed(3));
        }
      });
      var t = hover ? describe(hover) : sel && S.params[sel] ? summary(sel) : 'Click an empty cell to map its source at 25%; drag a cell to set its depth.';
      if (cap.textContent !== t) cap.textContent = t;
    }
    function summary(key) {
      var l = S.mapsOf(key), c0 = S.ctlOf(key), p = S.params[key];
      return pname(S, key) + ': ' + (l.length ? l.length + ' mapping' + (l.length > 1 ? 's' : '') : 'no mappings') + (c0 ? ' · controlled by ' + S.src(c0.src).name + ' (' + amount(S, c0) + ')' : '') + ' · now ' + p.fmt(now(S, p));
    }
    var redraw = keyed(paint);
    var view = { el: el0, render: function () { redraw(S.t + '|' + sig(S) + '|' + S.selected + '|' + S.map.src + '|' + (hover ? cells.indexOf(hover) : -1) + '|' + JSON.stringify(S.mappings.map(function (m) { return [m.depth, m.lo, m.hi, m.cd]; })) + '|' + S.pad.x + ',' + S.pad.y); } };
    S.on(function () { view.render(); });
    S.add(view); return view;
  }

  /* =====================================================================
     ROUTE ROW — one mapping as a line: source, the 4-state polarity pill, destination, its depth (or, for CTL, its
     low/high range), the authored → now readout, + depth, remove. ROUTE LIST lays the rows out with each mapping's
     depth routes indented under it (the lab's one-level rule drawn as layout).
     ===================================================================== */
  function rangeTrack(isRange) {
    var t = el('div', 'mx-trk' + (isRange ? ' rng' : ''), '<div class="track"><i class="fill"></i><i class="band"></i></div><i class="live"></i>' +
      (isRange ? '<i class="handle lo hd-focus" role="slider"></i><i class="handle hi hd-focus" role="slider"></i><i class="pos"></i>' : '<i class="handle hd-focus" role="slider"></i>'));
    return t;
  }
  function RouteRow(el0, S, m, opts) {
    opts = opts || {};
    var isD = !!opts.depthRoute, tgt = isD ? m.of : m;
    el0.classList.add('mx-row'); if (isD) el0.classList.add('dr'); el0.innerHTML = '';
    var s = S.src(m.src);
    el0.style.setProperty('--sc', srcColor(s));
    var srcE = el('div', 'mx-src', '<i class="dot"></i><span class="nm">' + esc(s.name) + '</span><kbd>' + num(S, m.src) + '</kbd>');
    var pill = el('button', 'hd-pill mx-pol hd-focus'); pill.type = 'button';
    var dst = el('div', 'mx-dst');
    dst.innerHTML = isD ? '<span class="arw" aria-hidden="true">↳</span><span class="nm">depth of ' + esc(S.src(tgt.src).name) + ' → ' + esc(pname(S, tgt.param)) + '</span>'
      : '<span class="arw" aria-hidden="true">→</span><span class="nm">' + esc(pname(S, m.param)) + '</span>';
    var amt = el('div', 'mx-amt'), trk = null;
    var rd = el('div', 'mx-read hd-value', '<span class="au"></span><span class="ar" aria-hidden="true">→</span><span class="nw"></span>');
    var addD = el('button', 'hd-iconbtn mx-adddep hd-focus', '<span aria-hidden="true">+◇</span>'); addD.type = 'button';
    var del = el('button', 'hd-iconbtn mx-del hd-focus', XSVG); del.type = 'button';
    del.setAttribute('aria-label', isD ? 'Remove this depth route' : 'Remove ' + s.name + ' → ' + pname(S, m.param) + (routesOn(S, m).length ? ' and its depth routes' : ''));
    var sil = el('span', 'mx-sil'); srcE.appendChild(sil);
    [srcE, pill, dst, amt, rd, addD, del].forEach(function (x) { el0.appendChild(x); });
    function build() {
      amt.innerHTML = ''; trk = rangeTrack(m.pol === 'ctl'); amt.appendChild(trk);
      var hs = trk.querySelectorAll('.handle');
      if (m.pol === 'ctl') { hs[0].setAttribute('aria-label', 'Low end of the Control range'); hs[1].setAttribute('aria-label', 'High end of the Control range'); }
      else hs[0].setAttribute('aria-label', (isD ? 'Depth-route amount' : 'Depth') + ', ' + s.name + ' → ' + pname(S, tgt.param));
      [].forEach.call(hs, function (h) { h.setAttribute('aria-valuemin', '0'); h.setAttribute('aria-valuemax', '1'); h.tabIndex = 0;
        h.addEventListener('keydown', function (e) {               // arrows: keyboard mode only (ADR-259 r6); the drag is the mouse route
          if (!S.keys) return; var dv = 0;
          if (e.key === 'ArrowUp' || e.key === 'ArrowRight') dv = e.shiftKey ? .0025 : .01; else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') dv = -(e.shiftKey ? .0025 : .01); else return;
          e.preventDefault(); e.stopPropagation();
          if (m.pol !== 'ctl') m.depth = clamp(m.depth + dv); else if (h.classList.contains('lo')) m.lo = clamp(m.lo + dv, 0, m.hi); else m.hi = clamp(m.hi + dv, m.lo, 1);
          S.emit(); }); });
      drag(trk, { axis: 'x',
        start: function (e) {
          var r = trk.querySelector('.track').getBoundingClientRect(), W = Math.max(20, r.width), which = 'd';
          if (m.pol === 'ctl') { var x = (e.clientX - r.left) / W; which = Math.abs(x - m.lo) <= Math.abs(x - m.hi) ? (x > m.hi ? 'hi' : 'lo') : 'hi'; if (m.lo === m.hi) which = x < m.lo ? 'lo' : 'hi'; }
          var ai = !isD && m.cd ? S.editing(S.params[m.param]) : null;
          S.lastMap = isD ? S.lastMap : m; S.selected = tgt.param; S.emit();
          trk.classList.add('drag');
          return { W: W, which: which, ai: ai, start: which === 'lo' ? m.lo : which === 'hi' ? m.hi : (m.cd && ai !== null ? m.cd[ai] : m.depth) };
        },
        move: function (st, d) {
          var nv = st.start + d / st.W, got;
          if (st.which === 'lo') { got = clamp(nv, 0, m.hi); m.lo = got; } else if (st.which === 'hi') { got = clamp(nv, m.lo, 1); m.hi = got; }
          else { got = clamp(nv); if (m.cd && st.ai !== null) m.cd[st.ai] = got; else m.depth = got; }
          return (got - st.start) * st.W;   // the delta reached, for the re-anchor
        },
        end: function (st, moved) { trk.classList.remove('drag'); if (moved) S.say((isD ? 'Depth route ' : '') + s.name + ' → ' + pname(S, tgt.param) + ' · ' + POLN[m.pol] + ' ' + (isD ? pct(m.depth) : amount(S, m))); S.emit(); } });
      trk.addEventListener('dblclick', function () {                   // double-click resets: 25%, or the full sweep
        if (m.pol === 'ctl') { m.lo = 0; m.hi = 1; } else m.depth = DEF_DEPTH;
        S.say((isD ? 'Depth route' : s.name + ' → ' + pname(S, tgt.param)) + ' reset to ' + (isD ? pct(m.depth) : amount(S, m))); S.emit();
      });
      trk.dataset.pol = m.pol;
    }
    /* MP1 (ADR-340 r9): click next, shift-click previous, a vertical drag widens and thins the band (the depth) */
    H.PolChip(pill, S, m, { depthRoute: isD, cycle: function (back) { cyclePolOf(S, m, isD, back); S.emit(); } });
    addD.addEventListener('click', function () {
      if (isD) { S.say('One level only: a depth route\'s own depth cannot be modulated (no route reads a route that reads a route)'); return; }
      addDepthRoute(S, m); S.emit();
    });
    del.addEventListener('click', function () { if (isD) { var l = dRoutes(S); l.splice(l.indexOf(m), 1); S.say('Removed the depth route ' + s.name + ' → depth of ' + S.src(tgt.src).name); S.emit(); } else { removeMapping(S, m); S.emit(); } });
    el0.addEventListener('click', function (e) { if (e.target.closest('button,.mx-trk')) return; if (!isD) { S.lastMap = m; S.focus[m.param] = S.mapsOf(m.param).indexOf(m); } S.selected = tgt.param; S.emit(); });
    var curPol = null;
    function sync() {
      if (m.pol !== curPol) { curPol = m.pol; build(); }
      var other = isD ? null : S.ctlOf(m.param), blocked = other && other !== m;
      pill.textContent = POLN[m.pol]; pill.dataset.pol = m.pol;
      pill.title = POLHINT[m.pol] + (blocked ? '. CTL unavailable: ' + S.src(other.src).name + ' already controls ' + pname(S, m.param) : '') + '. Click to cycle';
      pill.setAttribute('aria-label', 'Polarity ' + POLN[m.pol] + ', click to cycle ' + (isD ? 'UNI, BI, INV' : 'UNI, BI, INV, CTL') + (blocked ? ' (CTL skipped: ' + S.src(other.src).name + ' already controls it)' : ''));
      var refuse = isD || m.pol === 'ctl';
      addD.classList.toggle('refuse', refuse);
      addD.setAttribute('aria-label', isD ? 'Add a depth route: refused, one level only (click for the reason)' : m.pol === 'ctl' ? 'Add a depth route: refused, a Control has a range, not a depth (click for the reason)' : 'Modulate this depth with ' + (S.src(S.map.src) || s).name);
      el0.classList.toggle('focus', !isD && S.lastMap === m);
      var sl = !isD && silent(S, m); el0.classList.toggle('silent', sl);
      sil.innerHTML = sl ? H.glyph(m.corner, 8, 'currentColor') + '<span>silent</span>' : '';
      el0.title = sl ? 'Corner ' + H.CORNERS[m.corner].id + ' is silent (weight 0): this corner-level mapping has no effect now' : '';
    }
    var view = { el: el0, m: m, sync: sync, render: function () {
      var hs = trk.querySelectorAll('.handle'), live = trk.querySelector('.live'), band = trk.querySelector('.band'), fill = trk.querySelector('.fill'), pos = trk.querySelector('.pos');
      var au = rd.querySelector('.au'), nw = rd.querySelector('.nw');
      if (m.pol === 'ctl') {
        var p = ctlPos(S, m);
        trk.style.setProperty('--lo', m.lo); trk.style.setProperty('--hi', m.hi); trk.style.setProperty('--pos', p);
        hs[0].setAttribute('aria-valuenow', m.lo.toFixed(3)); hs[0].setAttribute('aria-valuetext', 'low ' + pct(m.lo));
        hs[1].setAttribute('aria-valuenow', m.hi.toFixed(3)); hs[1].setAttribute('aria-valuetext', 'high ' + pct(m.hi));
        au.textContent = amount(S, m); nw.textContent = pct(p);
        rd.title = 'Control: sweeps ' + amount(S, m) + ', now at ' + pct(p);
      } else {
        var dep = isD ? m.depth : S.depthOf(m), dr = isD ? [] : routesOn(S, m);
        trk.style.setProperty('--d', dep);
        trk.classList.toggle('mod', dr.length > 0);
        if (dr.length) { var rc = depthReach(S, m), dn = depthNow(S, m); trk.style.setProperty('--b0', rc[0]); trk.style.setProperty('--b1', rc[1]); trk.style.setProperty('--lv', dn); trk.style.setProperty('--rc', srcColor(S.src(dr[0].src))); }
        hs[0].setAttribute('aria-valuenow', dep.toFixed(3)); hs[0].setAttribute('aria-valuetext', pct(dep) + (dr.length ? ', moved to ' + pct(depthNow(S, m)) : ''));
        var pu = isD ? dPush(S, m) : push(S, m);
        au.textContent = pct(dep); nw.textContent = !isD && silent(S, m) ? '—' : spct(pu);   // a silent corner's push is not ruled: no number is claimed
        rd.title = isD ? 'authored ' + pct(dep) + ', moving the depth by ' + spct(pu) + ' now' : 'authored depth ' + pct(dep) + (dr.length ? ' (moved to ' + pct(depthNow(S, m)) + ')' : '') + ', pushing ' + spct(pu) + ' now';
      }
    } };
    sync();
    return view;
  }
  function RouteList(el0, S, opts) {
    opts = opts || {};
    el0.classList.add('mx-routes'); el0.__S = S; el0.setAttribute('role', 'list'); el0.setAttribute('aria-label', opts.label || 'Mappings');
    var rows = [];
    function build() {
      el0.innerHTML = ''; rows = [];
      var list = S.mappings.filter(function (m) { return !opts.param || m.param === opts.param; });
      if (!list.length) { var e = el('div', 'hd-hint mx-empty', 'No mappings. Click an empty grid cell, or turn MAP on and click a control.'); e.setAttribute('role', 'listitem'); el0.appendChild(e); return; }
      list.forEach(function (m) {
        var r = el('div'); r.setAttribute('role', 'listitem'); el0.appendChild(r); rows.push(RouteRow(r, S, m));
        routesOn(S, m).forEach(function (d) { var r2 = el('div'); r2.setAttribute('role', 'listitem'); el0.appendChild(r2); rows.push(RouteRow(r2, S, d, { depthRoute: true })); });
      });
    }
    var last = null;
    function check() { var g = sig(S); if (g !== last) { last = g; build(); } rows.forEach(function (r) { r.sync(); }); }
    S.on(check); check();
    var view = { el: el0, render: function () { var g = sig(S); if (g !== last) check(); rows.forEach(function (r) { r.render(); }); } };
    S.add(view); return view;
  }

  /* =====================================================================
     MOD LEDGER — "what is moving it, right now": the selected destination's mappings with their authored amount,
     depth now, source now and push now, then the sum, in the order the arithmetic runs.
     ===================================================================== */
  function ModLedger(el0, S, opts) {
    opts = opts || {};
    el0.classList.add('mx-ledger'); el0.__S = S; el0.innerHTML = '';
    var h = el('div', 'mx-lh'); el0.appendChild(h);
    var ban = el('div', 'mx-ban'); el0.appendChild(ban);
    var tb = el('div', 'mx-lt'); tb.setAttribute('role', 'table'); el0.appendChild(tb);
    var cells = [];
    function key() { return opts.key || S.selected; }
    function row(cls, items, hdr) {
      var r = el('div', 'mx-lr ' + cls); r.setAttribute('role', 'row');
      items.forEach(function (it, i) { var c = el('div', 'mx-lc c' + i, it); c.setAttribute('role', hdr ? 'columnheader' : 'cell'); r.appendChild(c); });
      tb.appendChild(r); return r;
    }
    function build() {
      tb.innerHTML = ''; cells = [];
      var k = key(), p = S.params[k];
      if (!p) { h.textContent = 'What is moving it, right now'; ban.textContent = ''; ban.hidden = true; row('none', ['<span class="hd-hint">Select a destination: a grid column, a lane or a control.</span>']); return; }
      h.textContent = 'What is moving ' + p.name + ', right now';
      row('hdr', ['SOURCE', 'POL', 'YOURS', 'DEPTH NOW', 'SOURCE NOW', 'PUSH NOW'], true);
      var l = S.mapsOf(k);
      if (!l.length) row('none', ['<span class="hd-hint">Nothing: ' + esc(p.name) + ' sits at its own value.</span>']);
      l.forEach(function (m) {
        var s = S.src(m.src), r = row('m ' + m.pol, ['<i class="dot"></i><span class="nm">' + esc(s.name) + '</span>', POLN[m.pol], '', '', '', '']);
        r.style.setProperty('--sc', srcColor(s)); cells.push({ r: r, m: m });
        routesOn(S, m).forEach(function (d) { var sd = S.src(d.src), r2 = row('dr', ['<span class="arw" aria-hidden="true">↳</span><i class="dot"></i><span class="nm">' + esc(sd.name) + '</span>', POLN[d.pol], '', '<span class="hd-hint">→ depth</span>', '', '']);
          r2.style.setProperty('--sc', srcColor(sd)); cells.push({ r: r2, d: d }); });
      });
      var sum = row('sum', ['', '', '', '', '', '']); cells.push({ r: sum, sum: true, p: p });
    }
    function paint() {
      var k = key(), p = S.params[k];
      if (!p) return;
      var c0 = S.ctlOf(k);
      ban.hidden = !c0;
      if (c0) { var cs = S.src(c0.src); ban.style.setProperty('--sc', srcColor(cs));
        var bt = 'Controlled by ' + cs.name + ' · sweeping ' + amount(S, c0) + ' · now ' + pct(ctlPos(S, c0)) + ' · its own value (' + p.fmt(S.base(p)) + ') is ignored';
        if (ban.textContent !== bt) ban.textContent = bt; }
      cells.forEach(function (c) {
        var q = c.r.children;
        if (c.sum) {
          var rw = raw(S, p), o = S.origin(p), su = rw - o, clip = rw > 1.0005 || rw < -.0005;
          q[0].innerHTML = (c0 ? 'CTL ' : 'own ') + '<b>' + esc(p.fmt(o)) + '</b> + Σ ' + spct(su) + ' of range';
          q[3].innerHTML = clip ? '<span class="mx-clip">raw ' + pct(rw) + ', clamped</span>' : '';
          q[5].innerHTML = '= <b>' + esc(p.fmt(now(S, p))) + '</b>';
          return;
        }
        if (c.d) { q[2].textContent = pct(c.d.depth); q[4].textContent = pct(S.sourceValue(c.d.src)); q[5].textContent = spct(dPush(S, c.d)); return; }
        var m = c.m, dr = routesOn(S, m).length;
        c.r.classList.toggle('silent', silent(S, m));
        q[2].textContent = amount(S, m);
        q[3].innerHTML = m.pol === 'ctl' ? '<span class="hd-hint">range</span>' : dr ? pct(depthNow(S, m)) : '<span class="hd-hint">—</span>';
        q[4].textContent = pct(S.sourceValue(m.src));
        q[5].textContent = m.pol === 'ctl' ? 'sets ' + pct(ctlPos(S, m)) : spct(push(S, m));
      });
    }
    var last = null;
    function check() { var g = sig(S) + '|' + key(); if (g !== last) { last = g; build(); } paint(); }
    S.on(check); check();
    var redraw = keyed(paint);
    var view = { el: el0, render: function () { var g = sig(S) + '|' + key(); if (g !== last) check(); else redraw(S.t + '|' + JSON.stringify(S.mappings.map(function (m) { return [m.depth, m.lo, m.hi, m.cd]; })) + JSON.stringify(dRoutes(S).map(function (d) { return d.depth; })) + S.pad.x + ',' + S.pad.y); } };
    S.add(view); return view;
  }

  /* =====================================================================
     SCREEN HELPERS for the two canvases (the six scr- roles only, through Viz.kit)
     ===================================================================== */
  function monoFont(px, V) { var f = getComputedStyle(document.body).getPropertyValue('--font-mono') || 'monospace'; return '500 ' + (px * V) + 'px ' + f; }
  function fitText(g, t, w) { if (g.measureText(t).width <= w) return t; while (t.length > 1 && g.measureText(t + '…').width > w) t = t.slice(0, -1); return t + '…'; }
  function routedKeys(S, opts) {
    if (opts.keys) return opts.keys.filter(function (k) { return S.params[k]; });
    var out = []; Object.keys(S.params).forEach(function (k) { if (S.mapsOf(k).length) out.push(k); }); return out;
  }
  /* transparent hit buttons over a canvas region: the click (mouse route) and the keyboard route to select it */
  function hitLayer(host) { var l = el('div', 'mx-hits'); host.appendChild(l); return l; }

  /* =====================================================================
     TRACE LANES — the value over time against its reach band, one lane per routed destination (a screen).
     The band is the reach at the depths in force at each instant; the dashed line is where the mappings start
     (the own value, or the Control's position); the meter line is the value now. Each lane autoscales and says so.
     ===================================================================== */
  function TraceLanes(el0, S, opts) {
    opts = opts || {};
    var span = opts.span || 3, N = 150, laneH = opts.laneH || 46;
    el0.classList.add('mx-trace'); el0.__S = S;
    el0.innerHTML = ''; var viz = el('div', 'hd-viz'); el0.appendChild(viz);
    var cv = el('canvas'); cv.setAttribute('role', 'img'); viz.appendChild(cv);
    var hits = hitLayer(viz), keys = [], hist = {}, lastT = null, lastSig = null;
    function sample(t) {
      var t0 = S.t; S.t = t; var out = {};
      keys.forEach(function (k) { var p = S.params[k], rc = reach(S, p), c0 = S.ctlOf(k), rw = raw(S, p);
        out[k] = { lo: rc[0], hi: rc[1], o: S.origin(p), n: clamp(rw), clip: rw > 1.0005 || rw < -.0005, cl: c0 ? c0.lo : null, ch: c0 ? c0.hi : null }; });
      S.t = t0; return out;
    }
    function prefill() {   // the history as it would read had the current mappings always held: a full first frame, and the still frame
      hist = {}; keys.forEach(function (k) { hist[k] = []; });
      for (var i = 0; i < N; i++) { var t = S.t - span + span * (i + 1) / N, smp = sample(t); keys.forEach(function (k) { hist[k].push(smp[k]); }); }
      lastT = S.t;
    }
    function layout() {
      keys = routedKeys(S, opts);
      viz.style.height = Math.max(1, keys.length) * laneH + 'px';
      hits.innerHTML = '';
      keys.forEach(function (k, i) { var b = el('button', 'mx-hit hd-focus'); b.type = 'button'; b.style.top = (i * laneH) + 'px'; b.style.height = laneH + 'px';
        b.addEventListener('click', function () { S.selected = k; S.emit(); }); b.dataset.key = k; hits.appendChild(b); });
      prefill();
    }
    function draw() {
      var K = H.Viz.kit(cv), g = K.g, W = K.W, Hh = K.H, V = K.V;
      cv.__draws = (cv.__draws || 0) + 1;                                   // a draw counter the reduced-motion test reads
      K.tube(); K.scan();
      g.font = monoFont(9, V); g.textBaseline = 'top';
      if (!keys.length) { g.fillStyle = K.c('grid'); g.fillText('no routed destinations', 10 * V, 10 * V); cv.setAttribute('aria-label', 'Trace lanes: no routed destinations'); return; }
      var lh = Hh / keys.length, X = function (i) { return 4 * V + (W - 8 * V) * i / (N - 1); }, labels = [];
      keys.forEach(function (k, L) {
        var p = S.params[k], hs = hist[k], top = L * lh, pad = 4 * V, head = 12 * V, i;
        var p0 = 1, p1 = 0; hs.forEach(function (q) { [q.lo, q.hi, q.o, q.n].forEach(function (x) { if (x < p0) p0 = x; if (x > p1) p1 = x; }); if (q.cl !== null) { p0 = Math.min(p0, q.cl); p1 = Math.max(p1, q.ch); } });
        if (p1 - p0 < .1) { var mid = (p0 + p1) / 2; p0 = mid - .05; p1 = mid + .05; }
        var pd = (p1 - p0) * .08; p0 -= pd; p1 += pd;
        var Y = function (x) { return top + pad + head + (1 - (x - p0) / (p1 - p0)) * (lh - 2 * pad - head); };
        if (L) { g.fillStyle = K.c('grid', .35); g.fillRect(0, top, W, V); }
        if (k === S.selected) { g.fillStyle = K.c('grid', .1); g.fillRect(0, top + (L ? V : 0), W, lh); }
        g.save(); g.beginPath(); g.rect(0, top + pad + head - 2 * V, W, lh - 2 * pad - head + 4 * V); g.clip();
        g.fillStyle = K.c('physics', .26); g.beginPath();                                            // the reach band
        for (i = 0; i < N; i++) { if (i) g.lineTo(X(i), Y(hs[i].hi)); else g.moveTo(X(i), Y(hs[i].hi)); }
        for (i = N - 1; i >= 0; i--) g.lineTo(X(i), Y(hs[i].lo)); g.closePath(); g.fill();
        if (hs[N - 1].cl !== null) {                                                                // a Control's swept range: dotted edges
          g.strokeStyle = K.c('grid', .8); g.lineWidth = V; g.setLineDash([1.5 * V, 2.5 * V]);
          g.beginPath(); for (i = 0; i < N; i++) { if (hs[i].cl === null) continue; g.moveTo(X(i), Y(hs[i].cl)); g.lineTo(X(i) + (W - 8 * V) / (N - 1), Y(hs[i].cl)); g.moveTo(X(i), Y(hs[i].ch)); g.lineTo(X(i) + (W - 8 * V) / (N - 1), Y(hs[i].ch)); } g.stroke(); g.setLineDash([]); }
        g.strokeStyle = K.c(hs[N - 1].cl !== null ? 'physics' : 'value'); g.lineWidth = V; g.setLineDash([4 * V, 3 * V]);   // where the mappings start
        g.beginPath(); for (i = 0; i < N; i++) { if (i) g.lineTo(X(i), Y(hs[i].o)); else g.moveTo(X(i), Y(hs[i].o)); } g.stroke(); g.setLineDash([]);
        function nowLine() { g.beginPath(); for (i = 0; i < N; i++) { if (i) g.lineTo(X(i), Y(hs[i].n)); else g.moveTo(X(i), Y(hs[i].n)); } }
        g.lineJoin = 'round'; nowLine(); g.strokeStyle = K.c('meter', .22); g.lineWidth = 5 * V; g.stroke();                 // the glow: a wider underlay
        nowLine(); g.strokeStyle = K.c('meter'); g.lineWidth = 1.6 * V; g.stroke(); g.lineJoin = 'round';
        g.restore();
        g.fillStyle = K.c('marker'); for (i = 0; i < N; i++) if (hs[i].clip) g.fillRect(X(i) - V, top + pad + head - 3 * V, 2 * V, 2 * V);
        var c0 = S.ctlOf(k), lab = p.name + '  ' + p.fmt(hs[N - 1].n) + (c0 ? '  · CTL ' + S.src(c0.src).name : ''), sc = pct(clamp(p0)) + '–' + pct(clamp(p1));
        g.fillStyle = K.c('grid'); g.fillText(fitText(g, lab, W - g.measureText(sc).width - 24 * V), 6 * V, top + 3 * V);
        g.fillText(sc, W - g.measureText(sc).width - 6 * V, top + 3 * V);
        labels.push(p.name + ' now ' + p.fmt(hs[N - 1].n) + (c0 ? ', controlled by ' + S.src(c0.src).name : '') + ', scale ' + sc);
        var hb = hits.children[L]; if (hb) hb.setAttribute('aria-label', p.name + ' lane: now ' + p.fmt(hs[N - 1].n) + (k === S.selected ? ', selected' : ', select it'));
      });
      cv.setAttribute('aria-label', 'Trace lanes, ' + span + ' s: ' + labels.join('; '));
    }
    var redraw = keyed(draw);
    var view = { el: el0, render: function () {
      var g2 = sig(S) + '|' + (opts.keys || []).join();
      if (g2 !== lastSig) { lastSig = g2; layout(); }
      else if (S.reduce) prefill();                                      // reduced motion: the still frame follows every edit
      else { var dt = span / N, n = 0; while (S.t - lastT >= dt && n < N) { lastT += dt; n++; var smp = sample(lastT); keys.forEach(function (k) { hist[k].push(smp[k]); hist[k].shift(); }); } }
      var sch = (cv.closest('[data-scr]') || {}).dataset; sch = sch ? sch.scr : '';
      redraw((S.reduce ? JSON.stringify(S.mappings.map(function (m) { return [m.depth, m.lo, m.hi, m.pol]; })) + JSON.stringify(dRoutes(S).map(function (d) { return d.depth; })) + S.pad.x + S.pad.y : lastT) + '|' + S.selected + '|' + cv.clientWidth + '|' + sch + '|' + document.documentElement.getAttribute('data-theme'));
    } };
    S.on(function () { view.render(); });
    S.add(view); return view;
  }

  /* =====================================================================
     COMPACT WIRES — the patch-bay view, dense: the sources and destinations in use. Width is the depth now,
     brightness the source's value now, dashed is inverse, a CTL wire ends in a bar. A screen (the lab's tube): scr- roles
     only, so no source hues inside it.
     W-a (ADR-341 r5, ADR-340 r10's ML1; source composition-lab-r3 W-a), the wires made a control:
       values     every node shows its value (a source its level, a destination its value now)
       ◇ valve    every wire carries a valve at its middle that fills to its depth, and is the DROP TARGET for a depth route
                  (B70: a source dropped on a wire's middle modulates that wire's depth); while a drag is in flight every valve
                  grows and the nearest within 16 px lights
       new wires  drag from a source node to a destination node (a new wire beside any already there: several sources on one
                  destination stay separate wires) or onto a valve
     opts: { keys? destination order, sources? ids shown even with no route (so a first wire can be dragged from them),
             drag: false turns the dragging off, onWire(srcId, key), onDepthRoute(srcId, mapping) }. The engine has no depth
             routes yet (B70): a drop on a valve only EMITS onDepthRoute; with no callback the master's own placeholder
             (S.depthRoutes, the matrix's) takes it so the preview shows something. A drop on a destination without onWire
             adds a mapping to the Session at 25 %, as a click on an empty grid cell does.
     ===================================================================== */
  function CompactWires(el0, S, opts) {
    opts = opts || {};
    var rowH = 30, geo = { src: [], dst: [], valve: [] }, wd = null;   // geo in CSS px, read by the pointer; wd the wire being dragged
    el0.classList.add('mx-wires'); el0.__S = S; el0.innerHTML = '';
    var viz = el('div', 'hd-viz'); el0.appendChild(viz);
    var cv = el('canvas'); cv.setAttribute('role', 'img'); viz.appendChild(cv);
    var hits = hitLayer(viz), srcs = [], dsts = [], lastSig = null;
    function layout() {
      var used = {}; S.mappings.forEach(function (m) { used[m.src] = 1; }); dRoutes(S).forEach(function (d) { used[d.src] = 1; });
      (opts.sources || []).forEach(function (id) { used[id] = 1; });
      srcs = S.sources.filter(function (s) { return used[s.id]; });
      dsts = routedKeys(S, opts);
      var n = Math.max(srcs.length, dsts.length, 1); viz.style.height = (n * rowH + 12) + 'px';
      hits.innerHTML = '';
      dsts.forEach(function (k, j) { var b = el('button', 'mx-hit hd-focus'); b.type = 'button'; b.dataset.key = k; b.style.right = '0'; b.style.left = 'auto'; b.style.width = '34%';
        b.style.top = (6 + j * (n * rowH) / dsts.length) + 'px'; b.style.height = Math.max(28, (n * rowH) / dsts.length) + 'px';
        b.addEventListener('click', function () { S.selected = k; S.emit(); }); hits.appendChild(b); });
    }
    function draw() {
      var K = H.Viz.kit(cv), g = K.g, W = K.W, Hh = K.H, V = K.V;
      cv.__draws = (cv.__draws || 0) + 1;
      geo = { src: [], dst: [], valve: [] };
      K.tube(); K.scan(); g.font = monoFont(9, V); g.textBaseline = 'middle';
      if (!srcs.length && !dsts.length) { g.fillStyle = K.c('grid'); g.fillText('no mappings', 10 * V, Hh / 2); cv.setAttribute('aria-label', 'Wires: no mappings'); return; }
      var nw = Math.max(58 * V, Math.min(118 * V, W * .26)), nh = 22 * V, top = 6 * V, inner = Hh - 12 * V;
      var sx = 4 * V, dx = W - nw - 4 * V;
      var sY = function (j) { return top + inner * (j + .5) / srcs.length; }, dY = function (j) { return top + inner * (j + .5) / dsts.length; };
      var fanN = {}, fanK = {}, mids = new Map(), desc = [];
      S.mappings.forEach(function (m) { fanN[m.param] = (fanN[m.param] || 0) + 1; });
      function bez(x0, y0, x1, y1, t) { var u = 1 - t, c0 = x0 + (x1 - x0) * .45, c1 = x1 - (x1 - x0) * .45; return [u * u * u * x0 + 3 * u * u * t * c0 + 3 * u * t * t * c1 + t * t * t * x1, u * u * u * y0 + 3 * u * u * t * y0 + 3 * u * t * t * y1 + t * t * t * y1]; }
      function diamond(x, y, r) { g.beginPath(); g.moveTo(x, y - r); g.lineTo(x + r, y); g.lineTo(x, y + r); g.lineTo(x - r, y); g.closePath(); }
      g.lineCap = 'round';
      S.mappings.forEach(function (m) {
        var j = srcs.findIndex(function (s) { return s.id === m.src; }), dj = dsts.indexOf(m.param); if (j < 0 || dj < 0) return;
        var n = fanN[m.param], kk = fanK[m.param] = (fanK[m.param] || 0) + 1;
        var x0 = sx + nw, y0 = sY(j), x1 = dx, y1 = dY(dj) + (kk - (n + 1) / 2) * 5 * V, sv = S.sourceValue(m.src);
        var wd2 = V * (1 + 6 * (m.pol === 'ctl' ? m.hi - m.lo : depthNow(S, m)));
        g.strokeStyle = K.c('physics', .25 + .7 * sv); g.lineWidth = wd2; g.setLineDash(m.pol === 'inv' ? [6 * V, 4 * V] : []);
        g.beginPath(); g.moveTo(x0, y0); g.bezierCurveTo(x0 + (x1 - x0) * .45, y0, x1 - (x1 - x0) * .45, y1, x1, y1); g.stroke(); g.setLineDash([]);
        if (m.pol === 'ctl') { g.fillStyle = K.c('physics'); g.fillRect(x1 - 3 * V, y1 - 6 * V, 3 * V, 12 * V); }   // a Control ends in a bar: it SETS, it does not add
        mids.set(m, bez(x0, y0, x1, y1, .5));
        if (m.pol !== 'uni') { var q = bez(x0, y0, x1, y1, .86); g.fillStyle = K.c('grid'); g.fillText(POLN[m.pol], q[0] - 16 * V, q[1] - 7 * V); }
        desc.push(S.src(m.src).name + ' to ' + pname(S, m.param) + ' ' + POLN[m.pol] + ' ' + amount(S, m));
      });
      dRoutes(S).forEach(function (d) {                                 // a depth wire runs, dotted, to its target wire's valve
        var mm = mids.get(d.of), j = srcs.findIndex(function (s) { return s.id === d.src; }); if (!mm || j < 0) return;
        var x0 = sx + nw, y0 = sY(j), up = mm[1] > y0 ? -1 : 1;
        g.strokeStyle = K.c('physics', .3 + .65 * S.sourceValue(d.src)); g.lineWidth = V * (1 + 3 * d.depth); g.setLineDash(d.pol === 'inv' ? [2 * V, 3 * V] : [1 * V, 2.5 * V]);
        g.beginPath(); g.moveTo(x0, y0); g.bezierCurveTo(x0 + (mm[0] - x0) * .5, y0, mm[0], mm[1] + up * 30 * V, mm[0], mm[1] + up * 7 * V); g.stroke(); g.setLineDash([]);
        desc.push(S.src(d.src).name + ' to the depth of ' + S.src(d.of.src).name + ' to ' + pname(S, d.of.param));
      });
      /* the valves, one per wire, over the wires. At rest 5.5 px and filled to the wire's depth (a Control's range, for CTL); a wire
         whose depth is moved by depth routes fills to where the moved depth sits in its reach (the lab's W-a). While a wire is
         being dragged every valve grows and the one under the drop (the nearest within 16 px) takes the value colour. */
      mids.forEach(function (q, m) {
        var hotV = wd && wd.target && wd.target.kind === 'valve' && wd.target.m === m, r = (wd ? (hotV ? 8.5 : 7) : 5.5) * V;
        var routed = routesOn(S, m).length, rc = depthReach(S, m);
        var f = routed ? (rc[1] - rc[0] > 1e-9 ? clamp((depthNow(S, m) - rc[0]) / (rc[1] - rc[0])) : 1) : clamp(m.pol === 'ctl' ? m.hi - m.lo : depthNow(S, m));
        diamond(q[0], q[1], r); g.fillStyle = hotV ? K.c('value') : K.c('ground'); g.fill();
        if (!hotV) { g.save(); g.clip(); g.fillStyle = K.c('physics', .85); g.fillRect(q[0] - r, q[1] + r - 2 * r * f, 2 * r, 2 * r * f); g.restore(); }
        diamond(q[0], q[1], r); g.strokeStyle = hotV ? K.c('value') : K.c('physics'); g.lineWidth = 1.5 * V; g.stroke();
        geo.valve.push({ m: m, x: q[0] / V, y: q[1] / V });
      });
      g.lineCap = 'butt';
      srcs.forEach(function (s, j) {
        var y = sY(j) - nh / 2, sv = S.sourceValue(s.id), vt = pct(sv);
        g.fillStyle = K.c('ground'); g.fillRect(sx, y, nw, nh); g.strokeStyle = K.c('grid', .8); g.lineWidth = V; g.strokeRect(sx + .5 * V, y + .5 * V, nw - V, nh - V);
        g.fillStyle = K.c('value'); g.textAlign = 'right'; g.fillText(vt, sx + nw - 12 * V, y + nh / 2 - 2 * V); g.textAlign = 'left';
        g.fillStyle = K.c('grid'); g.fillText(fitText(g, num(S, s.id) + ' ' + s.name, nw - 20 * V - g.measureText(vt).width), sx + 4 * V, y + nh / 2 - 2 * V);
        g.fillStyle = K.c('physics', .9); g.fillRect(sx + 2 * V, y + nh - 4 * V, (nw - 4 * V) * sv, 2 * V);
        if (opts.drag !== false) { g.beginPath(); g.arc(sx + nw, sY(j), 3.2 * V, 0, 6.2832); g.fillStyle = K.c('ground'); g.fill(); g.strokeStyle = K.c('physics'); g.lineWidth = 1.4 * V; g.stroke(); }   // the port: where a wire starts
        geo.src.push({ id: s.id, x: sx / V, y: y / V, w: nw / V, h: nh / V });
      });
      dsts.forEach(function (k, j) {
        var p = S.params[k], y = dY(j) - nh / 2, sel = k === S.selected, c0 = S.ctlOf(k), rc = reach(S, p), rw = raw(S, p), bx = dx + 4 * V, bw = nw - 8 * V, by = y + nh - 4 * V;
        var hotD = wd && wd.target && wd.target.kind === 'dst' && wd.target.key === k, vt = p.fmt(clamp(rw));
        g.fillStyle = K.c('ground'); g.fillRect(dx, y, nw, nh);
        g.strokeStyle = hotD ? K.c('value') : K.c('grid', sel ? 1 : .7); g.lineWidth = (hotD ? 2.4 : sel ? 2.2 : 1) * V; if (c0) g.setLineDash([3 * V, 2 * V]); g.strokeRect(dx + .5 * V, y + .5 * V, nw - V, nh - V); g.setLineDash([]);
        g.fillStyle = K.c('value'); g.textAlign = 'right'; g.fillText(vt, bx + bw, y + nh / 2 - 3 * V); g.textAlign = 'left';
        g.fillStyle = K.c('grid'); g.fillText(fitText(g, (c0 ? 'CTL ' : '') + p.name, bw - g.measureText(vt).width - 4 * V), bx, y + nh / 2 - 3 * V);
        g.fillStyle = K.c('grid', .25); g.fillRect(bx, by, bw, 2 * V);
        g.fillStyle = K.c('physics', .55); g.fillRect(bx + rc[0] * bw, by - V, Math.max(V, (rc[1] - rc[0]) * bw), 3 * V);
        g.fillStyle = K.c(c0 ? 'physics' : 'value'); g.fillRect(bx + clamp(S.origin(p)) * bw - V, by - 3 * V, 2 * V, 6 * V);
        g.fillStyle = K.c(rw > 1.0005 || rw < -.0005 ? 'marker' : 'meter'); g.fillRect(bx + clamp(rw) * bw - V, by - 4 * V, 2 * V, 7 * V);
        geo.dst.push({ key: k, x: dx / V, y: y / V, w: nw / V, h: nh / V });
        var hb = hits.querySelector('[data-key="' + k + '"]'); if (hb) hb.setAttribute('aria-label', p.name + ': ' + S.mapsOf(k).length + ' wire' + (S.mapsOf(k).length > 1 ? 's' : '') + (c0 ? ', controlled by ' + S.src(c0.src).name : '') + ', now ' + p.fmt(clamp(rw)) + (sel ? ', selected' : ', select it'));
      });
      if (wd) {                                                         // the wire in flight, from the source's port to the pointer
        var jd = srcs.findIndex(function (s) { return s.id === wd.src; });
        if (jd >= 0) { var px = sx + nw, py = sY(jd), qx = wd.x * V, qy = wd.y * V;
          g.strokeStyle = K.c('value'); g.lineWidth = 1.6 * V; g.setLineDash([4 * V, 3 * V]); g.lineCap = 'round';
          g.beginPath(); g.moveTo(px, py); g.bezierCurveTo(px + 60 * V, py, qx - 60 * V, qy, qx, qy); g.stroke(); g.setLineDash([]); g.lineCap = 'butt'; }
      }
      cv.setAttribute('aria-label', 'Wires: ' + desc.join('; '));
    }
    /* THE DRAG (ML1, ADR-340 r10): pointer capture on the viz, guarded; geo is where the last frame put things */
    function at(e) { var r = cv.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; }
    function inBox(n, x, y) { return x >= n.x && x <= n.x + n.w && y >= n.y && y <= n.y + n.h; }
    function srcAt(x, y) { for (var i = 0; i < geo.src.length; i++) if (inBox(geo.src[i], x, y)) return geo.src[i]; return null; }
    function targetAt(x, y) {
      var best = null, bd = 16;
      geo.valve.forEach(function (q) { var d = Math.hypot(q.x - x, q.y - y); if (d < bd) { bd = d; best = { kind: 'valve', m: q.m }; } });
      if (best) return best;
      for (var i = 0; i < geo.dst.length; i++) if (inBox(geo.dst[i], x, y)) return { kind: 'dst', key: geo.dst[i].key };
      return null;
    }
    function drop(d) {
      var t = d.target, s = S.src(d.src);
      if (!t) { S.say('Dropped on nothing: no wire made'); return; }
      if (t.kind === 'dst') {
        if (opts.onWire) { opts.onWire(d.src, t.key); return; }
        var m = { param: t.key, src: d.src, pol: 'uni', depth: DEF_DEPTH }; S.mappings.push(m); S.focus[t.key] = S.mapsOf(t.key).length - 1; S.lastMap = m;
        S.say('New wire: ' + s.name + ' → ' + pname(S, t.key) + ' · UNI · 25% (' + S.mapsOf(t.key).length + ' on it now)');
      } else if (t.m.pol === 'ctl') S.say('A Control has a range, not a depth: a depth route needs a UNI, BI or INV wire');
      else if (opts.onDepthRoute) opts.onDepthRoute(d.src, t.m);
      else { dRoutes(S).push({ src: d.src, pol: 'uni', depth: DEF_DEPTH, of: t.m }); S.say('Depth route: ' + s.name + ' now moves the depth of ' + S.src(t.m.src).name + ' → ' + pname(S, t.m.param) + ' · UNI · 25%'); }
    }
    viz.addEventListener('pointerdown', function (e) {
      if (opts.drag === false || e.button !== 0) return;
      var q = at(e), n = srcAt(q[0], q[1]); if (!n) return;
      e.preventDefault(); try { viz.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
      wd = { src: n.id, x: q[0], y: q[1], target: null }; view.render();
    });
    viz.addEventListener('pointermove', function (e) {
      var q = at(e);
      if (!wd) { if (opts.drag !== false) cv.style.cursor = srcAt(q[0], q[1]) ? 'grab' : ''; return; }
      wd.x = q[0]; wd.y = q[1]; wd.target = targetAt(q[0], q[1]); cv.style.cursor = 'grabbing'; view.render();
    });
    function wend(e) {
      if (e && e.pointerId !== undefined && viz.hasPointerCapture && viz.hasPointerCapture(e.pointerId)) viz.releasePointerCapture(e.pointerId);
      if (!wd) return; var d = wd; wd = null; cv.style.cursor = ''; drop(d); S.emit(); view.render();
    }
    viz.addEventListener('pointerup', wend); viz.addEventListener('pointercancel', wend); viz.addEventListener('lostpointercapture', wend);
    var redraw = keyed(draw);
    var view = { el: el0, render: function () {
      var g2 = sig(S); if (g2 !== lastSig) { lastSig = g2; layout(); }
      var sch = (cv.closest('[data-scr]') || {}).dataset; sch = sch ? sch.scr : '';
      redraw(S.t + '|' + JSON.stringify(S.mappings.map(function (m) { return [m.depth, m.lo, m.hi]; })) + JSON.stringify(dRoutes(S).map(function (d) { return d.depth; })) + '|' + S.selected + '|' + cv.clientWidth + '|' + sch + '|' + document.documentElement.getAttribute('data-theme') + S.pad.x + S.pad.y +
        '|' + (wd ? wd.src + ',' + wd.x.toFixed(1) + ',' + wd.y.toFixed(1) : ''));
    } };
    S.on(function () { view.render(); });
    S.add(view); return view;
  }

  /* =====================================================================
     HOSTED MACRO ROWS — a hosted module's macros as horde's matrix shows them: visible, read-only, never
     cross-mapped (ADR-266 r3). Each module keeps its own macro mapping; these rows report it, live.
     opts.module = { name, macros: [{ id, name, f(t) → 0..1, targets: [names inside the module] }] }
     ===================================================================== */
  function HostedMacroRows(el0, S, opts) {
    var mod = opts.module;
    el0.classList.add('mx-hosted'); el0.__S = S; el0.innerHTML = '';
    el0.setAttribute('role', 'group'); el0.setAttribute('aria-label', mod.name + ' macros, read-only, mapped inside ' + mod.name);
    var hd = el('div', 'mx-hhead', LOCK + '<span class="tag">' + esc(mod.name) + '</span><span class="cap">macros · read-only · mapped inside ' + esc(mod.name) + '</span>');
    el0.appendChild(hd);
    var rows = mod.macros.map(function (mc, j) {
      var b = el('button', 'mx-hrow hd-focus', '<span class="no">M' + (j + 1) + '</span><span class="nm">' + esc(mc.name) + '</span><span class="meter" aria-hidden="true"><i></i></span><span class="hd-value vl"></span><span class="tg">→ ' + esc(mc.targets.join(' · ')) + '</span>');
      b.type = 'button'; b.title = mod.name + ' maps ' + mc.name + ' to ' + mc.targets.join(', ') + ' inside itself; horde shows it and cannot cross-map it';
      b.addEventListener('click', function () { S.say(mod.name + ' ' + mc.name + ' is read-only here: ' + mod.name + ' maps its own macros (to ' + mc.targets.join(', ') + '); they cannot be cross-mapped into horde'); });
      el0.appendChild(b); return { b: b, bar: b.querySelector('.meter i'), vl: b.querySelector('.vl'), mc: mc, j: j };
    });
    var view = { el: el0, render: function () {
      rows.forEach(function (r) { var x = clamp(r.mc.f(S.t)); r.bar.style.width = (x * 100).toFixed(1) + '%'; var t = pct(x); if (r.vl.textContent !== t) r.vl.textContent = t;
        r.b.setAttribute('aria-label', mod.name + ' macro ' + (r.j + 1) + ', ' + r.mc.name + ', ' + t + ', read-only: mapped inside ' + mod.name + ' to ' + r.mc.targets.join(', ')); });
    } };
    S.add(view); view.render(); return view;
  }

  H.SourceStrip = SourceStrip; H.ModMatrixGrid = ModMatrixGrid; H.RouteRow = RouteRow; H.RouteList = RouteList;
  H.ModLedger = ModLedger; H.TraceLanes = TraceLanes; H.CompactWires = CompactWires; H.HostedMacroRows = HostedMacroRows;
  /* the shared arithmetic, for tests and for the integrator (the core's numbers when no depth route exists) */
  H.Matrix = { POLS: POLS, POLN: POLN, push: push, now: now, raw: raw, reach: reach, ctlPos: ctlPos, depthNow: depthNow, depthReach: depthReach,
    silent: silent, addDepthRoute: addDepthRoute, remove: removeMapping, routesOn: routesOn,
    /* for batch 2a's views (the by-source list, the polarity chip): the structure signature, the depth routes and the cycle */
    sig: sig, dRoutes: dRoutes, cyclePolOf: cyclePolOf };
})(window.Horde);

/* =====================================================================
   THE MODULATORS (B531, ADR-266): the modulator lab's components in the house system.
   Sources: docs/design/shape-lab-mod.html (the current lab, B208/B226/B237) and, for the S&H clock card
   and the ownership tier only, docs/design/modulator-lab.html (superseded by it). Rulings applied:
   ADR-266 r6 (S&H is an LFO shape; "Sample on trigger" is a trigger action; S&H is also a mode on
   ORBITAL's mappings with its own rate and sync), r7 (a retrigger switch on every modulator card),
   r10 (rate plus sync, one shared control: RateSync, also the STEPPED morph's).
   THE ONE-LAW RULE (lab B177): every picture here asks ModLaw for its curve, and ModLaw's functions are
   the lab's, token for token, so a port that calls the same functions draws what it plays.
   Screens draw only through H.Viz.kit with the six scr- roles; glass TEXT is scr-grid, scr-physics or
   scr-marker only (ORCHID's value and meter read under 4.5:1, so they draw lines and marks only).
   ===================================================================== */
(function (H) {
  'use strict';
  var TAU = 2 * Math.PI, K1 = Math.log(100), DRAG = 160, MAXPTS = 32, HIT = 14;   // HIT: a 28 px target's radius
  function clamp(x, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, x)); }
  function frac(x) { return x - Math.floor(x); }
  function tag(t, cls, txt) { var e = document.createElement(t); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; }
  function keysOn(o) { return typeof o.keys === 'function' ? !!o.keys() : !!o.keys; }
  /* a stepper whose RANGE changes (points added to an envelope, bodies spawned in a field): H.Stepper fixes its range at mount, so
     the stepper is remounted in its slot when `key` changes; set() then follows the value silently */
  function dynStepper(slot, make) {
    var key = null, cur = null;
    return function (k, value) { if (k !== key) { key = k; slot.innerHTML = ''; var d = document.createElement('div'); slot.appendChild(d); cur = make(d); } cur.set(value); return cur; };
  }
  function fSec(s) { return s < 1 ? (s * 1000).toFixed(s < .01 ? 1 : 0) + ' ms' : s.toFixed(2) + ' s'; }
  function f2(x) { return (+x).toFixed(2); }
  function sgn2(x) { return (x > 0 ? '+' : x < 0 ? '−' : '') + Math.abs(x).toFixed(2); }
  function fire(fn, a, b) { if (typeof fn === 'function') fn(a, b); }
  function niceStep(span, n) { var raw = span / (n || 5), p = Math.pow(10, Math.floor(Math.log10(raw))); return raw / p < 2 ? p : raw / p < 5 ? 2 * p : 5 * p; }

  /* ---------- THE LAWS (shape-lab-mod.html's DSP section, unchanged in arithmetic) ---------- */
  /* THE CURVE, in the ENGINE's unit (SPEC-ENGINE C.12, the lead's ruling of 2026-10-21): a bend u in [-2.5, 2.5],
     y(x) = (1 - e^(-c·x)) / (1 - e^(-c)) with c = 4·u; u > 0 moves fast first. The lab wrote the same law as c_lab in
     [-1, 1] with k = 2·ln(100)·c_lab, so u = (ln 100 / 2)·c_lab ≈ 2.3026·c_lab, same sign: the lab's +1 is the bank's
     exponential (u = ln(10^4)/4 ≈ +2.303) and its +0.5, the one-pole scaled to arrive, is u = ln(100)/4 ≈ +1.151.
     Negating u is the exact time-mirror (f_-k(x) = 1 - f_k(1 - x)), which MIRROR relies on. */
  var U_MAX = 2.5, U_BANK = Math.log(1e4) / 4, U_ONEPOLE = K1 / 4;
  function segCurve(x, u) { var k = 4 * u; if (Math.abs(k) < 1e-9) return x; return (1 - Math.exp(-k * x)) / (1 - Math.exp(-k)); }
  function uFromLab(c) { return c * K1 / 2; }
  /* a bend's readout: signed, two decimals, the bank's exponential named ("+2.30 bank exp", as Envelope shows it) */
  function fmtU(u) { return (u > 0 ? '+' : u < 0 ? '−' : '') + Math.abs(u).toFixed(2) + (Math.abs(u - U_BANK) < .005 ? ' bank exp' : ''); }
  /* THE SHAPE LAW: pts [{t, v, u}] sorted by t; segment i runs pts[i] → pts[i+1] with bend pts[i].u (C.12's unit). Units of t are the
     caller's (cycles for the LFO, seconds for the envelope). i0/v0 start at segment i0 FROM v0 (the release enters from
     the level reached). Equal t on two points is a vertical step. */
  function shapeAt(pts, t, i0, v0) {
    var n = pts.length, s = i0 | 0, sv = v0 === undefined ? pts[s].v : v0, i;
    if (t <= pts[s].t) return sv;
    for (i = s; i < n - 1; i++) {
      var a = pts[i], b = pts[i + 1];
      if (t <= b.t) { var av = i === s ? sv : a.v, span = b.t - a.t; if (span <= 1e-12) return b.v; return av + (b.v - av) * segCurve((t - a.t) / span, a.u); }
    }
    return n - 1 === s ? sv : pts[n - 1].v;
  }
  function susOf(pts, sus) { return sus >= 0 && sus < pts.length - 1 ? sus : -1; }
  /* the envelope as an explicit function of time since the gate (t) and the release instant (tOff, null while held) */
  function envAt(pts, sus, t, tOff, v0) {
    var s = susOf(pts, sus);
    if (s < 0) return shapeAt(pts, t, 0, v0);
    if (tOff === null || tOff === undefined || t < tOff) return shapeAt(pts, Math.min(t, pts[s].t), 0, v0);
    var held = shapeAt(pts, Math.min(tOff, pts[s].t), 0, v0);
    return shapeAt(pts, pts[s].t + (t - tOff), s, held);
  }
  /* the SHIPPED tau law (advanceAdsr in closed form), kept only as the arrival plot's ghost */
  function tauGated(p, A, D, u) { var tPk = A * K1; return u < tPk ? 1 - Math.exp(-u / A) : p.s + (1 - p.s) * Math.exp(-(u - tPk) / D); }
  function envTauAt(p, t, tOff) {
    var A = Math.max(1e-4, p.a), D = Math.max(1e-4, p.d), R = Math.max(1e-4, p.r);
    if (tOff === null || tOff === undefined || t < tOff) return tauGated(p, A, D, Math.max(0, t));
    var v = tauGated(p, A, D, tOff) * Math.exp(-(t - tOff) / R); return v < 1e-6 ? 0 : v;
  }
  /* the ADSR knobs as breakpoints: the numbers are ARRIVAL times, the curve the one-pole's shape (c = +0.5) */
  /* the ADSR sliders as breakpoints with the ENGINE's defaults (C.12): attack u = 0, decay and release u = +1. Each stage
     ends exactly at its time (C.12: y(T) = 1). */
  var U_DEF = { attack: 0, decay: 1, release: 1 };
  function adsrPts(a, d, s, r, ua, ud, ur) { return [{ t: 0, v: 0, u: ua === undefined ? U_DEF.attack : ua }, { t: a, v: 1, u: ud === undefined ? U_DEF.decay : ud },
    { t: a + d, v: s, u: ur === undefined ? U_DEF.release : ur }, { t: a + d + r, v: 0, u: 0 }]; }
  /* presets are points, the editor's own currency: sample fn and give each segment the c whose midpoint matches */
  function fitCycle(fn, n) {
    var p = [], i; for (i = 0; i <= n; i++) p.push({ t: i / n, v: fn(i / n), u: 0 });
    for (i = 0; i < n; i++) { var a = p[i].v, b = p[i + 1].v, dv = b - a; if (Math.abs(dv) < 1e-9) continue;
      var m = Math.min(.999, Math.max(.001, (fn((i + .5) / n) - a) / dv)); p[i].u = clamp(-Math.log(1 / m - 1) / 2, -U_MAX, U_MAX); }   // f(½) = 1/(1 + e^(-2u)), inverted
    return p;
  }
  function mulberry(a) { return H.Synth.rng(a); }
  var LFO_PRESETS = {
    'SINE': function () { return fitCycle(function (t) { return Math.sin(TAU * t); }, 8); },
    'TRI': function () { return [{ t: 0, v: 0, u: 0 }, { t: .25, v: 1, u: 0 }, { t: .75, v: -1, u: 0 }, { t: 1, v: 0, u: 0 }]; },
    'SAW ↑': function () { return [{ t: 0, v: -1, u: 0 }, { t: 1, v: 1, u: 0 }]; },
    'SAW ↓': function () { return [{ t: 0, v: 1, u: 0 }, { t: 1, v: -1, u: 0 }]; },
    'SQUARE': function () { return [{ t: 0, v: 1, u: 0 }, { t: .5, v: 1, u: 0 }, { t: .5, v: -1, u: 0 }, { t: 1, v: -1, u: 0 }]; },
    /* seeded: (patch seed, draw count) → the same eight steps, one draw per click */
    'STEPS': function (seed, draw) { var r = mulberry((seed + Math.imul(0x9E3779B9, draw)) >>> 0), p = [];
      for (var i = 0; i < 8; i++) { var v = Math.round((r() * 2 - 1) * 8) / 8; p.push({ t: i / 8, v: v, u: 0 }, { t: (i + 1) / 8, v: v, u: 0 }); } return p; }
  };
  /* the lab's preset bends, converted exactly to u (× ln 100 / 2): its c +0.5 is u +1.1513 */
  var ENV_PRESETS = {
    'PLUCK': function () { return { sus: -1, pts: [{ t: 0, v: 0, u: 1.1513 }, { t: .003, v: 1, u: 1.8421 }, { t: .7, v: 0, u: 0 }] }; },
    'SWELL': function () { return { sus: 2, pts: [{ t: 0, v: 0, u: -1.1513 }, { t: 1.1, v: 1, u: 0.6908 }, { t: 1.6, v: .8, u: 1.1513 }, { t: 2.6, v: 0, u: 0 }] }; },
    'GATE': function () { return { sus: 1, pts: [{ t: 0, v: 0, u: 0 }, { t: .002, v: 1, u: 0 }, { t: .012, v: 0, u: 0 }] }; },
    'AD': function () { return { sus: -1, pts: [{ t: 0, v: 0, u: 1.1513 }, { t: .01, v: 1, u: 1.1513 }, { t: .5, v: 0, u: 0 }] }; }
  };
  /* THE DIVISION LIST, slow → fast: [name, alias, beats, family, row]; beats are quarter notes; exactly the values
     inside the shipped 0.0625..8 beat range. The name is UI, the beats are the stored truth (PLACEHOLDER: the lab's
     proposal B208, not yet ruled). */
  var DIVS = [
    ['2/1', '', 8, 0, 0], ['3/2', '1/1D', 6, 1, 1], ['2/1T', '', 16 / 3, 2, 0],
    ['1/1', '', 4, 0, 1], ['3/4', '1/2D', 3, 1, 2], ['1/1T', '', 8 / 3, 2, 1],
    ['1/2', '', 2, 0, 2], ['3/8', '1/4D', 1.5, 1, 3], ['1/2T', '', 4 / 3, 2, 2],
    ['1/4', '', 1, 0, 3], ['3/16', '1/8D', .75, 1, 4], ['1/4T', '', 2 / 3, 2, 3],
    ['1/8', '', .5, 0, 4], ['3/32', '1/16D', .375, 1, 5], ['1/8T', '', 1 / 3, 2, 4],
    ['1/16', '', .25, 0, 5], ['3/64', '1/32D', .1875, 1, 6], ['1/16T', '', 1 / 6, 2, 5],
    ['1/32', '', .125, 0, 6], ['3/128', '1/64D', .09375, 1, 7], ['1/32T', '', 1 / 12, 2, 6],
    ['1/64', '', .0625, 0, 7]];
  /* nearest by RATIO, not difference: at 8 beats 0.1 beat is nothing, at 1/16 it is another note value */
  function nearestDiv(beats) { var best = 0, bd = Infinity; for (var i = 0; i < DIVS.length; i++) { var d = Math.abs(Math.log(beats / DIVS[i][2])); if (d < bd - 1e-12) { bd = d; best = i; } } return best; }
  function divLabel(i) { return DIVS[i][0] + (DIVS[i][1] ? ' · ' + DIVS[i][1] : ''); }
  function divIndex(beats) { for (var i = 0; i < DIVS.length; i++) if (DIVS[i][2] === beats) return i; return -1; }
  /* a stored value's display: its name when exactly on the list, else "≈ nearest ±x%" (an off-list patch keeps its value) */
  function divName(beats) { var i = nearestDiv(beats), r = beats / DIVS[i][2] - 1;
    return Math.abs(r) < 1e-12 ? divLabel(i) : '≈ ' + DIVS[i][0] + ' ' + (r > 0 ? '+' : '−') + Math.abs(r * 100).toFixed(1) + '%'; }
  /* THE RANDOM STREAM (B237): draw k of mulberry32(seed), read by INDEX, so a picture never advances the sound's stream.
     mulberry32's state after j calls is seed + j·0x6D2B79F5; the k-th output is one hash of seed + (k+1)·G. */
  function m32at(seed, k) {
    var a = (seed + Math.imul(k + 1, 0x6D2B79F5)) | 0;
    var t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
    return ((t ^ t >>> 14) >>> 0) / 4294967296;
  }
  function drawV(seed, k) { return 2 * m32at(seed, k) - 1; }
  function lfoSeedOf(patchSeed) { return (patchSeed | 0) ^ 0x9E3779B9; }   // the engine's lfoSeed(0)
  var RAND_MODES = ['NONE', 'LINEAR', 'COSINE', 'CUBIC', 'SLEW'];
  /* the four PURE smoothings from v_{k-1}..v_{k+2}; each reads v_k at x = 0; CUBIC (Catmull-Rom) clamps at ±1 */
  function randInterp(vm, v0, v1, v2, x, mode) {
    if (mode === 1) return v0 + (v1 - v0) * x;
    if (mode === 2) return v0 + (v1 - v0) * (1 - Math.cos(Math.PI * x)) * .5;
    if (mode === 3) { var y = v0 + .5 * x * (v1 - vm + x * (2 * vm - 5 * v0 + 4 * v1 - v2 + x * (3 * (v0 - v1) + v2 - vm))); return y < -1 ? -1 : y > 1 ? 1 : y; }
    return v0;
  }
  function randAt(seed, k, x, mode) { if (mode === 0) return drawV(seed, k); return randInterp(drawV(seed, k - 1), drawV(seed, k), drawV(seed, k + 1), drawV(seed, k + 2), x, mode); }
  /* SLEW: from L0 toward v, ARRIVING at slewSec along the segment law at c = +0.5 (an envelope time means when it gets there) */
  function slewAt(L0, v, t, slewSec) { if (!(t < slewSec)) return v; return L0 + (v - L0) * segCurve(t / slewSec, U_ONEPOLE); }   // the one-pole's shape, arriving (the lab's c +0.5)
  /* where each cycle's SLEW glide starts, chained from the page's first draw. The engine records the cycles already played;
     here the whole page is the forecast at the current cycle length (PLACEHOLDER: the plugin reads the record). */
  function slewStarts(seed, k0, n, cycSec, sl) { var L0 = new Float64Array(n); for (var j = 0; j < n; j++) L0[j] = j ? slewAt(L0[j - 1], drawV(seed, k0 + j - 1), cycSec, sl) : drawV(seed, k0); return L0; }
  function randPage(mode, seed, k0, nCyc, N, cycSec, sl) {
    var o = new Float64Array(N + 1), L0 = mode === 4 ? slewStarts(seed, k0, nCyc, cycSec, sl) : null;
    for (var i = 0; i <= N; i++) { var p = i / N * nCyc, j = Math.min(nCyc - 1, Math.floor(p)), x = p - j;
      o[i] = mode === 4 ? slewAt(L0[j], drawV(seed, k0 + j), x * cycSec, sl) : randAt(seed, k0 + j, x, mode); }
    return o;
  }
  function randValue(mode, seed, p, cycSec, sl, page) {
    var k = Math.floor(p), x = p - k; if (mode !== 4) return randAt(seed, k, x, mode);
    var k0 = k - ((k % page) + page) % page, L0 = slewStarts(seed, k0, k - k0 + 1, cycSec, sl);
    return slewAt(L0[k - k0], drawV(seed, k), x * cycSec, sl);
  }
  var ModLaw = { K_ONEPOLE: K1, U_MAX: U_MAX, U_BANK: U_BANK, U_ONEPOLE: U_ONEPOLE, U_DEF: U_DEF, uFromLab: uFromLab, fmtU: fmtU, segCurve: segCurve, shapeAt: shapeAt, susOf: susOf, envAt: envAt, envTauAt: envTauAt, adsrPts: adsrPts, fitCycle: fitCycle,
    LFO_PRESETS: LFO_PRESETS, ENV_PRESETS: ENV_PRESETS, DIVS: DIVS, nearestDiv: nearestDiv, divLabel: divLabel, divName: divName, divIndex: divIndex,
    m32at: m32at, drawV: drawV, lfoSeedOf: lfoSeedOf, RAND_MODES: RAND_MODES, randInterp: randInterp, randAt: randAt, slewAt: slewAt,
    randPage: randPage, randValue: randValue, fSec: fSec };

  /* ---------- screen helpers: every glass is a .hd-viz well drawn through Viz.kit ---------- */
  function well(host, aspect, label) {
    var w = tag('div', 'hd-viz hd-mscr'); w.style.aspectRatio = aspect;
    var cv = tag('canvas'); cv.setAttribute('role', 'img'); cv.setAttribute('aria-label', label); w.appendChild(cv); host.appendChild(w);
    return cv;
  }
  function kit(cv) {
    var K = H.Viz.kit(cv), fam = (getComputedStyle(cv).getPropertyValue('--font-mono') || 'monospace').trim();
    /* GLASS TEXT. Every label's box is recorded (CSS px) on the canvas as __labels: mod_labels.py proves no trace,
       tick, glow or dot is painted under one. window.__modNoText (that test only) records without painting. */
    cv.__labels = [];
    K.text = function (s, x, y, role, align, px) { var g = K.g, sz = px || 9; g.font = (sz * K.V) + 'px ' + fam; g.textAlign = align || 'left'; g.textBaseline = 'alphabetic';
      var w = g.measureText(s).width / K.V, lx = align === 'right' ? x - w : align === 'center' ? x - w / 2 : x;
      cv.__labels.push({ s: s, x: lx, y: y - sz * .8, w: w, h: sz * 1.1 });
      if (!window.__modNoText) { g.fillStyle = K.c(role || 'grid'); g.fillText(s, x * K.V, y * K.V); } g.textAlign = 'left'; };
    /* a label that floats over moving marks (ORBITAL's body numbers) sits on a chip of the glass ground */
    K.label = function (s, x, y, role, align, px) { var g = K.g, sz = px || 9; g.font = (sz * K.V) + 'px ' + fam; var w = g.measureText(s).width / K.V;
      g.fillStyle = K.mix('ground', 'ground', 0, 1); g.fillRect((x - 3) * K.V, (y - sz) * K.V, (w + 6) * K.V, sz * 1.5 * K.V); K.text(s, x, y, role, align, px); };
    K.line = function (pts, role, a, lw, dash) { var g = K.g, V = K.V; g.strokeStyle = K.c(role, a); g.lineWidth = lw * V; g.setLineDash(dash ? dash.map(function (d) { return d * V; }) : []);
      g.beginPath(); for (var i = 0; i < pts.length; i++) i ? g.lineTo(pts[i][0] * V, pts[i][1] * V) : g.moveTo(pts[i][0] * V, pts[i][1] * V); g.stroke(); g.setLineDash([]); };
    K.beam = function (pts, role, lw) { K.g.lineJoin = 'round'; K.g.lineCap = 'round'; K.line(pts, role, .22, (lw || 2) * 3.4); K.line(pts, role, 1, lw || 2); K.g.lineCap = 'butt'; K.g.lineJoin = 'round'; };
    K.dot = function (x, y, r, role, a) { var g = K.g; g.fillStyle = K.c(role, a); g.beginPath(); g.arc(x * K.V, y * K.V, r * K.V, 0, TAU); g.fill(); };
    K.ring = function (x, y, r, role, lw, a) { var g = K.g; g.strokeStyle = K.c(role, a); g.lineWidth = (lw || 1.2) * K.V; g.beginPath(); g.arc(x * K.V, y * K.V, r * K.V, 0, TAU); g.stroke(); };
    K.cw = cv.clientWidth; K.ch = cv.clientHeight;
    return K;
  }
  /* a lane history: one Float32 ring per row, the newest at the right edge (what was RENDERED, logged per frame) */
  function Lanes(rows, n) { this.n = n || 240; this.w = 0; this.len = 0; this.a = rows.map(function () { return new Float32Array(n || 240); }); }
  Lanes.prototype.push = function (vals) { for (var r = 0; r < this.a.length; r++) this.a[r][this.w] = vals[r] || 0; this.w = (this.w + 1) % this.n; if (this.len < this.n) this.len++; };
  Lanes.prototype.at = function (r, k) { return this.a[r][(this.w - this.len + k + this.n) % this.n]; };   // k-th oldest point still held
  /* paint rows of lanes into [x0,x1]×[y0,y1]: 'level' a line (0..1 or ±1), 'step' the same line drawn stepped,
     'event' ticks only (an event has no value between its ticks). rows: { name, kind, bip, role } */
  /* THE LABEL BAND: each lane's label sits in its top LANE_TOP px (baseline LANE_LABEL); traces, ticks, glows and dots are
     scaled into the space below it, so no mark ever passes under a label (the integrator's legibility rule). */
  var LANE_LABEL = 10, LANE_TOP = 17;
  function paintLanes(K, L, rows, x0, x1, y0, y1) {
    var bh = (y1 - y0) / rows.length;
    rows.forEach(function (row, r) {
      var top = y0 + r * bh + LANE_TOP, bot = y0 + (r + 1) * bh - 4, Y = function (q) { return row.bip ? (top + bot) / 2 - q * (bot - top) / 2 : bot - q * (bot - top); };
      K.line([[x0, bot + 2], [x1, bot + 2]], 'grid', .18, 1);
      if (row.bip) K.line([[x0, Y(0)], [x1, Y(0)]], 'grid', .14, 1, [2, 3]);
      var n = L.len, k, X = function (k2) { return x1 - (n - 1 - k2) / (L.n - 1) * (x1 - x0); };
      if (row.kind === 'event') {
        for (k = 0; k < n; k++) if (L.at(r, k) > 0) { var x = X(k); K.line([[x, bot], [x, top + 3]], row.role || 'marker', 1, 1.4); K.dot(x, top + 3, 2.4, row.role || 'marker'); }
      } else if (n > 1) {
        var pts = []; for (k = 0; k < n; k++) { var y = Y(L.at(r, k)); if (row.kind === 'step' && k) pts.push([X(k), pts[pts.length - 1][1]]); pts.push([X(k), y]); }
        K.line(pts, row.role || 'meter', 1, 1.4);
      }
      K.text(row.name, x0 + 2, y0 + r * bh + LANE_LABEL, 'grid', 'left', 8.5);
    });
  }

  /* ---------- a house slider row (the PlainSlider look and DOM), driven by the HOUSE drag rule: size-drag (160 px) is
     the full range whatever the slider's width, shift x0.25, relative (never jump-to-click); double-click resets;
     Option + wheel sets the value; arrows only in keyboard mode. H.PlainSlider drags by its own width, which the house
     rule forbids, and the core is not ours to edit, so this family drives its own rows. onCommit: once per gesture. ---------- */
  function slider(el, o) {
    var lo = o.min, hi = o.max, toN = function (x) { return o.log ? Math.log(x / lo) / Math.log(hi / lo) : (x - lo) / (hi - lo); },
      fromN = function (n) { return o.log ? lo * Math.pow(hi / lo, n) : lo + (hi - lo) * n; }, val = o.value, st = null;
    el.className = 'hd-srow'; el.innerHTML = '';
    var lab = tag('label', 'hd-label', o.name), sl = tag('div', 'hd-slider hd-focus'), out = tag('output', 'hd-value');
    sl.setAttribute('role', 'slider'); sl.setAttribute('aria-label', o.path || o.name); sl.innerHTML = '<div class="track"><i class="fill"></i></div><i class="handle"></i>';
    out.style.textAlign = 'right'; el.appendChild(lab); el.appendChild(sl); el.appendChild(out);
    function paint() { var n = clamp(toN(val)); sl.style.setProperty('--v', n); sl.style.setProperty('--f0', '0px'); sl.style.setProperty('--fw', 'calc(' + n + ' * (100% - 13px) + 5px)');
      out.textContent = o.fmt(val); sl.setAttribute('aria-valuetext', o.fmt(val)); sl.tabIndex = keysOn(o) ? 0 : -1; }
    function set(x, fireIt) { val = clamp(x, Math.min(lo, hi), Math.max(lo, hi)); paint(); if (fireIt) fire(o.onInput, val); }
    sl.addEventListener('pointerdown', function (e) { if (e.button !== 0) return; try { sl.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
      sl.classList.add('drag'); st = { x: e.clientX, y: e.clientY, n: clamp(toN(val)), moved: false }; });
    sl.addEventListener('pointermove', function (e) { if (!st) return; var d = (e.clientX - st.x) - (e.clientY - st.y); if (Math.abs(d) > 1) st.moved = true;
      set(fromN(clamp(st.n + d / DRAG * (e.shiftKey ? .25 : 1))), true); });
    function end(e) { if (e && e.pointerId !== undefined && sl.hasPointerCapture && sl.hasPointerCapture(e.pointerId)) sl.releasePointerCapture(e.pointerId);
      sl.classList.remove('drag'); if (st) { var m = st.moved; st = null; if (m) fire(o.onCommit, val); } }
    sl.addEventListener('pointerup', end); sl.addEventListener('pointercancel', end); sl.addEventListener('lostpointercapture', end);
    sl.addEventListener('dblclick', function () { set(o.def === undefined ? o.value : o.def, true); fire(o.onCommit, val); });
    sl.addEventListener('wheel', function (e) { if (!e.altKey) return; e.preventDefault(); set(fromN(clamp(toN(val) - Math.sign(e.deltaY) * .01)), true); fire(o.onCommit, val); }, { passive: false });
    sl.addEventListener('keydown', function (e) { if (!keysOn(o)) return; var s = e.shiftKey ? .0025 : .01, n = clamp(toN(val));
      if (e.key === 'ArrowRight' || e.key === 'ArrowUp') n += s; else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') n -= s; else return;
      e.preventDefault(); set(fromN(clamp(n)), true); fire(o.onCommit, val); });
    paint();
    return { get: function () { return val; }, set: function (x) { set(x, false); }, el: sl };
  }

  /* ---------- MOD CARD HEADER: kind swatch, name and number, kind tag, scope tag, the RETRIGGER switch ---------- */
  var KIND = { lfo: 'LFO', env: 'ENV', macro: 'MACRO', engine: 'ENGINE' };
  function ModCardHeader(el, o) {
    o = o || {};
    el.classList.add('hd-mhead'); el.innerHTML = '';
    var sw = tag('i', 'kd'); sw.style.setProperty('--sc', 'var(--src-' + (o.kind || 'lfo') + ')'); sw.setAttribute('aria-hidden', 'true');
    var nm = tag('span', 'nm', o.name || 'LFO 1'), kt = tag('span', 'kt', o.kindLabel || KIND[o.kind || 'lfo']);
    kt.style.setProperty('--sc', 'var(--src-' + (o.kind || 'lfo') + ')');
    el.appendChild(sw); el.appendChild(nm); el.appendChild(kt);
    if (o.scope) el.appendChild(tag('span', 'vtag', o.scope));
    el.appendChild(tag('span', 'sp'));
    var on = !!o.retrig, btn = null;
    if (o.retrig !== null && o.retrig !== undefined) {
      var lab = tag('span', 'rt'); lab.appendChild(tag('span', 'hd-label', o.retrigLabel || 'Retrig'));
      btn = tag('button', 'hd-toggle hd-focus'); btn.type = 'button'; btn.setAttribute('role', 'switch');
      btn.setAttribute('aria-label', (o.name || 'Modulator') + ' · Retrigger' + (o.retrigHint ? ' (' + o.retrigHint + ')' : ''));
      if (o.retrigHint) btn.title = o.retrigHint;
      lab.appendChild(btn); el.appendChild(lab);
      btn.addEventListener('click', function () { set(!on); fire(o.onRetrig, on); fire(o.onCommit, { retrig: on }); });
    }
    if (o.extra) el.appendChild(o.extra);
    function set(v) { on = !!v; if (btn) btn.setAttribute('aria-checked', String(on)); }
    set(on);
    return { set: set, get: function () { return on; }, el: el };
  }

  /* ---------- RATE + SYNC: one control for a rate in Hz (or steps/s) or a tempo division (ADR-266 r10) ---------- */
  var CHL = '<svg viewBox="0 0 8 8"><path d="M6 0v8L1 4z"/></svg>', CHR = '<svg viewBox="0 0 8 8"><path d="M2 0v8l5-4z"/></svg>';
  function RateSync(el, o) {
    o = o || {};
    var st = { sync: o.sync ? 1 : 0, hz: o.hz === undefined ? 1 : o.hz, beats: o.beats === undefined ? 1 : o.beats }, unit = o.unit || 'Hz', name = o.name || 'Rate';
    var bpm = function () { return typeof o.bpm === 'function' ? o.bpm() : (o.bpm || 120); };
    var path = o.path || name;
    el.classList.add('hd-rs'); el.innerHTML = '';
    var top = tag('div', 'hd-srow'), lab = tag('label', 'hd-label', name), modes = tag('div', 'hd-pills');
    modes.setAttribute('role', 'radiogroup'); modes.setAttribute('aria-label', path + ' · mode');
    var mb = ['FREE ' + unit, 'SYNC'].map(function (t, i) { var b = tag('button', 'hd-pill hd-focus', t); b.type = 'button'; b.setAttribute('role', 'radio');
      b.addEventListener('click', function () { if (st.sync !== i) { st.sync = i; sync(); changed(); commit(); } }); modes.appendChild(b); return b; });
    var per = tag('output', 'hd-value rs-per'); per.setAttribute('aria-live', 'off');
    top.appendChild(lab); top.appendChild(modes); top.appendChild(per); el.appendChild(top);
    var slot = tag('div', 'rs-slot'); el.appendChild(slot);
    var free = tag('div'); slot.appendChild(free);
    var lo = o.min || .02, hi = o.max || 40;
    var fmtHz = function (x) { return x.toFixed(x < 1 ? 3 : x < 10 ? 2 : 1) + ' ' + unit; };
    var ps;
    ps = slider(free, { name: 'Free', path: path + ' · free rate', min: lo, max: hi, log: true, value: st.hz, def: o.defHz || (o.hz === undefined ? 1 : o.hz), fmt: fmtHz, keys: o.keys,
      onInput: function (v) { st.hz = v; changed(); }, onCommit: commit });
    var div = tag('div', 'hd-srow'); slot.appendChild(div);
    div.appendChild(tag('label', 'hd-label', 'Division'));
    var stp = tag('div', 'hd-stepper rs-step hd-focus'); stp.setAttribute('role', 'spinbutton'); stp.setAttribute('aria-label', path + ' · tempo division');
    stp.setAttribute('aria-valuemin', '1'); stp.setAttribute('aria-valuemax', String(DIVS.length));
    stp.innerHTML = '<button type="button" tabindex="-1" aria-label="slower division">' + CHL + '</button><output class="hd-value rs-name"></output><button type="button" tabindex="-1" aria-label="faster division">' + CHR + '</button>';
    div.appendChild(stp); var hzo = tag('output', 'hd-value rs-hz'); div.appendChild(hzo);
    var nameEl = stp.querySelector('.rs-name'), bs = stp.querySelectorAll('button');
    /* step through the list; an off-list stored value lands on its nearest name first (it is never silently changed) */
    function step(d) { var i = divIndex(st.beats), n = i < 0 ? nearestDiv(st.beats) : clamp(i + d, 0, DIVS.length - 1); if (DIVS[n][2] !== st.beats) { st.beats = DIVS[n][2]; sync(); changed(); commit(); } }
    bs[0].addEventListener('click', function () { step(-1); }); bs[1].addEventListener('click', function () { step(1); });
    nameEl.addEventListener('dblclick', function () { st.beats = o.defBeats || 1; sync(); changed(); commit(); });
    stp.addEventListener('wheel', function (e) { if (!e.altKey) return; e.preventDefault(); step(e.deltaY < 0 ? 1 : -1); }, { passive: false });   // B374: Option + wheel sets the value
    stp.addEventListener('keydown', function (e) { if (!keysOn(o)) return; if (e.key === 'ArrowUp' || e.key === 'ArrowRight') step(1); else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') step(-1); else return; e.preventDefault(); });
    function hz() { return st.sync ? bpm() / 60 / st.beats : st.hz; }
    function sync() {
      mb.forEach(function (b, i) { b.setAttribute('aria-checked', String(i === st.sync)); b.tabIndex = i === st.sync ? 0 : -1; });
      free.hidden = !!st.sync; div.hidden = !st.sync;
      var nm = divName(st.beats); nameEl.textContent = nm; nameEl.title = nm; stp.setAttribute('aria-valuetext', nm + ', ' + (+st.beats.toFixed(5)) + ' beat');
      stp.setAttribute('aria-valuenow', String(nearestDiv(st.beats) + 1)); stp.tabIndex = keysOn(o) ? 0 : -1;
      hzo.textContent = fmtHz(bpm() / 60 / st.beats);
      per.textContent = fSec(1 / hz()); per.title = '1 cycle = ' + fSec(1 / hz());
    }
    function changed() { sync(); fire(o.onChange, get()); }
    function commit() { fire(o.onCommit, get()); }
    function get() { return { sync: st.sync, hz: st.hz, beats: st.beats, rate: hz(), name: st.sync ? divName(st.beats) : fmtHz(st.hz) }; }
    sync();
    return { get: get, rate: hz, refresh: sync,
      set: function (p) { if (p.sync !== undefined) st.sync = p.sync ? 1 : 0; if (p.beats !== undefined) st.beats = p.beats; if (p.hz !== undefined) { st.hz = p.hz; ps.set(p.hz); } sync(); } };
  }

  /* ---------- TEMPO DIVISIONS: the 22 names in three families, a picker panel for RateSync's SYNC side ---------- */
  function TempoDivisions(el, o) {
    o = o || {};
    var beats = o.beats === undefined ? 1 : o.beats, bpm = function () { return typeof o.bpm === 'function' ? o.bpm() : (o.bpm || 120); };
    el.classList.add('hd-tdiv'); el.innerHTML = '';
    var grid = tag('div', 'td-well'); grid.setAttribute('role', 'radiogroup'); grid.setAttribute('aria-label', (o.path || 'Tempo division') + ' list');
    ['STRAIGHT', 'DOTTED', 'TRIPLET'].forEach(function (h) { grid.appendChild(tag('div', 'td-h', h)); });
    var cells = [];
    for (var r = 0; r < 8; r++) for (var f = 0; f < 3; f++) (function (r, f) {
      var i = -1; for (var q = 0; q < DIVS.length; q++) if (DIVS[q][3] === f && DIVS[q][4] === r) i = q;
      if (i < 0) { grid.appendChild(tag('div', 'td-gap', f === 1 ? '12 beats: out of range' : 'below range')); return; }
      var b = tag('button', 'hd-pill hd-focus', divLabel(i)); b.type = 'button'; b.setAttribute('role', 'radio');
      b.setAttribute('aria-label', divLabel(i) + ', ' + (+DIVS[i][2].toFixed(5)) + ' beat');
      b.addEventListener('click', function () { beats = DIVS[i][2]; paint(); fire(o.onChange, beats); fire(o.onCommit, beats); });
      grid.appendChild(b); cells.push([b, i]);
    })(r, f);
    el.appendChild(grid);
    var read = tag('div', 'hd-hint td-read'); read.setAttribute('aria-live', 'polite'); el.appendChild(read);
    function paint() {
      /* lit only on an EXACT match: an off-list value lights nothing rather than letting the nearest name pose as it */
      cells.forEach(function (c) { var on = DIVS[c[1]][2] === beats; c[0].setAttribute('aria-checked', String(on)); });
      var sec = beats * 60 / bpm(), i = nearestDiv(beats);
      read.textContent = divName(beats) + ' = ' + (+beats.toFixed(5)) + ' beat = ' + fSec(sec) + ' = ' + (1 / sec).toFixed(3) + ' Hz at ' + bpm() + ' BPM · ' + (i + 1) + ' of 22, slow to fast';
    }
    paint();
    return { set: function (b) { beats = b; paint(); }, get: function () { return beats; }, refresh: paint };
  }

  /* ---------- THE BREAKPOINT EDITOR (one editor for both modulators; the lab's gestures, the house's rules) ---------- */
  /* cfg: { cv, lfo, pts(), sus(), tMax(), enabled(), onEdit(kind), onCommit(), extras(K, M), status(text), name } */
  function bpEditor(cfg) {
    var cv = cfg.cv, drag = null, sel = -1, hover = null;
    function M() {
      var W = cv.clientWidth, Ht = cv.clientHeight, x0 = 30, x1 = W - 12, y0 = 24, y1 = Ht - 22,   /* y0: the caption band; no point, ring or glow reaches it */ tMax = drag ? drag.win : cfg.tMax(), lo = cfg.lfo ? -1 : 0, hi = 1;
      return { W: W, H: Ht, x0: x0, x1: x1, y0: y0, y1: y1, tMax: tMax, lo: lo, hi: hi,
        X: function (t) { return x0 + t / tMax * (x1 - x0); }, Y: function (v) { return y1 - (v - lo) / (hi - lo) * (y1 - y0); },
        T: function (x) { return clamp((x - x0) / (x1 - x0) * tMax, 0, tMax); }, V: function (y) { return clamp(lo + (y1 - y) / (y1 - y0) * (hi - lo), lo, hi); } };
    }
    function pos(e) { var r = cv.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; }
    /* points first, then the mid-segment curve handles; each a 28 px target (HIT radius) */
    function hit(p) {
      if (!cfg.enabled()) return null;
      var m = M(), pts = cfg.pts(), i, best = null, bd = HIT;
      for (i = 0; i < pts.length; i++) { var d = Math.hypot(m.X(pts[i].t) - p[0], m.Y(pts[i].v) - p[1]); if (d < bd) { bd = d; best = { part: 'pt', i: i }; } }
      if (best) return best;
      bd = HIT;
      for (i = 0; i < pts.length - 1; i++) { var a = pts[i], b = pts[i + 1]; if (b.t - a.t < 1e-9 || a.v === b.v) continue;
        var tm = (a.t + b.t) / 2, d2 = Math.hypot(m.X(tm) - p[0], m.Y(shapeAt(pts, tm)) - p[1]); if (d2 < bd) { bd = d2; best = { part: 'crv', i: i }; } }
      return best;
    }
    function fixedT(i) { var n = cfg.pts().length; return i === 0 || (cfg.lfo && i === n - 1); }
    function del(i) { var pts = cfg.pts(); if (i === 0 || (cfg.lfo && i === pts.length - 1) || pts.length <= 2) { cfg.status('An end point anchors the axis: it moves but is never deleted.'); return false; }
      pts.splice(i, 1); if (sel >= pts.length) sel = pts.length - 1; cfg.onEdit('delete', i); return true; }
    function add(t, v) { var pts = cfg.pts(); if (pts.length >= MAXPTS) { cfg.status('At most ' + MAXPTS + ' points.'); return false; }
      var i = 1; while (i < pts.length && pts[i].t <= t) i++; if (cfg.lfo && i >= pts.length) i = pts.length - 1;
      pts.splice(i, 0, { t: t, v: v, u: 0 }); sel = i; cfg.onEdit('add', i); return true; }
    cv.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    cv.addEventListener('pointerdown', function (e) {
      var hh = hit(pos(e));
      if (e.button === 2) { if (hh && hh.part === 'pt' && del(hh.i)) cfg.onCommit(); return; }
      if (e.button !== 0 || !hh) return;
      var p = cfg.pts()[hh.i]; if (hh.part === 'pt') sel = hh.i;
      drag = { part: hh.part, i: hh.i, x: e.clientX, y: e.clientY, t0: p.t, v0: p.v, u0: p.u, moved: false, win: cfg.tMax() };
      try { cv.setPointerCapture(e.pointerId); } catch (_) { /* a pointer the browser no longer tracks must not cost the drag */ }
      cfg.onEdit('select');
    });
    cv.addEventListener('pointermove', function (e) {
      if (!drag) { var hh = hit(pos(e)); hover = hh; cv.style.cursor = !hh ? (cfg.enabled() ? 'crosshair' : 'default') : hh.part === 'pt' ? 'grab' : 'ns-resize'; return; }
      var dx = e.clientX - drag.x, dy = e.clientY - drag.y, k = e.shiftKey ? .25 : 1, m = M(), pts = cfg.pts(), p = pts[drag.i];
      if (!p) return;
      if (Math.abs(dx) + Math.abs(dy) > 2) drag.moved = true; if (!drag.moved) return;
      /* RELATIVE drags (house rule): the point moves by the pointer's travel, never jumps to the pointer */
      if (drag.part === 'pt') {
        if (!fixedT(drag.i)) { var lo = pts[drag.i - 1].t, hi = drag.i < pts.length - 1 ? pts[drag.i + 1].t : (cfg.lfo ? 1 : 10);
          p.t = clamp(drag.t0 + dx / (m.x1 - m.x0) * m.tMax * k, lo, hi); }
        p.v = clamp(drag.v0 - dy / (m.y1 - m.y0) * (m.hi - m.lo) * k, m.lo, m.hi);
      } else {
        /* up raises the midpoint: +u on a rising segment, -u on a falling one; size-drag (160 px) is the whole -2.5..+2.5 */
        var q = pts[drag.i + 1], s = q.v >= p.v ? 1 : -1; p.u = clamp(drag.u0 - s * dy / DRAG * 2 * U_MAX * k, -U_MAX, U_MAX);
      }
      cfg.onEdit('move', drag.i, drag.part);
    });
    function end(e) {
      if (e && e.pointerId !== undefined && cv.hasPointerCapture && cv.hasPointerCapture(e.pointerId)) cv.releasePointerCapture(e.pointerId);
      if (!drag) return; var moved = drag.moved; drag = null; cfg.onEdit('end'); if (moved) cfg.onCommit();   // each gesture: one undo step
    }
    cv.addEventListener('pointerup', end); cv.addEventListener('pointercancel', end); cv.addEventListener('lostpointercapture', end);
    cv.addEventListener('dblclick', function (e) {
      var p = pos(e), hh = hit(p), m = M();
      if (!cfg.enabled()) return;
      if (hh && hh.part === 'pt') { if (del(hh.i)) cfg.onCommit(); }
      else if (hh && hh.part === 'crv') { cfg.pts()[hh.i].u = cfg.uDef ? cfg.uDef(hh.i) : 0; cfg.onEdit('reset', hh.i); cfg.onCommit(); }   // double-click: the bend's default
      else if (!hh && add(m.T(p[0]), m.V(p[1]))) cfg.onCommit();
    });
    cv.addEventListener('keydown', function (e) {   // accelerators, keyboard mode only; every one has the mouse route above
      if (!cfg.keys() || !cfg.enabled()) return;
      var pts = cfg.pts(), m = M(), s = e.shiftKey ? .0025 : .01;
      if (sel < 0 || sel >= pts.length) sel = 0;
      var p = pts[sel], hiT = sel < pts.length - 1 ? pts[sel + 1].t : (cfg.lfo ? 1 : 10), loT = sel ? pts[sel - 1].t : 0;
      if (e.key === 'Tab') return;
      if (e.key === ']') sel = Math.min(pts.length - 1, sel + 1); else if (e.key === '[') sel = Math.max(0, sel - 1);
      else if (e.key === 'ArrowUp') p.v = clamp(p.v + s * (m.hi - m.lo), m.lo, m.hi); else if (e.key === 'ArrowDown') p.v = clamp(p.v - s * (m.hi - m.lo), m.lo, m.hi);
      else if (e.key === 'ArrowRight' && !fixedT(sel)) p.t = clamp(p.t + s * m.tMax, loT, hiT); else if (e.key === 'ArrowLeft' && !fixedT(sel)) p.t = clamp(p.t - s * m.tMax, loT, hiT);
      else if (e.key === 'Delete' || e.key === 'Backspace') del(sel); else return;
      e.preventDefault(); cfg.onEdit('key', sel); cfg.onCommit();
    });
    function paint(K) {
      var m = M(), pts = cfg.pts(), lfo = cfg.lfo, i;
      K.tube(); K.scan();
      var vt = lfo ? [-1, -.5, 0, .5, 1] : [0, .25, .5, .75, 1];
      vt.forEach(function (q) { K.line([[m.x0, m.Y(q)], [m.x1, m.Y(q)]], 'grid', q === 0 && lfo ? .4 : .2, 1);
        if (q === 1 || q === 0 || q === -1 || (!lfo && q === .5)) K.text((q > 0 && lfo ? '+' : q < 0 ? '−' : '') + Math.abs(q), 4, m.Y(q) + 3, 'grid'); });
      var st = lfo ? .25 : niceStep(m.tMax, 5), n = 0;
      for (var t = 0; t <= m.tMax + 1e-9 && n < 40; t += st, n++) { K.line([[m.X(t), m.y0], [m.X(t), m.y1]], 'grid', .2, 1);
        var xl = m.X(t), al = t === 0 ? 'left' : xl > m.x1 - 22 ? 'right' : 'center';
        K.text(t === 0 ? '0' : lfo ? (t === 1 ? '1' : String(t).replace(/^0/, '')) : fSec(t), al === 'right' ? m.x1 + 4 : xl, m.H - 6, 'grid', al); }
      var N = Math.max(60, Math.round(m.x1 - m.x0)), line = [];
      for (var k = 0; k <= N; k++) { var tt = k / N * m.tMax * (lfo ? (1 - 1e-9) : 1); line.push([m.X(tt), m.Y(cfg.curve(tt))]); }
      if (!lfo) { var g = K.g, V = K.V; g.beginPath(); line.forEach(function (q, j) { j ? g.lineTo(q[0] * V, q[1] * V) : g.moveTo(q[0] * V, q[1] * V); });
        g.lineTo(m.X(m.tMax) * V, m.Y(0) * V); g.lineTo(m.X(0) * V, m.Y(0) * V); g.closePath(); g.fillStyle = K.c('value', .18); g.fill(); }
      K.beam(line, 'value', 2);
      if (cfg.extras) cfg.extras(K, m);
      for (i = 0; i < pts.length - 1; i++) { var a = pts[i], b = pts[i + 1]; if (b.t - a.t < 1e-9 || a.v === b.v) continue;   // the bend handles, on the law
        var tm = (a.t + b.t) / 2, hv = hover && hover.part === 'crv' && hover.i === i;
        K.ring(m.X(tm), m.Y(shapeAt(pts, tm)), 3.6, 'value', hv ? 2 : 1.3, hv ? 1 : .8); }
      for (i = 0; i < pts.length; i++) { var x = m.X(pts[i].t), y = m.Y(pts[i].v), r = 4, gg = K.g, VV = K.V;
        gg.fillStyle = K.c('value'); gg.fillRect((x - r) * VV, (y - r) * VV, 2 * r * VV, 2 * r * VV);
        gg.strokeStyle = K.mix('ground', 'ground', 0, 1); gg.lineWidth = VV; gg.strokeRect((x - r) * VV, (y - r) * VV, 2 * r * VV, 2 * r * VV);
        if (i === sel || (drag && drag.part === 'pt' && drag.i === i)) { gg.strokeStyle = K.c('marker'); gg.lineWidth = 1.5 * VV; gg.strokeRect((x - r - 2.5) * VV, (y - r - 2.5) * VV, (2 * r + 5) * VV, (2 * r + 5) * VV); } }
    }
    function describe() { var pts = cfg.pts(); if (sel < 0 || sel >= pts.length) return ''; var p = pts[sel];
      return 'Point ' + (sel + 1) + ' of ' + pts.length + ' · ' + (cfg.lfo ? 't ' + p.t.toFixed(3) + ' cycle' : 't ' + fSec(p.t)) + ' · ' + (cfg.lfo ? sgn2(p.v) : p.v.toFixed(2)) + (sel < pts.length - 1 ? ' · bend ' + fmtU(p.u) : ''); }
    return { paint: paint, M: M, describe: describe, dragging: function () { return !!drag; }, select: function (i) { sel = i; }, selected: function () { return sel; }, hit: hit };
  }
  function pills(host, names, role, label, onPick) {
    var g = tag('div', 'hd-pills'); if (role === 'radio') { g.setAttribute('role', 'radiogroup'); g.setAttribute('aria-label', label); } else { g.setAttribute('role', 'group'); g.setAttribute('aria-label', label); }
    var bs = names.map(function (n, i) { var b = tag('button', 'hd-pill hd-focus', n); b.type = 'button'; if (role === 'radio') b.setAttribute('role', 'radio');
      b.addEventListener('click', function () { onPick(i, n); }); g.appendChild(b); return b; });
    host.appendChild(g);
    return { el: g, set: function (cur) { bs.forEach(function (b, i) { b.setAttribute('aria-checked', String(i === cur)); b.tabIndex = role === 'radio' && i !== cur ? -1 : 0; }); }, bs: bs };
  }
  function mrow(host, label) { var r = tag('div', 'hd-mrow'); r.appendChild(tag('span', 'hd-label ml', label)); host.appendChild(r); return r; }

  /* ---------- LFO SHAPE EDITOR: breakpoints over one cycle (±1), S&H and RANDOM as shapes (ADR-266 r6) ---------- */
  var KINDS = ['DRAWN', 'S&H', 'RANDOM'], PAGE = 8;
  function LfoShapeEditor(el, o) {
    o = o || {};
    var st = { pts: o.pts || LFO_PRESETS.SINE(), kind: o.kind || 0, smooth: o.smooth === undefined ? 1 : o.smooth, slewMs: o.slewMs || 80, seed: o.seed === undefined ? 1234 : o.seed, phase: o.phase || 0, draw: 0 };
    var name = o.name || 'LFO 1';
    el.classList.add('hd-medit'); el.innerHTML = '';
    var cv = well(el, o.aspect || '2.4 / 1', name + ' shape editor');
    var status = tag('div', 'hd-hint ms-status'); status.setAttribute('aria-live', 'polite'); el.appendChild(status);
    var dirty = true, hint = '';
    function cycSec() { return typeof o.cycleSec === 'function' ? o.cycleSec() : 1; }
    function seed() { return lfoSeedOf(st.seed); }
    var E = bpEditor({ cv: cv, lfo: true, name: name, pts: function () { return st.pts; }, tMax: function () { return 1; },
      enabled: function () { return st.kind === 0; }, keys: function () { return keysOn(o); },
      curve: function (t) { return shapeAt(st.pts, t); },
      onEdit: function (k) { dirty = true; hint = ''; if (k !== 'select' && k !== 'end') fire(o.onChange, get()); },
      onCommit: function () { dirty = true; fire(o.onCommit, get()); },
      status: function (s) { hint = s; dirty = true; },
      extras: function (K, m) {
        var pts = st.pts;
        K.line([[m.X(1), m.Y(pts[pts.length - 1].v)], [m.X(1), m.Y(pts[0].v)]], 'value', .6, 1, [3, 3]);   // the wrap: a real step, not a handle
        var xp = m.X(st.phase); K.line([[xp, m.y0], [xp, m.y1]], 'value', .7, 1, [3, 3]); K.text('φ', xp + 3, m.y0 - 9, 'grid');
        var p = typeof o.position === 'function' ? o.position() : null;
        if (p !== null && p !== undefined) { var x = frac(p); K.dot(m.X(x), m.Y(shapeAt(pts, x)), 8, 'meter', .3); K.dot(m.X(x), m.Y(shapeAt(pts, x)), 4.2, 'meter'); }
        K.text(label(), m.x1 - 2, m.y0 - 9, 'grid', 'right');
      } });
    function label() { return '1 cycle = ' + fSec(cycSec()) + (o.rateLabel ? ' · ' + o.rateLabel() : ''); }
    /* S&H / RANDOM: eight cycles of the stream instead of breakpoints (a new seeded value each cycle is no fixed cycle) */
    function paintRandom(K) {
      var m = E.M(), mode = st.kind === 1 ? 0 : st.smooth, sd = seed(), p = typeof o.position === 'function' ? o.position() : null;
      var kNow = p === null || p === undefined ? 0 : Math.floor(p), k0 = kNow - ((kNow % PAGE) + PAGE) % PAGE, sl = st.slewMs / 1000, cs = cycSec();
      var X = function (q) { return m.x0 + q / PAGE * (m.x1 - m.x0); }, Y = function (q) { return m.y1 - (q + 1) / 2 * (m.y1 - m.y0); };
      K.tube(); K.scan();
      [-1, -.5, 0, .5, 1].forEach(function (q) { K.line([[m.x0, Y(q)], [m.x1, Y(q)]], 'grid', q ? .2 : .4, 1); if (q === 1 || q === 0 || q === -1) K.text((q > 0 ? '+' : q < 0 ? '−' : '') + Math.abs(q), 4, Y(q) + 3, 'grid'); });
      for (var j = 0; j <= PAGE; j++) { K.line([[X(j), m.y0], [X(j), m.y1]], 'grid', .2, 1); if (j < PAGE) K.text('k ' + (k0 + j), X(j) + 3, m.H - 6, 'grid'); }
      var N = Math.max(PAGE * 24, Math.round(m.x1 - m.x0)), toPts = function (arr) { var q = []; for (var i = 0; i < arr.length; i++) q.push([X(i / (arr.length - 1) * PAGE), Y(arr[i])]); return q; };
      if (mode === 4) K.line(toPts(randPage(0, sd, k0, PAGE, N, cs, sl)), 'value', .45, 1, [3, 3]);   // SLEW trails the S&H step it chases
      K.beam(toPts(randPage(mode, sd, k0, PAGE, N, cs, sl)), 'value', 2);
      for (j = 0; j < PAGE; j++) K.dot(X(j), Y(drawV(sd, k0 + j)), 3, 'value');
      if (p !== null && p !== undefined) { var xp = X(p - k0), v = randValue(mode, sd, p, cs, sl, PAGE);
        K.line([[xp, m.y0], [xp, m.y1]], 'meter', .5, 1); K.dot(xp, Y(v), 8, 'meter', .3); K.dot(xp, Y(v), 4.2, 'meter'); }
      K.text(KINDS[st.kind] + (st.kind === 2 ? ' · ' + RAND_MODES[mode] + (mode === 4 ? ' ' + Math.round(st.slewMs) + ' ms' : '') : '') + ' · ' + label() + ' · seed ' + st.seed, m.x1 - 2, m.y0 - 9, 'grid', 'right');
    }
    var r1 = mrow(el, 'Shape'), kp = pills(r1, KINDS, 'radio', name + ' · shape kind', function (i) { if (st.kind !== i) { st.kind = i; sync(); fire(o.onChange, get()); fire(o.onCommit, get()); } });
    var r2 = mrow(el, 'Draw'), names = Object.keys(LFO_PRESETS).concat(['MIRROR', 'INVERT']);
    pills(r2, names, 'button', name + ' · draw a shape', function (i, n) {
      if (n === 'MIRROR') { var src = st.pts, len = src.length, out = [], q;   // exact under this curve law: run backwards = negate c
        for (q = 0; q < len; q++) out.push({ t: 1 - src[len - 1 - q].t, v: src[len - 1 - q].v, u: 0 }); for (q = 0; q < len - 1; q++) out[q].u = -src[len - 2 - q].u; st.pts = out; }
      else if (n === 'INVERT') st.pts.forEach(function (p) { p.v = -p.v; });
      else st.pts = n === 'STEPS' ? LFO_PRESETS.STEPS(st.seed, ++st.draw) : LFO_PRESETS[n]();
      st.kind = 0; E.select(-1); sync(); fire(o.onChange, get()); fire(o.onCommit, get());
    });
    var r3 = mrow(el, 'Smooth'), sp = pills(r3, RAND_MODES, 'radio', name + ' · RANDOM smoothing', function (i) { st.smooth = i; st.kind = 2; sync(); fire(o.onChange, get()); fire(o.onCommit, get()); });
    var slw = tag('div'); el.appendChild(slw);
    slider(slw, { keys: o.keys, onCommit: function () { fire(o.onCommit, get()); }, name: 'Slew', path: name + ' · RANDOM slew time', min: 1, max: 2000, log: true, value: st.slewMs, def: 80, fmt: function (x) { return x.toFixed(x < 10 ? 1 : 0) + ' ms'; },
      onInput: function (v) { st.slewMs = v; dirty = true; fire(o.onChange, get()); } });
    function sync() {
      kp.set(st.kind); sp.set(st.kind === 2 ? st.smooth : -1);
      r3.classList.toggle('idle', st.kind !== 2); slw.classList.toggle('idle', !(st.kind === 2 && st.smooth === 4));
      cv.tabIndex = keysOn(o) && st.kind === 0 ? 0 : -1; cv.classList.toggle('hd-focus', true);
      cv.setAttribute('aria-label', name + ' shape: ' + (st.kind === 0 ? 'drawn, ' + st.pts.length + ' points' : st.kind === 1 ? 'S&H, a new seeded value each cycle' : 'RANDOM, ' + RAND_MODES[st.smooth] + ' smoothing'));
      dirty = true;
    }
    function get() { return { pts: st.pts.map(function (p) { return { t: p.t, v: p.v, u: p.u }; }), kind: st.kind, shape: KINDS[st.kind], smooth: st.smooth, slewMs: st.slewMs, seed: st.seed, phase: st.phase }; }
    function draw() {
      var K = kit(cv); if (st.kind === 0) E.paint(K); else paintRandom(K);
      var s = hint || (st.kind === 0 ? (E.describe() || 'Drag a point · drag a ring to bend its segment · double-click: add a point, delete a point, reset a bend') : 'S&H and RANDOM have no points: DRAWN brings the kept breakpoints back.');
      if (status.textContent !== s) status.textContent = s;
      dirty = false;
    }
    sync();
    return { draw: draw, get: get, value: function (p) { return st.kind === 0 ? shapeAt(st.pts, frac(p)) : randValue(st.kind === 1 ? 0 : st.smooth, seed(), p, cycSec(), st.slewMs / 1000, PAGE); },
      set: function (p) { if (p.phase !== undefined) st.phase = p.phase; if (p.seed !== undefined) st.seed = p.seed; if (p.pts) st.pts = p.pts; if (p.kind !== undefined) st.kind = p.kind; sync(); },
      dirty: function () { return dirty || E.dragging(); }, canvas: cv, editor: E };
  }

  /* ---------- ENVELOPE EDITOR: breakpoints in seconds, a sustain point, release from the level reached ---------- */
  function EnvelopeEditor(el, o) {
    o = o || {};
    var st = { adsr: o.adsr || { a: .01, d: .3, s: .5, r: .4 }, pts: null, sus: 2, custom: false }, name = o.name || 'ENV 1';
    st.pts = o.pts || adsrPts(st.adsr.a, st.adsr.d, st.adsr.s, st.adsr.r); if (o.sus !== undefined) st.sus = o.sus; if (o.pts) st.custom = true;
    var win = 1, dirty = true, hint = '';
    function winOf() { return Math.max(.05, st.pts[st.pts.length - 1].t) * 1.2; }   // 1.2x: room to drag the last point later; frozen while dragging
    win = winOf();
    el.classList.add('hd-medit'); el.innerHTML = '';
    var cv = well(el, o.aspect || '2.4 / 1', name + ' envelope editor');
    var E = bpEditor({ cv: cv, lfo: false, name: name, pts: function () { return st.pts; }, tMax: function () { return win; },
      /* a reset bend takes the ENGINE's stage default (C.12): the first segment is the attack (u 0), every later one a decay or
         a release (u +1) */
      uDef: function (i) { var s = susOf(st.pts, st.sus); return i === 0 ? U_DEF.attack : s >= 0 && i >= s ? U_DEF.release : U_DEF.decay; },
      enabled: function () { return true; }, keys: function () { return keysOn(o); },
      curve: function (t) { var s = susOf(st.pts, st.sus); return envAt(st.pts, st.sus, t, s >= 0 ? st.pts[s].t : null); },
      onEdit: function (k, i, part) {
        if (k === 'delete') { if (st.sus > i) st.sus--; else if (st.sus === i) st.sus = Math.min(i, st.pts.length - 2); }
        if (k === 'add') { if (st.sus >= i) st.sus++; }
        /* a bend on the ADSR shape is its stage's curve row (attackCurve, decayCurve, releaseCurve): it does not make it hand-drawn */
        if (k !== 'select' && k !== 'end' && k !== 'reset' && part !== 'crv') st.custom = true;
        if (k === 'end' || k === 'add' || k === 'delete') win = winOf();
        dirty = true; hint = ''; if (k !== 'select' && k !== 'end') { syncSus(); fire(o.onChange, get()); } },
      onCommit: function () { dirty = true; fire(o.onCommit, get()); },
      status: function (s) { hint = s; dirty = true; },
      extras: function (K, m) {
        var s = susOf(st.pts, st.sus);
        if (s >= 0) { var xs = m.X(st.pts[s].t); K.line([[xs, m.y0], [xs, m.y1]], 'marker', 1, 1, [3, 3]); K.text('SUS', xs + 3, m.y0 - 9, 'marker'); }
        else K.text('ONE-SHOT', m.x0 + 4, m.y0 - 9, 'marker');
        var vs = typeof o.voices === 'function' ? o.voices() : [];
        vs.forEach(function (vc) { var t = vc.t, tOff = vc.tOff === undefined ? null : vc.tOff;
          var td = s < 0 ? t : (tOff === null ? Math.min(t, st.pts[s].t) : st.pts[s].t + (t - tOff)); if (td > m.tMax) return;
          var lv = envAt(st.pts, st.sus, t, tOff, vc.v0); K.dot(m.X(td), m.Y(lv), 7, 'meter', .3); K.dot(m.X(td), m.Y(lv), 4, 'meter'); });
      } });
    var status = tag('div', 'hd-hint ms-status'); status.setAttribute('aria-live', 'polite'); el.appendChild(status);
    var r1 = mrow(el, 'Sustain');
    var slot = tag('span', 'es-sus'); r1.appendChild(slot); var susHint = tag('span', 'hd-hint es-sh'); r1.appendChild(susHint);
    function setSus(n) { if (n !== susOf(st.pts, st.sus)) { st.sus = n; st.custom = true; syncSus(); dirty = true; fire(o.onChange, get()); fire(o.onCommit, get()); } }
    /* option 0 is NONE (a one-shot), option i is point i: the range follows the number of points */
    var susStep = dynStepper(slot, function (d) { var n = st.pts.length;
      return H.Stepper(d, { name: 'Sustain point', path: name + ' · sustain point', options: ['NONE'].concat(st.pts.slice(0, n - 1).map(function (q, i) { return 'P' + (i + 1); })), value: 0, def: 0,
        spoken: function (i) { return i === 0 ? 'none: one-shot' : 'point ' + i; }, keys: function () { return keysOn(o); }, onInput: function (i) { setSus(i - 1); } }); });
    function syncSus() { var s = susOf(st.pts, st.sus); susStep(st.pts.length, s + 1);
      susHint.textContent = s < 0 ? 'one-shot: the gate only starts it' : 'holds here while the key is down'; }
    var sliders = tag('div', 'es-adsr'); el.appendChild(sliders);
    var A = st.adsr, adsrDefs = [['a', 'Attack', .001, 2, true, fSec, .01], ['d', 'Decay', .005, 4, true, fSec, .3], ['s', 'Sustain', 0, 1, false, f2, .5], ['r', 'Release', .005, 8, true, fSec, .4]];
    adsrDefs.forEach(function (d) { var row = tag('div'); sliders.appendChild(row);
      slider(row, { name: d[1], path: name + ' · ' + d[1], min: d[2], max: d[3], log: d[4], value: A[d[0]], def: d[6], fmt: d[5], keys: o.keys,
        onInput: function (v) { A[d[0]] = v; applyAdsr(); fire(o.onChange, get()); }, onCommit: function () { fire(o.onCommit, get()); } }); });
    function applyAdsr() { var keep = !st.custom && st.pts.length === 4; st.pts = adsrPts(A.a, A.d, A.s, A.r, keep ? st.pts[0].u : undefined, keep ? st.pts[1].u : undefined, keep ? st.pts[2].u : undefined); st.sus = 2; st.custom = false; win = winOf(); E.select(-1); syncSus(); dirty = true; }
    var r3 = mrow(el, 'Shapes');
    pills(r3, ['ADSR'].concat(Object.keys(ENV_PRESETS)), 'button', name + ' · envelope shapes', function (i, n) {
      if (n === 'ADSR') { st.custom = true; applyAdsr(); } else { /* the ADSR pill: the engine's default bends */ var r = ENV_PRESETS[n](); st.pts = r.pts; st.sus = r.sus; st.custom = true; win = winOf(); E.select(-1); syncSus(); dirty = true; }
      fire(o.onChange, get()); fire(o.onCommit, get()); });
    function get() { return { pts: st.pts.map(function (p) { return { t: p.t, v: p.v, u: p.u }; }), sus: susOf(st.pts, st.sus), custom: st.custom, adsr: { a: A.a, d: A.d, s: A.s, r: A.r } }; }
    function draw() {
      sliders.classList.toggle('idle', st.custom); cv.tabIndex = keysOn(o) ? 0 : -1;
      E.paint(kit(cv));
      var s = hint || E.describe() || (st.custom ? 'Edited by hand: the A/D/S/R sliders no longer describe it (ADSR rewrites the four points).' : 'Drag a point · drag a ring to bend · double-click: add, delete, reset a bend');
      if (status.textContent !== s) status.textContent = s;
      cv.setAttribute('aria-label', name + ' envelope: ' + st.pts.length + ' points, ' + (susOf(st.pts, st.sus) < 0 ? 'one-shot' : 'sustain at point ' + (susOf(st.pts, st.sus) + 1)));
      dirty = false;
    }
    syncSus();
    return { draw: draw, get: get, level: function (t, tOff, v0) { return envAt(st.pts, st.sus, t, tOff, v0); }, done: function (t, tOff) { var s = susOf(st.pts, st.sus), end = st.pts[st.pts.length - 1].t; return s < 0 ? t >= end : tOff !== null && tOff !== undefined && st.pts[s].t + (t - tOff) >= end; },
      dirty: function () { return dirty || E.dragging(); }, canvas: cv, editor: E };
  }

  /* ---------- CURVE PLOT: value over time for the segment law and its arrival marks (a static glass) ---------- */
  function CurvePlot(el, o) {
    o = o || {};
    el.classList.add('hd-cplot'); el.innerHTML = '';
    var cv = well(el, o.aspect || '2.2 / 1', o.label || 'Curve plot');
    function draw() {
      var K = kit(cv), W = K.cw, Ht = K.ch, x0 = o.yLabels === false ? 8 : 26, x1 = W - 10, y0 = 24, y1 = Ht - 18, T = typeof o.T === 'function' ? o.T() : (o.T || 1);
      var X = function (t) { return x0 + t / T * (x1 - x0); }, Y = function (v) { return y1 - v * (y1 - y0); };
      K.tube(); K.scan();
      [0, .5, 1].forEach(function (q) { K.line([[x0, Y(q)], [x1, Y(q)]], 'grid', .2, 1); if (o.yLabels !== false) K.text(String(q), 4, Y(q) + 3, 'grid'); });
      var st = niceStep(T, 4), n = 0, fx = o.xFmt || fSec;
      for (var t = 0; t <= T + 1e-9 && n < 30; t += st, n++) { K.line([[X(t), y0], [X(t), y1]], 'grid', .16, 1);
        var far = X(t) > x1 - 40; K.text(t === 0 ? '0' : fx(t), far ? x1 : X(t) + 2, Ht - 5, 'grid', far ? 'right' : 'left'); }
      var N = Math.max(80, Math.round(x1 - x0));
      (typeof o.lines === 'function' ? o.lines() : o.lines || []).forEach(function (L) {
        var pts = []; for (var k = 0; k <= N; k++) { var tt = k / N * T; pts.push([X(tt), Y(clamp(L.f(tt)))]); /* never above 1: the band over y0 carries the labels */ }
        if (L.glow) K.beam(pts, L.role || 'value', L.lw || 2); else K.line(pts, L.role || 'grid', L.a === undefined ? .9 : L.a, L.lw || 1.5, L.dash);
      });
      (typeof o.marks === 'function' ? o.marks() : o.marks || []).forEach(function (mk) { K.line([[X(mk.t), y0], [X(mk.t), y1]], 'marker', .8, 1, [2, 3]); K.text(mk.label, X(mk.t) + 2, y0 - 9, 'marker'); });
      (typeof o.rings === 'function' ? o.rings() : o.rings || []).forEach(function (rg) { K.ring(X(rg.t), Y(rg.v), 3.5, 'grid', 1.2); });
      if (o.caption) K.text(typeof o.caption === 'function' ? o.caption() : o.caption, x1, y0 - 9, 'grid', 'right');
    }
    if (typeof ResizeObserver === 'function') new ResizeObserver(function () { draw(); }).observe(cv);
    draw();
    return { draw: draw, canvas: cv };
  }

  /* ---------- SAMPLE generators for the previews (seeded; never the engine) ---------- */
  /* ORBITAL: reference/gravity-modulator.html's step() as the lab transcribed it (velocity Verlet, softened gravity, the
     3.0 speed clamp, bounce/wrap/open edges, centre-of-mass lock), advanced by counted steps of 1/480 s */
  var ORB_DT = 1 / 480, ORB_MAXV = 3, MAXB = 8, ORB_TRL = 180, ORB_EDGES = ['reflect', 'wrap', 'open'];
  function orbPreset(name) {
    var B = function (m, x, y, vx, vy, pinned) { return { m: m, x: x, y: y, vx: vx || 0, vy: vy || 0, pinned: !!pinned }; };
    if (name === 'sun') return { G: .025, eps: .06, bounds: 'reflect', com: false, bodies: [B(8, .5, .5, 0, 0, true), B(.4, .5, .32, .62, 0), B(.6, .5, .86, -.42, 0), B(.15, .12, .15, .35, .12)] };
    if (name === 'figure8') { var s = .28, v = Math.sqrt(.05 / s); return { G: .05, eps: .01, bounds: 'open', com: true, bodies: [
      B(1, .5 + .97000436 * s, .5 - .24308753 * s, .46620369 * v, .43236573 * v), B(1, .5 - .97000436 * s, .5 + .24308753 * s, .46620369 * v, .43236573 * v), B(1, .5, .5, -.93240737 * v, -.86473146 * v)] }; }
    if (name === 'chaos') return { G: .025, eps: .06, bounds: 'reflect', com: true, bodies: [B(1.2, .25, .25, .1, .05), B(.8, .75, .3, -.05, .15), B(2, .6, .72, 0, -.1), B(.5, .2, .7, .2, -.1)] };
    return { G: .025, eps: .06, bounds: 'reflect', com: true, bodies: [B(3, .38, .5, 0, -.28), B(3, .62, .5, 0, .28)] };
  }
  function Orbital(cfg) {
    cfg = cfg || {}; this.seed = cfg.seed === undefined ? 1024 : cfg.seed; this.P = { ts: 1, damp: 0, smooth: .005, kick: .3, kickBody: 0 };
    this.load(cfg.preset || 'binary');
  }
  Orbital.PRESETS = [['BINARY', 'binary'], ['SUN + PLANETS', 'sun'], ['FIGURE EIGHT', 'figure8'], ['FOUR-BODY CHAOS', 'chaos']];
  Orbital.prototype.load = function (name) {
    var p = orbPreset(name), O = this; this.preset = name; this.P.G = p.G; this.P.eps = p.eps; this.P.bounds = p.bounds; this.P.com = p.com;
    this.nodes = []; this.adds = 0; this.acc = 0; this.steps = 0; this.kicks = 0;
    this.kickRng = mulberry(((this.seed | 0) ^ 0x5EED) >>> 0);
    p.bodies.forEach(function (b) { O.addBody(b); });
    this.tr = []; this.trDec = 0; this.raw = []; this.sm = []; this.observe(); this.prime();
  };
  Orbital.prototype.addBody = function (b) { if (this.nodes.length >= MAXB) return false; this.nodes.push({ m: b.m, x: b.x, y: b.y, vx: b.vx, vy: b.vy, pinned: !!b.pinned, hx: b.x, hy: b.y, hvx: b.vx, hvy: b.vy, ax: 0, ay: 0 }); return true; };
  /* ADD BODY, seeded: draw #adds of (seed, add count), so an added body is reproducible patch state */
  Orbital.prototype.add = function () { var r = mulberry((this.seed + Math.imul(0x9E3779B9, ++this.adds)) >>> 0); var ok = this.addBody({ m: .8, x: .2 + r() * .6, y: .2 + r() * .6, vx: 0, vy: 0 }); this.observe(); this.prime(); return ok; };
  Orbital.prototype.accel = function () {
    var n = this.nodes, P = this.P, e2 = P.eps * P.eps, i, j;
    n.forEach(function (b) { b.ax = 0; b.ay = 0; });
    for (i = 0; i < n.length; i++) for (j = i + 1; j < n.length; j++) { var a = n[i], b = n[j], dx = b.x - a.x, dy = b.y - a.y;
      if (P.bounds === 'wrap') { if (dx > .5) dx -= 1; if (dx < -.5) dx += 1; if (dy > .5) dy -= 1; if (dy < -.5) dy += 1; }
      var r2 = dx * dx + dy * dy + e2, inv = P.G / (r2 * Math.sqrt(r2)); a.ax += dx * inv * b.m; a.ay += dy * inv * b.m; b.ax -= dx * inv * a.m; b.ay -= dy * inv * a.m; }
  };
  Orbital.prototype.step = function (dt) {
    var n = this.nodes, P = this.P; this.accel();
    n.forEach(function (b) { if (b.pinned) return; b.vx += .5 * b.ax * dt; b.vy += .5 * b.ay * dt; b.x += b.vx * dt; b.y += b.vy * dt; });
    this.accel();
    n.forEach(function (b) { if (b.pinned) return; b.vx += .5 * b.ax * dt; b.vy += .5 * b.ay * dt; if (P.damp > 0) { var k = Math.exp(-P.damp * 3 * dt); b.vx *= k; b.vy *= k; }
      if (b.vx * b.vx + b.vy * b.vy > ORB_MAXV * ORB_MAXV) { var s = Math.hypot(b.vx, b.vy); b.vx *= ORB_MAXV / s; b.vy *= ORB_MAXV / s; }
      if (P.bounds === 'reflect') { if (b.x < 0) { b.x = -b.x; b.vx = Math.abs(b.vx); } else if (b.x > 1) { b.x = 2 - b.x; b.vx = -Math.abs(b.vx); }
        if (b.y < 0) { b.y = -b.y; b.vy = Math.abs(b.vy); } else if (b.y > 1) { b.y = 2 - b.y; b.vy = -Math.abs(b.vy); } }
      else if (P.bounds === 'wrap') { b.x = ((b.x % 1) + 1) % 1; b.y = ((b.y % 1) + 1) % 1; } });
    if (P.com && P.bounds !== 'wrap') { var M = 0, cx = 0, cy = 0, vx = 0, vy = 0, pin = false;
      n.forEach(function (b) { if (b.pinned) { pin = true; return; } M += b.m; cx += b.x * b.m; cy += b.y * b.m; vx += b.vx * b.m; vy += b.vy * b.m; });
      if (M > 0 && !pin) { cx /= M; cy /= M; vx /= M; vy /= M; n.forEach(function (b) { b.x += .5 - cx; b.y += .5 - cy; b.vx -= vx; b.vy -= vy; }); } }
    this.steps++;
  };
  /* SPEC-ORBITAL §4's observables per body: x, y (up), speed (÷ the clamp), near (1 − softened nearest distance), angle */
  Orbital.prototype.observe = function () {
    var n = this.nodes, e2 = this.P.eps * this.P.eps, O = this;
    n.forEach(function (b, i) { var dmin = Infinity; n.forEach(function (c, j) { if (j !== i) dmin = Math.min(dmin, Math.hypot(c.x - b.x, c.y - b.y)); });
      O.raw[i] = [clamp(b.x), clamp(1 - b.y), Math.min(1, Math.hypot(b.vx, b.vy) / ORB_MAXV), n.length > 1 ? 1 - Math.min(1, Math.sqrt(dmin * dmin + e2)) : 0, (Math.atan2(.5 - b.y, b.x - .5) / TAU + 1) % 1]; });
  };
  Orbital.prototype.prime = function () { var O = this; this.sm = this.raw.map(function (r) { return r.slice(); }); };
  /* one frame: as many fixed steps as dt allows (never more than 64), then the one-pole smoothing (angle never smoothed) */
  Orbital.prototype.advance = function (dt) {
    this.acc += this.P.ts * dt; var k = 0, O = this;
    while (this.acc >= ORB_DT && k < 64) { this.step(ORB_DT); this.acc -= ORB_DT; k++;
      if (++this.trDec >= 4) { this.trDec = 0; this.tr.push(this.nodes.map(function (b) { return [b.x, b.y]; })); if (this.tr.length > ORB_TRL) this.tr.shift(); } }
    if (k >= 64) this.acc = 0;
    if (k) this.observe();
    var a = this.P.smooth > 0 ? 1 - Math.exp(-dt / this.P.smooth) : 1;
    this.raw.forEach(function (r, i) { if (!O.sm[i]) O.sm[i] = r.slice(); for (var q = 0; q < 4; q++) O.sm[i][q] += (r[q] - O.sm[i][q]) * a; O.sm[i][4] = r[4]; });
  };
  Orbital.prototype.obs = function (b, q) { return this.sm[b] ? this.sm[b][q] : 0; };
  Orbital.prototype.reset = function () { this.nodes.forEach(function (b) { b.x = b.hx; b.y = b.hy; b.vx = b.pinned ? 0 : b.hvx; b.vy = b.pinned ? 0 : b.hvy; }); this.tr = []; this.observe(); this.prime(); };
  Orbital.prototype.kick = function () { var b = this.nodes[this.P.kickBody | 0]; if (!b || b.pinned) return; var th = TAU * this.kickRng(); this.kicks++; b.vx += this.P.kick * Math.cos(th); b.vy -= this.P.kick * Math.sin(th); };
  Orbital.prototype.energy = function () { var K = 0, U = 0, n = this.nodes, P = this.P, i, j;
    n.forEach(function (b) { K += .5 * b.m * (b.vx * b.vx + b.vy * b.vy); });
    for (i = 0; i < n.length; i++) for (j = i + 1; j < n.length; j++) { var r = Math.hypot(n[j].x - n[i].x, n[j].y - n[i].y); U -= P.G * n[i].m * n[j].m / Math.sqrt(r * r + P.eps * P.eps); }
    return K + U; };
  /* a clocked sample-and-hold stand-in: the held value changes only when the clock wraps (rate in Hz) */
  function SampleHold() { this.ph = 0; this.held = 0; this.primed = false; }
  SampleHold.prototype.step = function (dt, hz, input) { this.ph += hz * dt; var f = !this.primed || this.ph >= 1; if (f) { this.ph -= Math.floor(this.ph); this.held = input; this.primed = true; } return f; };
  SampleHold.prototype.reset = function () { this.ph = 0; this.primed = false; };

  /* MIDI TRACKERS: the lab's NoteTracker rules, global scope. GATE is legato (never dips while any key is held);
     TRIGGER is an event (EVERY: each note-on; FIRST: the note-on that opens the gate). */
  var PHRASE = [[0, .4, 60, .9, .5], [.3, .4, 64, .6, .3], [.6, .3, 67, .75, .7], [1.1, .15, 72, 1, .9], [1.4, .15, 71, .45, .2],
    [1.8, .5, 60, .8, .4], [1.81, .5, 64, .7, .5], [1.82, .5, 67, .6, .6], [2.6, .3, 57, .5, .5]];
  function Tracker(cfg) { this.cfg = cfg || { mode: 0, orderN: 4, sinceT: 2 }; this.held = []; this.note = 60; this.vel = 0; this.rel = 0; this.k = 0; this.since = -1; this.fired = 0; this.trig = 0; }
  Tracker.prototype.noteOn = function (n, v) { var open = this.held.length > 0; this.held = this.held.filter(function (h) { return h[0] !== n; }); if (this.held.length < 16) this.held.push([n, v]);
    this.k = open ? this.k + 1 : 1; this.note = n; this.vel = v; if (this.cfg.mode === 0 || !open) { this.trig++; this.fired++; this.since = 0; } };
  Tracker.prototype.noteOff = function (n, r) { var l = this.held.length; this.held = this.held.filter(function (h) { return h[0] !== n; }); if (this.held.length === l) return; this.rel = r;
    if (this.held.length) { var t = this.held[this.held.length - 1]; this.note = t[0]; this.vel = t[1]; } };
  Tracker.prototype.gate = function () { return this.held.length ? 1 : 0; };
  Tracker.prototype.values = function () { var N = Math.max(2, this.cfg.orderN | 0);
    return { gate: this.gate(), note: clamp((this.note - 60) / 48, -1, 1), vel: this.vel, rel: this.rel, held: Math.min(1, this.held.length / 8),
      order: this.k > 0 ? ((this.k - 1) % N) / (N - 1) : 0, since: this.since < 0 ? 1 : Math.min(1, this.since / Math.max(1e-3, this.cfg.sinceT)) }; };
  /* THE BENCH: the phrase on a 3.4 s loop through a tracker, with the trigger's stateful actions: SAMPLE ON TRIGGER
     (a seeded draw per trigger, or the sampled source) and MOD ENV (restarted from the level it has reached) */
  function ModBench(cfg) {
    cfg = cfg || {}; this.cfg = { mode: 0, orderN: 4, sinceT: 2 }; this.trk = new Tracker(this.cfg); this.t = 0; this.loop = cfg.loop || 3.4; this.ev = []; this.evp = 0;
    this.seed = cfg.seed === undefined ? 4242 : cfg.seed; this.rng = mulberry(this.seed); this.sh = 0; this.shN = 0; this.act = cfg.act || [1, 1, 0, 0, 0, 0, 0];
    this.shIn = 'rand'; this.env = adsrPts(.008, .22, .45, .35); this.envT = -1; this.envOff = null; this.envV0 = 0; this.envLvl = 0; this.counts = [0, 0, 0, 0, 0, 0, 0]; this.schedule(0);
  }
  ModBench.prototype.schedule = function (t0) { var e = []; PHRASE.forEach(function (p) { e.push([t0 + p[0], 1, p[2], p[3]]); e.push([t0 + p[0] + p[1], 0, p[2], p[4]]); });
    this.ev = e.sort(function (a, b) { return a[0] - b[0] || a[1] - b[1]; }); this.evp = 0; this.t0 = t0; };
  ModBench.prototype.reseed = function (s) { this.seed = s | 0; this.rng = mulberry(this.seed); this.shN = 0; };
  ModBench.prototype.advance = function (dt) {
    var tr = this.trk, gate0 = tr.gate(), fired = 0, end = this.t + dt;
    while (this.evp < this.ev.length && this.ev[this.evp][0] <= end) { var e = this.ev[this.evp++], f0 = tr.trig;
      if (e[1]) tr.noteOn(e[2], e[3]); else tr.noteOff(e[2], e[3]);
      if (tr.trig > f0) { fired++; this.onTrigger(); }
      if (!tr.gate() && this.envT >= 0 && this.envOff === null) this.envOff = this.envT; }
    if (this.evp >= this.ev.length && end >= this.t0 + this.loop) this.schedule(this.t0 + this.loop);
    this.t = end; if (tr.since >= 0) tr.since += dt;
    if (this.envT >= 0) { this.envT += dt; this.envLvl = envAt(this.env, 2, this.envT, this.envOff, this.envV0); }
    return fired;
  };
  ModBench.prototype.onTrigger = function () {
    for (var i = 0; i < this.act.length; i++) if (this.act[i]) this.counts[i]++;
    /* input: the sampled source, read AT the trigger (a tracker key such as 'vel'); 'rand' takes the seeded draw */
    if (this.act[0]) { this.shN++; this.sh = this.shIn === 'rand' ? 2 * this.rng() - 1 : this.trk.values()[this.shIn]; }
    if (this.act[1]) { this.envV0 = this.envLvl; this.envT = 0; this.envOff = null; }
  };
  H.Synth.Orbital = Orbital; H.Synth.orbitalPreset = orbPreset; H.Synth.SampleHold = SampleHold; H.Synth.Tracker = Tracker; H.Synth.ModBench = ModBench; H.Synth.PHRASE = PHRASE;

  /* ---------- ORBITAL CARD: the field, its controls, and its mappings (S&H is a mode on a mapping, ADR-266 r6) ---------- */
  var OBS = ['x', 'y', 'speed', 'near', 'angle'];
  function OrbitalCard(el, o) {
    o = o || {};
    var F = o.field, maps = o.maps || [], name = o.name || 'ORBITAL 1', hist = new Lanes(maps.map(function () { return 0; }).concat(maps.map(function () { return 0; }), maps.map(function () { return 0; })), 200), dirty = true;
    el.classList.add('hd-modcard', 'hd-orb'); el.innerHTML = '';
    var hd = tag('div'); el.appendChild(hd);
    var head = ModCardHeader(hd, { name: name, kind: 'lfo', kindLabel: 'MOD', scope: 'GLOBAL', retrig: !!o.retrig, retrigHint: 'a note-on resets the field to its initial conditions',
      onRetrig: function (v) { fire(o.onRetrig, v); } });
    var cv = well(el, o.aspect || '2.3 / 1', name + ': bodies, trails, and each mapping\'s lane');
    var acts = mrow(el, 'Field');
    var pp = pills(acts, Orbital.PRESETS.map(function (p) { return p[0]; }), 'radio', name + ' · preset', function (i) { F.load(Orbital.PRESETS[i][1]); after(); });
    var act2 = mrow(el, 'Act');
    pills(act2, ['ADD BODY', 'RESET', 'KICK'], 'button', name + ' · actions', function (i) { if (i === 0) { if (!F.add()) say('At most 8 bodies.'); } else if (i === 1) F.reset(); else F.kick(); after(); });
    var msg = tag('span', 'hd-hint or-msg'); act2.appendChild(msg);
    function say(s) { msg.textContent = s; }
    var grid = tag('div', 'or-grid'); el.appendChild(grid);
    function sl(label, k, min, max, log, fmt, def) { var d = tag('div'); grid.appendChild(d);
      return slider(d, { keys: o.keys, onCommit: function () { fire(o.onCommit, k); }, name: label, path: name + ' · ' + label, min: min, max: max, log: log, value: F.P[k], def: def, fmt: fmt, onInput: function (v) { F.P[k] = v; dirty = true; fire(o.onChange, k, v); } }); }
    sl('Gravity G', 'G', .001, 1, true, function (x) { return x.toFixed(4); }, .025);
    sl('Soften ε', 'eps', .005, .5, true, function (x) { return x.toFixed(3); }, .06);
    sl('Time ×', 'ts', 0, 4, false, function (x) { return x.toFixed(2) + '×'; }, 1);
    sl('Drag', 'damp', 0, 1, false, f2, 0);
    sl('Smooth', 'smooth', 0, .05, false, function (x) { return (x * 1000).toFixed(1) + ' ms'; }, .005);
    sl('Kick', 'kick', 0, 1, false, function (x) { return x.toFixed(2) + ' u/s'; }, .3);
    var opts = tag('div', 'or-opts'); el.appendChild(opts);
    var er = mrow(opts, 'Edges'), ep = pills(er, ['BOUNCE', 'WRAP', 'OPEN'], 'radio', name + ' · edges', function (i) { F.P.bounds = ORB_EDGES[i]; after(); });
    var cr = mrow(opts, 'COM lock'), cb = tag('button', 'hd-toggle hd-focus'); cb.type = 'button'; cb.setAttribute('role', 'switch'); cb.setAttribute('aria-label', name + ' · centre-of-mass lock'); cr.appendChild(cb);
    cb.addEventListener('click', function () { F.P.com = !F.P.com; after(); });
    var kr = mrow(opts, 'Kick body'), kslot = tag('span', 'or-kick'); kr.appendChild(kslot);
    /* one option per body: the field spawns bodies, so the range follows F.nodes.length (remounted when it changes) */
    var kickStep = dynStepper(kslot, function (d) {
      return H.Stepper(d, { name: 'Kick body', path: name + ' · kick body', options: F.nodes.map(function (q, i) { return 'body ' + (i + 1); }), value: 0, def: 0, keys: function () { return keysOn(o); },
        onInput: function (i) { F.P.kickBody = i; after(); } }); });
    /* the mappings: each a source (body n observable or a field measure) with its mode; S&H holds the source on the
       mapping's OWN clock, its own rate and sync (ADR-266 r6) */
    var mh = tag('div', 'or-maps'); el.appendChild(mh);
    mh.appendChild(tag('div', 'hd-cluster or-mt', 'Mappings'));
    var rows = maps.map(function (m, i) {
      var box = tag('div', 'or-map'); mh.appendChild(box);
      var r = tag('div', 'hd-mrow'); box.appendChild(r);
      r.appendChild(tag('span', 'or-mk', 'AB'[i] || String(i + 1)));
      r.appendChild(tag('span', 'or-mn', 'body ' + (m.b + 1) + ' ' + OBS[m.q] + ' → ' + m.dest));
      var mp = pills(r, ['CONT', 'S&H'], 'radio', name + ' · mapping ' + ('AB'[i] || i + 1) + ' mode', function (k) { m.mode = k ? 'sh' : 'cont'; sync(); fire(o.onChange, 'map', m); fire(o.onCommit, 'map', m); });
      var rsd = tag('div', 'or-rs'); box.appendChild(rsd);
      var rs = RateSync(rsd, { name: 'S&H rate', path: name + ' · mapping ' + ('AB'[i] || i + 1) + ' · S&H rate', sync: m.sync, hz: m.hz || 4, beats: m.beats || .5, bpm: o.bpm, min: .05, max: 40, defHz: 4, defBeats: .5, keys: o.keys,
        onChange: function (s) { m.sync = s.sync; m.hz = s.hz; m.beats = s.beats; fire(o.onChange, 'map', m); }, onCommit: function () { fire(o.onCommit, 'map', m); } });
      m.rate = rs.rate;
      return { m: m, mp: mp, rsd: rsd };
    });
    function sync() {
      pp.set(Orbital.PRESETS.map(function (p) { return p[1]; }).indexOf(F.preset)); ep.set(ORB_EDGES.indexOf(F.P.bounds)); cb.setAttribute('aria-checked', String(!!F.P.com));
      kickStep(F.nodes.length, F.P.kickBody | 0);
      rows.forEach(function (r) { r.mp.set(r.m.mode === 'sh' ? 1 : 0); r.rsd.classList.toggle('idle', r.m.mode !== 'sh'); });
      dirty = true;
    }
    function after() { if ((F.P.kickBody | 0) >= F.nodes.length) F.P.kickBody = 0; sync(); fire(o.onChange, 'field'); fire(o.onCommit, 'field'); }
    /* read() is the consumer's: each mapping's source value, its output (held under S&H) and whether its clock fired */
    function log() { var src = [], out = [], tk = []; maps.forEach(function (m, i) { var r = o.read ? o.read(i) : { src: 0, out: 0, tick: 0 }; src.push(r.src); out.push(r.out); tk.push(r.tick ? 1 : 0); }); hist.push(src.concat(out, tk)); }
    function draw() {
      var K = kit(cv), W = K.cw, Ht = K.ch, nb = F.nodes.length, side = Math.min(Ht - 18, W * .5), fx = 14, fy = 6;   // room below for the x projection ticks
      var X = function (u) { return fx + u * side; }, Y = function (u) { return fy + u * side; }, read = {};
      maps.forEach(function (m) { read[m.b] = 1; });
      K.tube(); K.scan();
      for (var k = 0; k <= 8; k++) { var q = k / 8; K.line([[X(q), Y(0)], [X(q), Y(1)]], 'grid', k === 4 ? .45 : .14, 1); K.line([[X(0), Y(q)], [X(1), Y(q)]], 'grid', k === 4 ? .45 : .14, 1); }
      var g = K.g; g.save(); g.beginPath(); g.rect(0, 0, (fx + side + 4) * K.V, Ht * K.V); g.clip();
      for (var i = 0; i < nb; i++) { var seg = [], px = NaN, py = NaN;
        F.tr.forEach(function (fr) { var p = fr[i]; if (!p) return; if (seg.length && (Math.abs(p[0] - px) > .5 || Math.abs(p[1] - py) > .5)) { K.line(seg, 'physics', .35, 1); seg = []; } seg.push([X(p[0]), Y(p[1])]); px = p[0]; py = p[1]; });
        if (seg.length > 1) K.line(seg, 'physics', .35, 1); }
      F.nodes.forEach(function (b, i) { var role = read[i] ? 'meter' : 'physics', r = 3 + Math.sqrt(b.m) * 3, x = X(b.x), y = Y(b.y);
        if (b.pinned) K.ring(x, y, r, role, 2); else { K.dot(x, y, r + 4, role, .25); K.dot(x, y, r, role); }
        g.fillStyle = K.c(role); g.fillRect((X(F.obs(i, 0)) - 1) * K.V, (Y(1) + 2) * K.V, 2 * K.V, 6 * K.V); g.fillRect((X(0) - 8) * K.V, (Y(1 - F.obs(i, 1)) - 1) * K.V, 6 * K.V, 2 * K.V); });
      /* body numbers LAST, each on a ground chip, so no body, trail or bloom is drawn over one */
      var placed = [];   // and never on each other: each takes the first free spot of four round its body
      F.nodes.forEach(function (b, i) { var r = 3 + Math.sqrt(b.m) * 3, x = X(b.x), y = Y(b.y), w = 6 * String(i + 1).length + 4, spot = null;
        [[x + r + 2, y - r], [x + r + 2, y + r + 9], [x - r - w, y - r], [x - r - w, y + r + 9]].some(function (q) {
          var hitIt = placed.some(function (o) { return q[0] < o[0] + o[2] + 3 && o[0] < q[0] + w + 3 && q[1] - 9 < o[1] + 3 && o[1] - 9 < q[1] + 3; }); if (!hitIt) spot = q; return !hitIt; });
        spot = spot || [x + r + 2, y - r]; placed.push([spot[0], spot[1], w]); K.label(String(i + 1), spot[0], spot[1], 'grid'); });
      g.restore();
      var x0 = fx + side + 14, x1 = W - 8, nm = maps.length, bh = (Ht - 20) / Math.max(1, nm), n = hist.len;   // the last 16 px carry the field stats
      var Xh = function (kk) { return x1 - (n - 1 - kk) / (hist.n - 1) * (x1 - x0); };
      maps.forEach(function (m, i) {
        var top = 4 + i * bh, t2 = top + LANE_TOP, b2 = top + bh - 4, sh = m.mode === 'sh', kk;
        /* the source as rendered, thin; the mapping's output over it (stepped, with its clock's ticks, under S&H) */
        paintLanes(K, { len: n, n: hist.n, at: function (r, k2) { return hist.at(i, k2); } },
          [{ name: ('AB'[i] || i + 1) + ' · body ' + (m.b + 1) + ' ' + OBS[m.q] + ' · ' + (sh ? 'S&H ' + (m.sync ? divName(m.beats) : (m.hz || 4).toFixed(1) + ' Hz') : 'CONT'), kind: 'level', role: 'physics' }], x0, x1, top, top + bh);
        if (n < 2) return;
        var pts = []; for (kk = 0; kk < n; kk++) { var yy = b2 - hist.at(nm + i, kk) * (b2 - t2); if (sh && kk) pts.push([Xh(kk), pts[pts.length - 1][1]]); pts.push([Xh(kk), yy]); }
        K.line(pts, 'meter', 1, 1.6);
        if (sh) for (kk = 0; kk < n; kk++) if (hist.at(2 * nm + i, kk) > 0) K.line([[Xh(kk), b2], [Xh(kk), b2 - 5]], 'marker', 1, 1.2);
      });
      K.text('t ' + (F.steps * ORB_DT).toFixed(1) + ' s · E ' + F.energy().toFixed(4) + ' · ' + nb + ' bodies', x1, Ht - 3, 'grid', 'right', 8.5);
      cv.setAttribute('aria-label', name + ': ' + nb + ' bodies, preset ' + F.preset + '; lanes: ' + maps.map(function (m, i) { return ('AB'[i] || i + 1) + ' body ' + (m.b + 1) + ' ' + OBS[m.q] + ' ' + (m.mode === 'sh' ? 'sample and hold' : 'continuous'); }).join(', '));
      dirty = false;
    }
    sync();
    return { draw: draw, log: log, header: head, dirty: function () { return dirty; }, refresh: sync, canvas: cv };
  }

  /* ---------- MIDI TRACKERS: the gate, the trigger mode and six trackers, each a source (the lab's rules) ---------- */
  var TRK = [['GATE', '0 / 1', 'any key held; legato, never dips', 'gate'], ['TRIGGER', 'event', 'EVERY: each note-on · FIRST: the one that opens the gate', null],
    ['NOTE', '±1', '(n − 60) ÷ 48, last-note priority, holds', 'note'], ['VELOCITY', '0..1', 'the NOTE key\'s velocity', 'vel'], ['REL VEL', '0..1', 'the latest note-off\'s velocity, held', 'rel'],
    ['HELD', '0..1', 'keys held ÷ 8', 'held'], ['ORDER', '0..1', 'k-th note since the gate opened, wraps at N', 'order'], ['SINCE', '0..1', 'seconds since the TRIGGER ÷ T', 'since']];
  function MidiTrackers(el, o) {
    o = o || {}; var cfg = o.cfg || { mode: 0, orderN: 4, sinceT: 2 }, name = o.name || 'MIDI';
    el.classList.add('hd-modcard', 'hd-trk'); el.innerHTML = '';
    var hd = tag('div'); el.appendChild(hd);
    ModCardHeader(hd, { name: name, kind: 'engine', kindLabel: 'TRACKERS', scope: 'GLOBAL', retrig: null });
    var r1 = mrow(el, 'Trigger'), tp = pills(r1, ['EVERY NOTE-ON', 'FIRST · LEGATO'], 'radio', name + ' · trigger mode', function (i) { cfg.mode = i; sync(); fire(o.onChange, cfg); fire(o.onCommit, cfg); });
    var r2 = mrow(el, 'Order N'), nd = tag('div'); r2.appendChild(nd);
    var ns = H.Stepper(nd, { name: 'Order N', path: name + ' · order wraps at N', min: 2, max: 8, value: cfg.orderN, def: 4, keys: function () { return keysOn(o); },
      onInput: function (n) { cfg.orderN = n; fire(o.onChange, cfg); fire(o.onCommit, cfg); } });
    var sd = tag('div'); el.appendChild(sd);
    slider(sd, { keys: o.keys, onCommit: function () { fire(o.onCommit, cfg); }, name: 'Since T', path: name + ' · since full scale', min: .1, max: 8, log: true, value: cfg.sinceT, def: 2, fmt: fSec, onInput: function (v) { cfg.sinceT = v; fire(o.onChange, cfg); } });
    var tb = tag('div', 'trk-table'); tb.setAttribute('role', 'table'); tb.setAttribute('aria-label', name + ' trackers'); el.appendChild(tb);
    var hr = tag('div', 'trk-row trk-h'); hr.setAttribute('role', 'row'); ['Source', 'Range', 'Rule', 'Now'].forEach(function (h) { var c = tag('span', null, h); c.setAttribute('role', 'columnheader'); hr.appendChild(c); }); tb.appendChild(hr);
    var nows = TRK.map(function (t) { var r = tag('div', 'trk-row'); r.setAttribute('role', 'row');
      [t[0], t[1], t[2]].forEach(function (s, j) { var c = tag('span', j === 0 ? 'trk-n' : j === 2 ? 'trk-rule' : 'trk-rg', s); c.setAttribute('role', j === 0 ? 'rowheader' : 'cell'); if (j === 2) c.title = s; r.appendChild(c); });
      var now = tag('span', 'hd-value trk-now', '—'); now.setAttribute('role', 'cell'); r.appendChild(now); tb.appendChild(r); return [now, t[3]]; });
    function sync() { tp.set(cfg.mode); ns.set(cfg.orderN); }
    sync();
    /* read() is the consumer's: { values {gate, note, ...}, fired (triggers so far) } */
    function draw() { var r = o.read ? o.read() : null; if (!r) return;
      nows.forEach(function (n) { var s = n[1] ? (n[1] === 'note' ? sgn2(r.values[n[1]]) : r.values[n[1]].toFixed(3)) : r.fired + ' fired'; if (n[0].textContent !== s) n[0].textContent = s; }); }
    return { draw: draw, cfg: cfg, refresh: sync };
  }
  /* ---------- GATE LANES: the trackers as lanes, the gate a level, the trigger ticks only ---------- */
  function GateLanes(el, o) {
    o = o || {};
    var rows = o.rows || TRK.map(function (t) { return { name: t[0], kind: t[3] ? 'level' : 'event', bip: t[3] === 'note', key: t[3] }; });
    var L = new Lanes(rows, o.points || 240), dirty = true;
    el.classList.add('hd-lanes'); el.innerHTML = '';
    var cv = well(el, o.aspect || '2 / 1', (o.label || 'MIDI tracker lanes') + ': ' + rows.map(function (r) { return r.name; }).join(', '));
    cv.parentElement.style.minHeight = (rows.length * (LANE_TOP + 11) + 4) + 'px';   // each lane keeps its label band AND a readable plot at any width
    /* push() logs one frame of what was rendered: values by row key, events as a count fired this frame */
    function push(vals, fired) { L.push(rows.map(function (r) { return r.kind === 'event' ? (fired || 0) : vals[r.key]; })); dirty = true; }
    function draw() { var K = kit(cv); K.tube(); K.scan(); paintLanes(K, L, rows, 6, K.cw - 6, 2, K.ch - 2); dirty = false; }
    return { push: push, draw: draw, rows: rows, dirty: function () { return dirty; }, canvas: cv };
  }

  /* ---------- EVENT LANES: events are not levels; the trigger's actions and the lanes that show them ---------- */
  var ACTIONS = ['SAMPLE ON TRIGGER', 'MOD ENV RESTART', 'LFO RESTART', 'KURO RESET', 'ORBITAL KICK', 'ORBITAL RESET', 'HOSTED: RANDOMIZE UNLOCKED'];
  function EventLanes(el, o) {
    o = o || {}; var act = o.act || [1, 1, 0, 0, 0, 0, 0], name = o.name || 'Trigger';
    el.classList.add('hd-evl'); el.innerHTML = '';
    var r1 = mrow(el, 'Fires'), g = tag('div', 'hd-pills'); g.setAttribute('role', 'group'); g.setAttribute('aria-label', name + ' · actions it fires'); r1.appendChild(g);
    var bs = ACTIONS.map(function (a, i) { var b = tag('button', 'hd-pill hd-focus ev-act', a); b.type = 'button';
      if (i === 6) b.title = 'PENDING: whether a trigger may cross the module boundary is not ruled (lab round 2, question 1). Posts nothing here.';
      b.addEventListener('click', function () { act[i] = act[i] ? 0 : 1; sync(); fire(o.onChange, act); fire(o.onCommit, act); }); g.appendChild(b); return b; });
    var r2 = mrow(el, 'Sample in'), sel = tag('select', 'hd-select hd-focus ev-in'); sel.setAttribute('aria-label', name + ' · what SAMPLE ON TRIGGER samples');
    (o.inputs || [['rand', 'RANDOM (seeded)']]).forEach(function (p) { var op = tag('option', null, p[1]); op.value = p[0]; sel.appendChild(op); });
    sel.value = o.shIn || 'rand'; r2.appendChild(sel);
    sel.addEventListener('change', function () { fire(o.onInput, sel.value); fire(o.onCommit, act); });
    r2.appendChild(tag('span', 'hd-label ev-sl', 'Seed'));
    var sd = tag('input', 'hd-mnum ev-seed hd-focus'); sd.type = 'number'; sd.value = String(o.seed === undefined ? 4242 : o.seed); sd.setAttribute('aria-label', name + ' · SAMPLE ON TRIGGER seed'); r2.appendChild(sd);
    sd.addEventListener('change', function () { fire(o.onSeed, (parseInt(sd.value, 10) || 0) | 0); });
    var rows = [{ name: 'GATE: a level, every sample', kind: 'level', key: 'gate' }, { name: 'TRIGGER: an event, ticks only', kind: 'event' },
      { name: 'SAMPLE ON TRIGGER: event → level, holds', kind: 'step', bip: true, key: 'sh' }, { name: 'MOD ENV: restarted by the trigger, released by the gate', kind: 'level', key: 'env' }];
    var ld = tag('div'); el.appendChild(ld); var lanes = GateLanes(ld, { rows: rows });
    lanes.canvas.setAttribute('aria-label', name + ': gate, trigger, SAMPLE ON TRIGGER and MOD ENV lanes');
    var read = tag('div', 'hd-hint ev-read'); read.setAttribute('aria-live', 'off'); el.appendChild(read);
    function sync() { bs.forEach(function (b, i) { b.setAttribute('aria-pressed', String(!!act[i])); }); }
    sync();
    return { push: lanes.push, draw: function (info) { lanes.draw(); if (info) { var s = info; if (read.textContent !== s) read.textContent = s; } }, act: act, dirty: lanes.dirty, canvas: lanes.canvas };
  }

  /* ---------- SMOOTHING LANES: one seeded stream, five laws; click a lane to pick it ---------- */
  function SmoothingLanes(el, o) {
    o = o || {}; var mode = o.mode === undefined ? 1 : o.mode, name = o.name || 'RANDOM';
    el.classList.add('hd-smooth'); el.innerHTML = '';
    var box = tag('div', 'sm-box'); el.appendChild(box);
    var cv = well(box, o.aspect || '1.7 / 1', name + ' smoothing lanes'); var wl = cv.parentElement;
    var grp = tag('div', 'sm-picks'); grp.setAttribute('role', 'radiogroup'); grp.setAttribute('aria-label', name + ' · smoothing'); wl.appendChild(grp);
    var bs = RAND_MODES.map(function (m, i) { var b = tag('button', 'sm-pick hd-focus'); b.type = 'button'; b.setAttribute('role', 'radio');
      b.setAttribute('aria-label', m + (i === 0 ? ' (the S&H law)' : '') + ' smoothing'); b.style.top = (i * 20) + '%';
      b.addEventListener('click', function () { mode = i; sync(); fire(o.onPick, i); fire(o.onCommit, i); }); grp.appendChild(b); return b; });
    function sync() { bs.forEach(function (b, i) { b.setAttribute('aria-checked', String(i === mode)); b.tabIndex = i === mode ? 0 : -1; }); }
    function draw() {
      var K = kit(cv), W = K.cw, Ht = K.ch, bh = Ht / 5, sd = lfoSeedOf(typeof o.seed === 'function' ? o.seed() : (o.seed === undefined ? 1234 : o.seed));
      var p = typeof o.position === 'function' ? o.position() : 0, kNow = Math.floor(p), k0 = kNow - ((kNow % PAGE) + PAGE) % PAGE, cs = typeof o.cycleSec === 'function' ? o.cycleSec() : 1, sl = (typeof o.slewMs === 'function' ? o.slewMs() : (o.slewMs || 80)) / 1000;
      var x0 = 6, x1 = W - 6, X = function (q) { return x0 + q / PAGE * (x1 - x0); }, N = Math.max(PAGE * 24, Math.round(x1 - x0));
      K.tube(); K.scan();
      var stp = randPage(0, sd, k0, PAGE, N, cs, sl);
      for (var m = 0; m < 5; m++) {
        var top = m * bh + LANE_TOP, bot = (m + 1) * bh - 4, Y = function (q) { return bot - (q + 1) / 2 * (bot - top); }, on = m === mode;
        for (var j = 0; j <= PAGE; j++) K.line([[X(j), top], [X(j), bot]], 'grid', .16, 1);
        K.line([[x0, Y(0)], [x1, Y(0)]], 'grid', .18, 1);
        var arr = m === 0 ? stp : randPage(m, sd, k0, PAGE, N, cs, sl), pts = []; for (var i = 0; i <= N; i++) pts.push([X(i / N * PAGE), Y(arr[i])]);
        if (m === 4) { var sp = []; for (i = 0; i <= N; i++) sp.push([X(i / N * PAGE), Y(stp[i])]); K.line(sp, 'value', .3, 1, [3, 3]); }
        if (on) K.beam(pts, 'value', 1.8); else K.line(pts, 'value', .42, 1.1);
        for (j = 0; j < PAGE; j++) K.dot(X(j), Y(drawV(sd, k0 + j)), 2.4, 'value', on ? 1 : .5);
        K.text(RAND_MODES[m] + (m === 0 ? ' (= S&H)' : m === 4 ? ' ' + Math.round(sl * 1000) + ' ms' : '') + (on ? '  ◀ selected' : ''), x0 + 2, m * bh + LANE_LABEL, on ? 'physics' : 'grid', 'left', 8.5);
        if (m < 4) K.line([[0, (m + 1) * bh], [W, (m + 1) * bh]], 'grid', .3, 1);
        if (on) { var xp = X(p - k0); K.dot(xp, Y(randValue(m, sd, p, cs, sl, PAGE)), 3.8, 'meter'); }
      }
      var xq = X(p - k0); for (m = 0; m < 5; m++) K.line([[xq, m * bh + LANE_TOP], [xq, (m + 1) * bh - 2]], 'meter', .7, 1); /* under the bands, never through a label */
    }
    sync();
    return { draw: draw, set: function (m) { mode = m; sync(); }, get: function () { return mode; }, canvas: cv };
  }

  /* ---------- S&H CLOCK CARD: what clocks an S&H (modulator-lab's global clock, read under ADR-266 r6) ---------- */
  var CLOCKS = ['CYCLE', 'GLOBAL', 'KURO'];
  function ShClockCard(el, o) {
    o = o || {}; var st = { clock: o.clock || 0, bind: o.bind || 0 }, name = o.name || 'S&H 1', hist = new Lanes([0, 0, 0, 0], 200), dirty = true;
    el.classList.add('hd-modcard', 'hd-shc'); el.innerHTML = '';
    var hd = tag('div'); el.appendChild(hd);
    var head = ModCardHeader(hd, { name: name, kind: 'lfo', kindLabel: 'LFO · S&H', scope: 'GLOBAL', retrig: !!o.retrig, retrigHint: 'a note-on restarts the clock and takes a new draw', onRetrig: function (v) { fire(o.onRetrig, v); } });
    var scr = tag('div', 'shc-scr'); el.appendChild(scr);
    var cv = well(scr, '2.2 / 1', name + ': held values of the attached modulators and their sample instants');
    var rot = tag('div', 'shc-rot'); scr.appendChild(rot); var rcv = well(rot, '1 / 1', 'The rotor that clocks KURO (LfoRotor)'); var rcap = tag('div', 'hd-hint shc-rcap', 'KURO\'s rotor'); rot.appendChild(rcap);
    var r1 = mrow(el, 'Clock'), cp = pills(r1, CLOCKS, 'radio', name + ' · clock', function (i) { st.clock = i; sync(); fire(o.onChange, get()); fire(o.onCommit, get()); });
    var rsd = tag('div'); el.appendChild(rsd);
    var rs = RateSync(rsd, { name: 'Clock rate', path: name + ' · clock rate', sync: o.sync, hz: o.hz || 2, beats: o.beats || .5, bpm: o.bpm, min: .1, max: 40, defHz: 2, defBeats: .5, keys: o.keys,
      onChange: function () { dirty = true; fire(o.onChange, get()); }, onCommit: function () { fire(o.onCommit, get()); } });
    var r2 = mrow(el, 'Bind to'), bd = tag('div'); r2.appendChild(bd);
    var bsw = H.Stepper(bd, { name: 'Bind to', path: name + ' · bound rotor voice (KURO)', options: ['K1', 'K2', 'K3', 'K4'], value: st.bind, def: 0, keys: function () { return keysOn(o); },
      spoken: function (i) { return 'rotor voice ' + (i + 1); }, onInput: function (i) { st.bind = i; sync(); fire(o.onChange, get()); fire(o.onCommit, get()); } });
    var rd = tag('div', 'shc-read'); el.appendChild(rd);
    var last = tag('span', 'hd-value'), att = tag('span', 'hd-value'), why = tag('div', 'hd-hint shc-why');
    var l1 = tag('span', 'hd-label', 'Last sampled'), l2 = tag('span', 'hd-label', 'Attached');
    rd.appendChild(l1); rd.appendChild(last); rd.appendChild(l2); rd.appendChild(att); el.appendChild(why);
    var WHY = ['CYCLE: the S&H shape samples at its own LFO\'s cycle start (ADR-266 r6): each attached S&H has its own clock and its own draw.',
      'GLOBAL: one clock and one draw, broadcast: every attached S&H holds the identical value.', 'KURO: each S&H samples when its bound rotor voice wraps, from its own stream: coupling locks when, never what.'];
    function sync() { cp.set(st.clock); rsd.classList.toggle('idle', st.clock === 2); r2.classList.toggle('idle', st.clock !== 2); rot.classList.toggle('idle', st.clock !== 2);
      bsw.set(st.bind); why.textContent = WHY[st.clock]; dirty = true; }
    function get() { var r = rs.get(); return { clock: CLOCKS[st.clock], bind: st.bind, sync: r.sync, hz: r.hz, beats: r.beats, rate: r.rate }; }
    /* push(): one frame of what was rendered: [heldA, heldB, firedA, firedB]; info { last, attached, rotor {phase, out} } */
    function push(v) { hist.push(v); dirty = true; }
    function draw(info) {
      var K = kit(cv), W = K.cw, Ht = K.ch; K.tube(); K.scan();
      paintLanes(K, { len: hist.len, n: hist.n, at: function (r, k) { return hist.at(0, k); } }, [{ name: 'A held', kind: 'step', bip: true, role: 'value' }], 6, W - 6, 2, Ht / 2 - 4);
      paintLanes(K, { len: hist.len, n: hist.n, at: function (r, k) { return hist.at(1, k); } }, [{ name: 'B held', kind: 'step', bip: true, role: 'physics' }], 6, W - 6, Ht / 2, Ht - 14);
      var n = hist.len; for (var k = 0; k < n; k++) { var x = W - 6 - (n - 1 - k) / (hist.n - 1) * (W - 12);
        if (hist.at(2, k) > 0) K.line([[x, Ht - 12], [x, Ht - 7]], 'marker', 1, 1.2); if (hist.at(3, k) > 0) K.line([[x, Ht - 6], [x, Ht - 1]], 'meter', 1, 1.2); }
      if (info) { if (info.rotor) H.Viz.rotor(rcv, info.rotor); var ls = sgn2(info.last); if (last.textContent !== ls) last.textContent = ls; var as = String(info.attached); if (att.textContent !== as) att.textContent = as; }
      dirty = false;
    }
    sync();
    return { push: push, draw: draw, get: get, header: head, dirty: function () { return dirty; } };
  }

  /* ---------- OWNERSHIP TIER: the open question made concrete (B16): baked, derived, modifiable ---------- */
  var TIER = { baked: 'baked', derived: 'derived', mod: 'modifiable' };
  function OwnershipTier(el, o) {
    o = o || {}; var rows = o.rows || [];
    el.classList.add('hd-tier'); el.innerHTML = '';
    var tb = tag('div', 'tier-table'); tb.setAttribute('role', 'table'); tb.setAttribute('aria-label', 'Modulator ownership tiers'); el.appendChild(tb);
    var msg = tag('div', 'hd-msg tier-msg'); msg.setAttribute('aria-live', 'polite'); el.appendChild(msg);
    function draw() {
      tb.innerHTML = '';
      var h = tag('div', 'tier-row tier-h'); h.setAttribute('role', 'row'); ['Modulator', 'Tier', 'Identity comes from', 'In preset'].forEach(function (s) { var c = tag('span', null, s); c.setAttribute('role', 'columnheader'); h.appendChild(c); }); tb.appendChild(h);
      rows.forEach(function (r, i) {
        var row = tag('div', 'tier-row'); row.setAttribute('role', 'row');
        var nm = tag('span', 'tier-n'); nm.setAttribute('role', 'rowheader'); var d = tag('i', 'kd'); d.style.setProperty('--sc', 'var(--src-' + r.kind + ')'); d.setAttribute('aria-hidden', 'true'); nm.appendChild(d); nm.appendChild(document.createTextNode(r.id)); row.appendChild(nm);
        var cell = tag('span'); cell.setAttribute('role', 'cell'); var b = tag('button', 'tier-chip hd-focus t-' + r.tier, TIER[r.tier]); b.type = 'button';
        b.setAttribute('aria-label', r.id + ': tier ' + TIER[r.tier] + (r.tier === 'mod' || r.was ? ', click to toggle baked' : ', fixed'));
        b.addEventListener('click', function () { toggle(i); }); cell.appendChild(b); row.appendChild(cell);
        var fr = tag('span', 'tier-from', r.tier === 'mod' ? 'its own parameters' : r.from); fr.setAttribute('role', 'cell'); fr.title = fr.textContent; row.appendChild(fr);
        var ip = tag('span', 'hd-value tier-ip', r.tier === 'mod' ? 'yes' : 'no'); ip.setAttribute('role', 'cell'); row.appendChild(ip);
        tb.appendChild(row);
      });
    }
    /* reading (i): a baked or derived identity is not a permission to grant, so the toggle is REFUSED and says why;
       reading (ii): a modifiable mod toggled to baked leaves the preset (In preset: no) */
    function toggle(i) {
      var r = rows[i];
      if ((r.tier === 'baked' && !r.was) || r.tier === 'derived') { msg.textContent = r.id + ': refused. Its identity is ' + r.from + ': there is no knob to unlock.'; fire(o.onRefuse, r); return; }
      if (r.tier === 'mod') { r.was = 'mod'; r.tier = 'baked'; r.from = r.from || 'a fixed identity'; } else { r.tier = 'mod'; r.was = null; }
      msg.textContent = r.id + ' → ' + TIER[r.tier] + (r.tier === 'baked' ? ': its parameters leave the preset.' : ': its parameters are stored again.');
      draw(); fire(o.onChange, rows); fire(o.onCommit, rows);
    }
    draw();
    return { rows: rows, draw: draw };
  }

  H.ModLaw = ModLaw; H.ModCardHeader = ModCardHeader; H.RateSync = RateSync; H.TempoDivisions = TempoDivisions;
  H.LfoShapeEditor = LfoShapeEditor; H.EnvelopeEditor = EnvelopeEditor; H.CurvePlot = CurvePlot; H.OrbitalCard = OrbitalCard;
  H.MidiTrackers = MidiTrackers; H.GateLanes = GateLanes; H.EventLanes = EventLanes; H.SmoothingLanes = SmoothingLanes;
  H.ShClockCard = ShClockCard; H.ModSlider = slider; H.OwnershipTier = OwnershipTier; H.ModActions = ACTIONS;
})(window.Horde);
/* =====================================================================
   THE MORPH EDITOR (B211, B269; ADR-266 rulings 9, 10, 11). The morph field corner by corner: the pad's
   quantum regions and their boundary drag, the colour scale, the mode banner and the lock bar, the
   cross-section, the grid with its three cell forms, find, the inspector, the kind legend, STEPPED as
   rate plus sync, and the one Cohesion control. Ported from docs/design/morph-editor-lab.html; the
   rulings win where the lab differs (edits are LIVE, not staged; STEPPED is a rate, not a step size).
   Everything a plugin would read from the engine (who owns a row where, the region scores, the lock
   law) goes through Morph.engine, a deterministic stand-in labelled as such: components never decide
   ownership themselves, they ask that seam.
   ===================================================================== */
(function (H) {
  'use strict';
  if (!H) return;
  var NS = 'http://www.w3.org/2000/svg', CORNERS = H.CORNERS, DRAG = 160;
  function clamp(x, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, x)); }
  function v(n) { return 'var(--' + n + ')'; }
  function mk(t, a, parent) { var e = document.createElementNS(NS, t); for (var k in a) e.setAttribute(k, a[k]); if (parent) parent.appendChild(e); return e; }
  function pct(x) { return Math.round(x * 100) + '%'; }
  function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }
  /* the core's FNV-1a, re-declared (core privates are not visible here): the stand-in's offsets must hash
     exactly as Session.ownerOf does, or the two stand-ins would disagree at the puck */
  function hash(s) { var h = 2166136261; for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
  function weights(x, y) { return [(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y]; }
  function div(cls, html, parent) { var e = document.createElement('div'); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; if (parent) parent.appendChild(e); return e; }
  function btn(cls, html, label, parent) { var b = document.createElement('button'); b.type = 'button'; b.className = cls; b.innerHTML = html; if (label) b.setAttribute('aria-label', label); if (parent) parent.appendChild(b); return b; }
  /* a token's colour as rgb, for canvases that paint non-screen roles (the corner fills on the pad) */
  function rgbOf(host, name) {
    var e = document.createElement('i'); e.style.color = 'var(--' + name + ')'; e.style.display = 'none'; host.appendChild(e);
    var m = getComputedStyle(e).color.match(/[\d.]+/g).map(Number); e.remove(); return [m[0], m[1], m[2]];
  }
  function rgba(c, a) { return 'rgba(' + c[0] + ',' + c[1] + ',' + c[2] + ',' + a + ')'; }
  function themeKey(node) { var t = node.closest('[data-scr]'); return (document.documentElement.getAttribute('data-theme') || '') + '/' + (t ? t.dataset.scr : ''); }

  /* ---------- the three kinds: a behaviour, drawn in ink with a word, never a colour (the lab's ruling) ---------- */
  var GLYPH = {
    c: '<svg viewBox="0 0 18 14" width="18" height="14" aria-hidden="true"><path d="M1 12 L17 2" style="stroke:currentColor;stroke-width:1.6;fill:none;stroke-linecap:round"/></svg>',
    s: '<svg viewBox="0 0 18 14" width="18" height="14" aria-hidden="true"><path d="M1 12 H6 V7 H11 V2 H17" style="stroke:currentColor;stroke-width:1.6;fill:none;stroke-linejoin:round"/></svg>',
    g: '<svg viewBox="0 0 18 14" width="18" height="14" aria-hidden="true"><path d="M1 12 L12 3" style="stroke:currentColor;stroke-width:1.6;fill:none;stroke-linecap:round"/><path d="M15 1 V13" style="stroke:currentColor;stroke-width:1.6;stroke-dasharray:2 1.6"/></svg>'
  };
  var KIND_WORD = { c: 'SLIDES', s: 'SNAPS', g: 'RAMPS' };
  var KIND_TITLE = { c: 'continuous: blends between corners, or flips and glides under QUANTUM', s: 'structural: snaps to one corner\'s value, whole', g: 'gate: its level ramps with the pad; the switch waits until it is inaudible' };
  function kindChip(k) { return '<span class="hm-kchip" title="' + esc(KIND_TITLE[k]) + '">' + GLYPH[k] + KIND_WORD[k] + '</span>'; }
  function kindOf(p) { return p.kind || 'c'; }
  function nOpts(p) { return p.labels ? p.labels.length : 2; }
  function fmtOf(p, x) {
    var k = kindOf(p);
    if (k === 'g') return x > .5 ? 'ON' : 'OFF';
    if (k === 's') { var j = Math.round(x); return p.labels && p.labels[j] !== undefined ? p.labels[j] : String(j); }
    return (p.fmt || pct)(x);
  }
  function norm(p, x) { return kindOf(p) === 's' ? (nOpts(p) > 1 ? x / (nOpts(p) - 1) : 0) : clamp(x); }
  function differs(p) { var a = p.vals; return !(a[0] === a[1] && a[1] === a[2] && a[2] === a[3]); }

  /* =====================================================================
     THE MORPH STATE on the session (S.morph). The integrator may fold it into Session; until then every
     morph component reads and writes it here, so they agree with each other.
     ===================================================================== */
  function state(S) {
    if (S.morph) return S.morph;
    var M = S.morph = {
      mode: 'blend',            // 'quantum' | 'blend' | 'stepped'
      cohesion: .3,             // PLACEHOLDER default: the factory patch's (the lab's MO - Quantum Morph); one control for the field (ADR-266 r11)
      seed: 0,                  // the stand-in's deal; 0 hashes exactly as Session.ownerOf
      locks: {},                // unit key -> four region offsets: a LOCKED row's own boundaries (B269)
      rate: 4, sync: false, beats: .25, bpm: 120,   // STEPPED: steps per second, or a tempo division in beats (.25 = 1/16; ADR-266 r10, B424)
      glide: .05,               // the morph glide, seconds: it softens each step's edge (B424)
      heard: { x: S.pad.x, y: S.pad.y }, held: { x: S.pad.x, y: S.pad.y }, stepN: -1, lastT: null, trace: [],
      hover: null, selCorner: 0,
      hist: [], fut: [],        // undo: one entry per gesture (ADR-266 r9: live edits, undo covers mistakes)
      view: 'differ', kinds: { c: true, s: true, g: true }, q: '', collapsed: {}
    };
    S.add({ render: function () { tick(S, M); } });   // added FIRST by whichever morph view is built first, so every view reads this frame's heard position
    addEventListener('keydown', function (e) {        // accelerators, keyboard mode only; the mouse routes are the UNDO and REDO buttons
      if (!S.keys || !(e.metaKey || e.ctrlKey) || e.code !== 'KeyZ' || (e.target && e.target.tagName === 'INPUT')) return;
      e.preventDefault(); if (e.shiftKey) redo(S); else undo(S);
    });
    return M;
  }
  /* one step per division at the tempo, as the modulators' RateSync counts it (rate = bpm / 60 / beats) */
  function stepsPerSecond(M) { return M.sync ? M.bpm / 60 / M.beats : M.rate; }
  /* STEPPED (B424, ADR-266 r10): a move travels in steps. PLACEHOLDER reading: the heard position samples the
     puck once per step (rate, or the division at the tempo) and the morph glide softens each step's edge.
     Frame time only; with reduced motion the heard position IS the puck (one still frame). */
  function tick(S, M) {
    var px = S.pad.x, py = S.pad.y;
    if (M.mode !== 'stepped' || S.reduce) { M.heard.x = M.held.x = px; M.heard.y = M.held.y = py; M.lastT = S.t; M.stepN = -1; return; }
    var n = Math.floor(S.t * stepsPerSecond(M));
    if (n !== M.stepN) { M.stepN = n; M.held = { x: px, y: py }; M.trace.push({ t: S.t, step: 1 }); }
    var dt = M.lastT === null ? 0 : Math.max(0, S.t - M.lastT), a = M.glide > 0 ? 1 - Math.exp(-dt / M.glide) : 1; M.lastT = S.t;
    M.heard.x += (M.held.x - M.heard.x) * a; M.heard.y += (M.held.y - M.heard.y) * a;
    M.trace.push({ t: S.t, px: px, hx: M.heard.x });
    while (M.trace.length && M.trace[0].t < S.t - 3.2) M.trace.shift();
  }
  function heardPad(S) { var M = state(S); return M.mode === 'stepped' ? M.heard : S.pad; }
  function setMode(S, mode) { var M = state(S); if (M.mode === mode) return; M.mode = mode; M.trace = []; S.say('Morph mode: ' + mode.toUpperCase()); }

  /* =====================================================================
     ENGINE STAND-IN — NEVER THE ENGINE. The plugin reads every answer here from the morph layer (RC-8,
     ADR-261's requirement: the UI never computes ownership). Deterministic: a row's four region offsets
     are its hash (the SAME spread Session.ownerOf uses, ±0.16), mixed toward one shared draw by Cohesion;
     a row's score at a pad point is its corner weight plus its offset, and the highest score owns it.
     A LOCKED row carries its own offsets, so the seed and Cohesion no longer reach it (B269). A unit
     (rows sharing p.unit) flips, and locks, as one. Offsets stay within ±0.45, so a corner always owns
     its own corner (there its weight is 1, the others 0): every region keeps its corner, the quantum
     twin of the lab's family guarantee.
     ===================================================================== */
  var BMAX = .45;
  function jitter(id) { var h = hash(id), o = []; for (var i = 0; i < 4; i++) o.push((((h >>> (i * 8)) & 255) / 255 - .5) * .32); return o; }
  function unitOf(p) { return p.unit || p.key; }
  var engine = {
    standIn: true,
    offsets: function (S, p) {
      var M = state(S), u = unitOf(p);
      if (M.locks[u]) return M.locks[u];
      var a = jitter(M.seed ? u + '#' + M.seed : u), g = jitter('#field#' + M.seed), c = M.cohesion;
      return a.map(function (x, i) { return (1 - c) * x + c * g[i]; });
    },
    scoresAt: function (S, p, x, y) { var w = weights(x, y), b = this.offsets(S, p); return w.map(function (wi, i) { return wi + b[i]; }); },
    ownerAt: function (S, p, x, y) { var s = this.scoresAt(S, p, x, y), best = 0; for (var i = 1; i < 4; i++) if (s[i] > s[best]) best = i; return best; },
    /* the same answer for many points: the offsets read once (a region map samples thousands of points) */
    ownerFn: function (S, p) { var b = this.offsets(S, p).slice(); return function (x, y) { var w = weights(x, y), best = 0, bv = w[0] + b[0]; for (var i = 1; i < 4; i++) if (w[i] + b[i] > bv) { bv = w[i] + b[i]; best = i; } return best; }; },
    owner: function (S, p) { var h = heardPad(S); return this.ownerAt(S, p, h.x, h.y); },
    isLocked: function (S, p) { return !!state(S).locks[unitOf(p)]; },
    lock: function (S, p) { var M = state(S), u = unitOf(p); if (!M.locks[u]) M.locks[u] = this.offsets(S, p).slice(); },
    unlock: function (S, p) { delete state(S).locks[unitOf(p)]; },
    /* drag the k|m boundary to (x, y): the one equation that puts the point on the k|m line, keeping the pair's sum
       (a symmetric edit), then re-centred so the bound rarely bites. Locks the row first. */
    dragBoundary: function (S, p, k, m, x, y) {
      this.lock(S, p); var L = state(S).locks[unitOf(p)];
      x = clamp(x, .002, .998); y = clamp(y, .002, .998);
      var w = weights(x, y), d = w[m] - w[k], s = L[k] + L[m];
      L[k] = (s + d) / 2; L[m] = (s - d) / 2;
      var mean = (L[0] + L[1] + L[2] + L[3]) / 4;
      for (var q = 0; q < 4; q++) L[q] = clamp(L[q] - mean, -BMAX, BMAX);
    },
    /* each corner's share of the pad for this row: the engine would publish it; here, sampled from the scores */
    shares: function (S, p, G) { G = G || 24; var n = [0, 0, 0, 0]; for (var gy = 0; gy < G; gy++) for (var gx = 0; gx < G; gx++) n[this.ownerAt(S, p, (gx + .5) / G, (gy + .5) / G)]++; return n.map(function (c) { return c / (G * G); }); }
  };
  /* does this row have regions in this mode? (B269 qRefusal) */
  function picks(S, p) { var k = kindOf(p); return k === 's' || (k === 'c' && state(S).mode === 'quantum'); }
  function noRegions(S, p) {
    if (!p) return 'Select a row first: boundaries belong to one parameter.';
    var k = kindOf(p);
    if (k === 'g') return 'A gate has no regions: its level ramps with the pad.';
    if (k === 'c' && state(S).mode !== 'quantum') return 'In ' + state(S).mode.toUpperCase() + ' a continuous row blends, so it has no regions. Its boundaries act in QUANTUM.';
    return '';
  }
  /* WHAT IS HEARD at the (heard) pad position: a gate's level ramp, a blend, or the owning corner's value */
  function hear(S, p) {
    var M = state(S), h = heardPad(S), w = weights(h.x, h.y), k = kindOf(p), i, b = 0;
    if (k === 'g') { for (i = 0; i < 4; i++) b += w[i] * p.vals[i]; b = clamp(b); return { v: b > 1e-3 ? 1 : 0, ramp: b, owner: -1, how: 'ramp', w: w }; }
    if (k === 'c' && M.mode !== 'quantum') { for (i = 0; i < 4; i++) b += w[i] * p.vals[i]; return { v: b, owner: -1, how: 'blend', w: w }; }
    var o = engine.ownerAt(S, p, h.x, h.y);
    return { v: p.vals[o], owner: o, how: k === 's' ? 'snap' : 'flip', w: w };
  }
  function heardText(p, r) { return r.how === 'ramp' ? 'level ' + pct(r.ramp) : fmtOf(p, r.v); }

  /* ---------- undo: every gesture is one entry; an undo into a locked corner is refused like any edit ---------- */
  function record(S, e) { var M = state(S); M.hist.push(e); if (M.hist.length > 200) M.hist.shift(); M.fut.length = 0; S.emit(); }
  function undo(S) { var M = state(S), e = M.hist[M.hist.length - 1]; if (!e) { S.say('Nothing to undo'); return; } if (e.can && !e.can()) return; M.hist.pop(); e.undo(); M.fut.push(e); S.say('Undid: ' + e.label); }
  function redo(S) { var M = state(S), e = M.fut[M.fut.length - 1]; if (!e) { S.say('Nothing to redo'); return; } if (e.can && !e.can()) return; M.fut.pop(); e.redo(); M.hist.push(e); S.say('Redid: ' + e.label); }
  function valEntry(S, p, k, before, after, label) {
    return { label: label || (p.name + ' · corner ' + CORNERS[k].id + ' ' + fmtOf(p, before) + ' → ' + fmtOf(p, after)),
      can: function () { return S.writable(p, k); }, undo: function () { p.vals[k] = before; }, redo: function () { p.vals[k] = after; } };
  }
  function lockEntry(S, p, before, after, label) {
    var M = state(S), u = unitOf(p);
    function put(L) { if (L) M.locks[u] = L.slice(); else delete M.locks[u]; }
    return { label: label, undo: function () { put(before); }, redo: function () { put(after); } };
  }
  function lockCopy(S, p) { var L = state(S).locks[unitOf(p)]; return L ? L.slice() : null; }
  function setLock(S, p, on) {
    var before = lockCopy(S, p); if (on) engine.lock(S, p); else engine.unlock(S, p);
    var after = lockCopy(S, p); if (String(before) === String(after)) return;
    record(S, lockEntry(S, p, before, after, (on ? 'Locked ' : 'Unlocked ') + p.name + '\'s regions'));
    S.say(on ? p.name + ' locked: its regions are its own; the seed and Cohesion no longer move them' : p.name + ' unlocked: its regions follow the seed and Cohesion again');
  }
  function select(S, key, k) { S.selected = key; if (k !== undefined && k >= 0) state(S).selCorner = k; S.emit(); }
  function register(S, rows) {
    rows.forEach(function (p) {
      p.kind = kindOf(p); if (p.def === undefined) p.def = p.kind === 'c' ? .5 : 0;
      if (!S.params[p.key]) { S.params[p.key] = p; S.saved[p.key] = p.vals.slice(); }
    });
  }
  function rowsOf(S) { var o = []; for (var k in S.params) if (S.params[k].section !== undefined || S.params[k].kind) o.push(S.params[k]); return o; }

  /* =====================================================================
     ONE CORNER VALUE, EDITED LIVE (ADR-266 r9): written at once, the corner latched for the gesture, one undo
     entry per gesture. Relative drag (right or up raises), 160 px the full range, shift x0.25; double-click
     resets to the default; Option + wheel always sets the value; arrows only in keyboard mode. A write to a
     locked corner (ADR-262 r4) is refused through Session.writable, which says why.
     ===================================================================== */
  function valueGesture(target, S, p, k) {
    var st = null, wheelAt = -1e9, wheelEntry = null, kind = kindOf(p);
    function write(x) {
      if (kind === 's') x = clamp(Math.round(x), 0, nOpts(p) - 1); else if (kind === 'g') x = x > .5 ? 1 : 0; else x = clamp(x);
      p.vals[k] = x;
    }
    function span() { return kind === 's' ? nOpts(p) - 1 : 1; }
    target.addEventListener('pointerdown', function (e) {
      if (e.button !== 0 || e.altKey) return;
      try { target.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ } select(S, p.key, k);
      st = { x: e.clientX, y: e.clientY, start: p.vals[k], moved: false, ok: S.writable(p, k) };
    });
    target.addEventListener('pointermove', function (e) {
      if (!st) return;
      var d = (e.clientX - st.x) - (e.clientY - st.y);
      if (Math.abs(d) > 3) st.moved = true; if (!st.moved || !st.ok) return;
      write(st.start + d / DRAG * span() * (e.shiftKey ? .25 : 1));
    });
    function end(e) {
      if (e && e.pointerId !== undefined && target.hasPointerCapture && target.hasPointerCapture(e.pointerId)) target.releasePointerCapture(e.pointerId);
      if (!st) return; var s0 = st; st = null;
      if (s0.ok && p.vals[k] !== s0.start) record(S, valEntry(S, p, k, s0.start, p.vals[k]));
    }
    target.addEventListener('pointerup', end); target.addEventListener('pointercancel', end); target.addEventListener('lostpointercapture', end);
    target.addEventListener('dblclick', function (e) {
      if (e.altKey || !S.writable(p, k)) return;
      var b = p.vals[k]; write(p.def); if (p.vals[k] !== b) record(S, valEntry(S, p, k, b, p.vals[k], p.name + ' · corner ' + CORNERS[k].id + ' reset'));
    });
    target.addEventListener('wheel', function (e) {
      if (!e.altKey) return; e.preventDefault();
      if (!S.writable(p, k)) return;
      var b = p.vals[k]; write(b - Math.sign(e.deltaY) * (kind === 'c' ? .01 : 1));
      if (p.vals[k] === b) return;
      /* one wheel spin is one gesture: notches within 450 ms of each other extend the same undo entry */
      if (wheelEntry && e.timeStamp - wheelAt < 450 && state(S).hist[state(S).hist.length - 1] === wheelEntry) { var a = p.vals[k]; wheelEntry.redo = function () { p.vals[k] = a; }; wheelEntry.label = p.name + ' · corner ' + CORNERS[k].id + ' → ' + fmtOf(p, a); }
      else { wheelEntry = valEntry(S, p, k, b, p.vals[k]); record(S, wheelEntry); }
      wheelAt = e.timeStamp;
    }, { passive: false });
    target.addEventListener('keydown', function (e) {
      if (!S.keys) return;
      var s = kind === 'c' ? (e.shiftKey ? .0025 : .01) : 1, d = 0;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') d = s; else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') d = -s; else return;
      e.preventDefault(); if (!S.writable(p, k)) return;
      var b = p.vals[k]; write(b + d); if (p.vals[k] !== b) record(S, valEntry(S, p, k, b, p.vals[k]));
    });
  }
  /* a gate's switch: a click toggles that corner's ON/OFF (one gesture, one undo entry) */
  function gateSwitch(b, S, p, k) {
    b.addEventListener('click', function (e) {
      e.stopPropagation(); select(S, p.key, k);
      if (!S.writable(p, k)) return;
      var before = p.vals[k]; p.vals[k] = before > .5 ? 0 : 1; record(S, valEntry(S, p, k, before, p.vals[k]));
    });
    b.addEventListener('dblclick', function (e) { e.stopPropagation(); });   // two clicks already toggled twice; a reset would be a third write
  }

  /* =====================================================================
     QUANTUM PAD — a layer on the core XY pad (H.XYPad), never a fork of it. It paints the selected row
     across the pad: a picking row (structural always, continuous under QUANTUM) as its corners' REGIONS with
     the boundaries drawn; a continuous row under BLEND or STEPPED as its value's brightness; a gate as its
     level, with the line where the switch flips. Dragging a boundary hands ground from one corner to the
     other and LOCKS the row (B269). The puck moves only when it is dragged (B269: "it will be necessary to be
     able to hear how the value changes at the current position"); the corner chips still pin.
     ===================================================================== */
  var M_GRID = 96, EDGE_PX = 5;
  function QuantumPad(padEl, S, opts) {
    opts = opts || {}; var M = state(S);
    padEl.classList.add('hm-qp');
    var layer = div('hm-qp-layer'), cv = document.createElement('canvas');
    cv.setAttribute('aria-hidden', 'true'); layer.appendChild(cv);
    layer.setAttribute('role', 'img');
    padEl.insertBefore(layer, padEl.querySelector('.puck'));
    var cache = { sig: '', own: null, val: null, col: null }, drag = null, hot = null, lastSig = '';
    function row() { var k = opts.key || S.selected; return k ? S.params[k] : null; }
    function at(e) { var r = padEl.getBoundingClientRect(); return { x: clamp((e.clientX - r.left) / r.width), y: clamp((e.clientY - r.top) / r.height), w: r.width, h: r.height }; }
    function boundaryUnder(p, q) {
      if (!p || !picks(S, p)) return null;
      var o = engine.ownerAt(S, p, q.x, q.y), dx = EDGE_PX / q.w, dy = EDGE_PX / q.h;
      var near = [[q.x + dx, q.y], [q.x - dx, q.y], [q.x, q.y + dy], [q.x, q.y - dy]].map(function (c) { return engine.ownerAt(S, p, clamp(c[0]), clamp(c[1])); });
      if (near.every(function (n) { return n === o; })) return null;
      var sc = engine.scoresAt(S, p, q.x, q.y), ks = [0, 1, 2, 3].sort(function (a, b) { return sc[b] - sc[a]; });
      return { k: ks[0], m: ks[1] };
    }
    /* the layer takes every pointer the puck and the chips do not: an empty click moves nothing */
    layer.addEventListener('pointerdown', function (e) {
      e.stopPropagation(); if (e.button !== 0) return;
      var p = row(), q = at(e), bd = boundaryUnder(p, q);
      if (!bd) { if (p && !picks(S, p)) S.say(noRegions(S, p) + ' Drag the puck to move where you listen.'); return; }
      try { layer.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
      drag = { k: bd.k, m: bd.m, before: lockCopy(S, p), p: p, moved: false }; hot = bd;
    });
    layer.addEventListener('pointermove', function (e) {
      var q = at(e), p = row();
      if (!drag) { M.hover = { x: q.x, y: q.y }; hot = boundaryUnder(p, q); layer.style.cursor = hot ? 'move' : 'default'; return; }
      engine.dragBoundary(S, drag.p, drag.k, drag.m, q.x, q.y); drag.moved = true;
    });
    function end(e) {
      if (e && e.pointerId !== undefined && layer.hasPointerCapture(e.pointerId)) layer.releasePointerCapture(e.pointerId);
      if (!drag) return; var d = drag; drag = null; hot = null;
      if (d.moved) { record(S, lockEntry(S, d.p, d.before, lockCopy(S, d.p), 'Moved ' + d.p.name + '\'s ' + CORNERS[d.k].id + '|' + CORNERS[d.m].id + ' boundary'));
        S.say(d.p.name + ': ' + CORNERS[d.k].id + '|' + CORNERS[d.m].id + ' boundary moved; the row is locked'); }
    }
    layer.addEventListener('pointerup', end); layer.addEventListener('pointercancel', end); layer.addEventListener('lostpointercapture', end);
    layer.addEventListener('pointerleave', function () { if (!drag) { M.hover = null; hot = null; } });
    layer.addEventListener('dblclick', function (e) {      // reset: a double-click on a boundary hands the row back to the seed
      e.stopPropagation(); var p = row(); if (p && boundaryUnder(p, at(e)) && engine.isLocked(S, p)) setLock(S, p, false);
    });
    function paint() {
      var p = row(), r = padEl.getBoundingClientRect(), W = Math.max(2, Math.round(r.width * Math.min(devicePixelRatio || 1, 2))), Hh = Math.max(2, Math.round(r.height * Math.min(devicePixelRatio || 1, 2)));
      if (cv.width !== W || cv.height !== Hh) { cv.width = W; cv.height = Hh; }
      var g = cv.getContext('2d'); g.lineJoin = 'round'; g.clearRect(0, 0, W, Hh);
      padEl.classList.toggle('hm-painted', !!p);
      if (!p) { layer.setAttribute('aria-label', 'Morph pad: no row selected'); return; }
      var K = H.Viz.kit(cv), cw = W / M_GRID, ch = Hh / M_GRID, xx, yy, q, h = heardPad(S);
      if (picks(S, p)) {
        /* each region in its corner's fill, stronger for a higher value, PRE-MIXED over the glass ground and painted
           opaque (overlapping translucent cells would leave a mesh at their seams) */
        var gr = K.P.ground, cols = [0, 1, 2, 3].map(function (i) { var c = rgbOf(padEl, 'c' + CORNERS[i].id), a = .55 + .4 * norm(p, p.vals[i]);
            return 'rgb(' + [0, 1, 2].map(function (j) { return Math.round(gr[j] + (c[j] - gr[j]) * a); }).join(',') + ')'; }),
          edge = rgbOf(padEl, 'on-corner'), own = new Int8Array(M_GRID * M_GRID), of = engine.ownerFn(S, p);
        for (yy = 0; yy < M_GRID; yy++) for (xx = 0; xx < M_GRID; xx++) { q = yy * M_GRID + xx; own[q] = of((xx + .5) / M_GRID, (yy + .5) / M_GRID);
          g.fillStyle = cols[own[q]]; g.fillRect(Math.floor(xx * cw), Math.floor(yy * ch), Math.ceil(cw) + 1, Math.ceil(ch) + 1); }
        var locked = engine.isLocked(S, p);
        function edges(pred, lw, dash) {
          g.beginPath();
          for (yy = 0; yy < M_GRID; yy++) for (xx = 0; xx < M_GRID; xx++) { q = yy * M_GRID + xx;
            if (xx + 1 < M_GRID && own[q] !== own[q + 1] && pred(own[q], own[q + 1])) { g.moveTo((xx + 1) * cw, yy * ch); g.lineTo((xx + 1) * cw, (yy + 1) * ch); }
            if (yy + 1 < M_GRID && own[q] !== own[q + M_GRID] && pred(own[q], own[q + M_GRID])) { g.moveTo(xx * cw, (yy + 1) * ch); g.lineTo((xx + 1) * cw, (yy + 1) * ch); } }
          g.strokeStyle = rgba(edge, 1); g.lineWidth = lw * K.V; g.setLineDash(dash ? dash.map(function (d) { return d * K.V; }) : []); g.stroke(); g.setLineDash([]);
        }
        /* boundaries: on-corner ink (dark in both themes, so it reads on every corner fill); dashed while they follow
           the seed, solid once LOCKED (authored); the boundary under the hand or being dragged, thickest */
        edges(function () { return true; }, locked ? 2.2 : 1.4, locked ? null : [4, 3]);
        if (hot) edges(function (a, b) { return (a === hot.k && b === hot.m) || (a === hot.m && b === hot.k); }, 3.6);
        var sh = [0, 0, 0, 0]; for (q = 0; q < own.length; q++) sh[own[q]]++;
        layer.setAttribute('aria-label', 'Morph pad, ' + p.name + ' regions' + (locked ? ', locked' : ', following the seed') + ': ' + CORNERS.map(function (c, i) { return c.id + ' ' + Math.round(100 * sh[i] / own.length) + '%'; }).join(', ') + '. Owner at the puck: ' + CORNERS[engine.owner(S, p)].id);
      } else if (kindOf(p) === 'g' || kindOf(p) === 'c') {
        /* a value map on the screen: brightness = the value (or the gate's level) in the physics role, the scale bar keys it */
        K.tube(); var gate = kindOf(p) === 'g', lv = new Float32Array(M_GRID * M_GRID);
        for (yy = 0; yy < M_GRID; yy++) for (xx = 0; xx < M_GRID; xx++) { q = yy * M_GRID + xx; var w = weights((xx + .5) / M_GRID, (yy + .5) / M_GRID), b = 0;
          for (var i = 0; i < 4; i++) b += w[i] * p.vals[i]; lv[q] = clamp(b);
          g.fillStyle = K.mix('ground', 'physics', Math.min(1, (.06 + .8 * lv[q]) * K.sc.alpha)); g.fillRect(Math.floor(xx * cw), Math.floor(yy * ch), Math.ceil(cw) + 1, Math.ceil(ch) + 1); }
        if (gate) { g.beginPath(); for (yy = 0; yy < M_GRID; yy++) for (xx = 0; xx < M_GRID; xx++) { q = yy * M_GRID + xx; var on = lv[q] > 1e-3;
            if (xx + 1 < M_GRID && on !== (lv[q + 1] > 1e-3)) { g.moveTo((xx + 1) * cw, yy * ch); g.lineTo((xx + 1) * cw, (yy + 1) * ch); }
            if (yy + 1 < M_GRID && on !== (lv[q + M_GRID] > 1e-3)) { g.moveTo(xx * cw, (yy + 1) * ch); g.lineTo((xx + 1) * cw, (yy + 1) * ch); } }
          g.strokeStyle = K.c('marker'); g.lineWidth = 1.6 * K.V; g.stroke(); }
        K.scan();
        layer.setAttribute('aria-label', 'Morph pad, ' + p.name + (gate ? ' level' : ' value') + ' across the pad; heard ' + heardText(p, hear(S, p)));
      }
      /* STEPPED: where the morph is heard now, a hollow square stepping after the puck */
      if (M.mode === 'stepped') { var K2 = H.Viz.kit(cv); g.strokeStyle = K2.c('marker'); g.lineWidth = 2 * K2.V; var s = 6 * K2.V; g.strokeRect(h.x * W - s, h.y * Hh - s, 2 * s, 2 * s); }
    }
    S.add({ render: function () {
      var p = row(), sig = [p ? p.key : '', p ? p.vals.join() : '', M.mode, M.cohesion, M.seed, p ? String(M.locks[unitOf(p)]) : '', hot ? hot.k + '' + hot.m : '',
        padEl.clientWidth, padEl.clientHeight, themeKey(padEl), M.mode === 'stepped' ? M.heard.x.toFixed(4) + M.heard.y.toFixed(4) : ''].join('|');
      if (sig !== lastSig) { lastSig = sig; paint(); }
    } });
    return { set: function (key) { opts.key = key; }, boundaryUnder: function (x, y) { var r = padEl.getBoundingClientRect(); return boundaryUnder(row(), { x: x, y: y, w: r.width, h: r.height }); } };
  }

  /* =====================================================================
     THE PAD'S BARS (one folder, PadBars): the colour scale under the pad, the mode banner above it, and the
     row's lock bar. The lab draws all three in one function (renderPadText) as the pad's attendant text.
     Each keeps a constant box: a mode or row change rewrites its words, never its size.
     ===================================================================== */
  function MorphScaleBar(el, S, opts) {
    opts = opts || {}; var M = state(S);
    el.classList.add('hm-scale');
    el.innerHTML = '<div class="hm-sc-row"><span class="hm-sc-lo hd-value"></span><div class="hm-sc-ramp"><div class="hd-viz hm-sc-glass"><canvas aria-hidden="true"></canvas></div><div class="hm-sc-marks" aria-hidden="true"></div></div><span class="hm-sc-hi hd-value"></span></div><div class="hm-sc-cap hd-hint" aria-live="off"></div>';
    var cv = el.querySelector('canvas'), marks = el.querySelector('.hm-sc-marks'), lo = el.querySelector('.hm-sc-lo'), hi = el.querySelector('.hm-sc-hi'), cap = el.querySelector('.hm-sc-cap'), last = '';
    function row() { var k = opts.key || S.selected; return k ? S.params[k] : null; }
    function x(p, val) { return (norm(p, val) * 100).toFixed(2) + '%'; }
    S.add({ render: function () {
      var p = row(), r = p ? hear(S, p) : null, hv = M.hover && p ? hearAt(S, p, M.hover.x, M.hover.y) : null;
      var sig = [p ? p.key + p.vals.join() : '', r ? r.v + '/' + r.ramp : '', hv ? hv.v + '/' + hv.ramp : '', M.mode, cv.clientWidth, themeKey(el)].join('|');
      if (sig === last) return; last = sig;
      var K = H.Viz.kit(cv), g = K.g, n = 64; K.tube();
      if (!p) { lo.textContent = ''; hi.textContent = ''; marks.innerHTML = ''; cap.textContent = 'Select a row: the scale shows its whole range.'; el.setAttribute('aria-label', 'Colour scale: no row selected'); return; }
      var k = kindOf(p), steps = k === 's' ? nOpts(p) : n;
      for (var q = 0; q < steps; q++) { g.fillStyle = K.c('physics', .06 + .8 * (k === 's' ? q / Math.max(1, steps - 1) : (q + .5) / steps)); g.fillRect(q * K.W / steps, 0, K.W / steps + .5, K.H); }
      if (k === 's') { g.fillStyle = K.c('grid', .9); for (q = 1; q < steps; q++) g.fillRect(q * K.W / steps - K.V / 2, 0, K.V, K.H); }
      K.scan();
      lo.textContent = k === 'g' ? '0%' : fmtOf(p, k === 's' ? 0 : 0); hi.textContent = k === 'g' ? '100%' : fmtOf(p, k === 's' ? nOpts(p) - 1 : 1);
      /* marks beside the screen, never over its data: each corner's stored value as its shape-coded pip above, the heard value
         as an ink bar, the value under the pointer as an ink triangle below */
      var h = '';
      /* corners holding the same value sit side by side, so every glyph stays visible */
      var seen = {};
      if (k !== 'g') for (var i = 0; i < 4; i++) { var at = x(p, p.vals[i]), n = seen[at] = (seen[at] || 0) + 1;
        h += '<span class="hm-pip" style="left:calc(' + at + ' + ' + (n - 1) * 11 + 'px)">' + H.glyph(i, 10, v('c' + CORNERS[i].id)) + '</span>'; }
      var hn = r.how === 'ramp' ? r.ramp : norm(p, r.v);
      h += '<span class="hm-heard" style="left:' + (hn * 100).toFixed(2) + '%"></span>';
      if (hv) h += '<span class="hm-hov" style="left:' + ((hv.how === 'ramp' ? hv.ramp : norm(p, hv.v)) * 100).toFixed(2) + '%"></span>';
      marks.innerHTML = h;
      cap.innerHTML = '▌ heard <b>' + esc(heardText(p, r)) + '</b> · ▼ ' + (hv ? 'pointer <b>' + esc(heardText(p, hv)) + '</b>' : 'hover the pad');
      el.setAttribute('aria-label', p.name + ' scale ' + lo.textContent + ' to ' + hi.textContent + ', heard ' + heardText(p, r));
    } });
  }
  /* the value the morph would give if it stood at (x, y): the HEARING column's own law, at another point */
  function hearAt(S, p, x, y) {
    var w = weights(x, y), k = kindOf(p), b = 0, i;
    if (k === 'g') { for (i = 0; i < 4; i++) b += w[i] * p.vals[i]; b = clamp(b); return { v: b > 1e-3 ? 1 : 0, ramp: b, owner: -1, how: 'ramp' }; }
    if (k === 'c' && state(S).mode !== 'quantum') { for (i = 0; i < 4; i++) b += w[i] * p.vals[i]; return { v: b, owner: -1, how: 'blend' }; }
    var o = engine.ownerAt(S, p, x, y); return { v: p.vals[o], owner: o, how: 'pick' };
  }
  var MODE_SAY = {
    quantum: '<b>QUANTUM.</b> Each row flips to one corner. On the pad you edit the <b>boundaries</b>: drag the line between two corner colours.',
    blend: '<b>BLEND.</b> Continuous rows interpolate between the corners; structural rows still snap. Their regions act in QUANTUM.',
    stepped: '<b>STEPPED.</b> A blend heard in steps: the morph follows the puck at the step rate, the glide softening each step.'
  };
  function QuantumBanner(el, S) {
    var M = state(S);
    el.classList.add('hm-banner'); el.setAttribute('role', 'status');
    el.innerHTML = '<span class="hm-bn-say">' + ['quantum', 'blend', 'stepped'].map(function (m) { return '<span data-m="' + m + '">' + MODE_SAY[m] + '</span>'; }).join('') + '</span>';
    var b = btn('hd-pill hd-focus hm-bn-go', '', '', el), last = '';
    b.addEventListener('click', function () { setMode(S, M.mode === 'quantum' ? 'blend' : 'quantum'); });
    function sync() {
      if (last === M.mode) return; last = M.mode;
      el.classList.toggle('q', M.mode === 'quantum');
      el.querySelectorAll('.hm-bn-say > span').forEach(function (sp) { var on = sp.dataset.m === M.mode; sp.style.visibility = on ? 'visible' : 'hidden'; sp.setAttribute('aria-hidden', String(!on)); });
      b.textContent = M.mode === 'quantum' ? 'SWITCH TO BLEND' : 'SWITCH TO QUANTUM';
    }
    S.on(sync); sync();
  }
  var LOCK_SVG = '<svg viewBox="0 0 10 10" width="10" height="10" aria-hidden="true"><path d="M3 4.6V3.3a2 2 0 0 1 4 0v1.3" style="fill:none;stroke:currentColor;stroke-width:1.3"/><rect x="1.8" y="4.6" width="6.4" height="4.6" rx="1" style="fill:currentColor"/></svg>';
  function LockBar(el, S, opts) {
    opts = opts || {}; var M = state(S), last = '';
    el.classList.add('hm-lockbar');
    el.innerHTML = '<span class="hm-lk-chip" aria-hidden="true"></span><span class="hm-lk-say"></span>';
    var b = btn('hd-pill hd-focus hm-lk-go', '', '', el);
    function row() { var k = opts.key || S.selected; return k ? S.params[k] : null; }
    b.addEventListener('click', function () { var p = row(); if (!p || kindOf(p) === 'g') return; setLock(S, p, !engine.isLocked(S, p)); });
    S.add({ render: function () {
      var p = row(), lk = p && engine.isLocked(S, p), pk = p && picks(S, p), sig = [p ? p.key : '', lk, pk, M.mode].join('|');
      if (sig === last) return; last = sig;
      var say = el.querySelector('.hm-lk-say'), chip = el.querySelector('.hm-lk-chip'), unit = p && p.unit ? ' (with its unit: a unit flips, so locks, as one)' : '';
      el.classList.toggle('locked', !!(lk && pk)); el.classList.toggle('dormant', !!(lk && !pk));
      chip.innerHTML = lk ? LOCK_SVG + 'LOCKED' : 'FOLLOWS SEED';
      if (!p) { say.innerHTML = 'Select a row to see whether its regions are its own.'; }
      else if (kindOf(p) === 'g') say.innerHTML = '<b>' + esc(p.name) + '</b> is a gate: no regions, nothing to lock. Its level ramps with the pad.';
      else if (lk) say.innerHTML = pk ? '<b>' + esc(p.name) + '\'s regions are its own</b>' + unit + ': the seed and Cohesion no longer move them.' : '<b>Locked, dormant here</b>: its regions act in QUANTUM' + unit + '.';
      else say.innerHTML = pk ? 'Boundaries <b>follow the seed</b> and Cohesion' + unit + '. Drag one on the pad to edit: that locks the row.' : 'In ' + M.mode.toUpperCase() + ' <b>' + esc(p.name) + '</b> blends, so it has no regions; they act in QUANTUM.';
      b.textContent = lk ? 'UNLOCK' : 'LOCK AS IS';
      b.disabled = !p || kindOf(p) === 'g';
      b.setAttribute('aria-label', (lk ? 'Unlock ' : 'Lock ') + (p ? p.name + '\'s regions' : 'regions'));
    } });
  }

  /* =====================================================================
     CROSS-SECTION (the lab's :633 / :3353): one row along a line across the pad. The heard value as the beam;
     under QUANTUM the BLEND it would otherwise play, dotted, so a flip reads against the slide; the row's range
     edges; the corners the line passes; the puck's projection when it sits on the line. The line's two ends are
     dragged on the pad (opts.pad, an XY pad element), relative, never a jump.
     PLACEHOLDER: waypoints (B235) and their four laws are not ruled; when they are, their pins join this plot.
     ===================================================================== */
  /* the chord of the line through p and q across the unit square (the lab's clipToSquare): the two handles only
     aim the line, so they never have to sit on a corner chip or at the pad's clipped edge */
  function chord(p, q) {
    var dx = q[0] - p[0], dy = q[1] - p[1], t0 = -Infinity, t1 = Infinity;
    if (Math.hypot(dx, dy) < 1e-9) { dx = 1; dy = 0; }
    [[p[0], dx], [p[1], dy]].forEach(function (od) { var o = od[0], d = od[1]; if (Math.abs(d) < 1e-12) return; var a = -o / d, b = (1 - o) / d; t0 = Math.max(t0, Math.min(a, b)); t1 = Math.min(t1, Math.max(a, b)); });
    return [[clamp(p[0] + dx * t0), clamp(p[1] + dy * t0)], [clamp(p[0] + dx * t1), clamp(p[1] + dy * t1)]];
  }
  var HMIN = .08, HMAX = .92;
  function CrossSection(el, S, opts) {
    opts = opts || {}; var M = state(S), line = { a: (opts.a || [.3, .3]).slice(), b: (opts.b || [.7, .7]).slice() }, last = '';
    el.classList.add('hm-xs');
    el.innerHTML = '<div class="hd-viz hm-xs-glass"><canvas role="img"></canvas></div><div class="hm-xs-key hd-hint"></div>';
    var cv = el.querySelector('canvas'), key = el.querySelector('.hm-xs-key');
    function row() { var k = opts.key || S.selected; return k ? S.params[k] : null; }
    var ov = null, hs = [];
    if (opts.pad) {
      ov = mk('svg', { class: 'hm-xs-ov', 'aria-hidden': 'true', preserveAspectRatio: 'none' }); opts.pad.appendChild(ov);
      [0, 1].forEach(function (end) {
        var h = document.createElement('button'); h.type = 'button'; h.className = 'hm-xs-end hd-focus'; h.innerHTML = end ? '›' : '‹';
        h.setAttribute('aria-label', 'Cross-section line, ' + (end ? 'second' : 'first') + ' handle. Drag on the pad to aim the line; double-click resets to A to D');
        opts.pad.appendChild(h); hs.push(h);
        var st = null;
        h.addEventListener('pointerdown', function (e) { e.stopPropagation(); if (e.button !== 0) return; try { h.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ } var q = line[end ? 'b' : 'a']; st = { x: e.clientX, y: e.clientY, q: q.slice() }; });
        h.addEventListener('pointermove', function (e) { if (!st) return; var r = opts.pad.getBoundingClientRect(), k = e.shiftKey ? .25 : 1;
          line[end ? 'b' : 'a'] = [clamp(st.q[0] + (e.clientX - st.x) / r.width * k, HMIN, HMAX), clamp(st.q[1] + (e.clientY - st.y) / r.height * k, HMIN, HMAX)]; });
        function up(e) { if (e && e.pointerId !== undefined && h.hasPointerCapture(e.pointerId)) h.releasePointerCapture(e.pointerId); st = null; }
        h.addEventListener('pointerup', up); h.addEventListener('pointercancel', up); h.addEventListener('lostpointercapture', up);
        h.addEventListener('dblclick', function (e) { e.stopPropagation(); line.a = [.3, .3]; line.b = [.7, .7]; });
        h.addEventListener('keydown', function (e) { if (!S.keys) return; var s = e.shiftKey ? .0025 : .01, q = line[end ? 'b' : 'a'];
          if (e.key === 'ArrowRight') q[0] = clamp(q[0] + s, HMIN, HMAX); else if (e.key === 'ArrowLeft') q[0] = clamp(q[0] - s, HMIN, HMAX); else if (e.key === 'ArrowUp') q[1] = clamp(q[1] - s, HMIN, HMAX); else if (e.key === 'ArrowDown') q[1] = clamp(q[1] + s, HMIN, HMAX); else return; e.preventDefault(); });
      });
    }
    function sample(p, a, b, n) {
      var out = [];
      for (var s = 0; s <= n; s++) { var t = s / n, x = a[0] + (b[0] - a[0]) * t, y = a[1] + (b[1] - a[1]) * t, w = weights(x, y), bl = 0, i;
        for (i = 0; i < 4; i++) bl += w[i] * p.vals[i];
        var hv = hearAt(S, p, x, y); out.push({ t: t, v: hv.how === 'ramp' ? hv.ramp : norm(p, hv.v), blend: kindOf(p) === 's' ? null : clamp(bl), owner: hv.owner }); }
      return out;
    }
    S.add({ render: function () {
      var p = row(), hp = heardPad(S), cd = chord(line.a, line.b);
      if (ov) { var r = opts.pad.getBoundingClientRect(); ov.setAttribute('viewBox', '0 0 ' + r.width + ' ' + r.height); ov.innerHTML = '';
        var x1 = cd[0][0] * r.width, y1 = cd[0][1] * r.height, x2 = cd[1][0] * r.width, y2 = cd[1][1] * r.height;
        mk('line', { x1: x1, y1: y1, x2: x2, y2: y2, style: 'stroke:var(--panel);stroke-width:4;opacity:.85;stroke-linecap:round' }, ov);
        mk('line', { x1: x1, y1: y1, x2: x2, y2: y2, style: 'stroke:var(--ink);stroke-width:1.5;stroke-dasharray:5 4' }, ov);
        hs[0].style.left = line.a[0] * r.width + 'px'; hs[0].style.top = line.a[1] * r.height + 'px'; hs[1].style.left = line.b[0] * r.width + 'px'; hs[1].style.top = line.b[1] * r.height + 'px';
        hs.forEach(function (h) { h.tabIndex = S.keys ? 0 : -1; }); }
      var sig = [p ? p.key + p.vals.join() : '', line.a.join(), line.b.join(), M.mode, M.cohesion, M.seed, p ? String(M.locks[unitOf(p)]) : '', hp.x.toFixed(4), hp.y.toFixed(4), cv.clientWidth, cv.clientHeight, themeKey(el)].join('|');
      if (sig === last) return; last = sig;
      var K = H.Viz.kit(cv), g = K.g, W = K.W, Hh = K.H, V = K.V; K.tube(); K.scan();
      var font = (getComputedStyle(el).getPropertyValue('--font-mono') || 'monospace');
      g.font = (9 * V) + 'px ' + font; g.textBaseline = 'middle';
      if (!p) { g.fillStyle = K.c('physics'); g.textAlign = 'center'; g.fillText('select a row', W / 2, Hh / 2); key.textContent = ''; cv.setAttribute('aria-label', 'Cross-section: no row selected'); return; }
      var ML = 46 * V, MR = 10 * V, MT = 10 * V, MB = 20 * V, X = function (t) { return ML + t * (W - ML - MR); }, Y = function (n) { return MT + (1 - n) * (Hh - MT - MB); };
      var A = cd[0], B = cd[1], pts = sample(p, A, B, 200), k = kindOf(p);
      /* the range edges, dashed, labelled in the row's own units */
      g.strokeStyle = K.c('grid', .7); g.lineWidth = V; g.setLineDash([3 * V, 3 * V]);
      [0, 1].forEach(function (n) { g.beginPath(); g.moveTo(ML, Y(n)); g.lineTo(W - MR, Y(n)); g.stroke(); }); g.setLineDash([]);
      g.fillStyle = K.c('grid'); g.textAlign = 'right';
      g.fillText(k === 'g' ? '100%' : fmtOf(p, k === 's' ? nOpts(p) - 1 : 1), ML - 4 * V, Y(1)); g.fillText(k === 'g' ? '0%' : fmtOf(p, 0), ML - 4 * V, Y(0));
      /* the corners the line passes, as their letters (a screen uses its own A–D, ADR-259 r4) */
      var dx = B[0] - A[0], dy = B[1] - A[1], L2 = dx * dx + dy * dy || 1;
      function proj(px, py) { var t = ((px - A[0]) * dx + (py - A[1]) * dy) / L2; return { t: t, d: Math.hypot(px - (A[0] + dx * t), py - (A[1] + dy * t)) }; }
      g.textAlign = 'center';
      [[0, 0], [1, 0], [0, 1], [1, 1]].forEach(function (c, i) { var pr = proj(c[0], c[1]); if (pr.d < .03 && pr.t > -.01 && pr.t < 1.01) { g.fillStyle = K.c('physics'); g.fillText(CORNERS[i].id, X(clamp(pr.t)), Hh - 8 * V); } });
      /* an end that is not a corner says where it meets the edge */
      function atCorner(q) { return (q[0] < .02 || q[0] > .98) && (q[1] < .02 || q[1] > .98); }
      g.fillStyle = K.c('grid');
      if (!atCorner(A)) { g.textAlign = 'left'; g.fillText(A[0].toFixed(2) + ', ' + A[1].toFixed(2), ML, Hh - 8 * V); }
      if (!atCorner(B)) { g.textAlign = 'right'; g.fillText(B[0].toFixed(2) + ', ' + B[1].toFixed(2), W - MR, Hh - 8 * V); }
      /* the blend it would play, dotted, wherever the heard law is not the blend */
      var showBlend = k !== 's' && k !== 'g' && M.mode === 'quantum';
      if (showBlend) { g.strokeStyle = K.c('grid', .9); g.lineWidth = 1.2 * V; g.setLineDash([1.5 * V, 2.5 * V]); g.beginPath(); pts.forEach(function (q, s) { s ? g.lineTo(X(q.t), Y(q.blend)) : g.moveTo(X(q.t), Y(q.blend)); }); g.stroke(); g.setLineDash([]); }
      /* the heard value: the beam (an 8 px underlay at 22% under a 2.2 px core; glow is never a blur) */
      /* a picking row is drawn as steps (a flip is a jump, so the riser is vertical); a blend or a ramp as a curve */
      var stepwise = k === 's' || (k === 'c' && M.mode === 'quantum');
      g.lineJoin = 'round'; g.lineCap = 'round';
      var path = new Path2D(); pts.forEach(function (q, s) { var y = Y(q.v); if (s && stepwise && q.v !== pts[s - 1].v) path.lineTo(X(q.t), Y(pts[s - 1].v)); s ? path.lineTo(X(q.t), y) : path.moveTo(X(q.t), y); });
      g.strokeStyle = K.c('value', .22); g.lineWidth = 8 * V; g.stroke(path); g.strokeStyle = K.c('value'); g.lineWidth = 2.2 * V; g.stroke(path);
      g.lineJoin = 'round'; g.lineCap = 'butt';
      /* the puck, where it projects onto the line within 0.03 of it */
      var pp = proj(hp.x, hp.y), onLine = pp.d < .03 && pp.t >= 0 && pp.t <= 1;
      if (onLine) { g.strokeStyle = K.c('marker'); g.lineWidth = 1.2 * V; g.setLineDash([3 * V, 3 * V]); g.beginPath(); g.moveTo(X(pp.t), MT); g.lineTo(X(pp.t), Hh - MB); g.stroke(); g.setLineDash([]); }
      var heard = hear(S, p);
      cv.setAttribute('aria-label', p.name + ' along the line from ' + A.map(function (z) { return z.toFixed(2); }).join(', ') + ' to ' + B.map(function (z) { return z.toFixed(2); }).join(', ') + '; heard at the puck ' + heardText(p, heard));
      key.innerHTML = '<span class="hm-lk-sw b"></span>heard (' + M.mode.toUpperCase() + ')' + (showBlend ? ' · <span class="hm-lk-sw o"></span>what BLEND would play' : '') + ' · <span class="hm-lk-sw d"></span>range · ' + (onLine ? '<span class="hm-lk-sw m"></span>the puck, on the line' : 'the puck is off the line');
    } });
    return { line: function (a, b) { if (a) { line.a = a.map(function (z) { return clamp(z, HMIN, HMAX); }); line.b = b.map(function (z) { return clamp(z, HMIN, HMAX); }); } return { a: line.a.slice(), b: line.b.slice(), chord: chord(line.a, line.b) }; }, set: function (key) { opts.key = key; } };
  }

  /* =====================================================================
     MORPH GRID — four corners x every member (B211): sections, group captions, a kind chip per row, the four
     corner cells in their kind's form (SLIDES a bar, SNAPS a pill, RAMPS a switch), and what you are HEARING.
     Edits are live (ADR-266 r9). Ownership shows as the owner's stripe in its line ink; a blend as each
     corner's weight underline. Rows that agree in all four corners are dimmed and, in the default view, hidden.
     ===================================================================== */
  function MorphGrid(el, S, opts) {
    opts = opts || {}; var M = state(S); register(S, opts.rows || []);
    var rows = (opts.rows || []).slice(), sections = opts.sections || [];
    rows.forEach(function (p) { if (sections.indexOf(p.section) < 0) sections.push(p.section); });
    el.classList.add('hm-grid'); el.setAttribute('role', 'grid'); el.setAttribute('aria-label', opts.label || 'Morph field: four corners by every member');
    el.innerHTML = '<div class="hm-gbar"><span class="hd-cluster">' + esc(opts.title || 'Morph field') + '</span><span class="hm-gcount hd-hint"></span><span class="hm-gsp"></span></div><div class="hm-ghead hm-gr" role="row"></div><div class="hm-gbody"></div>';
    var bar = el.querySelector('.hm-gbar'), count = el.querySelector('.hm-gcount'), head = el.querySelector('.hm-ghead'), body = el.querySelector('.hm-gbody');
    if (opts.maxHeight) body.style.maxHeight = opts.maxHeight + 'px';
    var U = btn('hd-iconbtn hd-focus hm-undo', '<svg viewBox="0 0 10 10"><path d="M2.4 4.4A3.1 3.1 0 1 1 3.2 7.6" style="fill:none;stroke:currentColor;stroke-width:1.3;stroke-linecap:round"/><path d="M.9 2.6 2.6 5 4.6 3.1Z"/></svg>', 'Undo', bar);
    var R = btn('hd-iconbtn hd-focus hm-redo', '<svg viewBox="0 0 10 10" style="transform:scaleX(-1)"><path d="M2.4 4.4A3.1 3.1 0 1 1 3.2 7.6" style="fill:none;stroke:currentColor;stroke-width:1.3;stroke-linecap:round"/><path d="M.9 2.6 2.6 5 4.6 3.1Z"/></svg>', 'Redo', bar);
    U.title = 'Undo the last edit'; R.title = 'Redo';
    U.addEventListener('click', function () { undo(S); }); R.addEventListener('click', function () { redo(S); });
    /* head: the parameter column, the four corners (glyph chip, letter, the preset's real name, weight), HEARING */
    head.innerHTML = '<div class="hm-gk" aria-hidden="true"></div><div class="hm-gn hm-hd" role="columnheader">PARAMETER</div>' +
      CORNERS.map(function (c, i) { return '<div class="hm-gc hm-hd hm-ch" role="columnheader" data-c="' + i + '" style="--cc:var(--c' + c.id + ')"><span class="hd-chip on" aria-hidden="true"><svg viewBox="0 0 10 10"><path d="' + c.glyph + '"/></svg></span><span class="hm-chn"><b>' + c.id + '</b> <span class="hm-chp"></span></span><span class="hm-chw hd-value"></span></div>'; }).join('') +
      '<div class="hm-gh hm-hd" role="columnheader">HEARING</div>';
    var refs = {};
    function visible(p) {
      if (!M.kinds[kindOf(p)]) return false;
      var q = M.q.trim().toLowerCase();
      if (q) return (p.name + ' ' + (p.group || '') + ' ' + p.section + ' ' + p.key).toLowerCase().indexOf(q) >= 0;
      return M.view === 'all' || differs(p) || p.key === S.selected;
    }
    function build() {
      var h = '', shown = 0;
      sections.forEach(function (sec, si) {
        var all = rows.filter(function (p) { return p.section === sec; }), vis = all.filter(visible), col = !!M.collapsed[sec];
        if (!vis.length) return;
        var nd = all.filter(differs).length;
        h += '<div class="hm-sec" role="row" data-sec="' + esc(sec) + '" id="' + (el.id || 'hm') + '-sec-' + si + '"><button type="button" class="hm-secb hd-focus" aria-expanded="' + !col + '"><svg viewBox="0 0 10 10" width="9" height="9" aria-hidden="true" style="transform:rotate(' + (col ? -90 : 0) + 'deg)"><path d="M1.5 3 5 7 8.5 3Z" style="fill:currentColor"/></svg>' + esc(sec) + '</button><span class="hm-secn hd-value">' + all.length + ' members · ' + nd + ' differ' + (vis.length < all.length ? ' · showing ' + vis.length : '') + '</span></div>';
        if (col) return;
        var lastG = null;
        vis.forEach(function (p) {
          if (p.group && p.group !== lastG) { h += '<div class="hm-gcap" role="row"><span role="rowheader">' + esc(p.group) + '</span></div>'; lastG = p.group; }
          var k = kindOf(p);
          h += '<div class="hm-gr hm-row" role="row" data-key="' + esc(p.key) + '"><div class="hm-gk" title="' + esc(KIND_TITLE[k]) + '">' + GLYPH[k] + '<span class="hd-sr">' + KIND_WORD[k] + '</span></div>' +
            '<div class="hm-gn" role="rowheader"><button type="button" class="hm-pname">' + esc(p.name) + (p.unit ? '<span class="hm-unit">UNIT·' + esc(p.unit) + '</span>' : '') + '</button><span class="hm-ppath">' + esc(p.path || (p.section + (p.group ? ' · ' + p.group : ''))) + '</span></div>';
          for (var c = 0; c < 4; c++) {
            h += '<div class="hm-gc hm-cell" role="gridcell" data-c="' + c + '" style="--oc:var(--c' + CORNERS[c].id + '-tx)">' +
              (k === 'g' ? '<button type="button" class="hm-gsw hd-focus" role="switch"></button>'
                : '<div class="hm-cv hd-focus" role="slider" aria-valuemin="0" aria-valuemax="' + (k === 's' ? nOpts(p) - 1 : 1) + '">' + (k === 's' ? '<span class="hm-spill"></span>' : '<span class="hm-bar"><i></i></span><span class="hm-tx"></span>') + '</div>') +
              '<i class="hm-wu" aria-hidden="true"></i></div>';
          }
          h += '<div class="hm-gh hm-hear" role="gridcell"><span class="hm-bar h"><i></i></span><span class="hm-tx"></span><span class="hm-hx"></span></div></div>';
          shown++;
        });
      });
      if (!shown) h = '<div class="hm-empty hd-hint">No row matches. The default view shows only rows whose corners disagree: try ALL.</div>';
      body.innerHTML = h; refs = {};
      body.querySelectorAll('.hm-row').forEach(function (r) {
        var p = S.params[r.dataset.key]; refs[p.key] = { row: r, cells: r.querySelectorAll('.hm-cell'), hear: r.querySelector('.hm-hear'), sig: '' };
        r.querySelector('.hm-pname').addEventListener('click', function () { select(S, p.key); });
        refs[p.key].cells.forEach(function (cell, c) {
          var ctl = cell.querySelector('.hm-cv'), sw = cell.querySelector('.hm-gsw');
          if (ctl) valueGesture(ctl, S, p, c); if (sw) gateSwitch(sw, S, p, c);
        });
      });
      body.querySelectorAll('.hm-secb').forEach(function (b) { b.addEventListener('click', function () { var s = b.parentElement.dataset.sec; M.collapsed[s] = !M.collapsed[s]; build(); }); });
      count.textContent = rows.length + ' members · showing ' + shown;
      lastHead = '';
    }
    var lastHead = '', lastFilter = '', busy = false;
    body.addEventListener('pointerdown', function () { busy = true; }, true);
    addEventListener('pointerup', function () { busy = false; }); addEventListener('pointercancel', function () { busy = false; });
    function paintRow(p, R) {
      var r = hear(S, p), sel = S.selected === p.key, sc = M.selCorner, k = kindOf(p), lk = S.locks.join(), same = !differs(p);
      var sig = [p.vals.join(), r.v, r.owner, r.ramp, r.w.map(function (x) { return x.toFixed(3); }).join(), sel, sc, lk, S.keys].join('|');
      if (sig === R.sig) return; R.sig = sig;
      R.row.classList.toggle('sel', sel); R.row.classList.toggle('same', same);
      R.cells.forEach(function (cell, c) {
        var val = p.vals[c], ctl = cell.querySelector('.hm-cv'), sw = cell.querySelector('.hm-gsw');
        cell.classList.toggle('pick', sel && c === sc); cell.dataset.selc = sel && c === sc ? CORNERS[c].id : '';
        cell.classList.toggle('own', r.owner === c); cell.classList.toggle('lockd', S.locks[c]);
        cell.querySelector('.hm-wu').style.width = r.owner < 0 ? 'calc((100% - 12px) * ' + r.w[c].toFixed(3) + ')' : '0';
        var label = p.name + ', corner ' + CORNERS[c].id + (S.corners[c] ? ' (' + S.corners[c] + ')' : '') + ': ' + fmtOf(p, val) + (r.owner === c ? ', owns it now' : '') + (S.locks[c] ? ', corner locked' : '');
        if (sw) { sw.textContent = val > .5 ? 'ON' : 'OFF'; sw.setAttribute('aria-checked', String(val > .5)); sw.setAttribute('aria-label', label); sw.tabIndex = 0; }
        else {
          ctl.tabIndex = S.keys ? 0 : -1; ctl.setAttribute('aria-label', label); ctl.setAttribute('aria-valuenow', String(val)); ctl.setAttribute('aria-valuetext', fmtOf(p, val));
          if (k === 's') ctl.querySelector('.hm-spill').textContent = fmtOf(p, val);
          else { ctl.querySelector('.hm-bar i').style.width = (norm(p, val) * 100).toFixed(1) + '%'; ctl.querySelector('.hm-tx').textContent = fmtOf(p, val); }
        }
      });
      var hh = R.hear, hn = r.how === 'ramp' ? r.ramp : norm(p, r.v);
      hh.querySelector('.hm-bar i').style.width = (hn * 100).toFixed(1) + '%';
      hh.querySelector('.hm-tx').textContent = k === 'g' ? (r.v ? 'ON' : 'OFF') : fmtOf(p, r.v);
      hh.querySelector('.hm-hx').innerHTML = r.how === 'ramp' ? pct(r.ramp) : r.how === 'blend' ? 'blend' : H.glyph(r.owner, 7, v('c' + CORNERS[r.owner].id + '-tx')) + CORNERS[r.owner].id;
      hh.setAttribute('aria-label', 'Hearing ' + heardText(p, r) + (r.owner >= 0 ? ', from corner ' + CORNERS[r.owner].id : r.how === 'blend' ? ', a blend' : ''));
    }
    S.add({ render: function () {
      var fsig = [M.view, M.q, M.kinds.c, M.kinds.s, M.kinds.g, S.selected, rows.map(function (p) { return differs(p) ? 1 : 0; }).join('')].join('|');
      if (fsig !== lastFilter && !busy) { lastFilter = fsig; build(); }
      var w = weights(heardPad(S).x, heardPad(S).y), hs = [w.map(function (x) { return x.toFixed(2); }).join(), S.corners.join(), S.locks.join(), M.mode].join('|');
      if (hs !== lastHead) { lastHead = hs;
        head.querySelectorAll('.hm-ch').forEach(function (h, c) {
          h.querySelector('.hm-chp').textContent = S.corners[c] || 'Empty'; h.querySelector('.hm-chp').title = S.corners[c] || 'Empty';
          h.querySelector('.hm-chw').innerHTML = (S.locks[c] ? LOCK_SVG : '') + pct(w[c]);
          h.setAttribute('aria-label', 'Corner ' + CORNERS[c].id + ', ' + (S.corners[c] || 'empty') + ', weight ' + pct(w[c]) + (S.locks[c] ? ', locked' : ''));
        }); }
      for (var key in refs) paintRow(S.params[key], refs[key]);
      U.disabled = !M.hist.length; R.disabled = !M.fut.length;
    } });
    function jump(sec) {
      M.collapsed[sec] = false; if (M.view !== 'all') M.view = 'all'; lastFilter = ''; build();
      var t = body.querySelector('[data-sec="' + sec.replace(/"/g, '') + '"]'); if (t) body.scrollTop = t.offsetTop - body.offsetTop;
      S.emit();
    }
    build();
    return { rows: rows, sections: sections, build: function () { lastFilter = ''; }, jump: jump, shown: function () { return Object.keys(refs).length; } };
  }

  /* ---------- FIND: search, the view (where corners differ, or all), the kinds, jump to a section, the census ---------- */
  function FindPane(el, S, grid) {
    var M = state(S);
    el.classList.add('hm-find'); el.setAttribute('role', 'search');
    el.innerHTML = '<input type="text" class="hm-q hd-focus" placeholder="search name, group, section" aria-label="Search the morph field">' +
      '<div class="hm-fcap">SHOW</div><div class="hd-pills hm-fview" role="radiogroup" aria-label="Rows to show"></div>' +
      '<div class="hm-fcap">KINDS</div><div class="hd-pills hm-fkind" role="group" aria-label="Kinds to show"></div>' +
      '<div class="hm-fcap">JUMP TO SECTION</div><div class="hd-pills hm-fjump"></div><div class="hm-census"></div>';
    var q = el.querySelector('.hm-q'), vw = el.querySelector('.hm-fview'), kd = el.querySelector('.hm-fkind'), jp = el.querySelector('.hm-fjump'), cen = el.querySelector('.hm-census');
    q.addEventListener('input', function () { M.q = q.value; S.emit(); });
    [['differ', 'WHERE CORNERS DIFFER'], ['all', 'ALL']].forEach(function (o) {
      var b = btn('hd-pill hd-focus', o[1], '', vw); b.setAttribute('role', 'radio'); b.dataset.v = o[0];
      b.addEventListener('click', function () { M.view = o[0]; S.emit(); });
    });
    ['c', 's', 'g'].forEach(function (k) {
      var b = btn('hd-pill hd-focus hm-kp', GLYPH[k] + KIND_WORD[k], '', kd); b.dataset.k = k; b.title = KIND_TITLE[k];
      b.addEventListener('click', function () { M.kinds[k] = !M.kinds[k]; S.emit(); });
    });
    grid.sections.forEach(function (sec) { var b = btn('hd-pill hd-focus', esc(sec), 'Jump to ' + sec, jp); b.addEventListener('click', function () { grid.jump(sec); }); });
    var last = '';
    S.add({ render: function () {
      var rows = grid.rows, nd = rows.filter(differs).length, kc = { c: 0, s: 0, g: 0 }; rows.forEach(function (p) { kc[kindOf(p)]++; });
      var sig = [M.view, M.kinds.c, M.kinds.s, M.kinds.g, nd, grid.shown(), M.q].join('|'); if (sig === last) return; last = sig;
      if (q.value !== M.q) q.value = M.q;
      vw.querySelectorAll('button').forEach(function (b) { b.setAttribute('aria-checked', String(b.dataset.v === M.view)); });
      kd.querySelectorAll('button').forEach(function (b) { b.setAttribute('aria-pressed', String(!!M.kinds[b.dataset.k])); });
      cen.innerHTML = '<span>members <b>' + rows.length + '</b></span><span>showing <b>' + grid.shown() + '</b></span>' +
        '<span>' + GLYPH.c + ' slides <b>' + kc.c + '</b></span><span>corners differ <b>' + nd + '</b></span>' +
        '<span>' + GLYPH.s + ' snaps <b>' + kc.s + '</b></span><span>' + GLYPH.g + ' ramps <b>' + kc.g + '</b></span>';
    } });
  }

  /* =====================================================================
     MORPH INSPECTOR — the selected row: its place, its kind and what that means in this mode, the four corners
     each with its preset name and a live control, the IMPACT box (whether an edit to the selected corner is heard
     at the puck, and what is heard), and, for a row that picks, its regions' shares and its lock.
     ===================================================================== */
  function MorphInspector(el, S) {
    var M = state(S), lastKey = null, lastSig = '';
    el.classList.add('hm-insp');
    function build() {
      var p = S.params[S.selected]; lastKey = S.selected; lastSig = '';
      if (!p || !p.vals || p.section === undefined) { el.innerHTML = '<div class="hd-cluster">Inspector</div><span class="hd-hint">Select a row, or a cell to pick that corner.</span>'; return; }
      var k = kindOf(p), rng = k === 'g' ? 'OFF / ON' : fmtOf(p, 0) + ' … ' + fmtOf(p, k === 's' ? nOpts(p) - 1 : 1);
      var h = '<div class="hm-ihead"><span class="hd-label">' + esc(p.section + (p.group ? ' · ' + p.group : '')) + '</span></div>' +
        '<div class="hm-ititle">' + esc(p.name) + '</div><div class="hm-imeta hd-hint">' + esc(rng) + ' · default ' + esc(fmtOf(p, p.def)) + (p.unit ? ' · unit ' + esc(p.unit) + ' flips and locks as one' : '') + '</div>' +
        '<div class="hm-ikind">' + kindChip(k) + '<span class="hm-isay"></span></div><div class="hm-ceds" role="group" aria-label="The four corners">';
      CORNERS.forEach(function (c, i) {
        h += '<div class="hm-ced" data-c="' + i + '" style="--cc:var(--c' + c.id + ')"><span class="hd-chip on" aria-hidden="true"><svg viewBox="0 0 10 10"><path d="' + c.glyph + '"/></svg></span>' +
          '<span class="hm-cl"><b>' + c.id + '</b> <span class="hm-cpn"></span></span>' +
          (k === 'g' ? '<button type="button" class="hd-toggle hd-focus hm-ictl" role="switch"></button>'
            : k === 's' ? '<select class="hd-select hd-focus hm-ictl">' + p.labels.map(function (t, j) { return '<option value="' + j + '">' + esc(t) + '</option>'; }).join('') + '</select>'
            : '<div class="hm-ictl hm-mini hd-focus" role="slider" aria-valuemin="0" aria-valuemax="1"><span class="hm-mtr"><i></i></span><span class="hm-mth"></span></div>') +
          '<output class="hd-value hm-iout"></output><span class="hm-iown" aria-hidden="true"></span></div>';
      });
      h += '</div><div class="hm-impact" aria-live="polite"></div><div class="hm-iact"></div><div class="hm-iq"></div>';
      el.innerHTML = h;
      el.querySelectorAll('.hm-ced').forEach(function (row, i) {
        var ctl = row.querySelector('.hm-ictl');
        row.addEventListener('pointerdown', function () { if (M.selCorner !== i) { M.selCorner = i; S.emit(); } });
        if (k === 'c') valueGesture(ctl, S, p, i);
        else if (k === 'g') gateSwitch(ctl, S, p, i);
        else ctl.addEventListener('change', function () {
          var before = p.vals[i], after = +ctl.value; M.selCorner = i;
          if (!S.writable(p, i)) { ctl.value = String(before); return; }
          p.vals[i] = after; record(S, valEntry(S, p, i, before, after));
        });
      });
      var act = el.querySelector('.hm-iact');
      var all = btn('hd-pill hd-focus', 'SET ALL FOUR', 'Write the selected corner\'s value into all four corners', act);
      all.addEventListener('click', function () {
        var src = M.selCorner, val = p.vals[src], before = p.vals.slice();
        for (var c = 0; c < 4; c++) if (c !== src && p.vals[c] !== val && S.locks[c]) { S.writable(p, c); return; }   // refused whole, and said why
        for (c = 0; c < 4; c++) p.vals[c] = val;
        var after = p.vals.slice(); if (String(before) === String(after)) { S.say('All four corners already hold ' + fmtOf(p, val)); return; }
        record(S, { label: p.name + ': ' + fmtOf(p, val) + ' in all four corners', can: function () { for (var c = 0; c < 4; c++) if (before[c] !== after[c] && !S.writable(p, c)) return false; return true; },
          undo: function () { for (var c = 0; c < 4; c++) p.vals[c] = before[c]; }, redo: function () { for (var c = 0; c < 4; c++) p.vals[c] = after[c]; } });
        S.say(p.name + ': ' + fmtOf(p, val) + ' written into all four corners');
      });
      btn('hd-pill hd-focus hm-iundo', 'UNDO', 'Undo the last edit', act).addEventListener('click', function () { undo(S); });
      var iq = el.querySelector('.hm-iq');
      if (k === 'g') iq.innerHTML = '<div class="hm-fcap">QUANTUM BOUNDARIES</div><span class="hd-hint">No boundaries on a gate: it never picks, its level ramps with the pad.</span>';
      else { iq.innerHTML = '<div class="hm-fcap">QUANTUM BOUNDARIES · WHICH CORNER OWNS IT WHERE</div><div class="hm-shares"></div><div class="hm-iqrow"><span class="hm-iqs hd-hint"></span></div>';
        var lb = btn('hd-pill hd-focus hm-iql', '', '', iq.querySelector('.hm-iqrow')); lb.addEventListener('click', function () { setLock(S, p, !engine.isLocked(S, p)); }); }
    }
    function paint() {
      var p = S.params[S.selected]; if (!p || !p.vals || p.section === undefined) return;
      var r = hear(S, p), sc = M.selCorner, k = kindOf(p), lk = engine.isLocked(S, p), pk = picks(S, p);
      var sig = [p.vals.join(), r.v, r.owner, r.ramp, r.w.map(function (x) { return x.toFixed(2); }).join(), sc, S.locks.join(), S.corners.join(), M.mode, lk, M.cohesion, M.seed, String(M.locks[unitOf(p)]), S.keys, M.hist.length].join('|');
      if (sig === lastSig) return; lastSig = sig;
      el.querySelector('.hm-isay').innerHTML = k === 'c' ? (M.mode === 'quantum' ? '<b>Flips, then glides</b>: one corner wins it at a time; the value slides to the winner over the morph glide.' : M.mode === 'stepped' ? '<b>Blends in steps</b>: every corner at its weight, heard at the step rate.' : '<b>Blends</b>: every corner is heard at its weight.')
        : k === 's' ? '<b>Snaps</b>: the winning corner\'s value, whole and at once.' : '<b>Ramps</b>: the level is the weighted share of corners holding it ON; the switch flips only when inaudible.';
      el.querySelectorAll('.hm-ced').forEach(function (row, i) {
        var ctl = row.querySelector('.hm-ictl'), val = p.vals[i], lab = p.name + ', corner ' + CORNERS[i].id + (S.corners[i] ? ' (' + S.corners[i] + ')' : '');
        row.classList.toggle('pick', i === sc); row.classList.toggle('sel', i === sc); row.dataset.selc = i === sc ? CORNERS[i].id : '';
        row.querySelector('.hm-cpn').textContent = S.corners[i] || 'Empty'; row.querySelector('.hm-cpn').title = S.corners[i] || 'Empty';
        row.querySelector('.hm-iout').textContent = fmtOf(p, val);
        row.querySelector('.hm-iown').innerHTML = (S.locks[i] ? LOCK_SVG : '') + (r.owner === i ? '<span class="hm-ownt">OWNS</span>' : r.owner < 0 ? '<span class="hm-wt">' + pct(r.w[i]) + '</span>' : '');
        if (k === 'g') { ctl.setAttribute('aria-checked', String(val > .5)); ctl.setAttribute('aria-label', lab); }
        else if (k === 's') { ctl.value = String(Math.round(val)); ctl.setAttribute('aria-label', lab); }
        else { ctl.tabIndex = S.keys ? 0 : -1; ctl.style.setProperty('--v', norm(p, val)); ctl.setAttribute('aria-label', lab); ctl.setAttribute('aria-valuenow', String(val)); ctl.setAttribute('aria-valuetext', fmtOf(p, val)); }
      });
      var C = CORNERS[sc].id, why;
      if (S.locks[sc]) why = 'Corner <b>' + C + '</b> is locked: edits to it are refused (it still morphs).';
      else if (r.how === 'ramp') why = 'A gate hears every corner at its weight: <b>' + C + '</b> is <b>' + pct(r.w[sc]) + '</b> here.';
      else if (r.how === 'blend') why = (M.mode === 'stepped' ? 'STEPPED' : 'BLEND') + ' hears every corner at its weight: <b>' + C + '</b> is <b>' + pct(r.w[sc]) + '</b> here, so an edit to it is heard in that proportion.';
      else if (r.owner === sc) why = '<b>' + C + '</b> owns this row at the puck: an edit to it is heard at once.';
      else why = '<b>' + CORNERS[r.owner].id + '</b> owns this row here, not ' + C + ': an edit to ' + C + ' is <b>silent until ' + C + ' wins it</b>.';
      el.querySelector('.hm-impact').innerHTML = 'Editing corner <b>' + C + '</b>. ' + why + ' Hearing <span class="hm-hv">' + esc(heardText(p, r)) + '</span>.';
      el.querySelector('.hm-iundo').disabled = !M.hist.length;
      var sh = el.querySelector('.hm-shares');
      if (sh) {
        var f = engine.shares(S, p, 24);
        sh.innerHTML = CORNERS.map(function (c, i) { return '<span class="hm-sh" style="--cc:var(--c' + c.id + ')">' + H.glyph(i, 8, v('c' + c.id + '-tx')) + '<b>' + c.id + '</b><span class="hm-shb"><i style="width:' + (f[i] * 100).toFixed(1) + '%"></i></span><span class="hd-value">' + pct(f[i]) + '</span></span>'; }).join('');
        el.querySelector('.hm-iqs').innerHTML = (lk ? LOCK_SVG + ' <b>locked</b>' + (pk ? ': its regions are its own.' : ', dormant: acts in QUANTUM.') : '<b>following the seed</b>' + (pk ? ': drag a boundary on the pad to lock it.' : ': acts in QUANTUM.')) + ' Shares ' + (pk ? 'of the pad now.' : 'it would take in QUANTUM.');
        var lb = el.querySelector('.hm-iql'); lb.textContent = lk ? 'UNLOCK' : 'LOCK AS IS'; lb.setAttribute('aria-label', (lk ? 'Unlock ' : 'Lock ') + p.name + '\'s regions');
      }
    }
    S.add({ render: function () { if (S.selected !== lastKey) build(); paint(); } });
    build();
  }

  /* ---------- KIND LEGEND: the three behaviours, each a chip, a sentence and a real row swept A → D ---------- */
  function KindLegend(el, S, opts) {
    opts = opts || {}; var M = state(S), last = '';
    el.classList.add('hm-legend');
    var ex = opts.examples || {};
    el.innerHTML = ['c', 's', 'g'].map(function (k) {
      return '<div class="hm-lg-chip">' + kindChip(k) + '</div><div class="hm-lg-say" data-k="' + k + '"></div><div class="hd-viz hm-lg-glass"><canvas role="img" data-k="' + k + '"></canvas></div>';
    }).join('');
    function say(k) {
      var p = S.params[ex[k]], n = p ? ' <i>' + esc(p.name) + ', swept A → D:</i>' : '';
      if (k === 'c') return (M.mode === 'quantum' ? '<b>Continuous.</b> Under QUANTUM it flips like a snap, but glides to each new winner over the morph glide, so it is never heard jumping.' : '<b>Continuous.</b> The value moves through the space between corners.') + n;
      if (k === 's') return '<b>Structural.</b> Jumps to one corner\'s value, whole: a waveform or a mode never passes through values nobody chose.' + n;
      return '<b>Gate.</b> The level ramps with the pad; the switch waits until it is inaudible.' + n;
    }
    S.add({ render: function () {
      var sig = [M.mode, M.cohesion, M.seed, JSON.stringify(M.locks), ['c', 's', 'g'].map(function (k) { var p = S.params[ex[k]]; return p ? p.vals.join() : ''; }).join(), el.clientWidth, themeKey(el)].join('|');
      if (sig === last) return; last = sig;
      ['c', 's', 'g'].forEach(function (k) {
        el.querySelector('.hm-lg-say[data-k="' + k + '"]').innerHTML = say(k);
        var cv = el.querySelector('canvas[data-k="' + k + '"]'), p = S.params[ex[k]], K = H.Viz.kit(cv), g = K.g, W = K.W, Hh = K.H, V = K.V; K.tube(); K.scan();
        if (!p) return;
        var pts = [], N = 90, i;
        for (i = 0; i <= N; i++) { var t = i / N, r = hearAt(S, p, t, t); pts.push(r.how === 'ramp' ? r.ramp : norm(p, r.v)); }
        g.strokeStyle = K.c('physics', .22); g.lineWidth = 6 * V; g.lineJoin = 'round';
        function path() { g.beginPath(); pts.forEach(function (q, s) { var x = 4 * V + s / N * (W - 8 * V), y = Hh - 5 * V - q * (Hh - 10 * V); s ? g.lineTo(x, y) : g.moveTo(x, y); }); }
        path(); g.stroke(); path(); g.strokeStyle = K.c('physics'); g.lineWidth = 1.8 * V; g.stroke(); g.lineJoin = 'round';
        g.fillStyle = K.c('grid'); g.font = (8 * V) + 'px ' + (getComputedStyle(el).getPropertyValue('--font-mono') || 'monospace'); g.textBaseline = 'top';
        g.textAlign = 'left'; g.fillText('A', 4 * V, 3 * V); g.textAlign = 'right'; g.fillText('D', W - 4 * V, 3 * V);
        cv.setAttribute('aria-label', KIND_WORD[k] + ': ' + p.name + ' swept from corner A to corner D, from ' + heardText(p, hearAt(S, p, 0, 0)) + ' to ' + heardText(p, hearAt(S, p, 1, 1)));
      });
    } });
  }

  /* =====================================================================
     A FIELD SLIDER: the house slider's face (.hd-slider) with the house gestures: relative drag, 160 px the full
     range, shift x0.25, double-click resets, Option + wheel sets the value, arrows only in keyboard mode. One undo
     entry per gesture when a session is given.
     ===================================================================== */
  function FieldSlider(el, o) {
    var lo = o.min, hi = o.max, toN = function (x) { return o.log ? Math.log(x / lo) / Math.log(hi / lo) : (x - lo) / (hi - lo); }, fromN = function (n) { return o.log ? lo * Math.pow(hi / lo, n) : lo + (hi - lo) * n; };
    var val = o.value, off = false, S = o.S;
    el.className = 'hd-srow hm-fs'; el.innerHTML = '<label class="hd-label">' + esc(o.name) + '</label><div class="hd-slider hd-focus" role="slider" aria-label="' + esc(o.path || o.name) + '"><div class="track"><i class="fill"></i></div><i class="handle"></i></div><output class="hd-value"></output>';
    var sl = el.querySelector('.hd-slider'), out = el.querySelector('output'), st = null, wheelAt = -1e9, wEntry = null;
    function paint() { var n = clamp(toN(val)); sl.style.setProperty('--v', n); sl.style.setProperty('--f0', '0px'); sl.style.setProperty('--fw', 'calc(' + n + ' * (100% - 13px) + 5px)');
      out.textContent = o.fmt(val); sl.setAttribute('aria-valuetext', o.fmt(val)); sl.setAttribute('aria-valuenow', String(val)); sl.setAttribute('aria-valuemin', String(lo)); sl.setAttribute('aria-valuemax', String(hi)); sl.tabIndex = S && !S.keys ? -1 : 0; }
    function set(x, fire) { val = clamp(x, lo, hi); paint(); if (fire && o.onInput) o.onInput(val); }
    function rec(before, label) { if (!S || before === val) return null; var a = val, e = { label: label || o.name + ' ' + o.fmt(before) + ' → ' + o.fmt(a), undo: function () { set(before, true); }, redo: function () { set(a, true); } }; record(S, e); return e; }
    sl.addEventListener('pointerdown', function (e) { if (off || e.button !== 0) return; try { sl.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ } sl.classList.add('drag'); st = { x: e.clientX, y: e.clientY, n: toN(val), v0: val }; });
    sl.addEventListener('pointermove', function (e) { if (!st) return; set(fromN(clamp(st.n + ((e.clientX - st.x) - (e.clientY - st.y)) / DRAG * (e.shiftKey ? .25 : 1))), true); });
    function end(e) { if (e && e.pointerId !== undefined && sl.hasPointerCapture(e.pointerId)) sl.releasePointerCapture(e.pointerId); sl.classList.remove('drag'); if (st) { var s0 = st; st = null; rec(s0.v0); } }
    sl.addEventListener('pointerup', end); sl.addEventListener('pointercancel', end); sl.addEventListener('lostpointercapture', end);
    sl.addEventListener('dblclick', function () { if (off) return; var b = val; set(o.def === undefined ? o.value : o.def, true); rec(b, o.name + ' reset'); });
    sl.addEventListener('wheel', function (e) { if (!e.altKey || off) return; e.preventDefault(); var b = val; set(fromN(clamp(toN(val) - Math.sign(e.deltaY) * .01)), true);
      if (wEntry && e.timeStamp - wheelAt < 450 && S && state(S).hist[state(S).hist.length - 1] === wEntry) { var a = val; wEntry.redo = function () { set(a, true); }; } else wEntry = rec(b); wheelAt = e.timeStamp; }, { passive: false });
    sl.addEventListener('keydown', function (e) { if (off || (S && !S.keys)) return; var s = e.shiftKey ? .0025 : .01, n = toN(val), b = val;
      if (e.key === 'ArrowRight' || e.key === 'ArrowUp') n += s; else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') n -= s; else return; e.preventDefault(); set(fromN(clamp(n)), true); rec(b); });
    paint();
    return { get: function () { return val; }, set: function (x) { set(x, false); }, disable: function (d) { off = !!d; sl.setAttribute('aria-disabled', String(off)); }, refresh: paint };
  }

  /* ---------- MORPH MODE pills: QUANTUM · BLEND · STEPPED (the lab's header control) ---------- */
  function MorphMode(el, S) {
    var M = state(S);
    el.classList.add('hd-pills'); el.setAttribute('role', 'radiogroup'); el.setAttribute('aria-label', 'Morph mode');
    var bs = ['quantum', 'blend', 'stepped'].map(function (m) { var b = btn('hd-pill hd-focus', m.toUpperCase(), '', el); b.setAttribute('role', 'radio'); b.dataset.m = m;
      b.addEventListener('click', function () { var before = M.mode; if (before === m) return; setMode(S, m); record(S, { label: 'Mode ' + before.toUpperCase() + ' → ' + m.toUpperCase(), undo: function () { setMode(S, before); }, redo: function () { setMode(S, m); } }); }); return b; });
    function sync() { bs.forEach(function (b) { b.setAttribute('aria-checked', String(b.dataset.m === M.mode)); }); }
    S.on(sync); sync();
  }

  /* ---------- STEPPED: rate plus sync (ADR-266 r10), the glide that softens each step, and a scope of the travel ---------- */
  function SteppedControl(el, S, opts) {
    opts = opts || {}; var M = state(S), last = -1;
    el.classList.add('hm-step');
    /* size 'compact' (the Density ladder): the dense switch faces and slider rows, and a lower scope glass */
    if (opts.size === 'compact') el.classList.add('hd-dense', 'hm-step-sm');
    el.innerHTML = '<div class="hm-st-rs"></div>' + (opts.glide === false ? '' : '<div class="hm-st-gl"></div>') +
      '<div class="hd-viz hm-st-glass"><canvas role="img" aria-label="Stepped travel: the puck and the heard position over the last three seconds"></canvas></div><div class="hm-st-key hd-hint"><span class="hm-lk-sw o"></span>the puck · <span class="hm-lk-sw b"></span>heard, in steps · <span class="hm-st-note"></span></div>';
    /* the house's ONE rate-plus-sync control (H.RateSync, the modulator family's, ADR-266 r10); each committed change is
       one undo entry here, restored through its own set() */
    function snap() { return { sync: M.sync, hz: M.rate, beats: M.beats }; }
    var before = snap(), rs = H.RateSync(el.querySelector('.hm-st-rs'), { name: 'Step rate', path: 'Morph · Step rate', unit: '/s', sync: M.sync, hz: M.rate, beats: M.beats, bpm: function () { return M.bpm; },
      min: .25, max: 32, defHz: 4, defBeats: .25, keys: function () { return S.keys; },
      onChange: function (x) { M.sync = !!x.sync; M.rate = x.hz; M.beats = x.beats; },
      onCommit: function () { var b = before, a = snap(); before = a; if (JSON.stringify(a) === JSON.stringify(b)) return;
        function put(z) { M.sync = z.sync; M.rate = z.hz; M.beats = z.beats; rs.set(z); before = snap(); }
        record(S, { label: 'Step rate ' + (b.sync ? b.beats + ' beat' : b.hz.toFixed(2) + '/s') + ' → ' + (a.sync ? a.beats + ' beat' : a.hz.toFixed(2) + '/s'), undo: function () { put(b); }, redo: function () { put(a); } }); } });
    if (opts.glide !== false) FieldSlider(el.querySelector('.hm-st-gl'), { name: 'Glide', path: 'Morph · Glide', min: 0, max: .5, value: M.glide, def: .05, S: S,
      fmt: function (x) { return Math.round(x * 1000) + ' ms'; }, onInput: function (x) { M.glide = x; } });
    var cv = el.querySelector('canvas'), note = el.querySelector('.hm-st-note');
    S.add({ render: function () {
      var on = M.mode === 'stepped';
      note.textContent = on ? (S.reduce ? 'reduced motion: heard = the puck' : stepsPerSecond(M).toFixed(2) + ' steps/s') : 'acts in STEPPED';
      if (S.reduce && last === 1) return;
      var K = H.Viz.kit(cv), g = K.g, W = K.W, Hh = K.H, V = K.V; K.tube(); K.scan(); last = S.reduce ? 1 : 0;
      g.strokeStyle = K.c('grid', .35); g.lineWidth = V; g.beginPath(); g.moveTo(0, Hh / 2); g.lineTo(W, Hh / 2); g.stroke();
      if (!on || !M.trace.length) { g.fillStyle = K.c('physics'); g.font = (9 * V) + 'px ' + (getComputedStyle(el).getPropertyValue('--font-mono') || 'monospace'); g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(on ? 'move the puck' : 'STEPPED is off', W / 2, Hh / 2); return; }
      var t1 = S.t, X = function (t) { return W - (t1 - t) / 3 * W; }, Y = function (x) { return 4 * V + (1 - x) * (Hh - 8 * V); };
      g.fillStyle = K.c('marker', .8); M.trace.forEach(function (q) { if (q.step) g.fillRect(X(q.t) - V / 2, Hh - 4 * V, V, 4 * V); });
      var tr = M.trace.filter(function (q) { return !q.step; });
      function line(f, k, a, lw, dash) { g.strokeStyle = K.c(k, a); g.lineWidth = lw * V; g.setLineDash(dash ? dash.map(function (d) { return d * V; }) : []); g.beginPath(); tr.forEach(function (q, s) { s ? g.lineTo(X(q.t), Y(f(q))) : g.moveTo(X(q.t), Y(f(q))); }); g.stroke(); g.setLineDash([]); }
      line(function (q) { return q.px; }, 'grid', .9, 1.2, [3, 3]);
      line(function (q) { return q.hx; }, 'value', .22, 7); line(function (q) { return q.hx; }, 'value', 1, 2);
    } });
    return rs;
  }

  /* ---------- COHESION: ONE control for the whole field (ADR-266 r11), and the field's owners at the puck under it ---------- */
  function Cohesion(el, S, opts) {
    opts = opts || {}; var M = state(S), last = '';
    el.classList.add('hm-coh');
    el.innerHTML = '<div class="hm-coh-s"></div><p class="hm-coh-say hd-hint">How strongly the parameters pick the <b>same corner together</b>: at 0 each draws its own, at 1 the whole field flips as one (a unit always does). Not the swarm\'s Coupling.</p><div class="hm-coh-strip" role="img"></div><div class="hm-coh-sum hd-value"></div>';
    var fs = FieldSlider(el.querySelector('.hm-coh-s'), { name: 'Cohesion', path: 'Morph · Cohesion', min: 0, max: 1, value: M.cohesion, def: .3, S: S, fmt: function (x) { return x.toFixed(2); }, onInput: function (x) { M.cohesion = x; } });
    var strip = el.querySelector('.hm-coh-strip'), sum = el.querySelector('.hm-coh-sum');
    S.add({ render: function () {
      if (Math.abs(fs.get() - M.cohesion) > 1e-9) fs.set(M.cohesion);
      var rows = rowsOf(S).filter(function (p) { return kindOf(p) !== 'g'; }).slice(0, opts.max || 24), hp = heardPad(S);
      var own = rows.map(function (p) { return engine.ownerAt(S, p, hp.x, hp.y); }), sig = own.join('') + M.cohesion + M.seed + JSON.stringify(M.locks); if (sig === last) return; last = sig;
      strip.innerHTML = own.map(function (o, i) { return '<span class="hm-coh-d" title="' + esc(rows[i].name) + ': ' + CORNERS[o].id + '" style="--cc:var(--c' + CORNERS[o].id + ')">' + H.glyph(o, 8, 'var(--on-corner)') + '</span>'; }).join('');
      var n = [0, 0, 0, 0]; own.forEach(function (o) { n[o]++; }); var top = n.indexOf(Math.max.apply(null, n));
      sum.textContent = own.length ? n[top] + ' of ' + own.length + ' rows would pick ' + CORNERS[top].id + ' at the puck' : '';
      strip.setAttribute('aria-label', 'Owners at the puck under QUANTUM: ' + CORNERS.map(function (c, i) { return c.id + ' ' + n[i]; }).join(', '));
    } });
    return fs;
  }

  H.Morph = { state: state, engine: engine, hear: hear, hearAt: hearAt, picks: picks, undo: undo, redo: redo, setMode: setMode, setLock: setLock, stepsPerSecond: stepsPerSecond, register: register, fmt: fmtOf, kindChip: kindChip, FieldSlider: FieldSlider };
  H.QuantumPad = QuantumPad; H.MorphScaleBar = MorphScaleBar; H.QuantumBanner = QuantumBanner; H.LockBar = LockBar;
  H.CrossSection = CrossSection; H.MorphGrid = MorphGrid; H.FindPane = FindPane; H.MorphInspector = MorphInspector;
  H.KindLegend = KindLegend; H.SteppedControl = SteppedControl; H.MorphMode = MorphMode; H.Cohesion = Cohesion;
})(window.Horde);
/* =====================================================================
   FX CHAIN VIEWS (B531, ADR-266; the FX chain morph lab, round 3: docs/design/fx-chain-morph-lab-r3.html).
   Every module is resident at a fixed place and the morph moves only the CABLES between them. Three views
   draw ONE cable state: RACK (signal flow in house order), MATRIX (from x to), PANEL (a case of modules and
   patch cords). They share one encoding, FxCables, so a cable reads the same in all three.
   The views DRAW what the engine publishes (lab L0064: the view draws the object the audio plays); they
   never compute a weight, a loop or a wait. FxSample is the previews' seeded stand-in for that engine.
   ===================================================================== */
(function (H) {
  'use strict';
  var NS = 'http://www.w3.org/2000/svg';
  var MIN = 0.003;                     // below this a cable is not drawn (-50 dB; the lab's VIEW_MIN)
  function v(n) { return 'var(--' + n + ')'; }
  function clamp(x, a, b) { return Math.min(b, Math.max(a, x)); }
  function mk(t, a, p) { var e = document.createElementNS(NS, t); for (var k in a) if (a[k] !== null && a[k] !== undefined) e.setAttribute(k, a[k]); if (p) p.appendChild(e); return e; }
  function txt(p, x, y, s, a) { a = a || {}; a.x = x; a.y = y; var e = mk('text', a, p); e.textContent = s; return e; }
  function f2(x) { return Math.round(x * 100) / 100; }

  /* ---------- MODULE LOOK: a cream card with a small screen (ADR-273, the human's ruling 2026-10-23, option B) ----------
     A module is an ordinary panel card with ink edges, like every other card in the window. Its identity is a small
     SCREEN INSET in its own display style, and the only colour it owns is that screen's: fxs-<module>-ground, -ink, -mid
     (ADR-273 retired the flat fx-<module> fills of ADR-266 ruling 5). A screen is its own display, so those three are the
     same in light and dark; the cream card around it follows the theme. A fourth token, fxs-<module>-cable (ADR-273 A1, the
     comparison page's cable colour), is the colour of the module's cables, arrowheads and matrix bars; it is not the screen
     ink, and it is always cased in the house ink (CASE below), so a pale cable on the white well still reads. A lane (Body, Sub) and OUT are not modules: plain cards with no screen; Body's cable is t2 and Sub's
     hint, with no casing. Shriek (80s workstation) and Sluice (Game Boy Color LCD) are the two REAL display styles;
     the other six are labelled STAND-INS (SCREENS below) until their skins are chosen. */
  var STYLES = ['bulwark', 'filter', 'eq', 'drive', 'shriek', 'echo', 'sluice', 'scape'];
  var SCREENS = { bulwark: { name: 'Vector monitor', standIn: true }, filter: { name: 'Segment LCD', standIn: true }, eq: { name: 'Blue CRT', standIn: true },
    drive: { name: 'Grey-green scope', standIn: true }, shriek: { name: '80s workstation', standIn: false }, echo: { name: 'Sepia print', standIn: true },
    sluice: { name: 'Game Boy Color LCD', standIn: false }, scape: { name: 'Dusk glass', standIn: true } };
  function styleOf(id) {
    if (id === 'lane' || id === 'lane2' || id === 'out' || !id) return { id: id || 'out', body: v('panel'), line: v(id === 'lane2' ? 'hint' : id === 'lane' ? 't2' : 'ink'), text: v('ink'), edge: v('ink'), plain: true, screen: null };
    if (STYLES.indexOf(id) < 0) throw new Error('FxCables: unknown module colour "' + id + '"');
    var ink = v('fxs-' + id + '-ink');
    return { id: id, body: v('panel'), line: v('fxs-' + id + '-cable'), text: v('ink'), edge: v('ink'), plain: false, screen: { ground: v('fxs-' + id + '-ground'), ink: ink, mid: v('fxs-' + id + '-mid') } };
  }
  /* ONE WEIGHT SCALE in all three views (the lab's): core width 0.8-5.6 px, opacity 0.22-1 */
  function wW(w) { return 0.8 + 4.8 * Math.min(1, w); }
  function wA(w) { return 0.22 + 0.78 * Math.min(1, w); }
  var CASE = 3, HALO = 7, DASH = '5 3.5';   // CASE: the ink casing, 1.5 px each side of the core

  /* A CABLE in the shared encoding, as one SVG group on path d:
     - the waiting ghost (held): a dotted caution-tx line, drawn even at zero weight;
     - the loop halo (loop): a wide caution underlay at opacity-halo, never a blur;
     - the forward part (wf): a CORD, its core the SOURCE module's cable colour (fxs-<module>-cable), its casing the house ink (1.5 px each side);
     - the backward part (wb, one sample late): the same cord, dashed (casing and core share the dash).
     A part's group opacity follows its weight, so casing and core fade as one. */
  function cable(g, d, e, sty) {
    var grp = mk('g', { 'class': 'fx-cable', 'data-k': e.k }, g), w = e.wf + e.wb;
    if (e.held) mk('path', { d: d, 'class': 'fx-wait' }, grp);
    if (w < MIN) return grp;
    if (e.loop) mk('path', { d: d, 'class': 'fx-halo', 'stroke-width': (wW(w) + HALO).toFixed(2) }, grp);
    function part(x, back) {
      if (x < MIN) return;
      var pg = mk('g', { 'class': back ? 'fx-back' : 'fx-fwd', opacity: wA(x).toFixed(3) }, grp), da = back ? DASH : null;
      if (!sty.plain) mk('path', { d: d, fill: 'none', stroke: sty.edge, 'stroke-width': (wW(x) + CASE).toFixed(2), 'stroke-dasharray': da, 'stroke-linecap': back ? 'butt' : 'round' }, pg);
      mk('path', { d: d, fill: 'none', stroke: sty.line, 'stroke-width': wW(x).toFixed(2), 'stroke-dasharray': da, 'stroke-linecap': back ? 'butt' : 'round', 'class': 'fx-core' }, pg);
    }
    part(e.wf, false); part(e.wb, true);
    return grp;
  }
  /* the z^-1 chip on a backward part: caution-tx on caution-bg (5.80 / 8.16:1), the words for the dash */
  function zchip(g, x, y) {
    var c = mk('g', { 'class': 'fx-z' }, g);
    mk('rect', { x: x - 11, y: y - 6, width: 22, height: 12, rx: 3, 'class': 'fx-zbox' }, c);
    txt(c, x, y + .5, 'z⁻¹', { 'class': 'fx-ztx', 'data-on': 'caution-bg' });
    return c;
  }
  /* a SCREEN INSET: the module's small display, drawn in its own screen tokens (ground fill, ink and mid for the picture).
     The pictures are PLACEHOLDERS (one tiny drawing per module, none is the real skin art); the colours are the tokens.
     The glass has a 1 px house-ink edge: dark screens (Shriek, Scape, EQ) are 1.1-1.6:1 against the dark card, so without
     it they would melt into it. Sluice's fourth Game Boy green is not a token, so its pixel grid leaves those cells as ground. */
  function pathN(spec, x, y, w, h) { return spec.replace(/(-?\d*\.?\d+)[ ,]+(-?\d*\.?\d+)/g, function (_, a, b) { return f2(x + a * w) + ' ' + f2(y + b * h); }); }
  function screen(g, id, x, y, w, h, r) {
    var S = styleOf(id).screen; if (!S) return null;
    var gr = mk('g', { 'class': 'fx-screen', 'data-s': id }, g), sw = Math.max(1, h / 20);
    mk('rect', { x: f2(x), y: f2(y), width: f2(w), height: f2(h), rx: r, fill: S.ground, stroke: v('ink'), 'stroke-width': 1 }, gr);
    function P(spec, col, wd) { mk('path', { d: pathN(spec, x, y, w, h), fill: 'none', stroke: col, 'stroke-width': f2(wd || sw), 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }, gr); }
    function R(u, vv, uw, vh, col) { mk('rect', { x: f2(x + u * w), y: f2(y + vv * h), width: f2(uw * w), height: f2(vh * h), fill: col }, gr); }
    var k, a, b;
    switch (id) {
      case 'bulwark': P('M.08 .28 L.92 .28', S.mid, sw * .7); P('M.08 .82 L.42 .52 L.92 .4', S.ink); break;
      case 'filter': P('M.08 .3 L.5 .3 C.68 .3 .72 .82 .92 .86', S.ink); break;
      case 'eq': P('M.06 .62 C.2 .62 .22 .24 .36 .3 C.5 .36 .5 .84 .64 .76 C.78 .68 .8 .44 .94 .5', S.ink); break;
      case 'drive': P('M.08 .85 C.38 .85 .5 .15 .92 .15', S.ink); P('M.08 .85 L.92 .15', S.mid, sw * .6); break;
      case 'shriek': for (k = 0; k < 6; k++) { b = .2 + .12 * k; R(.1 + k * .14, .86 - b, .1, b, S.ink); } break;
      case 'echo': for (k = 0; k < 5; k++) { b = .7 * Math.pow(.62, k); R(.12 + k * .17, .86 - b, .07, b, k ? S.mid : S.ink); } break;
      case 'sluice': for (a = 0; a < 6; a++) for (b = 0; b < 3; b++) { k = (a * 2 + b * 3 + a * b) % 3; if (k < 2) R(.08 + a * .14, .12 + b * .27, .12, .22, k ? S.mid : S.ink); } break;
      case 'scape': P('M.2 .7 L.4 .3 L.62 .6 L.82 .25', S.mid);
        [[.2, .7], [.4, .3], [.62, .6], [.82, .25]].forEach(function (q) { mk('circle', { cx: f2(x + q[0] * w), cy: f2(y + q[1] * h), r: f2(Math.max(1.4, h / 11)), fill: S.ink }, gr); }); break;
    }
    return gr;
  }
  /* a module's mark, drawn as a CARD: panel cream, ink edge (1.5, or 1 dashed when ASLEEP), its name in ink, and the module's
     screen inset. layout 'box' (the rack, a narrow matrix header): the name over a screen; default (a matrix header): the
     name at the left, a small screen at the right. A lane or OUT (plain) has the name alone. The card and the name keep full
     contrast asleep (a faded label would fall under 4.5:1); the dash and a hollow LED are the sleep. */
  function inW(w) { return Math.max(8, Math.min(18, w * .26)); }   // the right-hand screen's width in a default-layout chip
  function chip(g, x, y, w, h, node, label, layout) {
    var sty = styleOf(node.style), c = mk('g', { 'class': 'fx-node' + (node.asleep ? ' asleep' : ''), 'data-id': node.id }, g), x0 = x - w / 2, y0 = y - h / 2;
    mk('rect', { x: x0, y: y0, width: w, height: h, rx: 3, fill: sty.body, stroke: sty.edge, 'stroke-width': node.asleep ? 1 : 1.5, 'stroke-dasharray': node.asleep ? '2.5 2.5' : null }, c);
    if (layout === 'box' && !sty.plain) {
      var top = Math.round(Math.min(17, h * .42)), pad = w < 34 ? 3 : 5;   // the name's line, then the screen below it
      txt(c, x, y0 + top - 6.5, label, { fill: sty.text, 'data-on': 'panel' });
      screen(c, node.style, x0 + pad, y0 + top, w - 2 * pad, h - top - 5, 2);
    } else if (sty.plain) txt(c, x, y + .5, label, { fill: sty.text, 'data-on': 'panel' });
    else {
      var aw = inW(w);
      txt(c, x0 + 4, y + .5, label, { 'class': 'fx-l', fill: sty.text, 'data-on': 'panel' });
      screen(c, node.style, x0 + w - aw - 3, y0 + 4, aw, h - 8, 1.5);
    }
    return c;
  }
  function label(n, room) { var full = n.name + (n.tag ? ' ' + n.tag : ''), sh = (n.short || n.name.slice(0, 2)) + (n.tag ? n.tag : ''); return room >= full.length * 5.6 + 8 ? full : sh; }

  /* ---------- the state every view reads (the consumer provides it, from the engine) ----------
     { lanes: [{ id, name, short, style: 'lane' | 'lane2' }], nodes: [{ id, name, short, style, asleep, cost?, tag? }] in house
       order, out: { id, name, short }, cables: [{ u, v, wf, wb, loop, held }], rules: { 'u>v': 'banned' | '<note>' } } */
  function index(s) { var m = {}; s.lanes.concat(s.nodes, [s.out]).forEach(function (n) { m[n.id] = n; }); return m; }
  function visible(c) { return c.wf + c.wb >= MIN || !!c.held; }
  function keyOf(s, W) {
    var k = W + '|';
    s.nodes.forEach(function (n) { k += n.id + (n.asleep ? 'z' : '') + (n.tag || '') + n.style + ','; });
    s.cables.forEach(function (c) { if (visible(c)) k += c.u + '>' + c.v + ':' + c.wf.toFixed(3) + '/' + c.wb.toFixed(3) + (c.loop ? 'L' : '') + (c.held ? 'H' : '') + ';'; });
    return k;
  }
  /* the words for the colours: live loops, waiting cables, sleeping modules (the status line and the accessible text) */
  function summary(s) {
    var ix = index(s), loops = [], held = [], live = 0, nm = function (id) { return ix[id] ? ix[id].name : id; };
    s.cables.forEach(function (c) { if (c.wf + c.wb >= MIN) live++; if (c.loop && c.wf + c.wb >= MIN) loops.push(nm(c.u) + ' → ' + nm(c.v)); if (c.held) held.push(nm(c.u) + ' → ' + nm(c.v)); });
    var asleep = s.nodes.filter(function (n) { return n.asleep; }).map(function (n) { return n.name; });
    return { live: live, loops: loops, held: held, asleep: asleep };
  }
  function status(el, s) {
    var q = summary(s), h = '';
    if (q.loops.length) h += '<span class="fx-flag" title="' + q.loops.join(', ') + '">LIVE LOOP ' + q.loops.length + '</span>';
    if (q.held.length) h += '<span class="fx-flag" title="' + q.held.join(', ') + '">WAITING ' + q.held.length + '</span>';
    h += '<span class="fx-ok">' + q.live + ' cables</span>';   // the count never shrinks; the words after it end in an ellipsis
    if (q.held.length) h += '<span>wait: ' + q.held.join(', ') + '</span>';
    else if (q.loops.length) h += '<span>loop: ' + q.loops.join(', ') + '</span>';
    if (q.asleep.length) h += '<span>asleep: ' + q.asleep.join(', ') + '</span>';
    el.innerHTML = h;
    return q;
  }
  function srList(el, s, view) {
    var ix = index(s), rows = s.cables.filter(visible).map(function (c) {
      return (ix[c.u] ? ix[c.u].name : c.u) + ' to ' + (ix[c.v] ? ix[c.v].name : c.v) + ' ' + Math.round((c.wf + c.wb) * 100) + '%' + (c.wb >= MIN ? ', one sample late' : '') + (c.loop ? ', in a live loop' : '') + (c.held ? ', waiting' : '');
    });
    el.textContent = view + ' cables: ' + (rows.length ? rows.join('; ') : 'none') + '.';
  }
  /* the frame the three views share: an svg, the status line, the screen-reader list */
  function frame(el, cls, name) {
    el.classList.add('hd-fx', cls); el.innerHTML = '';
    el.setAttribute('role', 'figure'); el.setAttribute('aria-label', name);
    var svg = mk('svg', { 'class': 'fx-svg', role: 'img', 'aria-label': name }); el.appendChild(svg);
    var st = document.createElement('div'); st.className = 'fx-status'; st.setAttribute('aria-hidden', 'true'); el.appendChild(st);
    var sr = document.createElement('div'); sr.className = 'hd-sr'; el.appendChild(sr);
    return { svg: svg, st: st, sr: sr };
  }
  function view(el, get, S, cls, name, paint) {
    var F = frame(el, cls, name), last = '', ro = null;
    var vw = { render: function (force) {
      var s = typeof get === 'function' ? get() : get.state(), W = Math.max(160, Math.floor(el.clientWidth || 0));
      if (!s) return;                                  // no state yet (the engine has published nothing): draw nothing
      var k = keyOf(s, W); if (k === last && !force) return; last = k;
      F.svg.innerHTML = ''; var Hh = paint(F.svg, s, W); F.svg.setAttribute('viewBox', '0 0 ' + W + ' ' + Hh); F.svg.setAttribute('height', Hh); F.svg.setAttribute('width', W);
      var q = status(F.st, s); srList(F.sr, s, name);
      F.svg.setAttribute('aria-label', name + ': ' + q.live + ' live cables' + (q.loops.length ? ', a live loop' : '') + (q.held.length ? ', ' + q.held.length + ' waiting' : '') + (q.asleep.length ? ', ' + q.asleep.length + ' asleep' : ''));
      vw.renders = (vw.renders || 0) + 1;
    }, renders: 0 };
    if (typeof ResizeObserver === 'function') { ro = new ResizeObserver(function () { vw.render(); }); ro.observe(el); }
    el.__fxview = vw;                                  // the tests read the render count here
    if (S) S.add(vw); else vw.render();
    return vw;
  }

  /* ---------- RACK: one row in house order, the lanes first and OUT last ----------
     A cable running left to right arcs ABOVE the row, right to left BELOW it; it leaves its source right of
     centre and lands left of centre (right of centre going left), so in- and outgoing arcs part at every box.
     Arcs are half-ellipses whose height grows with the square root of the span (long arcs do not tower). */
  function FxRack(el, get, S) {
    return view(el, get, S, 'hd-fxrack', 'FX rack', function (svg, s, W) {
      var cols = s.lanes.concat(s.nodes, [s.out]), n = cols.length, cw = W / n, ix = {}, Hh = clamp(Math.round(W / 5), 170, 200), mid = Hh / 2;
      cols.forEach(function (c, i) { ix[c.id] = i; });
      var X = function (id) { return (ix[id] + .5) * cw; }, port = .2 * cw, bw = Math.min(cw * .84, 76), bh = 52, room = mid - bh / 2 - 10;
      mk('line', { x1: 0, y1: mid, x2: W, y2: mid, 'class': 'fx-base' }, svg);
      var arcs = s.cables.filter(visible).map(function (c) {
        var up = ix[c.v] > ix[c.u], x0 = X(c.u) + port, x1 = X(c.v) + (up ? -port : port), span = Math.abs(ix[c.v] - ix[c.u]);
        return { c: c, up: up, x0: x0, x1: x1, ry: Math.min(room, (0.07 + 0.085 * Math.sqrt(span)) * Hh), y: mid + (up ? -bh / 2 : bh / 2) };
      }).sort(function (a, b) { return Math.abs(b.x1 - b.x0) - Math.abs(a.x1 - a.x0); });   // long arcs first: short ones lie on top
      var lay = mk('g', { 'class': 'fx-cables' }, svg), tops = mk('g', {}, svg);
      arcs.forEach(function (a) {
        var c = a.c, sty = styleOf((cols[ix[c.u]] || {}).style), rx = Math.abs(a.x1 - a.x0) / 2;
        var d = 'M' + f2(a.x0) + ' ' + f2(a.y) + 'A' + f2(rx) + ' ' + f2(a.ry) + ' 0 0 1 ' + f2(a.x1) + ' ' + f2(a.y);
        var grp = cable(lay, d, c, sty), w = c.wf + c.wb; if (w < MIN) return;
        var s2 = 2.5 + 2.5 * Math.min(1, w), dy = a.up ? -1.6 * s2 : 1.6 * s2;   // the arrowhead where it lands
        mk('path', { d: 'M' + f2(a.x1) + ' ' + f2(a.y) + 'L' + f2(a.x1 - s2 * .8) + ' ' + f2(a.y + dy) + 'L' + f2(a.x1 + s2 * .8) + ' ' + f2(a.y + dy) + 'Z', fill: sty.line, stroke: sty.plain ? null : sty.edge, 'stroke-width': sty.plain ? null : 1, 'stroke-linejoin': 'round', opacity: wA(w).toFixed(3), 'class': 'fx-arrow' }, grp);
        if (c.wb >= MIN) zchip(tops, (a.x0 + a.x1) / 2, a.y + (a.up ? -a.ry : a.ry));
      });
      cols.forEach(function (c) { chip(svg, X(c.id), mid, bw, bh, c.style ? c : Object.assign({ style: 'out' }, c), label(c, bw), 'box'); });
      svg.appendChild(tops);
      txt(svg, 6, 9, 'left → right ↑', { 'class': 'fx-hint', 'data-on': 'well' }); txt(svg, 6, Hh - 8, 'right → left ↓', { 'class': 'fx-hint', 'data-on': 'well' });
      return Hh;
    });
  }

  /* ---------- MATRIX: rows are where a cable comes FROM, columns where it goes TO ----------
     A live cell is an ink edge on the well: the forward part a bar in the source's cable colour (edged in ink), its
     length the weight; the backward part a dashed bar along the foot; the weight printed in ink. A loop
     rings the cell in a caution halo; a waiting cable rings it dotted. A cable the rules forbid is hatched
     with a cross; a conditional one carries its note. */
  function FxMatrix(el, get, S) {
    return view(el, get, S, 'hd-fxmatrix', 'FX matrix', function (svg, s, W) {
      var rows = s.lanes.concat(s.nodes), cols = s.nodes.concat([s.out]), wide = W >= 560, hx = wide ? 78 : 34, hy = wide ? 28 : 40;   // narrow: the column headers stack name over screen, so they are taller
      var cw = (W - hx - 4) / cols.length, ch = Math.round(clamp(cw * .62, 22, 30)), Hh = hy + rows.length * ch + 4, gap = 3, map = {}, rules = s.rules || {};
      s.cables.forEach(function (c) { map[c.u + '>' + c.v] = c; });
      cols.forEach(function (c, j) { var cwid = Math.min(cw - gap, 76), nd = c.style ? c : Object.assign({ style: 'out' }, c);
        chip(svg, hx + (j + .5) * cw, hy / 2, cwid, wide ? 22 : hy - 6, nd, label(c, wide && c.style ? cwid - inW(cwid) - 4 : cwid), wide ? null : 'box'); });
      rows.forEach(function (r, i) { chip(svg, hx / 2 + 1, hy + (i + .5) * ch, hx - 4, ch - gap - 2, r, label(r, r.style && r.style.indexOf('lane') < 0 ? hx - 4 - inW(hx - 4) - 4 : hx - 4)); });
      txt(svg, 4, hy / 2, '↓→', { 'class': 'fx-hint', 'data-on': 'well' });
      var halos = mk('g', {}, svg);
      rows.forEach(function (r, i) { cols.forEach(function (c, j) {
        var x = hx + j * cw + gap / 2, y = hy + i * ch + gap / 2, w = cw - gap, h = ch - gap, k = r.id + '>' + c.id, e = map[k];
        if (r.id === c.id) { mk('rect', { x: x, y: y, width: w, height: h, rx: 3, fill: v('hair'), opacity: .6 }, svg); return; }
        var cell = mk('g', { 'class': 'fx-cell', 'data-k': k }, svg), tot = e ? e.wf + e.wb : 0, sty = styleOf(r.style);
        mk('rect', { x: x, y: y, width: w, height: h, rx: 3, fill: 'none', stroke: v('hair'), 'stroke-width': 1 }, cell);
        if (rules[k]) {
          var cp = 'fxc' + el.dataset.fxid + i + '_' + j, cl = mk('clipPath', { id: cp }, cell); mk('rect', { x: x, y: y, width: w, height: h, rx: 3 }, cl);
          var hg = mk('g', { 'clip-path': 'url(#' + cp + ')', stroke: v('div'), 'stroke-width': .8 }, cell);
          for (var q = -h; q < w; q += 4) mk('line', { x1: x + q, y1: y + h, x2: x + q + h, y2: y }, hg);
          txt(cell, x + w / 2, y + h / 2, rules[k] === 'banned' ? '✕' : rules[k], { 'class': 'fx-sm fx-rule', fill: v(rules[k] === 'banned' ? 't2' : 'hint'), 'data-on': 'well' });
        }
        if (!e || (tot < MIN && !e.held)) return;
        var g2 = mk('g', { 'class': 'fx-cable', 'data-k': k }, cell);
        if (tot >= MIN) {
          if (e.loop) mk('rect', { x: x - 1, y: y - 1, width: w + 2, height: h + 2, rx: 4, 'class': 'fx-halo', 'stroke-width': 4, 'data-k': k }, halos);   // under every cell, so no neighbour covers it
          mk('rect', { x: x, y: y, width: w, height: h, rx: 3, fill: 'none', stroke: v('ink'), 'stroke-width': 1 }, g2);   // a live cell: an ink edge on the well
          var bx = x + 3, bwid = w - 6, eg = sty.plain ? null : sty.edge;
          if (e.wf >= MIN) mk('rect', { x: bx, y: f2(y + h * .54), width: f2(Math.max(1.5, bwid * Math.min(1, e.wf))), height: f2(h * .24), rx: 1, fill: sty.line, stroke: eg, 'stroke-width': eg ? 1 : null, 'class': 'fx-fwd' }, g2);
          if (e.wb >= MIN) { var bg = mk('g', { 'class': 'fx-back' }, g2), x2 = f2(bx + Math.max(3, bwid * Math.min(1, e.wb))), yb = f2(y + h * .88);
            if (eg) mk('line', { x1: bx, y1: yb, x2: x2, y2: yb, stroke: eg, 'stroke-width': 4.5, 'stroke-dasharray': '3 2' }, bg);
            mk('line', { x1: bx, y1: yb, x2: x2, y2: yb, stroke: sty.line, 'stroke-width': 2.5, 'stroke-dasharray': '3 2' }, bg); }
          if (w >= 22) txt(g2, x + w / 2, y + h * .3, tot >= .995 ? '1' : tot.toFixed(2).replace(/^0/, ''), { 'class': 'fx-sm', fill: v('ink'), 'data-on': 'well' });
        }
        if (e.held) mk('rect', { x: x + .5, y: y + .5, width: w - 1, height: h - 1, rx: 3, 'class': 'fx-wait' }, g2);
      }); });
      return Hh;
    });
  }

  /* ---------- PANEL: a case. An IN panel (the lanes' jacks), the modules in house order, an OUT panel ----------
     Each module face is a cream panel card with an ink edge (dashed asleep): its name, its screen inset, an LED (lit awake,
     hollow asleep), captions and jacks, all in ink. OUT jack upper, IN jack lower; a cord runs from an OUT jack down and
     across to an IN jack, sagging with its span. Captions sit ABOVE their jacks and cords leave jacks downward,
     so a cord never covers the caption of the jack it plugs into; jacks are drawn LAST. */
  function FxPanel(el, get, S) {
    return view(el, get, S, 'hd-fxpanel', 'FX panel', function (svg, s, W) {
      var IN = { id: 'IN', name: 'In', short: 'In', style: 'out' }, OUT = Object.assign({}, s.out, { style: 'out' });
      var mods = [IN].concat(s.nodes, [OUT]), n = mods.length, pw = W / n, Hh = 236, top = 6, bot = Hh - 6, fh = bot - top;
      var col = {}; mods.forEach(function (m, j) { col[m.id] = j; });
      s.lanes.forEach(function (l) { col[l.id] = 0; });
      var JY = { out: top + fh * .36, inp: top + fh * .64, lane0: top + fh * .36, lane1: top + fh * .58, sink: top + fh * .58 }, jr = Math.min(4.5, pw * .15);
      function jack(id, dir) {
        var x = (col[id] + .5) * pw;
        if (col[id] === 0) return [x, s.lanes[0] && id === s.lanes[0].id ? JY.lane0 : JY.lane1];
        if (id === s.out.id) return [x, JY.sink];
        return [x, dir === 'out' ? JY.out : JY.inp];
      }
      var px = pw >= 46 ? 'name' : 'short';
      mods.forEach(function (m, j) {
        var x = (j + .5) * pw, sty = styleOf(m.style), face = mk('g', { 'class': 'fx-node' + (m.asleep ? ' asleep' : ''), 'data-id': m.id }, svg);
        mk('rect', { x: x - pw / 2 + 1.5, y: top, width: pw - 3, height: fh, rx: 3, fill: sty.body, stroke: sty.edge, 'stroke-width': 1.5, 'stroke-dasharray': m.asleep ? '2.5 2.5' : null }, face);
        txt(face, x, top + 14, px === 'name' ? label(m, pw - 6) : (m.short || m.name.slice(0, 2)) + (m.tag || ''), { fill: sty.text, 'data-on': 'panel' });
        if (!sty.plain) {
          var pad = pw < 34 ? 3 : 5, ly = top + 59;   // the screen spans 22 to 52; the LED sits under it, clear of the 'out' caption (centre 70)
          screen(face, m.style, x - pw / 2 + 1.5 + pad, top + 22, pw - 3 - 2 * pad, 30, 2);
          // the LED in ink: lit when awake (a wider underlay disc for the glow), a hollow ring asleep
          if (!m.asleep) mk('circle', { cx: x, cy: ly, r: 5, fill: sty.text, opacity: v('opacity-halo') }, face);
          mk('circle', { cx: x, cy: ly, r: 2.4, fill: m.asleep ? 'none' : sty.text, stroke: m.asleep ? sty.text : 'none', 'stroke-width': 1, 'class': 'fx-led' + (m.asleep ? ' off' : '') }, face);
          if (m.cost !== undefined) txt(face, x, bot - 9, m.cost + 'pt', { 'class': 'fx-sm', fill: sty.text, 'data-on': 'panel' });
        }
      });
      var lay = mk('g', { 'class': 'fx-cables' }, svg), tops = mk('g', {}, svg), plugged = {};
      var cords = s.cables.filter(visible).map(function (c) {
        var a = jack(c.u, 'out'), b = jack(c.v, 'in'), span = Math.abs(b[0] - a[0]) / pw;
        return { c: c, a: a, b: b, sag: Math.min(.3, .05 + .045 * span) * Hh };
      }).sort(function (p, q) { return q.sag - p.sag; });
      var nodeOf = index(s);
      cords.forEach(function (o) {
        var a = o.a, b = o.b, low = Math.max(a[1], b[1]) + o.sag, c = o.c;
        var d = 'M' + f2(a[0]) + ' ' + f2(a[1]) + 'C' + f2(a[0] + (b[0] - a[0]) * .15) + ' ' + f2(low) + ' ' + f2(b[0] - (b[0] - a[0]) * .15) + ' ' + f2(low) + ' ' + f2(b[0]) + ' ' + f2(b[1]);
        cable(lay, d, c, styleOf((nodeOf[c.u] || {}).style));
        if (c.wb >= MIN) zchip(tops, (a[0] + b[0]) / 2, low - o.sag * .25);
        if (c.wf + c.wb >= MIN) { plugged[c.u + ':out'] = 1; plugged[c.v + ':in'] = 1; }
      });
      var jacks = s.lanes.map(function (l) { return [l, 'out', l.name]; }).concat([[s.out, 'in', 'in']]);
      s.nodes.forEach(function (m) { jacks.push([m, 'out', 'out'], [m, 'in', 'in']); });
      jacks.forEach(function (J) {
        var m = J[0], d = J[1], p = jack(m.id, d), host = col[m.id] === 0 ? IN : m.id === s.out.id ? OUT : m, hs = styleOf(host.style), ms = styleOf(m.style || 'out'), on = plugged[m.id + ':' + d];
        var jg = mk('g', { 'class': 'fx-jack', 'data-j': m.id + ':' + d }, svg);
        mk('circle', { cx: p[0], cy: p[1], r: jr, fill: hs.body, stroke: hs.plain ? v('ink') : hs.text, 'stroke-width': 1.6 }, jg);
        if (on) mk('circle', { cx: p[0], cy: p[1], r: jr * .45, fill: hs.plain ? ms.line : hs.text, 'class': 'fx-plug' }, jg);
        if (pw >= 26) txt(jg, p[0], p[1] - jr - 6, J[2], { 'class': 'fx-sm', fill: hs.text, 'data-on': 'panel' });
      });
      svg.appendChild(tops);
      return Hh;
    });
  }

  /* ---------- the encoding's LEGEND (the lab's cable legend, drawn with the same functions) ---------- */
  function legend(el, opts) {
    opts = opts || {};
    el.classList.add('hd-fxleg'); el.innerHTML = '';
    var rows = document.createElement('div'); rows.className = 'fl-rows fl-well'; el.appendChild(rows);
    function row(draw, html, aria) {
      var r = document.createElement('div'); r.className = 'fl-row';
      var svg = mk('svg', { viewBox: '0 0 96 28', role: 'img', 'aria-label': aria, 'class': 'fx-svg' }); r.appendChild(svg); draw(svg);
      var s = document.createElement('span'); s.innerHTML = html; r.appendChild(s); rows.appendChild(r);
    }
    var sty = styleOf(opts.style || 'filter'), D = 'M6 14H90', base = { k: 'demo', wf: 0, wb: 0 };
    row(function (g) { [.08, .35, .7, 1].forEach(function (w, i) { cable(g, 'M' + (6 + i * 22) + ' ' + (22 - i * 5) + 'h18', Object.assign({}, base, { wf: w }), sty); }); }, '<b>Weight</b>: thicker and more opaque, one scale in all views', 'Four cables of rising weight');
    row(function (g) { cable(g, D, Object.assign({}, base, { wf: .8 }), sty); }, '<b>Forward</b>: a cord, the source module\'s cable colour edged in house ink', 'A forward cable');
    row(function (g) { cable(g, D, Object.assign({}, base, { wb: .8 }), sty); zchip(g, 48, 14); }, '<b>Backward</b>, one sample late: dashed, marked z⁻¹', 'A backward cable, dashed, marked z to the minus one');
    row(function (g) { cable(g, D, Object.assign({}, base, { wf: .6, loop: true }), sty); }, '<b>Live loop</b>: a caution halo; the status line names it', 'A cable in a live loop, with a halo');
    row(function (g) { cable(g, D, Object.assign({}, base, { held: 'loop' }), sty); }, '<b>Waiting</b>: dotted, caution ink, at any weight', 'A waiting cable, dotted');
    row(function (g) { cable(g, 'M6 9H90', Object.assign({}, base, { wf: .8 }), styleOf('lane')); cable(g, 'M6 20H90', Object.assign({}, base, { wf: .8 }), styleOf('lane2')); }, '<b>Lanes</b> Body (t2) and Sub (hint): plain, no edge', 'Two lane cables, Body and Sub');
    row(function (g) { chip(g, 26, 14, 40, 20, { id: 'a', name: 'Fl', style: sty.id }, 'Fl'); chip(g, 72, 14, 40, 20, { id: 'b', name: 'Dy', style: sty.id, asleep: true }, 'Dy'); }, '<b>Awake</b> and <b>asleep</b> (dashed edge): a cream card, its screen kept', 'An awake module and an asleep module');
    row(function (g) {
      var cl = mk('clipPath', { id: 'fxleg-hatch' + (el.dataset.fxid || '') }, g); mk('rect', { x: 34, y: 3, width: 28, height: 22, rx: 3 }, cl);
      mk('rect', { x: 34, y: 3, width: 28, height: 22, rx: 3, fill: 'none', stroke: v('hair') }, g);
      var hg = mk('g', { 'clip-path': 'url(#fxleg-hatch' + (el.dataset.fxid || '') + ')', stroke: v('div'), 'stroke-width': .8 }, g);
      for (var q = 12; q < 64; q += 4) mk('line', { x1: q, y1: 25, x2: q + 22, y2: 3 }, hg);
      txt(g, 48, 14.5, '✕', { 'class': 'fx-sm', fill: v('t2'), 'data-on': 'well' });
    }, '<b>Unavailable</b> (matrix): hatched, a cross; a conditional cable shows its note', 'A hatched, unavailable matrix cell');
    if (opts.palette === false) return;
    /* THE SCREENS, each module's ink on its ground and its ground's lightness measured live in the current theme */
    var pal = document.createElement('div'); pal.className = 'fl-rows'; el.appendChild(pal);
    var mods = opts.modules || [];
    mods.forEach(function (m) {
      var r = document.createElement('div'); r.className = 'fl-row fl-pal';
      var svg = mk('svg', { viewBox: '0 0 96 28', role: 'img', 'aria-label': m.name + ': screen ' + SCREENS[m.style].name + (SCREENS[m.style].standIn ? ' (stand-in)' : ''), 'class': 'fx-svg' }); r.appendChild(svg);
      cable(svg, 'M52 14H92', { k: m.id, wf: 1, wb: 0 }, styleOf(m.style)); chip(svg, 24, 14, 42, 20, m, m.short);
      var s = document.createElement('span'); s.className = 'fl-ct'; s.dataset.style = m.style; r.appendChild(s); pal.appendChild(r);
      s.innerHTML = '<b>' + m.name + '</b> · ' + SCREENS[m.style].name + (SCREENS[m.style].standIn ? ' <i>stand-in</i>' : ' <i>real</i>') + '<br><span class="fl-n"></span>';
    });
    function measure() {
      pal.querySelectorAll('.fl-ct').forEach(function (s) {
        var st = styleOf(s.dataset.style), c = H.FxCables.contrast(el, st);
        s.querySelector('.fl-n').textContent = 'ground L* ' + c.L.toFixed(1) + ' · ink ' + c.screen.toFixed(2) + ':1 on it · cord core ' + c.line.toFixed(2) + ', cased ' + c.cord.toFixed(2) + ':1';
      });
    }
    measure();
    return { measure: measure };
  }
  /* a style's numbers in the current theme (WCAG 2): its core (the cable colour) and its casing against a ground (the views' WELL by
     default), its text on its card, its screen ink on its screen ground, and that ground's CIE L* (the screens' spacing is in L*) */
  function rgb(el, expr) { var e = document.createElement('i'); e.style.color = expr; e.style.display = 'none'; el.appendChild(e); var m = getComputedStyle(e).color.match(/[\d.]+/g).map(Number); e.remove(); return m; }
  function lum(c) { var f = function (x) { x /= 255; return x <= .04045 ? x / 12.92 : Math.pow((x + .055) / 1.055, 2.4); }; return .2126 * f(c[0]) + .7152 * f(c[1]) + .0722 * f(c[2]); }
  function ratio(a, b) { var x = lum(a), y = lum(b); return (Math.max(x, y) + .05) / (Math.min(x, y) + .05); }
  function contrast(el, st, ground) {
    var gr = rgb(el, ground || v('well')), li = rgb(el, st.line), bo = rgb(el, st.body), tx = rgb(el, st.text);
    var ed = st.plain ? null : rgb(el, st.edge), sg = st.screen ? rgb(el, st.screen.ground) : null, Y = lum(sg || bo), Ls = Y > 216 / 24389 ? 116 * Math.cbrt(Y) - 16 : Y * 24389 / 27;
    return { line: ratio(li, gr), edge: ed ? ratio(ed, gr) : 0, body: ratio(bo, gr), text: ratio(tx, bo), L: Ls, screen: sg ? ratio(rgb(el, st.screen.ink), sg) : 0, cord: Math.max(ratio(li, gr), ed ? ratio(ed, gr) : 0) };
  }

  /* =====================================================================
     FxSample: the previews' SAMPLE cable engine (seeded nothing: it is deterministic arithmetic on the pad).
     NOT the engine. A small reading of the lab's round 3 so the views have honest data to draw:
     - the TARGET in closed form (the lab's targetCables, BLEND): one order per pad point by the weighted mean
       rank (absent modules ranked by house order), each module's wet gain its presence; a source entering the
       junction before chain position i reaches position j through every dry in between;
     - each cable GLIDES through two cascaded one-poles of tau/2 (seconds, ADR-009), so a glide starts flat;
     - TRANSIENT LOOPS, two strategies: (a) a cable that would close a cycle of forward cables emerges at once
       on its BACKWARD part (one sample late) and returns forward when the cycle clears; (b) it WAITS at zero
       until the cable it would loop with has retreated. The lab's bypass, normaliser and loop bound are not
       modelled (they change the sound, not what the views draw);
     - a module with nothing coming in is ASLEEP.
     ===================================================================== */
  var TYPES = ['DYN', 'EQ', 'FLT', 'DRV', 'SAT', 'NET', 'DLY', 'REV'];
  /* each module's `style` is its key into the screen tokens, fxs-<module>-ground|ink|mid (ADR-273) */
  var NODES = [
    { id: 'DYN', name: 'Bulwark', short: 'Dy', style: 'bulwark', cost: 1 }, { id: 'EQ', name: 'EQ', short: 'Eq', style: 'eq', cost: 1 },
    { id: 'FLT', name: 'Filter', short: 'Fl', style: 'filter', cost: 1 }, { id: 'DRV', name: 'Drive', short: 'Dr', style: 'drive', cost: 1 },
    { id: 'SAT', name: 'Shriek', short: 'Sa', style: 'shriek', cost: 2 }, { id: 'NET', name: 'Sluice', short: 'Nw', style: 'sluice', cost: 4 },
    { id: 'DLY', name: 'ECHO', short: 'Dl', style: 'echo', cost: 2 }, { id: 'REV', name: 'Scape', short: 'Rv', style: 'scape', cost: 3 }];
  /* four SAMPLE corner chains: A and B reverse Filter and Drive (a two-node loop when they cross), C and D
     move ECHO and Scape round each other (a loop through a delay line) */
  var CHAINS = [{ chain: ['FLT', 'DRV', 'DLY', 'REV'], sub: 'OUT' }, { chain: ['DRV', 'FLT', 'SAT', 'DLY'], sub: 'DRV' },
    { chain: ['DYN', 'EQ', 'FLT', 'REV', 'DLY'], sub: 'OUT' }, { chain: ['EQ', 'DRV', 'DLY', 'NET', 'REV'], sub: 'EQ' }];
  function FxSample(opts) {
    opts = opts || {};
    var me = this;
    me.nodes = (opts.nodes || NODES).map(function (n) { return Object.assign({}, n); });
    me.types = me.nodes.map(function (n) { return n.id; });
    me.lanes = [{ id: 'BODY', name: 'Body', short: 'Bo', style: 'lane' }, { id: 'SUB', name: 'Sub', short: 'Su', style: 'lane2' }];
    me.out = { id: 'OUT', name: 'Out', short: 'Out' };
    me.rules = opts.rules || { 'REV>NET': 'banned', 'REV>DYN': 'E1' };   // the lab's V: Scape -> Sluice never; Scape -> Bulwark only as E1
    me.corners = opts.corners || CHAINS;
    me.tau = opts.tau || 1; me.strategy = opts.strategy || 'a'; me.gamma = opts.gamma === undefined ? 2 : opts.gamma;
    var src = ['BODY', 'SUB'].concat(me.types), dst = me.types.concat(['OUT']);
    me.cab = []; src.forEach(function (u) { dst.forEach(function (w) { if (u !== w) me.cab.push({ u: u, v: w, k: u + '>' + w }); }); });
    me.ix = {}; me.cab.forEach(function (c, i) { me.ix[c.k] = i; });
    var N = me.cab.length; me.m1 = new Float64Array(N); me.m2 = new Float64Array(N); me.tgt = new Float64Array(N); me.back = new Uint8Array(N); me.held = new Uint8Array(N);
    me.house = {}; me.types.forEach(function (t, i) { me.house[t] = i; });
  }
  FxSample.prototype.target = function (w4) {
    var me = this, n = me.corners.length, w = w4.slice(0, n).map(function (x) { return Math.pow(Math.max(0, x), me.gamma); }), sw = w.reduce(function (a, b) { return a + b; }, 0) || 1, T = new Float64Array(me.cab.length);
    w = w.map(function (x) { return x / sw; });
    var p = {}, score = {};
    me.types.forEach(function (t) {
      var pr = 0, sc = 0;
      me.corners.forEach(function (c, k) {
        var at = c.chain.indexOf(t), rank = at >= 0 ? at : c.chain.filter(function (u) { return me.house[u] < me.house[t]; }).length - .5;
        if (at >= 0) pr += w[k]; sc += w[k] * rank;
      });
      p[t] = pr; score[t] = sc + me.house[t] * 1e-7;   // equal votes tie by house order, not by float noise
    });
    var order = me.types.filter(function (t) { return p[t] > 1e-9; }).sort(function (a, b) { return score[a] - score[b]; }), m = order.length;
    var vote = {}; me.corners.forEach(function (c, k) { vote[c.sub] = (vote[c.sub] || 0) + w[k]; });
    var sub = Object.keys(vote).sort(function (a, b) { return vote[b] - vote[a] || (a < b ? -1 : 1); })[0], si = order.indexOf(sub); if (si < 0) si = m;
    function add(u, x, i0) { var keep = x; for (var j = i0; j < m; j++) { T[me.ix[u + '>' + order[j]]] += keep; keep *= 1 - p[order[j]]; } T[me.ix[u + '>OUT']] += keep; }
    add('BODY', 1, 0); add('SUB', 1, si);
    for (var q = 0; q < m; q++) add(order[q], p[order[q]], q + 1);
    return T;
  };
  FxSample.prototype.land = function (w4) { this.step(w4, 1e9); };   // reduced motion: straight to the target, one still frame
  FxSample.prototype.settle = function (w4) { var T = this.target(w4); this.tgt = T; this.m1.set(T); this.m2.set(T); this.back.fill(0); this.held.fill(0); this.classify(); };
  /* one step of dt seconds toward the target at the pad's weights (dt 0 moves nothing; reduced motion uses land) */
  FxSample.prototype.step = function (w4, dt) {
    var me = this, T = me.target(w4), a = dt > 0 ? 1 - Math.exp(-dt / (me.tau / 2)) : 0; me.tgt = T;
    for (var i = 0; i < T.length; i++) {
      var rising = T[i] > me.m2[i] && me.m2[i] < MIN;
      if (me.strategy === 'b' && rising && me.closes(i, true)) { me.held[i] = 1; me.m1[i] = 0; me.m2[i] = 0; continue; }
      me.held[i] = 0;
      me.m1[i] += (T[i] - me.m1[i]) * a; me.m2[i] += (me.m1[i] - me.m2[i]) * a;
      if (Math.abs(me.m2[i] - T[i]) < 1e-5 && Math.abs(me.m1[i] - T[i]) < 1e-5) { me.m1[i] = me.m2[i] = T[i]; }   // a glide this close lands on its target
    }
    me.classify();
  };
  /* does cable i close a cycle through the live cables (all of them, or the forward ones only)? */
  FxSample.prototype.reach = function (from, to, use) {
    var me = this, seen = {}, stack = [from];
    while (stack.length) { var x = stack.pop(); if (x === to) return true; if (seen[x]) continue; seen[x] = 1;
      for (var i = 0; i < me.cab.length; i++) if (me.cab[i].u === x && use(i)) stack.push(me.cab[i].v); }
    return false;
  };
  FxSample.prototype.closes = function (i, any) {
    var me = this, c = me.cab[i]; if (c.v === 'OUT' || c.u === 'BODY' || c.u === 'SUB') return false;
    return me.reach(c.v, c.u, function (j) { return j !== i && me.m2[j] >= MIN && (any || !me.back[j]); });
  };
  FxSample.prototype.classify = function () {
    var me = this, N = me.cab.length, i;
    if (me.strategy === 'a') {
      for (i = 0; i < N; i++) if (me.m2[i] < MIN) me.back[i] = 0;
      for (i = 0; i < N; i++) if (me.back[i] && !me.closes(i, false)) me.back[i] = 0;      // the cycle cleared: forward again
      for (i = 0; i < N; i++) if (me.m2[i] >= MIN && !me.back[i] && me.tgt[i] > 0 && me.closes(i, false)) me.back[i] = 1;   // a rising cable that would close a cycle
    } else me.back.fill(0);
  };
  FxSample.prototype.state = function () {
    var me = this, cables = [], into = {};
    me.cab.forEach(function (c, i) {
      var w = me.m2[i]; if (w >= MIN) into[c.v] = 1;
      if (w < MIN && !me.held[i]) return;
      cables.push({ u: c.u, v: c.v, k: c.k, wf: me.back[i] ? 0 : w, wb: me.back[i] ? w : 0, loop: false, held: me.held[i] ? 'loop' : '' });
    });
    cables.forEach(function (c) { if (c.wf + c.wb >= MIN && c.v !== 'OUT' && me.reach(c.v, c.u, function (j) { return me.m2[j] >= MIN; })) c.loop = true; });
    return { lanes: me.lanes, out: me.out, rules: me.rules, cables: cables,
      nodes: me.nodes.map(function (n) { return Object.assign({}, n, { asleep: !into[n.id] }); }) };
  };

  var uid = 0;
  function tag(fn) { return function (el) { el.dataset.fxid = el.dataset.fxid || String(++uid); return fn.apply(null, arguments); }; }
  H.FxCables = { MIN: MIN, STYLES: STYLES, SCREENS: SCREENS, styleOf: styleOf, width: wW, alpha: wA, cable: cable, zchip: zchip, chip: chip, summary: summary, contrast: contrast, ratio: ratio, rgb: rgb, legend: tag(legend) };
  H.FxRack = tag(FxRack); H.FxMatrix = tag(FxMatrix); H.FxPanel = tag(FxPanel); H.FxSample = FxSample;
})(window.Horde);

/* =====================================================================
   THE PHASE RING AND THE GAIN-STAGING VIEW (B544). Two components, one part:
   KURAMOTO RING  Viz.kuramoto, RingXY, Synth.ring: the SCALPEL interface lab's phase ring (docs/design/scalpel-interface-lab.html:
                  paintRing, ringRBar, ringXYLayer; B294 the ring is the Coupling x Detune XY, B295 a click lands absolutely,
                  B323 the needle, the Coherence readout and the R bar share one ink). Replaces gui2's _drawPhaseOne.
   GAIN STAGING   GainStaging: the mixer lab's model (docs/design/mixer-lab.html, B225) as ADR-230 (3) and A1 rule it: a gain
                  control, a meter with a peak hold and a latching clip lamp, and the first-over marker at every stage.
   The meter is the Meter component's drawing (hd-mbar, hd-lamp, hd-tap), not a second one. Nothing here reads a clock or a
   random source: ballistics advance by the dt the caller passes, and every sample generator is seeded.
   ===================================================================== */
(function (H) {
  'use strict';
  var TAU = 2 * Math.PI, Viz = H.Viz, Synth = H.Synth;
  function clamp(x, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, x)); }
  function frac(x) { return x - Math.floor(x); }
  function tag(t, cls, txt) { var e = document.createElement(t); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; }

  /* ---------- KURAMOTO RING ----------
     s: { phase[] (cycles), blades?: [{ st1, w1, st2, w2 }] per member (cycles; a width of 0 is off), cross?: number[] per member,
          the RMS that member i+1's push adds to member i (null = cross-mod off), xy?: { x, y } the Detune and Coupling positions
          0..1 across the whole canvas, drag?: boolean }.
     Coherence is computed here from the phases, so the needle, the readout and the bar can only ever agree (B323). */
  Viz.kuramoto = function (cv, s) {
    var K = Viz.kit(cv), g = K.g, V = K.V, W = K.W / V, Hh = K.H / V, n = s.phase.length, i;
    g.setTransform(1, 0, 0, 1, 0, 0); K.tube(); K.scan();      // the wash and the scanlines are in device pixels
    g.setTransform(V, 0, 0, V, 0, 0);                          // everything below is in CSS pixels, as the lab draws it
    var cx = W / 2, cy = Hh / 2, Rr = Math.max(8, Math.min(W, Hh) / 2 - 8 - Math.min(9, n) * 2.2),    // ringGeom: room for the rim arcs
      mono = getComputedStyle(cv).getPropertyValue('--font-mono') || 'monospace', xy = s.xy, bl = s.blades || [];
    function P2(p) { return [cx + Math.cos(TAU * p) * Rr, cy - Math.sin(TAU * p) * Rr]; }
    /* the XY's own marks go first, under everything the ring draws (ringXYLayer) */
    if (xy) {
      g.font = '8.5px ' + mono; g.fillStyle = K.c('grid', .9); g.textAlign = 'left'; g.fillText('lock', 4, 11); g.fillText('splay', 4, Hh - 4);
      g.textAlign = 'right'; g.fillText('detune →', W - 4, Hh - 4);
      g.strokeStyle = K.c('grid', .5); g.lineWidth = 1; g.beginPath();                    // K = 0, the lock/splay boundary, as two edge ticks
      g.moveTo(0, Hh / 2 + .5); g.lineTo(7, Hh / 2 + .5); g.moveTo(W - 7, Hh / 2 + .5); g.lineTo(W, Hh / 2 + .5); g.stroke();
      var bx = xy.x * W, by = (1 - xy.y) * Hh, rail = 4;
      g.strokeStyle = K.c('value', .2); g.beginPath();                                    // the value's crosshair
      g.moveTo(Math.round(bx) + .5, 0); g.lineTo(Math.round(bx) + .5, Hh); g.moveTo(0, Math.round(by) + .5); g.lineTo(W, Math.round(by) + .5); g.stroke();
      g.fillStyle = K.c('value'); g.fillRect(bx - 1.5, Hh - rail - 3, 3, rail + 3); g.fillRect(0, by - 1.5, rail + 3, 3);   // rail ticks
      g.strokeStyle = K.c('value', .95); g.lineWidth = s.drag ? 2.4 : 1.8; g.beginPath(); g.arc(bx, by, 6.5, 0, TAU); g.stroke();   // a hollow puck, so a member under it shows
      g.fillStyle = K.c('value', .95); g.beginPath(); g.arc(bx, by, 1.4, 0, TAU); g.fill();
    }
    g.strokeStyle = K.c('grid', .3); g.lineWidth = 1; g.beginPath(); g.arc(cx, cy, Rr, 0, TAU); g.stroke();
    /* each member's own blade windows as a concentric arc outside the circle (member 1 innermost), so position spreads read as a fan */
    function arc(st, w, rad, b) {
      if (!(w >= .004)) return;
      g.strokeStyle = K.c('value', b === 2 ? .55 : .85); g.lineWidth = 1.8; if (b === 2) g.setLineDash([3, 2]);
      g.beginPath(); g.arc(cx, cy, rad, -TAU * (st + w), -TAU * st); g.stroke(); g.setLineDash([]);
    }
    for (i = 0; i < n && i < bl.length; i++) { var rad = Rr + 5 + i * 2.2; arc(bl[i].st1, bl[i].w1, rad, 1); arc(bl[i].st2, bl[i].w2, rad + .001, 2); }
    /* the mean field R e^i psi. ONE ink for the needle, the Coherence readout and the R bar, in every scheme (B323, the human
       2026-09-28): the readout's, because the number is what names it. */
    var sx = 0, sy = 0, pos = [];
    for (i = 0; i < n; i++) { sx += Math.cos(TAU * s.phase[i]); sy += Math.sin(TAU * s.phase[i]); pos.push(P2(s.phase[i])); }
    sx /= n; sy /= n;
    var r = Math.hypot(sx, sy), coh = K.c('meter');
    g.strokeStyle = coh; g.lineWidth = 2; g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + sx * Rr, cy - sy * Rr); g.stroke();
    /* cross-member modulation: member i+1 pushes member i (a ring). The arrow's weight is what the push ADDS to member i's output,
       not the knob; no effect is a dotted hairline. Each arrow bows toward the centre, so two coincident members (a locked
       swarm) still read as a small loop instead of vanishing. */
    if (s.cross && n > 1) for (i = 0; i < n; i++) {
      var a = pos[(i + 1) % n], b = pos[i], wgt = clamp(s.cross[i] / 1), none = s.cross[i] < 1e-6,
        mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2, dx = cx - mx, dy = cy - my, dl = Math.hypot(dx, dy) || 1,
        bow = 12 + .12 * Math.hypot(b[0] - a[0], b[1] - a[1]), qx = mx + dx / dl * bow, qy = my + dy / dl * bow;
      g.strokeStyle = K.c('meter', none ? .35 : .3 + .65 * wgt); g.fillStyle = g.strokeStyle; g.lineWidth = none ? .8 : .9 + 2.6 * wgt; g.setLineDash(none ? [1, 3] : []);
      g.beginPath(); g.moveTo(a[0], a[1]); g.quadraticCurveTo(qx, qy, b[0], b[1]); g.stroke(); g.setLineDash([]);
      if (none) continue;
      var tx = b[0] - qx, ty = b[1] - qy, tl = Math.hypot(tx, ty) || 1, ux = tx / tl, uy = ty / tl, ex = b[0] - ux * 6, ey = b[1] - uy * 6, hs = 3.5 + 2 * wgt;   // the head sits clear of the dot
      g.beginPath(); g.moveTo(ex, ey); g.lineTo(ex - ux * hs - uy * hs * .6, ey - uy * hs + ux * hs * .6); g.lineTo(ex - ux * hs + uy * hs * .6, ey - uy * hs - ux * hs * .6); g.closePath(); g.fill();
    }
    /* members: filled in the value phosphor while they are inside one of their blades */
    for (i = 0; i < n; i++) {
      var m = bl[i] || {}, inB = (m.w1 && frac(s.phase[i] - m.st1) < m.w1) || (m.w2 && frac(s.phase[i] - m.st2) < m.w2);
      g.fillStyle = K.c(inB ? 'value' : 'physics'); g.beginPath(); g.arc(pos[i][0], pos[i][1], 4.5, 0, TAU); g.fill();
    }
    g.font = '9.5px ' + mono; g.textAlign = 'right'; g.fillStyle = coh; g.fillText('Coherence ' + r.toFixed(2), W - 4, 11);
    g.fillStyle = K.c('grid', .9); g.fillText(n + ' members', W - 4, 23); g.textAlign = 'left';
    /* the R bar at the right edge, under the readout it repeats: a track, filled from the bottom to R in the Coherence ink, a hairline at 1/2 */
    var bw = 5, bxr = W - 9 - bw, top = 30, bot = Hh - 20, bh = bot - top, fh = Math.round(clamp(r) * bh);
    g.fillStyle = K.c('grid', .14); g.fillRect(bxr, top, bw, bh);
    g.strokeStyle = K.c('grid', .5); g.lineWidth = 1; g.strokeRect(bxr - .5, top - .5, bw + 1, bh + 1);
    g.fillStyle = coh; g.fillRect(bxr, bot - fh, bw, fh);
    g.strokeStyle = K.c('grid', .6); g.beginPath();                 // the 1/2 tick sits OUTSIDE the track, so the fill is never painted over
    g.moveTo(bxr - 4, top + bh / 2 + .5); g.lineTo(bxr - 1, top + bh / 2 + .5); g.moveTo(bxr + bw + 1, top + bh / 2 + .5); g.lineTo(bxr + bw + 4, top + bh / 2 + .5); g.stroke();
    g.setTransform(1, 0, 0, 1, 0, 0);
    cv.__ring = { r: r, ink: coh };   // what a check reads back: the one ink and the one R
    return r;
  };

  /* ---------- THE RING'S GESTURE (the lab's absGrab / absStep, B295) ----------
     st: { x, y, sx } positions 0..1, written in place; x = Detune across the canvas, y = Coupling up it, sx = the third axis
     (Cross-mod). A click LANDS x and y at the clicked point and the drag follows the pointer; each is `hand + off`, clamped, where
     off absorbs the motion that must not carry the value: alt (fine) puts 3/4 of each step into off, so the value moves 1/4 as
     far, and keeps it on release; shift hands the horizontal to sx (relative, 1:1, or 1/4 with alt) and holds x, so a shift-grab
     lands nothing. A clamped accumulator would drift from the hand when the pointer came back; hand + off cannot.
     on(st, { drag, shift, hover }) fires after every change. Arrows move x and y (alt fine), shift + left/right moves sx. */
  H.RingXY = function (cv, st, on) {
    var d = null;
    function box() { var r = cv.getBoundingClientRect(); return { l: r.left, t: r.top, w: Math.max(1, r.width), h: Math.max(1, r.height) }; }
    function say(hover) { if (on) on(st, { drag: !!d, shift: !!(d && d.shift), hover: !!hover }); }
    cv.style.touchAction = 'none'; cv.style.cursor = 'crosshair'; cv.tabIndex = 0; cv.classList.add('hd-ring');
    cv.addEventListener('pointerdown', function (e) {
      if (e.button !== 0) return;
      var b = box(), hx = (e.clientX - b.l) / b.w, hy = 1 - (e.clientY - b.t) / b.h;
      d = { px: e.clientX, py: e.clientY, hx: hx, hy: hy, w: b.w, h: b.h, ox: 0, oy: 0, shift: e.shiftKey };
      if (e.shiftKey) { d.ox = st.x - hx; d.oy = st.y - hy; } else { st.x = clamp(hx); st.y = clamp(hy); }
      try { cv.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
      say(false);
    });
    cv.addEventListener('pointermove', function (e) {
      if (!d) { say(true); return; }
      var dx = (e.clientX - d.px) / d.w, dy = -(e.clientY - d.py) / d.h;
      d.px = e.clientX; d.py = e.clientY; d.shift = e.shiftKey;
      if (!dx && !dy) return;                                  // no motion, no write: nothing jumps
      d.hx += dx; d.hy += dy;
      if (e.shiftKey) { d.ox -= dx; st.sx = clamp(st.sx + dx * (e.altKey ? .25 : 1)); }
      else if (e.altKey) d.ox -= .75 * dx;
      if (e.altKey) d.oy -= .75 * dy;
      st.x = clamp(d.hx + d.ox); st.y = clamp(d.hy + d.oy);
      say(false);
    });
    function end(e) { if (e && e.pointerId !== undefined && cv.hasPointerCapture && cv.hasPointerCapture(e.pointerId)) cv.releasePointerCapture(e.pointerId); if (d) { d = null; say(false); } }
    cv.addEventListener('pointerup', end); cv.addEventListener('pointercancel', end); cv.addEventListener('lostpointercapture', end);
    cv.addEventListener('pointerleave', function () { if (!d && on) on(st, { drag: false, shift: false, hover: false }); });
    cv.addEventListener('keydown', function (e) {
      var s = e.altKey ? .0025 : .01, k = e.key;
      if (k !== 'ArrowLeft' && k !== 'ArrowRight' && k !== 'ArrowUp' && k !== 'ArrowDown') return;
      e.preventDefault();
      var sg = (k === 'ArrowRight' || k === 'ArrowUp') ? 1 : -1;
      if (k === 'ArrowLeft' || k === 'ArrowRight') { if (e.shiftKey) st.sx = clamp(st.sx + sg * s); else st.x = clamp(st.x + sg * s); } else st.y = clamp(st.y + sg * s);
      say(false);
    });
    return { dragging: function () { return !!d; } };
  };

  /* ---------- SAMPLE SWARM for the ring's preview: seeded, never the engine ----------
     N members, natural frequencies spread by `spread`, coupled by K (rad/s, either sign: K < 0 splays), and per member two blade
     windows and a cross-mod strength. step(dt, K, spread) advances phases in cycles; cross(xm) is the per-member effect. */
  Synth.ring = function (seed, n) {
    var r = Synth.rng(seed), ph = [], w = [], bl = [], str = [], i;
    for (i = 0; i < n; i++) {
      ph.push(r()); w.push((r() - .5) * .9);
      var s1 = r(), s2 = frac(s1 + .5 + (r() - .5) * .2);
      bl.push({ st1: s1, w1: .08 + .16 * r(), st2: s2, w2: .05 + .1 * r() }); str.push(.4 + .6 * r());
    }
    return { phase: ph, blades: bl, R: 0, psi: 0,
      step: function (dt, K, spread) {
        var sx = 0, sy = 0, j; for (j = 0; j < n; j++) { sx += Math.cos(TAU * ph[j]); sy += Math.sin(TAU * ph[j]); }
        this.R = Math.hypot(sx, sy) / n; this.psi = Math.atan2(sy, sx);
        for (j = 0; j < n; j++) { var d = w[j] * spread + K * this.R * Math.sin(this.psi - TAU * ph[j]) / TAU; ph[j] = frac(ph[j] + (.35 + d) * dt); }
        return this;
      },
      /* a push lands only inside a member's blade windows, so its effect scales with how much of the cycle they cover */
      cross: function (xm) { return bl.map(function (b, k) { return xm * str[k] * Math.min(1, (b.w1 + b.w2) * 2.5); }); } };
  };

  /* ---------- GAIN STAGING ---------- */
  var DB_MIN = -66, DB_MAX = 6, HOLD_S = 1.5, FALL = 20, REL_TAU = .11, DRAG = 160, GR_FULL = 24;
  var dbFrac = function (db) { return clamp((db - DB_MIN) / (DB_MAX - DB_MIN)); };
  function fmtDb(v, dec, floor) { return floor !== undefined && v <= floor ? '−∞ dB' : (v > 0 ? '+' : v < 0 ? '−' : '') + Math.abs(v).toFixed(dec) + ' dB'; }
  function keysOn(o) { return typeof o.keys === 'function' ? !!o.keys() : !!o.keys; }
  function fire(fn, a, b) { if (typeof fn === 'function') fn(a, b); }

  /* The chain in signal order (ADR-230 (3), A1; spec RC-2). `kind` 'level' has a meter pair and a lamp; 'gr' is the limiter's gain
     reduction, a one-bar read-out that grows down. Ranges and defaults are ADR-230 A1's make-up (+12.04 dB), the module I/O
     standard's +/-24 dB (B435) and Volume's cut-only travel (B438); the rest are PLACEHOLDERS until the mixer panel's ranges are
     ruled (B402, RC-2 "deferred"). The consumer supplies the stage list from the manifest; this is the reference's own. */
  function chain(mode) {
    var io = function (def) { return { min: -24, max: 24, def: def || 0 }; }, out = [];
    function add(id, group, name, gain, kind, title) { out.push({ id: id, group: group, name: name, gain: gain || null, kind: kind || 'level', title: title || '' }); }
    if (mode === 'fx') {
      add('in', 'AUDIO IN', 'IN', io(), 'level', 'The device’s audio input (the FX device has no sources, filters or make-up)');
    } else {
      [['osc1', 'OSC 1', -10], ['osc2', 'OSC 2', -12], ['sub', 'SUB', -11], ['noise', 'NOISE', -26]].forEach(function (q) {
        add(q[0], 'SOURCES', q[1], { min: -60, max: 6, def: q[2], name: 'level' }, 'level', q[1] + ' after its level, pan and gate'); });
      [1, 2].forEach(function (k) {
        add('f' + k + 'i', 'FILTER ' + k, 'IN', io(), 'level', 'Filter slot ' + k + ' input'); add('f' + k + 'o', 'FILTER ' + k, 'OUT', io(), 'level', 'Filter slot ' + k + ' output'); });
      add('eng', 'ENGINE', 'OUT', { min: -60, max: 12, def: 0, name: 'out.level' }, 'level', 'The engine’s output (out.level); linear and unclipped');
      add('mk', 'MAKE-UP', 'RACK IN', { min: -12, max: 24, def: 12.04, dec: 2, name: 'make-up', x: true }, 'level', 'The make-up gain’s output, which is the rack’s input (ADR-230)');
    }
    [1, 2, 3, 4].forEach(function (k) {
      add('x' + k + 'i', 'FX ' + k, 'IN', io(), 'level', 'FX slot ' + k + ' input'); add('x' + k + 'o', 'FX ' + k, 'OUT', io(), 'level', 'FX slot ' + k + ' output'); });
    add('lim', 'MASTER', 'LIM IN', io(), 'level', 'The master limiter’s input: the pre-limiter tap, where the master lamp reads (ADR-203)');
    add('gr', 'MASTER', 'GR', null, 'gr', 'The limiter’s gain reduction');
    add('vol', 'MASTER', 'OUT', { min: -60, max: 0, def: 0, floor: -60, name: 'Volume' }, 'level', 'The master output after the limiter and Volume (cut only)');
    return out;
  }

  /* o: { stages, sr?, read(id) -> { l, r } dBFS peaks since the last frame, or { gr } dB of reduction; latch(id) -> { on, at, max };
          clear(id); onGain(id, dB); gains? { id: dB }; keys? }
     The latch lives on the audio side and is cleared here: a lamp can only latch what it was shown. step(dt, quiet) advances the
     ballistics (the meters' fall and the peak hold) by dt seconds and paints; quiet skips the paint (used to settle a still frame). */
  function GainStaging(el, o) {
    var sr = o.sr || 48000, taps = [], first = null;
    el.classList.add('hd-gs'); el.innerHTML = '';
    var top = tag('div', 'gs-top'), say = tag('span', 'gs-first'), clr = tag('button', 'hd-pill hd-focus', 'Clear all lamps'), wrap = tag('div', 'gs-groups');
    say.setAttribute('aria-live', 'polite'); clr.type = 'button'; clr.addEventListener('click', function () { taps.forEach(function (t) { if (t.lamp) o.clear(t.s.id); }); paint(); });
    top.appendChild(say); top.appendChild(clr); el.appendChild(top); el.appendChild(wrap);
    var label = function (s) { return s.group === s.name ? s.name : s.group + ' · ' + s.name; };

    function gainCtl(t, root) {
      var g = t.s.gain, val = t.gain, toN = function (x) { return (x - g.min) / (g.max - g.min); }, st = null;
      var sl = tag('div', 'hd-slider hd-focus'), out = tag('output', 'hd-value gs-gv'), nm = (g.name || 'gain');
      sl.setAttribute('role', 'slider'); sl.setAttribute('aria-label', label(t.s) + ' · ' + nm); sl.setAttribute('aria-valuemin', g.min); sl.setAttribute('aria-valuemax', g.max);
      sl.innerHTML = '<div class="track"><i class="fill"></i></div><i class="handle"></i>'; sl.tabIndex = keysOn(o) ? 0 : -1;
      var text = function (x) { return fmtDb(x, g.dec === undefined ? 1 : g.dec, g.floor) + (g.x ? ' · ×' + Math.pow(10, x / 20).toFixed(2) : ''); };
      function paintG() { var n = clamp(toN(val)); sl.style.setProperty('--v', n); sl.style.setProperty('--f0', '0px'); sl.style.setProperty('--fw', 'calc(' + n + ' * (100% - 13px) + 5px)');
        out.textContent = fmtDb(val, g.dec === undefined ? 1 : g.dec, g.floor); if (t.mul) t.mul.textContent = '\u00D7' + Math.pow(10, val / 20).toFixed(2); sl.setAttribute('aria-valuenow', val.toFixed(2)); sl.setAttribute('aria-valuetext', text(val)); }
      function set(x, fireIt) { val = clamp(x, g.min, g.max); t.gain = val; paintG(); if (fireIt) fire(o.onGain, t.s.id, val); }
      sl.addEventListener('pointerdown', function (e) { if (e.button !== 0) return; try { sl.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
        sl.classList.add('drag'); st = { x: e.clientX, y: e.clientY, n: clamp(toN(val)) }; });
      sl.addEventListener('pointermove', function (e) { if (!st) return; var d = (e.clientX - st.x) - (e.clientY - st.y);
        set(g.min + (g.max - g.min) * clamp(st.n + d / DRAG * (e.shiftKey ? .25 : 1)), true); });   // relative: size-drag is the full range, shift x0.25
      function end(e) { if (e && e.pointerId !== undefined && sl.hasPointerCapture && sl.hasPointerCapture(e.pointerId)) sl.releasePointerCapture(e.pointerId); sl.classList.remove('drag'); st = null; }
      sl.addEventListener('pointerup', end); sl.addEventListener('pointercancel', end); sl.addEventListener('lostpointercapture', end);
      sl.addEventListener('dblclick', function () { set(g.def, true); });
      sl.addEventListener('wheel', function (e) { if (!e.altKey) return; e.preventDefault(); set(val - Math.sign(e.deltaY) * (g.max - g.min) * .01, true); }, { passive: false });
      sl.addEventListener('keydown', function (e) { if (!keysOn(o)) return; var s = (e.shiftKey ? .0025 : .01) * (g.max - g.min), k = e.key;
        if (k === 'ArrowRight' || k === 'ArrowUp') set(val + s, true); else if (k === 'ArrowLeft' || k === 'ArrowDown') set(val - s, true); else return; e.preventDefault(); });
      root.appendChild(sl); root.appendChild(out);
      if (g.x) { var mul = tag('div', 'hd-hint gs-mul'); root.appendChild(mul); t.mul = mul; }   // the make-up reads as a factor too: x4.00 = +12.04 dB
      paintG();
      return { sl: sl, set: function (x) { set(x, false); } };
    }

    function build(stages) {
      wrap.innerHTML = ''; taps = []; var grp = null, row = null, last = null;
      stages.forEach(function (s) {
        if (s.group !== last) { grp = tag('div', 'gs-grp'); grp.appendChild(tag('div', 'hd-cluster', s.group)); row = tag('div', 'gs-row'); grp.appendChild(row); wrap.appendChild(grp); last = s.group; }
        var t = { s: s, bar: [DB_MIN, DB_MIN], pk: [DB_MIN, DB_MIN], held: [0, 0], gain: s.gain ? (o.gains && o.gains[s.id] !== undefined ? o.gains[s.id] : s.gain.def) : null, key: '', vkey: '', bars: [], lamp: null };
        var root = tag('div', 'hd-tap'); root.dataset.stage = s.id; if (s.title) root.title = s.title;
        if (s.kind === 'level') {
          t.lamp = tag('button', 'hd-lamp hd-focus'); t.lamp.type = 'button'; t.lamp.addEventListener('click', function () { o.clear(s.id); paint(); }); root.appendChild(t.lamp);
        } else root.appendChild(tag('span', 'gs-nolamp'));
        var pair = tag('div', 'hd-mpair'); pair.setAttribute('role', 'meter'); pair.setAttribute('aria-label', label(s) + (s.kind === 'gr' ? ' gain reduction' : ' level'));
        pair.setAttribute('aria-valuemin', s.kind === 'gr' ? 0 : DB_MIN); pair.setAttribute('aria-valuemax', s.kind === 'gr' ? GR_FULL : DB_MAX);
        for (var c = 0; c < (s.kind === 'gr' ? 1 : 2); c++) { var b = tag('div', 'hd-mbar' + (s.kind === 'gr' ? ' gr' : '')); b.innerHTML = '<i class="fill"></i><i class="hold"></i>'; pair.appendChild(b); t.bars.push({ el: b, fill: b.firstChild, hold: b.lastChild }); }
        root.appendChild(pair);
        t.pkEl = tag('div', 'hd-value'); t.pkEl.style.fontSize = '9.5px'; root.appendChild(t.pkEl);
        var nm = tag('div', 'hd-label', s.name); nm.style.fontSize = '9px'; nm.style.letterSpacing = '.06em'; root.appendChild(nm);
        t.pair = pair; if (s.gain) t.ctl = gainCtl(t, root);
        row.appendChild(root); taps.push(t);
      });
    }

    /* FIRST OVER: the latched stage with the earliest first-over sample; a tie (one hot sample crossing several taps at once)
       goes to the most upstream, which is the lowest index in the chain's order. */
    function firstOver() {
      var best = null;
      taps.forEach(function (t) { var L = t.L; if (L && L.on && (!best || L.at < best.L.at)) best = t; });
      return best;
    }
    function paint() {
      taps.forEach(function (t) { t.L = t.s.kind === 'level' ? o.latch(t.s.id) : null; });
      var f = firstOver(), lit = 0;
      taps.forEach(function (t) {
        var s = t.s, gr = s.kind === 'gr', L = t.L, on = !!(L && L.on); if (on) lit++;
        t.bars.forEach(function (b, c) {
          /* a level bar grows up from -66 dBFS; the reduction bar grows DOWN from the top, so its hold line sits (1 - g) up from the bottom */
          var lv = gr ? clamp(t.bar[0] / GR_FULL) : dbFrac(t.bar[c]), hp = gr ? 1 - clamp(t.pk[0] / GR_FULL) : dbFrac(t.pk[c]);
          b.fill.style.setProperty('--lv', lv.toFixed(4)); b.hold.style.setProperty('--hp', hp.toFixed(4));
          b.el.classList.toggle('over', on);
        });
        var pk = gr ? t.pk[0] : Math.max(t.pk[0], t.pk[1]), txt = gr ? (pk < .05 ? '0.0' : '−' + pk.toFixed(1)) : (pk <= DB_MIN ? '−∞' : (pk >= 0 ? '+' : '−') + Math.abs(pk).toFixed(1));
        if (t.vkey !== txt) {
          t.vkey = txt; t.pkEl.textContent = txt;
          t.pair.setAttribute('aria-valuenow', (gr ? pk : Math.max(DB_MIN, pk)).toFixed(1));
          t.pair.setAttribute('aria-valuetext', gr ? 'gain reduction ' + txt + ' dB' : txt === '−∞' ? 'silent' : txt + ' dBFS peak');
        }
        if (t.lamp) {
          t.lamp.classList.toggle('first', f === t); t.lamp.setAttribute('aria-pressed', String(on));
          var key = (on ? 'on' + L.max.toFixed(1) : 'off') + (f === t ? 'f' : '');
          if (t.key !== key) {
            t.key = key; t.lamp.innerHTML = on ? 'CLIP<small>+' + L.max.toFixed(1) + '</small>' : 'CLIP<small>—</small>';
            t.lamp.setAttribute('aria-label', label(s) + ' clip lamp' + (on ? ', latched at +' + L.max.toFixed(1) + ' dBFS' + (f === t ? ', the first over' : '') + ': click to clear' : ', no over since cleared'));
          }
        }
      });
      say.textContent = !f ? 'FIRST OVER — none latched'
        : 'FIRST OVER — ' + label(f.s) + ' at ' + (f.L.at / sr).toFixed(2) + ' s, +' + f.L.max.toFixed(1) + ' dB' + (lit > 1 ? ' · ' + (lit - 1) + ' more lamp' + (lit > 2 ? 's' : '') + ' lit' : '');
      first = f;
    }
    function step(dt, quiet) {
      taps.forEach(function (t) {
        var r = o.read(t.s.id) || {}, gr = t.s.kind === 'gr', cs = gr ? 1 : 2;
        for (var c = 0; c < cs; c++) {
          var db = gr ? (r.gr || 0) : (c ? r.r : r.l); if (db === undefined || db < (gr ? 0 : DB_MIN)) db = gr ? 0 : DB_MIN;
          t.bar[c] = db >= t.bar[c] ? db : db + (t.bar[c] - db) * Math.exp(-dt / REL_TAU);            // the bar falls with a 0.11 s time constant
          if (db >= t.pk[c]) { t.pk[c] = db; t.held[c] = 0; } else { t.held[c] += dt; if (t.held[c] > HOLD_S) t.pk[c] = Math.max(db, t.pk[c] - FALL * dt); }   // held 1.5 s, then 20 dB/s
        }
      });
      if (!quiet) paint();
    }
    build(o.stages);
    paint();
    return { step: step, render: paint, first: function () { return first ? first.s.id : null; }, stages: function (s) { o.stages = s; build(s); paint(); },
      gain: function (id) { var t = taps.filter(function (q) { return q.s.id === id; })[0]; return t ? t.gain : null; },
      setGain: function (id, db) { taps.forEach(function (t) { if (t.s.id === id && t.ctl) t.ctl.set(db); }); } };
  }
  GainStaging.chain = chain;
  H.GainStaging = GainStaging;
})(window.Horde);
/* =====================================================================
   THE CHAIN LINE AND THE BLEND MAPS (B545). Two components, one part, both from labs the human liked:
   CHAIN LINE   ChainLine, Viz.chainLine: the mixer lab's "Gain staging — every tap on one line" (docs/design/mixer-lab.html,
                drawStaging :991, STAGES :461, originStage :1130). It reads the SAME stage list and the same read/latch callbacks as
                GainStaging (50-ring-gain.js), so the two views are fed by one source and cannot disagree. B549: the dots are readings
                only; each stage's gain is a vertical offset bar in a lane under the line, in its dot's column, through the consumer's one
                gain model (gain(id), onGain(id, dB)). (B547 had made the dots the handles; they move on their own, which confused.)
   BLEND MAPS   BlendMaps: the morph editor lab's Waypoints pane (docs/design/morph-editor-lab.html, B235, B268, B272): one row's
                value across the pad as a heatmap per law, the pins and corners, TIN's dashed triangulation, the clamp to the set
                values, and a cross-section under each law. The law math (Block W, :1496-1650) is transcribed, not rewritten.
   Nothing here reads a clock or a random source: the line's ballistics advance by the dt the caller passes, and the blend maps are
   pure functions of the row (corners, pins, law, clamp). Colours are tokens only: the six screen roles through Viz.kit, plus
   alarm, caution and the corner fills where the lab draws them.
   ===================================================================== */
(function (H) {
  'use strict';
  if (!H) return;
  var Viz = H.Viz;
  function clamp(x, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, x)); }
  function tag(t, cls, txt) { var e = document.createElement(t); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; }
  function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }
  function monoOf(cv) { return getComputedStyle(cv).getPropertyValue('--font-mono') || 'monospace'; }
  /* a token that Viz.kit's palette does not carry (caution, the corner fills), read the way Viz.kit reads its own */
  function rgbOf(host, name) {
    var e = document.createElement('i'); e.style.color = 'var(--' + name + ')'; e.style.display = 'none'; host.appendChild(e);
    var m = getComputedStyle(e).color.match(/[\d.]+/g).map(Number); e.remove(); return m;
  }
  function rgba(c, a) { return 'rgba(' + c[0] + ',' + c[1] + ',' + c[2] + ',' + a + ')'; }
  /* the corner glyph as a canvas path, the same four shapes as the Design System's (A diamond, B triangle, C circle, D square) */
  function glyphPath(g, k, cx, cy, r) {
    g.beginPath();
    if (k === 0) { g.moveTo(cx, cy - r); g.lineTo(cx + r, cy); g.lineTo(cx, cy + r); g.lineTo(cx - r, cy); g.closePath(); }
    else if (k === 1) { g.moveTo(cx, cy - r); g.lineTo(cx + r * .95, cy + r * .75); g.lineTo(cx - r * .95, cy + r * .75); g.closePath(); }
    else if (k === 2) g.arc(cx, cy, r * .9, 0, 2 * Math.PI);
    else g.rect(cx - r * .8, cy - r * .8, r * 1.6, r * 1.6);
  }
  function themeSig(node) { var t = node.closest('[data-scr]'); return (document.documentElement.getAttribute('data-theme') || '') + '/' + (t ? t.dataset.scr : ''); }
  /* redraw when the box or the theme or the screen scheme changes, not on a clock. A component whose element has left the page (a preview
     rebuilds its host) stops watching: painting a detached canvas has no colours to read. */
  function watch(node, f) {
    var ro = null, mo = null;
    function go() { if (!node.isConnected) { if (ro) ro.disconnect(); if (mo) mo.disconnect(); return; } f(); }
    if (typeof ResizeObserver === 'function') { ro = new ResizeObserver(go); ro.observe(node); }
    if (typeof MutationObserver === 'function') { mo = new MutationObserver(go); mo.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme', 'data-scr'] }); }
  }

  /* =====================================================================
     CHAIN LINE
     ===================================================================== */
  /* The y scale is the lab's: -48 to +6 dBFS (the Gain staging bars run -66 to +6; below -48 a tap sits on the floor line).
     The peak hold is Gain staging's own law (held 1.5 s, then 20 dB/s, per channel; 50-ring-gain.js step), repeated here
     because that part keeps its constants private: change them together. */
  var CL_MIN = -48, CL_MAX = 6, DB_FLOOR = -66, HOLD_S = 1.5, FALL = 20;
  /* HIT is the pointing radius of a dot (a 32 px target). B549: the dots are READINGS and never move a gain; the gain of a stage is its own
     bar in the lane under the line. DRAG is the house size-drag (160 px = a stage's whole gain range, shift x0.25: 50-ring-gain.js and
     P/README, Interaction). MINW is the narrowest a stage's column may get (a bar's hit width; the house floor is 28): below it the
     line and its lane scroll together inside their box instead of shrinking the bars. LANE_H / LANE_PAD / HANDLE are the lane's CSS px:
     the pad keeps a bar's value label inside the lane at both ends of the scale. PL / PR are the glass's left and right margins, shared
     by the canvas and the lane so a stage's dot and its bar sit in one column. */
  var HIT = 16, DRAG = 160, MINW = 32, LANE_H = 168, LANE_PAD = 18, HANDLE = 13, PL = 38, PR = 10;

  /* s: { taps: [{ id, label, col, db (dBFS or null = no reading), fed, over, first, probe }], links: [[i, j], ...] }.
     Every tap has its own slot, in signal order (the source lanes sit side by side in the first column, no stacking), so the dot of
     tap k is at the centre of slot k, which is where the lane puts the bar of tap k. Records the hit list in CSS px (x, y as drawn)
     on cv.__line for the pointer and for a check to read back. */
  Viz.chainLine = function (cv, s) {
    var K = Viz.kit(cv), g = K.g, V = K.V, W = K.W, Hh = K.H, mono = monoOf(cv), taps = s.taps, n = taps.length, i;
    K.tube(); K.scan();
    g.font = '500 ' + (8.5 * V) + 'px ' + mono;
    var pl = PL * V, pr = PR * V, pt = 10 * V, slot = (W - pl - pr) / Math.max(1, n), rot = slot < 40 * V, pb = 18 * V;
    if (rot) { var mw = 0; taps.forEach(function (t) { mw = Math.max(mw, g.measureText(t.label).width); }); pb = mw + 12 * V; }   // labels read downwards when the columns are narrow
    var X = function (k) { return pl + (k + .5) * slot; }, Y = function (db) { return pt + (1 - (clamp(db, CL_MIN, CL_MAX) - CL_MIN) / (CL_MAX - CL_MIN)) * (Hh - pt - pb); };
    var Yt = function (t) { return Y(t.db === null ? CL_MIN : t.db); };
    g.textBaseline = 'middle'; g.textAlign = 'left';
    [-6, -12, -24, -36, -48].forEach(function (d) {
      g.strokeStyle = K.c('grid', .18); g.lineWidth = V; g.beginPath(); g.moveTo(pl, Y(d)); g.lineTo(W - pr, Y(d)); g.stroke();
      g.fillStyle = K.c('grid', .85); g.fillText(String(d).replace('-', '−'), 4 * V, Y(d));
    });
    g.setLineDash([5 * V, 3 * V]); g.strokeStyle = K.c('alarm'); g.lineWidth = 1.5 * V;           // the 0 dBFS rail: dashed, alarm
    g.beginPath(); g.moveTo(pl, Y(0)); g.lineTo(W - pr, Y(0)); g.stroke(); g.setLineDash([]);
    g.fillStyle = K.c('alarm'); g.fillText('0 dBFS', 4 * V, Y(0));
    g.strokeStyle = K.c('meter', .5); g.lineWidth = 1.5 * V; g.beginPath();                          // the links follow the signal
    s.links.forEach(function (l) { var a = taps[l[0]], b = taps[l[1]]; g.moveTo(X(l[0]), Yt(a)); g.lineTo(X(l[1]), Yt(b)); });
    g.stroke();
    var hits = [];
    taps.forEach(function (t, k) {
      var x = X(k), y = Yt(t), r = 4 * V;
      hits.push({ id: t.id, x: x / V, y: y / V });
      g.beginPath(); g.arc(x, y, r, 0, 2 * Math.PI);
      if (t.over) { g.fillStyle = K.c('alarm'); g.fill(); }                                         // red: latched over
      else if (t.fed) { g.fillStyle = K.c('meter'); g.fill(); }                                      // solid: the engine feeds this tap
      else { g.fillStyle = K.c('ground'); g.fill(); g.strokeStyle = K.c('meter'); g.lineWidth = 1.5 * V; g.stroke(); }   // hollow: owed
      if (t.probe) { g.strokeStyle = K.c('value'); g.lineWidth = 2 * V; g.beginPath(); g.arc(x, y, r + 4 * V, 0, 2 * Math.PI); g.stroke(); }
      if (t.first) { g.strokeStyle = K.c('marker'); g.lineWidth = 2 * V; g.beginPath(); g.arc(x, y, r + 7.5 * V, 0, 2 * Math.PI); g.stroke(); }   // the first over
    });
    var done = {};                                                                                   // one label per column, centred over its slots (the sources' four share one)
    taps.forEach(function (t, k) {
      if (done[t.col]) return; done[t.col] = 1;
      var mine = [], on = false; taps.forEach(function (q, j) { if (q.col === t.col) { mine.push(X(j)); on = on || q.probe; } });
      var cx = (mine[0] + mine[mine.length - 1]) / 2;
      g.fillStyle = on ? K.c('value') : K.c('grid', .9);
      if (rot) { g.save(); g.translate(cx, Hh - pb + 6 * V); g.rotate(Math.PI / 2); g.textAlign = 'left'; g.fillText(t.label, 0, 0); g.restore(); }
      else { g.textAlign = 'center'; g.fillText(t.label, cx, Hh - 7 * V); }
    });
    g.textAlign = 'left';
    cv.__line = { hits: hits, rot: rot, hit: HIT };
  };

  /* o: { stages (GainStaging.chain's list), sr?, read(id), latch(id), fed?(id), lane?(stage), gain?(id), onGain?(id, dB), keys? }
     read and latch are GainStaging's callbacks, verbatim. fed(id) says the engine publishes the tap; the default is "read(id) returns a
     reading", so a tap with no reading is hollow and sits on the floor, never a guess. lane(stage) names the stages that run in
     parallel and fold into the next one (default: the SOURCES group). The 'gr' stage is not a level and is not drawn.
     THE DOTS ARE READINGS (B549; B547 made them handles and the human found it confusing: they move on their own, so it was unclear where
     a dragged one would end up). A dot only points and names. GAINS are the bars of the lane under the line: with gain(id) -> dB and
     onGain(id, dB) every stage that has a `gain` range gets a vertical offset bar in its own dot's column, from a 0 dB baseline: it rises
     for boost and falls for cut, on ONE dB scale for the whole lane (the union of the stages' ranges, so equal dB is equal height). A bar
     moves only when it is dragged or the model changes. gain is the consumer's ONE model, the one Gain staging's faders also read and
     write (GainStaging.gain / setGain): the line writes through onGain, never holds a value of its own, and reads gain(id) on every
     paint, so a fader moves its bar and a bar moves its fader. A drag is relative (DRAG px = the stage's whole range, shift x0.25), a
     double-click sets the stage's default, up/down on the focused bar step it (1 % of the range, shift 0.25 %; keys() / keys gates the
     keys and the bars' tab stops, default on). A stage with no gain range has an empty slot. */
  H.ChainLine = function (el, o) {
    var sr = o.sr || 48000, taps = [], links = [], probe = -1, first = null, bars = [], scale = null, hand = null;
    var isLane = typeof o.lane === 'function' ? o.lane : function (s) { return s.group === 'SOURCES'; };
    el.classList.add('hd-cl'); el.innerHTML = '';
    var wired = typeof o.onGain === 'function';
    var scroll = tag('div', 'cl-scroll'), inner = tag('div', 'cl-inner'), viz = tag('div', 'hd-viz cl-glass'), cv = tag('canvas'), read = tag('div', 'hd-hint cl-read'), key = tag('div', 'hd-hint cl-key');
    var lane = tag('div', 'cl-lane'), plot = tag('div', 'cl-plot');
    cv.tabIndex = 0; cv.setAttribute('role', 'img'); cv.setAttribute('aria-roledescription', 'chain line'); cv.classList.add('hd-focus');
    cv.setAttribute('aria-keyshortcuts', 'ArrowLeft ArrowRight');
    lane.setAttribute('role', 'group'); lane.setAttribute('aria-label', 'Stage gains, one bar per stage, from 0 dB'); lane.style.height = 'calc(' + LANE_H + 'px + 2 * var(--stroke-ink))';
    viz.appendChild(cv); inner.appendChild(viz); lane.appendChild(plot); inner.appendChild(lane); scroll.appendChild(inner); el.appendChild(scroll); el.appendChild(read); el.appendChild(key);
    key.innerHTML = '<span><i class="cl-dot"></i>fed</span><span><i class="cl-dot hol"></i>owed (no reading yet)</span><span><i class="cl-dot over"></i>latched over</span>'
      + '<span><i class="cl-dot first"></i>first over</span><span><i class="cl-dot probe"></i>pointed at</span><span><i class="cl-rail"></i>0 dBFS</span>'
      + (wired ? '<span><i class="cl-barsw"></i>gain bar: the control, from 0 dB</span>' : '');
    function short(s) {
      var m = /^(FILTER|FX) (\d+)$/.exec(s.group);
      if (s.short) return s.short;
      if (isLane(s)) return 'SOURCES';
      if (m) return (m[1] === 'FX' ? 'FX' : 'F') + m[2] + ' ' + s.name;
      return s.group === 'MASTER' ? (s.name === 'OUT' ? 'MASTER' : s.name) : s.group;
    }
    var full = function (s) { return s.group === s.name ? s.name : s.group + ' · ' + s.name; };
    var keysOk = function () { return typeof o.keys === 'function' ? !!o.keys() : o.keys === undefined ? true : !!o.keys; };
    var gainOf = function (t) { var v = typeof o.gain === 'function' ? o.gain(t.s.id) : null; return typeof v === 'number' && isFinite(v) ? v : null; };
    function fmtGain(t, v) { var g = t.s.gain; return g.floor !== undefined && v <= g.floor ? '−∞ dB' : (v > 0 ? '+' : v < 0 ? '−' : '') + Math.abs(v).toFixed(g.dec === undefined ? 1 : g.dec) + ' dB'; }
    var fmtNum = function (t, v) { return fmtGain(t, v).replace(/ dB$/, ''); };
    function setGain(t, v) { var g = t.s.gain; v = clamp(v, g.min, g.max); if (v !== gainOf(t)) o.onGain(t.s.id, v); }
    /* a bar needs the consumer's model AND a stage with a gain range; a stage with none keeps an empty slot */
    var ctl = function (t) { return wired && !!t.s.gain && gainOf(t) !== null; };

    function build(stages) {
      taps = []; links = []; bars = []; hand = null; var cur = [], col = -1, curLane = false;
      stages.forEach(function (s) {
        if (s.kind !== 'level') return;                                    // the limiter's reduction is a reading in dB of reduction, not a level on this scale
        var lane1 = isLane(s), t = { s: s, label: short(s), pk: [DB_FLOOR, DB_FLOOR], held: [0, 0], db: null, fed: false, over: false };
        if (lane1 && curLane) { t.col = col; cur.push(taps.length); }      // parallel lanes share a column (their dots and bars sit side by side) and fold into the next stage
        else { col++; t.col = col; cur.forEach(function (a) { links.push([a, taps.length]); }); cur = [taps.length]; curLane = lane1; }
        taps.push(t);
      });
      buildLane();
    }
    /* The lane. ONE dB scale for all the bars: the union of every stage's gain range with 0, so equal dB is equal height whatever a stage's own range. */
    function buildLane() {
      plot.innerHTML = ''; lane.querySelectorAll('.cl-sc').forEach(function (e) { e.remove(); });
            var top = 0, bot = 0; taps.forEach(function (t) { if (t.s.gain) { top = Math.max(top, t.s.gain.max); bot = Math.min(bot, t.s.gain.min); } });
      scale = wired && top > bot ? { top: top, bot: bot, y: function (db) { return LANE_PAD + (top - clamp(db, bot, top)) / (top - bot) * (LANE_H - 2 * LANE_PAD); } } : null;
      lane.hidden = !scale;
      if (!scale) { el.style.removeProperty('--cl-n'); return; }                                   // no lane: the line is the B545 reading and shrinks to fit as it did
      el.style.setProperty('--cl-n', taps.length);                                                // the inner box's minimum width: MINW per slot (CSS)
      var z = tag('i', 'cl-zero'); z.style.top = scale.y(0) + 'px'; plot.appendChild(z);        // the 0 dB baseline, across every slot, empty ones too
      [[top, '+' + top + ' dB'], [0, '0 dB'], [bot, '−' + Math.abs(bot) + ' dB']].forEach(function (q) {   // the scale's ends and its zero, in the gutter
        var l = tag('span', 'cl-sc' + (q[0] === 0 ? ' z' : ''), q[1]); l.style.top = scale.y(q[0]) + 'px'; lane.appendChild(l);
      });
      taps.forEach(function (t, k) {
        var cell = tag('div', 'cl-slot'); plot.appendChild(cell);
        if (!t.s.gain) return;
        var g = t.s.gain, b = { t: t, el: cell, v: null };
        cell.classList.add('cl-bar'); cell.setAttribute('role', 'slider'); cell.setAttribute('aria-orientation', 'vertical');
        cell.setAttribute('aria-label', full(t.s) + ' · ' + (g.name || 'gain')); cell.setAttribute('aria-valuemin', g.min); cell.setAttribute('aria-valuemax', g.max);
        cell.dataset.stage = t.s.id; cell.title = full(t.s) + ' gain: drag, double-click for ' + fmtGain(t, g.def);
        var rail = tag('i', 'cl-rail2'), fill = tag('i', 'cl-fill'), zt = tag('i', 'cl-zt'), hd = tag('i', 'cl-handle'), val = tag('span', 'cl-val');
        rail.style.top = scale.y(g.max) + 'px'; rail.style.height = (scale.y(g.min) - scale.y(g.max)) + 'px'; rail.style.setProperty('--ry', scale.y(g.max) + 'px');
        zt.style.top = (scale.y(0) - .75) + 'px';
        rail.appendChild(fill); [rail, zt, hd, val].forEach(function (e) { cell.appendChild(e); });
        b.rail = rail; b.fill = fill; b.hd = hd; b.val = val;
        cell.addEventListener('pointerdown', function (e) {
          if (e.button !== 0 || !ctl(t)) return;
          hand = { b: b, y: e.clientY, n: (gainOf(t) - g.min) / (g.max - g.min) };
          try { cell.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
          cell.classList.add('drag');
        });
        cell.addEventListener('pointermove', function (e) {
          if (!hand || hand.b !== b) return;
          var dy = e.clientY - hand.y; hand.y = e.clientY;
          if (!dy) return;                                                  // no motion, no write: nothing jumps
          hand.n -= dy / DRAG * (e.shiftKey ? .25 : 1);                     // up is more; the hand is not clamped, so the value cannot drift from it
          setGain(t, g.min + (g.max - g.min) * clamp(hand.n)); paint();
        });
        function end(e) { if (e && e.pointerId !== undefined && cell.hasPointerCapture && cell.hasPointerCapture(e.pointerId)) cell.releasePointerCapture(e.pointerId); cell.classList.remove('drag'); if (hand && hand.b === b) hand = null; }
        cell.addEventListener('pointerup', end); cell.addEventListener('pointercancel', end); cell.addEventListener('lostpointercapture', end);
        cell.addEventListener('dblclick', function () { if (ctl(t)) { setGain(t, g.def); paint(); } });
        cell.addEventListener('keydown', function (e) {
          if ((e.key !== 'ArrowUp' && e.key !== 'ArrowDown') || !keysOk() || !ctl(t)) return;       // keys off or no gain: the page keeps the key
          e.preventDefault();
          var st = (e.shiftKey ? .0025 : .01) * (g.max - g.min);
          setGain(t, gainOf(t) + (e.key === 'ArrowUp' ? st : -st)); paint();
        });
        bars.push(b);
      });
    }
    /* a bar is drawn from the model and nothing else: it moves only when the value it reads has moved */
    function paintBars() {
      bars.forEach(function (b) {
        var t = b.t, g = t.s.gain, v = gainOf(t), on = v !== null;
        b.el.classList.toggle('off', !on); b.el.tabIndex = on && keysOk() ? 0 : -1;
        if (!on || v === b.v) return;
        b.v = v;
        var y0 = scale.y(0), yv = scale.y(v), up = v >= 0;
        b.fill.style.top = 'calc(' + Math.min(y0, yv) + 'px - var(--ry) - var(--stroke-ink))'; b.fill.style.height = Math.abs(yv - y0) + 'px';
        b.hd.style.top = (yv - HANDLE / 2) + 'px';
        b.val.textContent = fmtNum(t, v); b.val.style.top = (up ? yv - HANDLE / 2 - 11 : yv + HANDLE / 2 + 1) + 'px';
        b.el.setAttribute('aria-valuenow', v.toFixed(2)); b.el.setAttribute('aria-valuetext', fmtGain(t, v));
      });
    }
    function firstOver() {
      var best = null;
      taps.forEach(function (t) { if (t.L && t.L.on && (!best || t.L.at < best.L.at)) best = t; });   // a tie goes to the most upstream, the lowest index
      return best;
    }
    function readout() {
      if (probe < 0 || !taps[probe]) { read.textContent = 'Point at a dot, or focus the line and use ← →, for a tap’s peak hold and state.' + (scale ? ' The bars under it are the gains: drag a bar, or focus it and use ↑ ↓.' : ''); return; }
      var t = taps[probe], pk = Math.max(t.pk[0], t.pk[1]);
      read.textContent = full(t.s) + ' — peak hold ' + (t.db === null || pk <= DB_FLOOR ? '−∞' : (pk >= 0 ? '+' : '−') + Math.abs(pk).toFixed(1)) + ' dBFS · '
        + (t.fed ? 'fed by the engine' : 'owed: no reading yet') + (t.L && t.L.on ? ' · lamp latched at +' + t.L.max.toFixed(1) + ' dBFS' + (first === t ? ', the first over' : '') : ' · lamp dark')
        + (!wired ? '' : ctl(t) ? ' · gain ' + fmtGain(t, gainOf(t)) + ' (default ' + fmtGain(t, t.s.gain.def) + ')' : ' · no gain control');
    }
    function paint() {
      taps.forEach(function (t) {
        t.L = o.latch(t.s.id) || { on: false }; var r = o.read(t.s.id);
        t.fed = typeof o.fed === 'function' ? !!o.fed(t.s.id) : r !== undefined && r !== null;
        t.has = r !== undefined && r !== null; t.over = !!t.L.on;
        t.db = t.has ? Math.max(t.pk[0], t.pk[1]) : null;
      });
      var f = firstOver(); first = f;
      Viz.chainLine(cv, { taps: taps.map(function (t, k) {
        return { id: t.s.id, label: t.label, col: t.col, db: t.db, fed: t.fed, over: t.over, first: f === t, probe: k === probe }; }), links: links });
      paintBars();
      var over = taps.filter(function (t) { return t.over; }).length;
      cv.setAttribute('aria-label', 'Chain line, ' + taps.length + ' taps in signal order, peak holds in dBFS. ' + (f ? 'First over: ' + full(f.s) + ' at ' + (f.L.at / sr).toFixed(2) + ' s' + (over > 1 ? ', ' + (over - 1) + ' more latched.' : '.') : 'No lamp latched.')
        + (probe >= 0 ? ' Pointed at ' + full(taps[probe].s) + (wired ? (ctl(taps[probe]) ? ', gain ' + fmtGain(taps[probe], gainOf(taps[probe])) + '.' : ', no gain control.') : '.') : ''));
      readout();
    }
    function step(dt, quiet) {
      taps.forEach(function (t) {
        var r = o.read(t.s.id) || {}, held = t.held;
        for (var c = 0; c < 2; c++) {
          var db = c ? r.r : r.l; if (db === undefined || db < DB_FLOOR) db = DB_FLOOR;
          if (db >= t.pk[c]) { t.pk[c] = db; held[c] = 0; } else { held[c] += dt; if (held[c] > HOLD_S) t.pk[c] = Math.max(db, t.pk[c] - FALL * dt); }
        }
      });
      if (!quiet) paint();
    }
    /* the nearest dot to the pointer within the pointing radius, by where it is drawn */
    function nearest(e) {
      var b = cv.getBoundingClientRect(), x = e.clientX - b.left, y = e.clientY - b.top, best = -1, bd = HIT, h = (cv.__line || {}).hits || [];
      h.forEach(function (q, k) { var dd = Math.hypot(q.x - x, q.y - y); if (dd < bd) { bd = dd; best = k; } });
      return best;
    }
    function point(k) { if (k < 0 || k === probe) return; probe = k; paint(); }
    cv.addEventListener('pointermove', function (e) { point(nearest(e)); });
    cv.addEventListener('pointerdown', function (e) { if (e.button === 0) point(nearest(e)); });          // touch has no hover: a tap names the dot
    cv.addEventListener('keydown', function (e) {
      var k = e.key; if (k !== 'ArrowLeft' && k !== 'ArrowRight') return; e.preventDefault();
      probe = clamp((probe < 0 ? (k === 'ArrowRight' ? -1 : taps.length) : probe) + (k === 'ArrowRight' ? 1 : -1), 0, taps.length - 1); paint();
    });
    build(o.stages); paint(); watch(viz, function () { paint(); });
    return { step: step, render: paint, first: function () { return first ? first.s.id : null; }, probe: function (id) { if (id === undefined) return probe < 0 ? null : taps[probe].s.id; probe = -1; taps.forEach(function (t, k) { if (t.s.id === id) probe = k; }); paint(); },
      stages: function (s) { o.stages = s; probe = -1; build(s); paint(); } };
  };

  /* =====================================================================
     BLEND MAPS — the waypoint laws (morph-editor-lab.html Block W, :1496-1650; transcribed). PURE: no DOM, no clock.
     A continuous row may carry a few VALUES PINNED AT PAD POSITIONS ("pins") on top of its four corners, and BLEND interpolates through
     them. The Δ (residual) laws interpolate the pin value minus what today's bilinear law gives there, added back onto it, so no pin is
     exactly today's blend and a first pin set to what you hear changes nothing. TIN (plain) interpolates the values themselves.
     Corners are READ, never computed: surfAt returns the base law at the four exact corner positions and skips every clamp.
     ===================================================================== */
  var CXY = [[0, 0], [1, 0], [0, 1], [1, 1]];                  // A B C D on the pad: A top left, B top right, C bottom left, D bottom right
  var BM_CAP = 8, BM_SNAP = 0.04, BM_GUARD = 0.04, HIT_PX = 14;   // the lab's caps; its hit radius is 11 px, here 14 so a target is the house 28 px
  var LAWS = ['tin', 'tind', 'idw', 'tps'];
  var LAW_NAME = { tin: 'TIN', tind: 'TIN·Δ', idw: 'IDW·Δ', tps: 'TPS·Δ' };
  var LAW_SHORT = { tin: 'linear · values', tind: 'linear · residual', idw: 'inverse-distance', tps: 'thin-plate spline' };
  var LAW_TRAIT = { tin: 'kinks on the mesh · never overshoots', tind: 'kinks · today’s blend under the pins', idw: 'flat shoulder at every pin', tps: 'smooth · humps past the pins (clamp: flat top)' };
  /* one ink per law, shared by the mini's name, the cross-section curve and the key; a dash pattern backs every colour (lab :3191) */
  var LAW_INK = { tps: 'value', tin: 'marker', tind: 'physics', idw: 'meter' }, LAW_VAR = { tps: '--scr-value', tin: '--scr-marker', tind: '--scr-physics', idw: '--scr-meter' };
  var LAW_DASH = { tps: [], tin: [], tind: [7, 4], idw: [1.5, 3.5] }, LAW_DCLS = { tps: '', tin: ' thin', tind: ' d', idw: ' o' };

  function weights(x, y) { return [(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y]; }
  function isCornerXY(x, y) { return (x === 0 || x === 1) && (y === 0 || y === 1); }
  /* Bowyer-Watson seeded with the square's two triangles (diagonal A-D) rather than a super-triangle: every pin lies inside the square, so the square
     IS the hull, and a super-triangle's far vertices are what make pins near an edge lose their hull triangles. Deterministic: same pins in the same
     ORDER, same mesh; co-circular ties (the bare square is one) go to whoever was inserted first, so a row's stored pin order is part of its state. */
  function triOrient(P, a, b, c) { return (P[b][0] - P[a][0]) * (P[c][1] - P[a][1]) - (P[b][1] - P[a][1]) * (P[c][0] - P[a][0]); }
  function inCircle(P, t, px, py) {                     // t is CCW; > 0 strictly inside
    var ax = P[t[0]][0] - px, ay = P[t[0]][1] - py, bx = P[t[1]][0] - px, by = P[t[1]][1] - py, cx = P[t[2]][0] - px, cy = P[t[2]][1] - py;
    return (ax * ax + ay * ay) * (bx * cy - cx * by) - (bx * bx + by * by) * (ax * cy - cx * ay) + (cx * cx + cy * cy) * (ax * by - bx * ay) > 1e-13;
  }
  function triangulate(P) {
    var T = [[0, 1, 3], [0, 3, 2]], p, e, t, k;
    for (p = 4; p < P.length; p++) {
      var bad = [], keep = [], cnt = {};
      T.forEach(function (tr) { (inCircle(P, tr, P[p][0], P[p][1]) ? bad : keep).push(tr); });
      bad.forEach(function (tr) { for (e = 0; e < 3; e++) { var a = tr[e], b = tr[(e + 1) % 3]; k = a < b ? a + ',' + b : b + ',' + a; cnt[k] = (cnt[k] || 0) + 1; } });
      bad.forEach(function (tr) { for (e = 0; e < 3; e++) {
        var a = tr[e], b = tr[(e + 1) % 3]; k = a < b ? a + ',' + b : b + ',' + a;
        if (cnt[k] !== 1) continue;
        var o = triOrient(P, a, b, p);
        if (Math.abs(o) > 1e-14) keep.push(o > 0 ? [a, b, p] : [b, a, p]);   // a pin ON a hull edge splits it
      } });
      T = keep;
    }
    return T;
  }
  function tinAt(P, T, val, x, y) {
    var best = -Infinity, bv = 0, q;
    for (q = 0; q < T.length; q++) {
      var a = T[q][0], b = T[q][1], c = T[q][2];
      var d = (P[b][1] - P[c][1]) * (P[a][0] - P[c][0]) + (P[c][0] - P[b][0]) * (P[a][1] - P[c][1]);
      var l0 = ((P[b][1] - P[c][1]) * (x - P[c][0]) + (P[c][0] - P[b][0]) * (y - P[c][1])) / d;
      var l1 = ((P[c][1] - P[a][1]) * (x - P[c][0]) + (P[a][0] - P[c][0]) * (y - P[c][1])) / d;
      var l2 = 1 - l0 - l1, v = l0 * val[a] + l1 * val[b] + l2 * val[c], m = Math.min(l0, l1, l2);
      if (m >= -1e-12) return v;
      if (m > best) { best = m; bv = v; }
    }
    return bv;
  }
  function idwAt(P, val, x, y) {
    var sw = 0, s = 0, j;
    for (j = 0; j < P.length; j++) {
      var dx = x - P[j][0], dy = y - P[j][1], d2 = dx * dx + dy * dy;
      if (d2 < 1e-24) return val[j];
      var w = 1 / d2; sw += w; s += w * val[j];
    }
    return s / sw;
  }
  function tpsPhi(d) { return d > 0 ? d * d * Math.log(d) : 0; }
  /* Thin-plate spline through P with values r: [K P; P^T 0][w; a] = [r; 0], Gaussian elimination with partial pivoting. In the shell this is an
     EDIT-TIME solve on the main thread (at most 15 unknowns at the cap); the audio thread only evaluates. A zero right-hand side stays exactly zero
     through elimination, which is why a pin at residual 0 contributes exactly nothing. */
  function tpsSolve(P, r) {
    var m = P.length, n = m + 3, A = [], i, j, c;
    for (i = 0; i < n; i++) A.push(new Float64Array(n + 1));
    for (i = 0; i < m; i++) {
      for (j = 0; j < m; j++) A[i][j] = tpsPhi(Math.hypot(P[i][0] - P[j][0], P[i][1] - P[j][1]));
      A[i][m] = 1; A[i][m + 1] = P[i][0]; A[i][m + 2] = P[i][1]; A[i][n] = r[i];
      A[m][i] = 1; A[m + 1][i] = P[i][0]; A[m + 2][i] = P[i][1];
    }
    for (c = 0; c < n; c++) {
      var p = c;
      for (i = c + 1; i < n; i++) if (Math.abs(A[i][c]) > Math.abs(A[p][c])) p = i;
      if (p !== c) { var t = A[p]; A[p] = A[c]; A[c] = t; }
      var d = A[c][c];
      if (Math.abs(d) < 1e-300) continue;
      for (i = c + 1; i < n; i++) { var f = A[i][c] / d; if (f === 0) continue; for (j = c; j <= n; j++) A[i][j] -= f * A[c][j]; }
    }
    var x = new Float64Array(n);
    for (i = n - 1; i >= 0; i--) { var s = A[i][n]; for (j = i + 1; j < n; j++) s -= A[i][j] * x[j]; x[i] = Math.abs(A[i][i]) < 1e-300 ? 0 : s / A[i][i]; }
    return x;
  }
  function tpsAt(P, co, x, y) {
    var m = P.length, s = co[m] + co[m + 1] * x + co[m + 2] * y, j;
    for (j = 0; j < m; j++) s += co[j] * tpsPhi(Math.hypot(x - P[j][0], y - P[j][1]));
    return s;
  }
  /* c: the four corner values [A,B,C,D]; pins: [[x,y,v],...] in stored order; law in LAWS. (The lab's B232 off-corner rule, proposed and not built, is left out.) */
  function makeSurface(c, pins, law) {
    var base = function (x, y) { var w = weights(x, y), v = 0, k; for (k = 0; k < 4; k++) v += w[k] * c[k]; return v; };
    var n = pins.length, P = CXY.concat(pins.map(function (p) { return [p[0], p[1]]; })), lo = Math.min(c[0], c[1], c[2], c[3]), hi = Math.max(c[0], c[1], c[2], c[3]);
    pins.forEach(function (p) { lo = Math.min(lo, p[2]); hi = Math.max(hi, p[2]); });
    var val = law === 'tin' ? c.concat(pins.map(function (p) { return p[2]; })) : [0, 0, 0, 0].concat(pins.map(function (p) { return p[2] - base(p[0], p[1]); }));
    var R = function () { return 0; }, tri = null, terms = 4;
    if (n && (law === 'tin' || law === 'tind')) { tri = triangulate(P); R = function (x, y) { return tinAt(P, tri, val, x, y); }; terms = tri.length; }
    else if (n && law === 'idw') { R = function (x, y) { return idwAt(P, val, x, y); }; terms = P.length; }
    else if (n && law === 'tps') { var co = tpsSolve(P, val); R = function (x, y) { return tpsAt(P, co, x, y); }; terms = P.length; }
    var raw = law === 'tin' ? function (x, y) { return n ? R(x, y) : base(x, y); } : function (x, y) { return base(x, y) + R(x, y); };
    return { base: base, raw: raw, tri: tri, P: P, lo: lo, hi: hi, n: n, law: law, terms: terms };
  }
  /* What is HEARD from a surface at (x, y). clip bit 1: the law left the range of values the player set (the hull), clamped to it when hullClamp;
     bit 2: the value would have left the PARAMETER's range and was clamped there (always: that clamp is safety, not taste). */
  function surfAt(s, x, y, rmin, rmax, hullClamp) {
    if (isCornerXY(x, y)) { var v0 = s.base(x, y); return { v: v0, raw: v0, clip: 0 }; }
    var raw = s.raw(x, y);
    if (!s.n) return { v: raw, raw: raw, clip: 0 };
    var v = raw, clip = 0, eps = 1e-9 * ((rmax - rmin) || 1);
    if (raw < s.lo - eps || raw > s.hi + eps) { clip |= 1; if (hullClamp) v = Math.min(s.hi, Math.max(s.lo, v)); }
    if (v < rmin || v > rmax) { clip |= 2; v = Math.min(rmax, Math.max(rmin, v)); }
    return { v: v, raw: raw, clip: clip };
  }
  function clipToSquare(p, q) {
    var dx = q[0] - p[0], dy = q[1] - p[1];
    if (Math.hypot(dx, dy) < 1e-9) { dx = 1; dy = 0; }
    var t0 = -Infinity, t1 = Infinity, ax = [[p[0], dx], [p[1], dy]], k;
    for (k = 0; k < 2; k++) {
      var o = ax[k][0], d = ax[k][1];
      if (Math.abs(d) < 1e-12) { if (o < 0 || o > 1) return null; continue; }
      var a = -o / d, b = (1 - o) / d; t0 = Math.max(t0, Math.min(a, b)); t1 = Math.min(t1, Math.max(a, b));
    }
    if (!(t1 > t0)) return null;
    var c = function (v) { return Math.min(1, Math.max(0, v)); };
    return [[c(p[0] + dx * t0), c(p[1] + dy * t0)], [c(p[0] + dx * t1), c(p[1] + dy * t1)]];
  }
  /* the default section line runs through the row's two most distant pins (or its one pin and the pad's centre), clipped to the pad */
  function xsDefault(pins) {
    var p = [0, 0], q = [1, 1], a, b;
    if (pins.length >= 2) { var bd = -1; for (a = 0; a < pins.length; a++) for (b = a + 1; b < pins.length; b++) { var d = Math.hypot(pins[a][0] - pins[b][0], pins[a][1] - pins[b][1]); if (d > bd) { bd = d; p = pins[a]; q = pins[b]; } } }
    else if (pins.length === 1) { p = pins[0]; q = Math.hypot(.5 - p[0], .5 - p[1]) > .05 ? [.5, .5] : [p[0] + 1, p[1]]; }
    return clipToSquare(p, q) || [[0, 0], [1, 1]];
  }
  function pctOfSpan(n) { return n === 0 ? '0%' : n < 1e-5 ? (100 * n).toExponential(0) + '%' : (100 * n).toFixed(n < .1 ? 1 : 0) + '%'; }

  /* ---------- BLEND MAPS: the component ----------
     o: { row: { name, min, max, fmt?(v), corners: [A,B,C,D], pins: [[x, y, v], ...] },
          law?: 'tps' | 'tind' (the setting; default the first of `laws`), clamp?: true (CLAMP TO SET VALUES; on by default),
          laws?: ['tps', 'tind'] (B272: the two laws the setting offers; the first is the default),
          compare?: [] (extra laws drawn as labelled comparisons only, never selectable: a preview passes ['tin', 'idw']),
          maps?: 'value' | 'diff', onChange?(info) }
     Gestures follow the lab (:4000-4090): the pin and the section line's ends follow the pointer (ABSOLUTE, with a grab radius), a pin's value tag is
     dragged up and down (the pad's height is the parameter's range, shift x0.1), double-click on empty pad adds a pin at the value the law already
     gives there, right-click or Delete removes one, alt+wheel on a pin turns its value; arrows move the selected pin (shift fine), alt+up/down its value. */
  function BlendMaps(el, o) {
    var row = null, ruled = (o.laws && o.laws.length ? o.laws : ['tps', 'tind']).slice(), cmp = (o.compare || []).filter(function (l) { return ruled.indexOf(l) < 0; });
    var all = ruled.concat(cmp), REF = ruled[0];
    var st = { law: o.law && ruled.indexOf(o.law) >= 0 ? o.law : REF, clamp: o.clamp !== false, maps: o.maps === 'diff' ? 'diff' : 'value', sel: -1, xs: { auto: true, a: [0, 0], b: [1, 1] }, ver: 0, say: '' };
    var gest = null, tags = [], cache = {}, cacheVer = -1;
    el.classList.add('hd-bm'); el.innerHTML = '';

    /* ----- the row ----- */
    function setRow(r) {
      row = { name: r.name, min: r.min, max: r.max, fmt: r.fmt || null, corners: r.corners.slice(), pins: (r.pins || []).map(function (p) { return p.slice(); }) };
      st.sel = -1; st.xs = { auto: true, a: [0, 0], b: [1, 1] }; st.ver++; st.say = ''; cache = {};
    }
    var span = function () { return (row.max - row.min) || 1; };
    var norm = function (v) { return clamp((v - row.min) / span()); };
    function fmt(v) {
      if (row.fmt) return row.fmt(v);
      var s = span(), d = s >= 100 ? 0 : s >= 10 ? 1 : s >= 1.5 ? 2 : 3; return v.toFixed(d);
    }
    function surf(law, pins) {
      if (pins) return makeSurface(row.corners, pins, law);
      if (!row.pins.length) return null;
      if (cacheVer !== st.ver) { cache = {}; cacheVer = st.ver; }          // a surface is the row's corners and pins at one version; a TPS solve per sample would lie about the cost model
      return cache[law] || (cache[law] = makeSurface(row.corners, row.pins, law));
    }
    /* a row sampled on an M x M grid of cell centres (never a corner, so the corner guard never fires here) */
    function sample(law, M, pins) {
      var sf = surf(law, pins), vals = new Float64Array(M * M), raw = new Float64Array(M * M), clipv = new Uint8Array(M * M), xx, yy;
      for (yy = 0; yy < M; yy++) for (xx = 0; xx < M; xx++) {
        var x = (xx + .5) / M, y = (yy + .5) / M, q = yy * M + xx;
        if (sf) { var r = surfAt(sf, x, y, row.min, row.max, st.clamp); vals[q] = r.v; raw[q] = r.raw; clipv[q] = r.clip; }
        else { var w = weights(x, y), s = 0, k; for (k = 0; k < 4; k++) s += w[k] * row.corners[k]; vals[q] = raw[q] = s; }
      }
      return { M: M, sf: sf, vals: vals, raw: raw, clip: clipv };
    }
    function lawAt(law, x, y) { var sf = surf(law), w = weights(x, y), b = 0, k; for (k = 0; k < 4; k++) b += w[k] * row.corners[k]; return sf ? surfAt(sf, x, y, row.min, row.max, st.clamp).v : b; }
    function line() { return st.xs.auto ? xsDefault(row.pins) : [st.xs.a, st.xs.b]; }

    /* ----- the pin write path: every gesture goes through these, and a refused call writes nothing ----- */
    function nearCorner(x, y) { var k; for (k = 0; k < 4; k++) if (Math.hypot(x - CXY[k][0], y - CXY[k][1]) < BM_GUARD) return k; return -1; }
    function spotOk(x, y, skip) {
      if (!(x >= 0 && x <= 1 && y >= 0 && y <= 1)) return false;
      if (nearCorner(x, y) >= 0) return false;
      for (var j = 0; j < row.pins.length; j++) if (j !== skip && Math.hypot(x - row.pins[j][0], y - row.pins[j][1]) < BM_SNAP) return false;
      return true;
    }
    function pinAdd(x, y, v) {
      var k = nearCorner(x, y);
      if (k >= 0) return 'That is corner ' + 'ABCD'[k] + ' (within ' + BM_GUARD + '): a value there IS the corner’s. Edit the corner instead; a pin never sits on a corner.';
      if (row.pins.length >= BM_CAP) return 'This row already has ' + BM_CAP + ' pins, the per-row limit. Move or delete one.';
      if (!spotOk(x, y)) return 'Too close to another pin of this row. Drag that pin, or pick another spot.';
      row.pins.push([x, y, clamp(v, row.min, row.max)]); st.sel = row.pins.length - 1; st.ver++; return '';
    }
    // a move that would land on a corner or on another pin is refused and writes nothing, so a drag stops at the last legal spot, with the reason
    function pinMove(j, x, y) {
      x = clamp(x); y = clamp(y);
      var k = nearCorner(x, y);
      if (k >= 0) return 'A pin cannot sit on corner ' + 'ABCD'[k] + ': there the value IS the corner’s.';
      if (!spotOk(x, y, j)) return 'Too close to another pin of this row: two pins that close are one position.';
      row.pins[j] = [x, y, row.pins[j][2]]; st.ver++; return '';
    }
    function pinSetValue(j, v) { row.pins[j][2] = clamp(v, row.min, row.max); st.ver++; return ''; }
    function pinDelete(j) { row.pins.splice(j, 1); st.sel = -1; st.ver++; return ''; }
    function changed(type) { if (typeof o.onChange === 'function') o.onChange({ type: type, law: st.law, clamp: st.clamp, pins: row.pins.map(function (p) { return p.slice(); }), selected: st.sel }); }

    /* ----- the DOM ----- */
    var ctl = tag('div', 'hd-row bm-ctl'), top = tag('div', 'hd-fit bm-top'), padCol = tag('div', 'bm-padcol'), mapsCol = tag('div', 'bm-maps');
    var lawPills = tag('div', 'hd-pills'), clampSw = tag('button', 'hd-toggle hd-focus'), mapPills = tag('div', 'hd-pills');
    lawPills.setAttribute('role', 'radiogroup'); lawPills.setAttribute('aria-label', 'Blend law');
    ruled.forEach(function (l) {
      var b = tag('button', 'hd-pill hd-focus'); b.type = 'button'; b.setAttribute('role', 'radio'); b.dataset.law = l; b.innerHTML = '<i class="bm-lk' + LAW_DCLS[l] + '" style="--kc:var(' + LAW_VAR[l] + ')"></i>' + LAW_NAME[l] + (l === REF ? ' · default' : '');
      b.addEventListener('click', function () { setLaw(l); }); lawPills.appendChild(b);
    });
    clampSw.type = 'button'; clampSw.setAttribute('role', 'switch'); clampSw.setAttribute('aria-label', 'Clamp to set values');
    clampSw.addEventListener('click', function () { setClamp(!st.clamp); });
    mapPills.setAttribute('role', 'radiogroup'); mapPills.setAttribute('aria-label', 'The maps show');
    [['value', 'The value'], ['diff', 'Difference from ' + LAW_NAME[REF]]].forEach(function (q) {
      var b = tag('button', 'hd-pill hd-focus', q[1]); b.type = 'button'; b.setAttribute('role', 'radio'); b.dataset.m = q[0];
      b.addEventListener('click', function () { st.maps = q[0]; draw(); }); mapPills.appendChild(b);
    });
    var grp = function (label, node) { var d = tag('div', 'hd-row bm-grp'); d.appendChild(tag('span', 'hd-label', label)); d.appendChild(node); return d; };
    ctl.appendChild(grp('Law', lawPills)); ctl.appendChild(grp('Clamp to set values', clampSw)); ctl.appendChild(grp('Maps show', mapPills));

    var padViz = tag('div', 'hd-viz bm-pad'), padCv = tag('canvas'); padViz.appendChild(padCv);
    padCv.tabIndex = 0; padCv.setAttribute('role', 'application'); padCv.setAttribute('aria-roledescription', 'blend pad'); padCv.classList.add('hd-focus');
    padCv.style.touchAction = 'none';
    var pinRow = tag('div', 'hd-row bm-pins'), pinList = tag('span', 'hd-pills bm-pinlist'), pinKey = '', say = tag('div', 'hd-hint bm-say'); say.setAttribute('aria-live', 'polite');
    var addBtn = tag('button', 'hd-pill hd-focus', 'Add pin'), delBtn = tag('button', 'hd-pill hd-focus', 'Delete pin');
    addBtn.type = 'button'; delBtn.type = 'button';
    padCol.appendChild(tag('div', 'hd-cluster', 'Pad · ' + '')); padCol.firstChild.textContent = 'Pad';
    pinRow.appendChild(pinList); pinRow.appendChild(addBtn); pinRow.appendChild(delBtn);
    padCol.appendChild(padViz); padCol.appendChild(pinRow); padCol.appendChild(say);

    var minis = {};
    all.forEach(function (l) {
      var isCmp = cmp.indexOf(l) >= 0, m = tag('div', 'bm-mini' + (isCmp ? ' cmp' : '')), head = tag(isCmp ? 'div' : 'button', 'bm-mh' + (isCmp ? '' : ' hd-focus'));
      if (!isCmp) { head.type = 'button'; head.setAttribute('aria-label', LAW_NAME[l] + ' (' + LAW_SHORT[l] + '): hear this law'); head.addEventListener('click', function () { setLaw(l); }); }
      head.innerHTML = '<span class="bm-mn"><i class="bm-lk' + LAW_DCLS[l] + '" style="--kc:var(' + LAW_VAR[l] + ')"></i>' + LAW_NAME[l] + '</span><small>' + LAW_SHORT[l] + '</small>'
        + (isCmp ? '<span class="bm-badge">COMPARISON</span>' : l === REF ? '<span class="bm-badge ref">DEFAULT</span>' : '<span class="bm-badge ref">1.0 OPTION</span>');
      var vz = tag('div', 'hd-viz bm-mv'), cv = tag('canvas'); vz.appendChild(cv); cv.setAttribute('role', 'img');
      var tr = tag('div', 'hd-hint bm-tr', LAW_TRAIT[l]), stat = tag('div', 'hd-hint bm-stat');
      m.appendChild(head); m.appendChild(vz); m.appendChild(tr); m.appendChild(stat); mapsCol.appendChild(m);
      minis[l] = { el: m, cv: cv, stat: stat };
    });
    top.appendChild(padCol); top.appendChild(mapsCol);

    var legend = tag('div', 'hd-hint bm-legend');
    legend.innerHTML = '<span><i class="bm-sw al"></i>clipped at the parameter’s range (always clamped)</span>'
      + '<span><i class="bm-sw ca"></i>outside the values you set: flattened when CLAMP is on, heard when it is off</span>'
      + '<span><i class="bm-sw cn"></i>faint lines: contours</span><span><i class="bm-sw tr"></i>dashed: the triangulation (TIN laws; every dashed line is a crease)</span>';
    var xsCtl = tag('div', 'hd-row bm-xsctl'), xsViz = tag('div', 'hd-viz bm-xs'), xsCv = tag('canvas'), xsKey = tag('div', 'hd-hint bm-key'), xsNote = tag('div', 'hd-hint bm-note');
    xsViz.appendChild(xsCv); xsCv.setAttribute('role', 'img');
    var thru = tag('button', 'hd-pill hd-focus', 'Through the pins'), diag = tag('button', 'hd-pill hd-focus', 'A → D');
    thru.type = 'button'; diag.type = 'button';
    xsCtl.appendChild(tag('span', 'hd-cluster', 'Cross-section')); xsCtl.appendChild(thru); xsCtl.appendChild(diag);
    el.appendChild(ctl); el.appendChild(top); el.appendChild(legend); el.appendChild(xsCtl); el.appendChild(xsViz); el.appendChild(xsKey); el.appendChild(xsNote);
    thru.addEventListener('click', function () { st.xs = { auto: true, a: [0, 0], b: [1, 1] }; draw(); });
    diag.addEventListener('click', function () { st.xs = { auto: false, a: [0, 0], b: [1, 1] }; draw(); });
    addBtn.addEventListener('click', function () {
      var spots = [[.5, .5], [.3, .3], [.7, .7], [.3, .7], [.7, .3], [.5, .2], [.5, .8], [.2, .5], [.8, .5]], i;
      for (i = 0; i < spots.length; i++) if (spotOk(spots[i][0], spots[i][1])) break;
      st.say = i < spots.length ? pinAdd(spots[i][0], spots[i][1], lawAt(st.law, spots[i][0], spots[i][1])) : 'No free spot is left on the pad.';
      changed('add'); draw();
    });
    delBtn.addEventListener('click', function () { if (st.sel >= 0) { pinDelete(st.sel); st.say = ''; changed('delete'); draw(); } });

    function setLaw(l) { if (ruled.indexOf(l) < 0) return; st.law = l; changed('law'); draw(); }
    function setClamp(b) { st.clamp = !!b; changed('clamp'); draw(); }

    /* ----- painting ----- */
    function pal(K, host) { return { caution: rgbOf(host, 'caution'), alarm: K.P.alarm, onCorner: rgbOf(host, 'on-corner'), edge: rgbOf(host, 'mark-edge'), corner: [0, 1, 2, 3].map(function (k) { return rgbOf(host, 'c' + 'ABCD'[k]); }) }; }
    /* brightness = value, faint contours every tenth of the range (a crease shows as a kink in them), hatches where a clamp acted:
       alarm "/" at the parameter's range, caution "\" outside the values set */
    function field(K, P, W, Hh, smp) {
      var g = K.g, V = K.V, M = smp.M, cw = W / M, chh = Hh / M, lv = new Int16Array(M * M), q, xx, yy;
      for (q = 0; q < M * M; q++) { var n = norm(smp.vals[q]); lv[q] = Math.min(9, Math.floor(n * 10)); g.fillStyle = K.c('physics', .06 + .86 * n); g.fillRect((q % M) * cw, Math.floor(q / M) * chh, cw + .5, chh + .5); }
      K.scan();
      g.strokeStyle = K.c('grid', .45); g.lineWidth = .8 * V; g.beginPath();
      for (yy = 0; yy < M; yy++) for (xx = 0; xx < M; xx++) {
        q = yy * M + xx;
        if (xx + 1 < M && lv[q] !== lv[q + 1]) { g.moveTo((xx + 1) * cw, yy * chh); g.lineTo((xx + 1) * cw, (yy + 1) * chh); }
        if (yy + 1 < M && lv[q] !== lv[q + M]) { g.moveTo(xx * cw, (yy + 1) * chh); g.lineTo((xx + 1) * cw, (yy + 1) * chh); }
      }
      g.stroke();
      function hatch(bit, col, dir) {
        g.strokeStyle = col; g.lineWidth = 1.1 * V; g.beginPath();
        for (q = 0; q < M * M; q++) {
          if (bit === 2 ? !(smp.clip[q] & 2) : smp.clip[q] !== 1) continue;
          var x0 = (q % M) * cw, y0 = Math.floor(q / M) * chh;
          if (dir > 0) { g.moveTo(x0, y0 + chh); g.lineTo(x0 + cw, y0); } else { g.moveTo(x0, y0); g.lineTo(x0 + cw, y0 + chh); }
        }
        g.stroke();
      }
      hatch(1, rgba(P.caution, .9), -1); hatch(2, rgba(P.alarm, 1), 1);
    }
    // the DIFFERENCE view: a law minus the default law, the value phosphor where it sits above, the meter phosphor below, one scale for all of them
    function diffField(K, W, Hh, M, d, dmax) {
      var g = K.g, cw = W / M, chh = Hh / M, q;
      for (q = 0; q < M * M; q++) { if (!(dmax > 0) || d[q] === 0) continue; g.fillStyle = K.c(d[q] > 0 ? 'value' : 'meter', Math.min(1, .08 + .9 * Math.abs(d[q]) / dmax)); g.fillRect((q % M) * cw, Math.floor(q / M) * chh, cw + .5, chh + .5); }
      K.scan();
    }
    function triangulation(K, W, Hh, sf) {
      if (!sf || !sf.tri) return;
      var g = K.g, V = K.V;
      g.strokeStyle = K.c('grid', .7); g.lineWidth = .9 * V; g.setLineDash([2.5 * V, 2.5 * V]); g.beginPath();
      sf.tri.forEach(function (t) { for (var e = 0; e < 3; e++) { var a = sf.P[t[e]], b = sf.P[t[(e + 1) % 3]]; g.moveTo(a[0] * W, a[1] * Hh); g.lineTo(b[0] * W, b[1] * Hh); } });
      g.stroke(); g.setLineDash([]);
    }
    function sectionLine(K, W, Hh, handles) {
      var g = K.g, V = K.V, ab = line(), a = ab[0], b = ab[1];
      if (handles) { g.strokeStyle = K.c('ground', .7); g.lineWidth = 4 * V; g.beginPath(); g.moveTo(a[0] * W, a[1] * Hh); g.lineTo(b[0] * W, b[1] * Hh); g.stroke(); }   // an underlay, so the dashes read on the map
      g.strokeStyle = K.c('marker'); g.lineWidth = (handles ? 1.4 : 1) * V; g.setLineDash([5 * V, 3 * V]);
      g.beginPath(); g.moveTo(a[0] * W, a[1] * Hh); g.lineTo(b[0] * W, b[1] * Hh); g.stroke(); g.setLineDash([]);
      if (!handles) return;
      g.font = '700 ' + (9 * V) + 'px ' + monoOf(padCv); g.textAlign = 'center'; g.textBaseline = 'middle';
      [a, b].forEach(function (p, e) {
        g.beginPath(); g.arc(p[0] * W, p[1] * Hh, 6.5 * V, 0, 2 * Math.PI); g.fillStyle = K.c('ground'); g.fill(); g.strokeStyle = K.c('marker'); g.lineWidth = 1.6 * V; g.stroke();
        g.fillStyle = K.c('marker'); g.fillText(e ? '›' : '‹', p[0] * W, p[1] * Hh + .5 * V);
      });
      g.textAlign = 'left';
    }
    /* a pin: a diamond in the value phosphor, with its number and value. On the pad the tag is also the value handle. */
    function pins(K, W, Hh, withTags) {
      var g = K.g, V = K.V, L = row.pins;
      if (withTags) tags = [];
      g.font = '700 ' + (9 * V) + 'px ' + monoOf(padCv); g.textBaseline = 'middle'; g.textAlign = 'left';
      var dr = withTags && gest && gest.kind === 'move' ? gest : null;
      if (dr && L[dr.j]) {   // a pin being dragged leaves a hollow ghost where it started, joined by a dashed trail
        var p = L[dr.j], gx = dr.from[0] * W, gy = dr.from[1] * Hh, r = 5 * V;
        g.strokeStyle = K.c('value'); g.lineWidth = 1.2 * V; g.setLineDash([3 * V, 3 * V]); g.beginPath(); g.moveTo(gx, gy); g.lineTo(p[0] * W, p[1] * Hh); g.stroke(); g.setLineDash([]);
        g.beginPath(); g.moveTo(gx, gy - r); g.lineTo(gx + r, gy); g.lineTo(gx, gy + r); g.lineTo(gx - r, gy); g.closePath(); g.stroke();
      }
      L.forEach(function (p, j) {
        var x = p[0] * W, y = p[1] * Hh, hot = j === st.sel || (gest && gest.j === j), r = (hot ? 7 : 5.5) * V;
        g.beginPath(); g.moveTo(x, y - r); g.lineTo(x + r, y); g.lineTo(x, y + r); g.lineTo(x - r, y); g.closePath();
        g.fillStyle = K.c('value'); g.fill(); g.strokeStyle = K.c('ground'); g.lineWidth = 1.4 * V; g.stroke();
        if (!withTags) return;
        var t = (j + 1) + ' ' + fmt(p[2]), tw = g.measureText(t).width + 18 * V, th = 15 * V, tx = x + r + 4 * V, ty = y - th / 2;
        if (tx + tw > W - 2 * V) tx = x - r - 4 * V - tw;
        ty = Math.min(Hh - th - 2 * V, Math.max(2 * V, ty));
        g.fillStyle = K.c('ground'); g.fillRect(tx, ty, tw, th);
        g.strokeStyle = K.c('value'); g.lineWidth = (hot ? 1.8 : 1.1) * V; g.strokeRect(tx, ty, tw, th);
        g.fillStyle = K.c('value'); g.fillText(t, tx + 4 * V, ty + th / 2 + .5 * V);
        var gx = tx + tw - 7 * V, gy = ty + th / 2;   // the grip: up and down, the tag drags the value
        g.beginPath(); g.moveTo(gx, gy - 5 * V); g.lineTo(gx + 3 * V, gy - 1.5 * V); g.lineTo(gx - 3 * V, gy - 1.5 * V); g.closePath();
        g.moveTo(gx, gy + 5 * V); g.lineTo(gx + 3 * V, gy + 1.5 * V); g.lineTo(gx - 3 * V, gy + 1.5 * V); g.closePath(); g.fill();
        var padY = Math.max(0, (28 * V - th) / 2);   // the hit box is the house 28 px tall
        tags.push({ j: j, x0: tx / W, x1: (tx + tw) / W, y0: (ty - padY) / Hh, y1: (ty + th + padY) / Hh });
      });
    }
    function corners(K, P, W, Hh) {
      var g = K.g, V = K.V;
      g.font = '700 ' + (9 * V) + 'px ' + monoOf(padCv); g.textBaseline = 'middle'; g.textAlign = 'left';
      CXY.forEach(function (c, k) {
        var w = 28 * V, h = 18 * V, x0 = c[0] ? W - w - 4 * V : 4 * V, y0 = c[1] ? Hh - h - 4 * V : 4 * V;
        g.fillStyle = rgba(P.corner[k], 1); g.fillRect(x0, y0, w, h); g.strokeStyle = rgba(P.edge, 1); g.lineWidth = 1.5 * V; g.strokeRect(x0, y0, w, h);
        g.fillStyle = rgba(P.onCorner, 1); glyphPath(g, k, x0 + 9 * V, y0 + h / 2, 4.6 * V); g.fill(); g.fillText('ABCD'[k], x0 + 17 * V, y0 + h / 2 + .5 * V);
      });
    }
    function paintPad(P) {
      var K = Viz.kit(padCv), W = K.W, Hh = K.H; K.tube();
      var smp = sample(st.law, 56);
      field(K, P, W, Hh, smp); triangulation(K, W, Hh, smp.sf); sectionLine(K, W, Hh, true); corners(K, P, W, Hh); pins(K, W, Hh, true);
      padCv.setAttribute('aria-label', 'Blend pad for ' + row.name + ' under ' + LAW_NAME[st.law] + (st.clamp ? ' with the clamp' : ', unclamped') + ', ' + row.pins.length + ' pin' + (row.pins.length === 1 ? '' : 's')
        + (row.pins.length ? ': ' + row.pins.map(function (p, j) { return (j + 1) + ' at ' + p[0].toFixed(2) + ', ' + p[1].toFixed(2) + ' = ' + fmt(p[2]); }).join('; ') : '') + '. Select a pin with its button, then the arrow keys move it.');
    }
    function statLines(l, smp, dm) {
      var Mn = smp.M, c2 = 0, c1 = 0, pk = 0, q, sp = span(), pc = function (n) { return (100 * n / (Mn * Mn)).toFixed(0) + '%'; };
      for (q = 0; q < Mn * Mn; q++) { if (smp.clip[q] & 2) c2++; if (smp.clip[q] & 1) c1++; if (smp.sf && smp.sf.n) pk = Math.max(pk, smp.sf.lo - smp.raw[q], smp.raw[q] - smp.sf.hi); }
      return [l === REF ? 'the reference for difference' : 'differs from ' + LAW_NAME[REF] + ' by up to ' + pctOfSpan(dm / sp) + ' of range',
        c2 ? 'clipped on ' + pc(c2) + ' of the pad' : 'never clipped',
        c1 ? (st.clamp && l !== 'tin' ? 'flattened' : 'past set values') + ' on ' + pc(c1) + ' · peak +' + (100 * pk / sp).toFixed(0) + '% of span' : 'inside the values set'];
    }
    function paintMinis(P) {
      var M = 44, smps = {}, diffs = {}, dmax = 0;
      all.forEach(function (l) { smps[l] = sample(l, M); });
      all.forEach(function (l) { var d = new Float64Array(M * M), q; for (q = 0; q < M * M; q++) { d[q] = smps[l].vals[q] - smps[REF].vals[q]; dmax = Math.max(dmax, Math.abs(d[q])); } diffs[l] = d; });
      all.forEach(function (l) {
        var m = minis[l], K = Viz.kit(m.cv), W = K.W, Hh = K.H, g = K.g, V = K.V; K.tube();
        var smp = smps[l], dm = 0, q; for (q = 0; q < M * M; q++) dm = Math.max(dm, Math.abs(diffs[l][q]));
        if (st.maps === 'diff') diffField(K, W, Hh, M, diffs[l], dmax); else { field(K, P, W, Hh, smp); triangulation(K, W, Hh, smp.sf); }
        sectionLine(K, W, Hh, false); pins(K, W, Hh, false);
        var lines = statLines(l, smp, dm);
        m.stat.innerHTML = lines.map(esc).join('<br>');
        m.cv.setAttribute('aria-label', LAW_NAME[l] + (cmp.indexOf(l) >= 0 ? ' (comparison)' : '') + ' map of ' + row.name + ': ' + lines.join('; '));
        m.el.classList.toggle('on', l === st.law);
      });
    }
    /* THE CROSS-SECTION: x is the position along the pad's line (its ‹ end on the left), y the parameter's own value. Drawn back to front: the range limits
       (alarm) and the band past them, the set-value limits (caution, dashed), today's blend (dotted, what the pins changed), each law's RAW curve thin where a
       clamp cut it with the cut region shaded, each law's HEARD curve on top (the law in use, thickest), the pins that sit on the line. */
    function paintSection(P) {
      var K = Viz.kit(xsCv), g = K.g, W = K.W, Hh = K.H, V = K.V; K.tube();
      var ab = line(), a = ab[0], b = ab[1], n = 240, runs = all.map(function (l) {
        var sf = surf(l), pts = [], s; for (s = 0; s <= n; s++) {
          var t = s / n, x = a[0] + (b[0] - a[0]) * t, y = a[1] + (b[1] - a[1]) * t, w = weights(x, y), bil = 0, k; for (k = 0; k < 4; k++) bil += w[k] * row.corners[k];
          if (sf) { var r = surfAt(sf, x, y, row.min, row.max, st.clamp); pts.push({ t: t, v: r.v, raw: r.raw, clip: r.clip, base: bil }); } else pts.push({ t: t, v: bil, raw: bil, clip: 0, base: bil });
        }
        return { l: l, sf: sf, pts: pts };
      });
      var lo = row.min, hi = row.max; runs.forEach(function (run) { run.pts.forEach(function (p) { lo = Math.min(lo, p.raw); hi = Math.max(hi, p.raw); }); });
      var padV = .08 * ((hi - lo) || 1); lo -= padV; hi += padV;
      var ML = 44 * V, MR = 10 * V, MT = 10 * V, MB = 20 * V, X = function (t) { return ML + t * (W - ML - MR); }, Y = function (v) { return MT + (hi - v) / ((hi - lo) || 1) * (Hh - MT - MB); };
      g.font = '500 ' + (8.5 * V) + 'px ' + monoOf(xsCv); g.textBaseline = 'middle'; g.textAlign = 'right';
      g.fillStyle = rgba(P.alarm, .1 * K.sc.alpha);
      if (Y(row.max) > MT) g.fillRect(ML, MT, W - ML - MR, Y(row.max) - MT);
      if (Y(row.min) < Hh - MB) g.fillRect(ML, Y(row.min), W - ML - MR, Hh - MB - Y(row.min));
      g.strokeStyle = rgba(P.alarm, .9); g.lineWidth = 1.2 * V;
      [row.min, row.max].forEach(function (lim) { g.beginPath(); g.moveTo(ML, Y(lim)); g.lineTo(W - MR, Y(lim)); g.stroke(); });
      g.fillStyle = rgba(P.alarm, 1); g.fillText(fmt(row.max), ML - 4 * V, Y(row.max)); g.fillText(fmt(row.min), ML - 4 * V, Y(row.min));
      var sf0 = runs[0].sf;
      if (sf0 && sf0.n) {   // the values the player SET (corners + pins): the hull the clamp holds to
        g.strokeStyle = rgba(P.caution, .95); g.lineWidth = V; g.setLineDash([4 * V, 3 * V]);
        [sf0.lo, sf0.hi].forEach(function (lim) { g.beginPath(); g.moveTo(ML, Y(lim)); g.lineTo(W - MR, Y(lim)); g.stroke(); }); g.setLineDash([]);
        g.fillStyle = K.c('marker'); g.fillText('set ' + fmt(sf0.hi), ML - 4 * V, Y(sf0.hi) + (Math.abs(Y(sf0.hi) - Y(row.max)) < 10 * V ? 9 * V : 0));
        g.fillText('set ' + fmt(sf0.lo), ML - 4 * V, Y(sf0.lo) - (Math.abs(Y(sf0.lo) - Y(row.min)) < 10 * V ? 9 * V : 0));
      }
      g.fillStyle = K.c('grid'); g.textAlign = 'left'; g.fillText('‹ ' + a[0].toFixed(2) + ', ' + a[1].toFixed(2), ML, Hh - 8 * V);
      g.textAlign = 'right'; g.fillText(b[0].toFixed(2) + ', ' + b[1].toFixed(2) + ' ›', W - MR, Hh - 8 * V);
      var dx = b[0] - a[0], dy = b[1] - a[1], L2 = dx * dx + dy * dy || 1;
      var proj = function (px, py) { var t = ((px - a[0]) * dx + (py - a[1]) * dy) / L2; return { t: t, d: Math.hypot(px - (a[0] + dx * t), py - (a[1] + dy * t)) }; };
      CXY.forEach(function (c, k) {   // the corners the line passes, as their glyph and letter in the screen's physics ink (ADR-259 r4: no corner hues inside a glass)
        var pr = proj(c[0], c[1]); if (!(pr.d < .02 && pr.t > -.01 && pr.t < 1.01)) return;
        var cx = X(clamp(pr.t)); g.fillStyle = K.c('physics'); glyphPath(g, k, cx - 7 * V, Hh - 8 * V, 3.6 * V); g.fill(); g.textAlign = 'left'; g.fillText('ABCD'[k], cx - 2 * V, Hh - 8 * V);
      });
      g.strokeStyle = K.c('grid', .9); g.lineWidth = 1.2 * V; g.setLineDash([1.5 * V, 2.5 * V]); g.beginPath();   // today's blend: what the row would be with no pins
      runs[0].pts.forEach(function (p, s) { if (s) g.lineTo(X(p.t), Y(p.base)); else g.moveTo(X(p.t), Y(p.base)); }); g.stroke(); g.setLineDash([]);
      runs.forEach(function (run) {   // overshoot: where a clamp cut the law, shade raw to heard in the clamp's status colour
        for (var s = 1; s < run.pts.length; s++) {
          var p = run.pts[s], q = run.pts[s - 1]; if (!p.clip && !q.clip) continue;
          g.fillStyle = rgba(((p.clip & 2) || (q.clip & 2)) ? P.alarm : P.caution, .22 * K.sc.alpha);
          g.beginPath(); g.moveTo(X(q.t), Y(q.raw)); g.lineTo(X(p.t), Y(p.raw)); g.lineTo(X(p.t), Y(p.v)); g.lineTo(X(q.t), Y(q.v)); g.closePath(); g.fill();
        }
      });
      var order = all.filter(function (l) { return l !== st.law; }).concat([st.law]);
      order.forEach(function (l) {
        var run = runs[all.indexOf(l)];
        if (run.pts.some(function (p) { return p.clip; })) {   // the raw curve where it differs: thin, the same ink, dotted
          g.strokeStyle = K.c(LAW_INK[l], .55); g.lineWidth = V; g.setLineDash([2 * V, 2 * V]); g.beginPath();
          run.pts.forEach(function (p, s) { if (s) g.lineTo(X(p.t), Y(p.raw)); else g.moveTo(X(p.t), Y(p.raw)); }); g.stroke();
        }
        g.strokeStyle = K.c(LAW_INK[l]); g.lineWidth = (l === st.law ? 3 : 1.8) * V; g.setLineDash(LAW_DASH[l].map(function (d) { return d * V; })); g.beginPath();
        run.pts.forEach(function (p, s) { if (s) g.lineTo(X(p.t), Y(p.v)); else g.moveTo(X(p.t), Y(p.v)); }); g.stroke(); g.setLineDash([]);
      });
      var onLine = 0; g.textAlign = 'left'; g.textBaseline = 'bottom';
      row.pins.forEach(function (p, j) {   // pins ON the line (within 0.03): every law must pass through them
        var pr = proj(p[0], p[1]); if (pr.d > .03 || pr.t < -.01 || pr.t > 1.01) return; onLine++;
        var x = X(pr.t), y = Y(p[2]), rr = (j === st.sel ? 6 : 5) * V;
        g.beginPath(); g.moveTo(x, y - rr); g.lineTo(x + rr, y); g.lineTo(x, y + rr); g.lineTo(x - rr, y); g.closePath();
        g.fillStyle = K.c('value'); g.fill(); g.strokeStyle = K.c('ground'); g.lineWidth = 1.2 * V; g.stroke();
        g.fillStyle = K.c('value'); g.fillText((j + 1) + ' ' + fmt(p[2]), x + rr + 2 * V, y - 2 * V);
      });
      xsKey.innerHTML = all.map(function (l) { return '<span><i class="bm-lk' + LAW_DCLS[l] + '" style="--kc:var(' + LAW_VAR[l] + ')"></i><b>' + LAW_NAME[l] + '</b>' + (l === st.law ? ' (heard)' : '') + (cmp.indexOf(l) >= 0 ? ' (comparison)' : '') + '</span>'; }).join('')
        + '<span><i class="bm-lk o thin" style="--kc:var(--scr-grid)"></i>today’s blend, no pins</span><span><i class="bm-lk thin" style="--kc:var(--alarm)"></i>range</span>'
        + '<span><i class="bm-lk d thin" style="--kc:var(--caution)"></i>values you set</span><span><i class="bm-sw ca"></i>overshoot a clamp cut</span>';
      var sp = span(), worst = [];
      runs.forEach(function (run) {
        var pk = 0; if (run.sf && run.sf.n) run.pts.forEach(function (p) { pk = Math.max(pk, run.sf.lo - p.raw, p.raw - run.sf.hi); });
        if (pk > 1e-3 * sp) worst.push(LAW_NAME[run.l] + ' +' + pctOfSpan(pk / sp) + ' of range');
      });
      xsNote.textContent = row.name + ' along the dashed line on the pad (' + onLine + ' of ' + row.pins.length + ' pin' + (row.pins.length === 1 ? '' : 's') + ' on it' + (st.xs.auto ? ', line through the pins' : ', line placed by hand') + '). '
        + (worst.length ? 'Past the values you set, along this line: ' + worst.join(' · ') + (st.clamp ? ' — clamped flat (shaded), because CLAMP TO SET VALUES is on.' : ' — heard, because CLAMP is off.') : 'No law leaves the values you set along this line.');
      xsCv.setAttribute('aria-label', 'Cross-section of ' + row.name + ' along the pad line from ' + a.map(function (v) { return v.toFixed(2); }).join(', ') + ' to ' + b.map(function (v) { return v.toFixed(2); }).join(', ') + ' under ' + all.map(function (l) { return LAW_NAME[l]; }).join(', ') + '. ' + (worst.length ? 'Past the set values: ' + worst.join(', ') + '.' : 'No law leaves the set values.'));
    }
    function sync() {
      lawPills.querySelectorAll('button').forEach(function (b) { b.setAttribute('aria-checked', String(b.dataset.law === st.law)); b.tabIndex = b.dataset.law === st.law ? 0 : -1; });
      mapPills.querySelectorAll('button').forEach(function (b) { b.setAttribute('aria-checked', String(b.dataset.m === st.maps)); });
      clampSw.setAttribute('aria-checked', String(st.clamp));
      var key = row.pins.map(function (p) { return fmt(p[2]); }).join('|');
      if (key !== pinKey) {                                                // rebuilt only when the pins' labels change, so a focused pin button keeps its focus
        pinKey = key; pinList.innerHTML = '';
        row.pins.forEach(function (p, j) {
          var b = tag('button', 'hd-pill hd-focus bm-pin'); b.type = 'button';
          b.innerHTML = '<svg viewBox="0 0 10 10" width="9" height="9" aria-hidden="true"><path d="M5 .5 9.5 5 5 9.5 .5 5Z" fill="currentColor"/></svg>' + (j + 1) + ' ' + esc(fmt(p[2]));
          b.setAttribute('aria-label', 'Pin ' + (j + 1) + ', ' + fmt(p[2]));
          b.addEventListener('click', function () { st.sel = st.sel === j ? -1 : j; draw(true); }); pinList.appendChild(b);
        });
      }
      pinList.querySelectorAll('button').forEach(function (b, j) { b.setAttribute('aria-pressed', String(j === st.sel)); });
      delBtn.disabled = st.sel < 0;
      say.textContent = st.say || (row.pins.length ? 'Drag a pin to move it, its tag up or down for its value; double-click the pad to add one, right-click a pin to delete it.' : 'No pins: this is today’s blend. Double-click the pad to add one.');
    }
    var sig = '';
    function draw(force) {
      var s = [st.ver, st.law, st.clamp, st.maps, st.sel, st.say, st.xs.auto, st.xs.a, st.xs.b, gest ? gest.kind : '', padCv.clientWidth, xsCv.clientWidth, themeSig(el)].join('|');
      if (!force && s === sig && !gest) return; sig = s;
      sync();
      var P = pal(Viz.kit(padCv), padCv.parentElement);
      paintPad(P); paintMinis(P); paintSection(P);
    }

    /* ----- gestures (the lab's padDown / padMove / padUp / padDbl / padCtx / padWheel) ----- */
    function pxy(e) { var r = padCv.getBoundingClientRect(); return { x: clamp((e.clientX - r.left) / (r.width || 1)), y: clamp((e.clientY - r.top) / (r.height || 1)), w: r.width || 1, h: r.height || 1 }; }
    function pinUnder(p) {
      var best = -1, bd = HIT_PX; row.pins.forEach(function (q, j) { var d = Math.hypot((q[0] - p.x) * p.w, (q[1] - p.y) * p.h); if (d < bd) { bd = d; best = j; } }); return best;
    }
    function tagUnder(p) { for (var k = tags.length - 1; k >= 0; k--) { var t = tags[k]; if (p.x >= t.x0 && p.x <= t.x1 && p.y >= t.y0 && p.y <= t.y1) return t.j; } return -1; }
    function endUnder(p) { var ab = line(), e; for (e = 0; e < 2; e++) if (Math.hypot((ab[e][0] - p.x) * p.w, (ab[e][1] - p.y) * p.h) < HIT_PX) return e; return -1; }
    padCv.addEventListener('pointerdown', function (e) {
      if (e.button !== 0) return;
      var p = pxy(e); try { padCv.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ }
      var tg = tagUnder(p), j = tg >= 0 ? tg : pinUnder(p); st.say = '';
      if (j >= 0) { st.sel = j; var P0 = row.pins[j]; gest = (tg >= 0 || e.altKey) ? { kind: 'value', j: j, y0: p.y, v0: P0[2] } : { kind: 'move', j: j, from: [P0[0], P0[1]] }; }
      else {
        var end = endUnder(p);
        if (end >= 0) { if (st.xs.auto) { var ab = line(); st.xs = { auto: false, a: ab[0].slice(), b: ab[1].slice() }; } gest = { kind: 'xs', end: end }; }
        else { gest = { kind: 'none' }; st.sel = -1; }
      }
      draw(true);
    });
    padCv.addEventListener('pointermove', function (e) {
      var p = pxy(e);
      if (!gest) { padCv.style.cursor = tagUnder(p) >= 0 ? 'ns-resize' : (pinUnder(p) >= 0 || endUnder(p) >= 0 ? 'grab' : 'default'); return; }
      if (gest.kind === 'move') { var m = pinMove(gest.j, p.x, p.y); st.say = m; if (!m) changed('move'); }   // refused: the pin stays at its last legal spot
      else if (gest.kind === 'value') { pinSetValue(gest.j, gest.v0 + (gest.y0 - p.y) * span() * (e.shiftKey ? .1 : 1)); changed('value'); }
      else if (gest.kind === 'xs') st.xs[gest.end ? 'b' : 'a'] = [p.x, p.y];
      else return;
      draw(true);
    });
    function up(e) { if (e && e.pointerId !== undefined && padCv.hasPointerCapture && padCv.hasPointerCapture(e.pointerId)) padCv.releasePointerCapture(e.pointerId); if (!gest) return; gest = null; draw(true); }
    padCv.addEventListener('pointerup', up); padCv.addEventListener('pointercancel', up); padCv.addEventListener('lostpointercapture', up);
    padCv.addEventListener('dblclick', function (e) {
      var p = pxy(e); if (tagUnder(p) >= 0 || pinUnder(p) >= 0) return;
      st.say = pinAdd(p.x, p.y, lawAt(st.law, p.x, p.y)); changed('add'); draw(true);   // the value the law already gives there: under TPS·Δ the new pin changes nothing until its value is moved
    });
    padCv.addEventListener('contextmenu', function (e) {
      var p = pxy(e), tg = tagUnder(p), j = tg >= 0 ? tg : pinUnder(p); if (j < 0) return;   // off a pin the browser's menu is left alone
      e.preventDefault(); pinDelete(j); st.say = ''; changed('delete'); draw(true);
    });
    padCv.addEventListener('wheel', function (e) {
      if (!e.altKey) return;   // a plain wheel scrolls the page; alt adjusts (B301)
      var p = pxy(e), tg = tagUnder(p), j = tg >= 0 ? tg : pinUnder(p); if (j < 0) return;
      e.preventDefault(); st.sel = j; pinSetValue(j, row.pins[j][2] + span() * (e.shiftKey ? .001 : .01) * (e.deltaY < 0 ? 1 : -1)); changed('value'); draw(true);
    }, { passive: false });
    padCv.addEventListener('keydown', function (e) {
      var j = st.sel, k = e.key; if (j < 0 || !row.pins[j]) return;
      if (k === 'Delete' || k === 'Backspace') { e.preventDefault(); pinDelete(j); st.say = ''; changed('delete'); draw(true); return; }
      var dir = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }[k]; if (!dir) return; e.preventDefault();
      var step = e.shiftKey ? .0025 : .01;
      if (e.altKey) { pinSetValue(j, row.pins[j][2] - dir[1] * span() * step); changed('value'); }
      else { st.say = pinMove(j, row.pins[j][0] + dir[0] * step, row.pins[j][1] + dir[1] * step); changed('move'); }
      draw(true);
    });

    setRow(o.row); draw(true); watch(el, function () { draw(); });
    return {
      law: function () { return st.law; }, setLaw: setLaw, clamp: function () { return st.clamp; }, setClamp: setClamp,
      row: function () { return { name: row.name, min: row.min, max: row.max, corners: row.corners.slice(), pins: row.pins.map(function (p) { return p.slice(); }) }; },
      setRow: function (r) { setRow(r); draw(true); }, render: function () { draw(true); }, line: function (a, b) { st.xs = { auto: false, a: a, b: b }; draw(); },
      /* a law's measurements on a 44 x 44 grid: the share of the pad clipped at the range and outside the set values, and the peak past them (fractions of the pad / of the range) */
      stats: function (l) { var s = sample(l || st.law, 44), M = 44, c1 = 0, c2 = 0, pk = 0, q; for (q = 0; q < M * M; q++) { if (s.clip[q] & 1) c1++; if (s.clip[q] & 2) c2++; if (s.sf && s.sf.n) pk = Math.max(pk, s.sf.lo - s.raw[q], s.raw[q] - s.sf.hi); } return { range: c2 / (M * M), set: c1 / (M * M), peak: pk / span() }; },
      value: function (l, x, y) { return lawAt(l || st.law, x, y); }
    };
  }
  /* the law math, exposed so a consumer's tests (and this component's own checks) run the same functions the pictures run */
  BlendMaps.math = { surface: makeSurface, at: surfAt, triangulate: triangulate, weights: weights, laws: LAWS.slice(), name: LAW_NAME };
  H.BlendMaps = BlendMaps;
})(window.Horde);

/* =====================================================================
   DENSITY AND HIERARCHY (B589; the human's notes of 2026-10-30 on the GUI 3 shell). The page bar in two levels, the
   section header that replaces nested cards, and the mixer as a console of channel strips. Layout and behaviour only:
   every value they show is the consumer's (the engine's in GUI 3), and the previews label theirs SAMPLE.
   ===================================================================== */
(function (H) {
  'use strict';
  function tag(t, cls, txt) { var e = document.createElement(t); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; }
  function keysOn(o) { return typeof o.keys === 'function' ? !!o.keys() : !!o.keys; }
  function fire(fn, a, b, c) { if (typeof fn === 'function') fn(a, b, c); }
  var uid = 0;
  /* ROVING FOCUS for a tablist or radiogroup (accessibility.md): one tab stop per group, and only in keyboard mode;
     Left/Right (and Up/Down inside a radiogroup) move AND select, Home/End jump. Returns the handler. */
  function roving(list, sel, pick) {
    return function (e) {
      var i = list.indexOf(document.activeElement); if (i < 0) return;
      var k = e.key, n = list.length, j = k === 'ArrowRight' ? (i + 1) % n : k === 'ArrowLeft' ? (i - 1 + n) % n : k === 'Home' ? 0 : k === 'End' ? n - 1 : -1;
      if (j < 0) return; e.preventDefault(); pick(j); list[j].focus();
    };
  }
  /* compact pills as a radiogroup: the house .hd-pill with .hd-sm, so the face is 20 px and the hit 28 */
  function pillGroup(host, labels, cur, label, onPick, o) {
    host.classList.add('hd-pills', 'hd-sm'); host.setAttribute('role', 'radiogroup'); host.setAttribute('aria-label', label);
    var bs = labels.map(function (t, i) { var b = tag('button', 'hd-pill hd-sm hd-focus', t); b.type = 'button'; b.setAttribute('role', 'radio');
      b.addEventListener('click', function () { set(i); fire(onPick, i); }); host.appendChild(b); return b; });
    function set(i) { cur = i; bs.forEach(function (b, k) { b.setAttribute('aria-checked', String(k === i)); b.tabIndex = keysOn(o) && k === i ? 0 : -1; }); }
    host.addEventListener('keydown', function (e) { if (!keysOn(o)) return;
      var i = bs.indexOf(document.activeElement), d = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
      if (i < 0 || !d) return; e.preventDefault(); var j = (i + d + bs.length) % bs.length; set(j); fire(onPick, j); bs[j].focus(); });
    set(cur); return { set: set, get: function () { return cur; }, buttons: bs };
  }

  /* ---------- PAGE NAV: two levels. The top row is the page GROUPS (Main, Osc, FX, Mod, Morph, Mix, Hist, Set); under it,
     the selected group's PAGES (for Osc: Dashboard, Osc 1, Osc 2, Sub, Noise, Filters). A group remembers the page it was
     last on, so Osc → Mix → Osc returns to Osc 2. A one-page group still shows its page row (its one tab), so the bar never
     changes height. Both rows are role="tablist"; tabs take focus only in keyboard mode (ADR-259 r6), where Left/Right
     move and select, Down goes from a group to its pages and Up comes back. ---------- */
  function PageNav(el, o) {
    o = o || {}; var groups = o.groups || [], id = 'hd-pn' + (++uid), last = {};
    var cur = o.value || { group: groups[0] && groups[0].id, page: null };
    el.classList.add('hd-pnav'); el.innerHTML = '';
    var top = tag('div', 'pn-groups'), sub = tag('div', 'pn-pages');
    top.setAttribute('role', 'tablist'); top.setAttribute('aria-label', o.label || 'Page groups'); sub.id = id + '-pages';
    sub.setAttribute('role', 'tablist'); el.appendChild(top); el.appendChild(sub);
    function grp(gid) { for (var i = 0; i < groups.length; i++) if (groups[i].id === gid) return groups[i]; return groups[0]; }
    function pagesOf(gr) { return gr.pages && gr.pages.length ? gr.pages : [{ id: gr.id, label: gr.label }]; }
    var gb = groups.map(function (gr) {
      var b = tag('button', 'pn-g hd-focus', gr.label); b.type = 'button'; b.setAttribute('role', 'tab'); b.id = id + '-g-' + gr.id; b.setAttribute('aria-controls', sub.id);
      b.addEventListener('click', function () { set(gr.id); }); top.appendChild(b); return b;
    });
    var pb = [];
    top.addEventListener('keydown', function (e) {
      if (!keysOn(o)) return;
      if (e.key === 'ArrowDown') { var s = sub.querySelector('[aria-selected="true"]'); if (s) { e.preventDefault(); s.focus(); } return; }
      roving(gb, null, function (j) { set(groups[j].id); })(e);
    });
    sub.addEventListener('keydown', function (e) {
      if (!keysOn(o)) return;
      if (e.key === 'ArrowUp') { var s = top.querySelector('[aria-selected="true"]'); if (s) { e.preventDefault(); s.focus(); } return; }
      roving(pb, null, function (j) { var gr = grp(cur.group); set(gr.id, pagesOf(gr)[j].id); })(e);
    });
    function drawPages(gr) {
      sub.innerHTML = ''; sub.setAttribute('aria-label', gr.label + ' pages');
      pb = pagesOf(gr).map(function (pg) {
        var b = tag('button', 'pn-p hd-focus'); b.type = 'button'; b.setAttribute('role', 'tab'); b.dataset.id = pg.id; b.id = id + '-p-' + pg.id;
        if (o.panel) b.setAttribute('aria-controls', o.panel);
        if (pg.on !== undefined) { var d = tag('span', 'pn-dot' + (pg.on ? ' on' : '')); d.setAttribute('aria-hidden', 'true'); b.appendChild(d); }
        b.appendChild(document.createTextNode(pg.label));
        if (pg.on !== undefined) b.setAttribute('aria-label', pg.label + (pg.on ? ', on' : ', off'));
        b.addEventListener('click', function () { set(gr.id, pg.id); }); sub.appendChild(b); return b;
      });
    }
    var drawn = null;
    function sync() {
      var gr = grp(cur.group), ks = keysOn(o);
      gb.forEach(function (b, i) { var on = groups[i] === gr; b.setAttribute('aria-selected', String(on)); b.tabIndex = ks && on ? 0 : -1; });
      if (drawn !== gr) { drawPages(gr); drawn = gr; }
      pb.forEach(function (b) { var on = b.dataset.id === cur.page; b.setAttribute('aria-selected', String(on)); b.tabIndex = ks && on ? 0 : -1; });
    }
    function set(gid, pid, quiet) {
      var gr = grp(gid), ps = pagesOf(gr);
      if (pid === undefined || pid === null) pid = last[gr.id] || ps[0].id;
      if (!ps.some(function (q) { return q.id === pid; })) pid = ps[0].id;
      var changed = gr.id !== cur.group || pid !== cur.page;
      cur = { group: gr.id, page: pid }; last[gr.id] = pid; sync();
      if (changed && !quiet) fire(o.onChange, gr.id, pid);
    }
    set(cur.group, cur.page, true);
    return { set: function (g, p) { set(g, p, true); }, get: function () { return { group: cur.group, page: cur.page }; }, refresh: function () { drawn = null; sync(); },
      /* the page's on-dot, for an oscillator switched elsewhere */
      mark: function (gid, pid, on) { var gr = grp(gid); pagesOf(gr).forEach(function (q) { if (q.id === pid) q.on = on; }); drawn = null; sync(); } };
  }

  /* ---------- SECTION HEADER: the bench's one-line header. An identity rule above (ink, by pattern: solid, dash, dot),
     then on one line: the on toggle, the number badge, the NAME (cluster style), the mode pills, and at the right the
     section's readout slot (H.ReadoutSlot) or a plain summary. Below it the consumer places .hd-sgroups of .hd-sgrp, each a
     .hd-sublabel over a .hd-krow of controls. With `section` given, off dims the section's body (opacity-muted). ---------- */
  function SectionHeader(el, o) {
    o = o || {}; var path = o.path || o.name, on = o.on;
    el.classList.add('hd-sechead'); el.dataset.rule = o.rule || 'solid'; el.innerHTML = '';
    var tg = null;
    if (on !== undefined) {
      tg = tag('button', 'hd-toggle hd-sm hd-focus'); tg.type = 'button'; tg.setAttribute('role', 'switch'); tg.setAttribute('aria-label', path + ' · On');
      tg.addEventListener('click', function () { setOn(!on); fire(o.onToggle, on); }); el.appendChild(tg);
    }
    if (o.num !== undefined) { var nb = tag('span', 'sh-num', String(o.num)); nb.setAttribute('aria-hidden', 'true'); el.appendChild(nb); }
    var nm = tag('span', 'hd-cluster sh-name', o.name); el.appendChild(nm);
    var modes = null;
    if (o.modes) { var mh = tag('div'); el.appendChild(mh); modes = pillGroup(mh, o.modes.options, o.modes.value || 0, path + ' · ' + (o.modes.label || 'Mode'), o.modes.onPick, o); }
    var end = tag('div', 'sh-end'); el.appendChild(end);
    if (o.end) end.appendChild(o.end);
    var slot = tag('div'); end.appendChild(slot);
    if (o.S) H.ReadoutSlot(slot, o.S, { scope: o.section || o.scope, summary: o.summary });
    else { slot.className = 'hd-rslot'; slot.textContent = typeof o.summary === 'function' ? o.summary() : o.summary || ''; }
    function setOn(b) { on = !!b; if (tg) { tg.setAttribute('aria-checked', String(on)); tg.tabIndex = keysOn(o) ? 0 : -1; } if (o.section) o.section.classList.toggle('off', !on); }
    if (on !== undefined) setOn(on);
    return { on: function (b) { if (b === undefined) return on; setOn(b); }, mode: function (i) { if (!modes) return null; if (i === undefined) return modes.get(); modes.set(i); }, el: el };
  }

  /* ---------- CHANNEL STRIP: the mixer, loosely a physical console (the human, 2026-10-30: "levels sliders side-by-side
     ... attached to their toggles ... a live readout with a clipping warning next to it"). Strips stand side by side on ONE
     console card with hairlines between them, a master strip at the right behind a page rule. Each strip, top to bottom:
     its on toggle and name; the live peak readout with the latching CLIP lamp beside it; the fader (a vertical morph Slider,
     so corner pips, tether and mod lanes all show) with the strip's meter pair beside it, the fader's track and the meter
     sharing one height; the fader's value; mute and solo, directly under the fader.
     The meter keeps the Meter's anatomy (−66..+6 dBFS, 0 dBFS at 91.7%, the over zone in alarm, peak hold in value-ink,
     1.5 s then 20 dB/s) and Gain staging's rule that the latch lives on the audio side: read(id) gives the peaks since the
     last frame, latch(id) the lamp, clear(id) clears it. step(dt) advances the ballistics by the dt it is given (no clock). ---------- */
  var DB_MIN = -66, DB_MAX = 6;
  function frac(db) { return Math.max(0, Math.min(1, (db - DB_MIN) / (DB_MAX - DB_MIN))); }
  function fmtPk(db) { return db <= DB_MIN + .05 ? '−∞' : (db >= 0 ? '+' : '−') + Math.abs(db).toFixed(1); }
  function ChannelStrip(el, S, o) {
    o = o || {}; var strips = [];
    el.classList.add('hd-cs'); el.innerHTML = ''; el.setAttribute('role', 'group'); el.setAttribute('aria-label', o.label || 'Mixer');
    if (o.faderH) el.style.setProperty('--fader-h', o.faderH + 'px');
    function btn(cls, txt, label, pressed, onClick) {
      var b = tag('button', 'cs-btn ' + cls + ' hd-focus', txt); b.type = 'button'; b.setAttribute('aria-label', label); b.setAttribute('aria-pressed', String(!!pressed));
      b.addEventListener('click', onClick); return b;
    }
    (o.strips || []).forEach(function (s) {
      var t = { s: s, on: s.on !== false, mute: !!s.mute, solo: !!s.solo, bar: [DB_MIN, DB_MIN], pk: [DB_MIN, DB_MIN], held: [0, 0] };
      var path = s.path || s.name;
      var root = tag('div', 'cs-strip' + (s.master ? ' master' : '')); root.setAttribute('role', 'group'); root.setAttribute('aria-label', path + ' channel'); root.dataset.id = s.id;
      var head = tag('div', 'cs-head');
      if (!s.master && s.on !== undefined) {
        t.tg = tag('button', 'hd-toggle hd-sm hd-focus'); t.tg.type = 'button'; t.tg.setAttribute('role', 'switch'); t.tg.setAttribute('aria-label', path + ' · On');
        t.tg.addEventListener('click', function () { t.on = !t.on; fire(o.onToggle, s.id, 'on', t.on); sync(); }); head.appendChild(t.tg);
      }
      head.appendChild(tag('span', 'cs-name', s.name)); root.appendChild(head);
      var pkRow = tag('div', 'cs-pk'); t.pkEl = tag('output'); t.pkEl.setAttribute('aria-hidden', 'true');
      t.lamp = tag('button', 'hd-lamp hd-sm hd-focus', 'CLIP'); t.lamp.type = 'button';
      t.lamp.addEventListener('click', function () { fire(o.clear, s.id); paint(); });
      pkRow.appendChild(t.pkEl); pkRow.appendChild(t.lamp); root.appendChild(pkRow);
      var body = tag('div', 'cs-body'), fd = tag('div'); body.appendChild(fd);
      var fp = {}; for (var k in s.fader) fp[k] = s.fader[k]; fp.vert = true; fp.size = fp.size || 'compact'; fp.path = fp.path || path + ' · Level';
      H.Slider(fd, fp, S);
      var mt = tag('div', 'cs-meter'); mt.setAttribute('role', 'meter'); mt.setAttribute('aria-label', path + ' level');
      mt.setAttribute('aria-valuemin', DB_MIN); mt.setAttribute('aria-valuemax', DB_MAX);
      t.bars = [0, 1].map(function () { var b = tag('div', 'hd-mbar'); b.innerHTML = '<i class="fill"></i><i class="hold"></i>'; mt.appendChild(b); return { el: b, fill: b.firstChild, hold: b.lastChild }; });
      body.appendChild(mt); root.appendChild(body); t.meter = mt;
      if (!s.master) {
        var ms = tag('div', 'cs-ms');
        t.mb = btn('mute', 'M', path + ' · Mute', t.mute, function () { t.mute = !t.mute; fire(o.onToggle, s.id, 'mute', t.mute); sync(); });
        t.sb = btn('solo', 'S', path + ' · Solo', t.solo, function () { t.solo = !t.solo; fire(o.onToggle, s.id, 'solo', t.solo); sync(); });
        ms.appendChild(t.mb); ms.appendChild(t.sb); root.appendChild(ms);
      }
      el.appendChild(root); t.root = root; strips.push(t);
    });
    /* SOLO: any strip soloed silences every other source strip (never the master); such a strip dims and its M shows
       dashed (implicit), so the reason is visible without colour. A muted or off strip dims too. */
    function sync() {
      var anySolo = strips.some(function (t) { return t.solo && !t.s.master; }), ks = keysOn(o);
      strips.forEach(function (t) {
        var implicit = anySolo && !t.solo && !t.s.master;
        t.root.classList.toggle('quiet', !t.s.master && (t.mute || !t.on || implicit)); t.root.classList.toggle('implicit', implicit && !t.mute);
        if (t.tg) { t.tg.setAttribute('aria-checked', String(t.on)); t.tg.tabIndex = ks ? 0 : -1; }
        if (t.mb) { t.mb.setAttribute('aria-pressed', String(t.mute)); t.mb.tabIndex = ks ? 0 : -1; t.mb.title = implicit ? 'silenced by solo' : ''; }
        if (t.sb) { t.sb.setAttribute('aria-pressed', String(t.solo)); t.sb.tabIndex = ks ? 0 : -1; }
        t.lamp.tabIndex = ks ? 0 : -1;
      });
    }
    function step(dt) {
      var fall = Math.exp(-dt / .11);                                        // Gain staging's bar: a 0.11 s fall
      strips.forEach(function (t) {
        var r = o.read ? o.read(t.s.id) : null, now = r ? [r.l, r.r === undefined ? r.l : r.r] : [DB_MIN, DB_MIN];
        for (var c = 0; c < 2; c++) {
          var x = now[c] === undefined || now[c] === null ? DB_MIN : Math.max(DB_MIN, now[c]);
          var a = Math.max(Math.pow(10, x / 20), Math.pow(10, t.bar[c] / 20) * fall); t.bar[c] = Math.max(DB_MIN, 20 * Math.log10(Math.max(a, 1e-9)));
          if (x >= t.pk[c]) { t.pk[c] = x; t.held[c] = 0; } else { t.held[c] += dt; if (t.held[c] > 1.5) t.pk[c] = Math.max(x, t.pk[c] - 20 * dt); }   // duration-peak-hold, then 20 dB/s
        }
      });
    }
    function paint() {
      strips.forEach(function (t) {
        var L = o.latch ? o.latch(t.s.id) || {} : {}, top = Math.max(t.pk[0], t.pk[1]);
        t.bars.forEach(function (b, c) { b.fill.style.setProperty('--lv', frac(t.bar[c])); b.hold.style.setProperty('--hp', frac(t.pk[c])); b.el.classList.toggle('over', !!L.on); });
        t.pkEl.textContent = fmtPk(top);
        t.meter.setAttribute('aria-valuenow', top.toFixed(1)); t.meter.setAttribute('aria-valuetext', 'peak ' + fmtPk(top) + ' dBFS' + (L.on ? ', clipped' : ''));
        t.lamp.setAttribute('aria-pressed', String(!!L.on));
        t.lamp.setAttribute('aria-label', (t.s.path || t.s.name) + ' clip lamp' + (L.on ? ', latched at ' + fmtPk(L.max === undefined ? 0 : L.max) + ' dBFS: click to clear' : ''));
        t.lamp.title = L.on ? 'latched at ' + fmtPk(L.max === undefined ? 0 : L.max) + ' dBFS' : '';
      });
    }
    sync(); paint();
    return { step: step, render: paint, refresh: sync,
      set: function (id, what, val) { strips.forEach(function (t) { if (t.s.id === id) t[what] = !!val; }); sync(); },
      state: function () { return strips.map(function (t) { return { id: t.s.id, on: t.on, mute: t.mute, solo: t.solo, peak: Math.max(t.pk[0], t.pk[1]) }; }); } };
  }

  H.PageNav = PageNav; H.SectionHeader = SectionHeader; H.ChannelStrip = ChannelStrip; H.pillGroup = pillGroup;
})(window.Horde);

/* =====================================================================
   MASTER BATCH 2a (ADR-338 rulings 2 and 7, ADR-340 rulings 8 to 10, ADR-341 rulings 3 to 5): the controls and routes.
   Sources: docs/design/composition-lab.html (TC1 / TC2), composition-lab-r2.html (MS3, MS4, MP1, ML3),
   composition-lab-r3.html (MP-c, W-a). What is NOT new code lives where its component does: the corner mark and the level cell
   (T4c-f, Knob size 'level'), the dual readout (DR-c, H.ReadoutSlot), the stepper's SG-b drawing and dropdown (H.Stepper), the
   polarity chord and MP3 (the Session gesture), the wires' valves and dragging (H.CompactWires), the re-anchored matrix drag.
   New here: H.PolChip (MP1), H.SourceChip (MS3), H.SourceRows (MS4), H.RoutesBySource (ML3), H.Companion (TC1 / TC2).
   ===================================================================== */
(function (H) {
  'use strict';
  var DRAG = 160;                                       // size-drag: px for the full range
  function clamp(x, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, x)); }
  function tag(t, cls, txt) { var e = document.createElement(t); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; }
  function srcColor(s) { return s ? 'var(--src-' + s.kind + ')' : 'var(--hint)'; }
  function keysOn(o) { return typeof o.keys === 'function' ? !!o.keys() : !!o.keys; }
  function pct(x) { return Math.round(x * 100) + '%'; }
  function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }

  /* ---------- POLARITY CHIP (MP1, ADR-340 r9): one chip on every route ----------
     Click: the next polarity (UNI → BI → INV → CTL, CTL skipped where another mapping already controls the parameter, ADR-266 r2);
     shift-click: the previous; a VERTICAL DRAG widens and thins the band, which is the depth (a Control's width, which keeps its
     centre). Up is wider. The drag is 160 px for the full range (size-drag), Shift ×0.25, re-anchored at the stops and on a Shift
     change (ADR-340 r2); while it runs the chip shows the amount, and a click is a pointer that moved under 3 px.
     o: { cycle(back) (the owner's own cycle: it knows depth routes have no CTL), depthRoute }. The chip's text and title are the
     owner's to set; this only adds the gestures. */
  function PolChip(btn, S, m, o) {
    o = o || {};
    var st = null, dragged = false;
    btn.classList.add('hd-polchip');
    function width() { return m.pol === 'ctl' ? m.hi - m.lo : (!o.depthRoute && m.cd && st && st.ai !== null ? m.cd[st.ai] : m.depth); }
    function write(nv) {
      if (m.pol === 'ctl') { var c = (m.lo + m.hi) / 2; m.lo = clamp(c - nv / 2); m.hi = clamp(c + nv / 2); }
      else if (!o.depthRoute && m.cd && st.ai !== null) m.cd[st.ai] = nv; else m.depth = nv;
    }
    btn.addEventListener('pointerdown', function (e) {
      if (e.button !== 0 || (e.ctrlKey && !e.metaKey)) return;   // macOS Ctrl-click is the secondary click
      try { btn.setPointerCapture(e.pointerId); } catch (_) { /* keep the gesture */ }
      var p = S.params[m.param];
      st = { y: e.clientY, ai: !o.depthRoute && m.cd && p ? S.editing(p) : null, moved: false, k: e.shiftKey ? .25 : 1 };
      st.start = st.cur = width(); dragged = false;
      if (!o.depthRoute) S.lastMap = m;
    });
    btn.addEventListener('pointermove', function (e) {
      if (!st) return;
      var d = st.y - e.clientY, k = e.shiftKey ? .25 : 1;
      if (Math.abs(d) > 3) st.moved = true; if (!st.moved) return;
      if (k !== st.k) { st.start = st.cur; st.y = e.clientY; d = 0; st.k = k; }
      var raw = st.start + d / DRAG * k, nv = clamp(raw);
      if (raw !== nv) { st.start = nv; st.y = e.clientY; }
      st.cur = nv; write(nv); btn.classList.add('drag');
      btn.textContent = o.depthRoute ? pct(m.depth) : S.amount(m);   // the amount while it moves; the owner's sync puts the polarity back
    });
    function end(e) {
      if (e && e.pointerId !== undefined && btn.hasPointerCapture && btn.hasPointerCapture(e.pointerId)) btn.releasePointerCapture(e.pointerId);
      if (!st) return; var s = st; st = null; btn.classList.remove('drag');
      if (s.moved) { dragged = true; S.say((o.depthRoute ? 'Depth route · ' : S.src(m.src).name + ' → ' + m.param + ' · ') + 'depth ' + (o.depthRoute ? pct(m.depth) : S.amount(m)) + ' (chip drag)'); }
    }
    btn.addEventListener('pointerup', end); btn.addEventListener('pointercancel', end); btn.addEventListener('lostpointercapture', end);
    btn.addEventListener('click', function (e) {
      e.stopPropagation();   // the chip owns its click: a row or inspector listening above never sees it
      if (dragged) { dragged = false; return; }   // the click that ends a drag is not a cycle
      o.cycle(e.shiftKey);
    });
    return btn;
  }

  /* ---------- SOURCE CHIP (MS3, ADR-340 r8): the MAP control's source as a chip you drag through or step ----------
     ‹ chip › with the name in the source's colour: drag up or down on the name to scroll through the sources (one per 12 px, down
     is next, and it wraps), ‹ › step, the ▾ opens the full list (a native select). A plain wheel is NOT used (B301's gate: a
     plain-wheel chip would take the page's scroll whenever the pointer crossed it); Option + wheel would be possible. Switching
     the source chooses the source of the NEXT mapping and never implies one modulator per parameter (ADR-340 r8).
     o: { order() (default S.sources), onPick(id), keys (keyboard mode: ↑ ↓ step) } */
  function SourceChip(el, S, o) {
    o = o || {};
    var list = function () { return o.order ? o.order() : S.sources; };
    el.classList.add('hd-schip'); el.innerHTML = '';
    var dn = tag('button', 'sc-b hd-focus', '‹'), up = tag('button', 'sc-b hd-focus', '›'); dn.type = up.type = 'button'; dn.setAttribute('aria-label', 'previous source'); up.setAttribute('aria-label', 'next source');
    var nm = tag('span', 'sc-nm hd-focus'), sw = tag('i', 'sc-sw'), lab = tag('span', 'sc-t'), kb = tag('kbd');
    nm.setAttribute('role', 'spinbutton'); nm.setAttribute('aria-label', 'Map source: drag up or down to change'); nm.title = 'drag up / down: previous / next source';
    nm.appendChild(sw); nm.appendChild(lab); nm.appendChild(kb);
    var car = tag('span', 'sc-car', '▾'), sel = tag('select', 'hd-focus'); sel.setAttribute('aria-label', 'Map source (full list)'); car.title = 'the full list';
    S.sources.forEach(function (s) { var op = tag('option', null, s.name); op.value = s.id; sel.appendChild(op); });
    car.appendChild(sel);
    el.appendChild(dn); el.appendChild(nm); el.appendChild(car); el.appendChild(up);
    function step(d) {
      var l = list(), i = -1, j; for (var k = 0; k < l.length; k++) if (l[k].id === S.map.src) i = k;
      j = ((i < 0 ? 0 : i + d) % l.length + l.length) % l.length; S.setSrc(l[j].id); if (o.onPick) o.onPick(l[j].id); S.say('Source: ' + l[j].name);
    }
    dn.addEventListener('click', function () { step(-1); }); up.addEventListener('click', function () { step(1); });
    sel.addEventListener('change', function () { S.setSrc(sel.value); if (o.onPick) o.onPick(sel.value); });
    var st = null;
    nm.addEventListener('pointerdown', function (e) { if (e.button !== 0) return; try { nm.setPointerCapture(e.pointerId); } catch (_) { /* keep the drag */ } st = { y: e.clientY, done: 0 }; });
    nm.addEventListener('pointermove', function (e) { if (!st) return; var n = Math.trunc((e.clientY - st.y) / 12); if (n !== st.done) { step(n - st.done); st.done = n; } });
    function end(e) { if (e && e.pointerId !== undefined && nm.hasPointerCapture && nm.hasPointerCapture(e.pointerId)) nm.releasePointerCapture(e.pointerId); st = null; }
    nm.addEventListener('pointerup', end); nm.addEventListener('pointercancel', end); nm.addEventListener('lostpointercapture', end);
    nm.addEventListener('keydown', function (e) { if (!keysOn(o.keys === undefined ? S : o)) return; if (e.key === 'ArrowDown' || e.key === 'ArrowRight') step(1); else if (e.key === 'ArrowUp' || e.key === 'ArrowLeft') step(-1); else return; e.preventDefault(); });
    function sync() {
      var s = S.src(S.map.src); if (!s) return;
      var l = list(), n = 0; for (var k = 0; k < l.length; k++) if (l[k].id === s.id) n = k + 1;
      el.style.setProperty('--sc', srcColor(s)); lab.textContent = s.name; kb.textContent = n || ''; sel.value = s.id;
      nm.setAttribute('aria-valuetext', s.name + (n ? ', source ' + n + ' of ' + l.length : '')); nm.tabIndex = keysOn(o.keys === undefined ? S : o) ? 0 : -1;
    }
    S.on(sync); sync();
    return { sync: sync };
  }

  /* ---------- SOURCE ROWS (MS4, ADR-340 r8): the bank's row form, where a click on a row makes it the source ----------
     A thin row per source (26 px): its kind swatch, name and route count, a live meter, its value, a Map button. While MAP is on, a
     click on a row's BODY makes that source the one the next mapping uses (the row lights); the Map button turns MAP on with that
     source (map-then-pick, and again to turn it off). The bank is already the list of the page's sources, so this costs no new
     control. The row's picture is the live meter here: the bank's own pictures and its Macros | Mods toggle are the page forms'
     (batch 2b); this is only the row and its source-switching.
     o: { sources? (ids; default every source), fmt(source, value) } */
  function SourceRows(el, S, o) {
    o = o || {};
    el.classList.add('hd-srows'); el.innerHTML = ''; el.setAttribute('role', 'list'); el.setAttribute('aria-label', o.label || 'Modulation sources');
    var list = S.sources.filter(function (s) { return !o.sources || o.sources.indexOf(s.id) >= 0; }), rows = {};
    function routes(id) { var n = 0; S.mappings.forEach(function (m) { if (m.src === id) n++; }); H.Matrix.dRoutes(S).forEach(function (d) { if (d.src === id) n++; }); return n; }
    list.forEach(function (s, i) {
      var r = tag('div', 'sr-row'); r.setAttribute('role', 'listitem'); r.style.setProperty('--sc', srcColor(s)); r.dataset.src = s.id;
      var sw = tag('i', 'sr-sw'), nm = tag('span', 'sr-nm', s.name), sm = tag('small'), mt = tag('span', 'sr-meter'), mi = tag('i'), vl = tag('span', 'sr-v'), mb = tag('button', 'sr-map hd-focus', 'MAP');
      nm.appendChild(sm); mt.appendChild(mi); mb.type = 'button';
      r.appendChild(sw); r.appendChild(nm); r.appendChild(mt); r.appendChild(vl); r.appendChild(mb);
      r.title = s.name + ' (' + s.kind + ', source ' + (i + 1) + ')';
      r.addEventListener('click', function (e) { if (e.target.closest('button') || !S.map.on) return; S.setSrc(s.id); S.say('Source: ' + s.name + ' (the bank row)'); });
      mb.addEventListener('click', function () { if (S.map.on && S.map.src === s.id) S.setMap(false); else { S.setSrc(s.id); S.setMap(true); S.say('MAP on, source ' + s.name + ': click a control to map it'); } });
      el.appendChild(r); rows[s.id] = { r: r, mi: mi, vl: vl, sm: sm, mb: mb, s: s };
    });
    var last = '';
    var view = { el: el, render: function () {
      list.forEach(function (s) {
        var x = rows[s.id], v = S.sourceValue(s.id), on = S.map.on && S.map.src === s.id, n = routes(s.id);
        x.mi.style.width = (v * 100).toFixed(1) + '%';
        var t = o.fmt ? o.fmt(s, v) : String(Math.round(v * 100)); if (x.vl.textContent !== t) x.vl.textContent = t;
        var k = n + '|' + on + '|' + (S.map.src === s.id); if (x.k !== k) { x.k = k;
          x.sm.textContent = n ? n + ' route' + (n > 1 ? 's' : '') : 'no routes'; x.r.classList.toggle('sel', on);
          x.mb.setAttribute('aria-pressed', String(on)); x.mb.setAttribute('aria-label', 'Map ' + s.name + (on ? ', on: click to turn MAP off' : ': turn MAP on with this source')); }
      });
    } };
    S.add(view); view.render(); return view;
  }

  /* ---------- ROUTES BY SOURCE (ML3, ADR-340 r10): one group per source, a row per route ----------
     "What does this modulator do?": each group is headed by the source (swatch, name, live meter, route count, Map button) and holds
     one route row per mapping it drives, then a row per depth route it drives (H.RouteRow, the same row the grid's list uses, minus
     the source column: it is the group's header). The Mod page's detail pane (M5) shows the selected modulator's
     group; every group is the one route list's view, so an edit in any view shows in all.
     o: { sources? (ids), param?, empty (also show sources with no route, with a hint) } */
  function RoutesBySource(el, S, o) {
    o = o || {};
    var M = H.Matrix;
    el.classList.add('mx-bysrc', 'mx-routes'); el.__S = S; el.setAttribute('aria-label', o.label || 'Routes by source');
    var rows = [], heads = [];
    function build() {
      el.innerHTML = ''; rows = []; heads = [];
      var shown = 0;
      S.sources.forEach(function (s) {
        if (o.sources && o.sources.indexOf(s.id) < 0) return;
        var ms = S.mappings.filter(function (m) { return m.src === s.id && (!o.param || m.param === o.param); });
        var ds = M.dRoutes(S).filter(function (d) { return d.src === s.id && (!o.param || d.of.param === o.param); });
        if (!ms.length && !ds.length && !o.empty) return;
        shown++;
        var g = tag('div', 'mx-grp'); g.setAttribute('role', 'group'); g.setAttribute('aria-label', s.name + ', ' + (ms.length + ds.length) + ' route' + (ms.length + ds.length === 1 ? '' : 's')); g.style.setProperty('--sc', srcColor(s));
        var hd = tag('div', 'mx-sgh'), dot = tag('i', 'dot'), nm = tag('span', 'nm', s.name), mt = tag('span', 'meter'), mi = tag('i'), ct = tag('small', null, ms.length + ds.length ? (ms.length + ds.length) + ' route' + (ms.length + ds.length === 1 ? '' : 's') : 'no routes');
        var mb = tag('button', 'hd-pill hd-sm hd-focus', 'MAP'); mb.type = 'button'; mb.setAttribute('aria-label', 'Map ' + s.name + ': turn MAP on with this source');
        mb.addEventListener('click', function () { S.setSrc(s.id); S.setMap(true); S.say('MAP on, source ' + s.name + ': click a control to map it'); });
        mt.appendChild(mi); hd.appendChild(dot); hd.appendChild(nm); hd.appendChild(mt); hd.appendChild(ct); hd.appendChild(mb); g.appendChild(hd);
        heads.push({ id: s.id, mi: mi });
        if (!ms.length && !ds.length) g.appendChild(tag('div', 'hd-hint mx-empty', 'No routes. Turn MAP on and click a control.'));
        ms.forEach(function (m) { var r = tag('div'); r.setAttribute('role', 'listitem'); g.appendChild(r); rows.push(H.RouteRow(r, S, m)); });
        ds.forEach(function (d) { var r = tag('div'); r.setAttribute('role', 'listitem'); g.appendChild(r); rows.push(H.RouteRow(r, S, d, { depthRoute: true })); });
        el.appendChild(g);
      });
      if (!shown) el.appendChild(tag('div', 'hd-hint mx-empty', 'No routes. Click an empty grid cell, or turn MAP on and click a control.'));
    }
    var last = null;
    function check() { var g = M.sig(S); if (g !== last) { last = g; build(); } rows.forEach(function (r) { r.sync(); }); }
    S.on(check); check();
    var view = { el: el, render: function () { var g = M.sig(S); if (g !== last) check(); rows.forEach(function (r) { r.render(); }); heads.forEach(function (h) { h.mi.style.width = (S.sourceValue(h.id) * 100).toFixed(1) + '%'; }); } };
    S.add(view); return view;
  }

  /* ---------- COMPANIONS (ADR-338 r2): the toggle, segments, select and button at TC1's 16 px faces ----------
     TC1, in sections and cards: the house look in 16 px boxes (an 18 × 10 switch, segments, a select, a button; the stepper is
     H.Stepper with `tc: true`), each with a 28 px hit box through an ink-less ::before. TC2, `flat: true`, ONLY inside tables:
     a lamp, underline segments, a caret over a native select, a text button (the table's drag number is H.Stepper `flat`).
     o: { kind: 'toggle' | 'segments' | 'select' | 'button', name, path, options (segments, select: the manifest's labels), short
          (segments: the shown abbreviations), value (toggle: bool; segments, select: the index), label (a TC2 lamp's or a button's
          text), flat, onInput(v), keys() (keyboard mode: ← → move a segment group) } */
  function Companion(el, o) {
    var flat = !!o.flat, kind = o.kind, v = o.value, paint = function () {}, name = o.path || o.name || kind;
    el.classList.add('hd-tc'); el.innerHTML = '';
    function set(x, quiet) { v = x; paint(); if (!quiet && o.onInput) o.onInput(v); }
    if (kind === 'toggle') {
      var b = tag('button', flat ? 'hd-tc2-lamp hd-focus' : 'hd-tc1-tog hd-focus'); b.type = 'button'; b.setAttribute('role', 'switch'); b.setAttribute('aria-label', name);
      if (flat) { b.appendChild(tag('i')); b.appendChild(document.createTextNode(o.label === undefined ? (o.name || '') : o.label)); }
      b.addEventListener('click', function () { set(!v); });
      el.appendChild(b); paint = function () { b.setAttribute('aria-checked', String(!!v)); b.tabIndex = keysOn(o) ? 0 : -1; };
    } else if (kind === 'segments') {
      var g = tag('span', flat ? 'hd-tc2-seg' : 'hd-tc1-seg'); g.setAttribute('role', 'radiogroup'); g.setAttribute('aria-label', name);
      var bs = o.options.map(function (t, i) { var bb = tag('button', 'hd-focus', (o.short && o.short[i]) || t); bb.type = 'button'; bb.setAttribute('role', 'radio'); bb.title = t; bb.addEventListener('click', function () { set(i); }); g.appendChild(bb); return bb; });
      g.addEventListener('keydown', function (e) { if (!keysOn(o)) return; var n = bs.length; if (e.key === 'ArrowRight' || e.key === 'ArrowDown') set((v + 1) % n); else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') set((v + n - 1) % n); else return; e.preventDefault(); bs[v].focus(); });
      el.appendChild(g); paint = function () { bs.forEach(function (bb, i) { bb.setAttribute('aria-checked', String(i === v)); bb.tabIndex = keysOn(o) && i === v ? 0 : -1; }); };
    } else if (kind === 'select') {
      var s = tag('select', (flat ? '' : 'hd-tc1-sel ') + 'hd-focus'); s.setAttribute('aria-label', name);
      o.options.forEach(function (t, i) { var op = tag('option', null, t); op.value = String(i); s.appendChild(op); });
      s.addEventListener('change', function () { set(+s.value); });
      if (flat) { var face = tag('span', 'txt'), w = tag('span', 'hd-tc2-sel'); w.appendChild(face); w.appendChild(tag('span', 'car', '▾')); w.appendChild(s); el.appendChild(w);
        paint = function () { s.value = String(v); face.textContent = (o.short && o.short[v]) || o.options[v]; }; }
      else { el.appendChild(s); paint = function () { s.value = String(v); }; }
    } else if (kind === 'button') {
      var bt = tag('button', (flat ? 'hd-tc2-btn' : 'hd-tc1-btn') + ' hd-focus', o.label || o.name || 'Reset'); bt.type = 'button'; bt.setAttribute('aria-label', name);
      bt.addEventListener('click', function () { if (o.onInput) o.onInput(); });
      el.appendChild(bt);
    } else throw new Error('Companion: unknown kind ' + kind);
    paint();
    return { get: function () { return v; }, set: function (x) { set(x, true); } };
  }

  H.PolChip = PolChip; H.SourceChip = SourceChip; H.SourceRows = SourceRows; H.RoutesBySource = RoutesBySource; H.Companion = Companion;
})(window.Horde);

/* =====================================================================
   MASTER BATCH 2b (ADR-338 / ADR-340 / ADR-341): the page forms and the bank. Clusters C1 and C2, the page paradigm P1 (a
   lens) with P2's fold, PV1's table view, the four modulator tiers, the M2 bank with BT1 + BR-b, the M5 Mod page and M-R2's
   undo line. Layout and behaviour only: every value is the consumer's (the engine's in GUI 3) and the previews label theirs
   SAMPLE. Sources: docs/design/composition-lab.html (C1/C2, P1, P2, the tiers, M2, M5, M-R2), composition-lab-r2.html (PV1,
   BT1/BR1), composition-lab-r3.html (BR-b). Where a ruling leaves a reading open it is named in the component's README.
   ===================================================================== */
(function (H) {
  'use strict';
  var NS = 'http://www.w3.org/2000/svg';
  function tag(t, cls, txt) { var e = document.createElement(t); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; }
  function sv(t, a, parent) { var e = document.createElementNS(NS, t); for (var k in a) e.setAttribute(k, a[k]); if (parent) parent.appendChild(e); return e; }
  function clamp(v, a, b) { a = a === undefined ? 0 : a; b = b === undefined ? 1 : b; return Math.min(b, Math.max(a, v)); }
  function fire(fn, a, b, c) { if (typeof fn === 'function') return fn(a, b, c); }
  function keysOn(o) { return o && (typeof o.keys === 'function' ? !!o.keys() : !!o.keys); }
  /* pointer capture, guarded: a pointer the browser no longer tracks (a synthetic event, a pen lifted mid-press) throws
     NotFoundError, and an unguarded throw there cost the whole gesture */
  function capture(el, e) { try { el.setPointerCapture(e.pointerId); } catch (_) { /* keep the gesture */ } }
  function pctf(x) { return Math.round(x * 100) + '%'; }
  function arr(x) { return x === undefined || x === null ? [] : Array.isArray(x) ? x : [x]; }
  var CORN = H.CORNERS;

  /* A page's remembered choice (PV1's view, P2's fold): localStorage when the host has it, else the session's memory. Storage is
     wrapped because a sandboxed frame or a private window throws on access, and the control must work without it. */
  var MEM = {};
  function recall(key) { try { var s = window.localStorage.getItem(key); if (s !== null) return JSON.parse(s); } catch (_) { /* no storage: memory below */ } return MEM[key]; }
  function keep(key, v) { MEM[key] = v; try { window.localStorage.setItem(key, JSON.stringify(v)); } catch (_) { /* no storage: the memory holds it */ } }

  /* ONE parameter, MANY faces (a page's grid and its table, the lens's big knob). Knob and Slider register the props object they
     are given and reset the saved baseline, and the tiny slider sets p.horizontal on it, so a second face built from the same
     object would flip the first one's drag axis and the revert baseline. Each face therefore gets a SHADOW of the canonical
     props: the corner values (vals, shared by reference) and the global value (g, an accessor to the canonical one) are common,
     everything else is its own; the registry and the baseline are put back afterwards. */
  function canon(S, p) {
    var c = S.params[p.key];
    if (!c) { p.fmt = p.fmt || pctf; p.def = p.def === undefined ? (p.locked ? p.g : .5) : p.def; S.params[p.key] = p; if (!p.locked) S.saved[p.key] = p.vals.slice(); c = p; }
    return c;
  }
  function face(S, p, extra, build) {
    var c = canon(S, p), q = Object.create(c), had = S.saved[p.key], k;
    for (k in extra) q[k] = extra[k];
    Object.defineProperty(q, 'g', { get: function () { return c.g; }, set: function (x) { c.g = x; }, enumerable: true });
    var r = build(q);
    S.params[p.key] = c; if (had) S.saved[p.key] = had;
    return r;
  }
  /* an item of a cluster, a fold tier or a page grid: an element, a builder function(host, S), or one of
     { knob }, { slider }, { stepper }. Knobs and sliders default to the tiny tier; the host carries data-key, which is what a
     Map button's drag lands on and what PV1's find dims. */
  function cell(S, it, o) {
    o = o || {};
    if (it && it.nodeType) return it;
    var host = tag('div'), p;
    if (typeof it === 'function') { it(host, S); return host; }
    if (it.el) return it.el;
    if (it.knob) { p = it.knob; host.dataset.key = p.key; face(S, p, { size: p.size || 'tiny', label: o.noLabel ? false : p.label }, function (q) { H.Knob(host, q, S); }); return host; }
    if (it.slider) { p = it.slider; host.dataset.key = p.key; face(S, p, { size: p.size || 'tiny' }, function (q) { H.Slider(host, q, S); }); return host; }
    if (it.stepper) {
      var s = it.stepper, c = tag('div', 'hd-cl-cell'), lab = tag('span', 'cl-lab', s.abbr || s.name || ''), sh = tag('div');
      if (s.key) c.dataset.key = s.key; lab.title = s.path || s.name || '';
      var q2 = {}; for (var k in s) q2[k] = s[k]; if (q2.sm === undefined) q2.sm = true; if (q2.keys === undefined && o.keys !== undefined) q2.keys = function () { return keysOn(o); };
      c.appendChild(lab); c.appendChild(sh); H.Stepper(sh, q2); return c;
    }
    return host;
  }
  function fill(host, S, items, o) { arr(items).forEach(function (it) { host.appendChild(cell(S, it, o)); }); return host; }
  /* a cluster's header is the section header (rule none: a cluster sits inside a card's own section), its switches in the slot's place */
  function clusterHead(el, S, o) {
    var hd = tag('div'), end = tag('div', 'cl-switches');
    arr(o.head).forEach(function (n) { end.appendChild(cell(S, n, o)); });
    el.appendChild(hd);
    var hdr = H.SectionHeader(hd, { name: o.name, path: o.path || o.name, num: o.num, rule: o.rule || 'none', on: o.on, onToggle: o.onToggle, S: S, section: el, scope: el, summary: o.summary, keys: o.keys });
    /* the switches go beside the name, not into the slot's end: the slot is a fixed 250 px (H1) and clips whatever shares its end in a narrow cell */
    if (end.childNodes.length) hd.insertBefore(end, hd.querySelector('.sh-end'));
    return hdr;
  }

  /* ---------- CLUSTER, C1 (ADR-338 r4): the general rule. A row of 28 px cells in groups split by a dashed hairline, ONE shared
     label line above all of them (each tiny knob's own 4-character label sits on it), the section's switches in the header, and
     the section's one readout slot. A long section wraps BY GROUP, never mid-group. Advanced controls follow inline after an ADV
     mark (P1, ADR-338 r5). groups: [{ label, items }]: the label names the group for a screen reader and the hover; it is not drawn. ---------- */
  function Cluster(el, S, o) {
    o = o || {};
    el.classList.add('hd-sec', 'hd-clu'); el.innerHTML = '';
    var hdr = clusterHead(el, S, o), body = tag('div', 'cl-cols'); if (o.picture) el.appendChild(o.picture); el.appendChild(body);
    arr(o.groups).forEach(function (g) {
      var gr = tag('div', 'cl-grp'); gr.setAttribute('role', 'group'); if (g.label) { gr.setAttribute('aria-label', g.label); gr.title = g.label; }
      fill(gr, S, g.items, o); body.appendChild(gr);
    });
    if (arr(o.adv).length) {
      var ad = tag('div', 'cl-grp cl-adv'); ad.setAttribute('role', 'group'); ad.setAttribute('aria-label', 'Advanced');
      var mk = tag('span', 'cl-advmark', 'ADV'); mk.setAttribute('aria-hidden', 'true'); ad.appendChild(mk); fill(ad, S, o.adv, o); body.appendChild(ad);
    }
    return { el: el, header: hdr, body: body };
  }

  /* ---------- CLUSTER MATRIX, C2 (ADR-338 r4): for any block with a repeated sub-structure (envelope stages x (time, curve), the two
     blades). Column labels ONCE across the top, row labels ONCE down the left, 28 px cells, and an empty cell drawn as an honest
     "·" (no hold curve exists), never a disabled control. rows: [{ label, cells: [item | null] }]. ---------- */
  function ClusterMatrix(el, S, o) {
    o = o || {};
    el.classList.add('hd-sec', 'hd-clu'); el.innerHTML = '';
    var hdr = clusterHead(el, S, o), cols = arr(o.cols), g = tag('div', 'cl-mx'); if (o.picture) el.appendChild(o.picture); el.appendChild(g);
    g.setAttribute('role', 'group'); g.setAttribute('aria-label', (o.path || o.name || 'Matrix') + ' · matrix');
    g.style.gridTemplateColumns = 'auto repeat(' + cols.length + ', var(--size-hit))';
    g.appendChild(tag('span'));
    cols.forEach(function (c) { var l = tag('span', 'cl-col', c.label || c); if (c.title) l.title = c.title; g.appendChild(l); });
    var mo = {}; for (var k in o) mo[k] = o[k]; mo.noLabel = true;
    arr(o.rows).forEach(function (r) {
      g.appendChild(tag('span', 'cl-row', r.label));
      cols.forEach(function (c, i) {
        var it = r.cells && r.cells[i];
        if (it) g.appendChild(cell(S, it, mo));
        else { var e = tag('span', 'cl-empty', '·'); e.title = o.emptyTitle || 'no control here'; e.setAttribute('aria-hidden', 'true'); g.appendChild(e); }
      });
    });
    return { el: el, header: hdr, grid: g };
  }

  /* ---------- LENS, P1 (ADR-338 r5): the control under the pointer, or the last one clicked, enlarged: the full bundle knob (a
     SHADOW of the page's own props, so it edits the same corner values), every corner's value and weight, and every mapping.
     scope: the page's keys (an array, or a function key -> bool); fallback: what shows before anything is touched.
     Which value "where the hand is taking the control" shows in the dual readout is 2a's open reading (ADR-341); the lens
     lists the stored corner values and does not choose. ---------- */
  function Lens(el, S, o) {
    o = o || {};
    var inScope = typeof o.scope === 'function' ? o.scope : (function (a) { return function (k) { return a.indexOf(k) >= 0; }; })(arr(o.scope));
    el.classList.add('hd-lens'); el.innerHTML = ''; el.setAttribute('role', 'group'); el.setAttribute('aria-label', 'Lens: the control in focus');
    var top = tag('div', 'lz-top'), slot = tag('div', 'lz-knob'), who = tag('div', 'lz-who'), nm = tag('div', 'lz-name'), pth = tag('div', 'lz-path'),
      cor = tag('div', 'lz-corners'), maps = tag('div', 'lz-maps');
    who.appendChild(nm); who.appendChild(pth); top.appendChild(slot); top.appendChild(who);
    el.appendChild(tag('span', 'cl-lab', 'Lens')); el.appendChild(top); el.appendChild(cor); el.appendChild(maps);
    var pool = {}, cur = null, last = '';
    function ln(parent, text, color, bold) { var s = tag('span', 'lz-ln', text); if (color) s.style.color = color; if (bold) s.style.fontWeight = '700'; parent.appendChild(s); }
    S.add({ render: function () {
      if (!el.isConnected) return;
      var k = S.drag ? S.drag.key : S.hot;
      if (!k || !inScope(k) || !S.params[k]) k = S.selected && inScope(S.selected) && S.params[S.selected] ? S.selected : (o.fallback && S.params[o.fallback] ? o.fallback : null);
      if (!k) { if (cur !== null) { cur = null; nm.textContent = 'Nothing yet'; pth.textContent = 'hover a control'; slot.replaceChildren(); cor.replaceChildren(); maps.replaceChildren(); } return; }
      var p = S.params[k];
      if (k !== cur) {
        cur = k; last = ''; nm.textContent = p.name; pth.textContent = p.path || k;
        if (!pool[k]) {
          var host = tag('div');
          face(S, p, { size: o.size || 'standard' }, function (q) { var v = H.Knob(host, q, S), r0 = v.render; v.render = function () { if (host.isConnected) r0.call(v); }; });
          pool[k] = host;
        }
        slot.replaceChildren(pool[k]);
      }
      var w = S.weights(), ai = S.editing(p), ls = S.mapsOf(k);
      var sig = k + (p.locked ? 'g' + p.g : p.vals.join()) + w.map(function (x) { return x.toFixed(2); }).join() + ai + ls.map(function (m) { return m.src + m.pol + S.amount(m); }).join();
      if (sig === last) return; last = sig;
      cor.replaceChildren();
      if (p.locked) ln(cor, 'one global value · ' + p.fmt(p.g));
      else for (var i = 0; i < 4; i++) {
        var row = tag('div'); cor.appendChild(row);
        ln(row, CORN[i].id + ' ' + p.fmt(p.vals[i]) + (w[i] > .999 || w[i] < .001 ? '' : ' · ' + Math.round(w[i] * 100) + '%') + (i === ai ? ' · editing' : ''), 'var(--c' + CORN[i].id + '-tx)', i === ai);
      }
      maps.replaceChildren();
      if (!ls.length) ln(maps, 'no mappings');
      ls.forEach(function (m) { var row = tag('div'); maps.appendChild(row); ln(row, S.src(m.src).name + ' ' + m.pol.toUpperCase() + ' ' + S.amount(m), 'var(--src-' + S.src(m.src).kind + ')'); });
    } });
    return { el: el, key: function () { return cur; } };
  }

  /* ---------- FOLD, P2 (ADR-338 r5): per section, Face | All | +Adv, remembered per fold id. ONLY where a page still overflows with P1.
     tiers: { face, all, adv }, each an item list or a builder function(host, S); built the first time they are shown. A tier
     with no content has no segment. ---------- */
  function Fold(el, S, o) {
    o = o || {};
    var id = 'hd.fold.' + (o.id || o.name), mem = recall(id) || {}, names = ['Face'], idx = [0], T = o.tiers || {};
    if (T.all) { names.push('All'); idx.push(1); } if (T.adv) { names.push('+Adv'); idx.push(2); }
    var tier = mem.tier !== undefined && idx.indexOf(mem.tier) >= 0 ? mem.tier : (idx.indexOf(o.tier) >= 0 ? o.tier : 0), open = mem.open !== undefined ? !!mem.open : o.open !== false;
    el.classList.add('hd-fold'); el.innerHTML = '';
    var head = tag('div', 'fd-head'), chev = tag('button', 'fd-chev hd-focus'), nm = tag('span', 'hd-cluster', o.name), seg = tag('div'), sp = tag('span', 'fd-sp'), slot = tag('div');
    chev.type = 'button'; chev.setAttribute('aria-label', 'Fold ' + (o.path || o.name));
    head.appendChild(chev); head.appendChild(nm); head.appendChild(seg); head.appendChild(sp); head.appendChild(slot);
    var body = tag('div', 'fd-body'), blocks = { 0: tag('div', 'fd-tier'), 1: tag('div', 'fd-tier'), 2: tag('div', 'fd-tier') }, built = {};
    body.appendChild(blocks[0]); body.appendChild(blocks[1]); body.appendChild(blocks[2]);
    blocks[2].classList.add('fd-adv');
    el.appendChild(head); el.appendChild(body);
    if (S) H.ReadoutSlot(slot, S, { scope: el, summary: o.summary || '' });
    var pills = names.length > 1 ? H.pillGroup(seg, names, idx.indexOf(tier), (o.path || o.name) + ' · tier', function (i) { set(idx[i]); }, o) : null;
    function paint() {
      chev.textContent = open ? '▾' : '▸'; chev.setAttribute('aria-expanded', String(open)); chev.tabIndex = keysOn(o) ? 0 : -1;
      body.hidden = !open;
      [0, 1, 2].forEach(function (t) {
        var on = open && t <= tier && T[['face', 'all', 'adv'][t]];
        blocks[t].hidden = !on;
        if (on && !built[t]) { built[t] = true; var src = T[['face', 'all', 'adv'][t]]; if (typeof src === 'function') src(blocks[t], S); else fill(blocks[t], S, src, o); }
      });
      if (pills) pills.set(idx.indexOf(tier));
    }
    function set(t) { tier = t; keep(id, { open: open, tier: tier }); paint(); }
    chev.addEventListener('click', function () { open = !open; keep(id, { open: open, tier: tier }); paint(); });
    paint();
    return { tier: function (t) { if (t === undefined) return tier; if (idx.indexOf(t) >= 0) set(t); }, open: function (b) { if (b === undefined) return open; open = !!b; keep(id, { open: open, tier: tier }); paint(); }, el: el };
  }

  /* ---------- PARAM TABLE (the table form of PV1, P3 of the composition lab): every control of a page a 17 px row, in columns,
     grouped by section: the owner glyph, the FULL name (never an abbreviation), the control, the mappings as dots. Cells are TS2a
     value cells (a knob), flat steppers (K4), lamps and caret selects. groups: [{ title, controls: [{ key, name, kind, props,
     advanced }] }], kind knob | stepper | toggle | select. ---------- */
  function ParamTable(el, S, o) {
    o = o || {};
    el.classList.add('hd-pt'); el.innerHTML = '';
    var rows = [];
    arr(o.groups).forEach(function (g) {
      var th = tag('div', 'pt-head', g.title + ' · ' + g.controls.length); el.appendChild(th);
      var mine = [];
      g.controls.forEach(function (c) {
        var row = tag('div', 'pt-row' + (c.advanced ? ' adv' : '')), og = tag('span', 'pt-own'), nm = tag('span', 'pt-name', c.name), cl = tag('span', 'pt-ctl'), md = tag('span', 'pt-dots');
        var gl = sv('svg', { viewBox: '0 0 10 10', width: 8, height: 8, 'aria-hidden': 'true' }), gp = sv('path', { d: '' }, gl); og.appendChild(gl);
        row.dataset.key = c.key; row.title = c.path || c.name; row.dataset.find = (c.name + ' ' + c.key + ' ' + (c.abbr || '') + ' ' + g.title).toLowerCase();
        var p = c.props || {}, kind = c.kind || 'knob';
        if (kind === 'knob') { var kh = tag('div'); kh.dataset.key = c.key; cl.appendChild(kh); face(S, p, { size: 'cell', width: p.width || 76 }, function (q) { H.Slider(kh, q, S); }); }
        else if (kind === 'stepper') { var sh = tag('div'); var s2 = {}; for (var k in p) s2[k] = p[k]; s2.flat = true; s2.sm = true; s2.name = s2.name || c.name; cl.appendChild(sh); H.Stepper(sh, s2); }
        else if (kind === 'toggle') {
          var b = tag('button', 'hd-toggle hd-sm hd-focus'); b.type = 'button'; b.setAttribute('role', 'switch'); b.setAttribute('aria-label', c.path || c.name); b.setAttribute('aria-checked', String(!!p.value));
          b.addEventListener('click', function () { var on = b.getAttribute('aria-checked') !== 'true'; b.setAttribute('aria-checked', String(on)); fire(p.onChange, on); }); cl.appendChild(b);
        } else if (kind === 'select') {
          var se = tag('select', 'pt-sel hd-focus'); se.setAttribute('aria-label', c.path || c.name);
          arr(p.options).forEach(function (t, i) { var op = tag('option', '', t); op.value = String(i); se.appendChild(op); }); se.value = String(p.value || 0);
          se.addEventListener('change', function () { fire(p.onChange, +se.value); }); cl.appendChild(se);
        }
        row.appendChild(og); row.appendChild(nm); row.appendChild(cl); row.appendChild(md); el.appendChild(row); mine.push(row);
        rows.push({ row: row, th: th, key: c.key, gp: gp, md: md, last: '' });
      });
    });
    S.add({ render: function () {
      if (!el.isConnected || !el.getClientRects().length) return;
      rows.forEach(function (r) {
        var p = S.params[r.key], ai = p ? S.editing(p) : null, ls = S.mapsOf(r.key), sig = ai + ls.map(function (m) { return m.src; }).join();
        if (sig === r.last) return; r.last = sig;
        r.gp.setAttribute('d', ai === null || ai === undefined ? '' : CORN[ai].glyph); r.gp.setAttribute('fill', ai === null || ai === undefined ? 'none' : 'var(--c' + CORN[ai].id + '-tx)');
        r.md.replaceChildren();
        ls.slice(0, 3).forEach(function (m) { var s = S.src(m.src), d = tag('i'); d.style.setProperty('--src', 'var(--src-' + s.kind + ')'); d.title = s.name; r.md.appendChild(d); });
      });
    } });
    /* the find: hides the rows that do not match, and a section head with no row left; returns the match count */
    function filter(q) {
      q = (q || '').trim().toLowerCase(); var n = 0, shown = new Map();
      rows.forEach(function (r) { var hit = !q || r.row.dataset.find.indexOf(q) >= 0; r.row.hidden = !hit; r.row.classList.toggle('hit', !!q && hit); if (hit) { n++; shown.set(r.th, true); } });
      rows.forEach(function (r) { r.th.hidden = !shown.get(r.th); });
      return n;
    }
    return { el: el, filter: filter, count: function () { return rows.length; } };
  }

  /* ---------- PAGE VIEW, PV1 (ADR-340 r6): a Grid | Table switch in the page's header, with the find box, remembered PER PAGE (view
     and find text). The left column (pictures, the lens) stays put in both views. grid and table are an element, a builder
     function(host, S), or, for the table, { groups } for ParamTable. The find dims what does not match in the grid (never
     hides it: the page keeps its shape) and filters the table. FindPane (the morph grid's) is bound to MorphGrid and does not fit. ---------- */
  function matchText(S, key) { var p = S.params[key]; return (key + ' ' + (p ? p.name + ' ' + (p.abbr || '') + ' ' + (p.path || '') : '')).toLowerCase(); }
  function PageView(el, S, o) {
    o = o || {};
    var id = 'hd.pageview.' + (o.id || 'page'), mem = recall(id) || {}, view = mem.view === 'table' && o.table ? 'table' : (o.view || 'grid'), q = mem.find || '';
    el.classList.add('hd-pv'); el.innerHTML = '';
    var head = tag('div', 'pv-head'), title = tag('span', 'hd-cluster pv-title', o.title || ''), sp = tag('span', 'fd-sp'), seg = tag('div'),
      find = tag('input', 'hd-entry hd-focus pv-find'), cnt = tag('span', 'pv-count'), main = tag('div', 'pv-main'), right = tag('div', 'pv-views');
    find.type = 'search'; find.placeholder = 'find on this page'; find.setAttribute('aria-label', 'Find a control on ' + (o.label || 'this page'));
    if (o.title) head.appendChild(title); head.appendChild(sp); head.appendChild(seg); head.appendChild(find); head.appendChild(cnt);
    if (o.left) main.appendChild(o.left);
    main.appendChild(right); el.appendChild(head); el.appendChild(main);
    var nodes = {}, tbl = null;
    function make(v) {
      if (nodes[v]) return nodes[v];
      var host = tag('div', 'pv-' + v), src = o[v];
      if (v === 'table' && src && !src.nodeType && typeof src !== 'function' && src.groups) { tbl = ParamTable(host, S, src); }
      else if (typeof src === 'function') src(host, S); else if (src && src.nodeType) host.appendChild(src);
      right.appendChild(host); return (nodes[v] = host);
    }
    var pills = H.pillGroup(seg, o.table ? ['Grid', 'Table'] : ['Grid'], view === 'table' ? 1 : 0, (o.label || 'Page') + ' · view', function (i) { show(i ? 'table' : 'grid'); }, o);
    function apply() {
      var n = 0, total = 0;
      if (view === 'table') { if (tbl) { n = tbl.filter(q); total = tbl.count(); } }
      else {
        nodes.grid.querySelectorAll('[data-key]').forEach(function (c) {
          if (c.parentNode.closest && c.parentNode.closest('[data-key]')) return;   // one count per control, not per nested host
          var hit = !q || matchText(S, c.dataset.key).indexOf(q.trim().toLowerCase()) >= 0; c.classList.toggle('pv-dim', !hit); c.classList.toggle('pv-hit', !!q && hit); total++; if (hit) n++;
        });
      }
      cnt.textContent = q ? n + (n === 1 ? ' match' : ' matches') : total ? total + ' controls' : '';
    }
    function show(v) {
      view = v === 'table' && o.table ? 'table' : 'grid'; make(view);
      ['grid', 'table'].forEach(function (k) { if (nodes[k]) nodes[k].hidden = k !== view; });
      pills.set(view === 'table' ? 1 : 0); find.value = q; apply(); keep(id, { view: view, find: q }); fire(o.onView, view);
    }
    find.addEventListener('input', function () { q = find.value; keep(id, { view: view, find: q }); apply(); });
    show(view);
    return { view: function (v) { if (v === undefined) return view; show(v); }, find: function (t) { if (t === undefined) return q; q = String(t); find.value = q; keep(id, { view: view, find: q }); apply(); }, refresh: apply, el: el };
  }

  /* ---------- MODULATOR PICTURES. A source is a pure function of time (Source.f), so a picture needs no history: a trace is f sampled
     over the last `span` seconds. Drawn with the screen roles only, round joins, and a palette cached per theme and scheme (a
     thin row repaints every frame, so the 7 DOM probes Viz.kit makes per call would cost too much at 20 rows). A mod can bring
     its own drawing: m.paint(K, t, opt) gets the kit { g, W, H, k, P } and `col`. ---------- */
  var PALS = {};
  function palette(cv) {
    var host = cv.parentElement || cv, sc = host.closest ? host.closest('[data-scr]') : null,
      sig = (document.documentElement.getAttribute('data-theme') || '') + '|' + (sc ? sc.dataset.scr : '') + '|' + (window.matchMedia && matchMedia('(prefers-color-scheme: dark)').matches);
    if (PALS[sig]) return PALS[sig];
    var P = {}, probe = tag('i');
    probe.style.display = 'none'; host.appendChild(probe);
    ['ground', 'value', 'meter', 'physics', 'marker', 'grid'].forEach(function (n) {
      probe.style.color = 'var(--scr-' + n + ')'; var m = getComputedStyle(probe).color.match(/[\d.]+/g); P[n] = [+m[0], +m[1], +m[2]];
    });
    probe.remove(); return (PALS[sig] = P);
  }
  function col(c, a) { return 'rgba(' + c[0] + ',' + c[1] + ',' + c[2] + ',' + (a === undefined ? 1 : a) + ')'; }
  function kit(cv) {
    var V = Math.min(window.devicePixelRatio || 1, 2), w = Math.round(cv.clientWidth * V), h = Math.round(cv.clientHeight * V);
    if (w < 2 || h < 2) return null;
    if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
    var g = cv.getContext('2d'); g.setTransform(1, 0, 0, 1, 0, 0); g.globalAlpha = 1; g.lineJoin = 'round'; g.lineCap = 'round';   // round joins, always (ADR-340: a miter flips to a bevel and the trace jumps)
    return { g: g, W: w, H: h, k: V, P: palette(cv), col: col };
  }
  function srcOf(S, m) { return S.src(m.id); }
  function valueOf(S, m, t) { if (m.value) return clamp(m.value(t)); var s = srcOf(S, m); return s ? clamp(s.f(t)) : 0; }
  function paintPicture(cv, S, m, t, opt) {
    var K = kit(cv); if (!K) return; opt = opt || {};
    var g = K.g, W = K.W, Hh = K.H, k = K.k, P = K.P, pad = 2 * k;
    g.fillStyle = col(P.ground); g.fillRect(0, 0, W, Hh);
    if (m.paint) { m.paint(K, t, opt); return; }
    var f = m.f || (srcOf(S, m) ? srcOf(S, m).f : function () { return 0; }), v = clamp(f(t)), bip = !!m.bipolar;
    var Y = function (x) { return Hh - pad - (Hh - 2 * pad) * clamp(x); }, X = function (x) { return pad + (W - 2 * pad) * x; };
    if ((m.picture || (m.kind === 'macro' ? 'arc' : 'trace')) === 'arc') {
      var cx = W / 2, cy = Hh / 2 + 1.5 * k, r = Math.min(W, Hh) / 2 - 3 * k, a0 = Math.PI * (90 + 140) / 180, sw = 280 * Math.PI / 180;
      g.lineWidth = 3 * k; g.strokeStyle = col(P.grid, .35); g.beginPath(); g.arc(cx, cy, r, a0, a0 + sw); g.stroke();
      g.strokeStyle = col(P.value); g.beginPath(); g.arc(cx, cy, r, a0, a0 + sw * v); g.stroke(); return;
    }
    var N = opt.big ? 96 : 40, span = m.span || 2.4, i, x;
    g.strokeStyle = col(P.grid, .35); g.lineWidth = .8 * k; g.beginPath(); g.moveTo(0, bip ? Hh / 2 : Y(0)); g.lineTo(W, bip ? Hh / 2 : Y(0)); g.stroke();
    g.beginPath(); for (i = 0; i <= N; i++) { x = i / N; var y = Y(f(t - span * (1 - x))); if (i) g.lineTo(X(x), y); else g.moveTo(X(x), y); }
    if (!bip) { g.save(); g.lineTo(X(1), Y(0)); g.lineTo(X(0), Y(0)); g.closePath(); g.fillStyle = col(P.value, .2); g.fill(); g.restore(); }
    g.beginPath(); for (i = 0; i <= N; i++) { x = i / N; var y2 = Y(f(t - span * (1 - x))); if (i) g.lineTo(X(x), y2); else g.moveTo(X(x), y2); }
    g.strokeStyle = col(P.value); g.lineWidth = 1.5 * k; g.stroke();
    g.fillStyle = col(P.meter); g.beginPath(); g.arc(X(1) - 1.5 * k, Y(v), (opt.big ? 3.2 : 2.2) * k, 0, 2 * Math.PI); g.fill();
  }
  H.ModPicture = { kit: kit, paint: paintPicture, col: col };

  /* a view of one element: it draws only while the element is in the document and has a size (cached rows keep their views while detached) */
  function view(S, el, fn) { S.add({ render: function () { if (el.isConnected && el.getClientRects().length) fn(); } }); }

  var MAP_ICON = '<svg viewBox="0 0 10 10" aria-hidden="true"><circle cx="5" cy="5" r="3.2" fill="none" stroke="currentColor" stroke-width="1.3"/><circle cx="5" cy="5" r="1.1" fill="currentColor"/><path d="M5 0v1.6M5 8.4V10M0 5h1.6M8.4 5H10" stroke="currentColor" stroke-width="1.2"/></svg>';
  function routesOf(S, o, m) { return o && o.routes ? o.routes(m) : S.mappings.filter(function (x) { return x.src === m.id; }); }
  function routeFoot(S, o, m, short) {
    var rs = routesOf(S, o, m), pk = o && o.pageKeys, here = pk ? rs.filter(function (r) { return pk.indexOf(r.param) >= 0; }).length : rs.length;
    if (m.pending && !rs.length) return short ? 'pending' : 'PENDING · map it or it is dropped';
    if (!rs.length) return 'no routes';
    return pk ? here + (short ? ' here' : ' on this page') + (!short && rs.length > here ? ' · ' + rs.length + ' in all' : '') : rs.length + ' route' + (rs.length > 1 ? 's' : '');
  }
  function fmtOf(m, v) { return m.fmt ? m.fmt(v) : String(Math.round(v * 100)); }
  /* the Map handle (ADR-323 r3, ADR-322 r3): press it, then pick a control (the MAP gesture, MD1, does the picking); or drag it onto a
     control, which needs that control's host to carry data-key. A key press (click with no pointer: detail 0) presses it too. */
  function mapButton(S, m, o) {
    var b = tag('button', 'hd-mapb hd-focus'); b.type = 'button'; b.innerHTML = MAP_ICON; b.style.setProperty('--sc', 'var(--src-' + m.kind + ')');
    b.setAttribute('aria-label', 'Map ' + m.name + ': press, then click a control, or drag onto one'); b.title = 'Map ' + m.name + ' (press, then pick; or drag onto a control)';
    var st = null;
    function press() {
      if (o && o.onMap) { o.onMap(m); return; }
      if (!S.src(m.id)) return;
      var on = S.map.on && S.map.src === m.id; S.setSrc(m.id); S.setMap(!on);
    }
    b.addEventListener('pointerdown', function (e) { if (e.button !== 0) return; capture(b, e); st = { x: e.clientX, y: e.clientY, moved: false }; });
    b.addEventListener('pointermove', function (e) { if (st && Math.hypot(e.clientX - st.x, e.clientY - st.y) > 4) { st.moved = true; b.style.cursor = 'grabbing'; } });
    function up(e) {
      var s = st; st = null; b.style.cursor = ''; if (!s) return;
      if (!s.moved) { press(); return; }
      var el = document.elementFromPoint(e.clientX, e.clientY), host = el && el.closest && el.closest('[data-key]');
      if (host && o && o.onDrop) o.onDrop(m, host.dataset.key); else if (host && S.src(m.id)) { S.setSrc(m.id); S.say('Dropped ' + m.name + ' on ' + host.dataset.key + ': click it to map (the host decides the route)'); } else S.say('Dropped on nothing: no route made');
    }
    b.addEventListener('pointerup', up); b.addEventListener('pointercancel', function () { st = null; b.style.cursor = ''; });
    b.addEventListener('click', function (e) { if (e.detail === 0) press(); });
    return { el: b, sync: function () { b.classList.toggle('on', S.map.on && S.map.src === m.id); b.tabIndex = keysOn(o) ? 0 : -1; } };
  }
  /* M-R2 (ADR-338 r9): any modulator is removable; the host owns the undo stack and removes the routes with it as ONE step */
  function xButton(S, m, o) {
    var b = tag('button', 'hd-xb hd-focus', '×'); b.type = 'button'; b.setAttribute('aria-label', 'Remove ' + m.name);
    b.addEventListener('click', function () { fire(o.onRemove, m, routesOf(S, o, m)); });
    return { el: b, sync: function () {
      var n = routesOf(S, o, m).length; b.tabIndex = keysOn(o) ? 0 : -1;
      b.title = 'Remove ' + m.name + (n ? ' and its ' + n + ' route' + (n > 1 ? 's' : '') + ' (one undo step)' : m.fixed ? ' from the list' : '');
    } };
  }
  function swatch(m) { var s = tag('i', 'hd-mkswatch'); s.style.setProperty('--sc', 'var(--src-' + m.kind + ')'); return s; }

  /* ---------- THE FOUR MODULATOR TIERS (ADR-338 r6). m: { id, name, kind (env|lfo|macro|engine: the colour), kindLabel, group, fixed,
     pending, f(t) or a Source of the same id in the Session, fmt, picture 'trace'|'arc', bipolar, paint(K, t, opt), ctls }.
     o: { onMap, onDrop, onRemove(m, routes), onSelect(m), routes(m), pageKeys, selected(), keys }. ---------- */
  /* THIN ROW (26 px): swatch, name over its route count, the picture, the value, Map, x */
  function thin(el, S, m, o) {
    o = o || {};
    el.classList.add('hd-trow'); el.innerHTML = ''; el.dataset.mod = m.id; el.setAttribute('role', 'group'); el.setAttribute('aria-label', m.name);
    var nmB = tag('button', 'tnm hd-focus'), sm = tag('small'), cv = tag('canvas', 'tpic'), val = tag('span', 'tval'), mb = mapButton(S, m, o), xb = o.onRemove ? xButton(S, m, o) : null;
    nmB.type = 'button'; nmB.appendChild(document.createTextNode(m.name)); nmB.appendChild(sm); nmB.title = m.name + (o.onSelect ? ' · open' : '');
    cv.setAttribute('role', 'img'); cv.setAttribute('aria-label', m.name + ': live picture');
    el.appendChild(swatch(m)); el.appendChild(nmB); if (!o.noPicture) el.appendChild(cv); el.appendChild(val); el.appendChild(mb.el); if (xb) el.appendChild(xb.el);
    el.style.setProperty('--tcols', '8px 62px ' + (o.noPicture ? '' : '56px ') + '34px 18px' + (xb ? ' 16px' : ''));
    nmB.addEventListener('click', function () { fire(o.onSelect, m); });
    var last = '';
    view(S, el, function () {
      var t = S.t; if (!o.noPicture) paintPicture(cv, S, m, t, { big: false });
      val.textContent = fmtOf(m, valueOf(S, m, t));
      var s = routeFoot(S, o, m, true) + (o.selected ? o.selected() === m.id : '') + keysOn(o) + S.map.on + S.map.src + !!m.pending;
      if (s !== last) {
        last = s; sm.textContent = routeFoot(S, o, m, true); el.classList.toggle('pend', !!m.pending && !routesOf(S, o, m).length); el.classList.toggle('sel', !!(o.selected && o.selected() === m.id));
        nmB.tabIndex = keysOn(o) ? 0 : -1; mb.sync(); if (xb) xb.sync();
      }
    });
    return { el: el, map: mb, x: xb };
  }
  /* COMPACT CARD (188 px, ADR-323 r1, amended by ADR-338 r7: it is the thin row's EXPANDED state, so its head carries the same four
     parts (swatch and name, value, Map, x) and the picture grows under it, with the controls and the route count) */
  function compact(el, S, m, o) {
    o = o || {};
    el.classList.add('hd-mcard'); el.innerHTML = ''; el.dataset.mod = m.id;
    var hd = tag('div'), val = tag('span', 'tval'), ex = tag('span', 'mc-ex'), mb = mapButton(S, m, o), xb = o.onRemove ? xButton(S, m, o) : null;
    ex.appendChild(val); ex.appendChild(mb.el); if (xb) ex.appendChild(xb.el);
    H.ModCardHeader(hd, { kind: m.kind, name: m.name, kindLabel: m.kindLabel, retrig: null, extra: ex });
    var fr = tag('div', 'mc-pic'), cv = tag('canvas'), ctl = tag('div', 'mc-ctl'), foot = tag('div', 'mc-foot'), ft = tag('span');
    cv.setAttribute('role', 'img'); cv.setAttribute('aria-label', m.name + ': live picture'); fr.appendChild(cv);
    fill(ctl, S, m.ctls, o); if (!arr(m.ctls).length) ctl.appendChild(tag('span', 'hd-hint', m.note || 'nothing to set'));
    foot.appendChild(ft); if (m.sample) foot.appendChild(tag('span', 'mc-tag', m.sample));
    el.appendChild(hd); el.appendChild(fr); el.appendChild(ctl); el.appendChild(foot);
    var last = '';
    view(S, el, function () {
      paintPicture(cv, S, m, S.t, { big: false }); val.textContent = fmtOf(m, valueOf(S, m, S.t));
      var s = routeFoot(S, o, m) + keysOn(o) + S.map.on + S.map.src + !!m.pending;
      if (s !== last) { last = s; ft.textContent = routeFoot(S, o, m); el.classList.toggle('pend', !!m.pending && !routesOf(S, o, m).length); mb.sync(); if (xb) xb.sync(); }
    });
    return { el: el, map: mb, x: xb };
  }
  /* MICRO CHIP (20 px): a name, a level bar, and the chip is the Map handle. For section headers: who modulates this section. */
  function chip(el, S, m, o) {
    o = o || {};
    el.classList.add('hd-mchip', 'hd-focus'); el.innerHTML = ''; el.dataset.mod = m.id; el.style.setProperty('--sc', 'var(--src-' + m.kind + ')');
    var lv = tag('i', 'lv'); el.appendChild(swatch(m)); el.appendChild(document.createTextNode(m.short || m.name)); el.appendChild(lv);
    if (el.tagName === 'BUTTON') el.type = 'button';
    el.setAttribute('aria-label', 'Map ' + m.name + ': press, then click a control'); el.title = m.name + ': press to map (then pick a control)';
    el.addEventListener('click', function () { if (o.onMap) { o.onMap(m); return; } if (!S.src(m.id)) return; var on = S.map.on && S.map.src === m.id; S.setSrc(m.id); S.setMap(!on); });
    var last = '';
    view(S, el, function () {
      lv.style.width = Math.round(valueOf(S, m, S.t) * 100) + '%';
      var s = (S.map.on && S.map.src === m.id) + '' + !!m.pending + keysOn(o);
      if (s !== last) { last = s; el.classList.toggle('on', S.map.on && S.map.src === m.id); el.classList.toggle('pend', !!m.pending); el.tabIndex = keysOn(o) ? 0 : -1; }
    });
    return { el: el };
  }
  /* FULL TIER (the Mod page's detail): header with Map and x, a big picture, the bundle's own editor in o.editor(m, host), the
     controls, and the routes it drives */
  function full(el, S, m, o) {
    o = o || {};
    el.classList.add('hd-mfull'); el.innerHTML = ''; el.dataset.mod = m.id;
    var hd = tag('div'), mb = mapButton(S, m, o), xb = o.onRemove ? xButton(S, m, o) : null, ex = tag('span', 'mc-ex');
    ex.appendChild(mb.el); if (xb) ex.appendChild(xb.el);
    H.ModCardHeader(hd, { kind: m.kind, name: m.name, kindLabel: m.kindLabel, scope: m.fixed ? 'ENGINE' : 'PATCH', retrig: m.retrig === undefined ? null : m.retrig, onRetrig: m.onRetrig, extra: ex });
    var fr = tag('div', 'mf-pic'), cv = tag('canvas'), ed = tag('div', 'mf-edit'), ctl = tag('div', 'mc-ctl'), rt = tag('div', 'mf-routes');
    cv.setAttribute('role', 'img'); cv.setAttribute('aria-label', m.name + ': live picture'); fr.appendChild(cv);
    el.appendChild(hd); el.appendChild(fr);
    if (o.editor) { el.appendChild(ed); o.editor(m, ed); }
    if (arr(m.ctls).length) { fill(ctl, S, m.ctls, o); el.appendChild(ctl); }
    el.appendChild(tag('span', 'cl-lab', 'routes')); el.appendChild(rt);
    var last = '';
    view(S, el, function () {
      paintPicture(cv, S, m, S.t, { big: true });
      var rs = routesOf(S, o, m), s = rs.map(function (r) { return r.param + r.pol + S.amount(r); }).join() + keysOn(o) + S.map.on + S.map.src;
      if (s === last) return; last = s; mb.sync(); if (xb) xb.sync();
      rt.replaceChildren();
      if (!rs.length) rt.appendChild(tag('span', 'hd-hint', m.pending ? 'pending: press Map, then pick a control' : 'no routes'));
      rs.forEach(function (r) {
        var row = tag('div', 'mf-route'), pth = S.params[r.param] ? (S.params[r.param].path || S.params[r.param].name) : r.param, x = tag('button', 'hd-xb hd-focus', '×');
        x.type = 'button'; x.setAttribute('aria-label', 'Remove the route ' + m.name + ' to ' + pth); x.tabIndex = keysOn(o) ? 0 : -1;
        x.addEventListener('click', function () { S.remove(r); });
        row.appendChild(tag('span', 'mf-to', '→ ' + pth)); row.appendChild(tag('span', 'mf-pol', r.pol.toUpperCase())); row.appendChild(tag('span', 'mf-amt', S.amount(r))); row.appendChild(x); rt.appendChild(row);
      });
    });
    return { el: el, map: mb, x: xb };
  }
  H.ModTiers = { thin: thin, compact: compact, chip: chip, full: full };

  /* ---------- MACRO ROW, BR-b (ADR-341 r6 and r7; ADR-340 r11's BR1): a macro or a wheel as a thin row with its slider (a TS1a, 1:1, dragged in
     place): swatch, name over "touches n" (or springs / holds), slider, value, Map. The wheels' rows FOLLOW the wheels: a half-
     second trace behind the handle (the spring's return is visible, not only its end) and, on the bend row, a notch at the centre
     where it springs to. A mouse drag on the bend row springs back to centre on release (50 ms, a time constant in seconds,
     ADR-009); while the hardware holds a wheel the hardware wins (setHardware(v), null lets go); the mod wheel's row holds where it is
     left. The engine side (the intent bus) is not here: every change calls onChange(id, v, by) with by = mouse | spring, and the
     engine's own echo is fed back through set() or setHardware(), never emitted. m: { id, name, wheel: 'bend'|'mod'|null, value, def, fmt, touches }. ---------- */
  var SPRING_TAU = .05, TRACE_S = .5;
  function bendFmt(range) { return function (x) { var s = (x - .5) * 2 * range; return (s > 0 ? '+' : s < 0 ? '−' : '') + Math.abs(s).toFixed(1) + 'st'; }; }
  function MacroRow(el, S, m, o) {
    o = o || {};
    var wheel = m.wheel || null, kind = wheel ? 'engine' : 'macro', home = wheel === 'bend' ? .5 : 0, v0 = m.value === undefined ? home : m.value, SW = o.sliderWidth || 58;
    var p = { key: 'macro.' + m.id, name: m.name, path: (o.path || 'Macros') + ' · ' + m.name, locked: true, g: v0, def: m.def === undefined ? home : m.def,
      fmt: m.fmt || (wheel === 'bend' ? bendFmt(m.range || 2) : pctf), size: 'tiny', width: SW };
    el.classList.add('hd-macrow'); el.innerHTML = ''; el.dataset.mod = m.id; el.style.setProperty('--sc', 'var(--src-' + kind + ')');
    el.title = m.name + (wheel === 'bend' ? ' · springs to the centre when let go' : wheel === 'mod' ? ' · holds where it is left' : '');
    var sm = tag('small'), nm = tag('span', 'tnm'), slot = tag('div', 'mr-sl'), val = tag('span', 'tval'), mb = mapButton(S, { id: m.id, name: m.name, kind: kind }, o);
    nm.appendChild(document.createTextNode(m.name)); nm.appendChild(sm);
    el.appendChild(swatch({ kind: kind })); el.appendChild(nm); el.appendChild(slot); el.appendChild(val); el.appendChild(mb.el);
    H.Slider(slot, p, S);
    var ov = null, trace = null, X = function (x) { return 2 + (SW - 4) * clamp(x); };
    if (wheel) {
      ov = sv('svg', { viewBox: '0 0 ' + SW + ' 20', width: SW, height: 20, 'aria-hidden': 'true', class: 'mr-ov' }); slot.appendChild(ov);
      trace = sv('polyline', { points: '', fill: 'none', stroke: 'var(--value)', 'stroke-width': 2.2, opacity: .35, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }, ov);
      if (wheel === 'bend') sv('path', { d: 'M' + X(.5) + ' 2 L' + (X(.5) - 2.4) + ' -1 L' + (X(.5) + 2.4) + ' -1 Z', fill: 'var(--ink)', 'stroke-linejoin': 'round' }, ov);   // the centre notch: home is drawn where it springs to
    }
    var hw = null, lastNow = null, lastV = v0, pts = [];
    /* the physics runs every frame, painted or not: a wheel the hand moves while the Mods lane is showing must still follow and spring */
    S.add({ render: function () {
      var now = (S.now || 0) / 1000, dt = lastNow === null ? 0 : clamp(now - lastNow, 0, .1), drag = !!(S.drag && S.drag.key === p.key), spring = false;
      lastNow = now;
      if (hw !== null) { if (p.g !== hw) p.g = hw; }
      else if (wheel === 'bend' && !drag && p.g !== home) {
        if (S.reduce || Math.abs(p.g - home) < 1e-4) p.g = home; else p.g += (home - p.g) * (1 - Math.exp(-dt / SPRING_TAU));
        spring = true;   // the snap is the spring too: the host must never read a release as the hand
      }
      if (p.g !== lastV) { lastV = p.g; if (hw === null) fire(o.onChange, m.id, p.g, spring ? 'spring' : 'mouse'); }
      if (wheel && !S.reduce) { pts.push([now, p.g]); while (pts.length && now - pts[0][0] > TRACE_S) pts.shift(); }
    } });
    var lastS = '';
    view(S, el, function () {
      var t = typeof m.touches === 'function' ? m.touches() : m.touches, g = p.g;
      val.textContent = p.fmt(g);
      var lo = 1, hi = 0; pts.forEach(function (q) { lo = Math.min(lo, q[1]); hi = Math.max(hi, q[1]); });
      if (trace) trace.setAttribute('points', hi - lo > .004 ? pts.map(function (q) { return X(q[1]).toFixed(1) + ',16.5'; }).join(' ') : '');
      var s = String(t) + keysOn(o) + S.map.on + S.map.src;
      if (s !== lastS) { lastS = s; sm.textContent = wheel === 'bend' ? 'springs' : wheel === 'mod' ? 'holds' : (t ? 'touches ' + t : 'unbound'); mb.sync(); }
    });
    return { el: el, get: function () { return p.g; }, set: function (v) { p.g = clamp(v); },
      setHardware: function (v) { hw = v === null || v === undefined ? null : clamp(v); if (hw !== null) p.g = hw; }, held: function () { return hw !== null; } };
  }
  H.MacroRow = MacroRow;

  /* ---------- UNDO LINE and the removal text (M-R2, ADR-338 r9): the host removes the modulator and its routes as ONE undo step on its own
     stack; this line says what went and offers Undo. The engine's fixed sources leave the list but stay in the engine; + brings them back. ---------- */
  function UndoLine(el, o) {
    o = o || {};
    el.classList.add('hd-undo'); el.setAttribute('role', 'status'); el.setAttribute('aria-live', 'polite'); el.innerHTML = '';
    var msg = tag('span', 'ul-msg'), btn = tag('button', 'hd-btn hd-sm hd-focus', 'Undo'); btn.type = 'button'; btn.hidden = true;
    el.appendChild(msg); el.appendChild(btn); var undo = null;
    btn.addEventListener('click', function () { var f = undo; hide(); fire(f); });
    function show(text, onUndo) { msg.textContent = text; undo = onUndo || null; btn.hidden = !undo; btn.tabIndex = keysOn(o) ? 0 : -1; el.classList.add('on'); }
    function hide() { msg.textContent = ''; undo = null; btn.hidden = true; el.classList.remove('on'); }
    return { show: show, hide: hide, el: el };
  }
  H.UndoLine = UndoLine;

  /* ---------- THE INFO BOX (ADR-335 r4, ADR-345): one size in one place, its text scrolling inside it, so reading never moves the layout.
     The HOST owns the size and what it says (o.idle, and the content it passes to show and lock: { name, tag, key, text, meta, live }).
     This owns the three things every host would otherwise get wrong:
     - THE LOCK (ADR-345 r2). A selection locks the box, and a hover then changes nothing until another selection or a release, because the
       pointer has to cross other controls to reach the box. Release is the head's button, the host's onRelease, and Escape, whose layer sits
       BEHIND map and pin and in front of keyboard mode, so Escape still leaves MAP first (ADR-338 r11).
     - THE AUTO-SCROLL (ADR-345 r3). Only when the DESCRIPTION runs past the edge: the range and source line under it are reference, so a box
       whose only overflow is that line stays still. It stops for good, for this content, the moment the pointer enters the box or anything
       else moves the scroll, and it never runs under reduced motion. It reads S.now, the session's frame time, never a clock of its own.
     - The scroll starts at the top on new content, and a repaint of the same content keeps its place (a live "now" value must not jump it). ---------- */
  function InfoBox(el, S, o) {
    o = o || {};
    var DELAY = o.delay === undefined ? 2 : o.delay, SPEED = o.speed === undefined ? 8 : o.speed;   // seconds; px per second, about a line every two seconds
    var idle = o.idle || { name: 'Info', tag: 'HOVER', text: '' }, locked = false, armed = false, over = false, since = 0, pos = 0, seen = 0, content = null;
    el.classList.add('hd-info'); el.innerHTML = '';
    var head = tag('div', 'head'), name = tag('span', 'hd-cluster'), mark = tag('span', 'ib-lock'), rel = tag('button', 'hd-btn hd-sm hd-focus', 'Release'), face = tag('div', 'face');
    var parts = { name: name, tag: tag('span', 'tag'), key: tag('span', 'hd-hint'), text: tag('span', 'hd-value'), meta: tag('span', 'hd-hint'), live: tag('span', 'hd-hint') };
    rel.type = 'button'; rel.setAttribute('aria-label', 'Release the info box'); mark.hidden = true;
    mark.appendChild(tag('span', 'tag', 'LOCKED')); mark.appendChild(rel); head.appendChild(name); head.appendChild(mark);
    ['tag', 'key', 'text', 'meta', 'live'].forEach(function (k) { face.appendChild(parts[k]); });
    el.appendChild(head); el.appendChild(face);
    function paint(c) {
      c = c || idle; var same = c === content;
      name.textContent = c.name || ''; ['tag', 'key', 'text', 'meta', 'live'].forEach(function (k) { parts[k].textContent = c[k] || ''; });
      content = c; if (same) return;
      face.scrollTop = 0; pos = 0; seen = 0; since = 0; armed = !over;
    }
    function setMark() { mark.hidden = !locked; rel.tabIndex = keysOn(o) ? 0 : -1; el.classList.toggle('locked', locked); }
    function show(c) { if (!locked) paint(c); }
    function lock(c) { locked = true; setMark(); paint(c); }
    function release() { if (!locked) return; locked = false; setMark(); paint(null); fire(o.onRelease); }
    function runsPast() { return parts.text.getBoundingClientRect().bottom > face.getBoundingClientRect().bottom + 1; }
    function step(dt) {
      if (!armed || over || S.reduce) return;
      if (Math.abs(face.scrollTop - seen) > .5) { armed = false; return; }   // something else moved it: a hand on the scrollbar. Not a wheel listener (B301). `seen`, not pos: a browser rounds what it stores
      if (!runsPast()) { armed = false; return; }                            // it fits, or it has now all come into view
      since += dt; if (since < DELAY) return;
      pos += SPEED * dt; face.scrollTop = pos; seen = face.scrollTop;
    }
    el.addEventListener('pointerenter', function () { over = true; armed = false; });
    el.addEventListener('pointerleave', function () { over = false; });
    rel.addEventListener('click', release);
    var keysAt = S.escapeLayers.findIndex(function (L) { return L.name === 'keys'; }), layer = { name: 'info-lock', active: function () { return locked; }, cancel: release };
    S.escapeLayers.splice(keysAt < 0 ? S.escapeLayers.length : keysAt, 0, layer);
    var was = null;
    S.add({ render: function () { var now = S.now; if (was !== null && now > was) step(Math.min(.5, (now - was) / 1000)); was = now; } });   // a long stall is not a jump, and a slow frame rate keeps its speed
    paint(null); setMark();
    return { show: show, lock: lock, release: release, locked: function () { return locked; }, step: step, parts: parts, el: el };
  }
  H.InfoBox = InfoBox;
  H.ModRemoval = { UndoLine: UndoLine, describe: function (S, m, routes, o) {
    var names = arr(routes).map(function (r) { var q = S.params[r.param]; return q ? q.name : r.param; });
    return 'Removed ' + m.name + (names.length ? ' and its ' + names.length + ' route' + (names.length > 1 ? 's' : '') + ' (' + names.join(', ') + ')' : '') +
      (m.fixed ? ': off the list; the engine keeps the source, + brings it back.' : ': deleted.') + ' One undo step.';
  } };

  /* ---------- THE BANK, M2 (ADR-338 r7) with BT1 (ADR-340 r11): 68 px tall, TWO lanes of 26 px thin rows that scroll sideways, so pages keep
     468 px in the shell (ADR-345 r1). The side column holds the BT1 segments (MAC | MODS), the count and + (on Mods). Mods lists o.mods (a function or an
     array: the host decides what modulates this page, ADR-323 r4); Macros lists o.macros as MacroRows. A click on a row's name opens its
     compact card above the bank (the row expanded; Escape closes it through the layered Escape). Callbacks only: onAdd, onRemove, onMap,
     onDrop, onSelect, onSet(set), onMacro(id, v, by). ---------- */
  function ModBank(el, S, o) {
    o = o || {};
    var set = o.set || 'mods', rows = {}, cards = {}, mrows = {}, open = null, last = '';
    el.classList.add('hd-bank'); el.innerHTML = '';
    var side = tag('div', 'bk-side'), seg = tag('div', 'hd-bseg'), count = tag('span', 'cl-lab'), plus = tag('button', 'hd-btn hd-sm hd-focus', '+'), lanes = tag('div', 'bk-lanes'), pop = tag('div', 'bk-pop');
    seg.setAttribute('role', 'radiogroup'); seg.setAttribute('aria-label', 'Bank: macros or mods');
    plus.type = 'button'; plus.setAttribute('aria-label', 'Add a modulator'); pop.hidden = true;
    var segB = [['macros', 'MAC', 'The eight macros, the pitch bend and the mod wheel'], ['mods', 'MODS', 'The modulators on this page']].map(function (d) {
      var b = tag('button', 'hd-focus', d[1]); b.type = 'button'; b.setAttribute('role', 'radio'); b.title = d[2]; b.dataset.set = d[0];
      b.addEventListener('click', function () { setSet(d[0]); fire(o.onSet, d[0]); }); seg.appendChild(b); return b;
    });
    plus.addEventListener('click', function () { fire(o.onAdd); });
    side.appendChild(seg); side.appendChild(count); side.appendChild(plus); el.appendChild(side); el.appendChild(lanes); el.appendChild(pop);
    var mo = {}; for (var k in o) mo[k] = o[k]; mo.onSelect = function (m) { toggle(m); fire(o.onSelect, m); }; mo.selected = function () { return open; };
    arr(o.macros).forEach(function (m) {
      var host = tag('div'); mrows[m.id] = MacroRow(host, S, m, { keys: o.keys, onChange: o.onMacro, onMap: o.onMap, onDrop: o.onDrop, path: o.macroPath }); mrows[m.id].host = host;
    });
    function list() { return typeof o.mods === 'function' ? o.mods() : arr(o.mods); }
    function row(m) {
      if (!rows[m.id]) { var h = tag('div'); thin(h, S, m, mo); rows[m.id] = h; }
      return rows[m.id];
    }
    function toggle(m) {
      if (open === m.id) { close(); return; }
      open = m.id;
      if (!cards[m.id]) { var c = tag('div'); compact(c, S, m, mo); cards[m.id] = c; }
      pop.replaceChildren(cards[m.id]); pop.hidden = false;
      var er = el.getBoundingClientRect(), rr = rows[m.id].getBoundingClientRect();
      pop.style.left = clamp(rr.left - er.left, 0, Math.max(0, er.width - 192)) + 'px';
    }
    function close() { open = null; pop.hidden = true; pop.replaceChildren(); }
    S.escapeLayers.unshift({ name: 'bank-card', active: function () { return !pop.hidden; }, cancel: close });
    document.addEventListener('pointerdown', function (e) { if (!pop.hidden && !pop.contains(e.target) && !(e.target.closest && e.target.closest('.hd-trow .tnm'))) close(); });
    function setSet(s) { set = s; last = ''; draw(); }
    function draw() {
      var ms = list(), sig = set + ms.map(function (m) { return m.id + routesOf(S, o, m).length + !!m.pending; }).join();
      if (sig === last) return; last = sig;
      segB.forEach(function (b) { var on = b.dataset.set === set; b.setAttribute('aria-checked', String(on)); b.tabIndex = keysOn(o) && on ? 0 : -1; });
      plus.hidden = set !== 'mods'; plus.tabIndex = keysOn(o) ? 0 : -1;
      lanes.classList.toggle('macros', set === 'macros');
      if (set === 'mods') { count.textContent = ms.length + ' here'; lanes.replaceChildren.apply(lanes, ms.map(row)); }
      else { var all = arr(o.macros); count.textContent = all.length + ' rows'; lanes.replaceChildren.apply(lanes, all.map(function (m) { return mrows[m.id].host; })); }
    }
    S.add({ render: function () { if (el.isConnected) draw(); } });
    draw();
    return { set: setSet, get: function () { return set; }, refresh: function () { last = ''; draw(); }, close: close,
      macro: function (id) { return mrows[id]; },
      /* the hardware wheel's value, from the engine: the hardware wins while it holds; null lets go (the bend then springs, the mod wheel holds) */
      setHardware: function (id, v) { if (mrows[id]) mrows[id].setHardware(v); } };
  }
  H.ModBank = ModBank;

  /* ---------- THE MOD PAGE, M5 (ADR-338 r8): a list and a detail. Every modulator a thin row, grouped by kind with a + per kind; the selected one
     fills the rest of the page at the full tier. A Detail | Routes switch makes ADR-266's matrix grid (H.ModMatrixGrid, or the
     host's o.routeView(host, S)) the page's route view. NOT o.routes: the same options reach the tiers, whose routes(m) lists one modulator's routes. kinds: [{ id, label }] (default from the mods' `group`). ---------- */
  function ModPage(el, S, o) {
    o = o || {};
    el.classList.add('hd-mpage'); el.innerHTML = '';
    var left = tag('div', 'mp-list'), right = tag('div', 'mp-right'), bar = tag('div', 'mp-bar'), seg = tag('div'), pane = tag('div', 'mp-pane');
    var sel = o.select || null, rows = {}, full0 = {}, routesHost = null, mode = 'detail', last = '';
    right.appendChild(bar); right.appendChild(pane); el.appendChild(left); el.appendChild(right);
    var title = tag('span', 'hd-cluster', 'Modulator'); bar.appendChild(title); bar.appendChild(tag('span', 'fd-sp')); bar.appendChild(seg);
    var pills = H.pillGroup(seg, ['Detail', 'Routes'], 0, 'Mod page · view', function (i) { setMode(i ? 'routes' : 'detail'); }, o);
    var ro = {}; for (var k in o) ro[k] = o[k]; ro.onSelect = function (m) { select(m.id); fire(o.onSelect, m); }; ro.selected = function () { return sel; };
    function mods() { return typeof o.mods === 'function' ? o.mods() : arr(o.mods); }
    function kinds(ms) { if (o.kinds) return o.kinds; var seen = {}, out = []; ms.forEach(function (m) { var g = m.group || m.kind; if (!seen[g]) { seen[g] = 1; out.push({ id: g, label: (m.kindLabel || g).toUpperCase() }); } }); return out; }
    function drawList() {
      var ms = mods(), sig = ms.map(function (m) { return m.id; }).join() + sel; if (sig === last) return; last = sig;
      var kids = [];
      kinds(ms).forEach(function (kd) {
        var mine = ms.filter(function (m) { return (m.group || m.kind) === kd.id; }), head = tag('div', 'mp-kh'), add = tag('button', 'hd-btn hd-sm hd-focus', '+');
        head.appendChild(tag('span', '', kd.label + ' · ' + mine.length)); add.type = 'button'; add.setAttribute('aria-label', 'Add ' + kd.label); add.tabIndex = keysOn(o) ? 0 : -1; add.addEventListener('click', function () { fire(o.onAdd, kd.id); });
        head.appendChild(add); kids.push(head);
        mine.forEach(function (m) { if (!rows[m.id]) { var h = tag('div'); thin(h, S, m, ro); rows[m.id] = h; } kids.push(rows[m.id]); });
      });
      left.replaceChildren.apply(left, kids);
      if (!sel || !ms.some(function (m) { return m.id === sel; })) sel = ms.length ? ms[0].id : null;
    }
    function drawPane() {
      var ms = mods(), m = ms.filter(function (q) { return q.id === sel; })[0];
      if (mode === 'routes') {
        if (!routesHost) { routesHost = tag('div', 'mp-routes'); if (o.routeView) o.routeView(routesHost, S); else H.ModMatrixGrid(routesHost, S, o.matrix || {}); }
        pane.replaceChildren(routesHost); title.textContent = 'Routes'; return;
      }
      title.textContent = m ? m.name : 'Modulator';
      if (!m) { pane.replaceChildren(tag('span', 'hd-hint', 'No modulators: + adds one')); return; }
      if (!full0[m.id]) { var f = tag('div'); full(f, S, m, ro); full0[m.id] = f; }
      pane.replaceChildren(full0[m.id]);
    }
    function select(id) { sel = id; last = ''; drawList(); drawPane(); }
    function setMode(v) { mode = v; pills.set(v === 'routes' ? 1 : 0); drawPane(); }
    S.add({ render: function () { if (el.isConnected) { var b = last; drawList(); if (b !== last) drawPane(); } } });
    drawList(); drawPane();
    return { select: select, get: function () { return sel; }, mode: setMode, refresh: function () { last = ''; drawList(); drawPane(); }, el: el };
  }
  H.Cluster = Cluster; H.ClusterMatrix = ClusterMatrix; H.Lens = Lens; H.Fold = Fold; H.PageView = PageView; H.ParamTable = ParamTable; H.ModPage = ModPage;
  H.ctlFace = face;
})(window.Horde);
