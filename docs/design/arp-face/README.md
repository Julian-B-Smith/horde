# arp-face: vendored inputs of `docs/design/arp-face-lab.html` (B553)

Everything here is a copy. Nothing is hand-edited; re-vendor instead.

| File | What it is | Provenance |
|---|---|---|
| `params.snapshot.json` | Arp's `spec/params.json`, copied verbatim (47 settings, `arp-params/1`; key, label, kind, type, range, default, `display`, `value_labels`; D-035) | Arp commit `de4e2af`, 2026-10-27. JSON cannot carry a comment, so the provenance lives here. |
| `bank.snapshot.json` | Arp's `spec/bank/starter.json`, copied verbatim (`arp-bank/1`). The `pattern` setting's values are "the bank's pattern ids plus `user`" (its `values_from`), so the lab needs the bank to list them. | Arp commit `de4e2af`, 2026-10-27 |
| `bundle.css`, `bundle.js` | The `hd-...` classes and `window.Horde` | horde Design System artifact, version `1793334278-f870` (B589, the density revision), re-vendored 2026-10-30 by B584 slice 4b; before it, `1793080392-2372` (2026-10-27). One header comment line added; the rest is verbatim. |
| `tokens.css` | The CSS custom properties, generated from the artifact's `tokens.json` (the artifact generates its own `tokens.css`, which was not saved) | Same artifact and version. Rules below. |

## How `tokens.css` was generated (to reproduce on re-vendor)

- Names are `--<token name>`. Sources: `color.tokens`, then the `spacing`, `radius`, `shadow`, `size`, `stroke`, `opacity`, `duration` and `screen` token lists, plus `type.families` as `--font-sans` and `--font-mono`.
- A value that is a plain string goes into the bare `:root` only. A `{light, dark}` value puts `light` in the bare `:root` and `dark` in both dark blocks.
- A value that is exactly `{alias}` becomes `var(--alias)`.
- Dark blocks: `@media (prefers-color-scheme: dark)` on `:root:not([data-theme="light"])`, and `:root[data-theme="dark"]`.
- Checked on generation (2026-10-30, `1793334278-f870`): all 238 tokens are in `:root` (219 before; the revision added 19, changed none), all 52 themed tokens are in both dark blocks, no alias is left unresolved. The same rules run on the new `tokens.json` reproduce the earlier file line for line, plus only the new tokens' lines. Every `var(--x)` in `bundle.css` and `bundle.js` is defined, except the per-element locals the components set inline (`--cc`, `--own`, `--v`, `--f0`, the channel strip's `--fader-h` and the like).

## Nothing here is Arp's to edit

Arp's tree is read-only for this lab. A new Arp snapshot is a new copy here with a new commit hash above.

## Re-vendored 2026-10-30 (B584 slice 4b): what changed for the labs that load this copy

The revision keeps the classic (unsized) controls unchanged, and it gives the classes `.hd-dense` and `.sm` a new meaning: a
dense face (20 px pills, toggles, steppers and readouts at a 28 px pitch). Measured with headless Chrome, before and after,
each lab rendered twice after as a control:

| lab | before vs after | after vs after (control) |
|---|---|---|
| `arp-face-lab.html` | identical | identical |
| `h2-plan-map.html`, `morph-blend-demo.html` | identical | identical |
| `page-chrome-lab.html` | 307 px differ | 262 px differ (its own motion: unchanged) |
| `random-lab.html` | 126,925 px differ | identical |
| `filter-lab.html` | 70,097 px differ | identical |

The Random and filter labs use `sm` as their own size modifier (`.hd-pill.sm`, `.hd-select.sm`), which the revision now
also styles: their pill rows sit at the 28 px pitch instead of 26. Their load checks stay green. This is the revision's
class name colliding with the labs' own, not a change to either lab; the fix is the design system's or the labs' (a
question for the lead), and neither lab is edited here.
