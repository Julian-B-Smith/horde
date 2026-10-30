# Waveform

One period of the output, triggered so the shape stands still: the left channel as the beam, the right as a dashed reference.

**Looks** The trigger is the fundamental's last rising zero-crossing after a 2-pole (below 150 Hz) or 4-pole low-pass at 1.1 f0; the window is one period. The left trace is the beam: an 8 px underlay at 22% and a 2.2 px core, its colour mixing `scr-value` toward `scr-marker` as R rises. The right channel is a dashed `scr-meter` line at 65% (1.4 px, 2/3 dash). A centre line in `scr-grid` 40%.

**Fed by** An output audio tap and f0. NOT in RC-8 (the snapshot carries no samples): the shell's output tap feeds it. About 30–60 Hz.

**Voice-bound or audio-capable** Audio-capable: it reads any audio, so the FX device can use it as it is (triggering on its own pitch tracker).

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** gui2 `paintScopeInto`; UI Spec §6 WAVEFORM; Vaporwave Phosphor (beam + afterglow).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
