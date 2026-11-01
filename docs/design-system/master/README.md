horde is a synthesizer of responsive, dynamical engines whose centre is its morph logic over the full depth of every parameter: a swarm of coupled oscillators cut by blades, a sub, noise, and a rack of FX modules that each run as their own software on a screen. This is its house design system for GUI 3: cream chassis, flat ink outlines, one job per colour, and screens as a separate axis.

## Three rules for every screen

1. **Controls bind to generated manifest keys only** (B308 L2). A control names its key (`env1.attack`, `voice.mode`, `out.level`), and its law, range, default, unit and display name come from the manifest. Never a hand-written id, a position or a display string.
2. **No screen assumes the oscillator panels exist** (ADR-254). The FX device ships after 1.0 with the same rack, glasses, mod matrix, macros, morph pad, mixer and census and no source blocks. Every card, screen and pad must render alone; note-driven pictures (the Specimen) need an audio-driven stand-in or a still pose.
3. **Accessibility at Surge XT's level** (B432). Every control is reachable by keyboard (in keyboard mode), named for a screen reader by module path and unit, at least 28 px to hit, and legible at the contrast floors below. See the Accessibility section.

## Voice and content

- The brand name is always lowercase: **horde**. The founding engine is SWARM SAW; FX modules have one-word names (Shriek, Sluice, Scape, Bulwark, Echo).
- Cluster headings are UPPERCASE Archivo 800 with 0.1em tracking: "FILTER", "MACROS". Control labels are sentence case and short: "Cutoff", "Onset lock", "Coherence → Tone".
- Values carry their unit, in the shell's one format: "1.20 kHz", "−6.0 dB", "4.0 ms", "+18 st", "85 %". The minus is a true minus; −∞ dB is the bottom of a level.
- Status words are short capitals on a tag: MOCK, PROVISIONAL, ROUTING, PANIC, CLIP, KEYS ON.
- No emoji. The symbols that do appear are part of the instrument: the corner glyphs ◆ ▲ ● ■, the root ★, the lock spark ✶, the bipolar ±.

## Colour

