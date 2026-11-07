# 2026-11-06 — The morph questions lab (GB29)

**What changed.** `docs/design/morph-questions-lab.html`: eight cards, one per open morph question, each option on its own small pad with a button that shows it behave. The recommendation is outlined, each card says what you'd hear and what the engine carries, and a poll table sits at the foot. The ledger lists the lab as exploring, GB29 opens, and the index is regenerated.

**Why.** The lead (ADR-370, B631): "Please build a lab showing the morph's open questions, so the human can answer them by poll", from the human's "yes to all, build the morph lab and start everything". The morph goes to the builder after its CPU measurement.

**Choices made.**
- GN-27 is asked as its own row reads: whether mappings are per corner. Which corner an edit goes to is already settled (the pinned corner, else the owner), and the card says so; the lead had worded it as edits.
- Added, as real choices: GN-25 (RAMPS), GN-52 (where corner presets live) and GN-35 (what Save current sound stores).
- Left out: GN-8, GN-18, GN-26 and GN-14, for the reasons in the lab's header.

**Evidence.** The lab's self-check in the browser: 6 OK, 0 FAILED. It checks the cap refusing the 9th pin, the dormant pins against cleared ones (control), the shape-only lock against shape and values, per-corner against global depth at D (control), and the two save rules giving different corners. A real click on Quantum: the cleared option shows 0 pins, and the dormant one 4, dashed.

**Verify.** `./verify fast` on the commit this trace ships in.
