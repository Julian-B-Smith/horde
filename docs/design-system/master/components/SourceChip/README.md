# Source chip

MS3 (ADR-340 r8): the MAP control's source as a chip you **drag through or step**, in the source's colour. Source: `docs/design/composition-lab-r2.html` (MS3).

**The consumer provides** a `Horde.Session` with sources; `Horde.SourceChip(el, S, { order?, onPick?, keys? })`, or `Horde.MapControl(el, S, { chip: true })`, which uses it in place of the select. `order()` returns the sources in the order the chip steps (the page's list, so the numbers follow MS1's; default `S.sources`); `onPick(id)` is told after a pick.

**Anatomy** (206 px × 28 px: the longest of the 16 source names fits without an ellipsis, measured in GUI 3 (150 px cut six of them); a 1.5 px border in the source's colour, `panel`)
- ‹ and › (28 px each): the previous and the next source.
- The name: a swatch (`src-` family by kind), the source's name in Archivo 800 8.5 px, and its number in `hint`. **Drag up or down on it** to scroll through the sources: one step per 12 px, down is the next, up the previous, and it wraps. `role="spinbutton"`.
- ▾: opens the full list (a native select over the caret).
- A plain wheel does **nothing** here: B301's gate (`lab_wheel_scroll_check`) allows wheel scrolling only through ⌥ + wheel, and a plain-wheel chip would take the page's scroll whenever the pointer crossed it. ⌥ + wheel would be possible if the human wants it.

**Behaviour** switching the source chooses the source of the NEXT mapping, through `S.setSrc`, and never implies one modulator per parameter: several sources can map to one parameter, each its own mapping (the human, ADR-340 r8, "important to keep in mind that multiple modulators can map to a single parameter"). In keyboard mode ↑ ↓ ← → step. The footer says "Source: LFO 2".

**Placeholders** the 12 px step distance is the lab's, not ruled.
