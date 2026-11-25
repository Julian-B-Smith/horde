# 2026-11-24 — ADR-408 and B710: FX starts now, Scape after the engine in the test plug-in

**What changed.** DECISIONS.md: ADR-408 (the human: "start FX now with Scape in its own session"). ROADMAP: B710,
QUEUED, for the session the human opens. Design ledger: `not_design` for ADR-408 with the GUI hand-over.

**Why.** The critical-path measurement (2026-11-24) found the wavetable member reaches the test plug-in at S8, weeks
out, while RC-5's stereo hand-off lets a module play after the current engine now; B660 proved the pattern in a day.

**Evidence consulted.** B660 and ADR-311 (the test-only filter); `tools/engine-preview/` (the 0xA00010 block in
`filter_core.h`; `weir.pin`); `tools/admission/` (Scape's adapter, its build of Scape's `core/`); Scape's mailbox
(`integrations/scape/`, its notice seq 8: Vintage's sound changed, re-run the module rows).

**Verify.** `./verify fast` in this worktree, result in the PR.

**Open.** The session's build; the human's listening.
