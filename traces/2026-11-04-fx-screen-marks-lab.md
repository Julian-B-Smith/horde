# 2026-11-04 — Morph corners and modulation on every FX screen (GB15, GUI-23)

**What changed.** New `docs/design/fx-screen-marks-lab.html` and its ledger entry. The design index waits until the open lab PRs merge, so they don't conflict on it.

**Why.** The human: "For all FX screen variants, we need to work out a different visual way to display morph corners and modulation. This also deserves a lab." ADR-259 ruling 4 takes corner hues out of a glass.

**Evidence consulted.** `fx-screens-workshop.html`, the master's `screens.md`, the `ScreenFrame`, `Knob` and `ModTiers` READMEs, `tokens.css`, the Morph Knob and Map Mode specs, `SPEC-MODULE-MACROS`, ADR-235, ADR-259, ADR-261, ADR-266, ADR-273.

**Rejected.** Canvas 2D painters, because the checks couldn't read the pixels without a browser. Chassis bezel lamps aren't built; the page asks about them.

**Verify.** `./verify fast`, on the commit this trace ships in. In a real browser: 14 of 14 self-checks, no console errors; a real mouse drag on the puck moved the weights, and a real click on a card made the pick.

**Not checked.** Motion at refresh rates other than 60 Hz, since the animation counts frames, and the schemes other than ORCHID and TUBE by eye.
