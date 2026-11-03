# Assign XY

The pad whose two axes work as two extra macro knobs (GUI-9 r1): each moves any number of parameters, each with its own depth, saved with the patch (GUI-2 r2). Under it, on Main, the Specimen.

`Horde.AssignXY(el, S, { x, y, labelX, labelY, tag?, underlay?, rest?, axes?, onAxis?(id, chip), onInput?(x, y), keys? })` returns `{ get(), set(x, y), redraw() }`.

**Axes** `axes`: `[{ id, label, corner: 'tl' | 'br', text(), lit() }]`, a chip per axis in its corner, reading `text()` (what it touches) and pressed while `lit()`. A click calls `onAxis(id, chip)`, and the host opens the axis's `MacroCard`. With chips, leave `labelX` and `labelY` empty and the canvas draws no names. `rest` marks the centre, where the axes offset nothing (GUI-9 r3).

**The pad writes no parameter.** It moves two source values, and the host's mappings carry them (GUI-9 r3). A drag makes no undo step, as a macro knob's doesn't.

**Anatomy** the screen's ground, a veil of quarter lines at 18% `scr-grid`, a 6 px puck in `scr-value`, and the axis names in the corners. The underlay canvas sits beneath it.

**Gestures** a press lands the puck and a drag follows. Arrow keys move it in keyboard mode only (ADR-259 r6).

**Sources** GUI-9, `docs/design/xy-macro-lab.html`, GUI-2, the bench's `paintXY`, `docs/design/main-layout-lab.html`.
