# 2026-11-06 — The FX screen marks lab's picks (GUI-58, GB15)

**What changed.**
- **`DECISIONS.md`: GUI-58.**
  - It records a corner mark and a modulation mark for each of the eleven screens.
  - The human changed three corner marks to glyphs: on the workstation, the vector monitor and the character LCD.
  - It states the costs those choices carry, the LCD's full custom-character budget among them.
  - It leaves three questions open.
- **The ledger.**
  - `fx-screen-marks-lab.html` is ruled (GUI-58).
  - One row per screen.
  - `engine_impact` GUI-58 → GN-18.
- **`ENGINE-NEEDS.md`.** GN-18 cites GUI-58, since every corner mark names the owner the engine publishes.
- **GB15** is RULED, and the master build waits on the open questions.
- **The design index** is regenerated.

**Why.** The human picked on every screen and said "Done picking". The Labs facilitator session relayed the picks as read from the page (`window.FX_MARKS.PICKS`).

**Evidence.**
- The facilitator's message gives the three changes and the 19 kept suggestions.
- The costs are quoted from the lab's cards (`docs/design/fx-screen-marks-lab.html`, the `cost` strings for `bits`, `outline`, `pips` on the LCD and `tip`).

**Not done here.**
- The lab keeps its review pin while the three questions are open.
- Nothing goes into the master yet.
- Briefs to Shriek and Sluice are the lead's.

**A second commit: GUI-58 A1.** The human answered the three open questions, through the Labs facilitator.
- The answers: "Each screen its own", "Show weight where it fits", and "Yes, try a round" for bezel lamps.
- GB33 is opened for the bezel-lamp round.
- The lab's review pin is off, and the index is regenerated.
- GB15 and the ledger now point to the master as the next step.

**Verify.** `./verify fast` on the commit this trace ships in.
