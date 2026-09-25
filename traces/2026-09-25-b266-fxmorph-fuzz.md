# b266-fxmorph-fuzz — every FX-morph paradigm fuzzed to destruction; four collision-free architectures and a hybrid on one wiring planner

- **Queue item:** B266 (and B265's I7). Both rows are carried in `lead-records-91`. The horde lead dispatched this on 2026-09-25 and relayed the human verbatim: "no node (aside from sound-generating nodes) without any inputs should have outputs … So far none of the proposed algorithms seem to cover all the cases … I haven't really liked the 'four selectable slots' system … Maybe there's an architecture we're not considering."
- **Why:** The human wants cases, not percentages, and an architecture that avoids order collisions by construction. So the work has two parts. A property-based fuzzer runs the lab's own model and shrinks every failure to a minimal counterexample. Five new architectures (E, F, G, H, GC) are built to *claim* invariants, and the fuzzer *asserts* those claims.

## What changed

- **`docs/design/fx-chain-morph-lab.html`** (block 1 is still the one pure model; `lab-review` is now "B265 + B266 · 2026-09-25"):
  - **I7** is added, on the gain graph, in two readings:
    - **STRICT:** a processing node's live output requires its input transmission to be ≥ 0.5 (absolute, half of one unit path).
    - **TAIL-ALLOWED:** a Delay or Reverb may keep its outputs live for 0.1 of x after its input left, provided the output never rises.
    - `transmission` now shares one cached pass, `transAll`, with I7.
  - **The wiring planner.** It searches the order of single edge changes (or E's paired wet/dry mix) through states that each satisfy I2, I3·path, I6 and I7 STRICT. Only then does it place the moves on x, one crossfade apart; the crossfade narrows when there are many moves. It never stacks: T = 0 packs its moves about x = 0.5. An exhausted search is reported as a proof; a spent budget is reported as `budget`. Ranks can form a graph: GC uses the whole approved graph.
  - **E, fixed order + mix.** A junction wire J0…Jn. Each module has a send and a wet, or a dry bypass. The order is the first approved order holding both ends, and it never morphs.
  - **F, series↔parallel.** One rank jump Lx → Rx, taken at a moment when nothing connects the movers. Parallel waypoints are a second pass.
  - **G, send matrix.** Fixed rank: any common linear extension of the two ends.
  - **H, spine positions.** A's continuous keys give the crossing sequence, and each crossing is taken when the pair is disconnected.
  - **GC, the hybrid.** G's matrix walks C's approved graph.
  - **`CLAIMS` table.** Records what each architecture guarantees by construction.
  - **Self-check** grows from 12 to 15 rows, every row still with a must-fail plant:
    - I7 STRICT on E–GC, with the `i7rev` plant (an arriving module wired outputs-first).
    - The readings separate: the `i7tail` retime fails STRICT and holds TAIL-ALLOWED, while `i7rev` fails TAIL-ALLOWED too.
    - Planner soundness on the five pairs, with the `noplan` plant: the same moves, no search.
  - **Page.** Nine playable panels (a violet rule marks the planned ones), I7/I7·tail in every table and ribbon, a fuzz failure table, an objectives table, a counterexample gallery with "load ▸" (and `?gal=N`), and a new recommendation. Both themes.
- **`tools/labharness/fxmorph_fuzz.mjs`** (new, `WIRED: ./verify full`). It slices block 1 and runs it with `new Function`, not vm (L0052). It has three modes:
  - **gate:** self-check, the 5 lab pairs × 2 return settings, and 40 random pairs × T {0, .3, .7, 1} × 2 seeds; then gallery replay. About 28 s.
  - **`--sweep --chunk i/n`:** one chunk of the big run.
  - **`--merge --write`:** combines chunks, shrinks the smallest witness per (paradigm, failure), and writes the gallery.

  The seeded generator makes half independent pairs and half local edits. Each generated end is valid: every lane reaches OUT, the first module has a lane (I7 at the end), and returns follow F1–F3.
- **`docs/design/fx-chain-morph-gallery.json`** (new): 36 shrunk counterexamples plus the summary. The lab fetches it, and the gate replays it.
- **`verify`**: the fuzzer is wired into `full()` beside station/subosc. It is not in `fast()`, because it takes about 28 s.

## Results: 2000 pairs × 4 temperatures × 2 seeds = 16 000 runs per paradigm

| | hosted | I1 | I2 | I3 edge | I3 path | I4 | I5 | I6 | I7 STRICT | I7 TAIL | xf/morph | states | max live | broken claims |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A | 100% | 0 | 0 | 0 | <0.1% | 39.2% | 3.2% | 0.2% | **46.8%** | **42.3%** | 4.5 | 2.7 | 4 | n/a |
| B | 100% | 0 | 0 | 0 | <0.1% | 39.0% | 1.1% | 0.1% | **46.1%** | **42.0%** | 4.4 | 2.7 | 4 | n/a |
| C | 100% | 0 | 0 | 0 | 1.5% | 26.8% | 11.3% | 15.1% | **51.4%** | **46.1%** | 4.6 | 2.4 | 4 | n/a |
| D (control) | 100% | 40.5% | 20.6% | 66.2% | 21.6% | 56.0% | 0 | 0 | 14.9% | 13.6% | 3.5 | 5.2 | **8** | n/a |
| E* | 53.8% | 0 | 0 | 40.7% | 0 | 0 | 0 | 0 | 0 | 0 | 5.0 | 2.9 | 4 | 0 |
| F* | 100% | 0 | 0 | 0.1% | 0 | 38.6% | 0 | 0 | 0 | 0 | 5.8 | 3.3 | 4 | 0 |
| G* | 73.9% | 0 | 0 | 0 | 0 | 27.2% | 0 | 0 | 0 | 0 | 4.7 | 2.2 | 4 | 0 |
| H* | 100% | 0 | 0 | 0.1% | 0 | 39.2% | 0 | 0 | 0 | 0 | 5.8 | 3.3 | 4 | 0 |
| GC* | 66.5% | 0 | 0 | 0.2% | 0 | 0 | 0 | 0 | 0 | 0 | 5.1 | 2.7 | 4 | 0 |

- `*` marks the ASSERTED architectures. Rates are shares of hosted runs.
- F, H and GC fail I3 edge only in runs that used waypoints, where I3 edge is not claimed.
- GC's unhosted runs are refused by design (an end is not approved), except 0.2% that are proved impossible.

**The most instructive minimal counterexamples:**
1. **A, B and C break I7 on one module:** `L dry ⇒ R Rv · Bo→Rv` (T 0). The router gives each port its own time, so the Reverb's input and output fade in together: output 0.065 on input 0.004. This comes from per-port timing, so no order algorithm fixes it.
2. **E, I3 edge:** `L Dr Rv · Bo→Dr ⇒ R dry`. Half-way through the Drive's mix, the dry wire carries Body straight into the Reverb, an edge neither end has. The path reading holds.
3. **GC proved impossible:** `L Rv Dr Dl · Bo→Rv ⇒ R Dl Rv Dr · Su→Dl` (13 104 states). Rv→Dr is kept by both ends, but the only order that lets the Delay reach the front without reversing them (Rv Dl Dr) is not approved. Every approved route therefore needs a detour. F hosts this pair with one parallel jump.
4. **G and E unrepresentable:** `Rv Dr ⇒ Dr Rv`. One swap is two patches to a fixed-order architecture.
5. **D:** `L Dl Fl ⇒ R Fl` has two Filters live (I1). D's 8 max-live instances break bounded compute.

**Recommendation (in the lab):** GC, with F's jump as its fallback.
- Structure from G: four fixed module rows, so I1 and the compute bound are structural, plus a forward-only send grid.
- Order from C's approved set: the morph may walk the whole approved graph.
- Timing from the planner.
- It meets every invariant in both readings on everything it hosts, and that is asserted. On the objectives: compute 4; order variety equal to the approved set; exploration about C's (2.7 states); 5.1 crossfades per morph; misses come with a proof.
- **Gaps:** it refuses unapproved ends, which is the human's "bounded domain". It is impossible on 0.2% of runs, which is fixed by adding an order or allowing F's jump; F holds every claim too.
- **It replaces the four slots** with rows, a grid and a menu: no type-per-slot selector and no B117 shadow instance.

## Decisions made inside the brief

- **I7's floor is absolute, not endpoint-normalised.** The first sweep caught the normalised version, and it is the lesson of this run. E "broke" I7 at exactly 0.500 (L0024: a result on the threshold means the detector is wrong). The cause was that a node fed by 2–3 paths at one end dropped to 1/2 or 1/3 on keeping one. The same flaw made the planner "prove" some pairs impossible, and those pairs are solvable. The rule asks whether a node HAS input. After the fix: 0 broken claims, and the false impossibles are gone.
- **Two plant fixes, both recorded in the lab.**
  - `i7rev` first picked a Filter that already had input, so the plant did nothing. It now targets only an arriving module.
  - `i7tail` first reordered moves by slot, so the tail length depended on the seed (0.102 against 0.1). It now retimes the drop to exactly one crossfade before the output drop.
- **GC walks the approved GRAPH, not one shortest walk.** A "no plan" should mean no approved route exists at all. The one remaining impossible case survives this, and the lab comment says so rather than claiming a rescue.
- **Two passes in the planner.** Waypoints are allowed only after the ends' own edges fail, so I3 edge is claimed whenever no waypoint was used.
- **The fuzzer is named `fxmorph_fuzz.mjs`, per the brief.** It carries a `WIRED:` header anyway, but `test_table_check` only scans `*_check.mjs`, so the declaration is not machine-cross-checked (see open question 5).

## Evidence consulted

- ROADMAP B265/B266 rows (`origin/lead-records-91`).
- The B265 trace and lab.
- `tools/golden/extract_core.mjs` and `tools/labharness/modlab_reach.mjs` (the slicing idiom).
- `tools/labharness/lab_load_check.mjs` (sandbox globals: no `fetch`, hence the `typeof` guard).
- `tools/test_table_check.py` (WIRED grammar).
- `src/fx_rack.h:1-120, 270-390`: four series slots, order = slot index, per-type caps, rack-owned mix where 0 is a guaranteed bypass, and the B117 shadow crossfade.
- `src/gui/gui2.html` `#pg-FX` "About the rack".
- INDEX L0016, L0024, L0032, L0041, L0052.

## Alternatives rejected

- **Teaching the B265 router an I7 repair pass.** Per-port times plus repairs cannot prove a miss, and I6, the reach guard and I7 would fight as I6 and the reach guard already did (B265 trace). A search over states makes every constraint a state check.
- **Brute-force verification of "impossible" verdicts.** It is exponential once waypoints are included. The DFS memo is exact on (rank, units done), and each shrunk impossible case is small enough to check by hand (case 3 above).
- **Inlining the gallery in the HTML.** That would put a second copy beside the file the gate replays.

## Calibration of the gate itself (L0032)

- A scratch copy of the lab whose planner skips the I7 state check (anchor asserted) is run with `fxmorph_fuzz.mjs --lab <copy>`. It gives exit 1 with 13 FAILs: self-check rows 13 and 15, and F breaking its claimed I7 on random pairs.
- The unmutated lab gives exit 0 in 28 s with 36/36 gallery cases replayed.
- This also shows the planner's move ordering alone does not hold I7; the search does.

## Verify

`./verify fast` and `./verify full` were run on the committed hash; the results are in the PR body and the report back. This trace is committed before that run, so its own hash cannot appear here (as in the B263/B265 traces).

## Screenshots (scratch only, not committed)

- `fxmorph-b266-light.png`: Sub on the glue, listening to GC.
- `fxmorph-b266-dark-gallery.png`: gallery case 5 loaded (A's I7 STRICT minimal).

Both are headless Chrome at 1680×5600 against `tools/serve_labs.py 8266`; Chrome was killed after writing, as in B263.

## Open questions

1. Confirm that I7's floor is absolute (does the node HAVE input?) rather than relative to its endpoint input.
2. STRICT or TAIL-ALLOWED? The planner holds STRICT. Keeping tails on purpose would be a planner rule, not a checker excuse.
3. GC's impossible routes are gaps in the approved menu. Add Rv Dl Dr, or allow F's jump as the fallback?
4. The planner's T = 0 is "the fastest legal cut" (moves packed one crossfade apart), not the hard cut. Is that the right reading?
5. `test_table_check` only cross-checks `*_check.mjs`. Should `fxmorph_fuzz.mjs` be renamed `fxmorph_check.mjs` so its WIRED declaration is machine-checked? (The brief fixed the name.)
6. E's dry wire fails I3 edge by design. Does the edge reading still matter once the path reading is the rule (B265 open 1)?
7. Make-before-break loudness (B265 open 3) applies to the planner too.

**Renamed by the lead after hand-back (2026-09-25):** `tools/labharness/fxmorph_fuzz.mjs` → `fxmorph_check.mjs`, so `test_table_check`'s wired-or-explained census (which scans `*_check.mjs`) verifies its `WIRED:` header.
