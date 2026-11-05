# Morph mode

One morphing key's two per-patch choices (ADR-291 r3, ADR-324 H1, ADR-325 X30; SPEC-ENGINE MO-11 to MO-13), and the mark an override leaves on its control.

**The consumer provides** `read()`, returning `{ mode, count, countActs }` or null when nothing is selected, plus `onMode(mode)` and `onCount(count)`. The consumer owns the state, and it says when the count can act, so this never computes the law.

**Anatomy**
- **Mode:** Follow (the default), Quantum, Exempt, as pills.
- **Count:** As the morph (the default), Two, Four, as pills. It's drawn only while it can act: Follow under Blend or Stepped (MO-11, GUI-21).
- **The mark** (`MorphMode.mark(host, mode)`): a Q with a staircase or an EX with a slashed ring, in the host's corner. Exempt also dashes the host's box. Follow leaves no mark. `MorphMode.tagHTML(mode)` is the same tag inline, for a row's name.

**Never colour alone.** A mark is a glyph and letters. An Exempt key has no corner tint, because it belongs to no corner.

**Gestures** one pick is one undo step, and re-picking the current choice makes none. That's the consumer's to hold.

**Sources** `docs/design/page-chrome-lab.html`: the marks :139-150, the glyphs :460-462, the rules :272-278. Its menu arrow on every label isn't taken. The picker lives in the control's menu and the Morph page's selected row.

The preview draws SAMPLE state, never the engine.
