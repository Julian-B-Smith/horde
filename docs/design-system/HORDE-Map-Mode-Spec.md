# HORDE Map Mode Spec

Oct 16, 2026 · @Julian Beall Smith

## Summary

Recommendation: give HORDE a persistent MAP control (a toggle button plus a source selector) that is visible on every page. While MAP is on, clicking any parameter assigns the selected source to it, and Option-dragging sets the depth; a plain drag still sets the value (amended by ADR-340 ruling 7, MD1, 2026-10-31: the human found a plain drag that changed the scale confusing). While MAP is off, Option-drag edits an existing mapping's depth and Option-click switches which mapping is being edited.

The goal is to make the common case (assigning one source to several parameters across pages) a run of single clicks, with no mod-matrix row to fill in for each assignment. The mod matrix stays as the full view; map mode is a faster way to write into it.

Knob rings show each mapping's range stacked on the live output of the rings inside it. A dot marks every mapping's origin (an end for unipolar and inverse, the center for bipolar), and a tick marks each ring's current position. This was validated in a browser prototype (linked under Reference prototype).

## Data model

A mapping is one record: `{ param, source, depth, polarity }`. Depth is a magnitude from 0 to 1 in normalized parameter units. Polarity carries the sign and shape, so depth never goes negative.

Each source produces a raw value s in \[0, 1\]. A mapping contributes:

| Polarity | Contribution | Range around the base |
| --- | --- | --- |
| Unipolar | s × depth | base up to base + depth |
| Bipolar | (2s − 1) × depth | base − depth to base + depth |
| Inverse | −s × depth | base − depth up to base |

The modulated value is clamp(base + sum of all contributions, 0, 1). Clamp once after summing, not per mapping, so that opposing mappings can cancel near the edges.

Mappings on one parameter are an ordered list. The order does not affect the sound (the sum is commutative), but it sets ring order on the knob (see Visual language).

Integration with the existing tiers: a mapping created in map mode is global by default. A mapping whose target is a morph or intent-bus parameter must be global, as already decided. If corner-level mods are offered here, they need an explicit scope switch in the MAP control rather than an inferred one (see Open decisions).

New mapping defaults: polarity unipolar; depth 25% on a plain click, or whatever the Option-drag sets (ADR-340 r7).

## MAP control

The MAP control sits in the top bar, outside the page area, so it is identical on every page. It is a toggle button joined to a source dropdown.

- **Source color.** Each source has a fixed color. The button's lit state, the dropdown, and the panel outline all take the current source's color while MAP is on.
- **Entering and leaving.** Clicking the button toggles MAP. The M key does too: a tap (released within 280 ms) latches MAP on, and a hold turns it on only until the key is released. A hold shows a small HOLD label on the button. M while MAP is latched turns it off. Esc always turns it off.
- **Switching source.** Use the dropdown, a click on a source chip in the live source strip, or the number keys 1–9 in source-list order (only while MAP is on; with MAP off, digits pin morph corners per the morph knob spec). The switch takes effect immediately, including in the middle of a mapping session.
- **Page changes.** Switching pages keeps MAP on and keeps the current source.
- **Live source strip (recommended).** A row of small live meters, one per source, under the top bar. It confirms what each source is doing, doubles as a source picker, and shows the color key.

## Interactions

Pointer down on a knob always selects it in the inspector. What happens next depends on the mode. Vertical drag uses 160 px for the full range, and Shift gives fine control at one quarter of that.

**MAP on**

| Gesture | Result |
| --- | --- |
| Click a parameter with no mapping from the current source | Creates the mapping at 25% depth, unipolar |
| Drag (no modifier) | Sets the base value, exactly as with MAP off (ADR-340 r7) |
| Option-drag a parameter with no mapping from the current source | Creates the mapping at 0 and sets depth by the drag. Releasing at exactly 0 cancels the mapping. |
| Click or Option-drag a parameter already mapped to the current source | Click selects it only; Option-drag edits that mapping's depth |
| P | Cycles polarity of the last-touched mapping: unipolar, bipolar, inverse |
| Backspace / Delete | Removes the last-touched mapping |

