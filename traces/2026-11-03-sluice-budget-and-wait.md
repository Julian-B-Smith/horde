# Sluice's budget and RS-6.6's wait

**What changed.** ADR-359, new row B612, RS-6.6 and RS-6.12 in `docs/proposals/rack-slot-contract.md`, a design-ledger entry; four replies in `integrations/` committed; a response, seq 78, in Sluice's mailbox. No code, spec or test.

**Why.** Sluice was waiting on a ruled budget and on RS-6.6, and the lead had mislisted it.

**Evidence consulted.** Sluice seq 75, 76 and 77, with their measured figures; Bulwark, Shriek, Scape and Kiln's budget replies; ADR-339, ADR-343, ADR-351.

**Verify.** `./verify fast` on this tree; see the PR.
