# 2026-11-15 — B603: state budgets ruled (ADR-401)

**What changed.**
- DECISIONS: ADR-401, the human's four rulings by poll, each the recommended option:
  - the budget counts what the module writes;
  - 4,096 B per corner, Sluice 12,288 B;
  - Shriek's extra snapshots go in a v2 blob under that budget;
  - names 64 B and labels 32 B of UTF-8.
  - The patch ceiling stays open under ADR-359 r2, waiting on B612's host save test.
- ROADMAP:
  - new rows B700 (contract text), B701 (admission reads the ruled budgets), B702 (horde 2's creation check and caps) and B703 (the notices; Arp is told its count changed);
  - dated notes on B603, B576, B577, B604, B605, B610, B612 and B626.
- Notices `notice-state-budgets-ruled.md`:
  - beside each reply in horde's tree, for Bulwark, Kiln (seq 10), Mangle, Shriek, Scape and Weir;
  - in the module's own `integrations/hypersaw/`, uncommitted for its residents, for Arp, Echo and Sluice.
- Design ledger: a `not_design` entry for ADR-401.

**Why.** The human asked for B603 after the overdue mail. B603's ruling was asked for by 2026-11-20.

**Evidence consulted.**
- Every module's state-budget reply.
- The decision page went through three adversarial passes (16, then 12, then minor findings), and each pass's findings were applied before the poll.
- The records and notices were drafted by Opus and checked by Opus (11 findings, applied).
- The lead fixed the Kiln seq clash (seq 9 is the B605 notice, so this one is seq 10) and pointed each notice's horde-side work at B700 to B702.

**Verify.** `./verify fast` on this worktree (the result is recorded in the PR).
