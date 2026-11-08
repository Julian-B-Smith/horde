# B649: the wavetable build's spec and contract (ADR-385, with ADR-377 A3)

**What changed.** New `specs/SPEC-WAVETABLE.md` and `docs/wavetable/CONTRACT.md` (an Opus drafter, commits ae86cf20, 0fb365cf, 279d253e). DECISIONS gains ADR-385 (the human's six rulings for the build) and ADR-377 A3 (wavetable corner tables blend in 1.0). ROADMAP gains B649 and B650. The design ledger marks ADR-385 `not_design`.

**Why.** The human, 2026-11-07, asked for an isolated wavetable build that learns from the clean room (CPU and fidelity tested at every stage, the human listening at every stage), starting with the spec and the contract; the rulings came by poll.

**Evidence consulted.** SPEC-QUADRANT and its bench; ADR-384's eleven amendments; SPEC-ENGINE §5, Appendix D, IF-1..13, MS-*, QS-*, PF-*, PA-*; the clean-room engine at e72a092 (`swarm.cpp`, `voice.cpp`, `resolver.h`, `manifest.h`, `waves.h`), read-only; B648's instruction counts (`traces/2026-11-07-b648-quadrant-cost-bench.md`). An independent critic (Opus, fresh context) returned REWORK with three blockers and fifteen majors; each is answered in 279d253e (the drafter's per-finding list is in the PR). The drafter's engine-free calculations: WT-F2's C6 deficit per level spacing; the 6-point read's aliasing at C1–C8; K-weighted loudness spread of the factory frames.

**Caveat on independence.** The drafter and the critic are the same model lineage in separate contexts; the human's ratification is the independent judgement.

**Alternatives rejected.** Outcome (A), tables picked at quantum boundaries (ruled out by "Yes, in 1.0"). A `wt.on` host row (an engine selector under GUI-1; the member source is hidden engine state). Ids at 0x400 (the manifest's next-free-offset rule puts them at 0x030).

**Verify.** `./verify fast` on this tree; see the PR.
