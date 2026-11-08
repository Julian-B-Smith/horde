# 2026-11-07 — The share bars, in the Morph inspector (GUI-56 r3, GUI-56 A1, GB17)

**What changed.**
- **The master (`1794113431-06a5`):**
  - MorphInspector's QUANTUM BOUNDARIES now shows a share bar: one segment per corner and a divider between each pair, which you drag or move with the arrow keys. Each corner's percentage is listed under it.
  - The morph layer gains `solveShares`, a fixed 28-step solver from the gesture's starting offsets, and samples shares on 48 × 48 (`SHARE_G`).
  - `shares` now reads the offsets once (`ownerFn`). It gives the same answer, faster.
- **The vendored bundle:** re-vendored. The shell needed no wiring: it already mounts MorphInspector, and its overrides of the morph layer's `offsets`, `lock` and `locks` carry the bar's writes into the device.
- **Self-check 4b4a** checks, on a Structural row:
  - A asks for 15 points more;
  - A gets it while C and D hold;
  - the row locks;
  - one undo step;
  - the bar's segments match shares recomputed from the device's offsets;
  - undo restores them;
  - the same drag again gives identical offsets.
- **Records:**
  - GUI-56 A1;
  - the GB17 status;
  - the ledger row `gui56-share-bars` (in the shell) and `master_versions`;
  - the arp-face provenance row.

**Why.** The human ruled D2's share bars into 1.0 (GUI-56 r3, 2026-11-06) and asked to work through that session's lab rulings (2026-11-07).

**Evidence.**
- `gui3_browser_check`: GREEN, 94 self-checks. The line reads: "asking A for 35 % gave 35, 10, 30, 25 % (from 20, 24, 30, 25, control), C and D held, the row locked, 1 undo step, the bar agreeing with the pad, undo restoring it, and the same drag gave the same offsets."
- **Plants, each run and restored:**
  - **A solver writing to a copy:** red, "gave 20, 24, 30, 25 %", NOT locked, 0 undo steps.
  - **A random draw in the solver:** red, "the same drag gave DIFFERENT offsets".
  - **Dropping the master's own undo record:** still green, because the shell wraps every inspector gesture in its own history step. The step count is the shell's guarantee, not the master's.

**Verify.** `./verify fast` on the commit this trace ships in.
