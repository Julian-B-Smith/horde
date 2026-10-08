# b265-fx-chain-morph-lab-r3 — static nodes: the morph moves only cables, raced against round 2 on a click metric

- **Queue item:** B265, round 3. Dispatched by the horde lead on 2026-10-08 (fourth dispatch; the first three stalled when the laptop slept). The human, after listening to round 2: "I'm hearing a lot of clicks when devices enter and leave the chain. Since there is now only one of each device, let's think about it differently: no device has to click into or out of a slot. They all sit static, and the connections between them gradually emerge and retreat according to the same logic and prohibitions." Then: "Start round 3".
- **Why:** Test the static-node model against round 2's insert/remove behaviour by ear and by a stated metric, under the same map, rules (V, ADR-195) and pad.

## What was built

`docs/design/fx-chain-morph-lab-r3.html`, a new self-contained lab (`lab-part FX`, `lab-review B265 · 2026-10-08`, both themes). Rounds 1 and 2 are untouched. Round 2's map (lines 303–702), router (704–949), stand-ins and engine (1075–1190), sweep paths (998–1010, 1073), `db` (996) and painters (1654–1705, 1749–1761) were copied by line range with a script, not retyped. The router and engine were renamed `R2Router` and `R2Engine`. The navigator `docs/design/index.html` was regenerated, and it now pins the lab for review under FX.

**The model** (block 1: no DOM, no clock, mulberry32 only):

- **Cable universe.** There are 82 fixed cables: lane→node, lane→OUT, node→node and node→OUT. All eight nodes are resident.
- **Targets.** Round 2's `mapW` chain is written as cables in closed form. Through the dry products, each lane's static transmission is exactly 1. QUANTUM gives cables of 0 or 1; BLEND gives the wet gains as cables.
- **`Cables`** is a control-rate state machine:
  - K admission;
  - a tail hold (a leaving ECHO, Scape or Sluice keeps its output cable for min(its own decay, its cap));
  - a V path guard;
  - forward and backward classes;
  - two cascaded one-pole glides of τ/2 each;
  - the loop bound (a);
  - lane normalisation.
- **`Rack`** runs eight resident instances block-wise in the forward order. A backward cable reads the source's previous block. Sleep keeps state.
- **ADR-195.**
  - Bulwark's band count is a flip-class parameter. It uses round 1's threshold law, keyed by value.
  - In BLEND, a multiband Bulwark's presence flips.
  - The stand-in is an allpass-phased two-band split.
  - A band change is a 20 ms internal crossfade.
  - The bypass routes into a multiband Bulwark.
- **A new "bait" corner set.** Every corner is in V, but a naive crossfade between them opens Scape ⇝ Sluice and Scape ⇝ Bulwark paths, and Bulwark's band count flips across the top edge.

**Controls.**

- Dev toggles: the A/B (round 3 or round 2), loop (a) or (b), mode, sleep, sharpening, jitter, snap with decode or walk, normalisation, and tails.
- Advanced panel: glide τ, loop bound (top = OFF), sleep threshold, γ, temperature, p, K and seed.

**Cable view.**

- Nodes sit on a fixed ring in house order, with the lanes on the left and OUT on the right.
- Cables bow left of their direction. Thickness and brightness follow the weight.
- A dotted flow runs along each cable; it is static under `prefers-reduced-motion`.
- A backward part is dashed and marked z⁻¹. Live-loop cables are drawn in caution colour, and waiting cables are dotted ghosts.
- Scape → Sluice is shown crossed out; Scape → Bulwark is marked E1-only.
- Asleep nodes are dimmed.
- Round 2's frame is folded onto the same nodes, so the A/B is drawn in one language.

## Click metric (stated in the lab above `clickOf`)

- **Definition.** Each 2 ms slice gets the RMS of the output's second difference. That value is divided by the median of the slice's ±32 ms neighbours, in dB.
- **Per transition.** The click is the worst value in the 1 s after the change, minus the worst value in the steady 0.25 s before and after, floored at 0.
- **Transitions.** Each structural change a sweep path crosses becomes a step from the same steady state, rendered on a sine probe.
- **Why this metric.** It needs no reference render, it is local (so the live meter runs the same function), and it is deterministic.

## Results (defaults: four presets, quantum, decode, glide 60 ms, bound 0.5, p 2, K 2, seed 265; headless node, the same as the page)

Click metric:

| set · mode | n | (a) worst / mean / >3 dB | (b) | round 2 |
|---|---|---|---|---|
| four · quantum | 26 | 6.0 / 1.64 / 5 | 6.0 / 1.62 / 5 | 33.2 / 13.88 / 14 |
| four · blend | 32 | 5.8 / 1.03 / 3 | 4.5 / 0.96 / 3 | 33.0 / 11.71 / 15 |
| bait · quantum | 33 | 3.3 / 0.79 / 1 | 3.3 / 0.79 / 1 | 38.9 / 12.14 / 15 |
| bait · blend | 35 | 1.5 / 0.22 / 0 | 1.5 / 0.21 / 0 | 42.7 / 14.79 / 15 |
| close · quantum | 15 | 4.2 / 1.72 / 3 | 4.0 / 1.52 / 2 | 34.3 / 16.43 / 10 |
| round1 · quantum | 26 | 4.3 / 1.12 / 3 | 4.4 / 1.23 / 1 | 33.0 / 9.24 / 9 |

