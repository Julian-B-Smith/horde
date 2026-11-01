# Stepper

The house stepper for a stepped value (a count, a named option, a division): an inked capsule with ‹ › arrows around its value, or, in a table, the flat drag number. ADR-338 r2 took the steppers into the companions (TC1 in sections and cards, TC2 in tables); ADR-340 r5 ruled their gestures. Source: `docs/design/composition-lab-r2.html` (K2, K4).

**The consumer provides** `name` and `path` (the accessible name), either `options` (named values, the index is the value) or `min`, `max`, `step`, the `value`, the `def` (home), `fmt` for the unit, `onInput`, `say` (where a refusal or a clamp is said: the footer), `keys()` (keyboard mode) and `flat` for the table form, `sm` for the dense 20 px face.

**Gestures** (the human, 2026-10-30: "Steppers need to be draggable, possibly even able to accept number input"; 2026-10-31: "double clicking returns to home while option clicking allows you to type")

| Gesture | Does |
|---|---|
| drag the value up / down | one step per 6 px; Shift one per 24 px; past an end it re-anchors, so reversing moves at once |
| click ‹ › | one step (the mouse route; the flat form has none) |
| double-click anywhere | the default (home), as on a knob |
| Option-click | a number field in the same box, the value selected |
| Enter / Tab in the field | commit (clamped to the range, and said) |
| Escape in the field | cancel: the innermost layer of the layered Escape (ADR-338 r11), so it never reaches MAP or the page |
| a click away | commit |
| ↑ ↓ (keyboard mode) | one step; Shift ten; PgUp / PgDn ten; Home / End the ends; Enter or F2 the field |

Typed text: digits and a sign; a named option takes its name or its first letters ("tri"), or its 1-based number. Text that names no value is refused, the stepper keeps its value, and the footer says why. A digit typed into the field is text, never a map key.

**Look** the TC1 stepper is the house `.hd-stepper` (ink capsule, 28 px arrows, `.hd-sm` the 20 px face with 28 px hits); its value box is as wide as the widest value it can show, so nothing reflows as it changes (H1, ADR-340 r4), with a vertical-resize cursor. The flat form (`flat`, K4 for tables) is the value alone with a dotted `t2` underline. The field is the value box's own width on `well-alt`.

**Accessibility** `role="spinbutton"` with the value, its text, min and max; it takes focus only in keyboard mode. The title names the three gestures.

**Not yet** the steppers already inside other components (RateSync's division, the envelope's sustain point, the orbital's kick body, the trackers' order N, the S&H clock's binding) still carry their own handlers; they move onto `H.Stepper` with the composition work (B589). Round 3 of the composition lab draws how the drag and the home are shown (ADR-340 Consequences); the gestures here are ruled and do not wait for it.
