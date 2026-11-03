# Parameter picker

A search over named groups of parameters, opened at an anchor (GUI-9 r2).

`Horde.ParamPicker(S, { groups: [{ name, items: [{ key, label, taken? }] }], title?, anchor?, onPick?(key), host? })` returns `{ el, close(), closed() }`. A `taken` item reads bold. A pick closes it.

**Anatomy** (270 px, at most 330 tall, scrolling inside): a 24 px search capsule, then each group's name over its rows, label on the left and key on the right. A search matches the label, the key and the group's name.

**Gestures** typing in the search is typed entry, the one time this takes the keyboard outside keyboard mode. Escape or a click elsewhere closes it. The host decides what's listed, and in the shell that's every parameter a route can reach.

**Sources** GUI-9, `docs/design/xy-macro-lab.html` (X1).
