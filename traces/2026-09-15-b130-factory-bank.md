# b130-factory-bank — 40 categorised factory presets, a generator that writes them, and bank_check

- **Queue item:** B130 (the content half of B102; B129 owns the store path and
  the install, disjoint files). Brief:
  `briefs/2026-09-15-b130-factory-bank.md` on `lead-records-11`.

- **Why:** B102's "30–50 categorised factory presets built to demonstrate the
  coupling laws" is content, and content with no oracle rots silently — a patch
  that stopped locking would look exactly like a patch that still did. The
  decision behind the shape of this change set is that the bank is GENERATED,
  not hand-written: a factory patch is a derived blob (325 param keys, four
  224-entry corner arrays in morphIds order, the B100 header, the ADR-159 layout
  stamp), and ADR-159 is the record of what a derived order costs when it is
  promised in a comment instead of produced by code. `tools/gen_factory_bank.cpp`
  holds the only hand-authored data — a table of {param id → value} plus corner
  and teaching lines — and every shipped byte comes out of the shell's own
  writers through the host parameter path. `tools/bank_check.cpp` deliberately
  reads the SHIPPED FILES and never the generator's table: a check that consulted
  the table would certify the table.

- **Evidence consulted:** ROADMAP B102/B103/B129/B130 (read on
  `origin/lead-records-11`); ADR-152 (macro suspension + capture flatten),
  ADR-159 (frozen morph array layout, `morphLayout 2`), ADR-162, B122
  (`cornerNames` in the morph chunk); `src/hypersaw_clap.cpp` — `kParams`
  (142–676), `morphInit` (1651, the line that makes a fresh instance's four
  corners hold exactly the per-slot defaults), `cornerJson`/`cornerApply`/
  `cornerMatches` (3123–3222), `stateJson` (3448) and its B100 header comment
  ("`build` is provenance only, written, never read back"), `publishViz` (2685)
  and the `v.R = s->R` / `v.RA` / `v.RB` assignments; `src/swarm_core.h`
  `focus()` (671), `gravityStep` (691), the topo-0/1 branches that zero RA/RB
  (1723, 1741); `src/morph_core.h::weights` (59) for the corner→(x,y) map;
  `tools/statefix_check.cpp` + `statefix_common.h` (the bit-identical re-save
  idiom and its calibration), `tools/preset_probe.cpp` and
  `tools/notefuzz_scaffold.inc` (the apply → process → read rig),
  `tools/morphlayout_check.cpp`.

- **What changed:**
  1. `src/hypersaw_clap.cpp` — ONE new export, `hypersaw_debug_viz(p, osc, &R,
     &RA, &RB, &n)`, reading `cores[osc].focus()` directly (the same fields
     `publishViz` copies) rather than the published `VizSnapshot`, because the
     snapshot follows `vizOsc` and a probe that had to write `vizOsc` to name an
     oscillator would be mutating GUI state to measure. No other shell change,
     no GUI change, no RT-path change.
  2. `tools/gen_factory_bank.cpp` (new) — the bank as code; writes
     `docs/presets/factory/**`. Corner presets are authored WITHOUT a second
     debug export: a fresh instance's corner 0 read through the existing
     `hypersaw_debug_cornervals` IS the shell's morphIds order paired with the
     shell's defaults, so the generator overrides only the ids a corner names
     and prints with the shell's own `%.6g`.
  3. `docs/presets/factory/**` (new) — 40 patches in 8 categories, 4 corner
     presets, `BANK.md` generated from the same table so a teaching line cannot
     drift from the patch it describes (B103).
  4. `tools/bank_check.cpp` (new) — standalone, unwired.
  5. `CMakeLists.txt` — two targets, unwired, the established pattern.

- **Alternatives rejected:**
  - *A second debug export for `cornerJson(k)`* (the brief offers it as an
    option). Rejected: acceptance (5) says one export, and the corner ORDER is
    already reachable through `hypersaw_debug_cornervals`. Reduce, never invent.
  - *A shared `bank_common.h` between generator and check.* Rejected: the check
    must be independent of the table, and the rig it needs is already
    `notefuzz_scaffold.inc`.
  - *Comparing the whole blob including `build` on re-save.* Rejected: the key
    is written-never-read provenance, so including it would make the bank go red
    on the next commit for no musical reason. `dropBuild` strips exactly that
    key, and the comparison is CALIBRATED — a planted `masterVol` change must
    make it FAIL, asserted once per run.
  - *A K = 0 sixteen-voice cloud for quantum-morph corner A.* Rejected on
    MEASUREMENT: R 0.242 against the splay's 0.039, so the required ≥ 0.4
    pairwise separation is unreachable — R cannot tell a cloud from a splay,
    both are incoherent. Resolved by making corner A a THREE-voice cloud at
    K 0: an uncoupled swarm's order parameter is not 0 but its finite-size
    floor ~1/√n, and at n = 3 that floor (measured 0.527) sits between the
    splay's 0.039 and the lock's 0.965. Corner A stays a true K-0 cloud; the
    separation comes from physics, not from partial locking. See open questions.

- **Verify:** `./verify fast` exit 0 (`.harness/last-verify.json`:
  `{"target":"fast","exit":0,"git":"7d55e5e"}`), then `./verify full` exit 0 —
  output pasted verbatim in the PR body. `bank_check docs/presets/factory`:
  0 failures, 339 lines, also pasted verbatim in the PR.

- **Open questions:**
  1. **`hypersaw_debug_viz`'s signature cannot separate a cloud from a splay.**
     B130 fixes it at `(R, RA, RB, n)`, and R is degenerate between the two
     incoherent postures. The clean fix is a fifth observable — `RN`, the n-th
     order parameter, which is 1 for an even lattice and ~1/√n for a cloud and
     is ALREADY in `VizSnapshot`. Widening the signature is the lead's call, so
     it was not done. With it, corner A could be a sixteen-voice cloud and the
     splay exemplar's "even lattice, gap 1/n" would be asserted directly instead
     of through its scalar consequence.
  2. **Thin margin.** quantum-morph's tightest pair is A-vs-B at 0.438 against
     the 0.4 gate (0.038 of headroom). Deterministic and reproducible, not
     flaky, but a future engine change will move it. Every number is printed.
  3. **Measured correction to B130's wording:** the equal-tempered fifth is
     1.96 c NARROW of 3/2, not sharp (control reads −1.960 c; closed form
     1200·log2(1.5) = 701.955 vs 700). Same magnitude, opposite sign; the
     assertion is on |error|, and the teaching line says "tempered".
  4. `bank_check` is built but NOT wired into `./verify` — the standing ruling
     that wiring a gate is the human's decision. Proposed in the PR.
  5. Every patch carries `"build":"7d55e5e+"` as provenance of the capture. It
     is stripped for the re-save comparison, so it never gates; it will only
     change when the bank is regenerated.
