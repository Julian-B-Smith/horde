# b89-intent-flag — B89 phase 2b: the intent-bus flag, the `intent=` chunk, and the seam with its output shadowed

- **Queue item:** ROADMAP B89 (phase 2 paragraph), increment 2b of
  `docs/proposals/b89-phase2-intent-resolver.md`; ADR-176 + A1–A3.

- **Why:** 2a landed `IntentCore` and its parity oracle unwired (PR #629). 2b
  is the gate that makes 2c switchable: the resolver is wired into the shell
  behind param 266, resolves SPEC-INTENT-BUS §4.5's `final[p]` on morphStep's
  own grid, and writes it to a SHADOW array nothing but a debug export reads.
  Two facts had to be proven before the apply can land — that the flag off
  costs nothing (and that the comparison saying so could have failed), and that
  with the flag on and no bindings the resolver is a pure corner read, so
  switching it on changes only WHICH corner and never the value.

- **Evidence consulted:** `docs/proposals/b89-phase2-intent-resolver.md` §0
  (the seam), §1 (the data model and the chunk), §4's 2b increment;
  `DECISIONS.md` ADR-176 + Amendments 1–3, ADR-173, ADR-159 (why the chunk is
  id-keyed, not index-keyed), ADR-088/ADR-138 (the sparse-chunk precedent),
  ADR-163 A2 / B117 (the buried-dev-param precedent, ids 264/265),
  ADR-176 §3 / B142 (the routing block as one atom); `src/intent_core.h`;
  `src/hypersaw_clap.cpp` `morphInit` / `morphStep` / `state_save` /
  `state_load` / `stateJson` / `applyStateJson`; `tools/statefix_common.h`,
  `tools/polarity_check.cpp` (the "a section drives the plugin" shape),
  `tools/bank_check.cpp` (`dropBuild`); INDEX L0005, L0031, L0032, L0033,
  L0051, L0053.

- **Alternatives rejected:**
  - *Keying the chunk on the morphIds INDEX* (what the corner chunk does).
    Rejected: an index is a layout fact and ADR-159 is the scar. The parameter
    id is stable, an unknown id is skipped the way `routing=` skips one, and
    it means no layout marker moves in `gen_factory_bank` / `bank_check`.
  - *A second grid accumulator for `intentStep`.* Rejected: two accumulators
    are two things to keep in step. The branch shares `morphAccum`, so the
    resolver ticks exactly when the field would have.
  - *Hand-editing the 40 factory files to add the one key.* Rejected —
    regeneration is the standing route, cleared three times already; the diff
    is proven per file instead.
  - *A new mutator API for bindings/ranges/homes so the oracle could author
    them.* Rejected: the chunk parser already authors all four, so the oracle
    uses the shipped door and 2b adds no write surface it does not need.
  - *Leaving the ten intent captions uninitialised until activate.* Rejected
    once measured: `hypersaw_debug_intent_names` read ten blanks on a fresh
    instance. Fixed at the source — "" now MEANS the ADR-176 A3 default,
    through one accessor the export and the chunk both use.

- **Verify:** `./verify full`, exit 0, git `6850cb4`
  (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"6850cb4",
  "ts":"2026-09-19T04:04:20Z"}`). The three bit-identity legs:
  `parity_check: 156/156 scenarios within eps=1e-06 (worst 4.262e-09 @
  dyn-ring.seed42)`; `statefix_check: GREEN (3 fixtures, 0 failures)`;
  `intent_check` section S3 — the shadow planted into the applied path at flag
  0 renders DIFFERENTLY, the plant asserting its own anchor (248 slots written,
  detune moved 0.32). `bank_check: 0 failure(s)` after regeneration, with the
  per-file diff proven to be `,"intentBus":0` and nothing else (40 files, 0
  faults, the B100 `build` stamp excepted as `dropBuild` excepts it).
  Degenerate identity (S4), 200 positions × 3 patches, 49600 slot-reads each:
  worst |shadow − corner[owner]| = 1.42e-14 against a 1e-12 tolerance.

- **Open questions:**
  1. **(f) as worded is falsified by ADR-176 decision 1, and the ruling is the
     reason.** "The shadow equals morphStep's own output at 200 positions"
     cannot hold in the interior: the walk and the Gumbel draw are different
     SAMPLERS of the same distribution, so they name different owners. Measured
     per patch: they agree on 17669–23094 reads and disagree on 16906–22331.
     What IS true, and is what makes 2c safe, is proven instead — the shadow is
     a pure corner read (S4, 1.42e-14), and at the four EXACT corners, where
     both laws are one-hot, the identity with morphStep's output is total
     (S5x: 800 reads, 0 owner disagreements, worst 3.55e-15). The lead should
     decide whether ROADMAP B89's 2b wording is amended to say so.
  2. **A prediction that measured FALSE, and 2c depends on it (S3e).** The
     expectation was that with the morph ON, morphStep's first grid tick would
     overwrite every planted slot before a sample rendered. It does not — the
     plant is audible from interleaved sample 0. So a writer placed BESIDE
     morphStep is not harmlessly overwritten, and 2c's REPLACEMENT of that
     write (the early branch built here) is load-bearing rather than
     stylistic. Recorded, not retried until it fired (L0033).
  3. **`rtsafety_probe` is GREEN but does not drive the flag on.** Allocation-
     freedom on the `intentStep` path is ENTAILED (every table is sized in
     `morphInit`; nothing `intentStep` calls resizes; `IntentCore` allocates
     never by construction) — not MEASURED. Naming the boundary rather than
     letting a green run imply coverage it does not have (L0031).
  4. **One kParams row cost three satellite one-liners the brief did not scope**
     — `src/param_presentation.tsv`, `tools/gui_reach.py` EXEMPT,
     `tests/feature_tests.tsv`. `./verify fast` is red without each; the gate
     output for each is quoted in the commit that adds them. Flagged as a
     scope exception for the lead to accept or revert.
  5. **`intent_check` stays STANDALONE and unwired.** Wiring a gate into
     `./verify` is the human's decision (ADR-171 is the route); the tool's
     `feature_tests.tsv` row therefore reads `oracle = none`.
  6. **ADR candidates (not written — ROADMAP/DECISIONS are the lead's):**
     (a) the `intent=` chunk's grammar and its id-keyed, sparse, append-only
     contract; (b) "" means the default caption; (c) the S3e finding, as the
     reason 2c replaces rather than joins morphStep's write; (d) the amendment
     to B89 2b's acceptance wording in open question 1.
