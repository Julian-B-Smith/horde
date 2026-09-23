# b222-stepped-exact — stepped morph corners compare exactly; the quantum group rule's stated reason corrected

- **Queue item:** B222 (second rework on PR #732, from the critic's re-review of `dad0efb`). This corrects `traces/2026-09-23-b222-history-round3-rework.md`, which it cites rather than edits.
- **Why:**
  1. **The B1 tolerance was too loose for integers.** `5e-6*max(|a|,|b|)` let two authored oscillator Seeds (id 3, stepped, 0..999999; for example 999996 and 999999) count as equal. Editor morph-on then wrote the live seed over both corners.
     - **Fix:** `cornersAgreeAt` compares stepped slots exactly.
     - **Why exact is right:** %.6g stores every integer up to 999999 exactly, so any difference between two stepped corner values was authored.
     - **The SUB seed (4014, up to about 4.29e9) exceeds that.** After a reload its corners can differ by rounding alone. Exact comparison then treats them as split and leaves the slot to the field. That is the safe direction: the corners are kept, and only the live edit to that one slot is not adopted.
     - The pre-existing `1e+09` rounding of the SUB seed in the `morph=` chunk is deliberately NOT touched here; the lead is recording it as its own row.
  2. **The previous trace and source comment gave the wrong reason for the quantum group rule.** They cited ADR-175's cycle hazard, but the routing ids expose forward cells only, and ADR-175 calls that hazard "latent while only acyclic cells are exposed". The stated reason is now ADR-176 §3's ruling against ADR-124's chimera: under quantum, what you hear is ONE corner's whole table, and per-cell adoption would give every corner a table it was not authored with. It would become a cycle guard only if the B139 feedback cells are exposed.
- **Row** (`undo_check`, `steppedCornerChecks`), quantum and blend. Before the fix:
  - `FAIL STEPPED (quantum): authored seed corners 999996 / 999999 are NOT overwritten by morph-on (got 999998 / 999998)` (same for blend)
  - After the fix: `OK ... (got 999996 / 999999)`
  - Control: `STEPPED CONTROL (…): unanimous seed corners DO adopt the live 999998`, OK on both builds.
- **Evidence consulted:** the critic's `scratchpad/plant/probe3.inc` and `probe4.inc`; `src/hypersaw_clap.cpp:177` (the seed param); ADR-175/176 as quoted by the review.
- **Alternatives rejected:** a per-parameter tolerance table (another list to keep in step with the param table, where `stepped` is already the ruling fact); pairwise corner comparison (the optional note; compare-to-corner-0 is harmless and the fix did not need to touch it).
- **Verify:** see the PR comment. `./verify full` was run on the committed hash, and the exit code was read from `.harness/last-verify.json`.
- **Open questions:** none new.
