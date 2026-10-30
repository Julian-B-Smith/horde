# Scale selector

The one global scale: a root and a 12-bit mask of the pitch classes in it, chosen by name or drawn by hand, shown as a piano (GLOBAL) or as dominoes from the played note (OFFSET).

**The scale is global** (B260): note quantise, bend quantise, the sub, SCALPEL's cut rules and later devices all read the same `{ root, mask }`. A MIDI sidechain that sets the scale is after 1.0 (ADR-237 item 8).

**The mask is the truth, the name a convenience.** Consumers store and send `{ root, mask }` only, never a scale id, so a new named scale needs no core change. The name list reverse-matches the mask, or shows "custom". Names (the bend lab's list): chromatic, major (ionian), natural minor, harmonic minor, melodic minor, dorian, phrygian, lydian, mixolydian, locrian, major and minor pentatonic, blues, whole tone, diminished (h-w), phrygian dominant, hirajoshi, in-sen.

**Two skins, one control** (the Design System's bend + scale page): GLOBAL shows pitches as places, because they are; OFFSET flattens the twelve cells into equal dominoes labelled by interval (1, ♭2 … 7) from the note you play, the first cell the played-note puck in `value`, and greys the root selector, which means nothing there.

**Four GLOBAL visuals, to compare** (all read the same mask, toggle a note by click, and mark the root with ★ and a `marker` ring):
- **(a) Key circles** (the human's idea): twelve 28 px circles in two staggered rows laid out like white and black keys; an in-scale note is a filled ink circle, an out note a hollow one. 252 px wide.
- **(b) Piano**: the full keyboard, white keys a seventh of the width, black keys 28 px wide.
- **(c) Ring**: twelve 28 px dots on a circle, in chromatic order or, with the Fifths toggle, the circle of fifths (a major scale becomes one unbroken arc); a faint polygon joins the in-scale notes.
- **(d) Steps**: a narrow strip, one 28 px-wide cell per pitch class counted from the root; an in-scale cell rises as an ink bar, the root's name in `marker`. Below 400 px it folds into two rows of six, so no cell is under 28 px.
Under every visual a line spells the step pattern ("W · W · H · W · W · W · H").

**Rules** the mask is root-relative, so a new root transposes the lit keys; the root always stays lit; the last lit key cannot be cleared (an empty scale has no defined quantise). Every cell and key is at least 28 px in its narrow dimension; the dominoes and the steps strip fold to two rows of six below 400 px.

**The consumer provides** the global scale's state and a write path; the quantise MODE and its hysteresis live with each lane's law (Bend law), not here. There is no separate quantise "amount" in the sources.

**Accessibility** each key is a toggle button named by pitch class and state ("E, in the scale"); the selects are labelled.

**Voice-bound** in effect: a scale does nothing without notes, so the FX device has no use for it until a consumer there reads it.
