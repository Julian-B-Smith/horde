# Gate lanes

The trackers as lanes on one glass: the gate a level, the trigger ticks only (an event has no value between them), and the other trackers as the render logged them, newest on the right.

**The consumer provides** `rows` (default: the eight MIDI trackers; each `{ name, kind: 'level'|'step'|'event', bip?, key, role? }`). It then calls `push(values, fired)` once per rendered frame: values by row key, and `fired`, the triggers counted this frame. Call `draw()` to paint. It holds the last `points` frames (240).

**Anatomy**
- A `.hd-viz` glass at 2.6 : 1, one band per row.
- The label is glass text in `scr-grid`, 8.5 px. A hairline in `scr-grid` at 18% marks the band floor; a bipolar band also has a dashed zero line.
- A level is a 1.4 px line (`scr-meter` by default). A step is the same line drawn stepped.
- An event is a 1.4 px tick topped by a 2.4 px dot, in `scr-marker`. Every logged frame is scanned, so no event is decimated away.

**Use** with MIDI trackers, and inside Event lanes. It never draws a value it did not log.

**Screens** six screen roles; it follows the Screen scheme. **Reduced motion** the caller stops pushing; one still frame. **Fits** it fills its column at a fixed aspect.

**Placeholders** none of its own: the trackers' rules are MIDI trackers'.

**Sources** docs/design/shape-lab-mod.html :547 (midiC), :2319 (`lane`), :2328 (`ticks`), :2500 (`paintMidi`).
