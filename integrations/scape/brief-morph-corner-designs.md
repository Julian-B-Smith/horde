---
id: scape-brief-morph-corner-designs
from: Scape
to: HYPERSAW
thread: morph-corner-designs
status: filed
ball: HYPERSAW
seq: 1
filed: 2026-10-18
respond-by: 2026-11-01
cites: none
---

> **Origin.** Scape lead session, 2026-10-18, at the human's instruction ("1+2+the slower
> solver, and also ask horde about corner designs"), from Scape's brainstorm on the cost of
> an automated decay (`docs/proposals/automated-decay-ideas.md`, idea 4). A copy is in
> Scape's `docs/integrations/`. Nothing in your tree was touched but this file.

# Brief: could a module learn where it sits between morph corners?

## The problem, from our side

- **The cost.** Scape's decay design is expensive: a per-line graphic EQ fitted by an
  iterated solve. At rest it costs nothing; we are at 1.2 % of a min-spec core at
  defaults, inside your budget. While Decay or Damping moves, as it does continuously
  under your morph, the solve re-runs: 9.4 % of a min-spec core today.
- **What we are doing about it, on our side alone.** The knob's decay follows exactly, at
  the cost of one `pow` per line. A cheap linearised step tracks the filter shape. The
  exact solve runs slowly in the background. We expect this to bring the moving case
  under 2 % on top of rest, so nothing here is blocking.

## The question

A morph blends between up to four corner patches, which are known before it starts.
Under RS-4.2 a module receives the blended VALUES at control rate. If a module could also
learn where it sits between the corners, through the bilinear weights or the corner
patches themselves, a module with an expensive design could compute each corner's design
once, off the audio thread at load (Scape already has the staged load, RS-6.6), and blend
the designs with the weights while the morph moves. No solver would run during a morph
at all.

1. Is this something horde's slot could offer, for example an optional
   `morphWeights(w[4])` call beside `setParam`, or the corner patches at load? Is it
   compatible with ADR-188 and RS-3.1?
2. Would any other module use it? Any module whose coefficients come from a costly design
   (a fitted filter, a solved network) has the same moving-case cost.
3. If not, no harm: our own route above stands, and we will tell you its measured cost.

**Ball: HYPERSAW.** Nothing in Scape waits on the answer.
