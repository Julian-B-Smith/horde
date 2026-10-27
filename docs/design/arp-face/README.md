# arp-face: vendored inputs of `docs/design/arp-face-lab.html` (B553)

Everything here is a copy. Nothing is hand-edited; re-vendor instead.

| File | What it is | Provenance |
|---|---|---|
| `params.snapshot.json` | Arp's `spec/params.json`, copied verbatim (47 settings, `arp-params/1`; key, label, kind, type, range, default, `display`, `value_labels`; D-035) | Arp commit `de4e2af`, 2026-10-27. JSON cannot carry a comment, so the provenance lives here. |
| `bank.snapshot.json` | Arp's `spec/bank/starter.json`, copied verbatim (`arp-bank/1`). The `pattern` setting's values are "the bank's pattern ids plus `user`" (its `values_from`), so the lab needs the bank to list them. | Arp commit `de4e2af`, 2026-10-27 |
| `bundle.css`, `bundle.js` | The `hd-...` classes and `window.Horde` | horde Design System artifact, version `1793080392-2372`, 2026-10-27. One header comment line added; the rest is verbatim. |
| `tokens.css` | The CSS custom properties, generated from the artifact's `tokens.json` (the artifact generates its own `tokens.css`, which was not saved) | Same artifact and version. Rules below. |

## How `tokens.css` was generated (to reproduce on re-vendor)

- Names are `--<token name>`. Sources: `color.tokens`, then the `spacing`, `radius`, `shadow`, `size`, `stroke`, `opacity`, `duration` and `screen` token lists, plus `type.families` as `--font-sans` and `--font-mono`.
- A value that is a plain string goes into the bare `:root` only. A `{light, dark}` value puts `light` in the bare `:root` and `dark` in both dark blocks.
- A value that is exactly `{alias}` becomes `var(--alias)`.
- Dark blocks: `@media (prefers-color-scheme: dark)` on `:root:not([data-theme="light"])`, and `:root[data-theme="dark"]`.
- Checked on generation: all 219 tokens are in `:root`, all 52 themed tokens are in both dark blocks, no alias is left unresolved. Every `var(--x)` in `bundle.css` and `bundle.js` is defined, except the per-element locals the components set inline (`--cc`, `--own`, `--v`, `--f0` and the like).

## Nothing here is Arp's to edit

Arp's tree is read-only for this lab. A new Arp snapshot is a new copy here with a new commit hash above.
