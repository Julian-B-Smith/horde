# Accessibility

horde aims at Surge XT's level (B432, ruled 2026-10-04): full keyboard navigation and screen-reader support where the GUI stack allows, accepting the limits of a complex instrument.

## Keyboard

- **Keyboard mode is opt-in and off by default** (ADR-259 ruling 6). Off, no control takes focus and every key reaches the host, so computer-keyboard MIDI never stalls (B449). On, the Keyboard-mode lamp says "KEYS ON", the window wears `focus-ring`, Tab walks the controls in reading order, arrows adjust (shift = fine), and Esc hands focus back to the host. The morph and map accelerators (1–4 and 0 to pin a corner, M for MAP, 1–9 for a source, P for polarity, Backspace to remove) work only here, and each has a mouse route.
- A gate proves notes stay bit-identical while keyboard mode is off.
- A typed-entry field is the one exception: it holds the keys only while it is open.

## Screen readers

- The page bar is two `tablist`s (page groups, then the selected group's pages); in keyboard mode each is one tab stop, Left and Right move and select, Down goes to the pages and Up back. A section's on toggle is a `switch` named by its path ("Osc 1 · Blade 2 · On"); a channel strip is a `group` ("Osc 1 channel") holding its switch, its Mute and Solo buttons (`aria-pressed`), its fader and a `meter` whose value text is the held peak.
- A sized control shows its value only on hover, drag or focus, and its corner line in the section's readout slot; neither is the only carrier: the control's `aria-valuetext` keeps the full value, the editing corner and its stored value, as before.
- Every control has a role (`slider`, `switch`, `radio`, `spinbutton`, `button`) and a name built from its module path and parameter: "Filter · Cutoff", "Env 1 · Attack". Its value text includes the unit: "1.20 kHz".
- Host-visible parameter names are unique and carry module path and unit (B432 item 6).
- Canvas glasses are opaque to screen readers, so each FX module's house-style DOM view is its accessible view, and doubles as its backup UI (ADR-235).
- Status is never colour alone: corners carry glyphs, lamps carry words, the selected chip carries an ink ring, meters latch with a labelled lamp.

## Targets

Every hit target is at least 28 × 28 (`size-hit`, ADR-259 ruling 8). A smaller face (the 17 px toggle, the 20 px chip, the 16 px puck) extends an invisible hit box, and targets packed in a row sit at least 28 px apart. The labs' 26 px and 22 px rows return to 28. Dense panels (B589) shrink faces, not targets: a 20 px pill, stepper, readout, lamp or mute/solo face and a 26 × 14 toggle each carry an invisible box to 28 px, and wrapped rows sit at 28 px pitch; a 28 px utility knob's face is its target.

## Contrast

Text reaches 4.5:1 on the grounds its token's note names, in both themes; text 24 px and up, control edges, focus rings and meaningful icons reach 3:1. Disabled controls are exempt. The audit and every pair it changed are in the Contrast audit section.

## Motion

Honour `prefers-reduced-motion`: the wordmark uses its static asset, the Specimen holds still, flips do not tilt.
