# 2026-11-05 — A browser gate for GUI 3, proposed (GB24)

**What changed.** A proposal (`docs/design/gui3/browser-gate-proposal.md`) and its check, built as a prototype outside `tools/` (`docs/design/gui3/browser-check-prototype.mjs`), since an unwired check under `tools/` counts as weakening the gate set. `./verify` is unchanged.

**Why.** The human: "Yes, propose it", after the self-checks' fake clock froze every picture and three hand tests missed it.

**Evidence.** The check on this branch: GREEN in 32 s, with 83 self-checks passing in a real browser, the ring, carpet and voice map moving, and the control reading frozen. On `origin/main` without #1401: RED, "frozen after the self-checks: Phase ring, Phase carpet, Voice map".

**Verify.** `./verify fast` on the commit this trace ships in, with no check added to `./verify`'s set.
