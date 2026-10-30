# GUI 3: the wired shell

*Last verified 2026-10-30, branch `gui3-b584-slice4b` (ROADMAP B584, slice 4b; ADR-316, ADR-319, ADR-322, ADR-329, ADR-330; B589), stacked on slice 4a and spec round 48.*

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
| `layout.json` | Which key goes on which page and in which group, widget overrides with their reasons, PA-8's live-when for the designed pages, a page's shell-built panel, the bank's critical controls per modulator kind. | hand (the page designer) |
| `params.json` | SPEC-ENGINE section 5 as data: 330 rows (317 host parameters, 13 reserved), with label, unit, law, range, class, options, default and its normalised v, and the row's spec text. | generated |
| `shell-data.js` | Every row the shell shows (the engine's, D.2's, the FX modules', Arp's), each with its `target` (engine, resolver, rack, midi) and `source`, plus the pages' control lists and the bank's, as one script the shell loads from `file://`. | generated |
| `vendor/*.json` | Verbatim, dated copies of the hosted FX modules' manifests (provenance below). Never edited; re-vendor. | copied |
| `engine-stub.js` | The no-audio engine (`GUI3Engine.METHODS`, `GUI3Engine.Stub`), the morph resolver's stub (`ResolverStub`, IF-12's calls), the device side's stub (`DeviceSideStub`: the rack's modules, RC-5 and IF-7, and the MIDI module, Arp), and the synchronous transport over the three (`GUI3Engine.TRANSPORT`, `GUI3Engine.SyncTransport`). | hand |
| `device.js` | The adapter the shell talks to: morph, corners and their presets, routes, pending modulators and map drops, history, mirror and confirm. `GUI3Device.create(data, transport, {seed})`. | hand |

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

The design master was revised (artifact `1793334278-f870`, B589) and re-vendored into `../arp-face/`. The shell uses it so:

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
  card carries its source's power switch and only keys its source page carries. Twenty-one must-fail
  controls, among them a module's keys left unplaced, a vendored file with no provenance row, a page in no
  nav group, a dashboard key its source page lacks, and a dashboard card without its power switch.
- `node tools/labharness/gui3_seam_check.mjs`: nineteen tests of the stubs, the transport and the adapter
  against the spec, ADR-319, ADR-322 and ADR-330 (morph off, the morph toggle in history, corner snapshots,
  mirror and confirm over a synchronous and a delayed transport, pending modulators dropped on close, map
  drops refused on stepped and reserved rows, each source's values reaching its own side and never the
  engine, the MOCK meters through IF-6's gate with a clip latch that holds until cleared), twenty-one
  planted faults (one each for a module's, Arp's and a morph setting's values sent to the engine; a meter
  that ignores solo; a clear that does not unlatch). `--verbose` prints why
  each plant went red.
- `lab_load_check` loads the page's inline script (it stops at the missing bundle, as the other bundle
  labs do). The page's own fourteen self-checks run in a browser, each with a control.

## Proposals for the design system (nothing here is drawn locally)

The bundle (version `1793334278-f870`, B589) lacks these; each goes into the design system first. Proposals 12 and 14
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
20. **The class `sm`** now means a dense face in the bundle, and two labs (Random, filter) already used it as their own
    size modifier: their pill rows moved from a 26 px to a 28 px pitch (`../arp-face/README.md`). A prefixed name
    (`hd-sm`) would have kept them as they were.

## Known gaps

- **Descriptions:** no `desc` column exists for horde 2's parameters (B582). The info box shows each
  row's spec text, labelled SPEC TEXT. Missing: 317 of 317; two rows (`sub.semitones`, `sub.fine`) have
  no spec text at all.
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
  the bank (the Map handle is pointer-only), the WebAssembly engine, gui2.
