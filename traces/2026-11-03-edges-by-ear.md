# Standard's edges, by ear

**What changed.** ADR-356, ROADMAP B609, rulings row 89, a design-ledger entry and a new section of `docs/port/engine2-cpu-ledger.md`. No code, spec or test; the spec change waits for round 54.

**Why.** The human can't run the engine at its cost (ADR-354), and asked what the cleanliness standard buys.

**Evidence consulted.** Scratch patches of the builder's `5442272`; items 7.7 and 7.8 under each soft-edge law; local listening pages with a must-hear control; a critic's review of the no-change fixes; a read-only check of the current engine, of which the lead confirmed what main's plugin builds.

**Verify.** `./verify fast` on this tree; see the PR.
