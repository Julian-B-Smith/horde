# Logos

The locked horde wordmark, DROP + INSET + HUE ↘ (B368, locked B375): a baked "horde" warped by td::Warp, filled with the morph colour of the moment, an edge turned 120°, an inset shade, a crisp drop shadow, and a 150° diagonal hue gradient. Each PNG is 182 × 120 (the 2× store of the 91 × 60 header box), transparent, and was made by running the SCALPEL lab's own logo code (warp seed 7) and saving its canvas. It was not redrawn.

- `horde-a-light.png`, `horde-b-light.png`, `horde-c-light.png`, `horde-d-light.png`: the mark with the morph cursor on corner A, B, C or D, on the cream chassis.
- `horde-a-dark.png` … `horde-d-dark.png`: the same on the dark chassis. Only the drop shadow's ink changes.
- `horde-static-light.png`, `horde-static-dark.png`: the reduced-motion mark (unwarped; the drift's colour at rest). Use it wherever motion is off, and for any still use such as an About box or an installer.

Rules: show the mark at 91 × 60 CSS px (whole-number scale only), in the frame header's top-left slot, never on a well or a screen. Never recolour, redraw, re-set it in Archivo or crop it. In the plugin it is live: the colour follows the morph cursor (or a 50-second hue drift when morph is off), R drives the ripple and detune the wiggle, and it reseeds only per launch. The warp engine is `docs/design/logo/text_distortion.hpp` in the repo.

**In the bundle:** `Horde.MainStage.logoCanvas()`, the bench's warp and paint verbatim (ADR-333), in the locked style. The bench's click-to-cycle and its stored style are not carried over (GUI-2's reading).

## The Mindlathe mark

- `mindlathe-saturated.png`: the Mindlathe wordmark, cyan to magenta and shaded, on a transparent background, 960 × 719. It is the human's own header graphic (the saturated master from the Mindlathe brand's masters, 1492 × 1118), scaled to 960 px wide with every chunk but IHDR, IDAT and IEND dropped. It was not redrawn. The human picked it for GUI-54's splash on 2026-11-08.
- **Where it shows:** only in `Horde.Splash`, which carries it in the bundle as base64, since the plug-in's page can load no image. `tools/splash_art_check.py` holds the two to the same bytes.
- **Rules:** never recolour, redraw or crop it.
