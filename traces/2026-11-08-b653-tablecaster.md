# B653: TABLECASTER ingested as a CANDIDATE (ADR-386); ADR-385 A1 plans three oscillators with a PM matrix

**What changed.** `reference/tablecaster/tablecaster.html`, byte-identical to the human's delivery (commit 1), then the sanctioned edit: the roll and mutate draws come from a mulberry32 stream with a fixed seed instead of `Math.random()` (commit 2). It joins the charter's protected paths. DECISIONS gains ADR-386 (CANDIDATE, five integration points) and ADR-385 A1 (three oscillators with a PM matrix in the wavetable build). ROADMAP gains B653 and B654. The design ledger marks ADR-386 `not_design`.

**Why.** The human, 2026-11-08, by poll: "File it" and "Plan 3 with a PM matrix".

**Evidence consulted.** The file as delivered: its generator (`generate`, `ENG`, `combine`, `processFrame`, `finalize`), its PRNG (`mulberry32`, `hashSeed`), its roll path (`rnd`, `newSeed`, `rollAll`, `randVal`, `mutate`; every `rnd()` call site listed with grep). After the edit the one remaining `Math.random` is in the edit's own comment, and the page's script parses under node. SPEC-WAVETABLE's recipe slot, WT-16 and WT-47. The leak patterns flag only the CSS word `place-items`, the same false hit as QUADRANT's, which `private_name_check` passes.

**Alternatives rejected.** Leaving the roll draws unseeded because each table is reproducible from its settings and seed: the sequence of rolls is the user's discovery path, and the charter's determinism invariant covers it.

**Verify.** `./verify fast` on this tree; see the PR.
