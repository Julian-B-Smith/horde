# fx-logos — drop-in pixel logos for MAW, Sluice and the rest of the FX rack

Read by `docs/design/fx-screens-workshop.html` (B322, B326, B336), the workshop that shows MAW,
Sluice and (since B336) every other FX module as software running on screens inside horde. Until a file exists, the page draws a
labelled placeholder in code. No image is committed here unless the human has made it.

## The canvases (logical pixels)

| file | size | tiles (8×8) | what it is |
|---|---|---|---|
| `maw.png` | 96 × 24 | 12 × 3 | MAW title strip (the header of every screen) |
| `maw-icon.png` | 16 × 16 | 2 × 2 | MAW square icon (top-right corner; a window's proxy icon) |
| `sluice.png` | 96 × 24 | 12 × 3 | Sluice title strip |
| `sluice-icon.png` | 16 × 16 | 2 × 2 | Sluice square icon |

Draw one file pixel per logo pixel. On the Game Boy Color treatment one logo pixel is one
screen pixel (the screen is 160 × 144).

## The palette rule (Game Boy Color, as on the real hardware)

- At most **4 colours in each 8×8 tile**. A fully transparent pixel counts as one of the
  four, as a sprite's colour 0 does.
- At most **8 palettes** across the whole logo. The page counts them by greedy packing, which
  gives an upper bound.
- Colours on the **RGB555** grid: 5 bits a channel, so each 8-bit value is `(k << 3) | (k >> 2)`
  for k = 0…31 (0, 8, 16, 24, … 239, 247, 255). Colours off the grid are shown as drawn, and
  the page reports how many pixels real hardware would round.

The workshop checks a dropped file against all three rules and prints the result in its
"Logo canvases" box.

## The lettering rule (B343)

A GBC title spells a word, so its letters share one set of metrics. The workshop measures
them from the pixels of each lead proposal (self-check 15; the Sluice lab's C27 does the same
for its copy), with these tolerances:

- **Cap top and baseline:** the same row for every letter (0 px).
- **Stem width** (the usual horizontal run) and **bar thickness** (a flat horizontal stroke):
  within ±1 px of the word's. A diagonal can read a pixel off a vertical stem.
- **Spacing is optical, not measured.** Per row of the cap band, the blank between two
  letters' boxes plus how far each letter's edge recedes from its box, each recess counted up
  to 3 px, averaged. Across the word these effective gaps span at most 1 px. So an I gets the
  widest box gap and an L's open side the narrowest.
- **No stripes:** down any stroke the fill never goes back to a lighter band, and no single
  row of outline crosses a stroke. Every letter is one piece. Every pixel is on the palette:
  no anti-aliasing.

The check does not bind a dropped file (the human's art is the human's). It is a guide for
drawing one.

## The drop-in rule

1. Save the PNG here under its exact name (above).
2. Reload the workshop. Serve it with `python3 tools/serve_labs.py`, because the palette
   check cannot read pixels from a `file://` page.
3. Each file is shown at the **largest whole-number scale** that fits its slot, with
   `image-rendering: pixelated`. It is never resampled to a fractional size. Art larger
   than the canvas is shown at ×1 and clipped.
4. Delete a file to get the placeholder back.

## Lead proposals (B326)

Until you drop a file, the workshop can show a **lead proposal** in each slot: one MAW mark and
one Sluice mark per screen treatment, title and icon, each drawn in that medium's own terms
(pixel art for the GBC, character cells for the CRT, strokes for the vector monitor, 1-bit for
the workstation, segments for the glass, mosaics for teletext). They live as data inside the
workshop page, not as files here.

- The **LOGOS** toggle picks what an empty slot shows: **PROPOSALS** (the default), **FILES**
  (a placeholder until your file exists), or **PLACEHOLDERS** (the canvas guides only, even
  where a file exists).
- A file you drop here **always wins** over a proposal. The page only reads this folder.
- Each proposal has **export PNG** links (at the sizes above, and at ×4) in the workshop's
  LEAD LOGO PROPOSALS box, to use as a starting point. To use one, rename it to the exact file
  name above and save it here. The GBC exports pass the palette rule; the glass media's exports
  carry their ground.

## The rest of the rack (B336)

The workshop's THE RACK view gives every other FX module its own glass, and each has the same
two slots. The files are named by the module's id, not by a proposed name, so they stay put
whichever name the human picks:

| module | glass (proposed) | title | icon |
|---|---|---|---|
| Drive | Game Boy Color LCD | `drive.png` | `drive-icon.png` |
| Filter | vector monitor | `filter.png` | `filter-icon.png` |
| EQ | teletext page | `eq.png` | `eq-icon.png` |
| Comp | nixie + neon | `comp.png` | `comp-icon.png` |
| Echo | sonar scope | `echo.png` | `echo-icon.png` |
| Delay | dot-matrix LCD | `delay.png` | `delay-icon.png` |
| Room | CAD blueprint | `room.png` | `room-icon.png` |
| Reverb | segment glass | `reverb.png` | `reverb-icon.png` |

The sizes are the same (TITLE 96 × 24, ICON 16 × 16), and so is the drop-in rule: a file here
always beats the lead's proposal. The palette rule above is the GBC's, so it binds `drive.png`.
On the dot-matrix LCD one logo pixel is one LCD dot, at a pitch of 6 × 9 (5 × 8 dots and a
gutter), so art drawn to that grid lines up with the panel's cells.

The sizes are a proposal from the workshop, not a ruling (B322 is a comparison lab).
