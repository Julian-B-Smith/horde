# Sub dial

The Sub's picture (GUI-35, GB9): one heavy cycle wrapped round a dial, with its ceiling as a ring, and, where there's room, the same cycle unrolled beside it. From `sub-noise-lab.html`, the human's pick of its three.

**What it shows.** Drive flattens the cycle against the ring, and the stretch at the ceiling is drawn in `scr-marker`. Erode frays it, with three faint re-reads over the main trace. Every shape, the two bump harmonics, Crunch, Character, Tone and the chain's order read through.

**Where it goes.** A band across the Sub page, above the controls, which take the picture column's width. A smaller version on Main's Sub tab and on the Dashboard's Sub card. It draws the unrolled cycle only above 300 CSS px wide.

**The consumer provides** the canvas, the sub's settings in their units (`Synth.subCycle`'s list), and `st`: `{ t, level }`, seconds and 0..1. Call `Horde.Viz.subDial(canvas, sub, st)`. It turns with `t`. `Viz.off` covers it when the sub is off (ADR-105 A2).

**SAMPLE.** It's a stand-in of SN-2, SN-3, SN-5 and SN-14, computed from the settings, close enough to read and not the law. The engine's post-chain output replaces it (GN-48). The host says so under the glass.

**FX device (ADR-254).** It hosts no sources, so this doesn't appear there.
