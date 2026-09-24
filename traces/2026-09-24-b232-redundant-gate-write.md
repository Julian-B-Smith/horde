# b232-redundant-gate-write — a gate write that changes nothing no longer moves the glide cadence (critic on ac9bc38)

- **Queue item:** B232 / ADR-183. The critic returned APPROVE-WITH-NOTES on ac9bc38, relayed by the lead on 2026-09-24, with one should-fix before merge. This entry amends `traces/2026-09-24-b232-rereview-gate-direct.md`; it does not replace it.
- **Why:** The forced tick from ac9bc38 fired on EVERY live gate write at revision 2 in blend, even when the write changed nothing. Each forced tick re-phases the glide cadence. So revision 2 depended on how densely a host re-sends the gate's automation.
  - The critic's probe had osc 2 ON in all corners, the gate exempt, the puck sweeping and a 0.5 s glide. Writing 1150 = 1 every 64 samples moved revision 2 by −11.7 dB against the output peak. Revision 1 moved by 0.

## What changed

- `morphRouteEdit`, both sites (exempt gate and unarmed write): the tick is now forced only when a landing will happen. The condition is `gateLive && morphGatePrevLiveW[idx] <= kMorphOnFloor`, which the critic proposed. It is the same condition the landing itself reads.
- `tools/offcorner_check.cpp` gains 5e. Osc 2 is ON everywhere, glide 0.5 s, the puck sweeps 0 → 1 over 60 blocks, and 1150 = 1 is written every 64 samples. This is run twice, with the gate exempt and with it not exempt (unarmed). Each run must render bit-identically to no-op writes of id 41 (Bass XOver, written to its own value) at the same positions.

## Rows before and after

Before the fix (ac9bc38 plus the new rows):
```
FAIL redundant gate writes (exempt gate, 1150 = 1 every 64 samples during a sweep) render bit-identically to no-op writes of id 41 (47c42cae196eb094 vs 96bfda52f9900047)
FAIL redundant gate writes (unarmed gate, 1150 = 1 every 64 samples during a sweep) render bit-identically to no-op writes of id 41 (47c42cae196eb094 vs 96bfda52f9900047)
```
The rows print the gate-write hash first. After the fix, both rows are OK, both hashes are `96bfda52f9900047`, and `offcorner_check: GREEN (0 failures)`. The four gate-on-directly rows stay OK with unchanged hashes (`a01f84462821e439`, `72c4c8789a13b2fc`).

## Evidence

- The critic's measurements are quoted, not re-run.
- The rows above were run on this worktree's build.
- The factory bank was regenerated from the clean commit that carries this change (see the bank commit), and parse-compared against the previous bank.

## Alternatives rejected

- Dropping the forced tick and accepting the stale window. Rejected: the window was measured at up to 6 × 32-sample blocks of osc 2 sounding its silent values (the previous trace).

## Verify

`./verify full` ran on the committed tip of PR #744; it is reported in the PR comment.

## Open questions

None new. A forced tick still re-phases the grid on a write that DOES switch a source on. That is the one case where a landing is the point.
