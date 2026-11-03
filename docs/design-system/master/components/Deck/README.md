# Deck

One panel with tabs (GUI-4 r1, r3): Main's deck, where each tab gets real room and only one shows at a time.

`Horde.Deck(el, { tabs: [{ id, label }], active?, onPick?, keys? })` returns `{ panel(id), pick(id), active() }`. The host fills `panel(id)`.

**Every panel stays in the document,** hidden when not picked, so what sits in a tab is still mounted and a control's value still follows.

**Anatomy** a `panel` card with a 1.5 px ink edge, a row of 10 px pill tabs, the selected one filled in ink, and a body that scrolls if a panel runs over.

**Keyboard** tabs are tab stops in keyboard mode only (ADR-259 r6).

**Sources** GUI-4, `docs/design/main-workshop-lab.html` B.
