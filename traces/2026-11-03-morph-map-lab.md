# 2026-11-03 — The morph and MAP lab (GB5)

**What changed.** `docs/design/morph-map-lab.html`, a new lab: today's layout (T) beside three others on the real shell: A stacks the pad and Mappings in one column, B moves Mappings to the foot row, C swaps Mappings into the pad's column while MAP is on. Its ledger note and the lab index. The master, bundle and shell don't change, and it records no ruling.

**Why.** The human, 2026-11-03: "having morph and map active simultaneously currently breaks pretty much every page, and we're going to need to deal with that somehow." Then, asked whether to build a lab: "Yes, build the lab".

**Evidence consulted.** `docs/design/gui3.html` (`.body.with-dock.with-insp`, `paintDock`, `paintInspector`, `setSidebar`). The measures come from the lab's own self-check on all 15 pages, in the lab.

**A wrong first measure.** Counting only controls past the page's edge read 3 of 15 pages broken, against the human's "pretty much every page". The screenshots showed the human was right, so the measure now also counts controls hidden inside a box, cut-off text and how much taller a page gets, against the same page with nothing on. It reads 10 of 15.

**Not checked.** A plugin host, the keyboard, and the light theme by eye. The new console (#1362) isn't on this branch; its 906 px comes from that PR.

**Consequence for the FX device (ADR-254).** It has the same pad and the same MAP, so whichever is ruled applies there too.

**Verify.** See the PR body.
