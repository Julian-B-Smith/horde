# adr341-composition-r3-rulings — the composition lab's round 3 ruled by poll

- **Queue items:** B589, B594 (B70's gesture). GUI session; rulings are the human's (ADR-336).
- **Why:** round 3 (`docs/design/composition-lab-r3.html`, #1313) drew ADR-340's amendments; the human answered its polls on 2026-10-31 and 2026-11-01. A ruling enters the log before the master takes it (ADR-316).
- **Evidence consulted:** the round-3 page and its self-check (PASS 4 of 4: 25 of 25 variants mounted, 0 boxes moved on hover or drag, every stroke round), read in the browser before each poll; its measured contrasts (light panel: fills A, C, D 1.3-1.8 : 1, `-tx` inks 4.9-5.0 : 1). The human asked for two new T4c drawings (a hollow triangle, a hollow circle); they were added (T4c-e, T4c-f) and polled again.
- **The answers, verbatim where they changed a recommendation:**
  - T4c: "T4c-f circle, -tx ink, but I think I would like to see the circle smaller, like the one on the smallest knob".
  - Readout: DR-c, not the recommended DR-a.
  - Steppers: "I like SG-b, but I think non-numeric steppers should actually get a dropdown when you option+click them, because it doesn't make sense to be able to type in your own text".
  - Polarity chord: MP-c (⌘⌥-click), not the recommended MP-b; the toggle cycles, not the recommended UNI ↔ INV flip.
  - Wires W-a, macro rows BR-b, the bend row springs back (all as recommended).
- **What changed:** `DECISIONS.md` ADR-341; `docs/design/design-ledger.json` seven `adr341-*` rows (ruled), round 3's lab status ruled (ADR-341), a note on `adr340-t4c`; `docs/design/index.html` regenerated; `ROADMAP.md` notes on B589 and B594.
- **Open, recorded rather than guessed:** whether DR-c's "where the hand is" is the editing corner's value (assumed, a placeholder in the master) or the blend.
- **Verify:** see the PR. `design_chain_check` GREEN (coverage 338 of 338 ADRs, 28 plants).
