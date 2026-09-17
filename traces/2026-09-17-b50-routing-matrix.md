# b50-routing-matrix — the ADR-088 crosspoint matrix becomes parameters and a pane

- **Queue item:** B50, phase 1 (ROADMAP row B50, "PHASE 1 DISPATCHED 2026-09-17")
- **Why:** the matrix has run in the audio path at identity since B23 with no
  parameter pointing at it and no UI — the human's build-order ruling is that
  the routing INTERFACE lands before the module rebuild, so modules arrive into
  a visible graph rather than the graph arriving after them. Phase 1 is the
  parameter surface, the persistence, the morph membership and the pane; no
  feedback cells, no new modules.

## What changed

- `src/hypersaw_clap.cpp` — the ADR-088 routing block: a positional id layout
  (`coeff` 10000 + from*64 + to, `outAmount` 20000 + to, `slotInit` 21000 + to)
  written down once beside the table; a routing ParamDef table built at load
  time with defaults READ OFF a default-constructed `RoutingMatrix` (whose ctor
  is `setSerialChain`); dispatch branches in `findParam`, `paramClassOf`,
  `applyParam`, `readParam`, `params_count`, `params_get_info`; the sparse
  `routing` state chunk; routing ids in `paramsJson`/`defaultsJson`; the morph
  field append; five debug exports.
- `tools/routing_check.cpp` — assertions 8-13 over the SHELL (round-trip to the
  live matrix, the dispatch probe, default-is-series, a two-corner morph blend,
  the state chunk, reachability), each with must-fail controls, plus the
  four-plant calibration record.
- `CMakeLists.txt` — `routing_check` links the impl library (assertions 8-12
  need the shell). One line; flagged below as outside the brief's file list.
