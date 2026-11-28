# Screens and skins

Each FX module is software running on a screen inside horde's chassis (B322). The chassis is the same for every module, which is the coherence; the glass is each module's own, which is the individuality. The frame is the Screen frame component; this section is the glasses, and the rules list a module's design is reviewed against (ADR-409 ruling 2).

## Who designs what (ADR-409)

Each module designs its own glass and its pages, for horde's slot, the FX device and its own standalone, as Sluice does (ADR-400). horde keeps the chassis, these rules and each glass's GUI-58 marks, builds the house-style DOM view from the module's writers (the accessible view and the backup UI, ADR-235 ruling 1), and reviews each revision against this list without redrawing it. A module whose glass can't carry something essential says so on its glass thread, and the human rules.

| Module (most complex first) | Glass | Restrictiveness (1 = freest) |
|---|---|---|
| Sluice | Game Boy Color | 6 |
| Shriek | 80s workstation | 2 |
| Kiln EQ | Vector monitor | 5 |
| Echo | Sonar scope | 4 |
| Scape | CAD blueprint | 3 |
| Bulwark compressor | Nixie tubes and neon bargraphs | 9 |
| Mangle | Phosphor CRT terminal | 7 |
| Kiln Drive, Weir FX filter | House screen (one rules list, two layouts) | 1 |

