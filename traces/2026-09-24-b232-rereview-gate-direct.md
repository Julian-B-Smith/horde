# b232-rereview-gate-direct: a gate switched ON directly lands too (critic re-review of PR #744)

- **Queue item:** B232 / ADR-183. This follows the critic's APPROVE-WITH-NOTES on 83fbd72, relayed by the lead on 2026-09-24. It amends `traces/2026-09-24-b232-rework-landing.md` and does not replace it.
- **Why:** One path to S1 remained. Osc 2 was off in every corner, with the puck at x=0.3 and a 0.5 s glide. An UNARMED write of 1150 = 1 (the path both a GUI click and host automation take) turned corner A on. At revision 2, detune then glided 0.31 → 0.1 audibly.
  - The cause: `applyParam` switches the engine on as the event arrives. So the landing's "was off" read, which went through `readParam` and saw the committed enable, already said ON at the next tick.
  - Separately, the re-strike sounded the silent values for up to one grid tick before any landing.

## What changed

- **"Was off" = the previous tick's live weight.** It is `morphGatePrevLiveW[g] <= kMorphOnFloor`, the same sum and threshold the ramp switches the source by.
  - The value is refreshed at the top of every revision-2 tick, in either mode. `morphGatePrevLiveW` is sized in `morphInit`.
  - An exempt gate is live-only, so its weight is its live value.
  - The puck, the GUI and the host now take one path.
- **No stale re-strike.** A live gate write at revision 2 in blend (unarmed, or an exempt gate) sets `morphTickNow`. The field then ticks in the `morphStep` call before the event's span renders, so the landing precedes the first re-struck sample.
  - The forced tick's interval ends AT the event, and the upcoming span starts the next grid interval. The tick is therefore keyed on the event's sample position, not on how the host split the block.
  - Evidence: `subdiv_check` GREEN. The scratch window probe gives identical first-block output at all three grid phases.
- **offcorner_check 5d**, per source (osc 2 via 1150, the sub via 4015):
  - a VALUE row: one tick after the toggle, the slot reads the ON corner's value;
  - an AUDIO row: the toggle arrives at sample 100, between ticks, with an inert event at sample 200 so the block splits again before the grid boundary. The whole render must equal the as-if patch's.

## Rows before and after

On the 83fbd72 shell (this check source, `src/hypersaw_clap.cpp` restored from 83fbd72):
```
FAIL gate on directly: osc 2 (1150) toggled ON at x 0.3, glide 0.5 s — one tick later id 1004 reads the ON corner's 0.1 (read 0.30517998633693622), ON corner(s) 1
FAIL gate on directly: osc 2 (1150) toggled ON at sample 100 (mid-grid) — the render equals the as-if patch, so no sample sounds the silent values (4e572980e9df9890 vs 1148707f8cf43922)
FAIL gate on directly: sub (4015) toggled ON at x 0.3, glide 0.5 s — one tick later id 4007 reads the ON corner's 0.2 (read 0.40517998633693619), ON corner(s) 1
FAIL gate on directly: sub (4015) toggled ON at sample 100 (mid-grid) — the render equals the as-if patch, so no sample sounds the silent values (67abda39e3e9da43 vs 092f9af4b58f2a03)
offcorner_check: RED (4 failures)
```
After the fix (fa3480b), all four rows are OK and `offcorner_check: GREEN (0 failures)`. The AUDIO rows read `a01f84462821e439 vs a01f84462821e439` (osc 2) and `72c4c8789a13b2fc vs 72c4c8789a13b2fc` (sub).

**Calibration.**
- With only the forced tick disabled (the plant `if (morphAccum < grid) return;`), the VALUE rows pass and both AUDIO rows FAIL: osc 2 `eca008e124449257 vs 1148707f8cf43922`, sub `6aa6ae6d109077a3 vs 092f9af4b58f2a03`. So the AUDIO rows are what pin the forced tick.
- **Coverage boundary (L0033).** My first mid-block row read a VALUE at the end of the toggle block. It stayed green without the forced tick, because a single mid-block event makes the remainder of the block complete the grid, so the tick fired at the event anyway. It was replaced by the AUDIO row with a second split at sample 200.

**Pre-landing window, measured** with a scratch probe: 32-sample host blocks, toggle at grid phases 0, 3 and 7 blocks, compared against a no-toggle twin.
- **Before** — measured on the intermediate build that already had the live-weight keying but no forced tick; at 83fbd72 there was no landing at all, only the glide. For osc 2: detune read 0.31 (the silent plain blend) for 5, 2 and 6 blocks after the toggle, while osc 2 was already sounding (6.4e-2 peak above the twin in the toggle block).
  - The sub was silent before the landing tick (diff exactly 0), because its ramp weight held 0 until the tick. It too landed only at the next tick.
- **After:** both land in the event's own block at every phase. There are 0 samples of stale values.
- A consequence to note: both sources' level ramps now start at the event rather than up to one grid tick (5.8 ms) later.

## The as-if identity is not bitwise for every parameter

This is the critic's finding, recorded as asked. Two differences remain, and neither is the OFF corner's timbre; the rows prove 1004, 4007 and 4010.
- 1045 (o1.tilt) differs by −11.6 dB at the move. This is likely the known tilt/toneTilt readback alias (undo_check's "id 1045 o1.tilt still loses values above 1 to the toneTilt alias"), since the landing rewrites 1045.
- 91/1091 (onsetScatter) differs by −10.9 dB about 450 ms later. This is likely a re-seeded random stream.

## Evidence

- **Factory bank.** Regenerated from a clean fa3480b. Parse-compare against the previous bank shows `files differing beyond engine_revision/build: 0`; only `build` changed (6bf504a → fa3480b).
- **Render probe.** 451/451 revision-1 renders are bit-identical to origin/main, and 0 revision-2 renders differ from revision 1.
- **Other gates.** `subdiv_check` GREEN, `rtsafety_probe` GREEN, `./verify fast` GREEN before the commit.

## Alternatives rejected

- Keeping the `readParam` key and also watching the edit. That is two signals for one fact; the live weight is the ramp's own quantity and covers all three paths.
- Leaving the re-strike window and just reporting it. A clean fix existed, so I did not leave it open.
- Changing `applyParam`'s enable handling, or the B48 smoother. That is outside the revision gate, and the lead has the smoother as its own row.
- A forced tick whose interval includes the upcoming span (the regular tick's accounting). Its `dt` would depend on where the host or the next event splits the block.

## Verify

`./verify full` ran on the committed hash of this change set. It is reported in the PR comment and in `.harness/last-verify.json` at that hash.

## Open questions

1. The forced tick re-phases the morph grid after a live gate write, at revision 2 in blend only. Every later glide step is then keyed on the write's position. This is deterministic, but it is a behaviour a revision-2 patch now has.
2. Quantum mode (unchanged) has no forced tick and no landing. Its analogous stale window on a gate write remains, as noted in the rework trace.
