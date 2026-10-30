# Corner chip

The morph corner's identity: one round chip carrying the corner's colour AND its glyph, always (ADR-259 ruling 3).

**The corners** A ◆ diamond on `cA`, B ▲ triangle on `cB`, C ● circle on `cC`, D ■ square on `cD`. Colours never re-theme. The glyph is drawn as a shape (SVG), not a font character, so it renders the same on macOS and Windows.

**Anatomy** a circle `size-chip` (20), never stretched (one chip component, B361). Idle: a `stroke-focus` (2.5) ring in the corner colour, glyph in `text`. Selected (the corner being edited): filled with the corner colour, glyph in `on-corner`, plus a 1.5 px outer ink ring, so selection reads without colour. The hit box is 28 px; chips in a row sit 28 px apart or more.

**The name** is always the ACTUAL name of the corner preset the corner holds. There are no placeholder or invented corner names anywhere in the system; previews use real presets from the SCALPEL bench, marked as samples. A name truncates with an ellipsis past `14ch` (Archivo `label`, 10 px); the full name is the tooltip and the accessible name. A corner holding no preset reads **Empty** in `hint` italic, its ring dashed, its lock and revert disabled.

**Edited, lock and revert (per corner, independent)**
- **Edited mark**: a 7 px `marker` dot after the name when the corner's current values differ from its saved preset.
- **Lock** (a padlock button, 28 px): protects the corner from edits. Any write that would land on a locked corner (a knob or slider drag, a reset, alt+wheel, arrows, a corner-level depth) is refused, and the control says why: its readout shows the padlock and "C locked" and the footer names the corner. A locked corner still takes part in the morph.
- **Revert** (a return arrow, 28 px): returns that one corner to its saved preset; enabled only when the corner is edited and unlocked.
The same three sit on each corner row in the knob inspector.

**The consumer provides** the four corner presets' names (or empty), their saved values, the lock state, the editing state (Auto or pinned), and the revert action.

**The census bar** an 18 px capsule with ink edge and 1.5 px ink dividers; each corner's segment is its share, filled with its colour, its glyph in `on-corner`, and the share number when it is 10% or more.

**Pips** on a knob or slider are the same four glyph shapes, drawn small in the corner's fill (ADR-261 ruling 1), so a chip and a pip of one corner always match in shape.

**Inside a screen** the chassis carries the corner colours; the glass shows a corner only as its glyph or the letters A–D in the glass's own ink (ADR-259 ruling 4). Never paint a corner colour inside a glass.

**Text in a corner colour** uses the `-tx` tokens on `panel` or `well`, never the fills.
