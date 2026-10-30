# b584-gui3-slice6-followup: Mix fits with the bank shown, the bank's knobs draw, the verify comment carries no number

- **Queue item:** B584 slice 6, the lead's review follow-up on PR #1293 (corrects `traces/2026-10-30-b584-gui3-slice6.md`, which stays as written: its Mix overflow and its open question 3 are answered here).
- **Why:** the review found the console note clipped on Mix, a bank card showing labels with no knobs, and a stale count in `verify`'s comment.
- **What changed:**
  - **Mix:** the console's strips use about half the width, so the summed scope and spectrum (back to 84 px each) and the note moved into the room to their right (`.mixrow`); the strips keep the padding the human asked for. Measured with the `fit` flag, bank shown: 436 px in a 436 px area, fits, nothing hidden or cut (before: 456, overflowing by 20). Where the width is short (a docked pad, the inspector) the row wraps and the page area scrolls.
  - **The bank's missing knobs, cause: pre-existing, in the shell (not the bundle, not slice 6).** The shell's own rule `.bank-strip .hd-card .face { flex-wrap: nowrap }` put a card's four utility knobs in one row. The bundle's utility knob is a row of a name (at least 7ch) and a dial, about 85 px each, so four do not fit a 220 px card, and flex shrank every dial's face to 0 by 0 (measured in headless Chrome: face 0 x 0, its control 46 x 28, the name only). The DOM held the knob throughout, which is why the DOM dump looked right. The row now wraps (two by two).
  - **A self-check measures it** (the page's 21st): ENV 2's card must draw every control with a dial of real size; the control squeezes the card to 40 px with nowrap and must read as not drawn. Proven both ways: green with the fix, and red (`FAILED · Bank card ...`) with the old `nowrap` put back, then restored. It is a browser check (layout cannot be measured in node), so, like the page's other self-checks, it is not in `verify`.
  - **`verify`:** the seam check's comment no longer writes a count of tests or plants (ADR-180 §1); it names the new coverage. Comment only, no gate change.
  - **README, `layout.json` note, `shell-data.js`:** wording for the new layout; counts of tests, plants and self-checks removed from the README's prose.
- **Evidence:** headless Chrome probes of the card's face, control and svg rects; the `fit` flag on Main and Mix; the seam check (unchanged: 29 tests, 34 plants) and the generator (27 must-fail controls) still green.
- **Alternatives rejected:** widening the card to 370 px (the strip would show two cards); patching the bundle's knob (it is right: the shell squeezed it).
- **Verify:** `nice -n 10 ./verify fast`, run last on the committed tree; tail in the report.
- **Open questions:** none new.
