# 2026-09-14 — History round 3 (B119): rail graph + spine-protected eviction (ADR-160 A2)

**Rulings (human).** Rail graph for the HISTORY page; eviction protects the
spine.

**Core.** `src/undo_tree.h`: `push` takes a free slot if any, else
`victim()` = the oldest live node not on the spine (`onSpine` walks from
`current` to its root); the spine's root only when nothing else is left.
The write cursor is gone (victims are not slot-ordered).

**Check.** `tools/undo_check.cpp` (wired into `./verify full`): the
oldest-first legs replaced — off-spine fork evicted before the oldest root;
the human's scenario (189 trunk + 31 branch edits off edit 6) with an
oldest-first control; a recycled-slot parent invariant. Gate edit under the
human's ruling of 2026-09-14.

**GUI.** `src/gui/gui2.html`: `renderHistory` is the rail graph ported from
the layouts sheet (SVG lanes + rows, coalescing toggle); dropdowns and the
root picker are gone. Notes updated.

**Oracle.** `undo_check` GREEN; `./verify full` tail in the PR body.
