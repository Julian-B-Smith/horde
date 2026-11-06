# 2026-11-05 — GUI 3's browser check wired into `./verify fast` (GB24, GUI-44)

**What changed.** `docs/design/gui3/browser-check-prototype.mjs` moved to `tools/labharness/gui3_browser_check.mjs` (its docs root re-pointed, its header now `WIRED: ./verify fast`). `./verify fast` runs it beside `gui3_seam_check`: exit 1 fails verify and prints the check's output; exit 3 (no headless Chrome) prints `verify: HOLE -- ...` and does not fail. GUI-44 records the ruling and three readings (the dependency, the ~25 s cost, no armor-catalogue row); GB24 is DONE; the ledger has GUI-44's `not_design` and `engine_impact` entries.

**Why.** The human, choosing among the proposal's options: "A: fast, hole if absent".

**Evidence.** The check from its new home: GREEN in 25 s, "83 self-checks pass in a real browser, Phase ring, Phase carpet, Voice map move after them, and with frames stopped they read frozen (control)". Its RED reading on `origin/main` without #1401 is in the proposal's trace (`traces/2026-11-05-browser-gate.md`). The hole path is the branch the prototype already took with no browser found; CI's ubuntu runner has no Playwright cache, so it takes it.

**Verify.** `./verify fast` on the commit this trace ships in.
