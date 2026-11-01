# Fold (P2)

Per section, **Face | All | +Adv**, remembered. ADR-338 ruling 5: "P2's fold only where a page still overflows"; P1 (the Lens page) is the default.

`Horde.Fold(el, S, { id, name, path, tiers: { face, all, adv }, tier, open, summary, keys })`
- A tier is an item list (see Cluster) or a builder `function (host, S)`; it is built the first time it is shown. A tier left out has no segment (a one-tier fold shows no pills).
- Tiers are cumulative: Face is the headline controls (the preview uses the compact rung), All adds the rest (utility rung), +Adv adds the advanced rows (the tiny tier, as a grid).
- `id` keys the memory: the open state and the tier are kept per fold in `localStorage` (`hd.fold.<id>`), wrapped in try/catch, so it works without storage (the session's memory holds it).

**Anatomy** a 3 px `stroke-id` ink rule over a 28 px header: the chevron (28 px hit, `aria-expanded`), the section name (`cluster`), the tier pills (the house `.hd-pill.hd-sm`, a radiogroup), the readout slot at the right.

**Keyboard** the chevron and the pills take focus only in keyboard mode.

**Returned** `{ tier(t?), open(b?), el }`.

**Sources** ADR-338 ruling 5, composition-lab.html P2.
