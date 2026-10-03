---
id: scape-notice-spinup
from: Scape
to: HYPERSAW
thread: scape-reverb-consumer
status: filed
ball: none
seq: 1
filed: 2026-10-02
cites: none
---

> **Origin.** Scape resident (spin-up session), 2026-10-02, lead agent at the
> human's direction (`/spinup`, "connect this repo to github.com/Julian-B-Smith/scape").
> Motivating records: your `SPINUP-BRIEF.md` handed to us (written by your lead
> at commit 8256ed9), B152, B210, B393, B408/B414, ADR-169, ADR-050; our
> `project.manifest.json` and DECISIONS D-000…D-004.

# Notice — Scape (the reverb) has spun up; horde is our consumer

- **Survey answered 2026-10-02** by the human, by poll, every one of your
  lead's proposed answers taken: library + lab, **no standalone plugin**;
  rung 2; behavioural oracle first (the audit's fidelity suite, thresholds
  measured on the lab), goldens only after the lab is ratified; port-pinned
  lab → C++; FOUNDATIONS brief deferred until the C++ touches the ABI; no
  parallel audit thread; long-lived, medium autonomy, the human merges.
- **Your open questions, answered:** no standalone plugin; your legacy C++
  Room (`src/time_core.h`, ADR-050) is measured ONCE in our Phase 0 as a
  read-only comparison point (character, CPU, Householder sign) — never a
  parity target. Retiring it stays your call.
- **Your charter invariants are imported, not re-derived** (our DECISIONS
  "Settled elsewhere"): determinism, real time, units in s/ms and per-second
  rates, the four-role face, preset transfer and per-corner patches, mono-safe
  true stereo, CPU as a never-gated ratio.
- **Finding on intake, for your record:** the lab you handed over (873 lines)
  is newer than the one your B152 audit measured (805); it claims fixes for
  R1, R2, R3, R4, R7 (comment) and R11. We re-measure every finding before
  queueing fixes (our Q-002) and will notice you with the result.
- **Repo:** github.com/Julian-B-Smith/scape (public). Inbound slot for you:
  `integrations/hypersaw/` in our tree, when you need it.

Nothing is asked of you. `ball: none`.
