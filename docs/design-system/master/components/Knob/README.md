# Knob

The morph knob: one control that shows the blended value you hear and each morph corner's own stored value, plus every modulation mapped onto it (HORDE Morph Knob Spec and HORDE Map Mode Spec, the human, October 2026).

**The consumer provides**
- `key`: a generated manifest key (B308 L2), never a display name or a position; its law, range, default and unit come from the manifest, and the shell formats values.
- `path`: the accessible name, module path plus parameter ("Filter · Cutoff", B432).
- the four corner values `vals[A..D]`, or `locked` with one global value (intent and morph targets are always locked);
- the morph weights and the parameter's **owning corner, read from the engine** (the UI never computes ownership; the reference bundle uses a labelled stand-in);
- its mappings `{ source, polarity, depth or per-corner depths }`, or for a Control `{ source, low, high }`, and the live source values;
- `bipolar` where the value fills from the centre (coupling, pan); `size` (below) for any new layout; `big` only on the classic box.

**Sizes: the Density ladder** (B589; README, Density and hierarchy). `size` picks a rung; leaving it out keeps the classic box below, unchanged, so the shell and the labs that build knobs today render exactly as before.

| `size` | face box | track across | cell | label | value at rest |
|---|---|---|---|---|---|
| `primary` | `size-knob-primary` 56 | about 37 px | `size-cell-primary` 64 | above, 10 px | always, under the face |
| `standard` | `size-knob-standard` 48 | about 32 px | `size-cell-standard` 56 | above, 9.5 px | on hover, drag or focus |
| `compact` | `size-knob-compact` 40 | about 27 px | `size-cell-compact` 48 | above, 9.5 px | on hover, drag or focus |
| `utility` | `size-knob-utility` 28 | about 22 px | its label's width | inline, left | on hover, drag or focus |
| (none: classic) | `size-knob-box` 68 (84 `big`) | about 33 px | 96 | under | two reserved lines |

- **Label above, one line**, ellipsis on overflow, the full name in the title and the accessible name. On hover, drag or keyboard focus the VALUE takes the label's own line (Roboto Mono 600, 9.5 px); nothing moves, because both sit in the same 12 px box. This is the SCALPEL bench's idiom.
- **No readout cell.** The classic box's line 2 (the editing corner's glyph, letter, value and weight) goes to the section's readout slot (`H.ReadoutSlot`, carried by the Section header), which reads the hot control. Screen readers lose nothing: `aria-valuetext` is unchanged.
- **The focus caption** under Option or MAP: the label line shows the short caption in the source colour, the source's name and its place ("LFO 2 2/3"), and the focused ring is drawn heavy on the face; the full caption ("LFO 2 BI 30% (2/3)") is in the slot for the hot control and in the inspector. A refused write shows the padlock and "C locked" on the label line for 1.6 s.
- **Tight geometry** (primary, standard, compact): a 72-unit box that hugs the track (radius 21, 5 units wide), the cap at 12.5 with the house hard shadow (1.5 px whatever the size), the tether at 16.6, up to four mod rings from radius 27.5, **2.4 apart**, 1.8 wide (1.3 px apart and about 1 px wide on screen at compact), with the stacked origins, origin dots and live ticks of the classic rings. Pips keep their shapes (ADR-261 r1) at 1.25× the classic radius so a triangle still reads as one at 40 px.
- **What the small sizes drop, and why**: the editing flag (the editing pip is already the large ink-edged one; the flag sat where the rings now are); the corner-depth hairline outside a corner-level ring (no room 2.4 units apart; the slot, the route row and the inspector carry the corner's depth); the in-drawing "+n" (7 drawing units would render under the 8 px text floor at these sizes, so it is an HTML badge at 8 px at the face's corner). At compact two same-kind rings read as one band; they are told apart as they always were, by the caption and the inspector.
- **Utility** keeps one ring: the focused mapping's (stacked on the output of the rings inside it), dashed when there are more; the badge counts the rest. Use it for trims in a row or on a dashboard card, never for a parameter that is usually modulated.
- Every size keeps the 28 px hit floor: the 28 px utility face IS the target, and the larger faces exceed it.

