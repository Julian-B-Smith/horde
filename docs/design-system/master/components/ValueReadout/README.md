# Value readout

The number beside or under a control: mono, tabular, right-aligned; double-clicking it opens typed entry (B449).

**The consumer provides** the manifest `key` it reads and writes, the shell's formatter and parser for that key's unit (Hz, kHz, dB, %, ms, s, st, cents, oct, choice labels), its range, and the accessible path. The readout never formats or parses by itself in the plugin: the shell owns both (B449 R9).

**Look** `value` style (Roboto Mono 10 px, 9.5 px floor) in `ink`, in a 34–58 px column. Hover shows a dashed `div` capsule so the target is discoverable. The hit box is at least 28 px tall.

**Typed entry**
- Double-click opens an inline field in place (`well` ground, ink edge, the focus ring), the current value selected. This is the one time the editor holds the keyboard outside keyboard mode (B449 R1).
- Units are optional: "2k", "2 kHz" and "2000" are the same; "-inf" is −∞ dB.
- Enter commits as one gesture and one undo node. Esc cancels. Leaving the field cancels.
- Bad input stays open and red (`alarm` edge and text, plus a one-line reason in `alarm`); nothing is written.
- Out of range is clamped, written, and shown: "40 k → 20.0 kHz" in `hint` beside it.

**Don't** commit on every keystroke, or reformat what the user is typing. The red error is the one use of `alarm` outside PANIC, clip and mute, taken from B449's design; it lasts only while the field is open.

**Dense and inline** (B589). In a `.hd-dense` container (or `.hd-sm`) the readout's face is 20 px (`size-face-dense`) with 4 px padding and a 28 px hit box; the entry field matches (68 px wide). A sized knob or slider has no readout cell at all: its value replaces its label on hover, drag or focus (knob) or sits in an 8ch column (slider), and the section's readout slot carries the second line. Double-clicking that label line or value column is the typed-entry target, as the classic readout is.
