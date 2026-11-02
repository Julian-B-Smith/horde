# `DECISIONS.md` merges by union (ADR-353)

**What changed.** One attribute in `.gitattributes`, with the records for it. No code.

**Why.** The human ruled yes, after the lead and GUI sessions' appends kept conflicting.

**Evidence consulted.** Forty real pairs of sections from this file replayed in a scratch repository, with a no-attribute control that conflicted on all 40; one test of a line edited on both sides. Local git only; GitHub is untested.

**Verify.** `./verify fast` on this tree; see the PR.
