# FX cables

The one cable encoding the FX chain's three views share: a cable's weight, its forward and one-sample-late backward parts, a live loop, a waiting cable, a sleeping module, and each module drawn as a cream card carrying a small screen in its own display style (ADR-273, the human's ruling of 2026-10-23, option B of the FX Module Colours comparison; it replaced the flat house-colour fills of ADR-266 ruling 5).

**What it is** a drawing module, `Horde.FxCables`, that FxRack, FxMatrix and FxPanel call for every mark, so a cable reads the same in all three. Its preview is the encoding's legend, `FxCables.legend(el, { modules })`, which a host page can show under the views.

**The consumer provides** nothing of its own: the views pass it each cable `{ u, v, wf, wb, loop, held }` from the engine's published state, and each module's screen key (`style`: `bulwark`, `filter`, `eq`, `drive`, `shriek`, `echo`, `sluice`, `scape`), which names its four `fxs-<module>-ground|ink|mid|cable` tokens. The legend takes the module list, so it shows the shell's real modules and their screens.

**Anatomy** (CSS px; one weight scale in every view, the lab's)
- **Weight** `w = wf + wb`: core width 0.8 + 4.8·min(1, w), opacity 0.22 + 0.78·min(1, w). Below `MIN` = 0.003 (−50 dB) a cable is not drawn, unless it is waiting.
- **Forward part** (`wf`): a cord. The core is the SOURCE module's cable colour, `fxs-<module>-cable`; the casing is `ink`, the house ink, 1.5 px each side. Casing and core fade together (one group opacity).
- **Backward part** (`wb`, one sample late): the same cord, dashed 5 / 3.5 (casing and core share the dash), plus a `z⁻¹` chip at the arc's apex or the cord's low point: `caution-tx` on `caution-bg`, 8 px.
- **Live loop** (`loop`): a `caution` underlay 7 px wider than the core at `opacity-halo`, never a blur. `caution` is not a meaning-carrying line on cream (1.6–2.1:1), so the view's status line names every loop in words (`LIVE LOOP n`, "loop: Filter → Drive").
- **Waiting** (`held`): a dotted line in `caution-tx`, 1.6 px, 1.5 / 3.5, round caps, drawn at any weight (6.96:1 well, 14.35:1 dark). The status line says `WAITING n` and names the cables.
- **Lanes** (Body, Sub) are not modules: plain cables with no edge, Body in `t2`, Sub in `hint`. OUT is a sink and draws no cable.
- **Module mark** (a box, a matrix header, a panel face): a CARD, `panel` cream with a 1.5 px `ink` edge like every card in the window, the name in `ink`, and a small SCREEN INSET in the module's own display style (below). **Asleep**: the edge goes dashed (2.5 / 2.5) and thin; the card, the screen and the name keep full contrast. A lane (Body, Sub) and OUT are the same cream card with the name alone and no screen.
- **Unavailable** (a cable the rules forbid, the matrix only): the cell hatched in `div`, a `✕` in `t2`. A conditional cable carries its note (`E1`) in `hint`.

**Screens** (ADR-273) a module's colour is the small screen it carries, drawn in its own display style with three tokens, `fxs-<module>-ground`, `fxs-<module>-ink` and `fxs-<module>-mid`, plus one more for its cables, `fxs-<module>-cable` (ADR-273 A1, the human: "Keep cables in the page's colours"). The same hex serves light and dark, because a screen is its own display; the cream card around it follows the theme. The inset is the ground with a 1 px `ink` edge (a deviation from the comparison page, which edged it in `mid` at 0.6: on dark the grounds of Shriek, Scape, EQ and Bulwark are 1.1–1.3:1 against the card and would melt into it), and one tiny picture drawn from `ink` and `mid`. Cables, arrowheads and matrix weight bars take the CABLE colour, the comparison page's own; for Bulwark, EQ, Shriek and Scape it is the same hex as the screen ink, for Filter, Drive, ECHO and Sluice it is not (Sluice's is `#8bac0f`, the Game Boy LCD's light green, where its ink is the dark `#0f380f`). Module chips and names stay ink on cream. Shriek and Sluice are the two real display styles; the other six are STAND-INS, invented for the comparison, until their skins are chosen.

| Module | Screen style | Real or stand-in | Ground L* | Ink on ground | Ground vs panel cream / dark | Cable | Bare cable on well cream / dark |
|---|---|---|---|---|---|---|---|
| Bulwark | Vector monitor | stand-in | 0.7 | 13.98 | 19.83 / 1.24 | `#3de8ff` | 1.48 / 13.10 |
| Filter | Segment LCD | stand-in | 77.6 | 6.55 | 1.75 / 9.17 | `#6f8a5c` | 3.84 / 5.05 |
| EQ | Blue CRT | stand-in | 21.3 | 7.09 | 12.08 / 1.33 | `#9fc4ff` | 1.78 / 10.91 |
| Drive | Grey-green scope | stand-in | 29.9 | 8.13 | 8.99 / 1.78 | `#7dff9a` | 1.26 / 15.37 |
| Shriek | 80s workstation | REAL | 6.0 | 9.13 | 17.74 / 1.11 | `#f0a830` | 2.03 / 9.56 |
| ECHO | Sepia print | stand-in | 41.0 | 5.51 | 5.96 / 2.69 | `#d9b98a` | 1.87 / 10.40 |
| Sluice | Game Boy Color LCD | REAL | 71.5 | 6.02 | 2.10 / 7.65 | `#8bac0f` | 2.62 / 7.39 |
| Scape | Dusk glass | stand-in | 15.2 | 4.96 | 14.47 / 1.11 | `#ff4fa3` | 3.04 / 6.37 |

Every screen ink clears 4.5:1 on its own ground (the cable colour is a line on the well and is gated through its casing, below) (least 4.96, Scape), and the eight grounds sit at least 5 L* apart (least 5.38, Bulwark and Shriek). Both are gated: `hds-tools/fx_palette.py` refuses to write the tokens otherwise, and `fx_contrast.py` re-measures them live in both themes. The pictures are placeholders for the real skin art; Sluice's fourth Game Boy green is not a token, so its pixel grid leaves those cells as ground.

**How the cables read in both themes** a cable colour is the page's, chosen as a display colour, not for the views' ground, so seven of the eight are weak as a bare line on the white `well` (Drive 1.26, Bulwark 1.48, EQ 1.78, ECHO 1.87, Shriek 2.03, Sluice 2.62, Scape 3.04:1; only Filter, 3.84, is over 3) and none is on the dark `well` (5.05:1 or better; 4.36:1 on the dark `panel`). The `ink` casing under every core carries them: it is 17.94:1 on the cream `well` and 16.35:1 on the dark one, so every cord clears 3:1 on both wells and on both cards (tested; the `noedge` plant removes the casing and fails). A pale core (Drive or ECHO on cream) reads as an outlined cord; on dark the bare cores already read, and the light casing outlines them. The lanes: `t2` 9.46 / 9.83:1 and `hint` 5.36 / 5.36:1 on `well`, with no casing.

**Reserved colours** source colours (`src-env`, `src-lfo`, `src-macro`, `src-engine`) never appear here; they stay reserved for modulation (ADR-266 ruling 5). Corner colours do not appear either: the views carry no corner marks (below). The screens are pictures, not signals, so they keep no distance from either; a screen's pink (Scape) or green (Sluice) is a display colour. `alarm` is not used. The retired `fx-<module>`, `on-fx-<module>` and `fx-edge` tokens no longer exist.

**Corner marks** none. The views draw the cable state the morph produces, not the corners that produce it; the lab's views carry no corner glyph or ownership mark. A host that puts a morph pad beside them uses the core XYPad, which follows the house corner rules.

**Placeholders**
- **Lane colours** (Body `t2`, Sub `hint`). The lab drew them in the pad's magenta and marker. Decided by: the human's review of the views.
- **Six screen styles** (Bulwark, Filter, EQ, Drive, ECHO, Scape), every module's mini picture and, with them, the six stand-in cable colours. Decided by: the human choosing each module's skin. (The cable colour itself was decided: the page's, ADR-273 A1.)

**Sources** `docs/design/fx-chain-morph-lab-r3.html`: the shared encoding :1989–1998, `VIEW_MIN` :1999, `strokeCable` :2611–2621, the legend `renderViewLegend` :2740–2745. The lab's own palette (`MOD_INK`, :2598–2607) was a marked departure, eight hand-picked hues; ADR-266 ruling 5 replaced it with eight house fills, and ADR-273 (2026-10-23) replaced those with cream cards and screens (the comparison page, `fx-module-colours.html`, option B).
