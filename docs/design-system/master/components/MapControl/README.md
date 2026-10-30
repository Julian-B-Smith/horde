# MAP control

The persistent map-mode switch: a toggle joined to a source selector, with the live source strip under the top bar and the inspector as its mouse routes (HORDE Map Mode Spec).

**Where** in the frame's top bar, outside the page area, identical on every page; switching pages keeps MAP and its source.

**Anatomy**
- MAP button: a 28 px capsule half with a lamp dot and "MAP"; off is `panel` with `t2` text and a hollow dot; on fills with the current source's colour and `on-src` text. "HOLD" shows while M is held (momentary).
- Source selector joined to it (mono 10 px), underlined in the source colour while MAP is on.
- Live source strip: one chip per source with a small live meter in its colour, its name and its number; click a chip to select it. It confirms what each source is doing and is the colour key.
- While MAP is on the window wears a 2.5 px ring in the source colour (the house has no glow), and the cursor over controls is a copy cursor.
- Inspector rail: the selected control's mappings in ring order, each with a colour dot, the source name (click to focus), a polarity pill (click cycles UNI, BI, INV, CTL; CTL is skipped while another mapping controls the parameter, ADR-266 ruling 2), its depth (a Control shows its low–high range), and a remove button.

**Source colours** come from the `src-` family by kind: envelopes `src-env`, LFOs and modulators `src-lfo`, macros and the intent bus `src-macro`, engine observables `src-engine`. No source colour is any corner's hue (yellow, pink, teal, lime). Sources of one kind share a colour (ruled 2026-10-21): on one knob they are told apart by the inspector's rows and the focus caption ("LFO 2 BI 30% (2/3)"), and in the strip by name and number.

**MAP on**: clicking a control with no mapping from the current source maps it at 25%, unipolar; click-drag creates it at 0 and sets depth by the drag (release at 0 cancels); on a control already mapped from this source, a click only selects and a drag edits depth. The base value never moves.

**MAP off**: drag sets the value (the editing corner's); Option-drag sets the focused mapping's depth; an unmapped control says it has no modulators.

**Choosing which modulator Option-drag controls** (the focused mapping; two ways, both mouse routes):
- **Option-click** a control (no movement): focus moves to its next mapping, wrapping round.
- **Click a row** in the modulator inspector.
Option + scroll wheel never changes the focus: it always adjusts the value (B374, ruled 2026-10-21), mapped control or not.
Holding Option shows the focus caption under every mapped control ("LFO 2 BI 30% (2/3)", in the source colour), so the focus is always visible before you drag.

**Keys are accelerators only, and only in keyboard mode** (ADR-259 ruling 6), each with a mouse route:

| Key | Does | Mouse route |
|---|---|---|
| M tap / hold | latch MAP / MAP while held | the MAP button |
| Esc | MAP off (MAP off: editing corner back to Auto) | the button; Auto |
| 1–9 (MAP on) | select source n | the selector, a strip chip |
| 1–4, 0 (MAP off) | pin corner A–D, back to Auto | the editing-corner selector, a pad corner |
| P / Option-P | cycle polarity (last touched / focused) | the inspector's polarity pill |
| Backspace, Delete | remove the last-touched mapping | the inspector's remove button |

**Rules** mappings made in map mode are global for 1.0 (a scope switch later); morph and intent targets are always global. Mapping changes are state, not automation, and each is one undo step. Option becomes Alt on Windows; if a host intercepts Alt-click, a setting moves it to Ctrl.
