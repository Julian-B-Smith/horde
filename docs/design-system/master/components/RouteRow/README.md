# Route row

One mapping as one line: its source, the four-state polarity pill, its destination, its depth (or a Control's low and high ends), what it was set to against what it does now, + depth and remove.

**The consumer provides** a `Horde.Session` and the mapping. `Horde.RouteList(el, S, { param?, label? })` lays out every mapping (or one destination's), each followed by its depth routes, indented; it rebuilds when the mappings change and renders in the session's loop. `Horde.RouteRow(el, S, m, { depthRoute? })` builds one row and returns `{ el, sync, render }`; call `sync()` on `S.on` and `render()` each frame (RouteList does both).

**Anatomy** (a grid row, 28 px tall; two lines below 480 px of container)
- Source: the kind dot (`src-*`), the name (mono 10 px, ellipsis) and its number in `hint`.
- Polarity pill: 44 × 28, mono 9.5 px, UNI, BI, INV, CTL. **CTL** fills with the source's colour and `on-src` text, because it takes the parameter over. **The chip is MP1** (ADR-340 r9; `H.PolChip`, on every route's row and every inspector row): **click** cycles UNI → BI → INV → CTL → UNI through `S.cyclePol`, which skips CTL while another source already controls the parameter (at most one Control, ADR-266 ruling 2); **shift-click** goes the other way; a **vertical drag** widens and thins the band, which is the depth (up is wider; 160 px for the full range, Shift ×0.25, re-anchored at the stops and on a Shift change; a Control's band keeps its centre; per-corner depths move the editing corner's), and while it moves the chip shows the amount. The click that ends a drag cycles nothing. The pill's title and accessible name say so. A depth route's chip cycles UNI, BI, INV only. The route-menu entry for polarity is **not in the master** (it has no control context menu); and the same cycle is on the controls as the ⌘⌥-click chord (the Knob README).
- Destination: "→ Cutoff". A depth route reads "↳ depth of LFO 1 → Cutoff" and indents 16 px.
- Amount, the house slider track (6 px, `well-alt`, ink edge, 13 px handle):
  - UNI, BI, INV: depth from 0 in the source's colour. When a depth route moves it, a 3 px band under the track shows the depth's reach and a tick the depth now, both in the depth route's colour;
  - **CTL**: two 9 px handles, low and high, the band between in the source's colour and the position now as a 2 px ink tick.
- Readout, 84 px, tabular: authored → now. UNI, BI, INV: "30% → +7%" (the depth, then its push now, signed). CTL: "20–80% → 56%" (the range, then the position).
- +◇ (28 px): adds a depth route from the current source. It refuses, in `hint`, on a depth route and on a Control, and a click says why.
- × (28 px): removes the mapping and its depth routes.
- The last-touched mapping's row is `sel-neutral` with its edge.

**Gestures**
- Drag a track: relative, one track length the full range, Shift ×0.25, re-anchored at the stops and on a Shift change (ADR-340 r2: no dead range after an overshoot, no jump when Shift goes down mid-drag). On a Control the nearer handle moves, never past the other. With per-corner depths the editing corner's depth moves.
- Double-click a track: resets it (25%; a Control 0–100%).
- Click the row: makes it the last touched and focuses it on its control (`S.lastMap`, `S.focus`), so Option-drag on the knob edits it.
- Keyboard mode only: arrows on a focused handle ±1% (Shift ±0.25%). The drag is the mouse route.
- Each gesture ends in one `S.emit()`: one undo step for the host.

**Fits** 320–1000 px; below 480 px of container the row is source, pill and × on line one, the destination and +◇ on line two, the track and the readout on line three. Nothing live changes its size: fixed columns, tabular figures, ellipses.

**Placeholders**
- Depth routes (the lab's B70 answer, one level, UNI, BI and INV only): `S.depthRoutes = [{ src, pol, depth, of: m }]`. A new one comes from the current source (`S.map.src`) at UNI 25%; the lab used ENV 1. The human's ruling on depth modulation (B207) decides.
- A new Control's range comes from the core (its own placeholder: the value ± the depth, at least ±25%); its reset here is 0–100%.
- Silent corner (ADR-266 ruling 4): a corner-level mapping (an optional `corner`) whose corner weighs 0 greys (`hint` text, `dis` fills), shows the corner's glyph and "silent", and claims no push ("—"). Mappings are global for 1.0; corner-level mappings decide this when they arrive.
- The lab's row re-aims a route with source and destination menus; here a mapping keeps its source and destination (remove it and map again). Kept to the Map Mode spec, which never re-aims.

**Sources** docs/design/mod-matrix-lab.html: the route editor's styles :266-305, `buildRoutes` :1875-1941, `paintRoutes` :1943-1961, `addDepth` :1862. The pill's four states and Control are ADR-266 ruling 2.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
