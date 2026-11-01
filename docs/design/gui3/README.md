# GUI 3: the wired shell

*Last verified 2026-11-01, branch `gui-shell-r1b` (the GUI session's shell hop, half B: batch 2 of the master, ADR-338, ADR-340, ADR-341; B584, B589, B593), on slice 7 plus batch 1 and the hop; the page structure below the "density revision" heading is superseded by the hop's section.*

`docs/design/gui3.html` is GUI 3's prototype: one window with twelve pages (the Random page folded into the
Morph page, ADR-322 item 2; a draft Arp page, ADR-330), every parameter that has a machine-readable
manifest reachable, a bottom modulator bank on every page, and no audio. It is built from the design
system's vendored bundle (`../arp-face/`). Its controls are generated from SPEC-ENGINE section 5, the
morph's settings (Appendix D.2), the hosted FX modules' manifests and Arp's (ADR-330). It talks to a stub
engine, a stub morph resolver and a stub device side (rack and MIDI module) through one adapter and a
swappable transport. The WebAssembly engine (SPEC-ENGINE IF-10, clean-room stage 8) replaces the stub
behind the same seam.

## Map

| file | what it is | edited by |
|---|---|---|
| `../gui3.html` | The shell: pages, chrome (docked pad, info box, right-click menu), the Morph page's panel, the seam panel, self-checks. Builds every parameter control in one function from `shell-data.js`, and names no parameter itself. | hand |
| `layout.json` | Which key goes on which page and in which group, widget overrides with their reasons, PA-8's live-when for the designed pages, a page's shell-built panel, the bank's critical controls per modulator kind; since the hop, each grid page's `abbr` column (a group's map of control to its label of at most 4 characters), a group's `matrix` (C2) and `fold` (P2). | hand (the page designer) |
| `params.json` | SPEC-ENGINE section 5 as data: 330 rows (317 host parameters, 13 reserved), with label, unit, law, range, class, options, default and its normalised v, and the row's spec text. | generated |
| `shell-data.js` | Every row the shell shows (the engine's, D.2's, the FX modules', Arp's), each with its `target` (engine, resolver, rack, midi) and `source`, plus the pages' control lists and the bank's, as one script the shell loads from `file://`. | generated |
| `vendor/*.json` | Verbatim, dated copies of the hosted FX modules' manifests (provenance below). Never edited; re-vendor. | copied |
| `engine-stub.js` | The no-audio engine (`GUI3Engine.METHODS`, `GUI3Engine.Stub`), the morph resolver's stub (`ResolverStub`, IF-12's calls), the device side's stub (`DeviceSideStub`: the rack's modules, RC-5 and IF-7, and the MIDI module, Arp), and the synchronous transport over the three (`GUI3Engine.TRANSPORT`, `GUI3Engine.SyncTransport`). | hand |
| `device.js` | The adapter the shell talks to: morph, corners and their presets, routes (and `syncRoutes`, which writes the map-mode session's working list into them), pending modulators and map drops, history, mirror and confirm. `GUI3Device.create(data, transport, {seed})`. | hand |
| `shell-logic.js` | The decisions of the shell that are not drawing, held apart so the seam check runs them: the page bar's groups (`navGroups`), an envelope section's readout (`followedSummary`), the map-mode bridge between the design system's session and the device (`attachMap`), the scope of the map keys and Escape (`MAP_KEYS_ALWAYS`, handed to the session as `mapKeysAlways`) and what the info box says about a section (`sectionInfo`). `GUI3Shell`. | hand |
| `sections.json` | What hovering a section's header shows in the info box (ADR-335 ruling 4): one sentence per section, drafted from the spec text that defines it, each with its source (a path and the heading it cites), the whole file marked a DRAFT the human has not read. Keyed by the shell's section ids. A section with no entry is not described: the box says so and the generator counts it. | hand (a draft) |

Regenerate after editing the spec or the layout: `python3 tools/gen_gui3_controls.py`.
Open the shell from `docs/design/` (or `tools/labs_preview.sh`); `gui3.html#noise,morph` opens a page
with morph on and the corners scattered.

## Provenance of the vendored manifests (ADR-330)

Each source beside SPEC-ENGINE 5 is a copy of another repo's file, never edited here. The generator reads
this table: a vendored file with no row, or whose bytes do not hash to its row's sha256, is red. The path is
relative to `docs/design/gui3/`; the commit is the source repo's HEAD when copied (2026-10-30).

| file | source | commit | sha256 |
|---|---|---|---|
| `vendor/echo.params.json` | Echo `spec/params.json` | `440a04494b980e6fdf318358e862fa2ddbf5f8a2` | `9fe29e7beae5edc8f9f2a0b34372669580883733451a0f364a16ea309764e7e0` |
| `vendor/echo.manifest.lock.json` | Echo `core/manifest.lock.json` | `440a04494b980e6fdf318358e862fa2ddbf5f8a2` | `a590363559ddfee7021e8e0e7528078186eae891c72a04fb62eb91d4434b56f4` |
| `vendor/scape.manifest.lock.json` | Scape `core/manifest.lock.json` | `70aa924716256d5ea1e719cda10eaed96babca04` | `6cacf78e6885e6fb7dd2002dee3ad4c9eec97ce8b9e9e7dabe594636d4de1406` |
| `vendor/shriek.manifest.lock.json` | Shriek `mawcore/manifest.lock.json` (the Maw repo) | `9cd79bb58fef062dd8f565ee5446093b8d0ae9a4` | `cd8f31a5152bebe1186b5e64e16e7b71dd9e502084ffba647d9a792d28289f1c` |
| `vendor/bulwark.manifest.lock.json` | Bulwark `core/manifest.lock.json` | `88ac52703ae56678807ad21377f3febf56055345` | `8294a43e60a165c6d1f81451bbf215d18f1d19bdad06b7946f7eff71402dbeeb` |
| `../arp-face/params.snapshot.json` | Arp `spec/params.json` (vendored by B553 at `de4e2af`; byte-identical at the HEAD named here) | `922f1e22e20c0e876cd92c38c7a99ddb135395e2` | `de3704d1a06214da1cf395e16c202b01b9dbf47e3cd82beb64a8534f14d6c63e` |
| `../arp-face/bank.snapshot.json` | Arp `spec/bank/starter.json` (the same; it supplies the pattern options) | `922f1e22e20c0e876cd92c38c7a99ddb135395e2` | `6925aa22be0c9100c09b4ad280e2bda911ccc39c95f911c0acbde304336f8490` |

**Which file is each module's manifest, and why.** Echo: `spec/params.json`, its declared manifest (its
schema line names it the B439 / RS-2.1 manifest, with ranges, defaults, laws and labels); its
`core/manifest.lock.json` freezes the same keys' identity without ranges, so both are vendored and the
generator checks they list the same keys. Scape, Shriek, Bulwark: their `manifest.lock.json`, the only
manifest each has. **What they do not declare is not filled in:** Scape gives no labels and no laws (only
its `io.*` follow RS-4.6's io law), so its controls show their keys and a linear display, marked as such
in the info box; no module manifest carries descriptions, so the info box says "no description in the
module's manifest"; Arp gives descriptions (`note`) for 26 of its 47 settings and no laws (its four
continuous settings are shown linearly).

