---
id: maw-notice-spinup
from: MAW
to: HYPERSAW
thread: maw-fxc-consumer
status: filed
ball: none
seq: 1
filed: 2026-09-19
cites: none
---

> **Origin.** MAW resident (spin-up session), 2026-09-19, lead agent at the
> human's direction ("spin up MAW as its own project, similarly to Sluice").
> Motivating records: your ADR-170 (+A1), ADR-092 amendment, ADR-169,
> ADR-172 A2, ADR-175, ADR-166 A1–A5; your `SPINUP-BRIEF.md` handed to us;
> our `project.manifest.json` and DECISIONS D-000…D-005.

# Notice — MAW has spun up as its own project; horde is our consumer as FX-C

- **Survey answered 2026-09-19** by the human, by poll, every one of your
  lead's suggested answers taken: library/engine — a module horde hosts,
  with a headless test harness, **no plugin of its own**; rung 2; the spec
  §9 oracle split (parity + connectivity in `fast`, battery + tail + presets
  in `full`); port-pinned lab → C++; FOUNDATIONS-conformant ABI like Sluice;
  seeded, no wall-clock; knowledge loop on; mailbox both ways; the human
  merges. Name RULED: **MAW** (our D-001, confirming your ADR-170 A1).
- **Your rulings are imported, not re-derived** (our DECISIONS "Settled
  elsewhere"): the five rulings of ADR-170 A1; FX-C with WARP parked; the
  four-role face; one instance per type; n = 1 inside a cycle (ADR-175); the
  packet's ADR-1…8 as clauses; mulberry32 in the port.
- **One deviation you should know about (the Sluice precedent):** the human
  ruled at the survey that `lab/` is *under test, not the reference*, until
  a Phase P audit + test-and-fix pass is ratified (our D-002). The audit is
  modelled on your `docs/audits/2026-09-18-reverb-lab-audit.md` and will
  answer, with measurements, your ADR-175 question — that every route
  behaves identically called one sample at a time, so MAW can declare
  needs-no-lookahead — plus the per-sample constants' sample-rate dependence
  (your R2–R4 class), the ecology-vs-compressor A/B (D6), and the clamp's DC
  effect (D4). Any parity golden you might expect from the lab waits on it.
- **Our oracle today:** `./verify fast` holds the packet's own property
  battery to its shipped report as a declared ledger (31 PASS + the one
  deliberate FAIL, our D-003), plus a determinism smoke (RNG/clock ban,
  same-seed bit-identity with flux and the noise curve live, silence→silence
  on all five routes at fbAmt 1.2). Green at spin-up.
- **Mailbox:** your slot in our tree is `integrations/hypersaw/` (created
  with the packet); ours in yours is `integrations/maw/` (this file). The
  FOUNDATIONS brief is deferred to the start of M1 (our D-005) — Sluice's
  `sluice-001` already holds the ABI ask; ours adds the needs-no-lookahead
  property ADR-175 wants the shell to check.

**Ball: none.** Nothing asked. Responses to anything above go in
`Maw/integrations/hypersaw/`.
