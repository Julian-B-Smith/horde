# b265-fx-chain-morph-lab — the FX-chain morph, pared to its core and raced four ways on six invariants

- **Queue item:** B265. The row is carried in `lead-records-89`, dispatched 2026-09-25 by the horde lead. Mid-task the lead relayed the human's sixth invariant, verbatim: "there needs to be a way to enforce that every input reaches an output; I don't want any intermediate morph states where all paths from an osc to an output are severed". It became I6.
- **Why:** The human dislikes the patch-model lab's hard cut at 50 % and wants a middle that is "a stochastic or, when achievable, gradual flow". The lab races four FX-chain morph algorithms on the lead's pared problem, so the choice rests on measured invariants rather than intuition.

## What was built

`docs/design/fx-chain-morph-lab.html` is a new, self-contained file. It carries `lab-review` "B265 · 2026-09-25" and a `.tagline`, uses gui2's tokens (copied through the patch-model lab), and has both themes.

**The world.**
- Four module types: Drive (tanh), Filter (TPT SVF lowpass), Delay (one line) and Reverb (a 4-comb, 2-allpass Schroeder). At most one of each.
- Two lanes: Body (a PolyBLEP saw chord) and Sub (a sine an octave down).
- Five endpoint pairs:
  - one swap;
  - across the set (four inversions);
  - two grow in;
  - **adversarial · Sub on the glue** (tempts I3). In L the Sub enters only the glue Drive at the chain end; in R it enters the Filter of a reversed chain. The Sub reaches the Reverb at neither end.
  - **adversarial · Sub through a leaver** (tempts I6). In L the Sub's only path runs through the Reverb, and R has no Reverb.

**The structure found.** A, B and C are ORDER-PATH generators. All three sit on one shared router:
- **Ports.** Each module and OUT has an input port. L-only sources fade out and R-only sources fade in.
- **Parking.** An absent module keeps a place in the order with no edges, so inserting or removing a module is a port fade rather than an order event.
- **The order as a topological gate.** An edge is live only while it runs forward, so a swap happens only when nothing connects the pair.
- **Per-edge seeded crossfades.** Each port draws one time from the quantum law and moves every edge whose legal window contains that time together.
- **Make-before-break repair (I6).**
- **A reach guard (I3 read on paths).**
- **Order-preserving I5 respacing.** A respacing is refused if it would break I6 or the reach guard.

D is the naive slot splicer: slots, serial wiring, lanes entering by slot index.

**The threshold law** is `morph_core.h` `pickCorner` reduced to two corners: an event fires when x > σ(T·(gL−gR)), with coupling mixing in a shared draw. At T = 0 every threshold is 0.5 (the hard cut). At T = 1 the thresholds are uniform.

**Audible.** POWER runs the model's own `Engine` (a sample counter, never a clock) through a ScriptProcessor. The puck's x slews with a 40 ms time constant. You can LISTEN to A, B, C or D, and PULSE gates the chord.

## Metrics (seed 265, T 0.7, coupling 0, no return; `scratchpad/b265/t7.mjs` headless, the same numbers the page shows)

| pair | A rank keys | B edit path | C approved graph | D splice (control) |
|---|---|---|---|---|
| one swap | all ✓ · 8 xf · 7 chains · 2.8 dB | all ✓ · 4 · 4 · 2.8 | all ✓ · 5 · 4 · 3.6 | I1 8 %, I3 37 %, I3·path 33 %, I4 8 % · 7 chains |
| across the set | I4 65 % · 7 · 8 · 4.5 | I4 8 % · 9 · 4 · 2.9 | all ✓ · 12 · 8 · 4.3 | I1 44 %, I3 38 %, I4 44 % · 9 |
| two grow in | all ✓ · 7 · 4 · 3.7 | all ✓ · 5 · 4 · 4.0 | all ✓ · 6 · 5 · 2.7 | I1 8 %, I3 12 %, I4 8 % · 8 |
| Sub on the glue | I4 35 % · 10 · 7 · 2.5 | all ✓ · 10 · 7 · 4.0 | I5 4 crowded · 10 · 7 · 3.6 | I1 45 %, **I3 74 % (Sub→Reverb)**, I4 45 % · 13 |
| Sub through a leaver | I5 3 crowded · 9 · 5 · 5.1 | all ✓ · 9 · 6 · 3.1 | all ✓ · 9 · 6 · 2.3 | I3 13 %, I4 4 % · 5 |

- Min lane→output gain is ≥ 0.63 everywhere, and D's is 1.00: D is smooth, and wrong. The still-endpoint baselines run 1.3–2.0 dB.
- Seven checks held, per algorithm: A 2/5 pairs, B 4/5, C 4/5, D 0/5. The count is the same with the return on.
- I1, I2, I3 (edge reading), I3·path and I6 hold for A, B and C on every pair in both return modes. The same was true across seeds 1–3, with one exception: A on the Sub-on-the-glue pair at seed 2 sat exactly on the floor (0.50).

