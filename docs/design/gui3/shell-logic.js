/* GUI 3's shell logic: the three decisions the shell makes that are not drawing, held apart from gui3.html so the seam
   check can run them (tools/labharness/gui3_seam_check.mjs loads this beside the design system's bundle, on a fake DOM).
   horde, 2026-10-30 (ROADMAP B584 slices 6 and 7; ADR-261, ADR-333, ADR-335).
     navGroups        the page bar's groups for the bundle's Page nav. A one-page group is given its page explicitly: left
                      to the bundle, a group whose id differs from its page's (Set, whose page is `settings`) asks the
                      shell for a page named after the group, which does not exist, and the click opens nothing.
     followedSummary  the readout a source's Envelope section shows: the attack and decay of the envelope the source
                      FOLLOWS (its `env` row), not of the first block.
     attachMap        MAP MODE (the Map Mode spec). The bundle's session edits a working list of mappings in place; this
                      writes every change of that list into the DEVICE's matrix (dev.syncRoutes, one history step per
                      gesture) and reads the device's routes back, so a mapping is never only the session's.
     attachMapKeys    the map keys (M, Esc, 1 to 9, P, Backspace) in the shell, whether or not keyboard mode is on (ADR-335 ruling 1).
     sectionInfo      what the info box shows for a hovered section: its description (a DRAFT, with its source) or, honestly,
                      that it has none yet (ADR-335 ruling 4).
   Deterministic: no clock, no randomness. ES5 style, as device.js. */