The Master limiter (Bulwark's) has no glass: it lives on horde's master or mixer page. The segment glass, the dot-matrix character LCD and teletext have no module.

## Rules for every glass (G1–G16)

| # | Rule | Where it is ruled |
|---|---|---|
| G1 | The frame is horde's: the chassis, the module tab, the state dot and the four corner chips. The design is the glass inside it. | ScreenFrame; ADR-400 r1 |
| G2 | Whole-number scale only, never resampled. A pixel glass draws a logical bitmap with `image-rendering: pixelated`; a stroke glass (vector, CAD, sonar, nixie) draws in the 160 × 144 unit space at display resolution. Glass fonts are drawn as data, never a system font. | ScreenFrame; GUI-58 |
| G3 | Four soft keys (at least 44 × 28) over four knobs under the glass. The chassis prints only 1–4; the glass labels them in its foot band. | ScreenFrame |
| G4 | Keyboard mode is opt-in and off by default; off, no control takes focus and every key reaches the host. Pointer and keyboard both work on the glass. | ADR-259 r6; Accessibility |
| G5 | A plain wheel never moves a value; Option + wheel does. | GUI-15 |
| G6 | A control that can't act under the current settings isn't drawn: never greyed, no space kept for it. Only a switch or mode (a stepped key) hides a row; a row gated by a continuous value at the end of its range (a level, depth or mix at 0) stays drawn, plainly. The module's live-when data marks each gate stepped or continuous. | GUI-21; GUI-77 r1 |
| G7 | The corner colours stay on the chassis. Inside the glass a corner is its glyph or A–D in the glass's own ink, never a corner hue. | ADR-259 r4 |
| G8 | The glass's GUI-58 marks: corners above a control's track, modulation below it. Each glass keeps its own grammar, and a glass whose mark shows no weight doesn't gain one. | GUI-58, A1 items 1–2 |
| G9 | One model, two views. Everything on the glass is reachable from the house-style DOM view, and both call the module's writers, keyed by manifest key. The module lists its writers (a list of actions) beside its design. | ADR-400 r1; ADR-235 r1 |
| G10 | Controls bind to manifest keys: name, law, range, default and unit come from the manifest. | README rule 1 |
| G11 | The glass states its scheme policy: takes the scheme (its inks are the `scr-` roles), overrides it (its inks are its medium's chemistry), or dark only. | this section |
| G12 | Mock data is labelled MOCK, and an unsettled layout PROVISIONAL, on the chassis foot. | this section |
| G13 | Reduced motion: a picture that moves (a sweep, a beam, a scroll) has a still form that loses no value. | Accessibility |
| G14 | Every screen renders alone, with no oscillator panels (ADR-254): a note- or audio-driven picture has a still pose with no signal. | README rule 2 |
| G15 | Rows are PROVISIONAL until GB33 (bezel lamps) is ruled: declare each row's band (its top and height in glass units, and the row it drives) as data beside the writers; assume no fixed row pitch and no line-up with the four knobs. | ADR-409 reading; GUI-58 A1 item 3 |
| G16 | During a switch's fade, rows follow the switch's target value, not the audible mix: they change once and never blink. | GUI-77 r2 |

## The house screen: Kiln Drive and the Weir FX filter

- The vaporwave phosphor tube (ADR-119), 160 × 144. Takes the scheme: `scr-ground` with traces in `scr-value`, measures in `scr-meter`, the system in `scr-physics`, attention in `scr-marker`, structure in `scr-grid` at low alpha. No colour outside the six roles.
- Glass text is `scr-physics` or `scr-grid` (both 6:1 or more); `scr-value` and `scr-meter` draw lines and marks only (ORCHID's read 4.45 and 4.499:1). Under 4.5:1 a role draws lines and marks only: ORCHID value and meter, LUMEN value, meter and marker, DUSK grid.
- Glow is a wider underlay stroke at about 22 %, never a blur.
- ORCHID pairs with the cream chassis, TUBE with the dark; the Screen scheme selector crosses them with any of seven schemes (TUBE, ORCHID, FROST, EMBER, DUSK, LUMEN, PAPER), each setting its scanline strength and alpha multiplier.
- Marks: **Pips in one ink** above (glyphs sized by weight, the owner larger in `scr-marker`); **the chassis's lane** below (range, origin dot, live tick, in `scr-meter`).
- Two modules share this list, not a layout. Where their idioms differ, horde's GUI session settles it.
- The house screen is a glass. It is not the house-style DOM view (G9), which horde builds for every module.

## Shriek: the 80s workstation (B373)

- Glass 320 × 288 (`size-ws-glass-w` × `size-ws-glass-h`) at ×2.
- 1-bit: paper is the scheme's ground, ink its physics ink; greys are ordered (Bayer) dither. Exactly two colours, no vector or text drawing outside the pixel font. Takes the scheme.
- The 3 × 5 pixel font; a menu bar (* FILE PAGE INPUT AUDIO VIEW), overlapping windows, radio buttons, sliders, dialog buttons; pages stepped by the soft keys; a data-entry strip and cursor keys.
- Marks: **Bitmap glyphs** above (the chassis's four pip shapes as 5 × 5 bitmaps, hollow, the owner's solid); **Dither band** below (50 % dither over the range, closed at both ends, a full-height XOR tick at the output). Weights don't show, and one route fits the band.
- The logo is native in the document window (one logo pixel per screen pixel); the icon is the window's proxy icon.
- Open from B373: menu bar plus data strip, or strictly four soft keys and four knobs; a lettering rule for workstation logos. Shriek's lab is parked (GB14) until its session takes the brief up.

## Sluice: the Game Boy Color (B338, B361)

- Glass 160 × 144 at ×3. Drawn into a 2-bit index buffer: at most four colours in each 8 × 8 tile, at most eight palettes, colours on the RGB555 grid. Takes the scheme: its four tile palettes are built from the `scr-` roles.
- The 3 × 5 font, the LCD dot grid (a 1 px line every logical pixel at 11 % black) and an inset shade; slow-pixel ghosting.
- The whole instrument runs on the glass: a MENU on soft key 1 and its pages; focus and hover drawn inside the tile rule; Tab, arrows, Enter, Esc and 1–4 when focused. One round chip shape everywhere.
- The logo is native: one logo pixel is one screen pixel. Titles follow the lettering rule (one cap top and baseline, stems within ±1 px, optical spacing, no stripes).
- Sluice's own list (its seq 88, rows A1–A13) stands beside G1–G16; where they differ, G1–G16 wins and the difference is raised on its thread.

## Kiln EQ: the vector monitor

- Glowing strokes on black in the 160 × 144 unit space. Any smooth curve or line; text stroked in a 14-segment font. Five inks: value, meter, physics, marker, grid.
- Never a fill or a bitmap: a meter is an outline or a hatch. Outlines under 2 units are hard to tell apart.
- Dark on every scheme (GUI-77 r3): on a dark scheme it takes the scheme's inks; on a pale scheme (the cream chassis is the default) it still draws black with TUBE's inks.
- Marks: **Outline glyphs** above (the owner's gets a crosshair down to it); **Beam retrace** below (a dashed range line, a beam dot with a fading trail). No weights.
- The logo is a coloured overlay, not drawn by the beam.

## Echo: the sonar scope

- 160 × 144 units at display resolution. Overrides the scheme: one P7 phosphor hue at about four intensities (ground, dim, mid, hot white-green) plus a ring tone. No second hue: two series differ by brightness or shape.
- A plan-position display with a turning sweep and its fading wedge, blips by range and bearing, A-scope traces over grass, filled bars, an eight-line readout. Persistence is the medium.
- The sweep reveals values, so a value on the round picture can lag by up to one turn. Under G13 the still form shows every value without the sweep.
- Text is the 3 × 5 font only.
- Marks: **Pips on the trace** above (a deflection as tall as the corner's weight, its letter on top, the owner's white-hot); **Afterglow** below (a dot trailing a smear, gate ticks at the range's ends). One of three glasses whose mark shows weight: keep it.
- The workshop's mock (a tap swarm) is not Echo's: start from Echo's own keys.

## Scape: the CAD blueprint

- 160 × 144 units at display resolution. Takes the scheme: the sheet is the tube, the pens are its roles (grid, line, dimension, value, ray, text).
- Single-pen hairline drawing: lines, arcs, dash patterns, polylines, dimension lines, leaders, a crosshair, a properties palette, a command line. A dotted snap grid every 4 units; one faint tint (14 %) for a palette header.
- Never a solid fill, a bitmap or glow: a meter is outlines, hatching or ticks. The plan is schematic, with no metres.
- Text drawn as data (a stroke font, as the logo is, or the marks lab's dot lettering), never `ui-monospace` as the workshop's mock did.
- Marks: **Balloons** above; **Phantom limits** below. No weights.

## Bulwark compressor: nixie tubes and neon bargraphs

- 160 × 144 units. Overrides the scheme: one neon orange, lit or glowing a little less, with unlit cathodes visible.
- Nixie tubes (a fixed cathode stack, one lit), neon bargraphs, neon lamps, a 22 × 16 lamp matrix for the transfer curve (input −48 to 0 dB across), printed legends; letters fall back to bent-wire 14-segment strokes.
- One hue. Nothing finer than the lamp matrix; two lamps closer than 3 units merge.
- Marks: **Lamps on a scale** above (owner, position and weight); **Second column** below (range and live output).
- The workshop's compressor draws the old lab compressor and fills the whole glass with no rows: it is a starting picture, not a layout.

## Mangle: the phosphor CRT terminal

- A grid of 64 × 36 text cells on 160 × 144 (each 2.5 × 4 units, one character). Overrides the scheme: one phosphor hue (the labs draw P3 amber; Mangle proposes its own).
- Four cell attributes: normal, dim, bright, inverse. The 3 × 5 font, box drawing, ░▒▓ and eight block heights ▁ to █. Position is only as fine as a column; one character per cell.
- Persistence and bloom are optional; a still frame must read on its own.
- Marks: **Letter ruler** above (each corner's letter at its column, the owner inverse; no weights); **Shade band** below (▒ over the range, [ and ] at its ends, █ at the output).

## Not assigned

The segment glass (7- and 14-segment; takes the scheme), the dot-matrix character LCD (takes the scheme; its eight custom characters are full with GUI-58's marks) and teletext (overrides: the eight teletext colours) have no module. Their workshop pages stay as research.

Whatever each takes, the chassis, the four-and-four contract and the logo slots stay the same.
