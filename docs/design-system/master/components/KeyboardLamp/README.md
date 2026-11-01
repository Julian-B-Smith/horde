# Keyboard-mode lamp

The switch and the indicator for keyboard mode: off by default, and whenever it is on the lamp says so and the window wears the focus ring (ADR-259 ruling 6).

**Why** the legacy editor held the host's keyboard whenever it had been clicked, which stalled computer-keyboard MIDI (B449). So by default the editor never takes keyboard focus; keys go to the host. Keyboard navigation (B432, Surge XT's level) is opt-in.

**Behaviour**
- Off (default): no control is focusable; the only time the editor holds keys is while a typed-entry field is open.
- On: Tab moves between controls in reading order, arrows adjust (shift = fine), Enter or double-click opens a value, Space toggles; every control shows `focus-ring`.
- Clicking the lamp turns it off and hands focus back to the host in the same turn. **Esc** does too, but only as the outermost layer of the layered cancel (ADR-338 r11): an open menu, a number field, MAP and a pinned editing corner are each cancelled first, one Escape each (`S.escapeLayers`, MapControl README). Where the host sets `S.mapKeysAlways` (GUI 3, ADR-335 r1), the map keys and Escape work whether or not keyboard mode is on.
- A gate proves notes stay bit-identical while it is off (ruling 6).

**Look** a 28 px capsule: off is `panel` with `t2` text and a hollow ink dot, "KEYS OFF"; on is ink-filled with `bg` text and a `celebrate` dot, "KEYS ON". The word always states the mode, so the lamp never relies on colour.

**The consumer provides** where it sits (the frame header, beside the logo's tabs), and the host focus hand-back.

**Open** whether a keyboard shortcut also turns it on is not ruled.