- `src/gui/gui2.html` — the MATRIX pane on the FX page (hand-placed cluster,
  cells derived from the shell's routing ids), a `.mxc` branch in
  `paintControl`, the mod-matrix menu item widened to accept a cell, and a
  clearly-labelled dev stub so the pane renders with no plugin behind it.

## Evidence consulted

- `src/routing_core.h` header comment in full (the design), `edgeLive` /
  `edgeForward` / `isTerminal` / `processBlock`.
- ROADMAP row B50 phase-1 acceptance (read verbatim off the row).
- `tools/gui_reach.py`, `tools/presentation_check.py`, `tools/test_table_check.py`
  — all three derive from the `kParams[] = {` block, which is why the routing
  table is deliberately NOT in it (see "Alternatives rejected").
- `tools/paramscope_check.cpp` for the host-rig idiom; `tools/state_check.cpp`
  for the string-backed CLAP streams; `tools/gen_factory_bank.cpp` header.
- L0023 (a range without a control is invisible), L0026 (TDZ / load order),
  L0028 (role vs instance — `data-fixed`), L0031 (an oracle spans only its own
  surface: the core assertions could not see the shell), L0032 (must-fail
  controls; the stale-object trap, which fired here), L0033 (a plant that does
  not fire is a coverage boundary, recorded).

## Alternatives rejected

- **Routing rows in `src/param_presentation.tsv`** (the brief suggested them).
  Rejected on evidence: `presentation_check.py` derives its address set from the
  `kParams[] = {` block and FAILS on "presentation row for a param the shell
  does not declare". Routing ids are positional, not address-keyed, so a row per
  crosspoint would either turn that gate red or force the routing table into
  `kParams` — where it would also change the frozen 243-row contract every other
  tool counts. Keeping the block in its own table leaves all three gates green
  with no edit at all; the tsv was not touched.
- **A `<input type=range>` hidden inside each cell** so the bulk `[data-p]`
  wiring paints and wires it for free. Rejected: its double-click means "reset
  to default" where B50 asks for "toggle 0/1", and the cell would carry two
  elements where one will do. The `.msbtn` precedent (a div with `data-p` and
  its own paint branch) already exists; this follows it.
- **Deriving the source count as `froms - slots` in the pane.** Written, then
  falsified by the DOM dump: the LAST slot never appears as a `from`, so the
  count was zero and the grid mislabelled the source row and shifted every
  terminal up a row.
- **Regenerating the factory bank** to clear `bank_check`, and **relaxing
  `morphlayout_check`'s T1** to clear the other red. Both rejected: out of
  scope, and both are the shape the instruction forbids — making a gate green by
  changing what it compares against. Reported instead.

## Verify

- `./verify full` — **EXIT 1**, and it is RED for one reason with two faces.
- **TWO gates fail, both caused by the single line**
  `for (const auto &d : g_routingTable.defs) morphIds.push_back(d.id);`
  — i.e. by acceptance (a)'s "in the field". Established by controlled
  experiment: remove that one line, change nothing else, rebuild, and both go
  green. Nothing else in the change set contributes.
  1. `bank_check: 41 failure(s)` — every factory patch fails "re-save is
     bit-identical to the file". The 40 patches store four corner arrays whose
     length IS `morphIds.size()` (`tools/gen_factory_bank.cpp` header: "four
     224-entry corner arrays in morphIds order"), so an 18-cell append makes
     every re-save 18 entries longer. Removing the line: 41 -> 0 failures.
     The sanctioned fix is `gen_factory_bank docs/presets/factory`, which is
     what the last two appends did (commit 06bb856, "bank regenerated").
  2. `morphlayout_check: FAIL` — one assertion, T1, which requires
     `live.size() == frozen.size() + 2` (exactly 224 morph slots). That clause
     pins the field's TOTAL LENGTH, which is stronger than the ADR-159 contract
     it is named for ("frozen 222-entry PREFIX, then 181/1181 last") — the
     prefix claim survives an append, the length claim cannot. Removing the
     line: T1 ok, gate PASS. Its other 18 assertions pass either way.
- **`bank_check`'s `|| return 1` ABORTS `full()`**, so the 18 gates after it
  never ran in the `./verify full` log. They were run individually against the
  same tree and are all exit 0, `morphlayout_check` excepted — including
  `routing_check` GREEN (15 assertions), `rtsafety_probe` GREEN (audio thread
  allocation-free, which is what the load-time table build bought),
  `paramscope_check` GREEN (its default-truth and round-trip sweeps enumerate
  `params_count`, so they now cover all 18 routing ids for free).
- `parity_check: 156/156 within eps=1e-06`; `state_check`, `statefix_check`,
  `undo_check`, `trajectory_check`, `presetstore_check` all GREEN.
- `node tools/labharness/lab_load_check.mjs`: GREEN — 42 labs, 0 broken.
- No golden regenerated; no fixture changed.

## Open questions

- **For the lead / human, and it is one decision, not two:** acceptance (a)
  says the crosspoints are "in the field". Honouring it necessarily lengthens
  the morph array, and two gates assert that length — one against 40 shipped
  files, one against a literal. The options are (i) regenerate the bank AND
  correct T1's length clause to a prefix clause, or (ii) keep routing out of the
  morph field and amend (a). This subagent took neither; both are outside the
  brief and (ii) would also invalidate routing_check's assertion 11.
- **ROADMAP contradiction, not resolved here.** B50 (a) describes `edgeLive` as
  "sources → any slot, slot → strictly later slot" and (f) says "edgeLive stays
  strictly acyclic". Neither is true of the tree: ADR-128 widened `edgeLive()`
  to `return true` on 2026-08-27 and `routing_core.h` already carries the
  per-sample `zPrev` loop. Phase 1 honours (f)'s INTENT — no feedback cell is
  exposed — by gating exposure on `edgeLive && edgeForward`, both predicates
  taken from `routing_core.h` so the shell never owns a second copy of "which
  edges are live". The ROADMAP text needs the correction; only the lead writes it.
- **`CMakeLists.txt` is outside the brief's file list.** One line, linking the
  impl library into `routing_check`, without which acceptance (e)'s round-trip
  and morph assertions cannot be written at all. Flagged rather than assumed.
- **`processBlock` does not implement feedback.** `process()` (scalar) reads
  `zPrev` for a cycle edge; `processBlock` (the one the shell actually calls)
  reads `slotL[f - NSRC]` unconditionally, so a backwards edge there would read
  a partially-gathered buffer. Unreachable today — phase 1 exposes no feedback
  cell — but it is a live divergence between the two passes and it is where
  ADR-128's loop has to land. Not touched: out of scope, and routing_core.h's
  seven invariants were to stay untouched.
- Unverified: the pane has not been seen in a real browser or a host. Its DOM
  was dumped by running the shipped builder source against a minimal shim; the
  canvas path (`drawMatrixGraph`) is exercised by `lab_load_check` only as
  "does not throw", never as pixels.
