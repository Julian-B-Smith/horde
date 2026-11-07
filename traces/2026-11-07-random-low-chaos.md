# 2026-11-07 — What a random patch may be at low chaos (GUI-55 and A1, GB32)

**What changed.**
- **`DECISIONS.md`: GUI-55 and A1**, the human's six Random rules relayed by the lead.
  - At low chaos the sources, the off sources and the blades are left alone.
  - Osc 1 stays on, or the sub plays alone now and then, so osc 2 never plays alone.
  - Blades are rare and simple.
  - ENV 4's decay is tight.
  - A1: a source's envelope selector never moves at low or moderate chaos, and only becomes possible, bounded, at the top.
- **The master, published as `1794029674-ea43`.**
  - `RandomPanel`'s `drawRows` takes row hints: `p`, `lean` and `centre`.
  - `RandomPanel.sources` draws the four source switches by the rule above.
  - `RandomPanel.prior` holds every stand-in curve in one place.
  - The whole-patch buttons pass `patch: true`.
  - The component's README says all this.
- **The vendored bundle** (`docs/design/arp-face/`) is re-vendored at that version. Its provenance row is updated, and the ledger's `master_versions` gains the version.
- **GUI 3** (`docs/design/gui3.html`).
  - `randomPlan` plans each corner's writes before writing: source switches on a patch draw only, then blades, then each source's envelope selector by A1's curve, then the rest.
  - The draw says when a source stays off.
  - Self-check 4c8b runs over seeded draws at Chaos .1 and .85. GUI-38's unconstrained rule is its must-fail control.
- **The ledger.** Row `gui55-random-low-chaos` is in-shell at the new master version. `engine_impact` GUI-55 → GN-21, and GN-21 cites GUI-55.
- **GB32** is built.

**Why.** The human: "randomizing almost always makes a limp noisy twang at low volume". The cause found was that source envelope selectors drew to ENV 4, and every switch flipped at Amount 1.

**Evidence.**
- With the new master bundle in GUI 3, before publishing, `gui3_browser_check` passed 88 self-checks, 4c8b among them.
- Before publishing, all 194 files of the artifact's newer version (`1794029597-e186`, a change to the type's own files) were compared with `VERSION`: none differed, so the publish carried only this PR's two files.

**Not done here.**
- "Often maps it to pitch" waits on route draws.
- The stand-in curves' shapes are for the tuning session (rule 6, ADR-342).
- An open question is in GUI-55 A1: does a tight decay mean zero sustain?

**Verify.** `./verify fast` on the commit this trace ships in.
