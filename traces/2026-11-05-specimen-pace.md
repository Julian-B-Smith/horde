# 2026-11-05 — The Specimen's rate, resolution, drift and notes (GUI-34, GB20)

**What changed.** The Specimen draws at 30 Hz at full speed, with dividers counted in frames. ADR-144's adaptive scale is back in the master. While it shows it never rests, so B365's idle drift is restored. `dev.feed()` carries notes to it, null in the stub. The pictures' pace is 28 ms instead of 33.

**Why.** The human, after the first Ableton readings: "The specimen is particularly choppy". The frame loop read 59 fps with 0 % over budget, so the fault was the Specimen's own pacing.

**Evidence consulted.** The test page's readout, held awake, before the fix: 60 fps, pearl 10 Hz, 0.50 s of motion per second, GPU 1.7 ms a draw at 466 x 514. `src/gui/gui2.html` chromeFrame (the adaptive law), ADR-139, ADR-140, ADR-144, B365's correction, RC-8.

**Two bugs of this session's.** GUI-33 divided the Specimen twice and rested it against B365's correction; both are repaired here. And this change's own first self-check fed a note with no R, which made the tension NaN and hid the pearl; a screenshot caught it, since every number still passed. The shell now ignores a non-finite R, and the check requires a finite tension.

**A false alarm.** The Fit self-check failed by 4 px in the browser pane, on the pushed code too: the pane had narrowed the window to 823 px and one title bar wrapped. At the plugin's 980 px it passes.

**Verify.** `./verify fast`, on the commit this trace ships in. 78 self-checks pass in a real browser at 980 px. The pearl's has its controls: an `every` of 6 reads 10 draws, a silent feed never strikes, and gaps inside the band leave the scale alone. `tools/labharness/gui3_seam_check.mjs` plants the idle gate and a cut feed.

**Not verified.** The rate by the human's eye, real notes in Ableton (the test build is installed for that), and its cost in Live's CPU meter.
