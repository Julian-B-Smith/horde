# b50-matrix-pane — the FX matrix pane becomes "well + rail, coupled"

- **Queue item:** B50, phase 1b (the LOOK; phase 1's plumbing landed in PR #607)
- **Why:** The human picked view 3 ("well + rail, coupled") from a four-view
  demo and named two defects with the choice — "the numbers are disappearing
  behind boxes" and "both are showing up with too low resolution". This replaces
  the pane's rendering with that view and fixes both: the coefficient now sits
  INSIDE the cell's top edge (it used to be the whole 26px line, where the next
  row's border crowded it), and the rail canvas is DPR-scaled through
  `fitViz()/VS()` instead of being a 520px bitmap stretched to fit.
- **Evidence consulted:** the lead's demo (`routing-matrix-views.html`, view 3 —
  `drawWell`/`drawRail`/`prep`); `src/gui/gui2.html` — `fitViz`/`VS` (B120,
  ~L1566), `TOK`/`TOKA` and the `_tok` flush on scheme change (~L1596, ~L1855),
  the `canvas { background:var(--scr-tube) }` rule (~L108) which is why the rail
  reads the `--scr-*` set and not the chrome set, `paintOwners` (~L3268) and the
  context menu's `ctx.inField` (~L3096) for the corner state, `MODROUTES`
  (~L2662) for the mod state, `paintLive`'s changed-only guard (~L3419);
  ROADMAP B50 phase-1b acceptance (a)–(g).

## What changed (one file, `src/gui/gui2.html`, the `#mxPane` cluster only)

- **Markup** — `#mxBody` flex wrapper holding `#mxWell` (the DOM grid) and
  `#mxGraph` (the rail canvas). The canvas lost its `width`/`height` attributes:
  `fitViz()` owns the backing store and CSS owns the box, and an attribute is a
  third opinion.
- **CSS** — `#mxPane { column-span:all }` so the pane spans the FX page's 240px
  columns (six slot cards left-to-right do not fit in one). Cells became square
  wells: fill rises from the BOTTOM in `--value`, number inside the top edge.
  `.row.owned`'s inset left stripe is cancelled inside the well and re-drawn as a
  halo ring in the same `--own`; `.mxmod` draws the violet dashed ring;
  `.mxc.mxhi` / `:hover` is the shared highlight in `--physics`.