**The tiny tier: `size: 'tiny'`, T2d** (ADR-338 r1, drawn as ADR-340 r1; source `docs/design/composition-lab-r2.html`). For P1's page-in-view grid: a `size-knob-tiny` 20 px face in a `size-hit` 28 px cell, under one 11 px line holding `abbr` (layout.json's abbreviation, ADR-338 r10), which **never changes** (ADR-340 r4, H1: the value goes to the section's fixed readout slot and the info box). `label: false` draws the cell alone.
- **What travels end to end.** A drag writes one corner, so the blend moves only by that corner's weight times the drag. Round 1's tiny knob drew only the blend, so nothing on its face crossed the sweep under the hand, and it "dragged arbitrarily" (the human). T2d draws the editing corner's value as a **see-through pip**: a hole of radius 2.1 cut through the track and the arc (an SVG mask, so the hole shows whatever is behind the knob, never a painted copy of a background colour), rimmed at 0.9 in the corner's line ink, travelling the whole 280° under the hand between two **ink end stops**.
- The arc (ink, 2.2 on a `well-alt` track, radius 6.2) is still the blend you hear; the needle (4.3, ink 1.4) the output; the cap (2.3) rimmed in the editing corner's ink.
- One ring at radius 9, as on `utility`: the focused mapping's, stacked on the rings inside it, dashed when there are more, with its live tick. No other corner pips (at this size a pip is colour only, so the shape rule could not hold).
- Gestures, MAP and Control behave as at every size.
- **The corner mark is one drawing for the tiny knob and the level cell** (ADR-341 r1): a hollow, see-through circle, hole radius 2.1, rim radius 1.65, stroke 0.9, in the corner's legible `-tx` ink (`cA-tx`…`cD-tx`). The code is one helper (`cornerMark`), so the two cannot drift apart.

**The level cell: `size: 'level'`, T4c-f** (ADR-340 r3 taking T4c as the level cell, amended by ADR-341 r1; sources `docs/design/composition-lab-r2.html` T4c and `composition-lab-r3.html` T4c-f). A **new size, not TS2a**: TS2a (Slider `size: 'cell'`) is the 16 px-tall horizontal value cell of a table row with its number in it; T4c is a 16 × 16 px vertical LEVEL in a `size-hit` 28 px cell under the same one-line abbreviation as the tiny knob (`abbr`, `label: false` for the cell alone), dragged up and down like a knob (160 px for the full range, Shift fine, re-anchored at the stops). Its look:
- the level is the **blend**: an ink fill rising in a `well-alt` frame (ink edge 1), **ink end caps** top and bottom (a bipolar parameter's fill grows from the middle);
- the focused mapping's **lane** runs down the left edge in its source colour, with a tick across the level at the output;
- the editing corner is the **corner mark** on the level's right edge, at the corner's own value, cutting the frame and the level (an SVG mask). It travels the whole height under the hand. The human found T4c's first drawing, a 2.6 × 4.4 px triangle filled with the corner's colour, "barely visible": on the light panel the corner fills for A, C and D reach only 1.3 to 1.8 : 1, while every `-tx` ink reaches about 4.9 : 1 (the lab measured it), and then asked for a hollow circle the size of the smallest knob's pip.
- A locked parameter shows no mark. Under a Control the fill ghosts to 22%, as on the other faces.

**The polarity chord and MP3** (ADR-341 r4, ADR-340 r9), on every knob and slider through the shared gesture:
- **⇧⌥-click** (Shift+Alt-click on Windows; ADR-341 r4 as amended by A1) a mapped control CYCLES the focused mapping's polarity exactly as a click on its chip does (UNI → BI → INV → CTL; CTL is skipped where another mapping already controls the parameter, ADR-266 r2). Under MAP it acts on the current source's mapping. It never makes a mapping. With movement it is a fine Option-drag (Shift is fine everywhere). Command and Ctrl must be up. The chord was ⌘⌥ until A1: Windows keyboards have no Command key, and Ctrl+Alt is AltGr on many European layouts. **Not verified:** whether a host (Live, Bitwig) keeps ⇧⌥-click in a plugin window, or whether Windows' Shift+Alt layout hotkey fires around the click; if either takes it, the chip and the menu stay the routes.
- **⌥-drag through zero** (MP3): an Option-drag of a UNI or INV mapping's depth runs through zero and comes out the other polarity (stored as magnitude + polarity, so a depth stays a magnitude, ADR-266 r2). BI and CTL have no sign to turn over, and a per-corner depth keeps the 0 to 1 drag.
- The right-click menu entry for polarity is **not in the master**: the master has no context menu on controls (the lab's route menu is not ported). Until it is, the chip and the chord are the routes.


**Sweep** 280° from 7 o'clock, the UI Spec's (ruled 2026-10-21). The map mode spec drew 270°; its ring rules (stacked origins, origin dot, live tick, emphasis, the four-ring limit) carry over unchanged on the 280° sweep.

**Anatomy of the classic box** (a 92-unit drawing, `size-knob-box` 68 px, `size-knob-box-big` 84 px; centre outward; the sized knobs keep it in the tight geometry above)
- Cap, r 12.5: `panel`, its rim in the editing corner's line ink (`cA-tx`…) at 1.6, neutral `ink` 1.0 when locked; the house hard shadow beneath, flat while dragging.
- Needle, 0–10.5: `ink` 2.0, pointing at the output after modulation.
- Tether, r 16.5: dashed 2 / 2.4 in the editing corner's line ink, from the blend to that corner's value; gone when they coincide.
- Value arc, r 22, 4.0: **neutral `ink`** from the minimum (or the centre) to the blend, on a `well-alt` track. (The UI Spec's magenta arc gives way here: corner B's pip is pink.)
- Corner pips, r 22: each non-editing corner at its own value, **in its glyph's shape** (A ◆ diamond, B ▲ triangle, C ● circle, D ■ square; ADR-261 ruling 1), sized to the area of a circle of radius 1.3 + 2.3w, opacity 0.28 + 0.72w, its fill colour, a `mark-edge` outline. A corner with no weight vanishes.
- Editing pip, the same shape at radius 3.9, full opacity, `ink` outline; the editing flag, a radial line from 25.2 to 27.6 in the corner's line ink.
- Mod rings from r 31, 3.7 apart: one per mapping in its **source colour** (`src-env`, `src-lfo`, `src-macro`, `src-engine`), at most four, then "+n". Each ring starts at the live output of the rings inside it (stacked origins). Its only marks: an origin dot (at the end for unipolar and inverse, at the centre for bipolar) and a neutral live tick at its output.
- Corner depth hairline, 3.2 outside a corner-level ring: the editing corner's own depth, its line ink, 1.1.
- Ownership change: one expanding halo off the cap rim in the new corner's ink, fading over 500 ms (none while a corner is pinned, none with reduced motion).
At a pure corner the other pips vanish and the tether shrinks to nothing: an ordinary knob.

**Readouts of the classic box** (a constant box: the cell is a fixed 96 px, each line one line high with its height reserved when empty, tabular figures, ellipsis on overflow) under the name: line 1 the output in the parameter's unit (`value` style, ink); line 2 in the editing corner's line ink, its **glyph** and letter, the stored value and the weight ("▲ B 72% · 41%", weight omitted at a pure corner), or LOCKED in `hint`. While Option is held or MAP is on, line 2 shows the focused mapping instead ("LFO 1 BI 30% (1/2)") in its source colour.

**Locked corners** a drag (or reset, alt+wheel, arrows, corner-level depth drag) that would write to a locked corner is refused: nothing moves, line 2 shows the padlock and "C locked", and the footer names the corner. While the editing corner is locked, line 2 carries the padlock.

**Control** (the fourth polarity, ADR-266 ruling 2): the source drives the parameter directly, sweeping it between the mapping's low and high ends.
- The knob's own value is ignored. Its value arc, corner pips, tether and editing flag fade to a ghost at 22%; they stay visible because removing the Control restores them. Line 2 reads "controlled by MACRO 1" in the source's colour. That caption is set in Archivo at 8.5 px so it fits the cell; a longer name ends in an ellipsis, and the title and accessible text carry it whole.
- The Control ring is the innermost mod ring: an arc from low to high, its origin dot at low, its live tick where the source puts the parameter now. UNI, BI and INV rings stack on that position; the needle is the output, clamped once.
- A drag, double-click, Option+wheel or arrow on a controlled knob is refused, and the footer says why ("Coupling is controlled by MACRO 1"). Option-drag with the Control focused, or a MAP-mode drag from its source, slides its whole range.
- The polarity pill cycles UNI → BI → INV → CTL. CTL is skipped while another mapping already controls the parameter (one Control per parameter), and the footer says so.
- Depth is a magnitude; the polarity carries the sign (ADR-168's signed depth is superseded).

**Placeholders** (behaviour the rulings leave open): where a new Control's range starts (here the value ± its depth, at least ±25%); what Option-drag does to a Control (here it slides the range, and the matrix's route row edits low and high separately).

**Inspector corner rows** for the selected knob: the corner chip, its preset name, a weight bar, its stored value, its corner-level depth, the edited mark, lock and revert, and a final MIX row (blend and blended depth). The editing corner's row takes that corner's selection tint (`sel-cA`…), and a selected knob does the same (`sel-neutral` when locked).

**Gestures**
- Drag vertically: edits the **editing corner's** stored value one to one, never the blend (no back-solving). `size-drag` (160 px) is the full range; shift is ×0.25. The editing corner latches at pointer-down until pointer-up. **The drag re-anchors** (ADR-340 r2): past a stop, or when Shift changes mid-drag, it restarts from where it is, so reversing moves at once (before, an overshoot was a dead range to unwind, and pressing Shift jumped the value).
- Option-drag (Alt on Windows): that corner's depth on the focused corner-level mapping, or the focused global mapping's depth.
- Double-click: reset the editing corner (or the global value) to the default. Double-click the readout: typed entry.
- Alt+wheel only (B374): always the value, mapped or not. Arrows only in keyboard mode.
- With MAP on (MD1, ADR-340 r7): a plain drag still moves the value; a click maps the current source (or focuses its mapping); Option-drag sets that source's depth. See MAP control.
- Every gesture is one undo node.

**Choosing which modulator Option-drag controls** (the focused mapping; two ways, both mouse routes):
- **Option-click** a control (no movement): focus moves to its next mapping, wrapping round.
- **Click a row** in the modulator inspector.
Option + scroll wheel never changes the focus: it always adjusts the value (B374, ruled 2026-10-21), mapped control or not.
Holding Option shows the focus caption under every mapped control ("LFO 2 BI 30% (2/3)", in the source colour), so the focus is always visible before you drag.

**Corner shapes.** A pip is too small to carry a glyph inside it, so the pip IS the glyph: its shape names the corner without colour, at any weight (ADR-261 ruling 1). The readout's line 2 and the inspector still carry the letter.

**Same-kind sources.** Ring colours are one per source kind (`src-env`, `src-lfo`, `src-macro`, `src-engine`; ruled 2026-10-21). Two sources of one kind on one knob share a colour; they are told apart by the inspector's rows and by the focus caption ("LFO 2 BI 30% (2/3)"), shown while Option is held or MAP is on.

**Not decided here** (the specs' open table): composition style as an inspector-only view; quantum-mode Auto following the resolved corner, with flips frozen while hovered; an edit-all-corners gesture.

**Found while sizing (not changed here):** the classic box's "+n" is SVG text at 7 drawing units, which renders at about 5.2 px at 68 px, under the 8 px text floor; the sized knobs draw it as HTML at 8 px. The classic one is left as it is for the shell and labs that use it today.
