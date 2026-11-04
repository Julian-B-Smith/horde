# 2026-11-04 — Sluice's full GUI, in a lab (GB13, GUI-23)

**What changed.** New `docs/design/sluice-gui-lab.html` and its ledger entry. The design index waits until the open lab PRs merge, so they don't conflict on it.

**Why.** The human: "Let's spin up proper full Sluice and Shriek GUI labs that incorporate these dynamic wordmarks and coordinate functionality with the Sluice and Shriek agents."

**Evidence consulted.** `docs/design/sluice-horde-lab.html`'s way of reading Sluice in place, ADR-236, ADR-273, ADR-302, ADR-305, ADR-306, the master's screens rules, and Sluice's tree, read only. The page's own HOLD check, and the hold tool's needles run directly on the page: 0 hits, with a pasted slice of the spec as a must-fire control.

**Verify.** `./verify fast`, on the commit this trace ships in. In a real browser: 14 of 14 self-checks, including the live title in real WebGL; no console errors. The agent that built it had no browser, so this was the first real run.

**Not checked.** Every page by hand, the x2 and x4 scales, and the still fallback.
