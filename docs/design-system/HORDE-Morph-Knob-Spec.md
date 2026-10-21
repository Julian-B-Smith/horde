# HORDE Morph Knob Spec

Oct 21, 2026 · @Julian Beall Smith

## Summary

When the morph pad sits between corners, an unlocked parameter's knob has two values: the blended value you hear, and the value stored in the corner you are editing. The recommendation is to show both on the same knob. The neutral value arc shows the blend. Colored pips on the track show each corner's own value, sized by that corner's weight. The editing corner's pip is emphasized and joined to the blended value by a dashed tether in its color.

Dragging edits the editing corner's value directly: the pip follows the pointer one to one, and the blend moves by that corner's weight. The same treatment applies to mod depth through a hairline outside the mod ring.

At a pure corner position the extra marks disappear and the knob looks like an ordinary knob, so the added detail appears only when the knob really is in between.

## Model

Each unlocked parameter stores four corner values, v\_A to v\_D. The pad position (x, y), with y measured downward, gives bilinear weights that sum to 1:

| Corner | Position | Weight |
| --- | --- | --- |
| A | top left | (1 − x)(1 − y) |
| B | top right | x(1 − y) |
| C | bottom left | (1 − x)y |
| D | bottom right | xy |

The blended value is the sum of w\_i × v\_i. If HORDE's morph uses a different blend law, keep that law; the visuals below depend only on having a per-corner weight and a blended value.

- **Editing corner.** The one corner whose stored value a gesture changes. By default it is the parameter's owning corner, as decided by the morph engine. The morph uses stochastic boundaries, with a threshold of its own for each parameter, in both continuous blend and quantum morph modes. Knobs on the same page can therefore be owned by different corners at the same pad position, and flip at different moments. The user can pin a specific corner instead, which overrides ownership for every parameter.
- **Ties.** The UI reads ownership from the engine and never computes it, so a knob always shows the corner that is really in control. Ties are the engine's to resolve.
- **Locked parameters.** One global value, used at every pad position. They have no corners and no editing corner.

## Knob anatomy

Radii are in the prototype's 92-unit viewBox, with the track at radius 22. Marks are listed from the center outward.

| Mark | Radius | Shows | Style |
| --- | --- | --- | --- |
| Center cap rim | 12.5 | Which corner owns edits | Editing corner's color, 1.6 stroke; neutral 1.0 when locked |
| Needle | 0 to 10.5 | Output after modulation | Neutral, 2.0 stroke |
| Tether | 16.5 | Distance from the blend to the editing corner's value | Editing corner's color, 1.6 stroke, dashed 2 / 2.4; hidden when the two coincide |
| Value arc | 22 | Blended value, 0 to blend | Neutral, 4.0 stroke, on the grey track |
| Corner pips | 22 | Each non-editing corner's own value | Corner color, radius 1.3 + 2.3w, opacity 0.28 + 0.72w, dark 1.1 outline |
| Editing pip | 22 | Editing corner's own value | Radius 3.9, full opacity, light 1.4 outline |
| Editing flag | 25.2 to 27.6 | Points out the editing pip at a glance | Short radial line in the editing corner's color |
| Mod ring | 31 | Blended mod depth | Per the map mode spec: source color, origin dot, live tick |
| Corner depth hairline | 34.2 | Editing corner's own mod depth | Editing corner's color, 1.1 stroke |

The weight-scaled pips are what make the in-between state readable. A corner with almost no weight shrinks to a faint dot, so the strongest influences stand out. At a pure corner position the other three pips have zero weight and disappear. The tether then has zero length, and the knob reduces to value arc, one pip and needle.

**Readouts** under the knob:

- Line 1 is the blended value in the parameter's units.
- Line 2, in the editing corner's color, is the corner letter, its stored value, and its weight, for example "B 72% · 41%". The weight is omitted at a pure corner. Locked parameters show LOCKED in neutral grey.

**Composition style (alternative, not recommended as default).** The value arc is split into consecutive segments, one per corner, each w\_i × v\_i long and in that corner's color. The editing corner's segment is at full opacity and the others at 42%. The segments sum exactly to the blend, and a neutral tick marks their end. Only the editing pip is drawn. This style shows how the blend is made up, but on a page of knobs it reads as busy. It may suit a single focused knob, such as the inspector's enlarged view.

## Editing

**Drag edits the corner, not the blend.** A vertical drag changes the editing corner's stored value one to one, using the same 160 px full range and Shift for fine control as the map mode spec. The editing pip follows the pointer, and the blended value moves by w × Δ. The value under the pointer is therefore always the number being stored.

Do not back-solve, that is, move the blend to the pointer and derive the corner's value. At low weights that drives the corner value to its limits, and at a weight of 0 it is undefined.

**Latching.** The editing corner is fixed at pointer-down and held until pointer-up. This holds even if Drift, inertia or modulation moves the pad during the gesture and changes which corner is dominant. Without the latch, one drag could write to two corners.

**Ownership change cue.** When a parameter's owner flips in Auto, it gives one expanding halo in the new corner's color, fading over 500 ms. On a knob the halo comes off the cap rim; on a slider it comes off the thumb. Across a page these halos show which parameters crossed their threshold and when. No cue appears while a corner is pinned, because the editing corner does not change. Under stochastic boundaries latching matters more: a drag that starts just before a flip keeps writing to the corner that owned the parameter at pointer-down.

**Choosing the editing corner.**

