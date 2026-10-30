# Logos

The locked horde wordmark, DROP + INSET + HUE ↘ (B368, locked B375): a baked "horde" warped by td::Warp, filled with the morph colour of the moment, an edge turned 120°, an inset shade, a crisp drop shadow, and a 150° diagonal hue gradient. Each PNG is 182 × 120 (the 2× store of the 91 × 60 header box), transparent, and was made by running the SCALPEL lab's own logo code (warp seed 7) and saving its canvas. It was not redrawn.

- `horde-a-light.png`, `horde-b-light.png`, `horde-c-light.png`, `horde-d-light.png`: the mark with the morph cursor on corner A, B, C or D, on the cream chassis.
- `horde-a-dark.png` … `horde-d-dark.png`: the same on the dark chassis. Only the drop shadow's ink changes.
- `horde-static-light.png`, `horde-static-dark.png`: the reduced-motion mark (unwarped; the drift's colour at rest). Use it wherever motion is off, and for any still use such as an About box or an installer.

Rules: show the mark at 91 × 60 CSS px (whole-number scale only), in the frame header's top-left slot, never on a well or a screen. Never recolour, redraw, re-set it in Archivo or crop it. In the plugin it is live: the colour follows the morph cursor (or a 50-second hue drift when morph is off), R drives the ripple and detune the wiggle, and it reseeds only per launch. The warp engine is `docs/design/logo/text_distortion.hpp` in the repo.
