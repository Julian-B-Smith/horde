# Compact wires

The patch-bay view of the matrix, dense: only the sources and destinations in use, each mapping a wire whose width is its depth and whose brightness is its source now (ADR-266 ruling 1, "wires in a compact form").

**The consumer provides** a `Horde.Session` with sources, mappings and destinations in `S.params`: `Horde.CompactWires(el, S, { keys? })`. `keys` fixes the destination order; otherwise every mapped destination in `S.params` order. Put it inside the element a `ScreenScheme` targets.

**Looks** (a screen: `scr-` roles only, through `Viz.kit`; so no source hues inside it: a source is told by its number and name)
- Source nodes on the left: number and name in `scr-grid` (ellipsis), a live meter in `scr-physics` along the node's foot.
- Destination nodes on the right: the name, a bar with the reach (`scr-physics` at 55%), where the mappings start (`scr-value`, or `scr-physics` under a Control) and the value now (`scr-meter`; `scr-marker` while clamped). The selected destination's edge is 2.2 px.
- A wire: a Bézier in `scr-physics`, 1 + 6 × depth px wide, its alpha 25% + 70% × the source's value now; INV is dashed; BI, INV and CTL are tagged near the destination (UNI untagged). Wires into one destination fan out 5 px apart.
- **CTL**: the wire's width is its swept range, it ends in a 3 px bar at the node (it sets, it does not add), the node's edge is dashed and its name reads "CTL <name>".
- A depth route: a thinner dotted wire landing on a ◇ valve at its target wire's middle; the valve fills to where the moved depth sits in its reach.

**Gestures** Click a destination node: selects it (`S.selected`). The nodes carry transparent 28 px buttons over the canvas for Tab and the focus ring; each is named with its wire count, Control and value now.

**Fits** Height is rows × 30 px for the larger side (structure only, never live); node width 58–118 px; 320–1000 px.

**Reduced motion** one still frame, redrawn only on change.

**Placeholders**
- Which polarities carry a tag (all but UNI) and the CTL bar end are assumed; the lab tagged only non-default polarities and had no Control.
- Depth routes read `S.depthRoutes` (see the grid's placeholder).

**Sources** docs/design/mod-matrix-lab.html: `paintWires` :2064-2170, the view's note :618-623.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
