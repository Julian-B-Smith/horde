# Cluster (C1) and Cluster matrix (C2)

How tiny controls compose into a section (ADR-338 ruling 4; source: docs/design/composition-lab.html, the C section, ENV 2's ten rows).

`Horde.Cluster(el, S, o)` is **C1, the general rule**; `Horde.ClusterMatrix(el, S, o)` is **C2**, for any block with a repeated sub-structure (envelope stages x (time, curve), the two blades). Both are layout: the controls inside are the tiny tier's (`Knob` and `Slider` with `size: 'tiny'`, `Stepper`) or any element.

**The consumer provides**
- `name` (and `path`, `num`, `on`/`onToggle`, `summary`, `keys`): they go to the section header, which carries the one readout slot. `head`: items placed beside the name (switches, a velocity knob). `picture`: an element shown between the header and the controls.
- C1: `groups: [{ label, items }]` and optionally `adv: [items]`.
- C2: `cols: [{ label, title }]`, `rows: [{ label, cells: [item | null] }]`, `emptyTitle`.
- An **item** is an element, a builder `function (host, S)`, `{ knob: props }`, `{ slider: props }` or `{ stepper: props }`. Knobs and sliders default to `size: 'tiny'`; the host carries `data-key` (the Map button's drop target, PV1's find).

**C1 anatomy**: one row of 28 px cells (`size-hit`) in groups split by a 1 px dashed `div` hairline (`stroke-sub`); ONE shared 11 px label line above all the cells (each tiny knob's own 4-character label, a stepper's `abbr`); the section's switches in the header beside the name; the readout slot at the right, 250 px (H1). A group's `label` is its accessible name and its hover, never drawn. **A long section wraps by group, never mid-group**: a group is `flex-wrap: nowrap`, the row wraps between groups. Advanced controls follow after the vertical ADV mark.

**C2 anatomy**: a grid of `auto` and 28 px columns. Column labels once across the top, row labels once down the left; the cells take no label of their own (so a hot knob's value shows only in the header's slot, the matrix's one cost). A cell with no control is a dot, never a disabled control, so the gap says "no hold curve exists".

**Rules**
- One box level: a cluster is a section, never a card inside a card (README, Density and hierarchy).
- Group by meaning, then by row order; a stage's curve sits under its stage when the block repeats a structure.
- The label line never changes (H1): the value is in the slot.

**Readings taken** The lab's C3 (docked to the picture) and C4 (value cells) are not taken (ADR-338 r4). Steppers in a cluster are today's `Stepper` at its dense face (`sm`), which is wider than 28 px; the TC1 companion boxes (ADR-338 r2) are the next batch's, and a cluster takes them as items without change.

**Placeholders** none beyond the SAMPLE values in the preview.

**Sources** ADR-338 ruling 4 (C1 + C2; C3, C4 not taken), ADR-340 ruling 4 (the slot), composition-lab.html C1/C2.
