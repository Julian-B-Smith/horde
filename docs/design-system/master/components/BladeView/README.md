# Blade view

One cycle of one voice with the blades that cut it: each window as a band, the base wave behind, what is heard on top. You drag it to move the blades (GUI-30).

`Horde.BladeView(el, { height, label, hint, roles, keyOf, read, circ, write, begin, end, label, value, keys })`, then `frame(cycle)` each frame. `Viz.blade(canvas, cycle)` is the painter alone.

**Three views (the pills).** The lab's, one choice for every instance, remembered per machine and never with a patch.
- **Sum:** every member over its own phase, summed: the swarm as if it were locked. The bands are below.
- **Members:** one lane per member, each over its own phase, with that member's windows.
- **At phase:** each member shifted by its offset from the mean field, so the swarm lines up as it does now. Short ticks on the top edge mark where each member's cycle starts. Pointing at a line lifts that member: 1.8 px at full ink, its windows edged, its offset named. At lock this view draws Sum's picture.

**The drag (B293, B294, B297, B299).** Relative: a grab moves nothing. It moves the blade whose window is under the pointer, or every blade that's on from empty space, and nothing when none is on. By default horizontal is Position, vertical is Cut rate, up for more, and shift and horizontal is Width. Alt is a quarter as far. A circular row crosses its range in one canvas width, so the window follows the hand; any other crosses its range in 420 px. A gesture is one undo step, and a grab with no motion writes nothing. The target is outlined in `scr-marker`, dashed on hover and solid in a drag, with a readout of what's moving.

**The mapping (the legend pill).** Three slots, each a control, an optional second control and an invert for the second: the lab's axis row. Its roles are the blade's, the swarm's and the blades' interplay. One mapping for every instance, remembered per machine, and Default puts it back.

**Looks** one cycle across the width, quarter gridlines and a centre line in `scr-grid` 18%. **What a band means:** each blade draws TWO bands from the same window positions the render used: the solid band is the voice's NOMINAL window (the blade's own position and width), and a lighter band with dashed edges is the members' full EXTENT, from the earliest member's window start to the latest member's end (members spread their windows). The sum can depart from the base wave only inside the extent band, and every departure lies inside some member's window: checked, with a must-fail plant (bands drawn at the nominal window alone). Blade 1 fills in `scr-value` (22% nominal, 10% extent); blade 2 is hatched in the same ink. A window that wraps the cycle is drawn in two parts. The base wave dashed in `scr-grid` 55%; each member faint in `scr-physics` 28%; the heard sum in `scr-meter` 1.8 px, with a ground-coloured halo on the pale schemes.

**Fed by** RC-8: each blade's window (position, width) and the frame angle; the drawn cycle is rendered from the same render the voice plays (L0064: views read what was rendered). RC-8 has no member's cycle, so that's a request (GN-41). Until then `Synth.cycle` draws the settings as a SAMPLE, and `Synth.atPhase` shifts it.

**Voice-bound or audio-capable** Voice-bound.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Cost** blade 2's hatch is one tile made once per ink and pixel ratio, and filled (GUI-33): 16 members in At phase measured about 1 ms a frame, down from 20 ms with a stroke per stripe.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** SCALPEL lab `paintCycle`, `drawWindow`, `phaseModel`, `phaseTicks`, `xyWire`, `xyOverlay` and `xyMapRow` (B293, B294, B297, B299).

**OFF** `Viz.off(canvas, off)` over it while the oscillator is off (ADR-105 A2). The drag still writes: an off source shows its controls (GUI-29).

**Not copied** the lab's Contributions pill, Cycle Edits grips and the R-hued beam under it.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
