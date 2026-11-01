# Page view (PV1) and Param table

ADR-340 ruling 6: **P3 is a view.** Every page gets a **Grid | Table** switch in its header with the find box, and remembers its view.

`Horde.PageView(el, S, { id, label, title, left, grid, table, view, onView, keys })`
- `grid`, `table`: an element, a builder `function (host, S)`, or, for the table, `{ groups }` for `Horde.ParamTable`. `left`: an element (pictures, the Lens) that stays put in both views. With no `table` there is no switch.
- **Remembered per page**: `{ view, find }` under `hd.pageview.<id>` in `localStorage`, in try/catch with a session-memory fallback.
- The find box (`type=search`) matches name, key, abbreviation and path. In the grid it **dims** what does not match (`opacity-muted`, never hides: the page keeps its shape) and outlines the matches; in the table it filters the rows and hides a section head with no row left. The count reads "n matches" or "n controls". **FindPane** (the morph grid's) is bound to MorphGrid and does not fit, so this has its own box.
- Returned `{ view(v?), find(q?), refresh(), el }`.

`Horde.ParamTable(el, S, { groups: [{ title, controls: [{ key, name, abbr, kind, props, advanced }] }] })`: the table itself. Each control is a 17 px row in CSS columns grouped by section: the owner glyph (the editing corner's, in its `-tx` ink), the FULL name (never an abbreviation), the control, the mappings as dots (up to three, source colours). `kind`: `knob` (a TS2a value cell, 76 px, dragging 1:1 like a slider), `stepper` (the flat K4 form), `toggle` (the dense switch), `select` (a native select drawn as a dotted caret). Advanced rows draw their name in `hint`.

**Rules** Table rows are 17 px and cells 16 px: the named exception to `size-hit` of ADR-338 ruling 3, on the cross axis only. Map mode works as in the grid: a click on a value cell maps, a plain drag moves the value, Option-drag sets depth (MD1), because the cell is the same slider.

**Readings taken** The table's toggles and selects use the house dense faces; TC2's drag number and caret select are 2a's companions and replace them without changing the descriptor. A control built in both views is one parameter with two faces (`Horde.ctlFace`): same corner values, same registry entry. A stepper holds its own value per view unless the consumer binds `onInput` to a model (the preview does not).

**Sources** ADR-340 ruling 6, composition-lab-r2.html PV1, composition-lab.html P3.
