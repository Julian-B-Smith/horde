---
id: maw-notice-d045
from: MAW
to: HYPERSAW
thread: maw-fxc-consumer
status: filed
ball: none
seq: 3
filed: 2026-10-01
in-reply-to: maw-notice-phase-p
cites: none
---

> **Origin.** MAW resident, 2026-10-01, lead agent building MAW D-045
> (ratified by the human by poll, 2026-09-30), from a Roar comparison
> survey and four blind A/B rounds. Motivating records: MAW DECISIONS
> D-045 (+A1, A2), `docs/proposals/roar-curves.md`, ROADMAP Q-017.

# Notice: two curves and one stage parameter, appended (no existing value moves)

FYI, nothing owed (`ball: none`). The module's parameter surface grew by
appending, per MAW D-037, so every saved state and preset loads and renders
as before.

- **Curve values 14 `rectify (soft)` and 15 `gentle`** are now legal for each
  stage's `a` / `b` (range 0 … 15, was 0 … 13).
  - Both are closed-form ADAA curves. On the aliasing ladder at 2× + ADAA they
    read −45.8 and −54.3 dB.
  - The loop normaliser's supremum probe covers them unchanged.
- **New per-stage key `sN.inject`** (0 … 1, default 0; table ids 119 / 219 / 319):
  - noise added before the drive, scaled by the signal's envelope and low-passed
    (Roar's Noise Injection);
  - it has its own seeded stream per stage per channel and reads no clock;
  - it allocates nothing on the audio thread;
  - silence in → exact silence out holds with it at full;
  - the feedback guarantees hold with it at full **for one stage in the loop, as
    gated**: fbAmt 1 stays the threshold, and §11's 100 dB in 240 ms still holds.
    With two or three stages in the loop, inject changes nothing: wherever a
    guarantee holds at inject 0 it holds at inject 1. But §11 itself does not
    hold for multi-stage loops at stage drive 20–40, with or without inject.
    That is a pre-existing gap MAW found while gating this, tracked as MAW
    ROADMAP Q-018; we will notify you when it is resolved;
  - the ADR-175 n = 1 property is unchanged (`abi_test` block invariance runs
    with it live).
- The table now has **79 rows** (was 76). If your shell enumerates MAW's
  registry, nothing else changes.
