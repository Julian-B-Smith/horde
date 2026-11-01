# Modulator tiers

The four tiers of ADR-338 ruling 6, for every kind of modulator (envelope, LFO, S&H, ORBITAL, macro, MIDI, engine source).

`Horde.ModTiers.thin | compact | chip | full (el, S, m, o)`
- `m`: `{ id, name, kind (env | lfo | macro | engine: the colour), kindLabel, group, fixed, pending, short, f(t), fmt, picture ('trace' | 'arc'), bipolar, paint(K, t, opt), ctls, note, sample }`. `id` matches a Source in the Session (its `f(t)` draws the picture unless `m.f` or `m.paint` is given). `Horde.ModPicture.kit` and `.col` are the drawing kit: the screen roles, round joins, a palette cached per theme. `ctls` are the compact card's few controls (items, see Cluster).
- `o`: `onMap(m)`, `onDrop(m, key)`, `onRemove(m, routes)`, `onSelect(m)`, `routes(m)`, `pageKeys` (the page's keys, for "n here"), `selected()`, `keys`. `onRemove` absent: no x.

| Tier | Size | Where | Parts |
|---|---|---|---|
| **Full** | the Mod page's detail | M5 | header (kind, name, scope tag, retrigger when given, Map, x), a 56 px picture, the bundle's own editor (`o.editor(m, host)`), the controls, the routes it drives |
| **Compact** | 188 px (`size-card-compact-w`) | the Mod page's grid, the bank's expanded state | the header (swatch, name, kind tag, value, Map, x), a 32 px picture, the controls, the route count |
| **Thin row** | 226 x 26 px (`size-row-thin`) | the bank | swatch, name over its route count, a 56 x 18 picture, the value, Map, x |
| **Micro chip** | 20 px (`size-micro-chip`) | later, in section headers: who modulates this section | swatch, name, a level bar; the chip is the Map handle |

**The compact card is the thin row expanded** (ADR-338 ruling 7, amending ADR-323 item 1's form): the same four parts at the head (swatch and name, value, Map, x), the picture grows under it and the controls open. It is no longer the bank's resting form.

**Map** (`.hd-mapb`, an 18 px round button with a 28 px hit): press it, then click a control (MD1 maps with a click while MAP is on; with no `onMap` the button sets the MAP source and turns MAP on, ADR-323 ruling 3); or drag it onto a control whose host carries `data-key` (the Serum-style drop, ADR-322 ruling 3). Enter and Space press it. The source colour fills it while it is the MAP source.

**x** asks the host to remove the modulator and its routes (M-R2, see Undo line); its title says how many routes go with it. A pending modulator (unmapped, dropped when the interface closes, ADR-322) has a dashed edge and says "pending".

**Pictures** a trace is the source sampled over its last 2.4 s (a pure function of time, so no history is kept) with a dot at now; an arc for a macro; or the consumer's own drawing. Every canvas stroke joins round.

**Focus** the row's name is a button; the name, Map and x are tab stops only in keyboard mode.

**Placeholders** the pictures are generic. Each kind's own drawing (the envelope from its rows, the orbital bodies, a phasor for R and psi) is the consumer's `paint`; the lab's MOCK drawings are not ported. LFOs, S&H, ORBITAL and macros have no manifest rows yet (RC-4 deferred), so their controls in the preview are samples.

**Sources** ADR-338 rulings 6 and 7, ADR-323 ruling 1, composition-lab.html M.
