# design-master-batch-2 — ADR-338, ADR-340 and ADR-341's remaining rulings into the design master, published and re-vendored

- **Queue items:** B589 (the design-system round), B595 (the chain), B584 (the shell takes it next). GUI session. The human said "Start master batch 2" (2026-11-01).
- **Why:** rounds 1 to 3 of the composition lab were ruled (ADR-338, ADR-340, ADR-341). Batch 1 (1793502898-ea3e) carried the control layer; everything else waited in the master queue, so none of it could reach the shell (ADR-316).
- **How it was built:** two Sonnet builders worked in parallel worktrees branched from the recovery branch (#1315). The halves and their own traces:
  - 2a, controls and routes: `traces/2026-11-01-master-batch-2a-controls-routes.md`;
  - 2b, page forms and the bank: `traces/2026-11-01-design-master-batch-2b.md`.

  The lead merged the halves (the conflicts were both-sides additions in bundle.js, bundle.css and the master README, all kept; the `@ds-bundle` header was checked to be the union of the two), checked them in a browser, published, re-vendored and updated the ledger.
- **Evidence consulted:** DECISIONS.md ADR-338, ADR-340, ADR-341; the three composition labs; both builders' reports; `tools/design_vendor.py` and `tools/design_chain_check.py`; `tools/labharness/gui3_seam_check.mjs`.
- **What changed (lead, beyond the two halves):**
  - **MacroRow:** the bend's final snap (and the reduced-motion snap) now reports `spring`, not `mouse`. Found by dragging the bend row in the browser; the host must never read a release as the hand.
  - **MP3 is limited to mappings that existed before the drag.** The first publish (1793516789-9f79) let a NEW mapping's Option-drag go below zero and come out INV, so a drag released at 0 made an INV route. That broke the Map Mode spec's rule, and `gui3_seam_check` mapdrag went red. The lab's own words are "⌥drag a UNI mapping", so the master now matches them. No gate was edited. The reading is in the master README and the ledger.
  - **Published** 1793516789-9f79, then 1793516938-100b with the fix (both from this branch before merge, cdf8772's precedent; the live version was first checked to be 1793502898-ea3e). VERSION is recorded at 100b, and `docs/design/arp-face/` is re-vendored with its provenance row.
  - **The ledger:** 19 rows moved to vendored at 100b. adr340-steppers and adr340-hover-h1-h4 were re-stamped. adr340-polarity stays ruled: the right-click menu entry is not built. The round 1 and round 3 labs are harvested; round 2 stays ruled (polarity menu).
  - The ROADMAP B589 note.
  - A stray hook marker (`components/.harness/dirty`, gitignored, written by the stop-gate hook from that cwd) had entered VERSION's file list; it was removed and VERSION re-recorded at the same version id.
- **Not done, and why:**
  - The polarity entry in a right-click menu: the master has no control context menu.
  - RateSync's division stepper keeps its own handler: its off-list value can't be expressed on `H.Stepper` without breaking double-click home (Stepper README).
  - The shell has not mounted any of this yet (B584's next hop), and has not moved onto `S.escapeLayers` / `mapKeysAlways`.
  - The Scalpel waveform XY.
  - Not verified: whether Live or Bitwig keep ⌘⌥-click for themselves; the Windows Ctrl+Alt chord.
- **Readings taken (open for the human):**
  - DR-c line 1 is the editing corner's own value (placeholder, ADR-341 Open).
  - The bank's segment order is MAC | MODS, per ADR-340 r11's "Macros | Mods".
  - Whether a macro value may be a MAP destination is not ruled.
  - W-a draws a valve on every wire, filled to its depth (the lab drew it only where a depth route landed).
  - MP3 applies to existing mappings only.
- **Verify:**
  - All 78 master previews load with no page error in the browser; a planted throw is caught, so the zero is meaningful.
  - Driven by hand or script: the named stepper's dropdown, the bend's spring-back.
  - Light and dark screenshots of the level cell and the bank.
  - The GUI 3 shell's self-checks: 28 of 28 on the re-vendored bundle, no console errors.
  - `gui3_seam_check`: GREEN, 34 tests, 49 plants.
  - `design_chain_check`: GREEN, 165 files at 1793516938-100b.
  - `./verify fast`: see the PR.
