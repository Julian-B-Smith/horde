# Module card

The card every module inherits: corner strip, header, mini-view and face, with an Advanced view behind it (Design System, FX module anatomy; the face/Advanced split, B227).

**Anatomy, top to bottom**
1. Corner strip, `size-strip` (6) high, ink rule beneath: the census landing site. Segment widths are each corner's grid share; a quantum (stepped) owner paints solid, a gradual one blends. The header repeats the shares with glyphs, because the strip is too thin to carry them.
2. Header, 28 px: a state dot (8 px, ink edge: `celebrate` active, `panel` bypassed, `alarm` clipping), the name in `cluster` style, the class tag (`radius-track`, `div` edge), the census glyphs, and the collapse button (28 × 28).
3. Mini-view: a well on `scr-ground`, ink edge, `radius`. The wet trace in `scr-value` with its glow as an underlay stroke at 22% (never a blur); the other corner's value of this module as a `ghost` line, so a coming flip is visible before it lands. **An FX module's mini-view is that module's own screen** (ADR-273): the three screen roles are re-pointed at its tokens, `scr-ground` → `fxs-<module>-ground`, `scr-value` → `fxs-<module>-ink`, `scr-grid` → `fxs-<module>-mid`, the way the Screen scheme selector re-points them at a scheme, and the same hex serves light and dark. The card around it stays the cream `panel`; the FX chain views (FX rack, matrix, panel) show the same screen as the inset on the module's card. The ghost takes the screen's `-mid`, because `ghost` is 1.3:1 on a pale ground such as Segment LCD and would vanish. Six of the eight modules' screens are stand-ins until their skins are chosen (the preview's Filter is one); Shriek and Sluice are real (FX cables).
4. Face: at most four morph knobs (the module's four-knob face, ADR-169 roles), each with its name and two readout lines.
5. Foot: the Advanced button and a `hint` line.
Collapsed cards keep the strip, header and mini-view only.

**The consumer provides** the module's manifest keys for its face (bound by key, never by position), its name and class, its corner shares, its state, and its mini-view's data (read from what was rendered, never recomputed in the GUI).

**Rules** `panel` ground, 1.5 px ink edge, `radius`, no shadow (cards are not interactive; their controls are). A card never assumes another module, or the oscillator panels, exists (ADR-254): every card renders alone in the FX device.
