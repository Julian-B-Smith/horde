# Quantum pad

A layer on the morph XY pad that paints the selected row across the pad: its quantum regions with draggable boundaries, its value map, or a gate's level.

**The consumer provides** an element already made by `H.XYPad` (the layer never forks the pad), the session, and the row to paint (`opts.key`, else the session's selected row). Each row is `{ key, name, kind: 'c' | 's' | 'g', vals[A..D], labels? (structural), unit? }`. Who owns a row where comes from the morph layer's readout (RC-8); the reference bundle reads it through `H.Morph.engine`, a labelled deterministic stand-in. The UI never computes ownership.

**Anatomy** (a canvas layer under the puck and the corner chips, over the pad's wash)
- A row that picks (structural always, continuous under QUANTUM): its corners' REGIONS, each in its corner fill (`cA`…`cD`), stronger for a higher stored value, pre-mixed over `scr-ground` and painted opaque. Boundaries in `on-corner`, which reads on every corner fill in both themes: dashed 1.4 while they follow the seed and Cohesion, solid 2.2 once the row is LOCKED; the boundary under the hand or being dragged at 3.6. The corner chips stay on top, so every region still shows its glyph at its own corner.
- A continuous row under BLEND or STEPPED: its value as brightness in `scr-physics` over `scr-ground` (the scale bar keys it), with scanlines.
- A gate: its level the same way, and the line where the switch flips in `scr-marker`.
- Under STEPPED: a hollow `scr-marker` square where the morph is heard, stepping after the puck.
The layer's accessible name states the row, its region shares, whether it is locked and the owner at the puck.

**Gestures**
- Drag a boundary: it moves to the pointer and the row LOCKS (its regions are then its own; the seed and Cohesion no longer reach it). One drag is one undo entry.
- Double-click a locked boundary: unlock (back to the seed's regions).
- A click on empty pad moves nothing (B269: "it will be necessary to be able to hear how the value changes at the current position"). Only dragging the puck moves it; the corner chips still pin.
- Hover sets the session's hover point; the scale bar shows the value under the pointer, beside the pad, never over it.
- A unit (rows sharing `unit`) flips, and locks, as one.

**Fits** the pad keeps the XY pad's size; the canvas fits its box at the display's pixel ratio (capped at 2) and repaints only when the row, mode, Cohesion, seed, lock, size or theme changes.

**Placeholders**
- The region law. The stand-in scores a corner as its weight plus a per-row offset (the same hash and ±0.16 spread as `Session.ownerOf`), mixed toward one shared offset by Cohesion; a lock keeps the row's own four offsets, bounded to ±0.45 so a corner always owns its own corner. Decided by: the morph layer's specification (H2-PLAN `fxmorph`/`modmorph`, ADR-261's requirement on B532), which also publishes the scores the boundary drag edits.
- What a lock stores. Assumed: four offsets per unit, sparse, keyed by the unit (the lab's proposed `morphLocks` state, B269). Decided by: the same morph-layer specification.
- Temperature. The lab's resolver has one; it is not in any ruling here, so the pad does not show it.

**Sources** `docs/design/morph-editor-lab.html`: B269 header :81-103; the pad's painters :2150-2331 (`paintRegions` :2191, `paintRamp` :2233); the lock law :1350-1449 (`qDrag` :1437); boundary hit-testing :3949-3961; pad gestures :3973-4019. ADR-266 r9, r11; ADR-261 (the UI never computes ownership).

The preview draws SAMPLE data generated in the preview (deterministic), never the engine.
