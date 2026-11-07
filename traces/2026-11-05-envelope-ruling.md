# 2026-11-05 — GUI-47: the envelope workshop ruled; the shell, Shriek, Sluice and Random labs off the review queue (GB23, GB13, GB14)

**What changed.**
- **GUI-47** records the human's three answers on the envelope workshop: "Override + grid", "four small, one large, and adding the map button to each of the small envelopes", and "Picture and readout". Engine impact: none.
- **Main's fit is open.** Main's stage fills its page area exactly (463 px of 463, measured in headless Chrome), so the strip's 40 px is a trade. Three ways go back to the human: shrink the deck, use smaller pictures, or replace ENV 1's knob row (recommended).
- **Review pins removed:** gui3.html (the shell, not a lab: "Yes, drop both"), envelope-workshop (ruled), shriek-gui and random (deferred: "Defer both", and random's next round isn't ready), and sluice-gui, until it carries Sluice's answer.
- **GB13** corrected: the brief was answered by Sluice's seq 85, and the ball is ours. **GB14** corrected: seq 41 and 43 are with Shriek. **GB23** is ruled, waiting on the fit.
- The ledger: envelope-workshop ruled, four GUI-47 rows, and the deferral notes. The index is regenerated: 7 pins.

**Why.** The human's answers through the Labs facilitator (batch 4), and the lead's note that GB13 and GB14's "needed from the lead" lines were stale.

**Evidence.** Sluice's `integrations/hypersaw/response-gui-lab.md` on its origin/main: seq 85, ball HYPERSAW (read, nothing quoted). `design_chain_check` and `engine_needs_check` pass (47 GUI rulings). The measurement: `#pages` client height 463 = scroll height 463, ENV 1's picture 72 px at the top of the right column, the deck's ring 140 px.

**Verify.** `./verify fast` on the commit this trace ships in.
