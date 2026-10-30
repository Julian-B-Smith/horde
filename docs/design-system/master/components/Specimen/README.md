# Specimen

MAIN's visualizer: a liquid-chrome pearl that sound strikes, drawn under the XY pad (ADR-139, CHROME-001; ported to the XY's underlay in B315). Off by default.

**What it shows** a raymarched mass of three smoothly blended metaballs, matcap light and thin-film iridescence (the "pearl" material, film 500 nm at rest). Sound drives it:
- a fresh note-on strikes it: a dent spring plus a ripple front, placed at random within view and then fixed to the mass; wavelength from pitch (6 to 24); four fronts at most, a held note keeps radiating;
- the loudest envelope blooms the film from 480 to 730 nm;
- coherence R pulls it taut (surface tension 0.45 → 0.15 as R rises, eased 280 ms tighter / 500 ms looser);
- held output level makes it quiver (eased in 250 ms, out 600 ms).
Pointer: drag orbits, click strikes. Its ground is `scr-ground`, so it sits in whichever screen scheme is on.

**Feeds** the per-voice note gates and envelope levels, R, and the output peak, from the snapshot the bridge already ships (RC-8 readouts); about 20–30 Hz. Never in the audio path.

**Off by default** (a GUI preference, never saved with the patch, ADR-147). Off means absent: no GL context is created until it is shown. When nothing sounds and nobody touches it, it draws one full-resolution frame and stops (the "plate"). Its render scale adapts to the machine, never its frame rate.

**Voice-bound.** It needs notes and voices. The FX device (ADR-254) has neither, so it needs an audio-driven stand-in: the preview's "audio onsets" mode is a PROPOSAL (an onset follower strikes, level blooms, R rests at 0.5), not a ruling.

**Reduced motion** one still frame; it redraws only when orbited, struck or re-schemed.

**Accessibility** it is decoration under the XY pad: the pad, not the Specimen, carries the values and the focus. If WebGL is refused, the well shows "SPECIMEN UNAVAILABLE" and why.

The preview runs gui2's shader verbatim, driven by a SAMPLE note sequence.
