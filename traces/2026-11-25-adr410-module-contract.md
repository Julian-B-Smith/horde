# 2026-11-25 — ADR-410: FX in morph, in patches and on real sends; B713, B714, B715 opened

**What changed.**
- **DECISIONS.md:** ADR-410, ruled by the human's poll.
- **ROADMAP.md:**
  - B713: the module contract round.
  - B714: one generic module slot, in the test plug-in first.
  - B715: real routing data for the views.
- **docs/design/design-ledger.json:** ADR-410's `not_design` line. The routing views' design is the GUI session's GN/GB work.
- **The GUI session** has the routing brief: replace GUI-41's mock with tidy wires and a matrix of real send percentages, from one wire model, master first.

**Why.** The human, 2026-11-25, after hearing Scape in build 14: "so we don't have to rebuild the whole system from scratch every time a module is reintroduced". The human asked for three things: FX in morph "just like everything else", FX in patches, and a new routing interface with real send percentages.

**Evidence.**
- **The survey** (2026-11-25, Opus, read-only) had four readers:
  - the contract;
  - the test plug-in's module hosting;
  - the routing UI;
  - the engine's sends.
  A synthesis and an adversarial critic followed.
- **The critic's corrections**, applied before the poll and in ADR-410:
  - ADR-254's checklist is the lead's, not the human's.
  - ADR-377 and ADR-201 Q2 are reconciled. Levels blend as a gain; wire-set changes follow the quantum boundary.
  - FX on/off is MO-26's law, not a new one.
  - ADR-088's crosspoint is reused, not reinvented.
  - The schema is FOUNDATIONS' (ADR-201 Q1).
  - The resolver is the clean room's, so module keys need a spec round.
  - Six forks stay open (ADR-410's list), not filed as settled: feedback, per-source inputs, bypass, host ids, corner-preset scope and the chunk ceiling.
- **Code facts the critic rechecked:**
  - `tools/engine-preview/preview_plugin.cpp:783-801` and `:1770-1784`;
  - `bank.h:8-21`;
  - `bridge_core.cpp:470-472`;
  - `reverb_core.h:19-31`;
  - Scape's `manifest.lock.json` at `8350fb8`, which has no `law` field.
- **The human's answers:** "Per corner, like engine (Recommended)", "Dry: FX off (Recommended)", "Real levels per corner (Recommended)" and "Test plug-in first (Recommended)".

**Verify.** `./verify fast` on this branch (in the PR body).

**Open.**
- B713's contract text and spec round.
- FOUNDATIONS' schema brief and the module notices.
- The six forks.
- B714 and B715 after B713.
