# Picture picker

A stepper whose value opens a list with a picture per entry (GUI-11, ADR-291 r4, B558). One component for Arp's Order and Pattern.

`Horde.PicturePicker(el, S, { name, path, options, get(), onSet?(i), thumb(i), head?(), sub?(i), keys? })` returns `{ open(), close(), isOpen(), panel() }`. `thumb(i)` returns the entry's picture as an SVG string, read when the list opens.

**Anatomy** a 28 px capsule with ‹ and ›, and the value between them with a caret. The list is 340 px, each row at least 36 px: its name, and its picture on `scr-ground`. The current row carries the selection tint.

**Gestures** the arrows step. A click on the value opens the list. A pick is one `onSet`, so one undo step; opening and closing are no edit. Escape closes it through the layered Escape. In keyboard mode, arrows move and Enter picks.

## `Horde.ArpViz`

What an Order and a Pattern play, drawn from Arp's own definitions and never by hand. All of it is MOCK: no engine, no held notes, a sample chord.

- `orderSVG(id, seed)`: the note sequence an order makes over the sample chord, 16 steps, dashed where a cycle ends. Random and Shuffle are one sample draw.
- `patternSVG(steps, { thumb })`: a pattern's own step data. Bar height is velocity, width gate, split bars ratchets, a hollow dot a rest, a foot bar a tie, a top dot a trigger, a triangle a step offset, dim a probability.
- `rollModel(i)`, `rollSVG(model, i)`: the piano roll of what the selection would play. Time across, pitch up, a bar per note. Not drawn: scale quantize, latch, restart, the chord stage.
- `stepsOf(bank, id)`: a pattern's steps from Arp's bank.

**Sources** GUI-11, `docs/design/arp-face-lab.html` (B553, B558), Arp's SPEC 4.2, 4.3, 4.7, 5.1, 5.2, 10.
