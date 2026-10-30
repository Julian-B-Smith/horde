# Page nav

The two-level page bar in the window's header: the page groups on top and, under them, the selected group's own pages (B589: "'Osc' should be one, and once you've selected it you should see the options for individual oscs below it").

**The consumer provides** `groups`, in order, each `{ id, label, pages? }`; a page is `{ id, label, on? }`, where `on` (an oscillator's power, read from the manifest) draws the page tab's dot. A group without `pages` has one page, itself. Also `value` (`{ group, page }`), `onChange(group, page)`, `panel` (the page area's element id, for `aria-controls`) and `keys` (keyboard mode).

**The groups** (README Layout, gui3's pages): Main · Osc · FX · Mod · Morph · Mix · Hist · Set. Osc holds Dashboard (first, and where Osc opens the first time), Osc 1, Osc 2, Sub, Noise and Filters. The others hold one page each until their own pages are ruled.

**Anatomy**
- Level 1, groups: the house pill drawn at 22 px (3 px margins: 28 px pitch, 28 px hit), `label` type in `t2` on `panel` with an ink edge; selected: ink fill, `bg` text, 600. This is the strongest chrome on the page after the logo.
- Level 2, pages: text tabs on the window ground, 28 px tall, 9.5 px Archivo in `t2`; selected: `ink`, 700, a 3 px ink underline; hover: a 2 px `div` underline. A 1 px `div` rule closes the row. A page's on-dot: 7 px, `value` fill with an ink edge when on, a hollow `t2` ring when off (the dot is never the only carrier: the accessible name says "on" or "off").
- The page row is always present (a one-page group shows its one tab), so the header never changes height.

**Behaviour** a click on a group selects it and shows the page it was last on (Osc → Mix → Osc returns to Osc 2); a click on a page selects it. `mark(group, page, on)` updates a dot when an oscillator is switched elsewhere (its dashboard card, its page).

**Accessibility** (accessibility.md) both rows are `role="tablist"` ("Page groups", "Osc pages"); every tab is `role="tab"` with `aria-selected`; group tabs control the page row, page tabs the page area. Off keyboard mode no tab takes focus (computer-keyboard MIDI reaches the host). In keyboard mode each row is one tab stop (roving): Left and Right move and select, Home and End jump, Down goes from a group to its selected page, Up comes back.

**The Osc dashboard** (a composition, shown in the preview): the Osc group's first page is a grid of one `.hd-cell` per oscillator, side by side (`minmax(190px, 1fr)`). Each card is a section with no rule: on toggle, the name (a click opens that oscillator's page), a summary in the slot; its picture (the blade view or the scope, `hd-viz`, 2.6 : 1) across the card's width; then two to four compact controls, the level first. The picture comes first because it says more than the knobs; the card is the oscillator's minimal view and never its whole face.

**Placeholders** which controls each oscillator's card carries (the preview: level plus two of detune, coupling, width, colour) is the shell's call, not ruled; whether Filters belongs to Osc (the bench's "Osc (+ Filt)", gui3's own page) is open; Mod and FX may grow pages of their own.

The preview's pictures and values are SAMPLES generated in the preview, never the engine.
