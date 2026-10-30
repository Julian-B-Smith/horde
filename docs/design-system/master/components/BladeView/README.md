# Blade view

One cycle of one voice with the blades that cut it: each window as a band, the base wave behind, what is heard on top.

**Looks** one cycle across the width, quarter gridlines and a centre line in `scr-grid` 18%. **What a band means:** each blade draws TWO bands from the same window positions the render used: the solid band is the voice's NOMINAL window (the blade's own position and width), and a lighter band with dashed edges is the members' full EXTENT, from the earliest member's window start to the latest member's end (members spread their windows). The sum can depart from the base wave only inside the extent band, and every departure lies inside some member's window: checked, with a must-fail plant (bands drawn at the nominal window alone). Blade 1 fills in `scr-value` (22% nominal, 10% extent); blade 2 is hatched in the same ink. A window that wraps the cycle is drawn in two parts. The base wave dashed in `scr-grid` 55%; each member faint in `scr-physics` 28%; the heard sum in `scr-meter` 1.8 px, with a ground-coloured halo on the pale schemes.

**Fed by** RC-8: each blade's window (position, width) and the frame angle; the drawn cycle is rendered from the same render the voice plays (L0064: views read what was rendered).

**Voice-bound or audio-capable** Voice-bound.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** SCALPEL lab `paintCycle` and `drawWindow` (B293, B297, B299).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
