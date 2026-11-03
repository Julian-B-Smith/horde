# Spec round 51: the coupling knob's new curve, and raw K (ADR-352)

**What changed.** SPEC-ENGINE C.1, SW-4, SW-5, the `coupling` row, X23 and B.1; the structure's spec blob; the rulings log, design ledger, DECISIONS and ROADMAP. No code.

**Why.** The human chose curve 3 by ear, on a sweep of K over three curves on five patches.

**Evidence consulted.** C.1's law simulated at SW-4's conditions, with SW-4's stated values as the control; the sweep's renders from a scratch change on the builder's commit.

**Verify.** `./verify fast` on this tree; see the PR.
