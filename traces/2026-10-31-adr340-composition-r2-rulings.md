# adr340-composition-r2-rulings — round 2 of the composition lab ruled, with amendments; ADR-226's no-panic holds

- **Queue items:** B589, B594 (GUI session, ADR-336); B70 cited, not edited (lead-owned).
- **Why:** variant sheets end in a poll, and the answer becomes an ADR (the GUI charter). Round 2 (#1309) answered the human's review of ADR-338. The batch-1 report (#1308) found that ADR-338 r11's panic clause contradicted ADR-226 item 6.
- **Evidence:** the human's poll answers of 2026-10-30 and 2026-10-31, with their notes quoted in the ADR. The variant cards' text in `docs/design/composition-lab-r2.html`. B70's row, the human's depth-modulation request of 2026-08-28, found by searching ROADMAP before filing.
- **What changed:**
  - `DECISIONS.md`: ADR-340, twelve rulings.
  - `docs/design/design-ledger.json`:
    - eleven `adr340-*` rows (ruled), each pointing at a port-tab row;
    - round 2's lab status set to ruled (ADR-340);
    - notes on `adr338-escape-layered`, `adr226-no-panic-control`, `adr338-tiny-knob` and `adr338-tiny-sliders`.
  - `docs/design/index.html`: regenerated.
  - `ROADMAP.md`: notes on B589 and B594, the GUI's rows; the lead's needs (B70's depth routes, the stub's macros) are named there.
- **Rejected:** taking the amended items straight to the master. T4c's corner mark, the dual readout, the stepper gestures, MP1's band drag, the polarity menu and modifier, and ML1's depth-route wires have not been drawn yet, so round 3 draws them first.
