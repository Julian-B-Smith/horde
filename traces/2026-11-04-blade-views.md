# 2026-11-04 — Blades in their title bars; the waveform, carpet, voice map and spectrum; scrollbars; the MAP capsule (GUI-30, GUI-31, GUI-32, GB18)

**What changed.** The master gains `H.BladeView` (three views, the drag, the mapping), `Synth.cycle`, `Synth.atPhase`, `Synth.cycleBands`, `Viz.off`, a formant band on `Viz.spectrum`, a title-bar switch on `Fold`, the scrollbar rules and the MAP capsule's flush chip. The shell's Osc pages follow the SCALPEL lab's layout, and Main's Osc tabs carry the waveform. A blade's `on` no longer hides its rows, and an Envelope or Dashboard card no longer hides with its source.

**Why.** The human's words in GUI-30, GUI-31 and GUI-32.

**Evidence consulted.** `docs/design/scalpel-interface-lab.html` (`buildOsc`, `paintCycle`, `phaseModel`, `xyWire`, `xyMove`, `xyOverlay`, `xyMapRow`, `formant`) and its pages in a browser at 980 x 720; `specs/SPEC-ENGINE.md` RC-8 and IF-10; the port ledger's `osc-visualizers` row.

**A self-check bug found on the way.** The fit check found a fold's tier pills as every radio in its header, so with a mode there it clicked mode pills to restore, and left blades at +Adv. It's scoped to the tier group now. That isn't a weakening: it reads what it always meant to.

**Verify.** `./verify fast`, on the commit this trace ships in. The shell opened in a real browser: 75 self-checks pass, three of them new for GUI-30 and one for ADR-105 A2, driving the real pointer path with controls.
