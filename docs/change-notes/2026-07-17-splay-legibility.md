# CHANGE-NOTE · reference/swarmsaw.html — splay legibility update (dual order parameters, seats, formation polygon)

**Date:** 2026-07-17
**Scope:** visualization + one new engine readout in the SAW reference implementation (`reference/swarmsaw.html`). No audio-path changes. Kit copy refreshed; treat this file as an amendment to `specs/SPEC.md §5.6` and a parity note for Phase 1/5.

## 1. The defect

The phase circle displayed only the first-harmonic order parameter R₁ ("how clumped are the dots"). R₁ reads ≈ 0 for **two entirely different states**:

- **Free** — no coupling, voices drifting independently (disorder)
- **Locked splay** — negative K, voices held in a rigid, evenly-spaced rotating formation (maximal *non-clumped* order)

So the display could not distinguish the instrument's most novel state from the absence of coupling. This was a spec defect, not just prototype polish: `specs/SPEC.md §5.6` specified the phase circle with only the R·e^{iψ} vector, under-specifying in exactly the same way.

## 2. Conceptual background (why a second order parameter)

Splay order is measured by the N-th generalized (Daido) order parameter:

```
R_N = | (1/N) · Σ_i exp(i · N · θ_i) |
```

Under perfect splay (θ_i = θ_anchor + i/N cycles), the factor N·θ_i makes all phasors align → R_N = 1 while R₁ = 0. Theory note: exact full sync would *also* give R_N = 1, but this ambiguity does not occur in practice — real sync lock leaves per-voice offsets asin(Δω_i/KR), and R_N magnifies phase deviations N-fold, decohering it. Measured on the reference implementation (A3, defaults, settled):

| Regime | K | R₁ | R_N |
|---|---|---|---|
| Sync | +1.0 | 0.97 | 0.07 |
| Free | 0.0 | 0.39 | 0.16 |
| Splay | −1.0 | 0.04 | 0.84 |

The two meters are therefore **orthogonal regime indicators**: R₁ high = sync; R₁ low + R_N high = splay; both low = free. These three rows are new acceptance data — add to `specs/ACCEPTANCE.md` as **L0-3 extension**: R_N regime classification must match this table, values ±0.08 (the splay R_N ≥ 0.75 criterion already existed; the sync-side R_N ≈ 0.07 and free-side values are new regression anchors).

## 3. What changed (implementation)

### 3.1 Engine (one addition)
`controlTick`: after the existing R₁/ψ mean-field computation, a second accumulation over `phase[i] · 2π · n` producing `s.RN` (stored per swarm). Cost: N extra cos/sin per control tick. **Parity-relevant:** the C++ port must compute R_N identically (same loop order, same n) since it is now a displayed and asserted quantity.

### 3.2 Visualization (three additions, all reading existing state)
1. **Seat rings.** When splay is engaged (`KsmP` above threshold), hollow amber rings are drawn at each voice's assigned slot: `θ_seat(i) = phase[centerIdx] + (i − centerIdx)/n`, rotating with the anchor. Opacity ramps with engagement: `min(0.7, KsmP/(KsmP + 2))`. Purpose: the splay attractor's *targets* are visible, so herding reads as voices being pulled into chairs.
2. **Formation polygon.** A closed low-alpha path connecting voices in index order. This single element disambiguates all three regimes at a glance: **collapsed point** (sync) · **regular N-gon parked on its seats** (splay) · **drifting scribble** (free).
3. **Second meter.** Amber R_N bar under the R₁ bar, with a one-line regime legend. Meter labels renamed: "R₁ sync (clump)" / "Rₙ splay (formation)".

Hint text updated to explain the harmonic-cancellation mechanism (splay cancels every harmonic that isn't a multiple of N; a saw keeps {N, 2N, 3N…} → saw at N·f; a sine keeps nothing → interference erasure) and the circle-reading guide.

## 4. Spec amendments to carry forward

- **SPEC §5.6 (visualization contract):** phase circle now requires — order vector R₁·e^{iψ}, **dual order meters (R₁, R_N)**, **seat rings under splay engagement**, **formation polygon**. The Phase 5 GUI inherits this; it is part of the instrument's face, not a debug view.
- **ACCEPTANCE L0-3:** add the three-regime R₁/R_N table above as regression anchors.
- **Forward-compatibility (Daido):** if the multi-pole coupling extension lands (ADR pending — coupling function with sin(qΔθ) harmonics, q-cluster states), this visualization generalizes for free: q-cluster states render as q-pointed formations on the same polygon, and the R_N meter generalizes to an R_q family. Design the GUI meter component to take q as a parameter, not hardcode N.

## 5. What did NOT change

Audio path, coupling math, splay attractor (still assigned-slot per ADR-007), all parameter ranges, all previously green acceptance numbers. `./verify`-equivalent headless checks re-run post-change: sync/free/splay regimes reproduce prior R₁ trajectories; peak/NaN sanity clean.
