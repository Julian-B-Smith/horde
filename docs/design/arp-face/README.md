# arp-face: vendored inputs of `docs/design/arp-face-lab.html` (B553)

Everything here is a copy. Nothing is hand-edited; re-vendor instead.

| File | What it is | Provenance |
|---|---|---|
| `params.snapshot.json` | Arp's `spec/params.json`, copied verbatim (47 settings, `arp-params/1`; key, label, kind, type, range, default, `display`, `value_labels`; D-035) | Arp commit `de4e2af`, 2026-10-27. JSON cannot carry a comment, so the provenance lives here. |
| `bank.snapshot.json` | Arp's `spec/bank/starter.json`, copied verbatim (`arp-bank/1`). The `pattern` setting's values are "the bank's pattern ids plus `user`" (its `values_from`), so the lab needs the bank to list them. | Arp commit `de4e2af`, 2026-10-27 |
| `bundle.css`, `bundle.js` | The `hd-...` classes and `window.Horde` | horde Design System artifact, version `1793571581-8cd3` (ADR-345: `InfoBox`, the lock and the auto-scroll), re-vendored 2026-11-01 by `tools/design_vendor.py`; before it `1793558157-f6a9` (SourceChip 206 px; ModPage's `routeView`), re-vendored 2026-11-01 by `tools/design_vendor.py`; before it `1793556326-fe52` (ADR-341 A1: the polarity chord is Shift+Option-click), re-vendored 2026-11-01 by `tools/design_vendor.py`; before it `1793517046-cbf4` (master batch 2, ADR-338, ADR-340 and ADR-341: the level cell and its corner mark, the dual readout, SG-b's stepper with the named dropdown, the companions, polarity, source switching, the wires and valves, routes by source; clusters, the lens and fold, the table view, the modulator tiers, the bank, the macro rows, the Mod page, the undo line), re-vendored 2026-11-01 by `tools/design_vendor.py`; before it `1793502898-ea3e` (ADR-338 and ADR-340, the control layer: the tiny tier, re-anchoring drags, MD1, the layered Escape, the Stepper, the fixed readout slot, round stroke joins), re-vendored 2026-10-31 by `tools/design_vendor.py`; before it `1793336699-65f2` (B589, the density revision, with its per-element modifier renamed `.hd-sm`), re-vendored 2026-10-30 by B584 slice 4b; before it `1793334278-f870` (the same revision with a bare `.sm`), and before that `1793080392-2372` (2026-10-27). One header comment line added; the rest is verbatim. |
| `tokens.css` | The CSS custom properties, generated from the artifact's `tokens.json` (the artifact generates its own `tokens.css`, which was not saved) | Same artifact and version. Rules below. |

## How `tokens.css` was generated (to reproduce on re-vendor)

- Names are `--<token name>`. Sources: `color.tokens`, then the `spacing`, `radius`, `shadow`, `size`, `stroke`, `opacity`, `duration` and `screen` token lists, plus `type.families` as `--font-sans` and `--font-mono`.
- A value that is a plain string goes into the bare `:root` only. A `{light, dark}` value puts `light` in the bare `:root` and `dark` in both dark blocks.
- A value that is exactly `{alias}` becomes `var(--alias)`.
- Dark blocks: `@media (prefers-color-scheme: dark)` on `:root:not([data-theme="light"])`, and `:root[data-theme="dark"]`.
- Checked on generation (2026-10-30, `1793334278-f870`; `1793336699-65f2` changed no token, only the header line): all 238 tokens are in `:root` (219 before; the revision added 19, changed none), all 52 themed tokens are in both dark blocks, no alias is left unresolved. The same rules run on the new `tokens.json` reproduce the earlier file line for line, plus only the new tokens' lines. Every `var(--x)` in `bundle.css` and `bundle.js` is defined, except the per-element locals the components set inline (`--cc`, `--own`, `--v`, `--f0`, the channel strip's `--fader-h` and the like).

## Nothing here is Arp's to edit

Arp's tree is read-only for this lab. A new Arp snapshot is a new copy here with a new commit hash above.

## Re-vendored 2026-10-30 (B584 slice 4b): what changed for the labs that load this copy

The revision keeps the classic (unsized) controls unchanged and gives `.hd-dense` (a container) and `.hd-sm` (an element)
the dense faces: 20 px pills, toggles, steppers and readouts at a 28 px pitch. **Fixed upstream:** the first copy of the
revision (`1793334278-f870`) used a bare `.sm`, which the Random and filter labs already use as their own size modifier, so
their pill rows moved from a 26 to a 28 px pitch; `1793336699-65f2` renames it `.hd-sm`, and they render as before again.

Measured with headless Chrome at 1400 × 1000, each lab rendered from the pre-revision tree (`1793080392-2372`) and from this
copy, and rendered twice from this copy as a control for its own motion:

| lab | before vs after (`f870`, bare `.sm`) | before vs after (`65f2`, `.hd-sm`) | after vs after (control) |
|---|---|---|---|
| `arp-face-lab.html` | identical | identical | identical |
| `h2-plan-map.html` | identical | identical | identical |
| `morph-blend-demo.html` | identical | identical | identical |
| `random-lab.html` | 126,925 px | **identical** | identical |
| `filter-lab.html` | 70,097 px | **199 px**, all in its "rev … · fp" stamp, which prints the file's last-modified time (the pre-revision copy is a fresh export) | identical |
| `page-chrome-lab.html` | 307 px | 55 px | 262 to 272 px (its own motion) |