| Control | Action |
| --- | --- |
| Editing corner selector (Auto, A, B, C, D) | Auto follows each parameter's owning corner, so different knobs show different colors. The Auto button shows no swatch, because there is no single answer. |
| Click a corner badge on the pad | Pins that corner; clicking it again returns to Auto |
| Click a row in the inspector's corner table | Pins that corner |
| Keys 1–4, 0 or Esc | Pin A–D; 0 or Esc returns to Auto. Only while MAP is off; while MAP is on, digits select sources and Esc exits map mode. |

The pin applies to all parameters at once, not per parameter. Its purpose is "I'm working on corner B right now". A pinned corner shows a light outline on its badge on the pad.

**Inspector.** For the selected parameter, a table lists each corner with its letter swatch, a weight bar, its stored value and, if mapped, its mod depth. A final MIX row shows the blended value and depth. The editing corner's row is outlined. A Lock checkbox switches the parameter to a single global value. Locking keeps the current blended value as the global value. Unlocking copies the global value into all four corners.

## Mod depth, locked parameters, quantum mode

**Per-corner mod depth.** This applies to corner-level mods, the tier inside the morph. Each corner stores its own depth, and the blended depth is the sum of w\_i × d\_i.

- **Mod ring.** Draws the blended depth, using the stacked-origin, origin-dot and live-tick rules from the map mode spec. It sits on the blended base value.
- **Hairline.** Drawn at radius 34.2 in the editing corner's color, it covers that corner's own depth around the same origin.
- **Editing.** Option-drag edits the editing corner's depth one to one and moves the hairline. The same latching rule applies.
- **Global mods.** Mods above the morph have no hairline, because they have nothing per corner to show.

**Locked parameters.** No pips, tether or hairline are drawn. The cap rim is neutral, line 2 of the readout says LOCKED, and dragging edits the global value. Parameters that must be global, such as intent and morph targets, are always drawn this way and offer no Lock checkbox.

**Quantum mode (to be confirmed by prototype).** In quantum mode, each parameter resolves stochastically to one corner's value at a time instead of blending. The expected mapping onto this design:

- The value arc always lands exactly on one corner's pip.
- The editing corner in Auto is the corner currently resolved for that parameter, not the dominant corner by weight.
- Pip size still follows weight, since weight is the flip probability.
- Latching matters more here, because flips can happen mid-drag. The gesture keeps writing to the corner that was resolved at pointer-down.

Risk: in Auto, the editing corner may visibly hop between corners while the user only hovers. Consider freezing a parameter's flips while it is hovered or selected.

## Sliders

On a slider, the thumb is the editing corner. It is drawn in the editing corner's color at that corner's stored value, so the thing under the pointer is the value being written, as with the knob's editing pip. Everything else maps directly onto the straight track.

| Knob element | Slider equivalent |
| --- | --- |
| Value arc | Neutral fill from the minimum (or the center, for bipolar parameters) to the blended value |
| Blend end | A neutral notch across the track, 10 units long, at the blended value |
| Corner pips | On the track centerline, with the same weight-scaled size and opacity. The editing corner has no pip, because the thumb replaces it. |
| Editing pip and flag | The thumb: a 7 × 16 rounded rectangle in the editing corner's color with a dark outline. It is neutral when the parameter is locked. |
| Center cap rim | None needed, because the thumb's color carries it |
| Tether | Dashed line in the editing corner's color, 9.5 units off the track on the side away from the mod lanes (above a horizontal slider, left of a vertical one), from the blend notch to the thumb |
| Corner depth hairline | A 1.1 line in the editing corner's color, 3.4 units outside the corner-level mod lane it belongs to. That lane takes 8.4 units of spacing instead of 5.4 to fit it. |

At a pure corner, the other pips vanish and the tether has zero length, so the notch sits under the thumb and the slider reads as an ordinary slider.

**Gestures.** Drag along the slider's axis edits the latched editing corner one to one. One slider length is the full range, with Shift for fine control. Option-drag edits that corner's mod depth on a corner-level lane. Drags are relative from anywhere on the slider, never jump-to-click.

**Readouts** for horizontal sliders sit to the right, and for vertical sliders below: the output value, then the corner line in the editing corner's color.

**Reference:** [HORDE Slider Marks](https://claude.ai/artifact/H13BmEV6BF5Gh3YYMZ5Qf8) prototype, which has both systems on horizontal and vertical sliders.

## Open decisions and reference

| Decision | Options | Recommendation |
| --- | --- | --- |
| Corner colors | Prototype placeholders; HORDE's existing corner palette | Use HORDE's existing palette. Check it against the mod source colors so a corner and a source never look alike on the same knob. |
| Default arc style | Pips; composition | Pips on the page, with composition offered in the inspector's enlarged knob |
| Scope of the pin | Global; per parameter | Global |
| Auto in quantum mode | Follows the resolved corner; follows the dominant corner | The resolved corner, with flips frozen while the parameter is hovered or selected (prototype first) |
| Interaction with map mode | Map mode writes to the editing corner; map mode always writes global | Global for 1.0, as in the map mode spec. Corner-level mapping later, through a scope switch. |
| Edit-all-corners gesture | None; a modifier that shifts all four corner values by the same amount | Worth adding later, for example Cmd-drag. It is not needed for the visuals. |

**Reference prototype:** [HORDE Morph Knobs](https://claude.ai/artifact/3TrPqjTBQjhMH9QsxkcT1z) (browser). It is a behavior and visual reference, not a parity oracle. Its bilinear weights and single LFO are illustrative. Its companion is the [HORDE Map Mode Spec](https://claude.ai/code/artifact/7e9eac13-7aa7-4afe-8269-f249f55ba2de), which defines the mod-ring rules used here.
