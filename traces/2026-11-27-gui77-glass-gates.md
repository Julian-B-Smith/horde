# 2026-11-27 — GUI-77: on the FX glasses a switch hides a row and a value at its end doesn't; rows follow a switch's target; the vector monitor stays dark (B712)

**What changed.**
- **DECISIONS.md:** GUI-77, the human's three answers by poll.
- **docs/design/design-ledger.json:** the ruled row `gui77-glass-gates` and its engine impact (none).
- **Not yet in the master:** G6, a new G16 and the vector monitor's line in the screens section's rules list. That hop waits for PR #1535 to merge.

**Why.** The human, 2026-11-27: "Yes, glass rules". Scape's and Bulwark's answers to ADR-409's briefs (each its seq 2, in its own mailbox) independently asked whether GUI-21 hides a row gated by a continuous value. Bulwark also asked which state rows follow during a switch's fade.

**Evidence.**
- **The poll's answers:** "Flips hide, sliders don't (Recommended)", "Target value (Recommended)", "Black with TUBE's inks (Recommended)".
- **GUI-21's wording:** "a control that depends on a mode or a switch appears with it" (master README rule 4).
- **The workshop's vector monitor:** already black with TUBE's inks on a pale scheme (`docs/design/fx-screens-workshop.html:528-529`).
- **The scope check:** the shell's `live_when` hides about ten rows on continuous gates (for example `driftRate` when `driftDepth` is 0). The ruling was asked for the glasses, so the shell is left as it is and put to the human separately.
- `design_chain_check` and `engine_needs_check`: green.

**Engine and FX device.** Engine: none. FX device: the same glasses and rules.

**Open.**
- Whether the shell's own continuous gates follow GUI-77 r1 (the human's).
- **Verify:** `./verify fast`, exit code in the PR.
