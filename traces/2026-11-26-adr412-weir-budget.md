# 2026-11-26 — ADR-412: B714's stage-1 rulings; Weir's state budget ruled at 4,096 B

**What changed.**
- **DECISIONS.md:** ADR-412, the human's four rulings by poll on B714's stage-1 questions.
- **docs/proposals/rack-slot-contract.md:**
  - §12.9's Weir row is ruled: 4,096 B, counting its state as its FX manifest declares it.
  - The amendment line says so.
  - RS-6.12, Q9, Q19 and §11's Weir note now read "ruled".
- **tools/admission/state_budgets.json:** Weir's `status` is `ruled` (ADR-401; ADR-412 r4).
- **docs/design/design-ledger.json:** ADR-412's `not_design` entry.

Rulings 1 to 3 (delete the per-module code, module ids as morph rows, pin FOUNDATIONS) are built in B714's stage 1, not here.

**Why.** B714's plan (six stages, after a four-reader Opus survey, a design pass and an Opus critic) put four questions to the human before stage 1. All four were answered by the recommended option, 2026-11-26.

**Evidence.**
- Weir `main` 92ff8af, `docs/fx/manifest-fxfilter.json`: `"stateBudget": 4096`.
- Weir's own record: D-020, commit e27d85a.
- `tools/state_budget_check.py` reads the §12.9 table against the JSON row for row, and `./verify fast` runs it.
- The poll answers: "Yes, delete them (Recommended)", "Morph rows, as ruled (Recommended)", "Pin FOUNDATIONS (Recommended)", "Rule it at 4,096 B (Recommended)".

**Verify.** `./verify fast` on this branch (in the PR body).

**Open.**
- Weir's creation bound (ADR-401's other ask, due 2026-12-11).
- B714 stage 1.
