# 2026-11-27 — The rules list per glass in the design master's screens section (ADR-409, GUI-77; B712 (2))

**What changed.**
- **`docs/design-system/master/screens.md`, published as `1795785582-38fb`:**
  - "Who designs what": ADR-409's map, the Master limiter with no glass, and the three glasses with no module.
  - **Rules for every glass, G1–G16:** the frame, whole-number scale and fonts as data, the four-and-four, keyboard mode, the wheel (GUI-15), GUI-21 with GUI-77 r1, corner colours on the chassis, the GUI-58 marks, one model and two views with the module's writers, manifest keys, scheme policy, MOCK and PROVISIONAL, reduced motion, rendering alone, the row bands until GB33, and GUI-77 r2.
  - **One list per glass:** the house screen (Kiln Drive and the Weir filter, one list and two layouts), the 80s workstation, the Game Boy Color, the vector monitor (GUI-77 r3), the sonar scope, the CAD blueprint, nixie and neon, and the phosphor CRT terminal. Each names its size, inks, scheme policy, what it can't draw and its GUI-58 marks.
  - The auditions table is gone.
- **Re-vendored** to `docs/design/arp-face/` (only the stamp lines change). The provenance row names the version.
- **The ledger:** `master_versions`; `adr409-fx-module-glasses` and `gui77-glass-gates` are in-master on `screens.md`.

**Why.** The human, 2026-11-27: "Yes, glass rules". ADR-409's readings make the rules lists the GUI session's, and the B712 briefs say this section replaces their draft lists.

**Evidence.**
- **The rules come from the eight briefs' draft lists** (one per module mailbox), merged into one common list, with each glass's facts from its brief's "what it can draw" section and the labs those cite (`docs/design/fx-screens-workshop.html`, `docs/design/fx-screen-marks-lab.html`).
- **Where the drafts differed** (reduced motion, the wheel, rendering alone appeared in some but not others), the common list carries them for every glass.
- `design_chain_check`: green at `1795785582-38fb`.
- `engine_needs_check`: green.

**Engine and FX device.** Engine: none. FX device: the same glasses and rules (ADR-409).

**Open.**
- Each module's revisions are reviewed against this list when they arrive (B712 (3), through the lead's mailbox filings).
- **Verify:** `./verify fast`, exit code in the PR.
