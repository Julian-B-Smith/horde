# b461-frame-hold-d6-rework2 — D6 `frameHold`: the check judges what the human ruled, and the two-member rows are held out

- **Queue item:** ROADMAP B461, ADR-216 with A1. Rework round 2, dispatched by the lead after the human's ruling. It continues `traces/2026-10-12-b461-frame-hold-d6.md` and `traces/2026-10-12-b461-frame-hold-d6-rework1.md`.
- **Why:** the human ruled B461's acceptance (2) on 2026-10-12 ("go with your recommendations on 1 and 3"). Recommendation 1, verbatim from the lead's message to the human: "judge the fix with Position spread at 0, at both 1× and 2× oversampling. The present law must still read at least one jump there, so the test can see the fault. The pad-as-shipped row is printed but not judged, and becomes B460's test." What the frame does after a crossing is STILL with the human: no tie rule is added and the frame turns as it did.

## What the check judges now (`tools/h2_frame_hold_check.mjs`)

- **JUMP (judged):** the pinned pad with Position spread 0 (`bspread` 0), `frameHold` 1, at C3, G3 and C4, at os 2 and at os 1: no isolated jump on any of the six renders.
- **CONTROL (judged):** the same six with `frameHold` 0: at least one at C3 and at C4, at each os. G3 is printed, not required.
- **B460 rows (NOTE lines, not judged):** the pad as shipped, flag on and off, at both os. Each line says it is ROADMAP B460's, that B460's acceptance promotes the flag-on reading to a judged row requiring 0 / 0 / 0, and that it is printed so the reading stays on the record. The ATTRIB note is kept.
- **Unchanged:** the detector, every threshold, the seed, both ceilings, LIVENESS (it covers every flag-on render, the newly judged ones included), the LOCKED cases and the two named skips.
- Why the as-shipped rows are not judged is in the file's header: the critic's seed sweep (the residue moves with the host seed, which only the spread's drift draws from; all 28 hits over 18 seeds fall on sample 1 of a tick; with `bspread` 0 the flag-on render reads 0 on 18 of 18 seeds).

Readings (2026-10-12, darwin-arm64; isolated jumps at C3 / G3 / C4):

| render | flag off | flag on |
|---|---|---|
| os 2, Position spread 0 (JUDGED) | 3 / 1 / 2 | 0 / 0 / 0 |
| os 1, Position spread 0 (JUDGED) | 5 / 2 / 4 | 0 / 0 / 0 |
| os 2, the pad as shipped (B460's, not judged) | 5 / 1 / 2 | 1 / 0 / 0 |
| os 1, the pad as shipped (B460's, not judged) | 6 / 5 / 3 | 3 / 0 / 0 |

## The two rows held out

`C/D6 two members, detune 14` and `C/D6 two members, detune 30` are OUT of the committed scenario set, **held out until the human rules what the frame does after a crossing (ROADMAP B461).** They pass only where the C++'s and the JS's math libraries agree to the last bit, CI's lanes run another libm, and whether they should exist at all depends on that ruling.

Their full definition, so they can be put back as they were (in `tools/h2_engine_render.mjs`'s `composedRows`, after the last D6 row):

```js
const two = { ...B2, N: 2, frame: 1, phaseMode: 2, law: 0, driftRate: 0.1, bspread: 0.3, detune: 14, K: 0.45, A: 300, frameHold: 1 };
const held2 = [on(45), on(52), blocks(900), off(45), off(52), blocks(20)];
row('D6 two members, detune 14', two, held2);
row('D6 two members, detune 30', { ...two, detune: 30 }, held2);
```

(`B2` is the renderer's two-blade base: N 3, w 0.3, k 5.5, c 0.6, b2on 1, mode2 4, w2 0.2, k2 3, c2 0.35; `row` snaps the patch and uses seed 0xB385 at 48 kHz; `on` is velocity 0.85; a block is 128 samples.)

- **As written (measured in round 1):** both at parity. detune 14: rms 2.813e-15, max-abs 2.198e-13, events identical. detune 30: rms 4.320e-15, max-abs 2.753e-13, events identical.
- **Nudged (`--nudge` 1 to 15, every note-on frequency moved that many doubles up on both sides):** 2 of the 30 renders RED. detune 30 at 2 ULP: rms 2.711e-2, max-abs 0.6086, events differ. detune 14 at 13 ULP: rms 2.791e-2, max-abs 0.6392, events differ. The other 28 held, rms 1.6e-15 to 3.0e-13.
- **Why:** on two members every exit from a hold is half a cycle to within a rounding (a C++ probe: within 8.5e-15 of the tie on 22 of 22 exits, either sign), and the sign of that rounding decides which way the frame turns.

Removing them is not a re-pin of anything on main: they were added by this unpublished branch. The self-digest tool's own line, for each build's reference: `0 changed, 0 added, 2 removed, 565 unchanged`. Floors lowered 567 → 565 in `tools/h2_engine_parity_check.cpp` and `tools/h2_engine_rtsan_check.py`: the true count, and above main's 543.

## The final scenario set and pins

- 565 scenarios: main's 543, byte-identical in both self-digest references, and 22 `C/D6` rows: `:: Swarm-linked spread`, `:: Anchored horde`, `:: Smear and strike`, `:: Drift and lock`, `:: Cross-mod shimmer`, `a framed two-blade pad`, `the centre undefined, K 0`, `a splayed swarm, K -.6`, `a locked swarm, K 1`, `spread law 3`, `blade 2 on the frame, blade 1 off it`, `switched on and off mid-note`, `mono retrigger on a sounding voice`, `members waiting on onset scatter`, `N changes mid-note`, `frequency glide under drift and retune`, `under the cull, the cap lowered mid-phrase`, `37-sample blocks`, `at 44.1 kHz`, `at os 1`, `members join a held frame`, `members join before a mono retrigger`.
- Parity: 562 of 565, the 3 ring rows on their onset window, 0 red; mean bit-exact 36.49%. The 22 rows' own worst: rms 4.1e-14, max-abs 1.1e-12.
- Proof against MAIN's own references: both self-digest tools were run on this tree with main's two reference files in place (and the branch's then restored byte for byte). Each read `543 of 565 scenarios bit-identical`, 22 rows `added`, none `changed`, none `removed`. So every row main pins is unchanged, in the parity build and in the product build.
- Pins, main → this branch: `docs/design/scalpel-horde-engine.js` `581d7942684c91245e4a6637dd40d137335b5d69` → `5d3e3a37cc8f634772ae4c72fefeaf7b446cd15e` (it moved once more in this round, for the comment that cites ADR-216 A1; before that it was `ad8e6791…`); `docs/design/scalpel-interface-lab.html` `77ccee779baa09ce9fddb9d6527ecc0d68e77a8f` → `65d587e9f562008ba9285818cc74d621acebb74b`. After the comment edit the self-digest tool read `0 changed, 0 added, 0 removed, 565 unchanged` for each build.

## Also in this round

- Rebased onto `origin/main` at `bded5c7` (clean). It brought B448 P6's soak gate, which sits just above this check's line in `./verify`.
- The comment above the check's line in `./verify` says what the check judges now (my own comment lines only; the invocation is untouched).
- The lead's readings are cited as ADR-216 A1 in `h2/engine/engine.h`, the composed JS, `h2/README.md`, `docs/port/h2-engine.md` and the ledger.
- `h2/README.md`'s "Last verified" line and status figures, and `docs/port/h2-engine.md`'s counts, are those of the final tree.

- **Evidence consulted:** the lead's round-2 brief (the human's ruling as quoted); ADR-216 A1 on main; round 1's measurements (`traces/2026-10-12-b461-frame-hold-d6-rework1.md`).
- **Alternatives rejected:** keeping the two-member rows with a comment (round 1's choice; CI's other libm makes them a coin toss); judging the pad as shipped too (it is B460's by the ruling); a tie rule (the human's question).
- **Verify:** `./verify fast` and `./verify full` on the staged tree of the commit that adds this trace; the exit codes and `.harness/last-verify.json` are in the PR body. The branch is pushed only on `full` exit 0, so a pushed branch carrying this trace was green there.
- **Open questions:**
  1. What the frame does after a crossing (with the human). As built it turns back to the measured centre at 8 cycles per second, about 60 ms of sweep; on two members the direction of that turn is decided by a rounding.
  2. B460 owns the pad-as-shipped rows; its fix should turn the flag-on NOTE lines to 0 / 0 / 0 at both os, and then promote them.
  3. The branch `bank-breathing-pad-locked` (B462) touches the same two self-digest files; whichever lands second regenerates its TOTAL and header lines.
  4. The lab toggle has not been clicked in a browser.
