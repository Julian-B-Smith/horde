# b232-rework-landing: a source that was OFF lands, it does not glide (critic rework on PR #744)

- **Queue item:** B232 / ADR-183. This is the REWORK (small) the critic returned on PR #744, relayed by the lead on 2026-09-24. It amends `traces/2026-09-24-b232-offcorner-blend-rev2.md` and does not replace it.
- **Why:** Three problems from the review.
  - **S1: the OFF corner was audible when the puck moved up across the floor.** Below the floor, the blend reads the plain sum, and near a pure OFF corner that sum IS the OFF corner's value. The one-pole glide then carried the slot from that value after the source re-struck. The critic measured this against the as-if patch: the sub at −23 dB at the default glide and −13.5 dB at a 0.5 s glide, and osc 2 at about −9.5 dB. It had to be fixed before any revision-2 patch is saved.
  - **S2: the oracle never tested how the ON corners are mixed.** Every patch had B == D.
  - **(g): the off-grid control never proved its own power.**

## What changed

- `src/hypersaw_clap.cpp`, `morphStep`:
  - At revision ≥ 2 in blend, a gated continuous slot whose source was OFF at the top of the tick now commits its target outright. This is the "never applied yet" landing that morphApplyTarget's `-1e29` sentinel performs. I wrote it as a direct `morphCommitSlot(i, target)` instead of resetting the sentinel, because the sentinel would make a slot whose source stays off re-apply (and re-echo) every tick. `morphCommitSlot`'s 1e-9 deadband does not re-apply an unchanged value.
  - The gate state is snapshotted from `readParam(gate)` into `morphSrcWasOff` before the slot loop. The loop re-commits the gate itself, and the gate's position in morphIds relative to the slots it gates differs per source. The sub's gate comes after every sub row; osc 1's enable sits mid-prefix.
  - The distinct gate slots (`morphGateSlotList`) and the flag array are sized in `morphInit`. `rtsafety_probe` is still GREEN (allocation-free).
  - Quantum mode and revision 1 never read the new state.
- `tools/offcorner_check.cpp`:
  - **5c, moving puck.** The puck moves from a pure OFF corner to the midpoint in one block while a note sounds. It covers osc 2 and the sub, at glide 0.5 s and 0.008 s. The render must equal the as-if patch's render bit for bit. Downward moves are pinned too.
  - **5b, the ON corners' mix.** At (0.3, 0.4), with B = 0.8, D = 0.5 and A, C off, the value must equal `(wB·vB + wD·vD)/(wB + wD)`.
  - **Control power.** At (0.3, 0.4) the plain sum and the renormalised sum of the control DIFFER in doubles.
- `docs/presets/factory/**`: regenerated from clean `6bf504a`.

## Rows before and after

Before the fix (branch at 5b2bfc1 plus the new rows):
```
OK   CONTROL POWER at (0.3, 0.4): the plain and the renormalised sums of the control DIFFER in doubles (0.32000000000000001 vs 0.32000000000000006), so the row below cannot pass vacuously
OK   mix: at (0.3, 0.4) with B=0.8, D=0.5 (A, C off) rev 2 reads (wB vB + wD vD)/(wB + wD) = 0.67999999999999994 (read 0.67999999999999994; wB 0.18) wD 0.12)
FAIL moving puck: osc 2, x 0 -> 0.5 (OFF -> across the floor), glide 0.5 s — rev 2 renders bit-identically to the as-if patch (66a879223d128846 vs ccaf443f74d8f457)
OK   moving puck: osc 2, x 0.5 -> 0 (down to OFF), glide 0.5 s — rev 2 renders bit-identically to the as-if patch (d70e00c1fcd25f2d vs d70e00c1fcd25f2d)
FAIL moving puck: osc 2, x 0 -> 0.5 (OFF -> across the floor), glide 0.008 s — rev 2 renders bit-identically to the as-if patch (7bdbd689a239494b vs f403295ce23651bb)
OK   moving puck: osc 2, x 0.5 -> 0 (down to OFF), glide 0.008 s — rev 2 renders bit-identically to the as-if patch (bf4dfecbd0446f57 vs bf4dfecbd0446f57)
FAIL moving puck: sub, x 0 -> 0.5 (OFF -> across the floor), glide 0.5 s — rev 2 renders bit-identically to the as-if patch (9884496eac7021a6 vs 77341a0b3479bfc2)
OK   moving puck: sub, x 0.5 -> 0 (down to OFF), glide 0.5 s — rev 2 renders bit-identically to the as-if patch (2ffd9f65fa8bb821 vs 2ffd9f65fa8bb821)
FAIL moving puck: sub, x 0 -> 0.5 (OFF -> across the floor), glide 0.008 s — rev 2 renders bit-identically to the as-if patch (8a2f7e0bdbd7270b vs 77341a0b3479bfc2)
OK   moving puck: sub, x 0.5 -> 0 (down to OFF), glide 0.008 s — rev 2 renders bit-identically to the as-if patch (2ffd9f65fa8bb821 vs 2ffd9f65fa8bb821)
offcorner_check: RED (4 failures)
```
After the fix (6bf504a): all ten rows are OK, and `offcorner_check: GREEN (0 failures)`. The upward osc-2 rows now read `ccaf443f74d8f457` and `f403295ce23651bb` (equal to the as-if patch). The upward sub rows read `77341a0b3479bfc2` twice.

Two notes on these rows:
- The 5b mix row passed before the fix. It exercises the renormalisation, which was already correct; it is new COVERAGE, not a regression row. It would go red if the ON corners were weighted any other way. For example, B alone gives 0.8, and an equal B/D mix gives 0.65. The row asserts the value is more than 0.05 from 0.8 and more than 0.01 from 0.65.
- The control-power row is a property of the chosen position, computed with the shell's own expressions. It was not run at (0.3, 0.7). Hypothesis, from the first trace's Python search over a 9×9 grid: it would read red there, because (0.3, 0.7) was not among the positions where the two sums differ. That is also why the control moved off (0.3, 0.7).

## Evidence

- **Factory bank.** A parse-compare against the previous bank (bbaa4f3) shows `files differing beyond engine_revision/build: 0`. Only `build` changed (bbaa4f3 → 6bf504a).
- **Render probe.** A scratch probe rendered 41 patches at 11 puck positions. 451/451 revision-1 renders are bit-identical to origin/main, and 0 revision-2 renders differ from revision 1.
- **Other gates.** `rtsafety_probe` GREEN, `undo_check` GREEN, `bank_check` 0 failures, `intent_check` 32 fixtures 0 failures (all before the full run).

## Alternatives rejected

- Resetting `morphCur[i] = -1e30` every tick while the source is off. It gives the same values, but applyParam then runs every tick for every gated slot of a silent source.
- Landing in quantum mode too. The rule and ADR-183 are about the blend, and the brief pins quantum as unchanged. It is left as a question below.

## Verify

`./verify full` ran on the committed hash of this change set. See the PR comment, and `.harness/last-verify.json` at that hash.

## Open questions

1. Quantum mode has the analogous glide-from-an-unheard-value on a flip into a corner where the source comes back on. It is not revision-gated work and was not asked for. Should it get a row?
2. The landing is keyed on the gate's LIVE value at the top of the tick. If a host automates the gate directly while morph is on, the landing follows the host's value. That seems right, but no row pins it.
