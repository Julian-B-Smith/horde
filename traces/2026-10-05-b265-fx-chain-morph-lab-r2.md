# b265-fx-chain-morph-lab-r2 — per-corner FX chains on a four-corner XY pad, the round-2 brief's pipeline built and self-checked

- **Queue item:** B265, round 2. Dispatched by the horde lead on 2026-10-05 after the human ruled D1–D8 (ADR-193, lead commit `6f69b9c`, which is not yet on `origin/main` when this branch was cut). On D1 the human said "let's see it in action in the lab".
- **Why:** The brief (`docs/proposals/fx-chain-morph-round2.md`) asks for a four-corner build of the §2 pipeline, raced by ear. D2 asks for a dev toggle on every new behaviour. D3 asks for a minimal, legible rule set. D5 asks for tails that ring out, with a cap. The goal is for the human's listening pass to rule on behaviour heard, not on a description.

## What was built

`docs/design/fx-chain-morph-lab-r2.html` is a new, self-contained lab. It carries `lab-part FX` and `lab-review B265 · 2026-10-05`, both themes, and round 1's tokens. Round 1's files are untouched. The navigator was regenerated: `docs/design/index.html` now pins the lab for review and lists it under FX.

**The model** (block 1) has no DOM, no clock, and uses mulberry32 only. Its parts:

- **Corners and modules.** Four corners on a pad. Eight module types, one instance each, with the cap held as the named constant `INSTANCES_PER_TYPE`. Three corner sets: four unrelated presets; four neighbours; and round 1's "across" and "grow" pairs on the bottom and top edges.
- **The map** is a pure function of position. It runs:
  1. bilinear weights, optionally sharpened to wᵞ;
  2. presence votes, flipped at a seeded threshold in QUANTUM and used as the wet gain in BLEND;
  3. the lane 1 F1 order target: ghost ranks over all eight types, plus jitter scaled by (1 − max w);
  4. the snap to V, by either the lane 4 F1 decode (an exact DP over (set, last, closed), with the rules applied on append and the CPU budget on the finished set) or the lane 1 F2 walk (from the heaviest corner, with lawful drop, swap and insert hops);
  5. the Sub entry, chosen by round 1's threshold law over the entry values.
