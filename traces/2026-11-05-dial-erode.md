# 2026-11-05 — The Dial with Erode up (a GUI-35 bug)

**What changed.** `Viz.subDial` keeps its noise under names of its own, and the shell's frame count is `st.frames`. The Dial's self-check turns Erode up.

**Why.** The human: "turning up \"erode\" removes the sub osc visualizer". Both kept something under `st.n`; with Erode above 0 the buffer became a string, the draw threw (`Cannot assign to read only property '0' of string`), and the shell's view loop marks a view that throws as failed and never draws it again.

**Verify.** `./verify fast` on the commit this trace ships in. 83 shell self-checks in a real browser at 980 px, the Dial's now requiring that Erode at 0.6 draws with no view failing; against the old painter that fails. A real mouse turned Erode up on the Sub page with no console error.