**Owed, with no manifest found (2026-10-30):** Sluice, Weir. Nothing is shown or invented for them.

**The sources and where their values go.**

| source | keys | rows | where the values go |
|---|---|---|---|
| SPEC-ENGINE 5 | as the spec names them | 330 (317 host) | the engine (IF-11) |
| SPEC-ENGINE Appendix D.2 | `morph.*`, as the spec names them | 13 (11 host; `morph.seed` and `morph.audible` are page choices) | the morph resolver (IF-12), never the engine |
| Echo, Scape, Shriek, Bulwark | `fx.<module>.<key>`; a Bulwark face's `fx.bulwark.<face>.<key>` | see the generator's GREEN line | the rack (RC-5, IF-7), never the engine |
| Arp | `arp.<key>` | 47 | the device's MIDI module, never the engine |

## The widget rule (the design system's rule, ADR-319 item 5)

| a parameter that is | is drawn as |
|---|---|
| on/off (two options, `off` and `on`) | a toggle (`hd-toggle`) |
| 2 to 5 choices | buttons (`hd-pills`) |
| 6 to 64 choices | a drop-down (`hd-select`) |
| more than 64 choices | a stepper (`hd-stepper`) |
| continuous | a knob (`Knob`) or a slider (`Slider`) |

A page may override the rule for a parameter, with a reason: in `layout.json`, `"widget"` names the
override and `"reason"` says why. The generator (`tools/gen_gui3_controls.py`, `default_widget`) applies
the rule, and its check fails when an override has no reason, or a reason sits on an entry the rule
already gives. Today's one override: the sub's Wave as buttons (seven shapes, the sub's first choice).

## Morph off, snapshots and presets (ADR-319 items 1 and 2)

- **Morph off:** only the audible corner sounds and takes edits; the other three are silent and
  frozen (a write addressed to one is refused), and come back unchanged when morph turns on. The Morph
  page chooses the audible corner, A by default.
- **Save current sound to snapshot** (Morph page): what is heard now becomes the selected corner (its
  chip; with morph off and none selected, the audible corner), under a preset name, and is saved.
- **Save preset to [corner]** (one per corner): that corner's own values become its saved preset, under
  a name. The bundle's edited mark and revert compare against it.
- **History:** turning morph on or off is one undo step; the morph position is performance and is not.

## The bottom modulator bank (ADR-322 items 3 and 4, B556, ADR-291 item 2)

- **What it lists is read, never kept:** the modulators with a route (the adapter's routes, RC-4) to a
  control on the current page, plus those activated with + and not yet mapped. On a page with no engine
  parameters (Filters, Morph, FX) it lists only the pending ones.
- **Each card** is the bundle's `ModCardHeader` (kind colour, name, kind tag) with a **Map** handle, its
  kind's critical controls, and a foot saying how many of its routes land on this page, or that it is
  pending.
- **The critical controls are generated from the table:** `layout.json`'s `bank.kinds` names, per kind, a
  block pattern and the row suffixes, with the reason; the generator makes one control per row by the
  widget rule and refuses a suffix a block lacks. **Why these:** today the only modulators with rows in
  SPEC-ENGINE 5 are ENV 1 to ENV 4 (5.2), so an envelope's card carries its **attack, decay, sustain and
  release**: they are its shape at a glance, and each moves the sound. Hold, the three curves, velocity
  and one-shot refine that shape and stay on the envelope's page (Main for ENV 1, Mod for ENV 2 to 4).
  The engine's other published sources (velocity, aftertouch, R and psi, the onset burst, gravity, the
  MPE expressions; IF-3) have no rows, so their cards say so. The LFOs and macros are deferred (RC-4):
  when their rows exist (an LFO's rate, shape and depth), a `bank.kinds` entry adds them, with no shell
  change.
- **+ Modulator** activates a modulator that is neither routed nor pending. It is **pending**: shown,
  routed nowhere, outside the patch and the history. Closing the interface or ending the session
  (`pagehide`, or "Close the interface" in the seam panel) drops it unless it was mapped (`dev.close()`).
- **The map handle:** press Map, drag, release over any control. The drop goes through `dev.drop`, which
  is `addRoute`: a stepped or reserved row, or a source the engine does not publish, is refused (RC-4),
  and a refused drop leaves the modulator pending.
- **Both toggles, for the human to rule after playing (ADR-322 item 4):** (a) **Mods | Macros** flips
  the strip between modulators and macros (no macros exist yet, so that view says why); (b) **Bank**
  shows or hides it.
- **Window height:** with the bank shown the window is taller than ADR-259's 720 px; a card is one row of
  bundle knobs (about 200 px). A compact card (the bundle's `bare` knobs, or a smaller knob) is a
  proposal below.

## The density revision (B589, ADR-329): the page bar, dense pages, the dashboard, the console

The design master was revised (artifact `1793336699-65f2`, B589; its per-element modifier is `.hd-sm`) and re-vendored into `../arp-face/`. The shell uses it so:

- **The page bar** is `PageNav`: groups Main, Osc, FX, Mod, Morph, Mix, Set over the selected group's pages. **Hist is
  omitted:** the shell has no History page yet. Osc holds Dashboard, Osc 1, Osc 2, Sub, Noise and Filters (the lead's
  default: Filters under Osc). **Arp sits under Mod** (Mod: Envelopes, Arp), flagged for the human: the ruled set has
  no Arp page. An oscillator page's tab carries its on-dot (`oscN.on`, `sub.on`, `noise.on`). Every page still opens by
  its own id: an old `#noise` link opens Noise under Osc.
- **Dense pages** (Main, Osc 1, Osc 2, Sub, Noise): `.hd-cell` is the one box level; each module is a `SectionHeader`
  (identity rule by number: 1 solid, 2 dash, 3 dot), its power switch and its main enum in the header line, clusters
  under `.hd-sublabel`s, controls at the compact rung (the page's headline level at primary), the value on hover and the
  section's readout slot reading the hot control. Rarely-used rows sit under each section's collapsed **Advanced** group
  and are still mounted (the reach check and the self-checks count them). The pictures (a SAMPLE drawing of the settings
  the layout names, with the bundle's painters, never the engine's output) take the left column: the ring and one cycle
  on the oscillator pages, one cycle on Sub, the colour as a tilt on Noise, ENV 1 on Main (its stages and C.12's curves;
  the Hold row is not drawn).
- **The Osc dashboard:** one card per source with its on switch, a SAMPLE picture and the bare necessities as utility
  controls. The lead's default, mapped to real keys: Osc 1 `mix.osc1`, `osc1.detune`, `osc1.coupling`; Osc 2 `mix.osc2`,
  `osc2.detune`, `osc2.coupling`; Sub `mix.sub`, `sub.pulseWidth` (the sub's only row labelled Width: its pulse width,
  live only on the pulse wave, PA-8); Noise `mix.noise`, `noise.colour`. A card's controls are the same keys as the
  source's own page (one model, two views): the generator refuses a card key its source page does not carry.
- **The console** (Mix): `ChannelStrip`, one strip per source and the master. The faders are `mix.osc1`, `mix.osc2`,
  `mix.sub`, `mix.noise` and `out.level`; on, mute and solo are the engine's own rows (`oscN.on`, `oscN.mute`,
  `oscN.solo`, the sub's and the noise's; IF-6), not placeholders. The meters and peak readouts are the stub's **MOCK**
  meters from its observation view: no audio, so a strip reads its own level as if fed a full-scale test tone, through
  IF-6's gate; the output sums the sounding sources. The CLIP lamp latches over 0 dBFS (the latch lives on the engine
  side, `clearClip`, IF-6) and clears on a click.
- **What does not fit 980 x 720** (measured with the review link's `fit` flag): Main, the dashboard and Mix fit, with the
  bank shown or hidden. Osc 1 and Osc 2 overflow by about 172 px with the bank shown (100 px with it hidden): the
  oscillator section and two blade sections at the compact rung take two rows each plus their Advanced line; the
  pictures stay in view while the controls scroll. Sub overflows by 90 px (17 px without the bank); Noise by 314 px
  (241): seven sections. The page area scrolls inside the window; the window keeps its size.
- **Review links:** `gui3.html#osc1,light,win` (or `dark`) shows the window alone at its size; `,nobank` hides the bank,
  `,fit` prints the fit line, `,morph` and `,bank` prepare a morphing or mapped state.

## The shell hop, half B: the tiny tier, companions, clusters and the page view (ADR-338, ADR-340, ADR-341; B584, B589)

Batch 2 of the design master (2a: the controls and routes; 2b: the page forms) reached the vendored bundle, and the shell now
mounts it. **Half A** (the bank, the Mod page, removal, source switching, Escape and the map keys) is a separate hop and its
own section. Every parameter page except Mod (half A's) is a **page view** of the master's parts; nothing is drawn locally.

- **The page** (`paramPage`, `gui3.html` block E; pages marked `"grid": true` in `layout.json`: Main, Osc 1, Osc 2, Sub, Noise, Morph,
  FX, Settings, Arp): the bundle's `PageView` in one cell, a **Grid | Table** switch with the find box, remembered per page
  (ADR-340 r6); the pictures and the **lens** (P1, `Lens`) in its left column, which stays in both views; one cluster per section in
  the grid. Sections flow two to a row (they are mostly one or two rows of cells), and a section with a fold, a main enum of more than four
  segments or more than 20 cells takes the row. The **table** lists every control but a followed envelope row (that row is its source
  page's own): the full name, TS2a's value cell for a knob, a caret select, the dense switch, K4's drag number. Both views are built at
  load (PV1 builds the other lazily), so a control is mounted whichever view a page remembers; the grid comes first in the document.
- **The tiny tier** (ADR-338 r1/r3, ADR-340 r1/r2): a continuous control of a grid page is `Knob` `size: 'tiny'` (T2d, a 20 px face in a
  28 px cell) under its abbreviation; the value is the section's slot's and the info box's (H1). The layout's `size` (the headline level at
  `primary`, a dashboard card's `utility`) is honoured where the master puts it (the dashboard, the bank) and not in P1's grid, where the
  lens enlarges whatever is under the pointer. The tiny **slider** (TS2a, the table's value cell) is the table's face; TS1a (the route depth
  row) belongs to the Mod page's routes (half A).
- **The abbreviation column** (ADR-338 r10): `"abbr"` in a group of `layout.json`, `{ "<key inside the block, or the whole key>": "ABBR" }`;
  a followed envelope's rows go by suffix (`attack`), one line for the four blocks. The generator checks at most 4 characters of letters,
  digits and `& / + . -`, unique in the section (each followed block on its own), that a grid page labels every control but a header's power
  and modes, and that the map names only controls the group has. The abbreviations were derived mechanically from the labels (a word list for
  the engine's own terms, the first letter and the consonants for the rest, `Stage 2 Drive` is `DRV2`) and are **the human's to review**; the
  ones that are not simply a prefix are listed in the trace.
- **The shown options** of a segment group (`Companion` `short`): the manifest's labels exactly as listed are the title; the face shows an option
  of at most 4 characters as it is, the initials of a longer one of several words (`blade 2 over 1` is B2O1), or the first 4 characters of a
  longer word; two that still read alike are vowel-dropped, then numbered (`generator: short_options`).
- **Companions** (ADR-338 r2, ADR-341 r3): a toggle, segments (2 to 5 options), a select (6 to 64) are TC1's 16 px faces; a stepper is
  `Stepper` `tc: true` (a number field on Option-click; a named stepper opens its dropdown, in the bundle). The table's are TC2's flat forms.
  A pill group's label carries the owner's rule and glyph with morph on, as the pills did.
- **Clusters** (ADR-338 r4): C1 for a section (groups of 28 px cells, one shared label line, the power switch in the header, a main enum as
  segments beside the name). **C2** (`ClusterMatrix`) is Main's ENV 1: stage rows A, H, D, S, R by Time and Curve, a dot where no curve exists
  (`"matrix"` in the group; the generator checks it names every control of the group once). **Not done: the two blades as a C2** (reading below).
- **The fold** (P2, ADR-338 r5, only where a page still overflowed): `"fold": true` on the six oscillator sections that have advanced rows
  (Osc 1 and Osc 2, each its oscillator and two blades). Face is the first two sub-groups the layout lists, All the rest, +Adv the advanced rows
  (what `Advanced · N` held); remembered per section. The fold's header has no power switch, number or rule pattern, so the switch and the mode
  go first in Face and the number is part of the name. Every tier is built at load, so the Mounted check and the reach check see every control.
- **One parameter, many faces** (the bundle's `ctlFace`): the grid, the table and the lens each build a shadow of one registered props object, so the
  corner values and the global value are shared and the registry and the revert baseline are never reset by a second face. The shell registers
  each key's owner first (the first page that is neither a dashboard card nor a followed row), before any page is built, so the registered path is
  the owner's whatever order faces are built in; every `mountControl` goes through `ctlFace`.
- **DR-c** (ADR-341 r2): every section's header slot is the bundle's, 250 x 26 px at rest and hot, two lines when a control is hot (line 1 the
  editing corner's own value, a PLACEHOLDER the master marks; line 2 `now`); the shell's style sheet names neither the slot nor its parts (a
  self-check and a seam test hold that). In a half-width section the bundle's own header truncates the slot (`.sh-end`), which is why a fold, a
  long enum and a wide section take the row.
- **Fit.** With the tiny tier, the lens, two sections to a row and the folds, Main, Osc 1, Osc 2, Sub and Noise measure 432, 464, 422, 401 and 401 px
  against the 472 px ADR-338 r7's 68 px bank row leaves the page. **The page area is 374 px in this tree** (the foot row is still 216 px tall until
  half A's bank lands), so on this tree alone they scroll by up to 90 px; the self-check measures against the larger of the two and says so. The ring
  is 112 px tall and the cycle 40 (from 210 and 96) to leave the lens its room in the 206 px column: a consequence to see, not a ruling.
  Osc 1 with its three folds at +Adv is 861 px, so the folds are what hold it. The draft pages (FX 1225, Morph 655, Arp 275, Settings 146 px) are
  not held to the window.
- **Not done.** (6) **The level cell** (`Knob` `size: 'level'`, T4c-f): the master's README gives it no placement (it says what the cell is, not where a
  level goes), so nothing was mounted; a level in a row (the Out sub-group's level, a macro) is where it would go, for the human or the lead to say.
  (4, partial) **The blades as a C2**: a blade's mode, wave, edges and mirror are 54 to 76 px faces that cannot sit in a 28 px column, and the two
  blades are separately described sections (`sections.json`), so one matrix across them is a ruling; each blade is a C1 and a fold.
- **Proposals for the design master (new, nothing drawn locally):** 25. A fold header with the section's power switch, number and rule pattern (the
  fold drops all three). 26. The level cell's placement (above). 27. A cluster's group that wraps when it is wider than the row (an ADV group, a
  module's 30 to 87 controls: the shell sets `flex-wrap` on `.cl-grp`). 28. A `PageView` whose grid is built whichever view is remembered (the shell
  builds the second view itself at load and puts the remembered one back). 29. The table's stepped faces report their value to the owner (a
  handle, or an `onInput` echo): `ParamTable` keeps none, so the shell remounts a stepper whose value moved elsewhere.

## The port ledger, the mixer's room and summed views, envelopes on the oscillator views (ADR-333, B593)

- **The port ledger** (`port-tab.json`, the shell's **Ledger** page; a top-level group of its own, flagged: under Set
  is the alternative). One row per item not yet moved over: the item, where it lives today by repo path, why it has not
  moved (one of six reasons: a ruling, a design-system component, a spec table, a sibling's manifest, the engine or
  audio, not yet scheduled) with a sentence, its ROADMAP row, and a status (`partial` says what is missing; `precise`
  marks what the human asked to be copied precisely: the Scalpel bench's Kuramoto ring with its blades, and the
  Specimen XY). Seeded with the human's list (their words in `asked`) and the gaps found in the shell. A row leaves only
  when its item ships. The generator's check refuses a row with no reason, an unknown reason, no sentence, no source
  path, a path that does not exist, no B-number or one the ROADMAP lacks, or a `partial` with nothing missing.
- **The Settings page's failure** (found in slice 5, fixed in slice 6, above): the Set tab opened nothing.
- **The mixer:** more room per strip (a documented override of `size-strip-w`, 72 to 92 px and, in slice 6, 104 px, inside the console only;
  the token change is proposal 21), and a summed oscilloscope and a summed spectrum beside the console (slice 6: in the room to its right, so Mix fits the window with the bank shown), drawn with the
  bundle's `Viz.scope` and `Viz.spectrum` from the engine's observation view (`dev.summed()`, IF-10; MOCK in the stub:
  the sounding sources' test tones at their meters' gains times `out.level`).
- **Envelopes on the oscillator views:** each source's page (Osc 1, Osc 2, Sub, Noise) and its dashboard card shows the
  envelope its level follows, the block its `env` row selects (`osc1.env`, `osc2.env`, `sub.env`, `noise.env`: ENV 1 to
  ENV 4, RC-6.5), drawn with `Viz.envelope` (C.12's curves) and edited with that block's Attack, Decay, Sustain and
  Release, compact on the page and utility on the card. The controls are the shared rows (`env1.*` on Main, `env2.*` to
  `env4.*` on Mod), all four blocks mounted and the selected one shown, so an edit there is an edit on Mod and the other
  way round (one model, two views; the seam check's `envelope` test and the page's self-check prove it).

## MAP mode, the Set tab, envelope readouts, mixer room (slice 6; the Map Mode spec, ADR-261, ADR-333)

- **The MAP control** (`docs/design-system/HORDE-Map-Mode-Spec.md`, the human's own pick, 2026-10-30) sits in the window's top
  bar, outside the page area, so it is the same on every page: the bundle's `MapControl` (a toggle joined to a source
  dropdown) with the live `SourceStrip` of every source under it (one scrolling row; labelled MOCK: no voices, every source
  reads 0, so the rings sit at their origins). While MAP is on the window's border takes the source's colour. The session is
  the bundle's (`S.map`, polarity cycle, focus, depth drag, rings, captions); **one** session serves every page, and the page
  switch never touches it, so a page change keeps MAP on and the source.
- **Where a mapping goes.** The bundle's gestures edit `S.mappings` in place (a click makes a mapping at 25 % unipolar, a
  drag sets the depth, a release at exactly 0 cancels, P cycles the polarity, Backspace removes). `GUI3Shell.attachMap`
  reconciles that working list into the DEVICE at the end of each gesture (`dev.syncRoutes`: one undo step; a new mapping
  goes through the same RC-4 refusal as the bank's drop, so a destination no route can reach makes no mapping and says so) and
  reads the device's routes back whenever they change (undo, the bank's drop, init), so the ring on a knob, the bank, the
  strip's counts and the history read one set of routes. Never during a drag: a new mapping sits at depth 0 until it ends.
- **Mouse routes of the keys** (hotkeys are accelerators; slice 7 makes them work with keyboard mode off too, below):
  M and Esc = the MAP button; 1 to 9 = the dropdown and the strip's chips; P = the polarity pill and Backspace = the remove
  button of the **inspector** (the bundle's `ModInspector`, docked on the right while MAP is on and gone otherwise).
- **The Set tab** opened nothing: its group is `set`, its page `settings`, and Page nav left to itself asks for a page named
  after a one-page group. `navGroups` gives a one-page group its page explicitly.
- **Envelope readouts:** an oscillator's Envelope section reads the envelope its source follows (its `env` row's pick) and that
  block's attack and decay, not the first block's (`followedSummary`).
- **The mixer:** strips 104 px (from 92), the console's own padding, each strip's side padding and a gap between its rows, and
  extra room left of the on toggle, all `--space-*` tokens through the console's scoped override. The summed scope and spectrum
  (84 px each) and the note sit in the room to the console's right, so Mix fits the window with the bank shown; beside a docked
  pad or the inspector they wrap under the console and the page area scrolls.
- **A bank card draws every control it names:** four utility knobs in one nowrap row of a 220 px card shrank each dial to 0 wide
  (a label and no knob; a layout fault of the shell since the bank's first slice, not of the bundle); the card's row now wraps,
  and a self-check measures it, with a squeezed card as its control.
- **The Ledger stays its own top-level tab** "until it isn't needed at all anymore" (the human, 2026-10-30); its `quick-map` row is
  now `partial` and lists what the spec has that the shell lacks, its `settings` row says the click works.
- **Review links:** `#osc1,light,win,map` opens with MAP on, ENV 2 as the source and two knobs mapped.

## Map keys always, the wider MAP control, the fixed info box and section descriptions (slice 7; ADR-335, B582)

- **The map keys work always** (ADR-335 ruling 1; the human, 2026-10-30: "keys always (we'll have to rework them if they interfere with
  keyboard midi controls; I may ultimately reverse the "keys always" decision after some use)"). M (tap latches, hold is momentary), Esc,
  1 to 9 (the source, only while MAP is on), P and Backspace work with keyboard mode on or off. *Slice 7 took them in a listener of the
  shell's own (`attachMapKeys`); since the hop below (master batch 2) they are the SESSION's own, and the shell hands it the scope as
  the session option `mapKeysAlways`.* A key typed into a text field, a drop-down or an editable region is never the map's; Cmd and Ctrl keys
  are the host's. **Reversing the ruling is one line:** `MAP_KEYS_ALWAYS` in `shell-logic.js` (a test holds it, with the page passing it as
  it stands).
- **The panel docks only while MAP is on** (ruling 2): slice 6's behavior, confirmed by the in-page MAP check and the seam check's `mapkeys`
  rig; nothing changed.
- **The MAP control is whole** (ruling 3). What was buried: the source drop-down and the name in it. The bundle's `.hd-map` is
  `overflow: hidden`, so as a flex item it has no minimum width, and the source strip beside it (`flex: 1 1 auto`) shrank the control to
  87 px of its 213 until only a sliver of the drop-down showed. The shell's scoped CSS now keeps the control at its own width (the strip
  scrolls instead) and keeps the room of the HOLD tag the bundle shows while M is held, so the drop-down no longer moves under the pointer
  during a hold. A self-check measures the control, the longest source name ("Gravity offset") and the strip, with a squeezed control as
  its control. **Design-master proposal 24:** `MapControl` should not shrink below its content, and should reserve the HOLD tag's room.
- **The info box is fixed** (ruling 4, B582): one size (220 x 200 px) at the bottom left of the page column, left of the modulator bank,
  on every page, the foot row 200 px high whether or not the bank is shown. What it says scrolls inside it; hovering moves and resizes
  nothing. A self-check hovers the longest parameter text and the longest section description of every page and compares the page area,
  the bank, the box and the foot row, with a box that follows its text as its control. **Hiding the bank no longer gives its height back
  to the page** (the box keeps its place and size): an open question for the human. **Design-master proposal 23:** the fixed info box as
  a component.
- **Section info:** hovering a section's header or its empty space shows what the section does (a control inside it still shows its
  own parameter). The sentences are in `sections.json`, drafted from SPEC-ENGINE (5.2 to 5.6, 6.1, 6.2, 6.5, 6.10, 7.1, D.2) and each
  citing its source; the file is marked a draft and the box says DRAFT. A section with no source text, the FX page's five cards (four hosted modules; Bulwark has two faces)
  and Arp's (their manifests carry no description of the module), says "No section description yet." and is counted by the generator
  (its GREEN line). `gen_gui3_controls.py --check` holds every entry to an existing section id, a sentence, a source that exists with
  the heading it cites, and the DRAFT mark, with must-fail controls for each.
- **Review links:** `src=<id>` picks the MAP source (`#osc1,light,win,map,src=gravity` is the longest name); `info=<section id or
  key>` shows the info box as if hovering it (`#osc1,light,win,info=osc1%2Fblade-1`).

## Batch 1 of the GUI session: what was vendored is mounted, and the audit's defects (ADR-336, ADR-338; B596)

The design ledger audit (`docs/audits/2026-10-30-design-ledger.html`) found 35 ruled elements in the bundle and not in the shell. The
first GUI-session batch mounts the ones the stub can feed honestly, and fixes the shell defects the audit confirmed.

- **The ring is the oscillator's direct control** (ADR-156, ADR-333): Osc 1 and Osc 2 mount the bundle's `RingXY` over `Viz.kuramoto`
  (the SCALPEL bench's ring, harvested at B544): the blades' rim arcs, the cross-mod arrows, a click that lands Detune (↔) and Coupling
  (↕), a drag that follows, alt for fine, and **shift for Cross-mod**, the third axis (it holds Detune). Positions are the knobs' own
  travel, written through the device as **one undo step per gesture** (self-check 26). The swarm is the bundle's seeded SAMPLE
  (`Synth.ring`), stepped by the frame; the engine's phases arrive with IF-10. The bench's waveform XY with its view modes (at phase,
  members) is not in the bundle: batch 2 harvests it into the master first (the human, 2026-10-30).
- **The Mod page's matrix** (ADR-266, ADR-136): `ModMatrixGrid` and `RouteList` on the session's mappings, which the map bridge keeps
  equal to the device's routes. Columns are the routed parameters plus the selected one (self-check 27). The page's form (M5,
  list + detail, ADR-338 ruling 8) waits on the composition lab's round 2.
- **Gain staging on Mix** (ADR-230, ADR-235, ADR-272 A1): `ChainLine` over `GainStaging`, one gain model between them. Readings
  come from the device's MOCK meters where a tap is published (the four sources, the output); every other stage is hollow, never
  guessed. **Not wired:** the gains are the master chain's placeholder ranges held in the shell, because no stage gain or make-up
  is an engine row (B402, RC-2).
- **The FX rack on FX** (ADR-259 ruling 5, ADR-273): `FxRack`, each hosted module a cream card with its screen inset and cable
  colour, as a **serial stand-in** (the stub publishes no cable state): the body lane through the modules in the page's order to OUT.
- **The census ring** (ADR-262 ruling 5) is on the docked morph pad, as the bundle draws it (plain dots; the ruling's glyph
  shapes are the master's to add).
- **Defects fixed:** the spectrum's peak hold was passed seconds where the painter holds 1500 ms (caps held about 25 minutes); the
  keyboard lamp reads KEYS OFF, as the master says.
- **Escape is a layered cancel** (ADR-338 ruling 11): one press cancels the innermost open thing (the menu, MAP, the pinned corner,
  keyboard mode). *Batch 1 ran its own capture-phase handler (`attachEscape`); since the hop below it is the session's `S.escape()`
  with the shell's layers in front (self-check 25, seam test `escape`).* **Open:** ruling 11 gives panic its own control, against ADR-226 item 6 (no
  panel Panic control); none is built until the human says which holds.
- **The window's size is a trial** (ADR-338 ruling 12): a switch above the window, Fixed 980 x 720 or Scale (75 to 200 %, or Fit).
  Scale zooms the whole composition (CSS zoom, so the bundle's pointer maths hold); a canvas is stretched, not redrawn at the new
  size, which a plugin would do. `#<page>,scale=150` opens scaled.
- **Kept, not removed:** the bank's Mods | Macros pills. The audit read ADR-323 ruling 2 as rejecting them; the ruling says "Mods|Macros
  later", folded into the strip's header once macros exist, and the human asked for exactly that toggle on 2026-10-30 (intent-bus
  macros with the pitch bend and mod wheel, against the rest of the bank). It waits on macros in the master. *(Superseded: the bank's MAC | MODS segments, BT1, are that toggle since the hop below.)*
- **Not mounted, and why:** Main's performance pad (ADR-176 decision 5) drives intent-bus atoms the stub does not have, so it comes
  with the macros; each modulator card's retrigger switch (ADR-266 ruling 7) has no parameter (SPEC-ENGINE has no retrigger row for a
  modulator, only `osc1/osc2.retrigger`), which is the lead's to add.

## The seam and the transport

The shell calls only `device.js`. `device.js` reaches the engine only through a transport: any object
with `send(msg)` and `onSnapshot(fn)` (`GUI3Engine.TRANSPORT`). Messages are `{type: 'set', seq,
values}` and `{type: 'gesture', seq, key, phase}`; a snapshot is `{ack, params, events, observe, state}`,
`ack` the seq of the last message the engine applied.

**Mirror and confirm** (ADR-319 item 3). The display shows what the adapter sent, at once. A snapshot
changes the display only where the engine changed a value itself (host automation, a load, a clamp):
it is skipped for a key whose latest send it has not seen (`ack` older: stale), and for the key being
dragged; an engine-originated value is shown until the user writes that key again.

The engine methods behind the transport (`GUI3Engine.METHODS`):

| method | spec | what the stub does |
|---|---|---|
| `manifest()` | 5, IF-11 | the rows |
| `setParam(key, v)` / `getParam(key)` | IF-11, 5.1, PA-7 | v in [0, 1], a stepped row snapped to its grid; an unknown key, a reserved row and a non-number are refused and counted |
| `value(key)`, `display(key, v?)` | 5.1, IF-11 | the law's x, and its text with the unit |
| `snapshot()` / `restore(s)` | RC-11.2, RC-11.4 | every v and the two `ratios` texts |
| `saveState()` / `loadState(text)` | IF-4, RC-9.3 | IF-4's members as JSON (shape only; the real text format is the harness header's); unknown keys kept; version 0 refused |
| `gesture(key, phase)`, `events()` | RC-11.3 | begin and end reported back unchanged |
| `observe()` | IF-10, IF-4, IF-9 | no voices, zero counters, the heard value of every stepped key |

What is not the engine's stays in `device.js`, as the spec places it: the morph's corners, position,
on/off and audible corner (RC-11.1), the routes (RC-4 defers the matrix), and the undo history (RC-11).

**The three transports.**
1. **Synchronous** (`GUI3Engine.SyncTransport`, today): `send` applies the message to the stub and
   publishes a snapshot before it returns.
2. **The AudioWorklet's postMessage** (with the WebAssembly engine, stage 8): `send` posts the message to
   the worklet's port; the worklet applies it at the next processing call and posts RC-8's snapshot on
   its own clock (about 60 times a second), carrying `ack`; `onSnapshot` listens on the port. Asynchronous,
   so stale snapshots are normal and mirror and confirm handles them.
3. **Shared memory** (B588, to be weighed): the two sides share a `SharedArrayBuffer`, values written to
   a parameter block and the snapshot double-buffered with a sequence counter, as RC-8 describes for
   the plugin. It needs cross-origin isolation headers, which the labs' hosts may not send. The same two
   methods wrap it, so the adapter and the shell do not change.

## The gates (`./verify fast`)

- `python3 tools/gen_gui3_controls.py --check`: the expansion equals 5.8's counts; every layout key is
  a parameter; every host parameter is on a page; no reserved row is placed; widgets suit their rows;
  an override of the widget rule carries a reason; live-when conditions name real options; the shell
  mounts controls in one place and names no key; no gui3 script reads a clock; the outputs are current.
  The bank's critical controls exist in each block their kind matches, and their choice carries a reason.
  Every source beside SPEC-ENGINE 5 (D.2, the FX modules, Arp) passes the same rule and refusals, and every
  vendored file has a provenance row whose sha256 matches its bytes. Every page is in exactly one page-bar
  group; a section's power and modes and a strip's fader, on, mute and solo suit their rows; each dashboard
  card carries its source's power switch and only keys its source page carries; an envelope section's followed
  rows exist; every port-ledger row passes its rules (the shell-logic script is held to no clock like the rest), and every section description (sections.json) names
  a section that exists, is a sentence, cites a source that exists with the heading it names, and sits in a file marked DRAFT. Forty-two must-fail controls (the hop added the abbreviation column's four, a matrix's two and a fold with nothing to fold), among them a module's keys
  left unplaced, a vendored file with no provenance row, a page in no nav group, a dashboard key its source page
  lacks, and a ledger row with no reason, an unknown reason, no source, a missing source, or no B-number.
- `node tools/labharness/gui3_seam_check.mjs`: tests of the stubs, the transport and the adapter
  against the spec, ADR-319, ADR-322 and ADR-330 (morph off, the morph toggle in history, corner snapshots,
  mirror and confirm over a synchronous and a delayed transport, pending modulators dropped on close, map
  drops refused on stepped and reserved rows, each source's values reaching its own side and never the
  engine, the MOCK meters through IF-6's gate with a clip latch that holds until cleared, the summed views read
  from the observation view, an oscillator view's envelope as the shared keys), and, since slice 6, of MAP mode on the
  design system's REAL session, knob, Map control and Page nav (run on a small fake DOM, the bundle read and never
  edited): a click, a drag, the keys and their mouse routes, a page change, a refused destination, the Set tab and the
  envelope readout; and, since slice 7, the map keys with keyboard mode off and in fields, the section descriptions, and the info box and
  MAP control's fixed size (structural); and, since the hop, `abbr` (the abbreviation column, the shown options, a matrix and a fold in the data)
  and `pageforms` (the shell builds from the master's parts and every face through `ctlFace`, and does not restyle the readout slot). Planted faults, each red for its own test (the check prints its counts). `--verbose` prints why each plant went red.
- `lab_load_check` loads the page's inline script (it stops at the missing bundle, as the other bundle
  labs do). The page's own self-checks run in a browser, each with a control (the MAP, Set tab, envelope-readout, bank-card, MAP-control,
  info-box, section-info and map-key checks click, hover, press or measure the real controls).

## Proposals for the design system (nothing here is drawn locally)

The bundle (version `1793336699-65f2`, B589) lacks these; each goes into the design system first. Proposals 12 and 14
(a modulator card, a horizontal strip) are still open after the revision; 18 to 20 are new:

1. **Page navigation**: twelve tabs in two rows of six. The shell uses `hd-pill` with `role=tab`.
2. **The info box** (ADR-315, B582): a fixed box at the page's foot. The shell assembles it from `hd-card`.
3. **A context menu** (ADR-315, B583, B557): the shell assembles one from `hd-card` and `hd-btn`.
4. **The docked pad** (ADR-291, ADR-315) as a component with its hide toggle, not a lab layout.
5. **Ownership on a select, a toggle and a stepper**: only pills carry the owner's rule and glyph (`hd-owned`).
6. **An inert (PA-8) state for the knob, the select and the toggle**: the bundle's `.idle` dashes pills,
   sliders and steppers only, so an inert knob looks live. The shell sets `.idle` on every inert control,
   so the style lands everywhere once it exists, and the info box says why a control is inert.
7. **A bound stepper** for long integer ranges (an oscillator's seed, 0 to 999 999).
8. **A disabled toggle and button**: `hd-toggle` and `hd-btn` have no disabled look, so the Pad switch
   (disabled while morph is off) and "Save current sound to snapshot" (disabled until a corner is
   selected with morph on) look live.
9. **A naming dialog** (ADR-319 item 1): saving a snapshot or a preset asks for a name; the shell uses
   the browser's `prompt`, because the bundle has no text field or dialog.
10. **Corner actions on the corner chips**: "Save preset to [corner]" sits as a row of `hd-btn`s under
    `CornerChips`; the chip row (which already has lock and revert) is where it belongs.
11. **A corner picker** for the audible corner while morph is off: the shell uses `hd-pills` with the
    corner glyphs; the corner palette on the pill is missing.
12. **A modulator card** for the bank (ADR-322 item 3): the shell assembles one from `hd-card` and
    `ModCardHeader`, with full-size knobs; a compact card, so the window keeps its 720 px with the bank
    shown, is missing.
13. **A drag handle** (the Map handle, Serum-style): the shell uses an `hd-btn` labelled Map; a handle
    with a drag affordance and a drop-target highlight on the controls is missing.
14. **A horizontal strip** for the bank: the shell lays the cards out in an `hd-card` row that scrolls
    sideways; a strip component (with the +, the empty state and the Mods | Macros switch) is missing.
15. **A macro card** for the bank's Macros view, once macros exist (RC-4 deferred).
16. **A knob for a value outside the morph** (ADR-330): a Device-class value (the morph's settings, a
    module's I/O gains) has one value, no corners. The bundle draws that only as `locked`, whose readout
    says LOCKED, which reads as a refusal; a "global" state with no pips and no LOCKED is missing.
17. **A module card with its screen inset** (the FX chain rule, ADR-235): the shell uses `hd-card` with
    its `.viz` well as the labelled placeholder for the module's own screen; a module card component
    (name in ink, the inset, the face) is missing.

18. **A compact modulator card** for the bank (B589's size ladder applies to its controls; the card itself is
    still the shell's `hd-card` with `ModCardHeader`). With the bank shown, the window gives the page area 472 px.
19. **A dashboard card's picture** at the card's width with no caption: the shell reuses its page painters.
20. **The class `sm`**: **fixed upstream** (`1793336699-65f2`). The revision's per-element dense modifier is now `.hd-sm`, so
    the Random and filter labs, which use a bare `sm` of their own, render as before (`../arp-face/README.md`).

21. **A wider channel strip** (ADR-333: "a little more space on the mixer"): the shell overrides `size-strip-w` to 104 px, with its
    own padding, inside its console; the token (or a `size-strip-w-roomy` variant) and the strip's padding belong in the design
    system's workshop (B589).
22. **A change event on the session's mapping list** (Map Mode spec): the bundle's gestures edit `S.mappings` in place and tell no
    one, so the shell reconciles on every `S.emit` (shell-logic.js `attachMap`); an `onMap(change)` hook, and the keys accepted
    outside keyboard mode as the spec asks (accelerators, never the only route), belong in the design master. (Slice 7: the shell
    takes the keys itself while keyboard mode is off, ADR-335; the bundle's gate is still the proposal.)
23. **A fixed info box** (ADR-335 ruling 4): one size, scrolling its text, left of the modulator bank, explaining sections as well as
    parameters; the shell assembles it from `hd-card` and its own CSS.
24. **A MapControl that does not shrink** (ADR-335 ruling 3): the bundle's `.hd-map` is `overflow: hidden` and so shrinks to nothing in a
    flex row; it should keep its content's width, and reserve the HOLD tag's room so a hold does not move the drop-down.

## Shell hop A: the bank, the Mod page, source switching and the keys (master batch 2; ADR-338 r7 to r9 and r11, ADR-340 r8 to r11, ADR-341 r5 to r7)

*Last verified 2026-11-01, branch `gui-shell-r1` (half A of the hop; half B, the page forms, is built beside it).* The design master's
batch 2 components replace what the shell drew for itself. Where a component is mounted, and what the shell still adds:

- **The bank is `H.ModBank`** (BLOCK D3): 68 px, two lanes of 26 px thin rows, the BT1 segments **MAC | MODS** in the bank's side column
  (the top bar's Mods | Macros pills are gone: BT1 is that toggle, ruled in its place), a click on a row's name opens its compact card above the
  bank. The shell keeps what it had: **+** (the activation menu, `openAddMenu`), the Map handle (press it, then click a control, which MD1 maps;
  or drag it onto a control: the drop goes through `dev.drop`, so RC-4's refusals stand and a refused drop leaves the modulator pending), the
  page's listing (the adapter's routes to a control on the page, plus the pending ones, read, never kept), and Bank show or hide. The card's
  controls are the table's (`GUI3_DATA.bank`), built the first time its row opens.
- **The MAC lane is `H.MacroRow`** (BR-b): Tone, Space, Time, Motion, Macro 5 to 8 (slots 5 to 8 are numbered, not named: ADR-192 leaves
  their phrases open), Pitch bend and Mod wheel, in the ruled order. The stub has no intent bus (port-tab `macros`, spec-table), so every row
  shows UNBOUND and a drag, a Map press or a drop on one says "not in the engine yet" and sends nothing. The bend row springs back on release
  and the mod wheel's row holds (the bundle's physics; no hardware wheel is fed, `setHardware` is unused).
- **Removal, M-R2** (`dev.removeModulator`, the one addition to `device.js`): × on a row, or on the Mod page, removes the modulator and
  every route it drives as ONE undo step; `H.UndoLine` names them (`H.ModRemoval.describe`) and offers Undo, which is `dev.undo()`, so it goes
  the moment the history moves on. A pending modulator is outside the history: dropped, no step, no Undo. The engine's sources stay in the
  engine, and + brings one back. Every source is a "fixed" modulator (the engine publishes it), so the master's text says "off the list; the
  engine keeps the source" even where the row stays listed (the Mod page lists all sixteen).
- **The Mod page is `H.ModPage`** (ADR-338 r8): a thin row per source grouped ENVELOPES and ENGINE SOURCES, the selected one's full tier
  beside it, a **Detail | Routes** switch. It lists every source, not only the routed ones, because the envelopes' own rows (ENV 2 to 4) are
  the detail's controls and must stay reachable; ENV 1's rows are on Main (its detail says so). The detail has no `editor` (no envelope editor is
  wired to the rows). **Routes** holds the three route views of ADR-340 r10: **Grid** (ML4: the grid and the route list, as before),
  **By source** (ML3, `H.RoutesBySource`) and **Wires** (ML1, `H.CompactWires` with W-a's valves). A drop on a valve is a depth route, which the
  engine does not have (B70, the lead's): `onDepthRoute` says so and makes none. A new wire is a mapping; the map bridge writes it to the device.
  `+` on a kind activates the first source of that kind that is neither routed nor pending.
- **Source switching** (ADR-340 r8): **MS3**, the MAP control's source is the chip (`chip: true`; the shell widens it to 206 px, a placement
  override: the master's 150 px cut six of the sixteen names, and ADR-335 r3 holds); **MS4**, with MAP on a click on a bank row's body (not its
  name, Map or ×, which are buttons) makes it the source; **MS1**, the strip, is unchanged.
- **Escape and the map keys are the session's** (ADR-338 r11, ADR-335 r1): `S.escape()` with the shell's menu layer unshifted in front (the
  bank unshifts its open card itself), and the session option `mapKeysAlways: SH.MAP_KEYS_ALWAYS`. The shell's own `attachEscape` and
  `attachMapKeys` are gone, and the session now starts before the self-checks run (its listeners are what they press). The keyboard lamp follows
  `S.keys` through the session's emit, because the session's own layer turns keyboard mode off.
  **If the human reverses ADR-335 r1** (the one line above): Escape follows the same option, so with keyboard mode off Escape would no longer
  close the shell's menu or the bank's open card (batch 1's shell handler did, in every mode). That is the bundle's rule, not the shell's;
  say so when the ruling is reversed.
- **Readings taken.** The Mod page lists every source (above). The macro slots 5 to 8 are numbered. The foot row and the page area are ADR-345's now (below).
- **Found in the vendored bundle (not patched).** `ModPage` documents `routes(host, S)` as the route-view builder but hands its options to
  the tiers, which call `o.routes(m)` for a modulator's routes: one name, two jobs, and a host that passes the documented builder breaks every
  row. The shell passes a function that answers both. (A master fix: rename the builder, e.g. `routeView`.) `MapControl`'s chip is 150 px and
  cuts long names (proposal: size to the longest source name).
- **Checks.** In the page: 28 to 31 (removal, source switching, the MAC lane, the Mod page); 20, 21, 25 and 27 now measure the bundle's
  components (the compact card, the chip, the session's Escape, the Routes view) with the same laws and controls; 1 and 2 count the Mod page's
  held detail hosts. In the seam check: `escape`, `removal`, `shellparts`, and the key plants re-pointed at the session (see the trace).

## The foot row and the info box's lock (ADR-345; B589, B582)

The human picked F1 from `docs/design/foot-row-lab.html` and added the lock and the sidebar.

- **The foot row is 120 px and the box 300 × 120**, so pages get 468 px. Main, Osc 1, Osc 2, Sub, Noise and Mix fit; Mod, Morph, FX and Ledger still scroll. A bank row's card now opens over the foot of the page.
- **The box is the bundle's `InfoBox`.** It had been the shell's own `hd-card` since ADR-315. The shell says what it shows (`keyContent`, `sectionContent`) and what counts as a selection.
- **Any selection locks it.** A press on a control or section is heard in the capture phase, so a control that stops the event can't hide it, and a selection made elsewhere (`S.selected`) locks too. A press on anything else, Escape, or the head's Release button releases. Escape leaves MAP first. **Hovering shows nothing new after a first touch until that release**: the human chose this over a still-click lock, and may revisit it after use.
- **With no lock, the box keeps its text while the pointer is on it**, which is the lab's finding fixed.
- **Auto-scroll:** a description past the edge waits 2 s and moves at 8 px per second until it is all in view. The source line under it may stay cut off.
- **The Info sidebar toggle** is on the Settings page, in a new Interface card, and is remembered per viewer (`hd.info.sidebar`, or `#page,sidebar` in a review link). The box moves to `#rail`, under the morph pad while the pad shows, and the foot row is the bank's 92 px. With the bank off too, the foot row goes away.
- **Locks stay off while the self-checks run**, because they press and select controls for their own reasons (`infoLocks`).

## Readout slots (ADR-340 A1; B589)

A section's readout slot is fixed at the width its header leaves, up to 250 px, and its text never changes that width. 13 of the 38 are narrower at the 980 px window, the narrowest 138 px, so line 2 can be cut off in a crowded header. The human chose this over wrapping crowded headers to a second line. The self-check tests every slot on every page, at rest and hot, with a slot allowed to follow its text as its control. Two have no control that drives them (Sub · Voice, Settings · Engine) and are checked at rest only.

## Known gaps

- **Descriptions:** no `desc` column exists for horde 2's parameters (B582). The info box shows each
  row's spec text, labelled SPEC TEXT. Missing: 317 of 317; two rows (`sub.semitones`, `sub.fine`) have
  no spec text at all. Section descriptions (`sections.json`) are drafts the human has not read; the generator's GREEN line counts
  the sections that have none.
- **Manual anchors:** "Open in manual" is a stub until B451's pages exist (B583).
- **Owed by the modules (ADR-330):** Scape's labels and laws; descriptions from every FX module's
  manifest (none gives any); Arp's laws for its four continuous settings. Sluice and Weir have no manifest.
- **The morph's settings are held, not obeyed:** the resolver stub keeps D.2's values and the position,
  but the device's blend is still the bilinear stand-in (Blend); Quantum, Stepped, the count, the glide and
  the seed's draw are the resolver's (MO-6 to MO-10) and come with it.
- **The rack order** on the FX page (Echo, Scape, Shriek, Bulwark's compressor, then Bulwark's limiter as
  the master) is the FX chain lab's, not a ruling.
- **The Randomize panel** on the Morph page is a placeholder that lists its contents to come (ADR-318,
  ADR-320, ADR-321, ADR-322 item 1) and links to the Random lab, which stays the bench.
- **Not in this slice:** the other pages' designs (B581's order), the rest of the Morph page (B581 step
  3) and the Randomize panel's build, moving the parameter table beside `engine2/structure` (B586), PA-8
  as a table in the spec (B587), the mod map, the per-parameter morph mode (B557), keyboard mapping from
  the bank (the Map handle is pointer-only), the inspector as an always-on rail with reorderable rows, the corner-scope switch, the WebAssembly engine, gui2.