- **Ground**: `bg` is the window, `panel` the cards, `well` the data surfaces, `well-alt` tracks, empty meters and the bezel. `ink` draws every outline and the primary text. `hair` and `div` are decorative structure only.
- **Text**: `text` (ink) for values and headings, `t2` for labels, `hint` for captions. Each token's note names the grounds it is legible on; respect them. `dis` only for disabled controls.
- **Signal, one job each, never borrowed**: `value` is the user's own value (arcs, fills, the puck); `physics` the system acting (modulation, the order parameter); `meter` a measure; `marker` attention; `celebrate` OK; `caution` look-before-accepting, always with `caution-bg` and `caution-tx`; `alarm` PANIC, clip and mute only, at most twice a page; `ghost` the dry trace behind the wet one.
- **On cream, lines and text in `value` or `meter` use `value-ink` and `meter-ink`** (ADR-259 ruling 8). Fills keep the bright hues.
- **Mod sources** have their own family, by kind: `src-env`, `src-lfo`, `src-macro`, `src-engine`, with `on-src` for text on a source fill. One colour per kind (ruled); same-kind sources on one knob are told apart by the inspector and the focus caption. None is a corner hue, so a ring never reads as a corner on the same knob. Knob and slider value arcs are neutral `ink`; `mark-edge` outlines pips, origin dots and thumbs.
- **FX chain modules** (ADR-273, the human's ruling of 2026-10-23, option B of the FX Module Colours comparison): a module is an ordinary `panel` card with ink edges that carries a small screen inset in its own display style, and the screen is its only colour. Eight modules, four tokens each: `fxs-<module>-ground`, `fxs-<module>-ink`, `fxs-<module>-mid` and `fxs-<module>-cable` (`fxs-bulwark-*`, `fxs-filter-*`, `fxs-eq-*`, `fxs-drive-*`, `fxs-shriek-*`, `fxs-echo-*`, `fxs-sluice-*`, `fxs-scape-*`). A screen is its own display, so each token is one value in both themes; the card around it follows the theme. Cables, arrowheads and matrix weight bars take the module's cable colour, `fxs-<module>-cable` (the comparison page's own, ADR-273 A1: the screen's ink for four modules, a separate colour for Filter, Drive, Echo and Sluice), always cased in `ink` so a pale cable on the white well still reads; module chips and names stay ink on cream. Each screen's ink clears 4.5:1 on its ground, and the grounds sit at least 5 L* apart. Shriek (the 80s workstation) and Sluice (the Game Boy Color LCD) are the real display styles; the other six are labelled stand-ins until their skins are chosen. This retired the flat `fx-<module>` fills of ADR-266 ruling 5, their `on-fx-<module>` text colours and `fx-edge`.
- **Selection** (the human, 2026-10-21): a selected row, card or control fills with the editing corner's tint (`sel-cA`…`sel-cD`, edge `sel-cA-edge`…) or, when morph is off or the control is global or locked, violet (`sel-neutral`, `sel-neutral-edge`). Pills stay ink-inverted. Text on a selection is ink or t2.
- **Corners** `cA` `cB` `cC` `cD` are preset identity and never re-theme. Each always travels with its glyph (A ◆, B ▲, C ●, D ■). Text in a corner colour uses `cA-tx`…`cD-tx`; anything on a corner fill uses `on-corner`.
- **Screens are a second axis.** Inside a glass the `scr-` tokens apply: the house screen pairs ORCHID with the cream chassis and TUBE with the dark one; FROST, EMBER, DUSK, LUMEN and PAPER are the other schemes, chosen with the Screen scheme selector. The chassis theme never sets a screen colour directly.
- Full spectrum appears only where hue encodes a dimension: the phase carpet (five stepped colours) and the temperature wheel. Gradients appear only in the wordmark, the spectrum bars, the temperature wheel and the morph pad's four-way wash.

## Themes

Cream (`light`) is the default and the first theme. Dark is ratified after the contrast audit (ADR-259 ruling 7). Dark is the flat-ink idiom inverted, not merely darkened: the ground takes a violet bias, and wells go darker than their card so "recessed" still reads. Signals keep their hues; only emission moves; `ghost` goes darker so the reference stays behind.

## Type

- **Archivo** (500, 600, 800) for the chassis: `cluster`, `option`, `label`, `label-knob`. **Roboto Mono** (400) for everything numeric: `value`, `sticker`, `hint`. Both are OFL and bundled in the plugin as TTF, so it looks the same on every system (ADR-259 ruling 1).
- Floors: no text below 8 px anywhere, no readout below 9.5 px. Values are tabular and right-aligned in a 34–58 px column.
- The wordmark is never set in type: it is the locked logo (Logos).
- Glass fonts (3×5, 5×7, 5×8 cells, 14-segment) are drawn as data by each skin, not loaded.

## Geometry

- Every outline is `stroke-ink` (1.5 px) in `ink`; sub-elements `stroke-sub`; page dividers `stroke-page`.
- One roundness: `radius` (10) for cards, wells, the bezel and the glass; `radius-window` (12) for the plugin window and a screen's chassis; capsules (`radius-capsule`) for pills, tabs, toggles, fields and chips. Small parts take `radius-meter`, `radius-fader`, `radius-track`.
- Shadows are hard offsets on interactive elements only: `shadow-rest` 1.5 px, `shadow-hover` 2.5 px, `shadow-pressed` none. Never on wells, cards or text. No blur anywhere, and glow inside a glass is an underlay stroke, never a filter.
- Tilt (±2–4°) only on decorative stickers, never on controls or readouts.

## Layout

The window is `size-window-w` × `size-window-h`, 980 × 720, and resizable (ADR-259 ruling 2); design at 1× and scale by whole numbers. The frame has `space-chassis-y`/`space-chassis-x` padding and a header row: the wordmark (91 × 60), the two-level page bar (Page nav: the groups Main · Osc · FX · Mod · Morph · Mix · Hist · Set on top, the selected group's pages under them; Osc opens on its Dashboard, then Osc 1, Osc 2, Sub, Noise, Filters), and the keyboard lamp. MAIN and OSC split into a left column (about 272–282 px: morph pad, XY, visualisers, Advanced) and the controls on the right, with `space-column` between and `space-stack` between cells. Every hit target is at least `size-hit` (28); a smaller face extends its hit box, and two targets never sit closer than 28 px. How tightly controls pack, and what gets a box, is the Density and hierarchy section below. **Everything fits its container at any width**: flex and grid children take `min-width: 0`, drawings scale by their viewBox to `width: 100%`, nothing is wider than its card, and a glass steps down a whole-number scale (×2 to ×1) rather than overflow.

## Density and hierarchy (B589)

The human's notes on the GUI 3 shell (2026-10-30): MAIN and OSC should start from the SCALPEL bench's interface ("That interface is great"); everything should be more visual-forward; buttons, sliders and knobs should take less space and compose with a stronger hierarchy. The bench (`docs/design/scalpel-interface-lab.html`) is the reference; the rulings stand where it disagrees with them (listed below).

**Visuals take the area.** A page with pictures (the cycle view, the phase ring, the spectrum, the carpet, the voice map, an envelope) gives them at least half its body, and the controls pack around them at compact size. A module's main picture stays in view while its controls scroll beneath it (the bench's pinned cycle view). Controls never get a bigger face to fill space: spare width goes to the picture.

**The size ladder.** Four rungs, chosen per cluster, not per page:

| Rung | Knob face | Cell | Label | Value at rest | Use |
|---|---|---|---|---|---|
| Primary | `size-knob-primary` 56 | `size-cell-primary` 64 | above, 10 px | under the face, always | macros; a page's one to three headline controls (Volume, Members) |
| Standard | `size-knob-standard` 48 | `size-cell-standard` 56 | above, 9.5 px | on hover, drag or focus | a section's main controls where the panel is not dense |
| Compact | `size-knob-compact` 40 | `size-cell-compact` 48 | above, 9.5 px | on hover, drag or focus | **the default inside a dense panel** (oscillator, blade, filter, envelope) |
| Utility | `size-knob-utility` 28 | its label | inline, left | on hover, drag or focus | trims in a row, a dashboard card, a header; one mod ring |
| Tiny (ADR-338 r1, ADR-340 r1) | `size-knob-tiny` 20 in a 28 hit | `size-hit` 28 | above, the 4-character abbreviation | never in the cell: the section's fixed readout slot | P1's page-in-view grid; one mod ring; the editing corner a see-through pip between ink stops (T2d) |

The same rungs set the other controls: in a dense panel (`.hd-dense`, or `.hd-sm` per element) pills, steppers, readouts, lamps and mute/solo buttons have `size-face-dense` (20 px) faces, toggles are `size-toggle-dense-w` × `-h` (26 × 14), slider rows take a `size-label-dense` (52 px) label and a value-only column, and a slider's drawing is the tight 200 × 35. Outside a dense panel the classic faces stay (28 px pills, 32 × 17 toggles). A knob or slider given no `size` is the classic box (68 px, its 96 px two-line readout cell), unchanged, so the shell and the labs that use it today render as before.

**Labels and readouts.**
- The label sits ABOVE the face, one line, ellipsis (the bench's idiom); the full name is the title and the accessible name.
- The value is not given a cell. On hover, drag or keyboard focus it takes the label's own line, in Roboto Mono 600; the two share one box, so nothing moves. A primary control shows its value under the face at all times.
- **The section's readout slot** carries what the classic cell's second line did: the right end of every section header reads the hot control (under the pointer, dragged or focused) as name, value and the editing corner's glyph, letter, stored value and weight ("▲ B 72% · 41%"), or "controlled by MACRO 1", or LOCKED; at rest it shows the section's summary ("Sync · Saw · 25% @ 315°"). The face still shows the corners at all times (blend arc, pips by weight, the editing pip, the tether), and a selected control still takes its editing corner's selection tint.
- **The mod focus caption.** While Option is held or MAP is on, every mapped control's label line shows a short caption in its source colour ("LFO 2 2/3") and its focused ring is drawn heavy; the full caption ("LFO 2 BI 30% (2/3)") is in the slot for the hot control and in the inspector. A refused write puts the padlock and "C locked" on the label line for 1.6 s.
- Screen readers lose nothing: every control's `aria-valuetext` is as before.

**Spacing in a dense panel.** `space-dense` (4) between controls in a sub-group; `space-subgroup` (7) each side of the dashed hairline between sub-groups; `space-head` (2) under a section header; `space-section` (10) between sections; a cell's padding `space-cell-y` × `space-column` (5 × 7) and `space-stack` (6) between cells. Packed targets keep 28 px pitch: a wrapped row of 20 px faces sits 8 px below the last, so the hit boxes touch and never overlap; knob cells sit at 52 px.

**Card, section, sub-group label, or nothing.**
- **Card** (`.hd-cell`: `panel`, ink edge, `radius`): the one box level inside the window, for a unit that stands alone: a page column's cell, an FX module, a mod card, the mixer's console, an oscillator's dashboard card. Never a card inside a card.
- **Section** (Section header: an ink identity rule and one header line): one per module inside a card (an oscillator, a blade, an envelope, a filter slot). Sections stack in a card with no box; the rule and the header are the boundary. Repeated sections are told apart by the rule's pattern and a number, never by a colour.
- **Sub-group label** (`.hd-sublabel` over a `.hd-krow`, a dashed `div` hairline between groups): a cluster of two to six controls that share a purpose inside a section (CUT, MIRROR, FM, SPREAD).
- **Nothing**: a lone row of up to four controls, or controls whose labels already say what they are.
- Pictures (`hd-viz`) keep their ink frame wherever they are: a well is a data surface, not a card.

**Hierarchy, top to bottom**, each step lighter than the one above it: page groups (ink-inverted pills) → pages (underlined tabs) → cell (ink edge) → section (3 px ink rule, `cluster` name) → sub-group (`sublabel`, 8.5 px caps in `hint`, a dashed hairline) → control label (9.5 px `t2`) → value (on demand).

**Clusters and the page paradigm** (ADR-338 rulings 4 and 5, batch 2b; Cluster, Lens, Fold, Page view).
- **C1** is the general rule for a section of tiny controls: one row of 28 px cells in groups split by a dashed hairline, one shared label line (each cell's 4-character abbreviation), the switches in the header beside the name, the one readout slot. A long section wraps by group, never mid-group.
- **C2** is for any block with a repeated sub-structure (envelope stages x time and curve, the two blades): column labels once, row labels once, an empty cell drawn as a dot.
- **P1, focus + lens** is the page paradigm: every control of the page in view as tiny knobs and companions in clusters (advanced rows inline after an ADV mark), and the control under the pointer, or the last one clicked, enlarged in the lens with its full bundle knob, every corner's value and weight, and every mapping.
- **P2's fold** (Face | All | +Adv per section, remembered) only where a page still overflows with P1.
- **PV1**: every page also has a Grid | Table switch with a find box, remembered per page.

**Where the bench and the rulings disagree** (the ruling wins):
- The bench's 19–22 px pills, tabs and selects were 19–22 px targets. Here the face is 20 px and the hit box 28 (ADR-259 r8); rows wrap at 28 px pitch.
- The bench marks a blade with a pink (`value`) stripe on its left edge. `value` is the user's own value (one job per colour), and a left rule already means corner ownership on a label, so the identity is an ink rule above the header, by pattern and number.
- The bench's hint grey (#9a93a3) and its 45 % focus ring fail contrast; the audited `hint` and the solid `focus-ring` stand (Contrast audit rows 1–3).
- The bench's knob draws its value arc in `value` and its modulation in `physics`; the morph knob's neutral ink arc and source-coloured rings stand (the morph knob spec, audit row 13).
- The bench clamps knob labels at two lines; here one line, so a cell is one line plus the face.

## What changed in batch 2a (ADR-338, ADR-340, ADR-341: controls and routes)

Rulings 2 and 7 of ADR-338, 8 to 10 of ADR-340 and 3 to 5 of ADR-341 enter the master. Page forms and the bank are batch 2b.

- **The level cell, T4c-f** (ADR-340 r3, ADR-341 r1): `Knob` `size: 'level'`, a 16 px vertical level in a 28 px cell. **A new size, not TS2a** (TS2a, Slider `'cell'`, is the horizontal table value cell and is unchanged). Its corner mark is the tiny knob's pip: a hollow see-through circle, hole 2.1, rim 1.65, stroke 0.9, in the corner's `-tx` ink (the fills fail on cream: A, C and D reach only 1.3 to 1.8 : 1, the `-tx` inks about 4.9 : 1). One helper draws the mark for both.
- **The dual readout, DR-c** (ADR-341 r2): the section's `ReadoutSlot` is two stacked 13 px lines in a fixed 26 px box. **PLACEHOLDER:** line 1 is the editing corner's own value, not the blend; the human has not ruled which.
- **Stepper, SG-b** (ADR-341 r3): a ⇕ glyph and a step scale with a ringed home tick. **Option-click on a named stepper opens a dropdown** (arrows, Enter commits, Escape cancels); the typed-name entry is gone. Numeric steppers keep the number field. `tc: true` is TC1's 16 px face. Alt + wheel steps (B374, new on the stepper). The trackers' order N, the S&H clock's binding, the envelope's sustain point and the orbital's kick body are `H.Stepper` now; RateSync's division stepper is not (its off-list rule).
- **Companions, TC1** (ADR-338 r2): `H.Companion`, the toggle, segments, select and button at 16 px faces with 28 px hits; TC2 (`flat`) only inside tables.
- **Polarity** (ADR-340 r9, ADR-341 r4): MP1's chip (`H.PolChip`) on every route row and inspector row (click next, shift-click previous, a vertical drag for the band); MP3, an Option-drag of an existing UNI or INV mapping's depth through zero flips the polarity (a mapping the drag itself creates stays at 0, so a drag released at 0 still makes no route); **⇧⌥-click** (Shift+Alt on Windows; ADR-341 A1, which replaced ⌘⌥) a control cycles the focused mapping's polarity as the chip does. **Not done:** the right-click menu entry (the master has no control context menu; the lab's route menu is not ported). **Not verified:** whether a host keeps ⇧⌥-click.
- **Source switching** (ADR-340 r8): MS3, `H.SourceChip` (also `MapControl` `chip: true`); MS4, `H.SourceRows`, the bank's row form with the click-the-body rule. MS1 is `SourceStrip`, as before.
- **Wires, W-a** (ADR-341 r5, ADR-340 r10): `CompactWires` shows values, a ◇ valve at every wire's middle and takes dragged wires; a source dropped on a valve **emits `onDepthRoute`** (the engine has no depth routes yet, B70). **ML3**, routes by source, is `H.RoutesBySource`.
- **Housekeeping:** the matrix family's `drag()` (grid cells and route rows) re-anchors at the stops and on a Shift change, as the Session gesture does.
- **Behaviour changed in existing components:** a named `Stepper` no longer takes typed text; a `ReadoutSlot` reads two lines (its `.rs-2` is now line 1's corner span); the route row's polarity pill and the inspector's are chips (a vertical drag edits the depth); the Session's `cyclePol(m, back)` takes an optional direction; an Option-drag of a UNI or INV mapping can cross zero; the gesture knows the polarity chord; `CompactWires` draws a valve on every wire and its nodes show values.
- **Not in 2a:** the bank, the macro rows and the bend's spring-back, and the page forms, which are 2b (below); the polarity menu, which is not done.

## What changed in master batch 2b (ADR-338, ADR-340, ADR-341: page forms and the bank; B589, B595)

- **No existing token value or exported name changed; no existing component's behaviour changed.** New tokens (sizes only): `size-row-thin` 26, `size-row-thin-w` 226, `size-row-macro-w` 188, `size-card-compact-w` 188, `size-bank-h` 68, `size-bank-side` 52, `size-micro-chip` 20. The thin rows are a named exception to `size-hit` on the cross axis (their buttons are 18 and 16 px faces with 28 px hit boxes that reach past the row), like the tiny sliders.
- **Clusters (ADR-338 r4)**: `Cluster` (C1) and `ClusterMatrix` (C2). **Page paradigm (r5)**: `Lens` (P1) and `Fold` (P2). **Table view (ADR-340 r6)**: `PageView` (Grid | Table, the find box, remembered per page in try/catch storage) and `ParamTable`.
- **The four modulator tiers (ADR-338 r6)**: `ModTiers.thin` (26 px), `.compact` (the card of ADR-323 item 1, now the thin row's expanded state, r7), `.chip` (20 px) and `.full`, with `ModPicture` (screen roles, round joins, a source's trace as a pure function of time).
- **The bank (r7) with BT1 (ADR-340 r11)**: `ModBank`, 68 px, two lanes of 26 px rows, MAC | MODS segments, a click on a row opens its compact card. **Macro rows (ADR-341 r6, r7)**: `MacroRow`, BR-b: the rows follow the wheels with a half-second trace and a centre notch on the bend row; a mouse drag on the bend row springs back; `setHardware(v | null)` feeds the hardware wheel and the hardware wins; the mod wheel's row holds. The macros' engine side is not here: the row calls `onChange(id, v, by)` and takes the engine's echo through `set()` and `setHardware()`.
- **The Mod page (r8)**: `ModPage`, list + detail, with ADR-266's grid as its Routes view. **Removal (r9)**: `UndoLine` and `ModRemoval.describe`; the host owns the undo stack and removes a modulator and its routes as one step.
- **The info box (ADR-335 r4, ADR-345)**: `InfoBox`, one size in one place. A selection locks it so the pointer can reach it and scroll, and a description past its edge starts scrolling after 2 s. The host owns its size and its text.
- **Also**: `ctlFace` (one parameter, many faces: a page's grid, its table and the lens each get a shadow of the canonical props, so the corner values are shared and the registry and the revert baseline are never reset by a second face).
- **Not in 2b:** the stepper's affordances, T4c's corner mark, the dual readout, the companions TC1/TC2, polarity, source switching and the wires, which are 2a (above).
- **Readings taken where a ruling is open** are named in each component's README (the MAC | MODS order, the macro row's drawing, the compact card as a header-plus-picture, which controls a macro slider maps).

## What changed in the control layer revision (ADR-338, ADR-340)

- **The tiny tier** (ADR-338 r1/r3, drawn as ADR-340 r1/r2): `Knob` `size: 'tiny'` (T2d) and `Slider` `size: 'tiny'` (TS1a, 20 px rows) and `'cell'` (TS2a, 16 px table cells). The editing corner's value is a see-through mark that travels the whole range under the hand; the blend stays the arc or fill. New tokens `size-knob-tiny` 20, `size-slider-tiny` 20, `size-slider-cell` 16 (the last two a named exception to size-hit on the cross axis), `size-rslot` 250.
- **The house drag re-anchors** at the stops and on a Shift change (ADR-340 r2): no dead range after an overshoot, no jump when Shift is pressed mid-drag. The Session gesture and `PlainSlider`; the matrix family's own drag helper is not changed in this revision.
- **The map gesture is MD1** (ADR-340 r7, amending the Map Mode spec and ADR-261 r3): with MAP on a plain drag moves the value, a click maps, Option-drag sets depth.
- **Escape is the layered cancel** (ADR-338 r11): `S.escapeLayers`, `S.escape()`. The session option `mapKeysAlways` lets the map keys and Escape act outside keyboard mode (ADR-335 r1, GUI 3's one constant); off by default, so a page that does not ask keeps keyboard mode's rule. Panic is on no key and no panel control (ADR-340 r12, ADR-226 item 6).
- **Stepper** is new (ADR-340 r5): drag, double-click for the default, Option-click to type; `flat` for tables.
- **The readout slot is a fixed 250 px** (H1, ADR-340 r4).
- **Round joins on every canvas stroke** (the kit sets `lineJoin = 'round'` each frame, and nothing sets it back to miter): a miter join flips to a bevel past its limit, which the human saw as "jumpy" traces.
- **Pointer capture is guarded** everywhere: a pointer the browser no longer tracks no longer costs the gesture.
- **Not in this revision** (batch 2b, above, took the clusters, the lens and fold, PV1, the tiers, the bank, the Mod page and M-R2; the rest is batch 2a) (next, B589): the clusters C1/C2, P1's lens and P2's fold, PV1's table view, the four modulator tiers, the M2 bank with BT1 + BR1, the M5 Mod page, M-R2, the source switching MS1/MS3/MS4; and what round 3 of the composition lab draws first (T4c's corner, the dual readout, the stepper's affordances, polarity, the wires, the macro rows).

## What changed in the revision before (B589)

- **No existing token value changed.** New tokens: `size-knob-primary` 56, `size-knob-standard` 48, `size-knob-compact` 40, `size-knob-utility` 28, `size-cell-primary` 64, `size-cell-standard` 56, `size-cell-compact` 48, `size-label-dense` 52, `size-face-dense` 20, `size-toggle-dense-w` 26, `size-toggle-dense-h` 14, `size-strip-w` 72, `size-fader-h` 140, `size-meter-w-dense` 7; `space-dense` 4, `space-subgroup` 7, `space-head` 2, `space-section` 10; `stroke-id` 3; the type style `sublabel` (Archivo 700, 8.5 px, 0.12em). The `label-knob` style's usage note now says where a sized knob puts its label. No colour was added.
- **Layout**: the header's two rows of page tabs become the two-level page bar (groups over the selected group's pages).
- **Knob and Slider** take an optional `size` (primary, standard, compact, utility). Without it nothing changes: the default stays the classic box, so GUI 3 and the labs render as they do today. Recommended: make `compact` the default once GUI 3's slice 4 has moved to it (open question for the human).
- **Switch, Value readout, the CLIP lamp**: dense faces under `.hd-dense` or `.hd-sm`, hit boxes unchanged. **Stepped control**: `size: 'compact'`.
- **Namespaced modifier**: the per-element dense modifier is `.hd-sm`, never a bare `.sm`: the first publish of this revision styled `.hd-pill.sm` and `.hd-select.sm`-style compounds that two labs (random-lab, filter-lab) already use as their own modifier, moving their pill rows from 26 to 28 px pitch. With `.hd-sm` the classic faces are untouched everywhere, as promised.
- **Fixed in passing**: `Session` threw on an empty `sources` list (a page with no modulators); it now starts with no MAP source.
- **New**: Page nav, Section header (with the Readout slot, `H.ReadoutSlot`), Channel strip; the classes `.hd-cell`, `.hd-sec`, `.hd-sgroups`, `.hd-sgrp`, `.hd-sublabel`, `.hd-krow`, `.hd-dense`; the Osc dashboard as a composition (Page nav's guidelines).

## Interaction (B449, B374)

- A drag edits the **editing corner's** stored value (the parameter's owner, or the pinned corner), never the blend; the corner latches for the whole gesture. `size-drag` (160 px) is the full range; shift for ×0.25. Sliders drag relative, never jump-to-click.
- Option-drag (Alt on Windows) edits mod depth; Option-click focuses the next mapping on that control.
- Double-click a knob, slider or stepper: reset to default. Double-click a knob's or slider's value: type a value; Option-click a numeric stepper: type a value (ADR-340 r5), a named stepper: choose from a dropdown (ADR-341 r3). ⇧⌥-click (Shift+Alt on Windows) a mapped control: cycle the focused mapping's polarity (ADR-341 r4, A1); ⌥-drag a UNI or INV depth through zero: it flips polarity (ADR-340 r9). Right-click: a menu with "Reset to default" (not built in the master).
- Every relative drag re-anchors at its stops and on a Shift change (ADR-340 r2).
- Wheel only with alt: a plain wheel never turns a control.
- MAP mode (the MAP control, MD1, ADR-340 r7): a drag still sets the value; click a control to map the current source; Option-drag sets its depth. The map keys (M, 1–9, P, Backspace) and Escape act in keyboard mode, or always where the host sets `mapKeysAlways` (GUI 3, ADR-335 r1); the other hotkeys only in keyboard mode; each has a mouse route.
- Escape is a layered cancel (ADR-338 r11): the innermost open thing first (a menu, a field, a pick), then MAP, then a pinned corner, then keyboard mode. Panic is on no key (ADR-340 r12).
- One gesture, one undo node. The engine's echo never moves a control under the hand.
- The editor never holds the keyboard unless keyboard mode is on or a typed-entry field is open, so computer-keyboard MIDI always reaches the host.

## Morph presentation

- A corner is its colour plus its glyph, everywhere (ADR-259 ruling 3), and the ACTUAL name of the preset it holds; never a placeholder or an invented name. An empty corner reads "Empty". Each corner can be locked against edits (it still morphs) and reverted to its saved preset; an edited corner carries a `marker` dot.
- A knob or slider shows every corner at once: its blend, each corner's value as a pip sized by weight, and the editing corner (the owner, or the pinned corner) as the emphasised pip or the thumb. Pips are shape-coded by glyph (◆ ▲ ● ■; ADR-261 ruling 1), so they read without colour; the readout and inspector carry the letter. An enum or toggle shows its owner as a 3 px rule in the corner colour on its label, with the glyph. Frozen, exempt or morph-off parameters show no tint at all; absence means "belongs to nobody".
- A flip flashes the incoming corner colour on the control's border for `duration-flip`, tilts its sticker −2°, and stamps ✶ for `duration-sticker`. Flips play one at a time, at least `duration-flip-gap` apart.

## Motion

Timings are tokens (`duration-…`). The wordmark reseeds only per launch. With reduced motion the wordmark shows its static asset and the Specimen holds still. Meters animate by transform only.

## Iconography

There is no icon font. Glyphs are drawn shapes (SVG paths) so they match on macOS and Windows: the corner glyphs, ★ root, ✶ spark, chevrons and stepper arrows, all single-ink and coloured by the token named where used. Each FX module's TITLE (96 × 24) and ICON (16 × 16) are pixel PNGs drawn by the human at those sizes and shown at whole-number scales (the GBC rule: at most four colours per 8 × 8 tile, RGB555).

## Logo

Use the files in Logos, never a redraw: the locked DROP + INSET + HUE ↘ mark (B375). In the plugin the mark is live (td::Warp; its colour follows the morph cursor or a slow drift); the PNGs are frames of it made by its own code. Show it at 91 × 60 in the header only, never on a well or a screen.

## Components

Controls: Knob, Slider (both at four density sizes, the tiny tier or the classic box), MAP control, Value readout, Switch (toggle, pills, stepper; dense faces), Stepper (drag, double-click home, Option-click to type or choose; the ⇕ glyph and step scale), Companions (the TC1 16 px toggle, segments, select, button; TC2 flat in tables), XY pad. Navigation and structure for density: Page nav (groups over pages), Section header (identity rule, one header line, readout slot, sub-groups). Morph: Corner chip. Pitch: Scale selector, Bend law, Bend view. Structure: Module card, Screen frame, Screen scheme, Keyboard-mode lamp. Display and visualizers: Meter, Channel strip (the Mix page's console: strips side by side, each fader beside its meter with its peak, CLIP lamp, mute and solo), Gain staging (every stage's level, gain, peak hold and latching clip lamp, with the first-over marker), Chain line (every tap on one line: peak hold against the chain in signal order, the 0 dBFS rail, fed and owed taps, the first over), Specimen, Kuramoto circle (the SCALPEL phase ring, which is also the Coupling × Detune XY), Waveform, Spectrum, Phase carpet, Blade view, Envelope, LFO rotor, Voice map, Goniometer. Modulation (the mod matrix, ADR-266): Source strip, Source chip, Source rows, Mod matrix grid, Route row (with the polarity chip), Routes by source, Mod ledger, Trace lanes, Compact wires (draggable, with valves), Hosted macro rows; every polarity shows four states, UNI, BI, INV and CTL (Control: the source drives the parameter between a low and a high end, and its own value is ignored). Page forms and the bank (batch 2b): Cluster (C1, and the C2 matrix), Lens (P1), Fold (P2), Page view (PV1, with the Param table), Modulator tiers (full, compact, thin row, micro chip), Modulator bank (M2, BT1), Macro row (BR-b), Mod page (M5), Undo line (M-R2). Modulators: Mod card header (with the retrigger switch every card carries), LFO shape editor (S&H is a shape), Envelope editor, Tempo divisions (with the one rate-plus-sync control), Curve plot, ORBITAL card, MIDI trackers, Gate lanes, Event lanes, Smoothing lanes, S&H clock card, Ownership tier. Morph editor: Quantum pad, Pad bars (scale bar, QUANTUM banner, lock bar), Cross-section, Blend maps (one row across the pad under each law: heatmaps, contours, pins, corners, TIN's triangulation, the clamp to the set values, a cross-section), Morph grid (edits live), Find pane, Morph inspector, Kind legend, Stepped control (rate plus sync), Cohesion (one control). FX chain: Rack, Matrix and Panel views, and the cable encoding they share; each module a cream card with a small screen in its own display style, and cables in the module's cable colour. Each has guidelines and a live preview; behaviour the rulings leave open is a documented placeholder, named under Placeholders in its guidelines. Every visualizer is marked VOICE-BOUND (it needs notes: the FX device needs a stand-in or omits it) or AUDIO-CAPABLE (waveform, spectrum, goniometer: the FX device can use it as it is). `components/bundle.css` styles them (classes `hd-…`). `components/bundle.js` is a small vanilla reference (`window.Horde`, no React) for the shared morph and map model: `Session`, `Knob`, `Slider`, `MapControl`, `ModInspector`, `XYPad`, `RingXY`, `CornerPin`, `CornerChips`, `CornerInspector`, `ScreenScheme`, `ScalePicker`, `PlainSlider`, `Stepper`, `Companion`, `PolChip`, `SourceChip`, `SourceRows`, `RoutesBySource`, `ReadoutSlot`, `Cluster`, `ClusterMatrix`, `Lens`, `Fold`, `PageView`, `ParamTable`, `ModTiers`, `ModBank`, `MacroRow`, `ModPage`, `UndoLine`, `PageNav`, `SectionHeader`, `ChannelStrip`, the four lab families (`SourceStrip`, `ModMatrixGrid`, `RateSync`, `LfoShapeEditor`, `QuantumPad`, `MorphGrid`, `FxRack`, `FxCables`, `GainStaging`, `ChainLine`, `BlendMaps` and the rest; see `index.d.ts`), the painters `Viz.*`, the glide law `Glide.simulate` and seeded `Synth` generators; its ownership and source values are labelled stand-ins for what the engine reports. GUI 3 implements the components in its own view layer, keeping these tokens and behaviours.