Glide against the worst click (four, quantum, (a)):

| glide | worst click |
|---|---|
| 40 ms | 7.2 dB |
| 60 ms (default) | 6.0 dB |
| 80 ms | 5.1 dB |

Other figures:

- **CPU, rendered sweeps with the pulse on:** 8.6 points on average across the four and bait sets, against 15 with every node awake. Awake nodes ranged from 0–2 to 8.
- **Lane gain:** stays within +0.00 to +3.01 dB in every sweep.
- **Unbounded loop gain:** reaches 1.5 on the four set and 1.9 on round 1's pairs; it is held at 0.5.
- **Multiband Bulwark beside an unphased branch:** at most 0.32 s.

## Self-check: 8/8 claims hold, 8/8 planted faults caught (headless, and in headless Chrome at load)

The check is sliced into 175 steps; the longest takes about 2.6 s.

| # | claim | plant |
|---|---|---|
| 1 | Corners settle on their presets' cables; 32 pad-edge sweeps are identical to the two-corner morph on all 82 cables at every tick | Radial weights |
| 2 | No Scape → Sluice cable and no forbidden live path (112 sweeps) | Guard off on bait: 416 ticks |
| 3 | A multiband Bulwark beside an unphased branch for at most one glide (longest 0.32 s) | Presence blended: 1.97 s |
| 4 | Loop ≤ 0.5 in (a); (b) never has a cycle | Bound off: 1.50 |
| 5 | Lanes stay in [−6.02, +3.01] dB | (b) without the bypass: −180 dB |
| 6 | Sleep and wake steps ≤ 6× the threshold (worst −80 dB); CPU 8.6 / 15 | Sleep ignoring the tail: 0.141 |
| 7 | (a) 6.0 and (b) 6.0 dB worst, both below round 2 | Round 2 reads 33.2 dB (≥ 12) |
| 8 | Bit-identical render and identical cables on the same seed | The next seed differs |

## Decisions made inside the brief (each is commented in the lab)

- **The normaliser's notion of "target cables" glides** (a membership m ∈ [0, 1]). A binary mask stepped a lane's source cable by 0.74 in one tick. Gliding the factor instead lagged, overshot and collided with the +3 dB cap.
- **The loop bound scales the backward part of every cable in a violating cycle**, by bisection. Scaling only purely-backward cables stepped 0.27 when a cable began its backward→forward crossfade.
- **Sleep reads buffer contents** (`peakWin`), not just output. A disconnected ECHO slept at once and later replayed a frozen echo (a 0.033 step). Only a memory that never decays is slept loud, after its cap, with a one-glide wake ramp.
- **A ringing tail never blocks a cable;** it is cut short and logged.
- **"Per-cable thresholds" in QUANTUM** is read as round 2's keyed draws per module and per ordered pair. No new draw was added.
- **ADR-195's "brief"** is read as one glide (10 τ).
- **The continuity figure excludes lane source cables,** which carry the normaliser. The normaliser's rate is reported separately.
- **Default glide is 60 ms.**

## Evidence consulted

- `CLAUDE.md`
- The ROADMAP B265 row
- ADR-193 and ADR-195 in `DECISIONS.md`
- `docs/proposals/fx-chain-morph-round2.md` (headings)
- `traces/2026-10-05-b265-fx-chain-morph-lab-r2.md`
- `docs/design/fx-chain-morph-lab-r2.html`, in full by slices
- `tools/labharness/lab_load_check.mjs`, `tools/gen_lab_index.py` and `tools/serve_labs.py`
- INDEX entries L0026, L0064 and L0073

## Alternatives rejected

- **A continuous per-cable blend of the corners' cables in BLEND.** It sustains cycles and V-breaking paths at the pad's middle. The brief says round 2's logic decides which cables exist.
- **Re-classifying a cable forward/backward by the current order.** That switches its delay at full weight, which is a step. The twin parts with a crossfade replaced it.
- **A path-aware glide to remove the sag the normaliser compensates.** Not built; it is open question 5.

## Verify

`./verify fast` and `./verify full` run on the committed hash. Their JSON lines go in the PR body and the report back. This trace is committed before those runs, so its own hash cannot appear here (as in the round 2 trace).

## Screenshots (scratch only, not committed)

- `r3-light.png`: defaults, with the advanced panel open.
- `r3-dark.png`: dark theme, bait set, blend, (b), with the self-check finished at 8/8.

Both were taken in headless Chrome at 1680×3300 through `tools/serve_labs.py 8365`. Chrome was killed after each shot.

## Open questions

1. (a) or (b), by ear on reorders.
2. Glide τ.
3. The QUANTUM per-cable reading.
4. The loop bound and the declared node gains are declarations.
5. The normaliser is now the main transient (a path-aware glide was not built).
6. A self-oscillating Sluice's kept state returns on wake.
7. The ADR-195 "brief" bound.
8. The click metric's blind spots: bumps over about 30 ms, and anything more than 1 s after a change.
9. The self-check is not wired into `./verify`, as in round 2.
10. Not yet heard by a person: audio runs through a ScriptProcessor and was not played back in this session.
