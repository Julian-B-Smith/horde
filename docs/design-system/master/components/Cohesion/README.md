# Cohesion

One Cohesion control for the whole morph field: how strongly the parameters pick the same corner together (ADR-266 r11).

**The consumer provides** the session; the control writes `S.morph.cohesion` (0-1). It is a device control of the morph, never a corner value.

**Anatomy**
- A house slider row: "Cohesion", the face, the value to two places.
- One sentence: at 0 each parameter draws its own corner, at 1 the whole field flips as one (a unit always does); not the swarm's Coupling.
- A strip: each picking row's corner at the puck as an 18 px disc in its corner fill with its glyph in `on-corner`, then "n of m rows would pick B at the puck".

**Gestures** relative drag, `size-drag` (160 px) the full range, shift ×0.25; double-click resets to 0.30; Option + wheel sets; arrows in keyboard mode. One undo entry per gesture.

**Placeholders**
- Per-group Cohesion. ADR-266 r11: one control for the field; per-group overrides may come later. Nothing per-group is drawn.
- The default, 0.30: the lab's factory patch value. Decided by: the morph layer's defaults.
- What Cohesion does to ownership. The stand-in mixes each row's offsets toward one shared draw; the engine's law decides it (the morph layer's specification).

**Sources** `docs/design/morph-editor-lab.html` :615-620 (the shell's id 155 still carries the old name, Coupling; B269 renamed it). ADR-266 r11.

The preview draws SAMPLE data generated in the preview, never the engine.