(function (root) {
  'use strict';

  function navGroups(NAV, PG, isOn) {
    return NAV.map(function (g) {
      return { id: g.id, label: g.label, pages: g.pages.length > 1
        ? g.pages.map(function (pid) { return { id: pid, label: PG[pid].label, on: PG[pid].power ? isOn(PG[pid].power) : undefined }; })
        : [{ id: g.pages[0], label: g.label }] };
    });
  }

  function followedSummary(dev, ENG, ROW, follow) {
    var fb = dev.followed(follow);
    return fb ? ['a', 'd'].map(function (k) { return ENG.display(ROW[fb.viz[k]], dev.shown(fb.viz[k])); }) : [];
  }

  /* A mapping as the device stores it (field order is part of the comparison). */
  function plain(m) {
    var o = { id: m.id, src: m.src, param: m.param, pol: m.pol, depth: m.depth };
    if (m.pol === 'ctl') { o.lo = m.lo; o.hi = m.hi; }
    return o;
  }
  function sig(list) { return JSON.stringify(list); }

  function attachMap(S, dev) {
    var last = sig(dev.routes()), seen = dev.rev(), busy = false;

    /* The device's routes become the session's mappings, keeping the objects when nothing differs (a drag edits them in
       place) and the last-touched mapping by its id when they are rebuilt. Never during a gesture. */
    function pull() {
      if (S.drag) return;
      var want = dev.routes();
      if (sig(S.mappings.map(plain)) !== sig(want)) {
        var lastId = S.lastMap && S.lastMap.id;
        S.mappings = want.map(function (r) { return plain(r); });
        S.lastMap = null;
        if (lastId) S.mappings.forEach(function (m) { if (m.id === lastId) S.lastMap = m; });
      }
      last = sig(want);
    }
    /* The session's mappings become the device's routes. Not during a gesture: a new mapping sits at depth 0 until the
       drag ends, and one released at 0 is dropped by the session itself before it gets here. */
    function push() {
      if (S.drag) return;
      var list = S.mappings.map(plain);
      if (sig(list) === last) return;
      var r = dev.syncRoutes(list);
      r.ids.forEach(function (id, i) { if (id) S.mappings[i].id = id; });
      // reading the device back drops a mapping the matrix refused (RC-4: a row no route can reach), so the session never
      // keeps a ring with no route behind it; the refusal is said
      pull();
      if (r.refused.length) S.say('Refused: ' + r.refused[0].why);
    }
    S.on(function () {
      if (busy) return;
      busy = true;
      try {
        // a route the device gained meanwhile (the bank's drop, undo) is read first, so a stale list never deletes it
        if (sig(dev.routes()) !== last) pull(); else push();
      } finally { busy = false; }
    });
    pull();
    return {
      pull: pull, push: push,
      /* once per frame: read the device's routes when its revision moved */
      frame: function () { if (dev.rev() !== seen) { seen = dev.rev(); if (sig(dev.routes()) !== last) pull(); } }
    };
  }

  /* MAP KEYS ALWAYS (ADR-335 ruling 1, the human, 2026-10-30: "keys always (we'll have to rework them if they interfere with
     keyboard midi controls; I may ultimately reverse the "keys always" decision after some use)").
     The design system's session accepts the map keys in keyboard mode only (ADR-259 ruling 6), and the bundle is vendored,
     read only. So when keyboard mode is OFF the shell takes the keys itself, by calling the session's own methods (setMap,
     setSrc, cyclePol, remove: the same calls the session's listener makes); when it is ON the session's listener runs and
     this one stands aside, so no key is handled twice and keyboard mode's other behavior (digits pin corners with MAP off,
     Esc unpins, the arrows, undo) stays the bundle's. REVERSING THE RULING IS THIS ONE LINE: set it to false and the keys
     are the session's again, keyboard mode only. A test holds both values (mapkeys, mapkeysflag). */
  var MAP_KEYS_ALWAYS = true;

  /* A key typed into a text field, a drop-down or an editable region is that control's, never the map's. */
  function inField(t) {
    var n = t && t.tagName ? String(t.tagName).toUpperCase() : '';
    return n === 'INPUT' || n === 'TEXTAREA' || n === 'SELECT' || !!(t && t.isContentEditable);
  }
  function isMapKey(e) {
    return e.code === 'KeyM' || e.key === 'Escape' || /^Digit[1-9]$/.test(e.code || '') || e.code === 'KeyP' || e.key === 'Backspace' || e.key === 'Delete';
  }

  /* `win` is the window (anything with addEventListener(type, fn, capture)). The listeners run in the CAPTURE phase so they
     see the key before the session's listener and can keep it from running twice or at all (stopImmediatePropagation). */
  function attachMapKeys(S, win, opts) {
    var always = opts && opts.always !== undefined ? opts.always : MAP_KEYS_ALWAYS;
    var mDown = 0, mHeld = false;
    if (!always) return { always: false };
    win.addEventListener('keydown', function (e) {
      if (e.key === 'Alt' || !isMapKey(e)) return;
      // a modified key is the host's or the browser's (Cmd-M minimises, Cmd-P prints): never the map's
      if (e.metaKey || e.ctrlKey) return;
      // in a field the key is the field's: the shell takes none, and keeps the session's listener from taking it either
      if (inField(e.target)) { e.stopImmediatePropagation(); return; }
      if (S.keys || e.repeat) return;               // keyboard mode: the session's own listener handles it, once
      if (e.code === 'KeyM') {
        // tap latches, hold is momentary (the Map Mode spec); the keyup below tells them apart
        if (S.map.on && !S.map.momentary) S.setMap(false); else { mDown = e.timeStamp; mHeld = true; S.setMap(true, true); }
        e.preventDefault(); return;
      }
      if (e.key === 'Escape') { if (S.map.on) S.setMap(false); return; }
      var dg = /^Digit([1-9])$/.exec(e.code || '');
      if (dg) {                                      // 1 to 9 choose the source, only while MAP is on; with MAP off digits are free
        if (!S.map.on) return;
        var n = +dg[1];
        if (n <= S.sources.length) S.setSrc(S.sources[n - 1].id);
        e.preventDefault(); return;
      }
      if (e.code === 'KeyP') { S.cyclePol(e.altKey ? S.focused(S.selected) : S.lastMap); e.preventDefault(); return; }
      if (S.map.on) { S.remove(S.lastMap); e.preventDefault(); }   // Backspace or Delete, only while MAP is on
    }, true);
    win.addEventListener('keyup', function (e) {
      // only a hold the shell began: the session's own keyup would read its own (unset) press time and call every tap a hold
      if (e.code !== 'KeyM' || !mHeld) return;
      mHeld = false;
      if (S.map.momentary) { if (e.timeStamp - mDown < 280) S.setMap(true, false); else S.setMap(false); }
      e.stopImmediatePropagation();
    }, true);
    return { always: true };
  }

  /* ADR-338 ruling 11: Escape is a layered cancel. `layers` is innermost first, each { name, active(), cancel() }; a press cancels
     the first active one and nothing else. Panic is not on Escape (it gets its own control). Capture phase, and it stops the key
     whether or not anything was open, so a later Escape handler (the map keys', the bundle's keyboard mode) can never run a
     second cancel on the same press. In a field the key is the field's (a drop-down closes itself), as for the map keys.
     Returns { last() }: the name of the layer the latest press cancelled, or null (for the self-check). */
  function attachEscape(win, layers) {
    var last = null;
    win.addEventListener('keydown', function (e) {
      if (e.key !== 'Escape' || e.metaKey || e.ctrlKey || inField(e.target)) return;
      last = null;
      for (var i = 0; i < layers.length; i++) if (layers[i].active()) { last = layers[i].name; layers[i].cancel(); e.preventDefault(); break; }
      e.stopImmediatePropagation();
    }, true);
    return { last: function () { return last; } };
  }

  /* The info box for a hovered section. `sections` is GUI3_DATA.sections (the generated copy of sections.json: `text` for
     the described ones, `missing` for the rest) and `find(id)` returns {page, group} for a section id. A section with no
     description says so; it is never given one. Every description is a DRAFT until the human has read it. */
  function sectionInfo(sections, id, find) {
    var f = find(id);
    if (!f) return null;
    var d = sections && sections.text && sections.text[id];
    var name = f.page.label + ' · ' + f.group.title + (f.group.num ? ' ' + f.group.num : '');
    if (!d) return { name: name, tag: 'SECTION', text: 'No section description yet.', meta: 'None drafted: no spec text describes this section (docs/design/gui3/sections.json).', described: false };
    var src = d.src.map(function (c) { return c.path + (c.at ? ' § ' + c.at : ''); }).join(' · ');
    return { name: name, tag: 'SECTION · DRAFT', text: d.text, meta: 'Source: ' + src + ' · ' + sections.status, described: true };
  }

  root.GUI3Shell = { navGroups: navGroups, followedSummary: followedSummary, attachMap: attachMap, attachMapKeys: attachMapKeys, attachEscape: attachEscape,
    sectionInfo: sectionInfo, MAP_KEYS_ALWAYS: MAP_KEYS_ALWAYS };
})(typeof window !== 'undefined' ? window : globalThis);