- **Content-keyed draws.** Every draw is keyed by its content (a type, a pair or a Sub entry), never by corner index.
- **The router** is a control-rate state machine on one spine of junctions (round 1's E encoding), so round 1's checkers read its frames. It works as follows:
  - Each change is one split group, u/‖u‖_p.
  - An arrival arms its input first.
  - A move is a wet dip.
  - A leaving ECHO, Scape or Sluice drops its send and rings for min(its decay, its cap).
  - At most K changes may be open per lane, under a +3 dB headroom cap, and one change may always start.
  - Changes are issued upstream-first.
- **Stand-ins** for all eight types run in round 1's Engine. Two things changed: an instance rebuilds when its `gen` changes, and nodes nothing touches are not run. POWER plays the same Router and Engine through a ScriptProcessor.
- **The V rules** are R1, a CPU budget of 10 points; R2, Bulwark never after Scape, with exception E1 (Scape straight into Bulwark as the last module); and R3, Sluice never after Scape.

**Dev toggles (D2):**

- SHARPEN γ
- JITTER (T)
- SNAP TO V, with DECODE or WALK
- NORMALISE
- TAILS RING
- MODE: QUANTUM or BLEND
- ALL ON and ALL OFF (ALL OFF is round-1 behaviour)

The advanced panel (D4) holds γ, T, p ∈ {1, 4/3, 2}, K ∈ 1–4, the seed with Reshuffle, and tail caps for ECHO, Scape and Sluice, where the top of each slider means ∞.

## Metrics (defaults: four presets, seed 265, T 0.7, γ 2, p 2, K 2, 41×41 grid; headless node runs, the same as the page)

The pad, by mode and snap:

| mode · snap | pure-preset area | distinct | jumps 1 / 2 / 3+ | outside V | unanimity, strict / supported reading |
|---|---|---|---|---|---|
| quantum · decode | 45.4 % | 18 | 222 / 62 / 15 | 0 | 0 / 0 |
| quantum · walk | 43.2 % | 22 | 357 / 53 / 1 | 0 | 0 / 417 |
| blend · decode | 16.1 % | 21 | 205 / 78 / 36 | 0 | 0 / 6 |
| blend · walk | 19.3 % | 44 | 397 / 72 / 48 | 0 | 0 / 911 |
| blend · snap off | 11.7 % | 62 | 528 / 110 / 16 | 1619 (CPU up to 15) | 0 / 1227 |
| quantum, all toggles off | 28.4 % | 11 | 302 / 7 / 41 | 0 | 0 / 545 |

The routed sweeps (quantum, decode):

- Every sweep stays within +0.00 to +3.01 dB of lane gain.
- No sweep breaks I1, I7-tail or unanimity.
- Router changes per sweep: 4–9 on the edges and diagonals, and 17 on the circle.
- Two-or-more-change target jumps per sweep: 0–2.
- The rendered worst level step is 2.8–4.6 dB, against 1.0–1.3 dB with the ends held still.
- The highest CPU including ringing tails is 15, on the circle, against a budget of 10 for the target.

## Self-check: 12/12 claims hold, 12/12 plants caught (headless, and in headless Chrome at load)

| # | claim | plant |
|---|---|---|
| 1 | Corners are exact (3 sets × 2 modes × 3 methods × sharpening on and off) | Decode with L = 2, below the noise clamp |
| 2 | Edge reduction: 48 edge maps at 41 points, and 16 routed edge sweeps identical at every tick | Radial weights |
| 3 | Strict unanimity on 18 pad maps and 14 sweeps | Jitter bound broken (h = 12) over seeds 265–272 |
| 4 | Lane gain stays in [−6, +3.01] dB on 14 sweeps | Break-before-make (−180 dB), and round 1's ramps (+6.02 dB) |
| 5 | The snap keeps every pad point in V | Snap off (1619 cells out) |
| 6 | I1 and I2 | A shadow move |
| 7 | I7 tail-allowed | Outputs-first arrival |
| 8 | Sluice's tail is capped (1.60 s) | No cap, with feedback 1.02 (5.16 s) |
| 9 | No more than K changes are open | Budget off (3 open) |
| 10 | Continuity: no weight moves more than 0.25 per tick | Hard switch (1.00) |
| 11 | γ = 2 widens the pure-preset area (19.0 % → 45.4 %) | Toggle disconnected |
| 12 | Determinism | Next seed (674 cells move) |

## Decisions made inside the brief (each is recorded in the lab's comments)

- **Ghost ranks range over all eight types,** not the corners' union. Otherwise an edge's ranks depend on the far corners, and edge reduction fails.
- **Edge reduction is compared on a canonical audible graph,** not on raw frames. The pad's router carries parked modules (dry wires) that the pair never had, and the first comparison called identical sound different.
- **The continuity check reads stable identities:** each module's wet and dry, and each lane's reach through each module times the module's wet. It does not read edge keys, because junction renames between equal signals looked like hard switches. The bare send is excluded, because arming it at wet 0 is inaudible.
- **A Sub crossfade is admitted against both of its ends.** The first build missed a mover reachable from the new end, and the Sub reached +4.7 dB.
- **Every comparison treats 1e-12 as a tie** and breaks it by house order. Float noise on an exact tie made the walk chatter: 74 changes on one diagonal.
- **The tail-allowed window is the cap plus two crossfades:** the rest of the leave once the send crosses the floor, plus the closing fade.
- **The walk is made a pure function of position,** by starting from the heaviest corner. Lane 1 F2's one-hop-per-slot pacing is left to the router.

## Evidence consulted

- `docs/proposals/fx-chain-morph-round2.md`.
- ADR-193 (from `6f69b9c`).
- The ROADMAP B265 and B266 rows.
- `docs/design/fx-chain-morph-lab.html` (the router, checkers, threshold law, Engine and self-check).
- The B265 and B266 traces.
- Lane reports 1, 2 and 4 in full, and lane 5 for tails and Sluice. These sit in the gitignored `local/research/reports/`.
- `tools/labharness/lab_load_check.mjs`, `tools/gen_lab_index.py` and `tools/serve_labs.py`.
- INDEX entries L0026, L0041, L0064 and L0073.

## Alternatives rejected

- **Corner-aware ghosts** ("ghosts follow the corners", lane 1 rule b). They break edge reduction, so the supported reading of unanimity is reported, not enforced.
- **Enumerating V per point.** For 8 types that is 109 601 chains per point; the exact DP replaced it.
- **A stateful lane-1 walker for the map.** It makes pad metrics path-dependent.
- **A shadow instance for moves.** It breaks I1, and it is the I1 plant.

## Verify

`./verify fast` and `./verify full` run on the committed hash. Their JSON lines go in the PR body and the report back. This trace is committed before those runs, so its own hash cannot appear here (as in the B263 and B265 traces).

## Screenshots (scratch only, not committed)

- `r2-light.png`: defaults, with the advanced panel open.
- `r2-dark.png`: dark theme, blend, walk.

Both were taken with headless Chrome at 1680×3300 through `tools/serve_labs.py 8265`. Chrome hung at exit and was killed, as in B263 and B265.

## Open questions

1. D1: "at most one switch point per transient path" is approached by upstream-first issuing; it is not measured.
2. The supported reading of unanimity is broken by lane 1's sort with house ghosts, and the decode keeps it. Does that reading matter?
3. The decode's pair bits admit modules below their presence threshold, giving a fuller middle.
4. R2, R3, E1 and the CPU point costs are proposals awaiting the listening pass.
5. Choose p and K by ear. At p = 2 the headroom cap allows one change at a time.
6. Ringing tails exceed the CPU budget transiently.
7. Not built: the parameter-class contract, the shadow pool, the allocation count, and mid-change reversal.
8. This lab's self-check is not wired into `./verify`. That is outside this brief's file scope; a `tools/labharness/*_check.mjs` like `fxmorph_check` would wire it.
