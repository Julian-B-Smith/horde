# 2026-11-03 — Mappings in the foot row; a Control's sweep (GB5, GUI-16, GUI-17)

**What changed.** `docs/design/gui3.html`: Mappings moved from a third column to the foot row, with self-checks 4l and 4m. The master's `components/bundle.js` and MapControl README: Option+Shift-drag moves a Control's high end. Published as master `1793764535-dcb5` and re-vendored. `morph-map-lab.html` retired. GUI-16, GUI-17, the ledger, GB1, GB5.

**Why.** The human, 2026-11-03: "I also approve of mappings in the foot row", "I approve of the C1 console and new signal flow diagram", and "when a mapping is set to CTL, there's no intuitive way to control the depth of modulation".

**Evidence consulted.** `arp-face/bundle.js`'s `gesture` (a Control's Option-drag slides its band, marked PLACEHOLDER) and `chordHit` (ADR-341 r4 A1: Shift+Option is already the polarity chord with no drag, and a fine drag with one). The lab's measurements, in its own self-check.

**A bug caught.** Check 4l first undid one step too few, so the shell loaded with morph on. It passed, and it showed only when a later click turned morph off. It now undoes both, and 4m removes its own test route through undo.

**Not checked.** Real keys through a drag: the browser tool can't hold modifiers on one, and a try arrived as a plain drag, which the shell correctly refused. A plugin host. The C1 console isn't ported yet.

**Verify.** See the PR body.
