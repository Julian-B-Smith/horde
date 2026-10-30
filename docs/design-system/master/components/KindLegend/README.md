# Kind legend

What a morph row can be: SLIDES, SNAPS and RAMPS, each with its glyph, one sentence for the current mode, and a real row swept from corner A to corner D.

**The consumer provides** the session and `opts.examples`, one row key per kind (`{ c, s, g }`), each a real member of the field.

**Anatomy** a three-row grid: the kind chip (`ink` glyph and word in a capsule; a kind is a behaviour, never a colour); the sentence (Archivo 10, `t2`, the kind's name in `ink`, the example row named in italics), its box reserved for the longest sentence so a mode change never moves the rows; a small glass (aspect 3.2:1) with the example swept along the A → D diagonal in `scr-physics` (a 6 px underlay at 22% under a 1.8 px core) and A and D in `scr-grid`. Under QUANTUM the continuous row's sweep steps like the structural one.

**Gestures** none.

**Fits** under 440 px the sentence takes its own line above the chip and the glass.

**Sources** `docs/design/morph-editor-lab.html`: the pane :666-673; legend CSS :260-276; `paintSweep` and `buildLegend` :2958-2988. The lab's EXEMPT, ABSENT and HELD rows are left out with the grid features they explain.

The preview draws SAMPLE data generated in the preview, never the engine.
