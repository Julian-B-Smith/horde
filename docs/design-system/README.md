# Design system

*Last verified 2026-10-30 against main `ea5ea53`, through ADR-336 and ROADMAP B596. The sections
from "What is here" down were last re-read against the rulings on 2026-10-24 (through ADR-276, B549)
and are kept as they were; B596's ledger audit says where they have moved on.*

## The chain of custody (ADR-336)

Every design decision travels one chain, and every hop has a record and a check:

| hop | where | its record | its check |
|---|---|---|---|
| ruling | an ADR in `DECISIONS.md` | the ADR | no ADR or B number taken twice |
| **design master** | **`master/` in this folder** (the artifact's `project/` tree) | `master/VERSION`: artifact, version, date, a sha256 per file | the tree is exactly those files with those hashes |
| vendored bundle | `docs/design/arp-face/` (`bundle.js`, `bundle.css`, `tokens.css`) | each file's first-line stamp names the master version | equals `vendor(master)` (`tools/design_vendor.py`), or a drift the ledger records |
| GUI 3 shell | `docs/design/gui3.html`, `docs/design/gui3/` | `docs/design/design-ledger.json` rows; the port ledger (`gui3/port-tab.json`) is the view of what has not arrived | every component, version, export and shell place a row names exists |
| engine | the no-audio stub today; the WebAssembly engine at stage 8 (IF-10) | the ledger's `wired` state | the seam check |

The ledger (`docs/design/design-ledger.json`) has one row per ruled design element and a state
for each: **ruled**, **in-master**, **vendored**, **in-shell**, **wired**. Every lab in the design
index carries a status there too: exploring, ruled (ADR), harvested (master version) or retired.
`tools/design_chain_check.py` holds all of it to the tree in `./verify fast`, with a must-fail plant
for every kind of finding. The GUI session's charter is `docs/design/CLAUDE.md`.

## The design master's home

**`docs/design-system/master/` is the source; the artifact is its published view** (ADR-336
ruling 1). It was imported on 2026-10-30, byte-identical to the live artifact's `project/` tree
at version `1793374583-b6a5` (137 files: the README and guidelines, `tokens.json`, every
component's README and preview, `components/bundle.js`, `bundle.css` and `index.d.ts`). The
artifact type's own files (its app, demo and reference pages) are not ours and are not here. The
ten logo PNGs live in the artifact's asset store, not in `project/`; `master/design-system.json`
lists them by blob id, and the repo's own copy of the warp engine is `docs/design/logo/`.

The published artifact is https://claude.ai/artifact/EGDwetDHwcvxhTbNffCzJc. It is **private
to its owner unless shared**; the link will not open for anyone else. Nothing in the build
depends on it: the repo holds the master.

## The publish procedure (from `docs/design/CLAUDE.md`)

1. **Edit the master here, never in the artifact.** Each master change names the ADR it carries
   out, and its ledger rows move to `in-master` in the same PR. A ruling not yet in the master
   cannot reach the shell (ADR-316): add it here first, never draw it locally.
2. **Merge**, then **publish the artifact from the merged tree** (its `project/` files).
3. **Record the published version**: `python3 tools/design_vendor.py record-master --version <id>
   --date <YYYY-MM-DD>` rewrites `master/VERSION` (version, date, a sha256 per file).
4. **Re-vendor**: `python3 tools/design_vendor.py vendor --date <YYYY-MM-DD> --out docs/design/arp-face`
   stamps the three vendored files with that version. Update the arp-face README's provenance
   row, remove any `vendoring.known_drift` entry the re-vendor resolves, and move the ledger rows
   to `vendored`. Never hand-edit the vendored files or `VERSION`.

**Known finding (2026-10-30):** the vendored files' bodies equal `vendor(master)` at
`1793374583-b6a5` byte for byte, but their stamps name `1793336699-65f2`, the version vendored
by commit `bdc7027`. The ledger records it as a stamp-only drift (ADR-336); re-stamping is a
re-vendor, which is the human's call.

## The living reference

The design system is **published** from the Design System Artifact type
(B531, ADR-259). It holds the tokens, the components with live
previews, and the guidelines. It is the living reference for tokens and
components, and it is revised as rulings arrive (ADR-261 A1, ADR-262, ADR-266,
ADR-272, ADR-273), now through the procedure above.

The `.dc.html` files below are **earlier design documents**. They are kept as
history where a ruling since has superseded them, and they stay normative only
where this README says so. When they and the published system differ, the
rulings in "What the rulings changed" decide.

## What is here

| file | what it is |
|---|---|
| `master/` | **The design master's source** (ADR-336): the Design System artifact's `project/` tree, byte-identical, with `master/VERSION`. The current tokens, components and guidelines; see "The design master's home" above. |
| `HORDE-UI-Spec.dc.html` | **The UI implementation spec, v1** (2026-08-23) — tokens, widget state tables, visualizer recipes, morph presentation. Still normative for **values** (ADR-261 ruling 4, A1), except where a ruling below changes one. An earlier document; the published system carries the current tokens. |
| `HORDE-Design-System.dc.html` | **The consolidated system** (2026-08-23) — page compositions for MAIN, OSC, FX, MIX and the bend+scale panels, with the reasoning. An earlier layout reference; where ADR-259 names a composition (the 980×720 window, the XY pad over the Specimen, module cards, the face/Advanced split), the ruling wins. |
| `HORDE-Vaporwave-Phosphor.dc.html` | **The display direction** (2026-08-24, adopted, ADR-119) — data wells become dark phosphor tubes; the cream chrome stays. Normative for **screen tokens and trace rendering**; where it touches chrome values, the UI Spec still wins. Screen schemes are user-selectable (ADR-262 ruling 2). |
| `HORDE-Morph-Knob-Spec.md` | **Morph knobs** (2026-10-21) — how a knob and a slider show and edit a parameter between morph corners: the blended value arc, weight-sized corner pips, the editing corner's pip and tether, latching, ownership cues, per-corner mod depth, locked parameters, quantum mode. Normative for **interaction** (ADR-261). |
| `HORDE-Map-Mode-Spec.md` | **Map mode** (2026-10-16) — the persistent MAP control and source selector; click to assign a source, drag for depth; stacked mod rings with origin dots and live ticks; the inspector; host constraints. Normative for **interaction** (ADR-261). |
| `HORDE-Morph-Knobs.html`, `HORDE-Map-Mode.html`, `HORDE-Slider-Marks.html` | The three browser prototypes behind those specs (knobs, map mode, and both on sliders). **Behaviour references, not parity targets**, as the specs say; their bilinear weights, random sources and fonts are illustrative. |
| `support.js` | The Claude Design canvas runtime both `.dc.html` files load. Vendored beside them **only** so the specs render from a checkout; it is not ours and is not edited. A separately-delivered `hp-support.js` was byte-identical (same SHA-256), so there is one copy, not two. |
| `HORDE-STATION-Page.dc.html` | The STATION page design, delivered with the STATION intake (ROADMAP.md:90, ADR-122). STATION is out of 1.0 (ADR-182), so this is history. |

The spec is kept **as delivered**, not transcribed into markdown. A normative
document with two copies is a normative document that will disagree with itself,
and this one is 200-odd exact values — hexes, pixel counts, angles, timings —
which is precisely the content transcription gets wrong. Read the `.dc.html`.

## How to read it

The UI Spec declares its own precedence, and it is worth repeating here:

> §1–7 are format-neutral values … normative for both stacks. §8 is the webview
> build … §9 is the native port appendix. **Where they conflict, §1–7 win.**

So: §1–7 is the design. §8 is one implementation of it, §9 is a second
implementation for a C++ painter that does not exist yet. A question like "what
colour is a knob's filled arc" is answered in §1, never in §8's CSS. A ruling
in DECISIONS.md beats all of it.

## Status: partly built; GUI 3 builds on the published system

- **gui2 (the shipping GUI) has adopted part of the UI Spec** (B37, ADR-116 to
  ADR-120): it is cream by default, with the spec's tokens, widget states,
  knobs and visualizer recipes, and a derived dark theme. It does not yet have
  the page compositions, the bundled fonts or the morph presentation (B37's
  "Remaining" list after its latest increment, 5, 2026-08-24).
- **GUI 3 (B302)** assembles the labs into a new GUI instead of editing gui2,
  and builds on the published system (B531). The system is built and being
  revised; the human ratifies it (B531 acceptance). Whether gui2 meets a given
  ruling below is not claimed here; B37 and B302 hold that status.
- The interaction specs (knob, slider, map mode) are ingested (B532, ADR-261);
  GUI 3's controls must match them.

## What the rulings changed

ADR-259 (2026-10-21) put eight design-system questions to the human, and
ADR-117 had left some of them open. These statements in the earlier README are
**replaced**:

| topic | earlier statement | ruled answer |
|---|---|---|
| window | UI Spec wins: 1180 × 820, min 1000 × 700 | **980 × 720, resizable** (ADR-259 ruling 2). Neither document's size applies. |
| fonts | not stated here (the UI Spec bundles TTF) | **Archivo with Roboto Mono**, bundled so the plugin looks the same everywhere; the wordmark stays the locked, baked logo (ADR-259 ruling 1). |
| glyphs | unresolved: UI Spec "NO glyphs anywhere" against the Design System's glyph on every corner | **Corner glyphs: always.** A corner's colour always comes with its glyph, for colour-blind users and single-colour screens (ADR-259 ruling 3, closing ADR-117). Pips on knobs take their glyph's shape, and the readout and inspector carry the letter (ADR-261 ruling 1). |
| corner names | GLASS, GRIT, HOLLOW, BLOOM | **Never product copy.** A corner shows its preset's real name; an empty corner reads "Empty" (ADR-262 ruling 3). |
| re-theming | unresolved: "corner colours never re-theme" against "corner identity becomes per-direction" | **The chassis carries the corner colours.** horde's frame around each FX screen shows them; inside, a screen uses its own glyphs or A–D, so each skin stays true to itself (ADR-259 ruling 4). Modulation sources keep four colours (envelope, LFO, macro, engine) that are always distinct from the corner palette (ADR-261 A1 item 1). |
| dark chassis | open question 3: a "derivation, not a ruling" | **Ratified after a contrast audit; light (cream) stays the default.** The audit is part of the build, and the human reviews it (ADR-259 ruling 7 and Consequences). The earlier README's four derivation assumptions (violet-biased ground; wells darker than cards; a darker ghost trace; screens as a separate axis) were the starting point; ADR-259 does not rule on them one by one. |
| contrast | 2.84:1 on `.note`, `.vlabel` and the inactive tab: "ruling wanted" | **Darker text and line colours that meet contrast, taken from the ORCHID screen scheme, and every hit target at least 28 px** (ADR-259 ruling 8). |
| screens | screens a separate axis from the chassis, pairing by default, as a derivation | **Screens follow the working skins:** Shriek on the 80s workstation, Sluice on the Game Boy Color; Scape, Bulwark and Echo audition the rest; a plain Drive uses horde's house screen (ADR-259 ruling 5). Schemes are user-selectable again and each redefines the same screen roles at its stated contrast (ADR-262 ruling 2). |
| keyboard | not stated here | **An opt-in keyboard mode,** off by default with a visible lamp; notes stay bit-identical while it is off (ADR-259 ruling 6, B449). Map-mode and morph-knob hotkeys work only in it (ADR-261 ruling 2). |
| knob sweep | not stated here | **280°, the UI Spec** (ADR-261 A1 item 2). |
| FX module colour | not stated here | **Cream modules with a small screen each,** in the module's own display style; the six modules without a chosen skin show a labelled stand-in. This replaces ADR-266 A1's distinct house colour per module and retires the `fx-` fill tokens for per-module `fxs-` screen tokens (ground, ink, mid) (ADR-273, B548). **Cables keep the comparison page's colours** as a fourth token, `fxs-<module>-cable`, with an ink casing (ADR-273 A1). |
| gain stages on the Chain line | not stated here | **Fixed offset bars in a gain lane under the line;** the level dots only measure. This replaces ruling 2's "drag a dot" (B547) (ADR-272 A1, B549). |

The value conflicts ADR-117 listed that no ruling touched still follow the UI
Spec: shadow 1.5px 1.5px 0 (Design System: 2px 2px 0), meter fill `#A6F219`
(acid `#9BE514`), and `well` `#FFFFFF` with `well-alt` `#E9E3D4` (Design System:
`#E9E3D4`). They are listed so nobody "corrects" the implementation toward the
losing side. If the published system differs on one of them, file it against
B531.

## What else the rulings added to the system

- **The Specimen** (ADR-139, MAIN's visualizer under the XY pad) is a
  component. It is voice-bound, so the FX device needs an audio-driven stand-in
  (ADR-262 ruling 1, ADR-254, B528).
- **Per-corner lock and revert,** independent per corner. Lock refuses writes
  to that corner and the control shows why; the corner still morphs. An edited
  corner shows a mark (ADR-262 ruling 4).
- **The census ring:** a fixed count of dots around the reader, in proportion
  to each corner's share, apportioned by largest remainder (ADR-262 ruling 5).
  B531 records it as 12 plain dots, optional and off by default.
- **The four labs' components** follow ADR-266: mod matrix grid with trace lanes
  and compact wires; four polarity states including Control; a hosted module's
  macros read-only in horde's matrix; S&H as an LFO shape; retrigger per
  modulator card; a 2-D random source (prototyped first, B540); live morph
  grid edits; STEPPED rate plus sync; one cohesion control. Ruling 5 (FX
  palette by screen colour) was replaced by ADR-266 A1 and then by ADR-273.
- **The ring and gain staging** (B544), **the Chain line and blend maps**
  (B545) and **the Chain line's gain bars** (B549) are components of the
  published system.
- **The stream selector** is three toggles, Direct, Chord and Arp, all on by
  default (ADR-272 ruling 1, ADR-270). It stays reserved in 1.0.
- **FX device consequence** (ADR-254): the Specimen needs a stand-in, and the
  rack, matrix and chain views start at the audio input.

## Which document wins

For a question about a **value**: a ruling in DECISIONS.md first; then the UI
Spec (`HORDE-UI-Spec.dc.html`) §1–7 (ADR-261 A1). The published system is built
from the UI Spec's §1 and §8 (ADR-259 Context) with the rulings applied; a
difference between the two that no ruling explains is a defect to file against
B531.
For **interaction** (knobs, sliders, map mode): the two protected specs
(ADR-261 ruling 4). For a **page composition**: the ruled compositions in
ADR-259's Context, then the published system, then
`HORDE-Design-System.dc.html`, whose own header defers on values: *"Values are
normative in HORDE UI Spec."* House rules win over the prototypes: Archivo with
Roboto Mono, the cream default theme, horde's corner palette, 28 px targets
(ADR-261).

The published system is private to its owner, but its source is `master/` in this
folder (ADR-336), so a reader without access reads that instead of asking for a share.

## Measured findings kept as history

On the light chassis, `.note`, `.vlabel` and the inactive `.tab` sat on `--dim`
(`#9A93A3`) over `--panel` at **2.84:1**; the derived dark chassis put the same
elements at 4.63:1 (2026-08-24). That finding led to ADR-259 ruling 8 above.
Labels had earlier moved from `--dim` to `--t2` (2.84 to 9.06:1, ADR-118 A3).
The UI Spec's own `--dim` value is unchanged in the `.dc.html`.

## Where to read more

- `DECISIONS.md`: ADR-116 (intake), ADR-117 (where the two documents fight),
  ADR-259, ADR-261, ADR-262, ADR-266, ADR-272, ADR-273.
- `ROADMAP.md`: B37 (the gui2 re-skin), B302 (GUI 3), B531 (the system), B532
  (the interaction specs), B544 to B549 (ring, gain staging, Chain line, FX
  module colours, gain bars).
- `docs/audits/2026-10-21-repo-audit.md` H5: why this file was rewritten
  (B536).
