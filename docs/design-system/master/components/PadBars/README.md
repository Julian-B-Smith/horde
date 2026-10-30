# Pad bars

The morph pad's three attendant bars: the mode banner above it, the selected row's lock bar, and the colour scale under it.

**The consumer provides** the session (mode, selected row, hover point from the quantum pad) and optionally `opts.key` to pin a bar to one row. Three functions, one folder, because the lab draws all three in one routine as the pad's text (`renderPadText`):
- `H.QuantumBanner(el, S)`: the mode, in one sentence, with a one-click switch between QUANTUM and BLEND.
- `H.LockBar(el, S)`: whether the row's regions follow the seed or are its own, with LOCK AS IS / UNLOCK.
- `H.MorphScaleBar(el, S)`: the row's whole range as a strip, with the corners' stored values, the heard value and the pointer's preview.

**Anatomy**
- Banner: `panel` with a `div` edge outside QUANTUM; in QUANTUM the caution banner (`caution` edge, `caution-bg`, `caution-tx` text), because nothing is wrong but the pad now edits boundaries. The button is a pill of fixed width.
- Lock bar: `well` ground; a dashed `div` edge while the row follows the seed, a solid `ink` edge when locked, `t2` when locked but dormant in this mode. A status chip (FOLLOWS SEED, or the padlock and LOCKED on `ink`), one or two lines of words, the pill. Disabled for a gate.
- Scale: the range ends in `value` style at each side (in the row's own units, or its first and last label); between them a small glass (`scr-ground`, `scr-physics` brightness, scanlines; for a structural row one cell per label). Above the glass each corner's stored value as its shape-coded glyph (`cA`…`cD` with a `mark-edge` outline; corners with the same value sit side by side); across it the heard value as an `ink` bar; below it the value under the pointer as an `ink` triangle. A one-line caption names both.

**Constant box** words change, sizes never: the banner's three sentences are stacked in one cell and only the current one shows, so its height is the tallest at every width; the lock bar's words are clamped to two lines; the caption to one; the buttons have fixed widths.

**Gestures** the banner's and lock bar's pills (28 px, each one undo entry for a lock change). The scale has none.

**Placeholders**
- The banner's QUANTUM sentence. The lab's banner said waypoints were dormant in QUANTUM; waypoints are not ruled (B235), so the banner says what QUANTUM edits instead. Decided by: a waypoint ruling.

**Sources** `docs/design/morph-editor-lab.html`: the banner :488-495 and :595-598; the lock bar :549-559 and :2356-2371; the scale :504-508, :609-610 and :2417-2458.

The preview draws SAMPLE data generated in the preview, never the engine.
