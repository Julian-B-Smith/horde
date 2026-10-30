# Trace lanes

The value of each routed destination over the last 3 s, against the band its mappings can reach: the one view that shows motion honestly (ADR-266 ruling 1).

**The consumer provides** a `Horde.Session` with sources, mappings and the destinations in `S.params`: `Horde.TraceLanes(el, S, { keys?, span?, laneH? })`. Without `keys` there is one lane per destination that has a mapping, in `S.params` order; `span` is the window in seconds (3), `laneH` the lane height in px (46). Put it inside the element a `ScreenScheme` targets to follow the scheme.

**Looks** (a screen: the six `scr-` roles only, through `Viz.kit`; glow is a wider underlay, never a blur; scanlines at the scheme's strength; no red)
- Each lane: the reach band (`scr-physics` at 26%) at the depths in force at each instant, so a depth route shows as the band breathing; where the mappings start, dashed: the parameter's own value in `scr-value`, or under a Control its position in `scr-physics`, with the Control's low and high ends as dotted `scr-grid` lines; the value now in `scr-meter`, 1.6 px over a 5 px underlay at 22%. A clamped instant marks the lane's top edge in `scr-marker`.
- The lane's name and value now top left, its scale top right, in `scr-grid` at full alpha (glass text). A Control adds "· CTL <source>". Lanes are split by `scr-grid` hairlines; the selected one takes a 10% `scr-grid` wash.
- Each lane autoscales to what moved in view (at least a 10% span, 8% padding) and prints that scale, so the zoom is never silent.

**Gestures** Click a lane: selects that destination (`S.selected`). Each lane is a transparent 28 px-or-taller button over the canvas, so Tab reaches it and the focus ring shows; its accessible name is the lane's name and value now.

**Fits** The canvas fills its column at the display's pixel ratio (capped at 2); its height is lanes × `laneH`, set by the structure, never by a live value. 320–1000 px.

**Reduced motion** one still frame of the whole window, recomputed only when a mapping, a depth, the selection, the scheme or the size changes.

**Placeholders**
- The first frame (and every reduced-motion frame) is the window as it would read had today's mappings always held: the history is evaluated from the sources at past times. A plugin reads a real history from the GUI readout; what it publishes is the readout contract's question.
- Depth routes widen the band through `S.depthRoutes` (see the grid's placeholder).
- The lab's 250 ms view, one step per control tick, is not built: the tick belongs to the engine, and the design system has no tick stand-in.

**Sources** docs/design/mod-matrix-lab.html: `paintTrace` :2253-2319, the view's note :631-637.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
