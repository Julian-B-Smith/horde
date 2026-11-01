# Section header

The one-line header that opens a section inside a card: an ink identity rule above, then the on toggle, the number, the name, the mode pills and a live readout slot on one line, with the section's sub-groups under small-caps labels below it. It is the hierarchy device that replaces a card inside a card (B589, after the SCALPEL bench's blade rows).

**The consumer provides** the section's `name` (cluster style, UPPERCASE) and accessible `path` ("Osc 1 · Blade 2"); `on` and `onToggle` when the section has a power key (a manifest toggle, never a local flag); `num` when sections of one kind repeat (blades 1 and 2); `rule`; `modes` (`options` exactly as the manifest lists them, `value`, `onPick`) when one enum is the section's main choice; `S` (the session) and `section` (the element whose controls feed the slot); and a `summary` (string or function) the slot shows at rest. Body markup is the consumer's: `.hd-sgroups` holding `.hd-sgrp`, each a `.hd-sublabel` over a `.hd-krow` of controls.

**Anatomy** (one line, `size-hit` tall)
- Identity rule: `stroke-id` (3 px) across the section's width, in `ink`. Sections are told apart by PATTERN and number, never by colour: `solid` (with a filled number), `dash` (hollow number), `dot` (hollow, dotted ring), `none`. The first section in a `.hd-cell` drops its rule: the cell's edge already draws it.
- On toggle: the dense toggle (`size-toggle-dense-w` × `-h`, 28 px hit). Off dims the section's body and its mode pills to `opacity-muted`; the header line stays readable so it can be switched back on.
- Number: a 16 px ink disc, `bg` text, Archivo 800.
- Name: `cluster` (11 px, 800, 0.1em). Mode pills: the dense pills (20 px faces, 28 px hit), a radiogroup.
- Readout slot (`H.ReadoutSlot`), right-aligned, truncating, and **a fixed `size-rslot` (250 px) wide whatever it says** (H1, ADR-340 r4: a slot sized to its content grew on hover and pushed the controls beside it, the human's "hovering moves everything around"). The info box (ADR-335 r4, H4) carries the description and the mappings. **Pending round 3** (ADR-340 r4's amendment): the slot shows both where the hand is taking the control and the value after modulation, drawn in the composition lab before it enters here. while a control in this section is hot (under the pointer, dragged or keyboard-focused) it reads its name, its value (ink, 600) and the second line the classic knob used to reserve a cell for (the editing corner's glyph, letter, stored value and weight; under Option or MAP the full focus caption, "LFO 2 BI 30% (2/3)", in the source colour; "controlled by MACRO 1"; LOCKED). At rest it shows the summary in `hint`.
- Below: `space-head` (2 px), then the sub-groups: `.hd-sublabel` (the `sublabel` style: 8.5 px, 700, 0.12em, UPPERCASE, `hint`) over a row of controls `space-dense` (4 px) apart; groups separated by a 1 px dashed `div` hairline with `space-subgroup` (7 px) each side. Sections stack `space-section` (10 px) apart.

**When to use what** (the card rule, README "Density and hierarchy"): a card (`.hd-cell`) is the one box level inside the window; inside it, a section per module (an oscillator, a blade, an envelope, a filter slot); inside a section, a sub-group label per cluster of two to six controls; a lone row of up to four controls needs nothing.

**Accessibility** the toggle is `role="switch"` named "<path> · On"; the pills a radiogroup named "<path> · Mode"; both take focus only in keyboard mode (arrows move and select). The slot is `aria-hidden`: every control's own `aria-valuetext` already carries the same text. The number is decorative; the name carries it ("Blade 2").

**Conflict resolved** the bench marks blade 1 with a pink stripe on the row's left edge (`value`) and blade 2 with the same pink hatched. Two rulings stand against that: one job per colour (`value` is the user's own value) and the left rule, which already means "owned by corner X" on a label (`.hd-owned`). The identity therefore moves to an ink rule above the header, told apart by pattern and the number.

**Placeholders** whether a section's rule pattern should follow its number everywhere (1 solid, 2 dash, 3 dot) is assumed here, not ruled.

The preview draws SAMPLE parameters and values (the SCALPEL bench's blade face), never the engine.
