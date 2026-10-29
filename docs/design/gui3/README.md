# GUI 3: the wired shell

*Last verified 2026-10-29, branch `gui3-b584-slice2` (ROADMAP B584, second slice; ADR-316, ADR-319).*

`docs/design/gui3.html` is GUI 3's prototype: one window with twelve pages, every engine parameter
reachable, and no audio. It is built from the design system's vendored bundle (`../arp-face/`), its
controls are generated from SPEC-ENGINE section 5, and it talks to a stub engine through one adapter and
a swappable transport. The WebAssembly engine (SPEC-ENGINE IF-10, clean-room stage 8) replaces the stub
behind the same seam.

## Map

| file | what it is | edited by |
|---|---|---|
| `../gui3.html` | The shell: pages, chrome (docked pad, info box, right-click menu), the Morph page's panel, the seam panel, self-checks. Builds every parameter control in one function from `shell-data.js`, and names no parameter itself. | hand |
| `layout.json` | Which key goes on which page and in which group, widget overrides with their reasons, PA-8's live-when for the designed pages, a page's shell-built panel. | hand (the page designer) |
| `params.json` | SPEC-ENGINE section 5 as data: 330 rows (317 host parameters, 13 reserved), with label, unit, law, range, class, options, default and its normalised v, and the row's spec text. | generated |
| `shell-data.js` | The same rows plus the pages' control lists, as one script the shell loads from `file://`. | generated |
| `engine-stub.js` | The no-audio engine (`GUI3Engine.METHODS`, `GUI3Engine.Stub`) and the synchronous transport (`GUI3Engine.TRANSPORT`, `GUI3Engine.SyncTransport`). | hand |
| `device.js` | The adapter the shell talks to: morph, corners and their presets, routes, history, mirror and confirm. `GUI3Device.create(data, transport, {seed})`. | hand |

Regenerate after editing the spec or the layout: `python3 tools/gen_gui3_controls.py`.
Open the shell from `docs/design/` (or `tools/labs_preview.sh`); `gui3.html#noise,morph` opens a page
with morph on and the corners scattered.

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
  Twelve must-fail controls.
- `node tools/labharness/gui3_seam_check.mjs`: fifteen tests of the stub, the transport and the adapter
  against the spec and ADR-319 (morph off, the morph toggle in history, corner snapshots, mirror and
  confirm over a synchronous and a delayed transport), thirteen planted faults. `--verbose` prints why
  each plant went red.
- `lab_load_check` loads the page's inline script (it stops at the missing bundle, as the other bundle
  labs do). The page's own nine self-checks run in a browser, each with a control.

## Proposals for the design system (nothing here is drawn locally)

The bundle (version `1793080392-2372`) lacks these; each goes into the design system first:

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

## Known gaps

- **Descriptions:** no `desc` column exists for horde 2's parameters (B582). The info box shows each
  row's spec text, labelled SPEC TEXT. Missing: 317 of 317; two rows (`sub.semitones`, `sub.fine`) have
  no spec text at all.
- **Manual anchors:** "Open in manual" is a stub until B451's pages exist (B583).
- **Not in this slice:** the other pages' designs (B581's order), the rest of the Morph page (B581 step
  3), moving the parameter table beside `engine2/structure` (B586), PA-8 as a table in the spec (B587),
  the mod map, the page bank (B556), the per-parameter morph mode (B557), the WebAssembly engine, gui2.
