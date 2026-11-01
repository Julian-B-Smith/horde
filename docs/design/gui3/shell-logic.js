/* GUI 3's shell logic: the decisions the shell makes that are not drawing, held apart from gui3.html so the seam
   check can run them (tools/labharness/gui3_seam_check.mjs loads this beside the design system's bundle, on a fake DOM).
   horde, 2026-10-30 (ROADMAP B584 slices 6 and 7; ADR-261, ADR-333, ADR-335); the keys and Escape moved to the session 2026-11-01 (ADR-338 r11).
     navGroups        the page bar's groups for the bundle's Page nav. A one-page group is given its page explicitly: left
                      to the bundle, a group whose id differs from its page's (Set, whose page is `settings`) asks the
                      shell for a page named after the group, which does not exist, and the click opens nothing.
     followedSummary  the readout a source's Envelope section shows: the attack and decay of the envelope the source
                      FOLLOWS (its `env` row), not of the first block.
     attachMap        MAP MODE (the Map Mode spec). The bundle's session edits a working list of mappings in place; this
                      writes every change of that list into the DEVICE's matrix (dev.syncRoutes, one history step per
                      gesture) and reads the device's routes back, so a mapping is never only the session's.
     MAP_KEYS_ALWAYS  whether the map keys and Escape work with keyboard mode off (ADR-335 ruling 1); the page hands it to the
                      session as `mapKeysAlways`. The keys themselves, and the layered Escape, are the session's (ADR-338 ruling 11).
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
     The design system's session accepts the map keys (M, 1 to 9, P, Backspace) AND Escape in keyboard mode only (ADR-259
     ruling 6), or in every mode when the host sets the session option `mapKeysAlways` (master batch 2, ADR-338 ruling 11:
     the keys, the layered Escape and the field and Cmd/Ctrl rules are the session's own now; this file used to run a second
     listener of its own for them). gui3.html passes THIS constant as that option, as it stands. REVERSING THE RULING IS THIS
     ONE LINE: set it to false and the keys are keyboard mode's again. Escape follows the same option (the session's, not
     ours), so with it false the shell's menu and the bank's open card also close on Escape in keyboard mode only: the human
     is told this where the ruling is reversed (gui3/README.md). A test holds both values (mapkeys, mapkeysflag). */
  var MAP_KEYS_ALWAYS = true;

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

  root.GUI3Shell = { navGroups: navGroups, followedSummary: followedSummary, attachMap: attachMap,
    sectionInfo: sectionInfo, MAP_KEYS_ALWAYS: MAP_KEYS_ALWAYS };
})(typeof window !== 'undefined' ? window : globalThis);
