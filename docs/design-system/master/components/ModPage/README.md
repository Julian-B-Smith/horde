# Mod page (M5)

ADR-338 ruling 8: **list + detail**. Every modulator a thin row, grouped by kind with a + per kind; the selected one fills the rest of the page at the **full** tier. ADR-266's matrix grid stays the page's route view.

`Horde.ModPage(el, S, { mods, kinds, select, onAdd(kind), onRemove(m, routes), onMap, onDrop, onSelect, routes(host, S), matrix, editor(m, host), keys })`
- `mods`: an array or a function (the active modulators). `kinds`: `[{ id, label }]` (default: the mods' `group`). `editor`: mounts the bundle's own editor into the detail (`EnvelopeEditor`, `LfoShapeEditor`, `ShClockCard`, `OrbitalCard`); the consumer's frame loop draws it.
- The right side has a **Detail | Routes** switch: Routes mounts `Horde.ModMatrixGrid(host, S, matrix)` (or your `routes(host, S)`): the grid, unchanged, on the same routes.
- Returned `{ select(id), get(), mode('detail' | 'routes'), refresh(), el }`.

**Layout** a 236 px list (scrolls) and the detail beside it; the page's body is 545 px tall with the bank hidden (958 x 545). The list scales to every source the engine publishes without scrolling the detail away. The list rows are thin rows (see Modulator tiers); a row's name selects it.

**Placeholders** the route views ML1 (interactive compact wires) and ML3 (a list by source) of ADR-340 ruling 10 are 2a's; the detail's own route list shows the target, polarity text and depth, with x per route.

**Sources** ADR-338 ruling 8, ADR-340 ruling 10 (ML4 the grid), composition-lab.html M5.
