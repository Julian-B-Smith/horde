# Spec round 64: the sources' on switches share one pick (ADR-383, B643)

**What changed.** SPEC-ENGINE MO-26 (3): under Quantum, and wherever a switch resolves by its pick, the four sources' on switches (`osc1.on`, `osc2.on`, `sub.on`, `noise.on`) share one switch pick, MO-7's at h = 1 (the shared draws g* alone), held by one MO-8 latch, so they change together and the on/off pattern is always one real corner's. Exempt switches and fading Follow switches don't join [X91]; a region lock on a switch is refused [X92]. MO-7, MO-13 and MO-24 point at it; D.8's intro quotes the ruling; item 34.19 gains "The switches pick together" and a must-fail control (today's per-switch picks, silent on the A–B edge at seed 1024); §11.1 gains X91 and X92 (71 → 73). The structure's spec blob. Notes for the lead: `docs/proposals/round-64-notes.md`. No engine code, no DECISIONS entry (the lead writes ADR-383).

**Why.** The human, 2026-11-07, by poll, on B641's finding that Quantum's independent switch picks leave held chords silent for 350–560 ms: "Switches pick together".

**Evidence consulted.** SPEC-ENGINE Appendix D (MO-4 to MO-13, MO-20, MO-24, MO-26, MO-27), IF-12, MS-25, LF-8, items 34.3 to 34.5, 34.9, 34.18 to 34.21, 36.7, §11.1. B641's `tools/engine-preview/morph_hole.cpp` and `traces/2026-11-07-b641-morph-dropouts.md` (branch `b641-morph-dropouts`, `4ae11813`). `engine2/structure/engine-structure.json`'s `morph_resolver` block (no item or edge change). `docs/design/gui3/device.js` (the page's region painting and locks). Round 63's notes and trace for form.

**Alternatives rejected.** A lock on one switch moving all four (needs a precedence rule over four locks, X92); a latch that clears while no switch picks (needs a clearing rule, X91).

**Open for the lead.** ADR-383 with X91 and X92; the GUI's switch regions and lock offer (an ENGINE-NEEDS row); the clean-room spec sync to blob `f61c2d7d0747405e1ef4d84111d318360e6d8966`.

**Verify.** `./verify fast` on this tree; see the commit.
