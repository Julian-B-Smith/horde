# FX rack

The signal-flow view of the FX chain: every module sits static in one row in house order, with the lanes first and OUT last, each a cream card carrying a small screen in its own display style (ADR-273), and the morph moves only the cables between them, drawn as arcs.

**The consumer provides** the engine's published cable state, read each frame (the view never computes a weight, a loop or a wait; the lab's L0064: the view draws the object the audio plays):
- `lanes`: `[{ id, name, short, style: 'lane' | 'lane2' }]`, Body and Sub;
- `nodes`: `[{ id, name, short, style, asleep, cost?, tag? }]` in house order (dynamics and EQ first, filter, drive, saturation, the network, then time); `style` is the module's screen key, naming its `fxs-<module>-ground|ink|mid` tokens (FX cables); `tag` is a short suffix such as `‖2` for a two-band Bulwark;
- `out`: `{ id, name, short }`;
- `cables`: every cable the engine has live or waiting, `{ u, v, wf, wb, loop, held }`, with `wf` the forward part, `wb` the backward part (one sample late), `loop` set while the cable is in a live cycle, and `held` set while it waits;
- `rules` (the matrix reads them; the rack ignores them).

Call `Horde.FxRack(el, getState, session)`. With a session, it draws on the session's frame loop; without one, call the returned view's `render()`.

**Anatomy** (CSS px, drawn at the container's width)
- The row: one column per lane, module and OUT. Height is the width ÷ 5, held between 170 and 200 (it was 150–190 before the cards grew from 22 to 52 px). A `hair` baseline runs through the row.
- Boxes: up to 76 × 52, at 0.84 of the column, every one a `panel` card with a 1.5 px `ink` edge. A module's card has its name in `ink` on top and its screen inset under it (the module's `fxs-<module>-ground` with a 1 px `ink` edge, a tiny picture in `-ink` and `-mid`; FX cables); a lane and OUT are the same card with the name alone. A box shows the full name when it fits and the short name otherwise (Bo, Su, Dy, Eq, Fl, Dr, Sa, Nw, Dl, Rv, Out). Under 34 px the screen's margin drops from 5 to 3 px.
- Arcs: a cable running left to right arcs ABOVE the row, and right to left BELOW it. It leaves its source 0.2 of a column right of centre and lands 0.2 left of centre (right of centre going left), so in- and outgoing arcs part at every box. Each arc is a half-ellipse; its height is (0.07 + 0.085·√span) of the view's height, capped so the apex stays inside the view. Long arcs are drawn first, so short ones lie on top.
- An arrowhead lands on the box edge, in the source's cable colour (`fxs-<module>-cable`), edged in `ink`, at the cable's opacity, sized 2.5 + 2.5·min(1, w).
- Each backward part carries a `z⁻¹` chip at its apex. Chips are drawn over every arc.
- The corner hints `left → right ↑` and `right → left ↓`: `hint`, 8 px.
- The status line under the view (22 px, constant): `LIVE LOOP n` and `WAITING n` flags (`caution-tx` on `caution-bg`), the live cable count, the loop or wait in words, and who sleeps. The words end in an ellipsis; the count never shrinks.

**Gestures** none. The rack is a display: the morph moves the cables, and nothing in it is clickable. The preview's morph pad is the core XYPad.

**Accessibility** the view is a figure. Its SVG's label gives the live cable count, any loop, waits and sleepers, and a visually hidden list names every cable with its weight, "one sample late", "in a live loop" or "waiting".

**Constant box** the view's size depends only on its container's width; no state changes it (tested at 288, 600 and 960 px over a sweep, loops and waits included).

**Reduced motion** one still frame, redrawn only when the state or the width changes.

**Fits** 320–1000 px with no scroll: below about 640 px the boxes take their short names, and at 288 px each column is 26 px (a 22 px card with a 16 px screen).

**Placeholders**
- **The screens.** Shriek's and Sluice's colours are their real display styles; the other six styles are stand-ins, and every module's picture is a placeholder (FX cables). Decided by: the human choosing each module's skin.
- **The flow dots.** The lab ran dots along each arc while audio played. They are left out here, because the views draw without audio. Decided by: whether the shell's views know the audio state.

**Sources** `docs/design/fx-chain-morph-lab-r3.html`: the views' place :276–278, `rackScene` :2014–2027, `paintRack` :2633–2661. The preview's data comes from `FxSample` (in the bundle), a stand-in for the engine built on the lab's `targetCables` (:1240) and its cascaded glide (:24, :1278). Its corner chains are SAMPLES.

The preview draws SAMPLE data generated in the preview (deterministic), never the engine.
