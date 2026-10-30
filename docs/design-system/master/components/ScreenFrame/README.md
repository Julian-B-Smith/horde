# Screen frame

The chassis that holds an FX module's screen: horde's card around the module's own software, so each module keeps its look while the rack stays one instrument (B322, B336).

**Anatomy**
- Chassis: `panel`, 1.5 px ink edge, `radius-window` (12), `shadow-rest`. Header: the module tab (ink capsule, `bg` text, mono 9.5), the state dot, and the four corner chips at the right. The chassis carries the corner colours (ADR-259 ruling 4).
- Bezel: `well-alt`, ink edge, `radius`, `space-chassis-y` (8) padding.
- Glass: a logical canvas (`size-glass-w` × `size-glass-h`, 160 × 144; the workstation 320 × 288) shown at a whole-number scale with `image-rendering: pixelated`, never resampled; content-box so a logical pixel lands on whole CSS pixels; ink edge, `radius`.
- Logo slots inside the glass: TITLE `size-title-slot-w` × `size-title-slot-h` (96 × 24) and ICON `size-icon-slot` (16 × 16), PNGs at a whole-number scale; a labelled placeholder until the file exists.
- Controls under the glass: four columns across its width, a soft key (at least 44 × 28) over a knob. The chassis prints only the numbers 1–4; the software labels them in the glass's foot band, each label over its key and knob.

**The contract** every screen's face is four soft keys and four knobs; everything else lives on the glass. One model, two views: the glass and the house-style DOM view drive the same state through the same writers, and every function is reachable from both (B361, B373). The DOM twin is the accessible view and the house-style backup UI (ADR-235).

**The consumer provides** the module's skin (see the Screens section), its four knob keys, its soft-key actions, its logo files, and its glass renderer.

**Inside the glass** the skin's own palette and fonts; a corner appears only as its glyph or A–D. A plain module with no skin of its own (Drive) uses the house screen: the phosphor tube on `scr-ground`, ORCHID on cream and TUBE on dark, text in `scr-physics` or `scr-grid`, traces in `scr-value` with a glow underlay.
