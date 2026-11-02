# Preset bar

One line for the global preset (GUI-6, P1): previous, the name with its list, next, Save. It goes in the top bar's top row, between the page groups and the switches, on every page.

`Horde.PresetBar(el, S, { bank, value, onPick, onSave, note? })` returns `{ set(name), get(), close() }`.
- `bank`: `{ category: [names] }`, in the order to show. `value`: the name showing.
- `onPick(name, category)` and `onSave()` are the host's. The bar names what was picked and fakes no load.
- `note`: a line at the head of the list, for a host whose loading isn't wired yet.

**Anatomy** Two 22 px step buttons, the name (bold, ellipsis, flexes), Save. The name opens a list under the bar: categories on the left, names on the right, with the current one bold. A click picks and closes. Escape goes through the layered cancel (`S.escapeLayers`), and a click elsewhere closes it.

**A global preset holds its four corners too** (ADR-192 r1), so loading one replaces the patch. A corner's own save stays on the Morph page (ADR-321 r1).

**FX device (ADR-254):** the same bar, listing its own presets.

**Sources** GUI-6, `docs/design/lens-preset-lab.html` P1.

The preview's names are SAMPLE.
