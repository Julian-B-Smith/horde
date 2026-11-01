# Routes by source

ML3 (ADR-340 r10): a list **grouped by source**, answering "what does this modulator do?". It is the route view for the Mod page's detail pane (M5): the selected modulator's group fills the pane. Source: `docs/design/composition-lab-r2.html` (ML3).

**The consumer provides** a `Horde.Session`; `Horde.RoutesBySource(el, S, { sources?, param?, empty?, label? })` returns `{ el, render }`. `sources` limits it to some sources (one id is the detail pane's group); `param` to one destination; `empty: true` also shows sources with no route, with a hint.

**Anatomy**
- A group per source, split by a dashed `div` hairline. Its head: the source's dot (`src-` by kind), the name (Archivo 800 9 px, caps), a live meter (56 px), the route count in `hint` and a Map button (turns MAP on with that source).
- A row per route: `Horde.RouteRow`, the same row the grid's list uses (the polarity chip MP1, the target, the depth track, authored to now, +◇, ×), minus its source column: the group's head is the source. Then a row per **depth route** the source drives ("↳ depth of LFO 1 → Cutoff"), under the source that drives it.
- Below 480 px of container the row wraps to two lines, as the list's.

**Behaviour** it is one more view of the one route list: an edit in any route view (the grid, the list, the wires) shows in all, and the groups rebuild when the mappings change.

**Placeholders** depth routes read the matrix's placeholder list (`S.depthRoutes`, the matrix README); the order of the groups is the Session's source order.

The preview shows SAMPLE sources and mappings, never the engine.
