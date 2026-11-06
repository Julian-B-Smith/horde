# Spec round 60: PF-3's chord stays in range (ADR-368, B627)

**What changed.** PF-3's chord clause in SPEC-ENGINE, and the structure's spec blob. Also the rulings log (row 97), the design ledger, DECISIONS and ROADMAP. No engine code and no test here.

**Why.** The human ruled "Fix the chord" on the builder's profile: PF-9's 64-voice cost came from keys up to 7.2 MHz, not from the voice count.

**Evidence consulted.** The builder's scratch profiles of 1ad270a (thread CPU, `sample`), reported by message and recorded in its REPORT.md. The spec's PF-3, PF-9 and PF-10 rows. SW-13.

**Verify.** `./verify fast` on this tree; see the PR.
