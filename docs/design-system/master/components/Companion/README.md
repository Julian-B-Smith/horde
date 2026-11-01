# Companions

The toggle, segments, select and button at the **TC1** faces: the house look in 16 px boxes, for sections and cards (ADR-338 r2). **TC2**, the flat form, is allowed only inside tables. Source: `docs/design/composition-lab.html` (round 1: TC1, TC2), unchanged by rounds 2 and 3 apart from the stepper (below).

**The consumer provides** `kind` (`'toggle'`, `'segments'`, `'select'`, `'button'`), `name` and `path` (the accessible name), the manifest's option labels exactly as listed (`options`; `short` the abbreviations the faces show, the full label is the title), `value` (a toggle: a boolean; segments and select: the index), `label` (a TC2 lamp's or either button's text), `flat` for TC2, `onInput(v)` and `keys()` (keyboard mode). `Horde.Companion(el, props)` returns `{ get(), set(v) }` (set is silent). The **stepper** is `Horde.Stepper` with `tc: true` (TC1) or `flat: true` (TC2's drag number).

**TC1** (a face of 16 px, a hit box of 28 px through an invisible `::before`; every outline is 1.2 px `ink`)
- Toggle: an 18 × 10 capsule, `well-alt` off with the thumb left, `value` on with the thumb right (a 6 px thumb), `role="switch"`.
- Segments: a 16 px `ink`-edged box with a 4 px radius, the options side by side split by a `div` hairline; the chosen one fills `ink` with `bg` text; `role="radiogroup"` and `radio`. Keep the labels short: the `short` abbreviations.
- Select: a native select at 16 px, at most 76 px wide.
- Button: 16 px, 4 px radius, 8.5 px caps.
- Stepper: the `H.Stepper` capsule at 16 px, with ‹ › 14 px, the ⇕ glyph and the step scale (SG-b).

**TC2, flat, inside tables only** (a 16 px line, the same 28 px hit box; no edge)
- A lamp (a 7 px dot, `value` when on, and a caps label), underline segments (the chosen one `ink` with a 2 px underline), the value with a ▾ over a native select (a dotted `t2` underline), a text button (caps, underlined on hover), and the drag number (`H.Stepper` `flat`).

**Behaviour** a click toggles or selects; with `keys()` true the segments take one tab stop and ← → move and select; nothing else takes the keyboard outside keyboard mode. The ownership underline (the 2 px rule in the owning corner's ink while the pad is between corners, README proposal 5) is not drawn here: it belongs to the page forms.

**Placeholders** none new. The lab's TC2 caret select overlays a native select; the master keeps it so a table opens the platform's list.

The preview shows SAMPLE option names and values, never the engine.
