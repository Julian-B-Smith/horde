# 2026-11-06 — The oscillator layout lab's four answers (GUI-57, GB19)

**What changed.**
- **`DECISIONS.md`: GUI-57.** Its rulings:
  - Main's tab is M3, with the summed waveform under the blade view.
  - The Osc page keeps P1, its tiers as built.
  - The dashboard is D3, the signal flow.
  - A dashboard card loses its envelope for a small control and gains a row.
  - Two readings are open to the human: the one-control reading of "selector or override", and measuring the height before shrinking anything.
- **The ledger.**
  - `osc-layout-lab.html` is ruled (GUI-57).
  - Four rows, one per ruling. P1 is in the shell as it stands.
  - `engine_impact` GUI-57 → GN-16.
- **`ENGINE-NEEDS.md`.** GN-16 cites GUI-57.
- **GB19** is RULED.
- **The design index** is regenerated.

**Why.** The human answered all four questions, reviewing the lab live with the Labs facilitator, which relayed the human's words. Ruling 1's waveform is the one the human asked for on the Osc page, recorded as GUI-49 in #1421: the SCALPEL lab's output trace, summed from the settings.

**Not done.** Building rulings 1, 3 and 4 goes through the master, then the shell, each in a PR with screenshots.

**Verify.** `./verify fast` on the commit this trace ships in.
