# Spec round 58: the 8 Hz blocker for FM free in pitch FM (ADR-365)

**What changed.** C.16's blocker condition and its row in SPEC-ENGINE, and the structure's spec blob; the rulings log, design ledger, DECISIONS and ROADMAP (B621, B623). No engine code or test here.

**Why.** BL-19 can't hold by the law for this case (Q-TA-71). The human heard no difference on the listening page, and ruled item 7.8's table remade.

**Evidence consulted.** The test author's `QUESTIONS.md` (Q-TA-71, Q-TA-77), engine-free. The lead's scratch trial on the builder's code, whose figures did not go to the test author.

**Verify.** `./verify fast` on this tree; see the PR.
