# 2026-11-27 — GUI-77: on the FX glasses a switch hides a row and a value at its end doesn't; rows follow a switch's target; the vector monitor stays dark (B712)

**What changed.**
- **DECISIONS.md:** GUI-77, the human's three answers by poll.
- **docs/design/design-ledger.json:** the ruled row `gui77-glass-gates` and its engine impact (none).
- **In the master** (`1795785582-38fb`, see `traces/2026-11-27-glass-rules-master.md`): G6, a new G16 and the vector monitor's line.

**Why.** The human, 2026-11-27: "Yes, glass rules". Scape's and Bulwark's answers to ADR-409's briefs (each its seq 2, in its own mailbox) independently asked whether GUI-21 hides a row gated by a continuous value. Bulwark also asked which state rows follow during a switch's fade.

**Evidence.**
- **The poll's answers:** "Flips hide, sliders don't (Recommended)", "Target value (Recommended)", "Black with TUBE's inks (Recommended)".
- **GUI-21's wording:** "a control that depends on a mode or a switch appears with it" (master README rule 4).
- **The workshop's vector monitor:** already black with TUBE's inks on a pale scheme (`docs/design/fx-screens-workshop.html:528-529`).
- **The scope check:** the shell's `live_when` hides about ten rows on continuous gates (for example `driftRate` when `driftDepth` is 0). The ruling was asked for the glasses, so the shell is left as it is and put to the human separately.
- `design_chain_check` and `engine_needs_check`: green.

**Engine and FX device.** Engine: none. FX device: the same glasses and rules.

**Open.**
- Answered the same day as GUI-77 A1; see below.
- **Verify:** `./verify fast`, exit code in the PR.

## A1 — horde's own pages follow ruling 1 too (2026-11-27)

**What changed.** `docs/design/gui3.html`:
- A new `drawn()` decides what's drawn and skips continuous comparisons (`gt`, `ne` on a row without options). `live()` still decides whether a control can act, so the info box still says "Inert now". Every place that hid a control, section or table row now asks `drawn()`.
- The GUI-21 self-check now asks that no drawn control sits behind a shut stepped gate.
- A new self-check, "Slider gates stay drawn", holds A1.

The layout's `live_when` table (PA-8's facts) is unchanged except for its note. DECISIONS has GUI-77 A1 and GUI-21's "what's shown follows what's heard" reading narrowed. The ledger row names the shell.

**Why.** The human asked for a demo of the question ("I don't think I totally understand that last question"). A side-by-side demo showed a slider-gated row hiding at 0 against one staying drawn, with a switch-gated row hiding on both. The human answered "Right: stay drawn". An earlier "Keep as they are", given before the demo, was not recorded.

**Evidence.**
- `gui3_browser_check`: GREEN, 95 self-checks.
- **Plant:** the continuous-gate line removed from `drawn()` → RED: "Slider gates stay drawn (GUI-77 A1): with Sub drive at 0 its Character is HIDDEN". Restored byte for byte (cmp).
- **Screenshots, the default Sub page** (Sub drive defaults to 0): main hides CHR in the Colour section, and this branch draws it (Tube, Diode, Fold).
- `gen_gui3_controls --check`, `design_chain_check` and `engine_needs_check`: green.

**Engine and FX device.** Engine: none. FX device: none.
