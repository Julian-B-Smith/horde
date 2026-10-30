# Slider

The morph slider: the same model as the Knob laid on a straight track, horizontal (readouts to the right) or vertical (readouts below).

**The consumer provides** exactly what the Knob takes: a manifest `key`, the accessible `path`, the four corner values or `locked`, the engine's owning corner, its mappings and live sources, and `bipolar`.

**Sizes** (B589, the Density ladder). With `size` (`compact`, `standard` or `primary`; `utility` draws as compact) the slider uses the TIGHT drawing: 200 × 35 (vertical 40 × 200), the track at 13, the tether 10 units off it, four mod lanes 3.2 apart from 10 units off on the other side (1.9 wide), no corner-depth hairline, the "+n" as an 8 px HTML badge. A horizontal row is label (`size-label-dense`, 52 px; 64 px at standard and primary) | face (at most 200 px wide, so one unit stays one pixel and the thumb never scales up) | the value only, 8ch, right-aligned. The corner line and the full focus caption go to the section's readout slot, as on the sized knob; under Option or MAP the value column shows the short caption in the source colour. A vertical sized slider is the channel strip's fader: the face is `size-fader-h` (140 px, or `--fader-h`) tall, with a wider 22 × 8 cap; name above, value below (9ch). Leaving `size` out keeps the classic drawing below, unchanged.

**A constant box.** Nothing live changes the slider's size: the drawing is a fixed 200 × 62 (vertical 76 × 200) that reserves the tether side and four lanes with their hairlines and the "+n" label; the readout column is a fixed `19ch` of tabular figures, each line one line high with its height reserved when empty, overflowing with an ellipsis (the full text is the accessible value). Tested: through all four corners, the centre, Auto and each pin, no slider, knob or card moves by 0.5 px.

**Anatomy** (a 200 × 62 drawing, track 180 units at y 14; the vertical one 76 × 200)
- Track `well-alt`, 4 units; a neutral `ink` fill from the minimum (or the centre notch, for bipolar) to the blend; a 10-unit `ink` notch across the track at the blend.
- **The thumb is the editing corner**: a 7 × 16 rounded rectangle in that corner's fill colour with a `mark-edge` outline, at the corner's stored value; `ink` when locked.
- Corner pips on the centreline, shape-coded by glyph (A ◆, B ▲, C ●, D ■) and sized and faded by weight as on the knob; the editing corner has none (the thumb replaces it).
- Tether 9.5 units off the track on the side away from the mod lanes (above a horizontal slider, left of a vertical one), dashed, the editing corner's line ink, from the notch to the thumb.
- Mod lanes on the other side, one colour per source kind (same-kind sources told apart by the inspector and the focus caption): lane 1 at 11 units, then 5.4 apart (8.4 for a corner-level lane, whose depth hairline sits 3.4 outside it); stacked origins, origin dot and live tick as on the knob; four lanes, then "+n" past the maximum end.
- An ownership flip: one halo off the thumb, 500 ms.

**Control** works as on the Knob (ADR-266 ruling 2): the own value (fill, pips, tether, thumb) fades to a ghost; the Control lane is lane 1, from low to high with the live tick; a source-coloured dot on the track marks the controlled position; the readout says "controlled by LFO 1"; own-value gestures are refused and said; Option-drag slides the Control's range (a placeholder, as on the Knob).

**Gestures** drag along the slider's axis is relative from anywhere, never jump-to-click (a jump would write a corner value nobody aimed for); one slider length is the full range, shift ×0.25. Option-drag edits the focused mapping's depth along the same axis (choose it as on the Knob: Option-click or an inspector row; hold Option to see the caption); double-click resets; arrows in keyboard mode; alt+wheel only, and always the value, never the focus. The hit area is the whole 62-unit-deep drawing (at least 28 px).
