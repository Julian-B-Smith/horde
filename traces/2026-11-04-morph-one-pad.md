# 2026-11-04 — One pad on the Morph page, the corners first, and recording what reaches the engine (GUI-26, GUI-27, GB7, GB16)

**What changed.** The dock leaves the Morph page, so it shows one pad, and the corners card moves to the top, laid out wide. The Pad switch isn't drawn where it can't act. The seam check's `dock` gate and its plants now name Main and Morph, on the human's word, recorded on GB7. The bank's drag no longer adds a second wire for one source and destination. `ENGINE-NEEDS.md` gains GN-34 to GN-39 and corrections to eleven rows and five questions. The GUI charter gains the rule on what reaches the engine. Five ledger rows that were out of date are marked in the shell.

**Why.** The human: "not doubling the morph grids in the morph page"; "the corner snapshots/presets section needs to be up near the top since most users will start there"; "Please err on the side of recording everything that qualifies as an architectural decision."

**Evidence consulted.** GUI-19 ruling 2 and GUI-20's "Not done". A read-only audit of the `GUI-n` rulings, `device.js`, `shell-logic.js`, `gui3.html` and the spec, whose main claims were reread against the code: `saveSoundTo` writes `heard()`, and `dev.drop` had no duplicate check on the bank's path. Its size figures are arithmetic, not measurements, and its hypotheses stay marked as such. Self-check 4b2's control is Filters, where the dock is the one pad.

**Verify.** `./verify fast`, on the commit this trace ships in. The shell opened in a real browser.
