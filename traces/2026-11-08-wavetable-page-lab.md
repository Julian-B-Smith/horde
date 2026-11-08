# 2026-11-08 — The wavetable page lab, and its lead picture (GB37, GUI-62)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (new, exploring). Two questions, each shown inside a 980 × 720 horde window:
  - **Q1:** six ways to pick and see an oscillator's four corner tables: around a pad, one at a time, corners on the stack, morph terrain, twelve on the Morph page, and crossfade lanes.
  - **Q2:** five places for the 3 × 3 PM matrix: a row per Osc page with the grid on the Dashboard, its own Osc ▸ FM page, an operator graph, in the signal flow, and only on the Dashboard.
  - **Mock tables:** eight seeded generators, blended Linear, with no audio.
  - **Published** for the human as an Artifact.
- **Records:**
  - GUI-62 (the lead picture);
  - GN-57 (the read table's frames);
  - GB37;
  - the ledger's lab entry and the row `gui62-blended-stack`, with engine impact GN-57.

**Why.** The lead handed the wavetable page to this session (ADR-387, `docs/wavetable/INTEGRATION.md` §7), and the human said "Wavetable page".
- The human answered the lead picture: "The blended frame stack".
- They asked to audition the corner tables, "including any creative approaches we haven't yet considered".
- They asked to audition the matrix too: "Let's try a few options".
- And to work as a variant lab, then rule.

**Evidence.**
- **The lab's self-check, in headless Chrome:** every variant builds inside its window. The blend is corner A's table at A and D's at D (control: they differ), a corner's table change reaches the blended table, and the rolled table is seeded. No uncaught errors.
- **Screenshots** of all eleven variants were read.
- **Two layout bugs fixed** before publishing:
  - V1's row-spanning pad was auto-placed into column 1;
  - P4's paired wires and labels overlapped.

**Verify.** `./verify fast` on the commit this trace ships in.