~~The base value cannot be changed while MAP is on. A drag in map mode never moves the knob itself.~~ Struck by ADR-340 ruling 7 (MD1): with MAP on a plain drag sets the base value; only an Option-drag sets depth.

**MAP off**

| Gesture | Result |
| --- | --- |
| Drag | Sets the base value |
| Double-click | Resets to default |
| Option-drag | Sets depth of the parameter's focused mapping |
| Option-click (no movement) | Moves focus to the next mapping on that parameter, wrapping around |
| Option-P | Cycles polarity of the selected parameter's focused mapping |
| Option-click on an unmapped parameter | Does nothing; the footer says it has no modulators |

**Focus.** Each parameter remembers one focused mapping. A newly created mapping takes focus (prototype toggle, on by default). Holding Option shows the focused source, polarity, depth and position (for example "LFO 2 BI 30% (1/2)") under every mapped knob. Option-P is deliberately scoped to the focused mapping, never to a last-touched mapping that might sit on another page.

**Mouse equivalents.** Every keyboard action needs a mouse route, because plugin windows often don't get keyboard focus (see Host constraints). The inspector provides them: a polarity pill to cycle, a remove button, and a click on a row to focus it.

## Visual language

A knob has a 270° sweep. Moving outward from the center, it draws:

1. **Value arc.** Neutral color, on the main track, from the minimum to the base value. Bipolar parameters (for example coupling K) draw it from center to base.
2. **Needle and output dot.** Point at the summed, clamped output. The dot's outline takes the focused mapping's color.
3. **Mod rings.** One concentric ring per mapping, in the source's color, about 3.7 units apart outside the track.

**Stacked origins.** Each ring's origin is the live output of everything inside it, not the base value. Ring 1 starts at base. Ring 2 starts at base + ring 1's current contribution, and so on. The rings therefore move with the modulators beneath them, and the outermost ring's tick always lands where the needle points.

**Ring marks.** These are the only two marks on a ring, and each means one thing:

- **Origin dot.** Every ring has a dot in the source's color, with a dark outline, at its origin. Unipolar and inverse rings have the dot at one end. Unipolar runs upward from it and inverse runs downward. Bipolar rings have the dot at the center of the arc, which is where their origin is. The dot's position on the arc gives the polarity at a glance.
- **Live tick.** A short neutral radial tick at the ring's current output.

**Emphasis.**

- MAP off with no modifier: all rings at full opacity.
- MAP on: rings from the current source are at full opacity and the rest drop to 30%. The focused ring also stays at full opacity.
- Option held: the focused ring is drawn heavier (3.2 vs 2.2 stroke) and the caption appears.

**Limits.** Show at most four rings per knob. Further mappings show as "+n" and stay editable in the inspector.

**Map-mode chrome.** While MAP is on, the panel border glows in the source color, and the cursor over knobs changes to a copy cursor. Knob captions show the current source's mapping when one exists.

**Value readout.** Mapped knobs show the modulated value live. Unmapped knobs show the base value.

## Inspector, host constraints, edge cases

**Inspector.** A rail beside the page. It shows the selected parameter and one row per mapping, in ring order. Each row has a color dot, the source name, a polarity pill (click to cycle), the depth as a percentage, and a remove button. The focused row is outlined in its source color, and clicking a row focuses it. When the parameter has no mappings, the rail says how to add one using the current source.

**Host constraints.**

- **Keyboard focus.** Hosts (Ableton in particular) often keep keystrokes for themselves unless the plugin window has focus. Treat every hotkey as an accelerator, never the only route. Test M, P, number keys and Backspace inside Live, with the plugin window both focused and unfocused.
- **Modifier on Windows.** Option maps to Alt. Check that no target host intercepts Alt-click on plugin windows; if one does, add a setting to switch the modifier to Ctrl.
- **Automation.** Mapping changes are UI and preset state, not automatable parameters. Mod depth is a candidate for automation later, but it is out of scope here.
- **Undo.** Creating, removing, and changing depth or polarity each go on the plugin's undo stack as one step per gesture, not one per drag frame.

