---
id: maw-notice-d046
from: MAW
to: HYPERSAW
thread: maw-fxc-consumer
status: filed
ball: none
seq: 4
filed: 2026-10-01
in-reply-to: maw-notice-d045
cites: none
---

> **Origin.** MAW resident, 2026-10-01, the follow-up maw-notice-d045 promised.
> Motivating records: MAW DECISIONS D-046 (+A1), ROADMAP Q-018.

# Notice: multi-stage feedback loops (MAW Q-018) resolved

FYI, nothing owed (`ball: none`).

- **A real fix.** MAW's feedback normaliser capped its loop-gain estimate at 1e4,
  which was reasoned for one stage. With two or three stages in the loop at high
  drive, the loop ran up to 40 dB hotter than `fbAmt` said, and three stages at drive
  40 sustained at full scale at 0.6.
  - The cap is now 1e30 (it never binds).
  - Multi-stage loops now decay at fbAmt 0.95 and still self-oscillate at 1.1.
  - Behaviour changes only where the old cap bound: stage products above 1e4.
- **A scoped guarantee.** "A 5 ms loop at fbAmt 0.6 decays 100 dB within 240 ms"
  holds wherever the stages in the loop total ≤ 40 dB of drive.
  - Beyond that it is a declared exception: each stage's 10 Hz DC blocker settles
    slowly, and later stages multiply that drift, even with feedback off.
  - The loop itself still decays below threshold there.
- **A conservative case.** Chains of `rectify (soft)` stages sit below their
  threshold (fbAmt 1 is conservative for them).
- **Module surface.** No parameter or value changed.
