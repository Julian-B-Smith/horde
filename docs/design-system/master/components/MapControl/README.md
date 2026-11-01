# MAP control

The persistent map-mode switch: a toggle joined to a source selector, with the live source strip under the top bar and the inspector as its mouse routes (HORDE Map Mode Spec).

**Where** in the frame's top bar, outside the page area, identical on every page; switching pages keeps MAP and its source.

**Anatomy**
- MAP button: a 28 px capsule half with a lamp dot and "MAP"; off is `panel` with `t2` text and a hollow dot; on fills with the current source's colour and `on-src` text. "HOLD" shows while M is held (momentary).
- Source selector joined to it (mono 10 px), underlined in the source colour while MAP is on. **MS3** (ADR-340 r8): with `chip: true` the selector is replaced by the **source chip** (`H.SourceChip`, its own README): you drag up or down on its name to scroll through the sources, step with ‹ ›, or open the full list; a plain wheel is not used (B301's gate). **MS1** is the strip below, **MS4** the bank rows (`H.SourceRows`). Switching the source chooses the source of the NEXT mapping and never implies one modulator per parameter: several modulators can map to one parameter (the human, ADR-340 r8).
- Live source strip: one chip per source with a small live meter in its colour, its name and its number; click a chip to select it. It confirms what each source is doing and is the colour key.
- While MAP is on the window wears a 2.5 px ring in the source colour (the house has no glow), and the cursor over controls is a copy cursor.
- Inspector rail: the selected control's mappings in ring order, each with a colour dot, the source name (click to focus), a polarity chip (MP1: click cycles UNI, BI, INV, CTL, shift-click goes back, a vertical drag sets the depth; CTL is skipped while another mapping controls the parameter, ADR-266 ruling 2), its depth (a Control shows its low–high range), and a remove button.

**Source colours** come from the `src-` family by kind: envelopes `src-env`, LFOs and modulators `src-lfo`, macros and the intent bus `src-macro`, engine observables `src-engine`. No source colour is any corner's hue (yellow, pink, teal, lime). Sources of one kind share a colour (ruled 2026-10-21): on one knob they are told apart by the inspector's rows and the focus caption ("LFO 2 BI 30% (2/3)"), and in the strip by name and number.

**MAP on** (MD1, ADR-340 r7, amending the Map Mode spec and ADR-261 r3): a plain **drag moves the value**, exactly as with MAP off. A **click** (no movement) on a control with no mapping from the current source maps it at 25%, unipolar; on one already mapped from this source, a click focuses that mapping. **Option-drag sets the current source's depth**: on an unmapped control it creates the mapping at 0 and sets depth by the drag (release at 0 cancels). The spec's earlier rule, a plain drag sets depth under MAP, changed the scale with no modifier and confused the human ("it should still require an option+drag to scale the mod").

**MAP off**: drag sets the value (the editing corner's); Option-drag sets the focused mapping's depth; an unmapped control says it has no modulators.

**Choosing which modulator Option-drag controls** (the focused mapping; two ways, both mouse routes):
- **Option-click** a control (no movement): focus moves to its next mapping, wrapping round.
- **Click a row** in the modulator inspector.
Option + scroll wheel never changes the focus: it always adjusts the value (B374, ruled 2026-10-21), mapped control or not.
Holding Option shows the focus caption under every mapped control ("LFO 2 BI 30% (2/3)", in the source colour), so the focus is always visible before you drag.

**Keys are accelerators**, each with a mouse route. **The map keys and Escape act in keyboard mode, or always when the host sets `S.mapKeysAlways`** (the session option `mapKeysAlways`). GUI 3 sets it from one constant (ADR-335 r1, provisional by the human's words: reworked if the keys collide with keyboard MIDI input, and reversible by flipping that constant). The rest act only in keyboard mode (ADR-259 ruling 6). No key acts while a text field has focus.

| Key | Does | Mouse route | When |
|---|---|---|---|
| M tap / hold | latch MAP / MAP while held | the MAP button | keyboard mode, or always with `mapKeysAlways` |
| 1–9 (MAP on) | select source n | the selector, a strip chip | keyboard mode, or always with `mapKeysAlways` |
| P / Option-P | cycle polarity (last touched / focused) | the inspector's polarity pill | keyboard mode, or always with `mapKeysAlways` |
| Backspace, Delete (MAP on) | remove the last-touched mapping | the inspector's remove button | keyboard mode, or always with `mapKeysAlways` |
| 1–4, 0 (MAP off) | pin corner A–D, back to Auto | the editing-corner selector, a pad corner | keyboard mode |
| ⌘⌥-click (Ctrl+Alt on Windows) a control | cycle the focused mapping's polarity, as a click on its chip (ADR-341 r4) | the inspector's polarity chip | always |
| Esc | the layered cancel (below) | each layer's own close | keyboard mode, or always with `mapKeysAlways` |

**Escape is a layered cancel** (ADR-338 r11): it cancels the innermost open thing and nothing else, in the order a host's own layers (a menu, a map-then-pick, a number field), then MAP, then a pinned editing corner, then keyboard mode. `S.escapeLayers` holds the order; a host puts its layers in front with `S.escapeLayers.unshift({ name, active, cancel })`, and `S.escape()` runs one Escape. **Panic is on no key and has no panel control** (ADR-340 r12: ADR-226 item 6 holds; the engine answers the host's all-notes-off).

**Rules** mappings made in map mode are global for 1.0 (a scope switch later); morph and intent targets are always global. Mapping changes are state, not automation, and each is one undo step. Option becomes Alt on Windows; if a host intercepts Alt-click, a setting moves it to Ctrl.
