# Spectrum

The output's spectrum in third-octave bars from 30 Hz to 16 kHz, with held peaks.

**Looks** About 27 third-octave bands, each the MAX of its bins so a peak is never averaged away; bars 75% of a slot (gap about a third of a bar). Bars rise instantly and fall at 0.28 a frame. The one gradient, mapped to level: `scr-physics` → `scr-value` → `scr-marker`. Peak caps 2.5 px in `scr-meter`, held 1.5 s then falling; a cap at 0 dBFS turns `alarm` (the one red). A 2 px floor and a dashed 0 dBFS ceiling in `scr-grid` 55%. Hover names the band's centre and edges.

**Fed by** An output analysis tap (256 log bins 30 Hz–16 kHz, v = (dB + 80) / 80). NOT in RC-8; the shell's tap feeds it. About 30 Hz.

**Voice-bound or audio-capable** Audio-capable: the FX device can use it as it is.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** gui2 `_drawSpecOne`, `SPEC_BANDS`, `sunset()`; UI Spec §6 SPECTRUM; Vaporwave Phosphor (sunset bars).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
