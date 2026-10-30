# Goniometer

The stereo image: mid up, side across, with the correlation that says whether it survives mono.

**Looks** the left/right Lissajous rotated 45° (mid up, side across) drawn as an OPEN polyline, never closed and never filled (the human, 2026-10-21; the UI Spec's 25% fill is withdrawn): a 1.5 px core in `scr-value` over a 6 px round-joined underlay at 22% (the glow). Joins and caps are round, the miter limit clamped, and points closer than 0.5 px dropped, so tight turns cannot spike; consecutive points stay continuous, with no jump at the wrap. Checked: no pixel farther from the trace than the underlay's half-width (plus antialiasing) is painted, with a filled path planted as the must-fail control. The mono-safe diamond dashed in `scr-grid`. A correlation bar grows from the centre in `scr-meter`; below −0.2 it turns `scr-marker` (the UI Spec says red, but the screen has no red role and `alarm` is reserved for clip and PANIC: OPEN).

**Fed by** An output audio tap (stereo). NOT in RC-8; the shell's tap feeds it. 30–60 Hz.

**Voice-bound or audio-capable** Audio-capable: the FX device can use it as it is.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** UI Spec §6 GONIOMETER.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
