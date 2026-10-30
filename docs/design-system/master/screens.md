# Screens and skins

Each FX module is software running on a screen inside horde's chassis (B322). The chassis is the same for every module, which is the coherence; the glass is each module's own, which is the individuality. The frame is the Screen frame component; this section is the skins.

## Rules for every skin

- **The chassis carries the corner colours** (ADR-259 ruling 4). Inside the glass a corner is its glyph or A–D in the skin's own ink, so each skin stays true to its medium.
- **Four soft keys and four knobs** sit under every glass. The chassis prints 1–4; the software labels them in its foot band.
- **Whole-number scale only**, `image-rendering: pixelated`, never resampled. Glass fonts are drawn as data.
- **One model, two views.** Every function reachable on the glass is reachable from the house-style DOM view, and both call the same writers. Pointer and keyboard both work on the glass; alt+wheel only.
- **A skin states its scheme policy**: it takes the screen scheme (its colours come from the `scr-` roles), or it overrides it (its colours are its medium's chemistry), and says which.
- **Mock data is labelled MOCK**, and an unsettled layout PROVISIONAL, on the chassis foot.

## The house screen

For a plain module with no skin of its own, such as a plain Drive (ruling 5). The vaporwave phosphor tube (ADR-119): `scr-ground` with traces in `scr-value`, measures in `scr-meter`, the system in `scr-physics`, attention in `scr-marker`, structure in `scr-grid` at low alpha. Glass text is `scr-physics` or `scr-grid` (both 6:1 or more); `scr-value` and `scr-meter` draw lines and marks only (ORCHID's read 4.45 and 4.499:1). Glow is a wider underlay stroke at about 22%, never a blur. ORCHID pairs with the cream chassis, TUBE with the dark; the Screen scheme selector crosses them with any of seven schemes: TUBE, ORCHID, FROST, EMBER, DUSK, and LUMEN and PAPER (retired in gui2 on 2026-08-24, restored from its history as selectable). Each scheme sets its scanline strength (`scan-…`) and alpha multiplier (`alpha-…`). Under 4.5:1 a role draws lines and marks only: ORCHID value and meter, LUMEN value, meter and marker, DUSK grid.

## Shriek: the 80s workstation (B373)

- Glass `size-ws-glass-w` × `size-ws-glass-h` (320 × 288) at ×2.
- 1-bit: paper is the scheme's ground, ink is its physics ink; greys are ordered (Bayer) dither. Exactly two colours, no vector or text drawing outside the pixel font. Takes the scheme.
- The 3 × 5 pixel font; a menu bar (* FILE PAGE INPUT AUDIO VIEW), overlapping windows, radio buttons, sliders, dialog buttons; pages stepped by the soft keys; a data-entry strip and cursor keys.
- The logo is native in the document window (one logo pixel per screen pixel); the icon is the window's proxy icon.
- Open from B373: menu bar plus data strip, or strictly four soft keys and four knobs; a lettering rule for workstation logos.

## Sluice: the Game Boy Color (B338, B361)

- Glass 160 × 144 at ×3. Drawn into a 2-bit index buffer: at most four colours in each 8 × 8 tile, at most eight palettes, colours on the RGB555 grid. Takes the scheme: its four tile palettes are built from the `scr-` roles.
- The 3 × 5 font, the LCD dot grid (a 1 px line every logical pixel at 11% black) and an inset shade; slow-pixel ghosting.
- The whole instrument runs on the glass: a MENU on soft key 1 and its pages; focus and hover drawn inside the tile rule; Tab, arrows, Enter, Esc and 1–4 when focused. One round chip shape everywhere.
- The logo is native: one logo pixel is one screen pixel. Titles follow the lettering rule (one cap top and baseline, stems within ±1 px, optical spacing, no stripes).

## Scape, Bulwark and Echo: auditions

They audition the remaining glasses (ruling 5); none is assigned yet. The candidates, with the workshop's first pairing:

| Glass | Policy | Workshop pairing |
|---|---|---|
| Segment glass (7- and 14-segment, LCD on pale, VFD on dark) | takes the scheme | reverb (Scape) |
| Nixie tubes and neon bargraphs | overrides: neon's orange is its gas | compressor (Bulwark) |
| Sonar scope (P7 phosphor) | overrides | Echo |
| Phosphor CRT terminal | overrides: the phosphor's colour | (MAW, before Shriek took the workstation) |
| Vector monitor | dark schemes only | filter |
| Teletext page | overrides: the eight teletext colours | EQ |
| Dot-matrix character LCD | takes the scheme | delay |
| CAD blueprint | takes the scheme | room |

Whatever each takes, the chassis, the four-and-four contract and the logo slots stay the same.
