# History rail

The undo tree as a commit log (GUI-10; ADR-160, B119): one row per state, newest first, with a lane graph on the left.

`Horde.HistoryRail(el, { tree(), onRestore?(i), coalesce?, keys? })` returns `{ refresh(force?), setCoalesce(on), coalesce(), rows() }`. `tree()` returns `{ cur, nodes: [{ i, parent, tick, label }] }`. A node with no parent in the list is a root, so a forest after eviction draws as more than one root. `refresh()` repaints only if the tree changed.

**Reading it**
- The path to the current state is lane 0, in a bold `ink` line. Every other branch takes the next free lane, in a thin `t2` line.
- The current state's dot is larger, in `value`. A fork's dot is `marker`. On-path dots are `ink` and off-path dots `panel`.
- Rows ahead of where you stand, off its path, are the redo side, in `hint`.
- A click restores that state, and the next edit branches from it, so nothing you pass through is lost.

**Coalescing** folds a straight run of one label into one row with a count ("Width ×4"), and never across the current state, which must stay clickable on its own.

**Rows** are 26 px, and the graph's rows share that pitch. The row list doesn't scroll; the host's page does.

**Sources** GUI-10, `src/gui/gui2.html` (`renderHistory`), ADR-160, B118, B119.
