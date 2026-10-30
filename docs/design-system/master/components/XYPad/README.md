# XY pad

Two parameters on one surface, with a puck; the morph pad is the same component with the four-way corner wash.

**The consumer provides** two manifest keys (X and Y; MAIN's pad is free to reassign), their formatters and defaults, the accessible name, and for the morph pad the four corner identities.

**Anatomy** a well on `scr-ground`, ink edge, `radius`; quarter gridlines in `scr-grid` at 30%; the puck `size-puck` (16) in `value`, 2 px ink edge, a 2 px hard shadow (flat while dragging), with a 28 px hit box. The X and Y readouts sit under the pad, never over its marks. The morph pad adds radial washes from each corner in the corner colour at about 50% fading by half the diagonal, and a filled corner chip in each corner, its glyph the same shape as that corner's pips on every knob and slider.

**Morph pad** a click on a corner chip pins that corner as the editing corner for every control (again returns to Auto); a pinned chip wears an ink outline. Pinning is global, never per parameter.

**Census ring (optional, off by default)** a toggle in the pad's options shows 12 small dots round the morph puck (the reader), in the corner colours, each corner's count its census share (the parameters it currently owns, or has flipped to), apportioned by largest remainder so the total is always 12. Dots run clockwise from 12 o'clock, A then B, C, D, on a 19 px radius, with a `mark-edge` outline. They are **plain discs, a deliberate exception to the glyph rule** (ADR-259 ruling 3, ADR-261): the ring is an optional decoration, and each corner's glyph and name stay on the pad's corner badges. When on, the puck's accessible description lists the counts.

**Behaviour** drag anywhere moves the puck there; double-click the puck resets both keys to their defaults; arrows move it in keyboard mode (shift = fine). It exposes itself as one slider whose value text names both parameters.

**Underlay** on MAIN the pad sits over the Specimen (pointer-events none beneath it). The Specimen is driven by notes, so the FX device (ADR-254) uses an audio-driven stand-in or a still pose; the pad never assumes the oscillator panels exist.
