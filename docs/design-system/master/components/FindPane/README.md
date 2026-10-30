# Find pane

The morph field's navigator: search, which rows to show, which kinds, a jump to each section, and the field's census.

**The consumer provides** the session and the grid handle returned by `H.MorphGrid` (its rows, sections and `jump`). The pane writes the grid's filter on the session (`S.morph.q`, `view`, `kinds`), so two views of one field agree.

**Anatomy**
- Search: a 28 px capsule field on `well`, mono 10, `hint` placeholder. A search shows every match whatever the view.
- SHOW: pills WHERE CORNERS DIFFER (the default) and ALL, a radio group.
- KINDS: three toggle pills, each with its kind glyph and word (SLIDES, SNAPS, RAMPS); pressed is `ink` with `bg` text.
- JUMP TO SECTION: a pill per section; a jump opens ALL, unfolds the section and scrolls the grid to it.
- Census: two mono columns, members, showing, each kind's count with its glyph, how many rows differ.
Captions in mono caps, `t2`.

**Gestures** clicks only; typing in the search field is typed entry, the one time the editor holds the keyboard outside keyboard mode (as the value readout does, B449 R1).

**Placeholders**
- The lab's ◆ WAYPOINTS, ⊘ EXEMPT and — NOT IN FIELD views are left out with the rows they show (see the morph grid).

**Sources** `docs/design/morph-editor-lab.html`: the pane :653-662; CSS :388-393; `buildFind` and `renderCensus` :2099-2143.

The preview draws SAMPLE data generated in the preview, never the engine.
