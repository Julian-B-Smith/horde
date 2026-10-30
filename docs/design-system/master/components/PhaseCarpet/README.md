# Phase carpet

Every member's phase over the last moments, one row (or column) per member, so locking reads as the dots lining up.

**Looks** two orientations of one carpet, shown together (switchable):
- **Horizontal** (the SCALPEL lab's port, B295): each row is one voice member; its dot's position along the row is its phase.
- **Vertical**: the same carpet turned 90°, short and wide (about 3:1, filling the width it is given). Each column is one voice member; its dot's position along the column (0 at the bottom) is its phase.
Both: a trail of the last 16 frames at 0.75^age (2 px dots, the newest 2.6 px); a member inside a blade window lights in `scr-value`, elsewhere `scr-physics`; guides in `scr-grid` 18%. Locking reads as the dots lining up. OPEN: which orientation GUI 3 uses where (the UI Spec's original stepped-hue carpet is not built).

**Fed by** RC-8: the members' phases per voice and the blade windows. The trail is counted in snapshot frames, so its length is time-stable.

**Voice-bound or audio-capable** Voice-bound.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** SCALPEL lab `paintCarpet` (B295, from gui2 `paintCarpet`); UI Spec §6 PHASE CARPET.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
