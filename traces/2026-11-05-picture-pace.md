# 2026-11-05 — The pictures' pace, and At phase made cheap (GUI-33, GB18)

**What changed.** Blade 2's hatch is one tile instead of a stroke per stripe. Every picture, the Specimen included, draws on a change or at most 30 times a second while awake, and otherwise the last frame stands. `dev.sounding()` is the seam for "a voice is sounding".

**Why.** The human, on the measurements in GUI-33: "Yes, fix them".

**Evidence consulted.** Painter timings in a browser under load: At phase at 16 members 20.0 ms before, about 1 ms after; Sum 0.7 ms. ADR-140's idle gate, ADR-099 A1, B76, B79, RC-8.

**Two self-check bugs found on the way.** The waveform check read the windows of whatever view was remembered, so it failed once At phase had been left on: it now sets Sum and restores. And the checks draw many times inside one frame, so they run on their own clock, or the pace would starve them.

**Verify.** `./verify fast`, on the commit this trace ships in. 77 self-checks pass in a real browser, including the pace with controls: a change draws while idle, a second frame inside 33 ms doesn't, and a hand's 2 s ends. At phase's self-check counts strokes rather than time, since the shell is clock-free (the determinism gate caught a `performance.now` here). Not measured: anything inside Ableton.
