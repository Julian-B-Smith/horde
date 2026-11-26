# FX routing

Routing on the FX page (GUI-75, the human's ruling of 2026-11-26, from `docs/design/fx-routing-lab.html`): **a ladder of separated wires beside a send matrix**, both reading one wire model and editing it through one writer. It supersedes GUI-41's drawing (FX patch on FX rack) and its law (no loop at rest), which ADR-411 r1 replaced: return wires are real.

**The model, `Horde.FxWires`, pure** (no DOM, no clock):
- `nodes`: `[{ id, kind: 'in' | 'mod' | 'out', name, short, style, mock? }]`, in the row's order: the inputs, the modules in house order, Out last. A module's `style` is its screen key (FX cables); an input's is `lane` (`t2`) or `lane2` (`hint`).
- `wires`: `[{ u, v, lv: [A, B, C, D] }]`. Each corner's level is in (0, 1], or `null` where the wire doesn't exist there.
- **The one writer** is `setLevel(u, v, corner, L)`. A level of 0 or less (or `null`) removes the wire at that corner, so **0 % is never a wire's state** (r5). A wire absent at every corner leaves the model. Heard is never written.
- `can(u, v)` refuses a box feeding itself, anything from Out, and anything into an input, with the reason.
- `heard(w, weights)`: the wire exists where the dominant corner holds it (the quantum boundary, ADR-377), and its level blends over the corners that hold it, by the pad's weights. This is GUI-75's reading until the bridge publishes its heard value (B715).
- `shown(corner, weights, prevent)`: the wires drawn, each `{ w, L, held }`. With Prevent feedback on, a loop's **back edge** (a return wire whose destination already reaches its source forward) is held at zero (ADR-411 r1, GN-62).
- `FxWires.ladder(model, items, hostWidth)`: **everything the view draws**, pure: the layout (`FxWires.layout`), the frame, every wire's screen points and the level chips (`FxWires.placeChips`, `FxWires.chipText`). `FxWires.measure(lay, points)` holds the lab's instruments: shared ports, collinear overlaps, crossings, band violations and the closest two ports.
- **The check:** `tools/labharness/fx_routing_check.mjs` (`./verify fast`) runs this block of the vendored bundle alone. It holds the drawing to the law on this preview's sample, at every corner and at Heard, with Prevent feedback off and on, at four widths. It also holds the model's laws, with must-fail controls.

**The ladder (r1, W1)**
- One row of boxes, Out last: a module is FX cables' cream card with its screen; an input or Out is the plain card. An input not yet real carries `MOCK`.
- Forward wires run **above** the row, return wires **below** it.
- **Tracks:** per direction, shortest span first; each wire goes one track above every placed wire it overlaps. Nested wires sit inside, and wires that don't overlap share a track.
- **Ports, every wire its own:** a box's left half holds forward arrivals (top edge) and return departures (bottom); its right half holds forward departures and return arrivals. Higher tracks sit nearer the centre, so nested wires never cross at a box. A half with no partner uses the whole edge.
- Tracks sit one level chip apart (13 + 2 px), so no two chips cover each other. Each chip slides along its track to the first free spot.
- **A wire** is a cord: the core in the source's cable colour (`fxs-<module>-cable`, ADR-273 A1), cased in `ink` 1 px each side; an input's core is `t2` or `hint` with no casing. Core width 1 + 2.5·L and opacity FX cables' 0.22 + 0.78·L, so its width and strength are its level. Corners are rounded at 3 px, and an arrowhead lands on the box edge.
- **A return wire** is the same cord dashed 5 / 3, and its chip reads `z⁻¹ 20 %` in `caution-tx` on `caution-bg`: one sample late (r6).
- **A held back edge** is FX cables' waiting line (dotted `caution-tx`), and its chip reads `held 0 %`.
- **The level chip:** `panel` with a 1 px `ink` edge, mono 9 px; selected, a 2 px `focus-ring` edge, and the wire gets a `focus-ring` halo.
- The band hints `forward ↑` and `return ↓ one sample late` (the second only while a return wire shows): `hint`, 8 px.

**The send matrix (r2)**
- Rows are sources (the inputs and the modules), columns are destinations (the modules and Out). `from ↓ to →` in the corner.
- Headers are small cards: `panel`, 1.5 px `ink` edge, a swatch of the module's screen ground. Row headers take full names and column headers short ones.
- **A cell:** `well`, edged in `hair`, or in `ink` when live. A live cell shows its level (`100 %`, mono 11 px) and a bar along its foot in the source's cable colour, edged in `ink`. A box's own cell is filled `hair`.
- **Pips** for the four corners: each corner's glyph, filled in its colour where the corner holds the wire and a `hair` outline where it doesn't.
- A return cell carries `z⁻¹` at its top right. A held one strikes its level through and reads `held` there instead.
- Under Heard the levels are italic and the cells refuse edits.

**Gestures**
- **Show** (house pills): A, B, C, D or Heard. Edits go to the corner shown. Heard is the pad's blend and is never edited: an edit there is refused, in words.
- **Make a wire (r3):** drag from one box onto another. It lands at the default, **100 %**, at the shown corner. A drop on nothing, on an input or on its own box makes none; a drop on Out from Out is refused. An existing wire is selected and its level said.
- **A level:** drag a chip or a cell vertically, in either view: 150 px is 100 %, and Option makes it a quarter as fine. **Dragging to 0 removes the wire at that corner** (r5); its other corners keep their levels.
- **A double-click** on a cell or a chip sets 100 %, on a live cell or an empty one (r4).
- **Keys** (keyboard mode only, like a knob): arrows step 1 % (Shift 10 %), Delete removes, Enter sets 100 %.
- A click on a wire selects it and says it.
- Each finished gesture is **one** `onChange(label)`: one undo step, a drag to 0 included.

**The consumer provides** `nodes` and `wires` (or `model`, an `FxWires`), `weights()` (the pad's four), `prevent()` (Settings' Prevent feedback), `keys()`, `start` (the corner shown first), `onChange(label)` and `say(text)`. Call `step()` from the frame loop: under Heard it redraws when the pad has moved. `Horde.FxRouting(el, o)` returns `{ el, model, show(i), redraw(), step(), select(k), lastLayout }`.

**Layout (r2, L1)** the ladder and the matrix side by side, from 760 px of host width; under that, stacked, the ladder on top.

**Box** the matrix's size depends on the node count and the width alone. The ladder holds two tracks each way before it grows, so a first wire doesn't move the page; a wire that needs a third track adds one pitch (15 px).

**Fits** from 320 px. A ladder column is never narrower than 44 px: below that, a box's ports would sit closer than 3 px and the wires would merge again. So a narrow host scrolls the ladder sideways (ten boxes need 456 px), and the matrix reflows.

**Accessibility**
- The ladder is a labelled figure, with a visually hidden list naming every wire with its level, "a return wire, one sample late" and "held at zero".
- Chips and cells are sliders with values. The status line is a polite live region, and every refusal is said in words.

**Measured** by the check (the preview's sample, every corner and Heard, at 333, 600, 693 and 960 px of host): 0 shared ports, 0 collinear overlaps, 0 band violations and 0 chip clashes. Ports sit at least 3 px apart, because a 333 px host gets the ladder's 456 px minimum. The 9 to 21 crossings all come from spans that genuinely interleave.

**MOCK**
- The wire model lives in the view: not in a patch, a snapshot or undo, and gone on reload. The bridge's wire model replaces it (GN-61): levels per corner and heard, and the messages make, level and remove.
- Prevent feedback's home is Settings (GN-62); its default waits on the human's listening.
- Inputs per source are open (B528) and marked `MOCK`.

**Readings, open to the human**
- The level drag and the keys (GUI-75's readings).
- Heard's rule (above).
- The 760 px threshold for stacking.
- The 44 px minimum column.
- The cord's width, 1 to 3.5 px: narrower than FX cables' 0.8 to 5.6 px, because every wire has its own port and the ports sit 3 px or more apart.

**Not drawn yet** the pick switch and the dead-end guard (ADR-411), which wait on B714's message shapes.

**FX device (ADR-254)** the same views and gestures, with the device's own inputs in place of the sources.

**Sources** `docs/design/fx-routing-lab.html`: `setLevel`, `heardOf`, `backEdges`, `layout`, `placeChips`, `measure`, `drawWires`, `connectFrom`, `drawMatrix`, `editable`. The lab's own cord was cased in `panel`; here it is cased in `ink`, as FX cables cases every cord, so a pale cable (Drive, ECHO) still reads on cream. The lab drew inputs in `src-engine`, which FX cables reserves for modulation, so inputs here are lanes (`t2`, `hint`).

The preview draws SAMPLE data generated in the preview (deterministic), never the engine.
