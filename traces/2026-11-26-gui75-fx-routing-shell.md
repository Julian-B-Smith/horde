# 2026-11-26 — FX routing on the shell's FX page (GUI-75 A1, GB41)

**What changed.**
- **The shell's FX page** mounts `FxRouting` in place of `FxPatch`.
  - **Opening patch:** GUI-41's house default. Six inputs of their own (MOCK) feed the first module, the modules run in order, and the last feeds Out, at 100 % at every corner.
  - Prevent feedback is off until GN-62's Settings value is drawn.
  - The card's hint describes the new gestures.
- **Randomize's FX routing level** draws and copies through `FxWires.drawCorner` and `copyCorner`. They are new in the master, carried over from `FxGraph.draw` (forward only, every wire at 100 %), and write through the one writer.
- **The master (`1795726426-0a41`), published and re-vendored**, has two layout changes found on the shell's default patch:
  - **A box's edge splits in proportion** to the wires arriving and leaving there. It was halves, so six inputs into Echo sat 2 px apart.
  - **Arrowheads shrink to the room beside their port**, with a new instrument, `FxWires.heads`.
- **`fx_routing_check`** measures the crowded patch as well: the shell's default, then seeded Randomize draws at B to D. That is 112 views, plus an arrowhead control and the draw and copy laws.
- **`gui3_browser_check`:**
  - Its GUI-41 line (loop refusal, FxGraph) is replaced by a GUI-75 line: the ladder is tidy, including the 3 px gap and no arrowhead over a neighbour.
  - The line also tests the side-by-side layout, six inputs, a matrix drag at 80 % with the chip agreeing, a drag to 0 removing the wire at A only, a double-click at 100 %, Heard refusing a drag (control: at A it moves), and a box drag making a wire (control: a drop on nothing makes none).
  - The box drag lets hit tests through GUI-54's splash for those synchronous lines only, so its life is untouched.
- **Records:**
  - GUI-75 A1;
  - GB41's status;
  - the ledger: `gui75-fx-routing-ladder` is in-shell; `gui41-fx-patching` is superseded by GUI-75 and leaves the shell (vendored); `gui36-fx-routing` leaves the shell; the three screen and cable-colour rows now reach the shell through `H.FxRouting(host,`;
  - `master_versions` and the arp-face provenance row.

**Why.** The human, 2026-11-26: "Yes, swap it".

**Evidence.**
- **`fx_routing_check`:** GREEN, 112 views. Ports are at least 3.07 px apart, with 0 arrowheads over a neighbour and 8 controls fired.
- **Plants on the vendored copy**, each red with its own line, then restored:
  - the old halves split: "crowded A at 333 px: two ports 1.58 px apart", and 5 arrowheads covering a neighbour;
  - full-size arrowheads: "6 arrowhead(s) cover a neighbouring head or cord".
- **The shell's FX routing line:** OK at 1044 px. The ladder is tidy, with ports 3.6 px apart at the closest and 0 arrowheads over a neighbour.
  - **Plant:** a level drag ten times stiffer (150 → 1500 px). The line went FAILED on "a matrix drag sets 80 %" and "a drag to 0 removes the wire at A only". Restored byte for byte, and the chain check is green.
- **`design_chain_check`:** green at 199 files. **`engine_needs_check`:** ok.
- **A headless render** of the shell's FX page at 1044 px: the ladder and the matrix side by side in a 933 px card (ladder 550 px, matrix 371 px), with six separate arrowheads on Echo.

**State (GUI-27).** Unchanged. The routing lives in the page: not in a patch, a snapshot or undo, as FxPatch's was. GN-61 brings it into the engine.

**Engine and FX device.** Engine: GN-61, GN-62. FX device: the same.

**Open.**
- The Mix picture (GUI-14 r5).
- B715 replaces the MOCK.
- GN-62's Settings toggle.
- The pick switch and the dead-end guard wait on B714.
- The readings in GUI-75 A1 are the human's to confirm.
- **Verify:** `./verify fast`, exit code in the PR.
