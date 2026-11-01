# Compact wires

The patch-bay view of the matrix, dense: the sources and destinations in use, each mapping a wire whose width is its depth and whose brightness is its source now (ADR-266 ruling 1, "wires in a compact form"). **W-a** (ADR-341 r5, ADR-340 r10's ML1) makes it a control: the nodes show their values, every wire carries a ◇ valve at its middle, and wires can be dragged.

**The consumer provides** a `Horde.Session` with sources, mappings and destinations in `S.params`: `Horde.CompactWires(el, S, { keys?, sources?, drag?, onWire?, onDepthRoute? })`. `keys` fixes the destination order (and lists destinations with no wire yet, so a wire can be dragged to them); otherwise every mapped destination in `S.params` order. `sources` lists source ids shown even with no route (a first wire has to start somewhere). `drag: false` turns the dragging off. `onWire(srcId, key)` and `onDepthRoute(srcId, mapping)` are the host's seams, below. Put it inside the element a `ScreenScheme` targets.

**Looks** (a screen: `scr-` roles only, through `Viz.kit`; so no source hues inside it: a source is told by its number and name)
- Source nodes on the left: number and name in `scr-grid` (ellipsis), a live meter in `scr-physics` along the node's foot.
- Destination nodes on the right: the name, a bar with the reach (`scr-physics` at 55%), where the mappings start (`scr-value`, or `scr-physics` under a Control) and the value now (`scr-meter`; `scr-marker` while clamped). The selected destination's edge is 2.2 px.
- A wire: a Bézier in `scr-physics`, 1 + 6 × depth px wide, its alpha 25% + 70% × the source's value now; INV is dashed; BI, INV and CTL are tagged near the destination (UNI untagged). Wires into one destination fan out 5 px apart.
- **CTL**: the wire's width is its swept range, it ends in a 3 px bar at the node (it sets, it does not add), the node's edge is dashed and its name reads "CTL <name>".
- A depth route: a thinner dotted wire landing on the ◇ valve at its target wire's middle.

**W-a, the wires as a control** (composition lab round 3)
- **Values:** a source node shows its level (`scr-value`, right), a destination its value now (`scr-value`, right of its name), both formatted by the parameter's own `fmt`.
- **◇ valve at every wire's middle**, 5.5 px, over the wires: it fills to the wire's depth (a Control's range for CTL); on a wire whose depth is moved by depth routes it fills to where the moved depth sits in its reach (the lab's drawing, kept). A depth route is still the dotted wire into the valve.
- **A wire is dragged from a source's port** (the ring on the node's right edge; the whole node is the handle) **to a destination** (a new wire beside any already there: **several sources on one destination stay separate wires**, fanned 5 px apart) **or onto a valve** (a depth route, B70). While a drag is in flight the rubber wire is dashed in `scr-value`, every valve grows to 7 px, the nearest valve within 16 px takes `scr-value` fill (8.5 px) and a destination under the pointer gets a 2.4 px `scr-value` edge. A drop on nothing makes nothing and says so.
- **The seam to the engine is `onDepthRoute(srcId, mapping)`.** The engine has no depth routes yet (B70, the lead's): a drop on a valve **only emits** the callback; the master fakes no engine. With no callback it falls back to the matrix's own placeholder (`S.depthRoutes`, below) so a preview shows something. A drop on a Control's valve is refused ("a Control has a range, not a depth"). `onWire(srcId, key)` is the same for a new wire; without it a mapping is added to the Session at 25%, UNI, as a click on an empty grid cell does.
- **No keyboard route** for dragging a wire: the grid (ML4) is the keyboard route to the same routes, as ADR-266 rules (the grid plus the wires).

**Gestures** Click a destination node: selects it (`S.selected`). The nodes carry transparent 28 px buttons over the canvas for Tab and the focus ring; each is named with its wire count, Control and value now.

**Fits** Height is rows × 30 px for the larger side (structure only, never live); node width 58–118 px; 320–1000 px.

**Ambiguity, read as the lab drew it:** ADR-341 r5 says the valve "shows its depth"; the lab's W-a showed the valve only where a depth route had landed. The master draws one on every wire (filled to the depth) and keeps the lab's reach-fill on a routed wire, so the same glyph means "the wire's depth" in both cases.

**Reduced motion** one still frame, redrawn only on change.

**Placeholders**
- Which polarities carry a tag (all but UNI) and the CTL bar end are assumed; the lab tagged only non-default polarities and had no Control.
- Depth routes read `S.depthRoutes` (see the grid's placeholder).

**Sources** docs/design/mod-matrix-lab.html: `paintWires` :2064-2170, the view's note :618-623.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
