# Spec round 52: the CPU target is 10.7 % (ADR-354)

**What changed.** SPEC-ENGINE §10.6 item 4; the structure's spec blob; the rulings log, design ledger, DECISIONS and ROADMAP. No code and no test.

**Why.** The human ruled it, after the lead showed that the 29 % band could not fit the spec's own budget.

**Evidence consulted.** PF-1, PF-2, PF-3, PF-5, SB-1, ADR-284, ADR-347. The arithmetic on morphing and voices is the lead's, from the spec, and nothing has measured it.

**Verify.** `./verify fast` on this tree; see the PR.
