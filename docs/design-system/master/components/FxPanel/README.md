# FX panel

The case view of the FX chain: an IN panel with the lanes' jacks, every module as a cream face carrying a small screen in its own display style (ADR-273) in house order, and an OUT panel, with each cable a patch cord from an OUT jack to an IN jack.

**The consumer provides** the same state as FX rack (`lanes`, `nodes` with optional `cost`, `out`, `cables`). Call `Horde.FxPanel(el, getState, session)`.

**Anatomy** (CSS px; the view is 236 high at every width)
- Faces: one column each, the full height less 6 px top and bottom, radius 3, a 1.5 px `ink` edge (dashed when asleep). Every face, a module's and the IN and OUT panels', is the `panel` cream with `ink` text. A module's face carries its name (14 px down), then its screen inset from 22 to 52 px down (its `fxs-<module>-ground` with a 1 px `ink` edge and a tiny picture in `-ink` and `-mid`; FX cables), then the LED (59 px), jacks and captions, all in `ink`.
- LED: lit means a 2.4 px disc over a 5 px glow disc at `opacity-halo` (never a blur). Asleep, it is a hollow ring. The module's cost in points sits at the foot (8 px) when the consumer gives `cost`.
- Jacks (radius 4.5, at most 0.15 of the column): the OUT jack at 36% of the face height, the IN jack at 64%. On the IN panel, Body is at 36% and Sub at 58%; OUT's input is at 58%. A jack is an `ink` ring on the cream face. A plugged jack shows a centre dot in `ink` (a lane's jack: its lane colour). Captions ("out", "in", "Body", "Sub") sit above their jacks in that ink, from a 26 px column up.
- Cords: a cubic from the OUT jack down and across to the IN jack, sagging (0.05 + 0.045·span columns) of the height, at most 0.3. The deepest sags are drawn first. Each cord is a cable in the shared encoding: the source's cable colour in an `ink` casing, which keeps crossing cords, and a pale cable on the white well, legible (FX cables, how the cables read in both themes). A backward part's `z⁻¹` chip sits near the cord's low point.
- Order: faces, then cords, then jacks and captions LAST. Cords leave jacks downward, so no cord covers the caption of the jack it plugs into.
- The status line, as in FX rack.

**Gestures** none: a display.

**Accessibility** as FX rack.

**Constant box** fixed height; width from the container only (tested).

**Reduced motion** one still frame, redrawn only on change.

**Fits** 320–1000 px with no scroll. Below a 46 px column the faces take short names; at 288 px each column is 28.8 px and every caption still fits.

**Placeholders**
- The screens: six of the eight styles are stand-ins and every picture is a placeholder (FX cables).
- The cost at each face's foot is the lab's CPU points (Bulwark 1, EQ 1, Filter 1, Drive 1, Shriek 2, ECHO 2, Scape 3, Sluice 4). They are stand-ins; the shell passes its own, or none.

**Sources** `docs/design/fx-chain-morph-lab-r3.html`: :278, `panelJack` and `panelScene` :2039–2057, `paintPanel` :2697–2733, `COST` :394.

The preview draws SAMPLE data generated in the preview (deterministic), never the engine.
