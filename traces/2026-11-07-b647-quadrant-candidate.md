# B647: QUADRANT ingested as a CANDIDATE (ADR-384); ADR-337 A1 records the human's positioning

**What changed.** `specs/SPEC-QUADRANT.md` and `reference/quadrant/quadrant-wavetable.html`, byte-identical to the human's delivery (commit 1), then the one sanctioned edit: the bench worklet's voice start phase draws from a seeded mulberry32 stream instead of `Math.random()` (commit 2). Both join the charter's protected paths. DECISIONS gains ADR-384 (CANDIDATE, with the eleven amendments from the lead's review) and ADR-337 A1 (the human's positioning, verbatim). ROADMAP gains B647 and B648 (the cost bench, in progress on its own branch). The design ledger marks ADR-384 `not_design`.

**Why.** The human, 2026-11-07, after B645's pass 2, by poll: "File it plus a cost bench" and, for the positioning, "Record it as said". The human called the review's recommendations right and described the bench as a proof of concept for the blend, to be merged with the swarm plus FM/PM, sync and warps.

**Evidence consulted.** The spec and bench as delivered; the bench's worklet (`on`, `lvl`, `process`), `recompute` and `blendSpec`; SPEC-ENGINE MS-46/MS-47 (per-note sources to shared destinations), the `quality` row (1×/2×) and QS-1 (the aliasing method); ADR-377 (structure follows quantum boundaries in 1.0; waveform morphing after 1.0); ADR-337; GUI-1; B645's engine insights (`traces/2026-11-07-b645-demo-bank.md` on its branch). The edit's check: the worklet source, with `N` and `VOICES` substituted, parses under node; one `Math.random()` remains, for library ids only.

**Alternatives rejected.** Folding the review into the spec text (the spec is the human's protected reference; the amendments live in ADR-384 until a ruling ports them). Seeding the library-id draw too (it never reaches the sound and is not a draw the core would make).

**Verify.** `./verify fast` on this tree; see the PR.
