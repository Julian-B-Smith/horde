# Stepper

The house stepper for a stepped value (a count, a named option, a division): an inked capsule with ‹ › arrows around its value, or, in a table, the flat drag number. ADR-338 r2 took the steppers into the companions (TC1 in sections and cards, TC2 in tables); ADR-340 r5 ruled their gestures; ADR-341 r3 ruled how they show them (SG-b) and that a named stepper chooses from a dropdown. Source: `docs/design-system/` composition lab rounds 2 and 3 (`docs/design/composition-lab-r2.html` K2, K4; `composition-lab-r3.html` SG-b).

**The consumer provides** `name` and `path` (the accessible name), either `options` (named values, the index is the value) or `min`, `max`, `step`, the `value`, the `def` (home), `fmt` for the unit, `onInput`, `say` (where a refusal or a clamp is said: the footer), `keys()` (keyboard mode), `spoken(v)` (the screen-reader text where the display is too short to say it), `flat` for the table form, `sm` for the dense 20 px face and `tc` for TC1's 16 px face. The range is fixed at mount: a stepper whose range changes is remounted by its owner (the envelope's sustain point and the orbital's kick body do).

**Gestures** (the human, 2026-10-30: "Steppers need to be draggable, possibly even able to accept number input"; 2026-10-31: "double clicking returns to home while option clicking allows you to type")

| Gesture | Does |
|---|---|
| drag the value up / down | one step per 6 px; Shift one per 24 px; past an end it re-anchors, so reversing moves at once |
| click ‹ › | one step (the mouse route; the flat form has none) |
| double-click anywhere | the default (home), as on a knob |
| Option-click, a NUMERIC stepper | a number field in the same box, the value selected |
| Enter / Tab in the field | commit (clamped to the range, and said) |
| Escape in the field | cancel: the innermost layer of the layered Escape (ADR-338 r11), so it never reaches MAP or the page |
| a click away from the field | commit |
| Option-click, a stepper of NAMED options | a dropdown of those options under the face (ADR-341 r3). **There is no typed-name entry**: the human, "it doesn't make sense to be able to type in your own text". |
| ↑ ↓ Home End PgUp PgDn in the dropdown | move the highlight (it starts on the current value and stops at the ends) |
| Enter / Tab in the dropdown, or a click on an option | commit that option (said) |
| Escape in the dropdown | cancel, the innermost layer of the layered Escape, like the field |
| a click away from the dropdown | cancel: nothing was typed, so nothing is committed |
| Option + wheel | one step (B374: a plain wheel never turns a control) |
| ↑ ↓ (keyboard mode) | one step; Shift ten; PgUp / PgDn ten; Home / End the ends; Enter or F2 the field or the dropdown |

Typed text in the number field: digits and a sign (a true minus is accepted). Text that is not a number is refused, the stepper keeps its value, and the footer says why. A digit typed into the field is text, never a map key. The dropdown swallows every other key for the same reason.

**Look** (SG-b, ADR-341 r3: the lab's round-3 drawing) the TC1 stepper is the house `.hd-stepper` (ink capsule, 28 px arrows, `.hd-sm` the 20 px face, `.hd-tc` the 16 px TC1 face, all with 28 px hits); its value box is as wide as the widest value it can show, so nothing reflows as it changes (H1, ADR-340 r4), with a vertical-resize cursor. Two additions say what the gestures do:
- **a ⇕ glyph** (6 × 10 px, `hint`) beside the value: it drags;
- **a step scale** under the face: one tick per step (up to 25; a bar with a marker beyond), the current step a wide ink tick and **home the ringed tick**; on a stepper at home the ring is the current tick. The scale lies under the face (`top: 100% + 2 px`, 4 px high); in the flat table form a 17 px row has no room below, so it lies on the cell's 2 px foot.
The flat form (`flat`, K4 for tables) is the value alone with a dotted `t2` underline. The field is the value box's own width on `well-alt`. The dropdown is a fixed `panel` box with an ink edge and a hard `shadow-rest`, rows at `size-hit` (28 px), at most eight rows high and then scrolling, current value marked with a dot, highlight `sel-neutral`; it flips above the face when there is no room below, so it never moves layout.

**Accessibility** `role="spinbutton"` with the value, its text, min and max; it takes focus only in keyboard mode. The title names the three gestures.

**Placement** the steppers inside other components are this one: the trackers' order N, the S&H clock's binding (K1 to K4), the envelope's sustain point (NONE, P1 to Pn-1; remounted when points are added or deleted) and the orbital's kick body (one option per body; remounted when bodies are added). **Not moved:** RateSync's division stepper keeps its own handlers: an off-list stored value is shown as "≈ 1/4 +3.0%" and its first step must land on the nearest name before moving on (an off-list patch is never silently changed), a rule that `H.Stepper` (a fixed list, the current index) cannot express without breaking the double-click default. Its Option-click therefore does nothing yet.

**Open** none new. Not verified in a host: whether a host keeps Option-click and Option + wheel in a plugin window.
