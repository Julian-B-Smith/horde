# 2026-11-05 — The bank's MORPH tab, workshopped; the pad's X and Y ruled and waiting on the spec (GB25, GUI-45)

**What changed.** GUI-45 records the human's direction and poll answers: a MORPH tab in the bank on every page, flipped to when morph turns on from Main, its lane to be workshopped; and the pad's X and Y as two knobs under the Morph pad. `docs/design/bank-morph-lane-lab.html` asks the lane's open questions (four candidates, the tab with morph off, where the bank goes when morph turns off), on the master's ModBank, CornerChips and XYPad. GN-51 asks for D.2 rows for the position. GB25 opens; the ledger has two ruled rows and the lab.

**Why.** The human: "What if we add a third tab here for "morph" ... And it occurs to me that the XY coordinates for Morph should be parameters visible below the grid on the Morph page."

**Not done, and why.** The X and Y knobs were built, then backed out: `gen_gui3_controls --check` went RED ("gui3.html must call H.Knob and H.Slider once each, inside mountControl"), because the shell draws a parameter knob only from a row and D.2 gives the position none. The gate stands; the knobs wait on GN-51. The master's third segment waits on the lane's ruling (ADR-316).

**Evidence.** The lab's self-check in the browser: 10 OK, 0 FAILED (the flip from Main, no flip from Osc 1, back to MAC on morph off, four rows in each lane, lane B's weights at 100 % and 0 %, the hidden and dimmed tab). A real click on its Morph switch from Main flipped the bank to MORPH with lane B's weights showing.

**Verify.** `./verify fast` on the commit this trace ships in.
