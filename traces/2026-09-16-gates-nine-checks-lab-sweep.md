# Trace — 2026-09-16 gates: nine checks wired into verify full, lab loader sweeps reference/ (ADR-171)

**What changed.** `verify` full runs statefix, presetstore, bank, anchor,
penv, twocluster, morphlayout, fxxfade and polarity after undo_check (human:
"Gates ratified"). `tools/labharness/lab_load_check.mjs` sweeps `reference/`
and `reference/maw/` beside `docs/design` and `src/gui`, with five new
sandbox globals and a thenable `navigator.requestMIDIAccess` (B138, human:
"Please patch"). Rulings recorded: B133 rail wins (ADR-137 amendment), B126
test both regulators (dispatched, branch `orbital-regulators`), B117 lead's
recommendation (bury, crossfade default) awaiting the human.

**Evidence.**
- Each check run standalone before wiring, all green: presetstore 55 checks;
  anchor, penv, twocluster, morphlayout PASS; fxxfade 0 failures; polarity 0
  failures; statefix 3 fixtures 0 failures; bank 0 failures (~1 s).
- First sweep with `reference/` added: 42 OK, 1 FAIL — the MAW prototype on
  `navigator.requestMIDIAccess(...).then is not a function`, a checker gap
  (the generic stub has no `then` by design). After the navigator stub:
  `GREEN — 43 labs loaded, 0 broken, 1 skipped`.
- Control: `reference/gravity-modulator.html` copied to scratch with
  `USE_BEFORE_DECL()` injected before its `let nodes=[]` →
  `RED — 1 labs loaded, 1 broken` (the gate still bites).
- Premise correction: B138's row said the layout move emptied `docs/design`;
  it did not (older design labs remain there and were swept all along) —
  `reference/` was never in the list. Row corrected.

**Verify.** `./verify full` on this tree — tail in the PR body.
