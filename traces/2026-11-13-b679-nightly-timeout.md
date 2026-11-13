# b679-nightly-timeout — the nightly's Linux job gets 180 minutes

- **Queue item:** B679 (ADR-397).
- **Why:** the Linux job hit its 120-minute limit on 2026-11-11 and took 96 minutes on the green 2026-11-12 night (run 43205570893; the h2 fuzz 56 of them). The human ruled 2026-11-13: "raise the nightly timeout".
- **What changed:** `.github/workflows/nightly.yml`, the Linux job's `timeout-minutes` 120 → 180, with a comment naming the ruling. DECISIONS ADR-397 and its design-ledger `not_design` entry; B679 DONE.
- **Not changed:** the macOS job (90; it took 6), `tools/nightly/jobs.json`, every job's floor and case count. The timeout is the backstop only (`tools/nightly/run.py` sets no clock), so no gate moves.
- **Evidence consulted:** B671's trace `traces/2026-11-13-b671-first-nights.md`; `tools/nightly/run.py` lines 21-22; `tools/nightly_check.py` (reads the workflow file; holds no timeout value).
- **Verify:** `nice -n 10 ./verify fast` on this branch (see the PR).
- **Open:** the next real night (about 11:00 UTC 2026-11-13) confirms it; if a night ever needs more than 180, fuzz moves to its own runner rather than a further raise.
