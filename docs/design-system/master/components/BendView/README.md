# Bend view

The travel made visible: the pitch moving from the old note to the new under the chosen law, with the spring's overshoot and the scale's snap points.

**Looks** time across, pitch up, a row per semitone named by note. The hand's target as a dashed step (`scr-grid`); the law's own path faint in `scr-physics` when quantise is on (it runs underneath); what is heard as the beam (`scr-value`, an 8 px underlay at 22% and a 2.2 px core); the peak overshoot as a `scr-marker` dot with its bloom. With quantise on, the admitted pitches are dashed snap lines and the heard line steps between them.

**The sticker** prices the law (the Design System): lag50 (time to half the distance), over (peak overshoot in cents), settle (time to stay within 5 ¢), rings (crossings of the target).

**Fed by** the law's parameters and the global scale; it computes the curve with the same law the plugin runs (the bend lab's, at 2756 ticks a second), not a sketch. Live, it can also trace the lane's actual pitch from the voice (RC-3's published sounding pitch).

**Screens** only the screen roles, so it follows the Screen scheme. **Reduced motion** it is static; it redraws on a change. **Fits** the canvas fills its column.

**Voice-bound**: a glide needs notes. The FX device does not show it.

The preview draws a SAMPLE move (C4 → G4, C major), computed in the preview.
