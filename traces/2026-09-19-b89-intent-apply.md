# b89-intent-apply — B89 phase 2c: the resolver APPLIES, and SPEC-INTENT-BUS §12 measured through the plugin

- **Queue item:** ROADMAP B89 (phase 2 paragraph), increment 2c of
  `docs/proposals/b89-phase2-intent-resolver.md`; ADR-176 + A1–A3, ADR-173,
  ADR-152. Builds on 2a (PR #629) and 2b (PR #654, branch `b89-intent-flag`).

- **Why:** 2b wired the resolver into the shell behind param 266 with its
  output SHADOWED, and proved the two facts 2c rests on — the flag off costs
  nothing (structurally: the branch is not taken), and the flag on with
  nothing bound is a pure corner read. 2c makes the value reach the engine.
  The decision that shaped the diff is 2b's second finding (S3e): a writer
  placed BESIDE morphStep is not harmlessly overwritten — the plant was
  audible from sample 0 — so the apply had to REPLACE morphStep's write, not
  join it. That is why the three pieces that do the writing were EXTRACTED
  from morphStep (`morphCommitSlot`, `morphApplyTarget`, `morphApplyOscEnable`,
  plus `morphExemptSlot`) rather than copied into the resolver: two laws
  choose an owner, exactly one applies, and "no second write path" is then a
  property a reader can check instead of a claim in a comment.

- **Evidence consulted:** `traces/2026-09-19-b89-intent-flag.md` (2b, in full —
  its open questions 1 and 2 are binding here);
  `docs/proposals/b89-phase2-intent-resolver.md` §0 (the seam), §3 R2/R10/R15/
  R16, §4's 2c increment (the two named hazards); `DECISIONS.md` ADR-176 +
  A1–A3, ADR-173, ADR-152, ADR-125 (structural requests in full), ADR-109 (the
  applyParam choke point, exempt), ADR-108 + `src/depends_graph.h` (the hold),
  ADR-110 (two copies of a law is the failure this repo already paid for),
  B48/ADR-100 (the osc-enable ramp), B49 (the FX slot atom), ADR-160 (undo),
  ADR-082 (write path and read path must be routed together — its other half
  is still open, see the findings below); `src/intent_core.h`;
  `src/hypersaw_clap.cpp` `morphStep` / `intentStep` / `applyParam` /
  `readParam` / `morphRouteEdit` / `morphCapture` / `morphToggleExempt` /
  `morphInit`; `tools/intent_check.cpp` section S; `tools/undo_check.cpp` (the
  headless gesture idiom); `tools/statefix_common.h`.

- **Alternatives rejected:**
  - *Copying morphStep's application into `intentApply`.* Rejected: ADR-110's
    scar, and it would make the acceptance's "no second write path" a promise
    about a copy. The extraction is behaviour-preserving and proven so by
    parity_check 156/156 + statefix_check + bank_check with no file
    regenerated.
  - *Letting the resolver reinterpret BLEND mode.* Not built: with the flag on,
    mode 1 patches resolve by OWNERSHIP, because SPEC-INTENT-BUS §4.3 has no
    blend analogue. Recorded as an ADR candidate rather than silently ruled.
  - *Re-clamping the ADR-108 hold into the owner's range.* Rejected — it is
    the hazard the plan names. The clamp is the owner's and happens inside
    `IntentCore::stepParams`; the hold replaces the value afterwards and is
    NOT re-clamped. Calibrated by deliberately miswiring it: TG2 goes red
    (reads 0.90 instead of 0.45), and the miswire was reverted.
  - *A new array to record the applied target.* Rejected: the applied value is
    readable through `get_value`, and pinning `morphGlide` to 0 in the oracle
    makes that reading exact — one fewer RT-owned array for the same evidence.
  - *Excluding the mismatching ids in TC by name.* Rejected: a name list is an
    excuse. TC measures the mismatching-id SET under the resolver and under the
    shipped field and asserts the first is a subset of the second.
  - *Fixing `morphRouteEdit`'s unarmed routing under the flag.* Out of scope,
    and deliberately not done silently — measured and reported instead (see
    open questions).

- **Verify:** `./verify full`, exit 0, git `83ca127`
  (`.harness/last-verify.json`). The legs that matter:
  `parity_check: 156/156 scenarios within eps=1e-06 (worst 4.262e-09 @
  dyn-ring.seed42)`; `statefix_check: GREEN (3 fixtures, 0 failures)`;
  `bank_check: 0 failure(s)` with NO golden or factory file regenerated;
  `rtsafety_probe: GREEN (audio thread is allocation-free)`;
  `undo_check: GREEN (0 failures)`; `routing_check: GREEN (0 failures)`;
  `include_check: GREEN (106 files)`. `intent_check`: 32 fixtures, 0
  failures — section S (2b) re-run verbatim and still green with the apply
  live, plus the new section T:
  - TC 200 positions x 3 patches: 43175 / 43133 / 43358 of 49600 reads land
    EXACTLY on the resolved target (worst 1.78e-15), worst
    |applied − corner[owner]| = 1.42e-14 against a 1e-12 tolerance; R15 the
    morph-off render is bit-identical at flag 1 and flag 0.
  - T1 lock: 67 owned positions, 0 movement, worst 5.55e-17; control moves 0.5.
  - T4 (0.5,0.5) temp 1: detune owned by corner 1, decay by corner 0, one macro
    moves both by their own corner's depth (0.18 / 0.3995, predicted from the
    plugin's own corner values); control collapses to one corner at temp 0.02.
  - T5: 0 of 200 `n` values outside the four requested, 4 of 4 visited, 0 scale
    splits; control produces 76 splits and 38 scales no corner authored.
  - T6: commit at the (1,1) corner, worst |resolved after − before| 4.44e-16,
    knobs sum to 0; control (corner 0) moves it 0.559.
  - T9: 0 owner disagreements over 49600 reads for one seed, 21590 for
    another, 0 at the four exact corners.
  - T10: shallow corner capped at 0.804 s while the deep one reaches 4.0 s;
    control swaps the roles.
  - TE: armed edit, capture, exempt and one-gesture-one-node all hold with the
    flag on. TG1/TG2: the clamp is the owner's range, and a held value is not
    re-clamped.

- **Open questions:**
  1. **`morphRouteEdit` routes an UNARMED edit with the shipped Gumbel pick,
     not with the resolver's owner.** Measured: over 24 unarmed edits the two
     disagreed 4 times, and where they disagree the edit lands in a corner that
     is not sounding and the next grid tick overwrites it — the v1 seam ADR-109
     closed, reopened by the flag. Out of this brief's scope (the acceptance
     names only the armed case); ADR candidate.
  2. **Three READ-path families report a value the field never gave them**,
     and they are pre-existing — TC's attribution leg measures the same ids
     under the shipped field: (a) `beatMult` (23) and the step grid (148) are
     snapped by applyParam (the destination's own law); (b) ids 44–55 and 65–68
     read the SHARED `spectra` core while applyParam writes it for every
     oscillator's copy, so osc 1 reports what osc 2 last applied; (c)
     `toneTilt` (71) collides with `tilt` (45) in the core key map — id 1071
     reads back 1.175 on a parameter declared −1..1. ADR-082's other half.
     ADR candidates; nothing here changes the read path.
  3. **BLEND mode under the flag** resolves by ownership (no §4.3 analogue).
     Harmless while the flag is a dev default-off, and a ruling before it
     isn't.
  4. **Four new debug doors**, each with exactly one caller and no shell/GUI/
     host path: `..._intent_commit` (§7, for T6), `..._intent_break_atom`
     (T5's control), `..._capture` (the GUI bridge's capture verb, headless —
     the same reason `hypersaw_debug_exempt` exists), and `..._intent_plant`
     carried over from 2b with its second write loop removed. The first three
     are new surface; if the lead would rather have fewer, T5's and T6's
     controls are what would be given up.
  5. **`rtsafety_probe` is GREEN but still does not drive the flag on**
     (2b's open question 3, unchanged): allocation-freedom on the apply path is
     ENTAILED — `intentApply` calls only `findParam`, `depLiveInCorner` and the
     extracted helpers, none of which allocate — not MEASURED.
  6. **`intent_check` remains STANDALONE and unwired** (ADR-171 is the route;
     wiring a gate is the human's decision). Its `feature_tests.tsv` row still
     reads `oracle = none`.
