/* GUI 3's shell logic: the three decisions the shell makes that are not drawing, held apart from gui3.html so the seam
   check can run them (tools/labharness/gui3_seam_check.mjs loads this beside the design system's bundle, on a fake DOM).
   horde, 2026-10-30 (ROADMAP B584 slice 6; ADR-261, ADR-333).
     navGroups        the page bar's groups for the bundle's Page nav. A one-page group is given its page explicitly: left
                      to the bundle, a group whose id differs from its page's (Set, whose page is `settings`) asks the
                      shell for a page named after the group, which does not exist, and the click opens nothing.
     followedSummary  the readout a source's Envelope section shows: the attack and decay of the envelope the source
                      FOLLOWS (its `env` row), not of the first block.
     attachMap        MAP MODE (the Map Mode spec). The bundle's session edits a working list of mappings in place; this
                      writes every change of that list into the DEVICE's matrix (dev.syncRoutes, one history step per
                      gesture) and reads the device's routes back, so a mapping is never only the session's.
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

  root.GUI3Shell = { navGroups: navGroups, followedSummary: followedSummary, attachMap: attachMap };
})(typeof window !== 'undefined' ? window : globalThis);
