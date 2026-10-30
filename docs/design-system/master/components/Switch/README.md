# Switch

The stepped controls: the toggle for off/on keys, enum pills for a few named choices, and the stepper for counted steps.

**Choose**
- Toggle: a key with exactly off and on (`oneShot`, `retrigger`). Face `size-toggle-w` × `size-toggle-h` (32 × 17), capsule; off is `well-alt` with the thumb left, on is `value` with the thumb right. Its hit box is 28 px tall.
- Enum pills: two to about seven named options shown at once (`voice.mode`: poly, mono, legato; a blade's Mode). Idle: `panel`, ink edge, `t2` sticker text; active: ink fill, `bg` text, weight 600. Each pill is at least 28 px tall.
- Stepper: counted or long stepped ranges (`voice.polyphony` 1..64, `octave`). Two 28 px arrow buttons around a mono readout; the readout double-clicks to typed entry like any value.

**The consumer provides** the manifest key, its option labels exactly as the manifest lists them (no renaming), the accessible path, and the owning corner if any.

**Behaviour** click selects; arrows step in keyboard mode; pills form a radio group (one tab stop, arrows move); the toggle is `role="switch"`. A stepped key never sits on a knob.

**Morph-owned** pills put the 3 px corner rule and glyph on the LABEL, as the slider does. A morph flip on a stepped key flashes the incoming corner colour on the control's border for `duration-flip` and tilts its sticker −2°; flips play one at a time, at least `duration-flip-gap` apart.

**Dense faces** (B589, the Density ladder). Inside a container marked `.hd-dense` (or with `.hd-sm` on the element) the faces shrink and the targets do not:
- Toggle: `size-toggle-dense-w` × `-h` (26 × 14), thumb 8 px; its hit box stays 28 px tall.
- Pills: `size-face-dense` (20 px) faces, at least 28 px wide, 7 px side padding, 3 px apart; an invisible box takes each to 28 px tall, and wrapped rows sit at 28 px pitch (4 px margins, no row gap), so two rows' faces are 8 px apart and their hit boxes touch without overlapping. Sticker text stays 9.5 px.
- Stepper: a 20 px face; its arrow buttons stay 28 px wide with 28 px tall hit boxes; the readout 40 px at least.
This is the bench's `pill.mini` (19 px) made legal: the bench's 19–22 px rows gave 19–22 px targets, and the 28 px floor (ADR-259 r8) wins, so the density comes from the face and the gaps, never the target.
