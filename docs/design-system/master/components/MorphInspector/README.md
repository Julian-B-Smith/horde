# Morph inspector

The selected morph row in full: what kind it is and what that means in this mode, the four corners with their preset names and live controls, whether an edit to the selected corner is heard at the puck, and the row's quantum regions and lock.

**The consumer provides** the session (selected row `S.selected`, selected corner `S.morph.selCorner`, corner names and locks). Rows as for the morph grid. Ownership and region shares come from the morph layer's readout (the stand-in here).

**Anatomy**
- Heading: section · group (`hd-label`), the name (Archivo 800, 15), the range, default and unit in `hint`.
- The kind chip and one sentence that changes with the mode (SLIDES: blends, flips then glides, or blends in steps; SNAPS; RAMPS).
- Four corner rows, 28 px: the corner chip, the letter and the real preset name (ellipsis), the control (a continuous corner a house slider face; a structural corner a select with the manifest's labels; a gate a toggle), the value in a fixed 9ch column, and a fixed 46 px column for OWNS (or the corner's weight under a blend) and the padlock. The selected corner's row is on its corner's selected fill (`sel-cA`…).
- The IMPACT box (`well`, `ink` edge, a fixed three-line box): "Editing corner B." then whether that corner is heard here (a share of a blend, the owner, or "silent until B wins it"; or that the corner is locked), and what is heard, in `physics`.
- Actions: SET ALL FOUR (the selected corner's value into all four; refused whole if any corner it would change is locked), UNDO.
- QUANTUM BOUNDARIES (not for a gate): each corner's share of the pad (glyph, letter, a bar in its fill, the percentage), the lock state in words, LOCK AS IS / UNLOCK.

**Gestures** as the morph grid: live, latched, one undo entry per gesture; relative drag at 160 px, shift fine; double-click resets; Option + wheel sets; arrows in keyboard mode. A click on a corner row selects that corner.

**Placeholders**
- The staged edit. The lab staged edits behind COMMIT and DISCARD; ADR-266 r9 rules edits live, so there is no staged state and the impact box speaks before and during the edit, not as a preview of a commit.
- EXEMPT, RETURN TO FIELD and the waypoint section are left out (not in this brief, and waypoints are not ruled).

**Sources** `docs/design/morph-editor-lab.html`: inspector CSS :395-415; `impactText`, `ctlHTML`, `renderInspector` :3598-3681; the quantum half :3711-3741.

The preview draws SAMPLE data generated in the preview, never the engine.