**Edge cases.**

| Case | Behavior |
| --- | --- |
| Drag releases at 0 on a new mapping | Mapping is not created |
| Window loses focus while M or Option is held | Momentary MAP ends; Option state clears |
| Last-touched mapping is removed | P and Backspace do nothing until another mapping is touched |
| Focused mapping is removed | Focus moves to the previous mapping in the list |
| Rings and output past 0 or 1 | Arcs and ticks clamp to the sweep; the sum is clamped once |
| Touch input (no Option key) | Depth and focus are edited through the inspector |

**Implementation note from the prototype.** Clamp the UI animation time step to non-negative values. A negative first-frame delta fed a square root inside the random source, produced NaN, and that NaN then persisted in every later frame. This was a bug in the prototype's UI, but any UI-side smoothing or random walk in the plugin needs the same guard.

## Sliders

Sliders follow the same rules as knobs. Each ring becomes a straight lane running parallel to the track.

| Knob element | Slider equivalent |
| --- | --- |
| Value arc | Neutral fill on the track, from the minimum to the base value. Bipolar parameters fill from a center notch. |
| Needle | None. The thumb is the grab handle and sits at the base value, or at the editing corner's value when morphing (see the morph knob spec). The live tick on the outermost lane shows the output. |
| Mod rings | Lanes on one side of the track: below a horizontal slider, right of a vertical one. Lane 1 sits 11 units from the track centerline, and each further lane is 5.4 units out. |
| Stacked origins | Unchanged. Each lane starts at the live output of the lanes nearer the track. |
| Origin dot and live tick | Unchanged. The tick is drawn perpendicular to the lane. |
| Four-ring limit, +n | Unchanged. The +n label sits past the slider's maximum end. |
| Emphasis and heavy focused ring | Unchanged |

The opposite side of the track is reserved for the morph tether, so mod marks and morph marks never share a side.

**Gestures.** Depth drags in map mode and Option-drags follow the slider's own axis: horizontal for horizontal sliders, vertical for vertical ones. One slider length equals full depth, so the lane grows under the pointer at the same rate it is dragged. A click with no movement behaves exactly as on a knob. Plain drags are relative, never jump-to-click, because a jump would write a corner value the user didn't aim for.

**Reference:** [HORDE Slider Marks](https://claude.ai/artifact/H13BmEV6BF5Gh3YYMZ5Qf8) prototype, which shows map mode and morph together on horizontal and vertical sliders.

## Open decisions and reference

These came up while testing the prototype and are not settled. Each has a recommendation.

| Decision | Options | Recommendation |
| --- | --- | --- |
| Ring order on a knob | Order mapped; slowest or largest inside; focused outermost; user-reorderable | Order mapped, reorderable by dragging inspector rows. Automatic ordering makes rings jump. |
| Clicking a parameter already mapped to the current source, in map mode | Select only; remove (makes map mode a toggle) | Select only. Removing with a click is too easy to trigger during a quick run of clicks. Keep Backspace and the inspector for removal. |
| Cycling focus with 3 or more mappings | Option-click; Option-scroll; a small popover | Option-click, plus the caption shown while Option is held. Revisit if four-mapping knobs turn out to be common. |
| Default depth on a plain click | 25%; 0% (click only arms the mapping) | 25%, so a click gives an audible result |
| Scope (global vs corner-level) when mapping in map mode | Always global; a scope switch next to the MAP control | Always global for 1.0, with a scope switch as a follow-up. Intent and morph targets are forced global regardless. |
| Stray mappings during page switching | Accept; a short delay before clicks register after a page change | Accept, and rely on undo |

**Reference prototype:** [HORDE Map Mode](https://claude.ai/artifact/RFbY41fivZuKm6QU58hCp4) (browser). It is a behavior reference, not a parity oracle. The time step and the random source's math are illustrative only.
