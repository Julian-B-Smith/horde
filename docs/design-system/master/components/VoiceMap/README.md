# Voice map

Where each member of the swarm sits in pan and detune, and how far it has wandered from where it was placed.

**Looks** Pan across, detune in cents up and down, with dashed rails at ±24 ct (`scr-grid` 40%) and centre lines at 25%. A hollow ring (`scr-physics`, 1.5 px, r 5) where the member is placed; the filled dot (`scr-value`, r 4.5, bloom r 8 at 30%) where it is now; a thin line between them. The gap IS drift, glide and pull. The root member wears a `scr-marker` star. The zoom steps out when members leave the rails and back in with hysteresis.

**Fed by** The members' pan and detune (placed and actual). NOT in RC-8 today: the snapshot carries phases and levels, not per-member pan or detune. OPEN: add them, or derive from the parameters.

**Voice-bound or audio-capable** Voice-bound.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** UI Spec §6 VOICE MAP; gui2 `drawVmap`; SCALPEL lab `paintVmap`.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
