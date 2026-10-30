# Bend law

The pitch system's travel controls: per lane, a law that decides how pitch gets where it is sent, with that law's own parameters, the scale quantiser as a modifier, and how notes glide in poly, mono and legato (B504, ADR-232, RC-3; the bend lab).

**Lanes** Bend wheel · Note glide · MPE bend · Sub. Each lane describes its travel once; the Sub has no glide of its own and follows the bend law by default (B196). The note lane can follow the bend law or keep its own settings.

**The laws** (`off (instant)`, `constant time`, `constant rate`, `lag (one-pole)`, `mass-spring`), each showing only its own parameters (legacy defaults):
- constant time: Time 5–1500 ms (120). Every move takes the same time; distance sets the speed.
- constant rate: Rate 0.5–200 st/s (24). Distance sets the time.
- lag: Lag 1–2000 ms (60). Asymptotic; never quite arrives.
- mass-spring: Spring 0.5–20 Hz (4), Damping 0–1 (0.6; 1 critical, below rings), Distance curve 0–2 (1: overshoot ∝ distance; 0 the same wobble at any distance; 2 only big sweeps bounce). The only true inertia: it overshoots and rings.
- Bend lane only: Return × 0.2–3 (1), the speed heading back to centre.

**Quantise** is a MODIFIER on any law, never a sixth law: the law runs on underneath and only the emitted pitch snaps, so spring + quantise is an overshooting autotune wobble and constant rate + quantise a stepped portamento. Modes: off · chromatic · scale (the global Scale selector; the bend lane adds scale (drag) and scale (offset)). Hysteresis 0–50 ¢ (8): a step must win by this much (fixes a spring parked on a boundary, not wide vibrato). Step timing: continuous · free (Step rate 0.2–50 Hz) · sync (Step grid 0.25–8 a beat): at most one step per gate, the glissando-run character.

**Voices** Poly: each new voice glides from its nearest predecessor (a chord, each voice from its own) when Poly glide is on. Mono: one voice glides from the last pitch. Legato: glides only between overlapping notes. Glide from: a key still held · a note still sounding (release tail included) · the last note played, even across silence.

**The consumer provides** the pitch system's manifest keys (PENDING: RC-3 defers the pitch system, so the preview names the legacy keys `bendLaw`, `bendTime`…`noteQuant`, `glideMode`, `polyGlide` only as references), the global scale, and the Bend view for the drawing.

**Never trimmed** poly glide, the spring law and scale quantise are central (B504, ADR-232).

**Voice-bound**: it needs notes; the FX device has no pitch lanes.

The preview uses plain slider rows; in the plugin these are morph sliders (Slider), with the same values.
