# set-bipolar-feeds — the `Bipolar feeds` device preference on the SET page (GUI only)

- **Queue item:** B50, paragraph "BIPOLAR FEEDS TOGGLE" (human 2026-09-17:
  *"Maybe there should be a bipolar feeds toggle stored in the settings page
  actually, since it's going to be rare for someone to actually want them"*).

- **Why:** the routing crosspoints are bipolar host parameters (−2…+2, ruled to
  stay — phase 1c's retraction: a negative feed is a polarity inversion). The
  preference bounds what the WELL will let the mouse author and nothing else, so
  the declared range never moves and a patch renders identically on an
  installation whatever the preference says. Three consequences of that choice,
  all deliberate: the clamp is applied in ONE place (`mxWire`'s `send`, which
  every gesture funnels through) rather than per gesture; a value outside the
  editable floor is still shown, still rendered and MARKED (`±`), because a
  value the preference hides from the mouse must not be hidden from the eye; and
  the preference is stored with the interface, never with the patch.

- **Evidence consulted:**
  - `ROADMAP.md` B50, "BIPOLAR FEEDS TOGGLE" (acceptance, verbatim) and the
    PHASE 1c retraction paragraph above it (why the range stays bipolar).
  - `src/gui/gui2.html:1911` / `:1952` — the EXISTING device-preference store:
    the theme chips save `{schemeIx, modeIx}` through `hzPresetSave('prefs',
    'gui', …)` and restore through `hzPresetLoad`. That is the mechanism reused;
    no new store was added. `src/gui/preset_store.h:87` confirms `prefs` is a
    user kind with a sanitised free-form record name, so `'set'` needs no C++.
  - `src/gui/gui2.html:2285` (and `hypersaw_gui_common.h:199`) — ADR-105 A2: the
    plugin's webview loads via `setHTML`, an OPAQUE ORIGIN where `localStorage`
    THROWS. Hence host store primary, `localStorage` only as the lab's fallback,
    which is exactly the split `presetStore` already makes.
  - `src/gui/gui2.html:1183` — the `data-diag` checkboxes, which the ROADMAP
    line names ("like the diagnostics toggles"). They are the SHAPE that was
    copied (a hand-placed non-parameter checkbox on SET), not the persistence:
    their own note says they "are session-local and never saved", so the
    webview's local store the ROADMAP points at is the preset/prefs store above.
  - `mxRange` / `mxWire` / `paintMatrixCell` / `mxMarkMod` (the pane's existing
    read, write and mark paths, all reused).

- **Alternatives rejected:**
  - *A new preference store (a `hz.prefs` localStorage key of its own).*
    Rejected: `localStorage` is dead in the plugin (opaque origin) — a new store
    would work in the lab and silently fail in the product, which is the exact
    bug ADR-105 A2 records.
  - *Writing into the theme chips' `'gui'` record.* Rejected: two writers of one
    record, each serialising only its own fields, is a clobber waiting to
    happen. Own record name `'set'`, and `SETPREFS` holds the whole record so a
    second preference added later merges rather than overwrites.
  - *Clamping in the three gesture handlers.* Rejected: `send` is the single
    write path; three clamps is three chances to honour it in two.
  - *Making the preference change the declared range.* Rejected outright — that
    is the one thing the acceptance forbids (patches must load and render
    identically whatever the preference).
  - *Marking with `::after`.* Rejected: `::after` on a well cell is the
    modulation ring, and a cell can be modulated AND hold a negative value.

- **The one interpretive call (flagged for the lead):** the acceptance says the
  well's editing "clamps to 0…1" while the crosspoint's declared range is
  −2…+2 and the OUT column's is 0…2. Implemented as a FLOOR at 0 with the
  declared ceiling untouched, because (a) the preference is named for and
  motivated by NEGATIVE feeds, and (b) the acceptance's own marker rule covers
  only negative values "so nothing is hidden" — under a 0…1 reading a cell
  holding 1.6 would be un-editable and unmarked, a hole the floor-only reading
  does not have. If the lead wants the ceiling too it is one line
  (`mxEditLo` gains an `mxEditHi` sibling); the mark rule would then need to
  cover >1 as well or the "nothing is hidden" clause breaks.
  A second, smaller call: the floor applies to every well cell whose declared
  `lo` is negative, which is the crosspoints AND the four `slotInit` cells (the
  init row, currently inert). The acceptance says "the well's … editing", so one
  rule for the well rather than two rules inside it.

- **Verify:** `./verify fast` — exit 0 (GREEN), on this worktree over
  `c95e8bb` (the branch point, `origin/main` containing #617); re-run chained to
  the commit per L0051. Plus the two gates the brief names:
  `node tools/labharness/lab_load_check.mjs` → `GREEN — 42 labs loaded, 0
  broken, 1 skipped` (gui2.html OK), and `python3 tools/gen_gui_controls.py
  --check` → `GREEN (197 generated control(s), gui2 markup current)` — the SET
  toggle is hand-placed, carries no `data-p`, and therefore enters neither the
  presentation table nor `gui_reach`.

- **Behavioural proof (a `file://` load of the shipping file, headless Chrome,
  the page's own dev stub made stateful so a planted value arrives by the normal
  paint path).** Verbatim probe output:

  ```
  well built: true (18 cells)
  boot: BIPOLAR_FEEDS=false checkbox=false store=null
  cell 10001 starts at 0
  A  OFF, scroll DOWN from 0      -> 0   [expect 0]
  B  negative from the shell      -> value=-0.75 text="-0.75" class="mxc on neg mxbip" ::before="±"
  B  control cell 10000 (=1) marked? false   [expect false]   marked cells total=1 [expect 1]
  C  OFF, drag UP 10px from -0.75 -> 0.267   [expect ~+0.27, not -0.48]
  D  toggle ON: BIPOLAR_FEEDS=true marked cells=0 [expect 0]
  E  ON, scroll DOWN from 0       -> -0.1   [expect NEGATIVE]
  F  store after the toggle       -> {"bipolarFeeds":true}
  RELOAD (fresh load, same profile): store={"bipolarFeeds":true} BIPOLAR_FEEDS=true checkbox=true  [expect true]
  ```

  The B line carries the must-NOT-fire control this project keeps demanding
  (L0032): a positive cell in the same well must be unmarked and the marked
  count must be exactly 1 — a mark rule that painted every cell would pass the
  "the negative cell is marked" assertion just as well.

- **Open questions:**
  1. The 0…1-vs-floor-only reading above — the lead's call, one line either way.
  2. Persistence is proven in the LAB (localStorage under `file://`). The plugin
     path (`hzPresetSave('prefs','set', …)`) is the same call the theme chips
     already make and is unverified here — no plugin build was run, and this
     brief was GUI-only. It is a hypothesis, not a verified claim, until someone
     toggles it in the plugin and reopens the editor.
  3. Not addressed on purpose (out of scope): the parameter menu's typed/reset
     entry and the mod matrix ignore the preference — a modulated crosspoint can
     still be driven negative, which is correct (modulation is not authoring)
     but is a thing the ± mark does not currently distinguish.
