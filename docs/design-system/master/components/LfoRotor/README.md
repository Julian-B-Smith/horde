# LFO rotor

The coupled LFO: its members as hands on one ring, the lead as the beam, and the output as a puck on a rail.

**Looks** Four members, each a hand from the hub's edge out to the ring, with round caps at the outer ends: the lead in `scr-physics` with the beam treatment (an 8 px underlay at 25%, a 2.5 px core), the followers thin in `scr-meter`, `scr-marker`, `scr-grid`; a short trail (the frame washes at 30%). The output puck on the right rail in `scr-value` with its bloom. Coupling reels the hands together, which reads as brightness stacking.

**The hub** A disc at the centre, drawn last over the hands: opaque `scr-ground`, outlined in `scr-grid`, its radius 1.5× the widest hand (the lead's 8 px underlay, so 12 px). No hand, glow or trail ever reaches the axis, so the wide strokes never pile up or cross there (the human, 2026-10-21).

**Fed by** The modulator's own state: its members' phases and its output. Modulators are not engine readouts (the engine computes no LFO, IF-3), so the modulator host feeds it. 30–60 Hz.

**Voice-bound or audio-capable** Runs without notes: a free LFO is a source of its own, so the FX device can show it.

**Screens** drawn only with the six screen roles, so it follows the Screen scheme; glow is a wider underlay stroke or disc, never a blur; scanlines at the scheme's strength.

**Reduced motion** one still frame, redrawn only when its data or the scheme changes; no trails.

**Fits** its canvas fills the column's width at a fixed aspect and draws at the display's pixel ratio (capped at 2), so it never runs past its card.

**Sources** UI Spec §6 LFO ROTOR; Vaporwave Phosphor (the lead beam).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
