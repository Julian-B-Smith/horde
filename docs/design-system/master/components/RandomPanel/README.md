# Randomize panel

The Random page's panel (GUI-38, a MOCK-UP the human asked to land), from `random-lab-report.md` section 5 and ADR-318, ADR-320, ADR-321, ADR-322 r1 and ADR-327.

**Anatomy.** On the left: a profile, Chaos and Amount (they bound the next draw and never change the sound), which corners a draw writes (the selected corner by default, ADR-320), Randomize patch, Randomize parameters, Undo the draw, the seed and draw count, the default exemptions, and Copy (a level, its architecture or parameters or both, from one corner to another). On the right: a row per level, with ARCH, PARAMS, a Lock and, on the four sources, their own solo and mute (monitoring, never drawn or stored).

**The law: `Horde.RandomPanel.draw(rows, kind, chaos, amount, seed, click, level)`, pure.** Rows are `{ key, n, home }`. A stepped row moves to another step, so ARCH changes something; a continuous row draws a bell around its home with σ from Chaos. Amount is the chance a row is drawn. Seed and click make a draw repeatable.

**What a random patch may be (GUI-55).**
- **Row hints.** A row may carry four hints:
  - `p` draws a two-way switch fresh, on with that chance (a blade's switch).
  - `lean` pulls a row toward its low end, or a stepped row toward its first options, harder at low Chaos.
  - `centre` replaces the home a continuous row is drawn around (ENV 4's tight decay).
  - `favour` is a stepped row's option index that a draw lands on with `prior.envOne`'s chance, the other options sharing the rest evenly (A2).
- **`Horde.RandomPanel.sources(cur, chaos, amount, seed, click)`.** It takes and returns `{main, second, sub, noise}` as 0 or 1.
  - Main, which is osc 1, stays on, except in the occasional patch where the sub plays alone.
  - The other three flip by chance.
  - So the second oscillator never plays alone.
- **The patch flag.** The whole-patch buttons pass `patch: true` to `draw`, and the consumer draws the sources only then. A level press never flips its own source's switch.
- **A source's envelope selector (A1).** Below `prior.envFrom`, the end of "moderate" chaos, it never moves. Above it, the consumer draws it with `prior.env(c)` as its chance, which rises to a cap.
- **Where a moved selector lands (A2).** The human: "fully randomized patches should heavily bias toward attaching every osc volume to Osc 1." The consumer passes osc 1's envelope as `favour` (ENV 1 for osc 1's own selector), so a moved selector lands there with `prior.envOne`'s chance, a stand-in of 0.8.
- **The curves are stand-ins.** They live in one object, `Horde.RandomPanel.prior`, and each is 0 at Chaos 0. Their shapes are the tuning session's (GUI-55 r6, ADR-342).

**The consumer provides** `levels` (`{ id, name, arch, params, custom?, mock?, monitor? }`), `profiles` (`{ name, chaos, amount, locks }`), `selected()`, `exemptNames`, `draw(levels, kind, params)`, `copy(level, kind, from, to)`, `undo()`, `say(text)`, `keys()`. `Horde.RandomPanel(el, o)`.

**MOCK, and what isn't built.** A profile only sets Chaos, Amount and locks: the role set is the human's to author and audition (ADR-318). Mod rules aren't drawn, level matching and the dependency tree aren't built, and pitch's minimal ruleset is only the exemptions so far. Filters and LFOs wait on their rows. See GN-21.

**FX device (ADR-254).** The same panel with FX roles and no sources' rows (ADR-318's consequence). ADR-321's three additions don't apply there.
