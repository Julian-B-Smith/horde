# Assign XY

The pad whose two axes the player assigns to any two parameters, saved with the patch (GUI-2 r2). Under it, on Main, the Specimen.

`Horde.AssignXY(el, S, { x, y, labelX, labelY, tag?, underlay?, onInput?(x, y), keys? })` returns `{ get(), set(x, y), redraw() }`.

**How a player picks the two parameters isn't designed yet.** Until then the host names the axes and owns the values, and in the shell it moves nothing but the Specimen, marked MOCK.

**Anatomy** the screen's ground, a veil of quarter lines at 18% `scr-grid`, a 6 px puck in `scr-value`, and the axis names in the corners. The underlay canvas sits beneath it.

**Gestures** a press lands the puck and a drag follows. Arrow keys move it in keyboard mode only (ADR-259 r6).

**Sources** GUI-2, the bench's `paintXY`, `docs/design/main-layout-lab.html`.
