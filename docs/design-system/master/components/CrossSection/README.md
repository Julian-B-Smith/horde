# Cross-section

One row along a line across the morph pad: the value heard at every point of the line, so a flip, a slide and a ramp read as a picture.

**The consumer provides** the session, the row (`opts.key`, else the selected row) and, to draw and drag the line on the pad, `opts.pad` (the `H.XYPad` element). The call returns `{ line(a, b), set(key) }`; `line()` aims the line through two handle points, and the plot runs along its chord from edge to edge.

**Anatomy**
- A glass (`hd-viz`, aspect 2.4:1) drawn only with the screen roles: the row's range edges dashed in `scr-grid` and labelled in the row's own units; the corners the line passes as their letters in `scr-physics` (inside a screen, A–D, ADR-259 r4), or the coordinates where the line meets the edge; the heard value as the beam (`scr-value`, an 8 px underlay at 22% under a 2.2 px core); a picking row as steps with vertical risers; under QUANTUM, for a continuous row, the BLEND it would otherwise play, dotted `scr-grid`; the puck's projection as a dashed `scr-marker` line when it sits within 0.03 of the line.
- On the pad: the line, `ink` dashed over a `panel` underlay, and two 28 px handles (‹ ›, a 15 px `panel` disc with an `ink` edge).
- A key under the glass in the same screen roles, in a fixed three-line box.

**Gestures** drag a handle (relative, shift ×0.25, kept inside the pad away from the corner chips); double-click a handle resets to A → D; arrows move a focused handle in keyboard mode. The handles never move the puck.

**Fits** the glass fills its column at a fixed aspect and draws at the display's pixel ratio (capped at 2); it repaints only when the row, the line, the mode, the puck or the theme changes.

**Placeholders**
- Waypoints. The lab's cross-section exists to compare four interpolation laws through pinned values (B235). Waypoints and their laws are not ruled, so this plot draws the heard value and the blend only. Decided by: a ruling on B235. The four-law minis are lab-only and are left out.

**Sources** `docs/design/morph-editor-lab.html`: the panel :629-635; `clipToSquare` and the default line :3136-3166; `paintSection` :3353-3475.

The preview draws SAMPLE data generated in the preview, never the engine.
