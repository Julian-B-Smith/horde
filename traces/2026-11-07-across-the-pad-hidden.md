# 2026-11-07 — Across the pad hides until a row is selected (GUI-60, GB35); the Morph banner follows the law

**What changed.**
- **`docs/design/gui3.html`.** The Morph page's "Across the pad" card is shown only while a row is selected and the Blend map isn't standing in for it. `mw` exposes `crossCard` for the self-check.
- **Self-check 4b3b.** With no row selected the card is hidden. **Control:** with a Structural or stepped row selected, it shows.
- **Records:**
  - GUI-60 in `DECISIONS.md`;
  - GB35;
  - the ledger row `gui60-cross-hidden`, with engine impact "none".

- **The Morph banner (a bug fix, no ruling).** The master's morph state starts in `'blend'`.
  - The shell's Morph view copied D.2's law into that state but never emitted, and `QuantumBanner` redraws only on an emit. So the banner read "BLEND … SWITCH TO QUANTUM" while the Morph mode control read Quantum, until something else emitted.
  - The view now emits when the law moves the master's mode, and the mode is seeded from D.2 before the bench draws.
  - **Self-check 4b3c:** the banner says blend after the law is set to Blend, and quantum after Quantum.

**Why.** The human took the empty card, which held only "select a row", for the parameter list on the plug-in's Morph page. They chose "Hide it until then".

**Why the banner.** The human's build 9 report, relayed by the lead: "it says it starts in quantum mode but it starts in blend mode".
- On the lab page the self-checks mount the bench and emit, which hid it.
- The plug-in runs no self-checks, and nor does `?fast`. On `?fast` the banner read BLEND at the start, and reads QUANTUM with this fix.
- The earlier stand-in bridge repro (#1452) clicked the morph switch first, and that emitted.

**Evidence.**
- `gui3_browser_check`: GREEN, 90 self-checks.
- **Controls, each run and restored:**
  - With the old rule (the card shown whenever the Blend map isn't), 4b3b failed.
  - Without the emit, 4b3c failed: "says quantum after the law is set to Blend".

**Verify.** `./verify fast` on the commit this trace ships in.