- **JS** — `mxSetHover(id)` is the one shared hover variable (cell
  `pointerenter` and the rail's ribbon hit test both write it). `mxMarkMod()`
  toggles `.mxmod` straight off `MODROUTES`. `drawMatrixGraph` rewritten as the
  rail: `fitViz(cv)`, draw in bitmap pixels with every width/radius/font × `VS()`,
  `--scr-*` tokens, ribbons weighted by |coefficient| with skip edges arcing over
  (lift clamped to the box), the path that reaches OUT in full ink and the rest at
  `TOKA('--scr-grid', 0.35)`, then slot cards with the hard offset shadow.
  A `ResizeObserver` on the canvas and a `MutationObserver` on `body[class]`
  invalidate — that is what repaints after the FX page is revealed (hidden =
  `clientWidth 0`) and after a theme/scheme change, which clears every canvas and
  repaints only the gviz set.

## Alternatives rejected

- **Cells on the canvas with a hit map.** Four independent passes key off
  `.row` + `[data-p]`: the context menu (`ev.target.closest('.row')`),
  `paintOwners`' corner mark, `paintExempt`, and the armed-corner ghost view.
  Canvas cells would mean re-implementing all four against a hit map — four
  second copies. Only the rail is a canvas; the well stays DOM.
- **A literal four-arc halo in all four corner colours** (what the demo draws).
  `paintOwners` knows WHICH corner owns a cell; painting all four would claim the
  field applies four corners there. The halo takes `--own`, which IS this cell's
  colour out of `MCOLORS`. Flagged to the lead as the one interpretive call
  against acceptance (e)'s wording.
- **A frame loop for the rail.** Rejected on ADR-143; the pane is invalidated by
  events only.

## DOM, per theme (Chrome headless, dpr 2, `file://` load of the shipped file)

Same split patch both times: src→S1 1.00 · S1→S2 0.80 · S1→S3 0.60 · S2→S4 1.00 ·
S2→OUT 0.30 · S4→OUT 1.00, S3 a dead end. Ownership and mod routes fed through
`hzMorphOwners` / `hzModRoutes` — the binds the knobs read, no pane-specific flag.

```
========== LIGHT (body.scr-orchid) ==========
<canvas id="mxGraph" width="1638" height="334">      (CSS box 819x167, VS()=2)
  Src 1 > Slot 1    p=10000  row="row"               cell="mxc on" mx=0.500 own=-       text='1.00'
  Slot 1 > Slot 2   p=10065  row="row owned"         cell="mxc on" mx=0.400 own=#FF2E88 text='0.80'
  Slot 1 > Slot 3   p=10066  row="row mxmod"         cell="mxc on" mx=0.300 own=-       text='0.60'
  Slot 2 > Slot 4   p=10131  row="row"               cell="mxc on" mx=0.500 own=-       text='1.00'
  Out Slot 2        p=20001  row="row mxterm"        cell="mxc on" mx=0.150 own=-       text='0.30'
  Out Slot 4        p=20003  row="row mxterm owned"  cell="mxc on" mx=0.500 own=#A6F219 text='1.00'
  (10 more cells at 0, 4 init cells, 11 absent cells as .mxgap)

========== DARK (body.dark) ==========
<canvas id="mxGraph" width="1638" height="334">      (CSS box 819x167, VS()=2)
  ... byte-identical cell table; only the body class and therefore the resolved
      tokens differ. Corner hexes are the same in both themes by design
      (--cA..--cD are unchanged in body.dark), so `own=` matching is expected.
```

Backing store vs CSS box, measured across device scale factors (the stamp waits
for the first real draw rather than a timer, because headless virtual time
distorts wall-clock windows):

```
dpr 1  ->  MX  821x168 css 821x168     (VS()=1)
dpr 2  ->  MX 1638x334 css 819x167     (VS()=2)  — both themes
dpr 3  ->  MX 1640x334 css 820x167     (VS()=2, clamped as VS() specifies)
```

## Frame cost (acceptance f) — MEASURED INCONCLUSIVE, entailed by construction

Counting `setTransform` calls on the `mxGraph` context (exactly one per
`drawMatrixGraph`) over a 27.1 s idle window read **1 draw**. The ceiling control
read **frames=1** over the same window: headless Chrome under `--virtual-time-budget`
runs essentially no rAF frames, so the idle number is a stopped clock and proves
nothing (L0032 — the control is what caught it). A hover plant firing 54 times in
the same window read **8 draws / 8 frames**, which does show the one-redraw-per-frame
coalescing guard working, but not the idle rate.

What IS entailed from the call graph: `drawMatrixGraph` is reached only from the
rAF that `mxInvalidate` schedules; `mxInvalidate` is called from
`buildMatrixPane` (once), `paintMatrixCell` (only when a value is actually
painted), `mxSetHover`, the ResizeObserver and the MutationObserver; and
`paintLive` skips ids whose value is unchanged. Nothing registers a per-frame
callback and `hzFrame` is untouched. Entailed, not verified — a real-browser
measurement is the open item.

- **Verify:** `fast`, exit 0, git `2d0905a` (`.harness/last-verify.json`, the
  tree this change sits on). `node tools/labharness/lab_load_check.mjs` GREEN
  (42 labs, 0 broken); `python3 tools/gen_gui_controls.py --check` GREEN
  (197 generated controls, gui2 markup current). `./verify full` not re-run: no
  C++ moved.
- **Open questions:**
  1. Acceptance (e) asks for "the four-colour halo the other morphable controls
     use". No four-colour painter exists; the morphable mark in this file is
     `.row.owned`'s stripe in the ONE owning corner's colour. The halo here takes
     `--own` for that reason. If the human meant a literal four-arc ring, it is a
     one-line CSS change.
  2. `mxInvalidate` schedules via `requestAnimationFrame`. This file already
     records that "the measured preview browser fires none at all" (the morph
     pad's re-bake note), and the probe above measured ~1 frame per 27 s in
     headless. That is PHASE-1 plumbing, unchanged here and deliberately out of
     this brief's scope, but if the pane ever renders blank in the plugin this is
     the first suspect.
  3. The frame-cost criterion is entailed, not measured. Closing it needs a real
     browser (or the plugin) rather than headless virtual time.

---

## Review fixes — 2026-09-17 (appended; the sections above stand as written)

The lead reviewed the pane above against the human's chosen demo view (updated
after the first pass read it) and named three deviations. All three are in the
`#mxPane` cluster and its CSS block; nothing else moved.

1. **Illegal cells are ABSENT, not ghosted.** `.mxgap` drew a dashed box at
   `opacity:.20`, so the upper-left triangle read as a lattice of ghosts. The
   rule now carries the SIZING ONLY — no border, no background, no outline, no
   hover — because the element exists only to hold the grid slot. Legality is
   enforced on the read side (ADR-088); the empty staircase IS the rule, and a
   drawn placeholder would be a second copy of it in the one place it cannot be
   edited.
2. **The OUT column is no longer green.** `.mxterm .mxc { border-color:
   var(--celebrate) }` is deleted, not recoloured. `--celebrate` is "meters in
   the green" (README.md:366) — an OK status, which a routing coefficient does
   not have. Terminal cells take the same ink border as every other cell; the
   column is distinguished by its `OUT` header and its position, as the demo's
   well distinguishes it. `.mxterm` stays on the row as a DOM marker with no
   colour, and a comment where the rule was says why it must not get one.
3. **Rail values sit in a pill above the ribbon.** The label was drawn on the
   stroke at `my - 8*V`, where a heavy edge (up to ~3.1*V wide) struck it
   through. Now: measure the label, punch a `--scr-tube` hole `16*V` above the
   ribbon's midpoint (above the apex for a skip edge), a `1*V` outline in the
   ribbon's ink, then the number. Pill outline and number take the hover colour
   TOGETHER, so the highlight is one object from either direction.

**One structural departure from the demo, deliberate.** The demo draws each
pill inline in its edge loop, which leaves a pill crossed out by any ribbon
drawn AFTER it — measured in the first shot of this fix: the `0.80` pill had the
`0.60` skip arc through it, the same unreadable label one neighbour removed. So
the pills are a SECOND pass over a collected list, after every ribbon and still
before the cards. Same treatment, one draw-order change inside the one painter.

**One interpretive call, flagged.** The brief says the pill outline takes "the
ribbon's colour" and "the text in the same colour" (the demo uses ink for both).
The outline is `hot ? PHY : INK` as asked; the NUMBER keeps `--scr-value`, the
authored-value token the well's fill and the cell's number already use, because
recolouring the coefficient was not one of the three defects and "nothing else
changes" protects it. On hover both go `--scr-physics`, which is the clause with
teeth. A one-word change if the lead meant ink.

**Not done, and not silently.** The brief also states "the corner-tier halo and
the mod ring ON THE RAIL wrap the PILL … not the stroke". There is no halo and
no mod ring on the rail to move: `drawMatrixGraph` has never drawn either (the
section above records why — `paintOwners` and `MODROUTES` paint them on the DOM
well, and the rail carries no second copy). Adding two new painters is not a
correction of a deviation, so it is left for the lead to queue if the rail is
meant to carry them too.

### Evidence — the FX page after the fix, both themes

Probe: the SHIPPED `src/gui/gui2.html` + the same boot script the section above
used (`mkpreview.py --split --marks`, the split patch reported through
`hzGetParams` and the marks through `hzMorphOwners` / `hzModRoutes`), plus an
isolate pass that hides the pane's siblings so the shot frames the pane. Chrome
headless, `--force-device-scale-factor=2`, `file://`.

```
========== LIGHT (body.scr-orchid) ==========
<canvas id="mxGraph" width="1100" height="334">   title: MX 1100x334 css 550x167 dpr 2
  Src 1 > Slot 1     p=10000  row="row"               cell="mxc on" mx=0.500 own=-       text='1.00'
  Slot 1 > Slot 2    p=10065  row="row owned"         cell="mxc on" mx=0.400 own=#FF2E88 text='0.80'
  Slot 1 > Slot 3    p=10066  row="row mxmod"         cell="mxc on" mx=0.300 own=-       text='0.60'
  Slot 2 > Slot 4    p=10131  row="row"               cell="mxc on" mx=0.500 own=-       text='1.00'
  Out Slot 2         p=20001  row="row mxterm"        cell="mxc on" mx=0.150 own=-       text='0.30'
  Out Slot 4         p=20003  row="row mxterm owned"  cell="mxc on" mx=0.500 own=#A6F219 text='1.00'
  .mxgap elements: 11
  .mxgap CSS   : .mxgap { aspect-ratio:1 / 1; min-height:24px; max-height:44px; }
  .mxterm .mxc CSS rule present: False        .mxterm rows in DOM: 4

========== DARK (body.dark) ==========
<canvas id="mxGraph" width="1100" height="334">   title: MX 1104x334 css 552x167 dpr 2
  ... byte-identical cell table, same 11 bare .mxgap slots, same absent rule.
```

Read off the screenshots: the upper-left triangle and the sources' OUT cell are
EMPTY (fix 1); the two terminal cells carry the ordinary ink border, and the
only green on the pane is `Out Slot 4`'s `--own` halo, which is corner D's own
colour from the ownership table (fix 2); every coefficient on the rail sits in
an outlined pill clear of its ribbon and of the crossing arc, magenta on the
tube ground, and the hovered edge's pill outline AND number turn violet with
its ribbon (fix 3, and the shared hover still couples both ways).

The LIGHT dump needed a 30 s virtual-time budget: at 14 s it snapshotted an
undrawn canvas (no `width` attribute, title still `HYPERSAW`) while DARK came
back drawn — the same rAF-vs-dump race the section above hit, and the reason the
backing store is read off the title stamp rather than the attribute.

- **Verify:** `fast`, exit 0, git hash recorded in `.harness/last-verify.json`
  for this commit. `node tools/labharness/lab_load_check.mjs` GREEN (42 labs,
  0 broken); `python3 tools/gen_gui_controls.py --check` GREEN (197 controls,
  gui2 markup current). `./verify full` not re-run: no C++ moved, and the
  earlier sections' hash stands for the C++ tree this sits on.
- **Open questions:**
  1. The pill's number colour (`--scr-value` vs ink) — the interpretive call above.
  2. Whether the rail should carry a corner halo / mod ring at all. It does not
     today, so the brief's clause about them wrapping the pill has nothing to act
     on; the lead's ruling decides whether that is a gap or the design.
  3. At the pane's minimum width (`#mxGraph min-width:280px`) the gap between two
     cards is narrower than a pill, so the cards (drawn last) clip the pill's
     ends. Pre-existing — the bare label overlapped there before — and unchanged
     by this fix, but it is the shape the narrow-pane degradation now takes.
