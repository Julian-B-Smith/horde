# 2026-11-06 — Members at phase, the summed beam, the Osc pages scroll (GUI-49); the bank's MORPH lane ruled (GUI-50)

**What changed.**
- **The master** (published `1793939881-6f91`, then `1793941176-071d`):
  - `Viz.blade` draws a fourth view, `mphase`: Members' lanes, each shifted as At phase shifts it, with a tick at its cycle's start.
  - BladeView has four pills, kept on one row by `bundle.css`; the XY legend gives up its width instead.
  - The BladeView README is updated, and the bundle is re-vendored to arp-face with its provenance row and the ledger's `master_versions`.
- **The shell:**
  - The beam (`Viz.scope`) sits under the waveform on the full Osc pages.
  - Self-check 4b2d checks that the four views draw four different pictures and the beam draws. Its control is one view drawn twice with the swarm held still.
  - GUI-30's views check counts four views and six canvases.
  - The fit check holds Main, Sub and Noise to the page area and only measures the Osc pages.
- **Records:**
  - GUI-49, and GUI-50 with its six rulings.
  - GN-52, with GN-9 and GN-37 citing GUI-50.
  - GB25 ruled, GB28 added.
  - Ledger rows, and the bank morph lane lab ruled with its pin off.

**Why.** The human: "a scalpel view option that has members at phase", "add the summed waveform below the scalpel on the osc page, like in the scalpel lab", "Let the osc page scroll: we still need to add the advanced option sections below anyway". Also the bank morph lane: "rows and weights ... names menus ... save menu ... on the morph page ... an asterisk next to each relevant name", "Dimmed, with a switch", "Goes back to the tab you had".

**Evidence.**
- `gui3_browser_check` GREEN, 84 self-checks.
- **Plant:** with the vendored bundle drawing `mphase` as Members, 4b2d read FAILED ("draw 3 different pictures").
- **Before the fix:** the fourth pill wrapped Main's Osc tabs (+18 px); a screenshot showed it, and the master CSS fix keeps the pills on one row.
- **Measurements:** the beam added 78 px to the Osc pages (right column 475 against 397), and the left column had 29 px spare. That put the trade to the human.

**Verify.** `./verify fast` on the commit this trace ships in.
