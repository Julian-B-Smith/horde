# Lens (P1, focus + lens)

The page paradigm of ADR-338 ruling 5: every control of a page in view as tiny knobs and companions in clusters (advanced rows inline after the ADV mark), and the control under the pointer, or the last one clicked, enlarged in a **lens**.

`Horde.Lens(el, S, { scope, fallback, size })`
- `scope`: the page's manifest keys (an array, or `key => bool`); only those controls drive the lens.
- `fallback`: the key shown before anything is touched. `size`: the bundle knob size of the enlarged control, default `standard` (48).

**Anatomy** (a `panel` card, 1.5 px ink edge, radius 6): the label "Lens"; the full bundle knob (a SHADOW of the page control's props, so it edits the same corner values and `S.params` keeps the page's own object); the name and path; the four corners' values (the editing corner bold, each in its legible `-tx` ink, a weight where the pad sits between corners); every mapping in its source colour with polarity and amount.

**Which control**: the pointer's (`S.hot`, set by every knob), a drag's, else the last clicked (`S.selected`), else `fallback`. The lens never takes the keyboard: its knob is a tab stop only in keyboard mode, like every control.

**Layout of the page** (the preview): the left column (about 186-206 px) holds the pictures and the lens; the right holds one Cluster per section, each under a 3 px ink identity rule. Pictures take at least half the body (README, Density and hierarchy). The P1 grid replaces P2's fold except where a page still overflows (see Fold).

**Readings taken** ADR-341's open point (which value "where the hand is taking the control" shows in the dual readout) is 2a's; the lens lists the stored corner values and does not choose. The lens is `role="group"` with no live region: a polite live region on a hover-driven view would announce every pass of the pointer.

**Sources** ADR-338 ruling 5, ADR-340 ruling 4 ("the human may revisit the lens: I can see it being quite useful"), composition-lab.html P1.
