# GUI 3: the wired shell

*Last verified 2026-10-29, branch `gui3-b584-shell` (ROADMAP B584, first slice; ADR-316).*

`docs/design/gui3.html` is GUI 3's prototype: one window with twelve pages, every engine parameter
reachable, and no audio. It is built from the design system's vendored bundle (`../arp-face/`), its
controls are generated from SPEC-ENGINE section 5, and it talks to a stub engine through one adapter.
The WebAssembly engine (SPEC-ENGINE IF-10, clean-room stage 8) replaces the stub behind the same seam.

## Map

| file | what it is | edited by |
|---|---|---|
| `../gui3.html` | The shell: pages, chrome (docked pad, info box, right-click menu), the seam panel, self-checks. Builds every parameter control in one function from `shell-data.js`, and names no parameter itself. | hand |
| `layout.json` | Which key goes on which page and in which group, widget overrides, PA-8's live-when for the designed pages. | hand (the page designer) |
| `params.json` | SPEC-ENGINE section 5 as data: 330 rows (317 host parameters, 13 reserved), with label, unit, law, range, class, options, default and its normalised v, and the row's spec text. | generated |
| `shell-data.js` | The same rows plus the pages' control lists, as one script the shell loads from `file://`. | generated |
| `engine-stub.js` | The no-audio engine: `GUI3Engine.METHODS` and `GUI3Engine.Stub`. | hand |
| `device.js` | The adapter the shell talks to: morph, routes, history. `GUI3Device.create(data, engine, {seed})`. | hand |

Regenerate after editing the spec or the layout: `python3 tools/gen_gui3_controls.py`.
Open the shell from `docs/design/` (or `tools/labs_preview.sh`); `gui3.html#noise,morph` opens a page
with morph on and the corners scattered.

## The gates (`./verify fast`)

- `python3 tools/gen_gui3_controls.py --check`: the expansion equals 5.8's counts; every layout key is
  a parameter; every host parameter is on a page; no reserved row is placed; widgets suit their rows;
  live-when conditions name real options; the shell mounts controls in one place and names no key; no
  gui3 script reads a clock; the outputs are current. Ten must-fail controls.
- `node tools/labharness/gui3_seam_check.mjs`: eleven tests of the stub and the adapter against the
  spec's words, eight planted faults. `--verbose` prints why each plant went red.
- `lab_load_check` loads the page's inline script (it stops at the missing bundle, as the other bundle
  labs do). The page's own seven self-checks run in a browser, each with a control.

## The seam

The shell calls only `device.js`. `device.js` calls only these engine methods (`GUI3Engine.METHODS`):

| method | spec | what the stub does |
|---|---|---|
| `manifest()` | 5, IF-11 | the rows |
| `setParam(key, v)` / `getParam(key)` | IF-11, 5.1, PA-7 | v in [0, 1], a stepped row snapped to its grid; an unknown key, a reserved row and a non-number are refused and counted |
| `value(key)`, `display(key, v?)` | 5.1, IF-11 | the law's x, and its text with the unit |
| `snapshot()` / `restore(s)` | RC-11.2, RC-11.4 | every v and the two `ratios` texts |
| `saveState()` / `loadState(text)` | IF-4, RC-9.3 | IF-4's members as JSON (shape only; the real text format is the harness header's); unknown keys kept; version 0 refused |
| `gesture(key, phase)`, `events()` | RC-11.3 | begin and end reported back unchanged |
| `observe()` | IF-10, IF-4, IF-9 | no voices, zero counters, the heard value of every stepped key |

What is not the engine's stays in `device.js`, as the spec places it: the morph's corners and position
(RC-11.1), the routes (RC-4 defers the matrix), and the undo history (RC-11).

**What the WebAssembly engine owes the seam.** An object with the same methods, wrapping the
AudioWorklet's port. The shell's calls are synchronous; the worklet is not. The wrapper therefore keeps
the last value it sent per key and answers `getParam` from it, forwards `setParam` and `gesture` as
messages, answers `snapshot`, `saveState` and `observe` from the latest frame the worklet posted, and
`restore` and `loadState` by message. Whether `observe()` maps to IF-10's exact view or RC-8's lossy
GUI snapshot is an open question (below). Nothing above `device.js` changes.

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
8. **A disabled toggle**: `hd-toggle` has no disabled look, so the Pad switch, disabled while morph is
   off, still shows its saved preference as if it were live.

## Known gaps

- **Descriptions:** no `desc` column exists for horde 2's parameters (B582). The info box shows each
  row's spec text, labelled SPEC TEXT. Missing: 317 of 317; two rows (`sub.semitones`, `sub.fine`) have
  no spec text at all.
- **Manual anchors:** "Open in manual" is a stub until B451's pages exist (B583).
- **Not in this slice:** the other pages' designs (B581's order), the mod map, the page bank (B556), the
  per-parameter morph mode (B557), the WebAssembly engine, gui2.
