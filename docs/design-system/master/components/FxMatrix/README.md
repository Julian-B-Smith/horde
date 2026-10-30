# FX matrix

The from × to view of the FX chain: a grid with a row for every place a cable can come from (the lanes and the modules) and a column for every place it can go (the modules and OUT), each live cell showing that cable's weight as a bar in its source module's cable colour (ADR-273 A1); the headers are cream module cards, each with a small screen in its own display style.

**The consumer provides** the same state as FX rack (`lanes`, `nodes`, `out`, `cables`), plus `rules`: `{ 'u>v': 'banned' | '<note>' }` for the cables the rules forbid or condition. In the lab, Scape → Sluice is banned and Scape → Bulwark is allowed only as E1. Call `Horde.FxMatrix(el, getState, session)`.

**Anatomy** (CSS px)
- Headers are module cards, the same marks as the rack's boxes: a `panel` card with an `ink` edge, the name in `ink`, and the module's screen inset (FX cables). From 560 px up the column headers are 22 px tall with the name at the left and a small screen at the right, and the row headers are 78 px wide, which is when they take full names. Under 560 px the row headers are 34 px wide (name left, screen right) and the column headers stack the name over the screen and are 34 px tall (the top band is 40 px, not 28); every header takes its short name. `↓→` in the corner (`hint`): rows are where a cable comes from, columns where it goes.
- Cells: the column width is (width − header − 4) ÷ 9, and the row height is 0.62 of it, held between 22 and 30. There is a 3 px gap and radius 3. An empty cell is outlined in `hair`. A module's own cell (u = v) is filled with `hair`.
- **A live cell** (w ≥ 0.003) is a 1 px `ink` edge on the well:
  - the forward part is a bar in the source's cable colour (`fxs-<module>-cable`), edged in `ink`, along the cell's lower half, its length the weight;
  - the backward part is a dashed bar (3 / 2, 2.5 px core in the source's cable colour over a 4.5 px `ink` casing) along the foot, its length the weight;
  - the weight is printed at the top in `ink`, 8 px ("1", ".42"), when the cell is at least 22 px wide.
- A loop rings the cell with a 4 px `caution` halo at `opacity-halo`, drawn under every cell so no neighbour covers it. A waiting cable rings it dotted in `caution-tx`.
- **Unavailable**: a banned cell is hatched in `div` with `✕` in `t2`; a conditional cell is hatched with its note (`E1`) in `hint`.
- The status line, as in FX rack.

**Gestures** none: a display.

**Accessibility** as FX rack: a labelled figure and a visually hidden list of the live cables.

**Constant box** the height is a function of the width alone (tested over a sweep at three widths).

**Reduced motion** one still frame, redrawn only on change.

**Fits** 320–1000 px with no scroll. At 288 px the columns are 28 px and every header takes its short name. The grid never scrolls sideways; it reflows by width.

**Placeholders**
- The rules shown: the lab's V (Scape → Sluice banned; Scape → Bulwark as E1). The shell's rule set is the consumer's to pass.

**Sources** `docs/design/fx-chain-morph-lab-r3.html`: :277, `matrixScene` :2028–2038, `paintMatrix` :2663–2695, `BANNED` :1223. The lab filled a whole cell at the weight's opacity; here an edged bar in the source's cable colour carries the weight, so a faint cable still reads as a cell that holds a short bar.

The preview draws SAMPLE data generated in the preview (deterministic), never the engine.
