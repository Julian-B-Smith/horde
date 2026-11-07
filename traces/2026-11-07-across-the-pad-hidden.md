# 2026-11-07 — Across the pad hides until a row is selected (GUI-60, GB35)

**What changed.**
- **`docs/design/gui3.html`.** The Morph page's "Across the pad" card is shown only while a row is selected and the Blend map isn't standing in for it. `mw` exposes `crossCard` for the self-check.
- **Self-check 4b3b.** With no row selected the card is hidden. **Control:** with a Structural or stepped row selected, it shows.
- **Records:**
  - GUI-60 in `DECISIONS.md`;
  - GB35;
  - the ledger row `gui60-cross-hidden`, with engine impact "none".

**Why.** The human took the empty card, which held only "select a row", for the parameter list on the plug-in's Morph page. They chose "Hide it until then".

**Evidence.**
- `gui3_browser_check`: GREEN, 89 self-checks.
- **Control, run and restored:** with the old rule (the card shown whenever the Blend map isn't), 4b3b failed.

**Verify.** `./verify fast` on the commit this trace ships in.
