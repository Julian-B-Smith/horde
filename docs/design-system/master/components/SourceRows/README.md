# Source rows

MS4 (ADR-340 r8): the bank's **row form**, where a click on a row makes it the source. Source: `docs/design/composition-lab-r2.html` (MS4, the M2 thin row).

**The consumer provides** a `Horde.Session`; `Horde.SourceRows(el, S, { sources?, fmt?, label? })` returns `{ el, render }`. `sources` lists the ids shown (the page's own; default every source); `fmt(source, value)` formats the value (default 0 to 100).

**Anatomy** (a row, 26 px tall, `panel`, an ink edge, 6 px radius; grid: swatch, name, meter, value, Map)
- A swatch in the source's colour by kind, the name (Archivo 800 8.5 px, caps) with a one-line foot ("1 route", "no routes"), a **live meter** (56 px, the source's colour), the value (mono 9.5 px, tabular) and a MAP button (a 20 px face in a 28 px hit).
- A row is `sel-neutral` with the source's colour on its edge while MAP is on and it is the current source.

**Gestures**
- **While MAP is on, a click on the row's body makes that source the one the next mapping uses** (the row lights). With MAP off a click does nothing.
- The MAP button: turns MAP on with that source (map-then-pick); pressed again, MAP off.
- Switching the source never implies one modulator per parameter.

**Scope** this is the row and its source-switching only. The bank (M2: two lanes of 26 px rows in 68 px, the Macros | Mods toggle BT1, the macro rows BR-b, removal) and the rows' pictures (a source's shape) are the page forms' (batch 2b); the row's picture here is its live meter. The ×/remove button (M-R2) is not drawn.

The preview shows SAMPLE sources, never the engine.
