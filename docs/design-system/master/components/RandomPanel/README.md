# Randomize panel

The Random page's panel (GUI-38, a MOCK-UP the human asked to land), from `random-lab-report.md` section 5 and ADR-318, ADR-320, ADR-321, ADR-322 r1 and ADR-327.

**Anatomy.** On the left: a profile, Chaos and Amount (they bound the next draw and never change the sound), which corners a draw writes (the selected corner by default, ADR-320), Randomize patch, Randomize parameters, Undo the draw, the seed and draw count, the default exemptions, and Copy (a level, its architecture or parameters or both, from one corner to another). On the right: a row per level, with ARCH, PARAMS, a Lock and, on the four sources, their own solo and mute (monitoring, never drawn or stored).

**The law: `Horde.RandomPanel.draw(rows, kind, chaos, amount, seed, click, level)`, pure.** Rows are `{ key, n, home }`. A stepped row moves to another step, so ARCH changes something; a continuous row draws a bell around its home with σ from Chaos. Amount is the chance a row is drawn. Seed and click make a draw repeatable.

**The consumer provides** `levels` (`{ id, name, arch, params, custom?, mock?, monitor? }`), `profiles` (`{ name, chaos, amount, locks }`), `selected()`, `exemptNames`, `draw(levels, kind, params)`, `copy(level, kind, from, to)`, `undo()`, `say(text)`, `keys()`. `Horde.RandomPanel(el, o)`.

**MOCK, and what isn't built.** A profile only sets Chaos, Amount and locks: the role set is the human's to author and audition (ADR-318). Mod rules aren't drawn, level matching and the dependency tree aren't built, and pitch's minimal ruleset is only the exemptions so far. Filters and LFOs wait on their rows. See GN-21.

**FX device (ADR-254).** The same panel with FX roles and no sources' rows (ADR-318's consequence). ADR-321's three additions don't apply there.
