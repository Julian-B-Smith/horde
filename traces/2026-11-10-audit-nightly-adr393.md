# B671 and ADR-393: the nightly is installed; the Fable audit's first rulings

**What changed.** `tools/nightly/nightly.workflow.yml` moved (git mv) to `.github/workflows/nightly.yml`; its header now says when and
why it was installed. `tools/nightly_check.py`'s workflow rows read the installed file. `tools/nightly/README.md` describes the
installed workflow. DECISIONS ADR-393 records the human's poll on the audit; ROADMAP B671–B676 added and B670 updated with the
audit's finding 3 as the probe's first candidate; design-ledger `not_design` entry for ADR-393. These records ride on this work PR,
which is ADR-393 item 2 itself (the first under it).

**Why.** The audit (`docs/audits/2026-11-10-fable-audit.md`, finding 2): `freshness.py` printed `NIGHTLY UNKNOWN -- gh failed: HTTP 404:
workflow nightly.yml not found on the default branch`; four armor rows counted six jobs that had never run. The human: "Install it".

**Evidence consulted.** The audit's findings 1–20; the lead's own check of finding 1 the same day (engine repo ahead 85, tests repo
ahead 69, behind 0 on both, origin last 2026-10-27; `tmutil latestbackup`: "Failed to mount destination"); `tools/nightly/README.md`
and `jobs.json` (six jobs, four Linux, two macOS); the draft's header (the agent token may not push under `.github/workflows/`).

**Checks.** `python3 tools/nightly_check.py`: GREEN (0 failing), including "workflow only calls ./verify nightly" and "workflow passes
the house pin rules". `python3 tools/workflow_pin_check.py`: GREEN (4 workflows). `./verify fast` below, before the push.

**Not done here.** The drill and the first night run on GitHub after merge (B671's next step); the clean-room pushes are the
human's (the harness blocks an agent push to `main`).
