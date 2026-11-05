# 2026-11-04 — A lab for the oscillator's three layouts (GB19)

**What changed.** `docs/design/osc-layout-lab.html` is new: three candidates each for the mini view, the expanded page and the dashboard, at true size, with the master's own controls and pictures. It's listed in the ledger and the index. Nothing in the shell or the master changes.

**Why.** The human: "there are a lot more controls than just Detune on the oscillator outside the blade. We should probably do a lab to work out the layout of the mini osc view, expanded osc page and osc dashboard (which needs both filters as well)".

**Evidence consulted.** GUI 3's generated rows (49 oscillator rows outside the blades), the SCALPEL lab's tiers and `docs/scalpel/ACCOUNTING.md`, `layout.json`'s Dashboard and Filters pages, and GN-16 and GN-17 for what the filters lack.

**Verify.** `./verify fast`, on the commit this trace ships in. The lab opened in a real browser, with its self-check passing: each row in one group (with a control), only real rows mounted, every candidate fitting its size, and the waveform's drag on the real component (with a control).
