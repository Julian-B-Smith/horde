# 2026-11-05 — Envelope pictures you drag, curves on every section, the lab repaired (GUI-39, GB23)

**What changed.** `Horde.EnvDrag` in the master. In the shell, every envelope picture can be dragged, every source's Envelope section and Dashboard card carries the three curves, the Mod page draws its envelopes' shapes, and the Osc waveform is 116 px. `sub-noise-lab.html` handles the Sub page's new layout. GUI 3 shows a loading note and takes `?fast`.

**Why.** The human asked for interactive envelopes and curve controls on each section, and reported the lab's shell not loading. GUI-35 had removed the column that lab mounts into, so its boot threw and it stayed on its busy line.

**Evidence consulted.** UI Spec section 6, the hierarchy lab's section 0, C.12, ADR-345's fit. The lab's console: `Cannot read properties of null (reading 'prepend')`.

**Verify.** `./verify fast` on the commit this trace ships in. 83 shell self-checks in a real browser at 980 px, including the drag's own: a node moves its row in one undo step, a stage writes its curve, and empty glass does nothing (control). Fit reads 462, 462, 462 and 408 of 462 px. The lab's 13 pass. A real mouse moved a node and a stage on the Dashboard.

**Not verified.** The feel by the human's hand. No gate loads each lab through its real boot, which is how the broken lab reached main.
