# Mod matrix grid

Every source against every destination at once: a cell's fill is the depth you set, the bar on its floor is what it pushes now, and an empty cell is one click from a mapping (ADR-266 ruling 1).

**The consumer provides** a `Horde.Session` holding the sources (`{ id, name, kind, f }`) and mappings, each destination registered in `S.params` (`{ key, name, vals[4], def, fmt }`, as Knob and Slider register them), and the column list: `Horde.ModMatrixGrid(el, S, { dests: [{ key, name, short }], hosted: [{ name, macros: [{ id, name, f, targets }] }] })`. `short` is the column head (5 characters or fewer reads best); `name` is its tooltip and accessible name. `hosted` is optional: a hosted module's macros, kept OUT of `S.sources` (they cannot be mapped). The grid starts no frame loop of its own; it renders in `S.start()`'s.

**Anatomy** (panel UI, not a screen: the lab's well)
- A `well` table with an ink edge (`stroke-ink`, `radius`). Row heads: the source's kind dot (`src-env`, `src-lfo`, `src-macro`, `src-engine`), its name and number (mono 9.5 px, ellipsis). Column heads: 28 px buttons, mono 9 px `t2`; the selected destination is `sel-neutral` with its edge, and its column takes `sel-neutral`.
- A cell is a 32 px hit (at least 30 px wide) holding a 20 px box and a 4 px live lane:
  - empty: a `well-alt` box;
  - UNI, BI: the box takes `well` with an ink edge and fills from the bottom to the authored depth in the source's kind colour; INV hatches the same fill;
  - the lane is the push now, signed from the centre (a `t2` centre tick, the bar in `ink`): right adds, left subtracts;
  - **CTL**: the box shows the swept band (low to high) in the source colour and the position now as a 2 px ink line; the lane reads the position from 0. The column head wears a 2.5 px underline in the controlling source's colour and its accessible name says "controlled by <source>";
  - depth moved by a depth route: a dashed 1.5 px ring in that route's source colour.
- A source row is lit (`sel-neutral`) while it is the session's current source (`S.map.src`, the strip's and the MAP control's).
- Hosted macro rows: a lock, the module and macro name in `t2`, hatched read-only boxes (`div` edge), a not-allowed cursor.
- A caption line under the table (mono 9 px `t2`, one line, ellipsis): the hovered or focused cell ("LFO 1 → Cutoff · BI 30% · now +7%"), else the selected destination's summary.

**Gestures**
- Click an empty cell: maps that source at 25%, UNI (the Map Mode spec's default). Drag from an empty cell: the mapping is made at 0 and the drag sets its depth; released at 0, nothing is made.
- Drag a mapped cell up or down: its depth, relative, 160 px (`size-drag`) the full range, Shift ×0.25. A cell with per-corner depths (`cd`) edits the editing corner's, as the knob does. A CTL cell's drag slides its band, its width kept.
- Click a mapped cell or a column head: selects that destination (`S.selected`) and makes the mapping the last touched (`S.lastMap`), so the ledger, the lanes and the P key follow it.
- Double-click a mapped cell: resets it (25%; a Control to the full sweep).
- Keyboard mode only (ADR-259 ruling 6), on a focused cell: ↑/↓ depth ±1% (Shift ±0.25%), P cycles polarity, Backspace or Delete removes. Mouse routes: the drag, the route row's pill and its remove button.
- A hosted macro's cell never maps: a click says why ("MAW's macros are mapped inside MAW…").
- Each gesture ends in one `S.emit()`: one change, one undo step for the host.

**Fits** 320–1000 px. Columns are 30–64 px, so six destinations fit a 320 px column; a wider matrix scrolls inside its labelled region (`role="region"`, a Tab stop with the focus ring), never the page. Nothing live changes a cell's size: the fill, band and bar are drawn inside fixed boxes.

**Reduced motion** one still frame, repainted only when a mapping, the selection, the source or the cursor changes.

**Placeholders**
- Depth routes (the lab's B70 answer: a route whose destination is another mapping's depth, one level): read from `S.depthRoutes`, a placeholder field. The ring and the moved depth are drawn; whether depth routes ship, and their shape, is decided by the human's ruling on the lab's depth-modulation question (B207).
- The CTL cell's drag slides the band with its width kept, and a double-click resets a Control to 0–100%. Assumed; the Map Mode spec and ADR-266 do not say how a Control's range is dragged in a matrix cell.
- Silent corners (ADR-266 ruling 4): a mapping with an optional `corner` (0–3) whose corner weighs 0 draws its fill in `dis`. Mappings are global for 1.0, so nothing sets `corner` yet; corner-level mappings, when they arrive, decide its shape and arithmetic.

**Sources** docs/design/mod-matrix-lab.html: the grid's well styles :352-383, `buildGrid` :2173, `wireCell` :2204, `paintGrid` :2233. Colours by kind replace the lab's magenta and violet (ADR-261 A1); depth is a magnitude and the sign is the polarity's (ADR-266 ruling 2).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
