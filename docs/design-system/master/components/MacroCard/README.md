# Macro card

What a macro or an XY axis touches (GUI-9 r1, r4): one card at a time, opened from the macro's name or the axis's chip.

`Horde.MacroCard(S, { routes(id), nameOf(key), pathOf?(key), onDepthBegin?(routeId), onDepth?(routeId, v), onDepthEnd?(routeId), onRemove?(routeId), adds?(id), hold?(), host?, keys? })` returns `{ open(id, x, y?, above?), close(), refresh(), get(), el }`. `id` is a session source. `open` places the card by `x`'s left and `y`'s bottom (`y` defaults to `x`), above `y` if `above` or if it won't fit below. It should sit clear of the knob it's about, because you turn the knob to hear what you've mapped. Opening the open one closes it.

**Anatomy** (240 px): the name, "touches n" or "nothing here", and Map (Done while mapping). Then a row per mapping: name, a depth bar in `src-macro`, the percentage, ×. `adds` puts other ways in at the foot, as small buttons: an axis has List and Touch.

A double-click on a depth returns it to 25 %, as a mapping's ◇ does (GUI-12).

**Stays open** while MAP is on for this source, and while `hold()` is true, because mapping is what it's open for. Otherwise a click elsewhere or Escape closes it, through the layered Escape. Callbacks only: it edits nothing itself.

**It repaints only when its rows change,** or a depth drag would lose its row.

## The opener

`Horde.MacroOpener(el, S, { count(), onOpen(nameEl), keys? })` on a macro knob's host makes its name open the card and adds a badge counting what it touches. It listens on `pointerdown` in capture and not on `click`, because the knob captures the pointer as it's pressed, so a click lands on the knob and never reaches the name.

**Placeholders** the mappings are the shell's routes until the intent bus (B89), so depths aren't per corner yet.

**Sources** GUI-9, `docs/design/xy-macro-lab.html` (K3), `specs/SPEC-INTENT-BUS.md` section 5's readout.
