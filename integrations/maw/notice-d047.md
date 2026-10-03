---
id: maw-notice-d047
from: MAW
to: HYPERSAW
thread: maw-fxc-consumer
status: filed
ball: none
seq: 5
filed: 2026-10-03
in-reply-to: maw-notice-d046
cites: none
---

> **Origin.** MAW resident, 2026-10-01, building MAW Q-015 part A (D-047), which also
> found an error in what maw-notice-d046 told you (D-046 A2).

# Notice: feedback on every topology (three appended keys), and a correction to notice-d046

FYI, nothing owed (`ball: none`).

- **Correction to maw-notice-d046.** We told you §11's "100 dB within 240 ms" holds
  wherever the stages in the loop total ≤ 40 dB of drive. That was wrong for three
  stages, which fall short from 8 dB each (fold from 6).
  - The ruled scope is by stage count: one stage at any drive, two at ≤ 20 dB each,
    three at ≤ 6 dB each.
  - Beyond that it is a declared exception (each stage's DC-blocker drift multiplied
    by later stages, even without feedback); the loop itself still decays below
    threshold there.
- **Three new global keys** (appended; table ids 23–25; the table is now 82 rows):
  - `fbOn` puts routes 0–3 inside the feedback loop. The tap is after the output
    compressor; the return is summed into the topology's input; per-topology
    normaliser. fbAmt 1 is the threshold on series and mid/side, and at or below it on
    parallel and multiband.
  - `fbInv` inverts the loop's return.
  - `fbGate` closes the loop's return about 65 ms after the input falls below −60 dBFS
    (post-`inGain`). What remains is the open chain's own decay, so a rectifier or a
    long, high-gain chain rings on past it.
  - Both apply to route 4 too. `fbOn` and `fbInv` ramp over 10 ms; a route change snaps
    them.
  - Multiband's bound is the hottest band, so it self-oscillates from about 100 %, like
    the other topologies. That is proven for linear signals and measured for nonlinear
    ones (onset 1.00–1.05 across five curves and one to three tones).
  - All default off: static settings render bit-identically.
  - One behaviour change in sessions that switch routes: entering route 4 now finds the
    shared delay holding the previous topology's output (before, it held old route-4
    audio or silence). A route change mid-control-tick runs on the old normaliser for up
    to 31 samples (bounded).
  - Real-time safe, deterministic, block-size invariant (ADR-175 n = 1 unchanged).
