horde is a synthesizer of responsive, dynamical engines, aimed at the sounds of hyperpop: a swarm of coupled oscillators cut by blades, a sub, noise, and a rack of FX modules that each run as their own software on a screen. This is its house design system for GUI 3: cream chassis, flat ink outlines, one job per colour, and screens as a separate axis.

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

**Where the bench and the rulings disagree** (the ruling wins):
- The bench's 19–22 px pills, tabs and selects were 19–22 px targets. Here the face is 20 px and the hit box 28 (ADR-259 r8); rows wrap at 28 px pitch.
- The bench marks a blade with a pink (`value`) stripe on its left edge. `value` is the user's own value (one job per colour), and a left rule already means corner ownership on a label, so the identity is an ink rule above the header, by pattern and number.
- The bench's hint grey (#9a93a3) and its 45 % focus ring fail contrast; the audited `hint` and the solid `focus-ring` stand (Contrast audit rows 1–3).
- The bench's knob draws its value arc in `value` and its modulation in `physics`; the morph knob's neutral ink arc and source-coloured rings stand (the morph knob spec, audit row 13).
- The bench clamps knob labels at two lines; here one line, so a cell is one line plus the face.

## What changed in this revision (B589, for review)

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
- Double-click a knob or slider: reset to default. Double-click its value: type a value. Right-click: a menu with "Reset to default".
- Wheel only with alt: a plain wheel never turns a control.
- MAP mode (the MAP control): click a control to map the current source, drag to set depth. Hotkeys (M, 1–9, P, Backspace, Esc) are accelerators that work only in keyboard mode; each has a mouse route.
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

Controls: Knob, Slider (both at four density sizes or the classic box), MAP control, Value readout, Switch (toggle, pills, stepper; dense faces), XY pad. Navigation and structure for density: Page nav (groups over pages), Section header (identity rule, one header line, readout slot, sub-groups). Morph: Corner chip. Pitch: Scale selector, Bend law, Bend view. Structure: Module card, Screen frame, Screen scheme, Keyboard-mode lamp. Display and visualizers: Meter, Channel strip (the Mix page's console: strips side by side, each fader beside its meter with its peak, CLIP lamp, mute and solo), Gain staging (every stage's level, gain, peak hold and latching clip lamp, with the first-over marker), Chain line (every tap on one line: peak hold against the chain in signal order, the 0 dBFS rail, fed and owed taps, the first over), Specimen, Kuramoto circle (the SCALPEL phase ring, which is also the Coupling × Detune XY), Waveform, Spectrum, Phase carpet, Blade view, Envelope, LFO rotor, Voice map, Goniometer. Modulation (the mod matrix, ADR-266): Source strip, Mod matrix grid, Route row, Mod ledger, Trace lanes, Compact wires, Hosted macro rows; every polarity shows four states, UNI, BI, INV and CTL (Control: the source drives the parameter between a low and a high end, and its own value is ignored). Modulators: Mod card header (with the retrigger switch every card carries), LFO shape editor (S&H is a shape), Envelope editor, Tempo divisions (with the one rate-plus-sync control), Curve plot, ORBITAL card, MIDI trackers, Gate lanes, Event lanes, Smoothing lanes, S&H clock card, Ownership tier. Morph editor: Quantum pad, Pad bars (scale bar, QUANTUM banner, lock bar), Cross-section, Blend maps (one row across the pad under each law: heatmaps, contours, pins, corners, TIN's triangulation, the clamp to the set values, a cross-section), Morph grid (edits live), Find pane, Morph inspector, Kind legend, Stepped control (rate plus sync), Cohesion (one control). FX chain: Rack, Matrix and Panel views, and the cable encoding they share; each module a cream card with a small screen in its own display style, and cables in the module's cable colour. Each has guidelines and a live preview; behaviour the rulings leave open is a documented placeholder, named under Placeholders in its guidelines. Every visualizer is marked VOICE-BOUND (it needs notes: the FX device needs a stand-in or omits it) or AUDIO-CAPABLE (waveform, spectrum, goniometer: the FX device can use it as it is). `components/bundle.css` styles them (classes `hd-…`). `components/bundle.js` is a small vanilla reference (`window.Horde`, no React) for the shared morph and map model: `Session`, `Knob`, `Slider`, `MapControl`, `ModInspector`, `XYPad`, `RingXY`, `CornerPin`, `CornerChips`, `CornerInspector`, `ScreenScheme`, `ScalePicker`, `PlainSlider`, `ReadoutSlot`, `PageNav`, `SectionHeader`, `ChannelStrip`, the four lab families (`SourceStrip`, `ModMatrixGrid`, `RateSync`, `LfoShapeEditor`, `QuantumPad`, `MorphGrid`, `FxRack`, `FxCables`, `GainStaging`, `ChainLine`, `BlendMaps` and the rest; see `index.d.ts`), the painters `Viz.*`, the glide law `Glide.simulate` and seeded `Synth` generators; its ownership and source values are labelled stand-ins for what the engine reports. GUI 3 implements the components in its own view layer, keeping these tokens and behaviours.
