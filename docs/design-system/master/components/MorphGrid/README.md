# Morph grid

Four corners by every member of the morph field: what each corner holds, row by row, and what you are hearing at the puck; every cell is edited live.

**The consumer provides** the session and `opts.rows`: `{ key, name, path?, section, group?, kind: 'c' | 's' | 'g', vals[A..D], labels? (structural, the manifest's labels exactly), fmt? (continuous), def, unit? }`, in field order; `opts.title`, `opts.maxHeight` (the body scrolls). The corners' preset names are the session's (`S.corners`, the real names; an empty corner reads "Empty"). Ownership is the morph layer's readout (the stand-in `H.Morph.engine` here); the grid never computes it.

**Anatomy**
- A bar: the title (`hd-cluster`), the count, UNDO and REDO (28 px icon buttons).
- Head: PARAMETER; each corner as its chip (glyph on its fill), letter, preset name (ellipsis) and weight at the puck, with the padlock when the corner is locked; HEARING in `physics`.
- Section rows (`well-alt`, an `ink` rule): a 28 px fold button with the section name in caps, and "n members · d differ · showing s". Group captions in mono caps, `hint`.
- A row: the KIND chip (a glyph in `ink`: a rise SLIDES, a staircase SNAPS, a ramp with a deferred tick RAMPS; never a colour), the name (Archivo 10.5, ellipsis) over its path (mono 8.5, `hint`), a UNIT tag when it flips with others, the four cells, HEARING.
- Cells by kind: SLIDES a 26 × 6 bar (`ink` fill on `well-alt`) and the value; SNAPS a capsule with the label; RAMPS an ON/OFF switch (ON is `ink` with `bg` text).
- Ownership: the owning cell carries a 3 px stripe in its corner's line ink (`cA-tx`…); under a blend or a ramp each cell instead has a 2 px underline in that ink, its length the corner's weight.
- HEARING: a bar in `physics`, the value in `physics` (`ink` on a selected row), and "blend", the level, or the owner's glyph and letter.
- Selection: the selected row on `sel-neutral`; the picked cell on its corner's selected fill (`sel-cA`…) with its edge. Rows whose four corners agree are dimmed (`t2`, bars at `opacity-muted`) and hidden in the default view.

**Gestures** (ADR-266 r9: edits are live, latched for the gesture, undo covers mistakes)
- Drag a cell: right or up raises; `size-drag` (160 px) is the full range (a structural cell steps through its labels over the same 160 px); shift ×0.25; never a jump to the click. The value is written while you drag; the whole drag is one undo entry.
- Double-click a cell: reset to the default. Option + wheel: always sets the value (a spin is one entry). Arrows: keyboard mode only.
- Click a gate's switch: toggles that corner (one entry).
- A write to a locked corner is refused and the session says why (ADR-262 r4).
- Click a name: select the row. Click a section: fold it. UNDO / REDO, or Cmd/Ctrl+Z and Shift+Cmd/Ctrl+Z in keyboard mode.

**Constant box** fixed columns (the HEARING column is a fixed fraction); one line per readout with ellipsis; the body never rebuilds under a pressed pointer, so a row that stops differing mid-drag stays until release.

**Fits** under 560 px of container width (a container query) the name and HEARING share the first line and the four corners take the second.

**Placeholders**
- Undo. A local history on the session stands in for the host's undo (B84: one node per gesture). Decided by: the plugin's undo integration.
- EXEMPT (ADR-109) and NOT IN THE FIELD rows. The lab draws both; they are not in this brief, so the grid has no exempt band or absent rows yet.

**Sources** `docs/design/morph-editor-lab.html`: grid CSS :278-386; kinds :1945-1958; the grid :1960-2097; the staged edit it replaces :3598-3815 (ADR-266 r9 wins: live, no COMMIT). ADR-262 r3-r4.

The preview draws SAMPLE data generated in the preview (corner values set by hand, extra rows seeded), never the engine.
