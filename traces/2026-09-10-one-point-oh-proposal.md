# 2026-09-10 — the 1.0 proposal, verified and filed (pending ratification)

**What changed.** The human relayed an external audit's five-phase 1.0 plan.
Every checkable claim was verified against the tree before filing:
RIGHT — no LICENSE, no CHANGELOG, zero git tags, no LFO in the shell, Drive/
Filter/Gain still labelled placeholders (fx_rack.h:10,76), SPECTRA hidden
(param 43 has zero gui2 controls; gui_reach passes via legacy gui.html), no
factory bank (docs/presets: two files), CI builds no artifacts, no
notarization, no sanitizer runs. ALREADY DONE (the audit didn't know) —
CLAP param ids frozen/append-only (ADR-082); state schema 3 with a migrator
(ADR-103/138); plugin identity frozen (ADR-002/154); repo layout (ADR-155,
two days ago — its `prototypes/`+`docs/specs/` suggestion would churn it).
Filed: a PROPOSED 1.0 definition at the top of ROADMAP with the roster,
parked list, sequencing, and the FOUR collisions with the human's own
2026-09-05 priority tracks (FX matrix rework, new modules, QM-4, shape/morph
lab ports) left for the human to rule; B100 (state header + engine_revision
+ two gated golden sets + fixture corpus), B101 (release engineering), B102
(factory bank), B103 (evidence layer). No code changed.

**Verify.** `./verify fast` — docs only.