## Self-check (at load, deferred one step per tick; also run headless: 12/12 hold, 12/12 caught)

| # | claim | planted fault the check must catch | caught |
|---|---|---|---|
| 1 | I1 | two-instance swap | 48/601 x |
| 2 | I2 | three plants | all three caught |
| | | router without its order gate | forward cycle from x = 0.297 |
| | | return at 1.8× | gain 0.54 > 0.5 |
| | | return into Filter | caught |
| 3 | I3 | serial reading | 152/601 x |
| 4 | I3·path | reach guard off, C on Sub on the glue | Sub→Rv at 295/601 x |
| 5 | I4 (C) | unapproved order spliced in | 16/601 x |
| 6 | I5 crossfaded | w = 0 | weight step 1.00 |
| 7 | I5 spaced (B) | coupling 1 with spread off | 3 crowded |
| 8 | I6 | break-before-make on the leaver pair | min gain 0.00 at 352/601 x |
| 9 | endpoints exact | crossfades hanging off the ends | caught |
| 10 | render metric | break-before-make rendered | 43.7 dB against 3.1 morph and 1.4 still |
| 11 | threshold law | coupling 1 | σ collapses to 3.8e-15 |
| 12 | determinism | next seed | events move |

## Feedback

Rules:
- F1: a return lands on a Delay or Reverb.
- F2: it runs backward (the gate enforces this).
- F3: gain ≤ 0.5, through tanh.
- F4: at most one return. Two different returns hand over break-before-make.

Result: I2 and I3 hold for A, B and C with the return on. The return survives the whole morph when both ends share it (one swap: 100 %). With different returns, a return is live for 52–92 % of x, depending on the algorithm and pair. The rest is the handover gap. A: across 60 %, leaver 65 %. B: 64 % and 64 %. C: 52 % and 92 %. D reads 100 % because it crossfades the two returns at once, which is its I2 failure. The cost:
- one more port with a legality window;
- up to two more crossfades per morph (A/B/C +6, D +4 over five pairs);
- one sample of loop latency and a tanh.

## Decisions made inside the brief (each recorded in the lab's comments)

- **Ports schedule per edge, not per port.** Grouping a port's edges unconditionally forced an always-legal lane edge to drop with an order-pinned sibling, and severed the Sub in Sub-on-the-glue.
- **The I6 repair delays breaks and hurries makes in the same pass.** Doing breaks first, with makes as a fallback, made the I6 repair and the reach guard undo each other.
- **I6 floor = 0.5 of the lane's endpoint gain.** 0.707², two overlapping equal-power crossfades on one path, is the lowest a correct schedule can reach. Break-before-make reads 0.
- **A click ratio was tried and dropped.** The engine ramps every edge over one 2 ms block, so a hard switch never reaches the samples. The structural weight-step check catches hard switches instead.
- **B/C steps relax toward even spacing** (β = ½, then 1) only when the seeded placement crowds.

## Evidence consulted

- ROADMAP B263/B265 rows (`origin/lead-records-89`).
- `src/morph_core.h:55-100` (reshuffle, pickCorner, logW).
- `docs/design/patch-model-lab.html` and `station-page-lab.html`: tokens and idiom.
- `tools/labharness/lab_load_check.mjs` and `tools/gen_lab_index.py`.
- The B263 trace, for the screenshot method.

## Alternatives rejected

- Dual-instance crossfades for moves: they break the four-module bound and are I1's planted fault.
- Serial rewiring of intermediate orders: it invents adjacencies and is I3's planted fault.
- Bypass wires around a leaving module: they create edges neither end has. Rerouting the lane to its R entry does the job inside I3.

## Verify

`./verify fast` runs on the committed hash; the result is in the PR body and the report back. This trace is committed before that run, so its own hash cannot appear here (as in the B263 trace).

## Screenshots (scratch only, not committed)

- `fxmorph-light.png`: Sub on the glue, no return.
- `fxmorph-dark.png`: Sub through a leaver, return on, x = 0.45.

Both were taken with headless Chrome at 1680×3300 with a virtual-time budget so the deferred jobs finish, served by `tools/serve_labs.py 8265`. Chrome hung at exit and was killed after writing, as recorded in B263.

## Open questions

1. Is I3's PATH reading the rule? The human's words suggest it. The edge reading alone is broken by make-before-break.
2. Steps should be placed by router demand. This is C's crowding on dense reorders.
3. Make-before-break is louder (lane gain reaches 2–3 in parallel states). Normalise per lane?
4. The approved set needs a listening pass.
5. Is the return-handover gap acceptable?
6. Are 10–14 crossfades per morph "discovery" or busy?
7. A headless `fxchain_check.mjs` wired into `./verify full` (like `station_check`) would keep the self-check honest after edits. It was not added because it is outside this brief's file scope.
