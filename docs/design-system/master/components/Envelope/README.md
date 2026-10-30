# Envelope

An envelope's shape, every stage bent by its own curve, on the phosphor floor, with the playhead of a sounding voice.

**Looks** Attack, decay, hold at sustain and release; each stage's curve u from −2.5 to +2.5 (0 straight, +2.30 the bank's exponential), drawn by the engine's law (SPEC-ENGINE C.12: c = 4u, y = (1 − e^(−c·t/T)) / (1 − e^(−c))), the same unit and law as the modulators' Envelope editor and shape editor. The curve in `scr-value` (2.5 px core, 8 px underlay at 22%) over an 18% fill; a receding floor grid in `scr-grid` 22%. Node handles: 16 px hit (10–16 drawn), `scr-marker` (attack peak), `scr-meter` (decay end), `scr-physics` (release start), `scr-grid` (end); the UI Spec's fourth, celebrate, has no screen role, so grid takes its place (OPEN). The playhead a `scr-physics` dot. Double-click a node: reset it.

**Fed by** The envelope's parameters (manifest keys `env1.attack`…`env1.releaseCurve`); the playhead from RC-8 (per voice, its envelopes' stages).

**Voice-bound or audio-capable** The shape draws without notes; the playhead is voice-bound.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** UI Spec §6 ENVELOPE; Vaporwave Phosphor (horizon grid, the sun node); the envelope hierarchy lab's curves (B377).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
