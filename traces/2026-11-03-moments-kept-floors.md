# Moments keeps the floors

**What changed.** ADR-358, B609's text, new row B611, rulings row 90, a design-ledger entry and a new section of `docs/port/engine2-cpu-ledger.md`. No code, spec or test.

**Why.** ADR-356 said a method that kept more of today's sound at the same cost would come back to the human, and moments did.

**Evidence consulted.** Items 7.7 and 7.8 on scratch builds under each method; a controlled alias reading; agents' reports on moments, direct Sync and tables, whose claims are marked scratch; the human's listens.

**Verify.** `./verify fast` on this tree; see the PR.
